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
#include <swl/swl_common.h>
#include <swla/swla_mac.h>

#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_linuxIfStats.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wifiGen_fsm.h"
#include "wld/wld_statsmon.h"

#define ME "genVap"

#define RRM_BEACON_REPORT_MODE_ACTIVE 0x01
#define RRM_DEFAULT_REQ_MODE 0x00
#define RRM_DEFAULT_RANDOM_INTERVAL 0x0000
#define RRM_DEFAULT_MEASUREMENT_DURATION 0x0004
#define RRM_DEFAULT_MEASUREMENT_MODE RRM_BEACON_REPORT_MODE_ACTIVE

int wifiGen_vap_createHook(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return SWL_RC_OK;
}

void wifiGen_vap_destroyHook(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_ap_nl80211_delEvtListener(pAP);
    wld_wpaCtrlInterface_cleanup(&pAP->wpaCtrlInterface);
}

int wifiGen_vap_ssid(T_AccessPoint* pAP, char* buf, int bufsize, int set) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;
    ASSERTI_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
    if(set & SET) {
        swl_str_copy(pSSID->SSID, sizeof(pSSID->SSID), buf);
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_SSID);
        SAH_TRACEZ_INFO(ME, "%s - %s", pAP->alias, pSSID->SSID);
    } else {
        strncpy(buf, pSSID->SSID, bufsize);
    }
    return SWL_RC_OK;
}

int wifiGen_vap_status(T_AccessPoint* pAP) {
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), false, ME, "%s: wpactrl iface not ready", pAP->alias);
    ASSERTI_TRUE(pAP->index > 0, false, ME, "%s: iface has no netdev index", pAP->alias);
    int ret = wld_linuxIfUtils_getLinkState(wld_rad_getSocket(pAP->pRadio), pAP->alias);
    ASSERTS_FALSE(ret <= 0, false, ME, "%s: link down", pAP->alias);
    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_rc_ne rc = wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, false, ME, "%s: Fail to get nl80211 ap iface info", pAP->alias);
    ASSERTS_STR(ifaceInfo.ssid, false, ME, "%s: ssid down", pAP->alias);
    ASSERTS_NOT_EQUALS(ifaceInfo.chanSpec.ctrlFreq, 0, false, ME, "%s: radio down", pAP->alias);
    return true;
}

int wifiGen_vap_enable(T_AccessPoint* pAP, int enable, int set) {
    int ret;
    SAH_TRACEZ_INFO(ME, "VAP:%s State:%d-->%d - Set:%d", pAP->alias, pAP->enable, enable, set);
    if(set & SET) {
        ret = pAP->enable = enable;
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_AP);
    } else {
        ret = pAP->enable;
    }
    return ret;
}

int wifiGen_vap_sync(T_AccessPoint* pAP, int set) {
    int ret = 0;

    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "%s : set vap_sync", pAP->alias);
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    }
    return ret;
}

