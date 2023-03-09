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
    rad->runningChannelBandwidth = SWL_BW_20MHZ;
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
    rad->runningChannelBandwidth = SWL_BW_20MHZ;
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
    rad->runningChannelBandwidth = SWL_BW_80MHZ;
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
    assert_int_equal(SWL_RC_OK, s_setTargetAndWait(rad, chanspec5, false, CHAN_REASON_MANUAL, NULL));
    printf("SET %u \n", swl_typeChanspecExt_equals(rad->targetChanspec.chanspec, rad->targetChanspec.chanspec));

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
    rad->runningChannelBandwidth = SWL_BW_80MHZ;
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
    rad->runningChannelBandwidth = SWL_BW_80MHZ;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj;
    ttb_object_t* targetObj = ttb_object_getChildObject(radObj, "ChannelMgt.TargetChanspec");

    amxd_object_set_uint8_t(radObj, "Channel", 112);

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
    amxd_object_set_uint8_t(radObj, "Channel", 37);
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
    rad->runningChannelBandwidth = SWL_BW_80MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    rad->targetChanspec.chanspec.bandwidth = SWL_BW_80MHZ;
    ttb_object_t* radObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj;
    ttb_object_t* chanMgtObj = ttb_object_getChildObject(radObj, "ChannelMgt");

    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(52, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    s_setTargetAndWait(rad, chanspec, true, CHAN_REASON_MANUAL, NULL);
    char* data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Current", data);
    free(data);

    wld_chanmgt_reportCurrentChanspec(rad, chanspec, CHAN_REASON_MANUAL);
    data = swl_typeCharPtr_fromObjectParamDef(chanMgtObj, "ChanspecShowing", NULL);
    assert_string_equal("Sync", data);
    free(data);

    amxd_object_set_uint8_t(radObj, "Channel", 100);
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
    rad->runningChannelBandwidth = SWL_BW_20MHZ;
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
    amxc_var_add_new_key_cstring_t(arg, "reason", "AUTO");
    amxc_var_add_new_key_cstring_t(arg, "reasonExt", "Just testing...");

    ttb_var_t* replyVar;
    reply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", &arg, &replyVar);
    assert_true(ttb_object_replySuccess(reply));
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
    rad->runningChannelBandwidth = SWL_BW_20MHZ;
    rad->currentChanspec.chanspec.bandwidth = SWL_BW_20MHZ;

    ttb_var_t* arg = ttb_object_createArgs();
    amxc_var_add_new_key_int32_t(arg, "channel", 11);
    amxc_var_add_new_key_cstring_t(arg, "bandwidth", "40MHz");
    amxc_var_add_new_key_cstring_t(arg, "reason", "MANUAL");
    amxc_var_add_new_key_bool(arg, "direct", true);

    ttb_var_t* replyVar;
    ttb_reply_t* myReply = ttb_object_callFun(dm.ttbBus, rad->pBus, "setChanspec", &arg, &replyVar);
    assert_true(ttb_object_replySuccess(myReply));
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

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_setTargetChanspec),
        cmocka_unit_test(test_wld_reportCurrentChanspec),
        cmocka_unit_test(test_wld_setChannel),
        cmocka_unit_test(test_wld_checkSync),
        cmocka_unit_test(test_wld_setChanspec),
        cmocka_unit_test(test_wld_checkChannelChange),
    };
    ttb_util_setFilter();
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}
