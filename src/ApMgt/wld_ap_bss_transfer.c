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
static void s_bssTransferTimeoutHandler(amxp_timer_t* timer _UNUSED, void* userdata) {
    ASSERT_NOT_NULL(userdata, , ME, "NULL");
    bss_wait_t* waitItem = (bss_wait_t*) userdata;
    int cmdRetval = WLD_ERROR;
    if(waitItem->retries > waitItem->done_retries) {
        SAH_TRACEZ_INFO(ME, "retry %s %s %u / %u", waitItem->ap->alias, waitItem->params.sta.cMac, waitItem->done_retries, waitItem->retries);
        waitItem->done_retries++;
        waitItem->ap->pFA->mfn_wvap_transfer_sta(waitItem->ap, &waitItem->params);

        amxp_timer_start(waitItem->timer, waitItem->timeout);
    } else {
        ap_bss_done(waitItem, -1);
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
    wld_transferStaArgs_t params;
    memset(&params, 0, sizeof(params));

    const char* mac = GET_CHAR(args, "mac");
    ASSERT_TRUE(swl_typeMacChar_fromChar(&params.sta, mac), amxd_status_invalid_function_argument,
                ME, "invalid mac argument %s", mac);
    ASSERT_TRUE(swl_mac_charIsValidStaMac(&params.sta), amxd_status_invalid_function_argument,
                ME, "invalid station mac %s", params.sta.cMac);

    amxc_var_t* operClass = amxc_var_get_key(args, "class", AMXC_VAR_FLAG_DEFAULT);
    params.operClass = (operClass == NULL) ? 0 : amxc_var_dyncast(uint8_t, operClass);

    amxc_var_t* channel = amxc_var_get_key(args, "channel", AMXC_VAR_FLAG_DEFAULT);
    params.channel = (channel == NULL) ? 0 : amxc_var_dyncast(uint8_t, channel);

    amxc_var_t* ms_wait_timeVar = amxc_var_get_key(args, "wait", AMXC_VAR_FLAG_DEFAULT);
    uint32_t ms_wait_time = (ms_wait_timeVar == NULL) ? 0 : amxc_var_dyncast(uint32_t, ms_wait_timeVar);

    amxc_var_t* retriesVar = amxc_var_get_key(args, "retries", AMXC_VAR_FLAG_DEFAULT);
    uint32_t retries = (retriesVar == NULL) ? 0 : amxc_var_dyncast(uint32_t, retriesVar);

    amxc_var_t* validity = amxc_var_get_key(args, "validity", AMXC_VAR_FLAG_DEFAULT);
    params.validity = (validity == NULL) ? 0 : amxc_var_dyncast(int32_t, validity);

    amxc_var_t* disassoc = amxc_var_get_key(args, "disassoc", AMXC_VAR_FLAG_DEFAULT);
    params.disassoc = (disassoc == NULL) ? 15 : amxc_var_dyncast(int32_t, disassoc);

    amxc_var_t* bssidInfo = amxc_var_get_key(args, "bssidInfo", AMXC_VAR_FLAG_DEFAULT);
    params.bssidInfo = (bssidInfo == NULL) ? 0 : amxc_var_dyncast(uint32_t, bssidInfo);

    amxc_var_t* transitionReason = amxc_var_get_key(args, "transitionReason", AMXC_VAR_FLAG_DEFAULT);
    params.transitionReason = (transitionReason == NULL) ? 0 : amxc_var_dyncast(int32_t, transitionReason);

    amxc_var_t* mode = amxc_var_get_key(args, "mode", AMXC_VAR_FLAG_DEFAULT);
    params.reqModeMask = (mode == NULL) ? M_SWL_IEEE802_BTM_REQ_MODE_PREF_LIST_INCL |
        M_SWL_IEEE802_BTM_REQ_MODE_ABRIDGED |
        M_SWL_IEEE802_BTM_REQ_MODE_DISASSOC_IMMINENT :
        amxc_var_dyncast(uint8_t, mode);

    const char* bssid = GET_CHAR(args, "target");
    ASSERT_TRUE(swl_typeMacChar_fromChar(&params.targetBssid, bssid), amxd_status_invalid_function_argument,
                ME, "invalid target argument %s", bssid);
    ASSERT_TRUE(swl_mac_charIsValidStaMac(&params.targetBssid), amxd_status_invalid_function_argument,
                ME, "invalid target mac %s", bssid);
    ASSERT_NOT_EQUALS(params.channel, 0, amxd_status_invalid_function_argument, ME, "invalid channel argument");
    ASSERT_NOT_EQUALS(params.operClass, 0, amxd_status_invalid_function_argument, ME, "invalid operClass argument");

    SAH_TRACEZ_NOTICE(ME, "Steer %s @ %s 2 %s/%u:%u retry %u x %u ms",
                      params.sta.cMac, pAP->alias, params.targetBssid.cMac,
                      params.operClass, params.channel, retries, ms_wait_time);

    swl_rc_ne cmdRetval = pAP->pFA->mfn_wvap_transfer_sta(pAP, &params);
    ASSERT_NOT_EQUALS(cmdRetval, SWL_RC_NOT_IMPLEMENTED, amxd_status_function_not_implemented, ME, "Function not supported");
    ASSERT_FALSE(cmdRetval < SWL_RC_OK, amxd_status_unknown_error, ME, "Error during execution");
    if(ms_wait_time > 0) {
        SAH_TRACEZ_INFO(ME, "Waiting %u", ms_wait_time);
        bss_wait_t* waitItem = calloc(1, sizeof(bss_wait_t));
        ASSERT_NOT_NULL(waitItem, amxd_status_out_of_mem, ME, "waitItem is NULL");
        waitItem->ap = pAP;
        waitItem->params = params;
        waitItem->retries = retries;
        waitItem->timeout = ms_wait_time;
        amxp_timer_new(&waitItem->timer, s_bssTransferTimeoutHandler, waitItem);
        waitItem->timer->priv = waitItem;
        amxp_timer_start(waitItem->timer, ms_wait_time);
        amxc_llist_append(&bss_wait_list, &(waitItem->it));
        amxd_function_defer(func, &waitItem->call_id, ret, NULL, NULL);
        return amxd_status_deferred;
    }
    amxc_var_init(ret);
    amxc_var_set(int32_t, ret, -1);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}
