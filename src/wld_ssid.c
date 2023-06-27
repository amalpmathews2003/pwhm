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
#include "wld_dm_trans.h"
#include "swl/swl_string.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "ssid"

static char* SSID_SupStatus[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};

static amxc_llist_t sSsidList = {NULL, NULL};

static void s_syncEnable (amxp_timer_t* timer _UNUSED, void* priv) {
    T_SSID* pSSID = (T_SSID*) priv;
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;

    if(pSSID->syncEnableToIntf) {
        if(pAP) {
            /* Sync AP object values */
            amxd_object_set_bool(pAP->pBus, "Enable", pSSID->enable);
        } else if(pEP) {
            /* Sync Endpoint object values */
            amxd_object_set_bool(pEP->pBus, "Enable", pSSID->enable);
        }
    } else {
        if(pAP) {
            /* Sync AP object values */
            amxd_object_set_bool(pSSID->pBus, "Enable", pAP->enable);
        } else if(pEP) {
            /* Sync Endpoint object values */
            amxd_object_set_bool(pSSID->pBus, "Enable", pEP->enable);
        }
    }

}


T_SSID* s_createSsid(const char* name, uint32_t id) {
    ASSERT_STR(name, NULL, ME, "Empty name");
    T_SSID* pSSID = calloc(1, sizeof(T_SSID));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->debug = SSID_POINTER;
    swl_str_copy(pSSID->Name, sizeof(pSSID->Name), name);
    snprintf(pSSID->SSID, SSID_NAME_LEN, "PWHM_SSID%d", id);
    amxc_llist_append(&sSsidList, &pSSID->it);
    amxp_timer_new(&pSSID->enableSyncTimer, s_syncEnable, pSSID);
    pSSID->enable = 0;
    pSSID->changeInfo.lastDisableTime = swl_time_getMonoSec();
    return pSSID;
}

T_SSID* s_createSsidFromObj(amxd_object_t* obj) {
    ASSERT_NOT_NULL(obj, NULL, ME, "NULL");
    T_SSID* pSSID = s_createSsid(amxd_object_get_name(obj, AMXD_OBJECT_NAMED), amxd_object_get_index(obj));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->pBus = obj;
    obj->priv = pSSID;
    return pSSID;
}

T_SSID* wld_ssid_createApSsid(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");
    T_SSID* pSSID = s_createSsid(pAP->name, pAP->ref_index);
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->RADIO_PARENT = pAP->pRadio;
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;
    return pSSID;
}


static void s_cleanSSID(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: destroy SSID", pSSID->Name);
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
    if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
        //clear SSID Reference of AccessPoint
        pAP->pSSID = NULL;
    } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
        //clear SSID Reference of EndPoint
        pEP->pSSID = NULL;
    }
    amxc_llist_it_take(&pSSID->it);
    if(pSSID->pBus != NULL) {
        pSSID->pBus->priv = NULL;
    }
    free(pSSID);
}

/* Be sure that our attached memory structure is cleared */
static void s_destroySsid(amxd_object_t* object) {
    T_SSID* pSSID = (T_SSID*) object->priv;
    ASSERT_TRUE(debugIsSsidPointer(pSSID), , ME, "INVALID");
    s_cleanSSID(pSSID);
}

void wld_ssid_cleanAll() {
    amxc_llist_it_t* it = amxc_llist_get_first(&sSsidList);
    while(it != NULL) {
        T_SSID* pSSID = amxc_llist_it_get_data(it, T_SSID, it);
        s_cleanSSID(pSSID);
        it = amxc_llist_get_first(&sSsidList);
    }
}

