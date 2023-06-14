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

#include <stdlib.h>
#include <debug/sahtrace.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_channel.h"
#include "swl/swl_assert.h"
#include "wld_rad_stamon.h"
#include "wld_prbReq.h"

#define ME "radPrb"

typedef struct {
    amxc_llist_it_t it;

    unsigned char macStr[ETHER_ADDR_STR_LEN];
    /* In the future possibly RSSI,SNR, ... */
    int rssi;
    time_t timestamp;
} T_ProbeRequest;


const char* cstr_wld_prb_req_mode[WLD_PRB_MAX] = {
    "NoUpdate",
    "First",
    "FirstRSSI",
    "Always",
    "AlwaysRSSI"
};

static T_ProbeRequest* wld_getProbeRequest(const T_Radio* pR, const unsigned char* macStr) {
    ASSERT_NOT_NULL(pR, NULL, ME, "NULL");
    ASSERT_NOT_NULL(macStr, NULL, ME, "NULL");

    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pR->llProbeRequests) {
        T_ProbeRequest* req = amxc_llist_it_get_data(it, T_ProbeRequest, it);
        if(!memcmp(macStr, req->macStr, ETHER_ADDR_STR_LEN)) {
            return req;
        }
    }

    return NULL;
}

void wld_prb_req_populate_channelInfoMap(amxc_var_t* sta_list, const T_ProbeRequest* probe) {

    struct tm timestamp;
    gmtime_r(&probe->timestamp, &timestamp);
    amxc_ts_t tsp;
    amxc_ts_from_tm(&tsp, &timestamp);

    amxc_var_t* channelInfoMap = amxc_var_add(amxc_htable_t, sta_list, NULL);
    amxc_var_add_key(cstring_t, channelInfoMap, "MacAddress", (char*) probe->macStr);
    amxc_var_add_key(int32_t, channelInfoMap, "RSSI", probe->rssi);
    amxc_var_add_key(amxc_ts_t, channelInfoMap, "TimeStamp", &tsp);
}

/**
 * This function will insert new ProbeRequest data into the STA list before sending the notification.
 * Specific Probe object can be specified.
 */
void wld_prb_req_populate_sta_list(T_Radio* pR, time_t ref_timestamp, amxc_var_t* sta_list) {
    ASSERT_NOT_NULL(sta_list, , ME, "NULL");

    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pR->llProbeRequests) {
        T_ProbeRequest* req = amxc_llist_it_get_data(it, T_ProbeRequest, it);
        if(ref_timestamp <= req->timestamp) {
            wld_prb_req_populate_channelInfoMap(sta_list, req);
        }
    }
}

void wld_send_prob_req_notification(T_Radio* pR, amxc_var_t* sta_list) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);

    amxc_var_t* tmpMap = amxc_var_add_key(amxc_htable_t, &map, "Updates", NULL);

    amxc_ts_t ts;
    amxc_ts_now(&ts);
    amxc_var_add_key(amxc_ts_t, tmpMap, "TimeStamp", &ts);
    amxc_var_add_key(amxc_llist_t, tmpMap, "StaList", amxc_var_get_const_amxc_llist_t(sta_list));

    amxd_object_trigger_signal(pR->pBus, "ProbeRequest", &map);

    amxc_var_clean(tmpMap);
    amxc_var_clean(&map);
}

#define WLD_RSSI_STR_MAX_LEN 5

static void wld_sendProbeRequestNotify(T_Radio* pR, const T_ProbeRequest* probe) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_NULL(probe, , ME, "NULL");
    amxc_var_t sta_list;
    amxc_var_init(&sta_list);
    amxc_var_set_type(&sta_list, AMXC_VAR_ID_LIST);
    wld_prb_req_populate_channelInfoMap(&sta_list, probe);
    wld_send_prob_req_notification(pR, &sta_list);
    amxc_var_clean(&sta_list);
}

