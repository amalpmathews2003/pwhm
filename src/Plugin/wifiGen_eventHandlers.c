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
#include "wifiGen_fsm.h"
#include "wld/wld_wpaCtrlMngr.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_endpoint.h"
#include "wld/wld_chanmgt.h"
#include "swl/swl_hex.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_genericFrameParser.h"
#include <amxc/amxc.h>
#include "wifiGen_staCapHandler.h"
#include <errno.h>
#define ME "genEvt"

static void s_saveChanChanged(T_Radio* pRad, swl_chanspec_t* pChanSpec, wld_channelChangeReason_e reason) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pChanSpec, , ME, "NULL");
    ASSERTS_TRUE(pChanSpec->channel > 0, , ME, "invalid chan");
    swl_rc_ne rc = wld_chanmgt_reportCurrentChanspec(pRad, *pChanSpec, reason);
    ASSERTS_FALSE(rc < SWL_RC_OK, , ME, "no changes to be reported");
    wld_rad_updateOperatingClass(pRad);
}

static void s_syncCurrentChannel(T_Radio* pRad, wld_channelChangeReason_e reason) {
    swl_chanspec_t chanSpec;
    ASSERTS_FALSE(wld_rad_nl80211_getChannel(pRad, &chanSpec) < SWL_RC_OK, ,
                  ME, "%s: fail to get channel", pRad->Name);
    s_saveChanChanged(pRad, &chanSpec, reason);
}

static void s_chanSwitchCb(void* userData, char* ifName _UNUSED, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: channel switch event central_chan=%d bandwidth=%d band=%d", pRad->Name,
                       chanSpec->channel, chanSpec->bandwidth, chanSpec->band);
    s_saveChanChanged(pRad, chanSpec, pRad->targetChanspec.reason);
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
        //when manager is started, radio chanspec is read from secDmn conf file (saved conf)
        s_syncCurrentChannel(pRad, CHAN_REASON_PERSISTANCE);
        //update rad status from hapd main iface state
        if((wifiGen_hapd_updateRadState(pRad) >= SWL_RC_OK) &&
           (pRad->detailedState == CM_RAD_UP)) {
            //we may missed the CAC Done event waiting to finalize wpactrl connection
            //so mark current chanspec as active
            wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_STATE);
            wld_rad_doCommitIfUnblocked(pRad);
            return;
        }
    }
    wld_rad_updateState(pRad, false);
}

static void s_mainApSetupCompletedCb(void* userData, char* ifName) {
    SAH_TRACEZ_WARNING(ME, "%s: main AP has setup completed", ifName);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pRad->detailedState = CM_RAD_UP;
    // when main iface setup is completed, current chanspec is the last applied target chanspec
    s_syncCurrentChannel(pRad, pRad->targetChanspec.reason);
    wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_STATE);
    wld_rad_doCommitIfUnblocked(pRad);
    wld_rad_updateState(pRad, true);
    pRad->pFA->mfn_wrad_poschans(pRad, NULL, 0);
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
    wld_channelChangeReason_e reason = pRad->targetChanspec.reason;
    if(wld_channel_is_radar_detected(pRad->currentChanspec.chanspec)) {
        reason = CHAN_REASON_DFS;
    }
    s_syncCurrentChannel(pRad, reason);
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
        wld_chanmgt_writeDfsChannels(pRad);
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
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsRadarDetectedCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Radar detected on channel %d bw %d", ifName, chanSpec->channel, chanSpec->bandwidth);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_mark_radar_detected_band(*chanSpec);
    wld_rad_updateState(pRad, false);
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsNopFinishedCb(void* userData _UNUSED, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Non-Occupancy Period is over on channel %d", ifName, chanSpec->channel);
    wld_channel_clear_radar_detected_channel(*chanSpec);
    wld_channel_mark_passive_channel(*chanSpec);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsNewChannelCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Radar detection will trigger switching to channel %d", ifName, chanSpec->channel);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t currChanSpec = *chanSpec;
    currChanSpec.bandwidth = pRad->runningChannelBandwidth;
    s_saveChanChanged(pRad, &currChanSpec, CHAN_REASON_DFS);
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
    SAH_TRACEZ_NOTICE(ME, "%s: interface created (devIdx:%d)", pIfaceInfo->name, pIfaceInfo->ifIndex);
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
    if(pAP != NULL) {
        pAP->index = pIfaceInfo->ifIndex;
        wld_vap_updateState(pAP);
    }
}

