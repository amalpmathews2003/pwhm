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

#include "wld/wld.h"
#include "wld/wld_radio.h"
#include "wld/wld_nl80211_api.h"
#include "wifiGen_rad.h"
#include "wifiGen_vap.h"
#include "wifiGen_ep.h"
#include "wifiGen_fsm.h"

#define ME "gen"

static bool s_init = false;
static vendor_t* s_vendor = NULL;

bool wifiGen_init() {
    ASSERT_FALSE(s_init, false, ME, "already initialized");
    SAH_TRACEZ_ERROR(ME, "Generic init");
    wld_daemonMonitorConf_t dmnMoniConf = {
        .enableParam = true,
        .instantRestartLimit = 3,
        .minRestartInterval = 1800,
    };
    wld_dmn_setMonitorConf(&dmnMoniConf);

    T_CWLD_FUNC_TABLE fta;
    memset(&fta, 0, sizeof(T_CWLD_FUNC_TABLE));

    //Misc functions
    fta.mfn_misc_has_support = wifiGen_rad_miscHasSupport;

    //Wrad functions
    fta.mfn_wrad_create_hook = wifiGen_rad_createHook;
    fta.mfn_wrad_destroy_hook = wifiGen_rad_destroyHook;
    fta.mfn_wrad_addVapExt = wifiGen_rad_addVapExt;
    fta.mfn_wrad_delvapif = wifiGen_rad_delvapif;
    fta.mfn_wrad_poschans = wifiGen_rad_poschans;
    fta.mfn_wrad_supports = wifiGen_rad_supports;
    fta.mfn_wrad_radio_status = wifiGen_rad_status;
    fta.mfn_wrad_enable = wifiGen_rad_enable;
    fta.mfn_wrad_sync = wifiGen_rad_sync;
    fta.mfn_wrad_regdomain = wifiGen_rad_regDomain;
    fta.mfn_wrad_txpow = wifiGen_rad_txpow;
    fta.mfn_wrad_channel = wifiGen_rad_channel;
    fta.mfn_wrad_antennactrl = wifiGen_rad_antennactrl;
    fta.mfn_wrad_supstd = wifiGen_rad_supstd;
    fta.mfn_wrad_ochbw = wifiGen_rad_ochbw;

    //vap functions
    fta.mfn_wvap_create_hook = wifiGen_vap_createHook;
    fta.mfn_wvap_destroy_hook = wifiGen_vap_destroyHook;
    fta.mfn_wvap_bssid = wifiGen_vap_bssid;
    fta.mfn_wvap_ssid = wifiGen_vap_ssid;
    fta.mfn_wvap_status = wifiGen_vap_status;
    fta.mfn_wvap_enable = wifiGen_vap_enable;
    fta.mfn_wvap_sync = wifiGen_vap_sync;
    fta.mfn_wvap_get_station_stats = wifiGen_get_station_stats;
    fta.mfn_wvap_update_rssi_stats = wifiGen_get_station_stats;
    fta.mfn_wvap_sec_sync = wifiGen_vap_sec_sync;
    fta.mfn_wvap_transfer_sta = wifiGen_vap_sta_transfer;
    fta.mfn_wvap_transfer_sta_ext = wifiGen_vap_sta_transfer_ext;
    fta.mfn_wvap_mf_sync = wifiGen_vap_mf_sync;
    fta.mfn_wvap_pf_sync = wifiGen_vap_mf_sync;
    fta.mfn_wvap_wps_sync = wifiGen_vap_wps_sync;
    fta.mfn_wvap_kick_sta = wifiGen_vap_kick_sta;
    fta.mfn_wvap_kick_sta_reason = wifiGen_vap_kick_sta_reason;
    fta.mfn_wvap_multiap_update_type = wifiGen_vap_multiap_update_type;

    //endpoint function
    fta.mfn_wendpoint_create_hook = wifiGen_ep_createHook;
    fta.mfn_wendpoint_destroy_hook = wifiGen_ep_destroyHook;

    //endpoint function
    fta.mfn_wendpoint_disconnect = wifiGen_ep_disconnect;

    s_vendor = wld_nl80211_registerVendor(&fta);
    wld_nl80211_getSharedState();
    wifiGen_fsm_doInit(s_vendor);

    return true;
}

int wifiGen_addRadios() {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(s_vendor, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_nl80211_ifaceInfo_t wlIfacesInfo[MAXNROF_RADIO][MAXNROF_ACCESSPOINT] = {};
    rc = wld_nl80211_getInterfaces(MAXNROF_RADIO, MAXNROF_ACCESSPOINT, wlIfacesInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get nl80211 interfaces");
    uint8_t index = 0;
    for(uint32_t i = 0; i < MAXNROF_RADIO; i++) {
        wld_nl80211_ifaceInfo_t* pMainIface = &wlIfacesInfo[i][0];
        if(pMainIface->ifIndex <= 0) {
            continue;
        }
        T_Radio* pRad = wld_rad_get_radio(pMainIface->name);
        if(pRad) {
            SAH_TRACEZ_WARNING(ME, "Interface %s already handled by %s", pMainIface->name, pRad->vendor->name);
            index++;
            continue;
        }
        SAH_TRACEZ_WARNING(ME, "Interface %s handled by %s", pMainIface->name, s_vendor->name);
        wld_addRadio(pMainIface->name, s_vendor, index);
        index++;
    }
    if(index == 0) {
        SAH_TRACEZ_INFO(ME, "NO Wireless interface found");
        return SWL_RC_ERROR;
    }

    return SWL_RC_OK;
}

