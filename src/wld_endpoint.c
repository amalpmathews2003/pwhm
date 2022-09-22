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

#define ME "ep"

#include <stdlib.h>
#include <debug/sahtrace.h>


#include <string.h>
#include <swl/swl_mac.h>

#include "wld.h"
#include "wld_radio.h"
#include "wld_endpoint.h"
#include "wld_tinyRoam.h"
#include "wld_ssid.h"
#include "wld_wps.h"
#include "wld_util.h"
#include "swl/swl_assert.h"
#include "wld_eventing.h"
#include "wld_assocdev.h"
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_cfgManager.h"
#include "Utils/wld_autoCommitMgr.h"

/* Function prototypes for helpers. */
static void s_setEndpointStatus(T_EndPoint* pEP,
                                wld_intfStatus_e status,
                                wld_epConnectionStatus_e connectionStatus,
                                wld_epError_e error);
static void s_setProfileStatus(T_EndPointProfile* profile, bool connected);
static void endpoint_wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata);
static int endpoint_wps_start(T_EndPoint* pEP, uint64_t call_id, amxc_var_t* args);
static void endpoint_reconnect_handler(amxp_timer_t* timer, void* userdata);

/* Possible Endpoint Status Values */
const char* cstr_EndPoint_status[] = {"Disabled", "Enabled", "Error_Misconfigured", "Error", 0};

/* Possible Endpoint ConnectionStatus values */
const char* cstr_EndPoint_connectionStatus[] = {"Disabled", "Idle", "Discovering", "Connecting",
    "WPS_Pairing", "WPS_PairingDone", "WPS_Timeout", "Connected", "Disconnected", "Error",
    "Error_Misconfigured", NULL};

/* Possible Endpoint LastError values */
const char* cstr_EndPoint_lastError[] = {"None", "SSID_Not_Found", "Invalid_PassPhrase",
    "SecurityMethod_Unsupported", "WPS_Timeout", "WPS_Canceled", "Error_Misconfigured",
    "Association_Timeout", NULL};

/* Possible EndpointProfile Status values */
const char* cstr_EndPointProfile_status[] = {"Active", "Available", "Error", "Disabled", NULL};

int32_t wld_endpoint_isProfileIdentical(T_EndPointProfile* currentProfile, T_EndPointProfile* newProfile) {
    ASSERTS_NOT_NULL(currentProfile, LFALSE, ME, "currentProfile is NULL");
    ASSERTS_NOT_NULL(newProfile, LFALSE, ME, "newProfile is NULL");

    if(memcmp(currentProfile->BSSID, &wld_ether_null, ETHER_ADDR_LEN) != 0) {
        if(memcmp(currentProfile->BSSID, newProfile->BSSID, ETHER_ADDR_LEN) != 0) {
            return LFALSE;
        }
    }

    if(strcmp(currentProfile->SSID, newProfile->SSID)) {
        return LFALSE;
    }

    if(strcmp(currentProfile->keyPassPhrase, newProfile->keyPassPhrase)) {
        return LFALSE;
    }

    if(currentProfile->secModeEnabled != newProfile->secModeEnabled) {
        return LFALSE;
    }

    return LTRUE;
}

/**
 * @brief wld_endpoint_setRadToggleThreshold_pwf
 *
 * Parameter handler to handle Endpoint radio toggle parameter
 *
 * @param parameter endpoint threshold parameter
 * @param oldvalue previous value
 * @return  true on success, otherwise false
 */
amxd_status_t _wld_endpoint_setRadToggleThreshold_pwf(amxd_object_t* object _UNUSED,
                                                      amxd_param_t* parameter,
                                                      amxd_action_t reason _UNUSED,
                                                      const amxc_var_t* const args _UNUSED,
                                                      amxc_var_t* const retval _UNUSED,
                                                      void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "EndpointRadToggleThreshold changed");

    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "NULL");
        return amxd_status_ok;
    }

    pEP->reconnect_rad_trigger = amxc_var_dyncast(uint32_t, args);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setReconnectDelay_pwf
 *
 * Parameter handler to handle Endpoint reconnect delay time parameter
 *
 * @param parameter endpoint timeout parameter
 * @param oldvalue previous value
 * @return  true on success, otherwise false
 */
