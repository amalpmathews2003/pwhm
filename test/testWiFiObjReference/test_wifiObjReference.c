/*
 * Copyright (c) 2023 SoftAtHome
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

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

typedef struct {
    const char* inRefPath;
    amxd_object_t* inRefObj;
    swl_rc_ne outRc;
    const char* outRefPath;
} test_referenceInfo_t;

static void checkReference(test_referenceInfo_t* testData) {
    char outBuf[128] = {0};
    swl_rc_ne rc = wld_util_getRealReferencePath(outBuf, sizeof(outBuf), testData->inRefPath, testData->inRefObj);
    printf("in path(%s) obj(%p) / out path(%s) rc(%d) / expec path(%s) rc(%d)\n",
           testData->inRefPath, testData->inRefObj,
           outBuf, rc,
           testData->outRefPath, testData->outRc);
    assert_int_equal(rc, testData->outRc);
    assert_string_equal(outBuf, testData->outRefPath);
}

static void test_radioReference(void** state _UNUSED) {
    T_Radio* pRad2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    T_Radio* pRad5 = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    T_Radio* pRad6 = dm.bandList[SWL_FREQ_BAND_6GHZ].rad;
    test_referenceInfo_t tests[] = ARR(
        //success cases
        ARR("", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi1", NULL, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi0", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.wifi0."),
        ARR("WiFi.Radio.wifi1.", pRad5->pBus, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.wifi2", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.wifi2."),
        ARR("Device.WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("Device.WiFi.Radio.1", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("Device.WiFi.Radio.3.", pRad6->pBus, SWL_RC_OK, "Device.WiFi.Radio.3."),
        // override cases
        ARR("Device.WiFi.Radio.", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.3."),
        ARR("Radio.wifi2", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.3."),
        ARR("WiFi.Radio.wifi1", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        // error cases
        ARR(NULL, NULL, SWL_RC_ERROR, ""),
        ARR("", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.Radio.", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.Radio.10", NULL, SWL_RC_ERROR, ""),
        );
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        checkReference(&tests[i]);
    }
}

static void test_ssidReference(void** state _UNUSED) {
    T_SSID* pSsidPriv2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv5 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv6 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPriv->pSSID;

    T_SSID* pSsidEp2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].ep->pSSID;
    T_SSID* pSsidEp5 = dm.bandList[SWL_FREQ_BAND_5GHZ].ep->pSSID;
    T_SSID* pSsidEp6 = dm.bandList[SWL_FREQ_BAND_6GHZ].ep->pSSID;
    test_referenceInfo_t tests[] = ARR(
        //success cases
        ARR(NULL, pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.4."),
        ARR("WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("WiFi.SSID.wlan0", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.wlan0."),
        ARR("WiFi.SSID.wlan1.", pSsidPriv5->pBus, SWL_RC_OK, "WiFi.SSID.wlan1."),
        ARR("WiFi.SSID.wlan2", NULL, SWL_RC_OK, "WiFi.SSID.wlan2."),
        ARR("WiFi.SSID.sta0", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.sta0."),
        ARR("WiFi.SSID.sta1", NULL, SWL_RC_OK, "WiFi.SSID.sta1."),
        ARR("Device.WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.1."),
        ARR("Device.WiFi.SSID.2", pSsidPriv5->pBus, SWL_RC_OK, "Device.WiFi.SSID.2."),
        ARR("Device.WiFi.SSID.3.", pSsidPriv6->pBus, SWL_RC_OK, "Device.WiFi.SSID.3."),
        ARR("Device.WiFi.SSID.6.", pSsidEp6->pBus, SWL_RC_OK, "Device.WiFi.SSID.6."),
        // override cases
        ARR("Device.WiFi.SSID.", pSsidPriv5->pBus, SWL_RC_OK, "WiFi.SSID.2."),
        ARR("SSID.wlan2", pSsidPriv6->pBus, SWL_RC_OK, "WiFi.SSID.3."),
        ARR("WiFi.SSID.sta1", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.4."),
        ARR("Device.WiFi.SSID.sta2", pSsidEp5->pBus, SWL_RC_OK, "WiFi.SSID.5."),
        // error cases
        ARR(NULL, NULL, SWL_RC_ERROR, ""),
        ARR("", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.SSID.", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.SSID.10", NULL, SWL_RC_ERROR, ""),
        );
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        checkReference(&tests[i]);
    }
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "radRef");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_radioReference),
        cmocka_unit_test(test_ssidReference),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