amxd_status_t _wld_ssid_addInstance_ocf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {

    char* path = amxd_object_get_path(object, AMXD_OBJECT_NAMED);
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s:%s)",
                    object, name, path);
    free(path);
    amxd_status_t status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERTI_NOT_EQUALS(status, amxd_status_duplicate, status, ME, "override instance (%p:%s)", object, name);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance %s (status %d)", name, status);
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    s_createSsidFromObj(instance);
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
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    char* lowerLayer = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(lowerLayer, amxd_status_ok, ME, "NULL");
    T_Radio* pRad = (T_Radio*) swla_object_getReferenceObjectPriv(object, lowerLayer);
    if(pRad != NULL) {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has radio LowerLayer (%s) and refers to pRad(%p:%s)",
                        pSSID->Name, lowerLayer, pRad, pRad->Name);
        //sync potentially missed conf params on boot
        pRad->pFA->mfn_sync_radio(pRad->pBus, pRad, GET);
    } else {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has no identified radio LowerLayer (%s)",
                        pSSID->Name, lowerLayer);
    }
    free(lowerLayer);
    pSSID->RADIO_PARENT = pRad;
    return amxd_status_ok;
}

void wld_ssid_syncEnable(T_SSID* pSSID, bool syncToIntf) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: do sync %u", pSSID->Name, syncToIntf);
    pSSID->syncEnableToIntf = syncToIntf;
    if((amxp_timer_get_state(pSSID->enableSyncTimer) == amxp_timer_started) ||
       (amxp_timer_get_state(pSSID->enableSyncTimer) == amxp_timer_running)) {
        return;
    }

    amxp_timer_start(pSSID->enableSyncTimer, 1);
}

void wld_ssid_setEnable(T_SSID* pSSID, bool newEnable) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: SET ENABLE %u %u", pSSID->Name, pSSID->enable, newEnable);
    if(pSSID->enable == newEnable) {
        return;
    }
    pSSID->enable = newEnable;
    if(newEnable) {
        pSSID->changeInfo.nrEnables++;
        pSSID->changeInfo.lastEnableTime = swl_time_getMonoSec();
    } else {
        pSSID->changeInfo.lastDisableTime = swl_time_getMonoSec();
    }

    wld_ssid_syncEnable(pSSID, true);
}

amxd_status_t _wld_ssid_setEnable_pwf(amxd_object_t* object,
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

    bool newEnable = amxc_var_get_bool(args);

    wld_ssid_setEnable(pSSID, newEnable);
    SAH_TRACEZ_OUT(ME);

    return amxd_status_ok;
}

static void s_copyEpStats(T_Stats* pStats, T_EndPointStats* pEpStats) {
    ASSERTS_NOT_NULL(pStats, , ME, "NULL");
    ASSERTS_NOT_NULL(pEpStats, , ME, "NULL");
    pStats->BytesSent = pEpStats->txbyte;
    pStats->BytesReceived = pEpStats->rxbyte;
    pStats->PacketsSent = pEpStats->txPackets;
    pStats->PacketsReceived = pEpStats->rxPackets;
    pStats->RetransCount = pEpStats->Retransmissions;
    pStats->RetryCount = pEpStats->txRetries + pEpStats->rxRetries;
    pStats->noise = pEpStats->noise;
}

static amxd_status_t s_updateSsidStatsValues(T_SSID* pSSID, amxd_object_t* stats) {
    ASSERTS_NOT_NULL(pSSID, amxd_status_invalid_value, ME, "NULL");
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;

    /* Update the stats with Linux counters if we don't handle them in the vendor plugin.
       TRAP (-1) was called ? */
    if(debugIsEpPointer(pEP)) {
        SAH_TRACEZ_INFO(ME, "Endpoint SSID = %s", pSSID->SSID);

        T_EndPointStats epStats;
        memset(&epStats, 0, sizeof(epStats));
        if(pEP->pFA->mfn_wendpoint_stats(pEP, &epStats) == SWL_RC_OK) {
            wld_updateEPStats(pEP, NULL);
            s_copyEpStats(&pSSID->stats, &epStats);
        }

    } else if(debugIsVapPointer(pAP)) {
        SAH_TRACEZ_INFO(ME, "Accesspoint SSID = %s", pSSID->SSID);

        if(pAP->pFA->mfn_wvap_update_ap_stats(pAP) < 0) {
            wld_updateVAPStats(pAP, NULL);
        }

    } else {
        SAH_TRACEZ_INFO(ME, "invalid point");
        return amxd_status_unknown_error;
    }
    ASSERTS_NOT_NULL(stats, amxd_status_ok, ME, "obj NULL");
    return wld_util_stats2Obj(stats, &pSSID->stats);
}

