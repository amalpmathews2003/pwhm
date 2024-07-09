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
#include "wld_types.h"
#include "wld_radio.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "../testHelper/wld_th_types.h"
#include "../testHelper/wld_th_sensing.h"
#include "swl/ttb/swl_ttb.h"

static wld_th_dm_t dm;

static int setup_suite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

#define NB_CLIENT_DEVICE 3

char* csiDeviceMacAddrList[] = {"30:07:4D:54:3F:9E", "30:07:4D:54:3F:90", "30:07:4D:54:3F:80"};
swl_freqBand_e bandTab[] = {SWL_FREQ_BAND_2_4GHZ, SWL_FREQ_BAND_5GHZ, SWL_FREQ_BAND_6GHZ};

static T_Radio* getRadioBand(swl_freqBand_e band) {
    return (dm.bandList[band].rad);
}

static ttb_object_t* getSensingChildObject(swl_freqBand_e band) {
    return (ttb_object_getChildObject(dm.bandList[band].radObj, "Sensing"));
}

static ttb_object_t* getCsiClientChildObject(swl_freqBand_e band, int index) {
    char template[128];
    snprintf(template, sizeof(template), "Sensing.CSIClient.%d", index);
    return (ttb_object_getChildObject(dm.bandList[band].radObj, template));
}

static void sensing_enable(T_Radio* pRad, ttb_object_t* radObj, wld_th_rad_sensing_vendorData_t* sensingVendorData) {
    assert_non_null(pRad);
    assert_non_null(radObj);
    assert_non_null(sensingVendorData);

    assert_int_equal(0, swl_typeUInt8_fromObjectParamDef(radObj, "Enable", 0));
    assert_int_equal(0, pRad->csiEnable);
    assert_int_equal(0, sensingVendorData->csiEnable);

    swl_typeUInt8_commitObjectParam(radObj, "Enable", 1);
    ttb_mockTimer_goToFutureMs(1);

    assert_int_equal(1, swl_typeUInt8_fromObjectParamDef(radObj, "Enable", 0));
    assert_int_equal(1, pRad->csiEnable);
    assert_int_equal(1, sensingVendorData->csiEnable);
}

static bool sensingCommand(ttb_object_t* radObj, char* cmd, char* macAddr, uint32_t interval) {
    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = NULL;

    if(macAddr) {
        // addClient or delClient
        ttb_var_t* args = ttb_object_createArgs();
        assert_non_null(args);
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, args, "MACAddress", macAddr);
        if(interval != 0) {
            amxc_var_add_key(uint32_t, args, "MonitorInterval", interval);
        }
        reply = ttb_object_callFun(dm.ttbBus, radObj, cmd,
                                   &args, &replyVar);
    } else {
        reply = ttb_object_callFun(dm.ttbBus, radObj, cmd,
                                   NULL, &replyVar);
    }
    bool ret = ttb_object_replySuccess(reply);
    ttb_object_cleanReply(&reply, &replyVar);
    ttb_mockTimer_goToFutureMs(1);
    return ret;
}

static void checkCSICounters(ttb_object_t* radObj) {
    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, radObj, "csiStats",
                                            NULL, &replyVar);

    assert_true(ttb_object_replySuccess(reply));
    amxc_var_for_each(newValue, replyVar) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "NullFrameCounter")) {
            uint32_t nullFrameCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(nullFrameCounter, 10);
        } else if(swl_str_matches(pname, "M2MTransmitCounter")) {
            uint32_t m2MTransmitCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(m2MTransmitCounter, 20);
        } else if(swl_str_matches(pname, "UserTransmitCounter")) {
            uint32_t userTransmitCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(userTransmitCounter, 30);
        } else if(swl_str_matches(pname, "NullFrameAckFailCounter")) {
            uint32_t nullFrameAckFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(nullFrameAckFailCounter, 40);
        } else if(swl_str_matches(pname, "ReceivedOverflowCounter")) {
            uint32_t receivedOverflowCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(receivedOverflowCounter, 50);
        } else if(swl_str_matches(pname, "TransmitFailCounter")) {
            uint32_t transmitFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(transmitFailCounter, 60);
        } else if(swl_str_matches(pname, "UserTransmitFailCounter")) {
            uint32_t userTransmitFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(userTransmitFailCounter, 70);
        } else {
            continue;
        }
    }
    ttb_object_cleanReply(&reply, &replyVar);
    ttb_mockTimer_goToFutureMs(1);
}

