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
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_wps.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "swl/swl_uuid.h"

#define ME "wps"

/*
 * @brief names of all WPS configuration methods as per tr-181-2-15-1-usp
 * Device.WiFi.AccessPoint.{i}.WPS.ConfigMethodsSupported
 * */
static const char* cstr_WPS_CM_Supported[] = {
    "USBFlashDrive",
    "Ethernet",
    "Label",
    "Display",
    "ExternalNFCToken",
    "IntegratedNFCToken",
    "NFCInterface",
    "PushButton",
    "PIN",
    "PhysicalPushButton",
    "PhysicalDisplay",
    "VirtualPushButton",
    "VirtualDisplay",
    0,
};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(cstr_WPS_CM_Supported) == (WPS_CFG_MTHD_MAX + 1), "cstr_WPS_CM_Supported not correctly defined");

const char* cstr_AP_WPS_VERSUPPORTED[] = {"AUTO", "Auto", "auto", "1.0", "1", "2.0", "2",
    0};
const int ci_AP_WPS_VERSUPPORTED[] = {
    APWPSVERSI_AUTO, APWPSVERSI_AUTO, APWPSVERSI_AUTO,
    APWPSVERSI_10, APWPSVERSI_10,
    APWPSVERSI_20, APWPSVERSI_20,
    APWPSVERSI_UNKNOWN,
    0
};

amxd_status_t doCancelPairing(amxd_object_t* object) {
    T_AccessPoint* pAP = (T_AccessPoint*) amxd_object_get_parent(object)->priv;
    char strbuf[] = "STOP";
    // If a timer is running... stop and destroy it also.
    if(pAP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pAP->WPS_PBC_Delay.timer);
        pAP->WPS_PBC_Delay.timer = NULL;
        pAP->WPS_PBC_Delay.intf.vap = NULL;
    }
    wld_wps_clearPairingTimer(&pAP->wpsSessionInfo);
    pAP->pFA->mfn_wvap_wps_sync(pAP, strbuf, SWL_ARRAY_SIZE(strbuf), SET);

    SAH_TRACEZ_ERROR(ME, "WPS cancel %s", pAP->alias);
    return amxd_status_ok;
}

const char* wld_wps_ConfigMethod_to_string(wld_wps_cfgMethod_e value) {
    ASSERT_TRUE(value < WPS_CFG_MTHD_MAX, "", ME, "invalid %d", value);
    return cstr_WPS_CM_Supported[value];
}

bool wld_wps_ConfigMethods_mask_to_string(amxc_string_t* output, const wld_wps_cfgMethod_m configMethods) {
    ASSERTS_NOT_NULL(output, false, ME, "NULL");
    if(configMethods == 0) {
        amxc_string_clean(output);
        amxc_string_appendf(output, "None");
        return true;
    }
    return bitmask_to_string(output, cstr_WPS_CM_Supported, ',', configMethods);
}

bool wld_wps_ConfigMethods_mask_to_charBuf(char* string, size_t stringsize, const wld_wps_cfgMethod_m configMethods) {
    return swl_conv_maskToChar(string, stringsize, configMethods, cstr_WPS_CM_Supported, WPS_CFG_MTHD_MAX);
}

bool wld_wps_ConfigMethods_string_to_mask(wld_wps_cfgMethod_m* output, const char* input, const char separator) {
    ASSERTS_NOT_NULL(output, false, ME, "NULL");
    *output = swl_conv_charToMaskSep(input, cstr_WPS_CM_Supported, WPS_CFG_MTHD_MAX, separator, NULL);
    return true;
}

static amxd_status_t s_setCommandReply(amxc_var_t* retval, swl_usp_cmdStatus_ne cmdStatus, amxd_status_t defRc) {
    amxd_status_t rc = amxd_status_ok;
    if(amxc_var_add_key(cstring_t, retval, "Status", swl_uspCmdStatus_toString(cmdStatus)) == NULL) {
        /*
         * If failing to add command status, then return amx error
         * otherwise return amx success to allow returning the filled retval variant
         * including the status (potentially error) description
         */
        rc = defRc;
    }
    return rc;
}

void wld_wps_pushButton_reply(uint64_t call_id, swl_usp_cmdStatus_ne cmdStatus) {
    amxc_var_t retval;
    amxc_var_init(&retval);
    amxc_var_set_type(&retval, AMXC_VAR_ID_HTABLE);
    amxd_status_t rc = s_setCommandReply(&retval, cmdStatus, amxd_status_unknown_error);
    amxd_function_deferred_done(call_id, rc, NULL, &retval);
    amxc_var_clean(&retval);
}

