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
#include <debug/sahtrace.h>
#include <assert.h>


#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "Utils/wld_autoCommitMgr.h"
#include "swl/swl_assert.h"

#define ME "apNeigh"

void debugPrintNeighbour(T_ApNeighbour* neigh) {
    SAH_TRACEZ_INFO(ME, "Neigh %s : %u %u %u %u %u", swl_typeMacBin_toBuf32Ref((swl_macBin_t* ) neigh->bssid).buf,
                    neigh->information, neigh->operatingClass, neigh->channel, neigh->phyType, neigh->colocatedAp);
}

static T_ApNeighbour* s_findNeigh(amxd_object_t* templateObj, const char* bssidStr, amxd_object_t* optExclObj) {
    amxd_object_for_each(instance, it, templateObj) {
        amxd_object_t* neighObj = amxc_container_of(it, amxd_object_t, it);
        if(optExclObj == neighObj) {
            continue;
        }
        char* entryBssid = amxd_object_get_cstring_t(neighObj, "BSSID", NULL);
        bool match = (!swl_str_isEmpty(entryBssid) && SWL_MAC_CHAR_MATCHES(entryBssid, bssidStr));
        free(entryBssid);
        if(match) {
            return neighObj->priv;
        }
    }
    return NULL;
}

T_ApNeighbour* wld_ap_findNeigh(T_AccessPoint* pAP, const char* bssidStr) {
    return s_findNeigh(amxd_object_get(pAP->pBus, "Neighbour"), bssidStr, NULL);
}

static void s_updateNeighbor (T_AccessPoint* pAP, T_ApNeighbour* neigh) {

    ASSERTI_FALSE(swl_mac_binIsNull((swl_macBin_t*) neigh->bssid), , ME, "empty bssid");
    pAP->pFA->mfn_wvap_updated_neighbour(pAP, neigh);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    SAH_TRACEZ_INFO(ME, "%s: Updated neigh", pAP->alias);
    debugPrintNeighbour(neigh);
}

static void s_removeNeighbor (T_AccessPoint* pAP, T_ApNeighbour* neigh) {

    ASSERTI_FALSE(swl_mac_binIsNull((swl_macBin_t*) neigh->bssid), , ME, "empty bssid");
    pAP->pFA->mfn_wvap_deleted_neighbour(pAP, neigh);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    SAH_TRACEZ_INFO(ME, "%s: Deleted neigh", pAP->alias);
    debugPrintNeighbour(neigh);
}

/**
 * Called when an neighbor instance is destroyed
 */
