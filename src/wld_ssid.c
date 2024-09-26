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
#include "swl/swl_common.h"
#include "swl/swl_assert.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld/Utils/wld_autoNeighAdd.h"

#define ME "ssid"

static char* SSID_SupStatus[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};

static amxc_llist_t sSsidList = {NULL, NULL};

static const char* wld_autoMacSrc_str[WLD_AUTOMACSRC_MAX] = {"Dummy", "Radio"};

static void s_setEnable_internal(T_SSID* pSSID, bool enable) {
    ASSERTS_NOT_EQUALS(pSSID->enable, enable, , ME, "same value");
    SAH_TRACEZ_INFO(ME, "%s: SSID Enable %u -> %u", pSSID->Name, pSSID->enable, enable);
    pSSID->enable = enable;
    if(enable) {
        pSSID->changeInfo.nrEnables++;
        pSSID->changeInfo.lastEnableTime = swl_time_getMonoSec();
    } else {
        pSSID->changeInfo.lastDisableTime = swl_time_getMonoSec();
    }
}

static void s_syncEnable (amxp_timer_t* timer _UNUSED, void* priv) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = (T_SSID*) priv;
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;

    amxd_object_t* pTgtObj = NULL;
    bool tgtEnable;
    if(pSSID->syncEnableToIntf) {
        if(pAP) {
            /* Sync AP object values */
            pTgtObj = pAP->pBus;
            tgtEnable = pSSID->enable;
        } else if(pEP) {
            /* Sync Endpoint object values */
            pTgtObj = pEP->pBus;
            tgtEnable = pSSID->enable;
        }
    } else {
        if(pAP) {
            /* Sync AP object values */
            pTgtObj = pSSID->pBus;
            tgtEnable = pAP->enable;
        } else if(pEP) {
            /* Sync Endpoint object values */
            pTgtObj = pSSID->pBus;
            tgtEnable = pEP->enable;
        }
        if(pTgtObj != NULL) {
            /* set internal enable, so that writer handler can ignore next
             * change notification */
            s_setEnable_internal(pSSID, tgtEnable);
        }
    }
    if(pTgtObj != NULL) {
        swl_typeUInt8_commitObjectParam(pTgtObj, "Enable", tgtEnable);
    }
    SAH_TRACEZ_OUT(ME);
}

T_SSID* s_createSsid(const char* name, uint32_t id) {
    ASSERT_STR(name, NULL, ME, "Empty name");
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = calloc(1, sizeof(T_SSID));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->debug = SSID_POINTER;
    swl_str_copy(pSSID->Name, sizeof(pSSID->Name), name);
    snprintf(pSSID->SSID, SSID_NAME_LEN, "PWHM_SSID%d", id);
    amxc_llist_append(&sSsidList, &pSSID->it);
    amxp_timer_new(&pSSID->enableSyncTimer, s_syncEnable, pSSID);
    pSSID->enable = 0;
    swl_timeMono_t now = swl_time_getMonoSec();
    pSSID->changeInfo.lastDisableTime = now;
    pSSID->changeInfo.lastStatusChange = now;
    pSSID->mldUnit = -1;
    SAH_TRACEZ_INFO(ME, "created ssid(%s) ctx(%p) id(%d)", name, pSSID, id);
    SAH_TRACEZ_OUT(ME);
    return pSSID;
}

