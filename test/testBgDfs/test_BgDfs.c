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
#include "wld_bgdfs.h"
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

T_Radio pRad;
wld_startBgdfsArgs_t args;
wld_startBgdfsArgs_t dfsArgs;
struct S_CWLD_FUNC_TABLE functionTable;

/**
 * These two functions are implemented in wld_bgdfs.c
 * to facilitate testability of _startBgDfsClear and
 * _stopBgDfsClear. Since it's better not to expose them
 * on the interface, they are simply forward declared here
 */
amxd_status_t bgdfs_startClear(T_Radio* pR,
                               uint32_t bandwidth,
                               swl_channel_t channel,
                               wld_startBgdfsArgs_t* dfsArgs);
amxd_status_t bgdfs_stopClear(T_Radio* pRad);


static void clearStructs() {
    memset(&pRad, 0, sizeof(T_Radio));
    pRad.debug = RAD_POINTER;
    memset(&args, 0, sizeof(wld_startBgdfsArgs_t));
    memset(&dfsArgs, 0, sizeof(wld_startBgdfsArgs_t));
    memset(&functionTable, 0, sizeof(struct S_CWLD_FUNC_TABLE));
}

/**
 * Mocked functions
 */
static int mfn_wrad_bgdfs_start_ext_notImplemented(T_Radio* rad _UNUSED,
                                                   wld_startBgdfsArgs_t* args _UNUSED) {
    return WLD_ERROR_NOT_IMPLEMENTED;
}
static int mfn_wrad_bgdfs_start_ext_returnsError(T_Radio* rad _UNUSED,
                                                 wld_startBgdfsArgs_t* args _UNUSED) {
    return WLD_ERROR_INVALID_PARAM;
}
static int mfn_wrad_bgdfs_start_ext_returnsOkDone(T_Radio* rad _UNUSED,
                                                  wld_startBgdfsArgs_t* args _UNUSED) {
    return WLD_OK_DONE;
}
static int mfn_wrad_bgdfs_start_returnsOkDone(T_Radio* rad _UNUSED,
                                              int channel _UNUSED) {
    return WLD_OK_DONE;
}

static int mfn_wrad_bgdfs_stop_returnsOK(T_Radio* rad _UNUSED) {
    return WLD_OK_DONE;
}
static int mfn_wrad_bgdfs_stop_fails(T_Radio* rad _UNUSED) {
    return WLD_ERROR_INVALID_PARAM;
}

/**
 * Tests
 */
static void test_isRunningReturnsFalseWhenIdle(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_IDLE;
    assert_false(wld_bgdfs_isRunning(&pRad));
}

static void test_isRunningReturnsFalseWhenOff(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_OFF;
    assert_false(wld_bgdfs_isRunning(&pRad));
}
static void test_isRunningReturnsTrueWhenClear(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    assert_true(wld_bgdfs_isRunning(&pRad));
}

static void test_isRunningReturnsTrueWhenClearExt(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    assert_true(wld_bgdfs_isRunning(&pRad));
}

static void test_startDfsExtImplementedSucceeds(void** state _UNUSED) {
    clearStructs();
    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_returnsOkDone;

    assert_int_equal(wld_bgdfs_startExt(&pRad, &args), WLD_OK_DONE);
}

static void test_startDfsExtIsImplementedButFails(void** state _UNUSED) {
    clearStructs();
    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_returnsError;

    assert_int_equal(wld_bgdfs_startExt(&pRad, &args), WLD_ERROR_INVALID_PARAM);
}

static void test_startDfsExtNotImplemented(void** state _UNUSED) {
    clearStructs();
    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_notImplemented;
    functionTable.mfn_wrad_bgdfs_start = mfn_wrad_bgdfs_start_returnsOkDone;

    assert_int_equal(wld_bgdfs_startExt(&pRad, &args), WLD_OK_DONE);
}

static void test_startClear(void** state _UNUSED) {
    clearStructs();
    swl_channel_t channel = 0;
    swl_bandwidth_e bandwidth = SWL_BW_20MHZ;

    for(uint8_t type = BGDFS_TYPE_CLEAR; type < BGDFS_TYPE_MAX; type++) {
        wld_bgdfs_notifyClearStarted(&pRad, channel, bandwidth, type);

        assert_int_equal(pRad.bgdfs_config.channel, channel);
        assert_int_equal(pRad.bgdfs_config.bandwidth, bandwidth);
        assert_int_equal(pRad.bgdfs_config.type, type);
        assert_int_equal(pRad.bgdfs_stats.nrClearStart[type], 1);
    }
}