amxd_status_t _wld_ap_delete_neigh_odf(amxd_object_t* object,
                                       amxd_param_t* param,
                                       amxd_action_t reason,
                                       const amxc_var_t* const args,
                                       amxc_var_t* const retval,
                                       void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy neigh entry st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");

    T_ApNeighbour* neigh = (T_ApNeighbour*) object->priv;
    object->priv = NULL;
    ASSERTS_NOT_NULL(neigh, amxd_status_ok, ME, "No internal ctx");
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    amxc_llist_it_take(&(neigh->it));

    /* If a 6GHz BSS is removed, update discovery method */
    if(swl_chanspec_operClassToFreq(neigh->operatingClass) == SWL_FREQ_BAND_EXT_6GHZ) {
        wld_rad_updateDiscoveryMethod6GHz();
    }

    s_removeNeighbor(pAP, neigh);
    free(neigh);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/**
 * Called when a new neighbor instance is added.
 */
static void s_addNeighInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const intialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    T_ApNeighbour* neigh = calloc(1, sizeof(T_ApNeighbour));
    ASSERT_NOT_NULL(neigh, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Adding neigh", pAP->alias);
    amxc_llist_it_init(&neigh->it);
    amxc_llist_append(&pAP->neighbours, &neigh->it);
    object->priv = neigh;
    neigh->obj = object;

    SAH_TRACEZ_OUT(ME);
}

/**
 * Function call to create an exception.
 */
amxd_status_t _setNeighbourAP(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(obj);
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    const char* bssid = GET_CHAR(args, "BSSID");
    swl_macChar_t cBssidStd;
    ASSERT_TRUE(swl_mac_charToStandard(&cBssidStd, bssid), amxd_status_unknown_error, ME, "BSSID is invalid (%s)", bssid);

    amxd_trans_t trans;
    T_ApNeighbour* neigh = wld_ap_findNeigh(pAP, cBssidStd.cMac);

    if(neigh != NULL) {
        SAH_TRACEZ_INFO(ME, "Neighbor with bssid [%s] found in neighbour list", bssid);
        ASSERT_TRANSACTION_INIT(neigh->obj, &trans, amxd_status_unknown_error, ME, "%s : trans init failure", pAP->alias);
    } else {
        SAH_TRACEZ_INFO(ME, "Create new Neighbor with bssid [%s]", bssid);
        ASSERT_TRANSACTION_INIT(amxd_object_get(pAP->pBus, "Neighbour"), &trans, amxd_status_unknown_error, ME, "%s : trans init failure", pAP->alias);
        swl_macBin_t binBssid;
        swl_mac_charToBin(&binBssid, &cBssidStd);
        swl_mac_binToCharSep(&cBssidStd, &binBssid, false, '-');
        char name[SWL_MAC_CHAR_LEN + 1];
        snprintf(name, sizeof(name), "_%s", cBssidStd.cMac);
        amxd_trans_add_inst(&trans, 0, name);
    }

    SAH_TRACEZ_INFO(ME, "Updating neighbour object with provided arguments");
    const char* validArgs = "BSSID,SSID,Information,Channel,OperatingClass,PhyType,NASIdentifier,R0KHKey,ColocatedAP";
    amxc_var_for_each(newValue, args) {
        const char* pname = amxc_var_key(newValue);
        //skip arguments not matching instance's parameter names
        //to avoid transaction applying issue
        if(!swl_strlst_contains(validArgs, ",", pname)) {
            SAH_TRACEZ_WARNING(ME, "skip unknown argument %s", pname);
            continue;
        }
        //we assume known args and obj params types are matching
        amxd_trans_set_param(&trans, pname, newValue);
    }

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, amxd_status_unknown_error, ME, "%s : trans apply failure", pAP->alias);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _delNeighbourAP(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    T_AccessPoint* pAP = wld_ap_fromObj(obj);
    ASSERT_NOT_NULL(pAP, status, ME, "NULL");

    const char* bssid = GET_CHAR(args, "BSSID");
    swl_macChar_t cBssidStd;
    ASSERT_TRUE(swl_mac_charToStandard(&cBssidStd, bssid), status, ME, "BSSID is invalid (%s)", bssid);

    T_ApNeighbour* neigh = wld_ap_findNeigh(pAP, cBssidStd.cMac);
    ASSERT_NOT_NULL(neigh, status, ME, "Could not find neighbour with bssid %s", bssid);

    status = swl_object_delInstWithTransOnLocalDm(neigh->obj);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ap_validateNeighBssid_pvf(amxd_object_t* object,
                                             amxd_param_t* param _UNUSED,
                                             amxd_action_t reason _UNUSED,
                                             const amxc_var_t* const args,
                                             amxc_var_t* const retval _UNUSED,
                                             void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    amxd_object_t* tmplObj = amxd_object_get_parent(object);

    const char* currBssidStr = amxc_var_constcast(cstring_t, &param->value);
    const char* newBssidStr = amxc_var_constcast(cstring_t, args);
    if(swl_str_isEmpty(newBssidStr) || SWL_MAC_CHAR_MATCHES(newBssidStr, currBssidStr)) {
        return amxd_status_ok;
    }
    T_ApNeighbour* duplNeigh = s_findNeigh(tmplObj, newBssidStr, object);
    if(duplNeigh != NULL) {
        SAH_TRACEZ_ERROR(ME, "neigh entry[%d] bssid (%s) already exist", amxd_object_get_index(duplNeigh->obj), newBssidStr);
    } else {
        status = amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setNeighConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    T_ApNeighbour* neigh = object->priv;
    ASSERT_NOT_NULL(neigh, , ME, "NULL");

    s_removeNeighbor(pAP, neigh);

    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "BSSID")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour  BSSID = %s", valStr);
            swl_typeMacBin_fromChar((swl_macBin_t*) neigh->bssid, valStr);
        } else if(swl_str_matches(pname, "SSID")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour  SSID = %s", valStr);
            swl_str_copy(neigh->ssid, SSID_NAME_LEN, valStr);
        } else if(swl_str_matches(pname, "Information")) {
            neigh->information = amxc_var_dyncast(int32_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour BSSID Info = %d", neigh->information);
        } else if(swl_str_matches(pname, "OperatingClass")) {
            neigh->operatingClass = amxc_var_dyncast(int32_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour  operatingClass = %d", neigh->operatingClass);
        } else if(swl_str_matches(pname, "Channel")) {
            neigh->channel = amxc_var_dyncast(int32_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour Channel = %d", neigh->channel);
        } else if(swl_str_matches(pname, "PhyType")) {
            neigh->phyType = amxc_var_dyncast(int32_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour phyType = %d", neigh->phyType);
        } else if(swl_str_matches(pname, "NASIdentifier")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour nasIdentifier = %s", valStr);
            swl_str_copy(neigh->nasIdentifier, sizeof(neigh->nasIdentifier), valStr);
        } else if(swl_str_matches(pname, "R0KHKey")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour r0khkey = %s", valStr);
            swl_str_copy(neigh->r0khkey, sizeof(neigh->r0khkey), valStr);
        } else if(swl_str_matches(pname, "ColocatedAP")) {
            neigh->colocatedAp = amxc_var_dyncast(bool, newValue);
            SAH_TRACEZ_INFO(ME, "set Neighbour colocatedAp = %d", neigh->colocatedAp);
        } else {
            continue;
        }
        free(valStr);
    }
    /* If a 6GHz BSS is added, update discovery method */
    if(swl_chanspec_operClassToFreq(neigh->operatingClass) == SWL_FREQ_BAND_EXT_6GHZ) {
        wld_rad_updateDiscoveryMethod6GHz();
    }

    s_updateNeighbor(pAP, neigh);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApNeighDmHdlrs,
              ARR(),
              .instAddedCb = s_addNeighInst_oaf,
              .objChangedCb = s_setNeighConf_ocf,
              );

void _wld_ap_set_neigh_ocf(const char* const sig_name,
                           const amxc_var_t* const data,
                           void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApNeighDmHdlrs, sig_name, data, priv);
}

