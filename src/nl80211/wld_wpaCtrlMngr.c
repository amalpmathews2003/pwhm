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
#include <stdlib.h>
#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "wld_wpaCtrlMngr_priv.h"
#include "wld_wpaCtrlInterface_priv.h"
#include "wld_ssid.h"
#include "wld_nl80211_api.h"
#include "wld_linuxIfUtils.h"
#include <dirent.h>

#define ME "wpaCtrl"

static T_SSID* s_fetchLinkSSIDWithNl80211(const char* mldIface, int32_t linkId) {
    int ifIndex = -1;
    int ret = wld_linuxIfUtils_getIfIndexExt((char*) mldIface, &ifIndex);
    ASSERTS_TRUE(ret >= 0, NULL, ME, "fail to get iface %s index", mldIface);
    wld_nl80211_ifaceInfo_t ifaceInfo;
    memset(&ifaceInfo, 0, sizeof(ifaceInfo));
    ret = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &ifaceInfo);
    ASSERTS_TRUE(swl_rc_isOk(ret), NULL, "fail to get nl iface %s info", mldIface);
    swl_macBin_t* pMac = NULL;
    if((ifaceInfo.nMloLinks > 0) && (linkId >= 0)) {
        wld_nl80211_ifaceMloLinkInfo_t* pLinkInfo =
            (wld_nl80211_ifaceMloLinkInfo_t* ) wld_nl80211_fetchIfaceMloLinkById(&ifaceInfo, linkId);
        ASSERTI_NOT_NULL(pLinkInfo, NULL, ME, "mld iface(%s) linkId(%d) is not found", mldIface, linkId);
        pMac = &pLinkInfo->link.linkMac;
    } else {
        pMac = &ifaceInfo.mac;
    }
    ASSERTS_FALSE(swl_mac_binIsNull(pMac), NULL, ME, "iface(%s) linkId(%d) has null mac", mldIface, linkId);
    T_SSID* pSSID = wld_ssid_getSsidByMacAddress(pMac);
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "iface(%s) linkId(%d) has no ssid ctx", mldIface, linkId);
    SAH_TRACEZ_INFO(ME, "fetch with NL80211: iface(%s) linkId(%d) => linkMac(%s) => pSSID(%s)",
                    mldIface, linkId,
                    swl_typeMacBin_toBuf32Ref(pMac).buf, pSSID->Name);
    return pSSID;
}

static wld_wpaCtrlInterface_t** s_getWpaCtrlIfaceRef(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        return &pSSID->AP_HOOK->wpaCtrlInterface;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return &pSSID->ENDP_HOOK->wpaCtrlInterface;
    }
    return NULL;
}

static wld_wpaCtrlInterface_t* s_getWpaCtrlIface(T_SSID* pSSID) {
    wld_wpaCtrlInterface_t** ppIface = s_getWpaCtrlIfaceRef(pSSID);
    if(ppIface != NULL) {
        return *ppIface;
    }
    return NULL;
}

static T_SSID* s_fetchLinkSSIDWithWpaCtrl(const char* mldIface, int32_t linkId, const char* sockName) {
    T_SSID* pMldSSID = wld_ssid_getSsidByIfName(mldIface);
    wld_wpaCtrlInterface_t* pMldWpaIface = s_getWpaCtrlIface(pMldSSID);
    char* linkIface = NULL;
    CALL_INTF(pMldWpaIface, fFetchLinkIfaceCb, linkId, sockName, &linkIface);
    ASSERTS_NOT_NULL(linkIface, NULL, ME, "iface(%s) linkId(%d) does not point any link", mldIface, linkId);
    T_SSID* pSSID = wld_ssid_getSsidByIfName(linkIface);
    W_SWL_FREE(linkIface);
    ASSERTI_NOT_NULL(pSSID, NULL, ME, "iface(%s) linkId(%d) has no ssid", mldIface, linkId);
    SAH_TRACEZ_INFO(ME, "fetch with CUSTOM wpa: iface(%s) linkId(%d) sock(%s) => pSSID(%s)",
                    mldIface, linkId, sockName, pSSID->Name);
    return pSSID;
}

