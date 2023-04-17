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
    char bssid[ETHER_ADDR_STR_LEN];
    (void) neigh;
    (void) bssid;
    convMac2Str((unsigned char*) neigh->bssid, ETHER_ADDR_LEN, (unsigned char*) bssid, ETHER_ADDR_STR_LEN);

    SAH_TRACEZ_INFO(ME, "Neigh %s : %u %u %u %u", bssid,
                    neigh->information, neigh->operatingClass, neigh->channel, neigh->phyType);
}

static T_ApNeighbour* find_neigh(T_AccessPoint* vap, const char* bssid_str) {
    char bssid[ETHER_ADDR_LEN];
    if((bssid_str != NULL) && (strlen(bssid_str) == ETHER_ADDR_STR_LEN - 1)) {
        convStr2Mac((unsigned char*) bssid, ETHER_ADDR_LEN, (unsigned char*) bssid_str, ETHER_ADDR_STR_LEN);
    } else {
        return NULL;
    }
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &vap->neighbours) {
        T_ApNeighbour* neigh = amxc_llist_it_get_data(it, T_ApNeighbour, it);
        if(memcmp(bssid, neigh->bssid, ETHER_ADDR_LEN) == 0) {
            return neigh;
        }
    }
    return NULL;

}

/**
 *  Get neighbour by object reference
 */
static T_ApNeighbour* s_find_neighByObjIndex(T_AccessPoint* vap, amxd_object_t* object) {

    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &vap->neighbours) {
        T_ApNeighbour* neigh = amxc_llist_it_get_data(llit, T_ApNeighbour, it);
        if(swl_str_matches(amxd_object_get_name(neigh->obj, 0), amxd_object_get_name(object, 0))) {
            return neigh;
        }
    }
    return NULL;

}

static void s_updateNeighbor (T_AccessPoint* pAP, T_ApNeighbour* neigh) {

    ASSERTS_STR(neigh->ssid, , ME, "empty ssid");
    pAP->pFA->mfn_wvap_updated_neighbour(pAP, neigh);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    debugPrintNeighbour(neigh);
}

static void s_removeNeighbor (T_AccessPoint* pAP, T_ApNeighbour* neigh) {

    ASSERTS_STR(neigh->ssid, , ME, "empty ssid");
    pAP->pFA->mfn_wvap_deleted_neighbour(pAP, neigh);
}

static amxd_status_t s_checkStartCallVap(T_AccessPoint** pAP, amxd_object_t* wifiObj, amxd_object_t* object,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(wifiObj) != amxd_object_instance) {
        SAH_TRACEZ_ERROR(ME, "obj is not an instance");
        return amxd_status_unknown_error;
    }

    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "unable to write the param");
        return rv;
    }

    *pAP = (T_AccessPoint*) wifiObj->priv;

    if((*pAP == NULL)) {
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourPhyType_pwf(amxd_object_t* object,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int phyType = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_INFO(ME, "set Neighbour  phyType = %d", phyType);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);
    neigh->phyType = phyType;
    s_updateNeighbor(pAP, neigh);
    return amxd_status_ok;
}
amxd_status_t _wld_ap_setNeighbourChannel_pwf(amxd_object_t* object,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int channel = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_INFO(ME, "set Neighbour  Channel = %d", channel);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    neigh->channel = channel;
    s_updateNeighbor(pAP, neigh);
    return amxd_status_ok;
}



