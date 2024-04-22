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
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_ep_api.h"
#include "wld_wpaCtrl_api.h"
#include "wld_endpoint.h"

#define ME "wpaSupp"


bool s_sendWpaSuppCommand(T_EndPoint* pEP, char* cmd, const char* reason) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    ASSERTS_TRUE(wld_wpaCtrlInterface_isReady(pEP->wpaCtrlInterface), false, ME, "Connection with hostapd not yet established");
    T_Radio* pR = pEP->pRadio;
    ASSERTS_NOT_NULL(pR, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: send wpaSupp cmd %s for %s", pR->Name, cmd, reason);
    return wld_wpaCtrl_sendCmdCheckResponse(pEP->wpaCtrlInterface, cmd, "OK");
}

/**
 * @brief Disconnect the endpoint from an AP
 *
 * @param pEP endpoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_disconnect(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->pSSID, SWL_RC_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Disconnecting from "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(pEP->pSSID->BSSID));

    bool ret = s_sendWpaSuppCommand(pEP, "DISCONNECT", "");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send disconnect command", pEP->Name);
    return SWL_RC_OK;
}

/*
 * @brief get the EP's global status information
 * eg: when ep is connected
 * bssid=98:42:65:2d:23:43
 * freq=5260
 * ssid=prplos
 * id=0
 * mode=station
 * multi_ap_profile=0
 * multi_ap_primary_vlanid=0
 * wifi_generation=6
 * pairwise_cipher=CCMP
 * group_cipher=CCMP
 * key_mgmt=WPA2-PSK
 * wpa_state=COMPLETED
 * address=4e:ba:7d:80:80:87
 * uuid=4c5b5a59-4f5c-f048-ff5c-50484c5b5a59
 * ieee80211ac=1
 *
 * @param pEP endpoint
 * @param reply output buffer
 * @param maxReplySize output buffer size
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_wpaSupp_ep_getAllStatusDetails(T_EndPoint* pEP, char* reply, size_t replySize) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Send STATUS command", pEP->Name);
    bool ret = wld_wpaCtrl_sendCmdSynced(pEP->wpaCtrlInterface, "STATUS", reply, replySize);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to query ep status", pEP->Name);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaSupp_ep_getOneStatusDetail(T_EndPoint* pEP, const char* key, char* valStr, size_t valStrSize) {
    char reply[1024] = {0};
    swl_rc_ne rc = wld_wpaSupp_ep_getAllStatusDetails(pEP, reply, sizeof(reply));
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "failed to get global ep status");
    int valStrLen = wld_wpaCtrl_getValueStr(reply, key, valStr, valStrSize);
    ASSERT_FALSE(valStrLen <= 0, SWL_RC_ERROR, ME, "%s: not found status field %s", pEP->Name, key);
    SAH_TRACEZ_INFO(ME, "%s: %s = (%s)", pEP->Name, key, valStr);
    ASSERT_TRUE(valStrLen < (int) valStrSize, SWL_RC_ERROR,
                ME, "%s: buffer too short for field %s (l:%d,s:%zu)", pEP->Name, key, valStrLen, valStrSize);
    return SWL_RC_OK;
}

/**
 * @brief get the AP's bssid to which the endpoint is connected
 *
 * @param pEP endpoint
 * @param bssid the bssid string value
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getBssid(T_EndPoint* pEP, swl_macChar_t* bssid) {
    swl_macChar_t tmpBssid = SWL_MAC_CHAR_NEW();
    swl_rc_ne rc = wld_wpaSupp_ep_getOneStatusDetail(pEP, "bssid", tmpBssid.cMac, SWL_MAC_CHAR_LEN);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "failed to get endpoint bssid");
    ASSERT_TRUE(swl_mac_charIsValidStaMac(&tmpBssid), SWL_RC_ERROR, ME, "%s: invalid bssid (%s)", pEP->Name, tmpBssid.cMac);
    W_SWL_SETPTR(bssid, tmpBssid);
    return SWL_RC_OK;
}

/**
 * @brief get the AP's SSID to which the endpoint is connected
 *
 * @param pEP endpoint
 * @param ssid the remote AP ssid buffer
 * @param ssidSize the remote AP ssid buffer size
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getSsid(T_EndPoint* pEP, char* ssid, size_t ssidSize) {
    return wld_wpaSupp_ep_getOneStatusDetail(pEP, "ssid", ssid, ssidSize);
}

SWL_TABLE(sWpaStateDescMaps,
          ARR(char* wpaStateDesc; wld_epConnectionStatus_e epConnState; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"DISCONNECTED", EPCS_DISCONNECTED},
              {"INACTIVE", EPCS_IDLE},
              {"INTERFACE_DISABLED", EPCS_IDLE},
              {"SCANNING", EPCS_DISCOVERING},
              {"AUTHENTICATING", EPCS_CONNECTING},
              {"ASSOCIATING", EPCS_CONNECTING},
              {"ASSOCIATED", EPCS_CONNECTING},
              {"4WAY_HANDSHAKE", EPCS_CONNECTING},
              {"GROUP_HANDSHAKE", EPCS_CONNECTING},
              {"COMPLETED", EPCS_CONNECTED},
              ));
/**
 * @brief get the EP's current connection status
 * "DISCONNECTED", "INACTIVE", "INTERFACE_DISABLED", "SCANNING", "AUTHENTICATING",
 * "ASSOCIATING", "ASSOCIATED", "4WAY_HANDSHAKE", "GROUP_HANDSHAKE", "COMPLETED"
 * @param pEP endpoint
 * @param pEPConnState output endpoint connection state based on retrieved wpa_state
 * @return - SWL_RC_OK when the wpa_state is found and parsed successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getConnState(T_EndPoint* pEP, wld_epConnectionStatus_e* pEPConnState) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pEP->wpaCtrlInterface), SWL_RC_ERROR,
                 ME, "%s: main wpactrl iface is not ready", pEP->Name);
    char state[64] = {0};
    swl_rc_ne rc = wld_wpaSupp_ep_getOneStatusDetail(pEP, "wpa_state", state, sizeof(state));
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: failed to get endpoint wpa_state", pEP->Name);
    wld_epConnectionStatus_e* pConnDetState = (wld_epConnectionStatus_e*) swl_table_getMatchingValue(&sWpaStateDescMaps, 1, 0, state);
    ASSERTI_NOT_NULL(pConnDetState, SWL_RC_ERROR, ME, "%s: unknown conn state(%s)", pEP->Name, state);
    W_SWL_SETPTR(pEPConnState, *pConnDetState);
    SAH_TRACEZ_INFO(ME, "%s: ep state(%s) -> connState(%d)", pEP->Name, state, *pConnDetState);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_startWpsPbc
 *
 * send wps_pbc command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint.
 * @param bssid: The BSSID to connect to (optional)
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_startWpsPbc(T_EndPoint* pEP, swl_macChar_t* bssid) {
    SAH_TRACEZ_INFO(ME, "EndPoint %s %s send wps_pbc", pEP->alias, pEP->Name);

    char cmd[64] = {0};
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PBC");
    if(pEP->multiAPEnable) {
        swl_str_catFormat(cmd, sizeof(cmd), " multi_ap=1");
    }
    if(!swl_mac_charIsNull(bssid)) {
        swl_str_catFormat(cmd, sizeof(cmd), " %s", bssid->cMac);
    }
    bool ret = s_sendWpaSuppCommand(pEP, cmd, "WPS_PBC");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_pbc command", pEP->Name);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_startWpsPin
 *
 * send wps_pin command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint.
 * @param pin: The PIN to use
 * @param bssid: The BSSID to connect to (optional)
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_startWpsPin(T_EndPoint* pEP, char* pin, swl_macChar_t* bssid) {
    ASSERT_STR(pin, SWL_RC_INVALID_PARAM, ME, "NULL PIN");
    SAH_TRACEZ_INFO(ME, "%s: send wps start pin %s", pEP->Name, pin);
    char cmd[64] = {0};
    //WPS client PIN started with a default wps session walk time
    char* bssidStr = "any";
    if(!swl_mac_charIsNull(bssid)) {
        bssidStr = bssid->cMac;
    }
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PIN %s %s", bssidStr, pin);
    bool ret = s_sendWpaSuppCommand(pEP, cmd, "WPS_PIN");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_pin command", pEP->Name);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_cancelWps
 *
 * send wps_cancel command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_cancelWps(T_EndPoint* pEP) {
    SAH_TRACEZ_INFO(ME, "%s: send wps stop", pEP->Name);

    bool ret = s_sendWpaSuppCommand(pEP, "WPS_CANCEL", "WPS_CANCEL");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_cancel command", pEP->Name);
    return SWL_RC_OK;
}