amxd_status_t _wld_endpoint_setReconnectDelay_pwf(amxd_object_t* object _UNUSED,
                                                  amxd_param_t* parameter,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args _UNUSED,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERTI_NOT_NULL(pEP, amxd_status_ok, ME, "NULL");

    pEP->reconnectDelay = amxc_var_dyncast(uint32_t, args);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setReconnectInterval_pwf
 *
 * Parameter handler to handle Endpoint reconnect interval time parameter
 *
 * @param parameter endpoint timeout parameter
 * @param oldvalue previous value
 * @return  true on success, otherwise false
 */
amxd_status_t _wld_endpoint_setReconnectInterval_pwf(amxd_object_t* object _UNUSED,
                                                     amxd_param_t* parameter,
                                                     amxd_action_t reason _UNUSED,
                                                     const amxc_var_t* const args _UNUSED,
                                                     amxc_var_t* const retval _UNUSED,
                                                     void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "EndpointReconnectInterval changed");

    ASSERTS_NOT_NULL(pEP, amxd_status_ok, ME, "NULL");

    pEP->reconnectInterval = amxc_var_dyncast(uint32_t, args);
    amxp_timer_set_interval(pEP->reconnectTimer, pEP->reconnectInterval * 1000);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_endpoint_setMultiAPEnable_pwf(amxd_object_t* object _UNUSED,
                                                 amxd_param_t* parameter,
                                                 amxd_action_t reason _UNUSED,
                                                 const amxc_var_t* const args _UNUSED,
                                                 amxc_var_t* const retval _UNUSED,
                                                 void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_multiAPEnable = amxc_var_dyncast(bool, args);
    ASSERT_TRUE(debugIsEpPointer(pEP), amxd_status_ok, ME, "INVALID");
    if(pEP->multiAPEnable != new_multiAPEnable) {
        pEP->multiAPEnable = new_multiAPEnable;
        pEP->pFA->mfn_wendpoint_multiap_enable(pEP);
        wld_autoCommitMgr_notifyEpEdit(pEP);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setProfileReference_pwf
 *
 * Set the Current Profile
 *
 * @param parameter profile reference parameter
 * @param oldvalue previous value
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_setProfileReference_pwf(amxd_object_t* object _UNUSED,
                                                    amxd_param_t* parameter,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args _UNUSED,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    if(!pEP) {
        //Not having an EndPoint yet is a valid scenario here, but becomes an
        //error later on in the function.
        return amxd_status_ok;
    }

    if(pEP->internalChange) {
        return amxd_status_ok;
    }

    const char* profileRef = amxc_var_constcast(cstring_t, args);
    if(swl_str_isEmpty(profileRef)) {
        SAH_TRACEZ_WARNING(ME, "profileRef is not yet set");
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_INFO(ME, "setProfileReference - %s", profileRef);

    bool credentialsChanged = false;
    amxc_llist_it_t* it = NULL;
    T_EndPointProfile* oldProfile = pEP->currentProfile;
    pEP->currentProfile = NULL;
    SAH_TRACEZ_INFO(ME, "Number of Endpoint Profiles detected : [%zd]", amxc_llist_size(&pEP->llProfiles));

    for(it = amxc_llist_get_first(&pEP->llProfiles); it; it = amxc_llist_it_get_next(it)) {
        T_EndPointProfile* profile = (T_EndPointProfile*) amxc_llist_it_get_data(it, T_EndPointProfile, it);
        if(!profile) {
            SAH_TRACEZ_ERROR(ME, "Failed to retrieve the T_EndPointProfile struct from the Linked List");
            continue;
        }

        if(!swl_str_matches(profileRef, profile->reference)) {
            SAH_TRACEZ_INFO(ME, "profileRef [%s] != [%s] -> next", profileRef, profile->reference);
            continue;
        }

        int comparison = wld_endpoint_isProfileIdentical(oldProfile, profile);
        if(comparison < 0) {
            SAH_TRACEZ_INFO(ME, "Profile is not identical %i", comparison);
            credentialsChanged = true;
        } else {
            SAH_TRACEZ_INFO(ME, "Profile is identical, don't disconnect");
        }

        SAH_TRACEZ_INFO(ME, "Profile reference found - Setting Current Profile for [%s]", profileRef);
        pEP->currentProfile = profile;
        break;
    }

    if(!pEP->currentProfile) {
        SAH_TRACEZ_ERROR(ME, "No profile found matching the profileRef [%s]", profileRef);
        return amxd_status_unknown_error;
    }

    wldu_copyStr(pEP->pSSID->SSID, pEP->currentProfile->SSID, sizeof(pEP->pSSID->SSID));
    syncData_SSID2OBJ(pEP->pSSID->pBus, pEP->pSSID, SET);
    if(credentialsChanged) {
        endpointReconfigure(pEP);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


/**
 * The function is used to map an interface on a bridge. Some vendor deamons
 * depends on it for proper functionallity.
 */
amxd_status_t _wld_endpoint_setBridgeInterface_pwf(amxd_object_t* object _UNUSED,
                                                   amxd_param_t* parameter,
                                                   amxd_action_t reason _UNUSED,
                                                   const amxc_var_t* const args _UNUSED,
                                                   amxc_var_t* const retval _UNUSED,
                                                   void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* pstr_BridgeName = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set BridgeInterface %s", pstr_BridgeName);

    ASSERT_TRUE(debugIsEpPointer(pEP), amxd_status_ok, ME, "INVALID");
    ASSERT_NOT_NULL(pstr_BridgeName, amxd_status_ok, ME, "NULL");

    snprintf(pEP->bridgeName, sizeof(pEP->bridgeName), "%s", pstr_BridgeName);
    endpointReconfigure(pEP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_writeStats(T_EndPoint* pEP, uint64_t call_id _UNUSED, amxc_var_t* variant, T_EndPointStats* stats) {
    amxd_object_t* object = amxd_object_get(pEP->pBus, "Stats");
    wld_update_endpoint_stats(object, stats);
    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);
    //Add data
    amxc_var_move(variant, &map);
    amxc_var_clean(&map);
}

static void s_writeAssocStats(T_EndPoint* pEP) {
    amxd_object_t* object = amxd_object_get(pEP->pBus, "AssocStats");
    amxd_object_set_uint32_t(object, "NrAssocAttempts", pEP->assocStats.nrAssocAttempts);
    amxd_object_set_uint32_t(object, "NrAssocAttempsSinceDc", pEP->assocStats.nrAssocAttemptsSinceDc);
    amxd_object_set_uint32_t(object, "NrAssociations", pEP->assocStats.nrAssociations);
    amxd_object_set_uint32_t(object, "AssocTime", pEP->assocStats.lastAssocTime);
    amxd_object_set_uint32_t(object, "DisassocTime", pEP->assocStats.lastDisassocTime);
}

void wld_endpoint_writeStats(T_EndPoint* pEP, T_EndPointStats* stats, bool success) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_TRUE(pEP->statsCall.running, , ME, "%s no fcall", pEP->Name);
    pEP->statsCall.running = false;

    ASSERT_NOT_EQUALS(pEP->statsCall.call_id, 0, , ME, "%s no fcall", pEP->Name);

    amxc_var_t values;
    amxc_var_init(&values),
    amxc_var_set_type(&values, AMXC_VAR_ID_LIST);
    if(success) {
        s_writeStats(pEP, pEP->statsCall.call_id, &values, stats);
    }
    amxd_function_deferred_done(pEP->statsCall.call_id, success ? amxd_status_ok : amxd_status_unknown_error, NULL, success ? &values : NULL);
    amxc_var_clean(&values);
    pEP->statsCall.call_id = 0;
}

/*
   static void s_stats_cancel_handler(function_call_t* fcall, void* userdata _UNUSED) {
    T_EndPoint* pEP = fcall_userData(fcall);
    ASSERT_TRUE(debugIsEpPointer(pEP), , ME, "INVALID");
    SAH_TRACEZ_INFO(ME, "%s: stats call cancelled", pEP->Name);
    pEP->statsCall.fcall = NULL;
   }
 */

/**
 * @brief getStats
 *
 * Collect EndPoint statistics and update Stats object
 * @return true on success, false otherwise
 */
amxd_status_t _getStats(amxd_object_t* object,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* retval) {
    T_EndPoint* pEP = object->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");
    ASSERT_FALSE(pEP->statsCall.running, amxd_status_unknown_error, ME, "Stats call running");

    uint64_t call_id = amxc_var_constcast(uint64_t, retval);
    T_EndPointStats stats;
    memset(&stats, 0, sizeof(T_EndPointStats));
    swl_rc_ne retVal = pEP->pFA->mfn_wendpoint_stats(pEP, &stats);

    if(retVal == SWL_RC_OK) {
        s_writeStats(pEP, call_id, retval, &stats);
        return amxd_status_ok;
    } else if(retVal == SWL_RC_CONTINUE) {
        pEP->statsCall.running = true;
        pEP->statsCall.call_id = call_id;
        SAH_TRACEZ_OUT(ME);
        return amxd_status_deferred;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s : failed stats", pEP->Name);
        return amxd_status_unknown_error;
    }
}

/**
 * @brief setEndPointProfileDefaults
 *
 * Set some default settings for the Endpoint Profile
 * Used when the profile is just created
 *
 * @param profile Endpoint profile
 */
void setEndPointProfileDefaults(T_EndPointProfile* profile) {
    SAH_TRACEZ_INFO(ME, "Setting EndpointProfile Defaults");

    profile->enable = 0;
    profile->priority = 0;
    profile->status = EPPS_DISABLED;
    memset(profile->SSID, 0, sizeof(profile->SSID));
    memset(profile->BSSID, 0, sizeof(profile->BSSID));
    memset(profile->keyPassPhrase, 0, sizeof(profile->keyPassPhrase));
    memset(profile->alias, 0, sizeof(profile->alias));
    memset(profile->location, 0, sizeof(profile->location));
    memset(profile->WEPKey, 0, sizeof(profile->WEPKey));
    memset(profile->preSharedKey, 0, sizeof(profile->preSharedKey));
    profile->secModeEnabled = APMSI_NONE;
}

/**
 * @brief setEndpointCurrentProfile
 *
 * Set the current profile when a matching profile reference is set
 * Used when the profile is just created
 *
 * @param endpointObject Endpoint object
 * @param Profile new endpoint profile
 */
void setEndpointCurrentProfile(amxd_object_t* endpointObject, T_EndPointProfile* Profile) {
    ASSERT_NOT_NULL(endpointObject, , ME, "NULL");
    ASSERT_NOT_NULL(Profile, , ME, "NULL");

    T_EndPoint* EndPoint = (T_EndPoint*) endpointObject->priv;
    ASSERT_NOT_NULL(EndPoint, , ME, "NULL");

    const char* ProfileReference = amxd_object_get_cstring_t(endpointObject, "ProfileReference", NULL);
    if(ProfileReference == NULL) {
        SAH_TRACEZ_INFO(ME, "Profile Reference is not yet set - not setting currentProfile");
        return;
    }

    if(!swl_str_matches(ProfileReference, Profile->reference)) {
        SAH_TRACEZ_NOTICE(ME, "Profile Instance name [%s] does not match the ProfileReference [%s] - not setting currentProfile",
                          Profile->reference, ProfileReference);
        return;
    }

    EndPoint->currentProfile = Profile;
}

/**
 * @brief destroyEndPointProfile
 *
 * Cleanup the profile struct
 * Used when the profile instance is deleted
 *
 * @param Profile endpoint profile struct
 */
void destroyEndPointProfile(T_EndPointProfile* Profile) {
    free(Profile->reference);
    free(Profile);
}

/**
 * @brief setEndPointDefaults
 *
 * Set some default settings for the Endpoint
 * Used when the endpoint is just created
 *
 * @param EndPoint pointer to the struct
 * @param endpointname name of the endpoint
 * @param intfname name of interface of the endpoint
 * @param idx index number given to the endpoint
 */
void setEndPointDefaults(T_EndPoint* pEP, const char* endpointname, const char* intfname, int idx) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Setting Endpoint Defaults", pEP->alias);


    wldu_copyStr(pEP->alias, endpointname, sizeof(pEP->alias));
    wldu_copyStr(pEP->Name, intfname, sizeof(pEP->Name));
    pEP->index = idx;

    pEP->status = APSTI_DISABLED;
    pEP->connectionStatus = EPCS_DISABLED;
    pEP->error = EPE_NONE;
    pEP->enable = APSTI_DISABLED;

    pEP->reconnectDelay = 15;
    pEP->reconnectInterval = 15;

    pEP->WPS_Enable = true;

    pEP->WPS_ConfigMethodsSupported = (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL | M_WPS_CFG_MTHD_PBC_ALL | M_WPS_CFG_MTHD_PIN);
    pEP->WPS_ConfigMethodsEnabled = (M_WPS_CFG_MTHD_PBC | M_WPS_CFG_MTHD_DISPLAY);
    pEP->secModesSupported = 0;
    pEP->secModesSupported |= (pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "WEP", 0)) ?
        (APMS_WEP64 | APMS_WEP128 | APMS_WEP128IV) : 0;
    pEP->secModesSupported |= (pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "AES", 0)) ?
        (APMS_WPA_P | APMS_WPA2_P | APMS_WPA_WPA2_P) : 0;
    pEP->secModesSupported |= (pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "SAE", 0)) ?
        (APMS_WPA3_P | APMS_WPA2_WPA3_P) : 0;
}

/**
 * @brief t_destroy_handler_EP
 *
 * Clear our attached memory structure on the endpoint instance
 *
 * @param object endpoint object
 */
void t_destroy_handler_EP(amxd_object_t* object) {
    T_EndPoint* pEP = (T_EndPoint*) object->priv;
    ASSERT_TRUE(debugIsEpPointer(pEP), , ME, "INVALID");

    SAH_TRACEZ_INFO(ME, "%s: destroy EP", pEP->Name);

    wld_tinyRoam_cleanup(pEP);

    amxp_timer_delete(&pEP->reconnectTimer);

    free(pEP);
    object->priv = NULL;
}

/**
 * @brief wld_endpoint_deleteProfileInstance_odf
 *
 * Delete handler on the Profile template object
 * Remove the linked profile data
 *
 * @param template_object
 * @param instance_object
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_deleteProfileInstance_odf(amxd_object_t* template_object, amxd_object_t* instance_object) {
    amxd_object_t* endpointObject = NULL;
    T_EndPointProfile* Profile = NULL;
    T_EndPoint* EndPoint = NULL;

    SAH_TRACEZ_IN(ME);

    endpointObject = amxd_object_get_parent(template_object);
    EndPoint = (T_EndPoint*) endpointObject->priv;
    if(!EndPoint) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Endpoint structure");
        return amxd_status_unknown_error;
    }

    Profile = (T_EndPointProfile*) instance_object->priv;
    if(!Profile) {
        SAH_TRACEZ_ERROR(ME, "Failed to find EndpointProfile structure");
        return amxd_status_unknown_error;
    }

    if(EndPoint->currentProfile == Profile) {
        EndPoint->currentProfile = NULL;
        SAH_TRACEZ_WARNING(ME, "Current Active EndpointProfile gets deleted !!!");
        EndPoint->internalChange = true;
        amxd_object_set_cstring_t(EndPoint->pBus, "ProfileReference", "");
        EndPoint->internalChange = false;
        endpointReconfigure(EndPoint);
    }

    amxc_llist_it_take(&Profile->it);
    destroyEndPointProfile(Profile);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_addProfileInstance_ocf
 *
 * Endpoint Profile instance Add Handler
 * Create a new EndpointProfile Instance
 * and set its defaults
 *
 * @param template_object EndpointProfile template object
 * @param instance_object EndpointProfile instance object
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_addProfileInstance_ocf(amxd_object_t* template_object, amxd_object_t* instance_object) {
    amxd_object_t* endpointObject = NULL;
    T_EndPointProfile* Profile = NULL;
    T_EndPoint* EndPoint = NULL;

    SAH_TRACEZ_IN(ME);

    endpointObject = amxd_object_get_parent(template_object);
    EndPoint = (T_EndPoint*) endpointObject->priv;
    if(!EndPoint) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Endpoint structure");
        return amxd_status_unknown_error;
    }

    /* Set the T_EndPointProfile struct to the new instance */
    Profile = (T_EndPointProfile*) calloc(1, sizeof(T_EndPointProfile));
    if(!Profile) {
        SAH_TRACEZ_ERROR(ME, "Failed to allocate T_EndpointProfile structure");
        return amxd_status_unknown_error;
    }

    instance_object->priv = Profile;
    Profile->pBus = instance_object;

    /* Interlinking */
    Profile->reference = strdup(amxd_object_get_name(instance_object, AMXD_OBJECT_NAMED));
    amxc_llist_append(&EndPoint->llProfiles, &Profile->it);
    Profile->endpoint = EndPoint;

    /* Set some defaults to the endpoint profile struct */
    setEndPointProfileDefaults(Profile);

    /* Set the current profile when a matching profile reference is set */
    setEndpointCurrentProfile(endpointObject, Profile);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief syncData_Object2EndPointProfile
 *
 * Sync Datamodel Endpoint Profile object parameters to
 * the T_EndpointProfile struct
 *
 * @param object Endpoint profile object instance
 * @return true if a parameter of the profile has changed
 * that should trigger a change in the connection. false
 * otherwise.
 */
bool syncData_Object2EndPointProfile(amxd_object_t* object) {
    T_EndPointProfile* pProfile = NULL;
    T_EndPoint* pEP = NULL;
    bool changed = false;
    uint8_t BSSID[6] = {'\0'};
    uint8_t tmp_priority = 0;

    char rollbackBuffer[256] = {'\0'};

    amxd_object_t* secObj = amxd_object_findf(object, "Security");

    pProfile = (T_EndPointProfile*) object->priv;
    if(!pProfile) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Profile structure");
        return false;
    }

    pEP = pProfile->endpoint;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Endpoint struct within Profile");
        return false;
    }

    tmp_priority = amxd_object_get_uint8_t(object, "Priority", NULL);
    if(tmp_priority != pProfile->priority) {
        changed |= 1;
        pProfile->priority = tmp_priority;
    }

    const char* ssid = amxd_object_get_cstring_t(object, "SSID", NULL);
    if(strncmp(ssid, pProfile->SSID, SSID_NAME_LEN)) {
        changed |= 1;
        wldu_copyStr(pProfile->SSID, ssid, sizeof(pProfile->SSID));
    }

    const char* bssid = amxd_object_get_cstring_t(object, "ForceBSSID", NULL);
    if((bssid == 0) || (bssid[0] == 0)
       || !wldu_convStr2Mac((unsigned char*) BSSID, sizeof(BSSID), bssid, strlen(bssid))) {
        memcpy(pProfile->BSSID, wld_ether_null, sizeof(pProfile->BSSID));
    } else {
        changed |= !!memcmp(pProfile->BSSID, BSSID, sizeof(pProfile->BSSID));
        memcpy(pProfile->BSSID, BSSID, sizeof(pProfile->BSSID));
    }

    //These 2 parameters have no effect on the actual connection and should not cause
    //a connection change.
    wldu_copyStr(pProfile->alias, amxd_object_get_cstring_t(object, "Alias", NULL),
                 sizeof(pProfile->alias));
    wldu_copyStr(pProfile->location, amxd_object_get_cstring_t(object, "Location", NULL),
                 sizeof(pProfile->location));

    const char* mode = amxd_object_get_cstring_t(secObj, "ModeEnabled", NULL);
    if(!strncmp(mode, rollbackBuffer, 256)) {
        rollbackBuffer[0] = '\0';
    } else {
        wldu_copyStr(rollbackBuffer, mode, sizeof(rollbackBuffer));
    }

    wld_securityMode_e secMode = conv_strToEnum(cstr_AP_ModesSupported, rollbackBuffer, APMSI_MAX, APMSI_UNKNOWN);
    if(secMode != pProfile->secModeEnabled) {
        changed = true;
        pProfile->secModeEnabled = secMode;
    }

    const char* wepKey = amxd_object_get_cstring_t(secObj, "WEPKey", NULL);
    if(isModeWEP(pProfile->secModeEnabled)) {
        if(!isValidWEPKey(wepKey)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - WEPKey is not in a valid format");
        } else {
            changed |= 1;
            wldu_copyStr(pProfile->WEPKey, wepKey, sizeof(pProfile->WEPKey));
        }
    }

    const char* pskKey = amxd_object_get_cstring_t(secObj, "PreSharedKey", NULL);
    if(isModeWPAPersonal(pProfile->secModeEnabled) &&
       strncmp(pskKey, pProfile->preSharedKey, sizeof(pProfile->preSharedKey))) {
        if(!isValidPSKKey(pskKey)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - PreSharedKey is not in a valid format");
        } else {
            changed |= 1;
            wldu_copyStr(pProfile->preSharedKey, pskKey, sizeof(pProfile->preSharedKey));
        }
    }

    const char* keyPassPhrase = amxd_object_get_cstring_t(secObj, "KeyPassPhrase", NULL);
    if(isModeWPAPersonal(pProfile->secModeEnabled) &&
       strncmp(keyPassPhrase, pProfile->preSharedKey, sizeof(pProfile->preSharedKey))) {
        if(!isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - PreSharedKey is not in a valid format");
        } else {
            changed |= !(wldu_key_matches(pProfile->SSID, pProfile->keyPassPhrase, keyPassPhrase));
            wldu_copyStr(pProfile->keyPassPhrase, keyPassPhrase, sizeof(pProfile->keyPassPhrase));
        }
    }

    const char* mfp = amxd_object_get_cstring_t(secObj, "MFPConfig", NULL);
    if(strncmp(mfp, wld_mfpConfig_str[pProfile->mfpConfig],
               strlen(wld_mfpConfig_str[pProfile->mfpConfig]))) {
        wld_mfpConfig_e idx = conv_strToEnum(wld_mfpConfig_str, mfp, WLD_MFP_MAX, WLD_MFP_DISABLED);
        if(idx != pProfile->mfpConfig) {
            pProfile->mfpConfig = idx;
            changed |= 1;
        }
    }

    return changed;
}

