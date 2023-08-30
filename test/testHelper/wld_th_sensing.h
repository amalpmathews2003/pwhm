/*
 * Copyright (c) 2022 SoftAtHome
 *
 * The information and source code contained herein is the exclusive
 * property of SoftAtHome and may not be disclosed, examined, or
 * reproduced in whole or in part without explicit written authorization
 * of the copyright owner.
 *
 */

#ifndef __WLD_TESTHELPER_SENSING_H__
#define __WLD_TESTHELPER_SENSING_H__
#include "wld_types.h"
#include "wld_th_types.h"

typedef struct {
    bool csiEnable;
    swl_macChar_t macAddr;
    uint32_t monitor_interval;
    wld_csiState_t csimonState;
} wld_th_rad_sensing_vendorData_t;

wld_th_rad_sensing_vendorData_t* s_getSensingVendorData(T_Radio* pRad);
void freeSensingVendorData(T_Radio* pRad);
int wld_th_rad_sensing_create_hook(T_Radio* pRad);
void wld_th_rad_sensing_destroy_hook(T_Radio* pRad);
swl_rc_ne wld_th_rad_sensing_cmd(T_Radio* pRad);
swl_rc_ne wld_th_rad_sensing_addClient(T_Radio* pRad, wld_csiClient_t* client);
swl_rc_ne wld_th_rad_sensing_delClient(T_Radio* pRad, swl_macChar_t macAddr);
swl_rc_ne wld_th_rad_sensing_csiStats(T_Radio* pRad, wld_csiState_t* csimonState);
swl_rc_ne wld_th_rad_sensing_resetStats(T_Radio* pRad);

#endif
