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

#ifndef __WLD_RADIO_H__
#define __WLD_RADIO_H__




#include "swla/swla_trilean.h"
#include "wld.h"
#include "wld_channel.h"
#include "wld_radioOperatingStandards.h"

typedef enum {
    WLD_RAD_CHANGE_POSCHAN,
    WLD_RAD_CHANGE_MAX
} wld_rad_changeEvent_e;

typedef struct {
    wld_rad_changeEvent_e changeType;
    T_Radio* pRad;
    void* changeData;
} wld_rad_changeEvent_t;

T_Radio* wld_getRadioDataHandler(amxd_object_t* pobj, const char* rn);

int radio_libsync_status_cb(T_Radio* rad);
void wld_radio_updateAntenna(T_Radio* pRad);

void syncData_Radio2OBJ(amxd_object_t* object, T_Radio* pR, int set);
void syncData_VendorWPS2OBJ(amxd_object_t* object, T_Radio* pR, int set);

amxd_status_t _addVAPIntf(amxd_object_t* obj,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _delVAPIntf(amxd_object_t* obj,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _wps_DefParam_wps_GenSelfPIN(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED);

amxd_status_t _checkWPSPIN(amxd_object_t* obj,
                           amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret);

void _wld_radio_addInstance_ocf(const char* const sig_name _UNUSED,
                                const amxc_var_t* const data,
                                void* const priv _UNUSED);

extern amxc_llist_t g_radios;

/* DEBUG !!! FIX ME */
amxd_status_t _FSM_Start(amxd_object_t* wifi,
                         amxd_function_t* func,
                         amxc_var_t* args,
                         amxc_var_t* ret);

T_AccessPoint* wld_rad_getFirstVap(T_Radio* pR);
T_EndPoint* wld_rad_getFirstEp(T_Radio* pR);
T_AccessPoint* wld_rad_vap_from_name(T_Radio* pR, const char* ifname);
T_EndPoint* wld_rad_ep_from_name(T_Radio* pR, const char* ifname);
T_AccessPoint* wld_vap_from_name(const char* ifname);
T_EndPoint* wld_vep_from_name(const char* ifname);
T_Radio* wld_rad_from_name(const char* ifname);

T_AccessPoint* wld_rad_getVapByIndex(T_Radio* pRad, int index);
T_EndPoint* wld_rad_getEpByIndex(T_Radio* pRad, int index);

amxd_object_t* ssid_create(amxd_object_t* pBus, T_Radio* pR, const char* name);
bool wld_rad_SupFreqBands_mask_to_string(amxc_string_t* output, uint32_t supportedFrequencyBands);
bool radio_scanresults_to_variant(T_Radio* radio, amxc_var_t* variant_out);
bool wld_radio_notify_scanresults(amxd_object_t* obj);
bool wld_radio_stats_to_variant(T_Radio* a, T_Stats* stats, amxc_var_t* map);
bool wld_radio_scanresults_find(T_Radio* pR, const char* ssid, T_ScanResult_SSID* output);
bool wld_radio_scanresults_cleanup(T_ScanResults* results);

void wld_notifyProbeRequest(T_Radio* pR, const unsigned char* macStr);
void wld_notifyProbeRequest_rssi(T_Radio* pR, const unsigned char* macStr, int rssi);
int wld_prbReq_getRssi(T_Radio* pR, const unsigned char* macStr);
void wld_notifyStartScan(T_Radio* pRad, wld_scan_type_e type, const char* scanReason);
void wld_scan_diagNotifyRadDone(bool success);
void wld_scan_done(T_Radio* pR, bool success);
bool wld_scan_isRunning(T_Radio* pR);
bool wld_isAnyRadioRunningScan();
swl_rc_ne wld_scan_updateChanimInfo(T_Radio* pRad);
void wld_scan_cleanupScanResultSSID(T_ScanResult_SSID* ssid);
void wld_scan_cleanupScanResults(T_ScanResults* res);
T_Radio* wld_getRadioByFrequency(swl_freqBand_e freqBand);

int wld_rad_init_cap(T_Radio* pR);
void wld_rad_parse_cap(T_Radio* pR);
void wld_rad_parse_status(T_Radio* pR);
bool wld_rad_is_cap_enabled(T_Radio* pR, int capability);
bool wld_rad_is_cap_active(T_Radio* pR, int capability);

void wld_rad_addSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap);
swl_trl_e wld_rad_findSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap);
const char* wld_rad_getSuppDrvCaps(T_Radio* pRad, swl_freqBand_e suppBand);
void wld_rad_clearSuppDrvCaps(T_Radio* pRad);

bool wld_rad_hasWeatherChannels(T_Radio* pRad);
bool wld_rad_hasChannel(T_Radio* pRad, int chan);
void do_updateOperatingChannelBandwidth5GHz(T_Radio* pRad);
char* getChannelsInUseStr(T_Radio* pRad);
bool wld_rad_hasChannelWidthCovered(T_Radio* pRad, swl_bandwidth_e chW);
wld_channel_extensionPos_e wld_rad_getExtensionChannel(T_Radio* pRad);