/**
 * @brief writeEndpontProfile
 *
 * Write handler on the Endpoint Profile Template Object
 *
 * Sync object data to the EndPontProfile structure
 *
 * @param object Endpoint Profile Object Instance
 */
amxd_status_t _wld_endpoint_setProfile_owf(amxd_object_t* object) {
    T_EndPointProfile* pProfile = NULL;
    T_EndPoint* pEP = NULL;
    bool profileEnable_changed = false;
    bool profile_changed = false;

    SAH_TRACEZ_IN(ME);

    if(amxd_object_get_type(object) == amxd_object_template) {
        SAH_TRACEZ_INFO(ME, "Initial template run, skip");
        goto leave;
    }

    pProfile = (T_EndPointProfile*) object->priv;
    if(!pProfile) {
        SAH_TRACEZ_WARNING(ME, "Profile is not yet set");
        goto leave;
    }

    if(amxd_object_get_type(object) != amxd_object_instance) {
        SAH_TRACEZ_ERROR(ME, "Provided object is not an endpoint profile instance");
        goto leave;
    }

    pEP = pProfile->endpoint;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Endpoint structure");
        goto leave;
    }

    if(pEP->internalChange) {
        goto leave;
    }

    SAH_TRACEZ_INFO(ME, "Syncing : EndpointProfile enable parameter --> T_EndpointProfile");
    bool tmp_res = amxd_object_get_bool(object, "Enable", NULL);
    if(tmp_res != pProfile->enable) {
        profileEnable_changed = true;
        pProfile->enable = tmp_res;
    }

    SAH_TRACEZ_INFO(ME, "Syncing : EndpointProfile object --> T_EndpointProfile");
    profile_changed = syncData_Object2EndPointProfile(object);

    if(pEP->currentProfile != pProfile) {
        SAH_TRACEZ_INFO(ME, "Changed profile is not currently selected : done");
        goto leave;
    }

    if(!profileEnable_changed && !profile_changed) {
        SAH_TRACEZ_INFO(ME, "Profile enabled field and profile parameters not changed : done");
        goto leave;
    }

    if(profileEnable_changed) {
        s_setProfileStatus(pProfile, false);
    }

    if(profile_changed || profileEnable_changed) {
        endpointReconfigure(pEP);
    }

