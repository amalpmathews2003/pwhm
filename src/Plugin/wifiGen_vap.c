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
#include <swl/swl_mac.h>

#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wifiGen_fsm.h"

#define ME "genVap"

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
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        SAH_TRACEZ_INFO(ME, "%s - %s", pAP->alias, pSSID->SSID);
    } else {
        strncpy(buf, pSSID->SSID, bufsize);
    }
    return SWL_RC_OK;
}

int wifiGen_vap_status(T_AccessPoint* pAP) {
    return wld_linuxIfUtils_getState(wld_rad_getSocket(pAP->pRadio), pAP->alias);
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
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    return ret;
}

bool s_staCommand(T_AccessPoint* pAP, const char* cmd, swl_macBin_t* addrBuf) {
    char buff[32] = {0};

    int ret = wld_wpaCtrl_sendCmdSynced(pAP->wpaCtrlInterface, cmd, buff, sizeof(buff) - 1);

    ASSERTI_TRUE(ret >= 0, false, ME, "%s: Fail sta cmd: %s : ret %u", pAP->alias, cmd, ret);

    if(swl_str_nmatches(buff, "FAIL", 4)) {
        return false;
    }

    uint32_t endIndex = 0;
    while(endIndex < sizeof(buff) - 1 && buff[endIndex] != '\0' && buff[endIndex] != '\n') {
        endIndex++;
    }
    buff[endIndex] = '\0';

    size_t len = strlen(buff);
    SAH_TRACEZ_INFO(ME, "%s: recv %zu:  %s", pAP->alias, len, buff);
    ASSERTI_TRUE(len == 17, false, ME, "%s: mac too short: %zu", pAP->alias, len);

    return swl_mac_charToBin(addrBuf, (swl_macChar_t*) buff);
}

void s_syncMacList(T_AccessPoint* pAP) {
    wld_vap_mark_all_stations_unseen(pAP);

    swl_macBin_t myBin;

    bool hasSta = s_staCommand(pAP, "STA-FIRST", &myBin);
    while(hasSta) {
        SAH_TRACEZ_ERROR(ME, "%s:Recv sta " SWL_MAC_FMT, pAP->alias, SWL_MAC_ARG(myBin.bMac));
        T_AssociatedDevice* pDev = wld_vap_find_asociatedDevice(pAP, &myBin);
        if(pDev == NULL) {
            pDev = wld_ad_create_associatedDevice(pAP, &myBin);
            if(pDev == NULL) {
                SAH_TRACEZ_ERROR(ME, "could not create new AD %s @ %s", pDev->Name, pAP->alias);
                return;
            } else {
                SAH_TRACEZ_WARNING(ME, "created AD from assoclist %s @ %s", pDev->Name, pAP->alias);
            }
        }
        pDev->seen = 1;
        pDev->Active = 1;

        char cmdBuf[100] = {0};
        snprintf(cmdBuf, sizeof(cmdBuf), "STA-NEXT %.17s", pDev->Name);
        hasSta = s_staCommand(pAP, cmdBuf, &myBin);
    }

    wld_vap_update_seen(pAP);
}

static int64_t s_getIntParamCounter(const char* buf, const char* param) {
    char tmpBuf[64] = {0};
    bool paramFound = swl_str_getParameterValue(tmpBuf, sizeof(tmpBuf), buf, param, NULL);
    if(!paramFound) {
        return 0;
    }

    uint32_t endIndex = 0;
    while(endIndex < sizeof(tmpBuf) - 1 && tmpBuf[endIndex] != '\0'
          && tmpBuf[endIndex] != '\n' && tmpBuf[endIndex] != ' ') {
        endIndex++;
    }
    tmpBuf[endIndex] = '\0';


    int64_t val = swl_typeInt64_fromCharDef(tmpBuf, 0);
    return val;
}

