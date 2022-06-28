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
#include "wld_accesspoint.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "swl/swl_hex.h"
#include "wld_hostapd_cfgManager.h"
#include "wld_hostapd_cfgFile.h"

#define ME "hapdCfg"

const char* wld_hostapd_wpsConfigMethods[] = { "usba", "ethernet", "label", "display", "ext_nfc_token", "int_nfc_token",
    "nfc_interface", "push_button", "keypad", "virtual_display", "physical_display", "virtual_push_button",
    "physical_push_button", "PIN",
    0};


/**
 * @brief set the radio parameters
 *
 * @param pRad a radio 2.4/5/6GHz
 * @param cfgF a pointer to the configuration file of the hostapd
 *
 * @return void
 */
static void s_setRadioConfig(T_Radio* pRad, wld_hostapd_config_t* config) {
    char buffer[256] = {0};

    T_AccessPoint* firstVap = wld_rad_getFirstVap(pRad);
    swl_mapChar_t* interfaceConfigMap = wld_hostapd_getConfigMap(config, firstVap->alias);
    swl_mapChar_add(interfaceConfigMap, "hw_mode", pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ ? "g" : "a");
    snprintf(buffer, sizeof(buffer), "%d", pRad->channel);
    swl_mapChar_add(interfaceConfigMap, "channel", buffer);
    swl_mapChar_add(interfaceConfigMap, "country_code", pRad->regulatoryDomain);
    swl_mapChar_add(interfaceConfigMap, "ieee80211d", "1");
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%d", pRad->IEEE80211hSupported && pRad->setRadio80211hEnable);
    swl_mapChar_add(interfaceConfigMap, "ieee80211h", buffer);
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%d", pRad->beaconPeriod);
    swl_mapChar_add(interfaceConfigMap, "beacon_int", buffer);

    if(pRad->supportedStandards & M_SWL_RADSTD_N) {
        swl_mapChar_add(interfaceConfigMap, "ieee80211n", "1");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s", "[HT40+][LDPC][SHORT-GI-20][SHORT-GI-40][TX-STBC][RX-STBC1][MAX-AMSDU-7935][DSSS_CCK-40]");
        swl_mapChar_add(interfaceConfigMap, "ht_capab", buffer);
    }
    if(pRad->supportedStandards & M_SWL_RADSTD_AC) {
        swl_mapChar_add(interfaceConfigMap, "ieee80211ac", "1");

        if((pRad->runningChannelBandwidth == SWL_BW_20MHZ) ||
           (pRad->runningChannelBandwidth == SWL_BW_40MHZ)) {

            swl_mapChar_add(interfaceConfigMap, "vht_oper_chwidth", "0");

        } else if(pRad->runningChannelBandwidth == SWL_BW_80MHZ) {
            swl_mapChar_add(interfaceConfigMap, "vht_oper_chwidth", "1");

            swl_chanspec_t chanspec = {
                .channel = pRad->channel,
                .bandwidth = pRad->runningChannelBandwidth,
                .band = pRad->operatingFrequencyBand
            };

            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d", swl_chanspec_getCentreChannel(&chanspec));
            swl_mapChar_add(interfaceConfigMap, "vht_oper_centr_freq_seg0_idx", buffer);
        }

        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s", "[RXLDPC][SHORT-GI-80][TX-STBC-2BY1][SU-BEAMFORMER][SU-BEAMFORMEE][MU-BEAMFORMER][MU-BEAMFORMEE][RX-ANTENNA-PATTERN][TX-ANTENNA-PATTERN][RX-STBC-1][MAX-MPDU-11454][MAX-A-MPDU-LEN-EXP7]");
        swl_mapChar_add(interfaceConfigMap, "vht_capab", buffer);
    }
}