static void test_endClearResultOKClear(void** state _UNUSED) {
    for(uint8_t type = BGDFS_TYPE_CLEAR; type < BGDFS_TYPE_MAX; type++) {
        clearStructs();
        pRad.bgdfs_config.type = type;
        wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OK);

        assert_int_equal(pRad.bgdfs_stats.nrClearSuccess[type], 1);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar[type], 0);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailOther[type], 0);
        /* if not BGDFS_TYPE_CLEAR_CONTINUOUS, it will stop */
        assert_int_equal(pRad.bgdfs_stats.nrClearStopQuit[type], !!(type != BGDFS_TYPE_CLEAR_CONTINUOUS));
    }
}

static void test_endClearResultFailRadarClear(void** state _UNUSED) {
    for(uint8_t type = BGDFS_TYPE_CLEAR; type < BGDFS_TYPE_MAX; type++) {
        clearStructs();
        pRad.bgdfs_config.type = type;
        wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_RADAR);

        assert_int_equal(pRad.bgdfs_stats.nrClearSuccess[type], 0);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar[type], 1);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailOther[type], 0);
        assert_int_equal(pRad.bgdfs_stats.nrClearStopQuit[type], 1);
    }
}

static void test_endClearResultFailOtherClear(void** state _UNUSED) {
    for(uint8_t type = BGDFS_TYPE_CLEAR; type < BGDFS_TYPE_MAX; type++) {
        clearStructs();
        pRad.bgdfs_config.type = type;
        wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OTHER);

        assert_int_equal(pRad.bgdfs_stats.nrClearSuccess[type], 0);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar[type], 0);
        assert_int_equal(pRad.bgdfs_stats.nrClearFailOther[type], 1);
        assert_int_equal(pRad.bgdfs_stats.nrClearStopQuit[type], 1);
    }
}

static void test_endClearBgDfsIsOffIfEnableIsFalse(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.type = BGDFS_TYPE_CLEAR_EXT;
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    pRad.bgdfs_config.enable = false;
    pRad.bgdfs_config.available = true;

    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OK);

    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_OFF);
}

static void test_startClearFailsIfBgDfsDisabled(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = false;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 20;

    uint32_t bandwidthMHz = 40;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);
    assert_int_equal(status, amxd_status_invalid_function_argument);
}

static void test_startClearFailsIfBgDfsNotAvailable(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = false;
    pRad.bgdfs_config.channel = 20;

    uint32_t bandwidthMHz = 40;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);
    assert_int_equal(status, amxd_status_invalid_function_argument);
}
static void test_startClearFailsIfBgDfsChannelNot0(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 20;

    uint32_t bandwidthMHz = 40;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);
    assert_int_equal(status, amxd_status_invalid_function_argument);
}
static void test_startClearSucceeds(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 0;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_returnsOkDone;

    uint32_t bandwidthMHz = 40;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);
    assert_int_equal(dfsArgs.channel, 0);
    assert_int_equal(dfsArgs.bandwidth, SWL_BW_40MHZ);

    assert_int_equal(status, amxd_status_ok);
}
static void test_startClearSucceedsFailsIfExtFunctionFails(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 0;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_returnsError;

    uint32_t bandwidthMHz = 40;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);

    assert_int_equal(status, amxd_status_unknown_error);
}

static void test_startClearBandwidthIsAutoIfNotProvided(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 0;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_start_ext = mfn_wrad_bgdfs_start_ext_returnsOkDone;

    uint32_t bandwidthMHz = 0;
    amxd_status_t status = bgdfs_startClear(&pRad, bandwidthMHz,
                                            pRad.bgdfs_config.channel,
                                            &dfsArgs);
    assert_int_equal(dfsArgs.channel, 0);
    assert_int_equal(dfsArgs.bandwidth, SWL_BW_AUTO);

    assert_int_equal(status, amxd_status_ok);
}

static void test_StopClearFailsIfBgDfsDisabled(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = false;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 20;

    amxd_status_t status = bgdfs_stopClear(&pRad);

    assert_int_equal(status, amxd_status_invalid_function_argument);
}

