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
#include "swl/swl_hex.h"
#include "swl/swl_genericFrameParser.h"
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
static const char* s_assocTableNames[] = {"mac", "bssid", "frame", "timestamp", "request_type"};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(s_assocTableNames) == SWL_ARRAY_SIZE(assocTableTypes), "s_assocTableNames not correctly defined");

T_AccessPoint* wld_ap_fromObj(amxd_object_t* apObj) {
    ASSERTS_EQUALS(amxd_object_get_type(apObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(apObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "AccessPoint"), NULL, ME, "invalid parent obj(%s)", parentName);
    T_AccessPoint* pAP = (T_AccessPoint*) apObj->priv;
    ASSERTS_TRUE(pAP, NULL, ME, "NULL");
    ASSERT_TRUE(debugIsVapPointer(pAP), NULL, ME, "INVALID");
    return pAP;
}

static void s_setDefaults(T_AccessPoint* pAP, T_Radio* pRad, const char* vapName, uint32_t idx) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pAP->debug = VAP_POINTER;
    pAP->index = 0;
    pAP->enable = 0;
    pAP->ref_index = idx;

    pAP->pRadio = pRad;
    pAP->pFA = pRad->pFA;
    swl_str_copy(pAP->name, sizeof(pAP->name), vapName);
    swl_str_copy(pAP->alias, sizeof(pAP->alias), vapName);
    pAP->fsm.FSM_SyncAll = TRUE;

    swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), "password");
    swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), "123456789ABCDEF0123456789A");
    sprintf(pAP->preSharedKey, "password_%d", idx);
    sprintf(pAP->radiusSecret, "RadiusPassword_%d", idx);
    swl_str_copy(pAP->radiusServerIPAddr, sizeof(pAP->radiusServerIPAddr), "127.0.0.1");
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
    pAP->secModesSupported = M_SWL_SECURITY_APMODE_NONE;
    pAP->MaxStations = -1;

    pAP->UAPSDCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "UAPSD", 0);
    pAP->WMMCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WME", 0);
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WEP", 0)) ?
        (M_SWL_SECURITY_APMODE_WEP64 | M_SWL_SECURITY_APMODE_WEP128 | M_SWL_SECURITY_APMODE_WEP128IV) : 0;
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "AES", 0)) ?
        (M_SWL_SECURITY_APMODE_WPA_P | M_SWL_SECURITY_APMODE_WPA2_P | M_SWL_SECURITY_APMODE_WPA_WPA2_P |
         M_SWL_SECURITY_APMODE_WPA_E | M_SWL_SECURITY_APMODE_WPA2_E | M_SWL_SECURITY_APMODE_WPA_WPA2_E) : 0;
    if(pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "SAE", 0)) {
        pAP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA3_P;
        if(pAP->secModesSupported & M_SWL_SECURITY_APMODE_WPA2_P) {
            pAP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA2_WPA3_P;
        }
    }
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "OWE", 0)) ?
        (M_SWL_SECURITY_APMODE_OWE) : 0;
    if(!pAP->MCEnable) {
        /* Init this interface... */
        T_Radio* pR = (T_Radio*) pAP->pRadio;
        pAP->MCEnable = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE;
        pAP->doth = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE; //marianna: hardcoded false on 2.4GHz and true on 5GHz, not on WiFiATH
    }

    if(pAP->pRadio->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
        pAP->secModeEnabled = SWL_SECURITY_APMODE_WPA3_P;
    } else {
        pAP->secModeEnabled = SWL_SECURITY_APMODE_WPA2_P;
    }
    pAP->secModesAvailable = pAP->secModesSupported;
    pAP->mfpConfig = SWL_SECURITY_MFPMODE_DISABLED;
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
}

/**
 * @brief s_deinitAP
 *
 * Clear out resources of the accesspoint internal context
 *
 * @param accesspoint context
 */
static void s_deinitAP(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    SAH_TRACEZ_IN(ME);
    //assume disable ap
    pAP->enable = false;
    T_Radio* pR = pAP->pRadio;
    T_SSID* pSSID = pAP->pSSID;
    if(pR) {
        if((pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress))) {
            if(pAP->ActiveAssociatedDeviceNumberOfEntries > 0) {
                /* Deauthentify all stations */
                SAH_TRACEZ_INFO(ME, "%s: Deauth all stations", pAP->alias);
                pAP->pFA->mfn_wvap_kick_sta_reason(pAP, "ff:ff:ff:ff:ff:ff", 17, SWL_IEEE80211_DEAUTH_REASON_UNABLE_TO_HANDLE_STA);
            }
            /* Destroy vap*/
            pR->pFA->mfn_wvap_destroy_hook(pAP);
            /* Try to delete the requested interface by calling the HW function */
            pR->pFA->mfn_wrad_delvapif(pR, pAP->alias);
        }
        pAP->alias[0] = 0;
        pAP->index = 0;
        /* Take EP also out the Radio */
        amxc_llist_it_take(&pAP->it);
        pAP->pRadio = NULL;
        pAP->pFA = NULL;
    }
    for(int i = pAP->AssociatedDeviceNumberOfEntries - 1; i >= 0; i--) {
        wld_ad_destroy_associatedDevice(pAP, i);
    }
    pAP->ActiveAssociatedDeviceNumberOfEntries = 0;
    wld_assocDev_cleanAp(pAP);
    wld_ap_rssiMonDestroy(pAP);

    swl_circTable_destroy(&(pAP->lastAssocReq));

    free(pAP->dbgOutput);
    pAP->dbgOutput = NULL;

    if(pSSID != NULL) {
        memset(pSSID->MACAddress, 0, ETHER_ADDR_LEN);
        pSSID->AP_HOOK = NULL;
        if(pR) {
            pR->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        }
        pAP->pSSID = NULL;
    }
    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief s_initAp
 *
 * reserve and initialize resources for accesspoint internal context
 *
 * @param accesspoint context
 */
static bool s_initAp(T_AccessPoint* pAP, T_Radio* pRad, const char* vapName) {
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_STR(vapName, false, ME, "No vap name");
    s_setDefaults(pAP, pRad, vapName, amxc_llist_size(&pRad->llAP));
    /* Add pAP on linked list of pR */
    amxc_llist_append(&pRad->llAP, &pAP->it);

    wld_ap_rssiMonInit(pAP);
    wld_assocDev_initAp(pAP);

    swl_circTable_init(&(pAP->lastAssocReq), &assocTable, 20);
    return true;
}

static amxd_status_t _linkApSsid(amxd_object_t* object, amxd_object_t* pSsidObj) {
    ASSERT_NOT_NULL(object, amxd_status_unknown_error, ME, "NULL");
    T_SSID* pSSID = NULL;
    if(pSsidObj) {
        pSSID = (T_SSID*) pSsidObj->priv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    if(pAP != NULL) {
        ASSERTI_NOT_EQUALS(pSSID, pAP->pSSID, amxd_status_ok, ME, "same ssid reference");
        s_deinitAP(pAP);
    }
    const char* vapName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    ASSERT_NOT_NULL(vapName, amxd_status_unknown_error, ME, "NULL");
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio Ctx");
    SAH_TRACEZ_INFO(ME, "pSSID(%p) pRad(%p)", pSSID, pRad);
    if(pAP != NULL) {
        bool ret = s_initAp(pAP, pRad, vapName);
        ASSERT_EQUALS(ret, true, amxd_status_unknown_error, ME, "%s: fail to re-create accesspoint", vapName);
    } else {
        pAP = wld_ap_create(pRad, vapName, object);
        ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "%s: fail to create accesspoint", vapName);
    }

    SAH_TRACEZ_INFO(ME, "%s: add vap %s", pRad->Name, vapName);
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;

    return amxd_status_ok;
}

