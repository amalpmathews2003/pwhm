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
#include <stdint.h>

#include <signal.h>
#include <debug/sahtrace.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_ssid.h"
#include "wld_accesspoint.h"
#include "wld_endpoint.h"
#include "wld_wps.h"
#include "wld_rad_stamon.h"
#include "wld_eventing.h"
#include "wld_prbReq.h"
#include "wld_chanmgt.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld_nl80211_types.h"

#define ME "wld"

amxb_bus_ctx_t* wld_plugin_amx = NULL;
amxd_dm_t* wld_plugin_dm = NULL;
amxo_parser_t* wld_plugin_parser = NULL;
amxd_object_t* wifi = NULL;

static amxc_llist_t vendors = {NULL, NULL};

static swl_macBin_t sWanAddr = {{0}};

/**
 * Time component got init
 */
static swl_timeSpecMono_t initTime;

T_CONST_WPS g_wpsConst;
static bool init = false;

SWL_TT_C(gtWld_staHistory, wld_staHistory_t, X_WLD_STA_HISTORY);
SWL_NTT_C(gtWld_associatedDevice, T_AssociatedDevice, X_T_ASSOCIATED_DEVICE_ANNOT)


const swl_macBin_t* wld_getWanAddr() {
    return &sWanAddr;
}

T_Radio* wld_firstRad() {
    amxc_llist_it_t* it = amxc_llist_get_first(&g_radios);
    return wld_rad_fromIt(it);
}
T_Radio* wld_lastRad() {
    amxc_llist_it_t* it = amxc_llist_get_last(&g_radios);
    return wld_rad_fromIt(it);
}
T_Radio* wld_nextRad(T_Radio* pRad) {
    amxc_llist_it_t* it = amxc_llist_it_get_next(&pRad->it);
    return wld_rad_fromIt(it);
}

static void wld_plugin_set_busctx(amxo_parser_t* parser) {
    amxc_llist_for_each(it, parser->connections) {
        amxo_connection_t* con = amxc_container_of(it, amxo_connection_t, it);
        if(con->type == AMXO_BUS) {
            wld_plugin_amx = (amxb_bus_ctx_t*) con->priv;
        }
    }
}

void wld_plugin_init(amxd_dm_t* dm,
                     amxo_parser_t* parser) {
    wld_plugin_dm = dm;
    wld_plugin_parser = parser;
    wifi = amxd_dm_findf(wld_plugin_dm, "WiFi.");
    wld_plugin_set_busctx(parser);
}

bool wld_plugin_start() {
    ASSERT_FALSE(init, true, ME, "!!!!!! INIT ERROR !!!!!!!!!");
    init = true;

    const char* env = getenv(ME);
    if(env) {
        sahTraceAddZone(atol(env), ME);
    } else {
        sahTraceAddZone(TRACE_LEVEL_WARNING, ME);
    }
    swl_timespec_getMono(&initTime);

    SAH_TRACEZ_WARNING(ME, "Initializing wld (%s:%s)", __DATE__, __TIME__);

    swl_lib_initialize(wld_plugin_amx);

    /* Generate an initial default WPS pin */
    genSelfPIN();
    wld_event_init();

    // Set BaseMACAddress
    const char* MACStr = getenv("WLAN_ADDR"); /*backw compatible: */
    if(!MACStr || (MACStr[0] == '\0')) {
        MACStr = getenv("WAN_ADDR");
    }
    if(MACStr) {
        SWL_MAC_CHAR_TO_BIN(&sWanAddr, MACStr);
    } else {
        SAH_TRACEZ_ERROR(ME, "getenv(\"WLAN_ADDR\");failed and getenv(\"WAN_ADDR\");failed ");
    }

    return true;
}

