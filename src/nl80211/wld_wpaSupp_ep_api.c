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

/**
 * @brief get the AP's bssid to which the endpoint is connected
 *
 * @param pEP endpoint
 * @param cMac the bssid address
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getBssid(T_EndPoint* pEP, swl_macChar_t* bssid) {
    SAH_TRACEZ_INFO(ME, "%s: Send STATUS command", pEP->Name);
    char reply[1024] = {0};
    int ret = wld_wpaCtrl_sendCmdSynced(pEP->wpaCtrlInterface, "STATUS", reply, sizeof(reply));
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send status command", pEP->Name);
    ret = wld_wpaCtrl_getValueStr(reply, "bssid", bssid->cMac, SWL_MAC_CHAR_LEN);
    ASSERT_TRUE(ret > 0, SWL_RC_ERROR, ME, "%s: not found endpoint bssid", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: bssid[%s]", pEP->Name, bssid->cMac);
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
    SAH_TRACEZ_INFO(ME, "%s: send wps_pbc", pEP->Name);

    char cmd[64] = {0};
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PBC");
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