void wld_wps_clearPairingTimer(wld_wpsSessionInfo_t* pCtx) {
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    ASSERTS_NOT_NULL(pCtx->sessionTimer, , ME, "NULL");
    amxp_timer_delete(&pCtx->sessionTimer);
    pCtx->sessionTimer = NULL;
}

static void s_pairingTimeoutHandler(amxp_timer_t* timer _UNUSED, void* userdata) {
    wld_wpsSessionInfo_t* pCtx = (wld_wpsSessionInfo_t*) userdata;
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    ASSERT_NOT_NULL(pCtx->intfObj, , ME, "NULL");
    const char* name = amxd_object_get_name(pCtx->intfObj, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_ERROR(ME, "%s: wps pairing timeout (triggered by safety timer)", name);
    wld_wps_sendPairingNotification(pCtx->intfObj, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL, NULL);
    doCancelPairing(amxd_object_get(pCtx->intfObj, "WPS"));
}

void s_startPairingTimer(wld_wpsSessionInfo_t* pCtx) {
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    if(pCtx->sessionTimer == NULL) {
        amxp_timer_new(&pCtx->sessionTimer, s_pairingTimeoutHandler, pCtx);
    }
    ASSERT_NOT_NULL(pCtx->sessionTimer, , ME, "NULL");
    amxp_timer_start(pCtx->sessionTimer, WPS_SESSION_TIMEOUT * 1000);
}

void s_sendNotif(amxd_object_t* wps, const char* name, const char* reason, const char* macAddress, T_WPSCredentials* credentials) {
    amxc_var_t notifMap;
    amxc_var_init(&notifMap);
    amxc_var_set_type(&notifMap, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, &notifMap, "reason", reason);
    if(macAddress) {
        amxc_var_add_key(cstring_t, &notifMap, "macAddress", macAddress);
    }
    if(credentials) {
        amxc_var_add_key(cstring_t, &notifMap, "SSID", credentials->ssid);
        amxc_var_add_key(cstring_t, &notifMap, "securitymode", swl_security_apModeToString(credentials->secMode, SWL_SECURITY_APMODEFMT_LEGACY));
        amxc_var_add_key(cstring_t, &notifMap, "KeyPassPhrase", credentials->key);
    }
    SAH_TRACEZ_WARNING(ME, "notif %s, reason=%s, macAddress=%s}", name, reason, macAddress);

    amxd_object_trigger_signal(wps, name, &notifMap);
    amxc_var_clean(&notifMap);
}

void wld_wps_sendPairingNotification(amxd_object_t* object, uint32_t type, const char* reason, const char* macAddress, T_WPSCredentials* credentials) {
    amxd_object_t* wps = amxd_object_get(object, "WPS");
    ASSERT_NOT_NULL(wps, , ME, "NULL");
    char* name = WPS_PAIRING_NO_MESSAGE;
    bool inProgress = false;
    bool prevInProgress = amxd_object_get_bool(wps, "PairingInProgress", NULL);

    switch(type) {
    case (NOTIFY_PAIRING_READY):
        name = WPS_PAIRING_READY;
        inProgress = true;
        break;
    case (NOTIFY_PAIRING_DONE):
        name = WPS_PAIRING_DONE;
        inProgress = false;
        break;
    case (NOTIFY_PAIRING_ERROR):
        name = WPS_PAIRING_ERROR;
        inProgress = false;
        break;
    }

    wld_wpsSessionInfo_t* pCtx = wps->priv;
    const char* intfName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    if(!inProgress) {
        wld_wps_clearPairingTimer(pCtx);
        ASSERTI_FALSE(((type == NOTIFY_PAIRING_DONE) && !prevInProgress), , ME, "%s: received done(%s) while not in progress, ignore", intfName, reason);
    } else if(!prevInProgress) {
        s_startPairingTimer(pCtx);
    }

    s_sendNotif(wps, name, reason, macAddress, credentials);

    if(prevInProgress != inProgress) {
        amxd_object_set_bool(wps, "PairingInProgress", inProgress);
    }
}

void wld_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress) {
    wld_ap_sendPairingNotification(pAP, type, reason, macAddress);
}

/**
 * Sync updatable fields from `g_wpsConst` to `wps_DefParam`.
 *
 * Updatable fields are currently: DefaultPIN, UUID
 */