static T_SSID* s_findSsid(amxd_object_t* object) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(pSSID->pBus == object) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* s_createSsidFromObj(amxd_object_t* obj) {
    ASSERT_NOT_NULL(obj, NULL, ME, "NULL");
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = s_findSsid(obj);
    ASSERTI_NULL(pSSID, pSSID, ME, "obj %p has already internal ctx %p", obj, pSSID);
    pSSID = s_createSsid(amxd_object_get_name(obj, AMXD_OBJECT_NAMED), amxd_object_get_index(obj));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->pBus = obj;
    obj->priv = pSSID;
    SAH_TRACEZ_OUT(ME);
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

T_SSID* wld_ssid_fromObj(amxd_object_t* ssidObj) {
    ASSERTS_EQUALS(amxd_object_get_type(ssidObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(ssidObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "SSID"), NULL, ME, "invalid parent obj(%s)", parentName);
    T_SSID* pSSID = (T_SSID*) ssidObj->priv;
    ASSERTS_TRUE(pSSID, NULL, ME, "NULL");
    ASSERT_TRUE(debugIsSsidPointer(pSSID), NULL, ME, "INVALID");
    return pSSID;
}

static void s_clearApSSIDRef(void* param) {
    ASSERTS_NOT_NULL(param, , ME, "NULL");
    uint32_t vapInstIdx = (uint32_t) ((intptr_t) param);
    ASSERTS_TRUE(vapInstIdx > 0, , ME, "invalid");
    SAH_TRACEZ_IN(ME);
    amxd_object_t* pApTmplObj = amxd_object_get(get_wld_object(), "AccessPoint");
    amxd_object_t* pApObj = amxd_object_get_instance(pApTmplObj, NULL, vapInstIdx);
    ASSERTW_NOT_NULL(pApObj, , ME, "vap instance idx(%d) not found", vapInstIdx);
    swl_typeCharPtr_commitObjectParam(pApObj, "SSIDReference", "");
    SAH_TRACEZ_OUT(ME);
}

static void s_clearEpSSIDRef(void* param) {
    ASSERTS_NOT_NULL(param, , ME, "NULL");
    uint32_t epInstIdx = (uint32_t) ((intptr_t) param);
    ASSERTS_TRUE(epInstIdx > 0, , ME, "invalid");
    SAH_TRACEZ_IN(ME);
    amxd_object_t* pEpTmplObj = amxd_object_get(get_wld_object(), "EndPoint");
    amxd_object_t* pEpObj = amxd_object_get_instance(pEpTmplObj, NULL, epInstIdx);
    ASSERTW_NOT_NULL(pEpObj, , ME, "ep instance idx(%d) not found", epInstIdx);
    swl_typeCharPtr_commitObjectParam(pEpObj, "SSIDReference", "");
    SAH_TRACEZ_OUT(ME);
}

static void s_cleanSSID(T_SSID* pSSID, bool direct) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: destroy SSID", pSSID->Name);
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
    if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
        uint32_t vapInstIdx = amxd_object_get_index(pAP->pBus);
        if(direct || !vapInstIdx) {
            //clear SSID Reference of AccessPoint
            pAP->pSSID = NULL;
        } else {
            void* param = (void*) ((intptr_t) vapInstIdx);
            swla_delayExec_add(s_clearApSSIDRef, param);
        }
    } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
        uint32_t epInstIdx = amxd_object_get_index(pEP->pBus);
        if(direct || !epInstIdx) {
            //clear SSID Reference of EndPoint
            pEP->pSSID = NULL;
        } else {
            void* param = (void*) ((intptr_t) epInstIdx);
            swla_delayExec_add(s_clearEpSSIDRef, param);
        }
    }
    amxc_llist_it_take(&pSSID->it);
    if(pSSID->pBus != NULL) {
        pSSID->pBus->priv = NULL;
    }
    free(pSSID);
}

/* Be sure that our attached memory structure is cleared */
static void s_destroySsid(amxd_object_t* object) {
    T_SSID* pSSID = wld_ssid_fromObj(object);
    s_cleanSSID(pSSID, false);
}

void wld_ssid_cleanAll() {
    amxc_llist_it_t* it = amxc_llist_get_first(&sSsidList);
    while(it != NULL) {
        T_SSID* pSSID = amxc_llist_it_get_data(it, T_SSID, it);
        s_cleanSSID(pSSID, true);
        it = amxc_llist_get_first(&sSsidList);
    }
}