leave:
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setProfileSecurity_owf
 *
 * Write handler on the Endpoint Profile Security Object
 * Sync object data to the EndPontProfile structure
 *
 * @param object Endpoint Profile Security Object
 */
amxd_status_t _wld_endpoint_setProfileSecurity_owf(amxd_object_t* object) {
    bool profile_changed = false;
    amxd_object_t* profileObject = amxd_object_get_parent(object);

    if(amxd_object_get_type(profileObject) == amxd_object_template) {
        SAH_TRACEZ_INFO(ME, "Initial template run, skip");
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_IN(ME);
    T_EndPointProfile* pProfile = (T_EndPointProfile*) profileObject->priv;
    if(!pProfile) {
        SAH_TRACEZ_WARNING(ME, "Profile is not yet set");
        return amxd_status_unknown_error;
    }

    T_EndPoint* pEP = pProfile->endpoint;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");
    profile_changed = syncData_Object2EndPointProfile(profileObject) && (pProfile == pEP->currentProfile);

    if(profile_changed) {
        endpointReconfigure(pEP);
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setWPSEnable_pwf
 *
 * Write handler on the Endpoint WPS "Enable" parameter
 *
 * @param parameter Endpoint WPS Enable parameter
 * @param oldvalue previous value
 * @return true on success, false on failure
 */
amxd_status_t _wld_endpoint_setWPSEnable_pwf(amxd_object_t* object _UNUSED,
                                             amxd_param_t* parameter,
                                             amxd_action_t reason _UNUSED,
                                             const amxc_var_t* const args _UNUSED,
                                             amxc_var_t* const retval _UNUSED,
                                             void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool enable = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "EndpointWPSEnable changed");

    /* It's up to the RADIO settings when both (STA & AP) have WPS active set. */
    if(pEP && debugIsEpPointer(pEP)) {
        T_Radio* pR = pEP->pRadio;
        pEP->WPS_Enable = enable;

        /* Ignore and give warning when not properly set */
        if(pR->isSTA && pR->isWPSEnrol) {
            pR->fsmRad.FSM_SyncAll = TRUE;
        } else {
            SAH_TRACEZ_INFO(ME, "ignore WPS change - Radio STA mode (%d) and Enrollee (%d) not correct set",
                            pR->isSTA,
                            pR->isWPSEnrol);
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief wld_endpoint_setWPSConfigMethodsEnabled_pwf
 *
 * Write handler on the Endpoint WPS "ConfigMethodsEnabled" parameter
 *
 * @param parameter Endpoint WPS ConfigMethodsEnabled parameter
 * @param oldvalue previous value
 * @return true on success, false on failure
 */
amxd_status_t _wld_endpoint_setWPSConfigMethodsEnabled_pwf(amxd_object_t* object _UNUSED,
                                                           amxd_param_t* parameter,
                                                           amxd_action_t reason _UNUSED,
                                                           const amxc_var_t* const args _UNUSED,
                                                           amxc_var_t* const retval _UNUSED,
                                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    bool wps_cfgm = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "EndpointWPSConfigMethodsEnabled changed");

    /* It's up to the RADIO settings when both (STA & AP) have WPS active set. */
    if(pEP && debugIsEpPointer(pEP)) {
        T_Radio* pR = pEP->pRadio;
        pEP->WPS_ConfigMethodsEnabled = wps_cfgm;

        /* Ignore and give warning when not properly set */
        if(pR->isSTA && pR->isWPSEnrol) {
            pR->fsmRad.FSM_SyncAll = TRUE;
        } else {
            SAH_TRACEZ_INFO(ME, "ignore WPS change - Radio STA mode (%d) and Enrollee (%d) not correct set",
                            pR->isSTA,
                            pR->isWPSEnrol);
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;

}


/**
 * Check whether endpoint is ready for enable.
 * This means:
 * * Endpoint enabled
 * * Radio enabled
 * * Profile present
 * * Profile Enabled
 */
bool wld_endpoint_isReady(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, false, ME, "NULL");
    uint32_t mask = 0;
    T_Radio* pRad = pEP->pRadio;
    W_SWL_BIT_WRITE(mask, 0, !pEP->enable);
    W_SWL_BIT_WRITE(mask, 1, !pRad->enable);
    W_SWL_BIT_WRITE(mask, 2, pEP->currentProfile == NULL);
    if(pEP->currentProfile != NULL) {
        W_SWL_BIT_WRITE(mask, 3, !pEP->currentProfile->enable);
    }
    SAH_TRACEZ_INFO(ME, "%s: check ready 0x%2x", pEP->Name, mask);

    return mask == 0;
}

/**
 * @brief wld_endpoint_setEnable_pwf
 *
 * Write handler on the Endpoint "Enable" parameter
 *
 * @param parameter Endpoint Enable parameter
 * @param oldvalue previous value
 * @return true on success, otherwise false
 */
amxd_status_t _wld_endpoint_setEnable_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiEp = amxd_object_get_parent(amxd_param_get_owner(parameter));
    if(amxd_object_get_type(wifiEp) != amxd_object_instance) {
        return rv;
    }
    T_EndPoint* pEP = (T_EndPoint*) wifiEp->priv;
    rv = amxd_action_param_write(wifiEp, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    if(!pEP) {
        SAH_TRACEZ_WARNING(ME, "Endpoint is not yet set");
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_IN(ME);
    bool enable = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "%s: enable %d", pEP->alias, enable);

    T_Radio* pRad = pEP->pRadio;
    if(!pRad) {
        SAH_TRACEZ_ERROR(ME, "Failed to get the T_Radio struct from Endpoint [%s]", pEP->alias);
        return amxd_status_unknown_error;
    }

    /* Sync datamodel to internal endpoint struct */
    if(enable) {
        syncData_OBJ2EndPoint(wifiEp);
    }

    /* set enable flag */
    pRad->pFA->mfn_wendpoint_enable(pEP, enable);

    endpointReconfigure(pEP);

    /* when idle : kick the FSM state machine to handle the new state */
    wld_rad_doCommitIfUnblocked(pRad);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief syncData_OBJ2EndPoint
 *
 * Read the Endpoint settings from the datamodel
 * and set them to the T_EndPoint struct
 *
 * @param object Endpoint object instance
 * @return
 *      - true  : on success
 *      - false : when a system error occured or an invalid configuration is set
 */
bool syncData_OBJ2EndPoint(amxd_object_t* object) {
    bool retval = true;

    if(!object || (amxd_object_get_type(object) != amxd_object_instance)) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function");
        return false;
    }

    T_EndPoint* pEP = (T_EndPoint*) object->priv;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Failed to get the T_EndPoint struct");
        return false;
    }

    SAH_TRACEZ_INFO(ME, "Synchonize Endpoint Object");

    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(amxd_object_get_param_def(object, "Enable"), &value);
    pEP->enable = amxc_var_get_int32_t(&value);
    amxc_var_clean(&value);
    SAH_TRACEZ_INFO(ME, "Endpoint.enable=%d", pEP->enable);

    const char* alias = amxd_object_get_cstring_t(object, "Alias", NULL);
    if(alias != NULL) {
        wldu_copyStr(pEP->alias, alias, sizeof(pEP->alias));
    }

    /* WPS */
    amxd_object_t* const wpsobject = amxd_object_get(object, "WPS");
    if(!wpsobject) {
        SAH_TRACEZ_ERROR(ME, "WPS object not found");
        return false;
    }

    pEP->WPS_Enable = amxd_object_get_bool(wpsobject, "Enable", NULL);
    SAH_TRACEZ_INFO(ME, "Endpoint.WPS.enable = %d", pEP->WPS_Enable);

    const char* WPS_ConfigMethodsEnabled = amxd_object_get_cstring_t(wpsobject, "ConfigMethodsEnabled", NULL);
    if(!wld_wps_ConfigMethods_string_to_mask((uint32_t*) &pEP->WPS_ConfigMethodsEnabled, WPS_ConfigMethodsEnabled, ',')) {
        SAH_TRACEZ_ERROR(ME, "Invalid WPS ConfigMethodsEnabled: '%s'", WPS_ConfigMethodsEnabled);
        retval = false;
    }

    return retval;
}

/**
 * @brief syncData_EndPoint2OBJ
 *
 * Read out the internal T_EndPoint struct
 * and set it to the datamodel
 *
 * Note : NO COMMIT
 *
 * @param EndPoint T_EndPoint struct
 */
void syncData_EndPoint2OBJ(T_EndPoint* pEP) {
    char TBuf[256];

    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : EndPoint=NULL");
        return;
    }

    amxd_object_t* object = pEP->pBus;
    if(!object) {
        SAH_TRACEZ_ERROR(ME, "Endpoint datamodel object pointer is missing");
        return;
    }

    SAH_TRACEZ_INFO(ME, "Syncing Endpoint to Datamodel");

    amxd_object_set_cstring_t(object, "Status", cstr_EndPoint_status[pEP->status]);
    amxd_object_set_cstring_t(object, "ConnectionStatus",
                              cstr_EndPoint_connectionStatus[pEP->connectionStatus]);
    amxd_object_set_cstring_t(object, "LastError", cstr_EndPoint_lastError[pEP->error]);
    amxd_object_set_cstring_t(object, "Alias", pEP->alias);
    amxd_object_set_int32_t(object, "Index", pEP->index);
    amxd_object_set_cstring_t(object, "IntfName", pEP->Name);

    if(pEP->currentProfile) {
        amxd_object_set_cstring_t(object, "ProfileReference", pEP->currentProfile->alias);
    } else {
        amxd_object_set_cstring_t(object, "ProfileReference", "");
    }

    sprintf(TBuf, "%s.%s", "SSID", pEP->alias);
    amxd_object_set_cstring_t(object, "SSIDReference", TBuf);

    wld_bitmaskToCSValues(TBuf, sizeof(TBuf), pEP->secModesSupported, cstr_AP_ModesSupported);
    SAH_TRACEZ_INFO(ME, "Security.ModesSupported=%s", TBuf);
    amxd_object_set_cstring_t(amxd_object_findf(object, "Security"), "ModesSupported", TBuf);

    amxd_object_t* wpsObj = amxd_object_findf(object, "WPS");
    amxd_object_set_int32_t(wpsObj, "Enable", pEP->WPS_Enable);

    wld_wps_ConfigMethods_mask_to_charBuf(TBuf, sizeof(TBuf), pEP->WPS_ConfigMethodsSupported);
    amxd_object_set_cstring_t(wpsObj, "ConfigMethodsSupported", TBuf);

    wld_wps_ConfigMethods_mask_to_charBuf(TBuf, sizeof(TBuf), pEP->WPS_ConfigMethodsEnabled);
    amxd_object_set_cstring_t(wpsObj, "ConfigMethodsEnabled", TBuf);
}

void wld_endpoint_setConnectionStatus(T_EndPoint* pEP, wld_epConnectionStatus_e connectionStatus, wld_epError_e error) {
    wld_intfStatus_e status = APSTI_ENABLED;
    if(connectionStatus == EPCS_ERROR) {
        status = APSTI_ERROR;
    } else if(connectionStatus == EPCS_DISABLED) {
        status = APSTI_DISABLED;
    }

    bool connected = (connectionStatus == EPCS_CONNECTED);

    SAH_TRACEZ_INFO(ME, "%s: set status connStat %u(%u) err %u stat %u(%u) conn %u",
                    pEP->alias,
                    connectionStatus, pEP->connectionStatus,
                    error, status, pEP->status, connected);

    if(pEP->currentProfile) {
        s_setProfileStatus(pEP->currentProfile, connected);
        if(connected) {
            //update enpoint's SSID  with the current connected profile.
            wldu_copyStr(pEP->pSSID->SSID, pEP->currentProfile->SSID, sizeof(pEP->pSSID->SSID));
        }
    }

    int previousStatus = pEP->connectionStatus;
    s_setEndpointStatus(pEP, status, connectionStatus, error);

    if(!connected && wld_endpoint_isReady(pEP)) {
        //We are not connected, but we should be. Start the reconnection
        //timer.

        uint32_t targetInterval = (previousStatus == EPCS_CONNECTED) ? pEP->reconnectDelay * 1000 : pEP->reconnectInterval * 1000;
        amxp_timer_state_t timerState = amxp_timer_get_state(pEP->reconnectTimer);
        if((timerState != amxp_timer_running) && (timerState != amxp_timer_started)) {
            amxp_timer_start(pEP->reconnectTimer, targetInterval);
        }
    } else {
        pEP->reconnect_count = 0;
        amxp_timer_stop(pEP->reconnectTimer);
    }
}

/**
 * @brief wld_endpoint_sync_connection
 *
 * Will fill in the datamodel of the endpoint and the profile based on the
 * connection status. Will also start the reconnect timer if the profile and
 * the endpoint are enabled, but we are not connected.
 *
 * @param pEP The endpoint
 * @param connected The connected status of the endpoint.
 * @param error The current error of the endpoint (0 if no error present)
 */
void wld_endpoint_sync_connection(T_EndPoint* pEP, bool connected, wld_epError_e error) {
    wld_epConnectionStatus_e connectionStatus;

    SAH_TRACEZ_INFO(ME, "%s: enable %d; connected; %d error %d",
                    pEP->alias, pEP->enable, connected, error);

    T_Radio* pR = pEP->pRadio;

    if(error) {
        connectionStatus = (error == EPE_SSID_NOT_FOUND) || (error == EPE_ERROR_MISCONFIGURED) ? EPCS_IDLE : EPCS_ERROR;
    } else if(pEP->enable && pR->enable) {
        if(connected) {
            connectionStatus = EPCS_CONNECTED;
        } else {
            connectionStatus = EPCS_IDLE;
        }
    } else {
        connectionStatus = EPCS_DISABLED;
    }

    wld_endpoint_setConnectionStatus(pEP, connectionStatus, error);
}


void wld_endpoint_sendPairingNotification(T_EndPoint* pEP, uint32_t type, const char* reason, T_WPSCredentials* credentials) {
    wld_wps_sendPairingNotification(pEP->pBus, type, reason, NULL, credentials);
    amxd_object_t* wps = amxd_object_get(pEP->pBus, "WPS");
    pEP->WPS_PairingInProgress = amxd_object_get_bool(wps, "PairingInProgress", NULL);
}

/**
 * @brief __EndPoint_WPS_pushButton
 *
 * Start the WPS pairing process as an enrollee for the endpoint.
 *
 * @param fcall function call context
 * @param args argument list
 *     - clientPIN: optional argument to use clientPIN, give a 4-digit or 8-digit WPS PIN code.
 *     - ssid: restrict the wps session to a certain ssid
 *     - bssid: restrict the wps session to the AP with this MAC
 * @param retval variant that must contain the return value
 * @return function execution state
 */
amxd_status_t _EndPoint_pushButton(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    int ret = -1;
    amxd_object_t* epObject = amxd_object_get_parent(obj);
    T_EndPoint* pEP = epObject->priv;

    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "EP NULL");
    SAH_TRACEZ_INFO(ME, "pushButton called for EP %s", pEP->alias);

    T_Radio* pR = pEP->pRadio;
    if(!(pEP->enable && pEP->WPS_Enable && (wld_rad_hasOnlyActiveEP(pR) || (pR->status == RST_UP)))) {
        SAH_TRACEZ_ERROR(ME, "Radio not up, endpoint not enabled or WPS not enabled %u %u %u %u",
                         pEP->enable, pEP->WPS_Enable, wld_rad_hasOnlyActiveEP(pR), pR->status);
        return amxd_status_unknown_error;
    }

    uint64_t call_id = amxc_var_constcast(uint64_t, retval);
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        if(pEP->WPS_PBC_Delay.timer) {
            //Need to cancel the previous request and replace with the current one
            wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
            pEP->WPS_PBC_Delay.call_id = call_id;
            pEP->WPS_PBC_Delay.args = args;
        } else {
            //Need to create a timer
            amxp_timer_new(&pEP->WPS_PBC_Delay.timer, endpoint_wps_pbc_delayed_time_handler, pEP);
            amxp_timer_start(pEP->WPS_PBC_Delay.timer, 1000);
            pEP->WPS_PBC_Delay.intf.pEP = pEP;
            pEP->WPS_PBC_Delay.call_id = call_id;
            pEP->WPS_PBC_Delay.args = args;
        }
    } else {
        if(pEP->WPS_PBC_Delay.timer) {
            amxp_timer_delete(&pEP->WPS_PBC_Delay.timer);
            wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
            pEP->WPS_PBC_Delay.call_id = 0;
            pEP->WPS_PBC_Delay.args = NULL;
            pEP->WPS_PBC_Delay.timer = NULL;
            pEP->WPS_PBC_Delay.intf.pEP = NULL;
        }
    }

    if(pEP->WPS_PBC_Delay.call_id != 0) {
        return amxd_status_deferred;
    }

    ret = endpoint_wps_start(pEP, pEP->WPS_PBC_Delay.call_id, args);
    SAH_TRACEZ_OUT(ME);
    return ret ? amxd_status_unknown_error : amxd_status_ok;
}

amxd_status_t _getDebug(amxd_object_t* epObject,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_EndPoint* pEP = epObject->priv;

    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "EP NULL");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    char bssid[ETHER_ADDR_STR_LEN];

    if(pEP->currentProfile == NULL) {
        amxc_var_add_key(cstring_t, retval, "CurrentProfile", "");
    } else {
        amxc_var_add_key(cstring_t, retval, "CurrentProfile", pEP->currentProfile->alias);
        amxc_var_add_key(cstring_t, retval, "CurrentProfileSSID", pEP->currentProfile->SSID);
        wldu_convMac2Str(pEP->currentProfile->BSSID, ETHER_ADDR_LEN, bssid, ETHER_ADDR_STR_LEN);
        amxc_var_add_key(cstring_t, retval, "CurrentProfileBSSID", bssid);
    }

    return amxd_status_ok;
}