static void s_syncApSSIDDm(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "No mapped AP Ctx");
    ASSERTS_NOT_NULL(pAP->pRadio, , ME, "No mapped Rad Ctx");

    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, , ME, "No mapped SSID Ctx");
    pAP->pFA->mfn_sync_ap(pAP->pBus, pAP, SET);
    pAP->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);

    SAH_TRACEZ_OUT(ME);

}

/*
 * finalize AP interface creation and MAC/BSSID reading
 */
static bool s_finalizeApCreation(T_AccessPoint* pAP) {
    SAH_TRACEZ_IN(ME);

    ASSERTS_NOT_NULL(pAP, false, ME, "No mapped AP Ctx");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, false, ME, "%s: No mapped SSID Ctx", pAP->alias);
    ASSERT_NOT_NULL(pAP->pRadio, false, ME, "No lower radio");

    swl_rc_ne rc = pAP->pRadio->pFA->mfn_wvap_create_hook(pAP);
    ASSERT_FALSE(rc < SWL_RC_OK, false, ME, "%s: VAP create hook failed (rc:%d)", pAP->alias, rc);

    if(!pAP->index) {
        SAH_TRACEZ_INFO(ME, "%s: initialize ap interface", pAP->alias);
        wld_ap_initializeVendor(pAP->pRadio, pAP, pSSID);
    }
    //Finalize SSID mapping
    pAP->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, GET);

    //delay sync AP and SSID Dm after all conf has been loaded
    swla_delayExec_add((swla_delayExecFun_cbf) s_syncApSSIDDm, pAP);

    SAH_TRACEZ_OUT(ME);
    return true;
}

amxd_status_t _wld_ap_delInstance_odf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: destroy instance object(%p)", name, object);
    wld_ap_destroy(object->priv);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/*
 * AP instance addition action handler
 * early handling to early create map with internal ssid ctx
 * Needed for smooth dm conf loading as ssid may require knowing relative AP/EP
 */
amxd_status_t _wld_ap_addInstance_oaf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s)", object, name);
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERTI_NOT_EQUALS(status, amxd_status_duplicate, status, ME, "override instance (%p:%s)", object, name);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance (%p:%s)", object, name);
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    amxd_object_t* pSsidObj = amxd_object_findf(get_wld_object(), "SSID.%s", amxd_object_get_name(instance, AMXD_OBJECT_NAMED));
    status = _linkApSsid(instance, pSsidObj);
    return status;
}

/*
 * AP instance addition event handler
 * late handling only to finalize AP interface creation (using twin ssid instance)
 * when SSIDReference is not explicitely provided by user conf
 */
static void s_addApInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const initialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    if(GET_ARG(initialParamValues, "SSIDReference") == NULL) {
        /*
         * As user conf does not include specific SSIDReference
         * then use the default twin ssid mapping already initiated in the early action handler
         */
        s_finalizeApCreation(object->priv);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setSSIDRef_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    ASSERTI_EQUALS(amxd_object_get_type(object), amxd_object_instance, , ME, "Not instance");
    const char* ssidRef = amxc_var_constcast(cstring_t, newValue);
    const char* oname = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: ssidRef(%s)", oname, ssidRef);

    amxd_object_t* pSsidObj = swla_object_getReferenceObject(object, ssidRef);
    ASSERT_EQUALS(_linkApSsid(object, pSsidObj), amxd_status_ok, , ME, "%s: fail to link Ap to SSID (%s)", oname, ssidRef);

    s_finalizeApCreation(object->priv);

    SAH_TRACEZ_OUT(ME);
}

int wld_ap_initializeVendor(T_Radio* pR, T_AccessPoint* pAP, T_SSID* pSSID) {
    ASSERT_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    int ret = pR->pFA->mfn_wrad_addVapExt(pR, pAP);

    if(ret == SWL_RC_NOT_IMPLEMENTED) {
        char vapnamebuf[32] = {0};
        swl_str_copy(vapnamebuf, sizeof(vapnamebuf), pAP->alias);
        /* Try to create the requested interface by calling the HW function */
        ret = pR->pFA->mfn_wrad_addvapif(pR, vapnamebuf, sizeof(vapnamebuf));
        if(ret >= 0) {
            pAP->index = ret;
            snprintf(pAP->alias, sizeof(pAP->alias), "%s", vapnamebuf);
        }
    }
    ASSERTS_FALSE(ret < SWL_RC_OK, ret, ME, "fail to add vap iface");

    // Plugin has create the BSSID, be sure we update/select that one.
    pAP->pFA->mfn_wvap_bssid(NULL, pAP, NULL, 0, GET);
    memcpy(pSSID->MACAddress, pSSID->BSSID, sizeof(pSSID->MACAddress));

    return SWL_RC_OK;
}

