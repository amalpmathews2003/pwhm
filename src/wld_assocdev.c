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


#include <net/if.h>
#include <sys/ioctl.h>
#include <math.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "swl/swl_tupleType.h"

#include "wld_ap_rssiMonitor.h"

#define BRCTL_DEL_FDB_ENTRIES 27
#define STATIONS_STATS_TIMEOUT 5000

SWL_TABLE(modeToStandard,
          ARR(uint32_t radStd_e; char* radStd_s; ),
          ARR(swl_type_uint32, swl_type_charPtr),
          ARR({SWL_RADSTD_A, SWL_RADSTD_LEGACY_A},
              {SWL_RADSTD_B, SWL_RADSTD_LEGACY_B},
              {SWL_RADSTD_G, SWL_RADSTD_LEGACY_B_G},
              {SWL_RADSTD_AC, SWL_RADSTD_LEGACY_AC_AND_LOWER},
              {SWL_RADSTD_AX, SWL_RADSTD_LEGACY_AX_AND_LOWER},
              {SWL_RADSTD_AUTO, SWL_RADSTD_LEGACY_AUTO})
          );

#define ME "ad"

#define FAST_RECONNECT_TIMEOUT 20
#define FAST_RECONNECT_EVENT_TIMEOUT 30
#define FAST_RECONNECT_USER_MIN_TIME_MS 1500

typedef struct {
    swl_macBin_t macAddress;
    swl_timeSpecMono_t dcTime;
} wld_ad_dcLog_t;
char* tupleNames[] = {"macAddress", "dcTime"};
SWL_TUPLE_TYPE(wld_ad_dcLog, ARR(swl_type_macBin, swl_type_timeMono));

const char* fastReconnectTypes[WLD_FAST_RECONNECT_MAX] = {"Default", "OnStateChange", "OnScan", "User"};

static void s_incrementObjCounter(amxd_object_t* obj, char* counterName) {
    uint32_t val = amxd_object_get_uint32_t(obj, counterName, NULL);
    val++;
    amxd_object_set_uint32_t(obj, counterName, val);
}

static void s_incrementAssocCounter(T_AccessPoint* pAP, char* counterName) {
    amxd_object_t* counter = amxd_object_findf(pAP->pBus, "AssociationCount");
    s_incrementObjCounter(counter, counterName);
}

static void s_incrementFastReconnectType(T_AccessPoint* pAP, wld_fastReconnectType_e type) {
    char path[128] = {0};
    snprintf(path, sizeof(path), "AssociationCount.FastReconnectTypes.%s", fastReconnectTypes[type]);
    amxd_object_t* counter = amxd_object_findf(pAP->pBus, "%s", path);
    ASSERT_NOT_NULL(counter, , ME, "NULL");
    s_incrementObjCounter(counter, "Count");
}

static wld_ad_dcLog_t* s_findDcEntry(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    SAH_TRACEZ_INFO(ME, "%s: search %u", pAP->alias, swl_unLiList_size(&pAP->staDcList.list));
    swl_unLiListIt_t listIt;
    swl_unLiList_for_each(listIt, &pAP->staDcList.list) {
        wld_ad_dcLog_t* dcEntry = listIt.data;
        if(swl_mac_binMatches(&dcEntry->macAddress, macAddress)) {
            return dcEntry;
        }
    }
    return NULL;
}


static void s_dcEntryConnected(T_AccessPoint* pAP, wld_ad_dcLog_t* entry) {
    ASSERTI_NOT_NULL(entry, , ME, "NULL");
    swl_timeSpecMono_t now;
    swl_timespec_getMono(&now);
    int64_t timeDiffMs = swl_timespec_diffToNanosec(&entry->dcTime, &now) / 1000 / 1000;
    T_Radio* pRad = pAP->pRadio;


    int32_t timeDiffStateChange = entry->dcTime.tv_sec - pAP->lastStatusChange;
    int32_t timeDiffScanChange = entry->dcTime.tv_sec - pRad->scanState.lastScanTime;

    SAH_TRACEZ_INFO(ME, " * retime assoc %" PRId64 "ms state %is scan %is", timeDiffMs, timeDiffStateChange, timeDiffScanChange);

    if(timeDiffMs / 1000 <= FAST_RECONNECT_TIMEOUT) {
        s_incrementAssocCounter(pAP, "FastReconnects");

        wld_fastReconnectType_e reconnectType = WLD_FAST_RECONNECT_DEFAULT;

        if(timeDiffStateChange <= FAST_RECONNECT_EVENT_TIMEOUT) {
            reconnectType = WLD_FAST_RECONNECT_STATE_CHANGE;
        } else if(timeDiffScanChange <= FAST_RECONNECT_EVENT_TIMEOUT) {
            reconnectType = WLD_FAST_RECONNECT_SCAN;
        } else if(timeDiffMs >= FAST_RECONNECT_USER_MIN_TIME_MS) {
            reconnectType = WLD_FAST_RECONNECT_USER;
        }

        s_incrementFastReconnectType(pAP, reconnectType);


        SAH_TRACEZ_WARNING(ME, SWL_MAC_FMT ": Fast reconnect detected @ %s ( %" PRId64 " / %i / %i). Reason -%s-",
                           SWL_MAC_ARG(entry->macAddress.bMac),
                           pAP->name,
                           timeDiffMs, timeDiffStateChange, timeDiffScanChange,
                           fastReconnectTypes[reconnectType]);
    }
}