void wld_plugin_syncDm() {
    SAH_TRACEZ_WARNING(ME, "Syncing Objects RAD/SSID/AP/EP");
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP;
        T_SSID* pSSID;
        /*
         * Rad obj DM params was synced to internal when setting upperLayer SSID instances
         * Now we push updated Rad params to DM
         */
        pRad->pFA->mfn_sync_radio(pRad->pBus, pRad, SET);
        /*
         * SSID obj DM params was synced to internal when setting relative AP/EP instances
         * Now we push updated SSID params to DM
         */
        wld_rad_forEachAp(pAP, pRad) {
            pSSID = (T_SSID*) pAP->pSSID;
            pRad->pFA->mfn_sync_ap(pAP->pBus, pAP, GET);
            pRad->pFA->mfn_sync_ap(pAP->pBus, pAP, SET);
            pRad->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        }
        T_EndPoint* pEP;
        wld_rad_forEachEp(pEP, pRad) {
            wld_endpoint_reconfigure(pEP);
            pRad->pFA->mfn_sync_ep(pEP);
            pSSID = (T_SSID*) pEP->pSSID;
            pRad->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        }
    }
    syncData_VendorWPS2OBJ(amxd_object_get(get_wld_object(), "wps_DefParam"), wld_firstRad(), SET);
    SAH_TRACEZ_WARNING(ME, "Syncing Done");
}

vendor_t* wld_getVendorByName(const char* name) {
    ASSERTS_STR(name, NULL, ME, "Invalid");
    amxc_llist_for_each(it, &vendors) {
        vendor_t* pVendor = amxc_llist_it_get_data(it, vendor_t, it);
        if(pVendor && swl_str_matches(pVendor->name, name)) {
            return pVendor;
        }
    }
    return NULL;
}

void wld_cleanup() {
    SAH_TRACEZ_INFO(ME, "Cleaning up wld plugin");
    wld_deleteAllEps();
    wld_deleteAllVaps();
    wld_deleteAllRadios();
    wld_ssid_cleanAll();
    wld_event_destroy();
    wld_nl80211_cleanupAll();
    wld_channel_cleanAll();
    wld_unregisterAllVendors();
    swl_lib_cleanup();
    wld_plugin_dm = NULL;
    init = false;
    sahTraceRemoveAllZones();
}

vendor_t* wld_registerVendor(const char* name, T_CWLD_FUNC_TABLE* fta) {
    vendor_t* vendor = calloc(1, sizeof(vendor_t));
    if(!vendor) {
        SAH_TRACEZ_ERROR(ME, "calloc failed");
        return NULL;
    }
    vendor->name = strdup(name);
    if(!vendor->name) {
        SAH_TRACEZ_ERROR(ME, "strdup(%s) failed", name);
        free(vendor);
        return NULL;
    }
    amxc_llist_append(&vendors, &vendor->it);

    wld_functionTable_init(vendor, fta);

    vendor->fta.mfn_wrad_fsm_delay_commit = fta->mfn_wrad_fsm_delay_commit;

    fta->mfn_sync_radio = vendor->fta.mfn_sync_radio = syncData_Radio2OBJ;
    fta->mfn_sync_ap = vendor->fta.mfn_sync_ap = SyncData_AP2OBJ;
    fta->mfn_sync_ssid = vendor->fta.mfn_sync_ssid = syncData_SSID2OBJ;
    fta->mfn_sync_ep = vendor->fta.mfn_sync_ep = syncData_EndPoint2OBJ;

    fta->mfn_wrad_libsync_status = vendor->fta.mfn_wrad_libsync_status = radio_libsync_status_cb;
    fta->mfn_wvap_libsync_status = vendor->fta.mfn_wvap_libsync_status = vap_libsync_status_cb;

    return vendor;
}

bool wld_isVendorUsed(vendor_t* vendor) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(vendor && pRad->vendor && (pRad->pFA == &vendor->fta)) {
            SAH_TRACEZ_NOTICE(ME, "Vendor %s FTA is still used by Radio %s", vendor->name, pRad->Name);
            return true;
        }
    }
    return false;
}

static bool s_cleanVendor(vendor_t* vendor) {
    if(!vendor) {
        return true;
    }
    ASSERT_FALSE(wld_isVendorUsed(vendor), false, ME, "Fail to clear vendor %s: still used", vendor->name);
    amxc_llist_it_take(&vendor->it);
    free(vendor->name);
    free(vendor);
    return true;
}