void s_syncWpsConstToObject() {
    amxc_llist_it_t* itRad = NULL;
    amxc_llist_it_t* it = NULL;
    bool ret;

    /* Update wps_DefParam */
    amxd_object_t* pWPS_DefParam_obj = amxd_object_get(get_wld_object(), "wps_DefParam");
    amxd_object_set_cstring_t(pWPS_DefParam_obj, "DefaultPin", g_wpsConst.DefaultPin);
    amxd_object_set_cstring_t(pWPS_DefParam_obj, "UUID", g_wpsConst.UUID);

    /* Update updatable AccessPoint.WPS.* fields */
    for(itRad = amxc_llist_get_first(&g_radios); itRad; itRad = amxc_llist_it_get_next(itRad)) {
        T_Radio* pRad = amxc_llist_it_get_data(itRad, T_Radio, it);
        assert(pRad);

        for(it = amxc_llist_get_first(&pRad->llAP); it; it = amxc_llist_it_get_next(it)) {
            T_AccessPoint* pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
            assert(pAP);

            amxd_object_t* wpsObj = amxd_object_findf(pAP->pBus, "WPS");
            ret = amxd_object_set_cstring_t(wpsObj, "SelfPIN", pRad->wpsConst->DefaultPin);
            assert(ret);

            ret = amxd_object_set_cstring_t(wpsObj, "UUID", pRad->wpsConst->UUID);
            assert(ret);
        }
    }

    ret = (get_wld_object());
    assert(ret);
}

/**
 *
 * This does not inform the driver.
 */
static void updateSelfPIN(const char* selfPIN) {
    ASSERT_NOT_NULL(selfPIN, , ME, "NULL");
    ASSERT_TRUE(strlen(selfPIN) < sizeof(g_wpsConst.DefaultPin), , ME, "PIN too long");

    swl_str_copy(g_wpsConst.DefaultPin, sizeof(g_wpsConst.DefaultPin), selfPIN);

    SAH_TRACEZ_INFO(ME, "SelfPIN updated to %s", g_wpsConst.DefaultPin);
}

void genSelfPIN() {
    char defaultPin[WPS_PIN_LEN + 1] = "";
    /* Generate new PIN */
    wpsPinGen(defaultPin);
    updateSelfPIN(defaultPin);
}

/**
 * Callback when the field `WPS.SelfPIN` is written to.
 */
amxd_status_t _wld_ap_setWpsSelfPIN_pwf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter _UNUSED,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args _UNUSED,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERTI_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "no AP");
    ASSERTI_TRUE(debugIsSsidPointer(pAP->pSSID), amxd_status_ok, ME, "no SSID");

    uint32_t SelfPIN = amxc_var_dyncast(uint32_t, args);

    char defaultPIN[WPS_PIN_LEN + 1];
    if(SelfPIN == 0) {
        memset(defaultPIN, 0, sizeof(defaultPIN));
    } else {
        sprintf(defaultPIN, "%d", SelfPIN);
    }

    updateSelfPIN(defaultPIN);
    pAP->pFA->mfn_wvap_wps_label_pin(pAP, SET | DIRECT);

    SAH_TRACEZ_INFO(ME, "set WPS SelfPIN %s %d - %d", pAP->alias, SelfPIN, rv);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _generateSelfPIN(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args _UNUSED,
                               amxc_var_t* retval) {
    amxd_object_t* obj_AP = NULL;
    T_AccessPoint* pAP = NULL;

    /* Check our input data */
    obj_AP = amxd_object_get_parent(object);

    pAP = obj_AP->priv;
    if(pAP && debugIsVapPointer(pAP)) {
        genSelfPIN();
        amxc_var_set(cstring_t, retval, g_wpsConst.DefaultPin);
        return amxd_status_ok;
    }

    return amxd_status_unknown_error;
}

/**
 * Updates the UUID to the given UUID.
 *
 * Normalizes the UUID to lowercase.
 *
 * This also informs the driver.
 *
 * @return true on success, fails on failure: given UUID is not a valid UUID (wrong format
 *   or NULL pointer). If UUID is already set (ignoring case), this is considered success.
 */
