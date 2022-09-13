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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_wps.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_staCap.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_eventing.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld_ap_nl80211.h"
#include "wld_hostapd_ap_api.h"
#include "wld_dm_trans.h"

#define ME "ap"

/* FIX ME. Strings must be const STATIC? */
const char* cstr_AP_status[] = {"Disabled", "Enabled", "Error_Misconfigured", "Error",
    0};

const char* cstr_DEVICE_TYPES[] = {"Video", "Data", "Guest", 0};



const char* cstr_AP_MFMode[] = {"Off", "WhiteList", "BlackList",
    0};

const char* cstr_MultiAPType[] = {"FronthaulBSS", "BackhaulBSS", "BackhaulSTA", 0};

const char* wld_apRole_str[] = {"Off", "Main", "Relay", "Remote", 0};

const char* wld_vendorIe_frameType_str[] = {"Beacon", "ProbeResp", "AssocResp", "AuthResp", "ProbeReq", "AssocReq", "AuthReq"};

const char* g_str_wld_ap_dm[] = {"Default", "Disabled", "RNR", "UPR", "FILSDiscovery"};

SWL_TUPLE_TYPE_NEW(assocTable, ARR(swl_type_macBin, swl_type_macBin, swl_type_charPtr, swl_type_timeReal, swl_type_uint16))

static amxd_status_t _linkApSsid(amxd_object_t* object, amxd_object_t* pSsidObj) {
    ASSERT_NOT_NULL(object, amxd_status_unknown_error, ME, "NULL");
    T_SSID* pSSID = NULL;
    if(pSsidObj) {
        pSSID = (T_SSID*) pSsidObj->priv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    if(pAP) {
        ASSERTI_NOT_EQUALS(pSSID, pAP->pSSID, amxd_status_ok, ME, "same ssid reference");
        amxd_object_set_cstring_t(object, "RadioReference", "");
        pAP->pSSID->AP_HOOK = NULL;
        wld_ap_destroy(pAP);
        object->priv = NULL;
        pAP = NULL;
    }
    const char* vapName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    ASSERT_NOT_NULL(vapName, amxd_status_unknown_error, ME, "NULL");
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio Ctx");
    SAH_TRACEZ_INFO(ME, "pSSID(%p) pRad(%p)", pSSID, pRad);
    pAP = wld_ap_create(pRad, vapName, amxc_llist_size(&pRad->llAP));
    int ret = wld_ap_initObj(pAP, object);
    if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "wld_ap_initObj error %i", ret);
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_INFO(ME, "%s: add vap %s : %i", pRad->Name, vapName, ret);
    char* radObjPath = amxd_object_get_path(pRad->pBus, AMXD_OBJECT_NAMED);
    if(radObjPath) {
        amxd_object_set_cstring_t(pAP->pBus, "RadioReference", radObjPath);
        free(radObjPath);
    }
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;

    ret = wld_ap_initializeVendor(pRad, pAP, pSSID);
    if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: wld_ap_initializeVendor error add vap %s : %i", pRad->Name, vapName, ret);
        return amxd_status_unknown_error;
    }

    wld_ap_init(pAP);

    //Finalize AP/SSID mapping
    wld_ssid_initObjAp(pSSID, pSsidObj);
    /* Get defined paramater values from the default instance */
    syncData_SSID2OBJ(pSsidObj, pSSID, GET);
    SyncData_AP2OBJ(pAP->pBus, pAP, GET);
    syncData_VendorWPS2OBJ(NULL, pRad, GET); // init WPS
    /* DM will be synced with internal Ctxs later (on event or after dm load completed) */
    return amxd_status_ok;
}

amxd_status_t _wld_ap_addInstance_ocf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s)", object, amxd_object_get_name(object, AMXD_OBJECT_NAMED));
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    amxd_object_t* pSsidObj = amxd_object_findf(get_wld_object(), "SSID.%s", amxd_object_get_name(instance, AMXD_OBJECT_NAMED));
    status = _linkApSsid(instance, pSsidObj);
    return status;
}

