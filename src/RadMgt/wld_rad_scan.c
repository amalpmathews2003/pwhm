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

#include "wld_common.h"

#define ME "wldScan"

#define SCAN_DELAY 60000
uint64_t g_neighboringWiFiDiagnosticID = 0;
bool g_neighboringWiFiDiagnosticStatus = false;

const char* g_str_wld_radScanStatus[WLD_RAD_SCAN_STATUS_MAX] = {
    "ReportingRadios",
    "FailedRadios",
    "DisabledRadios",
};

const char* g_str_wld_blockScanMode[BLOCKSCANMODE_MAX] = {
    "Disable",
    "Prio",
    "All",
};

void wld_scan_stat_initialize(T_Radio* pRad, const char* scanReason) {
    amxd_object_t* scanStatsTemp = amxd_object_get(pRad->pBus, "ScanStats.ScanReason");
    ASSERTS_NOT_NULL(scanStatsTemp, , ME, "ScanReason Obj NULL");
    amxd_object_t* obj = amxd_object_get(scanStatsTemp, scanReason);
    if(obj == NULL) {
        amxd_object_new_instance(&obj, scanStatsTemp, scanReason, 0, NULL);
        ASSERTS_NOT_NULL(obj, , ME, "Failed to create object: %s", scanReason);
    }
}

amxd_status_t _wld_rad_writeScanConfig(amxd_object_t* object) {
    amxd_object_t* radObject = amxd_object_get_parent(object);
    ASSERTI_FALSE(amxd_object_get_type(radObject) == amxd_object_template, amxd_status_unknown_error, ME, "Initial template run, skip");

    T_Radio* pRad = (T_Radio*) radObject->priv;
    ASSERTI_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "Radio object is NULL");
    SAH_TRACEZ_INFO(ME, "%s update scan config", pRad->Name);

    pRad->scanState.cfg.homeTime = amxd_object_get_int32_t(object, "HomeTime", NULL);
    pRad->scanState.cfg.activeChannelTime = amxd_object_get_int32_t(object, "ActiveChannelTime", NULL);
    pRad->scanState.cfg.passiveChannelTime = amxd_object_get_int32_t(object, "PassiveChannelTime", NULL);
    pRad->scanState.cfg.blockScanMode = swl_conv_charToEnum(amxd_object_get_cstring_t(object, "BlockScanMode", NULL), g_str_wld_blockScanMode, BLOCKSCANMODE_MAX, BLOCKSCANMODE_DISABLE);
    pRad->scanState.cfg.maxChannelsPerScan = amxd_object_get_int32_t(object, "MaxChannelsPerScan", NULL);
    pRad->scanState.cfg.scanRequestInterval = amxd_object_get_int32_t(object, "ScanRequestInterval", NULL);
    pRad->scanState.cfg.scanChannelCount = amxd_object_get_int32_t(object, "ScanChannelCount", NULL);

    if(pRad->scanState.cfg.fastScanReasons != NULL) {
        free(pRad->scanState.cfg.fastScanReasons);
        pRad->scanState.cfg.fastScanReasons = NULL;
    }
    pRad->scanState.cfg.fastScanReasons = strdup(amxd_object_get_cstring_t(object, "FastScanReasons", NULL));
    return amxd_status_ok;
}

void wld_radio_update_ScanStats(T_Radio* pRad) {
    amxd_object_t* objScanStats = amxd_object_findf(pRad->pBus, "ScanStats");
    ASSERTS_NOT_NULL(objScanStats, , ME, "ScanStats Obj NULL");
    amxd_object_set_uint32_t(objScanStats, "NrScanRequested", pRad->scanState.stats.nrScanRequested);
    amxd_object_set_uint32_t(objScanStats, "NrScanDone", pRad->scanState.stats.nrScanDone);
    amxd_object_set_uint32_t(objScanStats, "NrScanError", pRad->scanState.stats.nrScanError);
    amxd_object_set_uint32_t(objScanStats, "NrScanBlocked", pRad->scanState.stats.nrScanBlocked);

    amxd_object_t* obj = amxd_object_findf(pRad->pBus, "ScanStats.ScanReason");
    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_brief_stats_t* stats = amxc_llist_it_get_data(it, wld_brief_stats_t, it);
        amxd_object_t* objExtendedStats = amxd_object_findf(obj, "%s", stats->scanReason);
        amxd_object_set_cstring_t(objExtendedStats, "Name", stats->scanReason);
        amxd_object_set_uint32_t(objExtendedStats, "NrScanRequested", stats->totalCount);
        amxd_object_set_uint32_t(objExtendedStats, "NrScanDone", stats->successCount);
    }
}