static T_SSID* s_fetchLinkSSID(const char* sockName, const char* mldIface, int32_t linkId) {
    ASSERTS_STR(sockName, NULL, ME, "empty socket name");
    T_SSID* pSSID = wld_ssid_getSsidByIfName(mldIface);
    ASSERT_NOT_NULL(pSSID, NULL, ME, "no ssid is matching iface(%s) linkId(%d) of sock(%s)",
                    mldIface, linkId, sockName);
    if(linkId < 0) {
        ASSERTI_FALSE(wld_mld_isLinkEnabled(pSSID->pMldLink), NULL,
                      ME, "skip sock(%s): wait for mld link socket for ssid %s", sockName, pSSID->Name);
        SAH_TRACEZ_INFO(ME, "fetch DIRECT: iface(%s) linkId(%d) sock(%s) => pSSID(%s)",
                        mldIface, linkId, sockName, pSSID->Name);
        return pSSID;
    }
    ASSERT_TRUE(pSSID->mldUnit >= 0, NULL, ME, "iface(%s) linkId(%d) sock(%s) => ssid(%s) is not mld member",
                mldIface, linkId, sockName, pSSID->Name);
    if(((pSSID = s_fetchLinkSSIDWithNl80211(mldIface, linkId)) != NULL) ||
       ((pSSID = s_fetchLinkSSIDWithWpaCtrl(mldIface, linkId, sockName)) != NULL)) {
        return pSSID;
    }
    SAH_TRACEZ_WARNING(ME, "sock(%s): Fail to match any SSID", sockName);
    return NULL;
}

static int s_filterNames(const struct dirent* pEntry) {
    const char* fname = pEntry->d_name;
    if(swl_str_startsWith(fname, ".") || swl_str_startsWith(fname, "..")) {
        return 0;
    }
    return 1;
}