/**
 * Remove a given associated device from the bridge table.
 */
int wld_ad_remove_assocdev_from_bridge(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {

    T_Radio* pRad = pAP->pRadio;

    ASSERT_FALSE(pRad->wlRadio_SK == -1, -1, ME,
                 "Radio %s socket not init", pRad->Name);

    int err;
    struct ifreq ifr;
    char* brName = pAP->bridgeName;


    ASSERT_FALSE(brName == NULL || strlen(brName) == 0, -1, ME,
                 "No bridge configured for ap %s", pAP->alias);

    unsigned long args[4] = { BRCTL_DEL_FDB_ENTRIES,
        (unsigned long) pAD->MACAddress,
        1, pAP->index };

    SAH_TRACEZ_INFO(ME, "Kicking dev "MAC_PRINT_FMT " out of rad %s on bridge %s",
                    MAC_PRINT_ARG(pAD->MACAddress), pRad->Name, brName);

    wldu_copyStr(ifr.ifr_ifrn.ifrn_name, brName, IFNAMSIZ);
    ifr.ifr_ifru.ifru_data = (char*) args;

    err = ioctl(pRad->wlRadio_SK, SIOCDEVPRIVATE, &ifr);
    return err;
}



/* free the T_AssociatedDevice and remove it from the list */
void wld_ad_destroy_associatedDevice(T_AccessPoint* pAP, int index) {
    int i;

    SAH_TRACEZ_IN(ME);
    if(0 == pAP->AssociatedDeviceNumberOfEntries) {
        SAH_TRACEZ_ERROR(ME, "len(AssociatedDevice[])==0");
        SAH_TRACEZ_OUT(ME);
        return;
    }

    if(!pAP->AssociatedDevice[index]) {
        SAH_TRACEZ_ERROR(ME, "AssociatedDevice[%d]==0 !", index);
        SAH_TRACEZ_OUT(ME);
        return;
    }

    wld_apRssiMon_destroyStaHistory(pAP->AssociatedDevice[index]);
    free(pAP->AssociatedDevice[index]);

    for(i = index; i < (pAP->AssociatedDeviceNumberOfEntries - 1); i++) {
        pAP->AssociatedDevice[i] = pAP->AssociatedDevice[i + 1];
    }
    pAP->AssociatedDevice[pAP->AssociatedDeviceNumberOfEntries - 1] = NULL;

    pAP->AssociatedDeviceNumberOfEntries--;
    SAH_TRACEZ_OUT(ME);
}

void wld_ad_init_oui(wld_assocDev_capabilities_t* caps) {
    ASSERTS_NOT_NULL(caps, , ME, "NULL");
    size_t i = 0;
    for(i = 0; i < WLD_MAX_OUI_NUM; i++) {
        memset(&caps->vendorOUI.oui[i], 0, sizeof(uint8_t) * SWL_OUI_BYTE_LEN);
    }
    caps->vendorOUI.count = 0;
}

/* create T_AssociatedDevice and populate MACAddress and Name fields */
T_AssociatedDevice* wld_ad_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    T_AssociatedDevice* pAD;
    SAH_TRACEZ_INFO(ME, "%s: create sta " SWL_MAC_FMT, pAP->name, SWL_MAC_ARG(macAddress->bMac));

    if(pAP->AssociatedDeviceNumberOfEntries >= MAXNROF_STAENTRY) {
        SAH_TRACEZ_ERROR(ME, "Maximum number of associated devices reached! (%d)", MAXNROF_STAENTRY);
        SAH_TRACEZ_OUT(ME);
        return NULL;
    }

    pAD = (T_AssociatedDevice*) calloc(1, sizeof(T_AssociatedDevice));
    if(!pAD) {
        SAH_TRACEZ_INFO(ME, "calloc failed! %p", pAD);
        SAH_TRACEZ_OUT(ME);
        return NULL;
    }

    memcpy(pAD->MACAddress, macAddress->bMac, ETHER_ADDR_LEN);

    SWL_MAC_BIN_TO_CHAR(pAD->Name, pAD->MACAddress);
    pAD->deviceType = pAP->defaultDeviceType;
    pAD->devicePriority = 1;
    pAD->Active = 0;
    pAD->Inactive = 0;
    pAD->AuthenticationState = 0;
    pAP->AssociatedDevice[pAP->AssociatedDeviceNumberOfEntries] = pAD;
    pAP->AssociatedDeviceNumberOfEntries++;
    pAD->latestStateChangeTime = swl_time_getRealSec();
    pAD->associationTime = swl_time_getMonoSec();
    pAD->MaxDownlinkRateReached = 0;
    pAD->MaxUplinkRateReached = 0;
    pAD->minSignalStrength = 0;
    pAD->maxSignalStrength = -200;
    pAD->meanSignalStrength = 0;
    pAD->meanSignalStrengthLinearAccumulator = 0;
    pAD->meanSignalStrengthExpAccumulator = 0;
    pAD->nrMeanSignalStrength = 0;

    size_t i = 0;
    for(i = 0; i < MAX_NR_ANTENNA; i++) {
        pAD->SignalStrengthByChain[i] = DEFAULT_BASE_RSSI;
    }
    memset(&pAD->assocCaps, 0, sizeof(wld_assocDev_capabilities_t));
    wld_ad_init_oui(&pAD->assocCaps);
    memset(&pAD->probeReqCaps, 0, sizeof(wld_assocDev_capabilities_t));
    wld_ad_init_oui(&pAD->probeReqCaps);
    pAD->AvgSignalStrengthByChain = 0;

    wld_apRssiMon_createStaHistory(pAD, pAP->rssiEventing.historyLen);

    return pAD;
}