void wld_radio_copySsidsToResult(T_ScanResults* results, amxc_llist_t* ssid_list) {
    ASSERTS_NOT_NULL(results, , ME, "NULL");
    ASSERTS_NOT_NULL(ssid_list, , ME, "NULL");
    amxc_llist_it_t* item = NULL;
    amxc_llist_for_each(item, ssid_list) {
        T_ScanResult_SSID* cur_ssid = amxc_llist_it_get_data(item, T_ScanResult_SSID, it);
        T_ScanResult_SSID* ssid_new = calloc(1, sizeof(T_ScanResult_SSID));
        ASSERTS_NOT_NULL(ssid_new, , ME, "NULL");

        memcpy(ssid_new, cur_ssid, sizeof(T_ScanResult_SSID));
        amxc_llist_it_init(&ssid_new->it);
        amxc_llist_append(&results->ssids, &ssid_new->it);
    }
}

bool wld_radio_scanresults_cleanup(T_ScanResults* results) {

    SAH_TRACEZ_INFO(ME, "Cleanup scanresults");

    amxc_llist_it_t* it = amxc_llist_get_first(&results->ssids);
    while(it != NULL) {
        T_ScanResult_SSID* SR_ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);
        amxc_llist_it_take(&SR_ssid->it);
        free(SR_ssid);
        SR_ssid = NULL;
    }

    return true;
}

bool wld_radio_scanresults_find(T_Radio* pR, const char* ssid, T_ScanResult_SSID* output) {
    ASSERT_NOT_NULL(pR, -1, ME, "EP NULL");
    ASSERT_NOT_NULL(ssid, -1, ME, "EP NULL");

    T_ScanResults res;
    char ssid_str[SSID_NAME_LEN];
    memset(ssid_str, 0, SSID_NAME_LEN);
    bool isEntryFound = false;

    amxc_llist_init(&res.ssids);

    /* get vendor specific scan results */
    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "failed to get results");
        return false;
    }

    amxc_llist_it_t* it = amxc_llist_get_first(&res.ssids);
    while(it != NULL) {
        T_ScanResult_SSID* SR_ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);

        convSsid2Str(SR_ssid->ssid, SR_ssid->ssid_len, ssid_str);
        SAH_TRACEZ_INFO(ME, "searching for matching ssid : [%s] <-> [%s]", ssid, ssid_str);

        if(!strncmp(ssid, ssid_str, strlen(ssid))) {
            SAH_TRACEZ_INFO(ME, "Found matching ssid");
            memcpy(output, SR_ssid, sizeof(T_ScanResult_SSID));
            isEntryFound = true;
            break;
        }
        amxc_llist_it_take(&SR_ssid->it);
    }

    wld_radio_scanresults_cleanup(&res);

    return isEntryFound;
}

wld_brief_stats_t* wld_getScanStatsByReason(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_brief_stats_t* stats = amxc_llist_it_get_data(it, wld_brief_stats_t, it);
        if(swl_str_matches(stats->scanReason, reason)) {
            return stats;
        }
    }
    return NULL;
}

static wld_brief_stats_t* s_getOrCreateStats(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");
    wld_brief_stats_t* stats = wld_getScanStatsByReason(pRad, reason);
    if(stats == NULL) {
        amxc_llist_t* statList = &(pRad->scanState.stats.extendedStat);
        stats = calloc(1, sizeof(wld_brief_stats_t));
        if(stats != NULL) {
            swl_str_copyMalloc(&(stats->scanReason), reason);
            SAH_TRACEZ_INFO(ME, "ScanReason %s added", stats->scanReason);
            amxc_llist_append(statList, &stats->it);
        }
    }
    return stats;
}

void wld_notifyStartScan(T_Radio* pRad, wld_scan_type_e type, const char* scanReason) {
    SAH_TRACEZ_INFO(ME, "%s: starting scan %s", pRad->Name, scanReason);

    wld_scanEvent_t ev;
    ev.pRad = pRad;
    ev.start = true;
    ev.scanType = type;
    wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

    pRad->scanState.scanType = type;
    pRad->scanState.stats.nrScanRequested++;
    pRad->scanState.lastScanTime = swl_time_getMonoSec();

    swl_str_copy(pRad->scanState.scanReason, sizeof(pRad->scanState.scanReason), scanReason);
    wld_brief_stats_t* stats = s_getOrCreateStats(pRad, scanReason);
    ASSERTS_NOT_NULL(stats, , ME, "NULL");
    stats->totalCount++;
    wld_scan_stat_initialize(pRad, scanReason);
    wld_radio_update_ScanStats(pRad);
}

