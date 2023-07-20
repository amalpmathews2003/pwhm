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
#include <arpa/inet.h>
#include "wld.h"
#include "wld_util.h"
#include "wld_hostapd_cfgFile.h"
#include "wld_hostapd_ap_api.h"
#include "wld_wpaCtrl_api.h"
#include "wld_accesspoint.h"
#include "swl/swl_hex.h"
#include "swl/swl_common_mac.h"

#define ME "hapdAP"

#define WPS_START  "WPS_PBC"
#define WPS_CANCEL "WPS_CANCEL"
#define NR_SIZE  36
typedef struct {
    char neighReport[NR_SIZE];
} wld_neighReport_t;
static void s_generateNeighReportField(swl_macChar_t bssid, uint32_t BssidInfo, uint8_t operClass, uint8_t channel, uint8_t phyType, wld_neighReport_t* result) {
    unsigned int MAC[8];
    unsigned char CMAC[SWL_MAC_BIN_LEN];

    memset(result->neighReport, 0, NR_SIZE);
    if(sscanf(bssid.cMac, "%x:%x:%x:%x:%x:%x",
              &MAC[0], &MAC[1], &MAC[2],
              &MAC[3], &MAC[4], &MAC[5]) == 6) {
        CMAC[0] = MAC[0];
        CMAC[1] = MAC[1];
        CMAC[2] = MAC[2];
        CMAC[3] = MAC[3];
        CMAC[4] = MAC[4];
        CMAC[5] = MAC[5];
        SAH_TRACEZ_INFO(ME, "%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x \n",
                        CMAC[0], CMAC[1], CMAC[2], CMAC[3], CMAC[4], CMAC[5]);
        snprintf(result->neighReport, 30, "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%08x%02x%02x%02x", CMAC[0], CMAC[1], CMAC[2], CMAC[3], CMAC[4], CMAC[5], BssidInfo, operClass, channel, phyType);
    }


}

bool s_sendHostapdCommand(T_AccessPoint* pAP, char* cmd, const char* reason) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), false, ME, "%s: wpactrl link not ready", pAP->alias);
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: send hostapd cmd %s for %s",
                    wld_wpaCtrlInterface_getName(pAP->wpaCtrlInterface), cmd, reason);
    return wld_wpaCtrl_sendCmdCheckResponse(pAP->wpaCtrlInterface, cmd, "OK");
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
 * @brief reload AP secret keys conf
 *
 * @param pAP accesspoint
 * @param reason the command caller
 * @return true when the RELOAD_WPA_PSK cmd is executed successfully. Otherwise false.
 */
bool wld_ap_hostapd_reloadSecKey(T_AccessPoint* pAP, const char* reason) {
    return s_sendHostapdCommand(pAP, "RELOAD_WPA_PSK", reason);
}

/**
 * @brief delete neighbor
 *
 * @param pAP accesspoint
 * @param pApNeighbor :Neighbor to be removed
 * @return true when the remove_neighbor cmd is executed successfully. Otherwise false.
 */
swl_rc_ne wld_ap_hostapd_removeNeighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor) {

    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pApNeighbor, false, ME, "NULL");
    ASSERTS_STR(pApNeighbor->ssid, false, ME, "empty ssid");
    char cmd[128];
    swl_macChar_t macStr;
    char hexSSSID[strlen(pApNeighbor->ssid) * 2 + 1 ];

    wldu_convMac2Str((unsigned char*) pApNeighbor->bssid, ETHER_ADDR_LEN, macStr.cMac, ETHER_ADDR_STR_LEN);

    swl_hex_fromBytes(hexSSSID, sizeof(hexSSSID), (uint8_t*) pApNeighbor->ssid, sizeof(pApNeighbor->ssid), 0);
    hexSSSID[strlen(pApNeighbor->ssid) * 2] = '\0';

    snprintf(cmd, sizeof(cmd), "REMOVE_NEIGHBOR %s ssid=%s", macStr.cMac, hexSSSID);
    SAH_TRACEZ_INFO(ME, "sending cmd : %s", cmd);
    bool ret = s_sendHostapdCommand(pAP, cmd, "remove_neighbor");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: remove_neighbor failed", pAP->alias);
    return SWL_RC_OK;
}

/**
 * @brief set neighbor
 *
 * @param pAP accesspoint
 * @param pApNeighbor :Neighbor to be added
 * @return true when the set_neighbor cmd is executed successfully. Otherwise false.
 */
swl_rc_ne wld_ap_hostapd_setNeighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor) {

    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pApNeighbor, false, ME, "NULL");
    ASSERTS_STR(pApNeighbor->ssid, false, ME, "empty ssid");
    char cmd[128];
    swl_macChar_t macStr;
    wld_neighReport_t nrResult;
    char hexSSSID[strlen(pApNeighbor->ssid) * 2 + 1 ];


    wldu_convMac2Str((unsigned char*) pApNeighbor->bssid, ETHER_ADDR_LEN, macStr.cMac, ETHER_ADDR_STR_LEN);

    swl_hex_fromBytes(hexSSSID, sizeof(hexSSSID), (uint8_t*) pApNeighbor->ssid, sizeof(pApNeighbor->ssid), 0);
    hexSSSID[strlen(pApNeighbor->ssid) * 2] = '\0';

    s_generateNeighReportField(macStr, pApNeighbor->information, pApNeighbor->operatingClass, pApNeighbor->channel, pApNeighbor->phyType, &nrResult);

    snprintf(cmd, sizeof(cmd), "SET_NEIGHBOR %s ssid=%s nr=%s %s %s %s", macStr.cMac, hexSSSID, nrResult.neighReport, "", "", "");
    SAH_TRACEZ_INFO(ME, "sending cmd : %s", cmd);

    bool ret = s_sendHostapdCommand(pAP, cmd, "set_neighbor");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: set_neighbor failed", pAP->alias);
    return SWL_RC_OK;
}

