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
#include "swl/swl_common.h"

#include "wld/wld_common.h"
#include "wld/wld_linuxIfUtils.h"

#include "wifiGen_fsm.h"

#include "wifiGen_rad.h"
#include "wifiGen_vap.h"
#include "wifiGen_hapd.h"

#define ME "genFsm"

static bool s_doEnableAp(T_AccessPoint* pAP, T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: enable vap", pAP->alias);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pAP->pRadio), pAP->alias, pRad->enable && pAP->enable);
    return true;
}

static bool s_doRadDisable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: disable rad", pRad->Name);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, false);
    pRad->detailedState = CM_RAD_DOWN;
    wld_rad_updateState(pRad, false);
    return true;
}

static bool s_doStopHostapd(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: stop hostapd", pRad->Name);
    wifiGen_hapd_stopDaemon(pRad);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        wld_vap_updateState(pAP);
    }
    pRad->fsmRad.timeout_msec = 100;
    return true;
}

static bool s_doConfHostapd(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: write hostapd conf", pRad->Name);
    wifiGen_hapd_writeConfig(pRad);
    return true;
}

static bool s_doReloadHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: reload hostapd", pRad->Name);
    wifiGen_hapd_reloadDaemon(pRad);
    return true;
}

static bool s_doStartHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wld_rad_hasEnabledVap(pRad), true, "%s: radio has not enabled vap", pRad->Name);
    ASSERTS_TRUE(pRad->enable, true, ME, "%s: radio disabled", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: start hostapd", pRad->Name);
    wifiGen_hapd_startDaemon(pRad);
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static bool s_doSetBssid(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTI_TRUE(pAP && pAP->pSSID && !swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->BSSID),
                 true, ME, "Null mac");
    SAH_TRACEZ_INFO(ME, "%s: set bssid", pAP->alias);
    wifiGen_vap_setBssid(pAP);
    return true;
}

static bool s_doRequestStatus(T_Radio* pRad) {
    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pRad) {
        wld_vap_updateState(pAP);
        SAH_TRACEZ_ERROR(ME, "%s status %u", pAP->alias, pAP->status);
    }

    T_EndPoint* pEP = wld_rad_getFirstEp(pRad);
    bool enabled = (wld_rad_hasActiveVap(pRad))
        || (pEP != NULL && pEP->status == APSTI_ENABLED);

    if(enabled) {
        pRad->detailedState = CM_RAD_UP;
    } else {
        pRad->detailedState = CM_RAD_DOWN;
    }

    wld_rad_updateState(pRad, false);
    return true;
}

/*
 * Check if we need to do radio up down actions. Need to happen if it's first interface that goes up.
 */
static void s_checkPreRadDependency(T_Radio* pRad _UNUSED) {
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD) || (pRad->status == RST_ERROR)) {
        SAH_TRACEZ_ERROR(ME, "%s : no pre dep change", pRad->Name);
        return;
    }

    bool targetEnable = (pRad->enable && (wld_rad_hasEnabledVap(pRad) || wld_rad_hasEnabledEp(pRad)));
    bool currentEnable = (pRad->status != RST_DOWN) && (pRad->status != RST_DORMANT);
    SAH_TRACEZ_ERROR(ME, "%s : curr %u tgt %u status %u enable %u", pRad->Name,
                     currentEnable, targetEnable, pRad->status, pRad->enable);

    if(targetEnable != currentEnable) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
    }
}

static void s_checkApDependency(T_AccessPoint* pAP, T_Radio* pRad) {
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD)) {
        setBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_AP);
    }
    if(isBitSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
}

static void s_checkRadDependency(T_Radio* pRad) {

    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_READ_STATE);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
        clearBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }
}

wld_fsmMngr_action_t actions[GEN_FSM_MAX] = {
    {FSM_ACTION(GEN_FSM_DISABLE_RAD), .doRadFsmAction = s_doRadDisable},
    {FSM_ACTION(GEN_FSM_STOP_HOSTAPD), .doRadFsmAction = s_doStopHostapd},
    {FSM_ACTION(GEN_FSM_MOD_BSSID), .doVapFsmAction = s_doSetBssid},
    {FSM_ACTION(GEN_FSM_MOD_HOSTAPD), .doRadFsmAction = s_doConfHostapd},
    {FSM_ACTION(GEN_FSM_UPDATE_HOSTAPD), .doRadFsmAction = s_doReloadHostapd},
    {FSM_ACTION(GEN_FSM_START_HOSTAPD), .doRadFsmAction = s_doStartHostapd},
    {FSM_ACTION(GEN_FSM_ENABLE_RAD)},// Rad enable requires no config
    {FSM_ACTION(GEN_FSM_ENABLE_AP), .doVapFsmAction = s_doEnableAp},
    {FSM_ACTION(GEN_FSM_READ_STATE), .doRadFsmAction = s_doRequestStatus},
};

wld_fsmMngr_t mngr = {
    .checkPreDependency = s_checkPreRadDependency,
//    .checkEpDependency = s_checkEpDependency,
    .checkVapDependency = s_checkApDependency,
    .checkRadDependency = s_checkRadDependency,
    .actionList = actions,
    .nrFsmBits = GEN_FSM_MAX,
};

void wifiGen_fsm_doInit(vendor_t* vendor) {
    wld_fsm_init(vendor, &mngr);
}