static amxd_status_t _startScan(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args,
                                amxc_var_t* ret _UNUSED) {
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");
    char* reason = NULL;

    if(wld_scan_isRunning(pR)) {
        SAH_TRACEZ_ERROR(ME, "A scan is already running");
        goto error;
    }

    const char* ssid = GET_CHAR(args, "SSID");
    const char* channels = GET_CHAR(args, "channels");
    const char* scanReason = GET_CHAR(args, "scanReason");

    int error = WLD_OK;
    bool extendedScanRequired = false;
    T_ScanArgs scanArgs;
    size_t len = 0;

    memset(&scanArgs, 0, sizeof(T_ScanArgs));

    if(channels != NULL) {
        // chanlist must have at most nrPossible elements,
        // otherwise there will be elements that are not part of possible channels.
        len = swl_type_arrayFromChar(swl_type_uint8, scanArgs.chanlist, WLD_MAX_POSSIBLE_CHANNELS, channels);
        if(!wld_rad_isChannelSubset(pR, scanArgs.chanlist, len) || (len == 0)) {
            SAH_TRACEZ_ERROR(ME, "%s: Invalid Channel list", pR->Name);
            goto error;
        }
        scanArgs.chanCount = len;
        extendedScanRequired = true;
    }

    if(ssid != NULL) {
        len = strlen(ssid);
        if(len >= SSID_NAME_LEN) {
            SAH_TRACEZ_ERROR(ME, "SSID too large");
            goto error;
        }

        swl_str_copy(scanArgs.ssid, SSID_NAME_LEN, ssid);
        scanArgs.ssid_len = len;
        extendedScanRequired = true;
    }
    wld_scan_type_e type = SCAN_TYPE_INTERNAL;
    if(scanReason == NULL) {
        type = SCAN_TYPE_SSID;
        reason = strdup("Ssid");
    } else {
        reason = strdup(scanReason);
    }

    bool updateUsageData = GET_BOOL(args, "updateUsage");
    if(updateUsageData) {
        scanArgs.updateUsageStats = true;
        extendedScanRequired = true;
    }

    bool hasForceFast = GET_BOOL(args, "forceFast");
    if(!hasForceFast) {
        scanArgs.fastScan = strstr(pR->scanState.cfg.fastScanReasons, reason) != NULL;
    }

    if(scanArgs.fastScan) {
        if(wld_functionTable_hasVendorScanExt(pR)) {
            extendedScanRequired = true;
        }
    }

    if((pR->scanState.cfg.blockScanMode == BLOCKSCANMODE_ALL)
       || ( pR->scanState.cfg.blockScanMode == BLOCKSCANMODE_PRIO)) {
        SAH_TRACEZ_ERROR(ME, "Scan blocked");
        pR->scanState.stats.nrScanBlocked++;
        wld_radio_update_ScanStats(pR);
        free(reason);
        return amxd_status_unknown_error;
    }

    if(extendedScanRequired) {
        scanArgs.reason = reason;
        error = pR->pFA->mfn_wrad_start_scan_ext(pR, &scanArgs);
    } else {
        error = pR->pFA->mfn_wrad_start_scan(pR);
    }

    if(error < 0) {
        goto error;
    } else {
        wld_notifyStartScan(pR, type, reason);
        free(reason);
        return amxd_status_deferred;
    }

error:
    free(reason);
    pR->scanState.stats.nrScanError++;
    wld_radio_update_ScanStats(pR);
    return amxd_status_unknown_error;
}