amxd_status_t _SSID_getSSIDStats(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {
    SAH_TRACEZ_INFO(ME, "getSSIDStats");

    amxd_status_t status = amxd_status_ok;
    T_SSID* pSSID = (T_SSID*) object->priv;

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_ERROR(ME, "Invalid pointer !");
        return amxd_status_ok;
    }

    amxd_object_t* stats = amxd_object_get(object, "Stats");
    status = s_updateSsidStatsValues(pSSID, stats);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "fail to update stats");
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_statsObj2Var(retval, stats);

    return amxd_status_ok;
}

amxd_status_t _wld_ssid_getStats_orf(amxd_object_t* const object,
                                     amxd_param_t* const param,
                                     amxd_action_t reason,
                                     const amxc_var_t* const args,
                                     amxc_var_t* const action_retval,
                                     void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_ok;
    if(reason != action_object_read) {
        status = amxd_status_function_not_implemented;
        return status;
    }

    T_SSID* pSSID = (T_SSID*) amxd_object_get_parent(object)->priv;

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_INFO(ME, "SSID not present !");
        return amxd_status_unknown_error;
    }

    s_updateSsidStatsValues(pSSID, object);

    status = amxd_action_object_read(object, param, reason, args, action_retval, priv);
    return status;
}


/**
 * check the SSID object block on wrong input.
 * Currently only SSID is checked. (All other are accept by default).
 */
