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
#include "wld_radio.h"
#include "swl/swl_assert.h"
#include "wld_rad_stamon.h"
#include "wld_monitor.h"

#define ME "radStm"

/**
 * Retrieve a Device based on its string MACAddress in a given list.
 */
wld_nasta_t* wld_rad_staMon_getDevice(const char* macAddrStr, amxc_llist_t* devList) {
    ASSERT_NOT_NULL(macAddrStr, NULL, ME, "NULL");
    ASSERTS_EQUALS(strlen(macAddrStr), ETHER_ADDR_STR_LEN - 1, NULL, ME, "Bad Format MacAddress");

    unsigned char macAddr[ETHER_ADDR_LEN];
    memset(macAddr, 0, sizeof(macAddr));
    convStr2Mac(macAddr, ETHER_ADDR_LEN, (unsigned char*) macAddrStr, ETHER_ADDR_STR_LEN);

    amxc_llist_it_t* it;
    amxc_llist_for_each(it, devList) {
        wld_nasta_t* pMD = amxc_llist_it_get_data(it, wld_nasta_t, it);
        if(memcmp(macAddr, pMD->MACAddress, ETHER_ADDR_LEN) == 0) {
            return pMD;
        }
    }

    return NULL;
}

T_NonAssociatedDevice* wld_rad_staMon_getNonAssociatedDevice(T_Radio* pRad, const char* macAddrStr) {
    return wld_rad_staMon_getDevice(macAddrStr, &pRad->naStations);
}

T_MonitorDevice* wld_rad_staMon_getMonitorDevice(T_Radio* pRad, const char* macAddrStr) {
    return wld_rad_staMon_getDevice(macAddrStr, &pRad->monitorDev);
}

amxd_status_t wld_rad_staMon_addDevice(T_Radio* pRad, amxd_object_t* instance_object, amxc_llist_t* devList, const amxc_var_t* const args) {
    amxc_var_t* params = amxc_var_get_key(args, "parameters", AMXC_VAR_FLAG_DEFAULT);
    const char* macAddrStr = GET_CHAR(params, "MACAddress");
    if(!macAddrStr || (strlen(macAddrStr) != ETHER_ADDR_STR_LEN - 1)) {
        SAH_TRACEZ_ERROR(ME, "macAddrStr %s is not accepted! ", macAddrStr);
        return amxd_status_unknown_error;
    }

    wld_nasta_t* pMD = calloc(1, sizeof(wld_nasta_t));
    ASSERT_NOT_NULL(pMD, amxd_status_unknown_error, ME, "NULL");
    amxc_llist_it_init(&pMD->it);
    amxc_llist_append(devList, &pMD->it);
    instance_object->priv = pMD;

    convStr2Mac(pMD->MACAddress, ETHER_ADDR_LEN, (unsigned char*) macAddrStr, ETHER_ADDR_STR_LEN);

    pMD->SignalStrength = 0;
    pMD->TimeStamp = 0;

    pMD->obj = instance_object;
    pRad->pFA->mfn_wrad_add_stamon(pRad, pMD);
    return amxd_status_ok;
}

/**
 *  Called when a new NonAssociatedDevice is added.
 */
amxd_status_t _wld_rad_staMon_addNaSta_ocf(amxd_object_t* object,
                                           amxd_param_t* param,
                                           amxd_action_t reason,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval,
                                           void* priv) {
    amxd_status_t status = amxd_status_ok;
    amxd_object_t* obj_rad = amxd_object_get_parent(amxd_object_get_parent(object));
    T_Radio* pRad = (T_Radio*) obj_rad->priv;
    ASSERT_NOT_NULL(pRad, true, ME, "NULL");
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    amxd_object_t* instance = NULL;
    if(status == amxd_status_ok) {
        instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
        status = wld_rad_staMon_addDevice(pRad, instance, &pRad->naStations, args);
    } else {
        SAH_TRACEZ_ERROR(ME, " Nasta creation failed with error: %d", status);
    }
    return status;
}

amxd_status_t wld_rad_staMon_deleteDevice(amxd_object_t* instanceObject) {
    ASSERT_NOT_NULL(instanceObject, amxd_status_unknown_error, ME, "NULL");
    amxd_object_t* templateObject = amxd_object_get_parent(instanceObject);
    if(amxd_object_get_type(instanceObject) != amxd_object_template) {
        char* path = amxd_object_get_path(instanceObject, AMXD_OBJECT_NAMED);
        SAH_TRACEZ_ERROR(ME, " Path %s is not a  template! ", path);
    }
    wld_nasta_t* pMD = (wld_nasta_t*) instanceObject->priv;
    ASSERT_NOT_NULL(pMD, amxd_status_unknown_error, ME, "NULL");

    amxd_object_t* obj_rad = amxd_object_get_parent(amxd_object_get_parent(templateObject));
    T_Radio* pRad = (T_Radio*) obj_rad->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxc_llist_it_take(&(pMD->it));

    pRad->pFA->mfn_wrad_del_stamon(pRad, pMD);

    free(pMD);

    return amxd_status_ok;
}