amxd_status_t _Radio_startScan(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {

    amxd_status_t res = _startScan(object, func, args, retval);

    if(res == amxd_status_deferred) {
        return amxd_status_ok;
    } else {
        return res;
    }
}

amxd_status_t _stopScan(amxd_object_t* object,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* ret _UNUSED) {
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    if(!wld_scan_isRunning(pR)) {
        SAH_TRACEZ_ERROR(ME, "Scan not running");
        return amxd_status_unknown_error;
    }

    if(pR->pFA->mfn_wrad_stop_scan(pR) < 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to stop scan");
        return amxd_status_unknown_error;
    }

    wld_scan_done(pR, false);

    return amxd_status_ok;
}

/*
   static void scan_request_cancel_handler(function_call_t* fcall, void* userdata _UNUSED) {

    SAH_TRACEZ_INFO(ME, "Scan call cancelled");
    T_Radio* pR = fcall_userData(fcall);
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    pR->scanState.fcall = NULL;
    pR->scanState.min_rssi = INT32_MIN;
   }
 */

amxd_status_t _scan(amxd_object_t* object,
                    amxd_function_t* func _UNUSED,
                    amxc_var_t* args,
                    amxc_var_t* retval) {

    T_Radio* pR = object->priv;
    uint64_t call_id = amxc_var_dyncast(uint64_t, retval);

    amxd_status_t res = _startScan(object, func, args, retval);
    if(res != amxd_status_deferred) {
        return res;
    }

    int32_t min_rssi = GET_INT32(args, "min_rssi");
    pR->scanState.min_rssi = min_rssi;
    pR->scanState.call_id = call_id;

    return amxd_status_deferred;
}

static bool wld_get_scan_results(T_Radio* pR, amxc_var_t* retval, int32_t min_rssi) {
    T_ScanResults res;

    amxc_llist_init(&res.ssids);

    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to retrieve scan results");
        return false;
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    amxc_llist_it_t* it = amxc_llist_get_first(&res.ssids);
    while(it != NULL) {
        T_ScanResult_SSID* ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);

        if(ssid->rssi < min_rssi) {
            amxc_llist_it_take(&ssid->it);
            free(ssid);
            continue;
        }

        amxc_var_t entry;
        amxc_var_init(&entry);

        amxc_string_t wpsmethods;
        amxc_string_init(&wpsmethods, 0);

        char bssid[18];
        mac2str(bssid, ssid->bssid, sizeof(bssid));

        char* ssid_str = wld_ssid_to_string(ssid->ssid, ssid->ssid_len);
        amxc_var_add_key(cstring_t, &entry, "SSID", ssid_str);

        free(ssid_str);
        amxc_var_add_key(cstring_t, &entry, "BSSID", bssid);
        amxc_var_add_key(int32_t, &entry, "SignalNoiseRatio", ssid->snr);
        amxc_var_add_key(int32_t, &entry, "Noise", ssid->noise);
        amxc_var_add_key(int32_t, &entry, "RSSI", ssid->rssi);
        amxc_var_add_key(int32_t, &entry, "Channel", ssid->channel);
        amxc_var_add_key(int32_t, &entry, "CentreChannel", ssid->centre_channel);
        amxc_var_add_key(int32_t, &entry, "Bandwidth", ssid->bandwidth);
        amxc_var_add_key(int32_t, &entry, "SignalStrength", ssid->signalStrength);

        amxc_var_add_key(cstring_t, &entry, "SecurityModeEnabled", cstr_AP_ModesSupported[ssid->secModeEnabled]);

        wld_wps_ConfigMethods_mask_to_string(&wpsmethods, ssid->WPS_ConfigMethodsEnabled);
        amxc_var_add_key(cstring_t, &entry, "WPSConfigMethodsSupported", amxc_string_get(&wpsmethods, 0));
        amxc_string_clean(&wpsmethods);

        amxc_var_add_key(cstring_t, &entry, "EncryptionMode", cstr_AP_EncryptionMode[ssid->encryptionMode]);

        char operatingStandardsChar[32] = "";
        swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar),
                          ssid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
        amxc_var_add_key(cstring_t, &entry, "OperatingStandards", operatingStandardsChar);

        amxc_var_move(retval, &entry);
        amxc_var_clean(&entry);

        amxc_llist_it_take(&ssid->it);
        free(ssid);
    }

    return true;
}