void wld_ap_doSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_sync(pAP, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

void wld_ap_doWpsSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_wps_enable(pAP, pAP->WPS_Enable, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

void s_saveMaxStations(T_AccessPoint* pAP) {
    ASSERT_TRUE(swl_typeUInt32_commitObjectParam(pAP->pBus, "MaxAssociatedDevices", pAP->MaxStations), ,
                ME, "%s: fail to commit maxAssocDevices (%d)", pAP->alias, pAP->MaxStations);
}

static void s_setMaxStations_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    int flag = amxc_var_dyncast(int32_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set MaxStations %d", pAP->alias, flag);
    ASSERTS_NOT_EQUALS(pAP->MaxStations, flag, , ME, "same value");
    pAP->MaxStations = (flag > MAXNROF_STAENTRY || flag < 0) ? MAXNROF_STAENTRY : flag;
    wld_ap_doSync(pAP);
    if(pAP->MaxStations != flag) {
        swla_delayExec_add((swla_delayExecFun_cbf) s_saveMaxStations, pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

/**
 * The function is used to map an interface on a bridge. Some vendor deamons
 * depends on it for proper functionality.
 */
static void s_setBridgeInterface_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* bridgeName = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(bridgeName, , ME, "NULL");
    ASSERTI_FALSE(swl_str_matches(bridgeName, pAP->bridgeName), , ME, "%s: same bridgeName %s", pAP->alias, bridgeName);
    SAH_TRACEZ_INFO(ME, "%s: set BridgeInterface %s", pAP->alias, bridgeName);
    swl_str_copy(pAP->bridgeName, sizeof(pAP->bridgeName), bridgeName);
    pAP->pFA->mfn_on_bridge_state_change(pAP->pRadio, pAP, SET);

    SAH_TRACEZ_OUT(ME);
}

/**
 * The function sets the default device type of new devices connected to the requested AP.
 * The value of the defaultDeviceType will only affect future devices connecting to the AP.
 * It will NOT be retroactively set to all connected devices.
 */
static void s_setDefaultDeviceType_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* deviceTypeStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(deviceTypeStr, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: set DefaultDeviceType %s", pAP->alias, deviceTypeStr);
    swl_enum_e newDeviceType = swl_conv_charToEnum(deviceTypeStr, cstr_DEVICE_TYPES, DEVICE_TYPE_MAX, DEVICE_TYPE_DATA);
    ASSERTI_NOT_EQUALS(pAP->defaultDeviceType, (int) newDeviceType, , ME, "%s: same bridgeName %s", pAP->alias, deviceTypeStr);
    SAH_TRACEZ_INFO(ME, "%s: Updating device type from %u to %u", pAP->alias, pAP->defaultDeviceType, newDeviceType);
    pAP->defaultDeviceType = newDeviceType;

    SAH_TRACEZ_OUT(ME);
}

/**
 * Enables debugging on third party software modules.
 * The goal of this is only for developpers!
 * We need a way to have/enable a better view how things are managed by some deamons.
 */
static void s_setDbgEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool dbgAPEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setdbgAPEnable %d", pAP->alias, dbgAPEnable);

    SAH_TRACEZ_OUT(ME);
}

static void s_setDbgFile_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* dbgFile = amxc_var_constcast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setdbgAPFile %s", pAP->alias, dbgFile);
    swl_str_copyMalloc(&pAP->dbgOutput, dbgFile);

    SAH_TRACEZ_OUT(ME);
}

static void s_set80211kEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setAp80211kEnable %d", pAP->alias, enable);
    pAP->IEEE80211kEnable = enable;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_validateIEEE80211rEnabled_pvf(amxd_object_t* object _UNUSED,
                                                    amxd_param_t* param _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "No AP mapped");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio mapped");
    bool flag = amxc_var_dyncast(bool, args);
    if((!flag) || (pRad->IEEE80211rSupported)) {
        return amxd_status_ok;
    }
    SAH_TRACEZ_ERROR(ME, "%s: invalid IEEE80211rEnabled %d", pRad->Name, flag);
    return amxd_status_invalid_value;
}

static void s_set80211rEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enabled = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set 80211rEnable %d", pAP->alias, enabled);
    pAP->IEEE80211rEnable = enabled;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setFTOverDSEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool new_value = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set FTOverDSEnable %d", pAP->alias, new_value);
    pAP->IEEE80211rFTOverDSEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMobilityDomain_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    uint16_t mobilityDomain = amxc_var_dyncast(uint16_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setMobilityDomain %d", pAP->alias, mobilityDomain);

    pAP->mobilityDomain = mobilityDomain;
    wld_ap_sec_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sAp11rDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("FTOverDSEnable", s_setFTOverDSEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MobilityDomain", s_setMobilityDomain_pwf),
                  SWLA_DM_PARAM_HDLR("Enabled", s_set80211rEnabled_pwf),
                  ));

void _wld_ap_11r_setConf_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAp11rDmHdlrs, sig_name, data, priv);
}

static void s_setInterworkingEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enabled = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set InterworkingEnable %d", pAP->alias, enabled);
    pAP->cfg11u.interworkingEnable = enabled;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setQosMapSet_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    char* qosMapStr = amxc_var_dyncast(cstring_t, newValue);
    ASSERT_NOT_NULL(qosMapStr, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: set QoS Map %s", pAP->alias, qosMapStr);
    if(!swl_str_matches(pAP->cfg11u.qosMapSet, qosMapStr)) {
        swl_str_copy(pAP->cfg11u.qosMapSet, QOS_MAP_SET_MAX_LEN, qosMapStr);
        wld_ap_doSync(pAP);
    }
    free(qosMapStr);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sAp11uDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("QoSMapSet", s_setQosMapSet_pwf),
                  SWLA_DM_PARAM_HDLR("InterworkingEnable", s_setInterworkingEnable_pwf),
                  ));

void _wld_ap_11u_setConf_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAp11uDmHdlrs, sig_name, data, priv);
}