int wifiGen_get_station_stats(T_AccessPoint* pAP) {
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_EQUALS(pRad->status, RST_ERROR, WLD_ERROR_INVALID_STATE, ME, "NULL");


    s_syncMacList(pAP);

    int32_t noise = 0;
    wld_rad_nl80211_getNoise(pRad, &noise);

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {

        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
        if(!pAD) {
            SAH_TRACEZ_ERROR(ME, "nullpointer! %p", pAD);
            return -1;
        }

        char buff[1024] = {0};
        char cmdBuf[100] = {0};
        snprintf(cmdBuf, sizeof(cmdBuf), "STA %.17s", pAD->Name);

        int ret = wld_wpaCtrl_sendCmdSynced(pAP->wpaCtrlInterface, cmdBuf, buff, sizeof(buff) - 1);
        ASSERT_TRUE(ret >= 0, false, ME, "%s: Fail sta cmd: %s : ret %u", pAP->alias, cmdBuf, ret);
        size_t len = strlen(buff);
        SAH_TRACEZ_INFO(ME, "%s: sta %s : len %zu", pAP->alias, pAD->Name, len);

        {
            char tmpBuf[64] = {0};

            bool paramFound = swl_str_getParameterValue(tmpBuf, sizeof(tmpBuf), buff, "flags", NULL);
            if(paramFound) {
                pAD->Active = (strstr(tmpBuf, "ASSOC") != NULL);
                pAD->AuthenticationState = (strstr(tmpBuf, "AUTHORIZED") != NULL);
            }
        }
        pAD->RxPacketCount = s_getIntParamCounter(buff, "rx_packets");
        pAD->TxPacketCount = s_getIntParamCounter(buff, "tx_packets");
        pAD->RxBytes = s_getIntParamCounter(buff, "rx_bytes");
        pAD->TxBytes = s_getIntParamCounter(buff, "tx_bytes");
        pAD->LastDataUplinkRate = s_getIntParamCounter(buff, "tx_rate_info") * 100;
        pAD->LastDataDownlinkRate = s_getIntParamCounter(buff, "rx_rate_info") * 100;
        pAD->SignalStrength = s_getIntParamCounter(buff, "signal");
        pAD->Inactive = s_getIntParamCounter(buff, "inactive_msec") / 1000;
        pAD->connectionDuration = s_getIntParamCounter(buff, "connected_time");
        int32_t secMode = s_getIntParamCounter(buff, "wpa");
        pAD->assocCaps.currentSecurity = APMSI_UNKNOWN;
        if(secMode == 1) {
            pAD->assocCaps.currentSecurity = APMSI_WPA_P;
        } else if(secMode == 2) {
            pAD->assocCaps.currentSecurity = APMSI_WPA2_P;
        }

        wld_nl80211_stationInfo_t stationInfo;
        swl_rc_ne retVal = wld_rad_nl80211_getStationInfo(pRad, (swl_macBin_t*) &pAD->MACAddress, &stationInfo);
        if(retVal != SWL_RC_OK) {
            continue;
        }
        pAD->downLinkRateSpec = stationInfo.txRate.mscInfo;
        pAD->TxFailures = stationInfo.txFailed;
        pAD->Tx_Retransmissions = stationInfo.txRetries;

        pAD->Noise = noise;
        if((noise != 0) && (pAD->SignalStrength != 0) && (pAD->SignalStrength > pAD->Noise)) {
            pAD->SignalNoiseRatio = pAD->SignalStrength - pAD->Noise;
        } else {
            pAD->SignalNoiseRatio = 0;
        }
    }


    return 0;
}
int wifiGen_vap_sec_sync(T_AccessPoint* pAP, int set) {
    int ret = 0;

    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "%s : set vap_sec_sync", pAP->alias);
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    return ret;
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
int wifiGen_vap_wps_sync(T_AccessPoint* pAP, char* val, int bufsize, int set) {
    if(!(set & SET)) {
        if((set & GET) && (val != NULL) && (bufsize > 64)) {
            snprintf(val, bufsize, "wps_configured=%s; wps_configmethod=%x",
                     (pAP->WPS_Configured) ? "Configured" : "Un-Configured",
                     pAP->WPS_ConfigMethodsEnabled);
        }

        return WLD_OK;
    }

    // When SSID is hided... we don't start WPS but STOP as escape...
    if(!pAP->SSIDAdvertisementEnabled || swl_str_matches(val, "STOP")) {
        wld_ap_hostapd_stopWps(pAP);
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
        return WLD_OK;
    }


    uint32_t iPIN = 0;

    /* First check for Client PIN? (if a pin is given it's client pin!) */
    if(!swl_str_isEmpty(val) && (pAP->WPS_ConfigMethodsEnabled & (APWPSCMS_LABEL | APWPSCMS_DISPLAY))) {

        // PIN can be 4 digits long, so using a sprintf %08d does not work (problems with certif)
        char* pCh = strchr(val, '=');
        if(pCh != NULL) {
            iPIN = atoi(pCh + 1); //I use it anyway to verify that val is a numeric argument, thus a PIN
        } else {
            iPIN = atoi(val);
        }

        if(iPIN != 0) {
            /* client PIN scenario */

            wld_ap_hostapd_startWpsPin(pAP, iPIN);
            wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PIN, NULL);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: pin invalid -%s-", pAP->alias, val);
            return WLD_ERROR;
        }

    } else if(pAP->WPS_ConfigMethodsEnabled & APWPSCMS_PBC) {
        wld_ap_hostapd_startWps(pAP);
        //Note, the command to the driver is not sent yet (see set_wps_env below)
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PBC, NULL);
    }
    return WLD_OK;
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
