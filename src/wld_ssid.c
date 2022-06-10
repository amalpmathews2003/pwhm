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
#include <malloc.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_ssid.h"
#include "wld_accesspoint.h"
#include "wld_statsmon.h"
#include "wld_radio.h"
#include "swl/swl_string.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "ssid"

static char* SSID_SupStatus[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};

T_SSID* s_createSsid(amxd_object_t* obj) {
    ASSERT_NOT_NULL(obj, NULL, ME, "NULL");
    T_SSID* pSSID = calloc(1, sizeof(T_SSID));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->pBus = obj;
    obj->priv = pSSID;
    pSSID->debug = SSID_POINTER;
    sprintf(pSSID->SSID, "PWHM_SSID%d", amxd_object_get_index(obj));
    return pSSID;
}

/* Be sure that our attached memory structure is cleared */
static void s_destroySsid(amxd_object_t* object) {
    T_SSID* pSSID = (T_SSID*) object->priv;
    ASSERT_TRUE(debugIsSsidPointer(pSSID), , ME, "INVALID");
    SAH_TRACEZ_INFO(ME, "%s: destroy SSID", pSSID->Name);
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
    if(debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
        pAP->pSSID = NULL;
        //update SSID Reference of AccessPoint
    } else if(debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
        pEP->pSSID = NULL;
        //update SSID Reference of AccessPoint
    }
    free(pSSID);
    object->priv = NULL;
}

amxd_status_t _wld_ssid_addInstance_ocf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s:%s)",
                    object, amxd_object_get_name(object, AMXD_OBJECT_NAMED), amxd_object_get_path(object, AMXD_OBJECT_NAMED));
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    s_createSsid(instance);
    return status;
}

amxd_status_t _wld_ssid_delInstance_odf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_INFO(ME, "del instance object(%p:%s:%s)",
                    object, amxd_object_get_name(object, AMXD_OBJECT_NAMED), amxd_object_get_path(object, AMXD_OBJECT_NAMED));
    amxd_status_t status = amxd_status_unknown_error;
    status = amxd_action_object_del_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to remove obj instance %d", status);
    s_destroySsid(object);
    return status;
}

amxd_status_t _wld_ssid_setLowerLayers_pwf(amxd_object_t* object,
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
    T_SSID* pSSID = (T_SSID*) object->priv;
    const char* radioRef = amxc_var_dyncast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "ssid_obj(%p:%s:%s) LowerLayer %s",
                    object, amxd_object_get_name(object, AMXD_OBJECT_NAMED), amxd_object_get_path(object, AMXD_OBJECT_NAMED),
                    radioRef);
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    T_Radio* pRad = NULL;
    amxd_object_t* pRadObj = amxd_object_findf(amxd_dm_get_root(wld_plugin_dm), "%s", radioRef);
    if(pRadObj) {
        pRad = (T_Radio*) pRadObj->priv;
        SAH_TRACEZ_INFO(ME, "radObj(%p:%s:%s) pRad(%p)",
                        pRadObj, amxd_object_get_name(pRadObj, AMXD_OBJECT_NAMED), amxd_object_get_path(pRadObj, AMXD_OBJECT_NAMED),
                        pRad);
    }
    pSSID->RADIO_PARENT = pRad;
    return amxd_status_ok;
}