amxd_status_t _wld_ap_setNeighbourOperatingClass_pwf(amxd_object_t* object,
                                                     amxd_param_t* parameter _UNUSED,
                                                     amxd_action_t reason _UNUSED,
                                                     const amxc_var_t* const args,
                                                     amxc_var_t* const retval _UNUSED,
                                                     void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int operatingClass = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_INFO(ME, "set Neighbour  operatingClass = %d", operatingClass);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    neigh->operatingClass = operatingClass;
    s_updateNeighbor(pAP, neigh);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourInformation_pwf(amxd_object_t* object,
                                                  amxd_param_t* parameter _UNUSED,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int bssidInfo = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_INFO(ME, "set Neighbour BSSID Info = %d", bssidInfo);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    neigh->information = bssidInfo;
    s_updateNeighbor(pAP, neigh);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourBSSID_pwf(amxd_object_t* object,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    char bssid[ETHER_ADDR_LEN];
    memset(bssid, 0, ETHER_ADDR_LEN);

    const char* bssid_str = amxc_var_constcast(cstring_t, args);
    if((bssid_str == NULL) || (strlen(bssid_str) != ETHER_ADDR_STR_LEN - 1)) {
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_INFO(ME, "set Neighbour  BSSID = %s", bssid_str);
    convStr2Mac((unsigned char*) bssid, ETHER_ADDR_LEN, (unsigned char*) bssid_str, ETHER_ADDR_STR_LEN);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    memcpy(neigh->bssid, bssid, ETHER_ADDR_LEN);
    s_updateNeighbor(pAP, neigh);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourSSID_pwf(amxd_object_t* object,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    const char* ssid_str = amxc_var_constcast(cstring_t, args);
    if(ssid_str == NULL) {
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_INFO(ME, "set Neighbour  SSID = %s", ssid_str);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    swl_str_copy(neigh->ssid, SSID_NAME_LEN, ssid_str);
    s_updateNeighbor(pAP, neigh);

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourR0KHKey_pwf(amxd_object_t* object,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    const char* r0khkey = amxc_var_constcast(cstring_t, args);
    if(r0khkey == NULL) {
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_INFO(ME, "set Neighbour r0khkey = %s", r0khkey);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    snprintf(neigh->r0khkey, sizeof(neigh->r0khkey), "%s", r0khkey);
    s_updateNeighbor(pAP, neigh);

    return amxd_status_ok;
}

amxd_status_t _wld_ap_setNeighbourNASIdentifier_pwf(amxd_object_t* object,
                                                    amxd_param_t* parameter _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    T_AccessPoint* pAP = NULL;
    amxd_object_t* wifiVap = amxd_object_get_parent(amxd_object_get_parent(object));
    rv = s_checkStartCallVap(&pAP, wifiVap, object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    const char* nasIdentifier = amxc_var_constcast(cstring_t, args);
    if(nasIdentifier == NULL) {
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_INFO(ME, "set Neighbour  nasIdentifier = %s", nasIdentifier);

    T_ApNeighbour* neigh = s_find_neighByObjIndex(pAP, object);
    if(neigh == NULL) {
        return amxd_status_unknown_error;
    }

    s_removeNeighbor(pAP, neigh);

    snprintf(neigh->nasIdentifier, sizeof(neigh->nasIdentifier), "%s", nasIdentifier);
    s_updateNeighbor(pAP, neigh);

    return amxd_status_ok;
}
/**
 * Called when an exception is deleted
 */
amxd_status_t _wld_ap_delete_neigh_odf(amxd_object_t* template_object, amxd_object_t* instance_object) {
    T_ApNeighbour* neigh = (T_ApNeighbour*) instance_object->priv;

    amxd_object_t* obj_vap = amxd_object_get_parent(template_object);
    T_AccessPoint* ap = (T_AccessPoint*) obj_vap->priv;
    ASSERT_NOT_NULL(neigh, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(ap, amxd_status_unknown_error, ME, "NULL");

    amxc_llist_it_take(&(neigh->it));
    ap->pFA->mfn_wvap_deleted_neighbour(ap, neigh);
    wld_autoCommitMgr_notifyVapEdit(ap);

    SAH_TRACEZ_INFO(ME, "Deleted neigh");
    debugPrintNeighbour(neigh);

    free(neigh);

    return amxd_status_ok;
}

/**
 * Called when a new exception is added.
 */
amxd_status_t _wld_ap_add_neigh_ocf(amxd_object_t* object,
                                    amxd_param_t* param,
                                    amxd_action_t reason,
                                    const amxc_var_t* const args,
                                    amxc_var_t* const retval,
                                    void* priv) {
    amxd_status_t status = amxd_status_ok;
    amxd_object_t* obj_vap = amxd_object_get_parent(object);
    T_AccessPoint* ap = (T_AccessPoint*) obj_vap->priv;
    ASSERT_NOT_NULL(ap, amxd_status_unknown_error, ME, "NULL");
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);

    if(status != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "Adding neigh failed");
        return status;
    }
    amxd_object_t* instance_object = NULL;
    instance_object = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    T_ApNeighbour* neigh = calloc(1, sizeof(T_ApNeighbour));
    ASSERT_NOT_NULL(neigh, amxd_status_unknown_error, ME, "NULL");

    amxc_llist_it_init(&neigh->it);
    amxc_llist_append(&ap->neighbours, &neigh->it);
    instance_object->priv = neigh;

    SAH_TRACEZ_INFO(ME, "Adding neigh");
    neigh->obj = instance_object;

    return amxd_status_ok;
}


/**
 * Function call to create an exception.
 */
amxd_status_t _setNeighbourAP(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = obj->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    const char* bssid = GET_CHAR(args, "BSSID");
    const char* ssid = GET_CHAR(args, "SSID");
    uint32_t info = GET_UINT32(args, "Information");
    uint32_t operatingClass = GET_UINT32(args, "OperatingClass");
    uint32_t channel = GET_UINT32(args, "Channel");
    uint32_t phyType = GET_UINT32(args, "PhyType");
    const char* nasIdentifier = GET_CHAR(args, "NASIdentifier");
    const char* r0khkey = GET_CHAR(args, "R0KHKey");

    ASSERT_NOT_NULL(bssid, amxd_status_unknown_error, ME, "BSSID is not filled in");

    swl_macChar_t cMacStd;
    cMacStd.cMac[0] = 0;
    if(!swl_mac_charToStandard(&cMacStd, (const char*) bssid)) {
        SAH_TRACEZ_ERROR(ME, "BSSID is INVALID %s", bssid);
        return amxd_status_unknown_error;
    }

    amxd_object_t* object;
    T_ApNeighbour* neigh = find_neigh(pAP, cMacStd.cMac);
    if(neigh != NULL) {
        SAH_TRACEZ_INFO(ME, "Neigbour with bssid [%s] found in neighbour list", cMacStd.cMac);
        object = neigh->obj;
    } else {
        amxd_object_t* exception_template = amxd_object_get(obj, "Neighbour");
        amxd_object_new_instance(&object, exception_template, NULL, 0, NULL);
        if(object == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to create new Neighbour instance");
            return amxd_status_unknown_error;
        }
        T_ApNeighbour* neigh = calloc(1, sizeof(T_ApNeighbour));
        ASSERT_NOT_NULL(neigh, amxd_status_unknown_error, ME, "NULL");
        amxc_llist_it_init(&neigh->it);
        amxc_llist_append(&pAP->neighbours, &neigh->it);

        neigh->obj = object;

        object->priv = neigh;
        SAH_TRACEZ_INFO(ME, "Create new neighbour with bssid [%s]", cMacStd.cMac);
        amxd_object_set_cstring_t(object, "BSSID", cMacStd.cMac);
    }

    SAH_TRACEZ_INFO(ME, "Updating neighbour object with provide arguments");
    if(ssid) {
        amxd_object_set_cstring_t(object, "SSID", ssid);
    }
    if(info != 0) {
        amxd_object_set_int32_t(object, "Information", info);
    }
    if(operatingClass != 0) {
        amxd_object_set_int32_t(object, "OperatingClass", operatingClass);
    }
    if(channel != 0) {
        amxd_object_set_int32_t(object, "Channel", channel);
    }
    if(phyType != 0) {
        amxd_object_set_int32_t(object, "PhyType", phyType);
    }
    if(nasIdentifier != NULL) {
        amxd_object_set_cstring_t(object, "NASIdentifier", nasIdentifier);
    }
    if(r0khkey != NULL) {
        amxd_object_set_cstring_t(object, "R0KHKey", r0khkey);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _delNeighbourAP(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = obj->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    const char* bssid = GET_CHAR(args, "BSSID");
    ASSERT_NOT_NULL(bssid, amxd_status_unknown_error, ME, "BSSID is not filled in");

    T_ApNeighbour* neigh = find_neigh(pAP, bssid);
    if(!neigh) {
        SAH_TRACEZ_INFO(ME, "Could not find neighbour with bssid %s", bssid);
        return amxd_status_unknown_error;
    }

    amxd_object_delete(&neigh->obj);

    amxc_llist_it_take(&(neigh->it));
    pAP->pFA->mfn_wvap_deleted_neighbour(pAP, neigh);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    SAH_TRACEZ_INFO(ME, "Deleted neigh");
    debugPrintNeighbour(neigh);

    free(neigh);

    return amxd_status_ok;
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

