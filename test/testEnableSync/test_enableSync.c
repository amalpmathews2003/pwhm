/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "wld_util.h"
#include "wld_ssid.h"
#include "Utils/wld_config.h"
#include "test-toolbox/ttb_mockClock.h"
#include "test-toolbox/ttb_assert.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"

#define ME "TST_ENB"

static wld_th_dm_t dm;
static amxd_object_t* configObj = NULL;
static T_Radio* pRad = NULL;
static T_SSID* pSSID = NULL;
static T_AccessPoint* pAP = NULL;
static T_EndPoint* pEP = NULL;
static T_SSID* pEPSSID = NULL;

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));

    configObj = amxd_object_get_child(get_wld_object(), "Config");
    assert_non_null(configObj);

    pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    assert_non_null(pRad);

    pSSID = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPrivSSID;
    assert_non_null(pSSID);

    pAP = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPriv;
    assert_non_null(pAP);

    pEP = dm.bandList[SWL_FREQ_BAND_5GHZ].ep;
    assert_non_null(pEP);

    pEPSSID = pEP->pSSID;
    assert_non_null(pEPSSID);

    swl_typeBool_commitObjectParam(pRad->pBus, "STASupported_Mode", true);
    swl_typeBool_commitObjectParam(pRad->pBus, "STA_Mode", true);
    ttb_mockTimer_goToFutureMs(10);

    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    configObj = NULL;
    wld_th_dm_destroy(&dm);
    return 0;
}


static char* s_getEnableSyncMode() {
    return swl_typeCharPtr_fromObjectParamDef(configObj, "EnableSyncMode", NULL);
}

static void s_setEnableSyncModeAndCheck(char* configMode) {
    swl_typeCharPtr_commitObjectParam(configObj, "EnableSyncMode", configMode);
    ttb_mockTimer_goToFutureMs(10);

    char* curMode = s_getEnableSyncMode();
    ttb_assert_str_eq(curMode, configMode);
    ttb_assert_str_eq(wld_config_getEnableSyncModeStr(), configMode);
    free(curMode);
}

static void s_checkStatus_apSsidRad(wld_intfStatus_e apStatus, wld_status_e ssidStatus, wld_status_e radStatus) {
    ttb_assert_int_eq(pAP->status, apStatus);
    ttb_assert_int_eq(pSSID->status, ssidStatus);
    ttb_assert_int_eq(pAP->pRadio->status, radStatus);
}

static void s_checkStatus_epSsid(wld_intfStatus_e epStatus, wld_status_e ssidStatus) {
    ttb_assert_int_eq(pEP->status, epStatus);
    ttb_assert_int_eq(pEP->pSSID->status, ssidStatus);
}