static void checkCSIResettedCounters(ttb_object_t* radObj) {
    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, radObj, "resetStats",
                                            NULL, &replyVar);

    assert_true(ttb_object_replySuccess(reply));
    amxc_var_for_each(newValue, replyVar) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "NullFrameCounter")) {
            uint32_t nullFrameCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(nullFrameCounter, 1);
        } else if(swl_str_matches(pname, "M2MTransmitCounter")) {
            uint32_t m2MTransmitCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(m2MTransmitCounter, 2);
        } else if(swl_str_matches(pname, "UserTransmitCounter")) {
            uint32_t userTransmitCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(userTransmitCounter, 3);
        } else if(swl_str_matches(pname, "NullFrameAckFailCounter")) {
            uint32_t nullFrameAckFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(nullFrameAckFailCounter, 4);
        } else if(swl_str_matches(pname, "ReceivedOverflowCounter")) {
            uint32_t receivedOverflowCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(receivedOverflowCounter, 5);
        } else if(swl_str_matches(pname, "TransmitFailCounter")) {
            uint32_t transmitFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(transmitFailCounter, 6);
        } else if(swl_str_matches(pname, "UserTransmitFailCounter")) {
            uint32_t userTransmitFailCounter = amxc_var_dyncast(uint32_t, newValue);
            assert_int_equal(userTransmitFailCounter, 7);
        } else {
            continue;
        }
    }
    ttb_object_cleanReply(&reply, &replyVar);
    ttb_mockTimer_goToFutureMs(1);
}

static void test_sensing_enable(void** state _UNUSED) {
    uint8_t tabIndex = 0;

    for(tabIndex = 0; tabIndex < SWL_FREQ_BAND_MAX; tabIndex++) {
        T_Radio* pRad = getRadioBand(bandTab[tabIndex]);
        assert_non_null(pRad);
        wld_th_rad_sensing_vendorData_t* sensingVendorData = sensingGetVendorData(pRad);
        assert_non_null(sensingVendorData);
        ttb_object_t* radObj = getSensingChildObject(bandTab[tabIndex]);
        assert_non_null(radObj);

        sensing_enable(pRad, radObj, sensingVendorData);

    }

}