/**
 * @brief set the IEEE80211r parameters of a vap
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
static void s_setVapIeee80211rConfig(T_AccessPoint* pAP, wld_hostapd_config_t* config) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(config, , ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    ASSERTS_TRUE(pAP->IEEE80211rEnable, , ME, "11r disabled");
    swl_mapChar_t* vapConfigMap = wld_hostapd_getConfigMap(config, pAP->alias);
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    char buffer[128] = {0};

    uint16_t mdid = ((((pAP->mobilityDomain) >> 8) & 0x00FF) | (((pAP->mobilityDomain) << 8) & 0xFF00));

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%u", pAP->IEEE80211rFTOverDSEnable);
    swl_mapChar_add(vapConfigMap, "ft_over_ds", buffer);

    swl_mapChar_add(vapConfigMap, "ft_psk_generate_local", "1");

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%04X", mdid);
    swl_mapChar_add(vapConfigMap, "mobility_domain", buffer);

    swl_mapChar_add(vapConfigMap, "nas_identifier", pAP->NASIdentifier);

    swl_macChar_t bssidWSep;
    swl_hex_fromBytes(bssidWSep.cMac, SWL_MAC_CHAR_LEN, pSSID->BSSID, ETHER_ADDR_LEN, true);
    swl_mapChar_add(vapConfigMap, "r1_key_holder", bssidWSep.cMac);

    ASSERTS_TRUE(pAP->IEEE80211rFTOverDSEnable, , ME, "FT over DS disabled");

    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &pAP->neighbours) {
        T_ApNeighbour* neighbour = amxc_llist_it_get_data(it, T_ApNeighbour, it);
        swl_macChar_t bssidStr;
        swl_mac_binToChar(&bssidStr, (swl_macBin_t*) neighbour->bssid);
        if(swl_str_isEmpty(bssidStr.cMac) || swl_str_isEmpty(neighbour->nasIdentifier) || swl_str_isEmpty(neighbour->r0khkey)) {
            SAH_TRACEZ_ERROR(ME, "Invalid 11r parameters");
            return;
        }
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s %s %s", bssidStr.cMac, neighbour->nasIdentifier, neighbour->r0khkey);
        swl_mapChar_add(vapConfigMap, "r0kh", buffer);
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s %s %s", bssidStr.cMac, bssidStr.cMac, neighbour->r0khkey);
        swl_mapChar_add(vapConfigMap, "r1kh", buffer);
    }
}

static void s_writeMfConfig(T_AccessPoint* vap, swl_mapChar_t* vapConfigMap) {
    if((vap->MF_Mode == APMFM_OFF) && !vap->MF_TempBlacklistEnable) {
        swl_mapChar_delete(vapConfigMap, "macaddr_acl");
        swl_mapChar_delete(vapConfigMap, "accept_mac_file");
        swl_mapChar_delete(vapConfigMap, "deny_mac_file");
        return;
    }
    char fileBuf[64];
    snprintf(fileBuf, sizeof(fileBuf), "/tmp/hostap_%s.acl", vap->alias);
    FILE* tmpFile = fopen(fileBuf, "w");
    if(vap->MF_Mode == APMFM_WHITELIST) {
        swl_mapChar_add(vapConfigMap, "macaddr_acl", "1");
        swl_mapChar_add(vapConfigMap, "accept_mac_file", fileBuf);

        for(int i = 0; i < vap->MF_EntryCount; i++) {
            if(!vap->MF_TempBlacklistEnable
               || (!wldu_is_mac_in_list(vap->MF_Entry[i], vap->MF_Temp_Entry, vap->MF_TempEntryCount))) {
                // add entry either if temporary blacklisting is disabled, or entry not in temp blaclist
                // Probe filtering currently not supported
                fprintf(tmpFile, SWL_MAC_FMT "\n", SWL_MAC_ARG(vap->MF_Entry[i]));
            }
        }
    } else {
        swl_mapChar_add(vapConfigMap, "macaddr_acl", "0");
        swl_mapChar_add(vapConfigMap, "deny_mac_file", fileBuf);

        if(vap->MF_Mode == APMFM_BLACKLIST) {
            for(int i = 0; i < vap->MF_EntryCount; i++) {
                fprintf(tmpFile, SWL_MAC_FMT "\n", SWL_MAC_ARG(vap->MF_Entry[i]));
            }
        }
        if(vap->MF_TempBlacklistEnable) {
            for(int i = 0; i < vap->MF_TempEntryCount; i++) {
                fprintf(tmpFile, SWL_MAC_FMT "\n", SWL_MAC_ARG(vap->MF_Temp_Entry[i]));
            }
        }
        // Probe filtering currently not supported.
    }

    fclose(tmpFile);


}

/**
 * @brief set the security parameters of a vap
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
void s_setVapSecurityConfig(T_AccessPoint* pAP, wld_hostapd_config_t* config) {
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    swl_mapChar_t* vapConfigMap = wld_hostapd_getConfigMap(config, pAP->alias);
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    int tval = 0;
    char WEPKEYCONV[36] = {0};
    T_AccessPoint* firstVap = wld_rad_getFirstVap(pRad);
    ASSERTS_NOT_NULL(firstVap, , ME, "NULL");
    swl_macChar_t bssidStr;
    wldu_convMac2Str(pSSID->BSSID, ETHER_ADDR_LEN, bssidStr.cMac, ETHER_ADDR_STR_LEN);
    char buffer[128] = {0};

    char* wpa_key_str = ((strlen(pAP->keyPassPhrase) + 1) == PSK_KEY_SIZE_LEN) ? "wpa_psk" : "wpa_passphrase";
    if(firstVap != pAP) {
        swl_mapChar_add(vapConfigMap, "bss", pAP->alias);
        swl_mapChar_add(vapConfigMap, "bssid", bssidStr.cMac);
    }
    if(strlen(pAP->bridgeName) > 0) {
        swl_mapChar_add(vapConfigMap, "bridge", pAP->bridgeName);
    }
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%d", pRad->dtimPeriod);
    swl_mapChar_add(vapConfigMap, "dtim_period", buffer);
    swl_mapChar_add(vapConfigMap, "ssid", pSSID->SSID);
    swl_mapChar_add(vapConfigMap, "auth_algs", "1");
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%d", pAP->clientIsolationEnable);
    swl_mapChar_add(vapConfigMap, "ap_isolate", buffer);
    if(pAP->enable) {
        swl_mapChar_add(vapConfigMap, "ignore_broadcast_ssid", pAP->SSIDAdvertisementEnabled ? "0" : "2");
    } else {
        swl_mapChar_add(vapConfigMap, "ignore_broadcast_ssid", "1");
        swl_mapChar_add(vapConfigMap, "start_disabled", "1");
    }

    s_setVapIeee80211rConfig(pAP, config);

    s_writeMfConfig(pAP, vapConfigMap);

    switch(pAP->secModeEnabled) {
    case APMSI_WEP64:
    case APMSI_WEP128:
    case APMSI_WEP128IV:
        swl_mapChar_add(vapConfigMap, "wpa", "0");
        swl_mapChar_add(vapConfigMap, "wep_default_key", "0");
        swl_mapChar_add(vapConfigMap, "wep_key0", convASCII_WEPKey(pAP->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV)));
        swl_mapChar_add(vapConfigMap, "wep_key1", convASCII_WEPKey(pAP->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV)));
        swl_mapChar_add(vapConfigMap, "wep_key2", convASCII_WEPKey(pAP->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV)));
        swl_mapChar_add(vapConfigMap, "wep_key3", convASCII_WEPKey(pAP->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV)));
        break;
    case APMSI_WPA_P:
    case APMSI_WPA2_P:
    case APMSI_WPA_WPA2_P:
        tval = (pAP->secModeEnabled - APMSI_WPA_P) + 1;
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", tval);
        swl_mapChar_add(vapConfigMap, "wpa", buffer);
        if(pAP->keyPassPhrase[0]) {    /* prefer AES key? ontop of TKIP */
            swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", pAP->IEEE80211rEnable ? "WPA-PSK FT-PSK" : "WPA-PSK");
            /* If key pass phrase is set, we use the Key pass phrase */
            if(tval == 3) {
                swl_mapChar_add(vapConfigMap, "wpa_pairwise", "TKIP CCMP"); /* WPA_WPA2 */
            } else {
                swl_mapChar_add(vapConfigMap, "wpa_pairwise", (tval == 1) ? "TKIP" : "CCMP");
            }
            swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        } else {
            /* Use the Pre Shared Key (PSK) */
            swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", "WPA-PSK");
            swl_mapChar_add(vapConfigMap, "wpa_pairwise", "TKIP"); /* WPA or WPA2 with TKIP */
            swl_mapChar_add(vapConfigMap, "wpa_psk", pAP->preSharedKey);
        }
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_group_rekey", buffer);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->mfpConfig);
        swl_mapChar_add(vapConfigMap, "ieee80211w", buffer);
        break;
    case APMSI_WPA2_WPA3_P:
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", "WPA-PSK SAE");
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", "CCMP");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_group_rekey", buffer);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        // If sae_password is set, hostapd will use the sae_password value
        // for WPA3 connection and wpa_passphrase for WPA-WPA2. If sae_password
        // is not set, wpa_passphrase will be used for WPA3 connection
        if(!swl_str_matches(pAP->saePassphrase, "")) {
            swl_mapChar_add(vapConfigMap, "sae_password", pAP->saePassphrase);
        }
        swl_mapChar_add(vapConfigMap, "sae_require_mfp", "1");
        swl_mapChar_add(vapConfigMap, "sae_anti_clogging_threshold", "5");
        swl_mapChar_add(vapConfigMap, "sae_sync", "5");
        swl_mapChar_add(vapConfigMap, "sae_groups", "19 20 21");
        swl_mapChar_add(vapConfigMap, "ieee80211w", "1");
        break;
    case APMSI_WPA3_P:
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", pAP->IEEE80211rEnable ? "SAE FT-SAE" : "SAE");
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", "CCMP");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_group_rekey", buffer);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        // If sae_password is set, wpa_passphrase is ignored by hostapd
        // otherwise wpa_passphrase is used
        if(!swl_str_matches(pAP->saePassphrase, "")) {
            swl_mapChar_add(vapConfigMap, "sae_password", pAP->saePassphrase);
        }
        swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        swl_mapChar_add(vapConfigMap, "sae_require_mfp", "1");
        swl_mapChar_add(vapConfigMap, "sae_anti_clogging_threshold", "5");
        swl_mapChar_add(vapConfigMap, "sae_sync", "5");
        swl_mapChar_add(vapConfigMap, "sae_groups", "19 20 21");
        swl_mapChar_add(vapConfigMap, "ieee80211w", "2");
        if(pAP->pRadio->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
            swl_mapChar_add(vapConfigMap, "sae_pwe", "1");
        } else {
            swl_mapChar_add(vapConfigMap, "sae_pwe", "2");
        }
        break;
    case APMSI_OWE:
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", "CCMP");
        swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", "OWE");
        swl_mapChar_add(vapConfigMap, "ieee80211w", "2");
        if(!swl_str_nmatches(pAP->oweTransModeIntf, "", strlen(pAP->oweTransModeIntf))) {
            T_AccessPoint* transitionAp = wld_ap_getVapByName(pAP->oweTransModeIntf);
            swl_mapChar_add(vapConfigMap, "owe_transition_ifname", transitionAp->alias);
        }
        break;
    case APMSI_NONE:
        swl_mapChar_add(vapConfigMap, "wpa", "0");
        if(!swl_str_nmatches(pAP->oweTransModeIntf, "", strlen(pAP->oweTransModeIntf))) {
            T_AccessPoint* transitionAp = wld_ap_getVapByName(pAP->oweTransModeIntf);
            swl_mapChar_add(vapConfigMap, "owe_transition_ifname", transitionAp->alias);
        }
        break;
    case APMSI_NONE_E:
        swl_mapChar_add(vapConfigMap, "wpa", "0");
        swl_mapChar_add(vapConfigMap, "auth_server_addr", *pAP->radiusServerIPAddr ? pAP->radiusServerIPAddr : "127.0.0.1");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->radiusServerPort);
        swl_mapChar_add(vapConfigMap, "auth_server_port", buffer);
        if(*pAP->radiusSecret) {
            swl_mapChar_add(vapConfigMap, "auth_server_shared_secret", pAP->radiusSecret);
        }
        break;
    case APMSI_WPA_E:
    case APMSI_WPA2_E:
    case APMSI_WPA_WPA2_E:
        tval = (pAP->secModeEnabled - APMSI_WPA_E) + 1;
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", tval);
        swl_mapChar_add(vapConfigMap, "wpa", buffer);
        swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", pAP->IEEE80211rEnable ? "WPA-EAP FT-EAP" : "WPA-EAP");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s %s", (tval & 1) ? "TKIP" : "", (tval & 2) ? "CCMP" : "");
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", buffer);
        swl_mapChar_add(vapConfigMap, "auth_server_addr", *pAP->radiusServerIPAddr ? pAP->radiusServerIPAddr : "127.0.0.1");
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->radiusServerPort);
        swl_mapChar_add(vapConfigMap, "auth_server_port", buffer);

        if(*pAP->radiusSecret) {
            swl_mapChar_add(vapConfigMap, "auth_server_shared_secret", pAP->radiusSecret);
        }
        if(*pAP->radiusOwnIPAddress) {
            swl_mapChar_add(vapConfigMap, "own_ip_addr", pAP->radiusOwnIPAddress);
        }
        if(*pAP->radiusNASIdentifier) {
            swl_mapChar_add(vapConfigMap, "nas_identifier", pAP->radiusNASIdentifier);
        }
        if(*pAP->radiusCalledStationId) {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "30:s:%s", pAP->radiusCalledStationId);
            swl_mapChar_add(vapConfigMap, "radius_auth_req_attr", buffer);
        }
        if(pAP->radiusDefaultSessionTimeout) {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%d", pAP->radiusDefaultSessionTimeout);
            swl_mapChar_add(vapConfigMap, "auth_server_default_session_timeout", buffer);
        }
        if(pAP->radiusChargeableUserId) {
            swl_mapChar_add(vapConfigMap, "radius_request_cui", "1");
        }
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%d", pAP->mfpConfig);
        swl_mapChar_add(vapConfigMap, "ieee80211w", buffer);
        break;
    default:
        break;
    }
    swl_mapChar_add(vapConfigMap, "disable_dgaf", "0");
    swl_mapChar_add(vapConfigMap, "hs20_deauth_req_timeout", "0");
    swl_mapChar_add(vapConfigMap, "osen", "0");
    swl_mapChar_add(vapConfigMap, "wmm_enabled", "1");
    //Temporarily disabled : triggering station disconnection
    swl_mapChar_add(vapConfigMap, "#bss_transition", "1");
    swl_mapChar_add(vapConfigMap, "notify_mgmt_frames", "1");
    if(pAP->transitionDisable != 0) {
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "0x%x", pAP->transitionDisable);
        swl_mapChar_add(vapConfigMap, "transition_disable", buffer);
    }
}

