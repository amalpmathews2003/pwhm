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

#include "wld_th_vap.h"
#include "wld.h"
#include "wld_accesspoint.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "wld_th_mockVendor.h"
#include "wld_th_sensing.h"
#include "test-toolbox/ttb_assert.h"

wld_th_rad_sensing_vendorData_t* sensingGetVendorData(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(!pRad->vendorData) {
        wld_th_rad_sensing_vendorData_t* vendorData = calloc(1, sizeof(wld_th_rad_sensing_vendorData_t));
        ttb_assert_non_null(vendorData);
        pRad->vendorData = vendorData;
    }
    return pRad->vendorData;
}

void sensingFreeVendorData(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(pRad->vendorData) {
        free(pRad->vendorData);
        pRad->vendorData = NULL;
    }
}

swl_rc_ne wld_th_rad_sensing_cmd(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    if(pRad->vendorData) {
        wld_th_rad_sensing_vendorData_t* sensingVendorData = sensingGetVendorData(pRad);
        ttb_assert_non_null(sensingVendorData);
        sensingVendorData->csiEnable = pRad->csiEnable;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_addClient(T_Radio* pRad, wld_csiClient_t* client) {
    ttb_assert_non_null(pRad);
    wld_th_rad_sensing_vendorData_t* sensingVendorData = sensingGetVendorData(pRad);
    ttb_assert_non_null(sensingVendorData);

    if(client->monitorInterval > sensingVendorData->maxMonitorInterval) {
        return SWL_RC_ERROR;
    }

    if(!swl_str_matches(client->macAddr.cMac, sensingVendorData->macAddr.cMac)) {
        if(sensingVendorData->maxClientsNbrs == 0) {
            return SWL_RC_ERROR;
        }
        sensingVendorData->maxClientsNbrs--;
        strcpy(sensingVendorData->macAddr.cMac, client->macAddr.cMac);
    }

    sensingVendorData->monitorInterval = client->monitorInterval;
    return SWL_RC_OK;
}

swl_rc_ne wld_th_rad_sensing_delClient(T_Radio* pRad, swl_macChar_t macAddr) {
    ttb_assert_non_null(pRad);
    wld_th_rad_sensing_vendorData_t* sensingVendorData = sensingGetVendorData(pRad);
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
    wld_th_rad_sensing_vendorData_t* sensingVendorData = sensingGetVendorData(pRad);
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