static void test_sensing_start_stop(void** state _UNUSED) {
    uint8_t tabIndex = 0;
    uint8_t csiClientListIndex = 0;
    ttb_object_t* radObj = NULL;
    ttb_object_t* csiClientObj = NULL;
    T_Radio* pRad = NULL;
    wld_th_rad_sensing_vendorData_t* sensingVendorData = NULL;

    for(tabIndex = 0; tabIndex < SWL_FREQ_BAND_MAX; tabIndex++) {
        pRad = getRadioBand(bandTab[tabIndex]);
        assert_non_null(pRad);
        sensingVendorData = sensingGetVendorData(pRad);
        assert_non_null(sensingVendorData);

        sensingVendorData->maxClientsNbrs = 3;
        sensingVendorData->maxMonitorInterval = 500;

        // Add some client devices
        radObj = getSensingChildObject(bandTab[tabIndex]);
        assert_non_null(radObj);

        for(csiClientListIndex = 0; csiClientListIndex < NB_CLIENT_DEVICE; csiClientListIndex++) {
            // ADD csi client test ---------------------------------------------------------------------//
            assert_true(sensingCommand(radObj, "addClient",
                                       csiDeviceMacAddrList[csiClientListIndex], csiClientListIndex * 100 + 10));

            csiClientObj = getCsiClientChildObject(bandTab[tabIndex], csiClientListIndex + 1);
            assert_non_null(csiClientObj);

            char* csiCLientMacAddr = swl_typeCharPtr_fromObjectParamDef(csiClientObj, "MACAddress", 0);
            assert_non_null(csiCLientMacAddr);
            assert_string_equal(csiDeviceMacAddrList[csiClientListIndex], csiCLientMacAddr);
            free(csiCLientMacAddr);

            uint32_t interval = swl_typeUInt32_fromObjectParamDef(csiClientObj, "MonitorInterval", 0);
            assert_int_equal(interval, csiClientListIndex * 100 + 10);

            // Check plugin
            swl_macChar_t swlMacAddr;
            strcpy(swlMacAddr.cMac, csiDeviceMacAddrList[csiClientListIndex]);
            swl_mac_charMatches(&(sensingVendorData->macAddr), &swlMacAddr);
            assert_int_equal(sensingVendorData->monitorInterval, interval);

            // Update client monitoring interval
            swl_typeUInt32_commitObjectParam(csiClientObj, "MonitorInterval", csiClientListIndex * 100 + 20);
            ttb_mockTimer_goToFutureMs(1);
            interval = swl_typeUInt32_fromObjectParamDef(csiClientObj, "MonitorInterval", 0);
            assert_int_equal(interval, csiClientListIndex * 100 + 20);
            assert_int_equal(sensingVendorData->monitorInterval, interval);

            // Set client monitoring interval up to max supported interval
            swl_typeUInt32_commitObjectParam(csiClientObj, "MonitorInterval", sensingVendorData->maxMonitorInterval + 100);
            ttb_mockTimer_goToFutureMs(1);
            interval = swl_typeUInt32_fromObjectParamDef(csiClientObj, "MonitorInterval", 0);
            assert_int_equal(interval, csiClientListIndex * 100 + 20);
            assert_int_equal(sensingVendorData->monitorInterval, interval);

            // Set client monitoring interval up to max supported interval, using addClient() API
            assert_false(sensingCommand(radObj, "addClient",
                                        csiDeviceMacAddrList[csiClientListIndex], sensingVendorData->maxMonitorInterval + 100));
            interval = swl_typeUInt32_fromObjectParamDef(csiClientObj, "MonitorInterval", 0);
            assert_int_equal(interval, csiClientListIndex * 100 + 20);
            assert_int_equal(sensingVendorData->monitorInterval, interval);
        }

        // Add new client with invalid MAC address
        assert_false(sensingCommand(radObj, "addClient",
                                    "40:07:4D:54:3F", sensingVendorData->maxMonitorInterval));

        // Add new client with valid MAC address and monitoring interval up to max supported interval
        assert_false(sensingCommand(radObj, "addClient",
                                    "40:07:4D:54:3F:9E", sensingVendorData->maxMonitorInterval + 100));

        // Add the 4th client with valid MAC address and valid monitoring interval, when the max supported clients is 3
        assert_false(sensingCommand(radObj, "addClient",
                                    "40:07:4D:54:3F:9E", sensingVendorData->maxMonitorInterval));

        // Show stats test -------------------------------------------------------------------------//
        checkCSICounters(radObj);

        // Reset stats test ------------------------------------------------------------------------//
        checkCSIResettedCounters(radObj);

        for(csiClientListIndex = 0; csiClientListIndex < NB_CLIENT_DEVICE; csiClientListIndex++) {
            // Delete csi client test ------------------------------------------------------------------//
            sensingCommand(radObj, "delClient", csiDeviceMacAddrList[csiClientListIndex], 0);

            csiClientObj = getCsiClientChildObject(bandTab[tabIndex], csiClientListIndex + 1);
            assert_null(csiClientObj);
        }
    }
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "csi");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sensing_enable),
        cmocka_unit_test(test_sensing_start_stop),
    };
    int rc = cmocka_run_group_tests(tests, setup_suite, teardown_suite);
    sahTraceClose();
    return rc;
}