amxd_status_t _wld_ap_setSSIDRef_pwf(amxd_object_t* object,
                                     amxd_param_t* parameter,
                                     amxd_action_t reason,
                                     const amxc_var_t* const args,
                                     amxc_var_t* const retval,
                                     void* priv) {
    amxd_status_t rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_IN(ME);
    char* ssidRef = amxc_var_dyncast(cstring_t, args);
    char* path = amxd_object_get_path(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "apObj(%p:%s:%s) ssidRef(%s)",
                    object, amxd_object_get_name(object, AMXD_OBJECT_NAMED), path,
                    ssidRef);
    free(path);

    amxd_object_t* pSsidObj = NULL;
    if(ssidRef && ssidRef[0]) {
        pSsidObj = amxd_object_findf(amxd_dm_get_root(wld_plugin_dm), "%s", ssidRef);
    }
    rv = _linkApSsid(object, pSsidObj);
    free(ssidRef);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

int wld_ap_initializeVendor(T_Radio* pR, T_AccessPoint* pAP, T_SSID* pSSID) {
    int ret = pR->pFA->mfn_wrad_addVapExt(pR, pAP);

    if(ret == WLD_ERROR_NOT_IMPLEMENTED) {
        char vapnamebuf[32] = {0};
        swl_str_copy(vapnamebuf, sizeof(vapnamebuf), pAP->alias);
        /* Try to create the requested interface by calling the HW function */
        ret = pR->pFA->mfn_wrad_addvapif(pR, vapnamebuf, sizeof(vapnamebuf));
        if(ret >= 0) {
            pAP->index = ret;
            snprintf(pAP->alias, sizeof(pAP->alias), "%s", vapnamebuf);
        }
    }

    if(ret < 0) {
        return ret;
    }

    // Plugin has create the BSSID, be sure we update/select that one.
    pAP->pFA->mfn_wvap_bssid(NULL, pAP, NULL, 0, GET);
    memcpy(pSSID->MACAddress, pSSID->BSSID, sizeof(pSSID->MACAddress));

    return WLD_OK;
}

void wld_ap_doSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_sync(pAP, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

void wld_ap_doWpsSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_wps_enable(pAP, pAP->WPS_Enable, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

amxd_status_t _wld_ap_setMaxStations_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {

    amxc_var_t Nvalue;
    amxc_var_init(&Nvalue);

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    int flag = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "set MaxStations %d", flag);

    // Set MaxStations supported and update Radio object param !
    if(pAP && debugIsVapPointer(pAP)) {
        pAP->MaxStations = (flag > MAXNROF_STAENTRY || flag < 0) ? MAXNROF_STAENTRY : flag;
        wld_ap_doSync(pAP);   // Force a resync of the structure (FIX ME)
        amxc_var_set_uint32_t(&Nvalue, pAP->MaxStations);
        amxd_param_set_value(parameter, &Nvalue);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * The function is used to map an interface on a bridge. Some vendor deamons
 * depends on it for proper functionallity.
 */
amxd_status_t _wld_ap_setBridgeInterface_pwf(amxd_object_t* object,
                                             amxd_param_t* parameter _UNUSED,
                                             amxd_action_t reason _UNUSED,
                                             const amxc_var_t* const args _UNUSED,
                                             amxc_var_t* const retval _UNUSED,
                                             void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* pstr_BridgeName = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set BridgeInterface %s", pstr_BridgeName);

    if(pAP && debugIsVapPointer(pAP) && pstr_BridgeName) {
        // We've a dependency on a bridge interface that we must check!
        snprintf(pAP->bridgeName, sizeof(pAP->bridgeName), "%s", pstr_BridgeName);
        // For the moment... the plugin must track if bridgeName has been used (== not empty)?
        pAP->pFA->mfn_on_bridge_state_change(pAP->pRadio, pAP, SET);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * The function sets the default device type of new devices connected to the requested AP.
 * The value of the defaultDeviceType will only affect future devices connecting to the AP.
 * It will NOT be retroactively set to all connected devices.
 */
amxd_status_t _wld_ap_setDefaultDeviceType_pwf(amxd_object_t* object,
                                               amxd_param_t* parameter _UNUSED,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args _UNUSED,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* default_devicetype_name = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set DefaultDeviceType %s", default_devicetype_name);

    if(pAP && debugIsVapPointer(pAP) && default_devicetype_name) {
        int newDeviceType = conv_ModeIndexStr(cstr_DEVICE_TYPES, default_devicetype_name);
        if(newDeviceType >= 0) {
            SAH_TRACEZ_INFO(ME, "Updating device type from %u to %u", pAP->defaultDeviceType, newDeviceType);
            pAP->defaultDeviceType = newDeviceType;
        } else {
            SAH_TRACEZ_ERROR(ME, "Device type -%s- is not valid", default_devicetype_name);
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Enables debugging on third party software modules.
 * The goal of this is only for developpers!
 * We need a way to have/enable a better view how things are managed by some deamons.
 */
amxd_status_t _wld_ap_setDbgEnable_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter _UNUSED,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    int dbgAPEnable = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "setdbgAPEnable %d", dbgAPEnable);

    if(pAP && debugIsVapPointer(pAP)) {
        pAP->dbgEnable = dbgAPEnable;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setDbgFile_pwf(amxd_object_t* object _UNUSED,
                                     amxd_param_t* parameter _UNUSED,
                                     amxd_action_t reason _UNUSED,
                                     const amxc_var_t* const args _UNUSED,
                                     amxc_var_t* const retval _UNUSED,
                                     void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* dbgFile = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "setdbgAPFile %s", dbgFile);
    if(pAP && debugIsVapPointer(pAP) && dbgFile) {
        if(pAP->dbgOutput) {
            free(pAP->dbgOutput);
        }
        pAP->dbgOutput = strdup(dbgFile);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_set80211kEnabled_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "setAp80211kEnable %d", new_value);

    if(pAP && debugIsVapPointer(pAP)) {
        pAP->IEEE80211kEnable = new_value;
        wld_ap_doSync(pAP);
    }

    return amxd_status_ok;
}

amxd_status_t _validateIEEE80211rEnabled(amxd_param_t* parameter _UNUSED, void* validationData _UNUSED) {
    return amxd_status_ok;
}

amxd_status_t _wld_ap_set80211rEnabled_pwf(amxd_object_t* object,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set 80211rEnable %d", new_value);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");

    pAP->IEEE80211rEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setFTOverDSEnable_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set FTOverDSEnable %d", new_value);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");

    pAP->IEEE80211rFTOverDSEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setMobilityDomain_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    uint16_t mobility_domain = amxc_var_dyncast(uint16_t, args);
    SAH_TRACEZ_INFO(ME, "setMobilityDomain %d", mobility_domain);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");

    pAP->mobilityDomain = mobility_domain;
    wld_ap_sec_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setInterworkingEnable_pwf(amxd_object_t* object _UNUSED,
                                                amxd_param_t* parameter _UNUSED,
                                                amxd_action_t reason _UNUSED,
                                                const amxc_var_t* const args _UNUSED,
                                                amxc_var_t* const retval _UNUSED,
                                                void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set InterworkingEnable %d", new_value);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");

    pAP->cfg11u.interworkingEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setQosMapSet_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter _UNUSED,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");

    const char* new_qosMapSet = amxc_var_constcast(cstring_t, args);
    if(new_qosMapSet && new_qosMapSet[0] && !swl_str_matches(pAP->cfg11u.qosMapSet, new_qosMapSet)) {
        snprintf(pAP->cfg11u.qosMapSet, QOS_MAP_SET_MAX_LEN, "%s", new_qosMapSet);
        wld_ap_doSync(pAP);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/* Check with ODL file if order and string format of VAP_AccessPoint_ObjParam is matching! */
const char* VAP_AccessPoint_ObjParam[] = {
    /* 0 */ "SSIDAdvertisementEnabled",
    /* 1 */ "RetryLimit",
    /* 2 */ "WMMEnable",
    /* 3 */ "UAPSDEnable",
    /* 4 */ "MCEnable",
    /* 5 */ "APBridgeDisable",
    /* 6 */ "IsolationEnable",
    NULL
};

amxd_status_t _wld_ap_setCommonParam_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {

    int idx = 0;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* pname = parameter->name;
    int new_value = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "set Param %s %d", pname, new_value);

    /* It's a bit stupid... but this should be the 'proper' way. */
    if(pAP && pname && findStrInArray(pname, VAP_AccessPoint_ObjParam, &idx)) {
        T_SSID* pSSID = pAP->pSSID;
        switch(idx) {
        case 0: /* SSIDAdvertisementEnabled - Needs also a resync of SSID */
            if(debugIsSsidPointer(pSSID)) {
                pAP->pFA->mfn_wvap_ssid(pAP, (char*) pSSID->SSID, strlen(pSSID->SSID), SET);
                wld_autoCommitMgr_notifyVapEdit(pAP);
            }
            wld_ap_doSync(pAP);
            break;
        case 1: /* RetryLimit */
        case 2: /* WMMEnable */
        case 3: /* UAPSDEnable */
        case 4: /* MCEnable */
        case 5: /* APBridgeDisable */
        case 6: /* IsolationEnable */
            wld_ap_doSync(pAP);
            break;
        default:
            SAH_TRACEZ_ERROR(ME, "Unhandled param %s", pname);
            break;
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


amxd_status_t _wld_ap_setWPSEnable_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter _UNUSED,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    if(!pAP) {
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    pAP->WPS_Enable = amxc_var_dyncast(bool, args);
    wld_ap_doWpsSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setWpsConfigMethodsEnabled_pwf(amxd_object_t* object _UNUSED,
                                                     amxd_param_t* parameter _UNUSED,
                                                     amxd_action_t reason _UNUSED,
                                                     const amxc_var_t* const args _UNUSED,
                                                     amxc_var_t* const retval _UNUSED,
                                                     void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    if(pAP && debugIsVapPointer(pAP)) {
        const char* StrParm = amxc_var_constcast(cstring_t, args);
        SAH_TRACEZ_INFO(ME, "set WPS ConfigMethodsEnabled %s", StrParm);
        /* convert it to our bit value... */
        wld_wps_cfgMethod_m nv;
        wld_wps_ConfigMethods_string_to_mask(&nv, StrParm, ',');
        if(nv != pAP->WPS_ConfigMethodsEnabled) {
            /* Ignore comparation with virtual bit settings of WPS 2.0 */
            bool needSync = ((!nv) || ((nv & M_WPS_CFG_MTHD_WPS10_ALL) != (pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_WPS10_ALL)));
            pAP->WPS_ConfigMethodsEnabled = nv;
            if(needSync) {
                wld_ap_doWpsSync(pAP);
            }
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setWpsCertModeEnable_pwf(amxd_object_t* object _UNUSED,
                                               amxd_param_t* parameter _UNUSED,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args _UNUSED,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    bool flag = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set WPS CertModeEnable %d", flag);
    if(pAP && debugIsVapPointer(pAP)) {
        pAP->WPS_CertMode = flag;
        wld_ap_doWpsSync(pAP); // Force a resync of the structure (FIX ME)
    }

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setWpsConfigured_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    ASSERT_NOT_NULL(parameter, amxd_status_ok, ME, "NULL");
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "Not debug vap pointer");

    bool new_value = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "set WPS Configured %p", parameter);
    if(new_value != pAP->WPS_Configured) {
        pAP->WPS_Configured = new_value;
        wld_ap_doWpsSync(pAP); // Force a resync of the structure (FIX ME)
    }

    return amxd_status_ok;
}

/**
 * All VAP (AccessPoint & SSID) parameters that must be
 * updated after a commit are handled in this function.
 */
int vap_libsync_status_cb(T_AccessPoint* pAP) {
    T_SSID* pSSID = pAP->pSSID;

    /* Keep WPS status sync when SSIDAdvertisement is enabled! Otherwise NeMo must handle it!  */
    if(pAP->SSIDAdvertisementEnabled) {
        pAP->WPS_Status = ((pAP->WPS_Enable) &&
                           (pAP->secModeEnabled == APMSI_NONE || pAP->secModeEnabled > APMSI_WEP128IV) &&
                           (pAP->MF_Mode != APMFM_WHITELIST || pAP->WPS_CertMode));
    }

    /* Update NEMO for this... */
    amxd_object_set_cstring_t(pAP->pBus, "Status", cstr_AP_status[pAP->status]);
    amxd_object_set_cstring_t(amxd_object_findf(pAP->pBus, "WPS"),
                              "Status", cstr_AP_status[pAP->WPS_Status]);
    amxd_object_set_cstring_t(pSSID->pBus, "Status", Rad_SupStatus[pSSID->status]);
    return 0;
}

void SyncData_AP2OBJ(amxd_object_t* object, T_AccessPoint* pAP, int set) {
    char TBuf[256];
    amxc_var_t variant;
    amxc_var_init(&variant);

    SAH_TRACEZ_IN(ME);
    if(!(object && pAP && debugIsVapPointer(pAP))) {
        /* Missing data pointers... */
        return;
    }

    amxd_object_t* secObj = amxd_object_findf(object, "Security");
    amxd_object_t* wpsObj = amxd_object_findf(object, "WPS");
    if(set & SET) {
        amxc_string_t TBufStr;
        amxc_string_init(&TBufStr, 0);

        /* Set AP data in mapped OBJ structure */
        /* object, must be the one mapping on the pAP structure (/WIFI/AccessPoint/xxx */

        /** 'Status' Indicates the status of this access point.
         */
        amxd_object_set_cstring_t(object, "Status", cstr_AP_status[pAP->status]);
        /** 'Index' NeMo helper value, this will contain the key or
         *  name index value. We keep this also in the AP structure. */
        amxd_object_set_int32_t(object, "Index", pAP->index);
        /** 'Alias' A non-volatile handle used to reference this
         *  instance. Alias provides a mechanism for an ACS to label
         *  this instance for future reference. An initial unique value
         *  MUST be assigned when the CPE creates an instance of this
         *  object. (Currently it's the VAP name wl0,wl1,...)   */
        amxd_object_set_cstring_t(object, "Alias", pAP->alias);
        /** 'SSIDReference' The value MUST be the path name of a row in
         *  the SSID table. If the referenced object is deleted, the
         *  parameter value MUST be set to an empty string. */
        TBuf[0] = 0;
        if(pAP->pSSID) {
            swl_str_copy(TBuf, sizeof(TBuf), amxd_object_get_path(pAP->pSSID->pBus, AMXD_OBJECT_NAMED));
        }
        amxd_object_set_cstring_t(object, "SSIDReference", TBuf);
        TBuf[0] = 0;
        if(pAP->pRadio) {
            swl_str_copy(TBuf, sizeof(TBuf), amxd_object_get_path(pAP->pRadio->pBus, AMXD_OBJECT_NAMED));
        }
        amxd_object_set_cstring_t(object, "RadioReference", TBuf);
        /** 'RetryLimit' The maximum number of retransmission for a
         *  packet. This corresponds to IEEE 802.11 parameter
         *  do11ShortRetryLimit [0..127] */
        amxd_object_set_int32_t(object, "RetryLimit", pAP->retryLimit);
        /** 'WMMCapability' Indicates whether this access point supports
         *  WiFi Multimedia(WMM) Access Cagegories (AC). */
        amxd_object_set_int32_t(object, "WMMCapability", pAP->WMMCapability);
        /** 'UAPSDCapability' Indicates whether this access point
         *  supports WMM Unscheduled Automatic Power Save Delivery
         *  (U-APSD). */
        //set_OBJ_ParameterHelper(TPH_INT32 , object, "UAPSDCapability", &pAP->UAPSDCapability);
        amxc_var_set_bool(&variant, ((pAP->WMMCapability) ? pAP->UAPSDCapability : 0));
        amxd_param_set_value(amxd_object_get_param_def(object, "UAPSDCapability"), &variant);
        /** 'WMMEnable' Whether WMM support is currently enabled.
         *  When enabled, this is indicated in beacon frames */
        amxc_var_set_bool(&variant, ((pAP->WMMCapability) ? pAP->WMMEnable : 0));
        amxd_param_set_value(amxd_object_get_param_def(object, "WMMEnable"), &variant);
        /** 'UAPSDEnable' Whether U-APSD support is currently
         *  enabled. When enabled, this is indicated in beacon frames */
        amxc_var_set_bool(&variant, ((pAP->UAPSDCapability) ? pAP->UAPSDEnable : 0));
        amxd_param_set_value(amxd_object_get_param_def(object, "UAPSDEnable"), &variant);

        amxc_var_set_bool(&variant, pAP->MCEnable);
        amxd_param_set_value(amxd_object_get_param_def(object, "MCEnable"), &variant);

        amxc_var_set_bool(&variant, pAP->APBridgeDisable);
        amxd_param_set_value(amxd_object_get_param_def(object, "APBridgeDisable"), &variant);

        amxd_object_set_bool(object, "IsolationEnable", pAP->clientIsolationEnable);

        amxd_object_set_bool(object, "IEEE80211kEnabled", pAP->IEEE80211kEnable);

        amxd_object_set_bool(object, "WDSEnable", pAP->wdsEnable);

        bitmask_to_string(&TBufStr, cstr_MultiAPType, ',', pAP->multiAPType);
        amxd_object_set_cstring_t(object, "MultiAPType", TBufStr.buffer);

        amxd_object_set_cstring_t(amxd_object_findf(object, "IEEE80211r"),
                                  "NASIdentifier", pAP->NASIdentifier);

        amxd_object_set_cstring_t(amxd_object_findf(object, "IEEE80211r"),
                                  "R0KHKey", pAP->R0KHKey);

        /** 'AssociatedDeviceNumberOfEntries' The number of entries
         *  in the AssociatedDevice table. */
        amxd_object_set_int32_t(object, "AssociatedDeviceNumberOfEntries", pAP->AssociatedDeviceNumberOfEntries);

        /** 'ActiveAssociatedDeviceNumberOfEntries' The number of entries
         *  in the AssociatedDevice table. */
        amxd_object_set_int32_t(object, "ActiveAssociatedDeviceNumberOfEntries", pAP->ActiveAssociatedDeviceNumberOfEntries);

        /** Security part */
        /** 'ModesSupported' Comma separated list of strings. Inicates
         *  which security modes this AccessPoint instance is capable
         *  of supporting. */
        wld_ap_ModesSupported_mask_to_string(&TBufStr, pAP->secModesSupported);

        amxd_object_set_cstring_t(secObj, "ModesSupported", TBufStr.buffer);

        wld_ap_ModesSupported_mask_to_string(&TBufStr, pAP->secModesAvailable);

        amxd_object_set_cstring_t(secObj, "ModesAvailable", TBufStr.buffer);

        /** 'ModeEnabled' The value MUST be a member of the list
         *  reported by the ModesSupported parameter. Indicates which
         *  security mode is enabled. */
        amxd_object_set_cstring_t(secObj, "ModeEnabled", cstr_AP_ModesSupported[pAP->secModeEnabled]);
        amxd_object_set_cstring_t(secObj, "MFPConfig", wld_mfpConfig_str[pAP->mfpConfig]);
        amxd_object_set_int32_t(secObj, "SPPAmsdu", pAP->sppAmsdu);

        /** 'WEPKey' A WEP key expressed as a hexadecimal string.
         *  WEPKey is used only if ModeEnabled is set to WEP-64,
         *  WEP-128 or WEP-128iv. A 5 byte WEPKey corresponds to
         *  security mode WEP-64 and a 13 byte WEPKey cooresponds to
         *  security mode WEP-128. */
        if(isValidWEPKey(pAP->WEPKey)) {    // Don't change if key isn't valid!
            amxd_object_set_cstring_t(secObj, "WEPKey", pAP->WEPKey);
        }

        /** 'PreSharedKey' A literal PreSharedKey (PSK) expressed as
         *  a hexadecimal string. PresharedKey is only used if
         *  ModeEnalbed is set to WPA/WPA2/WPA-WPA2-personal. If
         *  KeyPassphrase is written, then PreSharedKey is immediatley
         *  generated. The ACS SHOULD NOT set both the KeyPassphrase and
         *  the PreSharedKey directly (the result of doing this is
         *  undefined). When read this parameter returns an empty
         *  string, regardless of the actual value. */
        if(isValidPSKKey(pAP->preSharedKey)) {  // Don't change if key isn't valid!
            amxd_object_set_cstring_t(secObj, "PreSharedKey", pAP->preSharedKey);
        }

        /** 'KeyPassphrase' A passhprase from which the PreSharedKey
         *  is to be generated, for WPA/WPA2 or WPA-WPA2-Personal
         *  security modes. If KeyPassphrase is written, then
         *  PreSharedkey is immediatly generated. The ACS SHOULD NOT
         *  set both the KeyPassphrase and the PreSharedKey directly
         *  (The result of doing this is undefined). The key is
         *  generated as specified by WPA. which use PBKDF2 from PKCS
         *  #5: password-based Cryptography7 Specifiction Version
         *  2.0. When read, this paramter returns an empty string,
         *  regardless of the actual value. */
        if(isValidAESKey(pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {     // Don't change if key isn't valid!
            amxd_object_set_cstring_t(secObj, "KeyPassPhrase", pAP->keyPassPhrase);
        }

        if(isValidAESKey(pAP->saePassphrase, SAE_KEY_SIZE_LEN)) {     // Don't change if key isn't valid!
            amxd_object_set_cstring_t(secObj, "SAEPassphrase", pAP->saePassphrase);
        }

        /** 'EncryptionMode'  */
        amxd_object_set_cstring_t(secObj, "EncryptionMode", cstr_AP_EncryptionMode[pAP->encryptionModeEnabled]);

        /** 'RekeyingInterval' The interval (expressed in seconds) in
         *  which the keys are re-generated. This is applicable to
         *  WPA/WPA2 and Mixed modes in personal or Enterprise mode
         *  (i.e. when ModeEnabled is set to a value other than Non
         *  or WEP-64/128/128iv) */
        amxd_object_set_uint32_t(secObj, "RekeyingInterval", pAP->rekeyingInterval);

        amxd_object_set_cstring_t(secObj, "OWETransitionInterface", pAP->oweTransModeIntf);

        amxd_object_set_uint16_t(secObj, "TransitionDisable", pAP->transitionDisable);
        /** 'RadiusServerIPAddr' [IPAddress] The IP address of the
         *  RADIUS server used for WLAN security. RadiusServerIPAddr
         *  is only applicable when ModeEnabled is an Enterprise type
         *  (WPA/WPA2/WPA-WPA2-Enterprise) */
        amxd_object_set_cstring_t(secObj, "RadiusServerIPAddr", pAP->radiusServerIPAddr);

        /** 'RadiusServerPort' The port number of the RADIUS server
         *  used for WLAN security. RadiusServerPort is only
         *  applicable when ModeEnabled is an Enterprise type
         *  (WPA/WPA2/WPA-WPA2-Enterprise) */
        amxd_object_set_uint32_t(secObj, "RadiusServerPort", pAP->radiusServerPort);

        /** 'RadiusSecret' The secret used for handshaking with the
         *  RADIUS server [RFC2865]. When read, this parameter
         *  returns an empty string, regardless of the actual value. */
        amxd_object_set_cstring_t(secObj, "RadiusSecret", pAP->radiusSecret);

        amxd_object_set_int32_t(secObj, "RadiusDefaultSessionTimeout", pAP->radiusDefaultSessionTimeout);
        amxd_object_set_cstring_t(secObj, "RadiusOwnIPAddress", pAP->radiusOwnIPAddress);
        amxd_object_set_cstring_t(secObj, "RadiusNASIdentifier", pAP->radiusNASIdentifier);
        amxd_object_set_cstring_t(secObj, "RadiusCalledStationId", pAP->radiusCalledStationId);
        amxd_object_set_int32_t(secObj, "RadiusChargeableUserId", pAP->radiusChargeableUserId);

        /** WPS Part */

        /** 'Enable' Enables or disables WPS functionality for this
         *  accesspoint. */
        amxd_object_set_int32_t(wpsObj, "Enable", pAP->WPS_Enable);
        amxd_object_set_cstring_t(wpsObj, "SelfPIN", g_wpsConst.DefaultPin);

        /** 'ConfigMethodsSupported' Comma-separated list of strings.
         *  Indicates WPS configuration methods supported by the
         *  device. This parameter corresponds directly to the "Config
         *  Methods" attribute of the WPS specification [WPSv1.0]. The
         *  PushButton and PIN methods MUST be supported!!!
         */

        wld_wps_ConfigMethods_mask_to_string(&TBufStr, pAP->WPS_ConfigMethodsSupported);

        amxd_object_set_cstring_t(wpsObj, "ConfigMethodsSupported", TBufStr.buffer);

        /** 'ConfigMethodsEnabled' Comma-separated list of strings.
         *  Each list item MUST be a member of the list reported by
         *  the ConfigMethodsSupported parameter. Indicates WPS
         *  configuration methods enabled on the device */
        wld_wps_ConfigMethods_mask_to_string(&TBufStr, pAP->WPS_ConfigMethodsEnabled);

        amxd_object_set_cstring_t(wpsObj, "ConfigMethodsEnabled", TBufStr.buffer);

        /** 'Configured'
         *  Indicates WPS that AP is in configured or unconfigured state
         *  (Out Of the Box OOB mode)
         */
        amxd_object_set_int32_t(wpsObj, "Configured", pAP->WPS_Configured);

        amxd_object_set_cstring_t(wpsObj, "UUID", g_wpsConst.UUID);

        amxc_string_clean(&TBufStr);
    } else {
        /* Get AP data from OBJ to AP */
        int32_t tmp_int32 = 0;
        uint32_t tmp_uint32 = 0;
        bool tmp_bool = false;
        bool commit = false;

        // For enable just update the state.
        pAP->enable = amxd_object_get_bool(object, "Enable", NULL);

        tmp_bool = amxd_object_get_bool(object, "SSIDAdvertisementEnabled", NULL);
        if(pAP->SSIDAdvertisementEnabled != tmp_bool) {
            pAP->SSIDAdvertisementEnabled = tmp_bool;
            if(!pAP->SSIDAdvertisementEnabled) {   // Hiding SSID? Backup our current WPS state?
                pAP->WPS_Status = pAP->WPS_Enable; // Bug 32192 comment #10 !
            }
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "WMMEnable", NULL);
        if(pAP->WMMEnable != tmp_bool) {
            pAP->WMMEnable = tmp_bool;
            pAP->pFA->mfn_wvap_enable_wmm(pAP, pAP->WMMEnable, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "UAPSDEnable", NULL);
        if(pAP->UAPSDEnable != tmp_bool) {
            pAP->UAPSDEnable = tmp_bool;
            pAP->pFA->mfn_wvap_enable_uapsd(pAP, pAP->UAPSDEnable, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "MCEnable", NULL);
        if(pAP->MCEnable != tmp_bool) {
            pAP->MCEnable = tmp_bool;
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "APBridgeDisable", NULL);
        if(pAP->APBridgeDisable != tmp_bool) {
            pAP->APBridgeDisable = tmp_bool;
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "IsolationEnable", NULL);
        if(pAP->clientIsolationEnable != tmp_bool) {
            pAP->clientIsolationEnable = tmp_bool;
            pAP->pFA->mfn_wvap_sync(pAP, SET);
        }

        tmp_bool = amxd_object_get_bool(object, "IEEE80211kEnabled", NULL);
        if(pAP->IEEE80211kEnable != tmp_bool) {
            pAP->IEEE80211kEnable = tmp_bool;
            commit = true;
        }

        char* mode = amxd_object_get_cstring_t(secObj, "ModeEnabled", NULL);
        if(strncmp(cstr_AP_ModesSupported[pAP->secModeEnabled], mode,
                   strlen(cstr_AP_ModesSupported[pAP->secModeEnabled]))) {
            /* We still must update the pAP->secModeEnabled value */
            int idx = conv_ModeIndexStr(cstr_AP_ModesSupported, mode);
            if(idx >= 0) {
                pAP->secModeEnabled = idx;
                wld_ap_sec_doSync(pAP);
            }
        }
        free(mode);

        char* mfp = amxd_object_get_cstring_t(secObj, "MFPConfig", NULL);
        if(strncmp(wld_mfpConfig_str[pAP->mfpConfig], mfp,
                   strlen(wld_mfpConfig_str[pAP->mfpConfig]))) {
            /* We still must update the pAP->secModeEnabled value */
            wld_mfpConfig_e idx = conv_strToEnum(wld_mfpConfig_str, mfp, WLD_MFP_MAX, WLD_MFP_DISABLED);
            if(idx != pAP->mfpConfig) {
                pAP->mfpConfig = idx;
                wld_ap_sec_doSync(pAP);
            }
        }
        free(mfp);

        tmp_int32 = amxd_object_get_int32_t(secObj, "SPPAmsdu", NULL);
        if(pAP->sppAmsdu != tmp_int32) {
            pAP->sppAmsdu = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* wepKey = amxd_object_get_cstring_t(secObj, "WEPKey", NULL);
        if(strncmp(pAP->WEPKey, wepKey, strlen(pAP->WEPKey))) {
            if(isValidWEPKey(wepKey)) {
                wldu_copyStr(pAP->WEPKey, wepKey, sizeof(pAP->WEPKey));
                wld_ap_sec_doSync(pAP);
            }
        }
        free(wepKey);

        char* pskKey = amxd_object_get_cstring_t(secObj, "PreSharedKey", NULL);
        if(strncmp(pAP->preSharedKey, pskKey, strlen(pAP->preSharedKey))) {
            if(isValidPSKKey(pskKey)) {
                wldu_copyStr(pAP->preSharedKey, pskKey, sizeof(pAP->preSharedKey));
                wld_ap_sec_doSync(pAP);
            }
        }
        free(pskKey);

        char* keyPassPhrase = amxd_object_get_cstring_t(secObj, "KeyPassPhrase", NULL);
        if(strncmp(pAP->keyPassPhrase, keyPassPhrase, strlen(pAP->keyPassPhrase))) {
            if(isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                wldu_copyStr(pAP->keyPassPhrase, keyPassPhrase, sizeof(pAP->keyPassPhrase));
                wld_ap_sec_doSync(pAP);
            }
        }
        free(keyPassPhrase);

        char* saePassphrase = amxd_object_get_cstring_t(secObj, "SAEPassphrase", NULL);
        if(strncmp(pAP->saePassphrase, saePassphrase, strlen(pAP->saePassphrase))) {
            if(isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN)) {
                wldu_copyStr(pAP->saePassphrase, saePassphrase, sizeof(pAP->saePassphrase));
                wld_ap_sec_doSync(pAP);
            }
        }
        free(saePassphrase);

        char* encryption = amxd_object_get_cstring_t(secObj, "EncryptionMode", NULL);
        if(strncmp(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled], encryption,
                   strlen(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled]))) {
            pAP->encryptionModeEnabled = conv_strToEnum(cstr_AP_EncryptionMode, encryption, APEMI_MAX, APEMI_DEFAULT);
            wld_ap_sec_doSync(pAP);
        }
        free(encryption);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RekeyingInterval", NULL);
        if(pAP->rekeyingInterval != tmp_int32) {
            pAP->rekeyingInterval = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radSvrIp = amxd_object_get_cstring_t(secObj, "RadiusServerIPAddr", NULL);
        if(strncmp(pAP->radiusServerIPAddr, radSvrIp, strlen(pAP->radiusServerIPAddr))) {
            wldu_copyStr(pAP->radiusServerIPAddr, radSvrIp, sizeof(pAP->radiusServerIPAddr));
            wld_ap_sec_doSync(pAP);
        }
        free(radSvrIp);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusServerPort", NULL);
        if(pAP->radiusServerPort != tmp_int32) {
            pAP->radiusServerPort = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radSecret = amxd_object_get_cstring_t(secObj, "RadiusSecret", NULL);
        if(strncmp(pAP->radiusSecret, radSecret, strlen(pAP->radiusSecret))) {
            wldu_copyStr(pAP->radiusSecret, radSecret, sizeof(pAP->radiusSecret));
            wld_ap_sec_doSync(pAP);
        }
        free(radSecret);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusDefaultSessionTimeout", NULL);
        if(pAP->radiusDefaultSessionTimeout != tmp_int32) {
            pAP->radiusDefaultSessionTimeout = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radOwnIp = amxd_object_get_cstring_t(secObj, "RadiusOwnIPAddress", NULL);
        if(strncmp(pAP->radiusOwnIPAddress, radOwnIp, strlen(pAP->radiusOwnIPAddress))) {
            wldu_copyStr(pAP->radiusOwnIPAddress, radOwnIp, sizeof(pAP->radiusOwnIPAddress));
            wld_ap_sec_doSync(pAP);
        }
        free(radOwnIp);

        char* radNasId = amxd_object_get_cstring_t(secObj, "RadiusNASIdentifier", NULL);
        if(strncmp(pAP->radiusNASIdentifier, radNasId, strlen(pAP->radiusNASIdentifier))) {
            wldu_copyStr(pAP->radiusNASIdentifier, radNasId, sizeof(pAP->radiusNASIdentifier));
            wld_ap_sec_doSync(pAP);
        }
        free(radNasId);

        char* radStaId = amxd_object_get_cstring_t(secObj, "RadiusCalledStationId", NULL);
        if(strncmp(pAP->radiusCalledStationId, radStaId, strlen(pAP->radiusCalledStationId))) {
            wldu_copyStr(pAP->radiusCalledStationId, radStaId, sizeof(pAP->radiusCalledStationId));
            wld_ap_sec_doSync(pAP);
        }
        free(radStaId);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusChargeableUserId", NULL);
        if(pAP->radiusChargeableUserId != tmp_int32) {
            pAP->radiusChargeableUserId = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        tmp_bool = amxd_object_get_bool(wpsObj, "Enable", NULL);
        if(pAP->WPS_Enable != tmp_bool) {
            pAP->WPS_Enable = tmp_bool;
            wld_ap_doWpsSync(pAP);
        }

        char* cfgMethods = amxd_object_get_cstring_t(wpsObj, "ConfigMethodsEnabled", NULL);
        wld_wps_ConfigMethods_string_to_mask(&tmp_uint32, cfgMethods, ',');
        if(pAP->WPS_ConfigMethodsEnabled != tmp_uint32) {
            pAP->WPS_ConfigMethodsEnabled = tmp_uint32;
            wld_ap_doWpsSync(pAP);
            commit = true;
        }
        free(cfgMethods);

        tmp_int32 = amxd_object_get_int32_t(wpsObj, "Configured", NULL);
        if(pAP->WPS_Configured != tmp_int32) {
            pAP->WPS_Configured = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* oweTransIntf = amxd_object_get_cstring_t(secObj, "OWETransitionInterface", NULL);
        if(strncmp(pAP->oweTransModeIntf, oweTransIntf, strlen(pAP->oweTransModeIntf))) {
            swl_str_copy(pAP->oweTransModeIntf, sizeof(pAP->oweTransModeIntf), oweTransIntf);
            wld_ap_sec_doSync(pAP);
        }
        free(oweTransIntf);

        char* tdStr = amxd_object_get_cstring_t(secObj, "TransitionDisable", NULL);
        wld_ap_td_m transitionDisable = swl_conv_charToMask(tdStr, g_str_wld_ap_td, AP_TD_MAX);
        if(pAP->transitionDisable != transitionDisable) {
            pAP->transitionDisable = transitionDisable;
            wld_ap_sec_doSync(pAP);
        }
        free(tdStr);
        if(commit) {
            wld_ap_doSync(pAP);
        }
    }
    amxc_var_clean(&variant);
    SAH_TRACEZ_OUT(ME);
}

/* Be sure that our attached memory structure is cleared */
void t_destroy_handler_AP (amxd_object_t* object) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    ASSERT_TRUE(debugIsVapPointer(pAP), , ME, "INVALID");

    SAH_TRACEZ_INFO(ME, "%s : destroy AP", pAP->alias);
    free(pAP);
    object->priv = NULL;
}


amxd_status_t _validateWPSenable(amxd_param_t* parameter) {
    T_AccessPoint* pAP;
    amxd_object_t* wifiVap;
    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(parameter, &value);
    bool wps_enable = amxc_var_get_bool(&value);
    amxc_var_clean(&value);
    wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    pAP = wifiVap->priv;

    if(!pAP) {
        return amxd_status_ok;
    }

    if(wps_enable && (pAP->MF_Mode == APMFM_WHITELIST) && !pAP->WPS_CertMode) {
        SAH_TRACEZ_ERROR(ME, "WPS cannot be enabled if MAC Filtering is set to WhiteList");
        return amxd_status_invalid_action;
    }

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setWDSEnable_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter _UNUSED,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set WDSEnable %d", new_value);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");
    pAP->wdsEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setMBOEnable_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter _UNUSED,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool new_value = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set MBOEnable %d", new_value);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "NULL");
    pAP->mboEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setMultiAPType_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");
    char* new_multiAPType = amxc_var_dyncast(cstring_t, args);
    uint32_t m_multiAPType = 0;
    if(new_multiAPType && new_multiAPType[0]) {
        string_to_bitmask(&m_multiAPType, new_multiAPType, cstr_MultiAPType, NULL, ',');
    } else {
        m_multiAPType = 0;
    }
    free(new_multiAPType);
    if(pAP->multiAPType != m_multiAPType) {
        pAP->multiAPType = m_multiAPType;
        pAP->pFA->mfn_wvap_multiap_update_type(pAP);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setRole_pwf(amxd_object_t* object _UNUSED,
                                  amxd_param_t* parameter _UNUSED,
                                  amxd_action_t reason _UNUSED,
                                  const amxc_var_t* const args _UNUSED,
                                  amxc_var_t* const retval _UNUSED,
                                  void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");
    const char* new_str_apRole = amxc_var_constcast(cstring_t, args);
    uint32_t new_apRole = conv_ModeIndexStr(wld_apRole_str, new_str_apRole);
    if(pAP->apRole != new_apRole) {
        pAP->apRole = new_apRole;
        pAP->pFA->mfn_wvap_set_ap_role(pAP);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_enableVendorIEs(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTI_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "VAP Object is NULL");

    pAP->enableVendorIEs = amxc_var_dyncast(bool, args);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "INVALID");
    pAP->pFA->mfn_wvap_enab_vendor_ie(pAP, pAP->enableVendorIEs);

    return amxd_status_ok;
}

static bool isVendorIEValid(const char* oui, const char* data) {
    if(!oui || (strlen(oui) != SWL_OUI_STR_LEN - 1)) {
        SAH_TRACEZ_ERROR(ME, "OUI is invalid (null or wrong length)");
        return false;
    }

    if(!data || (strlen(data) <= 2) || (strlen(data) > WLD_VENDORIE_T_DATA_SIZE) || (strlen(data) % 2 != 0)) {
        SAH_TRACEZ_ERROR(ME, "Data is invalid (null or wrong length)");
        return false;
    }
    const char* p = data;
    unsigned int i;
    for(i = 0; i < strlen(data); i++) {
        if(!isxdigit(p[i])) {
            SAH_TRACEZ_ERROR(ME, "Data is invalid (not hexa)");
            return false;
        }
    }
    return true;
}

static wld_vendorIe_t* getVendorIE(T_AccessPoint* pAP, const char* oui, const char* data) {
    ASSERTS_NOT_NULL(pAP, NULL, ME, "NULL");
    ASSERT_NOT_NULL(oui, NULL, ME, "NULL");
    ASSERT_NOT_NULL(data, NULL, ME, "NULL");
    ASSERTI_FALSE(swl_str_matches("", oui) || swl_str_matches("", data), NULL, ME, "empty string");

    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &pAP->vendorIEs) {
        wld_vendorIe_t* vendor_ie_elt = amxc_llist_it_get_data(llit, wld_vendorIe_t, it);
        if(swl_str_matches(vendor_ie_elt->oui, oui) && swl_str_matches(vendor_ie_elt->data, data)) {
            return vendor_ie_elt;
        }
    }

    return NULL;
}

static wld_vendorIe_t* addVendorIEEntry(T_AccessPoint* pAP, amxd_object_t* instance_object) {
    ASSERTS_NOT_NULL(pAP, NULL, ME, "NULL");
    wld_vendorIe_t* new_vendor_ie = calloc(1, sizeof(wld_vendorIe_t));
    ASSERTS_NOT_NULL(new_vendor_ie, NULL, ME, "NULL");
    new_vendor_ie->object = instance_object;
    instance_object->priv = new_vendor_ie;
    amxc_llist_append(&pAP->vendorIEs, &new_vendor_ie->it);
    return new_vendor_ie;
}

amxd_status_t _wld_ap_addVendorIE_ocf(amxd_object_t* template_object _UNUSED, amxd_object_t* instance_object) {
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(instance_object)));
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    T_AccessPoint* pAP = wifiVap->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "pAP NULL");
    addVendorIEEntry(pAP, instance_object);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setVendorIE_owf(amxd_object_t* instance_object) {
    const char* oui = NULL;
    const char* data = NULL;
    const char* frame_type_var = NULL;
    uint32_t frame_type;

    ASSERTS_FALSE(amxd_object_get_type(instance_object) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(instance_object)));
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");

    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");
    wld_vendorIe_t* vendor_ie = (wld_vendorIe_t*) instance_object->priv;

    if(!vendor_ie) {
        vendor_ie = addVendorIEEntry(pAP, instance_object);
    }

    ASSERTS_NOT_NULL(vendor_ie, amxd_status_unknown_error, ME, "NULL");

    oui = amxd_object_get_cstring_t(instance_object, "OUI", NULL);
    data = amxd_object_get_cstring_t(instance_object, "Data", NULL);

    if(getVendorIE(pAP, oui, data)) {
        SAH_TRACEZ_ERROR(ME, "Vendor IE already present or can not be modified");
        return amxd_status_ok;
    }

    frame_type_var = amxd_object_get_cstring_t(instance_object, "FrameType", NULL);
    ASSERT_TRUE(isVendorIEValid(oui, data), amxd_status_unknown_error, ME, "Input are invalid");
    frame_type = conv_strToMaskSep(frame_type_var, wld_vendorIe_frameType_str, VENDOR_IE_MAX, ',');

    swl_str_copy(vendor_ie->oui, SWL_OUI_STR_LEN, oui);
    swl_str_copy(vendor_ie->data, WLD_VENDORIE_T_DATA_SIZE, data);
    vendor_ie->frame_type = frame_type;
    pAP->pFA->mfn_wvap_add_vendor_ie(pAP, vendor_ie);

    return amxd_status_ok;
}

amxd_status_t _wld_ap_delVendorIE_odf(amxd_object_t* template_object _UNUSED, amxd_object_t* instance_object) {
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(instance_object)));
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    T_AccessPoint* pAP = wifiVap->priv;

    wld_vendorIe_t* vendor_ie = (wld_vendorIe_t*) instance_object->priv;
    ASSERT_NOT_NULL(vendor_ie, amxd_status_unknown_error, ME, "Failed to write vendor IE, no user data obj");

    pAP->pFA->mfn_wvap_del_vendor_ie(pAP, vendor_ie);
    amxc_llist_it_take(&vendor_ie->it);
    free(vendor_ie);
    SAH_TRACEZ_INFO(ME, "Deleted vendor IE");

    return amxd_status_ok;
}

amxd_status_t _createVendorIE(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* ret _UNUSED) {

    int res = amxd_status_unknown_error;
    amxd_object_t* object;
    const char* oui = GET_CHAR(args, "oui");
    const char* data = GET_CHAR(args, "data");
    const char* frame_type_var = GET_CHAR(args, "frame_type");

    amxd_object_t* wifiVap = amxd_object_get_parent(obj);
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    T_AccessPoint* pAP = wifiVap->priv;

    if(!isVendorIEValid(oui, data)) {
        goto exit;
    }

    if(getVendorIE(pAP, oui, data)) {
        SAH_TRACEZ_ERROR(ME, "Vendor IE already present");
        goto exit;
    }

    amxd_object_t* vendor_ie_template = amxd_object_findf(pAP->pBus, "VendorIEs.VendorIE");
    if(!vendor_ie_template) {
        SAH_TRACEZ_ERROR(ME, "NULL");
        goto exit;
    }
    char name[16];
    snprintf(name, sizeof(name), "%s_%.6s", oui, data);

    object = amxd_object_findf(vendor_ie_template, "%s", name);
    if(object) {
        SAH_TRACEZ_ERROR(ME, "Object already exists");
        goto exit;
    }

    amxd_object_new_instance(&object, vendor_ie_template, name, 0, NULL);
    if(!object) {
        SAH_TRACEZ_ERROR(ME, "failed to create new vendor IE object");
        goto exit;
    }

    amxd_object_set_cstring_t(object, "OUI", oui);
    amxd_object_set_cstring_t(object, "Data", data);
    amxd_object_set_cstring_t(object, "FrameType", frame_type_var);

    res = amxd_status_ok;

exit:
    return res;
}

amxd_status_t _deleteVendorIE(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    int res = amxd_status_unknown_error;
    const char* oui = GET_CHAR(args, "oui");
    const char* data = GET_CHAR(args, "data");

    amxd_object_t* wifiVap = amxd_object_get_parent(obj);
    T_AccessPoint* pAP = wifiVap->priv;
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");

    if(!isVendorIEValid(oui, data)) {
        goto exit;
    }

    wld_vendorIe_t* vendor_ie = getVendorIE(pAP, oui, data);
    if(vendor_ie) {
        amxd_object_delete(&vendor_ie->object);
    } else {
        SAH_TRACEZ_INFO(ME, "Vendor IE not found");
    }

    res = amxd_status_ok;

exit:
    return res;
}

amxd_status_t _wld_ap_setHotSpotEnable_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool CB = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set HotSpotEnable %d", CB);
    if(pAP) {
        pAP->pFA->mfn_hspot_enable(pAP, CB, SET);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


amxd_status_t _validateHotSpot2Requirements(amxd_param_t* parameter) {
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = NULL;

    wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    pAP = wifiVap->priv;
    ASSERTI_TRUE(debugIsVapPointer(pAP), amxd_status_ok, ME, "VAP Object is NULL");

    if(!pAP->enable) {
        SAH_TRACEZ_INFO(ME, "Error: Access point is disabled");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

/* Broadcom requires that wpa2 is used when starting Hotspot. This only applies for 4.12.
 *  Later version don't have this requirement */
    if(pAP->secModeEnabled != APMSI_WPA2_E) {
        SAH_TRACEZ_INFO(ME, "Error: secModeEnabled is not  APMSI_WPA2_E ");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

static const char* HotSpot_Config_ObjParam[] = {
    "bool Enable",
    "bool DgafDisable",
    "string L2TrafficInspect",
    "bool IcmpV4Echo",
    "%persistent uint32 Interworking",
    "bool Internet",
    "%persistent uint32 Additional",
    "bool Hs2Ie",
    "bool P2PEnable",
    "int32 GasDelay",
    "uint8 AccessNetworkType",
    "uint8 VenueType",
    "uint8 VenueGroup",
    "string VenueName",
    "string HeSSID",
    "string RoamingConsortium",
    "string DomainName",
    "string Anqp3gpp_CellNet",
    "read-only string WanMetrics",
    "string OperatingClass",
    NULL,
};

typedef enum {
    HS_ENABLE,
    HS_DGAF_DISABLE,
    HS_L2_TRAFFIC_INSPECT,
    HS_ICMPV4_ECHO,
    HS_INTERWORKING,
    HS_INTERNET,
    HS_ADDITIONAL,
    HS_HS2_IE,
    HS_P2P_ENABLE,
    HS_GAS_DELAY,
    HS_ACCESS_NETWORK_TYPE,
    HS_VENUE_TYPE,
    HS_VENUE_GROUP,
    HS_VENUE_NAME,
    HS_HESSID,
    HS_ROAMING_CONSORTIUM,
    HS_DOMAIN_NAME,
    HS_ANQP_3GPP_CELLNET,
    HS_WAN_METRICS,
    HS_OPERATING_CLASS,
    HS_END_TOKEN
} hotspotstates_t;

amxd_status_t _wld_ap_configHotSpot_pwf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter _UNUSED,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args _UNUSED,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {

    int idx = 0;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* pname = parameter->name;
    SAH_TRACEZ_INFO(ME, "configHotSpot %s", pname);

    if(pAP && pname && findStrInArray(pname, HotSpot_Config_ObjParam, &idx)) {
        SAH_TRACEZ_INFO(ME, "switch case idx = %d ('%s') PAP %p  ", idx, pname, pAP);
        switch(idx) {
        case HS_DGAF_DISABLE:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.dgaf_disable = CB;
            break;
        }
        case HS_L2_TRAFFIC_INSPECT:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.l2_traffic_inspect = CB;
            break;
        }
        case HS_ICMPV4_ECHO:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.icmpv4_echo = CB;
            break;
        }
        case HS_INTERWORKING:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.interworking = CB;
            break;
        }
        case HS_INTERNET:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.internet = CB;
            break;
        }
        case HS_HS2_IE:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.hs2_ie = CB;
            break;
        }
        case HS_P2P_ENABLE:
        {
            bool CB = amxc_var_dyncast(bool, args);
            pAP->HotSpot2.p2p_enable = CB;
            break;
        }
        case HS_GAS_DELAY:
        {
            int CB = amxc_var_dyncast(int32_t, args);
            pAP->HotSpot2.gas_delay = CB;
            break;
        }
        case HS_ACCESS_NETWORK_TYPE:
        {
            int8_t CB = amxc_var_dyncast(int8_t, args);
            pAP->HotSpot2.access_network_type = CB;
            break;
        }
        case HS_VENUE_TYPE:
        {
            int8_t CB = amxc_var_dyncast(int8_t, args);
            pAP->HotSpot2.venue_type = CB;
            break;
        }
        case HS_VENUE_GROUP:
        {
            int8_t CB = amxc_var_dyncast(int8_t, args);
            pAP->HotSpot2.venue_group = CB;
            break;
        }
        default:
            break;
        }
        pAP->pFA->mfn_hspot_config(pAP, SET);
    }

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setDiscoveryMethod_pwf(amxd_object_t* object _UNUSED,
                                             amxd_param_t* parameter _UNUSED,
                                             amxd_action_t reason _UNUSED,
                                             const amxc_var_t* const args _UNUSED,
                                             amxc_var_t* const retval _UNUSED,
                                             void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "pAP NULL");

    T_Radio* pRad = (T_Radio*) pAP->pRadio;

    const char* dmStr = amxc_var_constcast(cstring_t, args);
    wld_ap_dm_m discoveryMethod = swl_conv_charToMask(dmStr, g_str_wld_ap_dm, AP_DM_MAX);

    /* Discovery method (UPR/FILS) is mandatory on 6GHz if no discovery
     * method is present on other bands (2.4/5GHz). Check if RNR is present
     */
    bool rnrEnabled = false;
    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &g_radios) {
        T_Radio* pOtherRad = amxc_llist_it_get_data(llit, T_Radio, it);
        if((swl_str_matches(pRad->Name, pOtherRad->Name)) ||
           (pOtherRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ)) {
            continue;
        }
        amxc_llist_it_t* it;
        for(it = amxc_llist_get_first(&pRad->llAP); it; it = amxc_llist_it_get_next(it)) {
            T_AccessPoint* pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
            if(pAP->discoveryMethod & M_AP_DM_RNR) {
                rnrEnabled = true;
                break;
            }
        }
    }

    ASSERTI_NOT_EQUALS(discoveryMethod, pAP->discoveryMethod, amxd_status_ok, ME, "%s: no change", pAP->alias);
    ASSERTI_FALSE((discoveryMethod & M_AP_DM_DISABLED) && (discoveryMethod & ~M_AP_DM_DISABLED), amxd_status_unknown_error, ME, "Can not set disabled with other methods");
    ASSERTI_FALSE(rnrEnabled && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod == M_AP_DM_DISABLED), amxd_status_unknown_error, ME, "Disabled is not valid for 6GHz AP");
    ASSERTI_FALSE((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod & M_AP_DM_UPR), amxd_status_unknown_error, ME, "Unsolicted Probe Responses method is 6GHz AP only");
    ASSERTI_FALSE((discoveryMethod & M_AP_DM_UPR) && (discoveryMethod & M_AP_DM_FD), amxd_status_unknown_error, ME, "Unsolicited Probe Response and FILS Discovery must not be enabled at the same time");

    pAP->discoveryMethod = discoveryMethod;
    pAP->pFA->mfn_wvap_set_discovery_method(pAP);

    SAH_TRACEZ_INFO(ME, "Configure discovery method: %d", pAP->discoveryMethod);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setEnable_pwf(amxd_object_t* object _UNUSED,
                                    amxd_param_t* parameter _UNUSED,
                                    amxd_action_t reason _UNUSED,
                                    const amxc_var_t* const args _UNUSED,
                                    amxc_var_t* const retval _UNUSED,
                                    void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool CB = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "setAccessPointEnable %d", CB);

    if(pAP && debugIsVapPointer(pAP)) {
        T_Radio* pRad = (T_Radio*) pAP->pRadio;
        T_SSID* pSSID = (T_SSID*) pAP->pSSID;

        pAP->pFA->mfn_wvap_enable(pAP, CB, SET);

        /* Always AUTO commit IF it's not blocked by Radio.xxx.edit() mode! */
        wld_rad_doCommitIfUnblocked(pRad);
        if(debugIsSsidPointer(pSSID)) {
            amxd_object_set_bool(pSSID->pBus, "Enable", CB);
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

T_AccessPoint* wld_ap_create(T_Radio* pRad, const char* vapName, uint32_t idx) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_AccessPoint* pAP = calloc(1, sizeof(T_AccessPoint));
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");

    pAP->debug = VAP_POINTER;
    pAP->index = 0;
    pAP->enable = 0;
    pAP->ref_index = idx;

    pAP->pRadio = pRad;
    pAP->pFA = pRad->pFA;
    snprintf(pAP->name, sizeof(pAP->name), "%s", vapName);
    snprintf(pAP->alias, sizeof(pAP->alias), "%s", vapName);
    pAP->fsm.FSM_SyncAll = TRUE;

    wldu_copyStr(pAP->keyPassPhrase, "password", sizeof(pAP->keyPassPhrase));
    wldu_copyStr(pAP->WEPKey, "123456789ABCDEF0123456789A", sizeof(pAP->WEPKey));
    sprintf(pAP->preSharedKey, "password_%d", idx);
    sprintf(pAP->radiusSecret, "RadiusPassword_%d", idx);
    wldu_copyStr(pAP->radiusServerIPAddr, "127.0.0.1", sizeof(pAP->radiusServerIPAddr));
    pAP->radiusServerPort = 1812;
    pAP->radiusDefaultSessionTimeout = 0;
    pAP->radiusOwnIPAddress[0] = '\0';
    pAP->radiusNASIdentifier[0] = '\0';
    pAP->radiusCalledStationId[0] = '\0';
    pAP->radiusChargeableUserId = 0;
    pAP->rekeyingInterval = WLD_SEC_PER_H;
    pAP->retryLimit = 3;
    pAP->SSIDAdvertisementEnabled = 1;
    pAP->status = APSTI_DISABLED;
    pAP->secModesSupported = APMS_NONE;
    pAP->MaxStations = -1;

    pAP->UAPSDCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "UAPSD", 0);
    pAP->WMMCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WME", 0);
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WEP", 0)) ?
        (APMS_WEP64 | APMS_WEP128 | APMS_WEP128IV) : 0;
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "AES", 0)) ?
        (APMS_WPA_P | APMS_WPA2_P | APMS_WPA_WPA2_P | APMS_NONE_E | APMS_WPA_E | APMS_WPA2_E | APMS_WPA_WPA2_E) : 0;
    if(pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "SAE", 0)) {
        pAP->secModesSupported |= APMS_WPA3_P;
        if(pAP->secModesSupported & APMS_WPA2_P) {
            pAP->secModesSupported |= APMS_WPA2_WPA3_P;
        }
    }
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "OWE", 0)) ?
        (APMS_OWE) : 0;
    pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, NULL, 0); // Update the Drv CAP value!
    pAP->pFA->mfn_wrad_poschans(pAP->pRadio, NULL, 0);         // Update Radio Bands...
    if(!pAP->MCEnable) {
        /* Init this interface... */
        T_Radio* pR = (T_Radio*) pAP->pRadio;
        pAP->MCEnable = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE;
        pAP->doth = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE; //marianna: hardcoded false on 2.4GHz and true on 5GHz, not on WiFiATH
    }

    if(pAP->pRadio->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
        pAP->secModeEnabled = APMSI_WPA3_P;
    } else {
        pAP->secModeEnabled = APMSI_WPA2_P;
    }
    pAP->secModesAvailable = pAP->secModesSupported;
    pAP->mfpConfig = WLD_MFP_DISABLED;
    pAP->MF_AddressList = NULL;
    pAP->MF_AddressListBlockSync = false;

    /* In case we've support for them, enable it by default. */
    pAP->UAPSDEnable = (pAP->UAPSDCapability) ? 1 : 0;
    pAP->WMMEnable = (pAP->WMMCapability) ? 1 : 0;
    pAP->WPS_ConfigMethodsSupported = (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL | M_WPS_CFG_MTHD_PBC_ALL | M_WPS_CFG_MTHD_PIN);
    pAP->WPS_ConfigMethodsEnabled = (M_WPS_CFG_MTHD_PBC | M_WPS_CFG_MTHD_PIN);
    pAP->WPS_Configured = TRUE;
    pAP->WPS_Enable = 0; /* Disable by default. Only '1' VAP can enable the WPS */
    pAP->defaultDeviceType = DEVICE_TYPE_DATA;
    memset(pAP->NASIdentifier, 0, NAS_IDENTIFIER_MAX_LEN);
    get_randomhexstr((unsigned char*) pAP->NASIdentifier, NAS_IDENTIFIER_MAX_LEN - 1);
    memset(pAP->R0KHKey, 0, KHKEY_MAX_LEN);
    get_randomhexstr((unsigned char*) pAP->R0KHKey, KHKEY_MAX_LEN - 1);
    pAP->addRelayApCredentials = false;
    pAP->wpsRestartOnRequest = false;
    pAP->cfg11u.interworkingEnable = false;
    memset(pAP->cfg11u.qosMapSet, 0, QOS_MAP_SET_MAX_LEN);
    return pAP;
}

int32_t wld_ap_initObj(T_AccessPoint* pAP, amxd_object_t* instance_object) {
    ASSERT_NOT_NULL(instance_object, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pAP, WLD_ERROR, ME, "NULL");

    instance_object->priv = pAP;
    pAP->pBus = instance_object;

    amxd_object_t* wpsinstance = amxd_object_get(instance_object, "WPS");
    pAP->wpsSessionInfo.intfObj = instance_object;
    wpsinstance->priv = &pAP->wpsSessionInfo;

    return WLD_OK;
}

int wld_ap_init(T_AccessPoint* pAP) {
    int ret = -1;
    T_Radio* pR = pAP->pRadio;

    /* Add pAP on linked list of pR */
    amxc_llist_append(&pR->llAP, &pAP->it);

    wld_ap_rssiMonInit(pAP);
    wld_assocDev_initAp(pAP);

    swl_circTable_init(&(pAP->lastAssocReq), &assocTable, 20);

    if((ret = pR->pFA->mfn_wvap_create_hook(pAP))) {
        SAH_TRACEZ_ERROR(ME, "%s: VAP create hook failed %d", pAP->alias, ret);
    }
    return ret;
}

/**
 * This function Destroys the access point
 * first deauthentify all associated stations, destroy vap then deletes the accesspoint.
 */
void wld_ap_destroy(T_AccessPoint* pAP) {
    T_Radio* pR = (T_Radio*) pAP->pRadio;
    /* Deauthentify all stations */
    SAH_TRACEZ_INFO(ME, "%s: Deauth all stations", pAP->alias);
    pAP->pFA->mfn_wvap_kick_sta_reason(pAP, "ff:ff:ff:ff:ff:ff", 17, SWL_IEEE80211_DEAUTH_REASON_UNABLE_TO_HANDLE_STA);

    /* Destroy vap*/
    pR->pFA->mfn_wvap_destroy_hook(pAP);

    for(int i = pAP->AssociatedDeviceNumberOfEntries - 1; i >= 0; i--) {
        wld_ad_destroy_associatedDevice(pAP, i);
    }

    wld_assocDev_cleanAp(pAP);
    wld_ap_rssiMonDestroy(pAP);

    swl_circTable_destroy(&(pAP->lastAssocReq));

    /* Try to delete the requested interface by calling the HW function */
    pR->pFA->mfn_wrad_delvapif(pR, pAP->name);

    free(pAP->dbgOutput);
    pAP->dbgOutput = NULL;

    /* Take VAP also out the Radio */
    amxc_llist_it_take(&pAP->it);

    if(pAP->pBus != NULL) {
        pAP->pBus->priv = NULL;
    }
    if(pAP->pSSID != NULL) {
        pAP->pSSID->AP_HOOK = NULL;
    }
    free(pAP);
}

/**
 * Write handler for associated devices.
 *
 * This function will retrieve the deviceType and devicePriority from the data model.
 * It will then call the plugin to update it's internal model with the change.
 *
 * Success or failure of plugin call is ignored.
 */
amxd_status_t _wld_ap_updateAssocDev(amxd_object_t* object _UNUSED,
                                     amxd_param_t* parameter _UNUSED,
                                     amxd_action_t reason _UNUSED,
                                     const amxc_var_t* const args _UNUSED,
                                     amxc_var_t* const retval _UNUSED,
                                     void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* station = object;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(station));
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_AssociatedDevice* assocDev = (T_AssociatedDevice*) station->priv;
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;

    SAH_TRACEZ_IN(ME);

    if(!(pAP && assocDev && debugIsVapPointer(pAP))) {
        SAH_TRACEZ_INFO(ME, "ap not yet init: ap %p dev %p", pAP, assocDev);
        return amxd_status_unknown_error;
    }

    char* deviceType = amxd_object_get_cstring_t(station, "DeviceType", NULL);
    int newDeviceType = conv_ModeIndexStr(cstr_DEVICE_TYPES, deviceType);
    free(deviceType);
    int newDevicePriority = amxd_object_get_int32_t(station, "DevicePriority", NULL);

    if((newDeviceType != assocDev->deviceType) || (newDevicePriority != assocDev->devicePriority)) {
        assocDev->deviceType = newDeviceType;
        assocDev->devicePriority = newDevicePriority;

        pAP->pFA->mfn_wvap_update_assoc_dev(pAP, assocDev);
        SAH_TRACEZ_INFO(ME, "update assocdev %s type %u prio %d", assocDev->Name, assocDev->deviceType, assocDev->devicePriority);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}



// This function will kick a STA with given MAC from the VAP interface.
amxd_status_t _kickStation(amxd_object_t* obj_AP,
                           amxd_function_t* func _UNUSED,
                           amxc_var_t* args,
                           amxc_var_t* ret _UNUSED) {

    const char* macStr = GET_CHAR(args, "macaddress");
    T_AccessPoint* pAP = NULL;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    pAP = obj_AP->priv;

    if(pAP && debugIsVapPointer(pAP)) {
        if(macStr) {
            pAP->pFA->mfn_wvap_kick_sta(pAP, (char*) macStr, strlen(macStr), SET);
        } else { /* Kick all */
            pAP->pFA->mfn_wvap_kick_sta(pAP, "ff:ff:ff:ff:ff:ff", 17, SET);
        }

        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_unknown_error;
}


// This function will kick a STA with given MAC from the VAP interface.
amxd_status_t _kickStationReason(amxd_object_t* obj_AP,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* ret _UNUSED) {

    const char* macStr = GET_CHAR(args, "macaddress");
    int reason = GET_INT32(args, "reason");

    T_AccessPoint* pAP = NULL;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    pAP = obj_AP->priv;

    if(pAP && debugIsVapPointer(pAP)) {
        if(macStr != NULL) {
            pAP->pFA->mfn_wvap_kick_sta_reason(pAP, (char*) macStr, strlen(macStr), reason);
        }
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_unknown_error;
}

// This function will cleanup a STA with given MAC from the VAP interface.
amxd_status_t _cleanStation(amxd_object_t* obj_AP,
                            amxd_function_t* func _UNUSED,
                            amxc_var_t* args,
                            amxc_var_t* ret _UNUSED) {

    ASSERT_NOT_NULL(obj_AP, amxd_status_unknown_error, ME, "NULL");
    T_AccessPoint* pAP = obj_AP->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "NULL");
    const char* macStr = GET_CHAR(args, "macaddress");

    SAH_TRACEZ_IN(ME);
    pAP->pFA->mfn_wvap_clean_sta(pAP, (char*) macStr, strlen(macStr));

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t __VerifyAccessPoint(amxd_object_t* object _UNUSED,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* ret _UNUSED) {
    return amxd_status_ok;
}

amxd_status_t __CommitAccessPoint(amxd_object_t* object _UNUSED,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* ret _UNUSED) {
    return amxd_status_ok;
}



swl_rc_ne wld_vap_sync_device(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {

    ASSERT_NOT_NULL(pAD, SWL_RC_ERROR, ME, "%s: AssociatedDevice==null", pAP->alias);

    if(pAD->object == NULL) {
        SAH_TRACEZ_INFO(ME, "Creating template object for mac %s", pAD->Name);
        amxd_object_t* templateObject = amxd_object_get(pAP->pBus, "AssociatedDevice");

        if(!templateObject) {
            SAH_TRACEZ_ERROR(ME, "%s: Could not get template: %p", pAP->alias, templateObject);
            return false;
        }

        amxd_status_t result = amxd_object_new_instance(&pAD->object, templateObject, NULL, 0, NULL);
        if((result != amxd_status_ok) || (pAD->object == NULL)) {
            SAH_TRACEZ_ERROR(ME, "%s: Could not create template object %u", pAP->alias, result);
            return false;
        }
        pAD->object->priv = (void*) pAD;
        amxd_object_set_cstring_t(pAD->object, "MACAddress", pAD->Name);
    }

    amxd_trans_t trans;
    amxd_object_t* object = pAD->object;
    ASSERT_TRANSACTION_INIT(object, &trans, false, ME, "%s : trans init failure", pAD->Name);

    amxd_trans_set_value(cstring_t, &trans, "ChargeableUserId", pAD->Radius_CUID);
    amxd_trans_set_value(bool, &trans, "AuthenticationState", pAD->AuthenticationState);
    amxd_trans_set_value(int32_t, &trans, "LastDataDownlinkRate", pAD->LastDataDownlinkRate);
    amxd_trans_set_value(int32_t, &trans, "LastDataUplinkRate", pAD->LastDataUplinkRate);
    amxd_trans_set_value(int32_t, &trans, "AvgSignalStrength", WLD_ACC_TO_VAL(pAD->rssiAccumulator));
    amxd_trans_set_value(int32_t, &trans, "SignalStrength", pAD->SignalStrength);

    char rssiHistory[64] = {'\0'};
    snprintf(rssiHistory, sizeof(rssiHistory), "%i,%i,%i,%i", pAD->minSignalStrength, pAD->maxSignalStrength, pAD->meanSignalStrength, WLD_ACC_TO_VAL(pAD->meanSignalStrengthExpAccumulator));
    amxd_trans_set_value(cstring_t, &trans, "SignalStrengthHistory", rssiHistory);

    int idx = 0;
    char TBuf[64] = {'\0'};
    char ValBuf[7] = {'\0'};//Example : -100.0
    for(idx = 0; idx < MAX_NR_ANTENNA && pAD->SignalStrengthByChain[idx] != DEFAULT_BASE_RSSI; idx++) {
        if(idx && TBuf[0]) {
            wldu_catStr(TBuf, ",", sizeof(TBuf));
        }
        snprintf(ValBuf, sizeof(ValBuf), "%.1f", pAD->SignalStrengthByChain[idx]);
        wldu_catStr(TBuf, ValBuf, sizeof(TBuf));
    }
    pAD->AvgSignalStrengthByChain = wld_ad_getAvgSignalStrengthByChain(pAD);
    amxd_trans_set_value(int32_t, &trans, "AvgSignalStrengthByChain", pAD->AvgSignalStrengthByChain);
    amxd_trans_set_value(cstring_t, &trans, "SignalStrengthByChain", TBuf);
    amxd_trans_set_value(int32_t, &trans, "SignalNoiseRatio", pAD->SignalNoiseRatio);
    amxd_trans_set_value(int32_t, &trans, "Retransmissions", pAD->Retransmissions);
    amxd_trans_set_value(int32_t, &trans, "Rx_Retransmissions", pAD->Rx_Retransmissions);
    amxd_trans_set_value(int32_t, &trans, "Tx_Retransmissions", pAD->Tx_Retransmissions);
    amxd_trans_set_value(int32_t, &trans, "Tx_RetransmissionsFailed", pAD->Tx_RetransmissionsFailed);
    amxd_trans_set_value(bool, &trans, "Active", pAD->Active);

    amxd_trans_set_value(int32_t, &trans, "Noise", pAD->noise);
    amxd_trans_set_value(uint32_t, &trans, "Inactive", pAD->Inactive);
    amxd_trans_set_value(uint32_t, &trans, "RxPacketCount", pAD->RxPacketCount);
    amxd_trans_set_value(uint32_t, &trans, "TxPacketCount", pAD->TxPacketCount);
    amxd_trans_set_value(uint32_t, &trans, "RxUnicastPacketCount", pAD->RxUnicastPacketCount);
    amxd_trans_set_value(uint32_t, &trans, "TxUnicastPacketCount", pAD->TxUnicastPacketCount);
    amxd_trans_set_value(uint32_t, &trans, "RxMulticastPacketCount", pAD->RxMulticastPacketCount);
    amxd_trans_set_value(uint32_t, &trans, "TxMulticastPacketCount", pAD->TxMulticastPacketCount);
    amxd_trans_set_value(uint64_t, &trans, "RxBytes", pAD->RxBytes);
    amxd_trans_set_value(uint64_t, &trans, "TxBytes", pAD->TxBytes);
    amxd_trans_set_value(uint32_t, &trans, "TxErrors", pAD->TxFailures);
    amxd_trans_set_value(uint32_t, &trans, "UplinkMCS", pAD->UplinkMCS);
    amxd_trans_set_value(uint32_t, &trans, "UplinkBandwidth", pAD->UplinkBandwidth);
    amxd_trans_set_value(uint32_t, &trans, "UplinkShortGuard", pAD->UplinkShortGuard);
    amxd_trans_set_value(uint32_t, &trans, "DownlinkMCS", pAD->DownlinkMCS);
    amxd_trans_set_value(uint32_t, &trans, "DownlinkBandwidth", pAD->DownlinkBandwidth);
    amxd_trans_set_value(uint32_t, &trans, "DownlinkShortGuard", pAD->DownlinkShortGuard);
    amxd_trans_set_value(uint16_t, &trans, "MaxRxSpatialStreamsSupported", pAD->MaxRxSpatialStreamsSupported);
    amxd_trans_set_value(uint16_t, &trans, "MaxTxSpatialStreamsSupported", pAD->MaxTxSpatialStreamsSupported);
    amxd_trans_set_value(uint32_t, &trans, "MaxDownlinkRateSupported", pAD->MaxDownlinkRateSupported);
    amxd_trans_set_value(uint32_t, &trans, "MaxDownlinkRateReached", pAD->MaxDownlinkRateReached);
    amxd_trans_set_value(uint32_t, &trans, "MaxUplinkRateSupported", pAD->MaxUplinkRateSupported);
    amxd_trans_set_value(uint32_t, &trans, "MaxUplinkRateReached", pAD->MaxUplinkRateReached);

    swl_bandwidth_e maxBw = pAD->MaxBandwidthSupported;
    if(maxBw == SWL_BW_AUTO) {
        maxBw = wld_util_getMaxBwCap(&pAD->assocCaps);
    }
    amxd_trans_set_value(cstring_t, &trans, "MaxBandwidthSupported", swl_bandwidth_unknown_str[maxBw]);

    amxd_trans_set_value(cstring_t, &trans, "DeviceType", cstr_DEVICE_TYPES[pAD->deviceType]);
    amxd_trans_set_value(int32_t, &trans, "DevicePriority", pAD->devicePriority);
    char buffer[32] = {0};
    wld_writeCapsToString(buffer, sizeof(buffer), swl_staCap_str, pAD->capabilities, SWL_STACAP_MAX);
    amxd_trans_set_value(cstring_t, &trans, "Capabilities", buffer);
    amxd_trans_set_value(uint32_t, &trans, "ConnectionDuration", pAD->connectionDuration);
    swl_time_objectParamSetReal(object, "LastStateChange", pAD->latestStateChangeTime);
    swl_time_objectParamSetMono(object, "AssociationTime", pAD->associationTime);
    swl_time_objectParamSetMono(object, "DisassociationTime", pAD->disassociationTime);
    amxd_trans_set_value(bool, &trans, "PowerSave", pAD->powerSave);
    amxd_trans_set_value(cstring_t, &trans, "Mode", wld_ad_getMode(pAP, pAD));
    amxd_trans_set_value(cstring_t, &trans, "OperatingStandard", swl_radStd_unknown_str[pAD->operatingStandard]);
    amxd_trans_set_value(uint32_t, &trans, "MUGroupId", pAD->staMuMimoInfo.muGroupId);
    amxd_trans_set_value(uint32_t, &trans, "MUUserPositionId", pAD->staMuMimoInfo.muUserPosId);
    amxd_trans_set_value(uint32_t, &trans, "MUMimoTxPktsCount", pAD->staMuMimoInfo.txAsMuPktsCnt);
    amxd_trans_set_value(uint32_t, &trans, "MUMimoTxPktsPercentage", pAD->staMuMimoInfo.txAsMuPktsPrc);

    swl_conv_objectParamSetMask(object, "UNIIBandsCapabilities", pAD->uniiBandsCapabilities, swl_uniiBand_str, SWL_BAND_MAX);

    wld_ad_syncCapabilities(object, &pAD->assocCaps);
    amxd_object_t* probeReqCapsObject = amxd_object_get(object, "ProbeReqCaps");
    wld_ad_syncCapabilities(probeReqCapsObject, &pAD->probeReqCaps);

    ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), false, ME, "%s : trans apply failure", pAD->Name);
    return SWL_RC_CONTINUE;
}

void wld_vap_syncNrDev(T_AccessPoint* pAP) {
    int active = 0;

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        if(pAP->AssociatedDevice[i]->Active) {
            active++;
        }
    }

    SAH_TRACEZ_INFO(ME, "%s: sync Assocdev %u %u", pAP->alias,
                    active, pAP->ActiveAssociatedDeviceNumberOfEntries);

    amxd_object_set_int32_t(pAP->pBus, "AssociatedDeviceNumberOfEntries", pAP->AssociatedDeviceNumberOfEntries);
    pAP->ActiveAssociatedDeviceNumberOfEntries = active;
    amxd_object_set_int32_t(pAP->pBus, "ActiveAssociatedDeviceNumberOfEntries", pAP->ActiveAssociatedDeviceNumberOfEntries);


    wld_rad_updateActiveDevices((T_Radio*) pAP->pRadio);
}

/* update the datamodel with the T_AccessPoint.AssociatedDevice[] */
bool wld_vap_sync_assoclist(T_AccessPoint* pAP) {

    SAH_TRACEZ_INFO(ME, "%s: sync, there are %d devices", pAP->alias, pAP->AssociatedDeviceNumberOfEntries);

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];

        swl_rc_ne ret = wld_vap_sync_device(pAP, pAD);
        if(ret < SWL_RC_OK) {
            return false;
        } else if(ret == SWL_RC_DONE) {
            i--;
        }
    }

    wld_vap_syncNrDev(pAP);

    SAH_TRACEZ_INFO(ME, "%s: done", pAP->alias);
    return true;
}

/* retrieve the AssociatedDevice with given MAC from T_AccessPoint.AssociatedDevice[] */
T_AssociatedDevice* wld_vap_get_existing_station(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    int AssociatedDeviceIdx;
    T_AssociatedDevice* EntryWithMatchingMac = NULL;
    T_AssociatedDevice* pAD;

    for(AssociatedDeviceIdx = 0; AssociatedDeviceIdx < pAP->AssociatedDeviceNumberOfEntries; AssociatedDeviceIdx++) {
        pAD = pAP->AssociatedDevice[AssociatedDeviceIdx];
        if(!pAD) {
            SAH_TRACEZ_ERROR(ME, "NULL pointer in AssociatedDevice[]!");
            SAH_TRACEZ_OUT(ME);
            return NULL;
        }
        if(!memcmp(macAddress->bMac, pAD->MACAddress, ETHER_ADDR_LEN)) {
            EntryWithMatchingMac = pAP->AssociatedDevice[AssociatedDeviceIdx];
            break;
        }
    }

    return EntryWithMatchingMac;
}


/**
 * free the T_AssociatedDevice and remove it from the list
 * @Deprecated
 *  please use wld_ad_destroy_associatedDevice instead
 **/
void wld_destroy_associatedDevice(T_AccessPoint* pAP, int index) {
    wld_ad_destroy_associatedDevice(pAP, index);
}

/**
 * create T_AssociatedDevice and populate MACAddress and Name fields
 * @Deprecated
 *  please use wld_ad_destroy_associatedDevice instead
 **/
T_AssociatedDevice* wld_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    return wld_ad_create_associatedDevice(pAP, macAddress);
}

/* clean up stations who failed authentication */
bool wld_vap_cleanup_stationlist(T_AccessPoint* pAP) {
    T_AssociatedDevice* oldestInactive = NULL;
    uint32_t inactiveCount = 0;
    SAH_TRACEZ_INFO(ME, "%s: clean", pAP->alias);

    // Find ad that was dc's longest ago
    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
        ASSERT_NOT_NULL(pAD, false, ME, "%s: AssociatedDevice[%d]==0!", pAP->alias, i);

        if(pAD->Active) {
            continue;
        }
        inactiveCount++;
        if((oldestInactive == NULL)
           || (oldestInactive->disassociationTime > pAD->disassociationTime)) {
            oldestInactive = pAD;
        }
    }

    if(inactiveCount > (NR_OF_STICKY_UNAUTHORIZED_STATIONS)) {
        SAH_TRACEZ_INFO(ME, " * Marking oldest failed-auth entry (%s) out of %d obsolete",
                        oldestInactive->Name,
                        inactiveCount);
        wld_ad_destroy(pAP, oldestInactive);
    }

    SAH_TRACEZ_INFO(ME, "%s: done clean", pAP->alias);
    return true;
}