swl_rc_ne wld_wpaCtrlMngr_checkAllIfaces(wld_wpaCtrlMngr_t* pMgr) {
    ASSERTS_NOT_NULL(pMgr, SWL_RC_INVALID_PARAM, ME, "NULL");
    const char* ctrlDirPath = wld_secDmn_getCtrlIfaceDirPath(pMgr->pSecDmn);
    ASSERT_STR(ctrlDirPath, SWL_RC_INVALID_STATE, ME, "no ctrl iface dir");
    struct dirent** namelist;
    int n = scandir(ctrlDirPath, &namelist, s_filterNames, alphasort);
    ASSERT_NOT_EQUALS(n, -1, SWL_RC_ERROR, ME, "fail to scan dir %s", ctrlDirPath);
    for(int i = 0; i < n; i++) {
        const char* sockName = namelist[i]->d_name;
        char mldIface[swl_str_len(sockName) + 1];
        int32_t linkId = -1;
        if(!wld_wpaCtrlInterface_parseSockName(sockName, mldIface, sizeof(mldIface), &linkId)) {
            SAH_TRACEZ_ERROR(ME, "fail to parse sockName (%s)", sockName);
            free(namelist[i]);
            continue;
        }
        T_SSID* pSSID = s_fetchLinkSSID(sockName, mldIface, linkId);
        wld_wpaCtrlInterface_t* pIface = s_getWpaCtrlIface(pSSID);
        wld_wpaCtrlMngr_t* pCurrMgr = wld_wpaCtrlInterface_getMgr(pIface);
        if((pCurrMgr != NULL) && (wld_secDmn_isRunning(pCurrMgr->pSecDmn))) {
            if(!swl_str_matches(wld_wpaCtrlInterface_getConnectionSockName(pIface), sockName)) {
                wld_wpaCtrlInterface_setConnectionInfo(pIface, ctrlDirPath, sockName);
            }
            if(swl_str_matches(wld_ssid_getIfName(pSSID), mldIface)) {
                wld_mld_setPrimaryLink(pSSID->pMldLink);
            }
            wld_mld_setLinkId(pSSID->pMldLink, linkId);
            if((pCurrMgr == pMgr) && (!wld_wpaCtrlInterface_isReady(pIface))) {
                wld_wpaCtrlInterface_setEnable(pIface, true);
                wld_wpaCtrlInterface_open(pIface);
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    return SWL_RC_OK;
}

swl_rc_ne s_checkMgrConnectedIfaces(wld_wpaCtrlMngr_t* pMgr) {
    uint32_t nIfacesReady = wld_wpaCtrlMngr_countReadyInterfaces(pMgr);
    uint32_t nExpecIfaces = wld_wpaCtrlMngr_countEnabledInterfaces(pMgr);
    if((!nIfacesReady) || (nIfacesReady < nExpecIfaces)) {
        return SWL_RC_CONTINUE;
    }
    amxp_timer_state_t timerState = amxp_timer_get_state(pMgr->connectTimer);
    if((timerState != amxp_timer_running) && (timerState != amxp_timer_started)) {
        return SWL_RC_OK;
    }
    pMgr->wpaCtrlConnectAttempts = 0;
    amxp_timer_stop(pMgr->connectTimer);
    if(nExpecIfaces > 0) {
        wld_wpaCtrlInterface_t* pFstIface = wld_wpaCtrlMngr_getFirstInterface(pMgr);
        wld_wpaCtrlInterface_t* pReadyIface = wld_wpaCtrlMngr_getFirstReadyInterface(pMgr);
        char* srvName = pReadyIface ? pReadyIface->name : pFstIface ? pFstIface->name : "";
        SAH_TRACEZ_INFO(ME, "%s: wpa_ctrl server is ready (%d/%d connected)", srvName, nIfacesReady, nExpecIfaces);
        CALL_MGR(pMgr, srvName, fMngrReadyCb, true);
    }
    return SWL_RC_DONE;
}

/**
 * @brief Try to connect to all detected wpa_ctrl interfaces
 *
 * @param timer pointer to timer context
 * @param userdata pointer to wpa_ctrl manager context
 *
 * @return void
 */
static void s_reconnectMgrTimer(amxp_timer_t* timer, void* userdata) {
    ASSERTS_NOT_NULL(timer, , ME, "NULL");
    ASSERTS_NOT_NULL(userdata, , ME, "NULL");
    wld_wpaCtrlMngr_t* pMgr = (wld_wpaCtrlMngr_t*) userdata;
    swl_rc_ne rc = wld_wpaCtrlMngr_checkAllIfaces(pMgr);
    ASSERT_TRUE(swl_rc_isOk(rc), , ME, "fail to check available wpactrl ifaces");
    wld_wpaCtrlInterface_t* pIfaceNotReady = wld_wpaCtrlMngr_getFirstNotReadyInterface(pMgr);
    const char* srvName = pIfaceNotReady ? wld_wpaCtrlInterface_getName(pIfaceNotReady) : "";
    if(!wld_secDmn_isRunning(pMgr->pSecDmn)) {
        SAH_TRACEZ_ERROR(ME, "%s: daemon not started yet, no need to connect", srvName);
        //no need to retry, as long as sec daemon is not running
        pMgr->wpaCtrlConnectAttempts += MAX_CONNECTION_ATTEMPTS;
    } else if(s_checkMgrConnectedIfaces(pMgr) == SWL_RC_CONTINUE) {
        pMgr->wpaCtrlConnectAttempts++;
    }
    if(pMgr->wpaCtrlConnectAttempts > 0) {
        uint32_t nIfacesReady = wld_wpaCtrlMngr_countReadyInterfaces(pMgr);
        uint32_t nExpecIfaces = wld_wpaCtrlMngr_countEnabledInterfaces(pMgr);
        if((nExpecIfaces > 0) && (pMgr->wpaCtrlConnectAttempts < MAX_CONNECTION_ATTEMPTS)) {
            SAH_TRACEZ_WARNING(ME, "%s: wpa_ctrl server not yet ready (%d/%d), waiting (%d/%d)..",
                               srvName, nIfacesReady, nExpecIfaces,
                               pMgr->wpaCtrlConnectAttempts, MAX_CONNECTION_ATTEMPTS);
        } else {
            pMgr->wpaCtrlConnectAttempts = 0;
            amxp_timer_stop(pMgr->connectTimer);
            SAH_TRACEZ_WARNING(ME, "%s: stop connecting to wpa_ctrl server", srvName);
        }
    }
}

/**
 * @brief initialize wpa_ctrl manager
 * If already allocated, then it will be reset
 *
 * @param ppMgr pointer wpa_ctrl manager to be allocated/initialized
 * @param pSecDmn pointer to target security deamon (server)
 *
 * @return true when wpa_ctrl manager is successfully initialized. Otherwise, false.
 */
bool wld_wpaCtrlMngr_init(wld_wpaCtrlMngr_t** ppMgr, struct wld_secDmn* pSecDmn) {
    ASSERT_NOT_NULL(ppMgr, false, ME, "NULL");
    wld_wpaCtrlMngr_t* pMgr = *ppMgr;

    if(pMgr == NULL) {
        pMgr = calloc(1, sizeof(wld_wpaCtrlMngr_t));
        ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
        swl_unLiList_init(&pMgr->ifaces, sizeof(wld_wpaCtrlInterface_t*));
        amxp_timer_new(&pMgr->connectTimer, s_reconnectMgrTimer, pMgr);
        amxp_timer_set_interval(pMgr->connectTimer, RETRY_DELAY_MS);
        pMgr->pSecDmn = pSecDmn;
        *ppMgr = pMgr;
    } else {
        wld_wpaCtrlMngr_disconnect(pMgr);
        uint32_t nIfaces = swl_unLiList_size(&pMgr->ifaces);
        for(int32_t i = nIfaces - 1; i >= 0; i--) {
            swl_unLiList_remove(&pMgr->ifaces, i);
        }
    }

    return true;
}

bool wld_wpaCtrlMngr_setEvtHandlers(wld_wpaCtrlMngr_t* pMgr, void* userdata, wld_wpaCtrl_radioEvtHandlers_cb* pHandlers) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    pMgr->userData = userdata;
    if(pHandlers) {
        pMgr->handlers = *pHandlers;
    } else {
        memset(&pMgr->handlers, 0, sizeof(pMgr->handlers));
    }

    return true;
}

bool wld_wpaCtrlMngr_getEvtHandlers(wld_wpaCtrlMngr_t* pMgr, void** userdata, wld_wpaCtrl_radioEvtHandlers_cb* pHandlers) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    if(userdata != NULL) {
        *userdata = pMgr->userData;
    }
    if(pHandlers != NULL) {
        *pHandlers = pMgr->handlers;
    }
    return true;
}

/**
 * @brief register wpa_ctrl interface to wpa_ctrl manager
 *
 * @param pMgr pointer to wpa ctrl manager
 * @param pIface pointer to wpa ctrl interface
 *
 * @return true when adding is successful
 *         false, otherwise.
 */
bool wld_wpaCtrlMngr_registerInterface(wld_wpaCtrlMngr_t* pMgr, wld_wpaCtrlInterface_t* pIface) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    ASSERT_NOT_NULL(pIface, false, ME, "NULL");

    if(!swl_unLiList_contains(&pMgr->ifaces, &pIface)) {
        swl_unLiList_add(&pMgr->ifaces, &pIface);
        pIface->pMgr = pMgr;
    }
    return true;
}