amxd_status_t _getScanResults(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval _UNUSED) {

    T_Radio* pR = object->priv;

    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    int32_t min_rssi = GET_INT32(args, "min_rssi");

    if(!wld_get_scan_results(pR, retval, min_rssi)) {
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

static void wld_scan_done_notification(T_Radio* pR, bool success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    char* path = amxd_object_get_path(pR->pBus, AMXD_OBJECT_NAMED);
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &ret, "Result", success ? "done" : "error");
    SAH_TRACEZ_INFO(ME, "%s: notif scan done @ path=%s %s, Result=%s}", pR->Name, path, "ScanComplete", success ? "done" : "error");
    amxd_object_trigger_signal(pR->pBus, "ScanComplete", &ret);
    amxc_var_clean(&ret);
    free(path);
}

static void wld_scan_done_finish_fcall(T_Radio* pR, bool success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_EQUALS(pR->scanState.call_id, 0, , ME, "%s no call_id", pR->Name);
    ASSERT_TRUE(wld_scan_isRunning(pR), , ME, "%s no scan", pR->Name);

    amxc_var_t ret;
    amxc_var_init(&ret);

    if(pR->scanState.scanType == SCAN_TYPE_SSID) {
        if(!wld_get_scan_results(pR, &ret, pR->scanState.min_rssi)) {
            success = false;
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "%s scan type unknown %s", pR->Name, pR->scanState.scanReason);
        success = false;
    }

    amxd_function_deferred_done(pR->scanState.call_id, success ? amxd_status_ok : amxd_status_unknown_error, NULL, &ret);

    amxc_var_clean(&ret);
    pR->scanState.call_id = 0;
    pR->scanState.min_rssi = INT32_MIN;
}

/**
 * Look for a channel object withing the ScanResults object.
 * If the requested channel object does not exist, a new channel object is created.
 * Returns null if the channel is 0.
 *
 * @param obj_scan
 *  the object of the scan results of the given radio
 * @param channel
 *  the channel for which we want the channel object
 * @return
 *  if the channel object existed in the data model, we return that channel
 *  if the channel is zero, we return null
 *  otherwise we create a new channel object.
 */
amxd_object_t* wld_scan_update_get_channel(amxd_object_t* obj_scan, uint16_t channel) {
    if(channel == 0) {
        return NULL;
    }

    char name[6];
    amxd_object_t* chan_template = amxd_object_get(obj_scan, "SurroundingChannels");
    snprintf(name, sizeof(name), "%u", channel);
    amxd_object_t* chan_obj = amxd_object_get(chan_template, name);

    if(chan_obj != NULL) {
        return chan_obj;
    } else {
        amxd_object_new_instance(&chan_obj, chan_template, name, 0, NULL);
        amxd_object_set_uint16_t(chan_obj, "Channel", channel);
        return chan_obj;
    }

}

swl_rc_ne wld_scan_updateChanimInfo(T_Radio* pRad) {

    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");

    int retVal = pRad->pFA->mfn_wrad_update_chaninfo(pRad);
    return retVal;
}


amxd_status_t _getSpectrumInfo(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pR = object->priv;

    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");
    bool update = GET_BOOL(args, "update");

    uint64_t call_id = amxc_var_dyncast(uint64_t, retval);
    amxd_status_t retVal = pR->pFA->mfn_wrad_getspectruminfo(pR, update, call_id, retval);
    if(retVal != amxd_status_deferred) {
        return retVal;
    }

    wld_notifyStartScan(pR, SCAN_TYPE_SPECTRUM, "Spectrum");

    pR->scanState.call_id = call_id;
    return amxd_status_deferred;
}


/**
 * Returns the amount of characters on which two character arrays differ, over the given length.
 */
static uint16_t nr_str_diff(const char* test1, const char* test2, uint16_t len) {
    uint16_t diff = 0;
    uint16_t i = 0;
    for(i = 0; i < len; i++) {
        if(*test1 != *test2) {
            diff++;
        }
        test1++;
        test2++;
    }

    return diff;
}

/**
 * Search for an access point object that matches the given bssid and the given rssi, withing the current
 * ap_template object. If no matching ap currently exists, null is returned.
 *
 * An accesspoint matches if the differences in bssid strings is less or equal to two (about 1 byte difference)
 * and the rssi matches. Since we look only at a given ap_template, the channel also must match.
 */
static amxd_object_t* wld_scan_find_ap(amxd_object_t* ap_template, char bssid[], int16_t rssi) {
    amxc_llist_it_t* it = amxd_object_first_instance(ap_template);
    amxd_object_t* test_ap = amxc_llist_it_get_data(it, amxd_object_t, it);
    int16_t test_rssi;
    const char* test_bssid;
    while(test_ap != NULL) {
        test_rssi = amxd_object_get_int16_t(test_ap, "RSSI", NULL);
        test_bssid = amxd_object_get_cstring_t(test_ap, "BSSID", NULL);
        if((test_rssi == rssi) && (nr_str_diff(test_bssid, bssid, 18) <= 2)) {
            return test_ap;
        } else {
            it = amxc_llist_it_get_next(it);
            test_ap = amxc_llist_it_get_data(it, amxd_object_t, it);
        }
    }
    return NULL;
}

/**
 * Add a given SSID result to the given channel.
 */
static void wld_scan_update_add_ssid(amxd_object_t* channel, T_ScanResult_SSID* ssid) {
    amxd_object_t* ap_template = amxd_object_get(channel, "Accesspoint");

    char bssid[19];
    mac2str(bssid, ssid->bssid, sizeof(bssid));

    amxd_object_t* ap_val = wld_scan_find_ap(ap_template, bssid, ssid->rssi);

    if(ap_val == NULL) {
        char id[4];
        snprintf(id, 4, "%u", amxd_object_get_instance_count(ap_template) + 1);
        amxd_object_new_instance(&ap_val, ap_template, id, 0, NULL);
        amxd_object_set_int16_t(ap_val, "RSSI", ssid->rssi);
        amxd_object_set_cstring_t(ap_val, "BSSID", bssid);

    }
    char* ssid_str = wld_ssid_to_string(ssid->ssid, ssid->ssid_len);
    amxd_object_t* ap_ssid_template = amxd_object_get(ap_val, "SSID");
    amxd_object_t* ap_ssid;
    amxd_object_new_instance(&ap_ssid, ap_ssid_template, bssid, 0, NULL);
    amxd_object_set_cstring_t(ap_ssid, "SSID", ssid_str);
    amxd_object_set_cstring_t(ap_ssid, "BSSID", bssid);
    amxd_object_set_uint16_t(ap_ssid, "Bandwidth", ssid->bandwidth);
    free(ssid_str);
}

/**
 * Clear the scan results. Should be called before we start adding new
 * results for the latest scan.
 */
static void wld_scan_update_clear_obj(amxd_object_t* obj_scan) {
    amxd_object_t* chan_template = amxd_object_get(obj_scan, "SurroundingChannels");
    amxc_llist_it_t* it = amxd_object_first_instance(chan_template);
    amxd_object_t* channel_obj = amxc_llist_it_get_data(it, amxd_object_t, it);
    while(channel_obj != NULL) {
        amxd_object_delete(&channel_obj);
        amxc_llist_it_t* it = amxd_object_first_instance(chan_template);
        channel_obj = amxc_llist_it_get_data(it, amxd_object_t, it);
    }
}

/**
 * Count the number of access points that are on the same channel as the radio currently is.
 */
static void wld_scan_count_cochannel(T_Radio* pR, amxd_object_t* obj_scan) {
    uint16_t channel = amxd_object_get_uint16_t(pR->pBus, "Channel", NULL);

    char name[5];
    amxd_object_t* chan_template = amxd_object_get(obj_scan, "SurroundingChannels");
    snprintf(name, sizeof(name), "%u", (uint8_t) channel);
    amxd_object_t* chan_obj = amxd_object_get(chan_template, name);

    if(chan_obj != NULL) {
        amxd_object_t* ap_template = amxd_object_get(chan_obj, "Accesspoint");
        uint32_t nr_cochannel = amxd_object_get_instance_count(ap_template);
        amxd_object_set_uint16_t(obj_scan, "NrCoChannelAP", nr_cochannel);
    } else {
        amxd_object_set_uint16_t(obj_scan, "NrCoChannelAP", 0);
    }
}

void wld_scan_cleanupScanResultSSID(T_ScanResult_SSID* ssid) {
    amxc_llist_it_take(&ssid->it);
    free(ssid);
}


void wld_scan_cleanupScanResults(T_ScanResults* res) {
    amxc_llist_it_t* it = amxc_llist_get_first(&res->ssids);
    while(it != NULL) {
        T_ScanResult_SSID* ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);
        wld_scan_cleanupScanResultSSID(ssid);
    }
}

