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
#include "wld_bgdfs.h"

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
    bool externalClear = false;

    wld_bgdfs_notifyClearStarted(&pRad, channel, bandwidth, externalClear);

    assert_int_equal(pRad.bgdfs_config.channel, channel);
    assert_int_equal(pRad.bgdfs_config.bandwidth, bandwidth);
    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_CLEAR);
}

static void test_startClearExternal(void** state _UNUSED) {
    clearStructs();
    swl_channel_t channel = 0;
    swl_bandwidth_e bandwidth = SWL_BW_20MHZ;
    bool externalClear = true;

    wld_bgdfs_notifyClearStarted(&pRad, channel, bandwidth, externalClear);

    assert_int_equal(pRad.bgdfs_config.channel, channel);
    assert_int_equal(pRad.bgdfs_config.bandwidth, bandwidth);
    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_CLEAR_EXT);
}

static void test_endClearResultOKClearExternal(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = true;

    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OK);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 1);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 0);

    assert_int_equal(pRad.bgdfs_config.channel, 0);
    assert_int_equal(pRad.bgdfs_config.bandwidth, SWL_BW_AUTO);
    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_IDLE);
}
static void test_endClearResultOKClear(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OK);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 1);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 0);
}

static void test_endClearResultFailRadarClear(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_RADAR);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 1);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 0);
}
static void test_endClearResultFailRadarClearExternal(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_RADAR);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 1);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 0);
}
static void test_endClearResultFailOtherClear(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OTHER);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 1);
}
static void test_endClearResultFailOtherClearExternal(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OTHER);

    assert_int_equal(pRad.bgdfs_stats.nrClearSuccessExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearSuccess, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadarExt, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailRadar, 0);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOtherExt, 1);
    assert_int_equal(pRad.bgdfs_stats.nrClearFailOther, 0);
}

static void test_endClearBgDfsIsOffIfEnableIsFalse(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    pRad.bgdfs_config.enable = false;
    pRad.bgdfs_config.available = true;

    wld_bgdfs_notifyClearEnded(&pRad, DFS_RESULT_OK);

    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_OFF);
}

static void test_endClearBgDfsIsOffIfAvailableIsFalse(void** state _UNUSED) {
    clearStructs();
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR_EXT;
    pRad.bgdfs_config.enable = true;
    pRad.bgdfs_config.available = false;

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
    pRad.bgdfs_config.status = BGDFS_STATUS_CLEAR;
    //Initialized to false, should be updated to true by wld_bgdfs_setAvailable()
    pRad.bgdfs_config.available = false;

    pRad.pFA = &functionTable;
    functionTable.mfn_wrad_bgdfs_stop = mfn_wrad_bgdfs_stop_returnsOK;

    wld_bgdfs_setAvailable(&pRad, true);

    assert_int_equal(pRad.bgdfs_config.status, BGDFS_STATUS_IDLE);
    assert_true(pRad.bgdfs_config.available);
}


int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_isRunningReturnsFalseWhenIdle),
        cmocka_unit_test(test_isRunningReturnsFalseWhenOff),
        cmocka_unit_test(test_isRunningReturnsTrueWhenClear),
        cmocka_unit_test(test_isRunningReturnsTrueWhenClearExt),
        cmocka_unit_test(test_startDfsExtImplementedSucceeds),
        cmocka_unit_test(test_startDfsExtIsImplementedButFails),
        cmocka_unit_test(test_startDfsExtNotImplemented),
        cmocka_unit_test(test_endClearResultOKClearExternal),
        cmocka_unit_test(test_endClearResultOKClear),
        cmocka_unit_test(test_endClearResultFailRadarClear),
        cmocka_unit_test(test_endClearResultFailRadarClearExternal),
        cmocka_unit_test(test_endClearResultFailOtherClear),
        cmocka_unit_test(test_endClearResultFailOtherClearExternal),
        cmocka_unit_test(test_endClearBgDfsIsOffIfEnableIsFalse),
        cmocka_unit_test(test_endClearBgDfsIsOffIfAvailableIsFalse),
        cmocka_unit_test(test_startClearExternal),
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
    };
    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    sahTraceClose();
    return rc;
}