/**
 * @brief set a parameter value in hostapd context
 *
 * @param pAP accesspoint
 * @param reason the command caller
 * @return true when the SET cmd is executed successfully. Otherwise false.
 */
bool wld_ap_hostapd_setParamValue(T_AccessPoint* pAP, const char* field, const char* value, const char* reason) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_STR(field, false, ME, "empty key");
    ASSERTS_STR(value, false, ME, "empty value");
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

SWL_TABLE(sHapdCfgParamsActionMap,
          ARR(char* param; wld_secDmn_action_rc_ne action; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR(//params set and applied with hostapd restart
              {"wep_default_key", SECDMN_ACTION_OK_NEED_RESTART},
              {"wep_key0", SECDMN_ACTION_OK_NEED_RESTART},
              {"wep_key1", SECDMN_ACTION_OK_NEED_RESTART},
              {"wep_key2", SECDMN_ACTION_OK_NEED_RESTART},
              {"wep_key3", SECDMN_ACTION_OK_NEED_RESTART},
              {"wps_state", SECDMN_ACTION_OK_NEED_RESTART},
              //params set and applied with main iface toggle
              {"rrm_neighbor_report", SECDMN_ACTION_OK_NEED_TOGGLE},
              //params set and applied with global saved hostapd conf reloading
              {"wpa", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"wpa_pairwise", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"wpa_key_mgmt", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"ieee80211w", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"wpa_group_rekey", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"auth_server_addr", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"auth_server_port", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"own_ip_addr", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"nas_identifier", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"radius_auth_req_attr", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"auth_server_shared_secret", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"auth_server_default_session_timeout", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"owe_transition_ifname", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"transition_disable", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"mobility_domain", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"ft_over_ds", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"config_methods", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"uuid", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"multi_ap", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"multi_ap_backhaul_ssid", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"multi_ap_backhaul_wpa_psk", SECDMN_ACTION_OK_NEED_SIGHUP},
              {"multi_ap_backhaul_wpa_passphrase", SECDMN_ACTION_OK_NEED_SIGHUP},
              //params set and applied on bss with reload_wpa_psk and update_beacon
              {"ssid", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY},
              {"wpa_psk", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY},
              {"wpa_passphrase", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY},
              {"sae_password", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY},
              //params set and applied with update_beacon
              {"ignore_broadcast_ssid", SECDMN_ACTION_OK_NEED_UPDATE_BEACON},
              {"ap_isolate", SECDMN_ACTION_OK_NEED_UPDATE_BEACON},
              //params set and applied without any action
              {"max_num_sta", SECDMN_ACTION_OK_DONE},
              ));

static bool s_setParam(T_AccessPoint* pAP, const char* param, const char* value, wld_secDmn_action_rc_ne* pAction) {
    ASSERTS_NOT_NULL(param, false, ME, "NULL");
    bool ret = wld_ap_hostapd_setParamValue(pAP, param, value, param);
    wld_secDmn_action_rc_ne* pMappedAction = (wld_secDmn_action_rc_ne*) swl_table_getMatchingValue(&sHapdCfgParamsActionMap, 1, 0, param);
    if(pAP->status == APSTI_DISABLED) {
        W_SWL_SETPTR(pAction, SECDMN_ACTION_OK_DONE);
    } else if(pMappedAction != NULL) {
        //keep most critical action
        W_SWL_SETPTR(pAction, SWL_MAX(*pAction, *pMappedAction));
    } else if(ret) {
        /*
         * If param is successfully set but has no specific applying action,
         * then reload whole save hostapd conf
         */
        W_SWL_SETPTR(pAction, SECDMN_ACTION_OK_NEED_SIGHUP);
    }
    return ret;
}

static bool s_setChangedParam(T_AccessPoint* pAP, swl_mapChar_t* pCurrVapParams,
                              const char* param, const char* newValue, wld_secDmn_action_rc_ne* pAction) {
    ASSERTS_NOT_NULL(param, false, ME, "NULL");
    const char* oldValue = swl_mapChar_get(pCurrVapParams, (char*) param);
    ASSERTS_FALSE(swl_str_matches(oldValue, newValue), false, ME, "same value");
    return s_setParam(pAP, param, newValue, pAction);
}

static bool s_setChangedMultiParams(T_AccessPoint* pAP, swl_mapChar_t* pCurrVapParams, swl_mapChar_t* pNewVapParams,
                                    const char* params[], uint32_t nParams, wld_secDmn_action_rc_ne* pAction) {
    bool ret = false;
    for(uint32_t i = 0; i < nParams; i++) {
        ret |= s_setChangedParam(pAP, pCurrVapParams, params[i], swl_mapChar_get(pNewVapParams, (char*) params[i]), pAction);
    }
    return ret;
}

static bool s_setExistingParam(T_AccessPoint* pAP, swl_mapChar_t* pCurrVapParams,
                               const char* param, const char* newValue, wld_secDmn_action_rc_ne* pAction) {
    ASSERTS_NOT_NULL(param, false, ME, "NULL");
    const char* oldValue = swl_mapChar_get(pCurrVapParams, (char*) param);
    ASSERTS_FALSE(swl_str_isEmpty(oldValue), false, ME, "not existing");
    return s_setParam(pAP, param, newValue, pAction);
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
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    // set the ssid field in the hostapd context
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    bool ret = s_setParam(pAP, "ssid", ssid, &action);
    ASSERT_TRUE(ret, SECDMN_ACTION_ERROR, ME, "%s: failed for setting SSID", pAP->alias);
    wld_hostapd_config_t* config = NULL;
    wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    //hostapd requires backhaul_ssid included in double quotes
    char bhSsid[strlen(ssid) + 3];
    snprintf(bhSsid, sizeof(bhSsid), "\"%s\"", ssid);
    s_setExistingParam(pAP, pCurrVapParams, "multi_ap_backhaul_ssid", bhSsid, &action);
    if(!swl_str_matches(swl_mapChar_get(pCurrVapParams, "wps_state"), "0")) {
        action = SWL_MAX(action, SECDMN_ACTION_OK_NEED_SIGHUP);
    }
    wld_hostapd_deleteConfig(config);
    return action;
}

/**
 * @brief update the secret key in the hostapd runtime conf
 * This function updates wep_keyx, wpa_psk, wpa_passphrase and sae_password keys
 * For wep_keyx, a hostapd restart is needed to take into account the new value
 *
 * @param pAP accesspoint
 *
 * @return - > SECDMN_ACTION_OK_DONE when secret keys are set and can be applied
 *         - Otherwise SECDMN_ACTION_ERROR
 */
static wld_secDmn_action_rc_ne s_ap_hostapd_setSecretKeyExt(T_AccessPoint* pAP, swl_mapChar_t* pCurrVapParams, swl_mapChar_t* pNewVapParams) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    // set the key value
    switch(pAP->secModeEnabled) {
    case SWL_SECURITY_APMODE_WEP64:
    case SWL_SECURITY_APMODE_WEP128:
    case SWL_SECURITY_APMODE_WEP128IV:
    {
        const char* secParams[] = {"wep_key0", "wep_key1", "wep_key2", "wep_key3", };
        //in wep mode, we need to restart hostapd to apply new wep_keys
        s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                                secParams, SWL_ARRAY_SIZE(secParams), &action);
    }
    break;
    case SWL_SECURITY_APMODE_WPA_P:
    case SWL_SECURITY_APMODE_WPA2_P:
    case SWL_SECURITY_APMODE_WPA_WPA2_P:
    {
        const char* secParams[] = {
            "wpa_psk", "wpa_passphrase",
            "multi_ap_backhaul_wpa_psk", "multi_ap_backhaul_wpa_passphrase",
        };
        //in wpa mode, we need to reload wpa_psk params per interface
        s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                                secParams, SWL_ARRAY_SIZE(secParams), &action);
    }
    break;
    case SWL_SECURITY_APMODE_WPA2_WPA3_P:
    case SWL_SECURITY_APMODE_WPA3_P:
    {
        const char* secParams[] = {
            "wpa_psk", "wpa_passphrase", "sae_password",
            "multi_ap_backhaul_wpa_psk", "multi_ap_backhaul_wpa_passphrase",
        };
        //in wpa mode, we just need to reload wpa_psk params per interface
        s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                                secParams, SWL_ARRAY_SIZE(secParams), &action);
    }
    break;
    case SWL_SECURITY_APMODE_WPA_E:
    case SWL_SECURITY_APMODE_WPA2_E:
    case SWL_SECURITY_APMODE_WPA_WPA2_E:
    case SWL_SECURITY_APMODE_WPA3_E:
    case SWL_SECURITY_APMODE_WPA2_WPA3_E:
    {
        const char* secParams[] = {"auth_server_shared_secret", };
        //in wpa eap, we need to refresh whole hostapd config from file
        s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                                secParams, SWL_ARRAY_SIZE(secParams), &action);
    }
    break;
    case SWL_SECURITY_APMODE_NONE:
    default:
        break;
    }
    return action;
}