static swl_rc_ne s_getNetlinkAllStaInfo(T_AccessPoint* pAP) {

    uint32_t nStations = 0;
    wld_nl80211_stationInfo_t* pAllStaInfo = NULL;
    swl_rc_ne retVal = wld_ap_nl80211_getAllStationsInfo(pAP, &pAllStaInfo, &nStations);
    ASSERT_FALSE(retVal < SWL_RC_OK, retVal, ME, "%s: fail to get all stations info", pAP->alias);

    int32_t noise = 0;
    wld_rad_nl80211_getNoise(pAP->pRadio, &noise);

    T_AssociatedDevice* pAD;
    wld_nl80211_stationInfo_t* pStationInfo = NULL;
    // Add new devices from driver maclist
    for(uint32_t id = 0; id < nStations; id++) {
        pStationInfo = &pAllStaInfo[id];
        if((pAD = wld_vap_find_asociatedDevice(pAP, &pStationInfo->macAddr)) == NULL) {
            if((pAD = wld_create_associatedDevice(pAP, &pStationInfo->macAddr)) == NULL) {
                SAH_TRACEZ_ERROR(ME, "%s: could not create new detected AD "SWL_MAC_FMT,
                                 pAP->alias, SWL_MAC_ARG(pStationInfo->macAddr.bMac));
                continue;
            }
            SAH_TRACEZ_WARNING(ME, "%s: created missing AD %s", pAP->alias, pAD->Name);
        }
        pAD->seen = true;
        wld_ap_nl80211_copyStationInfoToAssocDev(pAP, pAD, pStationInfo);
        if((!pAD->AuthenticationState) && (pStationInfo->flags.authenticated == SWL_TRL_TRUE)) {
            wld_ad_add_connection_success(pAP, pAD);
        }
        pAD->noise = noise;
        if((noise != 0) && (pAD->SignalStrength != 0) && (pAD->SignalStrength > pAD->noise)) {
            pAD->SignalNoiseRatio = pAD->SignalStrength - pAD->noise;
        } else {
            pAD->SignalNoiseRatio = 0;
        }
    }
    free(pAllStaInfo);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_get_station_stats(T_AccessPoint* pAP) {
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_EQUALS(pRad->status, RST_ERROR, SWL_RC_INVALID_STATE, ME, "NULL");

    wld_vap_mark_all_stations_unseen(pAP);
    s_getNetlinkAllStaInfo(pAP);

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {

        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
        if(!pAD) {
            SAH_TRACEZ_ERROR(ME, "nullpointer! %p", pAD);
            return SWL_RC_ERROR;
        }
        if(!pAD->seen) {
            pAD->SignalStrength = 0;
            for(int j = 0; j < MAX_NR_ANTENNA; j++) {
                pAD->SignalStrengthByChain[j] = 0;
            }
            pAD->SignalNoiseRatio = 0;
            SAH_TRACEZ_WARNING(ME, "Skip unseen sta %s", pAD->Name);
            continue;
        }
        wld_ap_hostapd_getStaInfo(pAP, pAD);
    }

    wld_vap_update_seen(pAP);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_get_single_station_stats(T_AssociatedDevice* pAD) {
    T_AccessPoint* pAP = (T_AccessPoint*) amxd_object_get_parent(amxd_object_get_parent(pAD->object))->priv;
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_EQUALS(pRad->status, RST_ERROR, SWL_RC_INVALID_STATE, ME, "NULL");

    wld_vap_mark_all_stations_unseen(pAP);
    s_getNetlinkAllStaInfo(pAP);

    SAH_TRACEZ_INFO(ME, "pAP->alias = %s", pAP->alias);
    SAH_TRACEZ_INFO(ME, "pAD->Name = %s", pAD->Name);

    if(!pAD->seen) {
        pAD->SignalStrength = 0;
        for(int j = 0; j < MAX_NR_ANTENNA; j++) {
            pAD->SignalStrengthByChain[j] = 0;
        }
        pAD->SignalNoiseRatio = 0;
    }
    wld_ap_hostapd_getStaInfo(pAP, pAD);
    wld_vap_update_seen(pAP);
    return SWL_RC_OK;
}

int wifiGen_vap_sec_sync(T_AccessPoint* pAP, int set) {
    int ret = 0;

    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "%s : set vap_sec_sync", pAP->alias);
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_SEC);
    }
    return ret;
}

int wifiGen_vap_multiap_update_type(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    return 0;
}

int wifiGen_vap_sta_transfer_ext(T_AccessPoint* pAP, wld_transferStaArgs_t* params) {
    return wld_ap_hostapd_transferStation(pAP, params);
}

int wifiGen_vap_sta_transfer(T_AccessPoint* pAP, char* sta, char* bssid, int operClass, int channel) {
    wld_transferStaArgs_t params;
    snprintf(params.sta, sizeof(params.sta), "%s", sta);
    snprintf(params.targetBssid, sizeof(params.targetBssid), "%s", bssid);
    params.operClass = operClass;
    params.channel = channel;
    params.disassoc = 15;
    params.validity = 0;
    return wifiGen_vap_sta_transfer_ext(pAP, &params);
}

