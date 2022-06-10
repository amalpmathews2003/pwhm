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
#include "swl/swl_string.h"

#define ME "apMf"

static void delay_WPS_disable(amxp_timer_t* timer, void* userdata) {
    amxd_object_t* object = (amxd_object_t*) userdata;
    T_AccessPoint* pAP = object->priv;
    amxd_object_t* wpsObj = amxd_object_findf(object, "WPS");
    amxd_object_set_int32_t(wpsObj, "Enable", pAP->WPS_Enable);
    amxp_timer_delete(&timer);
}

static int32_t findEntry(unsigned char maclist[][ETHER_ADDR_LEN], int32_t length, unsigned char mac[ETHER_ADDR_LEN]) {
    int32_t idx = 0;
    while(idx < length) {
        if(memcmp(mac, maclist[idx], ETHER_ADDR_LEN) == 0) {
            break;
        } else {
            idx++;
        }
    }
    return idx;
}

static bool sync_changes(amxd_object_t* mf, const char* objectName, unsigned char maclist[][ETHER_ADDR_LEN], int* count, int maxentries) {
    int idx = 0;
    int changes = 0;
    unsigned char macbin[ETHER_ADDR_LEN];
    amxd_object_t* templateObj = amxd_object_get(mf, objectName);

    amxd_object_for_each(child, it, templateObj) {
        if(idx >= maxentries) {
            break;
        }
        amxd_object_t* mfentry = amxc_llist_it_get_data(it, amxd_object_t, it);
        const char* macstr = amxd_object_get_cstring_t(mfentry, "MACAddress", NULL);
        if(convStr2Mac(macbin, ETHER_ADDR_LEN, (unsigned char*) (macstr ? : ""), strlen(macstr ? : ""))) {
            if(memcmp(macbin, maclist[idx], ETHER_ADDR_LEN)) {
                changes++;
                memcpy(maclist[idx], macbin, ETHER_ADDR_LEN);
            }
            idx++;
        }
    }

    if(idx != *count) {
        *count = idx;
        changes++;
    }
    return changes != 0;
}

void wld_ap_macfilter_updateMACFilterAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_FALSE(pAP->MF_AddressListBlockSync, , ME, "Sync blocked");
    if(pAP->MF_AddressList != NULL) {
        free(pAP->MF_AddressList);
        pAP->MF_AddressList = NULL;
    }
    int size = (ETHER_ADDR_STR_LEN + 1) * pAP->MF_EntryCount;
    if(size != 0) {
        pAP->MF_AddressList = calloc(1, size);
        int i = 0;
        for(i = 0; i < pAP->MF_EntryCount; i++) {
            char macStr[ETHER_ADDR_STR_LEN] = {'\0'};
            wldu_convMac2Str(pAP->MF_Entry[i], ETHER_ADDR_LEN, macStr, ETHER_ADDR_STR_LEN);
            wldu_catFormat(pAP->MF_AddressList, size, "%s%s", macStr, (i + 1 == pAP->MF_EntryCount) ? "" : ",");
        }
    }
    if(pAP->MF_AddressList) {
        swl_str_toUpper(pAP->MF_AddressList, strlen(pAP->MF_AddressList));
    }
    amxd_object_set_cstring_t(pAP->pBus, "MACFilterAddressList", pAP->MF_AddressList ? pAP->MF_AddressList : "");
}

/**
 * This function syncs the MAC filtering list for an access point object
 * E.g. WiFiABC.AccessPoint.wl0
 * Note that the mac list changes are only set in the driver, but not executed. One must perform a commit on the radio
 * to sync the plugin mac list with driver mac list.
 */
