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

#include "wld.h"
#include "wld_util.h"
#define ME "wldTrap"

/**
   Function traps - only used for failure or unsupported functions.
 */

static int TRAP_mfn_wrad_create_hook(T_Radio* rad) {
    _UNUSED_(rad);
    // Ignore silently. Constructor hooks are optional.
    return 0;
}

static void TRAP_mfn_wrad_destroy_hook(T_Radio* rad) {
    _UNUSED_(rad);
    // Ignore silently. Destructor hooks are optional.
}

static swl_rc_ne TRAP_mfn_wrad_scan_results(T_Radio* rad, T_ScanResults* results) {
    _UNUSED_(rad);
    _UNUSED_(results);
    SAH_TRACEZ_NOTICE(ME, "%p, %p", rad, results);
    return SWL_RC_ERROR;
}

static swl_rc_ne TRAP_mfn_wrad_start_scan_ext(T_Radio* rad, T_ScanArgs* args) {
    _UNUSED_(rad);
    _UNUSED_(args);
    SAH_TRACEZ_NOTICE(ME, "%p, %p", rad, args);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wrad_stop_scan(T_Radio* rad) {
    _UNUSED_(rad);
    SAH_TRACEZ_NOTICE(ME, "%p", rad);
    return SWL_RC_ERROR;
}

static int TRAP_mfn_wrad_addVapExt(T_Radio* rad, T_AccessPoint* ap) {
    _UNUSED_(rad);
    _UNUSED_(ap);
    SAH_TRACEZ_NOTICE(ME, "%p, %p", rad, ap);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static void TRAP_mfn_wvap_set_config_driver(T_AccessPoint* ap, wld_vap_driverCfgChange_m param) {
    _UNUSED_(ap);
    _UNUSED_(param);
    SAH_TRACEZ_NOTICE(ME, "%p %d", ap, param);
    // Ignore silently.
}

#define DEF_TRAP(type, func) \
    static int TRAP_ ## func(type * obj) { \
        _UNUSED_(obj); \
        SAH_TRACEZ_NOTICE(ME, "%p", obj); \
        return -1; \
    }

#define DEF_TRAP_int(type, func) \
    static int TRAP_ ## func(type * obj, int a) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        SAH_TRACEZ_NOTICE(ME, "%p %d", obj, a); \
        return -1; \
    }

#define DEF_TRAP_uint32(type, func) \
    static int TRAP_ ## func(type * obj, uint32_t a) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        SAH_TRACEZ_NOTICE(ME, "%p %d", obj, a); \
        return -1; \
    }

#define DEF_TRAP_int_int(type, func) \
    static int TRAP_ ## func(type * obj, int a, int b) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        _UNUSED_(b); \
        SAH_TRACEZ_NOTICE(ME, "%p %d %d", obj, a, b); \
        return -1; \
    }

#define DEF_TRAP_char_int(type, func) \
    static int TRAP_ ## func(type * obj, char* a, int b) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        _UNUSED_(b); \
        SAH_TRACEZ_NOTICE(ME, "%p %s %d", obj, a, b); \
        return -1; \
    }

#define DEF_TRAP_char_int_int(type, func) \
    static int TRAP_ ## func(type * obj, char* a, int b, int c) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        _UNUSED_(b); \
        _UNUSED_(c); \
        SAH_TRACEZ_NOTICE(ME, "%p %s %d %d", obj, a, b, c); \
        return -1; \
    }

/* Enum always int based... */
#define DEF_TRAP_enum(type, func, enumtype) \
    static int TRAP_ ## func(type * obj, enumtype a) { \
        _UNUSED_(obj); \
        _UNUSED_(a); \
        SAH_TRACEZ_NOTICE(ME, "%p %d", obj, a); \
        return -1; \
    }