/**
 * @brief unregister wpa_ctrl interface from wpa_ctrl manager
 *
 * @param pMgr pointer to wpa ctrl manager
 * @param pIface pointer to wpa ctrl interface
 *
 * @return true when removal is successful
 *         false, otherwise.
 */
bool wld_wpaCtrlMngr_unregisterInterface(wld_wpaCtrlMngr_t* pMgr, wld_wpaCtrlInterface_t* pIface) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    if(swl_unLiList_removeByData(&pMgr->ifaces, &pIface) != -1) {
        pIface->pMgr = NULL;
        return true;
    }
    return false;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getInterface(const wld_wpaCtrlMngr_t* pMgr, int32_t pos) {
    ASSERT_NOT_NULL(pMgr, NULL, ME, "NULL");
    wld_wpaCtrlInterface_t** ppIface = (wld_wpaCtrlInterface_t**) swl_unLiList_get(&pMgr->ifaces, pos);
    ASSERT_NOT_NULL(ppIface, NULL, ME, "Not found");
    return *ppIface;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getInterfaceByName(const wld_wpaCtrlMngr_t* pMgr, const char* name) {
    ASSERTS_STR(name, NULL, ME, "NULL");
    ASSERT_NOT_NULL(pMgr, NULL, ME, "NULL");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if(swl_str_matches(wld_wpaCtrlInterface_getName(pIface), name)) {
            return pIface;
        }
    }
    return NULL;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstReadyInterface(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, NULL, ME, "NULL");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if(wld_wpaCtrlInterface_isReady(pIface)) {
            return pIface;
        }
    }
    return NULL;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstNotReadyInterface(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, NULL, ME, "NULL");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if((wld_wpaCtrlInterface_isEnabled(pIface)) && (!wld_wpaCtrlInterface_isReady(pIface))) {
            return pIface;
        }
    }
    return NULL;
}

uint32_t wld_wpaCtrlMngr_countInterfaces(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERTS_NOT_NULL(pMgr, 0, ME, "NULL");
    return swl_unLiList_size(&pMgr->ifaces);
}

uint32_t wld_wpaCtrlMngr_countEnabledInterfaces(const wld_wpaCtrlMngr_t* pMgr) {
    uint32_t count = 0;
    ASSERT_NOT_NULL(pMgr, count, ME, "NULL");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if(wld_wpaCtrlInterface_isEnabled(pIface)) {
            count++;
        }
    }
    return count;
}

