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

#define DIAG_SCAN_REASON "NeighboringDiag"
typedef struct {
    uint64_t call_id;
    amxc_llist_t runningRads;   /* list of radios running diag scan. */
    amxc_llist_t completedRads; /* list of radios reporting scan results. */
    amxc_llist_t canceledRads;  /* list of radios having cancelled scan. */
    amxc_llist_t failedRads;    /* list of radios having failed to scan (busy, not ready...). */
} neighboringWiFiDiagnosticInfo_t;

/**
 * global neighboringWiFiDiagnostic context.
 */
neighboringWiFiDiagnosticInfo_t g_neighWiFiDiag = {
    .call_id = 0,
};

const char* g_str_wld_blockScanMode[BLOCKSCANMODE_MAX] = {
    "Disable",
    "Prio",
    "All",
};

static amxd_object_t* s_scanStatsInitialize(T_Radio* pRad, const char* scanReason) {
    amxd_object_t* scanStatsTemp = amxd_object_get(pRad->pBus, "ScanStats.ScanReason");
    ASSERTS_NOT_NULL(scanStatsTemp, NULL, ME, "ScanReason Obj NULL");
    amxd_object_t* obj = amxd_object_get(scanStatsTemp, scanReason);
    if(obj == NULL) {
        amxd_object_new_instance(&obj, scanStatsTemp, scanReason, 0, NULL);
        ASSERTS_NOT_NULL(obj, NULL, ME, "Failed to create object: %s", scanReason);
    }
    return obj;
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
    char* blockScanMode = amxd_object_get_cstring_t(object, "BlockScanMode", NULL);
    pRad->scanState.cfg.blockScanMode = swl_conv_charToEnum(blockScanMode, g_str_wld_blockScanMode, BLOCKSCANMODE_MAX, BLOCKSCANMODE_DISABLE);
    free(blockScanMode);
    pRad->scanState.cfg.maxChannelsPerScan = amxd_object_get_int32_t(object, "MaxChannelsPerScan", NULL);
    pRad->scanState.cfg.scanRequestInterval = amxd_object_get_int32_t(object, "ScanRequestInterval", NULL);
    pRad->scanState.cfg.scanChannelCount = amxd_object_get_int32_t(object, "ScanChannelCount", NULL);

    if(pRad->scanState.cfg.fastScanReasons != NULL) {
        free(pRad->scanState.cfg.fastScanReasons);
        pRad->scanState.cfg.fastScanReasons = NULL;
    }
    pRad->scanState.cfg.fastScanReasons = amxd_object_get_cstring_t(object, "FastScanReasons", NULL);
    return amxd_status_ok;
}