DEF_TRAP_enum(T_Radio, mfn_wrad_dfsradartrigger, dfstrigger_rad_state);
DEF_TRAP_int_int(T_Radio, mfn_wrad_enable);
DEF_TRAP_int_int(T_Radio, mfn_wrad_channel);
DEF_TRAP_int_int(T_Radio, mfn_wrad_autochannelenable);
DEF_TRAP_int_int(T_Radio, mfn_wrad_achrefperiod);
DEF_TRAP_int_int(T_Radio, mfn_wrad_ochbw);
DEF_TRAP_int_int(T_Radio, mfn_wrad_txpow);
DEF_TRAP_int_int(T_Radio, mfn_wrad_antennactrl);
DEF_TRAP_int_int(T_Radio, mfn_wrad_rifs);
DEF_TRAP_int_int(T_Radio, mfn_wrad_airtimefairness);
DEF_TRAP_int_int(T_Radio, mfn_wrad_intelligentAirtime);
DEF_TRAP_int_int(T_Radio, mfn_wrad_rx_powersave);
DEF_TRAP_int_int(T_Radio, mfn_wrad_multiusermimo);
DEF_TRAP_int(T_Radio, mfn_wrad_startacs);
DEF_TRAP_int(T_Radio, mfn_wrad_sync);
DEF_TRAP_char_int(T_Radio, mfn_wrad_addvapif);
DEF_TRAP_char_int(T_Radio, mfn_wrad_supfreqbands);
DEF_TRAP_uint32(T_Radio, mfn_wrad_supstd);
DEF_TRAP_char_int(T_Radio, mfn_wrad_chansinuse);
DEF_TRAP_char_int(T_Radio, mfn_wrad_supports);
DEF_TRAP_char_int_int(T_Radio, mfn_wrad_extchan);
DEF_TRAP_char_int_int(T_Radio, mfn_wrad_guardintval);
DEF_TRAP_char_int_int(T_Radio, mfn_wrad_mcs);
DEF_TRAP_char_int_int(T_Radio, mfn_wrad_regdomain);
DEF_TRAP(T_Radio, mfn_wrad_radio_status);
DEF_TRAP(T_Radio, mfn_wrad_maxbitrate);
DEF_TRAP(T_Radio, mfn_wrad_fsm_state);
DEF_TRAP(T_Radio, mfn_wrad_fsm);
DEF_TRAP(T_Radio, mfn_wrad_fsm_nodelay);
DEF_TRAP(T_Radio, mfn_wrad_update_chaninfo);
DEF_TRAP(T_Radio, mfn_wrad_update_prob_req);
DEF_TRAP(T_Radio, mfn_wrad_fsm_reset);
DEF_TRAP_int(T_Radio, mfn_wrad_bgdfs_enable);
DEF_TRAP_int(T_Radio, mfn_wrad_bgdfs_start);
DEF_TRAP(T_Radio, mfn_wrad_bgdfs_stop);
DEF_TRAP(T_Radio, mfn_wrad_delayApUpDone);

DEF_TRAP(T_AccessPoint, mfn_wvap_status);
DEF_TRAP(T_AccessPoint, mfn_wvap_fsm_state);
DEF_TRAP(T_AccessPoint, mfn_wvap_fsm);
DEF_TRAP(T_AccessPoint, mfn_wvap_fsm_nodelay);

DEF_TRAP_int_int(T_AccessPoint, mfn_wvap_enable);
DEF_TRAP_int_int(T_AccessPoint, mfn_wvap_enable_wmm);
DEF_TRAP_int_int(T_AccessPoint, mfn_wvap_enable_uapsd);
DEF_TRAP_char_int_int(T_AccessPoint, mfn_wvap_ssid);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_sync);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_sec_sync);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_wps_label_pin);
DEF_TRAP_int_int(T_AccessPoint, mfn_wvap_wps_comp_mode);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_mf_sync);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_pf_sync);
DEF_TRAP_char_int_int(T_AccessPoint, mfn_wvap_kick_sta);
DEF_TRAP_char_int_int(T_AccessPoint, mfn_wvap_kick_sta_reason);
DEF_TRAP_int_int(T_AccessPoint, mfn_hspot_enable);
DEF_TRAP_int(T_AccessPoint, mfn_hspot_config);
DEF_TRAP_char_int(T_AccessPoint, mfn_wvap_clean_sta);
DEF_TRAP(T_AccessPoint, mfn_wvap_multiap_update_type);
DEF_TRAP(T_AccessPoint, mfn_wvap_set_ap_role);
DEF_TRAP_int(T_AccessPoint, mfn_wvap_enab_vendor_ie);
DEF_TRAP(T_AccessPoint, mfn_wvap_set_discovery_method);