wld_secDmn_action_rc_ne wld_ap_hostapd_setSecretKey(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    swl_mapChar_t newVapParams;
    swl_mapChar_t* pNewVapParams = &newVapParams;
    swl_mapChar_init(pNewVapParams);
    wld_hostapd_cfgFile_setVapConfig(pAP, pNewVapParams, (swl_mapChar_t*) NULL);
    wld_secDmn_action_rc_ne action = s_ap_hostapd_setSecretKeyExt(pAP, pCurrVapParams, pNewVapParams);
    swl_mapChar_cleanup(pNewVapParams);
    wld_hostapd_deleteConfig(config);
    return action;
}

/**
 * @brief set the the security mode in the hostapd runtime conf
 * A hostapd restart or refresh (SIGHUP) may be needed to apply the new security mode
 *
 * @param pAP accesspoint
 * @return - > SECDMN_ACTION_OK_DONE when the security mode is set and can be applied
 *         - Otherwise SECDMN_ACTION_ERROR
 */
static wld_secDmn_action_rc_ne s_ap_hostapd_setSecurityModeExt(T_AccessPoint* pAP, swl_mapChar_t* pCurrVapParams, swl_mapChar_t* pNewVapParams) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    //when switching into or out of wep mode, we need to toggle hostapd to apply security mode
    const char* secParams[] = {"wpa", "wpa_pairwise", "wpa_key_mgmt", "wep_default_key", };
    s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                            secParams, SWL_ARRAY_SIZE(secParams), &action);
    return action;
}

