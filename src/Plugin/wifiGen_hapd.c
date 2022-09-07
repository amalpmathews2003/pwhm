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
#include "wld/wld_radio.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_wpaCtrl_events.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wifiGen_hapd.h"

#define ME "genHapd"
#define HOSTAPD_CONF_FILE_PATH_FORMAT "/tmp/%s_hapd.conf"
#define HOSTAPD_CMD "hostapd"
#define HOSTAPD_ARGS_FORMAT "-ddt %s"

static void s_stopHapdCb(wld_secDmn_t* pHapdInst _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    const char* mainIface = wld_rad_getFirstVap(pRad)->alias;
    SAH_TRACEZ_WARNING(ME, "%s: hostapd stopped", mainIface);
    wld_deamonExitInfo_t* pExitInfo = &pHapdInst->dmnProcess->lastExitInfo;
    if(pExitInfo && pExitInfo->isExited && (pExitInfo->exitStatus == 1)) {
        SAH_TRACEZ_ERROR(ME, "%s: invalid hostapd configuration", mainIface);
    }
    wld_rad_updateState(pRad, true);
    //finalize hapd cleanup
    wifiGen_hapd_stopDaemon(pRad);
}

swl_rc_ne wifiGen_hapd_init(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    char confFilePath[128] = {0};
    swl_str_catFormat(confFilePath, sizeof(confFilePath), HOSTAPD_CONF_FILE_PATH_FORMAT, pRad->Name);
    char startArgs[128] = {0};
    swl_str_catFormat(startArgs, sizeof(startArgs), HOSTAPD_ARGS_FORMAT, confFilePath);
    swl_rc_ne rc = wld_secDmn_init(&pRad->hostapd, HOSTAPD_CMD, startArgs, confFilePath, HOSTAPD_CTRL_IFACE_DIR);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to init hostapd", pRad->Name);
    wld_secDmnEvtHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
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

swl_rc_ne wifiGen_hapd_startDaemon(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Start hostapd", pRad->Name);
    return wld_secDmn_start(pRad->hostapd);
}

swl_rc_ne wifiGen_hapd_stopDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Stop hostapd", pRad->Name);
    swl_rc_ne rc = wld_secDmn_stop(pRad->hostapd);
    ASSERTI_FALSE(rc < SWL_RC_OK, rc, ME, "%s: hostapd not running", pRad->Name);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->index > 0) {
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, 0);
        }
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_reloadDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s : Reload Hostapd", pRad->Name);
    return wld_secDmn_reload(pRad->hostapd);
}

void wifiGen_hapd_writeConfig(T_Radio* pRad) {
    wld_hostapd_cfgFile_createExt(pRad);
}

wld_wpaCtrlInterface_t* s_mainInterface(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, NULL, ME, "NULL");
    return wld_wpaCtrlMngr_getInterface(pRad->hostapd->wpaCtrlMngr, 0);
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
swl_rc_ne wifiGen_hapd_updateRadState(T_Radio* pRad) {
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
    pRad->detailedState = *pRadDetState;
    SAH_TRACEZ_INFO(ME, "%s: hapd state(%s) -> radDetState(%d)", pRad->Name, state, pRad->detailedState);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_syncVapStates(T_Radio* pRad) {
    swl_rc_ne ret = SWL_RC_OK;
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(!wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface) || (pAP->index <= 0)) {
            continue;
        }
        if((!pAP->enable) &&
           (wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pAP->alias) > 0)) {
            /*
             * we need to disable passive bss net ifaces (except the radio interface),
             * that were implicitly enabled by hostapd startup
             * (although not broadcasting)
             */
            if(pAP->ref_index > 0) {
                SAH_TRACEZ_INFO(ME, "%s: sync disable vap", pAP->alias);
                wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, false);
            }
        } else if((pAP->enable) &&
                  (pAP->pFA->mfn_wvap_status(pAP) == 0) && (wld_rad_isUpAndReady(pRad))) {
            //need to restart broadcasting the enabled bss,
            //that were potentially stopped by hapd when disabling one AP
            SAH_TRACEZ_INFO(ME, "%s: sync enable vap", pAP->alias);
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, true);
            if(!wld_ap_hostapd_updateBeacon(pAP, "syncAp")) {
                ret = SWL_RC_ERROR;
            }
        }
    }
    return ret;
}