DEF_TRAP(T_EndPoint, mfn_wendpoint_multiap_enable);
DEF_TRAP(T_EndPoint, mfn_wendpoint_set_mac_address);

static swl_rc_ne TRAP_mfn_wvap_get_station_stats(T_AccessPoint* ap) {
    _UNUSED_(ap);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wvap_update_rssi_stats(T_AccessPoint* ap) {
    return ap->pFA->mfn_wvap_get_station_stats(ap);
}

static swl_rc_ne TRAP_mfn_wvap_get_single_station_stats(T_AssociatedDevice* ad) {
    _UNUSED_(ad);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wrad_bgdfs_start_ext(T_Radio* rad, wld_startBgdfsArgs_t* args) {
    _UNUSED_(rad);
    _UNUSED_(args);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wrad_poschans(T_Radio* rad, unsigned char* buf, int bufsize) {
    _UNUSED_(rad);
    _UNUSED_(buf);
    _UNUSED_(bufsize);
    SAH_TRACEZ_NOTICE(ME, "%p %p %d", rad, buf, bufsize);
    return -1;
}

static int TRAP_mfn_wrad_delvapif(T_Radio* rad, char* vap) {
    _UNUSED_(rad);
    _UNUSED_(vap);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, vap);
    return -1;
}

static int TRAP_mfn_wrad_beamforming(T_Radio* rad, beamforming_type_t type, int val, int set) {
    _UNUSED_(rad);
    _UNUSED_(type);
    _UNUSED_(val);
    _UNUSED_(set);
    SAH_TRACEZ_NOTICE(ME, "%p %d %d %d", rad, type, val, set);
    return -1;
}

static amxd_status_t TRAP_mfn_wrad_getspectruminfo(T_Radio* rad, bool update, uint64_t call_id, amxc_var_t* retval) {
    _UNUSED_(update);
    _UNUSED_(retval);
    _UNUSED_(rad);
    _UNUSED_(call_id);
    SAH_TRACEZ_NOTICE(ME, "%p", rad);
    return amxd_status_unknown_error;
}

static int TRAP_mfn_wrad_airstats(T_Radio* rad, T_Airstats* stats) {
    _UNUSED_(rad);
    _UNUSED_(stats);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, stats);
    return -1;
}

static int TRAP_mfn_wrad_per_ant_rssi(T_Radio* rad, T_ANTENNA_RSSI* stats) {
    _UNUSED_(rad);
    _UNUSED_(stats);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, stats);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wrad_latest_power(T_Radio* rad, T_ANTENNA_POWER* stats) {
    _UNUSED_(rad);
    _UNUSED_(stats);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, stats);
    return -1;
}

static int TRAP_mfn_wvap_create_hook(T_AccessPoint* vap) {
    _UNUSED_(vap);
    // Ignore silently. Constructor hooks are optional.
    return 0;
}

static void TRAP_mfn_wvap_destroy_hook(T_AccessPoint* vap) {
    _UNUSED_(vap);
    // Ignore silently. Destructor hooks are optional.
}

