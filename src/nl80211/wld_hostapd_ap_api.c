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
#include "wld.h"
#include "wld_util.h"
#include "wld_hostapd_cfgFile.h"
#include "wld_hostapd_ap_api.h"
#include "wld_wpaCtrl_api.h"
#include "wld_accesspoint.h"

#define ME "hapdAP"

#define WPS_START  "WPS_PBC"
#define WPS_CANCEL "WPS_CANCEL"

bool s_sendHostapdCommand(T_AccessPoint* pAP, char* cmd, const char* reason) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), false, ME, "Connection with hostapd not yet established");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: send hostapd cmd %s for %s", pR->Name, cmd, reason);
    return wld_wpaCtrl_sendCmdCheckResponse(pAP->wpaCtrlInterface, cmd, "OK\n");
}

/**
 * @brief reload the hostapd
 *
 * @param pAP accesspoint
 * @param reason the command caller
 * @return true when the Hostapd config is reloaded. Otherwise false.
 */
bool wld_ap_hostapd_reload(T_AccessPoint* pAP, const char* reason) {
    return s_sendHostapdCommand(pAP, "RELOAD", reason);
}

/**
 * @brief update beacon frame
 *
 * @param pAP accesspoint
 * @param reason the command caller
 * @return true when the  UPDATE_BEACON cmd is executed successfully. Otherwise false.
 */
bool wld_ap_hostapd_updateBeacon(T_AccessPoint* pAP, const char* reason) {
    return s_sendHostapdCommand(pAP, "UPDATE_BEACON", reason);
}

/**
 * @brief set a parameter value in hostapd context
 *
 * @param pAP accesspoint
 * @param reason the command caller
 * @return true when the SET cmd is executed successfully. Otherwise false.
 */
bool wld_ap_hostapd_setParamValue(T_AccessPoint* pAP, const char* field, const char* value, const char* reason) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "SET %s %s", field, value);
    return s_sendHostapdCommand(pAP, cmd, reason);
}

/**
 * @brief send hostapd command
 *
 * @param pAP accesspoint
 * @param cmd the command to send
 * @param reason the command caller
 * @return true when the SET cmd is executed successfully. Otherwise false.
 */
bool wld_ap_hostapd_sendCommand(T_AccessPoint* pAP, char* cmd, const char* reason) {
    return s_sendHostapdCommand(pAP, cmd, reason);
}

