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
#include "wifiGen_events.h"
#include "wld/wld_wpaCtrlMngr.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_util.h"

#define ME "genEvt"

static swl_rc_ne s_setWpaCtrlRadEvtHandlers(wld_wpaCtrlMngr_t* wpaCtrlMngr, T_Radio* pRad) {
    ASSERT_NOT_NULL(wpaCtrlMngr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrl_evtHandlers_cb wpaCtrlRadEvthandlers;
    memset(&wpaCtrlRadEvthandlers, 0, sizeof(wpaCtrlRadEvthandlers));
    //Set here the wpa_ctrl RAD event handlers
    wld_wpaCtrlMngr_setEvtHandlers(wpaCtrlMngr, pRad, &wpaCtrlRadEvthandlers);
    return SWL_RC_OK;
}

static void s_newInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
}

static void s_delInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
}

swl_rc_ne wifiGen_setRadEvtHandlers(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pRad->hostapd->wpaCtrlMngr, pRad);

    wld_nl80211_evtHandlers_cb nl80211RadEvtHandlers;
    memset(&nl80211RadEvtHandlers, 0, sizeof(nl80211RadEvtHandlers));
    //Set here the nl80211 RAD event handlers
    nl80211RadEvtHandlers.fNewInterfaceCb = s_newInterfaceCb;
    nl80211RadEvtHandlers.fDelInterfaceCb = s_delInterfaceCb;
    wld_rad_nl80211_setEvtListener(pRad, NULL, &nl80211RadEvtHandlers);

    return SWL_RC_OK;
}

static void s_cancelWps(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
}

static void s_timeoutWps(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL);
}

static void s_successWps(void* userData, char* ifName _UNUSED, swl_macChar_t* mac) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, mac->cMac);
}

static void s_stationConnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: connecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_station_move_to_end_of_list(pAP, macAddress);
    if(pAD == NULL) {
        pAD = wld_create_associatedDevice(pAP, macAddress);
        ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to create associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));
        SAH_TRACEZ_INFO(ME, "%s: created device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));
    }

    wld_ad_add_connection_try(pAP, pAD);
    wld_ad_add_connection_success(pAP, pAD);
}

static void s_stationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: disconnecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_station_move_to_end_of_list(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    wld_ad_add_disconnection(pAP, pAD);
    // clean up stations who failed authentication
    wld_vap_cleanup_stationlist(pAP);
}

static void s_btmReplyEvt(void* userData, char* ifName _UNUSED, swl_macChar_t* mac, uint8_t replyCode) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_bss_done(pAP, (const unsigned char*) mac->cMac, (int) replyCode);
}

swl_rc_ne wifiGen_setVapEvtHandlers(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    wld_wpaCtrl_evtHandlers_cb wpaCtrlVapEvtHandlers;
    memset(&wpaCtrlVapEvtHandlers, 0, sizeof(wpaCtrlVapEvtHandlers));

    //Set here the wpa_ctrl VAP event handlers
    wpaCtrlVapEvtHandlers.fWpsCancelMsg = s_cancelWps;
    wpaCtrlVapEvtHandlers.fWpsTimeoutMsg = s_timeoutWps;
    wpaCtrlVapEvtHandlers.fWpsSuccessMsg = s_successWps;
    wpaCtrlVapEvtHandlers.fStationConnectedCb = s_stationConnectedEvt;
    wpaCtrlVapEvtHandlers.fStationDisconnectedCb = s_stationDisconnectedEvt;
    wpaCtrlVapEvtHandlers.fBtmReplyCb = s_btmReplyEvt;

    ASSERT_TRUE(wld_wpaCtrlInterface_setEvtHandlers(pAP->wpaCtrlInterface, pAP, &wpaCtrlVapEvtHandlers),
                SWL_RC_ERROR, ME, "%s: fail to set interface wpa evt handlers", pAP->alias);

    wld_nl80211_evtHandlers_cb nl80211VapEvtHandlers;
    memset(&nl80211VapEvtHandlers, 0, sizeof(nl80211VapEvtHandlers));

    //Set here the nl80211 VAP event handlers
    wld_ap_nl80211_setEvtListener(pAP, NULL, &nl80211VapEvtHandlers);

    return SWL_RC_OK;
}

swl_rc_ne wifiGen_setEpEvtHandlers(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pEP->wpaSupp, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pEP->wpaSupp->wpaCtrlMngr, pEP->pRadio);

    wld_wpaCtrl_evtHandlers_cb wpaCtrlEpEvtHandlers;
    memset(&wpaCtrlEpEvtHandlers, 0, sizeof(wpaCtrlEpEvtHandlers));
    //Set here the wpa_ctrl EP event handlers
    wld_wpaCtrlInterface_setEvtHandlers(pEP->wpaCtrlInterface, pEP, &wpaCtrlEpEvtHandlers);

    return SWL_RC_OK;
}

