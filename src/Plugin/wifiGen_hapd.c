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
#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_wpaCtrl_events.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"
#include "wifiGen_rad.h"

#define ME "genHapd"
#define HOSTAPD_CONF_FILE_PATH_FORMAT "/tmp/%s_hapd.conf"
#define HOSTAPD_ARGS_FORMAT "-ddt"

#define HOSTAPD_EXIT_REASON_SUCCESS 0
/*
 * hostapd exits with retcode -1, when failing to:
 * - alloc memory for internal usage
 * - init global ctx
 * - init wpa ctrl interface (comm sock)
 */
#define HOSTAPD_EXIT_REASON_INIT_FAIL -1
/*
 * hostapd exits with retcode 1, when failing to:
 * - detect local interfaces
 * - parse config file or load config sections of global/iface/bss
 */
#define HOSTAPD_EXIT_REASON_LOAD_FAIL 1

bool wifiGen_hapd_isStartable(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pRad->hostapd, false, ME, "NULL");
    return (pRad->enable && wld_rad_hasEnabledVap(pRad));
}

bool wifiGen_hapd_isStarted(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isEnabled(pRad->hostapd)));
}

void wifiGen_hapd_restoreMainIface(T_Radio* pRad) {
    T_AccessPoint* pMainAP = wld_rad_hostapd_getCfgMainVap(pRad);
    ASSERTI_NOT_NULL(pMainAP, , ME, "No vaps");
    wifiGen_hapd_enableVapWpaCtrlIface(pMainAP);
    ASSERTI_FALSE(pMainAP->index > 0, , ME, "%s already created", pMainAP->alias);
    SAH_TRACEZ_WARNING(ME, "%s: main iface %s => must be re-created", pRad->Name, pMainAP->alias);
    wifiGen_rad_addVap(pRad, pMainAP);
    if(swl_str_matches(pRad->Name, pMainAP->alias)) {
        pRad->index = pMainAP->index;
        pRad->wDevId = pMainAP->wDevId;
    }
}

static const char* s_getMainIface(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    const char* mainIface = pRad->Name;
    T_AccessPoint* pMainAP = wld_rad_hostapd_getCfgMainVap(pRad);
    if(pMainAP != NULL) {
        mainIface = pMainAP->alias;
    }
    return mainIface;
}

static void s_restartHapdCb(wld_secDmn_t* pSecDmn, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    const char* mainIface = s_getMainIface(pRad);
    ASSERTW_TRUE(wifiGen_hapd_isStartable(pRad), , ME, "%s: hostapd iface %s is not startable", pRad->Name, mainIface);
    ASSERTW_FALSE(wifiGen_hapd_isRunning(pRad), , ME, "%s: hostapd running", mainIface);
    SAH_TRACEZ_WARNING(ME, "%s: restarting hostapd", mainIface);
    wld_secDmn_restartCb(pSecDmn);
}

static void s_stopHapdCb(wld_secDmn_t* pHapdInst _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    const char* mainIface = s_getMainIface(pRad);
    SAH_TRACEZ_WARNING(ME, "%s: hostapd stopped", mainIface);
    wld_deamonExitInfo_t* pExitInfo = &pHapdInst->dmnProcess->lastExitInfo;
    if((pExitInfo != NULL) && (pExitInfo->isExited) &&
       (pExitInfo->exitStatus == HOSTAPD_EXIT_REASON_LOAD_FAIL)) {
        SAH_TRACEZ_ERROR(ME, "%s: invalid hostapd configuration", mainIface);
    }
    wld_rad_updateState(pRad, true);
    //finalize hapd cleanup
    wifiGen_hapd_stopDaemon(pRad);
    //restore main iface if removed by hostapd
    wifiGen_hapd_restoreMainIface(pRad);
}

static void s_initHapdDynCfgParamSupp(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, , ME, "NULL");
    /*
     * here declare hostapd cfg params to check (or to define) as supported or not
     * wld_secDmn_setCfgParamSupp(pRad->hostapd, "custom_param", SWL_TRL_UNKNOWN);
     */
    wld_secDmn_setCfgParamSupp(pRad->hostapd, "rnr", SWL_TRL_UNKNOWN);
    wld_secDmn_setCfgParamSupp(pRad->hostapd, "config_id", SWL_TRL_UNKNOWN);
}