/**
 * @brief set the wps parameters of a vap
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
static void s_setVapWpsConfig(T_AccessPoint* pAP, wld_hostapd_config_t* config) {
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    swl_mapChar_t* vapConfigMap = wld_hostapd_getConfigMap(config, pAP->alias);
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    char buffer[128] = {0};
    bool wps_enable = (pAP->WPS_Enable &&
                       ((!pAP->secModeEnabled && pAP->WPS_CertMode) ||
                        (pAP->secModeEnabled > APMSI_WEP128IV) ||
                        (!pAP->WPS_Configured)));
    swl_mapChar_add(vapConfigMap, "wps_state", wps_enable ? (pAP->WPS_Configured ? "2" : "1") : "0");
    swl_mapChar_add(vapConfigMap, "wps_independent", "1");
    swl_mapChar_add(vapConfigMap, "ap_setup_locked", "1");
    swl_mapChar_add(vapConfigMap, "uuid", pRad->wpsConst->UUID);
    char* deviceName = (pRad->wpsConst->DevName && !strcmp(pRad->wpsConst->DevName, "") ? "unknownAp" : pRad->wpsConst->DevName);
    swl_mapChar_add(vapConfigMap, "device_name", deviceName);
    swl_mapChar_add(vapConfigMap, "manufacturer", pRad->wpsConst->Manufacturer);
    swl_mapChar_add(vapConfigMap, "model_name", pRad->wpsConst->ModelName);
    swl_mapChar_add(vapConfigMap, "model_number", pRad->wpsConst->ModelNumber);
    swl_mapChar_add(vapConfigMap, "serial_number", pRad->wpsConst->SerialNumber);
    int tmpver[4];
    sscanf(pRad->wpsConst->OsVersion, "%i.%i.%i.%i", &tmpver[0], &tmpver[1], &tmpver[2], &tmpver[3]);
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%.8x", ((unsigned int) (tmpver[0] << 24 | tmpver[1] << 16 | tmpver[2] << 8 | tmpver[3])));
    swl_mapChar_add(vapConfigMap, "os_version", buffer);
    swl_mapChar_add(vapConfigMap, "device_type", "6-0050F204-1");
    amxc_string_t configMethodsStr;
    amxc_string_init(&configMethodsStr, 0);
    bitmask_to_string(&configMethodsStr, wld_hostapd_wpsConfigMethods, ' ', pAP->WPS_ConfigMethodsEnabled);
    swl_mapChar_add(vapConfigMap, "config_methods", (char*) configMethodsStr.buffer);
    amxc_string_clean(&configMethodsStr);

    swl_mapChar_add(vapConfigMap, "wps_cred_processing", pAP->WPS_Configured ? "2" : "0");
    if(pAP->WPS_Enable && (pAP->WPS_ConfigMethodsEnabled & (APWPSCMS_KEYPAD | APWPSCMS_DISPLAY))) {
        swl_mapChar_add(vapConfigMap, "ap_pin", pRad->wpsConst->DefaultPin);
    }
    swl_mapChar_add(vapConfigMap, "wps_rf_bands", "ag");
    swl_mapChar_add(vapConfigMap, "friendly_name", "WPS Access Point");
    swl_mapChar_add(vapConfigMap, "eap_server", "1");
}

/**
 * @brief set a vap parameters. A vap configuration could be either an interface or a bss
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
static void s_setVapConfig(T_AccessPoint* pAP, wld_hostapd_config_t* config) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(config, , ME, "NULL");

    s_setVapSecurityConfig(pAP, config);

    if(!pAP->WPS_Enable || (pAP->secModeEnabled == APMSI_WPA3_P)) {
        return;
    }

    s_setVapWpsConfig(pAP, config);
}

/**
 * @brief create a configuration file for the hostapd
 *
 * @param pRad the radio
 * @param cfgFileName the hostapd configuration file name
 *
 * @return void
 */
