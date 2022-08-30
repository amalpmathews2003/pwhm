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
#include "wifiGen_hapd.h"
#include "wifiGen_rad.h"
#include "wld/wld_wpaCtrlMngr.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_endpoint.h"
#include "swl/swl_hex.h"
#include "swl/swl_ieee802_1x_defs.h"

#include "wifiGen_staCapHandler.h"
#define ME "genEvt"

static void s_saveChanChanged(T_Radio* pRad, swl_chanspec_t* pChanSpec) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pChanSpec, , ME, "NULL");
    ASSERTS_TRUE(pChanSpec->channel > 0, , ME, "invalid chan");
    if((pChanSpec->channel != pRad->channel) ||
       (pChanSpec->bandwidth != pRad->runningChannelBandwidth)) {
        wld_rad_chan_notification(pRad, pChanSpec->channel, pChanSpec->bandwidth);
    }
    wld_rad_chan_update_model(pRad);
    wld_rad_updateOperatingClass(pRad);
}

static void s_syncCurrentChannel(T_Radio* pRad) {
    swl_chanspec_t chanSpec;
    ASSERTS_FALSE(wld_rad_nl80211_getChannel(pRad, &chanSpec) < SWL_RC_OK, ,
                  ME, "%s: fail to get channel", pRad->Name);
    s_saveChanChanged(pRad, &chanSpec);
}

static void s_chanSwitchCb(void* userData, char* ifName _UNUSED, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: channel switch event central_chan=%d bandwidth=%d band=%d", pRad->Name,
                       chanSpec->channel, chanSpec->bandwidth, chanSpec->band);
    s_saveChanChanged(pRad, chanSpec);
}

static void s_csaFinishedCb(void* userData, char* ifName _UNUSED, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: csa finished to new_chan=%d", pRad->Name, chanSpec->channel);
}

static void s_mngrReadyCb(void* userData, char* ifName, bool isReady) {
    SAH_TRACEZ_WARNING(ME, "%s: wpactrl mngr is %s ready", ifName, (isReady ? "" : "not"));
    ASSERTS_TRUE(isReady, , ME, "Not ready");
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    if(wld_rad_vap_from_name(pRad, ifName) != NULL) {
        //once we restart hostapd, previous dfs clearing must be reinitialized
        wifiGen_rad_initBands(pRad);
        s_syncCurrentChannel(pRad);
        //update rad status from hapd main iface state
        if((wifiGen_hapd_updateRadState(pRad) >= SWL_RC_OK) &&
           (pRad->detailedState == CM_RAD_UP)) {
            //we may missed the CAC Done event waiting to finalize wpactrl connection
            //so mark current chanspec as active
            wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
            wifiGen_hapd_syncVapStates(pRad);
        }
    }
    wld_rad_updateState(pRad, false);
}

static void s_mainApSetupCompletedCb(void* userData, char* ifName) {
    SAH_TRACEZ_WARNING(ME, "%s: main AP has setup completed", ifName);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pRad->detailedState = CM_RAD_UP;
    s_syncCurrentChannel(pRad);
    wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
    wifiGen_hapd_syncVapStates(pRad);
    wld_rad_updateState(pRad, true);
}

static void s_mainApDisabledCb(void* userData, char* ifName) {
    SAH_TRACEZ_WARNING(ME, "%s: main AP has been disabled", ifName);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    //once we toggle hostapd, previous dfs clearing must be reinitialized
    wld_rad_updateState(pRad, true);
    if(pRad->detailedState == CM_RAD_DOWN) {
        wifiGen_rad_initBands(pRad);
    }
}

static void s_dfsCacStartedCb(void* userData, char* ifName, swl_chanspec_t* chanSpec, uint32_t cac_time) {
    SAH_TRACEZ_WARNING(ME, "%s: CAC started on channel %d for %d sec", ifName, chanSpec->channel, cac_time);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    s_syncCurrentChannel(pRad);
    wld_channel_mark_passive_band(*chanSpec);
    if(wld_channel_is_chan_in_band(wld_rad_getSwlChanspec(pRad), chanSpec->channel)) {
        pRad->detailedState = CM_RAD_FG_CAC;
        wld_rad_updateState(pRad, false);
    }
}

static void s_dfsCacDoneCb(void* userData, char* ifName, swl_chanspec_t* chanSpec, bool success) {
    SAH_TRACEZ_WARNING(ME, "%s: CAC finished on channel %d : %s", ifName, chanSpec->channel,
                       (success ? "completed" : "aborted"));
    //CAC completed
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t currChanSpec = *chanSpec;
    currChanSpec.bandwidth = pRad->runningChannelBandwidth;
    if(success) {
        wld_channel_clear_passive_band(currChanSpec);
    } else if(wld_rad_isDoingDfsScan(pRad) && wld_channel_is_chan_in_band(currChanSpec, pRad->channel)) {
        // CAC aborted: so mark temporary radio as down, until fallback to alternative channel (eg: non-dfs)
        // Radio detailed status will then be rectified
        pRad->detailedState = CM_RAD_DOWN;
    }
    wld_rad_updateState(pRad, false);
}