swl_rc_ne wifiGen_hapd_init(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    char confFilePath[128] = {0};
    swl_str_catFormat(confFilePath, sizeof(confFilePath), HOSTAPD_CONF_FILE_PATH_FORMAT, pRad->Name);
    char startArgs[128] = {0};
    swl_str_copy(startArgs, sizeof(startArgs), HOSTAPD_ARGS_FORMAT);
    swl_strlst_cat(startArgs, sizeof(startArgs), " ", confFilePath);
    swl_rc_ne rc = wld_secDmn_init(&pRad->hostapd, HOSTAPD_CMD, startArgs, confFilePath, HOSTAPD_CTRL_IFACE_DIR);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to init hostapd", pRad->Name);
    s_initHapdDynCfgParamSupp(pRad);
    wld_secDmnEvtHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.restartCb = s_restartHapdCb;
    handlers.stopCb = s_stopHapdCb;
    wld_secDmn_setEvtHandlers(pRad->hostapd, &handlers, pRad);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_initVAP(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pRadio, SWL_RC_ERROR, ME, "No radio");
    ASSERT_NOT_NULL(pAP->pRadio->hostapd, SWL_RC_ERROR, ME, "hostapd not initialized");
    ASSERT_TRUE(wld_wpaCtrlInterface_init(&pAP->wpaCtrlInterface, pAP->alias, pAP->pRadio->hostapd->ctrlIfaceDir),
                SWL_RC_ERROR, ME, "%s: fail to init wpa_ctrl interface", pAP->alias);
    ASSERT_TRUE(wld_wpaCtrlMngr_registerInterface(pAP->pRadio->hostapd->wpaCtrlMngr, pAP->wpaCtrlInterface),
                SWL_RC_ERROR, ME, "%s: fail to add wpa_ctrl interface to mngr", pAP->alias);
    return SWL_RC_OK;
}

void wifiGen_hapd_cleanup(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Destroy hostapd", pRad->Name);
    wld_secDmn_cleanup(&pRad->hostapd);
}

void wifiGen_hapd_enableVapWpaCtrlIface(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    bool ena = wld_hostapd_ap_needWpaCtrlIface(pAP);
    wld_wpaCtrlInterface_setEnable(pAP->wpaCtrlInterface, ena);
}

static void s_enableWpaCtrlIfaces(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        wifiGen_hapd_enableVapWpaCtrlIface(pAP);
    }
}

swl_rc_ne wifiGen_hapd_startDaemon(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Start hostapd", pRad->Name);
    //restore main iface if removed by hostapd
    wifiGen_hapd_restoreMainIface(pRad);
    s_enableWpaCtrlIfaces(pRad);
    return wld_secDmn_start(pRad->hostapd);
}

swl_rc_ne wifiGen_hapd_stopDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Stop hostapd", pRad->Name);
    swl_rc_ne rc = wld_secDmn_stop(pRad->hostapd);
    ASSERTI_FALSE(rc < SWL_RC_OK, rc, ME, "%s: hostapd not running", pRad->Name);
    ASSERTI_NOT_EQUALS(rc, SWL_RC_CONTINUE, rc, ME, "%s: hostapd being stopped", pRad->Name);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->index > 0) {
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, 0);
        }
    }
    return rc;
}

swl_rc_ne wifiGen_hapd_reloadDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s : Reload Hostapd", pRad->Name);
    return wld_secDmn_reload(pRad->hostapd);
}

void wifiGen_hapd_writeConfig(T_Radio* pRad) {
    wld_hostapd_cfgFile_createExt(pRad);
}

static wld_wpaCtrlInterface_t* s_mainInterface(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, NULL, ME, "NULL");
    return wld_wpaCtrlMngr_getFirstReadyInterface(pRad->hostapd->wpaCtrlMngr);
}

bool wifiGen_hapd_isRunning(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isRunning(pRad->hostapd)));
}

bool wifiGen_hapd_isAlive(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isAlive(pRad->hostapd)));
}

SWL_TABLE(sHapdStateDescMaps,
          ARR(char* hapdStateDesc; chanmgt_rad_state radDetState; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"UNKNOWN", CM_RAD_UNKNOWN},
              {"UNINITIALIZED", CM_RAD_ERROR},
              {"COUNTRY_UPDATE", CM_RAD_CONFIGURING},
              {"ACS", CM_RAD_CONFIGURING},
              {"HT_SCAN", CM_RAD_CONFIGURING},
              {"DFS", CM_RAD_FG_CAC},
              {"DISABLED", CM_RAD_DOWN},
              {"ENABLED", CM_RAD_UP},
              ));
