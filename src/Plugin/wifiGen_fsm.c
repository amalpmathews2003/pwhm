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
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wld/wld_rad_nl80211.h"

#include "wifiGen_fsm.h"

#include "wifiGen_rad.h"
#include "wifiGen_vap.h"
#include "wifiGen_hapd.h"
#include "wifiGen_wpaSupp.h"
#include "wifiGen_events.h"

#define ME "genFsm"

/*
 * List of actions that can be used to apply hostapd configuration,
 * starting from the most global (most critical)
 */
static const wifiGen_fsmStates_e sApplyActions[] = {
    GEN_FSM_START_HOSTAPD,    // leads to restarting hostapd
    GEN_FSM_ENABLE_HOSTAPD,   // leads to enabling (usually toggling) hostapd main interface
    GEN_FSM_UPDATE_HOSTAPD,   // leads to refreshing hostpad with SIGHUP (reloads conf file)
    GEN_FSM_RELOAD_AP_SECKEY, // leads to reloading AP secret key conf from memory
    GEN_FSM_UPDATE_BEACON,    // leads to refresh AP beacon frame from mem conf
};
/*
 * List of params (fsm actions) that may be warm applied,
 * at runtime, without restarting hostapd
 */
static const wifiGen_fsmStates_e sDynConfActions[] = {
    GEN_FSM_MOD_SSID,
    GEN_FSM_MOD_CHANNEL,
    GEN_FSM_ENABLE_AP,
    GEN_FSM_MOD_SEC,
    GEN_FSM_MOD_AP,
};
/*
 * returns the index of the first met fsm action (among provided actionArray),
 * that is more global than the action argument (i.e: implicitly applying it)
 */
static int s_fetchAction(const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                         wifiGen_fsmStates_e action) {
    ASSERTS_NOT_NULL(actionArr, -1, ME, "NULL");
    while(actionArrSize > 0) {
        if(actionArr[--actionArrSize] == action) {
            return actionArrSize;
        }
    }
    return -1;
}
/*
 * returns the enum id of the first met scheduled fsm action (among provided actionArray),
 * that is more global than the provided indexed action (i.e: implicitly applying it)
 */
static int s_fetchHigherAction(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                               const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                               int32_t minId) {
    ASSERTS_FALSE(minId < 0, -1, ME, "out of order");
    for(int32_t i = minId; i >= 0 && i < (int32_t) actionArrSize; i--) {
        if(isBitSetLongArray(bitMapArr, bitMapArrSize, actionArr[i])) {
            return i;
        }
    }
    return -1;
}
/*
 * returns the enum id of the first met scheduled fsm action (among provided actionArray),
 * that is more global than the provided enumerated action (i.e: implicitly applying it)
 */
static int s_fetchHigherActionExt(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                  const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                  wifiGen_fsmStates_e minAction) {
    int pos = s_fetchAction(actionArr, actionArrSize, minAction);
    if(pos <= 0) {
        return -1;
    }
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, actionArr, actionArrSize, pos - 1);
}
/*
 * clear from fsm actions bitmap, all fsm actions (among action array) implicitly applied
 * by provided indexed action (index in action array)
 */
static void s_clearLowerActions(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                int32_t minId) {
    for(int32_t i = minId; i >= 0 && i < (int32_t) actionArrSize; i++) {
        clearBitLongArray(bitMapArr, bitMapArrSize, actionArr[i]);
    }
}
/*
 * clear from fsm actions bitmap, all fsm actions (among action array) implicitly applied
 * by provided enumerated action
 */
static void s_clearLowerActionsExt(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                   const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                   wifiGen_fsmStates_e minAction) {
    int pos = s_fetchAction(actionArr, actionArrSize, minAction);
    ASSERTS_FALSE(pos < 0, , ME, "not found");
    s_clearLowerActions(bitMapArr, bitMapArrSize, actionArr, actionArrSize, pos + 1);
}
/*
 * fetch first met scheduled conf action implicitly applying fsm action
 */
static int s_fetchHigherApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    return s_fetchHigherActionExt(bitMapArr, bitMapArrSize, sApplyActions, SWL_ARRAY_SIZE(sApplyActions), action);
}
/*
 * clear conf applying fsm actions, that will be implicitly applied with than provided action
 */
static void s_clearLowerApplyActions(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    s_clearLowerActionsExt(bitMapArr, bitMapArrSize, sApplyActions, SWL_ARRAY_SIZE(sApplyActions), action);
}
/*
 * schedule fsm conf applying action, if no more global actions are already scheduled.
 * If successful, less global fsm conf applying actions are cleared (because implicit)
 */