bool wld_unregisterVendor(vendor_t* vendor) {
    if(!vendor) {
        return true;
    }
    amxc_llist_for_each(it, &vendors) {
        if(vendor == amxc_container_of(it, vendor_t, it)) {
            return s_cleanVendor(vendor);
        }
    }
    return false;
}

bool wld_unregisterAllVendors() {
    amxc_llist_for_each(it, &vendors) {
        vendor_t* vendor = amxc_container_of(it, vendor_t, it);
        s_cleanVendor(vendor);
    }
    return (amxc_llist_is_empty(&vendors));
}

const char* radCounterNames[WLD_RAD_EV_MAX] = {
    "FsmCommit",
    "FsmReset",
    "DoubleAssoc",
    "EpAssocFail",
};

int wld_addRadio(const char* name, vendor_t* vendor, int idx) {
    ASSERT_NOT_NULL(name, -1, ME, "NULL");
    ASSERT_NOT_NULL(vendor, -1, ME, "NULL");

    T_Radio* pTmpR;
    if(idx < 0) {
        idx = 0;
        wld_for_eachRad(pTmpR) {
            idx = SWL_MAX(idx, (pTmpR->ref_index + 1));
        }
    } else {
        wld_for_eachRad(pTmpR) {
            ASSERT_NOT_EQUALS(idx, pTmpR->ref_index, -1, ME, "%s: refIfx(%d) already used by radio(%s)", name, idx, pTmpR->Name);
        }
    }

    char macStr[SWL_MAC_CHAR_LEN] = {0};
    SAH_TRACEZ_INFO(ME, "Create new radio %s %u", name, idx);


    T_Radio* pR = calloc(1, sizeof(T_Radio));
    if(!pR) {
        SAH_TRACEZ_ERROR(ME, "calloc failed");
        return -1;
    }
    pR->vendor = vendor;
    pR->wlRadio_SK = -1;

    memcpy(pR->MACAddr, &wld_getWanAddr()->bMac, SWL_MAC_BIN_LEN);
    swl_mac_binAddVal((swl_macBin_t*) pR->MACAddr, idx, -1);

    /* Attach T_Radio to object*/
    pR->debug = RAD_POINTER;
    pR->pFA = &vendor->fta;                         // Attach our vendor function table on it!
    wldu_copyStr(pR->Name, name, sizeof(pR->Name)); // Name of the RADIO!
    static uint32_t index = 0;
    pR->index = index++;
    pR->ref_index = idx;
    pR->wiphy = WLD_NL80211_ID_UNDEF;
    pR->detailedState = CM_RAD_UNKNOWN;
    pR->detailedStatePrev = CM_RAD_UNKNOWN;
    pR->status = RST_UNKNOWN;
    swl_timeMono_t now = swl_time_getMonoSec();
    pR->changeInfo.lastStatusChange = now;
    pR->changeInfo.lastStatusHistogramUpdate = now;
    pR->probeRequestMode = WLD_PRB_NO_UPDATE;
    pR->probeRequestAggregationTime = 1000;
    pR->aggregationTimer = NULL;
    pR->genericCounters.nrCounters = WLD_RAD_EV_MAX;
    pR->genericCounters.values = pR->counterList;
    pR->genericCounters.names = radCounterNames;
    pR->nrAntenna[COM_DIR_TRANSMIT] = -1;
    pR->nrAntenna[COM_DIR_RECEIVE] = -1;
    pR->nrActiveAntenna[COM_DIR_TRANSMIT] = -1;
    pR->nrActiveAntenna[COM_DIR_RECEIVE] = -1;
    pR->changeInfo.lastDisableTime = swl_time_getMonoSec();

    SAH_TRACEZ_WARNING(ME, "Creating new Radio context [%s]", pR->Name);

    wld_prbReq_init(pR);
    wld_autoCommitMgr_init(pR);

    if(pR->pFA->mfn_wrad_create_hook(pR)) {
        SAH_TRACEZ_ERROR(ME, "Radio create hook failed");
        free(pR);
        return -1;
    }

    /* Get our default WPS data */
    pR->wpsConst = &g_wpsConst;

    /* Generate Common WPS Self-PIN */
    wpsPinGen(pR->wpsConst->DefaultPin);

    pR->bgdfs_config.available = false;
    pR->bgdfs_config.enable = false;
    pR->bgdfs_config.channel = 0;

    /* Update our RO driver parameters in this T_Radio structure */
    pR->pFA->mfn_wrad_supports(pR, NULL, 0);
    pR->maxBitRate = 0; // Mark this as AUTO!

    pR->multiUserMIMOSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "MU_MIMO", 0) == SWL_TRL_TRUE);
    pR->implicitBeamFormingSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "IMPL_BF", 0) == SWL_TRL_TRUE);
    /* Enabled by default, but not if it's not supported of course. */
    pR->implicitBeamFormingEnabled = pR->implicitBeamFormingSupported;
    pR->explicitBeamFormingSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "EXPL_BF", 0) == SWL_TRL_TRUE);
    pR->explicitBeamFormingEnabled = pR->explicitBeamFormingSupported;

    pR->pFA->mfn_wrad_beamforming(pR, beamforming_implicit, pR->implicitBeamFormingEnabled, SET);
    pR->pFA->mfn_wrad_beamforming(pR, beamforming_explicit, pR->explicitBeamFormingEnabled, SET);

    if(pR->maxChannelBandwidth == SWL_BW_AUTO) {
        if((pR->supportedFrequencyBands & RFB_5_GHZ) || (pR->supportedFrequencyBands & RFB_6_GHZ)) {
            bool supports160 = pR->pFA->mfn_misc_has_support(pR, NULL, "160MHz", 0);
            pR->maxChannelBandwidth = (supports160 ? SWL_BW_160MHZ : SWL_BW_80MHZ);
        } else {
            pR->maxChannelBandwidth = (pR->supportedStandards & M_SWL_RADSTD_N) ? SWL_BW_40MHZ : SWL_BW_20MHZ;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: max bw: %u", pR->Name, pR->maxChannelBandwidth);
    pR->autoBwSelectMode = BW_SELECT_MODE_DEFAULT;

    amxc_llist_init(&pR->channelChangeList);

    pR->DFSChannelChangeEventCounter = 0;
    pR->dfsEventNbr = 0;
    pR->dfsFileEventNbr = 0;

    pR->airtimeFairnessEnabled = 0;
    pR->intAirtimeSchedEnabled = 0;
    pR->multiUserMIMOEnabled = 0;
    pR->operatingClass = 0;

    pR->macCfg.useLocalBitForGuest = 0;
    pR->macCfg.localGuestMacOffset = 256;

    amxc_llist_init(&pR->scanState.stats.extendedStat);

    amxc_llist_append(&g_radios, &pR->it);

    // Apply the MAC address on the radio, update if needed inside vendor
    SWL_MAC_BIN_TO_CHAR(macStr, pR->MACAddr);
    pR->pFA->mfn_wvap_bssid(pR, NULL, (unsigned char*) macStr, SWL_MAC_CHAR_LEN, SET);
    // update macStr as macBin may be shifted inside vendor
    SWL_MAC_BIN_TO_CHAR(macStr, pR->MACAddr);

    wld_chanmgt_checkInitChannel(pR);
    pR->isReady = true;

    SAH_TRACEZ_WARNING(ME, "%s: radInit vendor %s, index %u, baseMac %s", name, vendor->name, idx, macStr);

    return 0;
}

void wld_deleteRadioObj(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    char name[IFNAMSIZ];
    memcpy(name, pRad->Name, sizeof(name));
    SAH_TRACEZ_INFO(ME, "start delete radio %s", name);

    wld_radStaMon_destroy(pRad);
    wld_autoCommitMgr_destroy(pRad);
    wld_rad_clearSuppDrvCaps(pRad);
    wld_prbReq_destroy(pRad);

    amxc_llist_for_each(it, &pRad->scanState.stats.extendedStat) {
        wld_scanReasonStats_t* stat = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        amxc_llist_it_take(&stat->it);
        free(stat->scanReason);
        free(stat);
    }

    wld_chanmgt_cleanup(pRad);

    free(pRad->scanState.cfg.fastScanReasons);
    pRad->scanState.cfg.fastScanReasons = NULL;
    wld_scan_cleanupScanResults(&pRad->scanState.lastScanResults);

    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pFA, , ME, "NULL");
    pRad->pFA->mfn_wrad_destroy_hook(pRad);
    amxp_timer_delete(&pRad->aggregationTimer);
    pRad->aggregationTimer = NULL;

    if(pRad->pBus != NULL) {
        pRad->pBus->priv = NULL;
        pRad->pBus = NULL;
    }

    free(pRad->dbgOutput);
    pRad->dbgOutput = NULL;

    amxc_llist_it_take(&pRad->it);
    free(pRad);


    SAH_TRACEZ_INFO(ME, "finished delete radio %s", name);
}

