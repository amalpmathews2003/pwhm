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
#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "wld_chanmgt.h"
#include "../testHelper/wld_th_dm.h"

static wld_th_dm_t dm;

static int setup_suite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void s_checkChannels(T_Radio* rad, char* expectedClear, char* expectedDfs) {
    ttb_object_t* chanMgtObj = ttb_object_getChildObject(wld_rad_getObject(rad), "ChannelMgt");
    assert_non_null(chanMgtObj);

    char* data = NULL;
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ClearedDfsChannels", NULL);
    printf("DFS Clear: %s\n", data);
    assert_string_equal(data, expectedClear);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "RadarTriggeredDfsChannels", NULL);
    printf("DFS Trigger: %s\n", data);
    assert_string_equal(data, expectedDfs);
    free(data);
}

static void test_startstop(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    ttb_object_t* obj = dm.bandList[SWL_FREQ_BAND_5GHZ].radObj;

    char* chandata = swl_typeCharPtr_fromObjectParamDef(obj, "Channel", NULL);
    printf("channel %s\n", chandata);
    free(chandata);

    char* data = swl_typeCharPtr_fromObjectParamDef(obj, "PossibleChannels", NULL);
    printf("chan: %s\n", data);
    assert_string_equal("36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128", data);
    free(data);

    s_checkChannels(rad, "", "");

    for(int i = 0; i < 4; i++) {
        swl_chanspec_t cs = SWL_CHANSPEC_NEW(rad->possibleChannels[8 + i], SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ);
        wld_channel_clear_passive_channel(cs);
    }
    wld_chanmgt_writeDfsChannels(rad);


    s_checkChannels(rad, "100,104,108,112", "");


    for(int i = 0; i < 4; i++) {
        swl_chanspec_t cs = SWL_CHANSPEC_NEW(rad->possibleChannels[8 + i], SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ);
        wld_channel_mark_passive_channel(cs);
        wld_channel_mark_radar_detected_channel(cs);
    }
    wld_chanmgt_writeDfsChannels(rad);

    s_checkChannels(rad, "", "100,104,108,112");


    T_DFSEvent evt;
    evt.bandwidth = rad->operatingChannelBandwidth;
    evt.channel = 100;
    evt.radarZone = RADARZONE_ETSI;
    evt.radarIndex = 1;
    evt.timestamp = swl_time_getRealSec();
    wld_rad_addDFSEvent(rad, &evt);

    ttb_object_t* dfsEvent = ttb_object_getChildObject(obj, "DFS.Event.1");
    assert_non_null(dfsEvent);
    data = swl_typeCharPtr_fromObjectParamDef(dfsEvent, "DFSRadarDetectionList", NULL);
    assert_string_equal(data, "100,104,108,112");
    free(data);
    assert_int_equal(1, swl_typeUInt8_fromObjectParamDef(dfsEvent, "RadarIndex", 0));
    assert_int_equal(100, swl_typeUInt32_fromObjectParamDef(dfsEvent, "Channel", 0));
    assert_int_equal(36, swl_typeUInt32_fromObjectParamDef(dfsEvent, "NewChannel", 0));


    for(int i = 0; i < 2; i++) {
        swl_chanspec_t cs = SWL_CHANSPEC_NEW(rad->possibleChannels[4 + i], SWL_BW_20MHZ, rad->operatingFrequencyBand);
        wld_channel_mark_passive_channel(cs);
        wld_channel_mark_radar_detected_channel(cs);
    }

    wld_chanmgt_writeDfsChannels(rad);

    s_checkChannels(rad, "", "52,56,100,104,108,112");

    evt.channel = 52;
    evt.radarZone = RADARZONE_ETSI;
    evt.radarIndex = 2;
    evt.timestamp = swl_time_getRealSec();
    wld_rad_addDFSEvent(rad, &evt);
    dfsEvent = ttb_object_getChildObject(obj, "DFS.Event.2");
    data = swl_typeCharPtr_fromObjectParamDef(dfsEvent, "DFSRadarDetectionList", NULL);
    assert_string_equal(data, "52,56");
    free(data);
    assert_int_equal(2, swl_typeUInt8_fromObjectParamDef(dfsEvent, "RadarIndex", 0));
    assert_int_equal(52, swl_typeUInt32_fromObjectParamDef(dfsEvent, "Channel", 0));
    assert_int_equal(36, swl_typeUInt32_fromObjectParamDef(dfsEvent, "NewChannel", 0));

}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_startstop),
    };
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}