static bool s_updateUUID(const char* srcUuid, T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(srcUuid, false, ME, "NULL");
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    ASSERT_TRUE(strlen(srcUuid) < sizeof(g_wpsConst.UUID), false, ME, "UUID too long");
    ASSERT_TRUE(swl_uuid_isValidChar(srcUuid), false, ME, "Invalid UUID");

    char srcUuidLowerCase[SWL_UUID_CHAR_SIZE] = "";
    bool ok = swl_str_copy(srcUuidLowerCase, sizeof(srcUuidLowerCase), srcUuid);
    ASSERT_TRUE(ok, false, ME, "Unexpected string copy failure");
    swl_str_toLower(srcUuidLowerCase, sizeof(srcUuidLowerCase));

    if(swl_str_matches(srcUuidLowerCase, g_wpsConst.UUID)) {
        return true;
    }

    ok = swl_str_copy(g_wpsConst.UUID, sizeof(g_wpsConst.UUID), srcUuidLowerCase);
    ASSERT_TRUE(ok, false, ME, "Unexpected string copy failure");

    // Force a resync of the structure
    wld_ap_doWpsSync(pAP);

    SAH_TRACEZ_INFO(ME, "UUID updated to %s", g_wpsConst.UUID);
    return true;
}

/**
 * Callback for custom constraint of field `WPS.UUID`
 */
amxd_status_t _wld_wps_validateUUID(amxd_param_t* parameter, void* oldValue _UNUSED) {
    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(parameter, &value);
    const char* newValue = amxc_var_get_cstring_t(&value);
    amxc_var_clean(&value);
    return swl_uuid_isValidChar(newValue);
}

amxd_status_t _wld_ap_setWpsRelayCredentialsEnable_pwf(amxd_object_t* object _UNUSED,
                                                       amxd_param_t* parameter _UNUSED,
                                                       amxd_action_t reason _UNUSED,
                                                       const amxc_var_t* const args _UNUSED,
                                                       amxc_var_t* const retval _UNUSED,
                                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_addRelayApCredentials = amxc_var_dyncast(bool, args);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");
    if(new_addRelayApCredentials != pAP->addRelayApCredentials) {
        pAP->addRelayApCredentials = new_addRelayApCredentials;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setWpsRestartOnRequest_pwf(amxd_object_t* object _UNUSED,
                                                 amxd_param_t* parameter _UNUSED,
                                                 amxd_action_t reason _UNUSED,
                                                 const amxc_var_t* const args _UNUSED,
                                                 amxc_var_t* const retval _UNUSED,
                                                 void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");

    pAP->wpsRestartOnRequest = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}
/**
 * Callback called when the field `WPS.UUID` is written to.
 */
amxd_status_t _wld_wps_setUUID(amxd_object_t* object _UNUSED,
                               amxd_param_t* parameter _UNUSED,
                               amxd_action_t reason _UNUSED,
                               const amxc_var_t* const args _UNUSED,
                               amxc_var_t* const retval _UNUSED,
                               void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTI_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");
    ASSERTI_TRUE(debugIsSsidPointer(pAP->pSSID), amxd_status_ok, ME, "NULL");

    char* uuid = amxc_var_dyncast(cstring_t, args);
    bool ok = s_updateUUID(uuid, pAP);
    free(uuid);

    return ok ? amxd_status_ok : amxd_status_unknown_error;
}

static void wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;
    T_Radio* pR = (T_Radio*) pAP->pRadio;

    SAH_TRACEZ_IN(ME);

    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        // We're still not ready... start timer again maybe we must count this ?
        // 1 sec is clearly too short! some services are active after 5 sec...
        // Even the link is marked as active. Safety we take 10 sec!
        amxp_timer_start(timer, 10000);
        SAH_TRACEZ_WARNING(ME, "%s: WPS (%s) Start delayed again", pAP->alias, pAP->WPS_PBC_Delay.val);
        SAH_TRACEZ_OUT(ME);
        return;
    }

    swl_usp_cmdStatus_ne cmdStatus;
    if(((pR->status != RST_UP) || (pAP->status != APSTI_ENABLED))) {
        SAH_TRACEZ_ERROR(ME, "WPS PBC not executed because radio/AP is down");
        cmdStatus = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
    } else {
        pAP->pFA->mfn_wvap_wps_sync(
            (T_AccessPoint*) pAP->WPS_PBC_Delay.intf.vap,
            (char*) pAP->WPS_PBC_Delay.val,
            pAP->WPS_PBC_Delay.bufsize,
            pAP->WPS_PBC_Delay.setAct);
        cmdStatus = SWL_USP_CMD_STATUS_SUCCESS;
    }

    amxp_timer_delete(&timer);
    pAP->WPS_PBC_Delay.timer = NULL;
    pAP->WPS_PBC_Delay.intf.vap = NULL;

    wld_wps_pushButton_reply(pAP->WPS_PBC_Delay.call_id, cmdStatus);
    SAH_TRACEZ_OUT(ME);
}