wld_secDmn_action_rc_ne wld_ap_hostapd_setSecurityMode(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    swl_mapChar_t newVapParams;
    swl_mapChar_t* pNewVapParams = &newVapParams;
    swl_mapChar_init(pNewVapParams);
    wld_hostapd_cfgFile_setVapConfig(pAP, pNewVapParams, (swl_mapChar_t*) NULL);
    wld_secDmn_action_rc_ne action = s_ap_hostapd_setSecurityModeExt(pAP, pCurrVapParams, pNewVapParams);
    swl_mapChar_cleanup(pNewVapParams);
    wld_hostapd_deleteConfig(config);
    return action;
}

/**
 * @brief set AP security parameters
 *
 * @param pAP accesspoint
 *
 * @return >= SECDMN_ACTION_OK_DONE when AP sec config is set. Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecParams(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    swl_mapChar_t newVapParams;
    swl_mapChar_t* pNewVapParams = &newVapParams;
    swl_mapChar_init(pNewVapParams);
    wld_hostapd_cfgFile_setVapConfig(pAP, pNewVapParams, (swl_mapChar_t*) NULL);
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;

    action = SWL_MAX(action, s_ap_hostapd_setSecurityModeExt(pAP, pCurrVapParams, pNewVapParams));
    action = SWL_MAX(action, s_ap_hostapd_setSecretKeyExt(pAP, pCurrVapParams, pNewVapParams));

    //set other changed security params
    const char* secParams[] = {
        "ieee80211w", "wpa_group_rekey",
        "auth_server_addr", "auth_server_port", "own_ip_addr", "nas_identifier",
        "radius_auth_req_attr", "auth_server_default_session_timeout", "radius_request_cui",
        "owe_transition_ifname",
        "transition_disable",
        "mobility_domain",
        "wps_state",
    };
    s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                            secParams, SWL_ARRAY_SIZE(secParams), &action);
    swl_mapChar_cleanup(pNewVapParams);
    wld_hostapd_deleteConfig(config);
    return action;
}


/**
 * @brief set the SSID Advertisement in the hostapd
 *
 * @param pAP accesspoint
 * @param enable if true the SSID Advertisement is enabled. Otherwise false
 * @return - > SECDMN_ACTION_OK_DONE when the ssid advertisement is set and can be applied
 *         - Otherwise SECDMN_ACTION_ERROR
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setSSIDAdvertisement(T_AccessPoint* pAP, bool enable) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    s_setChangedParam(pAP, pCurrVapParams, "ignore_broadcast_ssid", (enable ? "0" : "2"), &action);
    wld_hostapd_deleteConfig(config);
    return action;
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
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    s_setChangedParam(pAP, pCurrVapParams, "ap_isolate", (pAP->clientIsolationEnable ? "0" : "1"), &action);
    wld_hostapd_deleteConfig(config);
    return action;
}

/**
 * @brief set AP common parameters (non-security)
 *
 * @param pAP accesspoint
 *
 * @return >= SECDMN_ACTION_OK_DONE when AP config is set. Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setNoSecParams(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SECDMN_ACTION_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pR->hostapd, SECDMN_ACTION_ERROR, ME, "NULL");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_loadConfig(&config, pR->hostapd->cfgFile);
    ASSERTI_TRUE(ret, SECDMN_ACTION_ERROR, ME, "no saved config");
    swl_mapChar_t* pCurrVapParams = wld_hostapd_getConfigMap(config, pAP->alias);
    swl_mapChar_t newVapParams;
    swl_mapChar_t* pNewVapParams = &newVapParams;
    swl_mapChar_init(pNewVapParams);
    wld_hostapd_cfgFile_setVapConfig(pAP, pNewVapParams, (swl_mapChar_t*) NULL);
    wld_secDmn_action_rc_ne action = SECDMN_ACTION_OK_DONE;
    const char* params[] = {
        "max_num_sta", "ap_isolate", "ignore_broadcast_ssid",
        "ft_over_ds", "multi_ap",
        "wps_state", "config_methods", "uuid",
        "rrm_neighbor_report",
    };
    s_setChangedMultiParams(pAP, pCurrVapParams, pNewVapParams,
                            params, SWL_ARRAY_SIZE(params), &action);
    swl_mapChar_cleanup(pNewVapParams);
    wld_hostapd_deleteConfig(config);
    return action;
}

/**
 * @brief save dynamic conf for enabling/disabling hostapd vap (main or secondary bss)
 *
 * @param pAP accesspoint
 * @param enable the VAP enabling flag
 * @return - SECDMN_ACTION_OK_DONE when VAP ena/disabling conf is set successfully
 *         - Otherwise SECDMN_ACTION_ERROR
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_setEnableVap(T_AccessPoint* pAP, bool enable) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: save enable vap %d", pAP->alias, enable);
    bool ret = (wld_ap_hostapd_setParamValue(pAP, "start_disabled", (enable ? "0" : "1"), "bcastOnStart") &&
                wld_ap_hostapd_setParamValue(pAP, "send_probe_response", (enable ? "1" : "0"), "sndPrbResp"));
    ASSERTS_TRUE(ret, SECDMN_ACTION_ERROR, ME, "NULL");
    return SECDMN_ACTION_OK_DONE;
}

/**
 * @brief enable/disable hostapd vap (main or secondary bss)
 *
 * @param pAP accesspoint
 * @param enable the VAP enabling flag
 * @return - SECDMN_ACTION_OK_DONE when VAP is ena/disable successfully
 *         - SECDMN_ACTION_OK_NEED_TOGGLE when status application requires hostapd main iface toggle
 *         - Otherwise SECDMN_ACTION_ERROR
 */