static void s_dfsCacExpiredCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: Pre-CAC Expired on channel %d", ifName, chanSpec->channel);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_mark_passive_channel(*chanSpec);
    wld_rad_updateState(pRad, false);
}

static void s_dfsRadarDetectedCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Radar detected on channel %d bw %d", ifName, chanSpec->channel, chanSpec->bandwidth);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_mark_radar_detected_band(*chanSpec);
    wld_rad_updateState(pRad, false);
}

static void s_dfsNopFinishedCb(void* userData _UNUSED, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Non-Occupancy Period is over on channel %d", ifName, chanSpec->channel);
    wld_channel_clear_radar_detected_channel(*chanSpec);
    wld_channel_mark_passive_channel(*chanSpec);
}

static void s_dfsNewChannelCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Radar detection will trigger switching to channel %d", ifName, chanSpec->channel);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t currChanSpec = *chanSpec;
    currChanSpec.bandwidth = pRad->runningChannelBandwidth;
    pRad->channelChangeReason = CHAN_REASON_DFS;
    s_saveChanChanged(pRad, &currChanSpec);
}

static swl_rc_ne s_setWpaCtrlRadEvtHandlers(wld_wpaCtrlMngr_t* wpaCtrlMngr, T_Radio* pRad) {
    ASSERT_NOT_NULL(wpaCtrlMngr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrl_radioEvtHandlers_cb wpaCtrlRadEvthandlers;
    memset(&wpaCtrlRadEvthandlers, 0, sizeof(wpaCtrlRadEvthandlers));
    //Set here the wpa_ctrl RAD event handlers
    wpaCtrlRadEvthandlers.fChanSwitchStartedCb = s_chanSwitchCb;
    wpaCtrlRadEvthandlers.fChanSwitchCb = s_chanSwitchCb;
    wpaCtrlRadEvthandlers.fApCsaFinishedCb = s_csaFinishedCb;
    wpaCtrlRadEvthandlers.fMngrReadyCb = s_mngrReadyCb;
    wpaCtrlRadEvthandlers.fMainApSetupCompletedCb = s_mainApSetupCompletedCb;
    wpaCtrlRadEvthandlers.fMainApDisabledCb = s_mainApDisabledCb;
    wpaCtrlRadEvthandlers.fDfsCacStartedCb = s_dfsCacStartedCb;
    wpaCtrlRadEvthandlers.fDfsCacDoneCb = s_dfsCacDoneCb;
    wpaCtrlRadEvthandlers.fDfsCacExpiredCb = s_dfsCacExpiredCb;
    wpaCtrlRadEvthandlers.fDfsRadarDetectedCb = s_dfsRadarDetectedCb;
    wpaCtrlRadEvthandlers.fDfsNopFinishedCb = s_dfsNopFinishedCb;
    wpaCtrlRadEvthandlers.fDfsNewChannelCb = s_dfsNewChannelCb;
    wld_wpaCtrlMngr_setEvtHandlers(wpaCtrlMngr, pRad, &wpaCtrlRadEvthandlers);
    return SWL_RC_OK;
}

static void s_newInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
    if(pIfaceInfo->isAp) {
        T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
        if(pAP != NULL) {
            pAP->index = pIfaceInfo->ifIndex;
        }
    }
}

static void s_delInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
    if(pIfaceInfo->isAp) {
        T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
        if(pAP != NULL) {
            pAP->index = 0;
        }
    }
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

static void s_wpsCancel(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
}

static void s_wpsTimeout(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL);
}

static void s_wpsSuccess(void* userData, char* ifName _UNUSED, swl_macChar_t* mac) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, mac->cMac);
}

static void s_wpsOverlap(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_OVERLAP, NULL);
}

static void s_wpsFail(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_FAILURE, NULL);
}

static void s_apStationConnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: connecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    if(pAD == NULL) {
        pAD = wld_create_associatedDevice(pAP, macAddress);
        ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to create associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));
        SAH_TRACEZ_INFO(ME, "%s: created device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));
    }
    // initialize the associated device info at the connection time
    wld_nl80211_stationInfo_t stationInfo;
    if(wld_ap_nl80211_getStationInfo(pAP, (swl_macBin_t*) pAD->MACAddress, &stationInfo) >= SWL_RC_OK) {
        wld_ap_nl80211_copyStationInfoToAssocDev(pAP, pAD, &stationInfo);
    }

    wld_ad_add_connection_try(pAP, pAD);
    wld_ad_add_connection_success(pAP, pAD);
    wld_vap_sync_assoclist(pAP);
}

static void s_apStationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: disconnecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    wld_ad_add_disconnection(pAP, pAD);
    // clean up stations who failed authentication
    wld_vap_cleanup_stationlist(pAP);
    wld_vap_sync_assoclist(pAP);
}