/**
 * @brief set the SSID Advertisement in the hostapd
 *
 * @param pAP accesspoint
 * @param enable if true the SSID Advertisement is enabled. Otherwise false
 * @return - SECDMN_ACTION_OK_NEED_UPDATE_BEACON when the ignore_broadcast_ssid is updated for the hostapd config file and the hostapd context
 *         - SECDMN_ACTION_OK_NEED_RESTART when ignore_broadcast_ssid is updated the hostapd context but not for the hostapd config file.
 *         - Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSSIDAdvertisement(T_AccessPoint* pAP, bool enable) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    char* interface = pAP->alias;
    ASSERTS_NOT_NULL(interface, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");

    // set the value
    char val[2];
    val[0] = enable ? '0' : '2';
    val[1] = '\0';

    // set the ignore_broadcast_ssid field
    bool ret = wld_ap_hostapd_setParamValue(pAP, "ignore_broadcast_ssid", val, "ignore_broadcast_ssid");
    ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting ignore_broadcast_ssid");

    // update hostapd config file
    ret = wld_hostapd_cfgFile_update(pR->hostapd->cfgFile, (char*) interface, "ignore_broadcast_ssid", val);
    ASSERT_TRUE(ret, SECDMN_ACTION_OK_NEED_RESTART, ME, "update the hostapd config failed for setting ignore_broadcast_ssid");

    return SECDMN_ACTION_OK_NEED_UPDATE_BEACON;
}

/**
 * @brief set the SSID in the hostapd
 *
 * @param pAP accesspoint
 * @param ssid the new ssid
 * @return - SECDMN_ACTION_OK_NEED_UPDATE_BEACON when the SSID is updated for the hostapd config file and the hostapd context
 *         - SECDMN_ACTION_OK_NEED_RESTART  when the SSID is updated for the hostapd context but not for the hostapd config file
 *         - Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSsid(T_AccessPoint* pAP, const char* ssid) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    char* interface = pAP->alias;
    ASSERTS_NOT_NULL(interface, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(ssid, SECDMN_ACTION_ERROR, ME, "NULL");

    // set the ssid field in the hostapd context
    bool ret = wld_ap_hostapd_setParamValue(pAP, "ssid", ssid, "ssid");
    ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting SSID");

    // update the hostapd config file
    ret = wld_hostapd_cfgFile_update(pR->hostapd->cfgFile, (char*) interface, "ssid", ssid);
    ASSERT_TRUE(ret, SECDMN_ACTION_OK_NEED_RESTART, ME, "update the hostapd config failed for setting SSID");

    return SECDMN_ACTION_OK_NEED_UPDATE_BEACON;
}

/**
 * @brief update the secret key in the hostapd config file and context
 * This function updates wep_keyx, wpa_psk, wpa_passphrase and sae_password keys
 * For wep_keyx, a hostapd restart is needed to take into account the new value
 *
 * @param pAP accesspoint
 *
 * @return - SECDMN_ACTION_OK_NEED_RELOAD when the key is updated for the hostapd config file and the hostapd context (except wep_keyx)
 *         - SECDMN_ACTION_OK_NEED_RESTART when the key is updated for the hostapd config file but not for the hostapd context
 *         - Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecretKey(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");

    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    char* wpa_key_str = ((strlen(pAP->keyPassPhrase) + 1) == PSK_KEY_SIZE_LEN) ? "wpa_psk" : "wpa_passphrase";

    // set the key value
    switch(pAP->secModeEnabled) {
    case APMSI_WEP64:
    case APMSI_WEP128:
    case APMSI_WEP128IV:
    {
        // nothing to do here, restarting hostapd is needed in order to take into account the new wep_key
        action = SECDMN_ACTION_OK_NEED_RESTART;
    }
    break;
    case APMSI_WPA_P:
    case APMSI_WPA2_P:
    case APMSI_WPA_WPA2_P:
    {
        const char* key;
        const char* value;
        if(pAP->keyPassPhrase[0]) {
            key = wpa_key_str;
            value = pAP->keyPassPhrase;
        } else {
            key = "wpa_psk";
            value = pAP->preSharedKey;
        }
        bool ret = wld_ap_hostapd_setParamValue(pAP, key, value, key);
        ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting %s", key);

        // reload the hostapd interface, a hostapd restart is not needed
        action = SECDMN_ACTION_OK_NEED_RELOAD;
    }
    break;
    case APMSI_WPA2_WPA3_P:
    case APMSI_WPA3_P:
    {
        bool ret = wld_ap_hostapd_setParamValue(pAP, wpa_key_str, pAP->keyPassPhrase, wpa_key_str);
        ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting %s", wpa_key_str);
        if(!swl_str_matches(pAP->saePassphrase, "")) {
            ret = wld_ap_hostapd_setParamValue(pAP, "sae_password", pAP->saePassphrase, "sae_password");
            ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting sae_password");
        }
        // reload the hostapd interface, a hostapd restart is not needed
        action = SECDMN_ACTION_OK_NEED_RELOAD;
    }
    break;
    default:
        action = SECDMN_ACTION_ERROR;
        break;
    }

    // overwrite the current hostapd config file
    if(action != SECDMN_ACTION_ERROR) {
        wld_hostapd_cfgFile_createExt(pR);
    }

    return action;
}

/**
 * @brief update the the security mode in the hostapd
 * A hostapd restart is needed to take into account the new security mode
 *
 * @param pAP accesspoint
 * @return - SECDMN_ACTION_OK_NEED_RESTART when the security mode is updated in the hostapd config file
 *         - Otherwise SECDMN_ACTION_ERROR
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecurityMode(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");

    // overwrite the current hostapd config file
    wld_hostapd_cfgFile_createExt(pR);

    return SECDMN_ACTION_OK_NEED_RESTART;
}

/**
 * @brief kick a station from an AccessPoint
 * @param pAP accesspoint
 * @param mac address
 * @param reason: the reason to kick the station from the AccessPoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_ap_hostapd_kickStation(T_AccessPoint* pAP, swl_macBin_t* mac, swl_IEEE80211deauthReason_ne reason) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(mac, SWL_RC_INVALID_PARAM, ME, "NULL");

    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macChar_t macStr;
    swl_mac_binToChar(&macStr, mac);

    SAH_TRACEZ_INFO(ME, "%s: kickmac %s - (%s) reason %d", pR->Name, pAP->alias, macStr.cMac, reason);

    char cmd[256] = {'\0'};
    snprintf(cmd, sizeof(cmd), "DEAUTHENTICATE %s reason=%d", macStr.cMac, reason);


    bool ret = s_sendHostapdCommand(pAP, cmd, "kickStation");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: kickStation %s - (%s) reason %d failed", pR->Name, pAP->alias, macStr.cMac, reason);
    return SWL_RC_OK;
}

/**
 * @brief Transfer a station from a Bss to another
 *
 * @param pAP accesspoint
 * @param params parameters of BTM command
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_ap_hostapd_transferStation(T_AccessPoint* pAP, wld_transferStaArgs_t* params) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(params, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send tranfer from %s to %s of %s", pR->Name, pAP->alias, params->targetBssid, params->sta);

    char cmd[256] = {'\0'};
    snprintf(cmd, sizeof(cmd), "BSS_TM_REQ"
             " %s"
             " disassoc_timer=%d"
             " valid_int=%d"
             " neighbor=%s"
             ",%u,%d,%d,%d"   //<bssidInfo>,<operClass>,<channel>,<phyType>
             " pref=1 abridged=1 disassoc_imminent=1"
             " mbo=%d:%d:%d"  //mbo=<reason>:<reassoc_delay>:<cell_pref>
             , params->sta, params->disassoc, params->validity, params->targetBssid, params->bssidInfo, params->operClass, params->channel,
             swl_chanspec_operClassToPhyMode(params->operClass), params->transitionReason, params->disassoc ? 100 : 0, 0);

    bool ret = s_sendHostapdCommand(pAP, cmd, "bss transition management");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: btm from %s to %s of station %s failed", pR->Name, pAP->alias, params->targetBssid, params->sta);
    return SWL_RC_OK;
}

/**
 * @brief prevent exchange of frames between associated stations in the same VAP
 * @param pAP accesspoint
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setClientIsolation(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");

    // set the ap_isolate field in the hostapd context
    char val[2];
    snprintf(val, sizeof(val), "%d", pAP->clientIsolationEnable);
    bool ret = wld_ap_hostapd_setParamValue(pAP, "ap_isolate", val, "ap_isolate");
    ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "update the hostapd context failed for setting ap_isolate");

    // update the hostapd config file
    ret = wld_hostapd_cfgFile_update(pR->hostapd->cfgFile, pAP->alias, "ap_isolate", val);
    ASSERT_TRUE(ret, SECDMN_ACTION_OK_NEED_RESTART, ME, "update the hostapd config failed for setting ap_isolate");

    return SECDMN_ACTION_OK_NEED_UPDATE_BEACON;
}
/**
 * @brief start WPS session
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_startWps(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps start", pAP->alias);

    bool ret = s_sendHostapdCommand(pAP, WPS_START, "START_WPS");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send WPS start command", pAP->alias);
    return SWL_RC_OK;
}

swl_rc_ne wld_ap_hostapd_startWpsPin(T_AccessPoint* pAP, uint32_t pin) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps start pin %u", pAP->alias, pin);
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "WPS_PIN any %u 120", pin);

    bool ret = s_sendHostapdCommand(pAP, cmd, "START_WPS_PIN");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send WPS start command", pAP->alias);
    return SWL_RC_OK;
}

/**
 * @brief stop WPS session
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_stopWps(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps stop", pAP->alias);

    bool ret = s_sendHostapdCommand(pAP, WPS_CANCEL, "STOP_WPS");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send WPS stop command", pAP->alias);
    return SWL_RC_OK;
}

static swl_rc_ne s_opEntryAcl(T_AccessPoint* pAP, char* macStr, const char* acl, const char* op) {
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    char cmd[256] = {'\0'};
    if(macStr != NULL) {
        snprintf(cmd, sizeof(cmd), "%s %s %s", acl, op, macStr);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s", acl, op);
    }
    bool ret = s_sendHostapdCommand(pAP, cmd, cmd);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: %s failed", pR->Name, cmd);
    return SWL_RC_OK;
}
static swl_rc_ne s_addEntryAcceptAcl(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, "ACCEPT_ACL", "ADD_MAC");
}

static swl_rc_ne s_addEntryDenyAcl(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, "DENY_ACL", "ADD_MAC");
}
static swl_rc_ne s_delEntryAcceptAcl(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, "ACCEPT_ACL", "DEL_MAC");
}

static swl_rc_ne s_delEntryDenyAcl(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, "DENY_ACL", "DEL_MAC");
}

/**
 * @brief delete all MAC addresses on accept/deny access list depending
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_delAllMacFilteringEntries(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne ret = s_opEntryAcl(pAP, NULL, "ACCEPT_ACL", "CLEAR");
    ASSERT_TRUE(ret == SWL_RC_OK, SWL_RC_ERROR, ME, "accept_acl clearing failed");
    ret = s_opEntryAcl(pAP, NULL, "DENY_ACL", "CLEAR");
    ASSERT_TRUE(ret == SWL_RC_OK, SWL_RC_ERROR, ME, "deny_acl clearing failed");
    return SWL_RC_OK;
}

/**
 * @brief set the MAC Filetring mode and list
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_setMacFilteringList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    // clear accept and deny access lists
    wld_ap_hostapd_delAllMacFilteringEntries(pAP);

    if(pAP->MF_Mode == APMFM_WHITELIST) {
        bool ret = wld_ap_hostapd_setParamValue(pAP, "macaddr_acl", "1", "macaddr_acl");
        ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: error when setting macaddr_acl", pAP->alias);

        for(int i = 0; i < pAP->MF_EntryCount; i++) {
            if(!pAP->MF_TempBlacklistEnable
               || (!wldu_is_mac_in_list(pAP->MF_Entry[i], pAP->MF_Temp_Entry, pAP->MF_TempEntryCount))) {
                // add entry either if temporary blacklisting is disabled, or entry not in temp blaclist
                char macStr[ETHER_ADDR_STR_LEN] = {'\0'};
                SWL_MAC_BIN_TO_CHAR(macStr, pAP->MF_Entry[i]);
                s_addEntryAcceptAcl(pAP, macStr);
            }
        }
    } else {
        bool ret = wld_ap_hostapd_setParamValue(pAP, "macaddr_acl", "0", "macaddr_acl");
        ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: error when setting macaddr_acl", pAP->alias);

        if(pAP->MF_Mode == APMFM_BLACKLIST) {
            for(int i = 0; i < pAP->MF_EntryCount; i++) {
                char macStr[ETHER_ADDR_STR_LEN] = {'\0'};
                SWL_MAC_BIN_TO_CHAR(macStr, pAP->MF_Entry[i]);
                s_addEntryDenyAcl(pAP, macStr);
            }
        }
        if(pAP->MF_TempBlacklistEnable) {
            for(int i = 0; i < pAP->MF_TempEntryCount; i++) {
                char macStr[ETHER_ADDR_STR_LEN] = {'\0'};
                SWL_MAC_BIN_TO_CHAR(macStr, pAP->MF_Temp_Entry[i]);
                s_addEntryDenyAcl(pAP, macStr);
            }
        }

        //Note hostapd currently does not support probe filtering, as it will auto kick additions to list
    }
    return SWL_RC_OK;
}

/**
 * @brief add the MAC address on accept/deny access list depending on Mac Filetring mode value
 * Mac Filetring mode value: APMFM_OFF, APMFM_WHITELIST, APMFM_BLACKLIST
 * @param pAP accesspoint
 * @param macStr mac address
 */