wld_secDmn_action_rc_ne wld_ap_hostapd_enableVap(T_AccessPoint* pAP, bool enable) {
    ASSERTS_NOT_NULL(pAP, SECDMN_ACTION_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: apply enable vap %d", pAP->alias, enable);
    bool ret = false;
    if(!enable) {
        ret = wld_ap_hostapd_sendCommand(pAP, "STOP_AP", "disableAp");
    } else {
        ret = wld_ap_hostapd_updateBeacon(pAP, "syncAp");
    }
    ASSERTS_TRUE(ret, SECDMN_ACTION_OK_NEED_TOGGLE, ME, "NULL");
    return SECDMN_ACTION_OK_DONE;
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
 * @brief deauthenticate all AP's stations
 * (default reason AUTH_NO_LONGER_VALID)
 *
 * @param pAP accesspoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_ap_hostapd_deauthAllStations(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(pAP->ActiveAssociatedDeviceNumberOfEntries > 0, SWL_RC_OK, ME, "%s: AP has not active stations", pAP->alias);
    return wld_ap_hostapd_kickStation(pAP, (swl_macBin_t*) &g_swl_macBin_bCast, SWL_IEEE80211_DEAUTH_REASON_AUTH_NO_LONGER_VALID);
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

    SAH_TRACEZ_INFO(ME, "%s: Send tranfer from %s to %s of %s", pR->Name, pAP->alias, params->targetBssid.cMac, params->sta.cMac);

    char cmd[256] = {'\0'};
    swl_str_catFormat(cmd, sizeof(cmd), "BSS_TM_REQ"
                      " %s"
                      " pref=%u abridged=%u"
                      , params->sta.cMac,
                      SWL_BIT_IS_SET(params->reqModeMask, SWL_IEEE802_BTM_REQ_MODE_PREF_LIST_INCL),
                      SWL_BIT_IS_SET(params->reqModeMask, SWL_IEEE802_BTM_REQ_MODE_ABRIDGED));
    if(params->transitionReason >= 0) {
        swl_str_catFormat(cmd, sizeof(cmd),
                          " mbo=%d:%d:%d"  //mbo=<reason>:<reassoc_delay>:<cell_pref>
                          , params->transitionReason, (params->disassoc > 0) ? 100 : 0, 0);
    }
    if(params->disassoc > 0) {
        swl_str_catFormat(cmd, sizeof(cmd),
                          " disassoc_imminent=%u"
                          " disassoc_timer=%d"
                          , SWL_BIT_IS_SET(params->reqModeMask, SWL_IEEE802_BTM_REQ_MODE_DISASSOC_IMMINENT),
                          params->disassoc);
    }
    if(params->validity > 0) {
        //80211: validity interval 0 is a reserved value:
        //in this case, let hostapd/driver set the default number of beacon transmission times (TBTTs)
        //until the BSS transition candidate list is no longer valid
        swl_str_catFormat(cmd, sizeof(cmd),
                          " valid_int=%d"
                          , params->validity);
    }
    if(swl_mac_charIsValidStaMac(&params->targetBssid)) {
        swl_str_catFormat(cmd, sizeof(cmd),
                          " neighbor=%s"
                          ",%u,%d,%d,%d"   //<bssidInfo>,<operClass>,<channel>,<phyType>
                          , params->targetBssid.cMac
                          , params->bssidInfo, params->operClass, params->channel, swl_chanspec_operClassToPhyMode(params->operClass));
        if(SWL_BIT_IS_SET(params->reqModeMask, SWL_IEEE802_BTM_REQ_MODE_PREF_LIST_INCL)) {
            //add highest preference for the bss candidate: Tlv: Len:3,candidate:1,pref:255
            swl_str_catFormat(cmd, sizeof(cmd), ",0301ff");
        }
    }

    bool ret = s_sendHostapdCommand(pAP, cmd, "bss transition management");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: btm from %s to %s of station %s failed", pR->Name, pAP->alias, params->targetBssid.cMac, params->sta.cMac);
    return SWL_RC_OK;
}

/**
 * @brief start WPS session
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_startWps(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps start", pAP->alias);

    bool ret = s_sendHostapdCommand(pAP, WPS_START, "START_WPS");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send WPS start command", pAP->alias);
    return SWL_RC_OK;
}

/*
 * @brief start a WPS client PIN session, to pair against a remote device.
 *
 * @param pAP accesspoint
 * @param pin numerical string of 4 or 8 digits (Cf. WPS 2.x)
 *            that can even start with sequence of zero digit.
 * @param timeout Time (in seconds) when the PIN will be invalidated; 0 = no timeout
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_hostapd_startWpsPin(T_AccessPoint* pAP, const char* pin, uint32_t timeout) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps start pin %s", pAP->alias, pin);
    char cmd[64] = {0};
    //WPS client PIN started with a default wps session walk time
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PIN any %s %d", pin, timeout);

    bool ret = s_sendHostapdCommand(pAP, cmd, "START_WPS_PIN");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send WPS start command", pAP->alias);
    return SWL_RC_OK;
}

/*
 * @brief start a WPS AP Pin session, allowing a remote device to pair.
 *
 * @param pAP accesspoint
 * @param pin numerical string of 4 or 8 digits (Cf. WPS 2.x)
 *            that can even start with sequence of zero digit
 * @param timeout Time (in seconds) when the AP PIN will be disabled; 0 = no timeout
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_hostapd_setWpsApPin(T_AccessPoint* pAP, const char* pin, uint32_t timeout) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Send wps set AP pin %s", pAP->alias, pin);
    /*
     * when WPS AP PIN is set, peer stations can be WPS PIN paired,
     * and meanwhile WPS-AP-SETUP is unlocked until WPS-AP-PIN-DISABLED
     */
    char cmd[64] = {0};
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_AP_PIN set %s %d", pin, timeout);
    //when exec is successful, the applied PIN is returned in reply
    bool ret = wld_wpaCtrl_sendCmdCheckResponse(pAP->wpaCtrlInterface, cmd, (char*) pin);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to execute WPS_AP_PIN command", pAP->alias);
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
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");

    char cmd[256] = {'\0'};
    if(macStr != NULL) {
        snprintf(cmd, sizeof(cmd), "%s %s %s", acl, op, macStr);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s", acl, op);
    }
    bool ret = wld_wpaCtrl_sendCmdCheckResponse(pAP->wpaCtrlInterface, cmd, "OK");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: %s failed", pR->Name, cmd);
    return SWL_RC_OK;
}

static swl_rc_ne s_addEntryAcl(T_AccessPoint* pAP, char* macStr, bool useAcceptAcl) {
    ASSERTS_STR(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, (useAcceptAcl ? "ACCEPT_ACL" : "DENY_ACL"), "ADD_MAC");
}

static swl_rc_ne s_delEntryAcl(T_AccessPoint* pAP, char* macStr, bool useAcceptAcl) {
    ASSERTS_STR(macStr, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_opEntryAcl(pAP, macStr, (useAcceptAcl ? "ACCEPT_ACL" : "DENY_ACL"), "DEL_MAC");
}

static swl_rc_ne s_clearAcl(T_AccessPoint* pAP, bool useAcceptAcl) {
    return s_opEntryAcl(pAP, NULL, (useAcceptAcl ? "ACCEPT_ACL" : "DENY_ACL"), "CLEAR");
}

/**
 * @brief delete all MAC addresses on accept/deny access list depending
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_delAllMacFilteringEntries(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne ret = s_clearAcl(pAP, true);
    ASSERT_EQUALS(ret, SWL_RC_OK, ret, ME, "%s: accept_acl clearing failed", pAP->alias);
    ret = s_clearAcl(pAP, false);
    ASSERT_EQUALS(ret, SWL_RC_OK, ret, ME, "%s: deny_acl clearing failed", pAP->alias);
    return ret;
}

/**
 * @brief set the MAC Filetring mode and list
 * @param pAP accesspoint
 */
swl_rc_ne wld_ap_hostapd_setMacFilteringList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), SWL_RC_INVALID_STATE, ME, "%s: wpaCtrl disconnected", pAP->alias);

    wld_banlist_t banList;
    //hostapd/nl80211 do not support probe filtering, so assume PF entries in dynamic MF
    wld_apMacFilter_getBanList(pAP, &banList, true);

    bool useAcceptAcl = (pAP->MF_Mode == APMFM_WHITELIST);
    wld_ap_hostapd_delAllMacFilteringEntries(pAP);

    bool ret = wld_ap_hostapd_setParamValue(pAP, "macaddr_acl", (useAcceptAcl ? "1" : "0"), "macaddr_acl");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: error when setting macaddr_acl", pAP->alias);

    for(uint32_t i = 0; i < banList.staToBan; i++) {
        s_addEntryAcl(pAP, swl_typeMacBin_toBuf32Ref(&banList.banList[i]).buf, useAcceptAcl);
    }

    for(uint32_t i = 0; i < banList.staToKick; i++) {
        swl_IEEE80211deauthReason_ne reason = SWL_IEEE80211_DEAUTH_REASON_UNSPECIFIED_QOS;
        if(swl_typeMacBin_arrayContainsRef((swl_macBin_t*) pAP->MF_Entry, pAP->MF_EntryCount, &banList.kickList[i])) {
            reason = SWL_IEEE80211_DEAUTH_REASON_AUTH_NO_LONGER_VALID;
        }
        wld_ap_hostapd_kickStation(pAP, &banList.kickList[i], reason);
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
    swl_rc_ne rc = SWL_RC_OK;
    switch(pAP->MF_Mode) {
    //All MAC-addresses are allowed
    case APMFM_OFF:
        // no need to add this mac address to accept/deny access list
        break;
    //Access is granted only for MAC-addresses occurring in the Entry table
    case APMFM_WHITELIST:
    {
        ASSERTI_STR(macStr, SWL_RC_INVALID_PARAM, ME, "Empty");
        rc = s_addEntryAcl(pAP, macStr, true);
    }
    break;
    //Access is granted for all MAC-addresses except for the ones occurring in the Entry table
    case APMFM_BLACKLIST:
    {
        ASSERTI_STR(macStr, SWL_RC_INVALID_PARAM, ME, "Empty");
        rc = s_addEntryAcl(pAP, macStr, false);
    }
    break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: Unknown MacFiltering mode %d", pR->Name, pAP->MF_Mode);
        return SWL_RC_ERROR;
    }
    return rc;
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
    swl_rc_ne rc = SWL_RC_OK;
    switch(pAP->MF_Mode) {
    //All MAC-addresses are allowed
    case APMFM_OFF:
        // no need to add this mac address to accept/deny access list
        break;
    //Access is granted only for MAC-addresses occurring in the Entry table
    case APMFM_WHITELIST:
    {
        ASSERTI_STR(macStr, SWL_RC_INVALID_PARAM, ME, "Empty");
        rc = s_delEntryAcl(pAP, macStr, true);
    }
    break;
    //Access is granted for all MAC-addresses except for the ones occurring in the Entry table
    case APMFM_BLACKLIST:
    {
        ASSERTI_STR(macStr, SWL_RC_INVALID_PARAM, ME, "Empty");
        rc = s_delEntryAcl(pAP, macStr, false);
    }
    break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: Unknown MacFiltering mode %d", pR->Name, pAP->MF_Mode);
        return SWL_RC_ERROR;
    }
    return rc;
}