bool wld_vap_assoc_update_cuid(T_AccessPoint* pAP, swl_macBin_t* mac, char* cuid, int len _UNUSED) {

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, mac);
    ASSERT_NOT_NULL(pAD, false, ME, "NULL");

    memset(pAD->Radius_CUID, 0, sizeof(pAD->Radius_CUID));
    memcpy(pAD->Radius_CUID, cuid, sizeof(pAD->Radius_CUID));
    wld_vap_cleanup_stationlist(pAP);
    wld_vap_sync_assoclist(pAP);

    return true;
}


bool wld_ap_ModesSupported_mask_to_string(amxc_string_t* output, wld_securityMode_m secModesSupported) {
    return bitmask_to_string(output, cstr_AP_ModesSupported, ',', secModesSupported);
}

void wld_ap_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress) {
    wld_wps_sendPairingNotification(pAP->pBus, type, reason, macAddress, NULL);
    amxd_object_t* wps = amxd_object_get(pAP->pBus, "WPS");
    pAP->WPS_PairingInProgress = amxd_object_get_bool(wps, "PairingInProgress", NULL);
}

T_AccessPoint* wld_ap_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");
    return amxc_llist_it_get_data(it, T_AccessPoint, it);
}

/****************
 * DEBUG SUPPORT
 ***************/