amxd_status_t _wld_ssid_setEnable_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiSsid = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiSsid) != amxd_object_instance) {
        return rv;
    }
    T_SSID* pSSID = (T_SSID*) wifiSsid->priv;
    rv = amxd_action_param_write(wifiSsid, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool flag = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set Enable %d", flag);

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_ERROR(ME, "Invalid pointer !");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK; /* FIX ME */
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;     /* FIX ME */
    T_Radio* pRad = (T_Radio*) pSSID->RADIO_PARENT;

    if(pAP) {
        pAP->pFA->mfn_wvap_enable(pAP, flag, SET);
    } else if(pEP) {
        pEP->pFA->mfn_wendpoint_enable(pEP, flag, SET);
    }

    wld_rad_doCommitIfUnblocked(pRad);

    if(pAP) {
        /* Sync AP object values */
        amxd_object_set_bool(pAP->pBus, "Enable", flag);
    } else if(pEP) {
        /* Sync Endpoint object values */
        amxd_object_set_bool(pEP->pBus, "Enable", flag);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _SSID_getSSIDStats(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {
    T_AccessPoint* pAP;

    SAH_TRACEZ_INFO(ME, "getSSIDStats");

    T_SSID* pSSID = (T_SSID*) object->priv;

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_ERROR(ME, "Invalid pointer !");
        return amxd_status_ok;
    }

    amxd_object_t* stats = amxd_object_get(pSSID->pBus, "Stats");

    pAP = pSSID->AP_HOOK;
    /* Update the stats with Linux counters if we don't handle them in the vendor plugin.
       TRAP (-1) was called ? */
    if(pAP->pFA->mfn_wvap_update_ap_stats(pAP->pRadio, pAP) < 0) {
        wld_updateVAPStats(pAP, NULL);
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    /* Update object with what we have */
    amxd_object_set_uint64_t(stats, "BytesSent", pSSID->stats.BytesSent);
    amxd_object_set_uint64_t(stats, "BytesReceived", pSSID->stats.BytesReceived);
    amxd_object_set_uint64_t(stats, "PacketsSent", pSSID->stats.PacketsSent);
    amxd_object_set_uint64_t(stats, "PacketsReceived", pSSID->stats.PacketsReceived);
    amxd_object_set_uint32_t(stats, "ErrorsSent", pSSID->stats.ErrorsSent);
    amxd_object_set_uint32_t(stats, "RetransCount", pSSID->stats.RetransCount);
    amxd_object_set_uint32_t(stats, "ErrorsReceived", pSSID->stats.ErrorsReceived);
    amxd_object_set_uint32_t(stats, "UnicastPacketsSent", pSSID->stats.UnicastPacketsSent);
    amxd_object_set_uint32_t(stats, "UnicastPacketsReceived", pSSID->stats.UnicastPacketsReceived);
    amxd_object_set_uint32_t(stats, "DiscardPacketsSent", pSSID->stats.DiscardPacketsSent);
    amxd_object_set_uint32_t(stats, "DiscardPacketsReceived", pSSID->stats.DiscardPacketsReceived);
    amxd_object_set_uint32_t(stats, "MulticastPacketsSent", pSSID->stats.MulticastPacketsSent);
    amxd_object_set_uint32_t(stats, "MulticastPacketsReceived", pSSID->stats.MulticastPacketsReceived);
    amxd_object_set_uint32_t(stats, "BroadcastPacketsSent", pSSID->stats.BroadcastPacketsSent);
    amxd_object_set_uint32_t(stats, "BroadcastPacketsReceived", pSSID->stats.BroadcastPacketsReceived);
    amxd_object_set_uint32_t(stats, "UnknownProtoPacketsReceived", pSSID->stats.UnknownProtoPacketsReceived);
    amxd_object_set_uint32_t(stats, "FailedRetransCount", pSSID->stats.FailedRetransCount);
    amxd_object_set_uint32_t(stats, "RetryCount", pSSID->stats.RetryCount);
    amxd_object_set_uint32_t(stats, "MultipleRetryCount", pSSID->stats.MultipleRetryCount);
    amxd_object_set_uint32_t(stats, "Temperature", pSSID->stats.TemperatureDegreesCelsius);

    wld_util_writeWmmStats(stats, "WmmPacketsSent", pSSID->stats.WmmPacketsSent);
    wld_util_writeWmmStats(stats, "WmmPacketsReceived", pSSID->stats.WmmPacketsReceived);
    wld_util_writeWmmStats(stats, "WmmFailedSent", pSSID->stats.WmmFailedSent);
    wld_util_writeWmmStats(stats, "WmmFailedReceived", pSSID->stats.WmmFailedReceived);
    wld_util_writeWmmStats(stats, "WmmBytesSent", pSSID->stats.WmmBytesSent);
    wld_util_writeWmmStats(stats, "WmmFailedbytesSent", pSSID->stats.WmmFailedBytesSent);
    wld_util_writeWmmStats(stats, "WmmBytesReceived", pSSID->stats.WmmBytesReceived);
    wld_util_writeWmmStats(stats, "WmmFailedBytesReceived", pSSID->stats.WmmFailedBytesReceived);

    wld_util_addWmmStats(stats, retval, "WmmPacketsSent");
    wld_util_addWmmStats(stats, retval, "WmmPacketsReceived");
    wld_util_addWmmStats(stats, retval, "WmmFailedSent");
    wld_util_addWmmStats(stats, retval, "WmmFailedReceived");
    wld_util_addWmmStats(stats, retval, "WmmBytesSent");
    wld_util_addWmmStats(stats, retval, "WmmFailedbytesSent");
    wld_util_addWmmStats(stats, retval, "WmmBytesReceived");
    wld_util_addWmmStats(stats, retval, "WmmFailedBytesReceived");

    return amxd_status_ok;
}

/**
 * check the SSID object block on wrong input.
 * Currently only SSID is checked. (All other are accept by default).
 */
amxd_status_t _wld_ssid_validateSSID_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* param,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_value;
    const char* current_value = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(current_value, status, ME, "NULL");
    const char* new_value = amxc_var_constcast(cstring_t, args);
    if(swl_str_matches(current_value, new_value) || isValidSSID(new_value)) {
        status = amxd_status_ok;
    }
    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ssid_setSSID_pwf(amxd_object_t* object _UNUSED,
                                    amxd_param_t* parameter _UNUSED,
                                    amxd_action_t reason _UNUSED,
                                    const amxc_var_t* const args _UNUSED,
                                    amxc_var_t* const retval _UNUSED,
                                    void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return rv;
    }
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = (T_SSID*) object->priv;
    ASSERTW_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    ASSERT_TRUE(debugIsSsidPointer(pSSID), amxd_status_unknown_error, ME, "Invalid SSID Ctx");

    const char* SSID = amxc_var_constcast(cstring_t, args);
    ASSERT_NOT_NULL(SSID, amxd_status_invalid_value, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "set SSID %s", SSID);

    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    if(pAP) {
        SAH_TRACEZ_INFO(ME, "apply to AP %p", pAP);
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) SSID, strlen(SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }
    //Setting endpoint ssid should only be done internally => no change necessary here.

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

void syncData_SSID2OBJ(amxd_object_t* object, T_SSID* pS, int set) {
    //int idx,bitchk,asdev;
    char ValBuf[32];
    char TBuf[128];
    char objPath[128];
    T_AccessPoint* pAP = NULL;
    T_EndPoint* pEP = NULL;

    SAH_TRACEZ_IN(ME);
    if(!(object && pS)) {
        /* Missing data pointers... */
        return;
    }

    pAP = (T_AccessPoint*) pS->AP_HOOK;
    pEP = (T_EndPoint*) pS->ENDP_HOOK;

    if(set & SET) {
        memset(ValBuf, 0, sizeof(ValBuf));
        memset(TBuf, 0, sizeof(TBuf));
        memset(objPath, 0, sizeof(objPath));

        /* Set SSID data in mapped OBJ structure */
        /** 'Enable' Enables or disables the SSID entry  */
        //set_OBJ_ParameterHelper(TPH_INT32 , object, "Enable", &pS->enable);
        /** 'Status' The current operational state of the SSID entry. */
        amxd_object_set_cstring_t(object, "Status", SSID_SupStatus[pS->status]);
        /** 'SSID' The current service set identifier in use by the
         *  connection. The SSID is an identifier that is attached to
         *  packets sent over the wireless LAN that functions as an
         *  ID for joining a particular radio network (BSS). */
        amxd_object_set_cstring_t(object, "SSID", pS->SSID);
        /** 'MACAddress' The MAC address of this interface.  */
        sprintf(TBuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
                pS->MACAddress[0], pS->MACAddress[1], pS->MACAddress[2],
                pS->MACAddress[3], pS->MACAddress[4], pS->MACAddress[5]);
        amxd_object_set_cstring_t(object, "MACAddress", TBuf);
        /** 'BSSID' The Basic Service Set ID. This is the MAC address
         *  of the access point, which can either be local (when this
         *  instance models an access point SSID) or remote (when
         *  this instance models an end point SSID).
         *  In multiple VAP setup it differs */
        int err = 0;
        if(pAP) {
            err = pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) TBuf, sizeof(TBuf), GET);
            wld_autoCommitMgr_notifyVapEdit(pAP);
        } else if(pEP) {
            err = pEP->pFA->mfn_wendpoint_bssid(pEP, (unsigned char*) TBuf, sizeof(TBuf));
            wld_autoCommitMgr_notifyEpEdit(pEP);
        } else {
            err = WLD_ERROR_NOT_IMPLEMENTED;
        }
        if(err < 0) {
            snprintf(TBuf, sizeof(TBuf), WLD_EMPTY_MAC_ADDRESS);
            memset(pS->BSSID, 0, ETHER_ADDR_LEN);
        } else {
            convStr2Mac(pS->BSSID, ETHER_ADDR_LEN, (unsigned char*) TBuf, ETHER_ADDR_STR_LEN);
        }

        amxd_object_set_cstring_t(object, "BSSID", TBuf);

        if(!(set & NO_COMMIT)) {
            if(!(object)) {
                SAH_TRACEZ_ERROR(ME, "Failed to commit");
            }
        }
    } else {
        /* Get AP data from OBJ to AP */
        int32_t tmp_int32 = amxd_object_get_int32_t(object, "Enable", NULL);
        if(pS->enable != tmp_int32) {
            if(pAP) {
                pS->enable = pAP->enable;   // AP rulez state!
            }
        }

        const char* ssid = amxd_object_get_cstring_t(object, "SSID", NULL);
        if(strncmp(pS->SSID, ssid, strlen(pS->SSID))) {
            if(isValidSSID(ssid)) {
                swl_str_copy(pS->SSID, sizeof(pS->SSID), ssid);
                if(pAP) {
                    pAP->pFA->mfn_wvap_ssid(pAP, pS->SSID, strlen(pS->SSID), set);
                    wld_autoCommitMgr_notifyVapEdit(pAP);
                }
            }
        }
    }
    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _SSID_VerifySSID(amxd_object_t* object _UNUSED,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args _UNUSED,
                               amxc_var_t* retval) {
    amxc_var_set(uint32_t, retval, 0);
    return amxd_status_ok;
}

amxd_status_t _SSID_CommitSSID(amxd_object_t* object _UNUSED,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args _UNUSED,
                               amxc_var_t* retval) {
    amxc_var_set(uint32_t, retval, 0);
    return amxd_status_ok;
}

/**
 * Write handler for MAC address settings.
 *
 * This function will retrieve the deviceType and devicePriority from the data model.
 * It will then call the plugin to update it's internal model with the change.
 *
 * Success or failure of plugin call is ignored.
 */
amxd_status_t _wld_ssid_setMacAddress_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiSsid = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiSsid) != amxd_object_instance) {
        return rv;
    }
    T_SSID* pSSID = (T_SSID*) wifiSsid->priv;
    rv = amxd_action_param_write(wifiSsid, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    const char* pMacAddr = amxc_var_constcast(cstring_t, args);

    ASSERT_NOT_NULL(pMacAddr, amxd_status_unknown_error, ME, "Mac address null");
    ASSERTW_FALSE(swl_str_matches(pMacAddr, WLD_EMPTY_MAC_ADDRESS), amxd_status_unknown_error, ME, "MAC address not valid !");

    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "SSID Object is NULL");
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    int32_t ret = 0;
    if(debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
        SAH_TRACEZ_INFO(ME, "[%s] Endpoint Mac Address : %s", pEP->Name, pMacAddr);
        ret = convStr2Mac(pSSID->MACAddress, ETHER_ADDR_LEN, (unsigned char*) pMacAddr, ETHER_ADDR_STR_LEN);
        ASSERTI_NOT_EQUALS(ret, 0, amxd_status_ok, ME, "INVALID [%s]", pMacAddr);
        pEP->pFA->mfn_wendpoint_set_mac_address(pEP);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    } else if(debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
        SAH_TRACEZ_INFO(ME, "[%s] Accesspoint Mac Address : %s", pAP->alias, pMacAddr);
        ret = convStr2Mac(pSSID->BSSID, ETHER_ADDR_LEN, (unsigned char*) pMacAddr, ETHER_ADDR_STR_LEN);
        memcpy(pSSID->MACAddress, pSSID->BSSID, ETHER_ADDR_LEN);
        ASSERTI_NOT_EQUALS(ret, 0, amxd_status_ok, ME, "INVALID [%s]", pMacAddr);
        pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) pMacAddr, ETHER_ADDR_STR_LEN, SET);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    SAH_TRACEZ_WARNING(ME, "Handler error !");
    return amxd_status_unknown_error;
}

T_SSID* wld_ssid_createApSsid(T_AccessPoint* pAP) {
    T_SSID* pSSID = calloc(1, sizeof(T_SSID));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");

    pSSID->RADIO_PARENT = pAP->pRadio;
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;

    pSSID->debug = SSID_POINTER;
    sprintf(pSSID->SSID, "NeMo_SSID%d", pAP->ref_index);

    return pSSID;
}

int32_t wld_ssid_initObjAp(T_SSID* pSSID, amxd_object_t* instance_object) {
    ASSERT_NOT_NULL(instance_object, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pSSID, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pSSID->AP_HOOK, WLD_ERROR, ME, "NULL");

    swl_str_copy(pSSID->Name, sizeof(pSSID->Name), pSSID->AP_HOOK->name);

    instance_object->priv = pSSID;
    amxd_object_set_cstring_t(instance_object, "Name", pSSID->AP_HOOK->name);
    amxd_object_set_cstring_t(instance_object, "Alias", pSSID->AP_HOOK->alias);
    pSSID->pBus = instance_object;
    return WLD_OK;
}
