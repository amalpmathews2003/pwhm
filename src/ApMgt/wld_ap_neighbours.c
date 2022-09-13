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
 *  Called when an exception is written to
 */
amxd_status_t _wld_ap_update_neigh(amxd_object_t* instance_object) {
    ASSERTS_FALSE(amxd_object_get_type(instance_object) == amxd_object_template, amxd_status_unknown_error, ME, "Template");

    T_ApNeighbour* neigh = (T_ApNeighbour*) instance_object->priv;
    ASSERT_NOT_NULL(neigh, true, ME, "NULL");

    amxd_object_t* obj_vap = amxd_object_get_parent(amxd_object_get_parent(instance_object));
    T_AccessPoint* ap = (T_AccessPoint*) obj_vap->priv;
    ASSERT_NOT_NULL(ap, amxd_status_unknown_error, ME, "NULL");

    char bssid[ETHER_ADDR_LEN];
    memset(bssid, 0, ETHER_ADDR_LEN);

    const char* bssid_str = amxd_object_get_cstring_t(instance_object, "BSSID", NULL);
    if((bssid_str != NULL) && (strlen(bssid_str) == ETHER_ADDR_STR_LEN - 1)) {
        convStr2Mac((unsigned char*) bssid, ETHER_ADDR_LEN, (unsigned char*) bssid_str, ETHER_ADDR_STR_LEN);
    }
    memcpy(neigh->bssid, bssid, ETHER_ADDR_LEN);

    const char* ssid = amxd_object_get_cstring_t(instance_object, "SSID", NULL);
    if(ssid) {
        swl_str_copy(neigh->ssid, SSID_NAME_LEN, ssid);
    } else {
        swl_str_copy(neigh->ssid, SSID_NAME_LEN, "");
    }

    neigh->information = amxd_object_get_int32_t(instance_object, "Information", NULL);
    neigh->operatingClass = amxd_object_get_int32_t(instance_object, "OperatingClass", NULL);
    neigh->channel = amxd_object_get_int32_t(instance_object, "Channel", NULL);
    neigh->phyType = amxd_object_get_int32_t(instance_object, "PhyType", NULL);
    snprintf(neigh->nasIdentifier, sizeof(neigh->nasIdentifier), "%s", amxd_object_get_cstring_t(instance_object, "NASIdentifier", NULL));
    snprintf(neigh->r0khkey, sizeof(neigh->r0khkey), "%s", amxd_object_get_cstring_t(instance_object, "R0KHKey", NULL));

    ap->pFA->mfn_wvap_updated_neighbour(ap, neigh);
    wld_autoCommitMgr_notifyVapEdit(ap);
    SAH_TRACEZ_INFO(ME, "Updated neigh");
    debugPrintNeighbour(neigh);
    return amxd_status_ok;
}

/**
 * Called when an exception is deleted
 */
amxd_status_t _wld_ap_delete_neigh(amxd_object_t* template_object, amxd_object_t* instance_object) {
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
amxd_status_t _wld_ap_add_neigh(amxd_object_t* template_object, amxd_object_t* instance_object) {
    amxd_object_t* obj_vap = amxd_object_get_parent(template_object);
    T_AccessPoint* ap = (T_AccessPoint*) obj_vap->priv;
    ASSERT_NOT_NULL(ap, amxd_status_unknown_error, ME, "NULL");

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

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