static void test_StopClearFailsIfBgDfsNotAvailable(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = false;
    pRad.bgdfs_config.channel = 20;

    amxd_status_t status = bgdfs_stopClear(&pRad);

    assert_int_equal(status, amxd_status_invalid_function_argument);
}
static void test_StopClearFailsIfBgDfsChannel0(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 0;

    amxd_status_t status = bgdfs_stopClear(&pRad);

    assert_int_equal(status, amxd_status_invalid_function_argument);
}

static void test_StopClearSucceeds(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 20;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_returnsOK;

    amxd_status_t status = bgdfs_stopClear(&pRad);

    assert_int_equal(status, amxd_status_ok);
}
static void test_StopClearFailsIfExtFnFails(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;
    pRad.bgdfs_config.channel = 20;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_fails;

    amxd_status_t status = bgdfs_stopClear(&pRad);

    assert_int_equal(status, amxd_status_unknown_error);
}

static void test_setAvailableFailsIfBgDfsIsRunningAndAvailableFalse(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.status = BGDFS_STATUS_IDLE;
    //Initialized to true, should be updated to false by wld_bgdfs_setAvailable()
    pRad.bgdfs_config.available = true;


    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_returnsOK;

    wld_bgdfs_setAvailable(&pRad, false);

    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_OFF);
    assert_false(pRad.bgdfs_config.available);
}
static void test_setAvailableSucceeds(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    //Initialized to false, should be updated to true by wld_bgdfs_setAvailable()
    pRad.bgdfs_config.available = false;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_returnsOK;

    wld_bgdfs_setAvailable(&pRad, true);

    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_IDLE);
    assert_true(pRad.bgdfs_config.available);
}

