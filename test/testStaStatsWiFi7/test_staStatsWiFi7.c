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
#include "wld_radio.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "test-toolbox/ttb.h"
#include "test-toolbox/ttb_notifWatch.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "swla/ttb/swla_ttbVariant.h"
#include "swl/fileOps/swl_fileUtils.h"
#include "swl/swl_common.h"


static wld_th_dm_t dm;

static int setup_suite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

void s_checkDevEntries(T_AccessPoint* pAP, uint32_t nrAssoc, uint32_t nrActive) {
    uint32_t dmNrAssoc = swl_typeUInt32_fromObjectParamDef(pAP->pBus, "AssociatedDeviceNumberOfEntries", 0);
    ttb_assert_int_eq(dmNrAssoc, nrAssoc);
    ttb_assert_int_eq(pAP->AssociatedDeviceNumberOfEntries, nrAssoc);

    uint32_t dmNrActive = swl_typeUInt32_fromObjectParamDef(pAP->pBus, "ActiveAssociatedDeviceNumberOfEntries", 0);
    ttb_assert_int_eq(dmNrActive, nrActive);
    ttb_assert_int_eq(pAP->ActiveAssociatedDeviceNumberOfEntries, nrActive);
}

static void s_checkStaStats(T_AccessPoint* vap, ttb_object_t* vapObj, const char* fileName) {
    printf("%s: check stats\n", vap->name);

    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, vapObj, "getStationStats",
                                            NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));

    char fileBuf[64] = {0};
    snprintf(fileBuf, sizeof(fileBuf), "stationStats/stats_%s", fileName);
    swl_ttbVariant_assertToFileMatchesFile(replyVar, fileBuf);
    ttb_object_cleanReply(&reply, &replyVar);
}

static void s_fillAfSta(wld_affiliatedSta_t* afSta, uint32_t baseVal, int32_t sigStrength) {

    afSta->packetsSent = 2 * baseVal;
    afSta->packetsReceived = baseVal;
    afSta->bytesSent = 20 * baseVal;
    afSta->bytesReceived = 10 * baseVal;
    afSta->lastDataDownlinkRate = 2000 * baseVal;
    afSta->lastDataUplinkRate = 1000 * baseVal;
    afSta->upLinkRateSpec.standard = SWL_MCS_STANDARD_EHT;
    afSta->upLinkRateSpec.guardInterval = SWL_SGI_1600;
    afSta->upLinkRateSpec.mcsIndex = baseVal;
    afSta->upLinkRateSpec.numberOfSpatialStream = 2;
    afSta->upLinkRateSpec.bandwidth = SWL_BW_80MHZ;
    memcpy(&afSta->downLinkRateSpec, &afSta->upLinkRateSpec, sizeof(swl_mcs_t));
    afSta->downLinkRateSpec.mcsIndex = SWL_MCS_MAX - 1 - baseVal;

    afSta->signalStrength = sigStrength;

}

static void test_getStats(void** state _UNUSED) {
    T_AccessPoint* vap2 = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    assert_non_null(vap2);
    T_AccessPoint* vap5 = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPriv;
    assert_non_null(vap5);
    T_AccessPoint* vap6 = dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].vapPriv;
    assert_non_null(vap6);


    ttb_mockTimer_goToFutureSec(1);

    ttb_object_t* vap5Obj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPrivObj;
    assert_non_null(vap5Obj);

    s_checkDevEntries(vap2, 0, 0);
    s_checkDevEntries(vap5, 0, 0);
    s_checkDevEntries(vap6, 0, 0);

    s_checkStaStats(vap5, vap5Obj, "test1.txt");

    swl_macBin_t myBin = {.bMac = {0xaa, 0xbb, 0xaa, 0xbb, 0xaa, 0x01}};

    T_AssociatedDevice* pAD = wld_ad_create_associatedDevice(vap5, &myBin);
    wld_ad_add_connection_try(vap5, pAD);
    wld_ad_add_connection_success(vap5, pAD);

    wld_affiliatedSta_t* afSta2 = wld_ad_getOrAddAffiliatedSta(pAD, vap2);
    wld_ad_activateAfSta(pAD, afSta2);
    assert_non_null(afSta2);
    wld_affiliatedSta_t* afSta5 = wld_ad_getOrAddAffiliatedSta(pAD, vap5);
    wld_ad_activateAfSta(pAD, afSta5);
    assert_non_null(afSta5);
    wld_affiliatedSta_t* afSta6 = wld_ad_getOrAddAffiliatedSta(pAD, vap6);
    wld_ad_activateAfSta(pAD, afSta6);
    assert_non_null(afSta6);

    ttb_mockTimer_goToFutureSec(1);

    s_checkDevEntries(vap2, 0, 0);
    s_checkDevEntries(vap5, 1, 1);
    s_checkDevEntries(vap6, 0, 0);


    s_checkStaStats(vap5, vap5Obj, "test2.txt");

    s_fillAfSta(afSta2, 1, -80);
    s_fillAfSta(afSta5, 3, -81);
    s_fillAfSta(afSta6, 5, -82);
    ttb_mockTimer_goToFutureSec(1);
    s_checkStaStats(vap5, vap5Obj, "test3.txt");



    s_fillAfSta(afSta2, 2, -82);
    s_fillAfSta(afSta5, 6, -84);
    s_fillAfSta(afSta6, 10, -86);
    ttb_mockTimer_goToFutureSec(1);
    s_checkStaStats(vap5, vap5Obj, "test4.txt");


    wld_ad_add_disconnection(vap5, pAD);
    ttb_mockTimer_goToFutureSec(1);
    s_checkStaStats(vap5, vap5Obj, "test5.txt");

    wld_ad_destroy(vap5, pAD);
    pAD = NULL;
    ttb_mockTimer_goToFutureSec(1);
    s_checkStaStats(vap5, vap5Obj, "test6.txt");
}