static void s_updateScanStats(T_Radio* pRad) {
    amxd_object_t* objScanStats = amxd_object_findf(pRad->pBus, "ScanStats");
    ASSERTS_NOT_NULL(objScanStats, , ME, "ScanStats Obj NULL");
    amxd_object_set_uint32_t(objScanStats, "NrScanRequested", pRad->scanState.stats.nrScanRequested);
    amxd_object_set_uint32_t(objScanStats, "NrScanDone", pRad->scanState.stats.nrScanDone);
    amxd_object_set_uint32_t(objScanStats, "NrScanError", pRad->scanState.stats.nrScanError);
    amxd_object_set_uint32_t(objScanStats, "NrScanBlocked", pRad->scanState.stats.nrScanBlocked);

    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_scanReasonStats_t* stats = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        amxd_object_set_cstring_t(stats->object, "Name", stats->scanReason);
        amxd_object_set_uint32_t(stats->object, "NrScanRequested", stats->totalCount);
        amxd_object_set_uint32_t(stats->object, "NrScanDone", stats->successCount);
        amxd_object_set_uint32_t(stats->object, "NrScanError", stats->errorCount);
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
    /*
     * over-size buffer assuming content is not printable (i.e fully displayed as hex string)
     */
    char ssidStr[(2 * SSID_NAME_LEN) + 1];
    memset(ssidStr, 0, sizeof(ssidStr));
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

        convSsid2Str(SR_ssid->ssid, SR_ssid->ssidLen, ssidStr, sizeof(ssidStr));
        SAH_TRACEZ_INFO(ME, "searching for matching ssid : [%s] <-> [%s]", ssid, ssidStr);

        if(!strncmp(ssid, ssidStr, strlen(ssid))) {
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

wld_scanReasonStats_t* wld_getScanStatsByReason(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_scanReasonStats_t* stats = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        if(swl_str_matches(stats->scanReason, reason)) {
            return stats;
        }
    }
    return NULL;
}

static wld_scanReasonStats_t* s_getOrCreateStats(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");
    wld_scanReasonStats_t* stats = wld_getScanStatsByReason(pRad, reason);
    if(stats == NULL) {
        amxc_llist_t* statList = &(pRad->scanState.stats.extendedStat);
        stats = calloc(1, sizeof(wld_scanReasonStats_t));
        ASSERT_NOT_NULL(stats, NULL, ME, "NULL");

        swl_str_copyMalloc(&(stats->scanReason), reason);
        SAH_TRACEZ_INFO(ME, "ScanReason %s added", stats->scanReason);
        amxc_llist_append(statList, &stats->it);
        stats->object = s_scanStatsInitialize(pRad, reason);

    }
    return stats;
}

static void s_notifyStartScan(T_Radio* pRad, wld_scan_type_e type, const char* scanReason, swl_rc_ne resultCode) {

    if(scanReason == NULL) {
        scanReason = "Unknown";
    }

    SAH_TRACEZ_INFO(ME, "%s: starting scan %s", pRad->Name, scanReason);

    pRad->scanState.stats.nrScanRequested++;
    wld_scanReasonStats_t* stats = s_getOrCreateStats(pRad, scanReason);
    ASSERTS_NOT_NULL(stats, , ME, "NULL");
    stats->totalCount++;

    if((resultCode == SWL_RC_OK) || (resultCode == SWL_RC_CONTINUE)) {
        wld_scanEvent_t ev;
        ev.pRad = pRad;
        ev.start = true;
        ev.scanType = type;
        ev.scanReason = scanReason;
        wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

        pRad->scanState.scanType = type;
        pRad->scanState.lastScanTime = swl_time_getMonoSec();

        swl_str_copy(pRad->scanState.scanReason, sizeof(pRad->scanState.scanReason), scanReason);
    } else {
        pRad->scanState.stats.nrScanError++;
        stats->errorCount++;
    }

    s_updateScanStats(pRad);
}

static amxd_status_t _startScan(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args,
                                amxc_var_t* ret _UNUSED) {
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");
    char* reason = NULL;
    amxd_status_t errorCode = amxd_status_unknown_error;

    if(wld_scan_isRunning(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: scan is already running", pR->Name);
        goto error;
    }

    if(!wld_rad_isUpExt(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: rad not ready for scan (state:%s)",
                         pR->Name, cstr_chanmgt_rad_state[pR->detailedState]);
        errorCode = amxd_status_unknown_error;
        goto error;
    }

    if(pR->scanState.scanType != SCAN_TYPE_NONE) {
        SAH_TRACEZ_ERROR(ME, "%s: rad has ongoing scan type(%d)",
                         pR->Name, pR->scanState.scanType);
        errorCode = amxd_status_unknown_error;
        goto error;
    }

    const char* ssid = GET_CHAR(args, "SSID");
    const char* bssid = GET_CHAR(args, "BSSID");
    const char* channels = GET_CHAR(args, "channels");
    const char* scanReason = GET_CHAR(args, "scanReason");


    T_ScanArgs* scanArgs = &pR->scanState.cfg.scanArguments;
    size_t len = 0;

    memset(scanArgs, 0, sizeof(T_ScanArgs));

    if(channels != NULL) {
        // chanlist must have at most nrPossible elements,
        // otherwise there will be elements that are not part of possible channels.
        len = swl_type_arrayFromChar(swl_type_uint8, scanArgs->chanlist, WLD_MAX_POSSIBLE_CHANNELS, channels);
        if(!wld_rad_isChannelSubset(pR, scanArgs->chanlist, len) || (len == 0)) {
            SAH_TRACEZ_ERROR(ME, "%s: Invalid Channel list", pR->Name);
            errorCode = amxd_status_invalid_function_argument;
            goto error;
        }
        scanArgs->chanCount = len;
    }

    if(ssid != NULL) {
        len = strlen(ssid);
        if(len >= SSID_NAME_LEN) {
            SAH_TRACEZ_ERROR(ME, "SSID too large");
            errorCode = amxd_status_invalid_function_argument;
            goto error;
        }

        swl_str_copy(scanArgs->ssid, SSID_NAME_LEN, ssid);
        scanArgs->ssidLen = len;
    }
    if(!swl_str_isEmpty(bssid)) {
        swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
        memcpy(macChar.cMac, bssid, sizeof(swl_macChar_t));
        if(!swl_mac_charToBin(&scanArgs->bssid, &macChar) || (!swl_mac_charIsValidStaMac(&macChar))) {
            SAH_TRACEZ_ERROR(ME, "Invalid bssid %s", bssid);
            errorCode = amxd_status_invalid_function_argument;
            goto error;

        }
    }

    if(scanReason == NULL) {
        reason = strdup("ScanCall");
    } else {
        reason = strdup(scanReason);
    }

    bool updateUsageData = GET_BOOL(args, "updateUsage");
    if(updateUsageData) {
        scanArgs->updateUsageStats = true;
    }

    bool hasForceFast = GET_BOOL(args, "forceFast");
    if(!hasForceFast) {
        scanArgs->fastScan = (swl_str_find(pR->scanState.cfg.fastScanReasons, reason) > 0);
    }

    if((pR->scanState.cfg.blockScanMode == BLOCKSCANMODE_ALL)
       || ( pR->scanState.cfg.blockScanMode == BLOCKSCANMODE_PRIO)) {
        SAH_TRACEZ_ERROR(ME, "Scan blocked");
        pR->scanState.stats.nrScanBlocked++;
        s_updateScanStats(pR);
        free(reason);
        return amxd_status_unknown_error;
    }

    swl_rc_ne scanResult = wld_scan_start(pR, SCAN_TYPE_SSID);



    if(scanResult < 0) {
        goto error;
    } else {
        free(reason);
        return amxd_status_deferred;
    }

error:
    free(reason);
    return errorCode;
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
                    amxd_function_t* func,
                    amxc_var_t* args,
                    amxc_var_t* retval) {

    T_Radio* pR = object->priv;

    amxd_status_t res = _startScan(object, func, args, retval);
    if(res != amxd_status_deferred) {
        return res;
    }

    pR->scanState.min_rssi = INT32_MIN;
    amxc_var_t* minRssiVar = GET_ARG(args, "min_rssi");
    if(minRssiVar != NULL) {
        pR->scanState.min_rssi = amxc_var_dyncast(int32_t, minRssiVar);
    }
    amxd_function_defer(func, &pR->scanState.call_id, retval, NULL, NULL);

    return amxd_status_deferred;
}


/**
 * Filters out the scan Results if the scan was run with a matching filter
 *
 * Returns true if the result shall be filtered out.
 */
static bool s_filterScanResultsBasedOnArgs(T_Radio* pRad, T_ScanResult_SSID* pResult) {
    T_ScanArgs* pScanArgs = &pRad->scanState.cfg.scanArguments;
    ASSERT_NOT_NULL(pResult, false, ME, "NULL");
    ASSERT_NOT_NULL(pScanArgs, false, ME, "NULL");

    if((pScanArgs->ssid != NULL) && (pScanArgs->ssidLen > 0)) {
        char ssidStr[SSID_NAME_LEN];
        memset(ssidStr, 0, sizeof(ssidStr));
        convSsid2Str(pResult->ssid, pResult->ssidLen, ssidStr, sizeof(ssidStr));
        if(swl_str_matches(ssidStr, pScanArgs->ssid) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with SSID %s while the scan was started with param SSID %s", pScanArgs->ssid, ssidStr);
            return true;
        }
    }

    if(swl_mac_binIsNull(&pScanArgs->bssid) == false) {
        if(swl_mac_binMatches(&pResult->bssid, &pScanArgs->bssid) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with BSSID "SWL_MAC_FMT "  while the scan was started with param BSSID "SWL_MAC_FMT "", SWL_MAC_ARG(pResult->bssid.bMac), SWL_MAC_ARG(pScanArgs->bssid.bMac));
            return true;

        }
    }

    if(pScanArgs->chanCount > 0) {
        if(swl_type_arrayContains(swl_type_uint8, pScanArgs->chanlist, pScanArgs->chanCount, &pResult->channel) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with channel %d which is not part of the channels scan params", pResult->channel);
            return true;
        }
    }
    return false;
}



static bool wld_get_scan_results(T_Radio* pR, amxc_var_t* retval, int32_t min_rssi) {
    T_ScanResults res;

    amxc_llist_init(&res.ssids);

    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to retrieve scan results");
        return false;
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    amxc_llist_for_each(it, &res.ssids) {
        T_ScanResult_SSID* ssid = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);

        // Scan filter is normally handled by the vendor, but sometimes, for divers reasons, the filtering might fail.
        // So let's double-chek it
        if(s_filterScanResultsBasedOnArgs(pR, ssid) == true) {
            continue;
        }

        if(ssid->rssi < min_rssi) {
            continue;
        }

        amxc_var_t* pEntry = amxc_var_add(amxc_htable_t, retval, NULL);
        if(pEntry == NULL) {
            break;
        }

        swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
        swl_mac_binToChar(&bssidStr, &ssid->bssid);

        char* ssidStr = wld_ssid_to_string(ssid->ssid, ssid->ssidLen);
        amxc_var_add_key(cstring_t, pEntry, "SSID", ssidStr);
        free(ssidStr);

        amxc_var_add_key(cstring_t, pEntry, "BSSID", bssidStr.cMac);
        amxc_var_add_key(int32_t, pEntry, "SignalNoiseRatio", ssid->snr);
        amxc_var_add_key(int32_t, pEntry, "Noise", ssid->noise);
        amxc_var_add_key(int32_t, pEntry, "RSSI", ssid->rssi);
        amxc_var_add_key(int32_t, pEntry, "Channel", ssid->channel);
        amxc_var_add_key(int32_t, pEntry, "CentreChannel", ssid->centreChannel);
        amxc_var_add_key(int32_t, pEntry, "Bandwidth", ssid->bandwidth);
        amxc_var_add_key(int32_t, pEntry, "SignalStrength", ssid->rssi);

        amxc_var_add_key(cstring_t, pEntry, "SecurityModeEnabled", swl_security_apModeToString(ssid->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));

        char wpsmethods[256] = {0};
        swl_wps_configMethodMaskToNames(wpsmethods, SWL_ARRAY_SIZE(wpsmethods), ssid->WPS_ConfigMethodsEnabled);
        amxc_var_add_key(cstring_t, pEntry, "WPSConfigMethodsSupported", wpsmethods);

        amxc_var_add_key(cstring_t, pEntry, "EncryptionMode", swl_security_encMode_str[ssid->encryptionMode]);

        char operatingStandardsChar[32] = "";
        swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar),
                          ssid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
        amxc_var_add_key(cstring_t, pEntry, "OperatingStandards", operatingStandardsChar);
    }

    wld_scan_cleanupScanResults(&res);

    return true;
}

