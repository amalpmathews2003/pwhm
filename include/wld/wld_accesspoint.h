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

#ifndef __WLD_ACCESSPOINT_H__
#define __WLD_ACCESSPOINT_H__



#include "wld.h"
#include "wld_wps.h"

#define NR_OF_STICKY_UNAUTHORIZED_STATIONS 1

void t_destroy_handler_AP (amxd_object_t* object);
T_AccessPoint* wld_ap_create(T_Radio* pRad, const char* vapName, uint32_t idx);
int32_t wld_ap_initObj(T_AccessPoint* pAP, amxd_object_t* instance_object);
int wld_ap_initializeVendor(T_Radio* pR, T_AccessPoint* pAP, T_SSID* pSSID);
int wld_ap_init(T_AccessPoint* pAP);
void wld_ap_destroy(T_AccessPoint* pAP);
int vap_libsync_status_cb(T_AccessPoint* pAP);

void SyncData_AP2OBJ(amxd_object_t* object, T_AccessPoint* pAP, int set);
void SetAPDefaults(T_AccessPoint* pAP, int idx);

swl_rc_ne wld_vap_sync_device(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_vap_syncNrDev(T_AccessPoint* pAP);
bool wld_vap_sync_assoclist(T_AccessPoint* pAP);
void wld_ap_sec_doSync(T_AccessPoint* pAP);

amxd_status_t kickStation(amxd_object_t* obj_AP,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _AccessPoint_VerifyAccessPoint(amxd_object_t* object, amxd_function_t* func _UNUSED, amxc_var_t* args _UNUSED, amxc_var_t* ret _UNUSED);
amxd_status_t _AccessPoint_CommitAccessPoint(amxd_object_t* object, amxd_function_t* func _UNUSED, amxc_var_t* args _UNUSED, amxc_var_t* ret _UNUSED);


void wld_destroy_associatedDevice(T_AccessPoint* pAP, int index);
T_AssociatedDevice* wld_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* MacAddress);
T_AssociatedDevice* wld_vap_get_existing_station(T_AccessPoint* pAP, swl_macBin_t* macAddress);
bool wld_vap_cleanup_stationlist(T_AccessPoint* pAP);
bool wld_vap_assoc_update_cuid(T_AccessPoint* pAP, swl_macBin_t* mac, char* cuid, int len);

void wld_ap_bss_done(T_AccessPoint* ap, const unsigned char* mac, int reply_code);
void wld_ap_rrm_item(T_AccessPoint* ap, const unsigned char* mac, amxc_var_t* result);

bool wld_ap_ModesSupported_mask_to_string(amxc_string_t* output, wld_securityMode_m secModesSupported);

void wld_ap_notifyToggle(T_AccessPoint* pAP);

void wld_ap_performErrorToggle(T_AccessPoint* pAP, const char* reason);

void wld_ap_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress);

void wld_vap_updateState(T_AccessPoint* pAP);
T_AccessPoint* wld_vap_get_vap(const char* ifname);
T_AccessPoint* wld_ap_getVapByName(const char* name);
T_AccessPoint* wld_ap_fromIt(amxc_llist_it_t* it);
void wld_station_stats_done(T_AccessPoint* pAP, bool success);
bool wld_ap_getDesiredState(T_AccessPoint* pAp);

void wld_ap_doSync(T_AccessPoint* pAP);
void wld_ap_doWpsSync(T_AccessPoint* pAP);

#ifndef WLD_MAC_MAXBAN
#define WLD_MAC_MAXBAN 64
#endif

typedef struct {
    swl_macBin_t banList[WLD_MAC_MAXBAN];
    uint32_t staToBan;
    swl_macBin_t kickList[WLD_MAC_MAXBAN];
    uint32_t staToKick;
} wld_banlist_t;
void wld_apMacFilter_getBanList(T_AccessPoint* pAP, wld_banlist_t* banlist, bool includePf);
swl_rc_ne wld_ap_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, wld_vap_assocTableStruct_t** data);

#endif /* __WLD_ACCESSPOINT_H__ */
