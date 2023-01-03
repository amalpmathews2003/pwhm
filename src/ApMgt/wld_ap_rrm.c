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

#define ME "ap11k"

typedef struct {
    amxc_llist_it_t it;
    unsigned char mac[ETHER_ADDR_STR_LEN];
    int timeout;
    uint8_t token;
    amxc_var_t* result_list;
    uint64_t call_id;
    T_AccessPoint* ap;
    amxp_timer_t* timer;
} rrm_wait_t;

amxc_llist_t rrm_wait_list;

/**
 * Callback function for vendor plug-ins to indicate a given mac provided a reply.
 * Reply code should come from 802.11v request
 */
void wld_ap_rrm_item(T_AccessPoint* ap, const unsigned char* mac, amxc_var_t* result) {
    SAH_TRACEZ_INFO(ME, "rrm data %s %s", ap->alias, mac);
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &rrm_wait_list) {
        rrm_wait_t* wait = amxc_llist_it_get_data(it, rrm_wait_t, it);
        if((wait->ap == ap) && (strncasecmp((char*) mac, (char*) wait->mac, ETHER_ADDR_STR_LEN) == 0)) {
            amxc_var_add(amxc_htable_t, wait->result_list, amxc_var_get_amxc_htable_t(result));
            return;
        }
    }
}

/**
 * Timeout handler for bss transfer frame retry.
 */
static void rrm_timeout_handler(amxp_timer_t* timer _UNUSED, void* userdata) {
    ASSERT_NOT_NULL(userdata, , ME, "NULL");
    rrm_wait_t* wait_item = (rrm_wait_t*) userdata;
    amxc_llist_it_take(&wait_item->it);
    amxp_timer_delete(&wait_item->timer);
    SAH_TRACEZ_INFO(ME, "Ret %s : %zu items", wait_item->mac, amxc_llist_size(amxc_var_get_const_amxc_llist_t(wait_item->result_list)));
    amxd_function_deferred_done(wait_item->call_id, amxd_status_ok, NULL, wait_item->result_list);
    amxc_var_delete(&wait_item->result_list);
    free(wait_item);
}

amxd_status_t _sendRemoteMeasumentRequest(amxd_object_t* object,
                                          amxd_function_t* func _UNUSED,
                                          amxc_var_t* args,
                                          amxc_var_t* ret) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(pAP->IEEE80211kEnable, amxd_status_invalid_value, ME, "IEEE802.11k not enabled");

    SAH_TRACEZ_IN(ME);

    const char* macStr = GET_CHAR(args, "mac");
    ASSERTS_NOT_NULL(macStr, amxd_status_invalid_function_argument, ME, "NULL");

    const char* bssid = GET_CHAR(args, "bssid");
    const char* ssid = GET_CHAR(args, "ssid");
    if(bssid == NULL) {
        bssid = SWL_MAC_CHAR_BCAST;
    }

    amxc_var_t* class = amxc_var_get_key(args, "class", AMXC_VAR_FLAG_DEFAULT);
    uint32_t operClass = (class == NULL) ? 0 : amxc_var_dyncast(uint32_t, class);

    amxc_var_t* channelVar = amxc_var_get_key(args, "channel", AMXC_VAR_FLAG_DEFAULT);
    uint32_t channel = (channelVar == NULL) ? 0 : amxc_var_dyncast(uint32_t, channelVar);

    amxc_var_t* timeoutVar = amxc_var_get_key(args, "wait", AMXC_VAR_FLAG_DEFAULT);
    uint32_t timeout = (timeoutVar == NULL) ? 1000 : amxc_var_dyncast(uint32_t, timeoutVar);

    amxd_status_t status = amxd_status_ok;

    swl_macChar_t cBssid;
    swl_macChar_t cStation;
    swl_mac_charToStandard(&cBssid, bssid);
    swl_mac_charToStandard(&cStation, macStr);
    swl_rc_ne cmdRetval = pAP->pFA->mfn_wvap_request_rrm_report(pAP, &cStation, operClass, (swl_channel_t) channel, &cBssid, ssid);

    if(cmdRetval == SWL_RC_NOT_IMPLEMENTED) {
        SAH_TRACEZ_ERROR(ME, "Function not supported");
        status = amxd_status_function_not_implemented;
    } else if(cmdRetval < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Error during execution");
        status = amxd_status_unknown_error;
    } else if(timeout > 0) {
        SAH_TRACEZ_INFO(ME, "Waiting [%u] ms", timeout);
        rrm_wait_t* wait_item = calloc(1, sizeof(rrm_wait_t));
        ASSERT_NOT_NULL(wait_item, amxd_status_out_of_mem, ME, "NULL");

        wait_item->ap = pAP;
        wldu_copyStr((char*) wait_item->mac, macStr, ETHER_ADDR_STR_LEN);

        amxc_var_new(&wait_item->result_list);
        amxc_var_set_type(wait_item->result_list, AMXC_VAR_ID_LIST);

        amxp_timer_new(&wait_item->timer, rrm_timeout_handler, wait_item);
        wait_item->timer->priv = wait_item;
        amxd_function_defer(func, &wait_item->call_id, ret, NULL, NULL);
        amxp_timer_start(wait_item->timer, timeout);

        amxc_llist_append(&rrm_wait_list, &(wait_item->it));

        status = amxd_status_deferred;
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