static void s_delInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
    SAH_TRACEZ_NOTICE(ME, "%s: interface deleted (devIdx:%d)", pIfaceInfo->name, pIfaceInfo->ifIndex);
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
    if(pAP != NULL) {
        pAP->index = 0;
        wld_vap_updateState(pAP);
    }
}

static void s_scanResultsCb(void* priv, swl_rc_ne rc, T_ScanResults* results) {
    T_Radio* pRad = (T_Radio*) priv;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(results, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: rc:%d nResults:%zu", pRad->Name, rc, amxc_llist_size(&results->ssids));
    if(rc < SWL_RC_OK) {
        wld_scan_done(pRad, false);
        return;
    }
    wld_scan_cleanupScanResults(&pRad->scanState.lastScanResults);
    amxc_llist_for_each(it, &results->ssids) {
        T_ScanResult_SSID* pResult = amxc_container_of(it, T_ScanResult_SSID, it);
        SAH_TRACEZ_INFO(ME, "scan result entry: bssid("SWL_MAC_FMT ") ssid(%s) signal(%d dbm)",
                        SWL_MAC_ARG(pResult->bssid.bMac), pResult->ssid, pResult->rssi);
        amxc_llist_it_take(&pResult->it);
        amxc_llist_append(&pRad->scanState.lastScanResults.ssids, &pResult->it);
    }
    wld_scan_done(pRad, true);
}

static void s_scanDoneCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    SAH_TRACEZ_INFO(ME, "scan done on wiphy:%d iface:%d", wiphy, ifIndex);
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTI_NOT_EQUALS(pRad->scanState.scanType, SCAN_TYPE_NONE, , ME, "%s: No user scan running", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: getting scan async results", pRad->Name);
    wld_rad_nl80211_getScanResults(pRad, pRad, s_scanResultsCb);
}

static void s_scanAbortedCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    SAH_TRACEZ_INFO(ME, "scan aborted on wiphy:%d iface:%d", wiphy, ifIndex);
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTI_NOT_EQUALS(pRad->scanState.scanType, SCAN_TYPE_NONE, , ME, "%s: No user scan running", pRad->Name);
    wld_scan_done(pRad, false);
}

static void s_scanStartedCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    SAH_TRACEZ_INFO(ME, "scan started on wiphy:%d iface:%d", wiphy, ifIndex);
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
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
    nl80211RadEvtHandlers.fScanStartedCb = s_scanStartedCb;
    nl80211RadEvtHandlers.fScanAbortedCb = s_scanAbortedCb;
    nl80211RadEvtHandlers.fScanDoneCb = s_scanDoneCb;
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

    wld_ad_add_connection_success(pAP, pAD);
}

static void s_apStationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: disconnecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    wld_ad_add_disconnection(pAP, pAD);
}

static void s_btmReplyEvt(void* userData, char* ifName _UNUSED, swl_macChar_t* mac, uint8_t replyCode) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_bss_done(pAP, mac, (int) replyCode);
}

static void s_commonAssocReqCb(T_AccessPoint* pAP, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_bit8_t* iesData, size_t iesLen) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(pAP->pSSID, , ME, "%s: Ap has No SSID", pAP->alias);
    ASSERT_TRUE(SWL_MAC_BIN_MATCHES(pAP->pSSID->BSSID, &frame->bssid), ,
                ME, "%s: bssid does not match ap("MAC_PRINT_FMT ") frame("MAC_PRINT_FMT ")",
                pAP->alias, MAC_PRINT_ARG(pAP->pSSID->BSSID), MAC_PRINT_ARG(frame->bssid.bMac));

    T_AssociatedDevice* pAD = wld_vap_findOrCreateAssociatedDevice(pAP, &frame->transmitter);
    ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to retrieve associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(frame->transmitter.bMac));
    wld_vap_saveAssocReq(pAP, (swl_bit8_t*) frame, frameLen);

    wifiGen_staCapHandler_receiveAssocMsg(pAP, pAD, iesData, iesLen);
    W_SWL_BIT_SET(pAD->assocCaps.freqCapabilities, pAP->pRadio->operatingFrequencyBand);

    wld_ad_add_connection_try(pAP, pAD);
}