static void syncMACFiltering(amxd_object_t* object) {
    SAH_TRACEZ_IN(ME);

    int changes = 0;

    ASSERTI_FALSE(amxd_object_get_type(object) == amxd_object_template, , ME, "Template");

    T_AccessPoint* pAP = object->priv;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    amxd_object_t* mf = amxd_object_get(object, "MACFiltering");

    const char* mfModeStr = amxd_object_get_cstring_t(mf, "Mode", NULL);
    int idx = conv_ModeIndexStr(cstr_AP_MFMode, (mfModeStr != NULL) ? mfModeStr : "Off");
    if(idx != (int) pAP->MF_Mode) {
        changes++;
        pAP->MF_Mode = idx;
    }

    bool tempBlacklistEnable = amxd_object_get_bool(mf, "TempBlacklistEnable", NULL);
    if(tempBlacklistEnable != pAP->MF_TempBlacklistEnable) {
        changes++;
        pAP->MF_TempBlacklistEnable = tempBlacklistEnable;
    }

    if((pAP->MF_Mode != APMFM_OFF) || pAP->MF_TempBlacklistEnable) {
        int prev_WPS_Enable = pAP->WPS_Enable;
        if(!pAP->WPS_CertMode && (pAP->MF_Mode == APMFM_WHITELIST)) {
            /* We can't have WPS enabled if MAC filtering is set on white list. Disable it. */
            /* For Certification we keep WPS active */
            pAP->WPS_Enable = 0;
            wld_ap_doWpsSync(pAP);
        }

        if(prev_WPS_Enable != pAP->WPS_Enable) {
            /* Update the data model *after* this commit finishes */
            amxp_timer_t* timer;
            amxp_timer_new(&timer, delay_WPS_disable, object);
            amxp_timer_start(timer, 0);
        }

        if(sync_changes(mf, "Entry", pAP->MF_Entry, &pAP->MF_EntryCount, MAXNROF_MFENTRY)) {
            wld_ap_macfilter_updateMACFilterAddressList(pAP);
            changes++;
        }
        if(sync_changes(mf, "TempEntry", pAP->MF_Temp_Entry, &pAP->MF_TempEntryCount, MAXNROF_MFENTRY)) {
            changes++;
        }
    }


    if(changes) {
        SAH_TRACEZ_INFO(ME, "Syncing mac entries to HW");
        pAP->pFA->mfn_wvap_mf_sync(pAP, SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_setMACFiltering_owf(amxd_object_t* object) {
    syncMACFiltering(amxd_object_get_parent(object));
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setMACFilteringEntry_owf(amxd_object_t* object) {
    ASSERTI_FALSE(amxd_object_get_type(object) == amxd_object_template, amxd_status_unknown_error, ME, "Template");
    syncMACFiltering(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    return amxd_status_ok;
}

amxd_status_t _destroyMACFilteringEntry(amxd_object_t* object) {
    syncMACFiltering(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    return amxd_status_ok;
}

amxd_status_t _wld_ap_deleteMACFilteringEntry_odf(amxd_object_t* template_object _UNUSED, amxd_object_t* instance_object _UNUSED) {
    //nothing to do , as delete flag is not yet set in this obj instance
    //set the destroy handler for this object, to do sync at that moment
    return amxd_status_ok;
}

void wld_ap_macfilter_deleteAllEntries(T_AccessPoint* pAP, char* listname) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(listname, , ME, "NULL");
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    ASSERTS_NOT_NULL(mfObject, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Deleting all object entries");
    pAP->MF_AddressListBlockSync = true;
    amxd_object_t* templateObj = amxd_object_get(mfObject, listname);
    amxd_object_for_each(child, it, templateObj) {
        amxd_object_t* mfentry = amxc_llist_it_get_data(it, amxd_object_t, it);
        amxd_object_delete(&mfentry);
    }
    pAP->MF_AddressListBlockSync = false;
}

void wld_ap_macfilter_addEntryObject(T_AccessPoint* pAP,
                                     const char* objName,
                                     const char* listName,
                                     const char* macStr) {
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, objName);
    ASSERTS_NOT_NULL(mfObject, , ME, "NULL");
    amxd_object_t* entry = amxd_object_get(mfObject, listName);
    ASSERTS_NOT_NULL(entry, , ME, "NULL");
    amxd_object_t* new_entry_obj;
    amxd_object_new_instance(&new_entry_obj, entry, macStr, 0, NULL);
    ASSERTS_NOT_NULL(new_entry_obj, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: add sta %s to %s.%s", pAP->alias, objName, listName, macStr);
    amxd_object_set_cstring_t(new_entry_obj, "MACAddress", macStr);
    SAH_TRACEZ_INFO(ME, "Add object entry %s", macStr);
}

bool wld_ap_macfilter_entry_exist(T_AccessPoint* pAP, char* listname, char* macStr) {
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    ASSERTS_NOT_NULL(mfObject, false, ME, "NULL");
    amxd_object_t* templateObj = amxd_object_get(mfObject, listname);
    amxd_object_for_each(child, it, templateObj) {
        amxd_object_t* mf_entry = amxc_llist_it_get_data(it, amxd_object_t, it);
        const char* entry_str = amxd_object_get_cstring_t(mf_entry, "MACAddress", NULL);
        if(strncasecmp(macStr, entry_str, ETHER_ADDR_STR_LEN) == 0) {
            SAH_TRACEZ_INFO(ME, "Entry exist %s", macStr);
            return true;
        }
    }
    return false;
}

void wld_ap_macfilter_delete_unused_entry(T_AccessPoint* pAP, char* listname) {
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    ASSERTS_NOT_NULL(mfObject, , ME, "NULL");
    amxd_object_t* mf_entry = NULL;
    amxd_object_t* templateObj = amxd_object_get(mfObject, listname);
    amxd_object_for_each(child, it, templateObj) {
        const char* entry_str = amxd_object_get_cstring_t(mf_entry, "MACAddress", NULL);
        if(pAP->MF_AddressList && !strcasestr(pAP->MF_AddressList, entry_str)) {
            SAH_TRACEZ_INFO(ME, "Deleting entry %s", entry_str);
            amxd_object_delete(&mf_entry);
        }
    }
}

static void wld_ap_macfilter_syncAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP->MF_AddressList, , ME, "NULL");
    pAP->MF_AddressListBlockSync = true;
    char* allowed_str_list = strdup(pAP->MF_AddressList);
    char* allowed_str = allowed_str_list;
    while(allowed_str) {
        char macStr[ETHER_ADDR_STR_LEN] = {'\0'};
        memcpy(&macStr, allowed_str, ETHER_ADDR_STR_LEN - 1);
        if(!wld_ap_macfilter_entry_exist(pAP, "Entry", macStr)) {
            wld_ap_macfilter_addEntryObject(pAP, "MACFiltering", "Entry", macStr);
        }
        char* end_str = strstr(allowed_str, ",");
        if(!end_str) {
            break;
        }
        allowed_str = end_str + 1;
    }
    if(allowed_str_list != NULL) {
        free(allowed_str_list);
    }
    wld_ap_macfilter_delete_unused_entry(pAP, "Entry");
    pAP->MF_AddressListBlockSync = false;
}

void wld_ap_macfilter_cleanupMACFilterAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP->MF_AddressList, , ME, "NULL");
    free(pAP->MF_AddressList);
    pAP->MF_AddressList = NULL;
    wld_ap_macfilter_deleteAllEntries(pAP, "Entry");
}

bool validateMACFilterAddressList(amxd_param_t* parameter, void* validationData _UNUSED) {
    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(parameter, &value);
    const char* newMACFilterAddressList_param = amxc_var_get_cstring_t(&value);
    amxc_var_clean(&value);
    ASSERTS_NOT_NULL(newMACFilterAddressList_param, false, ME, "NULL");
    int len = strlen(newMACFilterAddressList_param);
    if(len == 0) {
        return true;
    }
    if(!strstr(newMACFilterAddressList_param, ",") && (len > (ETHER_ADDR_STR_LEN - 1))) {
        SAH_TRACEZ_INFO(ME, "Multiple entries without separator %s", newMACFilterAddressList_param);
        return false;
    }
    if(((len + 1) % ETHER_ADDR_STR_LEN) != 0) {
        SAH_TRACEZ_INFO(ME, "Invalid MACFilterAddressList parameter len=%d param=%s", len, newMACFilterAddressList_param);
        return false;
    }
    return true;
}

static void delay_sync_mf_addressList(amxp_timer_t* timer, void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;
    wld_ap_macfilter_syncAddressList(pAP);
    amxp_timer_delete(&timer);
}

amxd_status_t _wld_apMacFilter_setAddressList_pwf(amxd_object_t* object,
                                                  amxd_param_t* parameter,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args _UNUSED,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = object;
    ASSERTI_EQUALS(amxd_object_get_type(wifiVap), amxd_object_instance, rv, ME, "Not instance");
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "Invalid AP Ctx");
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    ASSERT_EQUALS(rv, amxd_status_ok, rv, ME, "ERR status:%d", rv);

    const char* newMACFilterAddressList_param = amxc_var_constcast(cstring_t, args);
    ASSERTS_NOT_NULL(newMACFilterAddressList_param, amxd_status_unknown_error, ME, "NULL");

    if(swl_str_matches(pAP->MF_AddressList, newMACFilterAddressList_param)) {
        SAH_TRACEZ_INFO(ME, "Same MACFilterAddressList");
        return amxd_status_ok;
    }
    if(swl_str_isEmpty(newMACFilterAddressList_param)) {
        wld_ap_macfilter_cleanupMACFilterAddressList(pAP);
        return amxd_status_ok;
    }
    swl_str_copyMalloc(&pAP->MF_AddressList, newMACFilterAddressList_param);
    SAH_TRACEZ_INFO(ME, "%s: Set new MF_AddressList %s", pAP->alias, pAP->MF_AddressList);
    /* delay syncing mf addressList string with mf obj entries list
     * to give time for nemo to push its entries to plugin side.
     * This avoid duplicating MF entries.
     */
    amxp_timer_t* timer;
    amxp_timer_new(&timer, delay_sync_mf_addressList, pAP);
    amxp_timer_start(timer, 0);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static bool addEntry(
    T_AccessPoint* pAP,
    const char* objName,
    amxc_var_t* args,
    const char* listname,
    unsigned char maclist[][ETHER_ADDR_LEN],
    int* count,
    int maxEntries,
    int (* sync_function)(T_AccessPoint*, int)) {

    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    ASSERT_NOT_NULL(objName, false, ME, "NULL");

    const char* macStr = GET_CHAR(args, "mac");
    unsigned char macBin[ETHER_ADDR_LEN];

    if(*count >= maxEntries) {
        SAH_TRACEZ_ERROR(ME, "Maximum number of mac filter entries reached.");
        return false;
    }

    if(!convStr2Mac(macBin, ETHER_ADDR_LEN, (unsigned char*) macStr, strlen(macStr))) {
        SAH_TRACEZ_ERROR(ME, "Unable to parse %s", macStr);
        return false;
    }

    int32_t index = findEntry(maclist, *count, macBin);
    if(index != *count) {
        SAH_TRACEZ_INFO(ME, "Mac exists %s", macStr);
        return true;
    }

    (*count)++;

    /* Create the data model object */
    wld_ap_macfilter_addEntryObject(pAP, objName, listname, macStr);

    sync_function(pAP, SET | DIRECT);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    return true;
}

static bool delEntry(
    T_AccessPoint* pAP,
    amxd_object_t* obj,
    amxc_var_t* args,
    const char* listname,
    unsigned char maclist[][ETHER_ADDR_LEN],
    int* count,
    int (* sync_function)(T_AccessPoint* vap, int set)) {

    ASSERT_NOT_NULL(pAP, false, ME, "NULL");

    const char* macStr = GET_CHAR(args, "mac");

    unsigned char mac[ETHER_ADDR_LEN];
    if(!convStr2Mac(mac, sizeof(mac), (unsigned char*) macStr, strlen(macStr))) {
        SAH_TRACEZ_ERROR(ME, "Unable to parse %s", macStr);
        return false;
    }

    int32_t idx = findEntry(maclist, *count, mac);

    if(idx == *count) {
        SAH_TRACEZ_INFO(ME, "MAC address %s not found in the MACFiltering entries.",
                        macStr);
        return true;
    }

    /* Overwrite entry at 'idx' with the last one. */
    memmove(maclist[idx], maclist[*count - 1], ETHER_ADDR_LEN);
    (*count)--;

    /* Delete the object entry */
    amxd_object_t* template_obj = amxd_object_get(obj, listname);
    if(!template_obj) {
        SAH_TRACEZ_ERROR(ME, "NULL");
        return false;
    }

    amxd_object_t* entry_obj = NULL;
    amxd_object_for_each(child, it, template_obj) {
        amxd_object_t* mfentry = amxc_llist_it_get_data(it, amxd_object_t, it);
        const char* entry_str = amxd_object_get_cstring_t(mfentry, "MACAddress", NULL);
        if(strncmp(macStr, entry_str, ETHER_ADDR_STR_LEN) == 0) {
            entry_obj = mfentry;
            break;
        }
    }

    if(entry_obj != NULL) {
        amxd_object_delete(&entry_obj);
    } else {
        SAH_TRACEZ_ERROR(ME, "MAC address %s not found in the %s MACFiltering table.",
                         macStr, pAP->alias);
    }

    wld_ap_macfilter_updateMACFilterAddressList(pAP);

    sync_function(pAP, SET | DIRECT);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    return true;
}

amxd_status_t _MACFiltering_addEntry(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args,
                                     amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args, "Entry", pAP->MF_Entry, &pAP->MF_EntryCount, MAXNROF_MFENTRY, pAP->pFA->mfn_wvap_mf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to add Entry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }
    wld_ap_macfilter_updateMACFilterAddressList(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_addTempEntry(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args, "TempEntry", pAP->MF_Temp_Entry, &pAP->MF_TempEntryCount, MAXNROF_MFENTRY, pAP->pFA->mfn_wvap_mf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to add TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_delEntry(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args,
                                     amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!delEntry(pAP, obj, args, "Entry", pAP->MF_Entry, &pAP->MF_EntryCount, pAP->pFA->mfn_wvap_mf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete Entry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_delTempEntry(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!delEntry(pAP, obj, args, "TempEntry", pAP->MF_Temp_Entry, &pAP->MF_TempEntryCount, pAP->pFA->mfn_wvap_mf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * This function syncs the Probe filtering list for an access point object
 * E.g. WiFiABC.AccessPoint.wl0
 * Note that the mac list changes are only set in the driver, but not executed. One must perform a commit on the radio
 * to sync the plugin mac list with driver mac list.
 */
static void syncProbeFiltering(amxd_object_t* object) {
    SAH_TRACEZ_IN(ME);

    ASSERTI_FALSE(amxd_object_get_type(object) == amxd_object_template, , ME, "Template");

    T_AccessPoint* pAP = object->priv;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    amxd_object_t* pf = amxd_object_get(object, "ProbeFiltering");
    if(sync_changes(pf, "TempEntry", pAP->PF_Temp_Entry, &pAP->PF_TempEntryCount, MAXNROF_PFENTRY)) {
        SAH_TRACEZ_INFO(ME, "Syncing mac entries to HW");
        pAP->pFA->mfn_wvap_pf_sync(pAP, SET | DIRECT);
    }

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_setProbeFilteringEntry_owf(amxd_object_t* object) {
    ASSERTS_TRUE(amxd_object_get_type(object) == amxd_object_instance, amxd_status_unknown_error, ME, "Object is not an instance");
    syncProbeFiltering(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    return amxd_status_ok;
}

void destroyProbeFilteringEntry(amxd_object_t* object) {
    syncProbeFiltering(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
}

amxd_status_t _wld_ap_deleteProbeFilteringEntry_odf(amxd_object_t* template_object _UNUSED, amxd_object_t* instance_object _UNUSED) {
    //nothing to do , as delete flag is not yet set in this obj instance
    //set the destroy handler for this object, to do sync at that moment
    return amxd_status_ok;
}

amxd_status_t _ProbeFiltering_addTempEntry(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args,
                 "TempEntry", pAP->PF_Temp_Entry,
                 &pAP->PF_TempEntryCount, MAXNROF_PFENTRY, pAP->pFA->mfn_wvap_pf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to add TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _ProbeFiltering_delTempEntry(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;

    if(!delEntry(pAP, obj, args, "TempEntry", pAP->PF_Temp_Entry, &pAP->PF_TempEntryCount, pAP->pFA->mfn_wvap_pf_sync)) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_addMfEntry(wld_banlist_t* banlist, unsigned char* mac) {
    if(banlist->staToBan >= WLD_MAC_MAXBAN) {
        return;
    }
    memcpy(&banlist->banList[banlist->staToBan], mac, SWL_MAC_BIN_LEN);
    banlist->staToBan++;
}

static void s_addKickEntry(wld_banlist_t* banlist, unsigned char* mac) {
    if(banlist->staToKick >= WLD_MAC_MAXBAN) {
        return;
    }
    memcpy(&banlist->kickList[banlist->staToKick], mac, SWL_MAC_BIN_LEN);
    banlist->staToKick++;
}

void wld_apMacFilter_getBanList(T_AccessPoint* vap, wld_banlist_t* banlist, bool includePf) {
    ASSERT_NOT_NULL(vap, , ME, "NULL");
    ASSERT_NOT_NULL(banlist, , ME, "NULL");
    banlist->staToBan = 0;
    banlist->staToKick = 0;
    SAH_TRACEZ_INFO(ME, "%s get data %u mode %u/%u tmp %u/%u pf %u", vap->alias, includePf,
                    vap->MF_Mode, vap->MF_EntryCount,
                    vap->MF_TempBlacklistEnable, vap->MF_TempEntryCount,
                    vap->PF_TempEntryCount);

    if(vap->MF_Mode == APMFM_WHITELIST) {
        for(int i = 0; i < vap->MF_EntryCount; i++) {
            if(!vap->MF_TempBlacklistEnable
               || (!wldu_is_mac_in_list(vap->MF_Entry[i], vap->MF_Temp_Entry, vap->MF_TempEntryCount)
                   && !wldu_is_mac_in_list(vap->MF_Entry[i], vap->PF_Temp_Entry, vap->PF_TempEntryCount))) {
                // add entry either if temporary blacklisting is disabled, or entry not in temp blaclist
                s_addMfEntry(banlist, vap->MF_Entry[i]);
            }
        }


        //Before setting whitelist, kick all stations not whitelisted
        for(int i = 0; i < vap->AssociatedDeviceNumberOfEntries; i++) {
            if(vap->AssociatedDevice[i]->Active
               && !swl_typeMacBin_arrayContainsRef(banlist->banList, banlist->staToBan, (swl_macBin_t*) vap->AssociatedDevice[i]->MACAddress)
               && !wldu_is_mac_in_list(vap->AssociatedDevice[i]->MACAddress, vap->PF_Temp_Entry, vap->PF_TempEntryCount)) {
                s_addKickEntry(banlist, vap->AssociatedDevice[i]->MACAddress);
            }
        }
    } else if((vap->MF_Mode == APMFM_BLACKLIST)
              || (vap->MF_TempBlacklistEnable && (vap->MF_TempEntryCount > 0))
              || (vap->PF_TempEntryCount > 0)) {
        if(vap->MF_Mode == APMFM_BLACKLIST) {
            for(int i = 0; i < vap->MF_EntryCount; i++) {
                s_addMfEntry(banlist, vap->MF_Entry[i]);
                T_AssociatedDevice* assocDev = wld_vap_get_existing_station(vap, (swl_macBin_t*) vap->MF_Entry[i]);
                if(( assocDev != NULL) && assocDev->Active) {
                    s_addKickEntry(banlist, assocDev->MACAddress);
                }
            }
        }
        if(vap->MF_TempBlacklistEnable) {
            for(int i = 0; i < vap->MF_TempEntryCount; i++) {
                s_addMfEntry(banlist, vap->MF_Temp_Entry[i]);

                T_AssociatedDevice* assocDev = wld_vap_get_existing_station(vap, (swl_macBin_t*) vap->MF_Entry[i]);
                if(( assocDev != NULL) && assocDev->Active) {
                    s_addKickEntry(banlist, assocDev->MACAddress);
                }
            }
        }
        if(includePf) {
            for(int i = 0; i < vap->PF_TempEntryCount; i++) {
                s_addMfEntry(banlist, vap->PF_Temp_Entry[i]);
                //don't kick probe filter entries.
            }
        }
    } else {
        SAH_TRACEZ_INFO(ME, "%s nothing to add", vap->alias);
    }
    SAH_TRACEZ_INFO(ME, "%s done add %u %u", vap->alias, banlist->staToBan, banlist->staToKick);
}