/**
 * Request to update the scan results in the datamodel.
 * This will remove the currently stored scan results, and replace the with the results from the latest scan.
 * It will just retrieve the latest results, it will NOT perform a scan itself.
 */
static void wld_scan_update_obj_results(T_Radio* pR) {
    T_ScanResults res;

    amxc_llist_init(&res.ssids);

    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "failed to get results");
        return;
    }
    amxd_object_t* obj_scan = amxd_object_get(pR->pBus, "ScanResults");

    wld_scan_update_clear_obj(obj_scan);
    amxc_llist_it_t* it = amxc_llist_get_first(&res.ssids);
    while(it != NULL) {
        T_ScanResult_SSID* ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);

        amxd_object_t* channel = wld_scan_update_get_channel(obj_scan, ssid->channel);
        wld_scan_update_add_ssid(channel, ssid);
        wld_scan_cleanupScanResultSSID(ssid);
    }
    wld_scan_count_cochannel(pR, obj_scan);
}

/**
 * This function should be called by the plug-in when it receives the notification that a scan is
 * completed.
 */
void wld_scan_done(T_Radio* pR, bool success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s : scan done %u Reason %s => run %u, call_id %" PRIu64,
                    pR->Name, success, pR->scanState.scanReason, wld_scan_isRunning(pR), pR->scanState.call_id);

    if(!wld_scan_isRunning(pR)) {
        return;
    }

    pR->scanState.stats.nrScanDone++;

    wld_brief_stats_t* stats = wld_getScanStatsByReason(pR, pR->scanState.scanReason);
    if(stats != NULL) {
        stats->successCount++;
    }
    wld_radio_update_ScanStats(pR);

    if(success && (pR->scanState.scanType == SCAN_TYPE_SSID)) {
        wld_scan_update_obj_results(pR);
    }

    if(pR->scanState.call_id == 0) {
        wld_scan_done_notification(pR, success);
    } else {
        wld_scan_done_finish_fcall(pR, success);
    }

    wld_scanEvent_t ev;
    ev.pRad = pR;
    ev.start = false;
    ev.success = success;
    ev.scanType = pR->scanState.scanType;
    wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

    /* Scan is done */
    swl_str_copy(pR->scanState.scanReason, sizeof(pR->scanState.scanReason), "None");
    pR->scanState.scanType = SCAN_TYPE_NONE;

    if(success) {
        pR->radScanStatus = WLD_RAD_SCAN_STATUS_COMPLETED;
    } else {
        pR->radScanStatus = WLD_RAD_SCAN_STATUS_FAILED;
    }

    /* Notify scan done*/
    wld_scan_diagNotifyRadDone(success);
}

