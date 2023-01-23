/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"

#define ME "ap11v"

typedef struct {
    amxc_llist_it_t it;
    int retries;
    wld_transferStaArgs_t params;
    int done_retries;
    int timeout;
    uint64_t call_id;
    T_AccessPoint* ap;
    amxp_timer_t* timer;
} bss_wait_t;

amxc_llist_t bss_wait_list;

/**
 * Generate reply when request is done
 */
static void ap_bss_done(bss_wait_t* wait, int reply_code) {
    SAH_TRACEZ_INFO(ME, "Done %i", reply_code);
    amxc_llist_it_take(&wait->it);
    amxp_timer_delete(&wait->timer);
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set(int32_t, &ret, reply_code);
    amxd_function_deferred_done(wait->call_id, amxd_status_ok, NULL, &ret);
    amxc_var_clean(&ret);
    free(wait);
}

/**
 * Callback function for vendor plug-ins to indicate a given mac provided a reply.
 * Reply code should come from 802.11v request
 */
void wld_ap_bss_done(T_AccessPoint* ap, const swl_macChar_t* mac, int reply_code) {
    SAH_TRACEZ_INFO(ME, "bss done %s", mac->cMac);
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &bss_wait_list) {
        bss_wait_t* wait = amxc_llist_it_get_data(it, bss_wait_t, it);
        if((wait->ap == ap) && swl_mac_charMatches(mac, &wait->params.sta)) {
            ap_bss_done(wait, reply_code);
            return;
        }
    }
}

/**
 * Timeout handler for bss transfer frame retry.
 */
static void bss_tranfer_timeout_handler(amxp_timer_t* timer _UNUSED, void* userdata) {
    ASSERT_NOT_NULL(userdata, , ME, "NULL");
    bss_wait_t* wait_item = (bss_wait_t*) userdata;
    int cmdRetval = WLD_ERROR;
    if(wait_item->retries > wait_item->done_retries) {
        SAH_TRACEZ_INFO(ME, "retry %s %s %u / %u", wait_item->ap->alias, wait_item->params.sta.cMac, wait_item->done_retries, wait_item->retries);
        wait_item->done_retries++;
        cmdRetval = wait_item->ap->pFA->mfn_wvap_transfer_sta_ext(wait_item->ap, &wait_item->params);
        if(cmdRetval == WLD_ERROR_NOT_IMPLEMENTED) {
            wait_item->ap->pFA->mfn_wvap_transfer_sta(
                wait_item->ap,
                wait_item->params.sta.cMac,
                wait_item->params.targetBssid.cMac,
                wait_item->params.operClass,
                wait_item->params.channel);
        }

        amxp_timer_start(wait_item->timer, wait_item->timeout);
    } else {
        ap_bss_done(wait_item, -1);
    }
}

/**
 * Call to initiate a 802.11v bss transfer request.
 */