amxd_status_t _dbgClearInactiveEntries(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args,
                                       amxc_var_t* ret _UNUSED) {

    amxd_object_t* obj;
    T_AccessPoint* pAP;
    _UNUSED_(args);


    SAH_TRACEZ_IN(ME);
    obj = object;
    pAP = obj->priv;

    int AssociatedDeviceIdx;
    T_AssociatedDevice* dev;
    for(AssociatedDeviceIdx = 0; AssociatedDeviceIdx < pAP->AssociatedDeviceNumberOfEntries; AssociatedDeviceIdx++) {
        dev = pAP->AssociatedDevice[AssociatedDeviceIdx];
        if((dev != NULL) && !dev->Active) {
            wld_ad_destroy(pAP, dev);
        }
    }
    wld_vap_sync_assoclist(pAP);


    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

T_AccessPoint* wld_vap_get_vap(const char* ifname) {
    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &g_radios) {
        T_Radio* pRad = amxc_llist_it_get_data(llit, T_Radio, it);
        amxc_llist_it_t* it;
        for(it = amxc_llist_get_first(&pRad->llAP); it; it = amxc_llist_it_get_next(it)) {
            T_AccessPoint* pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
            if(pAP && !strcmp(pAP->alias, ifname)) {
                return pAP;
            }
        }
    }

    return NULL;
}