swl_rc_ne wld_ap_hostapd_addMacFilteringEntry(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    switch(pAP->MF_Mode) {
    //All MAC-addresses are allowed
    case APMFM_OFF:
        // no need to add this mac address to accept/deny access list
        break;
    //Access is granted only for MAC-addresses occurring in the Entry table
    case APMFM_WHITELIST:
    {
        s_addEntryAcceptAcl(pAP, macStr);
    }
    break;
    //Access is granted for all MAC-addresses except for the ones occurring in the Entry table
    case APMFM_BLACKLIST:
    {
        s_addEntryDenyAcl(pAP, macStr);
    }
    break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: Unknown MacFiltering mode %d", pR->Name, pAP->MF_Mode);
        return SWL_RC_ERROR;
    }
    return SWL_RC_OK;
}

/**
 * @brief delete the MAC address on accept/deny access list depending on Mac Filetring mode value
 * Mac Filetring mode value: APMFM_OFF, APMFM_WHITELIST, APMFM_BLACKLIST
 * @param pAP accesspoint
 * @param macStr mac address
 */
swl_rc_ne wld_ap_hostapd_delMacFilteringEntry(T_AccessPoint* pAP, char* macStr) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    switch(pAP->MF_Mode) {
    //All MAC-addresses are allowed
    case APMFM_OFF:
        // no need to add this mac address to accept/deny access list
        break;
    //Access is granted only for MAC-addresses occurring in the Entry table
    case APMFM_WHITELIST:
    {
        s_delEntryAcceptAcl(pAP, macStr);
    }
    break;
    //Access is granted for all MAC-addresses except for the ones occurring in the Entry table
    case APMFM_BLACKLIST:
    {
        s_delEntryDenyAcl(pAP, macStr);
    }
    break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: Unknown MacFiltering mode %d", pR->Name, pAP->MF_Mode);
        return SWL_RC_ERROR;
    }
    return SWL_RC_OK;
}
