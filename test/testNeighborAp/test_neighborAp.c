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
#include "wld_accesspoint.h"
#include "wld_util.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "swl/swl_common.h"

static wld_th_dm_t dm;
typedef struct {
    char bssid[ETHER_ADDR_STR_LEN];
    char ssid[SSID_NAME_LEN];
    int32_t operatingClass;
    int32_t channel;
    int32_t phyType;
    int32_t bssidInfomation;
    char nasIdentifier[20];
    char rokhKey[20];
} neighbourAp_t;

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

swl_rc_ne wld_th_vap_updatedNeighbour(T_AccessPoint* pAP, T_ApNeighbour* pNeighbour) {
    assert_non_null(pAP);
    assert_non_null(pNeighbour);
    swl_macChar_t macStr;

    wldu_convMac2Str((unsigned char*) pNeighbour->bssid, ETHER_ADDR_LEN, macStr.cMac, ETHER_ADDR_STR_LEN);

    assert_string_not_equal(macStr.cMac, "");
    assert_string_not_equal(pNeighbour->ssid, "");
    return SWL_RC_OK;
}

swl_rc_ne wld_th_vap_removedNeighbour(T_AccessPoint* pAP, T_ApNeighbour* pNeighbour) {
    assert_non_null(pAP);
    assert_non_null(pNeighbour);
    swl_macChar_t macStr;

    wldu_convMac2Str((unsigned char*) pNeighbour->bssid, ETHER_ADDR_LEN, macStr.cMac, ETHER_ADDR_STR_LEN);

    assert_string_not_equal(macStr.cMac, "");
    assert_string_not_equal(pNeighbour->ssid, "");
    return SWL_RC_OK;
}


static void s_removeNeighbourAp(T_AccessPoint* vap, neighbourAp_t* pNeigh) {
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, args, "BSSID", pNeigh->bssid);

    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, vap->pBus, "delNeighbourAP",
                                            &args, &replyVar);
    assert_true(ttb_object_replySuccess(reply));

    ttb_object_cleanReply(&reply, &replyVar);
    free(pNeigh);
    pNeigh = NULL;
}
static void s_addNeighbourAp(T_AccessPoint* vap, neighbourAp_t* pNeigh) {
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, args, "BSSID", pNeigh->bssid);
    amxc_var_add_key(cstring_t, args, "SSID", pNeigh->ssid);
    amxc_var_add_key(int32_t, args, "Information", pNeigh->bssidInfomation);
    amxc_var_add_key(int32_t, args, "OperatingClass", pNeigh->operatingClass);
    amxc_var_add_key(int32_t, args, "Channel", pNeigh->channel);
    amxc_var_add_key(int32_t, args, "PhyType", pNeigh->phyType);
    amxc_var_add_key(cstring_t, args, "NASIdentifier", pNeigh->nasIdentifier);
    amxc_var_add_key(cstring_t, args, "R0KHKey", pNeigh->rokhKey);


    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, vap->pBus, "setNeighbourAP",
                                            &args, &replyVar);
    assert_true(ttb_object_replySuccess(reply));

    ttb_object_cleanReply(&reply, &replyVar);
}

static neighbourAp_t** s_fillNeighbourData(T_AccessPoint* vap, int count) {
    neighbourAp_t** neighbours = (neighbourAp_t**) malloc(count * sizeof(neighbourAp_t*));
    if(count > 9) {
        return NULL;
    }
    for(int i = 0; i < count; i++) {
        printf("add Neighbour %d \n", i);
        neighbours[i] = (neighbourAp_t*) malloc(sizeof(neighbourAp_t));
        if(neighbours[i] == NULL) {
            return neighbours;
        }
        memset(neighbours[i], 0, sizeof(neighbourAp_t));
        neighbours[i]->channel = i;
        neighbours[i]->operatingClass = i;
        neighbours[i]->bssidInfomation = i;
        neighbours[i]->phyType = i;
        snprintf(neighbours[i]->bssid, ETHER_ADDR_STR_LEN, "00:11:22:33:44:%d%d", i, i);
        snprintf(neighbours[i]->ssid, SSID_NAME_LEN, "test%d", i);
        snprintf(neighbours[i]->nasIdentifier, 20, "test%d", i);
        snprintf(neighbours[i]->rokhKey, 20, "test%d", i);
        s_addNeighbourAp(vap, neighbours[i]);
    }
    return neighbours;

}