amxd_status_t _sendBssTransferRequest(amxd_object_t* object,
                                      amxd_function_t* func,
                                      amxc_var_t* args,
                                      amxc_var_t* ret) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");

    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_ok;

    const char* mac = GET_CHAR(args, "mac");
    const char* bssid = GET_CHAR(args, "target");

    uint32_t ms_wait_time;
    uint32_t retries;

    int cmdRetval = WLD_ERROR;

    wld_transferStaArgs_t params;

    amxc_var_t* operClass = amxc_var_get_key(args, "class", AMXC_VAR_FLAG_DEFAULT);
    params.operClass = (operClass == NULL) ? 0 : amxc_var_dyncast(int8_t, operClass);

    amxc_var_t* channel = amxc_var_get_key(args, "channel", AMXC_VAR_FLAG_DEFAULT);
    params.channel = (channel == NULL) ? 0 : amxc_var_dyncast(int8_t, channel);

    amxc_var_t* ms_wait_timeVar = amxc_var_get_key(args, "wait", AMXC_VAR_FLAG_DEFAULT);
    ms_wait_time = (ms_wait_timeVar == NULL) ? 0 : amxc_var_dyncast(uint32_t, ms_wait_timeVar);

    amxc_var_t* retriesVar = amxc_var_get_key(args, "retries", AMXC_VAR_FLAG_DEFAULT);
    retries = (retriesVar == NULL) ? 0 : amxc_var_dyncast(uint32_t, retriesVar);

    amxc_var_t* validity = amxc_var_get_key(args, "validity", AMXC_VAR_FLAG_DEFAULT);
    params.validity = (validity == NULL) ? 0 : amxc_var_dyncast(int32_t, validity);

    amxc_var_t* disassoc = amxc_var_get_key(args, "disassoc", AMXC_VAR_FLAG_DEFAULT);
    params.disassoc = (disassoc == NULL) ? 15 : amxc_var_dyncast(int32_t, disassoc);

    amxc_var_t* bssidInfo = amxc_var_get_key(args, "bssidInfo", AMXC_VAR_FLAG_DEFAULT);
    params.bssidInfo = (bssidInfo == NULL) ? 0 : amxc_var_dyncast(uint32_t, bssidInfo);

    amxc_var_t* transitionReason = amxc_var_get_key(args, "transitionReason", AMXC_VAR_FLAG_DEFAULT);
    params.transitionReason = (transitionReason == NULL) ? 0 : amxc_var_dyncast(int32_t, transitionReason);

    if(!mac || !bssid || (params.channel == 0)) {
        SAH_TRACEZ_ERROR(ME, "Invalid argument");
        status = amxd_status_invalid_function_argument;
        goto end;
    }

    swl_typeMacChar_fromChar(&params.sta, mac);
    swl_typeMacChar_fromChar(&params.targetBssid, bssid);

    SAH_TRACEZ_NOTICE(ME, "Steer %s @ %s 2 %s/%u:%u retry %u x %u ms",
                      params.sta.cMac, pAP->alias, params.targetBssid.cMac,
                      params.operClass, params.channel, retries, ms_wait_time);

    cmdRetval = pAP->pFA->mfn_wvap_transfer_sta_ext(pAP, &params);
    if(cmdRetval == WLD_ERROR_NOT_IMPLEMENTED) {
        cmdRetval = pAP->pFA->mfn_wvap_transfer_sta(pAP, params.sta.cMac, params.targetBssid.cMac,
                                                    params.operClass, params.channel);
        SAH_TRACEZ_INFO(ME, "Extended Function not supported");
    }
    if(cmdRetval == WLD_ERROR_NOT_IMPLEMENTED) {
        SAH_TRACEZ_ERROR(ME, "Function not supported");
        status = amxd_status_function_not_implemented;
    } else if(cmdRetval < 0) {
        SAH_TRACEZ_ERROR(ME, "Error during execution");
        status = amxd_status_unknown_error;
    } else if(ms_wait_time > 0) {
        SAH_TRACEZ_INFO(ME, "Waiting %u", ms_wait_time);
        bss_wait_t* wait_item = calloc(1, sizeof(bss_wait_t));
        if(!wait_item) {
            SAH_TRACEZ_ERROR(ME, "wait_item is NULL");
            status = amxd_status_out_of_mem;
            goto end;
        }
        wait_item->ap = pAP;
        wait_item->params = params;
        wait_item->retries = retries;
        wait_item->timeout = ms_wait_time;
        amxp_timer_new(&wait_item->timer, bss_tranfer_timeout_handler, wait_item);
        wait_item->timer->priv = wait_item;
        amxp_timer_start(wait_item->timer, ms_wait_time);
        amxc_llist_append(&bss_wait_list, &(wait_item->it));
        wait_item->call_id = amxc_var_dyncast(uint64_t, ret);
        amxd_function_defer(func, &wait_item->call_id, ret, NULL, NULL);
        status = amxd_status_deferred;
    } else {
        amxc_var_init(ret);
        amxc_var_set(int32_t, ret, -1);
    }

end:
    SAH_TRACEZ_OUT(ME);
    return status;
}