static void s_setSsidAdvEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    T_SSID* pSSID = pAP->pSSID;
    /* SSIDAdvertisementEnabled - Needs also a resync of SSID */
    bool newEnable = amxc_var_dyncast(bool, newValue);
    pAP->SSIDAdvertisementEnabled = newEnable;
    if(debugIsSsidPointer(pSSID)) {
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) pSSID->SSID, strlen(pSSID->SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setApCommonParam_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    SAH_TRACEZ_INFO(ME, "%s: set Param %s", pAP->alias, amxd_param_get_name(param));
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
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
                           (swl_security_isApModeValid(pAP->secModeEnabled)) &&
                           (pAP->secModeEnabled == SWL_SECURITY_APMODE_NONE || (!swl_security_isApModeWEP(pAP->secModeEnabled))) &&
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
        if(pAP->pSSID != NULL) {
            char* currSsidRef = amxd_object_get_cstring_t(pAP->pBus, "SSIDReference", NULL);
            wld_util_getRealReferencePath(TBuf, sizeof(TBuf), currSsidRef, pAP->pSSID->pBus);
            free(currSsidRef);
        }
        amxd_object_set_cstring_t(object, "SSIDReference", TBuf);
        TBuf[0] = 0;
        if(pAP->pRadio) {
            char* path = amxd_object_get_path(pAP->pRadio->pBus, AMXD_OBJECT_NAMED);
            swl_str_copy(TBuf, sizeof(TBuf), path);
            free(path);
        }
        amxd_object_set_cstring_t(object, "RadioReference", TBuf);
        /** 'RetryLimit' The maximum number of retransmission for a
         *  packet. This corresponds to IEEE 802.11 parameter
         *  do11ShortRetryLimit [0..127] */
        amxd_object_set_int32_t(object, "RetryLimit", pAP->retryLimit);
        /** 'WMMCapability' Indicates whether this access point supports
         *  WiFi Multimedia(WMM) Access Cagegories (AC). */
        amxd_object_set_bool(object, "WMMCapability", pAP->WMMCapability);
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

        swl_conv_objectParamSetMask(object, "MultiAPType", pAP->multiAPType, cstr_MultiAPType, MULTIAP_MAX);

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
        swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pAP->secModesSupported);

        amxd_object_set_cstring_t(secObj, "ModesSupported", TBuf);

        swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pAP->secModesAvailable);

        amxd_object_set_cstring_t(secObj, "ModesAvailable", TBuf);

        /** 'ModeEnabled' The value MUST be a member of the list
         *  reported by the ModesSupported parameter. Indicates which
         *  security mode is enabled. */
        amxd_object_set_cstring_t(secObj, "ModeEnabled", swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
        amxd_object_set_cstring_t(secObj, "MFPConfig", swl_security_mfpModeToString(pAP->mfpConfig));
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

        amxd_object_set_uint32_t(secObj, "TransitionDisable", pAP->transitionDisable);
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
        if(wpsObj) {
            wpsObj->priv = &pAP->wpsSessionInfo;
        }
        amxd_object_set_int32_t(wpsObj, "Enable", pAP->WPS_Enable);
        amxd_object_set_cstring_t(wpsObj, "Status", cstr_AP_status[pAP->WPS_Status]);
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

        tmp_int32 = amxd_object_get_int32_t(object, "MaxAssociatedDevices", NULL);
        if(pAP->MaxStations != tmp_int32) {
            pAP->MaxStations = tmp_int32;
            commit = true;
        }

        char* mode = amxd_object_get_cstring_t(secObj, "ModeEnabled", NULL);
        const char* curSecMode = swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY);
        if(!swl_str_nmatches(curSecMode, mode, swl_str_len(curSecMode))) {
            /* We still must update the pAP->secModeEnabled value */
            swl_security_apMode_e idx = swl_security_apModeFromString(mode);
            if(swl_security_isApModeValid(idx)) {
                pAP->secModeEnabled = idx;
                wld_ap_sec_doSync(pAP);
            }
        }
        free(mode);

        char* mfp = amxd_object_get_cstring_t(secObj, "MFPConfig", NULL);
        const char* curMfpMode = swl_security_mfpModeToString(pAP->mfpConfig);
        if(!swl_str_nmatches(mfp, curMfpMode, swl_str_len(curMfpMode))) {
            /* We still must update the pAP->secModeEnabled value */
            swl_security_mfpMode_e idx = swl_security_mfpModeFromString(mfp);
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
        if(!swl_str_nmatches(pAP->WEPKey, wepKey, swl_str_len(pAP->WEPKey))) {
            if(isValidWEPKey(wepKey)) {
                swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), wepKey);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(wepKey);

        char* pskKey = amxd_object_get_cstring_t(secObj, "PreSharedKey", NULL);
        if(!swl_str_nmatches(pAP->preSharedKey, pskKey, swl_str_len(pAP->preSharedKey))) {
            if(isValidPSKKey(pskKey)) {
                swl_str_copy(pAP->preSharedKey, sizeof(pAP->preSharedKey), pskKey);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(pskKey);

        char* keyPassPhrase = amxd_object_get_cstring_t(secObj, "KeyPassPhrase", NULL);
        if(!swl_str_nmatches(pAP->keyPassPhrase, keyPassPhrase, swl_str_len(pAP->keyPassPhrase))) {
            if(isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), keyPassPhrase);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(keyPassPhrase);

        char* saePassphrase = amxd_object_get_cstring_t(secObj, "SAEPassphrase", NULL);
        if(!swl_str_nmatches(pAP->saePassphrase, saePassphrase, swl_str_len(pAP->saePassphrase))) {
            if(isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN)) {
                swl_str_copy(pAP->saePassphrase, sizeof(pAP->saePassphrase), saePassphrase);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(saePassphrase);

        char* encryption = amxd_object_get_cstring_t(secObj, "EncryptionMode", NULL);
        if(!swl_str_nmatches(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled], encryption,
                             swl_str_len(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled]))) {
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
        if(!swl_str_nmatches(pAP->radiusServerIPAddr, radSvrIp, swl_str_len(pAP->radiusServerIPAddr))) {
            swl_str_copy(pAP->radiusServerIPAddr, sizeof(pAP->radiusServerIPAddr), radSvrIp);
            wld_ap_sec_doSync(pAP);
        }
        free(radSvrIp);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusServerPort", NULL);
        if(pAP->radiusServerPort != tmp_int32) {
            pAP->radiusServerPort = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radSecret = amxd_object_get_cstring_t(secObj, "RadiusSecret", NULL);
        if(!swl_str_nmatches(pAP->radiusSecret, radSecret, swl_str_len(pAP->radiusSecret))) {
            swl_str_copy(pAP->radiusSecret, sizeof(pAP->radiusSecret), radSecret);
            wld_ap_sec_doSync(pAP);
        }
        free(radSecret);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusDefaultSessionTimeout", NULL);
        if(pAP->radiusDefaultSessionTimeout != tmp_int32) {
            pAP->radiusDefaultSessionTimeout = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radOwnIp = amxd_object_get_cstring_t(secObj, "RadiusOwnIPAddress", NULL);
        if(!swl_str_nmatches(pAP->radiusOwnIPAddress, radOwnIp, swl_str_len(pAP->radiusOwnIPAddress))) {
            swl_str_copy(pAP->radiusOwnIPAddress, sizeof(pAP->radiusOwnIPAddress), radOwnIp);
            wld_ap_sec_doSync(pAP);
        }
        free(radOwnIp);

        char* radNasId = amxd_object_get_cstring_t(secObj, "RadiusNASIdentifier", NULL);
        if(!swl_str_nmatches(pAP->radiusNASIdentifier, radNasId, swl_str_len(pAP->radiusNASIdentifier))) {
            swl_str_copy(pAP->radiusNASIdentifier, sizeof(pAP->radiusNASIdentifier), radNasId);
            wld_ap_sec_doSync(pAP);
        }
        free(radNasId);

        char* radStaId = amxd_object_get_cstring_t(secObj, "RadiusCalledStationId", NULL);
        if(!swl_str_nmatches(pAP->radiusCalledStationId, radStaId, swl_str_len(pAP->radiusCalledStationId))) {
            swl_str_copy(pAP->radiusCalledStationId, sizeof(pAP->radiusCalledStationId), radStaId);
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
        if(!swl_str_nmatches(pAP->oweTransModeIntf, oweTransIntf, swl_str_len(pAP->oweTransModeIntf))) {
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

static void s_setWDSEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: WDSEnable %d", pAP->alias, newEnable);

    pAP->wdsEnable = newEnable;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMBOEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: MBOEnable %d", pAP->alias, newEnable);

    pAP->mboEnable = newEnable;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMultiAPType_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* multiApTypeStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(multiApTypeStr, , ME, "NULL");
    wld_multiap_type_m multiApTypeMask = swl_conv_charToMask(multiApTypeStr, cstr_MultiAPType, MULTIAP_MAX);
    ASSERTI_NOT_EQUALS(multiApTypeMask, pAP->multiAPType, , ME, "%s: same MultiAPType %s", pAP->alias, multiApTypeStr);
    pAP->multiAPType = multiApTypeMask;
    pAP->pFA->mfn_wvap_multiap_update_type(pAP);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setApRole_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* apRoleStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(apRoleStr, , ME, "NULL");
    wld_apRole_e newApRole = swl_conv_charToEnum(apRoleStr, wld_apRole_str, AP_ROLE_MAX, AP_ROLE_OFF);
    ASSERTI_NOT_EQUALS(pAP->apRole, newApRole, , ME, "%s: same AP Role %s", pAP->alias, apRoleStr);
    pAP->apRole = newApRole;
    pAP->pFA->mfn_wvap_set_ap_role(pAP);

    SAH_TRACEZ_OUT(ME);
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

    ASSERTS_FALSE(amxd_object_get_type(instance_object) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(instance_object)));
    ASSERTS_FALSE(amxd_object_get_type(wifiVap) == amxd_object_template, amxd_status_unknown_error, ME, "Template");

    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");
    wld_vendorIe_t* vendor_ie = (wld_vendorIe_t*) instance_object->priv;

    if(vendor_ie == NULL) {
        vendor_ie = addVendorIEEntry(pAP, instance_object);
    }

    ASSERTS_NOT_NULL(vendor_ie, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t status = amxd_status_ok;

    char* oui = amxd_object_get_cstring_t(instance_object, "OUI", NULL);
    char* data = amxd_object_get_cstring_t(instance_object, "Data", NULL);

    if(getVendorIE(pAP, oui, data)) {
        SAH_TRACEZ_ERROR(ME, "Vendor IE already present or can not be modified");
    } else if(!isVendorIEValid(oui, data)) {
        SAH_TRACEZ_ERROR(ME, "Input are invalid");
        status = amxd_status_unknown_error;
    } else {
        swl_str_copy(vendor_ie->oui, SWL_OUI_STR_LEN, oui);
        swl_str_copy(vendor_ie->data, WLD_VENDORIE_T_DATA_SIZE, data);
        char* frame_type_var = amxd_object_get_cstring_t(instance_object, "FrameType", NULL);
        vendor_ie->frame_type = swl_conv_charToMask(frame_type_var, wld_vendorIe_frameType_str, VENDOR_IE_MAX);
        pAP->pFA->mfn_wvap_add_vendor_ie(pAP, vendor_ie);
        free(frame_type_var);
    }
    free(oui);
    free(data);

    return status;
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
    if(pAP->secModeEnabled != SWL_SECURITY_APMODE_WPA2_E) {
        SAH_TRACEZ_INFO(ME, "Error: secModeEnabled is not WPA2_E ");
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

static bool s_chechRnrEnabledOnOtherRadios(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    T_Radio* pOtherRad;
    wld_for_eachRad(pOtherRad) {
        if((swl_str_matches(pRad->Name, pOtherRad->Name)) ||
           (pOtherRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ)) {
            continue;
        }
        T_AccessPoint* pAPTmp;
        wld_rad_forEachAp(pAPTmp, pOtherRad) {
            if(pAPTmp->discoveryMethod & M_AP_DM_RNR) {
                return true;
            }
        }
    }
    return false;
}

amxd_status_t _wld_ap_validateDiscoveryMethod_pvf(amxd_object_t* object,
                                                  amxd_param_t* param,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    const char* oname = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    const char* dmStr = amxc_var_constcast(cstring_t, args);
    if(swl_str_matches(currentValue, dmStr)) {
        return amxd_status_ok;
    }
    wld_ap_dm_m discoveryMethod = swl_conv_charToMask(dmStr, g_str_wld_ap_dm, AP_DM_MAX);
    ASSERT_NOT_EQUALS(discoveryMethod, 0, status, ME, "%s: invalid value (%s)", oname, dmStr);
    ASSERT_FALSE((discoveryMethod & M_AP_DM_DISABLED) && (discoveryMethod & ~M_AP_DM_DISABLED), status, ME, "%s: Can not set disabled with other methods", oname);
    ASSERT_FALSE((discoveryMethod & M_AP_DM_UPR) && (discoveryMethod & M_AP_DM_FD), status, ME, "%s: Unsolicited Probe Response and FILS Discovery must not be enabled at the same time", oname);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "%s: no AP ctx", oname);
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "%s: no mapped Radio ctx", oname);

    /* Discovery method (UPR/FILS) is mandatory on 6GHz if no discovery
     * method is present on other bands (2.4/5GHz). Check if RNR is present
     */
    bool rnrEnabled = s_chechRnrEnabledOnOtherRadios(pRad);
    ASSERT_FALSE(rnrEnabled && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod == M_AP_DM_DISABLED), status, ME, "%s: Disabled is not valid for 6GHz AP", oname);
    ASSERT_FALSE((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod & M_AP_DM_UPR), status, ME, "%s: Unsolicted Probe Responses method is 6GHz AP only", oname);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_setDiscoveryMethod_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "pAP NULL");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "No mapped radio");

    const char* dmStr = amxc_var_constcast(cstring_t, newValue);
    wld_ap_dm_m discoveryMethod = swl_conv_charToMask(dmStr, g_str_wld_ap_dm, AP_DM_MAX);

    /* Discovery method (UPR/FILS) is mandatory on 6GHz if no discovery
     * method is present on other bands (2.4/5GHz). Check if RNR is present
     */
    bool rnrEnabled = s_chechRnrEnabledOnOtherRadios(pRad);
    ASSERT_FALSE(rnrEnabled && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod == M_AP_DM_DISABLED), , ME, "%s: Disabled is not valid for 6GHz AP", pAP->alias);
    ASSERT_FALSE((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod & M_AP_DM_UPR), , ME, "%s: Unsolicted Probe Responses method is 6GHz AP only", pAP->alias);

    pAP->discoveryMethod = discoveryMethod;
    SAH_TRACEZ_INFO(ME, "%s: Configure discovery method: %d", pAP->alias, pAP->discoveryMethod);

    pAP->pFA->mfn_wvap_set_discovery_method(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setApEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setAccessPointEnable %d", pAP->alias, newEnable);

    pAP->enable = newEnable;
    pAP->pFA->mfn_wvap_enable(pAP, newEnable, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    wld_ssid_syncEnable(pAP->pSSID, false);

    SAH_TRACEZ_OUT(ME);
}

T_AccessPoint* wld_ap_create(T_Radio* pRad, const char* vapName, amxd_object_t* vapObj) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_AccessPoint* pAP = calloc(1, sizeof(T_AccessPoint));
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");

    pAP->pBus = vapObj;
    if(!s_initAp(pAP, pRad, vapName)) {
        free(pAP);
        return NULL;
    }

    vapObj->priv = pAP;

    pAP->wpsSessionInfo.intfObj = vapObj;
    amxd_object_t* wpsinstance = amxd_object_get(vapObj, "WPS");
    ASSERTW_NOT_NULL(wpsinstance, pAP, ME, "%s: WPS subObj is not available", pAP->name);
    wpsinstance->priv = &pAP->wpsSessionInfo;

    return pAP;
}

/**
 * This function Destroys the access point
 * first deauthentify all associated stations, destroy vap then deletes the accesspoint.
 */
void wld_ap_destroy(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    s_deinitAP(pAP);

    if(pAP->pBus != NULL) {
        pAP->pBus->priv = NULL;
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

swl_rc_ne wld_vap_saveAssocReq(T_AccessPoint* pAP, swl_bit8_t* frameBin, size_t frameLen) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_80211_mgmtFrame_t* frame = swl_80211_getMgmtFrame(frameBin, frameLen);
    ASSERT_NOT_NULL(frame, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint8_t mgtFrameType = swl_80211_mgtFrameType(&frame->fc);
    if((mgtFrameType != SWL_80211_MGT_FRAME_TYPE_ASSOC_REQUEST) &&
       (mgtFrameType != SWL_80211_MGT_FRAME_TYPE_REASSOC_REQUEST)) {
        SAH_TRACEZ_ERROR(ME, "%s: not (re)assoc frame type (0x%x)", pAP->alias, frame->fc.subType);
        return SWL_RC_INVALID_PARAM;
    }
    char frameStr[(frameLen * 2) + 1];
    bool ret = swl_hex_fromBytesSep(frameStr, sizeof(frameStr), frameBin, frameLen, false, 0, NULL);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: fail to hex dump frame type %d (len:%zu)", pAP->alias, frame->fc.subType, frameLen)
    swl_timeMono_t timestamp = swl_time_getMonoSec();
    wld_vap_assocTableStruct_t tuple = {frame->transmitter, frame->bssid, frameStr, timestamp, frame->fc.subType};
    swl_circTable_addValues(&(pAP->lastAssocReq), &tuple);
    SAH_TRACEZ_INFO(ME, "%s: add/update assocReq entry for station "SWL_MAC_FMT, pAP->alias,
                    SWL_MAC_ARG(frame->transmitter.bMac));
    return SWL_RC_OK;
}

swl_rc_ne wld_vap_sync_device(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {

    ASSERT_NOT_NULL(pAD, SWL_RC_ERROR, ME, "%s: AssociatedDevice==null", pAP->alias);

    uint32_t nextDevIndex = pAP->lastDevIndex + 1;

    if(pAD->object == NULL) {
        SAH_TRACEZ_INFO(ME, "%s: Creating template object for mac %s (id %u)", pAP->name, pAD->Name, nextDevIndex);
        amxd_object_t* templateObject = amxd_object_get(pAP->pBus, "AssociatedDevice");

        if(!templateObject) {
            SAH_TRACEZ_ERROR(ME, "%s: Could not get template: %p", pAP->alias, templateObject);
            return SWL_RC_INVALID_PARAM;
        }

        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(templateObject, &trans, SWL_RC_ERROR, ME, "%s : trans init failure", pAD->Name);

        amxd_trans_add_inst(&trans, nextDevIndex, NULL);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "%s : trans apply failure", pAD->Name);

        pAD->object = amxd_object_get_instance(templateObject, NULL, nextDevIndex);
        ASSERT_NOT_NULL(pAD->object, SWL_RC_ERROR, ME, "%s: failure to create object", pAD->Name);
        pAD->object->priv = (void*) pAD;
        pAP->lastDevIndex = nextDevIndex;
    }

    amxd_object_t* object = pAD->object;

    if(swl_timespec_isZero(&pAD->lastSampleTime) || (swl_timespec_diff(NULL, &pAD->lastSampleSyncTime, &pAD->lastSampleTime) != 0)
       || (pAD->latestStateChangeTime >= pAD->lastSampleSyncTime.tv_sec)) {

        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, SWL_RC_ERROR, ME, "%s : trans init failure", pAD->Name);

        amxd_trans_set_cstring_t(&trans, "MACAddress", pAD->Name);

        amxd_trans_set_value(cstring_t, &trans, "ChargeableUserId", pAD->Radius_CUID);
        amxd_trans_set_value(bool, &trans, "AuthenticationState", pAD->AuthenticationState);
        amxd_trans_set_value(uint32_t, &trans, "LastDataDownlinkRate", pAD->LastDataDownlinkRate);
        amxd_trans_set_value(uint32_t, &trans, "LastDataUplinkRate", pAD->LastDataUplinkRate);
        amxd_trans_set_value(int32_t, &trans, "AvgSignalStrength", WLD_ACC_TO_VAL(pAD->rssiAccumulator));
        amxd_trans_set_value(int32_t, &trans, "SignalStrength", pAD->SignalStrength);

        char rssiHistory[64] = {'\0'};
        wld_ad_printSignalStrengthHistory(pAD, rssiHistory, sizeof(rssiHistory));
        amxd_trans_set_value(cstring_t, &trans, "SignalStrengthHistory", rssiHistory);

        char TBuf[64] = {'\0'};
        wld_ad_printSignalStrengthByChain(pAD, TBuf, sizeof(TBuf));

        amxd_trans_set_value(int32_t, &trans, "AvgSignalStrengthByChain", pAD->AvgSignalStrengthByChain);
        amxd_trans_set_value(cstring_t, &trans, "SignalStrengthByChain", TBuf);
        amxd_trans_set_value(int32_t, &trans, "SignalNoiseRatio", pAD->SignalNoiseRatio);
        amxd_trans_set_value(uint32_t, &trans, "Retransmissions", pAD->Retransmissions);
        amxd_trans_set_value(uint32_t, &trans, "Rx_Retransmissions", pAD->Rx_Retransmissions);
        amxd_trans_set_value(uint32_t, &trans, "Rx_RetransmissionsFailed", pAD->Rx_RetransmissionsFailed);
        amxd_trans_set_value(uint32_t, &trans, "Tx_Retransmissions", pAD->Tx_Retransmissions);
        amxd_trans_set_value(uint32_t, &trans, "Tx_RetransmissionsFailed", pAD->Tx_RetransmissionsFailed);
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
        amxd_trans_set_value(cstring_t, &trans, "LinkBandwidth", swl_bandwidth_unknown_str[pAD->assocCaps.linkBandwidth]);

        swl_bandwidth_e maxBw = pAD->MaxBandwidthSupported;
        if(maxBw == SWL_BW_AUTO) {
            maxBw = wld_util_getMaxBwCap(&pAD->assocCaps);
        }
        amxd_trans_set_value(cstring_t, &trans, "MaxBandwidthSupported", swl_bandwidth_unknown_str[maxBw]);

        swl_typeMcs_toTransParam(&trans, "UplinkRateSpec", pAD->upLinkRateSpec);
        swl_typeMcs_toTransParam(&trans, "DownlinkRateSpec", pAD->downLinkRateSpec);

        amxd_trans_set_value(cstring_t, &trans, "DeviceType", cstr_DEVICE_TYPES[pAD->deviceType]);
        amxd_trans_set_value(int32_t, &trans, "DevicePriority", pAD->devicePriority);
        char buffer[128] = {0};
        wld_writeCapsToString(buffer, sizeof(buffer), swl_staCap_str, pAD->capabilities, SWL_STACAP_MAX);
        amxd_trans_set_value(cstring_t, &trans, "Capabilities", buffer);
        amxd_trans_set_value(uint32_t, &trans, "ConnectionDuration", pAD->connectionDuration);
        swl_typeTimeMono_toTransParam(&trans, "LastStateChange", pAD->latestStateChangeTime);
        swl_typeTimeMono_toTransParam(&trans, "AssociationTime", pAD->associationTime);
        swl_typeTimeMono_toTransParam(&trans, "DisassociationTime", pAD->disassociationTime);
        amxd_trans_set_value(bool, &trans, "PowerSave", pAD->powerSave);
        amxd_trans_set_value(cstring_t, &trans, "OperatingStandard", swl_radStd_unknown_str[pAD->operatingStandard]);
        amxd_trans_set_value(uint32_t, &trans, "MUGroupId", pAD->staMuMimoInfo.muGroupId);
        amxd_trans_set_value(uint32_t, &trans, "MUUserPositionId", pAD->staMuMimoInfo.muUserPosId);
        amxd_trans_set_value(uint32_t, &trans, "MUMimoTxPktsCount", pAD->staMuMimoInfo.txAsMuPktsCnt);
        amxd_trans_set_value(uint32_t, &trans, "MUMimoTxPktsPercentage", pAD->staMuMimoInfo.txAsMuPktsPrc);

        wld_ad_syncCapabilities(&trans, &pAD->assocCaps);

        swl_conv_maskToChar(buffer, sizeof(buffer), pAD->uniiBandsCapabilities, swl_uniiBand_str, SWL_BAND_MAX),
        amxd_trans_set_value(cstring_t, &trans, "UNIIBandsCapabilities", buffer);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "%s : trans apply failure", pAD->Name);

        pAD->lastSampleSyncTime = pAD->lastSampleTime;
    }


    if(pAD->probeReqCaps.updateTime != pAD->lastProbeCapUpdateTime) {
        amxd_object_t* probeReqCapsObject = amxd_object_get(object, "ProbeReqCaps");

        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(probeReqCapsObject, &trans, SWL_RC_ERROR, ME, "%s : trans init failure", pAD->Name);
        wld_ad_syncCapabilities(&trans, &pAD->probeReqCaps);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "%s : trans apply failure", pAD->Name);
        pAD->lastProbeCapUpdateTime = pAD->probeReqCaps.updateTime;
    }


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

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pAP->pBus, &trans, , ME, "%s : trans init failure", pAP->alias);
    amxd_trans_set_value(int32_t, &trans, "AssociatedDeviceNumberOfEntries", pAP->AssociatedDeviceNumberOfEntries);
    pAP->ActiveAssociatedDeviceNumberOfEntries = active;
    amxd_trans_set_value(int32_t, &trans, "ActiveAssociatedDeviceNumberOfEntries", pAP->ActiveAssociatedDeviceNumberOfEntries);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pAP->alias);

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

/*
 * @brief compare assoc dev entries with their latestStateChangeTime values
 * @param e1 entry 1
 * @param e2 entry 2
 * @return < 0 when e1 is selected (ie *e1 not null and oldest against *e2)
 *         > 0 when e2 is selected (ie *e2 not null and oldest against *e1)
 *         = 0 when e1 and e2 are equivalent
 */
static int s_inactAdCmp(const void* e1, const void* e2) {
    T_AssociatedDevice** ppSta1 = (T_AssociatedDevice**) e1;
    T_AssociatedDevice** ppSta2 = (T_AssociatedDevice**) e2;
    //not null first
    ASSERT_NOT_NULL(ppSta1, ((ppSta2 != NULL) && (*ppSta2 != NULL)), ME, "NULL");
    ASSERT_NOT_NULL(ppSta2, -(*ppSta1 != NULL), ME, "NULL");
    ASSERT_NOT_NULL(*ppSta1, (*ppSta2 != NULL), ME, "NULL");
    ASSERT_NOT_NULL(*ppSta2, -1, ME, "NULL");
    //oldest first
    return ((*ppSta1)->latestStateChangeTime - (*ppSta2)->latestStateChangeTime);
}

/* clean up stations who failed authentication */
bool wld_vap_cleanup_stationlist(T_AccessPoint* pAP) {
    int inactiveStaCount = 0;

    /* Allow two devices with Authenticationstate==false
     * The first is one to keep around (to diagnose failed connection attempts)
     * The second is necessary to allow a client to go through
     * AuthenticationState==false => AuthenticationState==true when connecting
     * Since we should call this function each time a sta DC's, we use a simple
     * cleanup algorithm instead of doing the effort of sorting and cleaning.
     */
    T_AssociatedDevice* inactiveStaList[pAP->AssociatedDeviceNumberOfEntries + 1];
    for(int assocIndex = 0; assocIndex < pAP->AssociatedDeviceNumberOfEntries; assocIndex++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[assocIndex];
        if((pAD != NULL) && (!pAD->Active)) {
            inactiveStaList[inactiveStaCount] = pAD;
            inactiveStaCount++;
        }
    }
    if(inactiveStaCount <= NR_OF_STICKY_UNAUTHORIZED_STATIONS) {
        return true;
    }
    //sort inactive stations by latestStateChangeTime
    qsort(inactiveStaList, inactiveStaCount, sizeof(T_AssociatedDevice*), s_inactAdCmp);
    for(int i = 0; i < (inactiveStaCount - NR_OF_STICKY_UNAUTHORIZED_STATIONS); i++) {
        T_AssociatedDevice* inactiveSta = inactiveStaList[i];
        if(inactiveSta == NULL) {
            continue;
        }
        SAH_TRACEZ_INFO(ME, "Destroying oldest failed-auth entry (%s) out of %d",
                        inactiveSta->Name,
                        inactiveStaCount);
        if(!wld_ad_destroy(pAP, inactiveSta)) {
            SAH_TRACEZ_ERROR(ME, "Fail to destroy dm entry of (%s)", inactiveSta->Name);
            return false;
        }
    }
    return true;
}

bool wld_vap_assoc_update_cuid(T_AccessPoint* pAP, swl_macBin_t* mac, char* cuid, int len _UNUSED) {

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, mac);
    ASSERT_NOT_NULL(pAD, false, ME, "NULL");

    memset(pAD->Radius_CUID, 0, sizeof(pAD->Radius_CUID));
    memcpy(pAD->Radius_CUID, cuid, sizeof(pAD->Radius_CUID));

    wld_vap_sync_device(pAP, pAD);
    return true;
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
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");

    for(int AssociatedDeviceIdx = 0; AssociatedDeviceIdx < pAP->AssociatedDeviceNumberOfEntries; AssociatedDeviceIdx++) {
        T_AssociatedDevice* dev = pAP->AssociatedDevice[AssociatedDeviceIdx];
        if((dev != NULL) && !dev->Active) {
            wld_ad_destroy(pAP, dev);
        }
    }
    wld_vap_sync_assoclist(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

T_AccessPoint* wld_vap_get_vap(const char* ifname) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP;
        wld_rad_forEachAp(pAP, pRad) {
            if(pAP && swl_str_matches(pAP->alias, ifname)) {
                return pAP;
            }
        }
    }

    return NULL;
}

