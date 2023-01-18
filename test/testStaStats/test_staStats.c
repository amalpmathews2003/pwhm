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

    assert_true(swl_typeUInt32_toObjectParam(evObj, "HistoryLen", 3));
    assert_true(swl_typeUInt32_toObjectParam(evObj, "Enable", 1));


    s_checkHistory(vap, evObj, "historyEmpty.txt");

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
    pAD->probeReqCaps.currentSecurity = APMSI_WPA2_P;
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

    //Disassoc one manually.

    assert_non_null(pAD);
    wld_ad_add_disconnection(vap, pAD);


    // Disassoc one by having 0 updates.
    wld_th_vap_getVendorData(vap)->staStatsFileName = "data4.txt";
    wld_th_vap_getVendorData(vap)->nrStaInFile = 0;
    ttb_mockTimer_goToFutureSec(1);
    s_checkHistory(vap, evObj, "history4.txt");

    s_checkStaStats(vap, vapObj);

}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(500, "apRssi");
    sahTraceAddZone(500, "utilMon");
    sahTraceAddZone(500, "pcb_timer");

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_getStats),
    };
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}