/**
 * @brief __EndPoint_WPS_cancelPairing
 *
 * Cancel the WPS pairing process.
 *
 * @param fcall function call context
 * @param args argument list
 * @param retval variant that must contain the return value
 * @return function execution state
 */
amxd_status_t _EndPoint_cancelPairing(amxd_object_t* epObject,
                                      amxd_function_t* func _UNUSED,
                                      amxc_var_t* args _UNUSED,
                                      amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);
    int ret = -1;
    T_EndPoint* pEP = epObject->priv;

    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "No EP struct found");
        goto exit;
    }

    SAH_TRACEZ_INFO(ME, "cancelPairing called for %s", pEP->alias);

    // If a timer is running... stop and destroy it also.
    if(pEP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pEP->WPS_PBC_Delay.timer);
        wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
        pEP->WPS_PBC_Delay.call_id = 0;
        pEP->WPS_PBC_Delay.args = NULL;
        pEP->WPS_PBC_Delay.timer = NULL;
        pEP->WPS_PBC_Delay.intf.pEP = NULL;
    }

    wld_wps_clearPairingTimer(&pEP->wpsSessionInfo);
    ret = pEP->pFA->mfn_wendpoint_wps_cancel(pEP);
exit:
    SAH_TRACEZ_OUT(ME);
    return ret ? amxd_status_unknown_error : amxd_status_ok;
}