T_AccessPoint* wld_ap_getVapByName(const char* name) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP;
        wld_rad_forEachAp(pAP, pRad) {
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

    wld_status_e newSsidStatus = 0;

    int status = pAP->pFA->mfn_wvap_status(pAP);
    if(status > 0) {
        pAP->status = APSTI_ENABLED;
    } else {
        pAP->status = APSTI_DISABLED;
    }

    if(pRad->status == RST_ERROR) {
        newSsidStatus = RST_ERROR;
    } else if((pAP->status == APSTI_DISABLED) || (pRad->status == RST_DOWN)) {
        newSsidStatus = RST_DOWN;
    } else if(pRad->status == RST_DORMANT) {
        newSsidStatus = RST_LLDOWN;
    } else {
        newSsidStatus = RST_UP;
    }

    SAH_TRACEZ_INFO(ME, "%s: ap %u -> %u / ssid %u -> %u", pAP->alias, oldVapStatus, pAP->status, oldSsidStatus, newSsidStatus);

    if((oldVapStatus == pAP->status) && (oldSsidStatus == newSsidStatus)) {
        return;
    }
    pAP->lastStatusChange = swl_time_getMonoSec();
    wld_ssid_setStatus(pSSID, newSsidStatus, true);

    ASSERT_TRUE(swl_typeCharPtr_commitObjectParam(pAP->pBus, "Status", (char*) cstr_AP_status[pAP->status]), ,
                ME, "%s: fail to commit status (%s)", pAP->alias, cstr_AP_status[pAP->status]);

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

static void s_setApDriverConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, , ME, "NULL");

    wld_vap_driverCfgChange_m params = 0;
    amxc_var_for_each(newValue, newParamValues) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "BssMaxIdlePeriod")) {
            int32_t tmpInt32 = amxc_var_dyncast(int32_t, newValue);
            if(tmpInt32 != pAP->driverCfg.bssMaxIdlePeriod) {
                pAP->driverCfg.bssMaxIdlePeriod = tmpInt32;
                params |= M_WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD;
            }
        }
    }
    if(params) {
        SAH_TRACEZ_INFO(ME, "%s update driver config (0x%x)", pAP->alias, params);
        pAP->pFA->mfn_wvap_set_config_driver(pAP, params);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApDriverConfigDmHdlrs, ARR(), .objChangedCb = s_setApDriverConfig_ocf);