amxd_status_t _wld_ssid_addInstance_oaf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_IN(ME);

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

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ssid_delInstance_odf(amxd_object_t* object,
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
    s_destroySsid(object);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ssid_setLowerLayers_pwf(amxd_object_t* object,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval,
                                           void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    ASSERT_EQUALS(rv, amxd_status_ok, rv, ME, "fail to set value rv:%d", rv);
    const char* lowerLayer = amxc_var_constcast(cstring_t, args);
    ASSERT_NOT_NULL(lowerLayer, amxd_status_ok, ME, "NULL");
    T_SSID* pSSID = wld_ssid_fromObj(object);
    if((pSSID == NULL) && (!swl_str_isEmpty(lowerLayer))) {
        pSSID = s_createSsidFromObj(object);
    }
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");

    T_Radio* pRad = (T_Radio*) swla_object_getReferenceObjectPriv(object, lowerLayer);
    if(pRad != NULL) {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has radio LowerLayer (%s) and refers to pRad(%p:%s)",
                        pSSID->Name, lowerLayer, pRad, pRad->Name);
    } else {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has no identified radio LowerLayer (%s)",
                        pSSID->Name, lowerLayer);
    }
    pSSID->RADIO_PARENT = pRad;
    pSSID->autoMacSrc = WLD_AUTOMACSRC_RADIO_BASE;

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static const char* s_getAutoMacSrcName(uint32_t srcId) {
    ASSERT_TRUE(srcId < SWL_ARRAY_SIZE(wld_autoMacSrc_str), "Invalid", ME, "invalid id %d", srcId);
    return wld_autoMacSrc_str[srcId];
}

void wld_ssid_generateMac(T_Radio* pRad, T_SSID* pSSID, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(pRad, , ME, "%s: No mapped radio", pSSID->Name);
    T_AccessPoint* pAP = pSSID->AP_HOOK;
    T_EndPoint* pEP = pSSID->ENDP_HOOK;
    ASSERT_FALSE((pAP == NULL) && (pEP == NULL), , ME, "%s: No mapped AP/EP", pSSID->Name);
    swl_rc_ne rc = SWL_RC_ERROR;
    const char* ifname = (pAP != NULL) ? pAP->alias : pEP->Name;
    const char* macType = (pAP != NULL) ? "AP BSSID" : "EP MAC";
    const char* macSrc = s_getAutoMacSrcName(pSSID->autoMacSrc);

    switch(pSSID->autoMacSrc) {
    case WLD_AUTOMACSRC_RADIO_BASE: {
        if(pAP != NULL) {
            rc = wld_rad_macCfg_generateBssid(pRad, ifname, index, macBin);
        } else {
            rc = wld_rad_macCfg_generateEpMac(pRad, ifname, index, macBin);
        }
        break;
    }
    case WLD_AUTOMACSRC_DUMMY: {
        ASSERT_NULL(pEP, , ME, "MUST Not generate dummy MAC address for Endpoint %s interface", ifname);
        rc = wld_rad_macCfg_generateDummyBssid(pRad, ifname, index, macBin);
        break;
    }
    default:
        break;
    }
    ASSERT_TRUE(swl_rc_isOk(rc), , ME, "%s: fail to generate %s %s src %s",
                pSSID->Name, ifname, macType, macSrc);
    SAH_TRACEZ_INFO(ME, "%s: gen %s %s src %s rank(%d): "SWL_MAC_FMT,
                    pRad->Name, ifname, macType, macSrc, index, SWL_MAC_ARG(macBin->bMac));
    pSSID->bssIndex = index;
}

void wld_ssid_generateBssid(T_Radio* pRad, T_AccessPoint* pAP, uint32_t apIndex, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_ssid_generateMac(pRad, pAP->pSSID, apIndex, macBin);
}

void wld_ssid_setMac(T_SSID* pSSID, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(macBin, , ME, "NULL");
    memcpy(pSSID->MACAddress, macBin->bMac, sizeof(pSSID->MACAddress));
}

void wld_ssid_setBssid(T_SSID* pSSID, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(macBin, , ME, "NULL");
    memcpy(pSSID->BSSID, macBin->bMac, ETHER_ADDR_LEN);
    wld_ssid_setMac(pSSID, macBin);
}

T_SSID* wld_ssid_getSsidByBssid(swl_macBin_t* macBin) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(SWL_MAC_BIN_MATCHES(pSSID->BSSID, macBin)) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* wld_ssid_getSsidByMacAddress(swl_macBin_t* macBin) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(SWL_MAC_BIN_MATCHES(pSSID->MACAddress, macBin)) {
            return pSSID;
        }
    }
    return NULL;
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