T_AccessPoint* wld_ap_getVapByName(const char* name) {
    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &g_radios) {
        T_Radio* pRad = amxc_llist_it_get_data(llit, T_Radio, it);
        amxc_llist_it_t* it;
        for(it = amxc_llist_get_first(&pRad->llAP); it; it = amxc_llist_it_get_next(it)) {
            T_AccessPoint* pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
            if(pAP && swl_str_matches(pAP->name, name)) {
                return pAP;
            }
        }
    }

    return NULL;
}


void wld_vap_updateState(T_AccessPoint* pAP) {


    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;

    wld_intfStatus_e oldVapStatus = pAP->status;
    wld_status_e oldSsidStatus = pSSID->status;

    int status = pAP->pFA->mfn_wvap_status(pAP);
    if(status > 0) {
        pAP->status = APSTI_ENABLED;
    } else {
        pAP->status = APSTI_DISABLED;
    }
    if(pRad->status == RST_ERROR) {
        pSSID->status = RST_ERROR;
    } else if((pAP->status == APSTI_DISABLED) || (pRad->status == RST_DOWN)) {
        pSSID->status = RST_DOWN;
    } else if(pRad->status == RST_DORMANT) {
        pSSID->status = RST_LLDOWN;
    } else {
        pSSID->status = RST_UP;
    }

    SAH_TRACEZ_INFO(ME, "%s: ap %u -> %u / ssid %u -> %u", pAP->alias, oldVapStatus, pAP->status, oldSsidStatus, pSSID->status);

    if((oldVapStatus == pAP->status) && (oldSsidStatus == pSSID->status)) {
        return;
    }
    pAP->lastStatusChange = swl_time_getMonoSec();

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pAP->pBus, &trans, , ME, "%s : trans init failure", pAP->alias);
    amxd_trans_set_value(cstring_t, &trans, "Status", cstr_AP_status[pAP->status]);
    ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), , ME, "%s : trans apply failure", pAP->alias);

    ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->SSID);
    amxd_trans_set_value(cstring_t, &trans, "Status", Rad_SupStatus[pSSID->status]);
    ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), , ME, "%s : trans apply failure", pSSID->SSID);

    wld_vap_status_change_event_t vapUpdate;
    vapUpdate.vap = pAP;
    vapUpdate.oldSsidStatus = oldSsidStatus;
    vapUpdate.oldVapStatus = oldVapStatus;
    wld_event_trigger_callback(gWld_queue_vap_onStatusChange, &vapUpdate);


    wld_apRssiMon_updateEnable(pAP);
}