static void s_btmReplyEvt(void* userData, char* ifName _UNUSED, swl_macChar_t* mac, uint8_t replyCode) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_bss_done(pAP, (const unsigned char*) mac->cMac, (int) replyCode);
}

static void s_mgtFrameReceivedEvt(void* userData, char* ifName _UNUSED, uint16_t stype, char* data) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Received frame %d", pAP->alias, stype);
    if((stype == SWL_IEEE80211_FC_STYPE_ASSOC_REQ) || (stype == SWL_IEEE80211_FC_STYPE_REASSOC_REQ)) {
        // save last assoc frame
        char buf[12] = {0};
        memcpy(buf, &data[24], 12);

        swl_macBin_t mac;
        swl_macBin_t bssid;
        swl_hex_toBytes(mac.bMac, SWL_MAC_BIN_LEN, buf, 12);
        memset(bssid.bMac, 0, SWL_MAC_BIN_LEN);
        if(pAP->pSSID->BSSID) {
            memcpy(bssid.bMac, pAP->pSSID->BSSID, SWL_MAC_BIN_LEN);
        }
        swl_timeReal_t timestamp = swl_time_getRealSec();
        wld_vap_assocTableStruct_t tuple = {mac, bssid, (data + 4), timestamp, stype};
        swl_circTable_addValues(&(pAP->lastAssocReq), &tuple);
        SAH_TRACEZ_INFO(ME, "%s: add/update assocReq entry for station "SWL_MAC_FMT, pAP->alias,
                        SWL_MAC_ARG(mac.bMac));


        T_AssociatedDevice* pAD = wld_vap_findOrCreateAssociatedDevice(pAP, &mac);
        ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to retrieve associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(mac.bMac));


        wld_ad_add_connection_try(pAP, pAD);

        wifiGen_staCapHandler_receiveAssocMsg(pAP, pAD, data);
        W_SWL_BIT_SET(pAD->assocCaps.freqCapabilities, pAP->pRadio->operatingFrequencyBand);
    }
}
swl_rc_ne wifiGen_setVapEvtHandlers(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    wld_wpaCtrl_evtHandlers_cb wpaCtrlVapEvtHandlers;
    memset(&wpaCtrlVapEvtHandlers, 0, sizeof(wpaCtrlVapEvtHandlers));

    //Set here the wpa_ctrl VAP event handlers
    wpaCtrlVapEvtHandlers.fWpsCancelMsg = s_wpsCancel;
    wpaCtrlVapEvtHandlers.fWpsTimeoutMsg = s_wpsTimeout;
    wpaCtrlVapEvtHandlers.fWpsSuccessMsg = s_wpsSuccess;
    wpaCtrlVapEvtHandlers.fWpsOverlapMsg = s_wpsOverlap;
    wpaCtrlVapEvtHandlers.fWpsFailMsg = s_wpsFail;
    wpaCtrlVapEvtHandlers.fApStationConnectedCb = s_apStationConnectedEvt;
    wpaCtrlVapEvtHandlers.fApStationDisconnectedCb = s_apStationDisconnectedEvt;
    wpaCtrlVapEvtHandlers.fBtmReplyCb = s_btmReplyEvt;
    wpaCtrlVapEvtHandlers.fMgtFrameReceivedCb = s_mgtFrameReceivedEvt;

    ASSERT_TRUE(wld_wpaCtrlInterface_setEvtHandlers(pAP->wpaCtrlInterface, pAP, &wpaCtrlVapEvtHandlers),
                SWL_RC_ERROR, ME, "%s: fail to set interface wpa evt handlers", pAP->alias);

    wld_nl80211_evtHandlers_cb nl80211VapEvtHandlers;
    memset(&nl80211VapEvtHandlers, 0, sizeof(nl80211VapEvtHandlers));

    //Set here the nl80211 VAP event handlers
    wld_ap_nl80211_setEvtListener(pAP, NULL, &nl80211VapEvtHandlers);

    return SWL_RC_OK;
}

static void s_stationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: station disconnected from "MAC_PRINT_FMT " reason %d", pEP->Name, MAC_PRINT_ARG(bBssidMac->bMac), reason);

    if((pEP->currentProfile != NULL) && (pEP->connectionStatus == EPCS_CONNECTED)) {
        wld_endpoint_sync_connection(pEP, false, 0);
    }
}

swl_rc_ne wifiGen_setEpEvtHandlers(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pEP->wpaSupp, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pEP->wpaSupp->wpaCtrlMngr, pEP->pRadio);

    wld_wpaCtrl_evtHandlers_cb wpaCtrlEpEvtHandlers;
    memset(&wpaCtrlEpEvtHandlers, 0, sizeof(wpaCtrlEpEvtHandlers));
    //Set here the wpa_ctrl EP event handlers
    wpaCtrlEpEvtHandlers.fStationDisconnectedCb = s_stationDisconnectedEvt;

    wld_wpaCtrlInterface_setEvtHandlers(pEP->wpaCtrlInterface, pEP, &wpaCtrlEpEvtHandlers);

    return SWL_RC_OK;
}