int wifiGen_vap_mf_sync(T_AccessPoint* vap, int set) {
    ASSERTS_TRUE(set & SET, 0, ME, "Only do set");
    if(set & DIRECT) {
        return wld_ap_hostapd_setMacFilteringList(vap);
    } else {
        setBitLongArray(vap->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    return 0;
}

swl_rc_ne wifiGen_vap_wps_sync(T_AccessPoint* pAP, char* val, int bufsize, int set) {
    swl_rc_ne rc;
    if(!(set & SET)) {
        if((set & GET) && (val != NULL) && (bufsize > 64)) {
            snprintf(val, bufsize, "wps_configured=%s; wps_configmethod=%x",
                     (pAP->WPS_Configured) ? "Configured" : "Un-Configured",
                     pAP->WPS_ConfigMethodsEnabled);
        }

        return SWL_RC_OK;
    }

    // When SSID is hidden... we don't start WPS but STOP as escape...
    if(!pAP->SSIDAdvertisementEnabled || swl_str_matches(val, "STOP")) {
        rc = wld_ap_hostapd_stopWps(pAP);
        ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to stop wps session", pAP->alias);
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
        return SWL_RC_OK;
    }

    /*
     * Empty val or asking for supported one of PushButton methods: start PBC
     */
    if(((swl_str_isEmpty(val)) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC))) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC_P))) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC_V)))) &&
       (pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_PBC_ALL))) {
        rc = wld_ap_hostapd_startWps(pAP);
        ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps pbc session", pAP->alias);
        //Note, the command to the driver is not sent yet (see set_wps_env below)
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PBC, NULL);
        return SWL_RC_OK;
    }

    if(!swl_str_isEmpty(val)) {
        if(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_PIN)) {
            char* clientPIN = NULL;
            if(swl_str_startsWith(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PIN))) {
                char* pCh = strchr(val, '=');
                ASSERT_NOT_NULL(pCh, SWL_RC_ERROR, ME, "%s: no wps pin in (%s)", pAP->alias, val);
                pCh++;
                ASSERT_TRUE(wldu_checkWpsPinStr(pCh), SWL_RC_ERROR, ME, "%s: invalid wps pin in (%s)", pAP->alias, val);
                clientPIN = pCh;
            } else if(wldu_checkWpsPinStr(val)) {
                // when command is a valid PIN (format & value), we assume a WPS client pin request (Keypad)
                clientPIN = val;
            }
            if(clientPIN != NULL) {
                rc = wld_ap_hostapd_startWpsPin(pAP, clientPIN, WPS_WALK_TIME_DEFAULT);
                ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps client pin (%s) session", pAP->alias, clientPIN);
                wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PIN, NULL);
                return SWL_RC_OK;
            }
        }
        if((pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL)) &&
           ((swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_LABEL))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY_P))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY_V))))) {
            rc = wld_ap_hostapd_setWpsApPin(pAP, pAP->pRadio->wpsConst->DefaultPin, WPS_WALK_TIME_DEFAULT);
            ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps ap pin session", pAP->alias);
            //WPS AP Pin is not attached to a pairing session
            return SWL_RC_OK;
        }
        SAH_TRACEZ_ERROR(ME, "%s: unsupported wps sync val (%s)", pAP->alias, val);
        return SWL_RC_ERROR;
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_wps_enable(T_AccessPoint* pAP, int enable, int set) {
    /* WPS enabling requires restarting hostapd */
    if(set & SET) {
        pAP->WPS_Enable = enable;
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    }

    return SWL_RC_OK;
}

