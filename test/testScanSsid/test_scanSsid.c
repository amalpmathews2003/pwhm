/*
 * Copyright (c) 2022 SoftAtHome
 *
 * The information and source code contained herein is the exclusive
 * property of SoftAtHome and may not be disclosed, examined, or
 * reproduced in whole or in part without explicit written authorization
 * of the copyright owner.
 *
 */

#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"

static wld_th_dm_t dm;

#define NB_NEIGH_DEVICES 6
#define NB_BSS_PER_DEVICE 3
#define NB_SCAN_RESULTS (NB_NEIGH_DEVICES * NB_BSS_PER_DEVICE)
#define NB_SURROUNDING_CHANNELS 5
swl_channel_t scanResChannels[SWL_FREQ_BAND_MAX][NB_SURROUNDING_CHANNELS] = {{1, 3, 6, 8, 11}, {36, 48, 60, 100, 108}, {1, 33, 49, 65, 81}};
static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    T_Radio* rad;
    T_ScanResults* results;
    wld_for_eachRad(rad) {
        results = &rad->scanState.lastScanResults;
        for(int i = 0; i < NB_SCAN_RESULTS; i++) {
            T_ScanResult_SSID* item = calloc(1, sizeof(T_ScanResult_SSID));
            assert_non_null(item);
            item->ssidLen = snprintf((char*) item->ssid, sizeof(item->ssid), "ssid_%d", i);
            int devIdx = i % NB_NEIGH_DEVICES;
            // set distinct 2 bytes per device to create own AP device instance in scan resuls
            item->bssid.bMac[3] = item->bssid.bMac[4] = devIdx;
            item->bssid.bMac[5] = (((rad->ref_index << 4) & 0xf0) | i) & 0xff;
            item->rssi = -40 + ((devIdx % 2) ? devIdx : -devIdx);  //around rssi -40
            item->noise = -80 - ((devIdx % 2) ? devIdx : -devIdx); //around noise -80
            item->channel = scanResChannels[rad->operatingFrequencyBand][devIdx % NB_SURROUNDING_CHANNELS];
            item->centreChannel = item->channel;
            item->bandwidth = 20;
            item->operatingStandards = M_SWL_RADSTD_AUTO;
            if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
                item->operatingStandards = M_SWL_RADSTD_AX;
            } else if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) {
                item->operatingStandards = M_SWL_RADSTD_A;
                if(devIdx % 4) {
                    item->operatingStandards |= M_SWL_RADSTD_AX | M_SWL_RADSTD_AC | M_SWL_RADSTD_N;
                } else if(devIdx % 3) {
                    item->operatingStandards |= M_SWL_RADSTD_AC | M_SWL_RADSTD_N;
                } else if(devIdx % 2) {
                    item->operatingStandards |= M_SWL_RADSTD_N;
                }
            } else if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) {
                item->operatingStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G;
                if(devIdx % 3) {
                    item->operatingStandards |= M_SWL_RADSTD_AX | M_SWL_RADSTD_N;
                } else if(devIdx % 2) {
                    item->operatingStandards |= M_SWL_RADSTD_N;
                }
            }
            amxc_llist_append(&results->ssids, &item->it);
        }
    }
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_startAndGetScan(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    ttb_var_t* replyVar;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "startScan", NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));
    ttb_object_cleanReply(&reply, &replyVar);
    ttb_mockTimer_goToFutureMs(100);
    assert_true(wld_scan_isRunning(pRad));
    ttb_mockTimer_goToFutureMs(1000);
    wld_scan_done(pRad, true);
    assert_false(wld_scan_isRunning(pRad));
    T_ScanResults res;
    amxc_llist_init(&res.ssids);
    assert_true(pRad->pFA->mfn_wrad_scan_results(pRad, &res) != SWL_RC_NOT_IMPLEMENTED);
    amxc_llist_for_each(it, &res.ssids) {
        T_ScanResult_SSID* pSSID = amxc_container_of(it, T_ScanResult_SSID, it);
        assert_non_null(pSSID);
        assert_string_not_equal(pSSID->ssid, "");
        assert_true(wld_rad_hasChannel(pRad, pSSID->channel));
    }
    uint32_t totalResults = amxc_llist_size(&res.ssids);
    uint32_t match = 0;
    amxd_object_t* chan_template = amxd_object_findf(pRad->pBus, "ScanResults.SurroundingChannels");
    assert_int_equal(amxd_object_get_instance_count(chan_template), NB_SURROUNDING_CHANNELS);
    amxd_object_for_each(instance, it, chan_template) {
        amxd_object_t* chan_obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        uint16_t channel = amxd_object_get_uint16_t(chan_obj, "Channel", NULL);
        amxd_object_t* ap_template = amxd_object_get(chan_obj, "Accesspoint");
        amxd_object_for_each(instance, itAp, ap_template) {
            amxd_object_t* ap_obj = amxc_llist_it_get_data(itAp, amxd_object_t, it);
            amxd_object_t* apSsid_template = amxd_object_get(ap_obj, "SSID");
            assert_int_equal(amxd_object_get_instance_count(apSsid_template), NB_BSS_PER_DEVICE);
            amxd_object_for_each(instance, itApSsid, apSsid_template) {
                amxd_object_t* apSsid_obj = amxc_llist_it_get_data(itApSsid, amxd_object_t, it);
                char* ssid = amxd_object_get_cstring_t(apSsid_obj, "SSID", NULL);
                char* bssid = amxd_object_get_cstring_t(apSsid_obj, "BSSID", NULL);
                swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
                SWL_MAC_CHAR_TO_BIN(&bssidBin, bssid);
                amxc_llist_for_each(itScanSSID, &res.ssids) {
                    T_ScanResult_SSID* pSSID = amxc_container_of(itScanSSID, T_ScanResult_SSID, it);
                    if((swl_str_nmatches((char*) pSSID->ssid, ssid, pSSID->ssidLen)) &&
                       (pSSID->channel == channel) &&
                       (SWL_MAC_BIN_MATCHES(&pSSID->bssid, &bssidBin))) {
                        match += 1;
                    }
                }
                free(bssid);
                free(ssid);
            }
        }
    }
    wld_scan_cleanupScanResults(&res);
    assert_int_equal(totalResults, match);
    assert_int_equal(totalResults, NB_SCAN_RESULTS);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "ssid");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_startAndGetScan),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

