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
#include "wld_util.h"
#include "test-toolbox/ttb_mockClock.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"

static wld_th_dm_t dm;




static int setup_suite(void** state _UNUSED) {
    whm_th_fsm_useMgr(true);
    assert_true(wld_th_dm_init(&dm));

    ttb_object_t* obj = dm.ttbBus->rootObj;
    ttb_object_t* subObj = ttb_object_getChildObject(obj, "AutoCommitMgr");
    ttb_assert_non_null(subObj);
    swl_typeUInt32_commitObjectParam(subObj, "DelayTime", 4000);
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_postInit(void** state _UNUSED) {
    /********************
    * 2.4GHz
    ********************/
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    ttb_assert_int_eq(rad->fsmRad.FSM_State, FSM_IDLE);
    ttb_assert_int_eq(rad->counterList[WLD_RAD_EV_FSM_COMMIT].counter, 1);
    ttb_assert_int_eq(rad->fsmStats.nrStarts, 1);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, 1);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, 1);

    wld_th_rad_checkCommitAll(rad);
    wld_th_vap_checkCommitAll(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv);
}

static void test_disableAp(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    wld_fsmStats_t stats = rad->fsmStats;

    wld_th_dm_clearFsm(&dm);
    wld_th_vap_setApEnable(dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv, false, true);
    ttb_mockTimer_goToFutureSec(10);

    wld_th_rad_checkCommitted(dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad, 0);
    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_VAP_ENABLE_DOWN);

    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 1);


    wld_th_vap_setApEnable(dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv, true, true);
    ttb_mockTimer_goToFutureSec(10);
}

static void s_pushIwPriv(T_AccessPoint* pAP, T_Radio* pRad _UNUSED, wld_th_fsmStates_e state _UNUSED) {
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, WLD_TH_FSM_SET_IWPRIV_CMDS);
    printf("SET IW %u\n", pRad->fsmRad.FSM_ComPend);
}

static void s_pushEnable(T_Radio* pRad _UNUSED) {
    printf("PUSH PUSH %u\n", pRad->fsmRad.FSM_ComPend);
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    setBitLongArray(vap->fsm.FSM_BitActionArray, FSM_BW, WLD_TH_FSM_SET_VAP_ENABLE);
    wld_rad_doRadioCommit(pRad);
    printf("PUSH PUSH %u\n", pRad->fsmRad.FSM_ComPend);
}

/**
 * This test tests that before dependency copy step, any changes are taken into commit
 */
static void test_addFlagPreDependency(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    wld_fsmStats_t stats = rad->fsmStats;
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    wld_th_fsm_mngr.checkPreDependency = s_pushEnable;

    wld_th_dm_clearFsm(&dm);

    wld_th_vap_setApEnable(vap, false, true);
    ttb_mockTimer_goToFutureSec(10);

    wld_th_fsm_mngr.checkPreDependency = NULL;

    wld_th_rad_checkCommitted(dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad, 0);
    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_VAP_ENABLE_DOWN | M_WLD_TH_FSM_SET_VAP_ENABLE);

    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 1);


    wld_th_vap_setApEnable(vap, true, true);
    ttb_mockTimer_goToFutureSec(10);
}

static void test_addFlagPostDependencyWithComPend(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    wld_fsmStats_t stats = rad->fsmStats;
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(vap);
    vd->fsmCallback[WLD_TH_FSM_SET_VAP_ENABLE_DOWN] = s_pushIwPriv;
    wld_th_fsm_mngr.checkPreDependency = s_pushEnable;


    wld_th_dm_clearFsm(&dm);

    wld_th_vap_setApEnable(vap, false, true);
    ttb_mockTimer_goToFutureSec(5);

    vd->fsmCallback[WLD_TH_FSM_SET_VAP_ENABLE_DOWN] = NULL;
    wld_th_fsm_mngr.checkPreDependency = NULL;

    wld_th_rad_checkCommitted(dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad, 0);
    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_VAP_ENABLE_DOWN | M_WLD_TH_FSM_SET_VAP_ENABLE);

    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 1);

    // Commit the IW priv flag
    wld_th_dm_clearFsm(&dm);
    wld_rad_doRadioCommit(rad);
    ttb_mockTimer_goToFutureSec(10);

    wld_th_rad_checkCommitted(dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad, M_WLD_TH_FSM_SET_IWPRIV_CMDS);
    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_IWPRIV_CMDS);

    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 2);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 2);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 2);


    // Cleanup
    wld_th_vap_setApEnable(vap, true, true);
    ttb_mockTimer_goToFutureSec(10);
}


static void s_pushAutoCommit(T_AccessPoint* pAP, T_Radio* pRad _UNUSED, wld_th_fsmStates_e state _UNUSED) {
    wld_th_vap_setApEnable(pAP, true, true);
}

static void test_addAutoCommitPostDependency(void** state _UNUSED) {
    T_Radio* rad = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].rad;
    wld_fsmStats_t stats = rad->fsmStats;
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(vap);
    vd->fsmCallback[WLD_TH_FSM_SET_VAP_ENABLE_DOWN] = s_pushAutoCommit;

    wld_th_dm_clearFsm(&dm);

    wld_th_vap_setApEnable(vap, false, true);
    ttb_mockTimer_goToFutureSec(5);
    printf("Done \n");

    vd->fsmCallback[WLD_TH_FSM_SET_VAP_ENABLE_DOWN] = NULL;

    wld_th_rad_checkCommitted(dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad, 0);
    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_VAP_ENABLE_DOWN);

    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 1);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 1);


    wld_th_dm_clearFsm(&dm);
    ttb_mockTimer_goToFutureSec(10);


    wld_th_vap_checkCommitted(dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv, M_WLD_TH_FSM_SET_VAP_ENABLE);
    ttb_assert_int_eq(rad->fsmStats.nrStarts, stats.nrStarts + 2);
    ttb_assert_int_eq(rad->fsmStats.nrRunStarts, stats.nrRunStarts + 2);
    ttb_assert_int_eq(rad->fsmStats.nrFinish, stats.nrFinish + 2);


}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen("testApp", TRACE_TYPE_STDERR);
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_postInit),
        cmocka_unit_test(test_disableAp),
        cmocka_unit_test(test_addFlagPreDependency),
        cmocka_unit_test(test_addFlagPostDependencyWithComPend),
        cmocka_unit_test(test_addAutoCommitPostDependency),
    };
    ttb_util_setFilter();
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}