/* Ref. WPA3_Specification_v3.0 */
SWL_TABLE(sAkmSuiteSelectorMap,
          ARR(char* akmSuiteSelectorStr; swl_security_apMode_e secMode; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"00-0f-ac-1", SWL_SECURITY_APMODE_WPA2_E},      // EAP (SHA-1)
              {"00-0f-ac-2", SWL_SECURITY_APMODE_WPA2_P},      // PSK (SHA1)
              {"00-0f-ac-3", SWL_SECURITY_APMODE_WPA2_P},      // FT-EAP (SHA256)
              {"00-0f-ac-4", SWL_SECURITY_APMODE_WPA2_P},      // FT-PSK (SHA1) (11r)
              {"00-0f-ac-5", SWL_SECURITY_APMODE_WPA3_E},      // EAP (SHA-256)
              {"00-0f-ac-6", SWL_SECURITY_APMODE_WPA2_WPA3_P}, // PSK (SHA256)
              {"00-0f-ac-8", SWL_SECURITY_APMODE_WPA3_P},      // SAE (SHA256)
              {"00-0f-ac-9", SWL_SECURITY_APMODE_WPA3_P},      // FT-SAE (SHA256) (11r)
              ));
swl_rc_ne wld_ap_hostapd_getStaInfo(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAD, SWL_RC_INVALID_PARAM, ME, "NULL");
    // when failing to get sta info from hostpad, consider security mode unknown
    if(!swl_security_isApModeValid(pAD->assocCaps.currentSecurity)) {
        pAD->assocCaps.currentSecurity = SWL_SECURITY_APMODE_UNKNOWN;
    }
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    char buff[WLD_L_BUF] = {0};
    snprintf(buff, sizeof(buff), "STA %.17s", pAD->Name);
    bool ret = wld_wpaCtrl_sendCmdSynced(pAP->wpaCtrlInterface, buff, buff, sizeof(buff) - 1);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: Fail sta cmd: %s : ret %u", pAP->alias, buff, ret);
    ASSERTI_TRUE(swl_str_nmatchesIgnoreCase(buff, pAD->Name, strlen(pAD->Name)), SWL_RC_ERROR,
                 ME, "%s: wrong sta %s info: received(%s)",
                 pAP->alias, pAD->Name, buff);

    char valStr[WLD_M_BUF] = {0};
    int32_t val = 0;

    //Eg: flags=[AUTH][ASSOC][AUTHORIZED][WMM][MFP][HT][HE]
    if(wld_wpaCtrl_getValueStr(buff, "flags", valStr, sizeof(valStr)) > 0) {
        pAD->seen = (strstr(valStr, "[ASSOC]") != NULL);
        /*
         * when station is connected and authenticated, then initialize security with mode enabled on AP
         * as it is the only way to detect Open and WEP modes.
         * Other WPA modes can be detected below. */
        if(strstr(valStr, "[AUTHORIZED]")) {
            pAD->assocCaps.currentSecurity = pAP->secModeEnabled;
        }
    }

    if(wld_wpaCtrl_getValueIntExt(buff, "wpa", &val)) {
        switch(val) {
        case 1: pAD->assocCaps.currentSecurity = SWL_SECURITY_APMODE_WPA_P; break;  // WPA_VERSION_WPA = 1: WPA / IEEE 802.11i/D3.0
        case 2: pAD->assocCaps.currentSecurity = SWL_SECURITY_APMODE_WPA2_P; break; // WPA_VERSION_WPA2 = 2: WPA2 / IEEE 802.11i
        default: break;                                                             // WPA_VERSION_NO_WPA = 0 : WPA not used => Open or WEP or EAP
        }
    }
    // if WPA not used, then sec mode may be Open, WEP, EAP ...
    // even with wpa3, we need  refine secMode using the selected AuthenticationKeyManagement (AKM) suite
    // Eg: AKMSuiteSelector=00-0f-ac-8
    if(wld_wpaCtrl_getValueStr(buff, "AKMSuiteSelector", valStr, sizeof(valStr)) > 0) {
        swl_security_apMode_e* pCurrSec = (swl_security_apMode_e*) swl_table_getMatchingValue(&sAkmSuiteSelectorMap, 1, 0, valStr);
        if(pCurrSec) {
            pAD->assocCaps.currentSecurity = *pCurrSec;
        }
    }
    return SWL_RC_OK;
}