/**
 * Return whether the AP must be up (true) or down (false) regarding radio state:
 * - Check whether AP is enabled
 * - Check whether radio is up
 */
bool wld_ap_getDesiredState(T_AccessPoint* pAp) {
    ASSERT_NOT_NULL(pAp, false, ME, "Null AP");
    T_Radio* pR = pAp->pRadio;
    ASSERT_NOT_NULL(pR, false, ME, "Null radio");
    if(!pAp->enable) {
        return false;
    }
    if(!wld_rad_isUpExt(pR)) {
        return false;
    }
    return true;
}

amxd_status_t _wld_ap_setDriverConfig_owf(amxd_object_t* object) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* vapObject = amxd_object_get_parent(object);
    ASSERTS_FALSE(amxd_object_get_type(vapObject) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    T_AccessPoint* pAP = (T_AccessPoint*) vapObject->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s update driver config", pAP->alias);
    wld_vap_driverCfgChange_m params = 0;
    int32_t tmp_int32 = amxd_object_get_int32_t(object, "BssMaxIdlePeriod", NULL);
    if(pAP->driverCfg.bssMaxIdlePeriod != tmp_int32) {
        SAH_TRACEZ_INFO(ME, "Update BssMaxIdlePeriod");
        pAP->driverCfg.bssMaxIdlePeriod = tmp_int32;
        params |= M_WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD;
    }
    pAP->pFA->mfn_wvap_set_config_driver(pAP, params);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * @brief return the last assoc/reassoc frame sent by a station to a vap
 * @param pAP the accesspoint receiving the assoc/reassoc frame
 * @param macStation the station mac address
 * @param data contains the last assoc/reassoc tuple associated to macStation if found
 * @return - SWL_RC_OK: if the last assoc/reassoc frame is found for the given station
 *         - SWL_RC_INVALID_PARAM if pAP = NULL or macStation = NULL or data = NULL
 *         - SWL_RC_ERROR if no assoc/reassoc tuple associated to macStation is found
 */

swl_rc_ne wld_ap_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, wld_vap_assocTableStruct_t** data) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(macStation, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macBin_t bMac;
    SWL_MAC_CHAR_TO_BIN(&bMac, macStation);

    *data = swl_circTable_getMatchingTuple(&(pAP->lastAssocReq), 0, &bMac);
    ASSERT_NOT_NULL(*data, SWL_RC_ERROR, ME, "NULL");
    return SWL_RC_OK;
}