/**
 * This function will configure the WPS module in different supported modes.
 * The reason why we add this is for old devices that can't connect anymore due
 * compatibility changes between WPS 2.0 and WPS 1.0. Note this action will take
 * some time it's starting a commit session to set it all up!
 */
amxd_status_t _setCompatibilityWPS(amxd_object_t* object,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval) {
    amxd_object_t* obj_AP = NULL;
    T_AccessPoint* pAP = NULL;
    T_Radio* pR = NULL;
    int idx;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    obj_AP = amxd_object_get_parent(object); // Now we've the AccessPoint object handle.
    const char* StrVal = GET_CHAR(args, "supVerWPS");
    pAP = obj_AP->priv;

    // We've in StrVal our WPS version tag...
    if(pAP && debugIsVapPointer(pAP)) {
        pR = pAP->pRadio;

        idx = conv_ModeIndexStr(cstr_AP_WPS_VERSUPPORTED, StrVal);
        if(idx >= 0) {
            /* We support the request */
            if(pR->wpsConst->wpsSupVer != ci_AP_WPS_VERSUPPORTED[idx]) {
                /* We must change the WPS mode...*/
                pR->wpsConst->wpsSupVer = pAP->pFA->mfn_wvap_wps_comp_mode(pAP, ci_AP_WPS_VERSUPPORTED[idx], SET);
            }
        } else {
            /* No valid value requested, set the default mode from vendor plugin */
            pR->wpsConst->wpsSupVer = pAP->pFA->mfn_wvap_wps_comp_mode(pAP, APWPSVERSI_AUTO, SET);
        }
    }

    /* Cleanup */
    amxc_var_set(uint32_t, retval, 0);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * This function will start the WPS session
 * for given AP (vap) (WPS configuration methods already matched)
 */
static amxd_status_t s_initiateWPS(T_AccessPoint* pAP, amxc_var_t* retval, swl_rc_ne* pRc) {
    T_Radio* pR = pAP->pRadio;
    if(pR->status != RST_UP) {
        SAH_TRACEZ_ERROR(ME, "Radio is down");
        *pRc = SWL_RC_INVALID_STATE;
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_NOT_READY, amxd_status_unknown_error);
    }
    if(!(pAP->enable && pAP->SSIDAdvertisementEnabled && pAP->WPS_Enable)) {
        SAH_TRACEZ_ERROR(ME, "%s VAP, SSIDADV or WPS enable are NOT TRUE (%d,%d,%d)",
                         pAP->alias,
                         pAP->enable,
                         pAP->SSIDAdvertisementEnabled,
                         pAP->WPS_Enable);
        *pRc = SWL_RC_ERROR;
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_NOT_READY, amxd_status_unknown_error);
    }

    // Is our FSM running? Yes delay the command by a timer..
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        if(pAP->WPS_PBC_Delay.timer == NULL) {
            amxp_timer_new(&pAP->WPS_PBC_Delay.timer, wps_pbc_delayed_time_handler, pAP);
        }
        amxp_timer_start(pAP->WPS_PBC_Delay.timer, 1000);
        pAP->WPS_PBC_Delay.intf.vap = (void*) pAP;
        pAP->WPS_PBC_Delay.call_id = amxc_var_constcast(uint64_t, retval);
        /* If we defer the execution don't return that the call is done! */
        SAH_TRACEZ_WARNING(ME, "%s: WPS (%s) Start delayed", pAP->alias, pAP->WPS_PBC_Delay.val);
        return amxd_status_deferred;
    }

    // If timer is running clean it now!
    if(pAP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pAP->WPS_PBC_Delay.timer);
        wld_wps_pushButton_reply(pAP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
        pAP->WPS_PBC_Delay.call_id = 0;
    }

    pAP->WPS_PBC_Delay.timer = NULL;
    pAP->WPS_PBC_Delay.intf.vap = NULL;

    /* Cancel the ongoing wps session if RestartOnRequest is true */
    if((pAP->wpsRestartOnRequest) && (pAP->WPS_PairingInProgress)) {
        char strbuf[] = "STOP";
        pAP->pFA->mfn_wvap_wps_sync(pAP, strbuf, sizeof(strbuf), SET);
        SAH_TRACEZ_WARNING(ME, "%s: WPS cancel", pAP->alias);
    }

    SAH_TRACEZ_INFO(ME, "%s: WPS (%s) Start now", pAP->alias, pAP->WPS_PBC_Delay.val);

    // Start WPS Action (PBC,Client|Self-PIN)
    *pRc = pAP->pFA->mfn_wvap_wps_sync(pAP,
                                       pAP->WPS_PBC_Delay.val,
                                       pAP->WPS_PBC_Delay.bufsize,
                                       TRUE);
    if(*pRc < SWL_RC_OK) {
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    }
    SAH_TRACEZ_WARNING(ME, "WPS Start nodelay %s", pAP->alias);
    return s_setCommandReply(retval, SWL_USP_CMD_STATUS_SUCCESS, amxd_status_ok);
}