bool wld_rad_hasEnabledEp(T_Radio* pRad);
bool wld_rad_hasConnectedEp(T_Radio* pRad);
bool wld_rad_hasOnlyActiveEP(T_Radio* pRad);

bool wld_rad_hasActiveVap(T_Radio* pRad);
bool wld_rad_hasEnabledVap(T_Radio* pRad);

bool wld_rad_hasEnabledIface(T_Radio* pRad);
bool wld_rad_hasActiveIface(T_Radio* pRad);

uint32_t wld_rad_getFirstEnabledIfaceIndex(T_Radio* pRad);
uint32_t wld_rad_getFirstActiveIfaceIndex(T_Radio* pRad);

bool wld_rad_isChannelSubset(T_Radio* pRad, uint8_t* chanlist, int size);

bool wld_rad_is_6ghz(const T_Radio* pR);
bool wld_rad_is_5ghz(const T_Radio* pR);
bool wld_rad_is_24ghz(const T_Radio* pRad);
bool wld_rad_is_on_dfs(T_Radio* pR);
bool wld_rad_isDoingDfsScan(T_Radio* pRad);
bool wld_rad_isUpAndReady(T_Radio* pRad);
bool wld_rad_isUpExt(T_Radio* pRad);
swl_bandwidth_e wld_rad_verify_bandwidth(T_Radio* pRad, swl_bandwidth_e targetbandwidth);
void wld_rad_get_update_running_bandwidth(T_Radio* pRad);
void wld_rad_setRunningChannelBandwidth(T_Radio* pRad, swl_bandwidth_e newBw);
swl_bandwidth_e wld_rad_getBaseConfigBw(T_Radio* pRad);
swl_bandwidth_e wld_rad_get_target_bandwidth(T_Radio* pRad);
void wld_rad_write_possible_channels(T_Radio* pRad);
bool wld_rad_addDFSEvent(T_Radio* pR, T_DFSEvent* evt);
swl_chanspec_t wld_rad_getSwlChanspec(T_Radio* pRad);
uint32_t wld_rad_getCurrentFreq(T_Radio* pRad);

int wld_rad_doRadioCommit(T_Radio* pR);
void wld_rad_doSync(T_Radio* pRad);
int wld_rad_doCommitIfUnblocked(T_Radio* pR);
bool wld_rad_has_endpoint_enabled(T_Radio* rad);
T_EndPoint* wld_rad_getEnabledEndpoint(T_Radio* rad);
bool wld_rad_hasWpsActiveEndpoint(T_Radio* rad);
T_EndPoint* wld_rad_getWpsActiveEndpoint(T_Radio* rad);
void wld_rad_updateActiveDevices(T_Radio* pRad);

T_Radio* wld_rad_get_radio(const char* ifname);
void wld_rad_chan_update_model(T_Radio* pRad);
void wld_rad_chan_notification(T_Radio* pRad, int newChannelValue, int newBandwidthValue);
void wld_rad_updateOperatingClass(T_Radio* pRad);

void wld_rad_init_counters(T_Radio* pRad, T_EventCounterList* counters, const char** defaults);
void wld_rad_increment_counter(T_Radio* pRad, T_EventCounterList* counters, uint32_t counterIndex, const char* info);
void wld_rad_incrementCounterStr(T_Radio* pRad, T_EventCounterList* counters, uint32_t index, const char* template, ...);

void wld_radio_copySsidsToResult(T_ScanResults* results, amxc_llist_t* ssid_list);

bool wld_rad_isAvailable(T_Radio* pRad);
bool wld_rad_isActive(T_Radio* pRad);
bool wld_rad_isUp(T_Radio* pRad);
int wld_rad_getSocket(T_Radio* rad);
void wld_rad_resetStatusHistogram(T_Radio* pRad);
void wld_rad_updateState(T_Radio* pRad, bool forceVapUpdate);

T_AccessPoint* wld_radio_getVapFromRole(T_Radio* pRad, wld_apRole_e role);

T_AccessPoint* wld_rad_firstAp(T_Radio* pRad);
T_AccessPoint* wld_rad_nextAp(T_Radio* pRad, T_AccessPoint* pAP);
T_EndPoint* wld_rad_firstEp(T_Radio* pRad);
T_EndPoint* wld_rad_nextEp(T_Radio* pRad, T_EndPoint* pEP);
T_Radio* wld_rad_fromIt(amxc_llist_it_t* it);
void wld_rad_triggerChangeEvent(T_Radio* pRad, wld_rad_changeEvent_e event, void* data);
void wld_rad_notifyPublicAction(T_Radio* pRad, swl_macChar_t* macStr, swl_oui_t oui, uint8_t type, uint8_t subtype, uint8_t* data);

#define wld_rad_forEachAp(apPtr, radPtr) \
    for(apPtr = wld_rad_firstAp(radPtr); apPtr; apPtr = wld_rad_nextAp(radPtr, apPtr))
#define wld_rad_forEachEp(epPtr, radPtr) \
    for(epPtr = wld_rad_firstEp(radPtr); epPtr; epPtr = wld_rad_nextEp(radPtr, epPtr))

amxd_object_t* wld_rad_getObject(T_Radio* pRad);
#endif /* __WLD_RADIO_H__ */