void wld_deleteRadio(const char* name) {
    wld_deleteRadioObj(wld_rad_from_name(name));
}

void wld_deleteAllRadios() {
    amxc_llist_for_each(it, &g_radios) {
        T_Radio* pRad = wld_rad_fromIt(it);
        wld_deleteRadioObj(pRad);
    }
}

void wld_deleteAllVaps() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        amxc_llist_for_each(it, &pRad->llAP) {
            T_AccessPoint* pAP = wld_ap_fromIt(it);
            wld_ap_destroy(pAP);
        }
    }
}

void wld_deleteAllEps() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        amxc_llist_for_each(it, &pRad->llEndPoints) {
            T_EndPoint* pEP = wld_endpoint_fromIt(it);
            wld_endpoint_destroy(pEP);
        }
    }
}

/**
 * Return the first radio with the given netdev index
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioByIndex(int index) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(pRad->index == index) {
            return pRad;
        }
    }
    return NULL;
}

/**
 * Return the first radio with the given netdev index
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioByName(const char* name) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(swl_str_matches(pRad->Name, name)) {
            return pRad;
        }
    }
    return NULL;
}

T_Radio* wld_getUinitRadioByBand(swl_freqBandExt_e band) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if((pRad->operatingFrequencyBand == band) && (pRad->pBus == NULL)) {
            return pRad;
        }
    }
    return NULL;
}

T_EndPoint* wld_getEndpointByAlias(const char* name) {
    T_Radio* pRad = NULL;
    T_EndPoint* pEP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachEp(pEP, pRad) {
            if(swl_str_matches(pEP->alias, name)) {
                return pEP;
            }
        }
    }
    return NULL;
}

T_AccessPoint* wld_getAccesspointByAlias(const char* name) {
    T_Radio* pRad = NULL;
    T_AccessPoint* pAP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachAp(pAP, pRad) {
            if(swl_str_matches(pAP->alias, name)) {
                return pAP;
            }
        }
    }
    return NULL;
}

T_AccessPoint* wld_getAccesspointByAddress(unsigned char* macAddress) {
    ASSERTS_NOT_NULL(macAddress, NULL, ME, "NULL");
    T_Radio* pRad = NULL;
    T_AccessPoint* pAP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachAp(pAP, pRad) {
            T_SSID* pBSSID = pAP->pSSID;
            if(pBSSID && SWL_MAC_BIN_MATCHES(pBSSID->BSSID, macAddress)) {
                return pAP;
            }
        }
    }
    return NULL;
}

T_Radio* wld_getRadioByAddress(unsigned char* macAddress) {
    ASSERTS_NOT_NULL(macAddress, NULL, ME, "NULL");
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(SWL_MAC_BIN_MATCHES(pRad->MACAddr, macAddress)) {
            return pRad;
        }
    }
    return NULL;
}

/**
 * get the first radio interface for given frequency.
 */