static void test_addNeighborAP(void** state _UNUSED) {

    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;
    vap->pFA->mfn_wvap_updated_neighbour = wld_th_vap_updatedNeighbour;
    vap->pFA->mfn_wvap_deleted_neighbour = wld_th_vap_removedNeighbour;
    int count = 3;

    neighbourAp_t** pNeighbours = s_fillNeighbourData(vap, count);
    amxd_object_t* pNeighbourObject = amxd_object_get(vap->pBus, "Neighbour");
    int index = 0;
    amxc_llist_for_each(it, &pNeighbourObject->instances) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        index++;
        assert_non_null(instance);
        assert_non_null(instance->priv);
        assert_non_null(pNeighbours[index - 1]);
        printf("test Neighbour %d \n", index - 1);
        char* bssid = NULL;
        char* ssid = NULL;
        char* nasIdentifier = NULL;
        char* rokhKey = NULL;

        bssid = swl_typeCharPtr_fromObjectParamDef(instance, "BSSID", 0);
        ssid = swl_typeCharPtr_fromObjectParamDef(instance, "SSID", 0);
        nasIdentifier = swl_typeCharPtr_fromObjectParamDef(instance, "NASIdentifier", 0);
        rokhKey = swl_typeCharPtr_fromObjectParamDef(instance, "R0KHKey", 0);

        assert_string_equal(pNeighbours[index - 1]->bssid, bssid);
        assert_string_equal(pNeighbours[index - 1 ]->ssid, ssid);
        assert_int_equal(pNeighbours[index - 1 ]->channel, swl_typeUInt32_fromObjectParamDef(instance, "Channel", 0));
        assert_int_equal(pNeighbours[index - 1 ]->bssidInfomation, swl_typeUInt32_fromObjectParamDef(instance, "Information", 0));
        assert_int_equal(pNeighbours[index - 1]->operatingClass, swl_typeUInt32_fromObjectParamDef(instance, "OperatingClass", 0));
        assert_int_equal(pNeighbours[index - 1 ]->phyType, swl_typeUInt32_fromObjectParamDef(instance, "PhyType", 0));
        assert_string_equal(pNeighbours[index - 1 ]->nasIdentifier, nasIdentifier);
        assert_string_equal(pNeighbours[index - 1]->rokhKey, rokhKey);

        free(bssid);
        free(ssid);
        free(nasIdentifier);
        free(rokhKey);

    }

    //check that data is correctly stored
    index = 0;
    amxc_llist_for_each(it, &vap->neighbours) {
        T_ApNeighbour* neigh = amxc_llist_it_get_data(it, T_ApNeighbour, it);
        index++;
        printf("test stored data %d \n", index - 1);
        assert_non_null(pNeighbours[index - 1]);
        swl_macChar_t macStr;

        wldu_convMac2Str((unsigned char*) neigh->bssid, ETHER_ADDR_LEN, macStr.cMac, ETHER_ADDR_STR_LEN);

        assert_string_equal(pNeighbours[index - 1]->bssid, macStr.cMac);
        assert_string_equal(pNeighbours[index - 1 ]->ssid, neigh->ssid);
        assert_int_equal(pNeighbours[index - 1 ]->channel, neigh->channel);
        assert_int_equal(pNeighbours[index - 1 ]->bssidInfomation, neigh->information);
        assert_int_equal(pNeighbours[index - 1]->operatingClass, neigh->operatingClass);
        assert_int_equal(pNeighbours[index - 1 ]->phyType, neigh->phyType);
        assert_string_equal(pNeighbours[index - 1 ]->nasIdentifier, neigh->nasIdentifier);
        assert_string_equal(pNeighbours[index - 1]->rokhKey, neigh->r0khkey);

        //remove the neighbor here
        s_removeNeighbourAp(vap, pNeighbours[index - 1]);
    }
    free(pNeighbours);
}