void wld_vap_mark_all_stations_unseen(T_AccessPoint* pAP) {
    int i;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null device %i %s", i, pAP->alias);
            return;
        }
        pAD->seen = false;
    }
}

void wld_vap_update_seen(T_AccessPoint* pAP) {
    int i;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD->seen && !pAD->Active) {
            SAH_TRACEZ_INFO(ME, "Activating sta %s based on seen", pAD->Name);
            wld_ad_add_connection_try(pAP, pAD);
        } else if(!pAD->seen && pAD->Active) {
            SAH_TRACEZ_INFO(ME, "Deactivating sta %s based on seen", pAD->Name);
            wld_ad_add_disconnection(pAP, pAD);
        }
    }
    wld_vap_cleanup_stationlist(pAP);
}

void wld_vap_remove_all(T_AccessPoint* pAP) {
    int i;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD->Active) {
            wld_ad_add_disconnection(pAP, pAD);
            SAH_TRACEZ_ERROR(ME, "Deactivating sta %s remove all", pAD->Name);
        }
    }
    wld_vap_cleanup_stationlist(pAP);
}

swl_macBin_t* wld_ad_getMacBin(T_AssociatedDevice* pAD) {
    return (swl_macBin_t*) pAD->MACAddress;
}

T_AssociatedDevice* wld_vap_find_asociatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");
    ASSERT_NOT_NULL(macAddress, NULL, ME, "NULL");
    int i;
    T_AssociatedDevice* pAD;

    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];

        if(swl_mac_binMatches(macAddress, wld_ad_getMacBin(pAD))) {
            return pAD;
        }
    }
    return NULL;
}

T_AssociatedDevice* wld_vap_findOrCreateAssociatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    if(pAD != NULL) {
        return pAD;
    }
    return wld_ad_create_associatedDevice(pAP, macAddress);
}

static void wld_update_station_stats(T_AccessPoint* pAP) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD->SignalStrength < pAD->minSignalStrength) {
            pAD->minSignalStrength = pAD->SignalStrength;
        }
        if(pAD->SignalStrength > pAD->maxSignalStrength) {
            pAD->maxSignalStrength = pAD->SignalStrength;
        }
        pAD->nrMeanSignalStrength++;
        pAD->meanSignalStrengthLinearAccumulator += pAD->SignalStrength;
        pAD->meanSignalStrength = ((int) pAD->meanSignalStrengthLinearAccumulator / pAD->nrMeanSignalStrength);
        pAD->meanSignalStrengthExpAccumulator = wld_util_performFactorStep(pAD->meanSignalStrengthExpAccumulator, pAD->SignalStrength, 50);
        if((uint32_t) pAD->LastDataDownlinkRate > pAD->MaxDownlinkRateReached) {
            pAD->MaxDownlinkRateReached = pAD->LastDataDownlinkRate;
        }
        if((uint32_t) pAD->LastDataUplinkRate > pAD->MaxUplinkRateReached) {
            pAD->MaxUplinkRateReached = pAD->LastDataUplinkRate;
        }
    }
}

static void s_addStaStatsValues(T_AccessPoint* pAP, int ret, amxc_var_t* retval) {
    T_Radio* pRad = pAP->pRadio;
    if(!pAP->enable || !pRad->enable || (ret < 0)) {
        //remove all stations if AP or Radio is disabled or station stats returns error
        wld_vap_remove_all(pAP);
    }

    wld_update_station_stats(pAP);

    wld_vap_sync_assoclist(pAP);

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
        if((pAD == NULL) || (pAD->object == NULL)) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid sta %u", pAP->alias, i);
            continue;
        }

        amxc_var_t tmpVar;
        amxc_var_init(&tmpVar);
        amxd_object_get_params(pAD->object, &tmpVar, amxd_dm_access_private);

        amxc_var_add_new_amxc_htable_t(retval, &tmpVar.data.vm);

        amxc_var_clean(&tmpVar);
    }

}

