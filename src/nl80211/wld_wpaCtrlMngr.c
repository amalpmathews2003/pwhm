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

#define ME "wpaCtrl"

/**
 * @brief Try to connect to all registered wpa_ctrl interfaces
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

    uint32_t nIfacesReady = 0;
    const char* srvName = "";
    swl_unLiListIt_t it;
    wld_wpaCtrlInterface_t* pIface = NULL;
    swl_unLiList_for_each(it, &pMgr->ifaces) {
        pIface = *(swl_unLiList_data(&it, wld_wpaCtrlInterface_t * *));
        if(!srvName[0]) {
            srvName = wld_wpaCtrlInterface_getName(pIface);
        }
        if((pMgr->pSecDmn != NULL) &&
           (!wld_secDmn_isRunning(pMgr->pSecDmn))) {
            SAH_TRACEZ_ERROR(ME, "%s: %s not started yet, no need to reconnect", srvName, pMgr->pSecDmn->dmnProcess->cmd);
            //no need to retry, as long as sec daemon is not running
            pMgr->wpaCtrlConnectAttempts += MAX_CONNECTION_ATTEMPTS;
            break;
        }
        if(!wld_wpaCtrlInterface_open(pIface)) {
            pMgr->wpaCtrlConnectAttempts++;
            break;
        } else {
            nIfacesReady++;
        }
    }

    uint32_t nExpecIfaces = swl_unLiList_size(&pMgr->ifaces);
    if((nIfacesReady > 0) && (nIfacesReady == nExpecIfaces)) {
        pIface = wld_wpaCtrlMngr_getFirstReadyInterface(pMgr);
        ASSERTS_NOT_NULL(pIface, , ME, "NULL");
        SAH_TRACEZ_INFO(ME, "%s: wpa_ctrl server is ready (%d/%d connected)", srvName, nIfacesReady, nExpecIfaces);
        pMgr->wpaCtrlConnectAttempts = 0;
        amxp_timer_stop(pMgr->connectTimer);
        CALL_MGR(pMgr, pIface->name, fMngrReadyCb, true);
    } else if(pMgr->wpaCtrlConnectAttempts < MAX_CONNECTION_ATTEMPTS) {
        SAH_TRACEZ_WARNING(ME, "%s: wpa_ctrl server not yet ready (%d/%d), waiting (%d/%d)..",
                           srvName, nIfacesReady, nExpecIfaces,
                           pMgr->wpaCtrlConnectAttempts, MAX_CONNECTION_ATTEMPTS);
    } else {
        pMgr->wpaCtrlConnectAttempts = 0;
        amxp_timer_stop(pMgr->connectTimer);
        SAH_TRACEZ_ERROR(ME, "%s: Fail to connect to wpa_ctrl server", srvName);
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

uint32_t wld_wpaCtrlMngr_countInterfaces(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERTS_NOT_NULL(pMgr, 0, ME, "NULL");
    return swl_unLiList_size(&pMgr->ifaces);
}

wld_secDmn_t* wld_wpaCtrlMngr_getSecDmn(const wld_wpaCtrlMngr_t* pMgr) {
    ASSERT_NOT_NULL(pMgr, NULL, ME, "NULL");
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