static int TRAP_mfn_wvap_transfer_sta(T_AccessPoint* vap _UNUSED, char* sta _UNUSED, char* targetBssid _UNUSED,
                                      int operClass _UNUSED, int channel _UNUSED) {
    SAH_TRACEZ_NOTICE(ME, "%p %s %s %d %d", vap, sta, targetBssid, operClass, channel);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wvap_transfer_sta_ext(T_AccessPoint* vap, wld_transferStaArgs_t* params) {
    SAH_TRACEZ_NOTICE(ME, "%p %s %s %d %d %d %d", vap, params->sta.cMac, params->targetBssid.cMac,
                      params->operClass, params->channel, params->validity, params->disassoc);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wvap_sendPublicAction(T_AccessPoint* vap, swl_macBin_t* sta, swl_oui_t oui, uint8_t type, uint8_t subtype, char* data) {
    SAH_TRACEZ_NOTICE(ME, "%p %p %02X:%02X:%02X %d %d %s", vap, sta, oui.ouiBytes[0], oui.ouiBytes[1], oui.ouiBytes[2], type, subtype, data);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wvap_request_rrm_report(T_AccessPoint* vap, const swl_macChar_t* sta, wld_rrmReq_t* req _UNUSED) {
    SAH_TRACEZ_NOTICE(ME, "%p %p", vap, sta);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wvap_bssid(T_Radio* rad, T_AccessPoint* vap, unsigned char* buf, int bufsize, int set) {
    _UNUSED_(rad);
    _UNUSED_(vap);
    _UNUSED_(buf);
    _UNUSED_(bufsize);
    _UNUSED_(set);
    SAH_TRACEZ_NOTICE(ME, "%p %p %p %d %d", rad, vap, buf, bufsize, set);
    return -1;
}

static int TRAP_mfn_misc_has_support(T_Radio* rad, T_AccessPoint* vap, char* buf, int bufsize) {
    _UNUSED_(rad);
    _UNUSED_(vap);
    _UNUSED_(buf);
    _UNUSED_(bufsize);
    SAH_TRACEZ_NOTICE(ME, "%p %p %p %d", rad, vap, buf, bufsize);
    return -1;
}

static swl_rc_ne TRAP_mfn_wifi_supvend_modes(T_Radio* rad, T_AccessPoint* dstAP, amxd_object_t* object, amxc_var_t* params _UNUSED) {
    _UNUSED_(rad);
    _UNUSED_(dstAP);
    _UNUSED_(object);
    SAH_TRACEZ_NOTICE(ME, "%p %p %p %p", rad, dstAP, object, params);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_on_bridge_state_change(T_Radio* rad, T_AccessPoint* vap, int set) {
    _UNUSED_(rad);
    _UNUSED_(vap);
    _UNUSED_(set);
    SAH_TRACEZ_NOTICE(ME, "%p %p %d", rad, vap, set);
    return -1;
}

static int TRAP_mfn_wvap_update_assoc_dev(T_AccessPoint* vap, T_AssociatedDevice* dev) {
    _UNUSED_(vap);
    _UNUSED_(dev);
    SAH_TRACEZ_NOTICE(ME, "%p %p ", vap, dev);
    return -1;
}


static int TRAP_mfn_wvap_updated_neighbour(T_AccessPoint* vap, T_ApNeighbour* newNeighbour) {
    _UNUSED_(vap);
    _UNUSED_(newNeighbour);
    SAH_TRACEZ_NOTICE(ME, "%p %p ", vap, newNeighbour);
    return -1;
}

static int TRAP_mfn_wvap_deleted_neighbour(T_AccessPoint* vap, T_ApNeighbour* newNeighbour) {
    _UNUSED_(vap);
    _UNUSED_(newNeighbour);
    SAH_TRACEZ_NOTICE(ME, "%p %p ", vap, newNeighbour);
    return -1;
}

static swl_rc_ne TRAP_mfn_wvap_update_ap_stats(T_Radio* rad, T_AccessPoint* vap) {
    _UNUSED_(rad);
    _UNUSED_(vap);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, vap);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wvap_add_vendor_ie(T_AccessPoint* vap, wld_vendorIe_t* vendor_ie) {
    _UNUSED_(vap);
    _UNUSED_(vendor_ie);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wvap_del_vendor_ie(T_AccessPoint* vap, wld_vendorIe_t* vendor_ie) {
    _UNUSED_(vap);
    _UNUSED_(vendor_ie);
    return WLD_ERROR_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wrad_addendpointif(T_Radio* radio _UNUSED, char* endpoint _UNUSED, int bufsize _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p %s %d", radio, endpoint, bufsize);
    return -1;
}

static int TRAP_mfn_wrad_delendpointif(T_Radio* radio _UNUSED, char* endpoint _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p %s", radio, endpoint);
    return -1;
}

static swl_rc_ne TRAP_mfn_wendpoint_create_hook(T_EndPoint* pEP _UNUSED) {
    SAH_TRACEZ_NOTICE(ME, "%p", pEP);
    //create hook doesn't need implementation
    return SWL_RC_OK;
}

static swl_rc_ne TRAP_mfn_wendpoint_destroy_hook(T_EndPoint* pEP _UNUSED) {
    SAH_TRACEZ_NOTICE(ME, "%p", pEP);
    //destroy hook doesn't need implementation
    return SWL_RC_OK;
}

static swl_rc_ne TRAP_mfn_wendpoint_enable(T_EndPoint* endpoint, bool enable) {
    SAH_TRACEZ_WARNING(ME, "%p %d", endpoint, enable);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wendpoint_disconnect(T_EndPoint* endpoint _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", endpoint);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wendpoint_connect_ap(T_EndPointProfile* endpointProfile _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", endpointProfile);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wendpoint_status(T_EndPoint* endpoint _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", endpoint);
    return -1;
}

static swl_rc_ne TRAP_mfn_wendpoint_bssid(T_EndPoint* endpoint, swl_macChar_t* bssid _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", endpoint);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wendpoint_stats(T_EndPoint* endpoint _UNUSED, T_EndPointStats* stats _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", endpoint);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wendpoint_wps_start(T_EndPoint* pEP, wld_wps_cfgMethod_e method _UNUSED, char* pin _UNUSED, char* ssid _UNUSED, swl_macChar_t* bssid _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", pEP);
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wendpoint_wps_cancel(T_EndPoint* pEP) {
    SAH_TRACEZ_WARNING(ME, "%p", pEP);
    return SWL_RC_NOT_IMPLEMENTED;
}

static int TRAP_mfn_wendpoint_enable_vendor_roaming(T_EndPoint* pEP _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", pEP);
    return -1;
}

static int TRAP_mfn_wendpoint_update_vendor_roaming(T_EndPoint* pEP _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "%p", pEP);
    return -1;
}

static int TRAP_mfn_wrad_update_mon_stats(T_Radio* rad _UNUSED) {
    _UNUSED_(rad);
    SAH_TRACEZ_NOTICE(ME, "%p", rad);
    return -1;
}

static int TRAP_mfn_wrad_setup_stamon(T_Radio* rad _UNUSED, bool enable _UNUSED) {
    _UNUSED_(rad);
    _UNUSED_(enable);
    SAH_TRACEZ_NOTICE(ME, "%p %d", rad, enable);
    return -1;
}

static int TRAP_mfn_wrad_add_stamon(T_Radio* rad _UNUSED, T_NonAssociatedDevice* pMD _UNUSED) {
    _UNUSED_(rad);
    _UNUSED_(pMD);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, pMD);
    return -1;
}

static int TRAP_mfn_wrad_del_stamon(T_Radio* rad _UNUSED, T_NonAssociatedDevice* pMD _UNUSED) {
    _UNUSED_(rad);
    _UNUSED_(pMD);
    SAH_TRACEZ_NOTICE(ME, "%p %p", rad, pMD);
    return -1;
}

static swl_rc_ne TRAP_mfn_wvap_wps_sync(T_AccessPoint* pAP _UNUSED, char* val _UNUSED, int bufsize _UNUSED, int set _UNUSED) {
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wvap_wps_enable(T_AccessPoint* pAP _UNUSED, int enable _UNUSED, int set _UNUSED) {
    return SWL_RC_NOT_IMPLEMENTED;
}

static swl_rc_ne TRAP_mfn_wrad_stats(T_Radio* pRadio _UNUSED) {
    _UNUSED_(pRadio);
    SAH_TRACEZ_NOTICE(ME, "%p", pRadio);
    return SWL_RC_NOT_IMPLEMENTED;
}

void wld_functionTable_init(vendor_t* vendor, T_CWLD_FUNC_TABLE* fta) {

#define FTA_ASSIGN(x) \
    vendor->fta.x = (fta && fta->x) ? fta->x : TRAP_ ## x

    //mist functions
    FTA_ASSIGN(mfn_misc_has_support);

    //wrad functions
    FTA_ASSIGN(mfn_wrad_create_hook);
    FTA_ASSIGN(mfn_wrad_destroy_hook);
    FTA_ASSIGN(mfn_wrad_addVapExt);
    FTA_ASSIGN(mfn_wrad_addvapif);
    FTA_ASSIGN(mfn_wrad_delvapif);
    FTA_ASSIGN(mfn_wrad_radio_status);
    FTA_ASSIGN(mfn_wrad_enable);
    FTA_ASSIGN(mfn_wrad_maxbitrate);
    FTA_ASSIGN(mfn_wrad_dfsradartrigger);
    FTA_ASSIGN(mfn_wrad_supfreqbands);
    FTA_ASSIGN(mfn_wrad_supstd);
    FTA_ASSIGN(mfn_wrad_poschans);
    FTA_ASSIGN(mfn_wrad_chansinuse);
    FTA_ASSIGN(mfn_wrad_channel);
    FTA_ASSIGN(mfn_wrad_supports);
    FTA_ASSIGN(mfn_wrad_autochannelenable);
    FTA_ASSIGN(mfn_wrad_startacs);
    FTA_ASSIGN(mfn_wrad_bgdfs_enable);
    FTA_ASSIGN(mfn_wrad_bgdfs_start);
    FTA_ASSIGN(mfn_wrad_bgdfs_start_ext);
    FTA_ASSIGN(mfn_wrad_bgdfs_stop);
    FTA_ASSIGN(mfn_wrad_achrefperiod);
    FTA_ASSIGN(mfn_wrad_ochbw);
    FTA_ASSIGN(mfn_wrad_extchan);
    FTA_ASSIGN(mfn_wrad_guardintval);
    FTA_ASSIGN(mfn_wrad_mcs);
    FTA_ASSIGN(mfn_wrad_txpow);
    FTA_ASSIGN(mfn_wrad_antennactrl);
    FTA_ASSIGN(mfn_wrad_regdomain);
    FTA_ASSIGN(mfn_wrad_beamforming);
    FTA_ASSIGN(mfn_wrad_rifs);
    FTA_ASSIGN(mfn_wrad_airtimefairness);
    FTA_ASSIGN(mfn_wrad_intelligentAirtime);
    FTA_ASSIGN(mfn_wrad_rx_powersave);
    FTA_ASSIGN(mfn_wrad_multiusermimo);
    FTA_ASSIGN(mfn_wrad_airstats);
    FTA_ASSIGN(mfn_wrad_per_ant_rssi);
    FTA_ASSIGN(mfn_wrad_latest_power);
    FTA_ASSIGN(mfn_wrad_sync);
    FTA_ASSIGN(mfn_wrad_getspectruminfo);
    FTA_ASSIGN(mfn_wrad_start_scan_ext);
    FTA_ASSIGN(mfn_wrad_stop_scan);
    FTA_ASSIGN(mfn_wrad_scan_results);
    FTA_ASSIGN(mfn_wrad_update_mon_stats);
    FTA_ASSIGN(mfn_wrad_setup_stamon);
    FTA_ASSIGN(mfn_wrad_add_stamon);
    FTA_ASSIGN(mfn_wrad_del_stamon);
    FTA_ASSIGN(mfn_wrad_delayApUpDone);
    FTA_ASSIGN(mfn_wrad_stats);

    // wvap functions
    FTA_ASSIGN(mfn_wvap_create_hook);
    FTA_ASSIGN(mfn_wvap_destroy_hook);
    FTA_ASSIGN(mfn_wvap_get_station_stats);
    FTA_ASSIGN(mfn_wvap_get_single_station_stats);
    FTA_ASSIGN(mfn_wvap_update_rssi_stats);
    FTA_ASSIGN(mfn_wvap_status);
    FTA_ASSIGN(mfn_wvap_enable);
    FTA_ASSIGN(mfn_wvap_enable_wmm);
    FTA_ASSIGN(mfn_wvap_enable_uapsd);
    FTA_ASSIGN(mfn_wvap_ssid);
    FTA_ASSIGN(mfn_wvap_bssid);
    FTA_ASSIGN(mfn_wvap_sync);
    FTA_ASSIGN(mfn_wvap_sec_sync);
    FTA_ASSIGN(mfn_wvap_wps_sync);
    FTA_ASSIGN(mfn_wvap_wps_enable);
    FTA_ASSIGN(mfn_wvap_wps_label_pin);
    FTA_ASSIGN(mfn_wvap_wps_comp_mode);
    FTA_ASSIGN(mfn_wvap_mf_sync);
    FTA_ASSIGN(mfn_wvap_pf_sync);
    FTA_ASSIGN(mfn_wvap_kick_sta);
    FTA_ASSIGN(mfn_wvap_kick_sta_reason);
    FTA_ASSIGN(mfn_wvap_clean_sta);
    FTA_ASSIGN(mfn_wvap_multiap_update_type);
    FTA_ASSIGN(mfn_wvap_set_ap_role);
    FTA_ASSIGN(mfn_wvap_add_vendor_ie);
    FTA_ASSIGN(mfn_wvap_del_vendor_ie);
    FTA_ASSIGN(mfn_wvap_enab_vendor_ie);
    FTA_ASSIGN(mfn_wvap_set_discovery_method);
    FTA_ASSIGN(mfn_wvap_set_config_driver);
    FTA_ASSIGN(mfn_wvap_transfer_sta);
    FTA_ASSIGN(mfn_wvap_transfer_sta_ext);
    FTA_ASSIGN(mfn_wvap_sendPublicAction);
    FTA_ASSIGN(mfn_wvap_fsm_state);
    FTA_ASSIGN(mfn_wvap_fsm);
    FTA_ASSIGN(mfn_wvap_fsm_nodelay);
    FTA_ASSIGN(mfn_wrad_fsm_state);
    FTA_ASSIGN(mfn_wrad_fsm);
    FTA_ASSIGN(mfn_wrad_fsm_nodelay);
    FTA_ASSIGN(mfn_wrad_fsm_reset);
    FTA_ASSIGN(mfn_wifi_supvend_modes);
    FTA_ASSIGN(mfn_hspot_enable);
    FTA_ASSIGN(mfn_hspot_config);
    FTA_ASSIGN(mfn_on_bridge_state_change);
    FTA_ASSIGN(mfn_wvap_update_assoc_dev);
    FTA_ASSIGN(mfn_wrad_update_chaninfo);
    FTA_ASSIGN(mfn_wrad_update_prob_req);
    FTA_ASSIGN(mfn_wvap_updated_neighbour);
    FTA_ASSIGN(mfn_wvap_deleted_neighbour);
    FTA_ASSIGN(mfn_wvap_update_ap_stats);
    FTA_ASSIGN(mfn_wrad_addendpointif);
    FTA_ASSIGN(mfn_wrad_delendpointif);
    FTA_ASSIGN(mfn_wendpoint_create_hook);
    FTA_ASSIGN(mfn_wendpoint_destroy_hook);
    FTA_ASSIGN(mfn_wendpoint_enable);
    FTA_ASSIGN(mfn_wendpoint_disconnect);
    FTA_ASSIGN(mfn_wendpoint_connect_ap);
    FTA_ASSIGN(mfn_wendpoint_bssid);
    FTA_ASSIGN(mfn_wendpoint_status);
    FTA_ASSIGN(mfn_wendpoint_stats);
    FTA_ASSIGN(mfn_wendpoint_wps_start);
    FTA_ASSIGN(mfn_wendpoint_wps_cancel);
    FTA_ASSIGN(mfn_wendpoint_enable_vendor_roaming);
    FTA_ASSIGN(mfn_wendpoint_update_vendor_roaming);
    FTA_ASSIGN(mfn_wendpoint_multiap_enable);
    FTA_ASSIGN(mfn_wendpoint_set_mac_address);

    FTA_ASSIGN(mfn_wvap_request_rrm_report);
}

