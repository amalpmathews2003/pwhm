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

#ifndef SRC_INCLUDE_WLD_WLD_ASSOCDEV_H_
#define SRC_INCLUDE_WLD_WLD_ASSOCDEV_H_

#include "wld.h"

typedef struct {
    T_Radio* pRad;
    T_AccessPoint* pAP;
    T_AssociatedDevice* pAD;
} wld_assocDevInfo_t;

typedef enum {
    WLD_FAST_RECONNECT_DEFAULT,
    WLD_FAST_RECONNECT_STATE_CHANGE,
    WLD_FAST_RECONNECT_SCAN,
    WLD_FAST_RECONNECT_MAX
} wld_fastReconnectType_e;

int wld_ad_remove_assocdev_from_bridge(T_AccessPoint* pAP, T_AssociatedDevice* pAD);

T_AssociatedDevice* wld_ad_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress);
T_AssociatedDevice* wld_vap_find_asociatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress);
T_AssociatedDevice* wld_vap_findOrCreateAssociatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress);

void wld_ad_destroy_associatedDevice(T_AccessPoint* pAP, int index);
bool wld_ad_has_far_station(T_AccessPoint* pAP, int threshold);
int wld_ad_get_nb_far_station(T_AccessPoint* pAP, int threshold);

void wld_ad_add_connection_try(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_ad_add_connection_success(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_ad_add_disconnection(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_ad_add_sec_failure(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_ad_add_sec_failNoDc(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
int wld_ad_get_nb_active_stations(T_AccessPoint* pAP);
int wld_ad_get_nb_active_video_stations(T_AccessPoint* pAP);
int wld_rad_get_nb_active_stations(T_Radio* pRad);
int wld_rad_get_nb_active_video_stations(T_Radio* pRad);
bool wld_ad_has_active_stations(T_AccessPoint* pAP);
bool wld_ad_hasAuthenticatedStations(T_AccessPoint* pAP);
void wld_ad_syncCapabilities(amxd_object_t* object, wld_assocDev_capabilities_t* caps);
bool wld_ad_has_active_video_stations(T_AccessPoint* pAP);
bool wld_rad_has_active_stations(T_Radio* pRad);
bool wld_rad_has_active_video_stations(T_Radio* pRad);
int32_t wld_ad_getAvgSignalStrengthByChain(T_AssociatedDevice* pAD);

bool wld_ad_has_assocdev(T_AccessPoint* pAP, const unsigned char macAddress[ETHER_ADDR_LEN]);
T_AccessPoint* wld_rad_get_associated_ap(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]);
bool wld_rad_has_assocdev(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]);
wld_assocDevInfo_t wld_rad_get_associatedDeviceInfo(T_Radio* pRad, const unsigned char macAddress[ETHER_ADDR_LEN]);

void wld_vap_mark_all_stations_unseen(T_AccessPoint* pAP);
void wld_vap_update_seen(T_AccessPoint* pAP);

const char* wld_ad_getMode(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_ad_getHeMCS(uint16_t he_mcs, wld_sta_supMCS_adv_t* supportedHeMCS);

void wld_assocDev_initAp(T_AccessPoint* pAP);
void wld_assocDev_cleanAp(T_AccessPoint* pAP);
void wld_assocDev_listRecentDisconnects(T_AccessPoint* pAP, amxc_var_t* variant);

#endif /* SRC_INCLUDE_WLD_WLD_ASSOCDEV_H_ */