static void s_staStatsDoneHandler(T_AccessPoint* pAP, bool success) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_EQUALS(pAP->stationsStatsState.call_id, 0, , ME, "%s no fcall", pAP->alias);
    ASSERT_TRUE(pAP->stationsStatsState.running, , ME, "%s no fcall", pAP->alias);

    amxc_var_t values;
    amxc_var_init(&values);

    s_addStaStatsValues(pAP, success, &values);

    amxd_function_deferred_done(pAP->stationsStatsState.call_id,
                                success ? amxd_status_ok : amxd_status_unknown_error,
                                NULL,
                                &values);

    amxc_var_clean(&values);
    pAP->stationsStatsState.call_id = 0;
}

void wld_station_stats_done(T_AccessPoint* pAP, bool success) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_TRUE(pAP->stationsStatsState.running, , ME, "Stations stats not on going");

    if(pAP->stationsStatsState.call_id != 0) {
        s_staStatsDoneHandler(pAP, success);
    }
    if(pAP->stationsStatsState.timer != NULL) {
        amxp_timer_delete(&pAP->stationsStatsState.timer);
    }

    pAP->stationsStatsState.running = false;
}

static void s_staStatsTimeout(amxp_timer_t* timer _UNUSED, void* priv) {
    T_AccessPoint* pAP = (T_AccessPoint*) priv;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_station_stats_done(pAP, false);

}

static void s_staStatsCancel(uint64_t call_id _UNUSED, void* priv) {
    T_AccessPoint* pAP = (T_AccessPoint*) priv;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    pAP->stationsStatsState.call_id = 0;
    wld_station_stats_done(pAP, false);
}

amxd_status_t _getStationStats(amxd_object_t* obj_AP,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args _UNUSED,
                               amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    T_AccessPoint* pAP = obj_AP->priv;

    if(!debugIsVapPointer(pAP)) {
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_INFO(ME, "pAP = %p", pAP);

    if(pAP->stationsStatsState.running) {
        SAH_TRACEZ_ERROR(ME, "%s: getStationStats is already running", pAP->alias);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    swl_rc_ne ret = pAP->pFA->mfn_wvap_get_station_stats(pAP);
    SAH_TRACEZ_INFO(ME, "%s: sta stats %i", pAP->alias, ret);
    if(ret == SWL_RC_CONTINUE) {
        pAP->stationsStatsState.running = true;
        amxp_timer_t* timer = NULL;
        amxp_timer_new(&timer, s_staStatsTimeout, pAP);
        amxp_timer_start(timer, STATIONS_STATS_TIMEOUT);

        pAP->stationsStatsState.timer = timer;

        amxd_function_defer(func, &pAP->stationsStatsState.call_id, retval, s_staStatsCancel, pAP);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_deferred;
    }

    s_addStaStatsValues(pAP, ret, retval);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _getSingleStationStats(amxd_object_t* const object,
                                     amxd_param_t* const param,
                                     amxd_action_t reason,
                                     const amxc_var_t* const args,
                                     amxc_var_t* const action_retval,
                                     void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_ok;
    if(reason != action_object_read) {
        status = amxd_status_function_not_implemented;
        goto exit;
    }

    T_AssociatedDevice* pAD = object->priv;
    if(pAD == NULL) {
        SAH_TRACEZ_INFO(ME, "pAD is NULL, no device present");
        status = amxd_status_unknown_error;
        goto exit;
    }

    T_AccessPoint* pAP = (T_AccessPoint*) amxd_object_get_parent(amxd_object_get_parent(pAD->object))->priv;
    swl_rc_ne ret = pAP->pFA->mfn_wvap_get_single_station_stats(pAD);

    if(ret >= SWL_RC_OK) {
        WLD_SET_VAR_CSTRING(object, "OperatingStandard", swl_radStd_unknown_str[pAD->operatingStandard]);
        WLD_SET_VAR_UINT32(object, "LastDataDownlinkRate", pAD->LastDataDownlinkRate);
        WLD_SET_VAR_UINT32(object, "LastDataUplinkRate", pAD->LastDataUplinkRate);
        WLD_SET_VAR_UINT32(object, "SignalStrength", pAD->SignalStrength);
        WLD_SET_VAR_UINT32(object, "Noise", pAD->Noise);
        WLD_SET_VAR_UINT32(object, "TxBytes", pAD->TxBytes);
        WLD_SET_VAR_UINT32(object, "RxBytes", pAD->RxBytes);
        WLD_SET_VAR_UINT32(object, "TxPacketCount", pAD->TxPacketCount);
        WLD_SET_VAR_UINT32(object, "RxPacketCount", pAD->RxPacketCount);
        WLD_SET_VAR_UINT32(object, "TxErrors", pAD->TxFailures);
        WLD_SET_VAR_UINT32(object, "Tx_Retransmissions", pAD->Tx_Retransmissions);
        WLD_SET_VAR_UINT32(object, "Tx_RetransmissionsFailed", pAD->Tx_RetransmissionsFailed);
        WLD_SET_VAR_BOOL(object, "AuthenticationState", pAD->AuthenticationState);

        swl_time_objectParamSetReal(object, "LastStateChange", pAD->latestStateChangeTime);
        swl_time_objectParamSetMono(object, "AssociationTime", pAD->associationTime);
        swl_time_objectParamSetMono(object, "DisassociationTime", pAD->disassociationTime);
    }

    status = amxd_action_object_read(object, param, reason, args, action_retval, priv);

exit:
    SAH_TRACEZ_OUT(ME);
    return status;
}


/**
 * Return whether or not the given accesspoint has a far station.
 * @param pAP
 *  the accesspoint to check
 * @param threshold
 *  the RSSI threshold in dbm below which a station is considered far
 */
bool wld_ad_has_far_station(T_AccessPoint* pAP, int threshold) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(pAD->SignalStrength < threshold) {
            return true;
        }
    }
    return false;
}

bool wld_ad_has_active_stations(T_AccessPoint* pAP) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(pAD->Active == 1) {
            return true;
        }
    }
    return false;
}

