/*
 * Copyright (c) 2023 SoftAtHome
 *
 * The information and source code contained herein is the exclusive
 * property of SoftAtHome and may not be disclosed, examined, or
 * reproduced in whole or in part without explicit written authorization
 * of the copyright owner.
 *
 */

#include "wld_th_vap.h"
#include "wld.h"
#include "wld_accesspoint.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "wld_th_mockVendor.h"
#include "wld_th_sensing.h"
#include "test-toolbox/ttb_assert.h"


wld_th_rad_sensing_vendorData_t* s_getSensingVendorData(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(!pRad->vendorData) {
        wld_th_rad_sensing_vendorData_t* vendorData = calloc(1, sizeof(wld_th_rad_sensing_vendorData_t));
        ttb_assert_non_null(vendorData);
        pRad->vendorData = vendorData;
    }
    return pRad->vendorData;
}

void freeSensingVendorData(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(pRad->vendorData) {
        free(pRad->vendorData);
        pRad->vendorData = NULL;
    }
}

swl_rc_ne wld_th_rad_sensing_cmd(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(pRad->vendorData) {
        wld_th_rad_sensing_vendorData_t* sensingVendorData = s_getSensingVendorData(pRad);
        ttb_assert_non_null(sensingVendorData);
        sensingVendorData->csiEnable = pRad->csiEnable;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_addClient(T_Radio* pRad, wld_csiClient_t* client) {
    ttb_assert_non_null(pRad);
    wld_th_rad_sensing_vendorData_t* sensingVendorData = s_getSensingVendorData(pRad);
    ttb_assert_non_null(sensingVendorData);

    strcpy(sensingVendorData->macAddr.cMac, client->macAddr.cMac);
    sensingVendorData->monitor_interval = client->monitorInterval;
    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_delClient(T_Radio* pRad, swl_macChar_t macAddr) {
    ttb_assert_non_null(pRad);
    wld_th_rad_sensing_vendorData_t* sensingVendorData = s_getSensingVendorData(pRad);
    ttb_assert_non_null(sensingVendorData);

    strcpy(sensingVendorData->macAddr.cMac, macAddr.cMac);
    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_csiStats(T_Radio* pRad _UNUSED, wld_csiState_t* csimonState) {
    /* Return csimonState values */
    csimonState->nullFrameCounter = 10;
    csimonState->m2mTransmitCounter = 20;
    csimonState->userTransmitCounter = 30;
    csimonState->nullFrameAckFailCounter = 40;
    csimonState->receivedOverflowCounter = 50;
    csimonState->transmitFailCounter = 60;
    csimonState->userTransmitFailCounter = 70;

    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_resetStats(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    wld_th_rad_sensing_vendorData_t* sensingVendorData = s_getSensingVendorData(pRad);
    ttb_assert_non_null(sensingVendorData);

    sensingVendorData->csimonState.nullFrameCounter = 1;
    sensingVendorData->csimonState.m2mTransmitCounter = 2;
    sensingVendorData->csimonState.userTransmitCounter = 3;
    sensingVendorData->csimonState.nullFrameAckFailCounter = 4;
    sensingVendorData->csimonState.receivedOverflowCounter = 5;
    sensingVendorData->csimonState.transmitFailCounter = 6;
    sensingVendorData->csimonState.userTransmitFailCounter = 7;

    return SWL_RC_OK;
}