static void wld_sendAggregatedProbeRequestNotify(amxp_timer_t* timer _UNUSED, void* userdata) {
    ASSERT_NOT_NULL(userdata, , ME, "NULL");
    T_Radio* pR = (T_Radio*) userdata;
    amxc_var_t sta_list;
    amxc_var_init(&sta_list);
    amxc_var_set_type(&sta_list, AMXC_VAR_ID_LIST);
    wld_prb_req_populate_sta_list(pR, pR->probeReferenceTimestamp, &sta_list);
    wld_send_prob_req_notification(pR, &sta_list);
    amxc_var_clean(&sta_list);
}

void wld_startAggregatedProbeRequestTimer(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_TRUE(pRad->enable && pRad->status == RST_UP, , ME, "Radio not enable or radio off");
    ASSERTS_FALSE(amxp_timer_get_state(pRad->aggregationTimer) == amxp_timer_running, , ME, "Aggregation timer already running");
    if(!pRad->aggregationTimer) {
        amxp_timer_new(&pRad->aggregationTimer, wld_sendAggregatedProbeRequestNotify, pRad);
    }
    amxp_timer_start(pRad->aggregationTimer, pRad->probeRequestAggregationTime);
}

amxd_status_t _refreshProbeRequests(amxd_object_t* object,
                                    amxd_function_t* func _UNUSED,
                                    amxc_var_t* args _UNUSED,
                                    amxc_var_t* ret _UNUSED) {

    T_Radio* pR = (T_Radio*) object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    pR->pFA->mfn_wrad_update_prob_req(pR);

    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pR->llProbeRequests) {
        T_ProbeRequest* req = amxc_llist_it_get_data(it, T_ProbeRequest, it);
        wld_sendProbeRequestNotify(pR, req);
    }

    return amxd_status_ok;
}

amxd_status_t _getProbeRequests(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args,
                                amxc_var_t* ret) {
    T_Radio* pR = (T_Radio*) object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    pR->pFA->mfn_wrad_update_prob_req(pR);

    const char* fromTimeStr = GET_CHAR(args, "fromTime");

    amxc_ts_t from_time;
    time_t from_time_tm = 0;
    if(!swl_str_isEmpty(fromTimeStr)) {
        amxc_ts_parse(&from_time, fromTimeStr, strlen(fromTimeStr));
        from_time_tm = from_time.sec;
    }

    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);

    amxc_ts_t now_timestamp;
    amxc_ts_now(&now_timestamp);
    amxc_var_add_key(amxc_ts_t, &map, "TimeStamp", &now_timestamp);
    amxc_var_add_key(amxc_ts_t, &map, "FromTime", &from_time);

    amxc_var_t sta_list;
    amxc_var_init(&sta_list);
    amxc_var_set_type(&sta_list, AMXC_VAR_ID_LIST);

    wld_prb_req_populate_sta_list(pR, from_time_tm, &sta_list);
    amxc_var_add_key(amxc_llist_t, &map, "StaList", amxc_var_get_const_amxc_llist_t(&sta_list));

    amxc_var_move(ret, &map);

    amxc_var_clean(&sta_list);
    amxc_var_clean(&map);

    return amxd_status_ok;
}

void wld_notifyProbeRequest(T_Radio* pR, const unsigned char* macStr) {
    wld_notifyProbeRequest_rssi(pR, macStr, 0);
}

static void wld_notify_rssi_exist(T_Radio* pR, T_ProbeRequest* req, int rssi) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    //Put latest probe request first
    amxc_llist_it_take(&(req->it));
    amxc_llist_prepend(&pR->llProbeRequests, &(req->it));
    req->timestamp = time(NULL);

    wld_nasta_t* monDev = wld_rad_staMon_getDevice((const char*) req->macStr, &pR->monitorDev);

    if(rssi != 0) {
        int old_rssi = req->rssi;
        req->rssi = rssi;

        if(((old_rssi == 0) && (pR->probeRequestMode == WLD_PRB_FIRST_RSSI)) || (monDev != NULL)) {
            wld_sendProbeRequestNotify(pR, req);
        } else if(pR->probeRequestMode == WLD_PRB_ALWAYS_RSSI) {
            pR->probeReferenceTimestamp = req->timestamp;
            wld_startAggregatedProbeRequestTimer(pR);
        }

    } else {
        if(pR->probeRequestMode == WLD_PRB_ALWAYS) {
            wld_sendProbeRequestNotify(pR, req);
        }
    }
}