amxd_status_t _getScanResults(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval) {

    T_Radio* pR = object->priv;

    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    int32_t min_rssi = INT32_MIN;
    amxc_var_t* minRssiVar = GET_ARG(args, "min_rssi");
    if(minRssiVar != NULL) {
        min_rssi = amxc_var_dyncast(int32_t, minRssiVar);
    }

    if(!wld_get_scan_results(pR, retval, min_rssi)) {
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}


/**
 * Send ScanComplete notification which includes the Result("done" OR "error") to all subscribers
 **/
static void wld_rad_scan_SendDoneNotification(T_Radio* pR, bool success) {
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


/**
 * Send scan results back to the caller and reset the scanState call_id.
 **/
static void wld_rad_scan_FinishScanCall(T_Radio* pR, bool* success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_EQUALS(pR->scanState.call_id, 0, , ME, "%s no call_id", pR->Name);
    ASSERT_TRUE(wld_scan_isRunning(pR), , ME, "%s no scan", pR->Name);

    amxc_var_t ret;
    amxc_var_init(&ret);

    if(pR->scanState.scanType == SCAN_TYPE_SSID) {
        if(!wld_get_scan_results(pR, &ret, pR->scanState.min_rssi)) {
            *success = false;
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "%s scan type unknown %s", pR->Name, pR->scanState.scanReason);
        *success = false;
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
amxd_object_t* wld_scan_update_get_channel(amxd_object_t* objScan, uint16_t channel) {
    if(channel == 0) {
        return NULL;
    }

    amxd_object_t* chanTemplate = amxd_object_get(objScan, "SurroundingChannels");
    ASSERT_NOT_NULL(chanTemplate, NULL, ME, "NULL");
    amxd_object_t* chanObj = NULL;
    amxd_object_for_each(instance, it, chanTemplate) {
        chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(amxd_object_get_uint16_t(chanObj, "Channel", NULL) == channel) {
            return chanObj;
        }
    }
    amxd_status_t ret = amxd_object_new_instance(&chanObj, chanTemplate, NULL, 0, NULL);
    ASSERT_EQUALS(ret, amxd_status_ok, NULL,
                  ME, "fail to create channel instance for channel %d ret:%d", channel, ret);
    amxd_object_set_uint16_t(chanObj, "Channel", channel);

    return chanObj;
}

/**
 * Trigger an internal only scan to update chanim info.
 */
swl_rc_ne wld_scan_updateChanimInfo(T_Radio* pRad) {

    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne retVal = pRad->pFA->mfn_wrad_update_chaninfo(pRad);
    s_notifyStartScan(pRad, SCAN_TYPE_INTERNAL, "ChanimInfo", retVal);
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
        s_notifyStartScan(pR, SCAN_TYPE_SPECTRUM, "Spectrum", SWL_RC_ERROR);
        return retVal;
    }

    s_notifyStartScan(pR, SCAN_TYPE_SPECTRUM, "Spectrum", SWL_RC_OK);

    pR->scanState.call_id = call_id;
    return amxd_status_deferred;
}

/**
 * Search for an access point object that matches the given bssid and the given rssi, withing the current
 * ap_template object. If no matching ap currently exists, null is returned.
 *
 * An accesspoint matches if the differences in bssid strings is less or equal to two (about 1 byte difference)
 * and the rssi matches. Since we look only at a given ap_template, the channel also must match.
 */
static amxd_object_t* s_findScanApGroup(amxd_object_t* ap_template, swl_macChar_t* pBssidChar, int16_t rssi) {
    amxd_object_t* testAp;
    int16_t testRssi;
    char* testBssid;
    bool match = false;
    size_t macDevOffset = 9;
    swl_macChar_t macVendorOuiMask = {.cMac = {"FD:FF:FF:00:00:00"}};
    swl_macChar_t testBssidChar = SWL_MAC_CHAR_NEW();
    amxd_object_for_each(instance, it, ap_template) {
        testAp = amxc_llist_it_get_data(it, amxd_object_t, it);
        testRssi = amxd_object_get_int16_t(testAp, "RSSI", NULL);
        testBssid = amxd_object_get_cstring_t(testAp, "BSSID", NULL);
        match = (/* same rssi */
            (testRssi == rssi) &&
            /* valid test mac */
            (swl_typeMacChar_fromChar(&testBssidChar, testBssid)) &&
            /* same MAC Vendor OUI part as the reference mac, after clearing the LocallyAdminAddress bit */
            (swl_mac_charMatchesMask(&testBssidChar, pBssidChar, &macVendorOuiMask)) &&
            /* at most 2 different hex digits in the MAC Device part */
            (swl_str_nrStrDiff(&testBssidChar.cMac[macDevOffset], &pBssidChar->cMac[macDevOffset], SWL_MAC_CHAR_LEN - macDevOffset) <= 2));

        free(testBssid);
        if(match) {
            return testAp;
        }
    }
    return NULL;
}

/**
 * Add a given SSID result to the given channel.
 */
static void wld_scan_update_add_ssid(amxd_object_t* channel, T_ScanResult_SSID* ssid) {
    ASSERT_NOT_NULL(ssid, , ME, "NULL");
    amxd_object_t* ap_template = amxd_object_get(channel, "Accesspoint");
    ASSERT_NOT_NULL(ap_template, , ME, "Null ap template");

    swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
    swl_mac_binToChar(&bssidStr, &ssid->bssid);

    amxd_object_t* ap_val = s_findScanApGroup(ap_template, &bssidStr, ssid->rssi);

    amxd_status_t ret;
    if(ap_val == NULL) {
        uint32_t id = amxd_object_get_instance_count(ap_template) + 1;
        ret = amxd_object_new_instance(&ap_val, ap_template, NULL, id, NULL);
        ASSERT_EQUALS(ret, amxd_status_ok, , ME, "Fail to create scan AP instance (ret %d)", ret);
        amxd_object_set_int16_t(ap_val, "RSSI", ssid->rssi);
        amxd_object_set_cstring_t(ap_val, "BSSID", bssidStr.cMac);

    }
    amxd_object_t* ap_ssid_template = amxd_object_get(ap_val, "SSID");
    amxd_object_t* ap_ssid = NULL;
    ret = amxd_object_new_instance(&ap_ssid, ap_ssid_template, NULL, 0, NULL);
    ASSERT_EQUALS(ret, amxd_status_ok, , ME, "Fail to create scan SSID instance (ret %d)", ret);
    char* ssidStr = wld_ssid_to_string(ssid->ssid, ssid->ssidLen);
    amxd_object_set_cstring_t(ap_ssid, "SSID", ssidStr);
    amxd_object_set_cstring_t(ap_ssid, "BSSID", bssidStr.cMac);
    amxd_object_set_uint16_t(ap_ssid, "Bandwidth", ssid->bandwidth);
    free(ssidStr);
}

/**
 * Clear the scan results. Should be called before we start adding new
 * results for the latest scan.
 */
static void wld_scan_update_clear_obj(amxd_object_t* objScan) {
    amxd_object_t* chanTemplate = amxd_object_get(objScan, "SurroundingChannels");
    amxd_object_for_each(instance, it, chanTemplate) {
        amxd_object_t* chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        amxd_object_delete(&chanObj);
    }
}

/**
 * Count the number of access points that are on the same channel as the radio currently is.
 */
static void wld_scan_count_cochannel(T_Radio* pR, amxd_object_t* objScan) {
    uint16_t channel = amxd_object_get_uint16_t(pR->pBus, "Channel", NULL);

    uint32_t nrCoChannel = 0;
    amxd_object_t* chanTemplate = amxd_object_get(objScan, "SurroundingChannels");
    amxd_object_t* chanObj = NULL;

    amxd_object_for_each(instance, it, chanTemplate) {
        chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(amxd_object_get_uint16_t(chanObj, "Channel", NULL) == channel) {
            amxd_object_t* apTemplate = amxd_object_get(chanObj, "Accesspoint");
            nrCoChannel = amxd_object_get_instance_count(apTemplate);
            break;
        }
    }
    amxd_object_set_uint16_t(objScan, "NrCoChannelAP", nrCoChannel);
}

void wld_scan_cleanupScanResultSSID(T_ScanResult_SSID* ssid) {
    amxc_llist_it_take(&ssid->it);
    free(ssid);
}


void wld_scan_cleanupScanResults(T_ScanResults* res) {
    ASSERTS_NOT_NULL(res, , ME, "NULL");
    amxc_llist_for_each(it, &res->ssids) {
        T_ScanResult_SSID* ssid = amxc_container_of(it, T_ScanResult_SSID, it);
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
    amxc_llist_for_each(it, &res.ssids) {
        T_ScanResult_SSID* ssid = amxc_container_of(it, T_ScanResult_SSID, it);
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

    if(success) {
        pR->scanState.stats.nrScanDone++;
    } else {
        pR->scanState.stats.nrScanError++;
    }

    wld_scanReasonStats_t* stats = wld_getScanStatsByReason(pR, pR->scanState.scanReason);
    if(stats != NULL) {
        if(success) {
            stats->successCount++;
        } else {
            stats->errorCount++;
        }
    }
    s_updateScanStats(pR);

    if(success && (pR->scanState.scanType == SCAN_TYPE_SSID)) {
        wld_scan_update_obj_results(pR);
    }

    if(pR->scanState.call_id == 0) {
        wld_rad_scan_SendDoneNotification(pR, success);
    } else {
        wld_rad_scan_FinishScanCall(pR, &success);
    }

    wld_scanEvent_t ev;
    ev.pRad = pR;
    ev.start = false;
    ev.success = success;
    ev.scanType = pR->scanState.scanType;
    ev.scanReason = pR->scanState.scanReason;
    wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

    /* Scan is done */
    swl_str_copy(pR->scanState.scanReason, sizeof(pR->scanState.scanReason), "None");
    pR->scanState.scanType = SCAN_TYPE_NONE;
}

bool wld_scan_isRunning(T_Radio* pR) {
    return pR->scanState.scanType != SCAN_TYPE_NONE;
}

static void s_sendNeighboringWifiDiagnosticResult();

static void s_radScanStatusUpdateCb(wld_scanEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRadio = event->pRad;
    ASSERT_NOT_NULL(pRadio, , ME, "NULL");
    ASSERTS_TRUE(swl_str_matches(event->scanReason, DIAG_SCAN_REASON), , ME, "Not diag scan");
    if(event->start) {
        amxc_llist_add_string(&g_neighWiFiDiag.runningRads, pRadio->Name);
        return;
    }
    ASSERTI_FALSE(amxc_llist_is_empty(&g_neighWiFiDiag.runningRads), , ME, "No diag running");
    amxc_llist_for_each(it, &g_neighWiFiDiag.runningRads) {
        amxc_string_t* radName = amxc_string_from_llist_it(it);
        if(swl_str_matches(amxc_string_get(radName, 0), pRadio->Name)) {
            amxc_llist_it_take(it);
            if(event->success) {
                amxc_llist_append(&g_neighWiFiDiag.completedRads, it);
            } else {
                amxc_llist_append(&g_neighWiFiDiag.failedRads, it);
            }
            break;
        }
    }
    //all diag scans are answered: send deferred reply
    if(amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        /* Send the Neighboring WiFi Diagnostic Result list */
        s_sendNeighboringWifiDiagnosticResult();
    }
}

static wld_event_callback_t s_radScanStatusCbContainer = {
    .callback = (wld_event_callback_fun) s_radScanStatusUpdateCb
};

static void s_addDiagSingleResultToMap(amxc_var_t* pResultListMap, T_Radio* pRad, T_ScanResult_SSID* pSsid) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pSsid, , ME, "NULL");

    amxc_var_t* resulMap = amxc_var_add(amxc_htable_t, pResultListMap, NULL);
    ASSERTS_NOT_NULL(resulMap, , ME, "NULL");

    // Set Elements
    swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
    swl_mac_binToChar(&bssidStr, &pSsid->bssid);
    char* path = amxd_object_get_path(pRad->pBus, AMXD_OBJECT_INDEXED);
    amxc_var_add_key(cstring_t, resulMap, "Radio", path);
    free(path);
    char* ssidStr = wld_ssid_to_string(pSsid->ssid, pSsid->ssidLen);
    amxc_var_add_key(cstring_t, resulMap, "SSID", ssidStr);
    free(ssidStr);
    amxc_var_add_key(cstring_t, resulMap, "BSSID", bssidStr.cMac);
    amxc_var_add_key(uint32_t, resulMap, "Channel", pSsid->channel);
    amxc_var_add_key(uint32_t, resulMap, "CentreChannel", pSsid->centreChannel);
    amxc_var_add_key(int32_t, resulMap, "SignalStrength", pSsid->rssi);
    amxc_var_add_key(cstring_t, resulMap, "SecurityModeEnabled", swl_security_apModeToString(pSsid->secModeEnabled, SWL_SECURITY_APMODEFMT_ALTERNATE));
    amxc_var_add_key(cstring_t, resulMap, "EncryptionMode", swl_security_encMode_str[pSsid->encryptionMode]);
    amxc_var_add_key(cstring_t, resulMap, "OperatingFrequencyBand", Rad_SupFreqBands[pRad->operatingFrequencyBand]);

    char operatingStandardsChar[32] = "";
    swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar), pSsid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
    amxc_var_add_key(cstring_t, resulMap, "OperatingStandard", operatingStandardsChar);
    amxc_var_add_key(cstring_t, resulMap, "OperatingChannelBandwidth", Rad_SupBW[swl_chanspec_intToBw(pSsid->bandwidth)]);

    char wpsmethods[256] = {0};
    swl_wps_configMethodMaskToNames(wpsmethods, SWL_ARRAY_SIZE(wpsmethods), pSsid->WPS_ConfigMethodsEnabled);
    amxc_var_add_key(cstring_t, resulMap, "ConfigMethodsSupported", wpsmethods); // WPSConfigMethodsSupported
    amxc_var_add_key(uint32_t, resulMap, "Noise", pSsid->noise);
}


static void s_addDiagRadioResultsToMap(amxc_var_t* pResultListMap, T_Radio* pRad, T_ScanResults* pScanResults) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pScanResults, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");

    amxc_llist_for_each(it, &pScanResults->ssids) {
        T_ScanResult_SSID* pSsid = amxc_container_of(it, T_ScanResult_SSID, it);
        s_addDiagSingleResultToMap(pResultListMap, pRad, pSsid);
    }
}

static void s_addDiagRadiosListToMap(amxc_var_t* retval, const char* listName, amxc_llist_t* listEntries) {
    ASSERTS_FALSE(amxc_llist_is_empty(listEntries), , ME, "Empty");
    amxc_string_t radios;
    amxc_string_init(&radios, 0);
    amxc_string_join_llist(&radios, listEntries, ',');
    amxc_var_add_key(cstring_t, retval, listName, amxc_string_get(&radios, 0));
    amxc_string_clean(&radios);
}

static void s_addDiagRadiosStatusToMap(amxc_var_t* retval) {
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_ERROR));
        s_addDiagRadiosListToMap(retval, "FailedRadios", &g_neighWiFiDiag.runningRads);
        return;
    }
    /*
     * If at least one radio succeeds in making a scan:
     * ==> Status is "Complete"
     * ==> else, Status is "Error"
     */
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.completedRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_COMPLETE));
    } else if(!amxc_llist_is_empty(&g_neighWiFiDiag.canceledRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_CANCELED));
    } else {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_ERROR));
    }
    s_addDiagRadiosListToMap(retval, "ReportingRadios", &g_neighWiFiDiag.completedRads);
    s_addDiagRadiosListToMap(retval, "CanceledRadios", &g_neighWiFiDiag.canceledRads);
    s_addDiagRadiosListToMap(retval, "FailedRadios", &g_neighWiFiDiag.failedRads);
}

