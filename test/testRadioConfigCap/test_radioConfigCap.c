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
#include "test-toolbox/ttb_mockClock.h"
#include "test-toolbox/ttb_object.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "../testHelper/wld_th_radio.h"

static wld_th_dm_t dm;

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dmEnv_init(&dm));
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

wld_th_radCap_t testCap2 = {
    .name = "wifi0",
    .operatingFrequencyBand = SWL_FREQ_BAND_EXT_2_4GHZ,
    .supportedFrequencyBands = M_SWL_FREQ_BAND_2_4GHZ,
    .possibleChannels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
    .nrPossibleChannels = 13,
    .maxChannelBandwidth = SWL_BW_40MHZ,
    .operatingChannelBandwidth = SWL_RAD_BW_20MHZ,
    .channel = 1,
    .supportedStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G | M_SWL_RADSTD_N | M_SWL_RADSTD_AX | M_SWL_RADSTD_BE,
    .cap = {
        .apCap7 = {
            .emlmrSupported = true,
            .emlsrSupported = true,
            .strSupported = true,
            .nstrSupported = true
        },
        .staCap7 = {
            .emlmrSupported = true,
            .emlsrSupported = false,
            .strSupported = true,
            .nstrSupported = false
        }
    }
};


wld_th_radCap_t testCap5 = {
    .name = "wifi1",
    .operatingFrequencyBand = SWL_FREQ_BAND_EXT_5GHZ,
    .supportedFrequencyBands = M_SWL_FREQ_BAND_5GHZ,
    .possibleChannels = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140},
    .nrPossibleChannels = 19,
    .maxChannelBandwidth = SWL_BW_160MHZ,
    .operatingChannelBandwidth = SWL_RAD_BW_160MHZ,
    .channel = 36,
    .supportedStandards = M_SWL_RADSTD_A | M_SWL_RADSTD_N | M_SWL_RADSTD_AC | M_SWL_RADSTD_AX | M_SWL_RADSTD_BE,
    .cap = {
        .apCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = true,
            .nstrSupported = false
        },
        .staCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = false,
            .nstrSupported = true
        }
    }
};

static void test_radioStatus(void** state _UNUSED) {
    wld_th_radio_addCustomCap(&testCap2);
    wld_th_radio_addCustomCap(&testCap5);
    T_Radio* pRad2 = wld_th_radio_create(dm.ttbBus->bus_ctx, dm.mockVendor, "wifi0");
    T_Radio* pRad5 = wld_th_radio_create(dm.ttbBus->bus_ctx, dm.mockVendor, "wifi1");

    amxp_sigmngr_trigger_signal(&dm.ttbBus->dm.sigmngr, "app:start", NULL);
    ttb_mockTimer_goToFutureMs(10000);

    amxd_object_t* capObj2 = amxd_object_findf(pRad2->pBus, "Capabilities");
    amxd_object_t* capObj5 = amxd_object_findf(pRad5->pBus, "Capabilities");

    ttb_object_assertPrintEqFile(capObj2, 0, "rad2_cap.txt");
    ttb_object_assertPrintEqFile(capObj5, 0, "rad5_cap.txt");


}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "ssid");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_radioStatus),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