swl_rc_ne wifiGen_hapd_getRadState(T_Radio* pRad, chanmgt_rad_state* pDetailedState) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrlInterface_t* mainIface = s_mainInterface(pRad);
    ASSERTS_NOT_NULL(mainIface, SWL_RC_ERROR, ME, "%s: No main hapd wpactrl iface", pRad->Name);
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(mainIface), SWL_RC_ERROR,
                 ME, "%s: main wpactrl iface is not ready", pRad->Name);
    char reply[1024] = {0};
    char state[64] = {0};
    if((!wld_wpaCtrl_sendCmdSynced(mainIface, "STATUS", reply, sizeof(reply) - 1)) ||
       (wld_wpaCtrl_getValueStr(reply, "state", state, sizeof(state)) <= 0)) {
        SAH_TRACEZ_INFO(ME, "%s: status not yet available", pRad->Name);
        return SWL_RC_ERROR;
    }
    chanmgt_rad_state* pRadDetState = (chanmgt_rad_state*) swl_table_getMatchingValue(&sHapdStateDescMaps, 1, 0, state);
    ASSERTI_NOT_NULL(pRadDetState, SWL_RC_ERROR, ME, "%s: unknown hapd state(%s)", pRad->Name, state);
    W_SWL_SETPTR(pDetailedState, *pRadDetState);
    SAH_TRACEZ_INFO(ME, "%s: hapd state(%s) -> radDetState(%d)", pRad->Name, state, *pRadDetState);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_syncVapStates(T_Radio* pRad) {
    swl_rc_ne ret = SWL_RC_OK;

    /*
     * in case we missed BSS re-creation nl80211 event, refresh relative info (ifIndex, wdevId)
     */
    wifiGen_refreshVapsIfIdx(pRad);

    wld_rad_hostapd_updateAllVapsConfigId(pRad);

    chanmgt_rad_state detRadState = CM_RAD_UNKNOWN;
    wifiGen_hapd_getRadState(pRad, &detRadState);

    /*
     * Now as ifIndex are up to date, we can sync the enabling status
     */
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(!wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface) || (pAP->index <= 0) || (pAP->pBus == NULL)) {
            continue;
        }
        if((!pAP->enable) &&
           (pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, GET | DIRECT) > 0)) {
            if(pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, SET | DIRECT) == 0) {
                SAH_TRACEZ_INFO(ME, "%s: sync disable vap", pAP->alias);
            }
        } else if((pAP->enable) &&
                  (pAP->pFA->mfn_wvap_status(pAP) == 0) && (detRadState == CM_RAD_UP)) {
            //need to restart broadcasting the enabled bss,
            //that were potentially stopped by hapd when disabling one AP
            SAH_TRACEZ_INFO(ME, "%s: sync enable vap", pAP->alias);
            pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, SET | DIRECT);
            wld_secDmn_action_rc_ne rc;
            /*
             * first, set dyn ena/disabling vap params, before applying any action
             */
            if((rc = wld_ap_hostapd_setEnableVap(pAP, pAP->enable)) < SECDMN_ACTION_OK_DONE) {
                SAH_TRACEZ_ERROR(ME, "%s: fail to save enable %d rc %d", pAP->alias, pAP->enable, rc);
            }

            if(!wld_ap_hostapd_updateBeacon(pAP, "syncAp")) {
                ret = SWL_RC_ERROR;
            }
            /*
             * As iface has been stopped, nl80211 level has lost the station list, but not hostapd.
             * So stations need to re-authenticate to be able to retrieve connection status and stats from nl80211.
             */
            wld_ap_hostapd_deauthAllStations(pAP);
        }
        wld_vap_updateState(pAP);
    }
    return ret;
}

uint32_t wifiGen_hapd_countGrpMembers(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, 0, ME, "NULL");
    wld_secDmn_t* pSecDmn = pRad->hostapd;
    ASSERTS_NOT_NULL(pSecDmn->dmnProcess, 0, ME, "NULL");
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_getMembersCount(wld_secDmn_getGrp(pSecDmn));
    }
    //default: one hostapd instance per radio
    return 1;
}

static char* s_getGlobHapdArgsCb(wld_secDmnGrp_t* pSecDmnGrp, void* userData _UNUSED, const wld_process_t* pProc _UNUSED) {
    char* args = NULL;
    ASSERT_NOT_NULL(pSecDmnGrp, args, ME, "NULL");
    char startArgs[256] = {0};
    //set default start args
    swl_str_copy(startArgs, sizeof(startArgs), HOSTAPD_ARGS_FORMAT);
    for(uint32_t i = 0; i < wld_secDmnGrp_getMembersCount(pSecDmnGrp); i++) {
        const wld_secDmn_t* pSecDmn = wld_secDmnGrp_getMemberByPos(pSecDmnGrp, i);
        if((pSecDmn == NULL) || (swl_str_isEmpty(pSecDmn->cfgFile))) {
            continue;
        }
        //concat all radio ifaces conf files
        swl_strlst_cat(startArgs, sizeof(startArgs), " ", pSecDmn->cfgFile);
    }
    swl_str_copyMalloc(&args, startArgs);
    return args;
}

static bool s_isHapdIfaceStartable(wld_secDmnGrp_t* pSecDmnGrp _UNUSED, void* userData _UNUSED, wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pSecDmn->userData;
    ASSERT_TRUE(debugIsRadPointer(pRad), false, ME, "INVALID");
    wifiGen_hapd_restoreMainIface(pRad);
    return wifiGen_hapd_isStartable(pRad);
}