/**
 * @brief send RRM beacon Request
 * @param pAP accesspoint
 * @param sta mac address
 * @param operClass (optional) class to scan
 * @param channel (optional) channel to scan
 * @param timeout (optional) time to wait in milliseconds
 * @param bssid (optional) bssid argument
 * @param ssid (optional) ssid
 */
swl_rc_ne wld_ap_hostapd_requestRRMReport(T_AccessPoint* pAP, const swl_macChar_t* sta, uint8_t reqMode, uint8_t operClass, swl_channel_t channel, bool addNeighbor,
                                          uint16_t randomInterval, uint16_t measurementDuration, uint8_t measurementMode, const swl_macChar_t* bssid, const char* ssid) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(sta, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pR = pAP->pRadio;
    ASSERTS_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");
    char cmd[256] = {'\0'};
    swl_macBin_t bMac;
    SWL_MAC_CHAR_TO_BIN(&bMac, bssid);
    snprintf(cmd, sizeof(cmd), "REQ_BEACON %s "
             "req_mode=%.2x "
             "%.2x"
             "%.2x"
             "%.4x"
             "%.4x"
             "%.2x"
             "%.2x%.2x%.2x%.2x%.2x%.2x",
             sta->cMac, reqMode, operClass, channel, htons(randomInterval), htons(measurementDuration), measurementMode, MAC_PRINT_ARG(bMac.bMac));

    /* add SSID subelement. */
    if(ssid != NULL) {
        size_t ssidLen = swl_str_len(ssid);
        swl_str_catFormat(cmd, sizeof(cmd), "00%.2x", (unsigned int) ssidLen);
        for(size_t i = 0; i < ssidLen; i++) {
            bool ok = swl_str_catFormat(cmd, sizeof(cmd), "%.2x", ssid[i]);
            ASSERT_TRUE(ok, SWL_RC_ERROR, ME, "adding SSID subelement failed");
        }
    }

    /* add Reporting Detail subelement */
    bool ok = swl_str_catFormat(cmd, sizeof(cmd), "020100");
    ASSERT_TRUE(ok, SWL_RC_ERROR, ME, "adding Reporting Detail subelement failed");

    /* add AP Channel Report subelements. */
    if(addNeighbor) {
        T_Radio* pRad = pAP->pRadio;
        if(channel != pRad->channel) {
            swl_freqBandExt_e freqBand = swl_chanspec_operClassToFreq(pRad->operatingClass);
            swl_operatingClass_t operClass = swl_chanspec_getOperClassDirect(pRad->channel, freqBand, SWL_BW_20MHZ);
            ok = swl_str_catFormat(cmd, sizeof(cmd), "3302%.2x%.2x", operClass, pRad->channel);
            ASSERT_TRUE(ok, SWL_RC_ERROR, ME, "adding AP Channel Report subelement failed");
        }

        amxc_llist_for_each(it, &pAP->neighbours) {
            T_ApNeighbour* neigh = amxc_llist_it_get_data(it, T_ApNeighbour, it);
            if(channel != neigh->channel) {
                swl_freqBandExt_e freqBand = swl_chanspec_operClassToFreq(neigh->operatingClass);
                swl_operatingClass_t operClass = swl_chanspec_getOperClassDirect(neigh->channel, freqBand, SWL_BW_20MHZ);
                ok = swl_str_catFormat(cmd, sizeof(cmd), "3302%.2x%.2x", operClass, neigh->channel);
                ASSERT_TRUE(ok, SWL_RC_ERROR, ME, "adding AP Channel Report subelement failed");
            }
        }
    }

    char reply[8] = {0};
    bool ret = wld_wpaCtrl_sendCmdSynced(pAP->wpaCtrlInterface, cmd, reply, sizeof(reply));
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: Fail to send %s : ret %u", pAP->alias, cmd, ret);
    int32_t token;
    ok = swl_typeInt32_fromChar(&token, reply);
    ASSERT_TRUE(ok && (token >= 0), SWL_RC_ERROR, ME, "%s: Bad response %s : token %u reply(%s)", pAP->alias, cmd, token, reply);
    return SWL_RC_OK;
}