bool wld_ad_hasAuthenticatedStations(T_AccessPoint* pAP) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(pAD->AuthenticationState == 1) {
            return true;
        }
    }
    return false;
}


bool wld_ad_has_active_video_stations(T_AccessPoint* pAP) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(( pAD->Active == 1) && ( pAD->deviceType == DEVICE_TYPE_VIDEO)) {
            return true;
        }
    }
    return false;
}


bool wld_rad_has_active_stations(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(wld_ad_has_active_stations(pAP)) {
            return true;
        }
    }
    return false;
}

bool wld_rad_has_active_video_stations(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(wld_ad_has_active_video_stations(pAP)) {
            return true;
        }
    }
    return false;
}


/**
 * Return whether or not the given accesspoint has a far station.
 * @param pAP
 *  the accesspoint to check
 * @param threshold
 *  the RSSI threshold in dbm below which a station is considered far
 */
int wld_ad_get_nb_active_stations(T_AccessPoint* pAP) {
    return pAP->ActiveAssociatedDeviceNumberOfEntries;
}

int wld_ad_get_nb_active_video_stations(T_AccessPoint* pAP) {
    int i = 0;
    T_AssociatedDevice* pAD;
    int nr_sta = 0;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(( pAD->Active == 1) && ( pAD->deviceType == DEVICE_TYPE_VIDEO)) {
            nr_sta++;
        }
    }
    return nr_sta;
}

int wld_rad_get_nb_active_stations(T_Radio* pRad) {
    return pRad->currentStations;
}

int wld_rad_get_nb_active_video_stations(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;
    int nr_sta = 0;

    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        nr_sta += wld_ad_get_nb_active_video_stations(pAP);
    }
    return nr_sta;
}

bool wld_ad_has_assocdev(T_AccessPoint* pAP, const unsigned char macAddress[ETHER_ADDR_LEN]) {
    int i = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(( memcmp(macAddress, pAD->MACAddress, ETHER_ADDR_LEN) == 0) && pAD->Active) {
            return true;
        }
    }
    return false;
}


T_AccessPoint* wld_rad_get_associated_ap(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]) {
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(wld_ad_has_assocdev(pAP, macAddress)) {
            return pAP;
        }

    }
    return NULL;
}

wld_assocDevInfo_t wld_rad_get_associatedDeviceInfo(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]) {
    T_AccessPoint* pAP = NULL;
    T_AssociatedDevice* pAD = NULL;
    wld_assocDevInfo_t assocDevInfo;
    assocDevInfo.pRad = pRad;
    assocDevInfo.pAP = NULL;
    assocDevInfo.pAD = NULL;

    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        pAD = wld_vap_find_asociatedDevice(pAP, (void*) macAddress);
        if(pAD) {
            assocDevInfo.pAP = pAP;
            assocDevInfo.pAD = pAD;
            break;
        }
    }
    return assocDevInfo;
}


bool wld_rad_has_assocdev(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]) {
    return (wld_rad_get_associated_ap(pRad, macAddress) != NULL);
}

/**
 * Return the number of far stations.
 * @param pAP
 *  the accesspoint to check
 * @param threshold
 *  the RSSI threshold in dbm below which a station is considered far
 */
int wld_ad_get_nb_far_station(T_AccessPoint* pAP, int threshold) {
    int i = 0;
    int count = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(pAD->SignalStrength < threshold) {
            count++;
        }
    }
    return count;
}