static wld_secDmnGrp_EvtHandlers_t sGHapdEvtCbs = {
    .getArgsCb = s_getGlobHapdArgsCb,
    .isMemberStartableCb = s_isHapdIfaceStartable,
};

static swl_rc_ne s_initGlobalHapdGrp(vendor_t* pVdr, bool forceGlob) {
    swl_rc_ne rc = SWL_RC_ERROR;
    ASSERTS_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* gHapd = pVdr->globalHostapd;
    ASSERT_NOT_NULL(gHapd, SWL_RC_INVALID_PARAM, ME, "No glob hapd ctx");
    if((gHapd->globalDmnSupported == SWL_TRL_TRUE) || forceGlob) {
        if(gHapd->pGlobalDmnGrp == NULL) {
            SAH_TRACEZ_INFO(ME, "%s: initialize glob hostapd", pVdr->name);
            char grpName[128] = {0};
            swl_str_catFormat(grpName, sizeof(grpName), "%s-%s", HOSTAPD_CMD, pVdr->name);
            rc = wld_secDmnGrp_init(&gHapd->pGlobalDmnGrp, HOSTAPD_CMD, HOSTAPD_ARGS_FORMAT, grpName);
            wld_secDmnGrp_setEvtHandlers(gHapd->pGlobalDmnGrp, &sGHapdEvtCbs, pVdr);
        }
    } else if(gHapd->pGlobalDmnGrp != NULL) {
        SAH_TRACEZ_INFO(ME, "%s: cleanup glob hostapd", pVdr->name);
        rc = wld_secDmnGrp_cleanup(&gHapd->pGlobalDmnGrp);
    }
    return rc;
}

swl_rc_ne wifiGen_hapd_setGlobDmnSettings(vendor_t* pVdr, wld_dmnMgt_dmnExecSettings_t* pCfg) {
    ASSERT_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* gHapd = pVdr->globalHostapd;
    ASSERT_NOT_NULL(gHapd, SWL_RC_INVALID_PARAM, ME, "No glob hapd ctx");
    bool forceGlob = (pCfg->useGlobalInstance == SWL_TRL_TRUE);
    s_initGlobalHapdGrp(pVdr, forceGlob);
    if(!forceGlob) {
        ASSERTS_EQUALS(gHapd->globalDmnSupported, SWL_TRL_TRUE, SWL_RC_OK, ME, "glob hapd not supported on vdr %s", pVdr->name);
    }
    bool enableGlobHapd = ((pCfg->useGlobalInstance == SWL_TRL_TRUE) ||
                           ((pCfg->useGlobalInstance == SWL_TRL_AUTO) && gHapd->globalDmnRequired));
    if(wld_secDmnGrp_isEnabled(gHapd->pGlobalDmnGrp) != enableGlobHapd) {
        if(!enableGlobHapd) {
            SAH_TRACEZ_INFO(ME, "drop all members of gHapd %s", pVdr->name);
            wld_secDmnGrp_dropMembers(gHapd->pGlobalDmnGrp);
        }
        T_Radio* pRad;
        wld_for_eachRad(pRad) {
            if(pRad->vendor == pVdr) {
                if(enableGlobHapd) {
                    SAH_TRACEZ_INFO(ME, "add member %s to gHapd %s", pRad->Name, pVdr->name);
                    wld_secDmn_addToGrp(pRad->hostapd, gHapd->pGlobalDmnGrp, pRad->Name);
                }
                setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
                wld_rad_doCommitIfUnblocked(pRad);
            }
        }
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_initGlobDmnCap(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->vendor, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_initDmnExecInfo(&pRad->vendor->globalHostapd);
    wld_dmnMgt_dmnExecInfo_t* gHapd = pRad->vendor->globalHostapd;
    if(gHapd->globalDmnSupported == SWL_TRL_FALSE) {
        SAH_TRACEZ_INFO(ME, "%s: glob hapd %s not supported", pRad->Name, pRad->vendor->name);
        gHapd->globalDmnRequired = false;
        return SWL_RC_OK;
    }
    //global hostapd required for WiFi7 and WiFi6E (For RNR Mgmt)
    if((SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_BE)) ||
       ((SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX)) &&
        (SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_6GHZ)))) {
        gHapd->globalDmnRequired = true;
    }
    //global hostapd assumed supported since WiFi6
    if(gHapd->globalDmnSupported == SWL_TRL_UNKNOWN) {
        if((gHapd->globalDmnRequired) ||
           (SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX))) {
            gHapd->globalDmnSupported = SWL_TRL_TRUE;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: glob hapd %s supp:%d, req:%d", pRad->Name, pRad->vendor->name, gHapd->globalDmnSupported, gHapd->globalDmnRequired);
    return SWL_RC_OK;
}