bool wld_scan_isRunning(T_Radio* pR) {
    return pR->scanState.scanType != SCAN_TYPE_NONE;
}

bool wld_isAnyRadioRunningScan() {
    bool scanRunning = false;

    T_Radio* pRadio = NULL;
    wld_for_eachRad(pRadio) {
        if(pRadio->scanState.scanType != SCAN_TYPE_NONE) {
            scanRunning = true;
        }
    }
    return scanRunning;
}

static void s_addDiagSingleResultToMap(amxc_var_t* pResultListMap, T_Radio* pRad, T_ScanResult_SSID* pSsid) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pSsid, , ME, "NULL");

    amxc_var_t* resulMap = amxc_var_add(amxc_htable_t, pResultListMap, NULL);

    // Set Elements
    char bssid[ETHER_ADDR_STR_LEN];
    mac2str(bssid, pSsid->bssid, sizeof(bssid));
    amxc_var_add_key(cstring_t, resulMap, "Radio", pRad->instanceName);
    amxc_var_add_key(cstring_t, resulMap, "SSID", wld_ssid_to_string(pSsid->ssid, pSsid->ssid_len));
    amxc_var_add_key(cstring_t, resulMap, "BSSID", bssid);
    amxc_var_add_key(uint32_t, resulMap, "Channel", pSsid->channel);
    amxc_var_add_key(uint32_t, resulMap, "CentreChannel", pSsid->centre_channel);
    amxc_var_add_key(uint32_t, resulMap, "SignalStrength", pSsid->signalStrength);
    amxc_var_add_key(cstring_t, resulMap, "SecurityModeEnabled", cstr_AP_ModesSupported[pSsid->secModeEnabled]);
    amxc_var_add_key(cstring_t, resulMap, "EncryptionMode", cstr_AP_EncryptionMode[pSsid->encryptionMode]);
    amxc_var_add_key(cstring_t, resulMap, "OperatingFrequencyBand", Rad_SupFreqBands[pRad->operatingFrequencyBand]);

    char operatingStandardsChar[32] = "";
    swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar), pSsid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
    amxc_var_add_key(cstring_t, resulMap, "OperatingStandard", operatingStandardsChar);
    amxc_var_add_key(cstring_t, resulMap, "OperatingChannelBandwidth", Rad_SupBW[pSsid->bandwidth]);

    amxc_string_t wpsmethods;
    amxc_string_init(&wpsmethods, 0);
    wld_wps_ConfigMethods_mask_to_string(&wpsmethods, pSsid->WPS_ConfigMethodsEnabled);
    amxc_var_add_key(cstring_t, resulMap, "ConfigMethodsSupported", amxc_string_get(&wpsmethods, 0)); // WPSConfigMethodsSupported
    amxc_string_clean(&wpsmethods);
    amxc_var_add_key(uint32_t, resulMap, "Noise", pSsid->noise);

    SAH_TRACEZ_INFO(ME, "SET MAP FINISH");
}