void wld_ad_checkRoamSta(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    amxc_llist_it_t* rad_it = NULL;
    amxc_llist_for_each(rad_it, &g_radios) {
        T_Radio* testRad = amxc_llist_it_get_data(rad_it, T_Radio, it);
        amxc_llist_it_t* ap_it = NULL;
        amxc_llist_for_each(ap_it, &testRad->llAP) {
            T_AccessPoint* testAp = amxc_llist_it_get_data(ap_it, T_AccessPoint, it);
            if(testAp == pAP) {
                continue;
            }
            if(!wld_ad_has_assocdev(testAp, pAD->MACAddress)) {
                continue;
            }
            wld_rad_incrementCounterStr(pRad, &pRad->genericCounters, WLD_RAD_EV_DOUBLE_ASSOC,
                                        "%s @ %s => %s", pAD->Name, testAp->alias, pAP->alias);
            if(pRad->kickRoamStaEnabled && testRad->kickRoamStaEnabled) {
                SAH_TRACEZ_ERROR(ME, "Kicking %s @ %s, connecting @ %s",
                                 pAD->Name, testAp->alias, pAP->alias);
                int retCode = testAp->pFA->mfn_wvap_clean_sta(testAp, pAD->Name, strlen(pAD->Name));
                if(retCode == WLD_ERROR_NOT_IMPLEMENTED) {
                    testAp->pFA->mfn_wvap_kick_sta(testAp, (char*) pAD->Name, strlen(pAD->Name), SET);
                }
            }
        }
    }
}

void wld_ad_add_connection_try(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTI_FALSE(pAD->Active, , ME, "%s : already auth %s", pAP->alias, pAD->Name);

    SAH_TRACEZ_INFO(ME, "%s : Try %s", pAP->alias, pAD->Name);
    pAD->Active = 1;
    pAD->AuthenticationState = 0;
    pAD->Inactive = 0;
    pAD->hadSecFailure = false;
    pAD->latestStateChangeTime = swl_time_getRealSec();
    pAD->associationTime = swl_time_getRealSec();

    //kick from all other AP's
    wld_ad_checkRoamSta(pAP, pAD);

    pAD->associationTime = swl_time_getMonoSec();
    wld_vap_sync_assoclist(pAP);
}

static void s_addDcEntry(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    wld_ad_dcLog_t* log = s_findDcEntry(pAP, (swl_macBin_t*) &pAD->MACAddress);
    bool hasLog = (log == NULL);
    if(hasLog) {
        log = swl_unLiList_allocElement(&pAP->staDcList.list);
        memcpy(&log->macAddress, &pAD->MACAddress, ETHER_ADDR_LEN);
    }
    swl_timespec_getMono(&log->dcTime);
    SAH_TRACEZ_INFO(ME, "%s: addLog %s (had %u)", swl_typeMacBin_toBuf32(log->macAddress).buf,
                    swl_typeTimeSpecMono_toBuf32(log->dcTime).buf, hasLog);
}


static void s_add_dc_sta(T_AccessPoint* pAP, T_AssociatedDevice* pAD, bool failSec) {
    ASSERTI_TRUE(pAD->Active, , ME, "%s : inactive sta dc %s", pAP->alias, pAD->Name);

    SAH_TRACEZ_INFO(ME, "%s : Dc %s Auth %u", pAP->alias, pAD->Name, pAD->AuthenticationState);
    if(!pAD->AuthenticationState && !pAD->hadSecFailure) {
        if(failSec) {
            s_incrementAssocCounter(pAP, "FailSecurity");
            pAD->hadSecFailure = true;
        } else {
            s_incrementAssocCounter(pAP, "Fail");
        }
    } else if(pAD->AuthenticationState) {
        s_incrementAssocCounter(pAP, "Disconnect");
        s_addDcEntry(pAP, pAD);
    }
    if(pAD->Active) {
        uint32_t timeOnline = swl_time_getMonoSec() - pAD->associationTime;
        SAH_TRACEZ_WARNING(ME, "%s: disassoc sta %s - assoc @ %s - active %u sec - SNR %i",
                           pAP->alias, pAD->Name, swl_typeTimeMono_toBuf32(pAD->associationTime).buf,
                           timeOnline, pAD->SignalNoiseRatio);
        pAD->latestStateChangeTime = swl_time_getRealSec();
        pAD->disassociationTime = swl_time_getMonoSec();

        wld_ad_dcLog_t* log = s_findDcEntry(pAP, (swl_macBin_t*) &pAD->MACAddress);
        if(log == NULL) {
            log = swl_unLiList_allocElement(&pAP->staDcList.list);
        }
        log->dcTime = swl_timespec_getMonoVal();
        memcpy(&log->macAddress, &pAD->MACAddress, ETHER_ADDR_LEN);
    }
    pAD->AuthenticationState = 0;
    pAD->Active = 0;
    pAD->Inactive = 0;
    wld_ad_remove_assocdev_from_bridge(pAP, pAD);
}