amxd_status_t _wld_ssid_validateSSID_pvf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* param,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    if(swl_str_matches(currentValue, newValue) || isValidSSID(newValue)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "invalid SSID(%s)", newValue);
    }
    free(newValue);
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
    ASSERT_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    ASSERT_TRUE(debugIsSsidPointer(pSSID), amxd_status_unknown_error, ME, "Invalid SSID Ctx");

    char* SSID = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(SSID, amxd_status_invalid_value, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "set SSID %s", SSID);

    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    if(pAP) {
        SAH_TRACEZ_INFO(ME, "apply to AP %p", pAP);
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) SSID, strlen(SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }
    free(SSID);
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

        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pS->Name);

        /* Set SSID data in mapped OBJ structure */

        /** 'Status' The current operational state of the SSID entry. */
        amxd_trans_set_cstring_t(&trans, "Status", SSID_SupStatus[pS->status]);
        swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pS->changeInfo.lastStatusChange);

        /** 'SSID' The current service set identifier in use by the
         *  connection. The SSID is an identifier that is attached to
         *  packets sent over the wireless LAN that functions as an
         *  ID for joining a particular radio network (BSS). */
        amxd_trans_set_cstring_t(&trans, "SSID", pS->SSID);
        /** 'MACAddress' The MAC address of this interface.  */
        sprintf(TBuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
                pS->MACAddress[0], pS->MACAddress[1], pS->MACAddress[2],
                pS->MACAddress[3], pS->MACAddress[4], pS->MACAddress[5]);
        amxd_trans_set_cstring_t(&trans, "MACAddress", TBuf);
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
            swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
            err = pEP->pFA->mfn_wendpoint_bssid(pEP, &macChar);
            swl_str_copy(TBuf, sizeof(TBuf), macChar.cMac);
            wld_autoCommitMgr_notifyEpEdit(pEP);
        } else {
            err = WLD_ERROR_NOT_IMPLEMENTED;
        }
        if(err < 0) {
            snprintf(TBuf, sizeof(TBuf), WLD_EMPTY_MAC_ADDRESS);
            memset(pS->BSSID, 0, ETHER_ADDR_LEN);
        } else {
            wldu_convStr2Mac(pS->BSSID, ETHER_ADDR_LEN, (char*) TBuf, ETHER_ADDR_STR_LEN);
        }
        amxd_trans_set_cstring_t(&trans, "BSSID", TBuf);
        TBuf[0] = 0;
        int32_t ifIndex = 0;
        if(pAP != NULL) {
            swl_str_copy(TBuf, sizeof(TBuf), pAP->alias);
            ifIndex = pAP->index;
        } else if(pEP != NULL) {
            swl_str_copy(TBuf, sizeof(TBuf), pEP->Name);
            ifIndex = pEP->index;
        }
        amxd_trans_set_cstring_t(&trans, "Name", TBuf);
        amxd_trans_set_int32_t(&trans, "Index", ifIndex);

        ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), , ME, "%s : trans apply failure", pS->Name);
    } else {
        /* Get AP data from OBJ to AP */
        bool tmpEnable = amxd_object_get_bool(object, "Enable", NULL);
        wld_ssid_setEnable(pS, tmpEnable);


        char* ssid = amxd_object_get_cstring_t(object, "SSID", NULL);
        if(strncmp(pS->SSID, ssid, strlen(pS->SSID))) {
            if(isValidSSID(ssid)) {
                swl_str_copy(pS->SSID, sizeof(pS->SSID), ssid);
                if(pAP) {
                    pAP->pFA->mfn_wvap_ssid(pAP, pS->SSID, strlen(pS->SSID), set);
                    wld_autoCommitMgr_notifyVapEdit(pAP);
                }
            }
        }
        free(ssid);
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
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiSsid = object;
    ASSERTI_EQUALS(amxd_object_get_type(wifiSsid), amxd_object_instance, rv, ME, "Not instance");
    T_SSID* pSSID = (T_SSID*) wifiSsid->priv;
    ASSERT_TRUE(debugIsSsidPointer(pSSID), amxd_status_unknown_error, ME, "Invalid SSID Ctx");
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    ASSERT_EQUALS(rv, amxd_status_ok, rv, ME, "ERR status:%d", rv);

    char* pMacStr = amxc_var_dyncast(cstring_t, args);
    ASSERT_STR(pMacStr, amxd_status_unknown_error, ME, "Mac address empty");
    swl_macBin_t mac = SWL_MAC_BIN_NEW();
    if(!SWL_MAC_CHAR_TO_BIN(&mac, pMacStr)) {
        SAH_TRACEZ_ERROR(ME, "Invalid mac address (%s)", pMacStr);
        rv = amxd_status_invalid_value;
    } else if(!SWL_MAC_BIN_MATCHES(pSSID->MACAddress, &mac)) {
        memcpy(pSSID->MACAddress, mac.bMac, ETHER_ADDR_LEN);
        T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
        T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
        if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Accesspoint Mac Address : %s", pAP->alias, pMacStr);
            memcpy(pSSID->BSSID, mac.bMac, ETHER_ADDR_LEN);
            pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) pMacStr, ETHER_ADDR_STR_LEN, SET);
        } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Endpoint Mac Address : %s", pEP->Name, pMacStr);
            pEP->pFA->mfn_wendpoint_set_mac_address(pEP);
        }
        rv = amxd_status_ok;
    }
    free(pMacStr);
    return rv;
}

void wld_ssid_setStatus(T_SSID* pSSID, wld_status_e status, bool commit) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    ASSERTI_NOT_EQUALS(pSSID->status, status, , ME, "%s: same status %u", pSSID->Name, status);
    pSSID->changeInfo.lastStatusChange = swl_time_getMonoSec();
    pSSID->changeInfo.nrStatusChanges++;
    pSSID->status = status;

    if(commit) {
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
        amxd_trans_set_value(cstring_t, &trans, "Status", Rad_SupStatus[pSSID->status]);
        swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pSSID->changeInfo.lastStatusChange);
        ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), , ME, "%s : trans apply failure", pSSID->Name);
    }
}


amxd_status_t _SSID_getStatusHistogram(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = object->priv;

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    swl_type_toVariant((swl_type_t*) &gtWld_status_changeInfo, retval, &pSSID->changeInfo);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}
