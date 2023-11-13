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
#include "swla/swla_tupleType.h"
#include "swla/swla_oui.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_nl80211.h"
#include "wld_dm_trans.h"

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

#define X_WLD_AD_DC_LOG_T(X, Y) \
    X(Y, gtSwl_type_macBin, macAddress) \
    X(Y, gtSwl_type_timeSpecMono, dcTime)
SWL_TT(tWld_ad_dcLog, wld_ad_dcLog_t, X_WLD_AD_DC_LOG_T, )
char* tupleNames[] = {"macAddress", "dcTime"};

const char* fastReconnectTypes[WLD_FAST_RECONNECT_MAX] = {"Default", "OnStateChange", "OnScan", "User"};

static void s_incrementObjCounter(amxd_object_t* obj, char* counterName) {
    uint32_t val = amxd_object_get_uint32_t(obj, counterName, NULL);
    val++;

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(obj, &trans, , ME, "%s : trans init failure", counterName);

    amxd_trans_set_uint32_t(&trans, counterName, val);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", counterName);

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


#define X_WLD_AD_DISASSOC_EVENT(X, Y) \
    X(Y, gtSwl_type_macChar, mac, "MACAddress") \
    X(Y, gtSwl_type_timeReal, associationTime, "AssociationTime") \
    X(Y, gtSwl_type_timeReal, disassociationTime, "DisassociationTime") \
    X(Y, gtSwl_type_timeSpecMono, lastSampleTime, "LastSampleTime") \
    X(Y, gtSwl_type_uint32, deauthReason, "DeauthReason") \
    X(Y, gtSwl_type_int32, noise, "Noise") \
    X(Y, gtSwl_type_int32, signalStrength, "SignalStrength") \
    X(Y, gtSwl_type_int32, avgSignalStrengthByChain, "AvgSignalStrengthByChain") \
    X(Y, gtSwl_type_int32, minSignalStrength, "MinSignalStrength") \
    X(Y, gtSwl_type_timeMono, minSignalStrengthTime, "MinSignalStrengthTime") \
    X(Y, gtSwl_type_int32, maxSignalStrength, "MaxSignalStrength") \
    X(Y, gtSwl_type_timeMono, maxSignalStrengthTime, "MaxSignalStrengthTime") \
    X(Y, gtSwl_type_int32, signalNoiseRatio, "SignalNoiseRatio") \
    X(Y, gtSwl_type_uint32, lastDataDownlinkRate, "LastDataDownlinkRate") \
    X(Y, gtSwl_type_uint32, lastDataUplinkRate, "LastDataUplinkRate") \
    X(Y, gtSwl_type_mcs, lastUplinkMcs, "LastUplinkMcs") \
    X(Y, gtSwl_type_timeMono, lastUplinkMcsTime, "LastUplinkMcsTime") \
    X(Y, gtSwl_type_mcs, lastDownlinkMcs, "LastDownlinkMcs") \
    X(Y, gtSwl_type_timeMono, lastDownlinkMcsTime, "LastDownlinkMcsTime") \
    X(Y, gtSwl_type_uint32, rxFrameCount, "RxFrameCount") \
    X(Y, gtSwl_type_uint32, txFrameCount, "TxFrameCount") \
    X(Y, gtSwl_type_uint32, rxRetransmissions, "RxRetransmissions") \
    X(Y, gtSwl_type_uint32, txRetransmissions, "TxRetransmissions") \
    X(Y, gtSwl_type_uint32, rxRetransmissionsFailed, "RxRetransmissionsFailed") \
    X(Y, gtSwl_type_uint32, txRetransmissionsFailed, "TxRetransmissionsFailed") \
    X(Y, gtSwl_type_uint32, rxPacketCount, "RxPacketCount") \
    X(Y, gtSwl_type_uint32, txPacketCount, "TxPacketCount") \
    X(Y, gtSwl_type_uint32, rxUnicastPacketCount, "RxUnicastPacketCount") \
    X(Y, gtSwl_type_uint32, txUnicastPacketCount, "TxUnicastPacketCount") \
    X(Y, gtSwl_type_uint32, rxMulticastPacketCount, "RxMulticastPacketCount") \
    X(Y, gtSwl_type_uint32, txMulticastPacketCount, "TxMulticastPacketCount") \
    X(Y, gtSwl_type_uint64, rxBytes, "RxBytes") \
    X(Y, gtSwl_type_uint64, txBytes, "TxBytes") \
    X(Y, gtSwl_type_uint32, rxErrors, "RxErrors") \
    X(Y, gtSwl_type_uint32, txErrors, "TxErrors") \
    X(Y, gtSwl_type_uint32, inactive, "Inactive") \
    X(Y, gtSwl_type_timeSpecMono, recentSampleTime, "RecentSampleTime") \
    X(Y, gtSwl_type_int32, recentMinSignalStrength, "RecentMinSignalStrength") \
    X(Y, gtSwl_type_int32, recentMaxSignalStrength, "RecentMaxSignalStrength") \
    X(Y, gtSwl_type_int32, recentMinNoise, "RecentMinNoise") \
    X(Y, gtSwl_type_int32, recentMaxNoise, "RecentMaxNoise") \
    X(Y, gtSwl_type_int32, recentMinSNR, "RecentMinSNR") \
    X(Y, gtSwl_type_int32, recentMaxSNR, "RecentMaxSNR") \
    X(Y, gtSwl_type_uint64, recentTxBytes, "RecentTxBytes") \
    X(Y, gtSwl_type_uint64, recentRxBytes, "RecentRxBytes") \
    X(Y, gtSwl_type_uint32, recentTxPacketCount, "RecentTxPacketCount") \
    X(Y, gtSwl_type_uint32, recentRxPacketCount, "RecentRxPacketCount") \
    X(Y, gtSwl_type_uint32, recentTxError, "RecentTxError") \
    X(Y, gtSwl_type_uint32, recentRxError, "RecentRxError") \
    X(Y, gtSwl_type_uint32, recentTxFrameCount, "RecentTxFrameCount") \
    X(Y, gtSwl_type_uint32, recentRxFrameCount, "RecentRxFrameCount") \
    X(Y, gtSwl_type_uint32, recentTxRetransmissions, "RecentTxRetransmissions") \
    X(Y, gtSwl_type_uint32, recentTxRetransmissionsFailed, "RecentTxRetransmissionsFailed") \
    X(Y, gtSwl_type_uint32, recentRxRetransmissions, "RecentRxRetransmissions") \
    X(Y, gtSwl_type_uint32, recentRxRetransmissionsFailed, "RecentRxRetransmissionsFailed") \

SWL_NTT(gtWld_ad_disassocEvent, wld_ad_disassocEvent_t, X_WLD_AD_DISASSOC_EVENT, )

static void s_sendDisassocNotification(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");



    wld_ad_disassocEvent_t disassocEvent;
    memset(&disassocEvent, 0, sizeof(wld_ad_disassocEvent_t));
    swl_mac_binToChar(&disassocEvent.mac, (swl_macBin_t*) &pAD->MACAddress[0]);
    disassocEvent.associationTime = pAD->associationTime;
    disassocEvent.disassociationTime = pAD->disassociationTime;
    disassocEvent.deauthReason = pAD->lastDeauthReason;
    disassocEvent.lastSampleTime = pAD->lastSampleTime;

    disassocEvent.signalStrength = pAD->SignalStrength;
    disassocEvent.minSignalStrength = pAD->minSignalStrength;
    disassocEvent.minSignalStrengthTime = pAD->minSignalStrengthTime;
    disassocEvent.maxSignalStrength = pAD->maxSignalStrength;
    disassocEvent.maxSignalStrengthTime = pAD->maxSignalStrengthTime;
    disassocEvent.avgSignalStrengthByChain = pAD->AvgSignalStrengthByChain;
    disassocEvent.signalNoiseRatio = pAD->SignalNoiseRatio;
    disassocEvent.noise = pAD->noise;
    disassocEvent.lastDataDownlinkRate = pAD->LastDataDownlinkRate;
    disassocEvent.lastDataUplinkRate = pAD->LastDataUplinkRate;
    memcpy(&disassocEvent.lastDownlinkMcs, &pAD->lastNonLegacyDownlinkMCS, sizeof(swl_mcs_t));
    disassocEvent.lastDownlinkMcsTime = pAD->lastNonLegacyDownlinkTime;
    memcpy(&disassocEvent.lastUplinkMcs, &pAD->lastNonLegacyUplinkMCS, sizeof(swl_mcs_t));
    disassocEvent.lastUplinkMcsTime = pAD->lastNonLegacyUplinkTime;

    disassocEvent.rxFrameCount = pAD->RxFrameCount;
    disassocEvent.txFrameCount = pAD->TxFrameCount;
    disassocEvent.rxRetransmissions = pAD->Rx_Retransmissions;
    disassocEvent.txRetransmissions = pAD->Tx_Retransmissions;
    disassocEvent.rxRetransmissionsFailed = pAD->Rx_RetransmissionsFailed;
    disassocEvent.txRetransmissionsFailed = pAD->Tx_RetransmissionsFailed;
    disassocEvent.rxPacketCount = pAD->RxPacketCount;
    disassocEvent.txPacketCount = pAD->TxPacketCount;
    disassocEvent.rxUnicastPacketCount = pAD->RxUnicastPacketCount;
    disassocEvent.txUnicastPacketCount = pAD->TxUnicastPacketCount;
    disassocEvent.rxMulticastPacketCount = pAD->RxMulticastPacketCount;
    disassocEvent.txMulticastPacketCount = pAD->TxMulticastPacketCount;
    disassocEvent.rxBytes = pAD->RxBytes;
    disassocEvent.txBytes = pAD->TxBytes;
    disassocEvent.rxErrors = pAD->RxFailures;
    disassocEvent.txErrors = pAD->TxFailures;
    disassocEvent.inactive = pAD->Inactive;


    wld_staHistory_t* hist = wld_apRssiMon_getOldestStaSample(pAP, pAD);
    if((hist != NULL) && !swl_timespec_equals(&pAD->lastSampleTime, &hist->timestamp)) {
        disassocEvent.recentSampleTime = hist->timestamp;
        disassocEvent.recentTxBytes = pAD->TxBytes - hist->txBytes;
        disassocEvent.recentRxBytes = pAD->RxBytes - hist->rxBytes;
        disassocEvent.recentTxPacketCount = pAD->TxPacketCount - hist->txPacketCount;
        disassocEvent.recentRxPacketCount = pAD->RxPacketCount - hist->rxPacketCount;
        disassocEvent.recentTxError = pAD->TxFailures - hist->txError;
        disassocEvent.recentRxError = pAD->RxFailures - hist->rxError;
        disassocEvent.recentTxFrameCount = pAD->TxFrameCount - hist->txFrameCount;
        disassocEvent.recentRxFrameCount = pAD->RxFrameCount - hist->rxFrameCount;
        disassocEvent.recentTxRetransmissions = pAD->Tx_Retransmissions - hist->tx_Retransmissions;
        disassocEvent.recentTxRetransmissionsFailed = pAD->Tx_RetransmissionsFailed - hist->tx_RetransmissionsFailed;
        disassocEvent.recentRxRetransmissions = pAD->Rx_Retransmissions - hist->rx_Retransmissions;
        disassocEvent.recentRxRetransmissionsFailed = pAD->Rx_RetransmissionsFailed - hist->rx_RetransmissionsFailed;
    }
    wld_apRssiMon_signalRange_t sigRange;
    memset(&sigRange, 0, sizeof(wld_apRssiMon_signalRange_t));
    bool getSig = wld_apRssiMon_getMinMaxSignal(pAP, pAD, &sigRange);
    if(getSig) {
        disassocEvent.recentMinSignalStrength = sigRange.minRssi;
        disassocEvent.recentMaxSignalStrength = sigRange.maxRssi;
        disassocEvent.recentMinNoise = sigRange.minNoise;
        disassocEvent.recentMaxNoise = sigRange.maxNoise;
        disassocEvent.recentMinSNR = sigRange.minSNR;
        disassocEvent.recentMaxSNR = sigRange.maxSNR;
    }

    amxc_var_t myVar;
    amxc_var_init(&myVar);
    amxc_var_set_type(&myVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* tmpMap = amxc_var_add_key(amxc_htable_t, &myVar, "Data", NULL);

    swl_ntt_toMap(tmpMap, &gtWld_ad_disassocEvent, &disassocEvent);

    amxd_object_trigger_signal(pAP->pBus, "Disassociation", &myVar);

    amxc_var_clean(&myVar);
}


/**
 * Remove a given associated device from the bridge table.
 */
int wld_ad_remove_assocdev_from_bridge(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {

    T_Radio* pRad = pAP->pRadio;
    ASSERTI_FALSE(pRad->driverCfg.skipSocketIO, -1, ME, "%s skip SocketIO", pRad->Name);

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

    swl_str_copy(ifr.ifr_ifrn.ifrn_name, IFNAMSIZ, brName);
    ifr.ifr_ifru.ifru_data = (char*) args;

    err = ioctl(pRad->wlRadio_SK, SIOCDEVPRIVATE, &ifr);
    return err;
}

int wld_ad_getIndex(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        if(pAD == pAP->AssociatedDevice[i]) {
            return i;
        }
    }
    return -1;
}

bool wld_ad_destroy(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pAD, false, ME, "NULL");
    if(pAD->object) {
        amxd_trans_t trans;
        amxd_object_t* adObjTempl = amxd_object_get(pAP->pBus, "AssociatedDevice");
        amxd_object_for_each(instance, it, adObjTempl) {
            if(amxc_container_of(it, amxd_object_t, it) == pAD->object) {
                uint32_t index = amxd_object_get_index(pAD->object);
                ASSERT_TRUE(index > 0, false, ME, "wrong instance index");
                ASSERT_TRANSACTION_INIT(adObjTempl, &trans, false, ME, "%s : trans init failure", pAD->Name);
                amxd_trans_del_inst(&trans, index, NULL);
                pAD->object->priv = NULL;
                if(swl_object_finalizeTransactionOnLocalDm(&trans) != amxd_status_ok) {
                    SAH_TRACEZ_ERROR(ME, "%s : trans apply failure", pAD->Name);
                    pAD->object->priv = pAD;
                    return false;
                }
                break;
            }
        }
        pAD->object = NULL;
    } else {
        SAH_TRACEZ_INFO(ME, "%s: object did not exist!", pAD->Name);
    }

    int i = wld_ad_getIndex(pAP, pAD);
    if(i >= 0) {
        wld_ad_destroy_associatedDevice(pAP, i);
    }
    return true;
}



/* free the T_AssociatedDevice and remove it from the list */
void wld_ad_destroy_associatedDevice(T_AccessPoint* pAP, int index) {

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

    for(int i = index; i < (pAP->AssociatedDeviceNumberOfEntries - 1); i++) {
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

    swl_timeMono_t timeNow = swl_time_getMonoSec();

    memcpy(pAD->MACAddress, macAddress->bMac, ETHER_ADDR_LEN);

    SWL_MAC_BIN_TO_CHAR(pAD->Name, pAD->MACAddress);
    pAD->deviceType = pAP->defaultDeviceType;
    pAD->devicePriority = 1;
    pAD->Active = 0;
    pAD->Inactive = 0;
    pAD->AuthenticationState = 0;
    pAP->AssociatedDevice[pAP->AssociatedDeviceNumberOfEntries] = pAD;
    pAP->AssociatedDeviceNumberOfEntries++;
    pAD->latestStateChangeTime = timeNow;
    pAD->associationTime = timeNow;
    pAD->MaxDownlinkRateReached = 0;
    pAD->MaxUplinkRateReached = 0;
    pAD->minSignalStrength = 0;
    pAD->minSignalStrengthTime = timeNow;
    pAD->maxSignalStrength = -200;
    pAD->maxSignalStrengthTime = timeNow;
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
    int totalNrDev = pAP->AssociatedDeviceNumberOfEntries;
    for(i = 0; i < totalNrDev; i++) {
        pAD = pAP->AssociatedDevice[totalNrDev - 1 - i];
        if(pAD->Active) {
            wld_ad_add_disconnection(pAP, pAD);
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

static void s_updateStationStatsHistory(T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    if(pAD->SignalStrength < pAD->minSignalStrength) {
        pAD->minSignalStrength = pAD->SignalStrength;
        pAD->minSignalStrengthTime = swl_time_getMonoSec();
    }
    if(pAD->SignalStrength > pAD->maxSignalStrength) {
        pAD->maxSignalStrength = pAD->SignalStrength;
        pAD->maxSignalStrengthTime = swl_time_getMonoSec();
    }
    pAD->nrMeanSignalStrength++;
    pAD->meanSignalStrengthLinearAccumulator += pAD->SignalStrength;
    pAD->meanSignalStrength = pAD->meanSignalStrengthLinearAccumulator / (int32_t) pAD->nrMeanSignalStrength;

    pAD->meanSignalStrengthExpAccumulator = wld_util_performFactorStep(pAD->meanSignalStrengthExpAccumulator, pAD->SignalStrength, 50);
    if((uint32_t) pAD->LastDataDownlinkRate > pAD->MaxDownlinkRateReached) {
        pAD->MaxDownlinkRateReached = pAD->LastDataDownlinkRate;
    }
    if((uint32_t) pAD->LastDataUplinkRate > pAD->MaxUplinkRateReached) {
        pAD->MaxUplinkRateReached = pAD->LastDataUplinkRate;
    }

    if(pAD->downLinkRateSpec.standard > SWL_MCS_STANDARD_LEGACY) {
        memcpy(&pAD->lastNonLegacyDownlinkMCS, &pAD->downLinkRateSpec, sizeof(swl_mcs_t));
        pAD->lastNonLegacyDownlinkTime = pAD->lastSampleTime.tv_sec;
    }
    if(pAD->upLinkRateSpec.standard > SWL_MCS_STANDARD_LEGACY) {
        memcpy(&pAD->lastNonLegacyUplinkMCS, &pAD->upLinkRateSpec, sizeof(swl_mcs_t));
        pAD->lastNonLegacyUplinkTime = pAD->lastSampleTime.tv_sec;
    }
}

void wld_ad_printSignalStrengthHistory(T_AssociatedDevice* pAD, char* buf, uint32_t bufSize) {
    ASSERTS_TRUE((buf != NULL) && (bufSize > 0), , ME, "empty");
    snprintf(buf, bufSize, "%i,%i,%i,%i",
             pAD->minSignalStrength, pAD->maxSignalStrength,
             pAD->meanSignalStrength, WLD_ACC_TO_VAL(pAD->meanSignalStrengthExpAccumulator));
}

void wld_ad_printSignalStrengthByChain(T_AssociatedDevice* pAD, char* buf, uint32_t bufSize) {
    ASSERTS_TRUE((buf != NULL) && (bufSize > 0), , ME, "empty");
    for(uint32_t idx = 0; idx < MAX_NR_ANTENNA && pAD->SignalStrengthByChain[idx] != DEFAULT_BASE_RSSI; idx++) {
        //Example : -100.0
        swl_strlst_catFormat(buf, bufSize, ",", "%.1f", pAD->SignalStrengthByChain[idx]);
    }
}

static void wld_update_station_stats(T_AccessPoint* pAP) {
    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        s_updateStationStatsHistory(pAP->AssociatedDevice[i]);
    }
}

/**
 * Function to be called when station stats was successfully received from driver.
 * Can either be called synchronously if driver immediately returns from getStats call,
 * or in callback handler.
 * It will update staStats values in data model, and return the new values in retval.
 */
static void s_addStaStatsValues(T_AccessPoint* pAP, swl_rc_ne ret, amxc_var_t* retval) {
    T_Radio* pRad = pAP->pRadio;
    if(!pAP->enable || !pRad->enable || !swl_rc_isOk(ret)) {
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
        s_updateStationStatsHistory(pAD);
        wld_vap_sync_device(pAP, pAD);
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
    wld_rad_forEachAp(pAP, pRad) {
        if(wld_ad_has_active_stations(pAP)) {
            return true;
        }
    }
    return false;
}

bool wld_rad_has_active_video_stations(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
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
    int nr_sta = 0;

    wld_rad_forEachAp(pAP, pRad) {
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

    wld_rad_forEachAp(pAP, pRad) {
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

    wld_rad_forEachAp(pAP, pRad) {
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
uint16_t wld_ad_getFarStaCount(T_AccessPoint* pAP, int threshold) {
    int i = 0;
    int count = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->ActiveAssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "Null assoc dev on ap %s", pAP->alias);
        } else if(pAD->SignalStrength < threshold) {
            count++;
        }
    }
    return count;
}

amxd_status_t _getFarAssociatedDevicesCount(amxd_object_t* object,
                                            amxd_function_t* func _UNUSED,
                                            amxc_var_t* args,
                                            amxc_var_t* retval) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;

    int32_t threshold = INT32_MIN;
    threshold = GET_UINT32(args, "threshold");
    amxc_var_set_type(retval, AMXC_VAR_ID_INT32);
    amxc_var_add_new_int32_t(retval, wld_ad_getFarStaCount(pAP, threshold));

    return amxd_status_ok;
}

void wld_ad_checkRoamSta(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    T_Radio* testRad;
    wld_for_eachRad(testRad) {
        T_AccessPoint* testAp;
        wld_rad_forEachAp(testAp, testRad) {
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

static void s_activate(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    SAH_TRACEZ_INFO(ME, "%s : Try %s", pAP->alias, pAD->Name);
    pAD->Active = 1;
    pAD->AuthenticationState = 0;
    pAD->Inactive = 0;
    pAD->hadSecFailure = false;
    pAD->latestStateChangeTime = swl_time_getMonoSec();
    pAD->associationTime = swl_time_getMonoSec();

    swl_timespec_reset(&pAD->lastSampleTime);

    //kick from all other AP's
    wld_ad_checkRoamSta(pAP, pAD);
}

/**
 * Activate a station, updating the data model.
 * Note that if it is already known the station is authorised, please use
 * wld_ad_add_connection_succes immediately.
 */
void wld_ad_add_connection_try(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTI_FALSE(pAD->Active, , ME, "%s : already auth %s", pAP->alias, pAD->Name);

    s_activate(pAP, pAD);


    wld_vap_sync_device(pAP, pAD);
    wld_vap_syncNrDev(pAP);
}

static void s_addDcEntry(T_AccessPoint* pAP, T_AssociatedDevice* pAD, swl_IEEE80211deauthReason_ne deauthReason) {
    wld_ad_dcLog_t* log = s_findDcEntry(pAP, (swl_macBin_t*) &pAD->MACAddress);
    bool hasLog = (log != NULL);
    if(!hasLog) {
        log = swl_unLiList_allocElement(&pAP->staDcList.list);
        ASSERT_NOT_NULL(log, , ME, "%s: failed to create dc entry for %s", pAP->name, pAD->Name);
        memcpy(&log->macAddress, &pAD->MACAddress, ETHER_ADDR_LEN);
    }
    pAD->lastDeauthReason = deauthReason;
    swl_timespec_getMono(&log->dcTime);

    s_sendDisassocNotification(pAP, pAD);
    SAH_TRACEZ_INFO(ME, "%s: addLog %s (had %u)", swl_typeMacBin_toBuf32(log->macAddress).buf,
                    swl_typeTimeSpecMono_toBuf32(log->dcTime).buf, hasLog);
}


static void s_add_dc_sta(T_AccessPoint* pAP, T_AssociatedDevice* pAD, bool failSec, swl_IEEE80211deauthReason_ne deauthReason) {
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
        s_addDcEntry(pAP, pAD, deauthReason);
    }
    if(pAD->Active) {
        uint32_t timeOnline = swl_time_getMonoSec() - pAD->associationTime;
        SAH_TRACEZ_WARNING(ME, "%s: Disassoc sta %s - auth %u - assoc @ %s - active %u sec - SNR %i - reason %u",
                           pAP->alias, pAD->Name, pAD->AuthenticationState, swl_typeTimeMono_toBuf32(pAD->associationTime).buf,
                           timeOnline, pAD->SignalNoiseRatio, deauthReason);
        pAD->latestStateChangeTime = swl_time_getMonoSec();
        pAD->disassociationTime = swl_time_getMonoSec();
        pAD->connectionDuration = timeOnline;

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

    wld_vap_sync_device(pAP, pAD);
    wld_vap_cleanup_stationlist(pAP);
    wld_vap_syncNrDev(pAP);
}

/**
 * Notify the wld that a given station has successfully connected.
 * This will set the AuthenticationState to 1, indication successfull connection.
 * Note that even if security is off, if it succeeds to connect, we consider it authenticated.
 */
void wld_ad_add_connection_success(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTI_FALSE(pAD->AuthenticationState, , ME, "%s : already auth %s", pAP->alias, pAD->Name);

    if(!pAD->Active) {
        wld_ad_add_connection_try(pAP, pAD);
    }

    wld_ad_dcLog_t* entry = s_findDcEntry(pAP, (swl_macBin_t*) &pAD->MACAddress);
    SAH_TRACEZ_WARNING(ME, "%s: Connect %s (dc %s)", pAP->alias, pAD->Name, entry != NULL ?
                       swl_typeTimeSpecMono_toBuf32(entry->dcTime).buf : "NA");
    pAD->AuthenticationState = 1;
    pAD->Inactive = 0;
    pAD->latestStateChangeTime = swl_time_getMonoSec();
    s_incrementAssocCounter(pAP, "Success");
    if(!swl_security_isApModeValid(pAD->assocCaps.currentSecurity)) {
        pAD->assocCaps.currentSecurity = pAP->secModeEnabled;
    }
    //Update device type when connection succeeds.
    pAP->pFA->mfn_wvap_update_assoc_dev(pAP, pAD);

    if(entry != NULL) {
        s_dcEntryConnected(pAP, entry);
    }
    wld_vap_sync_device(pAP, pAD);
}


void wld_ad_deauthWithReason(T_AccessPoint* pAP, T_AssociatedDevice* pAD, swl_IEEE80211deauthReason_ne deauthReason) {
    wld_apRssiMon_sendHistoryOnDisassocEvent(pAP, pAD);
    s_add_dc_sta(pAP, pAD, false, deauthReason);
}

/**
 * Notify the wld that a station is disconnected.
 * This will set active and authenticated to 0.
 */
void wld_ad_add_disconnection(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    wld_ad_deauthWithReason(pAP, pAD, SWL_IEEE80211_DEAUTH_REASON_NONE);
}

/**
 * Notify the wld that a security failure has been detected.
 * This will deactivate the given station.
 */
void wld_ad_add_sec_failure(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    s_add_dc_sta(pAP, pAD, true, SWL_IEEE80211_DEAUTH_REASON_NONE);
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

static void s_setResetCounters_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    bool flag = amxc_var_dyncast(bool, newValue);
    if(flag) {
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, , ME, "trans init failure");
        // Reset On Write
        amxd_trans_set_bool(&trans, "ResetCounters", 0);
        // Reset Association Counters on 0!
        amxd_trans_set_uint32_t(&trans, "Success", 0);
        amxd_trans_set_uint32_t(&trans, "Fail", 0);
        amxd_trans_set_uint32_t(&trans, "FailSecurity", 0);
        amxd_trans_set_uint32_t(&trans, "Disconnect", 0);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");

    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApAssocCountDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("ResetCounters", s_setResetCounters_pwf)));

void _wld_ap_setAssocCountConf_ocf(const char* const sig_name,
                                   const amxc_var_t* const data,
                                   void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApAssocCountDmHdlrs, sig_name, data, priv);
}

static void s_updateAssocDev_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    bool needSyncAd = false;
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");
    T_AssociatedDevice* assocDev = (T_AssociatedDevice*) object->priv;
    ASSERT_NOT_EQUALS(wld_ad_getIndex(pAP, assocDev), -1, , ME, "%s: Invalid AD Ctx (%p)", pAP->alias, assocDev);
    amxc_var_for_each(newValue, newParamValues) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "DeviceType")) {
            const char* valStr = amxc_var_constcast(cstring_t, newValue);
            int newDeviceType = swl_conv_charToEnum(valStr, cstr_DEVICE_TYPES, DEVICE_TYPE_MAX, DEVICE_TYPE_DATA);
            if(newDeviceType == assocDev->deviceType) {
                continue;
            }
            assocDev->deviceType = newDeviceType;
        } else if(swl_str_matches(pname, "DevicePriority")) {
            int newDevicePriority = amxc_var_dyncast(int32_t, newValue);
            if(newDevicePriority == assocDev->devicePriority) {
                continue;
            }
            assocDev->devicePriority = newDevicePriority;
        } else {
            continue;
        }
        needSyncAd = true;
    }

    if(needSyncAd) {
        SAH_TRACEZ_INFO(ME, "%s: update assocdev %s type %u prio %u", pAP->alias, assocDev->Name, assocDev->deviceType, assocDev->devicePriority);
        pAP->pFA->mfn_wvap_update_assoc_dev(pAP, assocDev);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApAssocDevDmHdlrs, ARR(), .objChangedCb = s_updateAssocDev_ocf, );

void _wld_ap_setAssocDevConf_ocf(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApAssocDevDmHdlrs, sig_name, data, priv);
}

static void s_getOUIValue(amxc_string_t* output, swl_oui_list_t* vendorOui) {
    ASSERTI_TRUE(vendorOui->count != 0, , ME, "No OUI Vendor");
    char buffer[SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM];
    memset(buffer, 0, sizeof(buffer));
    swl_typeOui_arrayToChar(buffer, SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM, &vendorOui->oui[0], vendorOui->count);
    amxc_string_set(output, buffer);
}

void wld_ad_syncCapabilities(amxd_trans_t* trans, wld_assocDev_capabilities_t* caps) {
    ASSERT_NOT_NULL(trans, , ME, "NULL");
    ASSERT_NOT_NULL(caps, , ME, "NULL");

    char mcsList[100];
    memset(mcsList, 0, sizeof(mcsList));
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedMCS.mcs, caps->supportedMCS.mcsNbr);
    amxd_trans_set_cstring_t(trans, "SupportedMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHtMCS.mcs, caps->supportedHtMCS.mcsNbr);
    amxd_trans_set_cstring_t(trans, "SupportedHtMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedVhtMCS[COM_DIR_RECEIVE].nssMcsNbr, caps->supportedVhtMCS[COM_DIR_RECEIVE].nssNbr);
    amxd_trans_set_cstring_t(trans, "RxSupportedVhtMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedVhtMCS[COM_DIR_TRANSMIT].nssMcsNbr, caps->supportedVhtMCS[COM_DIR_TRANSMIT].nssNbr);
    amxd_trans_set_cstring_t(trans, "TxSupportedVhtMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHeMCS[COM_DIR_RECEIVE].nssMcsNbr, caps->supportedHeMCS[COM_DIR_RECEIVE].nssNbr);
    amxd_trans_set_cstring_t(trans, "RxSupportedHeMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHeMCS[COM_DIR_TRANSMIT].nssMcsNbr, caps->supportedHeMCS[COM_DIR_TRANSMIT].nssNbr);
    amxd_trans_set_cstring_t(trans, "TxSupportedHeMCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHe160MCS[COM_DIR_RECEIVE].nssMcsNbr, caps->supportedHe160MCS[COM_DIR_RECEIVE].nssNbr);
    amxd_trans_set_cstring_t(trans, "RxSupportedHe160MCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHe160MCS[COM_DIR_TRANSMIT].nssMcsNbr, caps->supportedHe160MCS[COM_DIR_TRANSMIT].nssNbr);
    amxd_trans_set_cstring_t(trans, "TxSupportedHe160MCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHe80x80MCS[COM_DIR_RECEIVE].nssMcsNbr, caps->supportedHe80x80MCS[COM_DIR_RECEIVE].nssNbr);
    amxd_trans_set_cstring_t(trans, "RxSupportedHe80x80MCS", mcsList);
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedHe80x80MCS[COM_DIR_TRANSMIT].nssMcsNbr, caps->supportedHe80x80MCS[COM_DIR_TRANSMIT].nssNbr);
    amxd_trans_set_cstring_t(trans, "TxSupportedHe80x80MCS", mcsList);

    amxc_string_t TBufStr;
    amxc_string_init(&TBufStr, 0);
    s_getOUIValue(&TBufStr, &caps->vendorOUI);
    amxd_trans_set_cstring_t(trans, "VendorOUI", amxc_string_get(&TBufStr, 0));
    amxd_trans_set_cstring_t(trans, "SecurityModeEnabled", swl_security_apModeToString(caps->currentSecurity, SWL_SECURITY_APMODEFMT_LEGACY));
    amxd_trans_set_cstring_t(trans, "EncryptionMode", cstr_AP_EncryptionMode[caps->encryptMode]);

    amxd_trans_set_cstring_t(trans, "LinkBandwidth", swl_bandwidth_unknown_str[caps->linkBandwidth]);
    char buffer[256];
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->htCapabilities, swl_staCapHt_str, SWL_STACAP_HT_MAX);
    amxd_trans_set_cstring_t(trans, "HtCapabilities", buffer);
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->vhtCapabilities, swl_staCapVht_str, SWL_STACAP_VHT_MAX);
    amxd_trans_set_cstring_t(trans, "VhtCapabilities", buffer);
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->heCapabilities, swl_staCapHe_str, SWL_STACAP_HE_MAX);
    amxd_trans_set_cstring_t(trans, "HeCapabilities", buffer);

    char frequencyCapabilitiesStr[128] = {0};
    swl_conv_maskToChar(frequencyCapabilitiesStr, sizeof(frequencyCapabilitiesStr), caps->freqCapabilities, swl_freqBandExt_unknown_str, SWL_FREQ_BAND_EXT_MAX);
    amxd_trans_set_cstring_t(trans, "FrequencyCapabilities", frequencyCapabilitiesStr);
    amxc_string_clean(&TBufStr);
}

void wld_ad_syncRrmCapabilities(amxd_trans_t* trans, wld_assocDev_capabilities_t* caps) {
    ASSERT_NOT_NULL(trans, , ME, "NULL");
    ASSERT_NOT_NULL(caps, , ME, "NULL");
    char buffer[256];
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->rrmCapabilities, swl_staCapRrm_str, SWL_STACAP_RRM_MAX);
    amxd_trans_set_cstring_t(trans, "RrmCapabilities", buffer);
    amxd_trans_set_uint32_t(trans, "RrmOnChannelMaxDuration", caps->rrmOnChannelMaxDuration);
    amxd_trans_set_uint32_t(trans, "RrmOffChannelMaxDuration", caps->rrmOffChannelMaxDuration);
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

void wld_ad_initAp(T_AccessPoint* pAP) {
    swl_unLiTable_initExt(&pAP->staDcList, &tWld_ad_dcLog, 3);
    swl_unLiList_setKeepsLastBlock(&pAP->staDcList.list, true);
}

void wld_ad_initFastReconnectCounters(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    amxd_object_t* templateObj = amxd_object_findf(pAP->pBus, "AssociationCount.FastReconnectTypes");
    ASSERT_NOT_NULL(templateObj, , ME, "NULL");

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(templateObj, &trans, , ME, "%s : trans init failure", pAP->name);

    for(uint32_t i = 0; i < WLD_FAST_RECONNECT_MAX; i++) {
        amxd_trans_select_object(&trans, templateObj);
        amxd_trans_add_inst(&trans, 0, fastReconnectTypes[i]);
        amxd_trans_set_cstring_t(&trans, "Type", fastReconnectTypes[i]);
    }
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pAP->name);
}


void wld_ad_cleanAp(T_AccessPoint* pAP) {
    swl_unLiTable_destroy(&pAP->staDcList);
}

void wld_ad_listRecentDisconnects(T_AccessPoint* pAP, amxc_var_t* variant) {
    swl_unLiTable_toListOfMaps(&pAP->staDcList, variant, tupleNames);
}

void wld_assocDev_copyAssocDevInfoFromIEs(T_Radio* pRad, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    pDev->capabilities |= pWirelessDevIE->capabilities;
    pDev->uniiBandsCapabilities |= pWirelessDevIE->uniiBandsCapabilities;
    cap->freqCapabilities = pWirelessDevIE->freqCapabilities;
    memcpy(&cap->vendorOUI, &pWirelessDevIE->vendorOUI, sizeof(swl_oui_list_t));
    cap->htCapabilities = pWirelessDevIE->htCapabilities;
    cap->vhtCapabilities = pWirelessDevIE->vhtCapabilities;
    cap->heCapabilities = pWirelessDevIE->heCapabilities;
    cap->rrmCapabilities = pWirelessDevIE->rrmCapabilities;
    cap->rrmOnChannelMaxDuration = pWirelessDevIE->rrmOnChannelMaxDuration;
    cap->rrmOffChannelMaxDuration = pWirelessDevIE->rrmOffChannelMaxDuration;
    cap->currentSecurity = pWirelessDevIE->secModeEnabled;
    pDev->MaxRxSpatialStreamsSupported = pWirelessDevIE->maxRxSpatialStreamsSupported;
    pDev->MaxTxSpatialStreamsSupported = pWirelessDevIE->maxTxSpatialStreamsSupported;
    pDev->MaxDownlinkRateSupported = pWirelessDevIE->maxDownlinkRateSupported;
    pDev->MaxUplinkRateSupported = pWirelessDevIE->maxUplinkRateSupported;
    memcpy(&cap->supportedMCS, &pWirelessDevIE->supportedMCS, sizeof(swl_mcs_supMCS_t));
    memcpy(&cap->supportedHtMCS, &pWirelessDevIE->supportedHtMCS, sizeof(swl_mcs_supMCS_t));
    memcpy(&cap->supportedVhtMCS, &pWirelessDevIE->supportedVhtMCS, sizeof(pWirelessDevIE->supportedVhtMCS));
    memcpy(&cap->supportedHeMCS, &pWirelessDevIE->supportedHeMCS, sizeof(pWirelessDevIE->supportedHeMCS));
    memcpy(&cap->supportedHe160MCS, &pWirelessDevIE->supportedHe160MCS, sizeof(pWirelessDevIE->supportedHe160MCS));
    memcpy(&cap->supportedHe80x80MCS, &pWirelessDevIE->supportedHe80x80MCS, sizeof(pWirelessDevIE->supportedHe80x80MCS));

    if(pDev->operatingStandard == SWL_RADSTD_AUTO) {
        pDev->operatingStandard = SWL_MIN(swl_bit32_getHighest(pWirelessDevIE->operatingStandards), swl_bit32_getHighest(pRad->operatingStandards));
    }
    if(cap->linkBandwidth == SWL_BW_AUTO) {
        cap->linkBandwidth = SWL_MIN(wld_util_getMaxBwCap(cap), pRad->runningChannelBandwidth);
    }
}

void wld_ad_handleAssocMsg(T_AccessPoint* pAP, T_AssociatedDevice* pAD, swl_bit8_t* iesData, size_t iesLen) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "PKT sta:"SWL_MAC_FMT " iesLen:%zu", SWL_MAC_ARG(pAD->MACAddress), iesLen);
    ASSERT_TRUE((iesData != NULL) && (iesLen > 0), , ME, "missing IEs in mgmt Frame");

    pAD->capabilities = 0;
    pAD->assocCaps.updateTime = swl_time_getMonoSec();
    pAD->lastSampleTime = swl_timespec_getMonoVal();

    swl_wirelessDevice_infoElements_t wirelessDevIE;
    swl_parsingArgs_t parsingArgs = {
        .seenOnChanspec = SWL_CHANSPEC_NEW(pAP->pRadio->channel, pAP->pRadio->runningChannelBandwidth, pAP->pRadio->operatingFrequencyBand),
    };
    ssize_t parsedLen = swl_80211_parseInfoElementsBuffer(&wirelessDevIE, &parsingArgs, iesLen, iesData);
    ASSERTW_FALSE(parsedLen < (ssize_t) iesLen, , ME, "Partial IEs parsing (%zi/%zu)", parsedLen, iesLen);

    wld_assocDev_copyAssocDevInfoFromIEs(pAP->pRadio, pAD, &pAD->assocCaps, &wirelessDevIE);
}