static bool s_setApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    if(s_fetchHigherApplyAction(bitMapArr, bitMapArrSize, action) >= 0) {
        return false;
    }
    setBitLongArray(bitMapArr, bitMapArrSize, action);
    s_clearLowerApplyActions(bitMapArr, bitMapArrSize, action);
    return true;
}
/*
 * fetch for scheduled conf applying fsm action
 */
static int s_fetchApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    int32_t size = SWL_ARRAY_SIZE(sApplyActions);
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, sApplyActions, size, size - 1);
}
/*
 * fetch for scheduled dynamically configurable param fsm action
 */
static int s_fetchDynConfAction(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    int32_t size = SWL_ARRAY_SIZE(sDynConfActions);
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, sDynConfActions, size, size - 1);
}
/*
 * clear all dynamically configurable param fsm actions
 * This is used when the conf will be entirely applied by restarting hostapd
 */
static void s_clearDynConfActions(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    s_clearLowerActions(bitMapArr, bitMapArrSize, sDynConfActions, SWL_ARRAY_SIZE(sDynConfActions), 0);
}
/*
 * schedule next conf applying fsm action after setting a dynamic param
 */
static void s_schedNextAction(wld_secDmn_action_rc_ne action, T_AccessPoint* pAP, T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    switch(action) {
    case SECDMN_ACTION_OK_NEED_RESTART:
        if(s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
        }
        break;
    case SECDMN_ACTION_OK_NEED_TOGGLE:
        if(s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        }
        break;
    case SECDMN_ACTION_OK_NEED_SIGHUP:
        s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
        break;
    case SECDMN_ACTION_OK_NEED_RELOAD_SECKEY:
        if(s_fetchHigherApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY) >= 0) {
            return;
        }
        if(s_setApplyAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY);
        }
        break;
    case SECDMN_ACTION_OK_NEED_UPDATE_BEACON:
        if(s_fetchHigherApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY) >= 0) {
            return;
        }
        if(s_setApplyAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
        }
        break;
    default:
        return;
    }
}

static bool s_doEnableAp(T_AccessPoint* pAP, T_Radio* pRad) {
    ASSERTI_TRUE(pRad->enable, true, ME, "%s: radio disabled", pRad->Name);
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    bool enable = pAP->enable;
    SAH_TRACEZ_INFO(ME, "%s: enable vap %d", pAP->alias, enable);
    wld_secDmn_action_rc_ne rc;
    /*
     * first, set dyn ena/disabling vap params, before applying any action
     */
    if((rc = wld_ap_hostapd_setEnableVap(pAP, enable)) < SECDMN_ACTION_OK_DONE) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to save enable %d", pAP->alias, enable);
    } else if(!enable) {
        wld_ap_hostapd_deauthAllStations(pAP);
        if((rc = wld_ap_hostapd_enableVap(pAP, false)) == SECDMN_ACTION_OK_DONE) {
            /*
             * in hostapd older than 2.10, disabling one bss leads
             * to disabling all BSSs of same main interface.
             * In order to restore right status of other BSSs,
             * we schedule fsm sync_state.
             */
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_SYNC_STATE);
        }
    } else {
        int currState = wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pAP->alias);
        if(!currState) {
            // we have to re-enable bss net interface before trying to (re)start beaconing
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, true);
        }
        if(((rc = wld_ap_hostapd_enableVap(pAP, true)) < SECDMN_ACTION_OK_DONE) && (!currState)) {
            //if failed to start beaconing, restore the net iface disabled state.
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, false);
        }
    }
    wld_vap_updateState(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to apply enable %d", pAP->alias, enable);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doRadDisable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: disable rad", pRad->Name);
    pRad->pFA->mfn_wrad_enable(pRad, false, SET | DIRECT);
    return true;
}

static bool s_doRadEnable(T_Radio* pRad) {
    ASSERTS_TRUE(pRad->enable, true, ME, "%s: rad disabled", pRad->Name);
    pRad->pFA->mfn_wrad_enable(pRad, true, SET | DIRECT);
    return true;
}

static bool s_doRadSync(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: sync rad conf", pRad->Name);
    pRad->pFA->mfn_wrad_sync(pRad, SET | DIRECT);
    return true;
}