void wld_endpoint_create_reconnect_timer(T_EndPoint* pEP) {
    amxp_timer_new(&pEP->reconnectTimer, endpoint_reconnect_handler, pEP);
    amxp_timer_set_interval(pEP->reconnectTimer, pEP->reconnectInterval * 1000);
}

/*****************************************************************************************************/
/* Endpoint Helper Functions                                                                         */
/*****************************************************************************************************/

/**
 * @brief wld_update_endpoint_stats
 *
 * Set Endpoint stats to the datamodel
 *
 * @param obj Endpoint Stats object
 * @param stats T_EndPointStats of the endpoint instance
 * @return
 */
bool wld_update_endpoint_stats(amxd_object_t* obj, T_EndPointStats* stats) {
    if(!obj || !stats) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : !obj||!stats");
        return false;
    }

    amxd_object_set_uint32_t(obj, "LastDataDownlinkRate", stats->LastDataDownlinkRate);
    amxd_object_set_uint32_t(obj, "LastDataUplinkRate", stats->LastDataUplinkRate);
    amxd_object_set_uint32_t(obj, "Retransmissions", stats->Retransmissions);
    amxd_object_set_int32_t(obj, "SignalStrength", stats->SignalStrength);
    amxd_object_set_int32_t(obj, "SignalNoiseRatio", stats->SignalNoiseRatio);
    amxd_object_set_int32_t(obj, "Noise", stats->noise);
    amxd_object_set_int32_t(obj, "RSSI", stats->RSSI);
    amxd_object_set_uint64_t(obj, "TxBytes", stats->txbyte);
    amxd_object_set_uint32_t(obj, "TxPacketCount", stats->txPackets);
    amxd_object_set_uint32_t(obj, "Tx_Retransmissions", stats->txRetries);
    amxd_object_set_uint64_t(obj, "RxBytes", stats->rxbyte);
    amxd_object_set_uint32_t(obj, "RxPacketCount", stats->rxPackets);
    amxd_object_set_uint32_t(obj, "Rx_Retransmissions", stats->rxRetries);

    amxd_object_set_cstring_t(obj, "OperatingStandard", swl_radStd_unknown_str[stats->operatingStandard]);
    amxd_object_set_uint32_t(obj, "MaxRxSpatialStreamsSupported", stats->maxRxStream);
    amxd_object_set_uint32_t(obj, "MaxTxSpatialStreamsSupported", stats->maxTxStream);
    wld_ad_syncCapabilities(obj, &stats->assocCaps);

    return true;
}

/**
 * @brief wld_endpoint_validate_profile
 *
 * Validate the Endpoint Profile
 *
 * @param Profile T_EndPointProfile struct
 * @return true when validated OK, false when validation failed
 */
bool wld_endpoint_validate_profile(const T_EndPointProfile* epProfile) {
    if(!epProfile) {
        SAH_TRACEZ_ERROR(ME, "Profile is NULL");
        return false;
    }

    /* check or the SSID is filled in */
    if((strlen(epProfile->SSID) == 0) || (epProfile->SSID[0] == '\0')) {
        SAH_TRACEZ_ERROR(ME, "SSID is missing");
        return false;
    }

    /* check or the security mode/keys are valid */

    if(epProfile->secModeEnabled == APMSI_UNKNOWN) {
        SAH_TRACEZ_ERROR(ME, "Invalid security mode");
        return false;
    }

    wld_securityMode_e keyClassification = APMSI_NONE;
    if((epProfile->secModeEnabled >= APMSI_WEP64) && (epProfile->secModeEnabled <= APMSI_WEP128IV)) {
        keyClassification = isValidWEPKey(epProfile->WEPKey);
        if(epProfile->secModeEnabled != keyClassification) {
            SAH_TRACEZ_ERROR(ME, "Invalid WEP key [%s] (detected mode [%u]) for mode [%u]", epProfile->WEPKey, keyClassification, epProfile->secModeEnabled);
            return false;
        }
    } else if((epProfile->secModeEnabled == APMSI_WPA_WPA2_P)
              || (epProfile->secModeEnabled == APMSI_WPA_P)
              || (epProfile->secModeEnabled == APMSI_WPA_E)
              || (epProfile->secModeEnabled == APMSI_WPA_WPA2_E)
              ) {
        if(!epProfile->keyPassPhrase[0]) {
            SAH_TRACEZ_ERROR(ME, "Invalid KeyPassPhrase: [%s]", epProfile->keyPassPhrase);
            return false;
        }

        if(isValidPSKKey(epProfile->keyPassPhrase)) {
            keyClassification = APMSI_WPA_P;
        }


        if(APMSI_WPA2_P != keyClassification) {
            if(isValidAESKey(epProfile->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                keyClassification = APMSI_WPA2_P;
            }

            if(APMSI_WPA2_P != keyClassification) {
                SAH_TRACEZ_ERROR(ME, "invalid KeyPassPhrase: [%s]", epProfile->keyPassPhrase);
                return false;
            }
        }
    } else if((epProfile->secModeEnabled == APMSI_WPA2_P) || (epProfile->secModeEnabled == APMSI_WPA2_E)) {
        if(isValidAESKey(epProfile->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            keyClassification = APMSI_WPA2_P;
        }
        if(!(APMSI_WPA2_P == keyClassification)) {
            SAH_TRACEZ_ERROR(ME, "invalid KeyPassPhrase :[%s] (detected mode [%u]) for mode [%u]", epProfile->keyPassPhrase, keyClassification, epProfile->secModeEnabled);
            return false;
        }
    } else if(epProfile->secModeEnabled == APMSI_AUTO) {
        keyClassification = APMSI_AUTO;
    }

    if((epProfile->secModeEnabled > APMSI_NONE) && (keyClassification == APMSI_NONE)) {
        SAH_TRACEZ_ERROR(ME, "No key available for security method [%d]", epProfile->secModeEnabled);
        return false;
    }

    return true;
}

/**
 * @brief wld_endpoint_set_status
 *
 * Sets the status fields of the endpoint in the datamodel.
 *
 * @param pEP The endpoint
 * @param status The current status of the endpoint
 * @param connectionStatus The current connection status of the endpoint
 * @param error The current error of the endpoint (0 if no error present)
 */
static void s_setEndpointStatus(T_EndPoint* pEP,
                                wld_intfStatus_e status,
                                wld_epConnectionStatus_e connectionStatus,
                                wld_epError_e error) {
    SAH_TRACEZ_INFO(ME, "Set status %s %i %i %i", pEP->Name, status, connectionStatus, error);

    bool changed = false;
    amxd_object_t* object = pEP->pBus;
    wld_epConnectionStatus_e oldConnectionStatus = pEP->connectionStatus;
    wld_intfStatus_e oldStatus = pEP->status;

    if(status != pEP->status) {
        pEP->status = status;
        amxd_object_set_cstring_t(object, "Status", cstr_EndPoint_status[pEP->status]);
        changed = true;
    }

    if(connectionStatus != pEP->connectionStatus) {
        pEP->connectionStatus = connectionStatus;
        amxd_object_set_cstring_t(object, "ConnectionStatus",
                                  cstr_EndPoint_connectionStatus[pEP->connectionStatus]);
        changed = true;
    }

    if(error != pEP->error) {
        pEP->error = error;
        amxd_object_set_cstring_t(object, "LastError",
                                  cstr_EndPoint_lastError[pEP->error]);
        changed = true;
    }

    int ssidStatus = RST_DOWN;
    if(pEP->connectionStatus == EPCS_CONNECTED) {
        ssidStatus = RST_UP;
    } else if((pEP->connectionStatus == EPCS_IDLE) || (status == APSTI_ENABLED)) {
        ssidStatus = RST_DORMANT;
    } else if(pEP->connectionStatus == EPCS_ERROR) {
        ssidStatus = RST_ERROR;
    }
    pEP->pSSID->status = ssidStatus;
    char oldSsid[SSID_NAME_LEN];
    swl_macBin_t oldBssid;
    swl_str_copy(oldSsid, sizeof(oldSsid), pEP->pSSID->SSID);
    memcpy(oldBssid.bMac, pEP->pSSID->BSSID, SWL_MAC_BIN_LEN);

    syncData_SSID2OBJ(pEP->pSSID->pBus, pEP->pSSID, SET);

    ASSERTI_TRUE(changed, , ME, "%s: unchanged", pEP->Name);

    SAH_TRACEZ_INFO(ME, "%s: changed status %u -> %u ; conn %u -> %u",
                    pEP->Name,
                    oldStatus, pEP->status,
                    oldConnectionStatus, pEP->connectionStatus);

    T_SSID* ssid = pEP->pSSID;

    if(connectionStatus != oldConnectionStatus) {
        pEP->lastConnStatusChange = swl_time_getMonoSec();
        if(oldConnectionStatus == EPCS_CONNECTED) {
            SAH_TRACEZ_WARNING(ME, "%s: EP disconnect from %s <"SWL_MAC_FMT "> (nr %u, try %u)",
                               pEP->Name, oldSsid, SWL_MAC_ARG(oldBssid.bMac),
                               pEP->assocStats.nrAssociations, pEP->assocStats.nrAssocAttemptsSinceDc);
            pEP->assocStats.lastDisassocTime = pEP->lastConnStatusChange;
            pEP->assocStats.nrAssocAttemptsSinceDc = 0;
        }
        if(connectionStatus == EPCS_CONNECTED) {
            pEP->assocStats.lastAssocTime = pEP->lastConnStatusChange;
            pEP->assocStats.nrAssociations++;
            swl_macBin_t bssid;
            wld_endpoint_getBssidBin(pEP, &bssid);
            SAH_TRACEZ_WARNING(ME, "%s: EP connect to %s <"MAC_PRINT_FMT ">", pEP->Name, ssid->SSID, MAC_PRINT_ARG(bssid.bMac));
        }
        s_writeAssocStats(pEP);
    }

    wld_ep_status_change_event_t event;
    event.ep = pEP;
    event.oldConnectionStatus = oldConnectionStatus;
    event.oldStatus = oldStatus;
    wld_event_trigger_callback(gWld_queue_ep_onStatusChange, &event);
}

/**
 * @brief wld_endpoint_profile_set_status
 *
 * Sets the status field of the endpoint profile in the datamodel.
 *
 * @param profile The endpoint profile
 * @param connected Is the profile connected or not
 */
static void s_setProfileStatus(T_EndPointProfile* profile, bool connected) {
    amxd_object_t* object = profile->pBus;
    wld_epProfileStatus_e status;

    SAH_TRACEZ_INFO(ME, "enable %d; profile->status %d; connected %d",
                    profile->enable, profile->status, connected);

    if(profile->enable) {
        status = connected ? EPPS_ACTIVE : EPPS_AVAILABLE;
    } else {
        status = EPPS_DISABLED;
    }

    if(status != profile->status) {
        profile->status = status;
        amxd_object_set_cstring_t(object, "Status",
                                  cstr_EndPointProfile_status[profile->status]);
        bool prevValue = profile->endpoint->internalChange;
        profile->endpoint->internalChange = true;
        profile->endpoint->internalChange = prevValue;
    }
}

/**
 * Get BSSID of the accesspoint that the given endpoint is connected to.
 *
 * In case of error or if not connected, the null-mac is written to `tgtMac`.
 */
swl_rc_ne wld_endpoint_getBssidBin(T_EndPoint* pEP, swl_macBin_t* tgtMac) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(tgtMac, SWL_RC_INVALID_PARAM, ME, "NULL");

    // Make sure in case of any errors, it's set to the null-mac.
    *tgtMac = g_swl_macBin_null;

    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    swl_rc_ne ok = pEP->pFA->mfn_wendpoint_bssid(pEP, &macChar);
    ASSERT_TRUE(swl_rc_isOk(ok), SWL_RC_ERROR, ME, "%s not connected", pEP->Name);
    bool ret = swl_mac_charToBin(tgtMac, &macChar);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "fail to convert mac address to binary");
    return SWL_RC_OK;
}