static void test_deactivate(void** state _UNUSED) {
    T_AccessPoint* vap2 = dm.bandList[SWL_FREQ_BAND_EXT_2_4GHZ].vapPriv;
    assert_non_null(vap2);
    T_AccessPoint* vap5 = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPriv;
    assert_non_null(vap5);
    T_AccessPoint* vap6 = dm.bandList[SWL_FREQ_BAND_EXT_6GHZ].vapPriv;
    assert_non_null(vap6);

    ttb_mockTimer_goToFutureSec(1);

    ttb_object_t* vap5Obj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPrivObj;
    assert_non_null(vap5Obj);

    ttb_assert_int_eq(wld_ad_get_nb_active_stations(vap5), 0);


    swl_macBin_t myBin = {.bMac = {0xaa, 0xbb, 0xaa, 0xbb, 0xaa, 0x01}};

    T_AssociatedDevice* pAD = wld_ad_create_associatedDevice(vap5, &myBin);
    pAD->operatingStandard = SWL_RADSTD_BE;

    ttb_assert_int_eq(wld_ad_get_nb_active_stations(vap5), 0);

    wld_ad_add_connection_try(vap5, pAD);
    wld_ad_add_connection_success(vap5, pAD);

    /* VAPs Max Station on each Radio do not change */
    ttb_assert_int_eq(wld_ap_getMaxNbrSta(vap2), vap2->MaxStations);
    ttb_assert_int_eq(wld_ap_getMaxNbrSta(vap5), vap5->MaxStations);
    ttb_assert_int_eq(wld_ap_getMaxNbrSta(vap6), vap6->MaxStations);

    ttb_assert_int_eq(wld_ad_get_nb_active_stations(vap5), 1);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 0);

    wld_affiliatedSta_t* afSta2 = wld_ad_getOrAddAffiliatedSta(pAD, vap2);
    wld_ad_activateAfSta(pAD, afSta2);
    assert_non_null(afSta2);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 1);

    wld_affiliatedSta_t* afSta5 = wld_ad_getOrAddAffiliatedSta(pAD, vap5);
    wld_ad_activateAfSta(pAD, afSta5);
    assert_non_null(afSta5);

    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 2);

    wld_affiliatedSta_t* afSta6 = wld_ad_getOrAddAffiliatedSta(pAD, vap6);
    wld_ad_activateAfSta(pAD, afSta6);
    assert_non_null(afSta6);

    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 3);

    wld_ad_deactivateAfSta(pAD, afSta2);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 2);


    wld_ad_deactivateAfSta(pAD, afSta5);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 1);


    wld_ad_deactivateAfSta(pAD, afSta6);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 0);

    ttb_assert_int_eq(wld_ad_get_nb_active_stations(vap5), 1);

    // Test reactivate & disconnect sta
    wld_ad_activateAfSta(pAD, afSta2);
    wld_ad_activateAfSta(pAD, afSta5);
    wld_ad_activateAfSta(pAD, afSta6);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 3);

    wld_ad_add_disconnection(vap5, pAD);
    ttb_assert_int_eq(wld_ad_getNrActiveAffiliatedSta(pAD), 0);
    ttb_assert_int_eq(wld_ad_get_nb_active_stations(vap5), 0);

    wld_ad_destroy(vap5, pAD);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(500, "apRssi");
    sahTraceAddZone(500, "utilMon");
    sahTraceAddZone(500, "pcb_timer");
    sahTraceAddZone(500, "ap");

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_getStats),
        cmocka_unit_test(test_deactivate),
    };
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}