static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    bool newEnable = amxc_var_get_bool(newValue);
    if(pSSID->enable == newEnable) {
        return;
    }
    s_setEnable_internal(pSSID, newEnable);
    wld_ssid_syncEnable(pSSID, true);

    SAH_TRACEZ_OUT(ME);
}


static void s_setMLDUnit_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    int32_t newMldUnit = amxc_var_get_int32_t(newValue);
    if(pSSID->mldUnit == newMldUnit) {
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: SET MLD_UNIT %d %d", pSSID->Name, pSSID->mldUnit, newMldUnit);

    pSSID->mldUnit = newMldUnit;
    if(pSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        pAP->pFA->mfn_wvap_setMldUnit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
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

    if(debugIsEpPointer(pEP)) {

        T_EndPointStats epStats;
        memset(&epStats, 0, sizeof(epStats));
        if(pEP->pFA->mfn_wendpoint_stats(pEP, &epStats) < SWL_RC_OK) {
            /* Update the stats with Linux counters if we don't handle them in the vendor plugin. */
            wld_updateEPStats(pEP, NULL);
        } else {
            s_copyEpStats(&pSSID->stats, &epStats);
        }

    } else if(debugIsVapPointer(pAP)) {

        if(pAP->pFA->mfn_wvap_update_ap_stats(pAP) < SWL_RC_OK) {
            /* Update the stats with Linux counters if we don't handle them in the vendor plugin. */
            wld_updateVAPStats(pAP, NULL);
        }

    } else {
        SAH_TRACEZ_INFO(ME, "%s: no mapped AP/EP", pSSID->Name);
        return amxd_status_unknown_error;
    }
    ASSERTS_NOT_NULL(stats, amxd_status_ok, ME, "Nothing to copy, only refresh");
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
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
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

static void s_setSSID_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");

    char* SSID = amxc_var_dyncast(cstring_t, newValue);
    ASSERT_NOT_NULL(SSID, , ME, "NULL");
    if(swl_str_matches(SSID, pSSID->SSID)) {
        SAH_TRACEZ_INFO(ME, "%s: same SSID %s", pSSID->Name, SSID);
        free(SSID);
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: set SSID %s", pSSID->Name, SSID);

    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    if(pAP) {
        SAH_TRACEZ_INFO(ME, "%s: apply to AP %p", pSSID->Name, pAP);
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) SSID, strlen(SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
        wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, true);
    }
    free(SSID);
    //Setting endpoint ssid should only be done internally => no change necessary here.

    SAH_TRACEZ_OUT(ME);
}

/**
 * Write handler for MAC address settings.
 */
static void s_setMacAddress_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    const char* pMacStr = amxc_var_constcast(cstring_t, newValue);
    swl_macBin_t mac = SWL_MAC_BIN_NEW();
    if((SWL_MAC_CHAR_TO_BIN(&mac, pMacStr)) &&
       (!SWL_MAC_BIN_MATCHES(pSSID->MACAddress, &mac))) {
        memcpy(pSSID->MACAddress, mac.bMac, ETHER_ADDR_LEN);
        T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
        T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
        if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Accesspoint Mac Address : %s", pAP->alias, pMacStr);
            memcpy(pSSID->BSSID, mac.bMac, ETHER_ADDR_LEN);
            pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) pMacStr, ETHER_ADDR_STR_LEN, SET);
            wld_autoCommitMgr_notifyVapEdit(pAP);
        } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Endpoint Mac Address : %s", pEP->Name, pMacStr);
            pEP->pFA->mfn_wendpoint_set_mac_address(pEP);
            wld_autoCommitMgr_notifyEpEdit(pEP);
        }
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setCustomNetDevName_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    const char* custNetDevName = amxc_var_constcast(cstring_t, newValue);
    ASSERTS_FALSE(swl_str_matches(pSSID->customNetDevName, custNetDevName), , ME, "same value");
    swl_str_copy(pSSID->customNetDevName, sizeof(pSSID->customNetDevName), custNetDevName);
    int32_t ifIndex = pSSID->AP_HOOK ? pSSID->AP_HOOK->index : (pSSID->ENDP_HOOK ? pSSID->ENDP_HOOK->index : -1);
    SAH_TRACEZ_INFO(ME, "%s: SET CustomNetDevName %s (ndIdx %u)",
                    pSSID->Name, pSSID->customNetDevName, ifIndex);
    if(ifIndex > 0) {
        SAH_TRACEZ_WARNING(ME, "%s: interface %d already created: new custom ifname %s will be applied on next boot",
                           pSSID->Name, ifIndex, custNetDevName);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sSsidDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("SSID", s_setSSID_pwf),
                  SWLA_DM_PARAM_HDLR("CustomNetDevName", s_setCustomNetDevName_pwf),
                  SWLA_DM_PARAM_HDLR("MACAddress", s_setMacAddress_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MLDUnit", s_setMLDUnit_pwf)
                  ));