void wld_hostapd_cfgFile_create(T_Radio* pRad, char* cfgFileName) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_STR(cfgFileName, , ME, "Empty path");
    T_AccessPoint* firstVap = wld_rad_getFirstVap(pRad);
    ASSERTS_NOT_NULL(firstVap, , ME, "NULL");
    wld_hostapd_config_t* config;
    bool ret = wld_hostapd_createConfig(&config, pRad->llAP);
    ASSERT_TRUE(ret, , ME, "Bad config");
    swl_mapChar_t* generalConfigMap = wld_hostapd_getConfigMap(config, NULL);
    ASSERTS_NOT_NULL(generalConfigMap, , ME, "NULL");
    swl_mapChar_add(generalConfigMap, "driver", "nl80211");
    swl_mapChar_add(generalConfigMap, "ctrl_interface", (pRad->hostapd ? pRad->hostapd->ctrlIfaceDir : HOSTAPD_CTRL_IFACE_DIR));
    swl_mapChar_add(generalConfigMap, "ctrl_interface_group", "0");
    swl_mapChar_t* interfaceConfigMap = wld_hostapd_getConfigMap(config, firstVap->alias);
    ASSERTS_NOT_NULL(interfaceConfigMap, , ME, "NULL");
    swl_mapChar_add(interfaceConfigMap, "interface", firstVap->alias);
    s_setRadioConfig(pRad, config);
    amxc_llist_it_t* ap_it;
    amxc_llist_for_each(ap_it, &pRad->llAP) {
        T_AccessPoint* pAp = amxc_llist_it_get_data(ap_it, T_AccessPoint, it);
        SAH_TRACEZ_INFO(ME, "%s: Write config %s %u", pRad->Name, pAp->alias, pAp->ref_index);
        s_setVapConfig(pAp, config);
    }
    wld_hostapd_writeConfig(config, cfgFileName);
    wld_hostapd_deleteConfig(config);
}