/**
 * Notify the wld that a given station has successfully connected.
 * This will set the AuthenticationState to 1, indication successfull connection.
 * Note that even if security is off, if it succeeds to connect, we consider it authenticated.
 */
void wld_ad_add_connection_success(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTI_FALSE(pAD->AuthenticationState, , ME, "%s : already auth %s", pAP->alias, pAD->Name);

    wld_ad_dcLog_t* entry = s_findDcEntry(pAP, (swl_macBin_t*) &pAD->MACAddress);
    SAH_TRACEZ_INFO(ME, "%s : Success %s (dc %s)", pAP->alias, pAD->Name, entry != NULL ?
                    swl_typeTimeSpecMono_toBuf32(entry->dcTime).buf : "NA");
    pAD->AuthenticationState = 1;
    pAD->Active = 1;
    pAD->Inactive = 0;
    pAD->latestStateChangeTime = swl_time_getRealSec();
    s_incrementAssocCounter(pAP, "Success");
    //Update device type when connection succeeds.
    pAP->pFA->mfn_wvap_update_assoc_dev(pAP, pAD);

    if(entry != NULL) {
        s_dcEntryConnected(pAP, entry);
    }
}


/**
 * Notify the wld that a station is disconnected.
 * This will set active and authenticated to 0.
 */
void wld_ad_add_disconnection(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    wld_apRssiMon_sendHistoryOnDisassocEvent(pAP, pAD);
    s_add_dc_sta(pAP, pAD, false);
}

/**
 * Notify the wld that a security failure has been detected.
 * This will deactivate the given station.
 */
void wld_ad_add_sec_failure(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    s_add_dc_sta(pAP, pAD, true);
}

/**
 * Notify the wld that a security failure has been detected, but do not deactivate station.
 */
void wld_ad_add_sec_failNoDc(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    if(pAD->Active && !pAD->hadSecFailure) {
        s_incrementAssocCounter(pAP, "FailSecurity");
        pAD->hadSecFailure = true;
    }
}