void _wld_ssid_setConf_ocf(const char* const sig_name,
                           const amxc_var_t* const data,
                           void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sSsidDmHdlrs, sig_name, data, priv);
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
        if(pEP != NULL) {
            // Only need to set SSID if Endpoint. In case of accespoint, this is incoming config.
            amxd_trans_set_cstring_t(&trans, "SSID", pS->SSID);
        }


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
        } else if(pEP) {
            err = pEP->pFA->mfn_wendpoint_bssid(pEP, (swl_macChar_t*) TBuf);
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
        if(pEP && pEP->pSSID){
            amxd_trans_set_uint32_t(&trans, "Multi_ap_profile", pEP->pSSID->Multi_ap_profile);
		      amxd_trans_set_uint32_t(&trans, "Multi_ap_primary_vlanid", pEP->pSSID->Multi_ap_primary_vlanid);
		  }

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
        amxd_trans_set_uint32_t(&trans, "Index", ifIndex);

        wld_util_initCustomAlias(&trans, object);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pS->Name);
    } else {
        /* Get SSID data from OBJ to pSSID */
        s_setSSID_pwf(NULL, object,
                      amxd_object_get_param_def(object, "SSID"),
                      amxd_object_get_param_value(object, "SSID"));

        s_setMacAddress_pwf(NULL, object,
                            amxd_object_get_param_def(object, "MACAddress"),
                            amxd_object_get_param_value(object, "MACAddress"));

        if((amxp_timer_get_state(pS->enableSyncTimer) != amxp_timer_started) &&
           (amxp_timer_get_state(pS->enableSyncTimer) != amxp_timer_running)) {
            s_setEnable_pwf(NULL, object,
                            amxd_object_get_param_def(object, "Enable"),
                            amxd_object_get_param_value(object, "Enable"));
        }
    }
    SAH_TRACEZ_OUT(ME);
}

void wld_ssid_setStatus(T_SSID* pSSID, wld_status_e status, bool commit) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    ASSERTI_NOT_EQUALS(pSSID->status, status, , ME, "%s: same status %u", pSSID->Name, status);
    pSSID->changeInfo.lastStatusChange = swl_time_getMonoSec();
    pSSID->changeInfo.nrStatusChanges++;
    pSSID->status = status;

    if(commit) {
        ASSERTI_NOT_NULL(pSSID->pBus, , ME, "%s: not dm configurable", pSSID->Name);
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
        amxd_trans_set_value(cstring_t, &trans, "Status", Rad_SupStatus[pSSID->status]);
        swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pSSID->changeInfo.lastStatusChange);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pSSID->Name);
    }
}


amxd_status_t _SSID_getStatusHistogram(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, amxd_status_unknown_error, ME, "invalid SSID");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    swl_type_toVariant((swl_type_t*) &gtWld_status_changeInfo, retval, &pSSID->changeInfo);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ssid_getLastChange_prf(amxd_object_t* object,
                                          amxd_param_t* param,
                                          amxd_action_t reason,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval,
                                          void* priv _UNUSED) {
    ASSERTS_NOT_NULL(param, amxd_status_unknown_error, ME, "NULL");
    ASSERTS_EQUALS(reason, action_param_read, amxd_status_function_not_implemented, ME, "not impl");
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERTS_NOT_NULL(pSSID, amxd_status_ok, ME, "no SSID mapped");
    uint32_t lastChange = swl_time_getMonoSec() - pSSID->changeInfo.lastStatusChange;
    amxc_var_set(uint32_t, retval, lastChange);
    return amxd_status_ok;
}