/**
 * @brief return the last assoc/reassoc frame sent by a station to a vap
 * @param pAP the accesspoint receiving the assoc/reassoc frame
 * @param mac the station mac address
 * @return - a variant map containing the sent assoc/reassoc frame by a station
 *         - Otherwise
 *              - amxd_status_parameter_not_found if mac is missing
 *              - amxd_status_unknown_error if no assoc/reassoc frame associated to mac is found
 */
amxd_status_t _AccessPoint_getLastAssocReq(amxd_object_t* object,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_ok;
    T_AccessPoint* pAP = object->priv;

    const char* macStation = GET_CHAR(args, "mac");
    ASSERT_NOT_NULL(macStation, amxd_status_parameter_not_found, ME, "No mac station given");

    wld_vap_assocTableStruct_t* tuple = NULL;
    swl_rc_ne ret = wld_ap_getLastAssocReq(pAP, macStation, &tuple);
    ASSERT_TRUE(swl_rc_isOk(ret), amxd_status_unknown_error, ME, "Error during execution");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    char* names[5] = {"mac", "bssid", "frame", "timestamp", "request_type"};
    for(size_t i = 0; i < assocTable.nrTypes; i++) {
        swl_type_t* type = assocTable.types[i];
        swl_typeData_t* tmpValue = swl_tupleType_getValue(&assocTable, tuple, i);
        amxc_var_t* tmpVar = amxc_var_add_new_key(retval, names[i]);
        swl_type_toVariant(type, tmpVar, tmpValue);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _AccessPoint_debug(amxd_object_t* obj,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* retMap) {
    T_AccessPoint* pAP = obj->priv;

    const char* feature = GET_CHAR(args, "op");
    ASSERT_NOT_NULL(feature, amxd_status_unknown_error, ME, "No argument given");

    amxc_var_init(retMap);
    amxc_var_set_type(retMap, AMXC_VAR_ID_HTABLE);

    if(!strcasecmp(feature, "RssiMon")) {
        wld_ap_rssiEv_debug(pAP, retMap);
    } else if(!strcasecmp(feature, "recentDcs")) {
        wld_assocDev_listRecentDisconnects(pAP, retMap);
    } else if(!strcasecmp(feature, "SSIDAdvertisementEnabled")) {
        bool enable = GET_BOOL(args, "enable");
        wld_ap_hostapd_setSSIDAdvertisement(pAP, enable);
    } else if(swl_str_matches(feature, "setSsid")) {
        const char* ssid = GET_CHAR(args, "ssid");
        bool ret = wld_ap_hostapd_setSsid(pAP, ssid);
    } else if(!strcasecmp(feature, "setKey")) {
        const char* key = GET_CHAR(args, "key");
        const char* value = GET_CHAR(args, "value");
        if(!strcasecmp(key, "wep_key")) {
            wldu_copyStr(pAP->WEPKey, value, sizeof(pAP->WEPKey));
        } else if(!strcasecmp(key, "wpa_psk")) {
            wldu_copyStr(pAP->preSharedKey, value, sizeof(pAP->preSharedKey));
        } else if(!strcasecmp(key, "wpa_passphrase")) {
            wldu_copyStr(pAP->keyPassPhrase, value, sizeof(pAP->keyPassPhrase));
        } else if(!strcasecmp(key, "sae_password")) {
            wldu_copyStr(pAP->saePassphrase, value, sizeof(pAP->saePassphrase));
        }
        wld_ap_hostapd_setSecretKey(pAP);
    } else if(!strcasecmp(feature, "nl80211IfaceInfo")) {
        wld_nl80211_ifaceInfo_t ifaceInfo;
        if(wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo) < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retMap, "Error", "Fail to get nl80211 iface info");
        } else {
            wld_nl80211_dumpIfaceInfo(&ifaceInfo, retMap);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211StationInfo")) {
        swl_rc_ne rc;
        const char* sta = GET_CHAR(args, "sta");
        if(sta && sta[0]) {
            wld_nl80211_stationInfo_t stationInfo;
            swl_macBin_t bMac;
            if(!swl_mac_charToBin(&bMac, (swl_macChar_t*) sta)) {
                rc = SWL_RC_INVALID_PARAM;
            } else if((rc = wld_ap_nl80211_getStationInfo(pAP, &bMac, &stationInfo)) >= SWL_RC_OK) {
                wld_nl80211_dumpStationInfo(&stationInfo, retMap);
            }
        } else {
            wld_nl80211_stationInfo_t* allStationInfo = NULL;
            uint32_t nStations = 0;
            if((rc = wld_ap_nl80211_getAllStationsInfo(pAP, &allStationInfo, &nStations)) >= SWL_RC_OK) {
                wld_nl80211_dumpAllStationInfo(allStationInfo, nStations, retMap);
            }
            free(allStationInfo);
        }
        if(rc < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retMap, "Error", swl_rc_toString(rc));
        }
    } else if(!strcasecmp(feature, "kickSta")) {
        const char* sta = GET_CHAR(args, "sta");
        uint32_t reason = GET_UINT32(args, "reason");
        if(reason == 0) {
            reason = SWL_IEEE80211_DEAUTH_REASON_UNSPECIFIED;
        }
        swl_macChar_t cMac;
        swl_macBin_t bMac;

        SWL_MAC_CHAR_TO_BIN(&bMac, sta);

        wld_ap_hostapd_kickStation(pAP, &bMac, (swl_IEEE80211deauthReason_ne) reason);
    } else if(swl_str_matchesIgnoreCase(feature, "clientIsolation")) {
        int ap_isolate = GET_INT32(args, "ap_isolate");
        pAP->clientIsolationEnable = ap_isolate;
        wld_secDmn_action_rc_ne ret = wld_ap_hostapd_setClientIsolation(pAP);
        wld_ap_hostapd_updateBeacon(pAP, "ap_isolate");
        if(ret == SECDMN_ACTION_ERROR) {
            amxc_var_add_key(cstring_t, retMap, "Error", "WLD_KO");
        } else {
            amxc_var_add_key(cstring_t, retMap, "OK", "WLD_OK");
        }
    } else if(swl_str_matchesIgnoreCase(feature, "clearMacAcl")) {
        swl_rc_ne ret = wld_ap_hostapd_delAllMacFilteringEntries(pAP);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "setMacAcl")) {
        swl_rc_ne ret = wld_ap_hostapd_setMacFilteringList(pAP);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "addMacEntry")) {
        const char* macStr = GET_CHAR(args, "macStr");
        swl_rc_ne ret = wld_ap_hostapd_addMacFilteringEntry(pAP, (char*) macStr);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "delMacEntry")) {
        const char* macStr = GET_CHAR(args, "macStr");
        swl_rc_ne ret = wld_ap_hostapd_delMacFilteringEntry(pAP, (char*) macStr);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else {
        amxc_var_add_key(cstring_t, retMap, "Error", "Unknown command");
    }

    return amxd_status_ok;
}