uint32_t wld_wpaCtrlMngr_countReadyInterfaces(const wld_wpaCtrlMngr_t* pMgr) {
    uint32_t count = 0;
    ASSERT_NOT_NULL(pMgr, count, ME, "NULL");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if(wld_wpaCtrlInterface_isReady(pIface)) {
            count++;
        }
    }
    return count;
}

wld_secDmn_t* wld_wpaCtrlMngr_getSecDmn(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERTS_NOT_NULL(pMgr, NULL, ME, "NULL");
    return pMgr->pSecDmn;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstInterface(const wld_wpaCtrlMngr_t* pMgr) {
    return wld_wpaCtrlMngr_getInterface(pMgr, 0);
}

bool wld_wpaCtrlMngr_ping(const wld_wpaCtrlMngr_t* pMgr) {
    return wld_wpaCtrlInterface_ping(wld_wpaCtrlMngr_getFirstReadyInterface(pMgr));
}

/**
 * @brief check that all registed wpa_ctrl interfaces are ready
 *
 * @param pMgr pointer to wpa ctrl manager
 *
 * @return true when all registed wpa_ctrl interfaces are ready,
 *         false, otherwise.
 */
bool wld_wpaCtrlMngr_isReady(wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    ASSERTI_NOT_EQUALS(swl_unLiList_size(&pMgr->ifaces), 0, false, ME, "Empty");
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        ASSERT_NOT_NULL(pIface, false, ME, "NULL");
        ASSERTS_TRUE(pIface->isReady, false, ME, "Not ready");
    }
    return true;
}

/**
 * @brief check that wpaCtrl manager is connected:
 * i.e can receive events from its main interface.
 *
 * @param pMgr pointer to wpa ctrl manager
 *
 * @return true when relative secDmn is running and main interface is connected,
 *         false, otherwise.
 */
bool wld_wpaCtrlMngr_isConnected(wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    ASSERTS_TRUE(wld_secDmn_isRunning(pMgr->pSecDmn), false, ME, "No running server");
    wld_wpaCtrlInterface_t* ifaceReady = wld_wpaCtrlMngr_getFirstReadyInterface(pMgr);
    ASSERTS_NOT_NULL(ifaceReady, false, ME, "no iface Ready");
    return true;
}

/**
 * @brief start connecting to wpa_ctrl server
 *
 * @param pMgr pointer to wpa ctrl manager
 *
 * @return true if connection is started or already established,
 *         false otherwise.
 */
bool wld_wpaCtrlMngr_connect(wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    ASSERTI_FALSE(wld_wpaCtrlMngr_isConnected(pMgr), true, ME, "already connected");
    amxp_timer_start(pMgr->connectTimer, FIRST_DELAY_MS);
    return true;
}

/**
 * @brief disconnect from wpa_ctrl server
 *
 * @param pMgr pointer to wpa ctrl manager
 *
 * @return true when all relative wpa_ctrl interfaces are closed,
 *         false otherwise.
 */
bool wld_wpaCtrlMngr_disconnect(wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, false, ME, "NULL");
    pMgr->wpaCtrlConnectAttempts = 0;
    amxp_timer_stop(pMgr->connectTimer);
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        wld_wpaCtrlInterface_t* pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        wld_wpaCtrlInterface_close(pIface);
    }
    return true;
}

void wld_wpaCtrlMngr_cleanup(wld_wpaCtrlMngr_t** ppMgr) {
    ASSERTS_NOT_NULL(ppMgr, , ME, "NULL");
    wld_wpaCtrlMngr_t* pMgr = *ppMgr;
    ASSERTS_NOT_NULL(pMgr, , ME, "NULL");
    uint32_t nIfaces = swl_unLiList_size(&pMgr->ifaces);
    for(int32_t i = nIfaces - 1; i >= 0; i--) {
        wld_wpaCtrlInterface_t* pIface = *((wld_wpaCtrlInterface_t**) swl_unLiList_get(&pMgr->ifaces, i));
        if(pIface) {
            wld_wpaCtrlInterface_close(pIface);
            pIface->pMgr = NULL;
        }
        swl_unLiList_remove(&pMgr->ifaces, i);
    }
    amxp_timer_delete(&pMgr->connectTimer);
    free(pMgr);
    *ppMgr = NULL;
}