T_Radio* wld_getRadioByFrequency(swl_freqBand_e freqBand) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(pRad->operatingFrequencyBand == (swl_freqBandExt_e) freqBand) {
            return pRad;
        }
    }
    return NULL;
}

amxd_object_t* get_wld_object() {
    return wifi;
}

amxb_bus_ctx_t* get_wld_plugin_bus() {
    return wld_plugin_amx;
}

amxd_dm_t* get_wld_plugin_dm() {
    return wld_plugin_dm;
}

amxo_parser_t* get_wld_plugin_parser() {
    return wld_plugin_parser;
}

/**
 * Get a char vendor parameter.
 * If pR or parameter are NULL, it returns NULL.
 * If the parameter does not exist, or the lenght of the result is zero, we return the default.
 * Otherwise it returns the parameter value from the data model.
 */
char* wld_getVendorParam(const T_Radio* pR, const char* parameter, const char* def) {
    ASSERT_NOT_NULL(pR, NULL, ME, "NULL");
    ASSERT_NOT_NULL(parameter, NULL, ME, "NULL");

    amxd_object_t* vendorObject = amxd_object_get(pR->pBus, "Vendor");
    char* value = amxd_object_get_cstring_t(vendorObject, parameter, NULL);

    if(!swl_str_isEmpty(value)) {
        return value;
    }
    free(value);

    if(def != NULL) {
        return strdup(def);
    }
    return NULL;
}

