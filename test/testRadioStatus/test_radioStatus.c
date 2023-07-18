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
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "wld_util.h"
#include "test-toolbox/ttb_mockClock.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
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

static void s_performCheck(ttb_object_t* radObj, wld_status_changeInfo_t* targetData, const char* status) {
    char* odlStatus = NULL;

    assert_true(swl_typeCharPtr_fromObjectParam(radObj, "Status", &odlStatus));
    assert_string_equal(odlStatus, status);
    free(odlStatus);

    swl_timeMono_t changeTime = 0;
    assert_true(swl_typeTimeMono_fromObjectParam(radObj, "LastStatusChangeTimeStamp", &changeTime));
    assert_int_equal(changeTime, targetData->lastStatusChange);

    targetData->lastStatusHistogramUpdate = swl_time_getMonoSec();

    ttb_var_t* replyVar;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, radObj, "getStatusHistogram", NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));

    wld_status_changeInfo_t testData;
    memset(&testData, 0, sizeof(wld_status_changeInfo_t));
    assert_true(swl_type_fromVariant((swl_type_t*) &gtWld_status_changeInfo, &testData, replyVar));

    assert_true(swl_typeTimeMono_equals(testData.lastStatusHistogramUpdate, targetData->lastStatusHistogramUpdate));

    char buf1[256] = {0};
    char buf2[256] = {0};
    swl_typeUInt32_arrayToChar(buf1, sizeof(buf1), targetData->statusHistogram.data, RST_MAX);
    swl_typeUInt32_arrayToChar(buf2, sizeof(buf2), testData.statusHistogram.data, RST_MAX);

    swl_ttb_assertTypeArrayEquals(&gtSwl_type_uint32,
                                  targetData->statusHistogram.data, RST_MAX,
                                  testData.statusHistogram.data, RST_MAX);

    swl_ttb_assertTypeValEquals(&gtSwl_type_timeMono, testData.lastDisableTime, targetData->lastDisableTime);
    swl_ttb_assertTypeValEquals(&gtSwl_type_timeMono, testData.lastEnableTime, targetData->lastEnableTime);
    swl_ttb_assertTypeValEquals(&gtSwl_type_timeMono, testData.lastStatusChange, targetData->lastStatusChange);


    ttb_object_cleanReply(&reply, &replyVar);
}

typedef struct {
    wld_status_changeInfo_t* targetData;
    wld_status_e status;
    bool enable;
} radioStatusTestState_t;

typedef struct {
    wld_th_dmBand_t* bandInfo;
    radioStatusTestState_t radState;
    radioStatusTestState_t vapState;
} radioStatusGlobalState_t;

static void s_incrementTime(radioStatusGlobalState_t* state, uint32_t sec, uint32_t line) {
    printf("%u : Increment time by -- %u --\n", line, sec);
    ttb_mockClock_addTime(sec, 0);
    state->radState.targetData->statusHistogram.data[state->radState.status] += sec;
    state->vapState.targetData->statusHistogram.data[state->vapState.status] += sec;
    s_performCheck(state->bandInfo->radObj, state->radState.targetData, wld_status_str[state->radState.status]);
    s_performCheck(state->bandInfo->vapPrivSSIDObj, state->vapState.targetData, wld_status_str[state->vapState.status]);
}

static void s_updateSingleStatus(radioStatusTestState_t* state, bool enable, wld_status_e status) {
    swl_timeMono_t now = swl_time_getMonoSec();
    if(state->status != status) {
        state->targetData->lastStatusChange = now;
        state->status = status;
        state->targetData->nrStatusChanges++;
    }
    if(state->enable != enable) {
        if(enable) {
            state->targetData->lastEnableTime = now;
        } else {
            state->targetData->lastDisableTime = now;
        }
        state->enable = enable;
    }
}