/**
 *  Called when a NonAssociatedDevice is deleted
 */
amxd_status_t _wld_rad_staMon_deleteNaSta_odf(amxd_object_t* object,
                                              amxd_param_t* param _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args _UNUSED,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    return wld_rad_staMon_deleteDevice(object);
}

amxd_status_t wld_rad_stamon_createDevice(amxd_object_t* parent, const char* template, amxc_var_t* args, amxc_llist_t* devList) {
    ASSERT_NOT_NULL(parent, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(template, amxd_status_unknown_error, ME, "NULL");

    const char* macAddr = GET_CHAR(args, "MACAddress");
    ASSERT_NOT_NULL(macAddr, amxd_status_unknown_error, ME, "MACAddress NULL");
    SAH_TRACEZ_INFO(ME, "Creating Device instance with template %s and macAddr %s", template, macAddr);

    amxd_object_t* object;
    wld_nasta_t* pMD = wld_rad_staMon_getDevice(macAddr, devList);
    if(pMD != NULL) {
        SAH_TRACEZ_INFO(ME, "Found Device instance with macAddr %s", macAddr);
        object = pMD->obj;
    } else {

        amxd_object_t* templateObject = amxd_object_get(parent, template);
        // the following is just starting the on action add-inst function
        amxd_status_t status = amxd_object_add_instance(&object, templateObject, macAddr, 0, args);
        if(object == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to create new object with error %d", status);
            return status;
        }
        amxd_object_set_cstring_t(object, "MACAddress", macAddr);
    }

    return amxd_status_ok;
}


amxd_status_t wld_rad_staMon_deleteNastaDevice(amxc_var_t* args, amxc_llist_t* devList) {
    const char* macAddr = GET_CHAR(args, "MACAddress");
    SAH_TRACEZ_INFO(ME, "DeleteNastaDevice with macaddress %s", macAddr);
    ASSERT_NOT_NULL(macAddr, amxd_status_unknown_error, ME, "NULL");

    wld_nasta_t* pMD = wld_rad_staMon_getDevice(macAddr, devList);
    if(!pMD) {
        SAH_TRACEZ_INFO(ME, "Invalid MACAddr delete");
        return amxd_status_ok;
    }
    amxd_object_delete(&pMD->obj);

    return amxd_status_ok;
}


/**
 * This function create a new non associated station based on MACAddress.
 */
amxd_status_t _createNonAssociatedDevice(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t state = wld_rad_stamon_createDevice(obj, "NonAssociatedDevice", args, &pRad->naStations);

    SAH_TRACEZ_OUT(ME);
    return state;
}

/**
 * This function delete a non associated station based on MACAddress.
 */
amxd_status_t _deleteNonAssociatedDevice(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t state = wld_rad_staMon_deleteNastaDevice(args, &pRad->naStations);

    SAH_TRACEZ_OUT(ME);
    return state;
}

amxd_status_t wld_rad_staMon_getDeviceStats(amxd_object_t* obj,
                                            uint64_t call_id _UNUSED,
                                            amxc_var_t* retval,
                                            const char* path) {
    SAH_TRACEZ_IN(ME);

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Invalid Radio Pointer");
    ASSERTI_TRUE(pRad->stationMonitorEnabled, amxd_status_ok, ME, "%s stamon disabled", pRad->Name);

    if(pRad->pFA->mfn_wrad_update_mon_stats(pRad) < 0) {
        SAH_TRACEZ_ERROR(ME, "Update monitor stats failed");
        return amxd_status_unknown_error;
    }

    amxd_object_t* object = amxd_object_get(pRad->pBus, path);
    amxd_object_for_each(instance, it, object) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        if(instance == NULL) {
            continue;
        }
        wld_nasta_t* pMD = (wld_nasta_t*) instance->priv;
        if(!pMD) {
            continue;
        }
        amxd_object_set_int32_t(instance, "SignalStrength", pMD->SignalStrength);
        struct tm now_tm;
        gmtime_r(&pMD->TimeStamp, &now_tm);
        amxc_ts_t ts;
        amxd_object_set_value(amxc_ts_t, instance, "TimeStamp", &ts);

        amxc_var_t map;
        amxc_var_init(&map);
        amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);

        amxc_var_move(retval, &map);
        amxc_var_clean(&map);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Retrieve NonAssociatedDevice statistics from WLAN driver
 */
amxd_status_t _getNaStationStats(amxd_object_t* obj,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {
    uint64_t call_id = amxc_var_dyncast(uint64_t, retval);
    return wld_rad_staMon_getDeviceStats(obj, call_id, retval, "NaStaMonitor.NonAssociatedDevice");
}

amxd_status_t _clearNonAssociatedDevices(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args _UNUSED,
                                         amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Invalid Radio Pointer");

    amxc_llist_it_t* it;
    it = amxc_llist_get_first(&pRad->naStations);
    while(it != NULL) {
        T_NonAssociatedDevice* pMD = amxc_llist_it_get_data(it, T_NonAssociatedDevice, it);
        if(pMD) {
            amxd_object_delete(&pMD->obj);
        }
        it = amxc_llist_get_first(&pRad->naStations);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_radStaMon_setEnable_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_object_get_parent(object);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }
    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool enabled = amxc_var_dyncast(bool, args);

    pR->stationMonitorEnabled = enabled;

    int ret = pR->pFA->mfn_wrad_setup_stamon(pR, enabled);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to %s station monitor", enabled ? "enable" : "disable");
        return amxd_status_unknown_error;
    }

    wld_radStaMon_updateActive(pR);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static int32_t wld_rad_staMon_getStats(amxc_var_t* myList, T_RssiEventing* ev, amxc_llist_t* devList) {
    int nrUpdates = 0;
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, devList) {
        wld_nasta_t* pMD = amxc_llist_it_get_data(it, wld_nasta_t, it);

        pMD->rssiAccumulator = wld_util_performFactorStep(
            pMD->rssiAccumulator, pMD->SignalStrength, ev->averagingFactor);
        int32_t rssi_val = WLD_ACC_TO_VAL(pMD->rssiAccumulator);

        uint32_t diff = abs(pMD->monRssi - rssi_val);
        SAH_TRACEZ_INFO(ME, "Check "MAC_PRINT_FMT " : %i : %i - %i ?>= %i",
                        MAC_PRINT_ARG(pMD->MACAddress), pMD->SignalStrength, rssi_val, pMD->monRssi, ev->rssiInterval);
        if(diff >= ev->rssiInterval) {
            pMD->monRssi = rssi_val;
            amxc_var_t myMap;
            amxc_var_init(&myMap);
            amxc_var_add_key(int32_t, &myMap, "SignalStrength", pMD->monRssi);
            unsigned char buffer[ETHER_ADDR_STR_LEN];
            convMac2Str(pMD->MACAddress, ETHER_ADDR_LEN, buffer, ETHER_ADDR_STR_LEN);
            amxc_var_add_key(cstring_t, &myMap, "MACAddress", (char*) buffer);

            struct tm now_tm;
            time_t now = time(NULL);
            gmtime_r(&now, &now_tm);
            amxc_ts_t ts;
            amxc_var_add_key(amxc_ts_t, &myMap, "TimeStamp", &ts);

            amxc_var_move(myList, &myMap);
            nrUpdates++;
        }
    }
    return nrUpdates;
}

static void timeHandler(void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;

    T_RssiEventing* ev = &pRad->naStaRssiMonitor;

    SAH_TRACEZ_INFO(ME, "Time rssiMon %s", pRad->Name);

    int result = pRad->pFA->mfn_wrad_update_mon_stats(pRad);
    if(result != WLD_OK) {
        wld_mon_stop(&ev->monitor);
    }

    int nrUpdates = 0;

    amxc_var_t myList;
    amxc_var_init(&myList);
    amxc_var_set_type(&myList, AMXC_VAR_ID_LIST);

    nrUpdates = wld_rad_staMon_getStats(&myList, ev, &pRad->monitorDev);
    nrUpdates += wld_rad_staMon_getStats(&myList, ev, &pRad->naStations);

    // Only send update if necessary, don't want to continuously spam
    if(nrUpdates > 0) {
        amxd_object_t* eventObject = amxd_object_findf(pRad->pBus, "NaStaMonitor.RssiEventing");
        wld_mon_sendUpdateNotification(eventObject, "RssiUpdate", &myList);
    }
    //Cleanup
    amxc_var_clean(&myList);
}

void wld_radStaMon_updateActive(T_Radio* pRad) {
    bool targetActive = (pRad->status == RST_UP) && pRad->stationMonitorEnabled;
    wld_mon_writeActive(&(pRad->naStaRssiMonitor.monitor), targetActive);
}

void wld_radStaMon_init(T_Radio* pRad) {
    amxd_object_t* eventObject = amxd_object_findf(pRad->pBus, "NaStaMonitor.RssiEventing");
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    char name[IFNAMSIZ + 16] = {0};
    snprintf(name, sizeof(name), "%s_RadRssiMon", pRad->Name);
    wld_mon_initMon(&ev->monitor, eventObject, name, pRad, timeHandler);
}

void wld_radStaMon_destroy(T_Radio* pRad) {
    SAH_TRACEZ_IN(ME);
    amxc_llist_it_t* it = NULL;
    while(!amxc_llist_is_empty(&pRad->naStations)) {
        it = amxc_llist_get_first(&pRad->naStations);
        wld_nasta_t* pNaSta = amxc_llist_it_get_data(it, wld_nasta_t, it);
        amxd_object_delete(&pNaSta->obj);
        pNaSta->obj = NULL;
    }

    while(!amxc_llist_is_empty(&pRad->monitorDev)) {
        it = amxc_llist_get_first(&pRad->monitorDev);
        wld_nasta_t* pMonDev = amxc_llist_it_get_data(it, wld_nasta_t, it);
        amxd_object_delete(&pMonDev->obj);
        pMonDev->obj = NULL;
    }
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    wld_mon_destroyMon(&ev->monitor);
    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_radStaMon_setRssiEventing_owf(amxd_object_t* object) {
    amxd_object_t* radObject = amxd_object_get_parent(amxd_object_get_parent(object));
    ASSERTI_FALSE(amxd_object_get_type(radObject) == amxd_object_template, amxd_status_unknown_error, ME, "Initial template run, skip");

    T_Radio* pRad = (T_Radio*) radObject->priv;
    ASSERTI_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Radio object is NULL");
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;

    SAH_TRACEZ_INFO(ME, "Update rssiMon @ %s", pRad->Name);

    ev->rssiInterval = amxd_object_get_uint32_t(object, "RssiInterval", NULL);
    ev->averagingFactor = amxd_object_get_uint32_t(object, "AveragingFactor", NULL);
    return amxd_status_ok;
}

void wld_radStaMon_debug(T_Radio* pRad, amxc_var_t* retMap) {
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    amxc_var_add_key(uint32_t, retMap, "RssiInterval", ev->rssiInterval);
    amxc_var_add_key(uint32_t, retMap, "AveragingFactor", ev->averagingFactor);
    wld_mon_debug(&ev->monitor, retMap);
}

amxd_status_t _wld_rad_staMon_addNastaMonDev_ocf(amxd_object_t* object,
                                                 amxd_param_t* param,
                                                 amxd_action_t reason,
                                                 const amxc_var_t* const args,
                                                 amxc_var_t* const retval,
                                                 void* priv) {
    amxd_status_t status = amxd_status_ok;
    amxd_object_t* obj_rad = amxd_object_get_parent(amxd_object_get_parent(object));
    T_Radio* pRad = (T_Radio*) obj_rad->priv;
    ASSERT_NOT_NULL(pRad, true, ME, "NULL");
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    amxd_object_t* instance = NULL;
    if(status == amxd_status_ok) {
        instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
        status = wld_rad_staMon_addDevice(pRad, instance, &pRad->monitorDev, args);
    } else {
        SAH_TRACEZ_ERROR(ME, "Creation of Nasta failed!");
    }
    return status;
}

amxd_status_t _wld_rad_staMon_deleteNastaMonDev_odf(amxd_object_t* object,
                                                    amxd_param_t* param _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args _UNUSED,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    return wld_rad_staMon_deleteDevice(object);
}

/**
 * Retrieve MonitorDevice statistics from WLAN driver
 */
amxd_status_t _getMonitorDeviceStats(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args _UNUSED,
                                     amxc_var_t* retval) {
    uint64_t call_id = amxc_var_dyncast(uint64_t, retval);
    return wld_rad_staMon_getDeviceStats(obj, call_id, retval, "NaStaMonitor.MonitorDevice");
}

/**
 * This function create a new monitor device based on MACAddress.
 */
amxd_status_t _createMonitorDevice(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");
    amxd_status_t state = wld_rad_stamon_createDevice(obj, "MonitorDevice", args, &pRad->monitorDev);

    SAH_TRACEZ_OUT(ME);
    return state;
}

amxd_status_t _deleteMonitorDevice(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args _UNUSED,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t state = wld_rad_staMon_deleteNastaDevice(args, &pRad->monitorDev);

    SAH_TRACEZ_OUT(ME);
    return state;
}

amxd_status_t _clearMonitorDevices(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args _UNUSED,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Invalid Radio Pointer");

    amxc_llist_it_t* it;
    it = amxc_llist_get_first(&pRad->monitorDev);
    while(it != NULL) {
        T_MonitorDevice* pMD = amxc_llist_it_get_data(it, T_MonitorDevice, it);
        if(pMD) {
            amxd_object_delete(&pMD->obj);
        }
        it = amxc_llist_get_first(&pRad->monitorDev);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}
