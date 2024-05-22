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
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <test-toolbox/ttb.h>
#include <string.h>
#include <debug/sahtrace.h>

#include <amxc/amxc_macros.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object_event.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#include <amxb/amxb.h>


#include <amxb/amxb_register.h>

#include "wld.h"
#include "wld_assocdev.h"
#include "wld_util.h"

#include "../testHelper/wld_th_dm.h"

static wld_th_dm_t dm;

static int test_setup(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int test_teardown(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void s_checkDevEntries(T_AccessPoint* pAP, uint32_t nrAssoc, uint32_t nrActive) {
    uint32_t dmNrAssoc = swl_typeUInt32_fromObjectParamDef(pAP->pBus, "AssociatedDeviceNumberOfEntries", 0);
    assert_int_equal(dmNrAssoc, nrAssoc);
    assert_int_equal(pAP->AssociatedDeviceNumberOfEntries, nrAssoc);

    //check assocDev obj instances count matching internal count
    amxd_object_t* pAdTempl = amxd_object_get(pAP->pBus, "AssociatedDevice");
    assert_non_null(pAdTempl);
    assert_int_equal(amxc_llist_size(&pAdTempl->instances), nrAssoc);

    uint32_t dmNrActive = swl_typeUInt32_fromObjectParamDef(pAP->pBus, "ActiveAssociatedDeviceNumberOfEntries", 0);
    assert_int_equal(dmNrActive, nrActive);
    assert_int_equal(pAP->ActiveAssociatedDeviceNumberOfEntries, nrActive);
}

static void s_removeDevEntriesRange(T_AccessPoint* vap, T_AssociatedDevice* devList[], int nbEntries, int startId, int stopId) {
    for(int i = startId; i < stopId; i++) {
        assert_non_null(devList[i]);
        s_checkDevEntries(vap, SWL_MIN(nbEntries, nbEntries - i + 1), (nbEntries - i));
        wld_ad_add_disconnection(vap, devList[i]);
        s_checkDevEntries(vap, nbEntries - i, SWL_MAX(0, nbEntries - i - 1));
    }
}

static void test_startStop_checkAssoc(_UNUSED void** state) {
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;
#define NR_TEST_DEV 5

    T_AssociatedDevice* devList[NR_TEST_DEV];

    for(int i = 0; i < NR_TEST_DEV; i++) {
        char devName[18];
        snprintf(devName, sizeof(devName), "AA:BB:CC:DD:EE:%02x", i);
        swl_macBin_t binAddr;
        swl_mac_charToBin(&binAddr, (swl_macChar_t*) devName);

        s_checkDevEntries(vap, i, i);

        devList[i] = wld_vap_findOrCreateAssociatedDevice(vap, &binAddr);
        //check instance creation
        assert_non_null(devList[i]);
        //check instance match
        assert_ptr_equal(devList[i], wld_vap_find_asociatedDevice(vap, &binAddr));
        wld_ad_add_connection_try(vap, devList[i]);
        ttb_mockTimer_goToFutureMs(1);

        s_checkDevEntries(vap, i + 1, i + 1);

        wld_ad_add_connection_success(vap, devList[i]);
    }

    amxc_var_t ret;
    amxc_var_init(&ret);

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);

    assert_int_equal(amxd_object_invoke_function(vap->pBus, "getStationStats", &args, &ret), 0);

    const amxc_llist_t* staList = amxc_var_get_const_amxc_llist_t(&ret);
    assert_non_null(staList);
    assert_int_equal(amxc_llist_size(staList), NR_TEST_DEV);

    amxc_var_clean(&ret);
    amxc_var_clean(&args);

    //remove 1st half of assoc dev entries
    s_removeDevEntriesRange(vap, devList, NR_TEST_DEV, 0, NR_TEST_DEV / 2);

    amxd_object_t* pAdTempl = amxd_object_get(vap->pBus, "AssociatedDevice");
    assert_non_null(pAdTempl);
    //check all obj instances pointing to valid internal contexts
    amxc_llist_for_each(it, &pAdTempl->instances) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        assert_non_null(instance);
        assert_non_null(instance->priv);
        int i;
        for(i = 0; i < NR_TEST_DEV && (instance->priv == devList[i]); i++) {
        }
        assert_int_not_equal(i, NR_TEST_DEV);
    }

    //removing remaining assoc dev entries
    s_removeDevEntriesRange(vap, devList, NR_TEST_DEV, NR_TEST_DEV / 2, NR_TEST_DEV);

    //check that only last inactive assocDev obj instance remains
    assert_int_equal(amxc_llist_size(&pAdTempl->instances), NR_OF_STICKY_UNAUTHORIZED_STATIONS);

    //readd

    for(int i = 0; i < NR_TEST_DEV; i++) {
        char devName[18];
        snprintf(devName, sizeof(devName), "AA:BB:CC:DD:EE:%02x", i);
        swl_macBin_t binAddr;
        swl_mac_charToBin(&binAddr, (swl_macChar_t*) devName);
        printf("Add1 %s\n", devName);
        s_checkDevEntries(vap, SWL_MIN(NR_TEST_DEV, i + 1), i);
        printf("Add %u\n", i);
        devList[i] = wld_vap_findOrCreateAssociatedDevice(vap, &binAddr);
        wld_ad_add_connection_try(vap, devList[i]);
        ttb_mockTimer_goToFutureMs(1);

        s_checkDevEntries(vap, SWL_MIN(i + 2, NR_TEST_DEV), i + 1);

        wld_ad_add_connection_success(vap, devList[i]);
    }
    wld_th_vap_getVendorData(vap)->errorOnStaStats = true;

    amxc_var_init(&ret);
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);

    assert_int_equal(amxd_object_invoke_function(vap->pBus, "getStationStats", &args, &ret), 0);

    staList = amxc_var_get_const_amxc_llist_t(&ret);
    assert_non_null(staList);
    assert_int_equal(amxc_llist_size(staList), 1);

    amxc_var_clean(&ret);
    amxc_var_clean(&args);

    wld_th_vap_getVendorData(vap)->errorOnStaStats = false;

    T_EndPoint* ep = dm.bandList[0].ep;
    assert_non_null(ep);
    amxc_var_init(&ret);
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    assert_int_equal(amxd_object_invoke_function(ep->pBus, "getStats", &args, &ret), 0);
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
    ttb_object_t* epStats = ttb_object_getChildObject(ep->pBus, "Stats");
    assert_non_null(epStats);
    ttb_assert_int_eq(12991, swl_typeUInt32_fromObjectParamDef(epStats, "TxBytes", 0));
    ttb_assert_int_eq(65, swl_typeUInt32_fromObjectParamDef(epStats, "TxPacketCount", 0));
    char* epSecMode = swl_typeCharPtr_fromObjectParamDef(epStats, "SecurityModeEnabled", 0);
    assert_string_equal("WPA2-Personal", epSecMode);
    free(epSecMode);

    // Check the endpoint radio reference
    T_Radio* rad = ep->pSSID->RADIO_PARENT;
    assert_non_null(rad);
    char* radioRef = amxd_object_get_path(rad->pBus, AMXD_OBJECT_NAMED);
    char* epRadioRef = swl_typeCharPtr_fromObjectParamDef(ep->pBus, "RadioReference", 0);
    assert_string_equal(radioRef, epRadioRef);
    assert_string_equal(radioRef, "WiFi.Radio.wifi0");
    free(radioRef);
    free(epRadioRef);

    // Check the endpoint profile reference
    // Named profile reference
    assert_int_equal(swl_typeCharPtr_commitObjectParam(ep->pBus, "ProfileReference", "WiFi.EndPoint.sta0.Profile.prof0"), true);
    ttb_mockTimer_goToFutureMs(1);
    assert_non_null(ep->currentProfile);
    char* profileAlias = swl_typeCharPtr_fromObjectParamDef(ep->currentProfile->pBus, "Alias", 0);
    assert_string_equal(profileAlias, "prof0");
    free(profileAlias);
    char* profileRef = amxd_object_get_path(ep->currentProfile->pBus, AMXD_OBJECT_NAMED);
    char* epProfileRef = swl_typeCharPtr_fromObjectParamDef(ep->pBus, "ProfileReference", 0);
    assert_string_equal(profileRef, epProfileRef);
    assert_string_equal(profileRef, "WiFi.EndPoint.sta0.Profile.prof0");
    free(profileRef);
    free(epProfileRef);
    // Indexed profile reference
    assert_int_equal(swl_typeCharPtr_commitObjectParam(ep->pBus, "ProfileReference", "WiFi.EndPoint.1.Profile.2"), true);
    ttb_mockTimer_goToFutureMs(1);
    assert_non_null(ep->currentProfile);
    profileAlias = swl_typeCharPtr_fromObjectParamDef(ep->currentProfile->pBus, "Alias", 0);
    assert_string_equal(profileAlias, "prof2");
    free(profileAlias);
    profileRef = amxd_object_get_path(ep->currentProfile->pBus, AMXD_OBJECT_INDEXED);
    epProfileRef = swl_typeCharPtr_fromObjectParamDef(ep->pBus, "ProfileReference", 0);
    assert_string_equal(profileRef, epProfileRef);
    assert_string_equal(profileRef, "WiFi.EndPoint.1.Profile.2");
    free(profileRef);
    free(epProfileRef);
}

int main(void) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_startStop_checkAssoc),
    };
    int rv = cmocka_run_group_tests(tests, test_setup, test_teardown);
    return rv;
}