void wld_notify_rssi_new(T_Radio* pR, const unsigned char* macStr, int rssi) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_NULL(macStr, , ME, "NULL");

    T_ProbeRequest* req = calloc(1, sizeof(T_ProbeRequest));
    ASSERTS_NOT_NULL(req, , ME, "NULL");
    memcpy(req->macStr, macStr, ETHER_ADDR_STR_LEN);
    req->rssi = rssi;
    req->timestamp = time(NULL);

    /* Cache it. */
    amxc_llist_prepend(&pR->llProbeRequests, &req->it);

    /* Trim cache. */
    if(amxc_llist_size(&pR->llProbeRequests) > 50) {
        amxc_llist_it_t* it = amxc_llist_take_last(&pR->llProbeRequests);
        T_ProbeRequest* del = amxc_llist_it_get_data(it, T_ProbeRequest, it);
        free(del);
    }

    wld_nasta_t* monDev = wld_rad_staMon_getDevice((const char*) macStr, &pR->monitorDev);

    if(pR->probeRequestMode != WLD_PRB_NO_UPDATE) {
        if((rssi != 0)
           || ((monDev != NULL) && (rssi != 0))
           || ((rssi == 0) && (pR->probeRequestMode == WLD_PRB_FIRST))
           || ((rssi == 0) && (pR->probeRequestMode == WLD_PRB_ALWAYS))) {
            wld_sendProbeRequestNotify(pR, req);
        }
    }
}

void wld_notifyProbeRequest_rssi(T_Radio* pR, const unsigned char* macStr, int rssi) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_NULL(macStr, , ME, "NULL");

    T_ProbeRequest* req = wld_getProbeRequest(pR, macStr);
    if(req != 0) {
        wld_notify_rssi_exist(pR, req, rssi);
    } else {
        wld_notify_rssi_new(pR, macStr, rssi);
    }
}

amxd_status_t _wld_prbReq_setNotify_pwf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return amxd_status_ok;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NO radio Ctx");

    int new_mode = 0;
    const char* new_val = amxc_var_constcast(cstring_t, args);
    if(!findStrInArray(new_val, cstr_wld_prb_req_mode, &new_mode)) {
        SAH_TRACEZ_ERROR(ME, "Mode not found %s", new_val);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_INFO(ME, "Setting mode %s to %u", pR->Name, new_mode);
    pR->probeRequestMode = new_mode;

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_prbReq_setNotifyAggregationTimer_pwf(amxd_object_t* object _UNUSED,
                                                        amxd_param_t* parameter,
                                                        amxd_action_t reason,
                                                        const amxc_var_t* const args,
                                                        amxc_var_t* const retval,
                                                        void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return amxd_status_ok;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NO radio Ctx");

    uint32_t new_val = amxc_var_dyncast(uint32_t, args);
    if(new_val != pR->probeRequestAggregationTime) {
        SAH_TRACEZ_INFO(ME, "Setting aggreg %s to %u", pR->Name, new_val);
        pR->probeRequestAggregationTime = new_val;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

int wld_prbReq_getRssi(T_Radio* pR, const unsigned char* macStr) {
    T_ProbeRequest* req = wld_getProbeRequest(pR, macStr);
    ASSERTI_NOT_NULL(req, 0, ME, "NULL");

    return req->rssi;
}

void wld_prbReq_init(T_Radio* pRad) {
    amxc_llist_init(&pRad->llProbeRequests);
}

void wld_prbReq_destroy(T_Radio* pRad) {
    amxc_llist_it_t* probeIt = amxc_llist_get_first(&pRad->llProbeRequests);
    while(probeIt) {
        T_ProbeRequest* probe = amxc_llist_it_get_data(probeIt, T_ProbeRequest, it);
        probeIt = amxc_llist_it_get_next(probeIt);
        amxc_llist_it_take(&probe->it);
        free(probe);
        probe = NULL;
    }
    amxc_llist_clean(&pRad->llProbeRequests, NULL);
    amxp_timer_delete(&pRad->aggregationTimer);
    pRad->aggregationTimer = NULL;
}