void _wld_ap_setDriverConfig_ocf(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApDriverConfigDmHdlrs, sig_name, data, priv);
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
    ASSERT_STR(macStation, SWL_RC_INVALID_PARAM, ME, "empty mac");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macBin_t bMac;
    SWL_MAC_CHAR_TO_BIN(&bMac, macStation);

    *data = swl_circTable_getMatchingTuple(&(pAP->lastAssocReq), 0, &bMac);
    ASSERT_NOT_NULL(*data, SWL_RC_ERROR, ME, "NULL");
    return SWL_RC_OK;
}

static amxd_status_t s_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, amxc_var_t* retval) {
    wld_vap_assocTableStruct_t* tuple = NULL;
    swl_rc_ne ret = wld_ap_getLastAssocReq(pAP, macStation, &tuple);
    ASSERT_TRUE(swl_rc_isOk(ret), amxd_status_unknown_error, ME, "Error during execution");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    for(size_t i = 0; i < assocTable.nrTypes; i++) {
        swl_type_t* type = assocTable.types[i];
        swl_typeData_t* tmpValue = swl_tupleType_getValue(&assocTable, tuple, i);
        amxc_var_t* tmpVar = amxc_var_add_new_key(retval, s_assocTableNames[i]);
        swl_type_toVariant(type, tmpVar, tmpValue);
    }
    return amxd_status_ok;
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
    T_AccessPoint* pAP = object->priv;
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "invalid AP");
    const char* macStation = GET_CHAR(args, "mac");
    ASSERT_NOT_NULL(macStation, amxd_status_parameter_not_found, ME, "No mac station given");
    return s_getLastAssocReq(pAP, macStation, retval);
}