void wld_hostapd_cfgFile_createExt(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, , ME, "NULL");
    wld_hostapd_cfgFile_create(pRad, pRad->hostapd->cfgFile);
}

/**
 * @brief update a configuration file for the hostapd
 *
 * @param configPath the path of the config file
 * @param interface bss name
 * @param key the param to update
 * @param value the key value to set
 * @return void
 */
bool wld_hostapd_cfgFile_update(char* configPath, const char* interface, const char* key, const char* value) {
    SAH_TRACEZ_IN(ME);
    ASSERT_STR(configPath, false, ME, "Empty path");
    ASSERT_STR(interface, false, ME, "Empty interface");
    ASSERT_STR(key, false, ME, "Empty key");
    ASSERT_NOT_NULL(value, false, ME, "NULL");
    wld_hostapd_config_t* config;
    bool ret = wld_hostapd_loadConfig(&config, configPath);
    ASSERTS_TRUE(ret, false, ME, "Bad config");
    SAH_TRACEZ_INFO(ME, "%s: updateConfig interface:%s, key:%s, value:%s", configPath, interface, key, value);
    ret = wld_hostapd_addConfigParam(config, interface, key, value);
    if(ret) {
        ret = wld_hostapd_writeConfig(config, configPath);
    }
    wld_hostapd_deleteConfig(config);
    SAH_TRACEZ_OUT(ME);
    return ret;
}