amxd_status_t _doAssociationCountReset(amxd_object_t* object _UNUSED,
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

    bool valid = amxc_var_dyncast(bool, args);

    if(valid) {  // Only when TRUE
        if(!(pAP && debugIsVapPointer(pAP))) {
            return amxd_status_unknown_error;
        }

        amxd_object_t* object = (amxd_object_t*) pAP->pBus;
        amxd_object_t* counter = amxd_object_findf(object, "AssociationCount");

        // Reset On Write
        amxd_object_set_bool(counter, "ResetCounters", 0);
        // Reset Association Counters on 0!
        amxd_object_set_uint32_t(counter, "Success", 0);
        amxd_object_set_uint32_t(counter, "Fail", 0);
        amxd_object_set_uint32_t(counter, "FailSecurity", 0);
        amxd_object_set_uint32_t(counter, "Disconnect", 0);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_getOUIValue(amxc_string_t* output, swl_oui_list_t* vendorOui) {
    ASSERTI_TRUE(vendorOui->count != 0, , ME, "No OUI Vendor");
    char buffer[SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM];
    memset(buffer, 0, sizeof(buffer));
    swl_typeOui_arrayToChar(buffer, SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM, vendorOui->oui, vendorOui->count);
    amxc_string_set(output, buffer);
}

void wld_ad_syncCapabilities(amxd_object_t* object, wld_assocDev_capabilities_t* caps) {
    ASSERT_NOT_NULL(object, , ME, "NULL");
    ASSERT_NOT_NULL(caps, , ME, "NULL");
    char mcsList[100];
    memset(mcsList, 0, sizeof(mcsList));
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedMCS.mcs, caps->supportedMCS.mcsNbr);
    amxd_object_set_cstring_t(object, "SupportedMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedVhtMCS.nssMcsNbr, caps->supportedVhtMCS.nssNbr);
    amxd_object_set_cstring_t(object, "SupportedVhtMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHeMCS.nssMcsNbr, caps->supportedHeMCS.nssNbr);
    amxd_object_set_cstring_t(object, "SupportedHeMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHe160MCS.nssMcsNbr, caps->supportedHe160MCS.nssNbr);
    amxd_object_set_cstring_t(object, "SupportedHe160MCS", mcsList);

    amxc_string_t TBufStr;
    amxc_string_init(&TBufStr, 0);
    s_getOUIValue(&TBufStr, &caps->vendorOUI);
    amxd_object_set_cstring_t(object, "VendorOUI", amxc_string_get(&TBufStr, 0));
    amxd_object_set_cstring_t(object, "SecurityModeEnabled", cstr_AP_ModesSupported[caps->currentSecurity]);
    amxd_object_set_cstring_t(object, "LinkBandwidth", swl_bandwidth_unknown_str[caps->linkBandwidth]);
    amxd_object_set_cstring_t(object, "EncryptionMode", cstr_AP_EncryptionMode[caps->encryptMode]);
    bitmask_to_string(&TBufStr, swl_staCapHt_str, ',', caps->htCapabilities);
    amxd_object_set_cstring_t(object, "HtCapabilities", amxc_string_get(&TBufStr, 0));
    bitmask_to_string(&TBufStr, swl_staCapVht_str, ',', caps->vhtCapabilities);
    amxd_object_set_cstring_t(object, "VhtCapabilities", amxc_string_get(&TBufStr, 0));
    bitmask_to_string(&TBufStr, swl_staCapHe_str, ',', caps->heCapabilities);
    amxd_object_set_cstring_t(object, "HeCapabilities", amxc_string_get(&TBufStr, 0));
    char frequencyCapabilitiesStr[128] = {0};
    swl_conv_maskToChar(frequencyCapabilitiesStr, sizeof(frequencyCapabilitiesStr), caps->freqCapabilities, swl_freqBandExt_unknown_str, SWL_FREQ_BAND_EXT_MAX);
    amxd_object_set_cstring_t(object, "FrequencyCapabilities", frequencyCapabilitiesStr);
    amxc_string_clean(&TBufStr);
}

int32_t wld_ad_getAvgSignalStrengthByChain(T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAD, 0, ME, "NULL");
    int nrValidAntennas = 0;
    double avgRssiByChain = 0.0;
    for(int i = 0; i < MAX_NR_ANTENNA; i++) {
        if((pAD->SignalStrengthByChain[i] != 0) && (pAD->SignalStrengthByChain[i] != DEFAULT_BASE_RSSI)) {
            nrValidAntennas++;
            avgRssiByChain += pow(10.0, pAD->SignalStrengthByChain[i] / 10.0);
        }
    }
    ASSERTS_NOT_EQUALS(avgRssiByChain, 0.0, 0, ME, "No SignalStrengthByChain available");
    ASSERTS_NOT_EQUALS(nrValidAntennas, 0, 0, ME, "No SignalStrengthByChain available");
    avgRssiByChain = 10.0 * log10(avgRssiByChain / nrValidAntennas);
    return (int32_t) avgRssiByChain;
}

const char* wld_ad_getMode(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAD, "Unknown", ME, "NULL");
    const char* mode = NULL;
    if(pAD->operatingStandard == SWL_RADSTD_N) {
        if(wld_rad_is_24ghz(pAP->pRadio)) {
            mode = SWL_RADSTD_LEGACY_B_G_N;
        } else {
            mode = SWL_RADSTD_LEGACY_A_N;
        }
    }
    if(mode == NULL) {
        mode = (char*) swl_table_getMatchingValue(&modeToStandard, 1, 0, &pAD->operatingStandard);
    }
    return mode != NULL ? mode : "Unknown";
}

void wld_ad_getHeMCS(uint16_t he_mcs, wld_sta_supMCS_adv_t* supportedHeMCS) {
    int nss;
    uint8_t mcsList[] = {7, 9, 11, 0};
    for(nss = 0; nss < WLD_MAX_NSS; nss++) {
        uint8_t maxHeMCS = he_mcs >> (nss * D11_HE_MCS_SUBFIELD_SIZE) & D11_HE_MCS_SUBFIELD_MASK;
        if(maxHeMCS > 3) {
            supportedHeMCS->nssMcsNbr[nss] = 0;
        } else {
            supportedHeMCS->nssMcsNbr[nss] = mcsList[maxHeMCS];
        }
        if(supportedHeMCS->nssMcsNbr[nss] == 0) {
            supportedHeMCS->nssNbr = (uint8_t) nss;
            break;
        }
    }
}

void wld_assocDev_initAp(T_AccessPoint* pAP) {
    swl_unLiTable_initExt(&pAP->staDcList, &wld_ad_dcLogTupleType, 3);
    swl_unLiList_setKeepsLastBlock(&pAP->staDcList.list, true);
    amxd_object_t* templateObj = amxd_object_get(pAP->pBus, "AssociationCount.FastReconnectTypes");
    for(uint32_t i = 0; i < WLD_FAST_RECONNECT_MAX; i++) {
        amxd_object_t* instObj = NULL;
        amxd_object_new_instance(&instObj, templateObj, fastReconnectTypes[i], 0, NULL);
        amxd_object_set_cstring_t(instObj, "Type", fastReconnectTypes[i]);
    }
}


void wld_assocDev_cleanAp(T_AccessPoint* pAP) {
    swl_unLiTable_destroy(&pAP->staDcList);
}

void wld_assocDev_listRecentDisconnects(T_AccessPoint* pAP, amxc_var_t* variant) {
    swl_unLiTable_toListOfMaps(&pAP->staDcList, variant, tupleNames);
}