static void test_removeNeighborAP(void** state _UNUSED) {

    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;

    vap->pFA->mfn_wvap_updated_neighbour = wld_th_vap_updatedNeighbour;
    vap->pFA->mfn_wvap_deleted_neighbour = wld_th_vap_removedNeighbour;

    int count = 6;

    neighbourAp_t** pNeighbours = s_fillNeighbourData(vap, count);
    amxd_object_t* pNeighbourObject = amxd_object_get(vap->pBus, "Neighbour");
    int index = 0;
    amxc_llist_for_each(it, &pNeighbourObject->instances) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        assert_non_null(instance);
        assert_non_null(instance->priv);
        assert_non_null(pNeighbours[index]);
        printf("test Neighbour before remove %d \n", index);
        char* bssid = NULL;
        char* ssid = NULL;
        bssid = swl_typeCharPtr_fromObjectParamDef(instance, "BSSID", 0);
        ssid = swl_typeCharPtr_fromObjectParamDef(instance, "SSID", 0);
        assert_string_equal(pNeighbours[index]->bssid, bssid);
        assert_string_equal(pNeighbours[index]->ssid, ssid);

        assert_int_equal(pNeighbours[index]->channel, swl_typeUInt32_fromObjectParamDef(instance, "Channel", 0));
        assert_int_equal(pNeighbours[index]->bssidInfomation, swl_typeUInt32_fromObjectParamDef(instance, "Information", 0));
        assert_int_equal(pNeighbours[index]->operatingClass, swl_typeUInt32_fromObjectParamDef(instance, "OperatingClass", 0));
        assert_int_equal(pNeighbours[index]->phyType, swl_typeUInt32_fromObjectParamDef(instance, "PhyType", 0));
        index++;
        free(bssid);
        free(ssid);
    }
    //remove the 3 first elements
    for(int i = 0; i < 3; i++) {
        if(pNeighbours[i] != NULL) {
            s_removeNeighbourAp(vap, pNeighbours[i]);
        }
    }

    // check Neighbours elements after remove of the 3 first elements
    pNeighbourObject = amxd_object_get(vap->pBus, "Neighbour");
    assert_non_null(pNeighbourObject);
    index = 3;
    amxc_llist_for_each(it, &pNeighbourObject->instances) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        index++;
        assert_non_null(instance);
        assert_non_null(instance->priv);
        assert_non_null(pNeighbours[index - 1]);
        printf("test Neighbour after remove %d \n", index - 1);
        char* bssid = NULL;
        char* ssid = NULL;
        bssid = swl_typeCharPtr_fromObjectParamDef(instance, "BSSID", 0);
        ssid = swl_typeCharPtr_fromObjectParamDef(instance, "SSID", 0);
        assert_string_equal(pNeighbours[index - 1]->bssid, bssid);

        assert_string_equal(pNeighbours[index - 1 ]->ssid, ssid);
        assert_int_equal(pNeighbours[index - 1 ]->channel, swl_typeUInt32_fromObjectParamDef(instance, "Channel", 0));
        assert_int_equal(pNeighbours[index - 1 ]->bssidInfomation, swl_typeUInt32_fromObjectParamDef(instance, "Information", 0));
        assert_int_equal(pNeighbours[index - 1]->operatingClass, swl_typeUInt32_fromObjectParamDef(instance, "OperatingClass", 0));
        assert_int_equal(pNeighbours[index - 1 ]->phyType, swl_typeUInt32_fromObjectParamDef(instance, "PhyType", 0));
        free(bssid);
        free(ssid);
        //remove the neighbor here
        s_removeNeighbourAp(vap, pNeighbours[index - 1]);

    }
    free(pNeighbours);


}


int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_addNeighborAP),
        cmocka_unit_test(test_removeNeighborAP),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