static bool s_doStopHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hapd stopped", pRad->Name);
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD)) {
        T_AccessPoint* pAP;
        wld_rad_forEachAp(pAP, pRad) {
            wld_ap_hostapd_deauthAllStations(pAP);
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: stop hostapd", pRad->Name);
    wifiGen_hapd_stopDaemon(pRad);
    pRad->fsmRad.timeout_msec = 100;
    return true;
}

static bool s_doConfHostapd(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: write hostapd conf", pRad->Name);
    wifiGen_hapd_writeConfig(pRad);
    return true;
}

static bool s_doUpdateHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: reload hostapd", pRad->Name);
    wifiGen_hapd_reloadDaemon(pRad);
    return true;
}

static bool s_doStartHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wld_rad_hasEnabledVap(pRad), true, ME, "%s: radio has no enabled vap", pRad->Name);
    ASSERTS_TRUE(pRad->enable, true, ME, "%s: radio disabled", pRad->Name);
    ASSERTS_FALSE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd running", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: start hostapd", pRad->Name);
    wifiGen_hapd_startDaemon(pRad);
    return true;
}

static bool s_doDisableHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    wld_rad_hostapd_disable(pRad);
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static bool s_doEnableHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    wld_rad_hostapd_enable(pRad);
    return true;
}

static bool s_doSetCountryCode(T_Radio* pRad) {
    pRad->pFA->mfn_wrad_regdomain(pRad, NULL, 0, SET | DIRECT);
    //Here radio shall be down: so toggle shortly to force reloading nl80211 wiphy channels
    if(wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pRad->Name) == 0) {
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 1);
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 0);
    }
    pRad->pFA->mfn_wrad_poschans(pRad, NULL, 0);
    return true;
}

static bool s_doSetBssid(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTI_TRUE(pAP && pAP->pSSID && !swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->BSSID),
                 true, ME, "Null mac");
    SAH_TRACEZ_INFO(ME, "%s: set bssid", pAP->alias);
    wifiGen_vap_setBssid(pAP);
    return true;
}