static void s_updateStatus(radioStatusGlobalState_t* state, bool radEnable, wld_status_e radStatus, bool vapEnable, wld_status_e vapStatus, uint32_t line) {
    printf("%u : update status rad %u %s / vap %u %s\n", line, radEnable, wld_status_str[radStatus], vapEnable, wld_status_str[vapStatus]);
    s_updateSingleStatus(&state->radState, radEnable, radStatus);
    s_updateSingleStatus(&state->vapState, vapEnable, vapStatus);

    printf("Check rad\n");
    s_performCheck(state->bandInfo->radObj, state->radState.targetData, wld_status_str[radStatus]);
    printf("Check vap\n");
    s_performCheck(state->bandInfo->vapPrivSSIDObj, state->vapState.targetData, wld_status_str[vapStatus]);
}

static void test_radioStatus(void** state _UNUSED) {
    wld_th_dmBand_t* bandInfo = &dm.bandList[SWL_FREQ_BAND_5GHZ];
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    T_SSID* pSSID = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPrivSSID;
    wld_th_ep_setEnable(bandInfo->ep, false, true);

    wld_status_changeInfo_t radTargetData;
    memset(&radTargetData, 0, sizeof(wld_status_changeInfo_t));
    wld_status_changeInfo_t vapTargetData;
    memset(&vapTargetData, 0, sizeof(wld_status_changeInfo_t));

    wld_util_updateStatusChangeInfo(&pRad->changeInfo, pRad->status);
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);

    memcpy(&radTargetData, &pRad->changeInfo, sizeof(wld_status_changeInfo_t));
    memcpy(&vapTargetData, &bandInfo->vapPrivSSID->changeInfo, sizeof(wld_status_changeInfo_t));

    radioStatusGlobalState_t globalState;
    globalState.bandInfo = bandInfo;
    globalState.radState.targetData = &radTargetData;
    globalState.radState.enable = true;
    globalState.radState.status = RST_UP;

    globalState.vapState.targetData = &vapTargetData;
    globalState.vapState.enable = true;
    globalState.vapState.status = RST_UP;

    s_updateStatus(&globalState, true, RST_UP, true, RST_UP, __LINE__);
    s_incrementTime(&globalState, 5, __LINE__);


    wld_th_rad_setRadEnable(pRad, false, true);
    s_updateStatus(&globalState, false, RST_DOWN, true, RST_DOWN, __LINE__);

    s_incrementTime(&globalState, 2, __LINE__);


    pRad->detailedState = CM_RAD_DEEP_POWER_DOWN;
    wld_rad_updateState(pRad, true);


    s_incrementTime(&globalState, 2, __LINE__);

    wld_th_rad_setRadEnable(pRad, true, true);
    s_updateStatus(&globalState, true, RST_UP, true, RST_UP, __LINE__);

    s_incrementTime(&globalState, 3, __LINE__);

    // Do dfs clear
    pRad->detailedState = CM_RAD_FG_CAC;
    wld_rad_updateState(pRad, true);

    s_updateStatus(&globalState, true, RST_DORMANT, true, RST_LLDOWN, __LINE__);

    s_incrementTime(&globalState, 60, __LINE__);



    pRad->detailedState = CM_RAD_UP;
    wld_rad_updateState(pRad, true);
    s_updateStatus(&globalState, true, RST_UP, true, RST_UP, __LINE__);

    s_incrementTime(&globalState, 100, __LINE__);

    wld_th_vap_setSSIDEnable(bandInfo->vapPriv, false, true);

    s_updateStatus(&globalState, true, RST_DORMANT, false, RST_DOWN, __LINE__);


    s_incrementTime(&globalState, 200, __LINE__);

    wld_th_vap_setSSIDEnable(bandInfo->vapPriv, true, true);

    s_updateStatus(&globalState, true, RST_UP, true, RST_UP, __LINE__);

    s_incrementTime(&globalState, 100, __LINE__);

    pRad->detailedState = CM_RAD_ERROR;
    wld_rad_updateState(pRad, true);

    s_updateStatus(&globalState, true, RST_ERROR, true, RST_ERROR, __LINE__);

    s_incrementTime(&globalState, 700, __LINE__);
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