// Test that syncing from AP to SSID works. Assumes everything is ON
static void test_syncApToSSIDWorks() {
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    uint32_t startToggleCount = pSSID->changeInfo.nrEnables;


    wld_th_vap_setApEnable(pAP, false, true);

    ttb_assert_false(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setApEnable(pAP, true, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    ttb_assert_int_eq(startToggleCount + 1, pSSID->changeInfo.nrEnables);
}

// Test that syncing from AP to SSID works. Assumes everything is ON
static void test_syncSSIDToApWorks() {
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    uint32_t startToggleCount = pSSID->changeInfo.nrEnables;


    wld_th_vap_setSSIDEnable(pAP, false, true);

    ttb_assert_false(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setSSIDEnable(pAP, true, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    ttb_assert_int_eq(startToggleCount + 1, pSSID->changeInfo.nrEnables);

}


// Test that syncing from AP to SSID works. Assumes everything is ON
static void test_syncApToSSIDNotWorks() {
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    uint32_t startToggleCount = pSSID->changeInfo.nrEnables;

    wld_th_vap_setApEnable(pAP, false, true);

    ttb_assert_false(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setApEnable(pAP, true, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    ttb_assert_int_eq(startToggleCount, pSSID->changeInfo.nrEnables);

    wld_th_vap_setApEnable(pAP, false, true);

    ttb_assert_false(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setSSIDEnable(pAP, false, true);

    ttb_assert_false(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setApEnable(pAP, true, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setSSIDEnable(pAP, true, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);


    ttb_assert_int_eq(startToggleCount + 1, pSSID->changeInfo.nrEnables);
}

// Test that syncing from AP to SSID works. Assumes everything is ON
static void test_syncSSIDToApNotWorks() {
    // Test sync while Ap On
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);
    uint32_t startToggleCount = pSSID->changeInfo.nrEnables;

    wld_th_vap_setSSIDEnable(pAP, false, true);

    ttb_assert_true(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setSSIDEnable(pAP, true, true);
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);

    ttb_assert_int_eq(startToggleCount + 1, pSSID->changeInfo.nrEnables);


    // Test sync while Ap Off, after setting SSID off
    wld_th_vap_setSSIDEnable(pAP, false, true);
    ttb_assert_true(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);

    wld_th_vap_setApEnable(pAP, false, true);
    ttb_assert_false(pAP->enable);
    ttb_assert_false(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);


    wld_th_vap_setSSIDEnable(pAP, true, true);
    ttb_assert_false(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_DISABLED, RST_DOWN, RST_DORMANT);


    wld_th_vap_setApEnable(pAP, true, true);
    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    s_checkStatus_apSsidRad(APSTI_ENABLED, RST_UP, RST_UP);


    ttb_assert_int_eq(startToggleCount + 2, pSSID->changeInfo.nrEnables);
}


// Test that syncing from Endpoint to SSID works. Assumes everything is ON
static void test_syncEndpointToSSIDWorks() {
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    uint32_t startToggleCount = pEPSSID->changeInfo.nrEnables;


    wld_th_ep_setEnable(pEP, false, true);

    ttb_assert_false(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setEnable(pEP, true, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    ttb_assert_int_eq(startToggleCount + 1, pEPSSID->changeInfo.nrEnables);
}

// Test that syncing from Endpoint to SSID works. Assumes everything is ON
static void test_syncSSIDToEndpointWorks() {
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    uint32_t startToggleCount = pEPSSID->changeInfo.nrEnables;


    wld_th_ep_setSSIDEnable(pEP, false, true);

    ttb_assert_false(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setSSIDEnable(pEP, true, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    ttb_assert_int_eq(startToggleCount + 1, pEPSSID->changeInfo.nrEnables);

}


// Test that syncing from Endpoint to SSID works. Assumes everything is ON
static void test_syncEndpointToSSIDNotWorks() {
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    uint32_t startToggleCount = pEPSSID->changeInfo.nrEnables;

    wld_th_ep_setEnable(pEP, false, true);

    ttb_assert_false(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setEnable(pEP, true, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    ttb_assert_int_eq(startToggleCount, pEPSSID->changeInfo.nrEnables);

    wld_th_ep_setEnable(pEP, false, true);

    ttb_assert_false(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setSSIDEnable(pEP, false, true);

    ttb_assert_false(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setEnable(pEP, true, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setSSIDEnable(pEP, true, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);


    ttb_assert_int_eq(startToggleCount + 1, pEPSSID->changeInfo.nrEnables);
}



// Test that syncing from Endpoint to SSID works. Assumes everything is ON
static void test_syncSSIDToEndpointNotWorks() {
    // Test sync while Endpoint On
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);
    uint32_t startToggleCount = pEPSSID->changeInfo.nrEnables;

    wld_th_ep_setSSIDEnable(pEP, false, true);

    ttb_assert_true(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setSSIDEnable(pEP, true, true);
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);

    ttb_assert_int_eq(startToggleCount + 1, pEPSSID->changeInfo.nrEnables);


    // Test sync while Endpoint Off, after setting SSID off
    wld_th_ep_setSSIDEnable(pEP, false, true);
    ttb_assert_true(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);

    wld_th_ep_setEnable(pEP, false, true);
    ttb_assert_false(pEP->enable);
    ttb_assert_false(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);


    wld_th_ep_setSSIDEnable(pEP, true, true);
    ttb_assert_false(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_DISABLED, RST_DOWN);


    wld_th_ep_setEnable(pEP, true, true);
    ttb_assert_true(pEP->enable);
    ttb_assert_true(pEPSSID->enable);
    s_checkStatus_epSsid(APSTI_ENABLED, RST_DORMANT);


    ttb_assert_int_eq(startToggleCount + 2, pEPSSID->changeInfo.nrEnables);
}

static void test_quickToggle(void** state _UNUSED) {
    uint32_t startToggleCount = pSSID->changeInfo.nrEnables;


    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);
    swl_typeUInt8_commitObjectParam(pSSID->pBus, "Enable", 0);
    amxp_signal_read();
    wld_ssid_dbgTriggerSync(pSSID);
    swl_typeUInt8_commitObjectParam(pSSID->pBus, "Enable", 1);

    amxp_signal_read();
    wld_ssid_dbgTriggerSync(pSSID);

    ttb_mockTimer_goToFutureSec(1);


    ttb_assert_true(pAP->enable);
    ttb_assert_true(pSSID->enable);

    ttb_assert_int_eq(startToggleCount + 1, pSSID->changeInfo.nrEnables);

}

static void test_syncMirror(void** state _UNUSED) {
    s_setEnableSyncModeAndCheck("Mirrored");

    test_syncApToSSIDWorks();
    test_syncSSIDToApWorks();
    test_syncEndpointToSSIDWorks();
    test_syncSSIDToEndpointWorks();
}


static void test_syncOff(void** state _UNUSED) {
    s_setEnableSyncModeAndCheck("Off");

    test_syncApToSSIDNotWorks();
    test_syncSSIDToApNotWorks();
    test_syncEndpointToSSIDNotWorks();
    test_syncSSIDToEndpointNotWorks();
}

static void test_syncToIntf(void** state _UNUSED) {
    s_setEnableSyncModeAndCheck("ToIntf");

    test_syncApToSSIDNotWorks();
    test_syncSSIDToApWorks();
    test_syncEndpointToSSIDNotWorks();
    test_syncSSIDToEndpointWorks();
}

static void test_syncFromIntf(void** state _UNUSED) {
    s_setEnableSyncModeAndCheck("FromIntf");

    test_syncApToSSIDWorks();
    test_syncSSIDToApNotWorks();
    test_syncEndpointToSSIDWorks();
    test_syncSSIDToEndpointNotWorks();
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen("testApp", TRACE_TYPE_STDERR);

    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_INFO, "ssid");
    sahTraceAddZone(TRACE_LEVEL_INFO, "ap");
    sahTraceAddZone(TRACE_LEVEL_INFO, "ep");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_quickToggle),
        cmocka_unit_test(test_syncMirror),
        cmocka_unit_test(test_syncOff),
        cmocka_unit_test(test_syncToIntf),
        cmocka_unit_test(test_syncFromIntf),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

