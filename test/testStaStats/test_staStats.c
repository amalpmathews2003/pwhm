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
    assert_int_equal(dmNrAssoc, nrAssoc);
    assert_int_equal(pAP->AssociatedDeviceNumberOfEntries, nrAssoc);

    uint32_t dmNrActive = swl_typeUInt32_fromObjectParamDef(pAP->pBus, "ActiveAssociatedDeviceNumberOfEntries", 0);
    assert_int_equal(dmNrActive, nrActive);
    assert_int_equal(pAP->ActiveAssociatedDeviceNumberOfEntries, nrActive);
}


#define NR_TEST_DEV 2

static void s_checkHistory(T_AccessPoint* vap, ttb_object_t* evObj, const char* fileName) {
    printf("%s: check rssi history\n", vap->name);
    ttb_var_t* replyVar = NULL;

    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, evObj, "getShortHistoryStats",
                                            NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));
    assert_non_null(replyVar);
    printf("VAR Type %s\n", amxc_var_get_type(replyVar->type_id)->name);

    swl_ttbVariant_assertToFileMatchesFile(replyVar, fileName);
    ttb_object_cleanReply(&reply, &replyVar);

}

static void s_checkStaStats(T_AccessPoint* vap, ttb_object_t* vapObj) {
    printf("%s: check stats\n", vap->name);

    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, vapObj, "getStationStats",
                                            NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));

    wld_th_vap_vendorData_t* vendorD = wld_th_vap_getVendorData(vap);
    char fileBuf[64] = {0};
    snprintf(fileBuf, sizeof(fileBuf), "stationStats/stats_%s", vendorD->staStatsFileName);
    swl_ttbVariant_assertToFileMatchesFile(replyVar, fileBuf);
    ttb_object_cleanReply(&reply, &replyVar);
}


static void test_getStats(void** state _UNUSED) {
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPriv;
    ttb_object_t* vapObj = dm.bandList[SWL_FREQ_BAND_EXT_5GHZ].vapPrivObj;
    wld_th_vap_getVendorData(vap)->nrStaInFile = NR_TEST_DEV;
    wld_th_vap_getVendorData(vap)->staStatsFileName = "data0.txt";


    ttb_mockTimer_goToFutureSec(1);
    printf("\n Start Rssi Ev \n");


    ttb_object_t* evObj = ttb_object_getChildObject(vapObj, "RssiEventing");
    assert_non_null(evObj);

    s_checkHistory(vap, evObj, "historyEmpty.txt");

    assert_true(swl_typeUInt32_commitObjectParam(evObj, "HistoryLen", 3));
    assert_true(swl_typeUInt32_commitObjectParam(evObj, "Enable", 1));

    amxd_object_t* templateObject = amxd_object_get(vapObj, "AssociatedDevice");

    ttb_notifWatch_t* notWatchVap = ttb_notifWatch_createOnObject(dm.ttbBus, templateObject);

    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history0.txt");

    size_t nrNot = ttb_notifWatch_nbNotifsSeen(notWatchVap);
    for(size_t i = 0; i < nrNot; i++) {
        char buffer[64] = {0};
        snprintf(buffer, sizeof(buffer), "assocNot/not_%zu.txt", i);
        ttb_notification_t* not = ttb_notifWatch_notif(notWatchVap, i);
        swl_ttbVariant_assertToFileMatchesFile(&not->variant, buffer);
    }

    ttb_notifWatch_printList(notWatchVap);
    ttb_notifWatch_destroy(notWatchVap);


    wld_th_vap_getVendorData(vap)->staStatsFileName = "data1.txt";
    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history1.txt");

    wld_th_vap_getVendorData(vap)->staStatsFileName = "data2.txt";
    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history2.txt");

    wld_th_vap_getVendorData(vap)->staStatsFileName = "data3.txt";
    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history3.txt");

    s_checkStaStats(vap, vapObj);
    ttb_mockTimer_goToFutureSec(1);

    swl_macBin_t macBin;
    memset(&macBin, 0, sizeof(swl_macBin_t));
    char testBuff[24] = "18:58:80:C2:FC:A1";
    swl_mac_charToBin(&macBin, (swl_macChar_t*) &testBuff[0]);
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(vap, &macBin);
    // Check probe update
    pAD->probeReqCaps.currentSecurity = SWL_SECURITY_APMODE_WPA2_P;
    pAD->probeReqCaps.freqCapabilities = M_SWL_FREQ_BAND_EXT_2_4GHZ | M_SWL_FREQ_BAND_EXT_5GHZ;
    pAD->probeReqCaps.htCapabilities = M_SWL_STACAP_HT_SGI20 | M_SWL_STACAP_HT_40MHZ;
    pAD->probeReqCaps.updateTime = swl_time_getMonoSec();
    ttb_amx_handleEvents();

    ttb_notifWatch_t* notWatch = ttb_notifWatch_createOnObject(dm.ttbBus, pAD->object);

    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, vapObj, "getStationStats",
                                            NULL, NULL);
    ttb_object_cleanReply(&reply, NULL);
    ttb_amx_handleEvents();

    ttb_notifWatch_printList(notWatch);

    ttb_assert_int_eq(2, ttb_notifWatch_nbNotifsSeen(notWatch));

    ttb_notification_t* not0 = ttb_notifWatch_notif(notWatch, 0);
    swl_ttbVariant_assertToFileMatchesFile(&not0->variant, "staUpdate.txt");

    ttb_notification_t* not1 = ttb_notifWatch_notif(notWatch, 1);
    swl_ttbVariant_assertToFileMatchesFile(&not1->variant, "probeUpdate.txt");

    ttb_notifWatch_destroy(notWatch);

    ttb_mockTimer_goToFutureSec(1);

    ttb_notifWatch_t* req = ttb_notifWatch_createOnObject(dm.ttbBus, vap->pBus);

    //Disassoc one manually.

    assert_non_null(pAD);
    wld_ad_deauthWithReason(vap, pAD, SWL_IEEE80211_DEAUTH_REASON_BTM);
    ttb_amx_handleEvents();


    // Disassoc one by having 0 updates.
    wld_th_vap_getVendorData(vap)->staStatsFileName = "data4.txt";
    wld_th_vap_getVendorData(vap)->nrStaInFile = 0;
    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history4.txt");
    ttb_amx_handleEvents();

    ttb_notifWatch_printList(req);

    size_t nrNotif = 13;
    assert_int_equal(nrNotif, ttb_notifWatch_nbNotifsSeen(req));

    for(size_t i = 0; i < nrNotif; i++) {
        char fileBuf[64] = {0};
        snprintf(fileBuf, sizeof(fileBuf), "deauthNot/notif_%zi.txt", i);
        ttb_notification_t* notif = ttb_notifWatch_notif(req, i);
        if(amxc_var_is_null(&notif->variant)) {
            printf("NULL %zu\n", i);
        } else {
            swl_ttbVariant_assertToFileMatchesFile(&notif->variant, fileBuf);
        }
    }


    s_checkStaStats(vap, vapObj);
    ttb_notifWatch_destroy(req);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(500, "apRssi");
    sahTraceAddZone(500, "utilMon");
    sahTraceAddZone(500, "pcb_timer");
    sahTraceAddZone(500, "ad");

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_getStats),
    };
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}

