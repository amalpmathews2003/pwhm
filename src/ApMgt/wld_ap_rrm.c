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
    swl_macChar_t mac;
    int timeout;
    uint8_t token;
    amxc_var_t* result_list;
    uint64_t call_id;
    T_AccessPoint* ap;
    amxp_timer_t* timer;
} rrm_wait_t;

amxc_llist_t rrm_wait_list;


/**
 * Timeout handler for bss transfer frame retry.
 */
static void rrm_timeout_handler(amxp_timer_t* timer _UNUSED, void* userdata) {
    ASSERT_NOT_NULL(userdata, , ME, "NULL");
    rrm_wait_t* wait_item = (rrm_wait_t*) userdata;
    amxc_llist_it_take(&wait_item->it);
    amxp_timer_delete(&wait_item->timer);
    SAH_TRACEZ_INFO(ME, "Ret %s : %zu items", wait_item->mac.cMac, amxc_llist_size(amxc_var_get_const_amxc_llist_t(wait_item->result_list)));
    amxd_function_deferred_done(wait_item->call_id, amxd_status_ok, NULL, wait_item->result_list);
    amxc_var_delete(&wait_item->result_list);
    free(wait_item);
}

/**
 * Callback function for vendor plug-ins to indicate a given mac provided a reply.
 * Reply code should come from 802.11v request
 */
void wld_ap_rrm_item(T_AccessPoint* ap, const swl_macChar_t* mac, amxc_var_t* result) {
    SAH_TRACEZ_INFO(ME, "rrm data %s %s", ap->alias, mac->cMac);

    amxc_llist_for_each(it, &rrm_wait_list) {
        rrm_wait_t* wait = amxc_llist_it_get_data(it, rrm_wait_t, it);
        if((wait->ap == ap) && swl_mac_charMatches(mac, &wait->mac)) {
            if(result) {
                amxc_var_add(amxc_htable_t, wait->result_list, amxc_var_get_amxc_htable_t(result));
                amxp_timer_start(wait->timer, 300);
                return;
            } else {
                rrm_timeout_handler(wait->timer, wait);
            }
        }
    }
}

#define BEACON_MEASURE_DEFAULT_MEAS_MODE SWL_IEEE802_RRM_BEACON_REQ_MODE_PASSIVE
#define BEACON_MEASURE_DEFAULT_TIMEOUT 1000
static const uint16_t s_defaultMeasDurationByMode[SWL_IEEE802_RRM_BEACON_REQ_MODE_MAX] = {50, 120, 120};
SWL_TABLE(sRrmBeaconCapModeMap,
          ARR(uint32_t swlRrmBeaconCap; uint32_t swlRrmBeaconMode; ),
          ARR(swl_type_uint32, swl_type_uint32),
          ARR({SWL_STACAP_RRM_BEACON_ACTIVE_ME, SWL_IEEE802_RRM_BEACON_REQ_MODE_ACTIVE},
              {SWL_STACAP_RRM_BEACON_PASSIVE_ME, SWL_IEEE802_RRM_BEACON_REQ_MODE_PASSIVE},
              {SWL_STACAP_RRM_BEACON_TABLE_ME, SWL_IEEE802_RRM_BEACON_REQ_MODE_TABLE})
          );