static void s_assocReqCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_80211_assocReqFrameBody_t* assocReq, size_t assocReqDataLen) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(assocReq, , ME, "NULL");
    s_commonAssocReqCb(pAP, frame, frameLen, assocReq->data, assocReqDataLen);
}

static void s_reassocReqCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_80211_reassocReqFrameBody_t* reassocReq, size_t reassocReqDataLen) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(reassocReq, , ME, "NULL");
    s_commonAssocReqCb(pAP, frame, frameLen, reassocReq->data, reassocReqDataLen);
}

static void s_btmQueryCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen _UNUSED, swl_80211_wnmActionBTMQueryFrameBody_t* btmQuery, size_t btmQueryDataLen _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(btmQuery, , ME, "NULL");
    swl_macChar_t mac;
    SWL_MAC_BIN_TO_CHAR(&mac, &frame->transmitter);
    SAH_TRACEZ_INFO(ME, "%s BSS-TM-QUERY %s reason %d", pAP->alias, mac.cMac, btmQuery->reason);
}

static void s_btmRespCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen _UNUSED, swl_80211_wnmActionBTMResponseFrameBody_t* btmResp, size_t btmRespDataLen _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(btmResp, , ME, "NULL");
    uint8_t replyCode = btmResp->statusCode;
    swl_macChar_t mac;
    SWL_MAC_BIN_TO_CHAR(&mac, &frame->transmitter);
    SAH_TRACEZ_INFO(ME, "%s BSS-TM-RESP %s status_code=%d", pAP->alias, mac.cMac, replyCode);
    s_btmReplyEvt(pAP, pAP->alias, &mac, replyCode);
}

static swl_80211_mgmtFrameHandlers_cb s_mgmtFrameHandlers = {
    .fProcAssocReq = s_assocReqCb,
    .fProcReAssocReq = s_reassocReqCb,
    .fProcBtmQuery = s_btmQueryCb,
    .fProcBtmResp = s_btmRespCb,
};

static void s_mgtFrameReceivedEvt(void* userData, char* ifName _UNUSED, swl_80211_mgmtFrame_t* mgmtFrame, size_t frameLen, char* frameStr _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(mgmtFrame, , ME, "NULL");
    swl_80211_handleMgmtFrame(userData, (swl_bit8_t*) mgmtFrame, frameLen, &s_mgmtFrameHandlers);
}

static void s_beaconResponseEvt(void* userData, char* ifName _UNUSED, swl_macBin_t* station, wld_wpaCtrl_rrmBeaconRsp_t* rrmBeaconResponse) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    swl_macChar_t cStation;
    swl_macChar_t cBssid;
    SWL_MAC_BIN_TO_CHAR(&cStation, station);
    SWL_MAC_BIN_TO_CHAR(&cBssid, &(rrmBeaconResponse->bssid));


    SAH_TRACEZ_INFO(ME, "%s: RRM reply received station:%s bssid:%s", pAP->alias, cStation.cMac, cBssid.cMac);

    SAH_TRACEZ_INFO(ME, "%s: opclass: %d channel: %d, duration: %d, frame info: %d, rcpi: %d, rsni: %d, antenna id: %d, parent tsf: %u\n",
                    pAP->alias, rrmBeaconResponse->opclass, rrmBeaconResponse->channel, rrmBeaconResponse->duration, rrmBeaconResponse->frameInfo,
                    rrmBeaconResponse->rcpi, rrmBeaconResponse->rsni, rrmBeaconResponse->antennaId, rrmBeaconResponse->parentTsf);

    amxc_var_t retval;
    amxc_var_init(&retval);
    amxc_var_set_type(&retval, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &retval, "BSSID", (char*) cBssid.cMac);
    amxc_var_add_key(uint8_t, &retval, "Channel", rrmBeaconResponse->channel);
    amxc_var_add_key(uint8_t, &retval, "RCPI", rrmBeaconResponse->rcpi);
    amxc_var_add_key(uint8_t, &retval, "RSNI", rrmBeaconResponse->rsni);
    wld_ap_rrm_item(pAP, &cStation, &retval);
    amxc_var_clean(&retval);
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
    wpaCtrlVapEvtHandlers.fBeaconResponseCb = s_beaconResponseEvt;

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