static void test_blockOperBlockUnblock(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].rad;
    assert_non_null(pRad);
    assert_non_null(pRad->pFA);

    memcpy(&functionTable, pRad->pFA, sizeof(struct S_CWLD_FUNC_TABLE));
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_returnsOK;
    pRad->pFA = &functionTable;

    pRad->bgdfs_config.enable = true;
    pRad->bgdfs_config.available = true;
    pRad->bgdfs_config.channel = 20;

    ttb_object_t* bgDfsObj = ttb_object_getChildObject(dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].radObj, "ChannelMgt.BgDfs");
    assert_non_null(bgDfsObj);

    // Valid block operations
    const char* blockServices[] = { "svc1", "svc2", "svc3", "svc2" };
    const char* blockExpected[] = { "svc1", "svc1,svc2", "svc1,svc2,svc3", "svc1,svc2,svc3" };
    const uint8_t blockCounts[] = { 1, 2, 3, 3 };

    for(size_t i = 0; i < 4; i++) {
        ttb_var_t* args = ttb_object_createArgs();
        assert_non_null(args);
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, args, "name", blockServices[i]);

        ttb_var_t* replyVar = NULL;
        ttb_reply_t* reply = ttb_object_callFun(NULL, bgDfsObj, "block", &args, &replyVar);
        assert_non_null(reply);
        assert_true(ttb_object_replySuccess(reply));
        ttb_object_cleanReply(&reply, &replyVar);

        char* blockedBy = swl_typeCharPtr_fromObjectParamDef(bgDfsObj, "BgDfsBlockedBy", NULL);
        assert_non_null(blockedBy);
        assert_string_equal(blockedBy, blockExpected[i]);
        assert_true(wld_util_blockOper_hasBlocker(&pRad->bgdfs_config.blockOper, blockServices[i]));
        assert_int_equal(pRad->bgdfs_config.blockOper.blockerCount, blockCounts[i]);
        free(blockedBy);
    }

    // Invalid block operations
    const char* invalidBlockers[] = { "", "inv,alid" };
    const char* lastExpectedBlockedBy = "svc1,svc2,svc3";
    const uint8_t lastExpectedCount = 3;

    for(size_t i = 0; i < 2; i++) {
        ttb_var_t* args = ttb_object_createArgs();
        assert_non_null(args);
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, args, "name", invalidBlockers[i]);

        ttb_var_t* replyVar = NULL;
        ttb_reply_t* reply = ttb_object_callFun(NULL, bgDfsObj, "block", &args, &replyVar);
        assert_non_null(reply);
        assert_false(ttb_object_replySuccess(reply));
        ttb_object_cleanReply(&reply, &replyVar);

        char* blockedBy = swl_typeCharPtr_fromObjectParamDef(bgDfsObj, "BgDfsBlockedBy", NULL);
        assert_non_null(blockedBy);
        assert_string_equal(blockedBy, lastExpectedBlockedBy);
        assert_false(wld_util_blockOper_hasBlocker(&pRad->bgdfs_config.blockOper, invalidBlockers[i]));
        assert_int_equal(pRad->bgdfs_config.blockOper.blockerCount, lastExpectedCount);
        free(blockedBy);
    }

    bool blocked = swl_typeBool_fromObjectParamDef(bgDfsObj, "BgDfsBlocked", NULL);
    assert_true(blocked);

    // Unblock operations
    const char* unblockServices[] = { "svc2", "svc1", "svc3" };
    const char* unblockExpected[] = { "svc1,svc3", "svc3", "" };
    const uint8_t unblockCounts[] = { 2, 1, 0 };

    for(size_t i = 0; i < 3; i++) {
        ttb_var_t* args = ttb_object_createArgs();
        assert_non_null(args);
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, args, "name", unblockServices[i]);

        ttb_var_t* replyVar = NULL;
        ttb_reply_t* reply = ttb_object_callFun(NULL, bgDfsObj, "unblock", &args, &replyVar);
        assert_non_null(reply);
        assert_true(ttb_object_replySuccess(reply));
        ttb_object_cleanReply(&reply, &replyVar);

        char* blockedBy = swl_typeCharPtr_fromObjectParamDef(bgDfsObj, "BgDfsBlockedBy", NULL);
        assert_non_null(blockedBy);
        assert_string_equal(blockedBy, unblockExpected[i]);
        assert_false(wld_util_blockOper_hasBlocker(&pRad->bgdfs_config.blockOper, unblockServices[i]));
        assert_int_equal(pRad->bgdfs_config.blockOper.blockerCount, unblockCounts[i]);
        free(blockedBy);
    }

    blocked = swl_typeBool_fromObjectParamDef(bgDfsObj, "BgDfsBlocked", NULL);
    assert_false(blocked);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_isRunningReturnsFalseWhenIdle),
        cmocka_unit_test(test_isRunningReturnsFalseWhenOff),
        cmocka_unit_test(test_isRunningReturnsTrueWhenClear),
        cmocka_unit_test(test_isRunningReturnsTrueWhenClearExt),
        cmocka_unit_test(test_startDfsExtImplementedSucceeds),
        cmocka_unit_test(test_startDfsExtIsImplementedButFails),
        cmocka_unit_test(test_startDfsExtNotImplemented),
        cmocka_unit_test(test_endClearResultOKClear),
        cmocka_unit_test(test_endClearResultFailRadarClear),
        cmocka_unit_test(test_endClearResultFailOtherClear),
        cmocka_unit_test(test_endClearBgDfsIsOffIfEnableIsFalse),
        cmocka_unit_test(test_startClear),
        cmocka_unit_test(test_startClearFailsIfBgDfsDisabled),
        cmocka_unit_test(test_startClearFailsIfBgDfsNotAvailable),
        cmocka_unit_test(test_startClearFailsIfBgDfsChannelNot0),
        cmocka_unit_test(test_startClearSucceeds),
        cmocka_unit_test(test_startClearSucceedsFailsIfExtFunctionFails),
        cmocka_unit_test(test_startClearBandwidthIsAutoIfNotProvided),
        cmocka_unit_test(test_StopClearFailsIfBgDfsDisabled),
        cmocka_unit_test(test_StopClearFailsIfBgDfsNotAvailable),
        cmocka_unit_test(test_StopClearFailsIfBgDfsChannel0),
        cmocka_unit_test(test_StopClearSucceeds),
        cmocka_unit_test(test_StopClearFailsIfExtFnFails),
        cmocka_unit_test(test_setAvailableFailsIfBgDfsIsRunningAndAvailableFalse),
        cmocka_unit_test(test_setAvailableSucceeds),
        cmocka_unit_test(test_blockOperBlockUnblock),
    };
    int rc = cmocka_run_group_tests(tests, setup_suite, teardown_suite);
    sahTraceClose();
    return rc;
}