int wifiGen_vap_kick_sta_reason(T_AccessPoint* pAP, char* buf, int bufsize _UNUSED, int reason) {
    ASSERT_TRUE(buf && strlen(buf), SWL_RC_INVALID_PARAM, ME, "Invalid");
    swl_macBin_t bMac;

    SWL_MAC_CHAR_TO_BIN(&bMac, buf);
    SAH_TRACEZ_INFO(ME, "kickmac %s - (%s) rsn %u", pAP->alias, buf, reason);
    return wld_ap_hostapd_kickStation(pAP, &bMac, (swl_IEEE80211deauthReason_ne) reason);
}

int wifiGen_vap_kick_sta(T_AccessPoint* pAP, char* buf, int bufsize, int set _UNUSED) {
    return wifiGen_vap_kick_sta_reason(pAP, buf, bufsize, SWL_IEEE80211_DEAUTH_REASON_AUTH_NO_LONGER_VALID);
}

int wifiGen_vap_updateApStats(T_Radio* rad, T_AccessPoint* vap) {
    _UNUSED_(vap);
    //TODO : wiphyInfo.suppCmds.survey is not registred in NL80211_ATTR_SUPPORTED_COMMANDS in kernel version 4.19
    wld_rad_nl80211_getNoise(rad, &rad->stats.noise);
    wld_updateRadioStats(rad, NULL);
    return 0;
}

swl_rc_ne wifiGen_wendpoint_stats(T_EndPoint* pEP, T_EndPointStats* stats) {
    ASSERT_NOT_NULL(pEP, SWL_RC_ERROR, ME, "NULL");

    swl_rc_ne ret;
    if(wld_linuxIfStats_getInterfaceStats(pEP->Name, &pEP->pSSID->stats)) {
        if(stats != NULL) {
            stats->LastDataDownlinkRate = 0;
            stats->LastDataUplinkRate = 0;
            stats->SignalStrength = 0;
            stats->noise = 0;
            stats->SignalNoiseRatio = 0;
            stats->RSSI = 0;
            stats->Retransmissions = pEP->pSSID->stats.RetransCount;
            stats->txbyte = pEP->pSSID->stats.BytesSent;
            stats->txPackets = pEP->pSSID->stats.PacketsSent;
            stats->txRetries = pEP->pSSID->stats.RetryCount;
            stats->rxbyte = pEP->pSSID->stats.BytesReceived;
            stats->rxPackets = pEP->pSSID->stats.PacketsReceived;
            stats->rxRetries = 0;

            stats->maxRxStream = 0;
            stats->maxTxStream = 0;
        }

        ret = SWL_RC_OK;
        SAH_TRACEZ_INFO(ME, "get stats for %s OK", pEP->Name);
    } else {
        ret = SWL_RC_ERROR;
        SAH_TRACEZ_INFO(ME, "get stats for %s fail", pEP->Name);
    }
    return ret;
}

swl_rc_ne wifiGen_update_ap_stats(T_Radio* rad _UNUSED, T_AccessPoint* pAP) {
    swl_rc_ne ret;
    if(wld_linuxIfStats_getInterfaceStats(pAP->alias, &pAP->pSSID->stats)) {
        ret = SWL_RC_OK;
        SAH_TRACEZ_INFO(ME, "get stats for %s OK", pAP->alias);
    } else {
        SAH_TRACEZ_INFO(ME, "get stats for %s fail", pAP->alias);
        ret = SWL_RC_ERROR;
    }
    return ret;
}

swl_rc_ne wifiGen_vap_requestRrmReport(T_AccessPoint* pAP, const swl_macChar_t* sta, int operClass, swl_channel_t channel, const swl_macChar_t* bssid, const char* ssid) {
    SAH_TRACEZ_INFO(ME, "%s: send rrm to %s %u/%u %s %s", pAP->alias, sta->cMac, operClass, channel, bssid->cMac, ssid);
    return wld_ap_hostapd_requestRRMReport(pAP, sta, RRM_DEFAULT_REQ_MODE, (uint8_t) operClass, channel, RRM_DEFAULT_RANDOM_INTERVAL,
                                           RRM_DEFAULT_MEASUREMENT_DURATION, RRM_DEFAULT_MEASUREMENT_MODE, bssid, ssid);
}