static void s_addDiagRadioResultsToMap(amxc_var_t* pResultListMap, T_Radio* pRad, T_ScanResults* pScanResults) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pScanResults, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");

    amxc_llist_it_t* it = amxc_llist_get_first(&pScanResults->ssids);
    while(it != NULL) {
        T_ScanResult_SSID* pSsid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        it = amxc_llist_it_get_next(it);

        s_addDiagSingleResultToMap(pResultListMap, pRad, pSsid);
    }
}

static void s_addRadiosScanStatusToMap(amxc_var_t* pMap) {
    amxc_string_t radLists[WLD_RAD_SCAN_STATUS_MAX];

    for(size_t i = 0; i < WLD_RAD_SCAN_STATUS_MAX; i++) {
        amxc_string_init(&radLists[i], 0);
    }

    T_Radio* pRadio = NULL;
    wld_for_eachRad(pRadio) {
        amxc_string_appendf(&radLists[pRadio->radScanStatus], "%s%s", amxc_string_is_empty(&radLists[pRadio->radScanStatus]) ? "" : ",", pRadio->instanceName);
    }

    for(size_t i = 0; i < WLD_RAD_SCAN_STATUS_MAX; i++) {
        amxc_var_add_key(cstring_t, pMap, g_str_wld_radScanStatus[i], amxc_string_get(&radLists[i], 0));
        amxc_string_clean(&radLists[i]);
    }
}

static void s_sendNeighboringWifiDiagnositResult() {
    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);

    /*
     * If at least one radio succeeds in making a scan:
     * ==> Status is "Complete"
     * ==> else, Status is "Error"
     */
    if(g_neighboringWiFiDiagnosticStatus) {
        amxc_var_add_key(cstring_t, &retMap, "Status", "Error");
    } else {
        amxc_var_add_key(cstring_t, &retMap, "Status", "Completed");
    }

    s_addRadiosScanStatusToMap(&retMap);

    amxc_var_t* pResultListMap = amxc_var_add_key(amxc_llist_t, &retMap, "Result", NULL);

    T_ScanResults scanRes;
    amxc_llist_init(&scanRes.ssids);

    T_Radio* pRadio = NULL;
    wld_for_eachRad(pRadio) {
        // Get data
        if(pRadio->pFA->mfn_wrad_scan_results(pRadio, &scanRes) < 0) {
            SAH_TRACEZ_ERROR(ME, "Unable to retrieve scan results");
            continue;
        }

        // Add data to map
        s_addDiagRadioResultsToMap(pResultListMap, pRadio, &scanRes);
        wld_scan_cleanupScanResults(&scanRes);
    }

    amxd_function_deferred_done(g_neighboringWiFiDiagnosticID, amxd_status_ok, NULL, &retMap);
    amxc_var_clean(&retMap);
    g_neighboringWiFiDiagnosticID = 0;         // reset call id
    g_neighboringWiFiDiagnosticStatus = false; // reset status

    SAH_TRACEZ_OUT(ME);
}

void wld_scan_diagNotifyRadDone(bool success) {
    if(success) {
        g_neighboringWiFiDiagnosticStatus = success;
    }

    /* Send the Neighboring WiFi Diagnostic Result list */
    if(!wld_isAnyRadioRunningScan()) {
        s_sendNeighboringWifiDiagnositResult();
    }
}

amxd_status_t _neighboringWiFiDiagnostic(amxd_object_t* pWifiObj,
                                         amxd_function_t* func,
                                         amxc_var_t* args,
                                         amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);

    g_neighboringWiFiDiagnosticID = amxc_var_dyncast(uint64_t, retval);

    char* path = amxd_object_get_path(pWifiObj, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "instance object(%p:%s:%s)", pWifiObj, amxd_object_get_name(pWifiObj, AMXD_OBJECT_NAMED), path);

    amxd_status_t status = amxd_status_ok;

    // Step 1 Launch scan:
    T_Radio* pRadio = NULL;
    wld_for_eachRad(pRadio) {

        if(pRadio->status == RST_UP) {
            status = _startScan(pRadio->pBus, func, args, retval);

            if(status != amxd_status_ok) {
                pRadio->radScanStatus = WLD_RAD_SCAN_STATUS_FAILED;
            }
            SAH_TRACEZ_INFO(ME, "Scan for (%s) %s", pRadio->Name, (status == amxd_status_ok) ? "is correctely launched" : "is not launched");

        } else {
            pRadio->radScanStatus = WLD_RAD_SCAN_STATUS_DISABLED;
        }
    }

    amxd_function_defer(func, &g_neighboringWiFiDiagnosticID, retval, NULL, NULL);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_deferred;
}
