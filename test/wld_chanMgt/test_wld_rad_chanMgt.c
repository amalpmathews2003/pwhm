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
#include "wld_chanmgt.h"
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "wld_channel.h"
#include "wld_chanmgt.h"
#include "test-toolbox/ttb_mockClock.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
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

static swl_rc_ne s_setTargetAndWait(T_Radio* pR, swl_chanspec_t chanspec, bool direct, wld_channelChangeReason_e reason, const char* reasonExt) {
    swl_rc_ne retVal = wld_chanmgt_setTargetChanspec(pR, chanspec, direct, reason, reasonExt);
    ttb_mockTimer_goToFutureMs(1);
    return retVal;
}

static void test_wld_setTargetChanspec(void** state _UNUSED) {
    T_Radio* rad = NULL;
    swl_chanspec_t chanspec2 = SWL_CHANSPEC_NEW(6, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);

    /* NULL radio */
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec2, true, CHAN_REASON_MANUAL, NULL));

    /********************
    * 2.4GHz
    ********************/
    rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    assert_non_null(rad);
    ttb_object_t* obj = ttb_object_getChildObject(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].radObj, "ChannelMgt.TargetChanspec");

    assert_non_null(obj);

    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec2, true, CHAN_REASON_MANUAL, NULL));
    ttb_mockTimer_goToFutureMs(1);

    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec2);
    assert_int_equal(6, swl_typeUInt8_fromObjectParamDef(obj, "Channel", 0));
    char* data = swl_typeCharPtr_fromObjectParamDef(obj, "Bandwidth", NULL);
    assert_string_equal("20MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Frequency", NULL);
    assert_string_equal("2.4GHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Reason", NULL);
    assert_string_equal("PERSISTANCE", data);
    free(data);

    /* Invalid channel */
    chanspec2.channel = 64;
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec2, true, CHAN_REASON_MANUAL, NULL));
    swl_ttb_assertTypeNotEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec2);

    /* Same channel */
    rad->channel = 6;
    rad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec2, true, CHAN_REASON_MANUAL, NULL));

    /********************
    * 5GHz
    ********************/

    rad = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    assert_non_null(rad);
    obj = ttb_object_getChildObject(dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj, "ChannelMgt.TargetChanspec");

    swl_chanspec_t chanspec5 = SWL_CHANSPEC_NEW(100, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_AUTO, NULL));

    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec5);
    assert_int_equal(100, swl_typeUInt8_fromObjectParamDef(obj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Bandwidth", NULL);
    assert_string_equal("80MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Reason", NULL);
    assert_string_equal("AUTO", data);
    free(data);

    chanspec5.channel = 1;
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_AUTO, NULL));
    swl_ttb_assertTypeNotEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec5);

    /* Has radar */
    swl_chanspec_t csRadar = SWL_CHANSPEC_NEW(52, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    wld_channel_mark_radar_detected_band(csRadar);
    chanspec5.channel = 52;
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_AUTO, NULL));
    swl_ttb_assertTypeNotEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec5);

    wld_channel_clear_radar_detected_channel(csRadar);

    chanspec5.channel = 140;
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_MANUAL, NULL));

    swl_chanspec_t targetSpec140 = SWL_CHANSPEC_NEW(140, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &targetSpec140);


    chanspec5.channel = 136;
    chanspec5.bandwidth = SWL_BW_AUTO;
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_MANUAL, NULL));

    swl_chanspec_t targetSpec136 = SWL_CHANSPEC_NEW(136, SWL_BW_40MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &targetSpec136);

    chanspec5.channel = 100;
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec5, true, CHAN_REASON_MANUAL, NULL));

    swl_chanspec_t targetSpec100_final = SWL_CHANSPEC_NEW(100, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &targetSpec100_final);

    /********************
    * 6GHz
    ********************/
    rad = dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].rad;
    assert_non_null(rad);
    obj = ttb_object_getChildObject(dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].radObj, "ChannelMgt.TargetChanspec");

    swl_chanspec_t chanspec6 = SWL_CHANSPEC_NEW(69, SWL_BW_160MHZ, SWL_FREQ_BAND_EXT_6GHZ);
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec6, true, CHAN_REASON_INITIAL, NULL));

    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->targetChanspec.chanspec, &chanspec6);
    assert_int_equal(69, swl_typeUInt8_fromObjectParamDef(obj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Bandwidth", NULL);
    assert_string_equal("160MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(obj, "Reason", NULL);
    assert_string_equal("INITIAL", data);
    free(data);

    chanspec6.channel = 22;
    assert_int_equal(SWL_RC_ERROR, s_setTargetAndWait(rad, chanspec6, true, CHAN_REASON_INITIAL, NULL));

}

/* Test report */
static void test_wld_reportCurrentChanspec(void** state _UNUSED) {
    T_Radio* rad = NULL;
    swl_chanspec_t chanspec2 = SWL_CHANSPEC_NEW(11, SWL_BW_40MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);

    /* NULL radio */
    assert_int_equal(SWL_RC_ERROR, wld_chanmgt_reportCurrentChanspec(rad, chanspec2, CHAN_REASON_MANUAL));

    /* 2.4GHz */
    rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    rad->channel = 1;
    rad->currentChanspec.chanspec.channel = 1;
    rad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_20MHZ;
    rad->channelChangeReason = CHAN_REASON_INITIAL;
    rad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].radObj;
    ttb_object_t* curChanObj = ttb_object_getChildObject(radObj, "ChannelMgt.CurrentChanspec");

    assert_int_equal(SWL_RC_OK, wld_chanmgt_reportCurrentChanspec(rad, chanspec2, CHAN_REASON_MANUAL));
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->currentChanspec.chanspec, &chanspec2);
    assert_int_equal(11, swl_typeUInt8_fromObjectParamDef(curChanObj, "Channel", 0));
    char* data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Bandwidth", NULL);
    assert_string_equal("40MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Reason", NULL);
    assert_string_equal("MANUAL", data);
    free(data);
    assert_int_equal(11, rad->channel);
    assert_int_equal(SWL_BW_40MHZ, rad->runningChannelBandwidth);
    assert_int_equal(11, swl_typeUInt8_fromObjectParamDef(radObj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(radObj, "CurrentOperatingChannelBandwidth", NULL);
    assert_string_equal("40MHz", data);
    free(data);
    assert_int_equal(CHAN_REASON_MANUAL, rad->channelChangeReason);
    assert_int_equal(CHAN_REASON_MANUAL, rad->channelBandwidthChangeReason);

    /* 5GHz */

    rad = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    rad->channel = 36;
    memset(&rad->currentChanspec.chanspec, 0, sizeof(swl_chanspec_t));
    memset(&rad->targetChanspec.chanspec, 0, sizeof(swl_chanspec_t));
    rad->currentChanspec.chanspec.channel = 36;
    rad->targetChanspec.chanspec.channel = 36;
    rad->runningChannelBandwidth = SWL_RAD_BW_80MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    rad->currentChanspec.chanspec.band = SWL_FREQ_BAND_EXT_5GHZ;
    rad->targetChanspec.chanspec.band = SWL_FREQ_BAND_EXT_5GHZ;
    rad->channelChangeReason = CHAN_REASON_INITIAL;
    rad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    printf("SET %u \n", swl_typeChanspecExt_equals(rad->targetChanspec.chanspec, rad->targetChanspec.chanspec));


    radObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj;
    curChanObj = ttb_object_getChildObject(radObj, "ChannelMgt.CurrentChanspec");
    swl_chanspec_t chanspec5 = SWL_CHANSPEC_NEW(100, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);

    assert_int_equal(SWL_RC_OK, wld_chanmgt_reportCurrentChanspec(rad, chanspec5, CHAN_REASON_MANUAL));
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->currentChanspec.chanspec, &chanspec5);
    assert_int_equal(100, swl_typeUInt8_fromObjectParamDef(curChanObj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Bandwidth", NULL);
    assert_string_equal("80MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Reason", NULL);
    assert_string_equal("MANUAL", data);
    free(data);
    assert_int_equal(100, rad->channel);
    assert_int_equal(SWL_BW_80MHZ, rad->runningChannelBandwidth);
    assert_int_equal(100, swl_typeUInt8_fromObjectParamDef(radObj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(radObj, "CurrentOperatingChannelBandwidth", NULL);
    assert_string_equal("80MHz", data);
    free(data);
    assert_int_equal(CHAN_REASON_MANUAL, rad->channelChangeReason);
    /* Bandwidth did not change */
    assert_int_equal(CHAN_REASON_INITIAL, rad->channelBandwidthChangeReason);


    /* 6GHz */
    rad = dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].rad;
    rad->channel = 5;
    rad->currentChanspec.chanspec.channel = 5;
    rad->runningChannelBandwidth = SWL_RAD_BW_80MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    rad->channelChangeReason = CHAN_REASON_INITIAL;
    rad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    radObj = dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].radObj;
    curChanObj = ttb_object_getChildObject(radObj, "ChannelMgt.CurrentChanspec");
    swl_chanspec_t chanspec6 = SWL_CHANSPEC_NEW(69, SWL_BW_160MHZ, SWL_FREQ_BAND_EXT_6GHZ);

    assert_int_equal(SWL_RC_OK, wld_chanmgt_reportCurrentChanspec(rad, chanspec6, CHAN_REASON_AUTO));
    swl_ttb_assertTypeEquals(&gtSwl_type_chanspec, &rad->currentChanspec.chanspec, &chanspec6);
    assert_int_equal(69, swl_typeUInt8_fromObjectParamDef(curChanObj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Bandwidth", NULL);
    assert_string_equal("160MHz", data);
    free(data);
    data = swl_typeCharPtr_fromObjectParamDef(curChanObj, "Reason", NULL);
    assert_string_equal("AUTO", data);
    free(data);
    assert_int_equal(69, rad->channel);
    assert_int_equal(SWL_BW_160MHZ, rad->runningChannelBandwidth);
    assert_int_equal(69, swl_typeUInt8_fromObjectParamDef(radObj, "Channel", 0));
    data = swl_typeCharPtr_fromObjectParamDef(radObj, "CurrentOperatingChannelBandwidth", NULL);
    assert_string_equal("160MHz", data);
    free(data);
    assert_int_equal(CHAN_REASON_AUTO, rad->channelChangeReason);
    /* Bandwidth did not change */
    assert_int_equal(CHAN_REASON_AUTO, rad->channelBandwidthChangeReason);
}

static void test_wld_setChannel(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    rad->channel = 36;
    rad->targetChanspec.chanspec.channel = 36;
    rad->runningChannelBandwidth = SWL_RAD_BW_80MHZ;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj;
    ttb_object_t* targetObj = ttb_object_getChildObject(radObj, "ChannelMgt.TargetChanspec");

    swl_typeUInt8_commitObjectParam(radObj, "Channel", 112);

    ttb_amx_handleEvents();
    ttb_mockTimer_goToFutureMs(1);

    assert_int_equal(112, swl_typeUInt8_fromObjectParamDef(radObj, "Channel", 0));
    assert_int_equal(112, rad->channel);
    assert_int_equal(112, rad->targetChanspec.chanspec.channel);
    assert_int_equal(SWL_BW_80MHZ, rad->runningChannelBandwidth);
    assert_int_equal(SWL_BW_80MHZ, rad->targetChanspec.chanspec.bandwidth);
    swl_channel_t test = swl_typeUInt8_fromObjectParamDef(targetObj, "Channel", 0);
    printf("testChan %p %u\n", targetObj, test);

    assert_int_equal(112, test);

    /* Invalid channel (must not change) */
    swl_typeUInt8_commitObjectParam(radObj, "Channel", 37);
    ttb_mockTimer_goToFutureMs(1);
    assert_int_equal(112, rad->channel);
    assert_int_equal(112, rad->targetChanspec.chanspec.channel);
    assert_int_equal(112, swl_typeUInt8_fromObjectParamDef(radObj, "Channel", 0));
}

/* Test sync */
static void test_wld_checkSync(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    rad->channel = 36;
    rad->currentChanspec.chanspec.channel = 36;
    rad->targetChanspec.chanspec.channel = 36;
    rad->runningChannelBandwidth = SWL_RAD_BW_80MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj;
    ttb_object_t* chanMgtObj = ttb_object_getChildObject(radObj, "ChannelMgt");

    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(100, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    wld_channel_clear_radar_detected_band(chanspec);
    chanspec.channel = 52;
    wld_channel_clear_radar_detected_band(chanspec);

    s_setTargetAndWait(rad, chanspec, true, CHAN_REASON_MANUAL, NULL);
    char* data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Sync", data);
    free(data);

    wld_chanmgt_reportCurrentChanspec(rad, chanspec, CHAN_REASON_MANUAL);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Sync", data);
    free(data);

    swl_typeUInt8_commitObjectParam(radObj, "Channel", 100);
    ttb_mockTimer_goToFutureMs(1);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Target", data);
    free(data);

    chanspec.channel = 100;
    wld_chanmgt_reportCurrentChanspec(rad, chanspec, CHAN_REASON_MANUAL);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Sync", data);
    free(data);

    chanspec.channel = 36;
    wld_chanmgt_reportCurrentChanspec(rad, chanspec, CHAN_REASON_MANUAL);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Current", data);
    free(data);
}

/* Test RPC */
static void test_wld_setChanspec(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    rad->channel = 1;
    rad->targetChanspec.chanspec.channel = 1;
    rad->currentChanspec.chanspec.channel = 1;
    rad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_20MHZ;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].radObj;

    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", NULL, NULL);

    /* No channel */
    assert_false(ttb_object_replySuccess(reply));
    ttb_object_cleanReply(&reply, NULL);

    /* 1. Wrong channel
     * 2. Wrong bandwdith
     * 3. Wrong frequency band
     * 4. Same chanspec
     */
    swl_chanspec_t bandChanspecs[4] = {SWL_CHANSPEC_NEW(36, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ),
        SWL_CHANSPEC_NEW(1, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_2_4GHZ),
        SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_6GHZ),
        SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ)};

    for(int i = 0; i < 4; i++) {

        ttb_var_t* arg = ttb_object_createArgs();
        amxc_var_add_new_key_int32_t(arg, "channel", bandChanspecs[i].channel);
        amxc_var_add_new_key_cstring_t(arg, "bandwidth", swl_bandwidth_str[bandChanspecs[i].bandwidth]);
        amxc_var_add_new_key_cstring_t(arg, "frequency", swl_freqBand_str[bandChanspecs[i].band]);

        ttb_var_t* replyVar;
        ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", &arg, &replyVar);
        assert_false(ttb_object_replySuccess(reply));
        ttb_object_cleanReply(&reply, &replyVar);
    }


    ttb_var_t* arg = ttb_object_createArgs();
    amxc_var_add_new_key_int32_t(arg, "channel", 11);
    amxc_var_add_new_key_cstring_t(arg, "bandwidth", "40MHz");
    amxc_var_add_new_key_bool(arg, "direct", true);
    amxc_var_add_new_key_cstring_t(arg, "reason", "AUTO");
    amxc_var_add_new_key_cstring_t(arg, "reasonExt", "Just testing...");

    ttb_var_t* replyVar;
    reply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", &arg, &replyVar);
    assert_true(ttb_object_isReplyDeferred(reply));
    ttb_object_cleanReply(&reply, &replyVar);


    assert_int_equal(11, rad->targetChanspec.chanspec.channel);
    assert_int_equal(SWL_BW_40MHZ, rad->targetChanspec.chanspec.bandwidth);
    assert_int_equal(CHAN_REASON_AUTO, rad->targetChanspec.reason);

    ttb_mockTimer_goToFutureMs(1);

    char* data = swl_typeCharPtr_fromObjectParamDef(ttb_object_getChildObject(radObj, "ChannelMgt.TargetChanspec"), "ReasonExt", NULL);
    assert_string_equal("Just testing...", data);
    free(data);
}

/* Test ChannelChange */
static void test_wld_checkChannelChange(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    rad->channel = 1;
    rad->targetChanspec.chanspec.channel = 1;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_20MHZ;
    rad->currentChanspec.chanspec.channel = 1;
    rad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_20MHZ;

    ttb_var_t* arg = ttb_object_createArgs();
    amxc_var_add_new_key_int32_t(arg, "channel", 11);
    amxc_var_add_new_key_cstring_t(arg, "bandwidth", "40MHz");
    amxc_var_add_new_key_cstring_t(arg, "reason", "MANUAL");
    amxc_var_add_new_key_cstring_t(arg, "reasonExt", "Just testing...");
    amxc_var_add_new_key_bool(arg, "direct", false);

    ttb_var_t* replyVar;
    ttb_reply_t* myReply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", &arg, &replyVar);
    assert_true(ttb_object_isReplyDeferred(myReply));
    ttb_object_cleanReply(&myReply, &replyVar);


    ttb_mockTimer_goToFutureMs(1);

    wld_chanmgt_reportCurrentChanspec(rad, rad->targetChanspec.chanspec, CHAN_REASON_MANUAL);

    amxc_llist_t* changeList = &(rad->channelChangeList);
    amxc_llist_it_t* it = amxc_llist_get_last(changeList);
    assert_non_null(it);
    wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
    int32_t testVal = swl_typeInt32_fromObjectParamDef(change->object, "NewChannel", 0);
    assert_int_equal(11, testVal);
    swl_timeMono_t Time = 0;
    swl_typeTimeMono_fromObjectParam(change->object, "TimeStamp", &Time);
    swl_timeMono_t curTime = swl_time_getMonoSec();
    assert_int_equal((uint32_t) Time, (uint32_t) curTime);
}

typedef struct {
    swl_chanspec_t tgtChSpec;
    wld_rad_bwSelectMode_e autoBwMode;
    swl_bandwidth_e maxBw;
    swl_bandwidth_e expecBw;
} autoBwModetestInfo_t;

static void test_wld_setAutoBwMode(void** state _UNUSED) {
    T_Radio* pR = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    pR->pFA->mfn_wrad_supports(pR, NULL, 0);
    wld_channel_clear_passive_band((swl_chanspec_t) SWL_CHANSPEC_NEW(100, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));
    wld_channel_mark_radar_detected_band((swl_chanspec_t) SWL_CHANSPEC_NEW(60, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));

    autoBwModetestInfo_t tests[] = {
        //default limited to maxBw 80
        {
            SWL_CHANSPEC_NEW(36, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_DEFAULT, SWL_BW_80MHZ,
            SWL_BW_80MHZ,
        },
        //default limited to maxBw 40
        {
            SWL_CHANSPEC_NEW(36, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_DEFAULT, SWL_BW_40MHZ,
            SWL_BW_40MHZ,
        },
        //max available limited to 80 because of radar on 60/20
        {
            SWL_CHANSPEC_NEW(40, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXAVAILABLE, SWL_BW_160MHZ,
            SWL_BW_80MHZ,
        },
        //max cleared limited to 80 as prim is non-dfs, but no dfs chans has been cleared
        {
            SWL_CHANSPEC_NEW(40, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_160MHZ,
            SWL_BW_80MHZ,
        },
        //normal default 80
        {
            SWL_CHANSPEC_NEW(52, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_DEFAULT, SWL_BW_160MHZ,
            SWL_BW_80MHZ,
        },
        //limited max cleared because of detected radar in chan 60
        {
            SWL_CHANSPEC_NEW(52, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_160MHZ,
            SWL_BW_40MHZ,
        },
        //max cleared initiated to default 80 (no previous clear, and no radar detected)
        {
            SWL_CHANSPEC_NEW(128, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_160MHZ,
            SWL_BW_80MHZ,
        },
        //max available limited to 20, last supported channel
        {
            SWL_CHANSPEC_NEW(140, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXAVAILABLE, SWL_BW_160MHZ,
            SWL_BW_20MHZ,
        },
        //normal default 80
        {
            SWL_CHANSPEC_NEW(100, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_DEFAULT, SWL_BW_80MHZ,
            SWL_BW_80MHZ,
        },
        //max available with all chanset supported withing max bw
        {
            SWL_CHANSPEC_NEW(100, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXAVAILABLE, SWL_BW_160MHZ,
            SWL_BW_160MHZ,
        },
        //max cleared limited to only cleared 100/20
        {
            SWL_CHANSPEC_NEW(100, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_160MHZ,
            SWL_BW_20MHZ,
        },
        //normal default 20
        {
            SWL_CHANSPEC_NEW(1, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ), BW_SELECT_MODE_DEFAULT, SWL_BW_40MHZ,
            SWL_BW_20MHZ,
        },
        //max available on 2.4 aligned with max bw 20
        {
            SWL_CHANSPEC_NEW(6, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ), BW_SELECT_MODE_MAXAVAILABLE, SWL_BW_20MHZ,
            SWL_BW_20MHZ,
        },
        //max available on 2.4 aligned with max bw 40
        {
            SWL_CHANSPEC_NEW(11, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_40MHZ,
            SWL_BW_40MHZ,
        },
        //error case: primary channel 14 is not supported
        {
            SWL_CHANSPEC_NEW(14, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ), BW_SELECT_MODE_MAXCLEARED, SWL_BW_40MHZ,
            SWL_BW_AUTO,
        },
        //error case: looking for max available, but has unknown max bw
        {
            SWL_CHANSPEC_NEW(3, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ), BW_SELECT_MODE_MAXAVAILABLE, SWL_BW_AUTO,
            SWL_BW_AUTO,
        },
    };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        swl_bandwidth_e resBw = wld_chanmgt_getAutoBwExt(tests[i].autoBwMode, tests[i].maxBw, tests[i].tgtChSpec);
        printf("#### [%d] in chspec(%s) maxBw(%d) autoBwMode(%s) / expec bw(%d) / out bw(%d)\n",
               i, swl_typeChanspecExt_toBuf32(tests[i].tgtChSpec).buf, swl_chanspec_bwToInt(tests[i].maxBw), wld_rad_autoBwSelectMode_str[tests[i].autoBwMode],
               swl_chanspec_bwToInt(tests[i].expecBw),
               swl_chanspec_bwToInt(resBw));
        assert_int_equal(resBw, tests[i].expecBw);
    }
    wld_channel_mark_passive_band((swl_chanspec_t) SWL_CHANSPEC_NEW(100, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));
    wld_channel_clear_radar_detected_band((swl_chanspec_t) SWL_CHANSPEC_NEW(60, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));
}

static void test_wld_setAutoBwModeMaxCleared(void** state _UNUSED) {
    T_Radio* pR = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    pR->pFA->mfn_wrad_supports(pR, NULL, 0);

    autoBwModetestInfo_t test;
    test.autoBwMode = BW_SELECT_MODE_MAXCLEARED;
    test.maxBw = SWL_BW_160MHZ;

    test.tgtChSpec = (swl_chanspec_t) SWL_CHANSPEC_NEW(40, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ);

    //max cleared initiated to default 80 (no previous clear, and no radar detected)
    swl_bandwidth_e resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_80MHZ);

    //max cleared extended to default 160 (prim nodfs + 52/80 cleared)
    wld_channel_clear_passive_band((swl_chanspec_t) SWL_CHANSPEC_NEW(52, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ));
    resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_160MHZ);

    test.tgtChSpec = (swl_chanspec_t) SWL_CHANSPEC_NEW(100, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ);

    //max cleared initiated to default 80 (no previous clear, and no radar detected)
    resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_80MHZ);

    wld_channel_clear_passive_band((swl_chanspec_t) SWL_CHANSPEC_NEW(100, SWL_BW_160MHZ, SWL_FREQ_BAND_EXT_5GHZ));

    //max cleared extended to 160
    resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_160MHZ);

    wld_channel_mark_radar_detected_band((swl_chanspec_t) SWL_CHANSPEC_NEW(108, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));

    //max cleared reduced to 40 because of radar detected in 108/20
    resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_40MHZ);

    wld_channel_mark_radar_detected_band((swl_chanspec_t) SWL_CHANSPEC_NEW(100, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ));

    //max cleared null because of radar detected also on prim chan 100
    resBw = wld_chanmgt_getAutoBwExt(test.autoBwMode, test.maxBw, test.tgtChSpec);
    assert_int_equal(resBw, SWL_BW_AUTO);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_setTargetChanspec),
        cmocka_unit_test(test_wld_reportCurrentChanspec),
        cmocka_unit_test(test_wld_setChannel),
        cmocka_unit_test(test_wld_checkSync),
        cmocka_unit_test(test_wld_setChanspec),
        cmocka_unit_test(test_wld_checkChannelChange),
        cmocka_unit_test(test_wld_setAutoBwMode),
        cmocka_unit_test(test_wld_setAutoBwModeMaxCleared),
    };
    ttb_util_setFilter();
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}