amxd_status_t _AssociatedDevice_getLastAssocReq(amxd_object_t* object,
                                                amxd_function_t* func _UNUSED,
                                                amxc_var_t* args _UNUSED,
                                                amxc_var_t* retval) {
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "invalid AP parent");
    char* macStation = amxd_object_get_cstring_t(object, "MACAddress", NULL);
    ASSERT_NOT_NULL(macStation, amxd_status_parameter_not_found, ME, "No mac station given");
    amxd_status_t status = s_getLastAssocReq(pAP, macStation, retval);
    free(macStation);
    return status;
}

amxd_status_t _AccessPoint_debug(amxd_object_t* obj,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* retMap) {
    T_AccessPoint* pAP = wld_ap_fromObj(obj);
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "No AP Ctx");

    const char* feature = GET_CHAR(args, "op");
    ASSERT_NOT_NULL(feature, amxd_status_unknown_error, ME, "No argument given");

    amxc_var_init(retMap);
    amxc_var_set_type(retMap, AMXC_VAR_ID_HTABLE);

    if(!strcasecmp(feature, "RssiMon")) {
        wld_ap_rssiEv_debug(pAP, retMap);
    } else if(!strcasecmp(feature, "recentDcs")) {
        amxc_var_set_type(retMap, AMXC_VAR_ID_LIST);
        wld_assocDev_listRecentDisconnects(pAP, retMap);
    } else if(!strcasecmp(feature, "SSIDAdvertisementEnabled")) {
        bool enable = GET_BOOL(args, "enable");
        wld_ap_hostapd_setSSIDAdvertisement(pAP, enable);
    } else if(!strcasecmp(feature, "setKey")) {
        const char* key = GET_CHAR(args, "key");
        const char* value = GET_CHAR(args, "value");
        if(!strcasecmp(key, "wep_key")) {
            swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), value);
        } else if(!strcasecmp(key, "wpa_psk")) {
            swl_str_copy(pAP->preSharedKey, sizeof(pAP->preSharedKey), value);
        } else if(!strcasecmp(key, "wpa_passphrase")) {
            swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), value);
        } else if(!strcasecmp(key, "sae_password")) {
            swl_str_copy(pAP->saePassphrase, sizeof(pAP->saePassphrase), value);
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
    } else if(!strcasecmp(feature, "writeSta")) {
        swl_print_args_t tmpArgs = g_swl_print_json;
        FILE* fp = fopen("/tmp/vapStaDump.txt", "w");
        if(fp == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failure to open file");
            return amxd_status_file_not_found;
        }
        swl_type_arrayToFilePrint(&gtWld_associatedDevicePtr.type, fp, pAP->AssociatedDevice,
                                  pAP->AssociatedDeviceNumberOfEntries, false, &tmpArgs);
        fclose(fp);
    } else {
        amxc_var_add_key(cstring_t, retMap, "Error", "Unknown command");
    }

    return amxd_status_ok;
}