static bool s_doSetSsid(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, true, ME, "%s: no ssid ctx", pAP->alias);
    SAH_TRACEZ_INFO(ME, "%s: set ssid (%s)", pSSID->Name, pSSID->SSID);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setSsid(pAP, pSSID->SSID);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set ssid", pSSID->Name);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doSetApSec(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setSecParams(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set secret key", pAP->alias);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doSyncAp(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setNoSecParams(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set common params", pAP->alias);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doReloadApSecKey(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_ap_hostapd_reloadSecKey(pAP, "reloadSecKey");
    wld_ap_hostapd_updateBeacon(pAP, "updateBeacon");
    return true;
}

static bool s_doUpdateBeacon(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_ap_hostapd_updateBeacon(pAP, "updateBeacon");
    return true;
}

static bool s_doSetChannel(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: set channel %d", pRad->Name, pRad->channel);
    if(pRad->pFA->mfn_misc_has_support(pRad, NULL, "CSA", 0)) {
        if((wld_channel_is_band_usable(pRad->targetChanspec.chanspec)) ||
           (pRad->pFA->mfn_misc_has_support(pRad, NULL, "DFS_OFFLOAD", 0))) {
            wld_rad_hostapd_switchChannel(pRad);
            return true;
        }
    }
    wld_secDmn_action_rc_ne rc = wld_rad_hostapd_setChannel(pRad);
    s_schedNextAction(rc, NULL, pRad);
    return true;
}

static bool s_doSyncState(T_Radio* pRad) {
    /*
     * when hostapd is alive (i.e radio administratively enabled)
     * and handling DFS clearing (i.e driver does not support DFS_OFFLOAD)
     * and that current band channels are no more usable
     * (previous CAC have expired, or channel not available)
     * => we have to recover by toggling hapd (which eventually triggers a new dfs cac),
     *    otherwise radio remains passively down
     */
    if((wifiGen_hapd_isAlive(pRad)) && (pRad->detailedState == CM_RAD_DOWN)) {
        swl_chanspec_t chanSpec;
        _UNUSED_(chanSpec);
        if((!wld_channel_is_band_usable(wld_rad_getSwlChanspec(pRad)) ||
            (wld_rad_nl80211_getChannel(pRad, &chanSpec) < SWL_RC_OK))) {
            SAH_TRACEZ_WARNING(ME, "%s: curr band [%s] is unusable", pRad->Name, pRad->channelsInUse);
            if(!pRad->pFA->mfn_misc_has_support(pRad, NULL, "DFS_OFFLOAD", 0)) {
                SAH_TRACEZ_WARNING(ME, "%s: no dfs_offload => need to toggle hostapd recover radio", pRad->Name);
                s_schedNextAction(SECDMN_ACTION_OK_NEED_TOGGLE, NULL, pRad);
                return true;
            }
        }
    }
    wifiGen_hapd_syncVapStates(pRad);
    return true;
}

#define FIRST_DELAY_MS 3000
#define RETRY_DELAY_MS 1000
#define WPA_SUPP_MAX_ATTEMPTS 3
static uint8_t wpaSuppStartingAttempts = 0;
static amxp_timer_t* wpaSuppTimer = NULL;

static void s_startWpaSuppTimer(amxp_timer_t* timer, void* userdata) {
    ASSERTS_NOT_NULL(timer, , ME, "NULL");
    ASSERTS_NOT_NULL(userdata, , ME, "NULL");
    T_EndPoint* pEP = (T_EndPoint*) userdata;
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    bool ret = wifiGen_hapd_isAlive(pRad);
    if(ret) {
        SAH_TRACEZ_WARNING(ME, "%s: disable hostapd", pRad->Name);
        wld_rad_hostapd_disable(pRad);
        wpaSuppStartingAttempts = 0;
        amxp_timer_delete(&timer);
    } else if(wpaSuppStartingAttempts < WPA_SUPP_MAX_ATTEMPTS) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd is not yet alive, waiting (%d/%d)..", pRad->Name, wpaSuppStartingAttempts, WPA_SUPP_MAX_ATTEMPTS);
        wpaSuppStartingAttempts++;
    } else {
        wpaSuppStartingAttempts = 0;
        amxp_timer_delete(&timer);
        SAH_TRACEZ_ERROR(ME, "%s: Fail to disable hostapd", pRad->Name);
    }
}
static bool s_doEnableEp(T_EndPoint* pEP, T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: enable endpoint", pEP->Name);
    // check if there is a running hostapd in order to disable it in order allow to wpa_supplicant to connect.
    // Otherwise, wpa_supplicant will fail to connect
    if(wifiGen_hapd_isRunning(pRad)) {
        wpaSuppStartingAttempts = 0;
        amxp_timer_new(&wpaSuppTimer, s_startWpaSuppTimer, pEP);
        amxp_timer_set_interval(wpaSuppTimer, RETRY_DELAY_MS);
        amxp_timer_start(wpaSuppTimer, FIRST_DELAY_MS);
    }
    return true;
}

static bool s_doConnectedEp(T_EndPoint* pEP, T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: connected endpoint", pEP->Name);
    if(wifiGen_hapd_isRunning(pRad)) {
        // update hostapd channel
        wld_secDmn_action_rc_ne rc = wld_rad_hostapd_setChannel(pRad);
        s_schedNextAction(rc, NULL, pRad);
    }
    return true;
}

static bool s_doStopWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s: stop wpa_supplicant", pEP->Name);
    wifiGen_wpaSupp_stopDaemon(pEP);
    return true;
}

static bool s_doStartWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    ASSERTS_TRUE(pEP->enable, true, ME, "%s: endpoint disabled", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: start wpa_supplicant", pEP->Name);
    wifiGen_wpaSupp_startDaemon(pEP);
    return true;
}

static bool s_doConfWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s: write wpa_supplicant config", pEP->Name);
    wifiGen_wpaSupp_writeConfig(pEP);
    return true;
}

static bool s_doReloadWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    ASSERTS_TRUE(wifiGen_wpaSupp_isRunning(pEP), true, ME, "%s: wpa_supplicant stopped", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: reload wpa_supplicant", pEP->Name);
    wifiGen_wpaSupp_reloadDaemon(pEP);
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
    bool currentEnable = wld_rad_isUpExt(pRad);
    SAH_TRACEZ_ERROR(ME, "%s : curr %u tgt %u status %u enable %u", pRad->Name,
                     currentEnable, targetEnable, pRad->status, pRad->enable);

    if(targetEnable != currentEnable) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
    }
}

static void s_checkApDependency(T_AccessPoint* pAP, T_Radio* pRad) {
    /*
     * Disabling/enabling radio will implicitly interact with hostapd,
     * which will manage the related VAPs
     */
    if(isBitSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    if(s_fetchDynConfAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW) >= 0) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
        if(!wifiGen_hapd_isRunning(pRad)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        }
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
        s_clearLowerApplyActions(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        s_clearDynConfActions(pAP->fsm.FSM_AC_BitActionArray, FSM_BW);
    }
}