amxd_status_t _WPS_InitiateWPSPBC(amxd_object_t* object,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* retval) {
    amxd_object_t* pApObj = amxd_object_get_parent(object);
    T_AccessPoint* pAP = NULL;
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    swl_rc_ne rc = SWL_RC_OK;
    amxd_status_t status = amxd_status_ok;
    if((pApObj == NULL) || ((pAP = pApObj->priv) == NULL) || (!debugIsVapPointer(pAP)) || (pAP->pRadio == NULL)) {
        rc = SWL_RC_INVALID_PARAM;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
    } else if(!(pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_PBC_ALL)) {
        SAH_TRACEZ_ERROR(ME, "%s: wps PushButton not supported", pAP->alias);
        rc = SWL_RC_INVALID_STATE;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    } else {
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC));
        pAP->WPS_PBC_Delay.setAct = TRUE;
    }
    if(rc == SWL_RC_OK) {
        status = s_initiateWPS(pAP, retval, &rc);
    }
    if(rc < SWL_RC_OK) {
        wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PBC, NULL);
    }
    return status;
}

amxd_status_t _WPS_InitiateWPSPIN(amxd_object_t* object,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args,
                                  amxc_var_t* retval) {
    amxd_object_t* pApObj = amxd_object_get_parent(object);
    T_AccessPoint* pAP = NULL;
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    const char* clientPIN = GET_CHAR(args, "clientPIN");
    swl_rc_ne rc = SWL_RC_OK;
    amxd_status_t status = amxd_status_ok;
    if((pApObj == NULL) || ((pAP = pApObj->priv) == NULL) || (!debugIsVapPointer(pAP)) || (pAP->pRadio == NULL)) {
        rc = SWL_RC_INVALID_PARAM;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
    } else if(clientPIN == NULL) {
        if((!(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL))) ||
           (swl_str_isEmpty(pAP->pRadio->wpsConst->DefaultPin))) {
            SAH_TRACEZ_ERROR(ME, "%s: wps self PIN not enabled", pAP->alias);
            wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_NO_SELF_PIN, NULL);
            return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
        }
        SAH_TRACEZ_INFO(ME, "%s: Self PIN %s", pAP->alias, pAP->pRadio->wpsConst->DefaultPin);
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY));
        pAP->WPS_PBC_Delay.setAct = TRUE;
    } else if(!(pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_PIN)) {
        SAH_TRACEZ_ERROR(ME, "%s: wps client PIN not enabled", pAP->alias);
        rc = SWL_RC_INVALID_STATE;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    } else {
        char clean_str[strlen(clientPIN) + 1];
        swl_str_copy(clean_str, sizeof(clean_str), clientPIN);

        stripOutToken(clean_str, "-");
        stripOutToken(clean_str, " ");
        /* Check if client PIN is valid! */
        bool isPinValid = wldu_checkWpsPinStr(clean_str);
        if(!isPinValid) {
            SAH_TRACEZ_ERROR(ME, "Client PIN (%s) is not valid!", clean_str);
            wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_INVALID_PIN, NULL);
            return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
        }
        /* Client PIN method is used */
        SAH_TRACEZ_INFO(ME, "%s: Client PIN %s", pAP->alias, clean_str);
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s=%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PIN),
                 clean_str);
        pAP->WPS_PBC_Delay.setAct = TRUE;
    }
    if(rc == SWL_RC_OK) {
        status = s_initiateWPS(pAP, retval, &rc);
    }
    if(rc < SWL_RC_OK) {
        wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PIN, NULL);
    }
    return status;
}

amxd_status_t _WPS_cancelWPSPairing(amxd_object_t* object,
                                    amxd_function_t* func _UNUSED,
                                    amxc_var_t* args _UNUSED,
                                    amxc_var_t* retval _UNUSED) {
    return doCancelPairing(object);
}