void endpointPerformConnectCommit(T_EndPoint* pEP, bool alwaysCommit) {

    T_Radio* pR = pEP->pRadio;

    /* stop any ongoing scans (if any) */
    if(pR->pFA->mfn_wrad_stop_scan(pR) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to stop ongoing scans");
    }

    if(pEP->pFA->mfn_wendpoint_connect_ap(pEP->currentProfile) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to connect to AP");
        s_setEndpointStatus(pEP, APSTI_ENABLED, EPCS_ERROR, EPE_ERROR_MISCONFIGURED);
    }

    if(alwaysCommit) {
        wld_rad_doRadioCommit(pR);
    } else {
        wld_rad_doCommitIfUnblocked(pR);
    }

    SAH_TRACEZ_OUT(ME);

}

//Deprecated
void endpointPerformConnect(T_EndPoint* pEP) {
    SAH_TRACEZ_WARNING(ME, "DEPRECATED");
    endpointPerformConnectCommit(pEP, false);
}

/**
 * @brief endpointReconfigure
 *
 * Reconfigures the endpoint based on the enable flags and the selected
 * profile.
 *
 * @param pEP The endpoint to reconfigure
 */
void endpointReconfigure(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, , ME, "null Endpoint");
    ASSERT_NOT_NULL(pEP->pRadio, , ME, "null radio");

    T_Radio* pR = pEP->pRadio;

    SAH_TRACEZ_IN(ME);

    // Ignore this when we're doing a WPS Pairing!
    if(pEP->connectionStatus == EPCS_WPS_PAIRING) {
        SAH_TRACEZ_INFO(ME, "%s: WPS EP pairing session ongoing", pEP->alias);
        return;
    }

    /*
     * Disconnect any previous connecting/connected AP. We do this in any
     * case. Either the profile is disabled and we need to disconnect. Or
     * we want to connect and we want disconnect any previously connected
     * profile. If there was no connection this should not hurt.
     */
    SAH_TRACEZ_INFO(ME, "%s: Disconnecting previous Profile (if any)", pEP->alias);
    pEP->pFA->mfn_wendpoint_disconnect(pEP);

    /* Ignore also when we're not in STA mode.
       We don't want to sync the connection, as this force SCAN for AP */
    if(!pR->isSTA) {
        SAH_TRACEZ_INFO(ME, "%s: STA mode not active", pEP->alias);
        return;
    }

    bool enabled = true;
    int error = EPE_NONE;
    if(!pEP->currentProfile) {
        SAH_TRACEZ_INFO(ME, "%s: endpoint does not have a profile assigned.", pEP->alias);
        enabled = false;
        error = EPE_ERROR_MISCONFIGURED;
    } else if(!wld_endpoint_isReady(pEP)) {
        SAH_TRACEZ_INFO(ME, "%s: endpoint not ready", pEP->alias);
        enabled = false;
    }

    if(!enabled) {
        /* Endpoint not enabled, nothing left to do. */
        SAH_TRACEZ_INFO(ME, "%s: endpoint disabled.", pEP->alias);
        wld_endpoint_sync_connection(pEP, false, error);
        return;
    }

    endpointPerformConnectCommit(pEP, false);
}


/**
 * @brief endpoint_wps_pbc_delayed_time_handler
 *
 * The handler that get's called when the WPS PBC get's delayed because of an
 * ongoing commit.
 *
 * @param timer: The timer responsible for the callback.
 * @param userdata: Void pointer to the T_EndPoint struct.
 */
static void endpoint_wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata) {
    SAH_TRACEZ_IN(ME);

    swl_usp_cmdStatus_ne cmdStatus = SWL_USP_CMD_STATUS_SUCCESS;
    T_EndPoint* pEP = userdata;
    T_Radio* pR = pEP->pRadio;
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        // We're still not ready... start timer again maybe we must count this?
        amxp_timer_start(timer, 10000);
        goto exit;
    }

    // In case we're enrollee (STA mode and no AP active) we accept Radio RST_DORMANT status.
    // As the Radio is UP but passively on a DFS channel.
    if(pEP->enable && pEP->WPS_Enable && (wld_rad_hasOnlyActiveEP(pR) || (pR->status == RST_UP))) {
        int ret = endpoint_wps_start(pEP, pEP->WPS_PBC_Delay.call_id, pEP->WPS_PBC_Delay.args);
        if(ret) {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "Radio condition not ok, endpoint not enabled or WPS not enabled");
        cmdStatus = SWL_USP_CMD_STATUS_ERROR_OTHER;
    }

    amxp_timer_delete(&timer);
    wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, cmdStatus);
    pEP->WPS_PBC_Delay.call_id = 0;
    pEP->WPS_PBC_Delay.args = NULL;
    pEP->WPS_PBC_Delay.timer = NULL;
    pEP->WPS_PBC_Delay.intf.pEP = NULL;

exit:
    SAH_TRACEZ_OUT(ME);
}

static int endpoint_wps_start(T_EndPoint* pEP, uint64_t call_id _UNUSED, amxc_var_t* args) {
    const char* ssid = GET_CHAR(args, "ssid");
    const char* bssid = GET_CHAR(args, "bssid");
    const char* pin = GET_CHAR(args, "clientPIN");

    int method;
    if(pin) {
        method = WPS_CFG_MTHD_PIN;
    } else {
        method = WPS_CFG_MTHD_PBC;
    }

    return pEP->pFA->mfn_wendpoint_wps_start(pEP, method, (char*) pin, (char*) ssid, (char*) bssid);
}