amxd_status_t _sendRemoteMeasumentRequest(amxd_object_t* object,
                                          amxd_function_t* func _UNUSED,
                                          amxc_var_t* args,
                                          amxc_var_t* ret) {
    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");
    const char* macStr = GET_CHAR(args, "mac");
    swl_macChar_t cStation;
    ASSERT_TRUE(swl_mac_charToStandard(&cStation, macStr), amxd_status_invalid_function_argument, ME, "invalid mac (%s)", macStr);
    ASSERT_TRUE(swl_mac_charIsValidStaMac(&cStation), amxd_status_invalid_function_argument, ME, "invalid sta mac (%s)", macStr);
    ASSERT_TRUE(pAP->IEEE80211kEnable, amxd_status_invalid_value, ME, "IEEE802.11k not enabled");

    SAH_TRACEZ_IN(ME);

    wld_rrmReq_t reqCall;
    memset(&reqCall, 0, sizeof(reqCall));

    const char* bssid = GET_CHAR(args, "bssid");
    const char* ssid = GET_CHAR(args, "ssid");
    if(bssid == NULL) {
        bssid = SWL_MAC_CHAR_BCAST;
    }

    swl_mac_charToStandard(&reqCall.bssid, bssid);
    swl_str_copy(reqCall.ssid, SSID_NAME_LEN, ssid);

    amxc_var_t* tpVar = amxc_var_get_key(args, "class", AMXC_VAR_FLAG_DEFAULT);
    reqCall.operClass = (tpVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, tpVar);

    tpVar = amxc_var_get_key(args, "channel", AMXC_VAR_FLAG_DEFAULT);
    reqCall.channel = (tpVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, tpVar);

    tpVar = amxc_var_get_key(args, "timeout", AMXC_VAR_FLAG_DEFAULT);
    uint32_t timeout = (tpVar == NULL) ? BEACON_MEASURE_DEFAULT_TIMEOUT : amxc_var_dyncast(uint32_t, tpVar);

    tpVar = amxc_var_get_key(args, "mode", AMXC_VAR_FLAG_DEFAULT);
    if((tpVar == NULL) || ((reqCall.mode = amxc_var_dyncast(uint8_t, tpVar)) >= SWL_IEEE802_RRM_BEACON_REQ_MODE_MAX)) {
        //as mode arg is optional: explicitly use default request mode when argument is missing or unknown
        SAH_TRACEZ_WARNING(ME, "%s: assume default rrm beacon meas mode %d because missing/invalid user conf(%d)", pAP->alias, BEACON_MEASURE_DEFAULT_MEAS_MODE, reqCall.mode);
        reqCall.mode = BEACON_MEASURE_DEFAULT_MEAS_MODE;
    }

    swl_macBin_t macStaBin;
    swl_mac_charToBin(&macStaBin, &cStation);
    T_AssociatedDevice* pAD = wld_vap_get_existing_station(pAP, &macStaBin);
    if(pAD != NULL) {
        swl_mask_m rrmBeaconCaps = pAD->assocCaps.rrmCapabilities & (M_SWL_STACAP_RRM_BEACON_ACTIVE_ME | M_SWL_STACAP_RRM_BEACON_PASSIVE_ME | M_SWL_STACAP_RRM_BEACON_TABLE_ME);
        swl_staCapRrmCap_e* rrmCap = (swl_staCapRrmCap_e*) swl_table_getMatchingValue(&sRrmBeaconCapModeMap, 0, 1, &reqCall.mode);
        if((rrmCap != NULL) && (!SWL_BIT_IS_SET(pAD->assocCaps.rrmCapabilities, *rrmCap))) {
            int32_t lPos = swl_bit32_getLowest(rrmBeaconCaps);
            swl_ieee802_rrmBeaconReqMode_e* mode = (swl_ieee802_rrmBeaconReqMode_e*) swl_table_getMatchingValue(&sRrmBeaconCapModeMap, 1, 0, &lPos);
            if(mode == NULL) {
                SAH_TRACEZ_WARNING(ME, "%s: sta %s caps does not include any rrm beacon mode: keep set %d", pAP->alias, pAD->Name, reqCall.mode);
            } else {
                SAH_TRACEZ_WARNING(ME, "%s: rectify rrm beacon mode of sta %s with supported mode %d (isof %d)", pAP->alias, pAD->Name, *mode, reqCall.mode);
                reqCall.mode = *mode;
            }
        }
    }

    tpVar = amxc_var_get_key(args, "modeMask", AMXC_VAR_FLAG_DEFAULT);
    reqCall.modeMask = (tpVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, tpVar);

    tpVar = amxc_var_get_key(args, "token", AMXC_VAR_FLAG_DEFAULT);
    reqCall.token = (tpVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, tpVar);

    tpVar = amxc_var_get_key(args, "duration", AMXC_VAR_FLAG_DEFAULT);
    if((tpVar == NULL) || ((reqCall.duration = amxc_var_dyncast(uint16_t, tpVar)) == 0)) {
        //as duration arg is optional: explicitly use default duration (by mode) when argument is missing or null
        //Most stations reject RRM Beacon request when duration is undefined
        reqCall.duration = s_defaultMeasDurationByMode[reqCall.mode];
        SAH_TRACEZ_WARNING(ME, "%s: assume default meas duration(%d) for mode(%d)", pAP->alias, reqCall.duration, reqCall.mode);
    }

    tpVar = amxc_var_get_key(args, "interval", AMXC_VAR_FLAG_DEFAULT);
    reqCall.interval = (tpVar == NULL) ? 0 : amxc_var_dyncast(uint16_t, tpVar);

    tpVar = amxc_var_get_key(args, "neighbor", AMXC_VAR_FLAG_DEFAULT);
    reqCall.addNeighbor = (tpVar == NULL) ? true : amxc_var_dyncast(bool, tpVar);

    const char* optionalElem = GET_CHAR(args, "optionalElements");
    if(swl_hex_isHexChar(optionalElem, swl_str_len(optionalElem))) {
        swl_str_copyMalloc(&reqCall.optionalEltHexStr, optionalElem);
    } else if(!swl_str_isEmpty(optionalElem)) {
        SAH_TRACEZ_ERROR(ME, "invalid optElt hex string(%s)", optionalElem);
    }

    amxd_status_t status = amxd_status_ok;

    swl_rc_ne cmdRetval = pAP->pFA->mfn_wvap_request_rrm_report(pAP, &cStation, &reqCall);

    W_SWL_FREE(reqCall.optionalEltHexStr);

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
        swl_mac_charToStandard(&wait_item->mac, macStr);

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