/**
 * Get an integer vendor parameter.
 * If the parameter does not exist, or pR or parameter is null, it returns default value.
 * Otherwise it returns the parameter value from the data model.
 */
int wld_getVendorParam_int(const T_Radio* pR, const char* parameter, const int def) {
    ASSERT_NOT_NULL(pR, def, ME, "NULL");
    ASSERT_NOT_NULL(parameter, def, ME, "NULL");

    amxd_object_t* vendorObject = amxd_object_get(pR->pBus, "Vendor");
    amxd_status_t s = amxd_status_unknown_error;
    int32_t value = amxd_object_get_int32_t(vendorObject, parameter, &s);

    if(s == amxd_status_ok) {
        return value;
    } else {
        return def;
    }
}

bool wld_isInternalBssidBin(const swl_macBin_t* bssid) {
    amxc_llist_it_t* rad_it;
    amxc_llist_it_t* ap_it;
    char strMacAddress[ETHER_ADDR_STR_LEN];

    /* Search in all my radio 2.4GhZ and 5Ghz */
    amxc_llist_for_each(rad_it, &g_radios) {
        T_Radio* pRad = amxc_llist_it_get_data(rad_it, T_Radio, it);
        /* Search my own radio into into VAP list */
        amxc_llist_for_each(ap_it, &pRad->llAP) {
            T_AccessPoint* pAP = amxc_llist_it_get_data(ap_it, T_AccessPoint, it);
            T_SSID* pSSID = pAP->pSSID;
            SAH_TRACEZ_INFO(ME, "VAP " SWL_MAC_FMT " checking into storedAP list ...", SWL_MAC_ARG(pSSID->MACAddress));
            if(swl_mac_binMatches((swl_macBin_t*) pSSID->MACAddress, bssid)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @deprecated in favor of #wld_isInternalBssidBin
 */
bool wld_isInternalBSSID(const char bssid[ETHER_ADDR_STR_LEN]) {
    swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
    SWL_MAC_CHAR_TO_BIN(&bssidBin, bssid);
    return wld_isInternalBssidBin(&bssidBin);
}

/**
 * Function returning mask of the currently configured freq bands of all available radio's,
 * ignoring the radio provided. If ingoreRad is NULL, the or of all radio's is returned.
 *
 */
swl_freqBandExt_m wld_getAvailableFreqBands(T_Radio* ignoreRad) {
    swl_freqBandExt_m freqMask = 0;
    T_Radio* tmpRad = NULL;
    wld_for_eachRad(tmpRad) {
        if((tmpRad != ignoreRad) && (tmpRad != NULL)) {
            freqMask |= (1 << tmpRad->operatingFrequencyBand);
        }
    }
    return freqMask;
}


swl_timeSpecMono_t* wld_getInitTime() {
    return &initTime;
}