static void endpoint_reconnect_handler(amxp_timer_t* timer _UNUSED, void* userdata) {
    T_EndPoint* pEP = userdata;
    T_Radio* pRad = pEP->pRadio;
    swl_chanspec_t chanspec;

    SAH_TRACEZ_WARNING(ME, "%s: Retrying to connect", pEP->alias);

    if(pEP->connectionStatus == EPCS_WPS_PAIRING) {
        SAH_TRACEZ_INFO(ME, "Skip reconnect trigger due WPS pairing");
        return;
    }

    chanspec = wld_rad_getSwlChanspec(pRad);
    uint32_t curStateTime = (uint32_t) (wld_util_time_monotonic_sec() - pRad->lastStatusChange);
    uint32_t clearTimeSec = wld_channel_get_band_clear_time(chanspec) / 1000;

    SAH_TRACEZ_INFO(ME, "%s: status %u timeInfo %u/%u chanInfo %u/%u", pRad->Name, pRad->status,
                    curStateTime, clearTimeSec, pRad->channel, pRad->runningChannelBandwidth);

    if(pRad->status == RST_DORMANT) {
        if(clearTimeSec == 0) {
            SAH_TRACEZ_ERROR(ME, "%s: dormant, no dfs chan %u/%u", pRad->Name, pRad->channel, pRad->runningChannelBandwidth);
            clearTimeSec = 60;
        }

        if(curStateTime < (clearTimeSec + 10)) {
            SAH_TRACEZ_INFO(ME, "%s: skip reconnect DFS CAC %u/%u : %u/%u ",
                            pRad->Name, pRad->channel, pRad->runningChannelBandwidth,
                            curStateTime, clearTimeSec);
            return;
        }
    }

    // Do reconnect
    endpointPerformConnectCommit(pEP, true);


    pEP->assocStats.nrAssocAttempts++;
    pEP->assocStats.nrAssocAttemptsSinceDc++;
    s_writeAssocStats(pEP);

    SAH_TRACEZ_INFO(ME, "%s: rad reconnect try %u / %u : prevTries %u / %u",
                    pRad->Name, pEP->reconnect_count, pEP->reconnect_rad_trigger,
                    pEP->assocStats.nrAssocAttempts, pEP->assocStats.nrAssocAttemptsSinceDc);

    if(pEP->reconnect_rad_trigger == 0) {
        SAH_TRACEZ_INFO(ME, "no rad trigger");
        return;
    }

    pEP->reconnect_count++;
    if(pEP->reconnect_count >= pEP->reconnect_rad_trigger) {
        SAH_TRACEZ_WARNING(ME, "%s: perform radio toggle, try %u", pRad->Name, pEP->assocStats.nrAssocAttemptsSinceDc);
        pRad->pFA->mfn_wrad_enable(pRad, 1, SET);
        pRad->pFA->mfn_wrad_channel(pRad, 0, SET);
        pEP->reconnect_count = 0;
    }
}

swl_rc_ne wld_endpoint_checkConnection(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(pEP->connectionStatus == EPCS_CONNECTED, SWL_RC_INVALID_STATE, ME, "%s not connected", pEP->Name);

    swl_macBin_t binMac;
    char* failMsg = NULL;

    swl_rc_ne ret = wld_endpoint_getBssidBin(pEP, &binMac);
    if(ret < SWL_RC_OK) {
        failMsg = "Failed to get BSSID";
    } else {
        T_SSID* pSSID = pEP->pSSID;
        if(!SWL_MAC_BIN_MATCHES(&binMac, pSSID->BSSID)) {
            failMsg = "HW BSSID does match stored BSSID";
        }
    }

    if(failMsg != NULL) {
        T_Radio* pRad = pEP->pRadio;
        wld_rad_incrementCounterStr(pRad, &pRad->genericCounters, WLD_RAD_EV_EP_FAIL,
                                    "%s: %s", pEP->Name, failMsg);
        wld_endpoint_sync_connection(pEP, false, false);
        return SWL_RC_ERROR;
    } else {
        SAH_TRACEZ_INFO(ME, "%s: check ok "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(binMac.bMac));
        return SWL_RC_OK;
    }
}

T_EndPoint* wld_endpoint_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");

    return amxc_llist_it_get_data(it, T_EndPoint, it);
}

/**
 * Provide the target mac address. If false, no target is currently configured.
 */
bool wld_endpoint_getTargetBssid(T_EndPoint* pEP, swl_macBin_t* macBuffer) {
    ASSERT_NOT_NULL(pEP, false, ME, "NULL");
    T_EndPointProfile* pEpProf = pEP->currentProfile;
    ASSERTI_NOT_NULL(pEpProf, false, ME, "NULL");

    if(!SWL_MAC_BIN_MATCHES(pEpProf->BSSID, &g_swl_macBin_null)) {
        memcpy(macBuffer->bMac, pEpProf->BSSID, ETHER_ADDR_LEN);
        return true;
    } else if(wld_tinyRoam_isRoaming(pEP)) {
        *macBuffer = *wld_tinyRoam_targetBssid(pEP);
        return true;
    }
    return false;
}

amxd_status_t _EndPoint_debug(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval) {

    T_EndPoint* pEP = object->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    const char* feature = GET_CHAR(args, "op");
    char buffer[100];

    if(feature == NULL) {
        feature = "help";
    }

    if(strcmp(feature, "profile") == 0) {
        if(pEP->currentProfile == NULL) {
            amxc_var_add_key(cstring_t, retval, "Profile", "<NULL>");
        } else {
            amxc_var_add_key(cstring_t, retval, "Profile", pEP->currentProfile->alias);
            amxc_var_add_key(cstring_t, retval, "SSID", pEP->currentProfile->SSID);
            char macBuffer[ETHER_ADDR_STR_LEN];
            wldu_convMac2Str(pEP->currentProfile->BSSID, ETHER_ADDR_LEN, macBuffer, sizeof(macBuffer));
            amxc_var_add_key(cstring_t, retval, "BSSID", macBuffer);
            amxc_var_add_key(cstring_t, retval, "Security", cstr_AP_ModesSupported[pEP->currentProfile->secModeEnabled]);
            amxc_var_add_key(cstring_t, retval, "KeyPassPhrase", pEP->currentProfile->keyPassPhrase);
            amxc_var_add_key(cstring_t, retval, "PreSharedKey", pEP->currentProfile->preSharedKey);
            amxc_var_add_key(cstring_t, retval, "WEPKey", pEP->currentProfile->WEPKey);
        }
    } else if(strcmp(feature, "assocStats") == 0) {
        swl_time_mapAddMono(retval, "LastConnChange", pEP->lastConnStatusChange);
        swl_time_mapAddMono(retval, "LastAssoc", pEP->assocStats.lastAssocTime);
        amxc_var_add_key(uint32_t, retval, "NrAssoc", pEP->assocStats.nrAssociations);
        amxc_var_add_key(uint32_t, retval, "NrAssocTry", pEP->assocStats.nrAssocAttempts);
        amxc_var_add_key(uint32_t, retval, "NrAssocTrySinceDc", pEP->assocStats.nrAssocAttemptsSinceDc);
    } else if(strcmp(feature, "help") == 0) {
        amxc_var_t cmdList;
        amxc_var_init(&cmdList);
        amxc_var_set_type(&cmdList, AMXC_VAR_ID_LIST);
        amxc_var_add_key(cstring_t, &cmdList, "assocStats", "");
        amxc_var_add_key(cstring_t, &cmdList, "hasEnoughRoamAp", "");
        amxc_var_add_key(cstring_t, &cmdList, "profile", "");
        amxc_var_add_key(cstring_t, &cmdList, "checkConnection", "");
        amxc_var_add_key(cstring_t, &cmdList, "WpaSuppCfg", "");
        amxc_var_add_key(cstring_t, &cmdList, "help", "");
        amxc_var_add_key(amxc_llist_t, retval, "cmds", amxc_var_get_const_amxc_llist_t(&cmdList));
    } else if(strcmp(feature, "checkConnection") == 0) {
        swl_rc_ne retCode = wld_endpoint_checkConnection(pEP);
        amxc_var_add_key(cstring_t, retval, "result", swl_rc_toString(retCode));
    } else if(strcmp(feature, "WpaSuppCfg") == 0) {
        swl_rc_ne retCode;
        const char* instruction = GET_CHAR(args, "instruction");
        char tmpName[128];
        snprintf(tmpName, sizeof(tmpName), "%s-%s.tmp.txt", "/tmp/wpa_supplicant", pEP->Name);
        if(swl_str_matches(instruction, "createConfig")) {
            retCode = wld_wpaSupp_cfgFile_create(pEP, tmpName);
        } else if(swl_str_matches(instruction, "globalUpdate")) {
            const char* key = GET_CHAR(args, "key");
            const char* value = GET_CHAR(args, "value");
            retCode = wld_wpaSupp_cfgFile_globalConfigUpdate(tmpName, key, value);
        } else if(swl_str_matches(instruction, "networkUpdate")) {
            const char* key = GET_CHAR(args, "key");
            const char* value = GET_CHAR(args, "value");
            retCode = wld_wpaSupp_cfgFile_networkConfigUpdate(tmpName, key, value);
        } else {
            // dump the config file
            wld_wpaSupp_config_t* config;
            wld_wpaSupp_loadConfig(&config, tmpName);
            swl_mapIt_t it;
            char key[64];
            char value[64];
            swl_map_for_each(it, wld_wpaSupp_getGlobalConfig(config)) {
                swl_map_itKeyChar(key, sizeof(key), &it);
                swl_map_itValueChar(value, sizeof(value), &it);
                amxc_var_add_key(cstring_t, retval, key, value);
            }
            swl_map_for_each(it, wld_wpaSupp_getNetworkConfig(config)) {
                swl_map_itKeyChar(key, sizeof(key), &it);
                swl_map_itValueChar(value, sizeof(value), &it);
                amxc_var_add_key(cstring_t, retval, key, value);
            }
            wld_wpaSupp_deleteConfig(config);
            retCode = SWL_RC_OK;
        }
        amxc_var_add_key(cstring_t, retval, "result", swl_rc_toString(retCode));
    } else {
        snprintf(buffer, sizeof(buffer), "unknown command %s", feature);
        amxc_var_add_key(cstring_t, retval, "error", buffer);
    }

    return amxd_status_ok;
}