static void s_stationConnectedEvt(void* pRef, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: station connected to "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(bBssidMac->bMac));

    T_Radio* pRad = pEP->pRadio;
    // enable the hapd previously disabled
    if(wifiGen_hapd_isRunning(pRad)) {
        wld_rad_hostapd_enable(pRad);
    }
    wld_endpoint_sync_connection(pEP, true, 0);

    // update radio datamodel
    swl_chanspec_t chanSpec;
    wld_rad_nl80211_getChannel(pRad, &chanSpec);
    s_saveChanChanged(pRad, &chanSpec, CHAN_REASON_EP_MOVE);
    wld_channel_clear_passive_band(chanSpec);
    wld_rad_updateState(pRad, true);

    // set hostapd channel
    if(wifiGen_hapd_isRunning(pRad)) {
        setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_CONNECTED_EP);
        wld_rad_doCommitIfUnblocked(pRad);
    }
}

static void s_stationScanFailedEvt(void* pRef, char* ifName, int error) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: scan Failed with error(%d)", pEP->Name, error);
    if((error == -EBUSY) && wifiGen_hapd_isAlive(pRad)) {
        // disable hostapd to allow wpa_supplicant to do scan
        wld_rad_hostapd_disable(pRad);
    }
}

static void s_stationWpsCredReceivedEvt(void* pRef, char* ifName, void* creds, swl_rc_ne status) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(creds, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: wps credentials received with status (%d)", pEP->Name, status);
    if(swl_rc_isOk(status)) {
        pEP->WPS_Configured = 1;
        pEP->connectionStatus = EPCS_WPS_PAIRINGDONE;
        wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, (T_WPSCredentials*) creds);
    } else {
        wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_CREDENTIALS, NULL);
    }
}

static void s_stationWpsCancel(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
}

static void s_stationWpsTimeout(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL);
}

static void s_stationWpsSuccess(void* userData, char* ifName _UNUSED, swl_macChar_t* mac _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, NULL);
}

static void s_stationWpsOverlap(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_OVERLAP, NULL);
}

static void s_stationWpsFail(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_FAILURE, NULL);
}

swl_rc_ne wifiGen_setEpEvtHandlers(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pEP->wpaSupp, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pEP->wpaSupp->wpaCtrlMngr, pEP->pRadio);

    wld_wpaCtrl_evtHandlers_cb wpaCtrlEpEvtHandlers;
    memset(&wpaCtrlEpEvtHandlers, 0, sizeof(wpaCtrlEpEvtHandlers));
    //Set here the wpa_ctrl EP event handlers
    wpaCtrlEpEvtHandlers.fStationDisconnectedCb = s_stationDisconnectedEvt;
    wpaCtrlEpEvtHandlers.fStationConnectedCb = s_stationConnectedEvt;
    wpaCtrlEpEvtHandlers.fStationScanFailedCb = s_stationScanFailedEvt;
    wpaCtrlEpEvtHandlers.fWpsCredReceivedCb = s_stationWpsCredReceivedEvt;
    wpaCtrlEpEvtHandlers.fWpsCancelMsg = s_stationWpsCancel;
    wpaCtrlEpEvtHandlers.fWpsTimeoutMsg = s_stationWpsTimeout;
    wpaCtrlEpEvtHandlers.fWpsSuccessMsg = s_stationWpsSuccess;
    wpaCtrlEpEvtHandlers.fWpsOverlapMsg = s_stationWpsOverlap;
    wpaCtrlEpEvtHandlers.fWpsFailMsg = s_stationWpsFail;

    wld_wpaCtrlInterface_setEvtHandlers(pEP->wpaCtrlInterface, pEP, &wpaCtrlEpEvtHandlers);

    return SWL_RC_OK;
}