SWLA_DM_HDLRS(sApDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("SSIDReference", s_setSSIDRef_pwf),
                  SWLA_DM_PARAM_HDLR("SSIDAdvertisementEnabled", s_setSsidAdvEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("RetryLimit", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("WMMEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("RetryLimit", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("UAPSDEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("MCEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("APBridgeDisable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("IsolationEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("BridgeInterface", s_setBridgeInterface_pwf),
                  SWLA_DM_PARAM_HDLR("DefaultDeviceType", s_setDefaultDeviceType_pwf),
                  SWLA_DM_PARAM_HDLR("IEEE80211kEnabled", s_set80211kEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("WDSEnable", s_setWDSEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MBOEnable", s_setMBOEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPType", s_setMultiAPType_pwf),
                  SWLA_DM_PARAM_HDLR("ApRole", s_setApRole_pwf),
                  SWLA_DM_PARAM_HDLR("MaxAssociatedDevices", s_setMaxStations_pwf),
                  SWLA_DM_PARAM_HDLR("DiscoveryMethodEnabled", s_setDiscoveryMethod_pwf),
                  SWLA_DM_PARAM_HDLR("dbgAPEnable", s_setDbgEnable_pwf),
                  SWLA_DM_PARAM_HDLR("dbgAPFile", s_setDbgFile_pwf),
                  SWLA_DM_PARAM_HDLR("MACFilterAddressList", wld_apMacFilter_setAddressList_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setApEnable_pwf),
                  ),
              .instAddedCb = s_addApInst_oaf, );

void _wld_ap_setConf_ocf(const char* const sig_name,
                         const amxc_var_t* const data,
                         void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApDmHdlrs, sig_name, data, priv);
}