static void s_checkRadDependency(T_Radio* pRad) {

    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
        s_clearLowerApplyActions(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_COUNTRYCODE) ||
       isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD)) {
        s_schedNextAction(SECDMN_ACTION_OK_NEED_TOGGLE, NULL, pRad);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }
    if((s_fetchApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW) >= 0) ||
       (s_fetchDynConfAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW) >= 0)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }
}

void s_checkEpDependency(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_EP)) {
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
    }
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP)) {
        clearBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_WPASUPP);
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_WPASUPP);
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_WPASUPP);
    }
}

wld_fsmMngr_action_t actions[GEN_FSM_MAX] = {
    {FSM_ACTION(GEN_FSM_STOP_HOSTAPD), .doRadFsmAction = s_doStopHostapd},
    {FSM_ACTION(GEN_FSM_STOP_WPASUPP), .doEpFsmAction = s_doStopWpaSupp},
    {FSM_ACTION(GEN_FSM_DISABLE_HOSTAPD), .doRadFsmAction = s_doDisableHostapd},
    {FSM_ACTION(GEN_FSM_DISABLE_RAD), .doRadFsmAction = s_doRadDisable},
    {FSM_ACTION(GEN_FSM_SYNC_RAD), .doRadFsmAction = s_doRadSync},
    {FSM_ACTION(GEN_FSM_MOD_COUNTRYCODE), .doRadFsmAction = s_doSetCountryCode},
    {FSM_ACTION(GEN_FSM_MOD_BSSID), .doVapFsmAction = s_doSetBssid},
    {FSM_ACTION(GEN_FSM_MOD_SEC), .doVapFsmAction = s_doSetApSec},
    {FSM_ACTION(GEN_FSM_MOD_AP), .doVapFsmAction = s_doSyncAp},
    {FSM_ACTION(GEN_FSM_MOD_SSID), .doVapFsmAction = s_doSetSsid},
    {FSM_ACTION(GEN_FSM_MOD_CHANNEL), .doRadFsmAction = s_doSetChannel},
    {FSM_ACTION(GEN_FSM_MOD_HOSTAPD), .doRadFsmAction = s_doConfHostapd},
    {FSM_ACTION(GEN_FSM_MOD_WPASUPP), .doEpFsmAction = s_doConfWpaSupp},
    {FSM_ACTION(GEN_FSM_RELOAD_AP_SECKEY), .doVapFsmAction = s_doReloadApSecKey},
    {FSM_ACTION(GEN_FSM_UPDATE_BEACON), .doVapFsmAction = s_doUpdateBeacon},
    {FSM_ACTION(GEN_FSM_UPDATE_HOSTAPD), .doRadFsmAction = s_doUpdateHostapd},
    {FSM_ACTION(GEN_FSM_UPDATE_WPASUPP), .doEpFsmAction = s_doReloadWpaSupp},
    {FSM_ACTION(GEN_FSM_ENABLE_HOSTAPD), .doRadFsmAction = s_doEnableHostapd},
    {FSM_ACTION(GEN_FSM_START_HOSTAPD), .doRadFsmAction = s_doStartHostapd},
    {FSM_ACTION(GEN_FSM_START_WPASUPP), .doEpFsmAction = s_doStartWpaSupp},
    {FSM_ACTION(GEN_FSM_ENABLE_RAD), .doRadFsmAction = s_doRadEnable},
    {FSM_ACTION(GEN_FSM_ENABLE_AP), .doVapFsmAction = s_doEnableAp},
    {FSM_ACTION(GEN_FSM_ENABLE_EP), .doEpFsmAction = s_doEnableEp},
    {FSM_ACTION(GEN_FSM_CONNECTED_EP), .doEpFsmAction = s_doConnectedEp},
    {FSM_ACTION(GEN_FSM_SYNC_STATE), .doRadFsmAction = s_doSyncState},
};

wld_fsmMngr_t mngr = {
    .checkPreDependency = s_checkPreRadDependency,
    .checkEpDependency = s_checkEpDependency,
    .checkVapDependency = s_checkApDependency,
    .checkRadDependency = s_checkRadDependency,
    .actionList = actions,
    .nrFsmBits = GEN_FSM_MAX,
};

void wifiGen_fsm_doInit(vendor_t* vendor) {
    wld_fsm_init(vendor, &mngr);
}