static void s_sendNeighboringWifiDiagnosticResult() {
    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);

    s_addDiagRadiosStatusToMap(&retMap);
    amxc_var_t* pResultListMap = amxc_var_add_key(amxc_llist_t, &retMap, "Result", NULL);

    T_ScanResults scanRes;
    amxc_llist_init(&scanRes.ssids);

    T_Radio* pRadio = NULL;
    amxc_llist_for_each(it, &g_neighWiFiDiag.completedRads) {
        amxc_string_t* radName = amxc_string_from_llist_it(it);
        if(amxc_string_is_empty(radName)) {
            continue;
        }
        pRadio = wld_rad_get_radio(amxc_string_get(radName, 0));
        if(pRadio == NULL) {
            continue;
        }
        // Get data
        if(pRadio->pFA->mfn_wrad_scan_results(pRadio, &scanRes) >= SWL_RC_OK) {
            // Add data to map
            s_addDiagRadioResultsToMap(pResultListMap, pRadio, &scanRes);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: Unable to retrieve scan results", pRadio->Name);
        }
        wld_scan_cleanupScanResults(&scanRes);
    }

    amxd_function_deferred_done(g_neighWiFiDiag.call_id, amxd_status_ok, NULL, &retMap);
    amxc_var_clean(&retMap);

    g_neighWiFiDiag.call_id = 0;
    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    amxc_llist_clean(&g_neighWiFiDiag.completedRads, amxc_string_list_it_free);
    amxc_llist_clean(&g_neighWiFiDiag.canceledRads, amxc_string_list_it_free);
    amxc_llist_clean(&g_neighWiFiDiag.failedRads, amxc_string_list_it_free);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _NeighboringWiFiDiagnostic(amxd_object_t* pWifiObj,
                                         amxd_function_t* func,
                                         amxc_var_t* args _UNUSED,
                                         amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);

    if(g_neighWiFiDiag.call_id != 0) {
        SAH_TRACEZ_ERROR(ME, "WiFiDiagnostic is already running");
        goto error;
    }

    amxc_llist_init(&g_neighWiFiDiag.runningRads);
    amxc_llist_init(&g_neighWiFiDiag.completedRads);
    amxc_llist_init(&g_neighWiFiDiag.canceledRads);
    amxc_llist_init(&g_neighWiFiDiag.failedRads);

    amxd_status_t status = amxd_status_ok;
    amxc_var_t localArgs;
    amxc_var_init(&localArgs);
    amxc_var_set_type(&localArgs, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &localArgs, "scanReason", DIAG_SCAN_REASON);
    wld_event_add_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    // use radio obj instances order (isof radio device detection order)
    amxd_object_t* radTempl = amxd_object_get(pWifiObj, "Radio");
    amxd_object_for_each(instance, it, radTempl) {
        amxd_object_t* radObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if((radObj == NULL) || (radObj->priv == NULL)) {
            continue;
        }
        T_Radio* pRadio = radObj->priv;
        status = _startScan(pRadio->pBus, func, &localArgs, retval);
        if(status != amxd_status_deferred) {
            amxc_llist_add_string(&g_neighWiFiDiag.failedRads, pRadio->Name);
        }
    }
    amxc_var_clean(&localArgs);
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        amxd_function_defer(func, &g_neighWiFiDiag.call_id, retval, NULL, NULL);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_deferred;
    }
    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
error:
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    s_addDiagRadiosStatusToMap(retval);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

swl_rc_ne wld_scan_start(T_Radio* pRad, wld_scan_type_e type) {
    swl_rc_ne error = pRad->pFA->mfn_wrad_start_scan(pRad);
    s_notifyStartScan(pRad, type, pRad->scanState.cfg.scanArguments.reason, error);
    return error;
}

