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

#define ME "rad"

#include <stdlib.h>
#include <debug/sahtrace.h>



#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <swla/swla_commonLib.h>
#include <swla/swla_conversion.h>
#include <swl/swl_string.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_endpoint.h"
#include "wld_wps.h"
#include "wld_channel.h"
#include "wld_chanmgt.h"
#include "swl/swl_assert.h"
#include "swla/swla_oui.h"
#include "swl/swl_hex.h"
#include "wld_statsmon.h"
#include "wld_channel_types.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_stamon.h"
#include "wld_assocdev.h"
#include "wld_eventing.h"
#include "Utils/wld_autoCommitMgr.h"

#include "wld_hostapd_cfgFile.h"
#include "wld_rad_nl80211.h"
#include "wld_linuxIfStats.h"
#include "wld_linuxIfUtils.h"
#include "wld_dm_trans.h"

#define GETENV(x, var) \
    { \
        char* str = getenv(var); \
        int __ret = snprintf(x, sizeof(x), "%s", str ? str : ""); \
        if(__ret < 0 || __ret >= (int) sizeof(x)) { \
            x[0] = 0;} \
    }

const char* Rad_SupStatus[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};
const char* wld_status_str[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};

const char** Rad_SupFreqBands = swl_freqBandExt_str;

const char** Rad_SupBW = swl_bandwidth_str;

const char* Rad_RadarZones[RADARZONES_MAX] = {"NONE", "ETSI", "STG", "UNCLASSIFIED", "FCC", "JP"};

const char* Rad_SupCExt[] = {"Auto", "AboveControlChannel", "BelowControlChannel", "None", NULL};
const char* Rad_SupGI[] = {"Auto", "400nsec", "800nsec", 0};

const char* wld_rad_autoBwSelectMode_str[] = {"MaxAvailable", "MaxCleared", "Default", NULL};

const char* cstr_chanmgt_rad_state[CM_RAD_MAX + 1] = {
    "Unknown",
    "Down",
    "Up",
    "FG_CAC",
    "BG_CAC",
    "BG_CAC_EXT",
    "BG_CAC_NS",
    "BG_CAC_EXT_NS",
    "Configuring",
    "DeepPowerDown",
    "DelayApUp",
    "Error",
    NULL
};

const char* g_str_wld_he_cap[HE_CAP_MAX] = {
    "DL_OFDMA",
    "UL_OFDMA",
    "DL_MUMIMO",
    "UL_MUMIMO"
};

const char* g_str_wld_rad_bf_cap[RAD_BF_CAP_MAX] = {
    "DEFAULT",
    "VHT_SU_BF",
    "VHT_MU_BF",
    "HE_SU_BF",
    "HE_MU_BF",
    "HE_CQI_BF"
};

const char* g_wld_channelChangeReason_str[CHAN_REASON_MAX] = {
    "INVALID",
    "UNKNOWN",
    "INITIAL",
    "AUTO",
    "DFS",
    "MANUAL",
    "REENTRY",
    "OBSS_COEX"
};

const char* g_str_wld_tpcMode[TPCMODE_MAX] = {
    "Off",
    "Sta",
    "Ap",
    "ApSta",
    "Auto",
};

static const char* Rad_RIFS_MODE[RIFS_MODE_MAX] = {"Default", "Auto", "Off", "On"};

amxc_llist_t g_radios = { NULL, NULL };

SWL_ARRAY_TYPE_C(gtWld_type_statusArray, gtSwl_type_uint32, RST_MAX, true, true);
SWL_NTT_C(gtWld_status_changeInfo, wld_status_changeInfo_t, X_WLD_STATUS_CHANGE_INFO);

const char* radCounterDefaults[WLD_RAD_EV_MAX] = {
    "0",
    "0",
    "",
};

static void s_doSyncUpdate(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    amxd_object_t* pRadioObj = pRad->pBus;

    wld_rad_init_counters(pRad, &pRad->genericCounters, radCounterDefaults);
    wld_radStaMon_init(pRad);
    syncData_VendorWPS2OBJ(NULL, pRad, GET);
    pRad->pFA->mfn_sync_radio(pRadioObj, pRad, SET | NO_COMMIT);
    amxd_object_t* dfsObj = amxd_object_get(pRadioObj, "DFS");
    if(dfsObj == NULL) {
        SAH_TRACEZ_ERROR(ME, "cannot found DFS object on %s", pRad->Name);
    } else {
        pRad->dfsEventLogLimit = amxd_object_get_uint8_t(dfsObj, "EventLogLimit", NULL);
        pRad->dfsFileLogLimit = amxd_object_get_uint8_t(dfsObj, "FileLogLimit", NULL);
    }
}

static amxd_status_t _linkFirstUinitRadio(amxd_object_t* pRadioObj, swl_freqBandExt_e band) {
    ASSERT_NOT_NULL(pRadioObj, amxd_status_unknown_error, ME, "NULL");
    T_Radio* pRad = wld_getUinitRadioByBand(band);
    ASSERTW_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No uninit pRad for frequency %s", swl_freqBandExt_str[band]);

    ASSERT_NULL(pRadioObj->priv, amxd_status_unknown_error, ME, "pRadioObj->priv not NULL %s", pRad->Name);

    swl_str_copy(pRad->instanceName, sizeof(pRad->instanceName), amxd_object_get_name(pRadioObj, AMXD_OBJECT_NAMED));
    SAH_TRACEZ_WARNING(ME, "Mapping new Radio instance [%s:%s]", pRad->Name, pRad->instanceName);

    /* General initializations */
    pRadioObj->priv = pRad;
    pRad->pBus = pRadioObj;

    return amxd_status_ok;
}

void _wld_radio_addInstance_ocf(const char* const sig_name _UNUSED,
                                const amxc_var_t* const data,
                                void* const priv _UNUSED) {
    amxd_dm_t* dm = get_wld_plugin_dm();
    amxd_object_t* templ_obj = amxd_dm_signal_get_object(dm, data);
    ASSERT_NOT_NULL(templ_obj, , ME, "Could not get template object");
    amxd_object_t* instance = amxd_object_get_instance(templ_obj, NULL, GET_UINT32(data, "index"));
    ASSERT_NOT_NULL(instance, , ME, "Could not get the added instance");
    const char* OFB = GETP_CHAR(data, "parameters.OperatingFrequencyBand");
    /* Link radio/object */
    T_Radio* pR = instance->priv;
    if(pR == NULL) {
        ASSERTW_STR(OFB, , ME, "Missing OperFreqBand");
        swl_freqBandExt_e band = swl_conv_charToEnum(OFB, Rad_SupFreqBands, SWL_FREQ_BAND_EXT_MAX, SWL_FREQ_BAND_EXT_2_4GHZ);
        _linkFirstUinitRadio(instance, band);
        pR = instance->priv;
        ASSERT_NOT_NULL(pR, , ME, "NULL");
        pR->operatingFrequencyBand = band;
    }
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "NO radio Ctx");
    s_doSyncUpdate(pR);
    SAH_TRACEZ_INFO(ME, "%s: added instance object(%p:%s:%s)", pR->Name, instance, pR->instanceName, OFB);
}

/*
    This function will update the read-only field based on the
    driver info.
 */
amxd_status_t _wld_rad_setEnable_pwf(amxd_object_t* wifiRad,
                                     amxd_param_t* parameter _UNUSED,
                                     amxd_action_t reason _UNUSED,
                                     const amxc_var_t* const args _UNUSED,
                                     amxc_var_t* const retval _UNUSED,
                                     void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;

    ASSERTS_EQUALS(amxd_object_get_type(wifiRad), amxd_object_instance, rv, ME, "TEMPLATE");

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    ASSERTS_EQUALS(rv, amxd_status_ok, rv, ME, "STATUS NOK");

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "INVALID");
    SAH_TRACEZ_IN(ME);
    bool flag = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "set Enable - %s: %d --> %d", pR->Name, pR->enable, flag);
    pR->pFA->mfn_wrad_enable(pR, flag, SET | DIRECT);   // Fix Bug 20729

    wld_autoCommitMgr_notifyRadEdit(pR);

    if(flag) {
        pR->changeInfo.nrEnables++;
        pR->changeInfo.lastEnableTime = swl_time_getMonoSec();
    } else {
        pR->changeInfo.lastDisableTime = swl_time_getMonoSec();
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setKickRoamSta_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    SAH_TRACEZ_IN(ME);
    bool flag = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "setKickRoamSta %p %p %d", pR, wifiRad, flag);
    pR->kickRoamStaEnabled = flag;

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _getSupportedFrequencyBands(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_object_t* wifiRad = NULL;
    T_Radio* pR = NULL;
    char TBuf[128];
    char* pSFB;
    int idx;

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "getSupportedFrequencyBands %p", parameter);

    wifiRad = amxd_param_get_owner(parameter);
    pR = (T_Radio*) wifiRad->priv;
    if(pR && debugIsRadPointer(pR)) {
        /* Update also the supported Band selection (Shop Shop) */
        TBuf[0] = '\0';
        for(idx = 0; idx < SWL_FREQ_BAND_MAX; idx++) {
            if(pR->supportedFrequencyBands & (1 << idx)) {
                if(idx && TBuf[0]) {
                    wldu_catStr(TBuf, ",", sizeof(TBuf));
                }
                wldu_catStr(TBuf, swl_freqBandExt_str[idx], sizeof(TBuf));   // 0 == NONE !
            }
        }
        pSFB = wld_getVendorParam(pR, "SupportedFrequencyBands", TBuf);
        amxc_var_t value;
        amxc_var_init(&value);
        amxc_var_set_cstring_t(&value, pSFB);
        amxd_param_set_value(parameter, &value);
        amxc_var_clean(&value);
        free(pSFB);
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_validateChannel_pvf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* param,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t status = amxd_status_invalid_value;
    T_Radio* pRad = (T_Radio*) object->priv;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No radio mapped");
    uint32_t currentValue = amxc_var_dyncast(uint32_t, &param->value);
    uint32_t newValue = amxc_var_dyncast(uint32_t, args);
    if((currentValue == newValue) || (wld_rad_hasChannel(pRad, newValue))) {
        return amxd_status_ok;
    }
    SAH_TRACEZ_ERROR(ME, "%s: invalid channel %d", pRad->Name, newValue);
    return amxd_status_invalid_value;
}

amxd_status_t _wld_rad_setChannel_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {

    bool epConnected = false;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    SAH_TRACEZ_IN(ME);
    int32_t channel = amxc_var_dyncast(int32_t, args);
    ASSERTI_NOT_NULL(pR, amxd_status_ok, ME, "Invalid radio");

    SAH_TRACEZ_INFO(ME, "set RadioChannel %p %p %d", pR, wifiRad, channel);

    if(pR && debugIsRadPointer(pR)) {
        SAH_TRACEZ_INFO(ME, "STATUS %d %d %d",
                        pR->channel,
                        pR->autoChannelEnable,
                        pR->autoChannelSetByUser);
        if(channel) {
            /* Disable autochannel if the selected channel isn't valid
            ** FIX ME: Have an issue here in 5GHz channels...
            ** If it was disabled by user, be sure it's fully disabled!
            */
            if(!pR->autoChannelSetByUser && pR->autoChannelEnable) {
                pR->pFA->mfn_wrad_autochannelenable(pR, 0, SET);
            }
            /**
             * Find if any EP from the radio is connected in order
             * to avoid any channel that may lose connection with
             * AP
             */
            T_EndPoint* ep = wld_rad_getEnabledEndpoint(pR);
            if(ep && (ep->connectionStatus == EPCS_CONNECTED)) {
                epConnected = true;
                SAH_TRACEZ_INFO(ME, "%s: EP %s connected, not change channel", pR->Name, ep->Name);
            }
            /* If we're in auto channel mode, the given channel is probably
            ** the one we've returned during the commit() state.
            */
            if(!pR->autoChannelEnable && !epConnected) {
                if(channel != pR->channel) {
                    pR->pFA->mfn_wrad_channel(pR, channel, SET);
                    wld_autoCommitMgr_notifyRadEdit(pR);
                    pR->channelChangeReason = CHAN_REASON_MANUAL;
                    wld_rad_chan_notification(pR, channel, pR->runningChannelBandwidth);
                }
            } else {
                pR->channel = channel;
                pR->channelChangeReason = CHAN_REASON_AUTO;
            }
        } else {
            pR->pFA->mfn_wrad_autochannelenable(pR, 1, SET);
        }
        SAH_TRACEZ_INFO(ME, "STATUS %d %d %d",
                        pR->channel,
                        pR->autoChannelEnable,
                        pR->autoChannelSetByUser);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setAPMode_pwf(amxd_object_t* object _UNUSED,
                                     amxd_param_t* parameter _UNUSED,
                                     amxd_action_t reason _UNUSED,
                                     const amxc_var_t* const args _UNUSED,
                                     amxc_var_t* const retval _UNUSED,
                                     void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool APMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set AP_Mode %d", APMode);
    if(pR) {
        pR->isAP = APMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }
    return rv;
}

amxd_status_t _wld_rad_setSTAMode_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool STAMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set STA_Mode %d", STAMode);
    if(pR) {
        pR->isSTA = STAMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }

    return rv;
}

amxd_status_t _wld_rad_setWDSMode_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool wdsMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set WDS_Mode %d", wdsMode);
    if(pR) {
        pR->isWDS = wdsMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }

    return rv;
}

amxd_status_t _wld_rad_setWETMode_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool wetMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set WET_Mode %d", wetMode);
    if(pR) {
        pR->isWET = wetMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }

    return rv;
}

amxd_status_t _wld_rad_setStaSupMode_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool StaSupMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set STASup_Mode %d", StaSupMode);
    if(pR) {
        pR->isSTASup = StaSupMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }

    return rv;
}

// Chicken Egg issue with WPS when both STA and AP have WPS set...
// This radio setting will decide how we will manage this!
amxd_status_t _wld_rad_setWPSEnrolMode_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    bool WPS_EnrolleeMode = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set WPSEnrol_Mode %d", WPS_EnrolleeMode);
    if(pR) {
        pR->isWPSEnrol = WPS_EnrolleeMode;
        pR->fsmRad.FSM_SyncAll = TRUE;
    }

    return rv;
}

amxd_status_t _wld_rad_setAutoChannelEnable_pwf(amxd_object_t* object _UNUSED,
                                                amxd_param_t* parameter _UNUSED,
                                                amxd_action_t reason _UNUSED,
                                                const amxc_var_t* const args _UNUSED,
                                                amxc_var_t* const retval _UNUSED,
                                                void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    ASSERTS_NOT_NULL(pRad, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "NULL");

    bool newAutochan = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "%s: ACS STATUS PRE  %d %d %d => val %u",
                    pRad->Name,
                    pRad->channel,
                    pRad->autoChannelEnable,
                    pRad->autoChannelSetByUser,
                    newAutochan);

    int ret = pRad->pFA->mfn_wrad_autochannelenable(pRad, newAutochan, SET);
    wld_autoCommitMgr_notifyRadEdit(pRad);
    SAH_TRACEZ_INFO(ME, "%s: ACS STATUS POST %d %d %d : ret %d",
                    pRad->Name,
                    pRad->channel,
                    pRad->autoChannelEnable,
                    pRad->autoChannelSetByUser,
                    ret);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_validateOperatingChannelBandwidth_pvf(amxd_object_t* object,
                                                             amxd_param_t* param,
                                                             amxd_action_t reason _UNUSED,
                                                             const amxc_var_t* const args,
                                                             amxc_var_t* const retval _UNUSED,
                                                             void* priv _UNUSED) {
    amxd_status_t status = amxd_status_invalid_value;
    T_Radio* pRad = (T_Radio*) object->priv;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No radio mapped");
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    swl_bandwidth_e radBw;
    if((swl_str_matches(currentValue, newValue)) ||
       ((radBw = swl_conv_charToEnum(newValue, Rad_SupBW, SWL_BW_MAX, SWL_BW_MAX) < SWL_BW_MAX) &&
        ((radBw == SWL_BW_AUTO) || (swl_chanspec_bwToInt(radBw) <= swl_chanspec_bwToInt(pRad->maxChannelBandwidth))))) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s: unsupported operating channel bandwidth(%s)", pRad->Name, newValue);
    }
    free(newValue);
    return status;
}

amxd_status_t _wld_rad_setOperatingChannelBandwidth_pwf(amxd_object_t* object _UNUSED,
                                                        amxd_param_t* parameter _UNUSED,
                                                        amxd_action_t reason _UNUSED,
                                                        const amxc_var_t* const args _UNUSED,
                                                        amxc_var_t* const retval _UNUSED,
                                                        void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "NULL");

    const char* OCBW = amxc_var_constcast(cstring_t, args);

    swl_bandwidth_e radBw = swl_conv_charToEnum(OCBW, Rad_SupBW, SWL_BW_MAX, SWL_BW_AUTO);
    bool autoBwChange = (pRad->operatingChannelBandwidth == SWL_BW_AUTO || radBw == SWL_BW_AUTO);
    pRad->channelBandwidthChangeReason = CHAN_REASON_MANUAL;

    int ret = pRad->pFA->mfn_wrad_ochbw(pRad, radBw, SET);
    SAH_TRACEZ_INFO(ME, "%s set OCBW %u : %u, %u", pRad->Name, radBw, autoBwChange, ret);

    if(autoBwChange) {
        // if autobandwidth is enabled, and going from or to autobw, trigger autochannelEnable
        pRad->pFA->mfn_wrad_autochannelenable(pRad, pRad->autoChannelEnable, SET);
    }
    wld_autoCommitMgr_notifyRadEdit(pRad);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setAutoBandwidthSelectMode_pwf(amxd_object_t* object _UNUSED,
                                                      amxd_param_t* parameter _UNUSED,
                                                      amxd_action_t reason _UNUSED,
                                                      const amxc_var_t* const args _UNUSED,
                                                      amxc_var_t* const retval _UNUSED,
                                                      void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "NULL");

    const char* TCBW = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set AutoBandwidthSelectMode %p", TCBW);
    pRad->autoBwSelectMode = swl_conv_charToEnum(TCBW, wld_rad_autoBwSelectMode_str, BW_SELECT_MODE_MAX, BW_SELECT_MODE_DEFAULT);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setObssCoexistenceEnable_pwf(amxd_object_t* object _UNUSED,
                                                    amxd_param_t* parameter _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args _UNUSED,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    bool flag = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set ObssCoexistenceEnable %d", flag);
    if(pR && debugIsRadPointer(pR)) {
        pR->obssCoexistenceEnabled = flag;
        wld_rad_doSync(pR);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setExtensionChannel_pwf(amxd_object_t* object _UNUSED,
                                               amxd_param_t* parameter _UNUSED,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args _UNUSED,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {
    return amxd_status_ok;
}

static void checkRadioEventLogLimitReached (T_Radio* pR) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    SAH_TRACEZ_IN(ME);
    amxd_object_t* evt = amxd_object_get(pR->pBus, "DFS.Event");
    amxc_llist_it_t* it = amxd_object_first_instance(evt);
    amxd_object_t* chld = amxc_llist_it_get_data(it, amxd_object_t, it);
    while(pR->dfsEventNbr > pR->dfsEventLogLimit) {
        ASSERT_NOT_NULL(chld, , ME, "NULL");
        amxd_object_delete(&chld);
        amxc_llist_it_t* it = amxd_object_first_instance(chld);
        chld = amxc_llist_it_get_data(it, amxd_object_t, it);
        pR->dfsEventNbr--;
    }
    SAH_TRACEZ_OUT(ME);
}

bool wld_rad_addDFSEvent(T_Radio* pR, T_DFSEvent* evt) {
    ASSERT_NOT_NULL(pR, false, ME, "NULL");

    amxd_object_t* event = amxd_object_findf(pR->pBus, "DFS.Event");
    ASSERT_NOT_NULL(event, false, ME, "NULL");
    amxd_object_t* evtInstance;
    amxd_object_new_instance(&evtInstance, event, NULL, 0, NULL);

    swl_time_objectParamSetReal(evtInstance, "TimeStamp", evt->timestamp);

    amxd_object_set_uint32_t(evtInstance, "Channel", evt->channel);
    amxd_object_set_cstring_t(evtInstance, "Bandwidth", Rad_SupBW[evt->bandwidth]);
    amxd_object_set_cstring_t(evtInstance, "RadarZone", Rad_RadarZones[evt->radarZone]);
    amxd_object_set_uint8_t(evtInstance, "RadarIndex", evt->radarIndex);

    amxd_object_set_uint32_t(evtInstance, "NewChannel", pR->channel);
    amxd_object_set_cstring_t(evtInstance, "NewBandwidth", swl_bandwidth_str[pR->runningChannelBandwidth]);

    swl_typeUInt8_arrayObjectParamSetChar(evtInstance, "DFSRadarDetectionList",
                                          pR->lastRadarChannelsAdded, pR->nrLastRadarChannelsAdded);

    pR->dfsEventNbr++;
    checkRadioEventLogLimitReached(pR);

    return true;
}

swl_chanspec_t wld_rad_getSwlChanspec(T_Radio* pRad) {
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(pRad->channel, pRad->runningChannelBandwidth, pRad->operatingFrequencyBand);
    return chanspec;
}

amxd_status_t _wld_rad_setGuardInterval_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    int i;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* OCBW = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set GuardInterval %s", OCBW);
    if(pR && debugIsRadPointer(pR)) {
        for(i = 0; i <= RGI_800NSEC; i++) {
            if(!strcmp(Rad_SupGI[i], OCBW)) {
                pR->guardInterval = i;
                pR->pFA->mfn_wrad_guardintval(pR, NULL, pR->guardInterval, SET);
                break;
            }
        }
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _getRadioStats(amxd_object_t* object,
                             amxd_function_t* func _UNUSED,
                             amxc_var_t* args _UNUSED,
                             amxc_var_t* retval) {

    SAH_TRACEZ_INFO(ME, "getRadioStats");
    T_Radio* pR = object->priv;

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NULL");
    amxd_object_t* stats = amxd_object_get(pR->pBus, "Stats");

    if(pR->pFA->mfn_wrad_stats(pR) < 0) {
        wld_updateRadioStats(pR, NULL);
    }

    wld_util_stats2Obj(stats, &pR->stats);
    wld_util_stats2Var(retval, &pR->stats);

    wld_util_writeWmmStats(stats, "WmmPacketsSent", pR->stats.WmmPacketsSent);
    wld_util_writeWmmStats(stats, "WmmPacketsReceived", pR->stats.WmmPacketsReceived);
    wld_util_writeWmmStats(stats, "WmmFailedSent", pR->stats.WmmFailedSent);
    wld_util_writeWmmStats(stats, "WmmFailedReceived", pR->stats.WmmFailedReceived);
    wld_util_writeWmmStats(stats, "WmmBytesSent", pR->stats.WmmBytesSent);
    wld_util_writeWmmStats(stats, "WmmFailedbytesSent", pR->stats.WmmFailedBytesSent);
    wld_util_writeWmmStats(stats, "WmmBytesReceived", pR->stats.WmmBytesReceived);
    wld_util_writeWmmStats(stats, "WmmFailedBytesReceived", pR->stats.WmmFailedBytesReceived);

    return amxd_status_ok;
}

static void s_updateRadioStatsvalues(T_Radio* pR, amxd_object_t* stats) {
    ASSERT_NOT_NULL(pR, , ME, "pRadio NULL");
    ASSERT_NOT_NULL(stats, , ME, "stats NULL");

    if(pR->pFA->mfn_wrad_stats(pR) < 0) {
        wld_updateRadioStats(pR, NULL);
    }

    WLD_SET_VAR_UINT64(stats, "BytesSent", pR->stats.BytesSent);
    WLD_SET_VAR_UINT64(stats, "BytesReceived", pR->stats.BytesReceived);
    WLD_SET_VAR_UINT64(stats, "PacketsSent", pR->stats.PacketsSent);
    WLD_SET_VAR_UINT64(stats, "PacketsReceived", pR->stats.PacketsReceived);
    WLD_SET_VAR_UINT32(stats, "ErrorsSent", pR->stats.ErrorsSent);
    WLD_SET_VAR_UINT32(stats, "RetransCount", pR->stats.RetransCount);
    WLD_SET_VAR_UINT32(stats, "ErrorsReceived", pR->stats.ErrorsReceived);
    WLD_SET_VAR_UINT32(stats, "UnicastPacketsSent", pR->stats.UnicastPacketsSent);
    WLD_SET_VAR_UINT32(stats, "UnicastPacketsReceived", pR->stats.UnicastPacketsReceived);
    WLD_SET_VAR_UINT32(stats, "DiscardPacketsSent", pR->stats.DiscardPacketsSent);
    WLD_SET_VAR_UINT32(stats, "DiscardPacketsReceived", pR->stats.DiscardPacketsReceived);
    WLD_SET_VAR_UINT32(stats, "MulticastPacketsSent", pR->stats.MulticastPacketsSent);
    WLD_SET_VAR_UINT32(stats, "MulticastPacketsReceived", pR->stats.MulticastPacketsReceived);
    WLD_SET_VAR_UINT32(stats, "BroadcastPacketsSent", pR->stats.BroadcastPacketsSent);
    WLD_SET_VAR_UINT32(stats, "BroadcastPacketsReceived", pR->stats.BroadcastPacketsReceived);
    WLD_SET_VAR_UINT32(stats, "UnknownProtoPacketsReceived", pR->stats.UnknownProtoPacketsReceived);
    WLD_SET_VAR_UINT32(stats, "FailedRetransCount", pR->stats.FailedRetransCount);
    WLD_SET_VAR_UINT32(stats, "RetryCount", pR->stats.RetryCount);
    WLD_SET_VAR_UINT32(stats, "MultipleRetryCount", pR->stats.MultipleRetryCount);

    wld_util_updateWmmStats(stats, "WmmPacketsSent", pR->stats.WmmPacketsSent);
    wld_util_updateWmmStats(stats, "WmmPacketsReceived", pR->stats.WmmPacketsReceived);
    wld_util_updateWmmStats(stats, "WmmFailedSent", pR->stats.WmmFailedSent);
    wld_util_updateWmmStats(stats, "WmmFailedReceived", pR->stats.WmmFailedReceived);
    wld_util_updateWmmStats(stats, "WmmBytesSent", pR->stats.WmmBytesSent);
    wld_util_updateWmmStats(stats, "WmmFailedbytesSent", pR->stats.WmmFailedBytesSent);
    wld_util_updateWmmStats(stats, "WmmBytesReceived", pR->stats.WmmBytesReceived);
    wld_util_updateWmmStats(stats, "WmmFailedBytesReceived", pR->stats.WmmFailedBytesReceived);
}

amxd_status_t _wld_radio_getStats_orf(amxd_object_t* const object,
                                      amxd_param_t* const param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const action_retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_ok;
    if(reason != action_object_read) {
        status = amxd_status_function_not_implemented;
        return status;
    }

    amxd_object_t* parentObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(parentObj, amxd_status_ok, ME, "parentObj is NULL");

    T_Radio* pR = (T_Radio*) parentObj->priv;
    s_updateRadioStatsvalues(pR, object);

    status = amxd_action_object_read(object, param, reason, args, action_retval, priv);
    return status;
}

amxd_status_t _getRadioTxPower(amxd_object_t* object _UNUSED,
                               amxd_param_t* parameter _UNUSED,
                               amxd_action_t reason _UNUSED,
                               const amxc_var_t* const args _UNUSED,
                               amxc_var_t* const retval,
                               void* priv _UNUSED) {
    amxd_object_t* wifiRad = NULL;
    T_Radio* pR = NULL;
    int32_t power = 0;

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "GetRadioTxPower %p", parameter);

    wifiRad = amxd_param_get_owner(parameter);
    pR = wifiRad->priv;

    if(pR && debugIsRadPointer(pR)) {
        power = pR->pFA->mfn_wrad_txpow(pR, 0, GET);
        amxc_var_add(int32_t, retval, power);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_validateTxPower_pvf(amxd_object_t* object,
                                           amxd_param_t* param,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    T_Radio* pRad = (T_Radio*) object->priv;
    ASSERTW_NOT_NULL(pRad, amxd_status_ok, ME, "No radio mapped");
    int8_t currentValue = amxc_var_dyncast(int8_t, &param->value);
    int8_t newValue = amxc_var_dyncast(int8_t, args);
    ASSERTS_NOT_EQUALS(currentValue, newValue, amxd_status_ok, ME, "same value");
    for(size_t i = 0; i < SWL_ARRAY_SIZE(pRad->transmitPowerSupported); i++) {
        if(pRad->transmitPowerSupported[i] == newValue) {
            return amxd_status_ok;
        }
    }
    SAH_TRACEZ_ERROR(ME, "%s: unsupported txPwr percentage value(%d)", pRad->Name, newValue);
    return amxd_status_invalid_value;
}

amxd_status_t _wld_rad_getTxPower_prf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval,
                                      void* priv _UNUSED) {
    amxd_status_t status = amxd_status_unknown_error;
    ASSERTS_NOT_NULL(param, amxd_status_unknown_error, ME, "NULL");
    int32_t percentage = -1;
    ASSERTS_EQUALS(reason, action_param_read, amxd_status_function_not_implemented, ME, "not impl");
    T_Radio* pR = (T_Radio*) object->priv;
    SAH_TRACEZ_IN(ME);
    if(pR && debugIsRadPointer(pR)) {
        percentage = pR->pFA->mfn_wrad_txpow(pR, 0, GET);
        if((percentage < -1) || (percentage > 100)) {
            SAH_TRACEZ_WARNING(ME, "%s: out of range txPwr prc %d, consider auto mode", pR->Name, percentage);
            percentage = -1;
        }
    }
    amxc_var_set(int32_t, retval, percentage);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setTxPower_pwf(amxd_object_t* object,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return rv;
    }

    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    T_Radio* pR = (T_Radio*) object->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");

    SAH_TRACEZ_IN(ME);

    int32_t power = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "set TxPower %d", power);
    if(pR && debugIsRadPointer(pR)) {
        pR->pFA->mfn_wrad_txpow(pR, power, SET);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setAntennaCtrl_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int32_t antenna = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "set AntennaCtrl %p %p %i", parameter, pR, antenna);

    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");
    pR->actAntennaCtrl = antenna;
    pR->pFA->mfn_wrad_antennactrl(pR, antenna, SET);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setDriverConfig_owf(amxd_object_t* object) {
    amxd_object_t* radObject = amxd_object_get_parent(object);
    ASSERTI_FALSE(amxd_object_get_type(radObject) == amxd_object_template, amxd_status_unknown_error, ME, "Initial template run, skip");

    T_Radio* pRad = (T_Radio*) radObject->priv;
    ASSERTI_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Radio Object is NULL");
    SAH_TRACEZ_INFO(ME, "%s update driver config", pRad->Name);

    pRad->driverCfg.txBurst = amxd_object_get_int32_t(object, "TxBurst", NULL);
    pRad->driverCfg.ampdu = amxd_object_get_int32_t(object, "Ampdu", NULL);
    pRad->driverCfg.amsdu = amxd_object_get_int32_t(object, "Amsdu", NULL);
    pRad->driverCfg.fragThreshold = amxd_object_get_int32_t(object, "FragmentationThreshold", NULL);
    pRad->driverCfg.rtsThreshold = amxd_object_get_int32_t(object, "RtsThreshold", NULL);
    pRad->driverCfg.txBeamforming = amxd_object_get_int32_t(object, "TxBeamforming", NULL);
    pRad->driverCfg.vhtOmnEnabled = amxd_object_get_bool(object, "VhtOmnEnabled", NULL);
    pRad->driverCfg.broadcastMaxBwCapability = amxd_object_get_int32_t(object, "BroadcastMaxBwCapability", NULL);
    pRad->driverCfg.tpcMode = swl_conv_charToEnum(amxd_object_get_cstring_t(object, "TPCMode", NULL), g_str_wld_tpcMode, TPCMODE_MAX, TPCMODE_OFF);

    wld_rad_doSync(pRad);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setMACConfig_pwf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_action_param_write(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "ERR");
    amxd_object_t* radObject = amxd_object_get_parent(object);
    ASSERTI_EQUALS(amxd_object_get_type(radObject), amxd_object_instance, amxd_status_ok, ME, "template");
    T_Radio* pRad = (T_Radio*) radObject->priv;
    ASSERTI_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Radio Object is NULL");
    const char* pname = param->name;
    if(swl_str_matches(pname, "UseBaseMacOffset")) {
        pRad->macCfg.useBaseMacOffset = amxc_var_dyncast(bool, args);
    } else if(swl_str_matches(pname, "UseLocalBitForGuest")) {
        pRad->macCfg.useLocalBitForGuest = amxc_var_dyncast(bool, args);
    } else if(swl_str_matches(pname, "BaseMacOffset")) {
        pRad->macCfg.baseMacOffset = amxc_var_dyncast(int64_t, args);
    } else if(swl_str_matches(pname, "LocalGuestMacOffset")) {
        pRad->macCfg.localGuestMacOffset = amxc_var_dyncast(int64_t, args);
    } else if(swl_str_matches(pname, "NrBssRequired")) {
        pRad->macCfg.nrBssRequired = amxc_var_dyncast(int64_t, args);
    }
    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_rad_setRxChainCtrl_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int32_t antenna = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "set RxChainCtrl %p %p %i", parameter, pR, antenna);

    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");

    pR->rxChainCtrl = antenna;
    pR->pFA->mfn_wrad_antennactrl(pR, pR->actAntennaCtrl, SET);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setTxChainCtrl_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    int32_t antenna = amxc_var_dyncast(int32_t, args);

    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "set TxChainCtrl %p %p %i", parameter, pR, antenna);

    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");

    pR->txChainCtrl = antenna;
    pR->pFA->mfn_wrad_antennactrl(pR, pR->actAntennaCtrl, SET);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

/**
 * Enables debugging on third party software modules.
 * The goal of this is only for developpers!
 * We need a way to have/enable a better view how things are managed by some deamons.
 */
amxd_status_t _wld_rad_setDbgEnable_pwf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter _UNUSED,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args _UNUSED,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    int dbgRadEnable = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "set dbgEnable %d", dbgRadEnable);

    if(pR && debugIsRadPointer(pR)) {
        pR->dbgEnable = dbgRadEnable;
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setDbgFile_pwf(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter _UNUSED,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* dbgFile = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set DbgFile %s", dbgFile);
    if(pR && debugIsRadPointer(pR) && dbgFile) {
        if(pR->dbgOutput) {
            free(pR->dbgOutput);
        }
        pR->dbgOutput = strdup(dbgFile);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

/* Setting Retry Limit */
amxd_status_t _wld_rad_setRetryLimit_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    uint8_t retryLimitValue = amxc_var_dyncast(uint8_t, args);
    SAH_TRACEZ_INFO(ME, "set RetryLimit %d", retryLimitValue);

    // Set Retry Limit and update Radio object param !
    if(pR && debugIsRadPointer(pR)) {
        pR->retryLimit = retryLimitValue;
        wld_rad_doSync(pR);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}
/* Setting Long Retry Limit */
amxd_status_t _wld_rad_setLongRetryLimit_pwf(amxd_object_t* object _UNUSED,
                                             amxd_param_t* parameter _UNUSED,
                                             amxd_action_t reason _UNUSED,
                                             const amxc_var_t* const args _UNUSED,
                                             amxc_var_t* const retval _UNUSED,
                                             void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    uint8_t lRetryLimitValue = amxc_var_dyncast(uint8_t, args);
    SAH_TRACEZ_INFO(ME, "set LongRetryLimit %d", lRetryLimitValue);

    // Set Long Retry Limit and update Radio object param !
    if(pR && debugIsRadPointer(pR)) {
        pR->longRetryLimit = lRetryLimitValue;
        wld_rad_doSync(pR);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}


amxd_status_t _wld_rad_setBeaconPeriod_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERTI_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "NULL");

    uint32_t newVal = amxc_var_dyncast(uint32_t, args);
    SAH_TRACEZ_INFO(ME, "%p %u", parameter, newVal);
    pR->beaconPeriod = newVal;
    wld_rad_doSync(pR);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setDtimPeriod_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter _UNUSED,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTI_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "NULL");

    uint32_t newVal = amxc_var_dyncast(uint32_t, args);

    SAH_TRACEZ_INFO(ME, "%p %u", parameter, newVal);

    pR->dtimPeriod = newVal;
    wld_rad_doSync(pR);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

/* Setting Target Wake Time Enable */
amxd_status_t _wld_rad_setTargetWakeTimeEnable_pwf(amxd_object_t* object _UNUSED,
                                                   amxd_param_t* parameter _UNUSED,
                                                   amxd_action_t reason _UNUSED,
                                                   const amxc_var_t* const args _UNUSED,
                                                   amxc_var_t* const retval _UNUSED,
                                                   void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTS_NOT_NULL(pR, amxd_status_ok, ME, "NULL"); // silent fail if userdata not yet set
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");

    pR->twtEnable = amxc_var_dyncast(bool, args);
    wld_rad_doSync(pR);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setOfdmaEnable_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTS_NOT_NULL(pRad, amxd_status_ok, ME, "NULL"); // silent fail if userdata not yet set
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "INVALID");

    pRad->ofdmaEnable = amxc_var_dyncast(bool, args);
    wld_rad_doSync(pRad);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setHeCaps_pwf(amxd_object_t* object _UNUSED,
                                     amxd_param_t* parameter _UNUSED,
                                     amxd_action_t reason _UNUSED,
                                     const amxc_var_t* const args _UNUSED,
                                     amxc_var_t* const retval _UNUSED,
                                     void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    ASSERTS_NOT_NULL(pRad, amxd_status_ok, ME, "NULL"); // silent fail if userdata not yet set
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_ok, ME, "INVALID");

    const char* caps = amxc_var_constcast(cstring_t, args);

    pRad->heCapsEnabled = conv_strToMask(caps, g_str_wld_he_cap, HE_CAP_MAX);
    wld_rad_doSync(pRad);

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_set80211hEnable_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    bool flag = amxc_var_dyncast(bool, args);
    SAH_TRACEZ_INFO(ME, "set 80211hEnable %d", flag);
    if(pR && debugIsRadPointer(pR)) {
        bool newValue = pR->IEEE80211hSupported & flag;
        if(newValue != pR->setRadio80211hEnable) {
            wld_rad_doSync(pR); // Force a resync of the structure (FIX ME)
        }
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _validateIEEE80211hEnabled(amxd_param_t* parameter, void* validationData) {
    _UNUSED_(validationData);
    T_Radio* pR;
    bool flag;

    amxc_var_t Nvalue;
    amxc_var_init(&Nvalue);
    amxd_param_get_value(parameter, &Nvalue);

    flag = amxc_var_get_bool(&Nvalue);
    amxc_var_clean(&Nvalue);
    pR = amxd_param_get_owner(parameter)->priv;

    if(!pR) {
        return amxd_status_ok;
    }

    return ((pR->IEEE80211hSupported && flag) == flag);
}

swl_rc_ne s_setCountryCode(T_Radio* pR, const char* countryCode) {
    ASSERT_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(swl_str_isEmpty(countryCode), SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(swl_str_matches(countryCode, " "), SWL_RC_ERROR, ME, "NULL");
    int indexCountryCode = atoi(countryCode);
    int idx;
    if(!indexCountryCode) {
        getCountryParam(countryCode, 0, &idx);
    } else {
        getCountryParam(NULL, indexCountryCode, &idx);
    }
    pR->regulatoryDomainIdx = idx;
    pR->pFA->mfn_wrad_regdomain(pR, NULL, 0, SET);
    return SWL_RC_OK;
}
amxd_status_t _wld_rad_setCountryCode_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    const char* CC = NULL;
    int iCC = 0, idx = 0;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    CC = amxc_var_constcast(cstring_t, args);
    SAH_TRACEZ_INFO(ME, "set CountryCode %s", CC);

    if(s_setCountryCode(pR, CC) == SWL_RC_OK) {
        wld_autoCommitMgr_notifyRadEdit(pR);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _validateImplicitBeamForming(amxd_param_t* parameter, void* validationData) {
    _UNUSED_(validationData);
    amxd_object_t* wifiRad = NULL;
    T_Radio* pR = NULL;
    amxc_var_t Nvalue;
    amxc_var_init(&Nvalue);
    amxd_param_get_value(parameter, &Nvalue);

    wifiRad = amxd_param_get_owner(parameter);
    pR = wifiRad->priv;

    if(!pR) {
        return amxd_status_ok;
    }

    /* Disabling is always allowed */
    bool enable = amxc_var_get_bool(&Nvalue);
    amxc_var_clean(&Nvalue);

    if(!enable) {
        return amxd_status_ok;
    }

    return pR->implicitBeamFormingSupported;
}

static amxd_status_t _setBeamForming(amxd_param_t* parameter, amxd_action_t reason,
                                     const amxc_var_t* const args,
                                     amxc_var_t* const retval,
                                     void* priv, beamforming_type_t type) {
    int ret;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    if(!pR) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);

    if(type == beamforming_implicit) {
        pR->implicitBeamFormingEnabled = enabled;
    } else {
        pR->explicitBeamFormingEnabled = enabled;
    }

    ret = pR->pFA->mfn_wrad_beamforming(pR, type, enabled, SET);
    wld_autoCommitMgr_notifyRadEdit(pR);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to set %s beam forming",
                         type == beamforming_implicit ? "implicit" : "explicit");
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setImplicitBeamForming_pwf(amxd_object_t* object _UNUSED,
                                                  amxd_param_t* parameter _UNUSED,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args _UNUSED,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    return _setBeamForming(parameter, reason, args, retval, priv, beamforming_implicit);
}

amxd_status_t _validateExplicitBeamForming(amxd_param_t* parameter, void* validationData) {
    _UNUSED_(validationData);
    amxd_object_t* wifiRad = NULL;
    T_Radio* pR = NULL;
    amxc_var_t Nvalue;
    amxc_var_init(&Nvalue);
    amxd_param_get_value(parameter, &Nvalue);

    wifiRad = amxd_param_get_owner(parameter);
    pR = wifiRad->priv;

    if(!pR) {
        return amxd_status_ok;
    }

    bool enable = amxc_var_get_bool(&Nvalue);
    amxc_var_clean(&Nvalue);
    /* Disabling is always allowed */
    if(!enable) {
        return amxd_status_ok;
    }

    return pR->explicitBeamFormingSupported;
}

amxd_status_t _wld_rad_setExplicitBeamForming_pwf(amxd_object_t* object _UNUSED,
                                                  amxd_param_t* parameter _UNUSED,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args _UNUSED,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    return _setBeamForming(parameter, reason, args, retval, priv, beamforming_explicit);
}

static amxd_status_t setBfCap(amxd_param_t* parameter _UNUSED,
                              amxd_action_t reason _UNUSED,
                              const amxc_var_t* const args _UNUSED,
                              amxc_var_t* const retval _UNUSED,
                              void* priv _UNUSED, com_dir_e comDir) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    const char* bfStr = amxc_var_constcast(cstring_t, args);
    ASSERTI_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    wld_rad_bf_cap_m capEnable = swl_conv_charToMask(bfStr, g_str_wld_rad_bf_cap, RAD_BF_CAP_MAX);

    ASSERTI_NOT_EQUALS(capEnable, pR->bfCapsEnabled[comDir], amxd_status_ok, ME, "%s bfCap no change %u %u",
                       pR->Name, comDir, capEnable);

    if((capEnable & !pR->bfCapsSupported[comDir]) && (capEnable != M_RAD_BF_CAP_DEFAULT)) {
        SAH_TRACEZ_WARNING(ME, "%s : enabling unsupported bf cap %u: %u sup %u", pR->Name,
                           comDir, capEnable, pR->bfCapsSupported[comDir]);
    }

    SAH_TRACEZ_INFO(ME, "%s : set bf cap %u : %u from %u / %u",
                    pR->Name, comDir, capEnable, pR->bfCapsEnabled[comDir], pR->bfCapsSupported[comDir]);

    pR->bfCapsEnabled[comDir] = capEnable;
    wld_rad_doSync(pR);

    return rv;
}

amxd_status_t _wld_rad_setRxBfCapsEnabled_pwf(amxd_object_t* object _UNUSED,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args _UNUSED,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    return setBfCap(parameter, reason, args, retval, priv, COM_DIR_RECEIVE);
}

amxd_status_t _wld_rad_setTxBfCapsEnabled_pwf(amxd_object_t* object _UNUSED,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args _UNUSED,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    return setBfCap(parameter, reason, args, retval, priv, COM_DIR_TRANSMIT);
}


amxd_status_t _wld_rad_setRIFS_pwf(amxd_object_t* object _UNUSED,
                                   amxd_param_t* parameter _UNUSED,
                                   amxd_action_t reason _UNUSED,
                                   const amxc_var_t* const args _UNUSED,
                                   amxc_var_t* const retval _UNUSED,
                                   void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    if(!pR) {
        return rv;
    }

    pR->RIFSEnabled = swl_conv_charToEnum(amxc_var_constcast(cstring_t, args), Rad_RIFS_MODE, RIFS_MODE_MAX, RIFS_MODE_DEFAULT);

    int ret = pR->pFA->mfn_wrad_rifs(pR, pR->RIFSEnabled, SET);
    wld_autoCommitMgr_notifyRadEdit(pR);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "%s :Failed to put RIFS to %s", pR->Name, Rad_RIFS_MODE[pR->RIFSEnabled]);
    }

    return rv;
}

amxd_status_t _wld_rad_setAirTimeFairness_pwf(amxd_object_t* object _UNUSED,
                                              amxd_param_t* parameter _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args _UNUSED,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    int ret;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    if(!pR) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);

    pR->airtimeFairnessEnabled = enabled;

    ret = pR->pFA->mfn_wrad_airtimefairness(pR, enabled, SET);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to %s AirTimeFairness",
                         enabled ? "enable" : "disable");
    }
    wld_autoCommitMgr_notifyRadEdit(pR);

    return rv;
}

/**
 * Callback function to set intelligent airtime.
 * This function will call the driver set intelligent airtime with the updated enable flag.
 * It will only do so if the current intelligentAirtimeScheduleEnabled status differs from the requested
 * state.
 * Note! It is up to the plugin to actually change the radio value, if valid.
 */
amxd_status_t _wld_rad_setIntelligentAirtime_pwf(amxd_object_t* object _UNUSED,
                                                 amxd_param_t* parameter _UNUSED,
                                                 amxd_action_t reason _UNUSED,
                                                 const amxc_var_t* const args _UNUSED,
                                                 amxc_var_t* const retval _UNUSED,
                                                 void* priv _UNUSED) {
    int ret;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if((rv != amxd_status_ok) || !pR) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);
    if(enabled != pR->intAirtimeSchedEnabled) {
        ret = pR->pFA->mfn_wrad_intelligentAirtime(pR, enabled, SET);
        if(ret != 0) {
            SAH_TRACEZ_ERROR(ME, "fail %u", enabled);
        } else {
            SAH_TRACEZ_INFO(ME, "set %u", enabled);
            wld_autoCommitMgr_notifyRadEdit(pR);
        }
    } else {
        SAH_TRACEZ_INFO(ME, "keep %u", enabled);
    }

    SAH_TRACEZ_OUT(ME);
    return rv;
}

amxd_status_t _wld_rad_setRxPowerSave_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if((rv != amxd_status_ok) || !pR) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");

    pR->rxPowerSaveEnabled = enabled;

    int ret = pR->pFA->mfn_wrad_rx_powersave(pR, enabled, SET);
    wld_autoCommitMgr_notifyRadEdit(pR);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "%s : Failed to %s power save",
                         pR->Name, enabled ? "enable" : "disable");
    }

    return rv;
}


amxd_status_t _wld_rad_setRxPowerSaveRepeater_pwf(amxd_object_t* object _UNUSED,
                                                  amxd_param_t* parameter _UNUSED,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args _UNUSED,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if((rv != amxd_status_ok) || !pR) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_ok, ME, "INVALID");

    pR->rxPowerSaveEnabledWhenRepeater = enabled;
    if(!wld_rad_hasEnabledEp(pR)) {
        SAH_TRACEZ_INFO(ME, "%s: no active repeater, ignore set", pR->Name);
        return amxd_status_ok;
    }

    int ret = pR->pFA->mfn_wrad_rx_powersave(pR, enabled, SET);
    wld_autoCommitMgr_notifyRadEdit(pR);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "%s : Failed to %s power save on repeater",
                         pR->Name, enabled ? "enable" : "disable");
    }
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setMultiUserMIMO_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    int ret;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return amxd_status_ok;
    }

    T_Radio* pR = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    bool enabled = amxc_var_dyncast(bool, args);
    if(!pR) {
        return amxd_status_ok;
    }

    pR->multiUserMIMOEnabled = enabled;

    ret = pR->pFA->mfn_wrad_multiusermimo(pR, enabled, SET);
    wld_autoCommitMgr_notifyRadEdit(pR);
    if(ret != 0) {
        SAH_TRACEZ_INFO(ME, "Failed to %s Multi-User MIMO", enabled ? "enable" : "disable");
    }
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setMaxStations_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter _UNUSED,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return amxd_status_ok;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    int flag = amxc_var_dyncast(int32_t, args);
    SAH_TRACEZ_INFO(ME, "set MaxStations %d", flag);

    // Set MaxStations supported and update Radio object param !
    if(pRad && debugIsRadPointer(pRad)) {
        pRad->maxStations = flag;
        wld_rad_doSync(pRad);
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

// See this as
const char* DFST_RP_Help[] = {
    "DFS_drvdbg (dbg_action:string) the DFS Radar pulse command",
    "-h shows full help",
    NULL
};

const char* DFST_RP_STR[] = {
    "Do nothing (default)",                                                  /* DFST_RP_NONE */
    "DFS Radar Pulse when ON-channel",                                       /* DFST_RP_ON_CH      */
    "DFS Radar Pulse when OFF-Channel",                                      /* DFST_RP_OFF_CH     */
    "Driver default Radar Pulse (When on DFS \"ON\", when on !DFS \"OFF\")", /* DFST_RP_AUTO       */
    "Force to Clear Radar status",                                           /* DFST_RP_CLR_STATUS */
    "Force to Clear Out of Service channels",                                /* DFST_RP_CLR_OSC    */
    NULL                                                                     /* DFST_RP_MAX */
};

/**
 * @name setDFSForceRadarTrigger
 * @details This function is a helper for DFS AUTOMATION testing actions.
 *
 */
amxd_status_t _DFS_drvdbg(amxd_object_t* object,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval) {
    T_Radio* pR = NULL;
    int act_idx;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    pR = object->priv;
    const char* StrVal = GET_CHAR(args, "dbg_action");

    // String as we can parse it in any format?
    if(!pR) {
        SAH_TRACEZ_ERROR(ME, "pR %p", pR);
        amxc_var_set(int32_t, retval, -1);
        return amxd_status_ok;
    }

    if(!(StrVal && StrVal[0])) {
        SAH_TRACEZ_ERROR(ME, "StrVal %p", StrVal);
        amxc_var_set(int32_t, retval, -2);
        return amxd_status_unknown_error;
    }

    /* Analyse input, do we've options? Options start with a dash '-'? */
    /* -h = help */
    if(StrVal[0] == '-') {
        switch(StrVal[1]) {
        case 'h':
        case 'H':
        default:
        {
            int i;
            SAH_TRACEZ_ERROR(ME, "Syntax Help");
            for(i = 0; DFST_RP_Help[i]; i++) {
                SAH_TRACEZ_ERROR(ME, "%s", DFST_RP_Help[i]);
            }
            // Default Index number action.
            for(i = 0; DFST_RP_STR[i]; i++) {
                SAH_TRACEZ_ERROR(ME, "%d - %s", i, DFST_RP_STR[i]);
            }
        }
            amxc_var_set(uint32_t, retval, 1);
            return amxd_status_ok;
        }
    }
    // Currently we only support/parse a number as action
    act_idx = atoi(StrVal);
    ASSERTS_TRUE((act_idx >= 0 && act_idx < DFST_RP_MAX), amxd_status_unknown_error, ME, "act_idx out of bound");

    /* Call the supported driver command */
    SAH_TRACEZ_ERROR(ME, "Req %s", DFST_RP_STR[act_idx]);
    pR->pFA->mfn_wrad_dfsradartrigger(pR, act_idx);

    /* Update ongoing channel states...*/
    if(pR->pFA->mfn_wrad_fsm_delay_commit) {
        pR->fsmRad.TODC = 1000;
        amxp_timer_start(pR->fsmRad.obj_DelayCommit, pR->fsmRad.TODC);
    }

    amxc_var_set(int32_t, retval, 0);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setFileLogLimit_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    ASSERT_NOT_NULL(wifiRad, amxd_status_unknown_error, ME, "NULL");
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return amxd_status_ok;
    }

    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    uint8_t newFileLogLimit = amxc_var_dyncast(uint8_t, args);

    SAH_TRACEZ_INFO(ME, "setFileLogLimit oldValue:%d newValue:%d", pRad->dfsFileLogLimit, newFileLogLimit);
    pRad->dfsFileLogLimit = newFileLogLimit;
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_rad_setEventLogLimit_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter _UNUSED,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiRad = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiRad) != amxd_object_instance) {
        return rv;
    }

    T_Radio* pRad = (T_Radio*) wifiRad->priv;
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");
    rv = amxd_action_param_write(wifiRad, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    uint8_t newEventLogLimit = amxc_var_dyncast(uint8_t, args);
    SAH_TRACEZ_INFO(ME, "setEventLogLimit oldValue:%d newValue:%d", pRad->dfsEventLogLimit, newEventLogLimit);
    pRad->dfsEventLogLimit = newEventLogLimit;
    checkRadioEventLogLimitReached(pRad);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


amxd_status_t _validateRadioOperatingFrequencyBand(amxd_param_t* parameter, void* validationData _UNUSED) {
    amxd_object_t* wifiRad = NULL;
    T_Radio* pR = NULL;
    amxc_var_t Nvalue;
    amxc_var_init(&Nvalue);
    amxd_param_get_value(parameter, &Nvalue);
    const char* OFB = NULL;
    swl_freqBand_e band;

    SAH_TRACEZ_IN(ME);
    OFB = amxc_var_get_cstring_t(&Nvalue);
    wifiRad = amxd_param_get_owner(parameter);
    pR = wifiRad->priv;
    amxc_var_clean(&Nvalue);

    if(!pR) {
        return amxd_status_ok; // We're creating the object entry but have no valid Radio handler.
    }

    band = swl_conv_charToEnum(OFB, Rad_SupFreqBands, SWL_FREQ_BAND_MAX, M_SWL_FREQ_BAND_EXT_AUTO);
    if(band == M_SWL_FREQ_BAND_EXT_AUTO) {
        SAH_TRACEZ_INFO(ME, "Invalid frequency band %s for %s",
                        OFB, pR->Name);
        return amxd_status_unknown_error;
    }

    if(!(pR->supportedFrequencyBands & (1 << band))) {
        SAH_TRACEZ_INFO(ME, "Frequency band %s not supported on %s",
                        OFB, pR->Name);
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

amxd_status_t _wld_rad_setOperatingFrequencyBand_pwf(amxd_object_t* object,
                                                     amxd_param_t* parameter,
                                                     amxd_action_t reason,
                                                     const amxc_var_t* const args,
                                                     amxc_var_t* const retval,
                                                     void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return rv;
    }
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    const char* OFB = amxc_var_constcast(cstring_t, args);
    ASSERTW_STR(OFB, amxd_status_unknown_error, ME, "Missing param");

    swl_freqBandExt_e band = swl_conv_charToEnum(OFB, Rad_SupFreqBands, SWL_FREQ_BAND_EXT_MAX, SWL_FREQ_BAND_EXT_2_4GHZ);

    /* Link radio/object */
    if(object->priv == NULL) {
        _linkFirstUinitRadio(object, band);
    }

    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NO radio Ctx");

    pR->operatingFrequencyBand = band;

    SAH_TRACEZ_INFO(ME, "set OperatingFrequencyBand %p %s %d", parameter, OFB, band);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**************************************************************************************************/
/**************************************************************************************************/
T_Radio* wld_getRadioDataHandler(amxd_object_t* pobj, const char* rn) {
    char FullPath[128];
    amxd_object_t* radio;
    amxd_object_t* inst;
    if(pobj) {
        radio = amxd_object_get_child(pobj, "Radio");
        inst = amxd_object_get_instance(radio, rn, 0);
        ASSERTI_NOT_NULL(inst, NULL, ME, "%s - %p\n", rn, inst);
        return (T_Radio*) inst->priv;
    }
    return NULL;
}

/**
 * All Radio parameters that must be updated after a commit are
 * handled in this function.
 */
int radio_libsync_status_cb(T_Radio* rad) {

    /* Update NEMO for this... */
    amxd_object_set_cstring_t(rad->pBus, "Status", Rad_SupStatus[rad->status]);
    /* Update also the Channel & AutoChannel value */
    if(rad->channel) {  // Don't set if channel is 0 (== autochannel)
        amxd_object_set_int32_t(rad->pBus, "Channel", rad->channel);
    }
    amxd_object_set_int32_t(rad->pBus, "AutoChannelEnable", rad->autoChannelSetByUser);

    return 0;
}

void wld_radio_updateAntenna(T_Radio* pRad) {
    amxd_object_t* object = pRad->pBus;
    amxd_object_t* hwObject = amxd_object_findf(object, "DriverStatus");
    amxd_object_set_int32_t(hwObject, "NrTxAntenna", pRad->nrAntenna[COM_DIR_TRANSMIT]);
    amxd_object_set_int32_t(hwObject, "NrRxAntenna", pRad->nrAntenna[COM_DIR_RECEIVE]);
    amxd_object_set_int32_t(hwObject, "NrActiveTxAntenna", pRad->nrActiveAntenna[COM_DIR_TRANSMIT]);
    amxd_object_set_int32_t(hwObject, "NrActiveRxAntenna", pRad->nrActiveAntenna[COM_DIR_RECEIVE]);
}

void syncData_Radio2OBJ(amxd_object_t* object, T_Radio* pR, int set) {
    int idx;
    char ValBuf[32];
    char TBuf[320];
    char objPath[128];

    SAH_TRACEZ_IN(ME);

    if(!(object && pR)) {
        /* Missing data pointers... */
        return;
    }

    if(set & SET) {
        memset(ValBuf, 0, sizeof(ValBuf));
        memset(TBuf, 0, sizeof(TBuf));
        memset(objPath, 0, sizeof(objPath));
        /* Set Radio data in mapped OBJ structure */
        /* update saved radio base mac address: matching primary interface (first endpoint/vap) */
        char macStr[SWL_MAC_CHAR_LEN] = {0};
        SWL_MAC_BIN_TO_CHAR(macStr, pR->MACAddr);

        amxd_object_set_cstring_t(object, "VendorPCISig", pR->vendor->name);
        amxd_object_set_cstring_t(object, "Name", pR->Name);

        amxd_object_set_cstring_t(object, "BaseMACAddress", macStr);
        /* 'Status' The current operational state of the radio */
        amxd_object_set_cstring_t(object, "Status", Rad_SupStatus[pR->status]);
        /* 'Index' contains the system interface index for this
         *  device (ip l) */
        amxd_object_set_int32_t(object, "Index", pR->index);
        /* 'MaxBitRate' The maximum PH bit rate supported by this
         *  interface (expressed in Mbps). */

        amxd_object_set_int32_t(object, "MaxBitRate", pR->maxBitRate);
        /* 'SupportedFrequencyBands' Comma-separated list of string.
         *  List items indicate the frequency bands at which the radio
         *  can operate. */
        char supportedFrequencyBandsStr[128] = {0};
        swl_conv_maskToChar(supportedFrequencyBandsStr, sizeof(supportedFrequencyBandsStr), pR->supportedFrequencyBands, swl_freqBandExt_str, SWL_FREQ_BAND_MAX);
        amxd_object_set_cstring_t(object, "SupportedFrequencyBands", supportedFrequencyBandsStr);

        /* 'OperatingFrequencyBand' The value MUST be a member of
         *  the list reported by the SupportedFrequencyBands
         *  parameter. Indicates the frequency band at which the
         *  radio is operating. */
        amxd_object_set_cstring_t(object, "OperatingFrequencyBand", Rad_SupFreqBands[pR->operatingFrequencyBand]);

        /* 'SupportedStandards' Comma separated list of strings.
         *  List items indicate which IEEE802.11 standards this Radio
         *  instance can support simultaneously, in the frequency
         *  band specified by OpertatingFrequencyBand. (CHECK THIS -
         *  FIX ME) */
        swl_radStd_supportedStandardsToChar(TBuf, sizeof(TBuf), pR->supportedStandards, pR->operatingStandardsFormat);
        amxd_object_set_cstring_t(object, "SupportedStandards", TBuf);

        conv_maskToStr(pR->heCapsSupported, g_str_wld_he_cap, HE_CAP_MAX, TBuf, sizeof(TBuf));
        amxd_object_set_cstring_t(object, "HeCapsSupported", TBuf);

        swl_conv_maskToChar(TBuf, sizeof(TBuf), pR->bfCapsSupported[COM_DIR_TRANSMIT], g_str_wld_rad_bf_cap, RAD_BF_CAP_MAX);
        amxd_object_set_cstring_t(object, "TxBeamformingCapsAvailable", TBuf);

        swl_conv_maskToChar(TBuf, sizeof(TBuf), pR->bfCapsSupported[COM_DIR_RECEIVE], g_str_wld_rad_bf_cap, RAD_BF_CAP_MAX);
        amxd_object_set_cstring_t(object, "RxBeamformingCapsAvailable", TBuf);


        amxd_object_set_cstring_t(object, "OperatingStandardsFormat", swl_radStd_formatToChar(pR->operatingStandardsFormat));


        /* 'OperatingStandards' Comma-separated list of strings.
         *  Each list item MUST be a member of the list reported by
         *  the SupportedStandards parameter. List items inidcate
         *  which IEEE 802.11 standard this radio instance is
         *  configured for. */
        swl_radStd_toChar(TBuf, sizeof(TBuf), pR->operatingStandards, pR->operatingStandardsFormat, pR->supportedStandards);
        amxd_object_set_cstring_t(object, "OperatingStandards", TBuf);
        /* 'PossibleChannels' Comma-separated list (maximum length
         *  1024) of strings. List items represent possible radio
         *  channels for the wireless standard (a,b,g,n) and the
         *  regulatory domain. */
        TBuf[0] = '\0';
        for(idx = 0; !idx || pR->possibleChannels[idx]; idx++) {
            if(idx && TBuf[0]) {
                wldu_catStr(TBuf, ",", sizeof(TBuf));
            }
            itoa(pR->possibleChannels[idx], ValBuf, 10);
            wldu_catStr(TBuf, ValBuf, sizeof(TBuf));
        }
        amxd_object_set_cstring_t(object, "PossibleChannels", TBuf);
        /* This one isn't clear? */
        //set_OBJ_ParameterHelper(TPH_INT32,  amxd_object_t, "ChannelsInUse", &pR->ChannelsInUse);
        /* 'Channel' The current radio channel used by the
         *  connection. To request automatic channel selection, set
         *  AutoChannelEnable to TRUE. Whenever AutoChannelEnale is
         *  true, the value of the channel parameter MUST be the
         *  channel selected by the automatic channel selection
         *  procedure. The channel is valid when not 0. (0==auto channel) */
        if(pR->channel) {
            amxd_object_set_int32_t(object, "Channel", pR->channel);
            wld_rad_chan_update_model(pR);
            wld_rad_updateOperatingClass(pR);
        }
        wld_rad_get_update_running_bandwidth(pR);
        /* 'AutoChannelSupported' Indicates whether automatic
         *  channel selection is supported by this  radio. IF false,
         *  then AutoChannelEnable MUST be FALSE! */
        amxd_object_set_int32_t(object, "AutoChannelSupported", pR->autoChannelSupported);
        /* 'AutoChannelEnable' Enable or disable automatic channel
         *  selection. */
        // Functions that have a "write-with" in the odl file don't need this!
        // set_OBJ_ParameterHelper(TPH_INT32 , object, "AutoChannelEnable", &pR->autoChannelSetByUser);
        /* 'AutoChannelRefreshPeriod' The time period in seconds
        *  between two consecutive automatic channel selections. A
        *  value of 0 means that the automatic channel selection is
        *  done alnly at boot time. (config of the interface) */
        amxd_object_set_int32_t(object, "AutoChannelRefreshPeriod", pR->autoChannelRefreshPeriod);
        /* 'OperatingChannelBandwidth' The channel bandwidth
         *  applicable to 802.11n specifications only. */
        amxd_object_set_cstring_t(object, "OperatingChannelBandwidth", Rad_SupBW[pR->operatingChannelBandwidth]);
        amxd_object_set_cstring_t(object, "MaxChannelBandwidth", Rad_SupBW[pR->maxChannelBandwidth]);
        amxd_object_set_cstring_t(object, "AutoBandwidthSelectMode", wld_rad_autoBwSelectMode_str[pR->autoBwSelectMode]);
        /* 'Obss Coexistence'. Indicates whether obss coexistence is currently enabled. When enabled,
         *  this is allows the change of bandwidth from 40 to 20Mhz in case of obss interference.
         *  Only applicable on 2.4GHz when configured on 40MHz bandwidth. */
        amxd_object_set_bool(object, "ObssCoexistenceEnable", pR->obssCoexistenceEnabled);
        /* 'ExtensionChannel' The secondary extension channel
         *  position, applicable when operating in wide channel mode
         *  (i.e. when OperatingChannelBandwidth is set to 40MHz or
         *  Auto). */
        amxd_object_set_cstring_t(object, "ExtensionChannel", Rad_SupCExt[pR->extensionChannel]);
        /* 'GuardInterval' The guard interval value between OFDM
         *  symbols. */
        amxd_object_set_cstring_t(object, "GuardInterval", Rad_SupGI[pR->guardInterval]);
        //amxd_object_set_int32_t(object,"MCS", pR->MCS);
        /* 'MCS' The Modulation Coding Scheme index. Values from 0
         *  to 15 MUST be supported. A value 0f -1 indicates
         *  automatic selection of the MCS index. (This is CHINEES
         *  FOR ME! CHECK!!!!) */
        amxd_object_set_int32_t(object, "MCS", pR->MCS);
        /* 'TransmitPowerSupported' Comma-separated list of
         *  integers. List items represent supported transmit power
         *  levels a percentage f full power. */
        TBuf[0] = '\0';
        for(idx = 0; !idx || pR->transmitPowerSupported[idx]; idx++) {
            if(idx && TBuf[0]) {
                wldu_catStr(TBuf, ",", sizeof(TBuf));
            }
            wldu_catStr(TBuf, itoa(pR->transmitPowerSupported[idx], ValBuf, 10), sizeof(TBuf));
        }
        amxd_object_set_cstring_t(object, "TransmitPowerSupported", TBuf);
        /* 'TransmitPower' Indicates the current transmit power
         *  level as a percentage of full power. The value MUST be
         *  one of the values reporte by the TransmitPowerSupported
         *  parameter. A value of -1 indicates auto mode (automatic
         *  decision by CPE). */
        //bitchk = pR->pFA->mfn_wrad_txpow(pR,pR->transmitPower,GET);
        //set_OBJ_ParameterHelper(TPH_INT32, object, "TransmitPower", &bitchk);

        /** 'Retry Limit'
         * The maximum number of retransmissions of a short packet
         * i.e. a packet that is no longer than the RTSThreshold.
         * This corresponds to IEEE 802.11 parameter dot11ShortRetryLimit [802.11-2012]. */
        amxd_object_set_uint8_t(object, "RetryLimit", pR->retryLimit);
        /**
         * Long Retry Limit
         * This indicates the maximum number of transmission attempts of a frame,
         * the length of which is greater than RTSThreshold,
         * that will be made before a failure condition is indicated.
         * This parameter is based on dot11LongRetryLimit from [802.11-2012]. */
        amxd_object_set_uint8_t(object, "LongRetryLimit", pR->longRetryLimit);
        /* 'IEEE80211hSupported' Indicates whether IEEE 802.11h
         *  functionality is supported by this radio. The value can
         *  be true obnly if the 802.11a or 802.11n @5GHz standard is
         *  supported. (i.e. SupportedFrequencyBands includes 5GHz
         *  and SupportedStandards include a or n. */
        amxd_object_set_bool(object, "IEEE80211hSupported", pR->IEEE80211hSupported);
        /* 'IEEE80211hEnabled' Indicates whether IEEE 802.11h
         *  functionlaity is enabled on this radio. */
        amxd_object_set_bool(object, "IEEE80211hEnabled", pR->setRadio80211hEnable);

        amxd_object_set_bool(object, "IEEE80211kSupported", pR->IEEE80211kSupported);

        amxd_object_set_bool(object, "IEEE80211rSupported", pR->IEEE80211rSupported);

        char multiAPTypesSupportedStr[128] = {0};
        swl_conv_maskToChar(multiAPTypesSupportedStr, sizeof(multiAPTypesSupportedStr), pR->m_multiAPTypesSupported, cstr_MultiAPType, MULTIAP_MAX);
        amxd_object_set_cstring_t(object, "MultiAPTypesSupported", multiAPTypesSupportedStr);

        /* 'RegulatoryDomain' The 802.11d Regulatory Domain. First
         *  two octects are two-character country code. The third
         *  octet is either ' ' (all environments, 'O' outside or 'I'
         *  Inside possible parameters [A..Z][A..Z][ OI] */
        TBuf[0] = '\0';
        pR->pFA->mfn_wrad_regdomain(pR, TBuf, sizeof(TBuf), GET);
        amxd_object_set_cstring_t(object, "RegulatoryDomain", TBuf);

        amxd_object_set_bool(object, "MultiUserMIMOSupported", pR->multiUserMIMOSupported);
        amxd_object_set_bool(object, "ImplicitBeamFormingSupported", pR->implicitBeamFormingSupported);
        amxd_object_set_bool(object, "ImplicitBeamFormingEnabled", pR->implicitBeamFormingEnabled);
        amxd_object_set_bool(object, "ExplicitBeamFormingSupported", pR->explicitBeamFormingSupported);
        amxd_object_set_bool(object, "ExplicitBeamFormingEnabled", pR->explicitBeamFormingEnabled);
        amxd_object_set_cstring_t(object, "RIFSEnabled", Rad_RIFS_MODE[pR->RIFSEnabled]);
        amxd_object_set_bool(object, "AirtimeFairnessEnabled", pR->airtimeFairnessEnabled);
        amxd_object_set_uint32_t(object, "DFSChannelChangeEventCounter", pR->DFSChannelChangeEventCounter);
        swl_time_objectParamSetReal(object, "DFSChannelChangeEventTimestamp", pR->DFSChannelChangeEventTimestamp);
        amxd_object_set_cstring_t(object, "ChannelChangeReason", g_wld_channelChangeReason_str[pR->channelChangeReason]);
        amxd_object_set_cstring_t(object, "ChannelBandwidthChangeReason", g_wld_channelChangeReason_str[pR->channelBandwidthChangeReason]);

        /* 'IEEE80211_Caps' S@H, shows the supported driver capabilities.
           When a VAP is attached, this field is update so don't fake it!
         */
        TBuf[0] = '\0';
        pR->pFA->mfn_misc_has_support(pR, NULL, TBuf, sizeof(TBuf)); /* Get fake value we need a VAP */
        amxd_object_set_cstring_t(object, "IEEE80211_Caps", TBuf);

        wld_rad_parse_cap(pR);
        wld_rad_update_operating_standard(pR);
        wld_radio_updateAntenna(pR);
    } else {
        int32_t tmp_int32 = 0;
        bool tmp_bool = false;
        bool commit = false;

        tmp_bool = amxd_object_get_bool(object, "AutoChannelEnable", NULL);
        if(pR->autoChannelEnable != tmp_bool) {
            pR->autoChannelEnable = tmp_bool;
            pR->pFA->mfn_wrad_autochannelenable(pR, pR->autoChannelEnable, SET);
        }

        tmp_bool = amxd_object_get_bool(object, "TargetWakeTimeEnable", NULL);
        if(pR->twtEnable != tmp_bool) {
            pR->twtEnable = tmp_bool;
            pR->pFA->mfn_wrad_sync(pR, SET);
            commit = true;
        }

        char* operatingStandardsFormatStr = amxd_object_get_cstring_t(object, "OperatingStandardsFormat", NULL);
        pR->operatingStandardsFormat = swl_radStd_charToFormat(operatingStandardsFormatStr);
        free(operatingStandardsFormatStr);
        operatingStandardsFormatStr = NULL;

        char* newStandardStr = amxd_object_get_cstring_t(object, "OperatingStandards", NULL);
        swl_radioStandard_m newStandard = 0;
        swl_radStd_fromCharAndValidate(&newStandard, newStandardStr, pR->operatingStandardsFormat, pR->supportedStandards, "syncData_Radio2OBJ");
        if(pR->operatingStandards != newStandard) {
            pR->pFA->mfn_wrad_supstd(pR, newStandard);
            commit = true;
        }
        free(newStandardStr);
        newStandardStr = NULL;

        tmp_bool = amxd_object_get_bool(object, "OfdmaEnable", NULL);
        if(pR->ofdmaEnable != tmp_bool) {
            pR->ofdmaEnable = tmp_bool;
            pR->pFA->mfn_wrad_sync(pR, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "RxPowerSaveEnabled", NULL);
        if(pR->rxPowerSaveEnabled != tmp_bool) {
            pR->rxPowerSaveEnabled = tmp_bool;
            pR->pFA->mfn_wrad_rx_powersave(pR, pR->rxPowerSaveEnabled, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "MultiUserMIMOEnabled", NULL);
        if(pR->multiUserMIMOEnabled != tmp_bool) {
            pR->multiUserMIMOEnabled = tmp_bool;
            pR->pFA->mfn_wrad_multiusermimo(pR, pR->multiUserMIMOEnabled, SET);
            commit = true;
        }

        tmp_int32 = amxd_object_get_int32_t(object, "Channel", NULL);
        if(pR->channel != tmp_int32) {
            if(!pR->autoChannelEnable) {
                wld_rad_chan_notification(pR, tmp_int32, pR->runningChannelBandwidth);
                pR->pFA->mfn_wrad_channel(pR, tmp_int32, SET);
                wld_autoCommitMgr_notifyRadEdit(pR);
            } else {
                pR->channelChangeReason = CHAN_REASON_AUTO;
            }
            pR->channel = tmp_int32;
        }

        tmp_int32 = amxd_object_get_int32_t(object, "AutoChannelRefreshPeriod", NULL);
        if(pR->autoChannelRefreshPeriod != tmp_int32) {
            pR->autoChannelRefreshPeriod = tmp_int32;
            pR->pFA->mfn_wrad_achrefperiod(pR, pR->autoChannelRefreshPeriod, SET);
        }

        char* guardInt = amxd_object_get_cstring_t(object, "GuardInterval", NULL);
        if(strncmp(Rad_SupGI[pR->guardInterval], guardInt, strlen(Rad_SupGI[pR->guardInterval]))) {
            pR->guardInterval = conv_strToEnum(Rad_SupGI, guardInt, RGI_800NSEC, RGI_AUTO);
            pR->pFA->mfn_wrad_guardintval(pR, NULL, pR->guardInterval, SET);
            commit = true;
        }
        free(guardInt);
        guardInt = NULL;

        tmp_int32 = amxd_object_get_int32_t(object, "MCS", NULL);
        if(pR->MCS != tmp_int32) {
            pR->MCS = tmp_int32;
            pR->pFA->mfn_wrad_mcs(pR, NULL, pR->MCS, SET);
        }

        //if(get_OBJ_ParameterHelper(TPH_INT32, object, "TransmitPower", &pR->transmitPower))
        //	pR->pFA->mfn_wrad_txpow(pR,pR->transmitPower,SET);

        tmp_bool = amxd_object_get_bool(object, "IEEE80211hEnabled", NULL);
        if(pR->setRadio80211hEnable != tmp_bool) {
            pR->setRadio80211hEnable = tmp_bool;
            pR->pFA->mfn_wrad_sync(pR, SET);
            commit = true;
        }
        /* Get AP data from OBJ to Radio */
        tmp_int32 = amxd_object_get_bool(object, "Enable", NULL);
        if(pR->enable != tmp_int32) {
            pR->enable = tmp_int32;
            pR->pFA->mfn_wrad_enable(pR, pR->enable, SET);
            commit = true;
        }

        char* regulatoryDomain = amxd_object_get_cstring_t(object, "RegulatoryDomain", NULL);
        if(!swl_str_matches(regulatoryDomain, pR->regulatoryDomain)) {
            if(s_setCountryCode(pR, regulatoryDomain) == SWL_RC_OK) {
                commit = true;
            }
        }
        free(regulatoryDomain);
        regulatoryDomain = NULL;

        tmp_bool = amxd_object_get_bool(object, "STA_Mode", NULL);
        if(pR->isSTA != tmp_bool) {
            pR->isSTA = tmp_bool;
            pR->fsmRad.FSM_SyncAll = TRUE;
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "STASupported_Mode", NULL);
        if(pR->isSTASup != tmp_bool) {
            pR->isSTASup = tmp_bool;
            pR->fsmRad.FSM_SyncAll = TRUE;
            commit = true;
        }

        if(commit) {
            wld_autoCommitMgr_notifyRadEdit(pR);
        }
    }
    SAH_TRACEZ_OUT(ME);
}

/* Set our WPS data. This is usual fix at compiler time */
/** the object must point to an WIFI.wps_DefParam object !*/
void syncData_VendorWPS2OBJ(amxd_object_t* object, T_Radio* pR, int set) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "Invalid rad ctx");
    T_CONST_WPS* pCWPS = pR->wpsConst;
    ASSERT_NOT_NULL(pCWPS, , ME, "NULL");
    SAH_TRACEZ_IN(ME);

    /* WPS stuff */
    if((set & SET)) {
        ASSERT_NOT_NULL(object, , ME, "NULL");
        amxd_object_set_cstring_t(object, "DefaultPin", pCWPS->DefaultPin);
        amxd_object_set_cstring_t(object, "DevName", pCWPS->DevName);
        amxd_object_set_cstring_t(object, "OUI", pCWPS->OUI);
        amxd_object_set_cstring_t(object, "FriendlyName", pCWPS->FriendlyName);
        amxd_object_set_cstring_t(object, "Manufacturer", pCWPS->Manufacturer);
        amxd_object_set_cstring_t(object, "ManufacturerUrl", pCWPS->ManufacturerUrl);
        amxd_object_set_cstring_t(object, "ModelDescription", pCWPS->ModelDescription);
        amxd_object_set_cstring_t(object, "ModelName", pCWPS->ModelName);
        amxd_object_set_cstring_t(object, "ModelNumber", pCWPS->ModelNumber);
        amxd_object_set_cstring_t(object, "ModelUrl", pCWPS->ModelUrl);
        amxd_object_set_cstring_t(object, "OsVersion", pCWPS->OsVersion);
        amxd_object_set_cstring_t(object, "SerialNumber", pCWPS->SerialNumber);
        amxd_object_set_cstring_t(object, "UUID", pCWPS->UUID);
        amxd_object_set_int32_t(object, "wpsSupVer", pCWPS->wpsSupVer);
        amxd_object_set_int32_t(object, "wpsUUIDShared", pCWPS->wpsUUIDShared);
    } else { /* Get! */
        amxc_var_t getVar;
        amxc_var_init(&getVar);
        /*
        ** Parameters are comming from the DeviceInfo!
        ** Be sure that the plugin is running before we're passing here.
        */
        int ret = amxb_get(get_wld_plugin_bus(), "DeviceInfo.", 0, &getVar, 1);
        if(ret == AMXB_STATUS_OK) {
            SAH_TRACEZ_INFO(ME, "Getting WPS Desc from DeviceInfo");
            amxc_var_t* devInfo = amxc_var_get_first(GET_ARG(&getVar, "0"));
            swl_str_copy(pCWPS->Manufacturer, sizeof(pCWPS->Manufacturer), GET_CHAR(devInfo, "Manufacturer"));
            swl_str_copy(pCWPS->ManufacturerUrl, sizeof(pCWPS->ManufacturerUrl), GET_CHAR(devInfo, "Manufacturer"));
            swl_str_copy(pCWPS->OUI, sizeof(pCWPS->OUI), GET_CHAR(devInfo, "ManufacturerOUI"));
            swl_str_copy(pCWPS->DevName, sizeof(pCWPS->DevName), GET_CHAR(devInfo, "DeviceName"));
#if CONFIG_USE_SAH_WPS_DEVICE_NAME
            swl_str_copy(pCWPS->DevName, sizeof(pCWPS->DevName), CONFIG_SAH_WPS_DEVICE_NAME);
#endif
            swl_str_copy(pCWPS->FriendlyName, sizeof(pCWPS->FriendlyName), GET_CHAR(devInfo, "FriendlyName"));
#if CONFIG_USE_SAH_WPS_FRIENDLY_NAME
            swl_str_copy(pCWPS->FriendlyName, sizeof(pCWPS->FriendlyName), CONFIG_SAH_WPS_FRIENDLY_NAME);
#endif
            swl_str_copy(pCWPS->ModelDescription, sizeof(pCWPS->ModelDescription), GET_CHAR(devInfo, "Description"));
            swl_str_copy(pCWPS->OsVersion, sizeof(pCWPS->OsVersion), GET_CHAR(devInfo, "SoftwareVersion"));
            swl_str_copy(pCWPS->SerialNumber, sizeof(pCWPS->SerialNumber), GET_CHAR(devInfo, "SerialNumber"));
            swl_str_copy(pCWPS->ModelName, sizeof(pCWPS->ModelName), GET_CHAR(devInfo, "ModelName"));
            swl_str_copy(pCWPS->ModelNumber, sizeof(pCWPS->ModelNumber), GET_CHAR(devInfo, "ModelNumber"));
            swl_str_copy(pCWPS->ModelUrl, sizeof(pCWPS->ModelUrl), GET_CHAR(devInfo, "VendorURL"));
        } else {
            SAH_TRACEZ_INFO(ME, "Getting WPS Desc from Environment");
            // Normally we're not passing here but in case generic build... we've some SAH data ;-)
            GETENV(pCWPS->DevName, "DevName");
#if CONFIG_USE_SAH_WPS_DEVICE_NAME
            swl_str_copy(pCWPS->DevName, sizeof(pCWPS->DevName), CONFIG_SAH_WPS_DEVICE_NAME);
#endif
            GETENV(pCWPS->OUI, "MANUFACTURER_OUI");
            GETENV(pCWPS->FriendlyName, "FRIENDLY_NAME");
#if CONFIG_USE_SAH_WPS_FRIENDLY_NAME
            swl_str_copy(pCWPS->FriendlyName, sizeof(pCWPS->FriendlyName), CONFIG_SAH_WPS_FRIENDLY_NAME);
#endif
            GETENV(pCWPS->Manufacturer, "MANUFACTURER");
            GETENV(pCWPS->ManufacturerUrl, "MANUFACTURER_URL");

            snprintf(pCWPS->ModelDescription, sizeof(pCWPS->ModelDescription),
                     "%s %s", (getenv("MODELNAME")) ? : "", (getenv("MANUFACTURER")) ? : "");

            GETENV(pCWPS->ModelName, "MODELNAME");
            GETENV(pCWPS->ModelNumber, "PRODUCT_CLASS");
            GETENV(pCWPS->ModelUrl, "MANUFACTURER_URL");
            GETENV(pCWPS->OsVersion, "OSVERSION");
            GETENV(pCWPS->SerialNumber, "SERIAL_NUMBER");
        }

        // WPS UUID not filled in? Do this only for each vendor!
        if(!pCWPS->UUID[0]) {

            char uuid[50] = {0};
            swl_uuid_t uuid_i = {{0}};
            char MACaddr[18] = {0};

            GETENV(MACaddr, "WAN_ADDR");

            uint8_t offset;
#ifdef CONFIG_USE_SAH_WPS_FORCE_EQUAL_WL_UUID
            offset = 0;
#else
            offset = pR->index;
#endif
            if((MACaddr[0] != 0) &&
               (swl_uuid_fromMacAddress(&uuid_i, MACaddr, offset) && swl_uuid_toChar(uuid, sizeof(uuid), &uuid_i))
               ) {
#ifdef CONFIG_USE_SAH_WPS_FORCE_EQUAL_WL_UUID
                pCWPS->wpsUUIDShared = 1;
#else
                pCWPS->wpsUUIDShared = 0;
#endif
                swl_str_copy(pCWPS->UUID, sizeof(pCWPS->UUID), uuid);
            } else {
                swl_str_copy(pCWPS->UUID, sizeof(pCWPS->UUID), "efe83912-97f4-4dcf-8a3f-8e5b27cddb9e");
                SAH_TRACEZ_ERROR(ME, "Could not create a valid UUID (uuid:'%s;), so we set a hardcoded valid UUID!!!!", uuid);
            }
            SAH_TRACEZ_INFO(ME, "UUID=%s", pCWPS->UUID);
        }
    }
    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _FSM_Start(amxd_object_t* wifi,
                         amxd_function_t* func _UNUSED,
                         amxc_var_t* args,
                         amxc_var_t* retval) {
    amxd_object_t* objAP = NULL;
    T_Radio* pR = NULL;
    T_AccessPoint* pAP = NULL;

    int32_t ret = -1;
    char objPath[64];

    const char* vapname = GET_CHAR(args, "vap");
    SAH_TRACEZ_INFO(ME, "argument_getChar vap %s", vapname);

    int32_t val_int32 = GET_INT32(args, "bitnr");
    SAH_TRACEZ_INFO(ME, "argument_getInt32 %d", val_int32);

    /* remove our object entry out of the SSID and ACCESSPOINT table */
    sprintf(objPath, "%s.%s", "AccessPoint", vapname);
    objAP = amxd_object_get(wifi, objPath);

    if(objAP) {
        /* Get our Radio system handler */
        pAP = (T_AccessPoint*) objAP->priv;
        if(pAP) {
            pR = (T_Radio*) pAP->pRadio;

            /* Set a bit in our state machine bit-array */
            clearAllBitsLongArray(pAP->fsm.FSM_CSC, FSM_BW);            /* Clear also current state */
            setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, (int) val_int32);

            SAH_TRACEZ_INFO(ME, "setBitLongArray");

            /* Start our state machine */
            if((ret = pR->pFA->mfn_wrad_fsm_state(pR)) == FSM_IDLE) {
                ret = pR->pFA->mfn_wrad_fsm(pR);
            }

            SAH_TRACEZ_INFO(ME, "mfn_wvap_fsm");
        }
    }

    if(ret == FSM_WAIT) {
        amxc_var_set(int32_t, retval, ret);
        return amxd_status_deferred; // as there is no "executing" I set it to deffered to say that the function result is adjourned
    }

    amxc_var_set(int32_t, retval, ret);
    return (ret < 0) ? amxd_status_unknown_error : amxd_status_ok;
}

/**************************************************************************************************/
/* Only a button function for this VAP! SelfPIN is radio bounded Client and PBC not! */
amxd_status_t _wps_DefParam_wps_GenSelfPIN(amxd_object_t* obj _UNUSED,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args _UNUSED,
                                           amxc_var_t* retval) {

    genSelfPIN();

    amxc_var_set(uint32_t, retval, 0);
    return amxd_status_ok;
}

/* Quick lookup of parameters we accept! */
const char* Radio_ObjParam[] = {
    "Alias",
    "OperatingFrequencyBand",
    "OperatingStandards",
    "Channel",
    "AutoChannelEnable",
    "AutoChannelRefreshPeriod",
    "OperatingChannelBandwidth",
    "ExtensionChannel",
    "GuardInterval",
    "MCS",
    "TransmitPower",
    "IEEE80211hEnabled",
    "RegulatoryDomain",
    "Enable",
    NULL
};

/**
 * disable an commit on an parameter enable toggle. (used for group config)
 */
amxd_status_t _edit(amxd_object_t* object,
                    amxd_function_t* func _UNUSED,
                    amxc_var_t* args _UNUSED,
                    amxc_var_t* retval) {
    T_Radio* pR = object->priv;
    bool success = false;

    /* Check our input data */
    if(pR && debugIsRadPointer(pR)) {
        pR->blockCommit = 1;
        pR->blockStart = wld_util_time_monotonic_sec();
        success = true;
    }

    amxc_var_set(bool, retval, success);
    return amxd_status_ok;
}

typedef struct {
    int objtype;
    char* ObjStr;
    int stucttype;
    int OffSetOf;
} T_DEBUG_OBJ_STR;

T_DEBUG_OBJ_STR VerifyRadio[] = {
    {TPH_BOOL, "Enable", TPH_INT32, offsetof(T_Radio, enable)},
    {TPH_STR, "Status", TPH_INT32, offsetof(T_Radio, status)},
    {TPH_INT32, "Index", TPH_INT32, offsetof(T_Radio, index)},
    {TPH_BOOL, "AP_Mode", TPH_BOOL, offsetof(T_Radio, isAP)},
    {TPH_BOOL, "STA_Mode", TPH_BOOL, offsetof(T_Radio, isSTA)},
    {TPH_BOOL, "WDS_Mode", TPH_BOOL, offsetof(T_Radio, isWDS)},
    {TPH_BOOL, "WET_Mode", TPH_BOOL, offsetof(T_Radio, isWET)},
    {TPH_BOOL, "STASupported_Mode", TPH_BOOL, offsetof(T_Radio, isSTASup)},
    {TPH_BOOL, "WPS_Enrollee_Mode", TPH_BOOL, offsetof(T_Radio, isWPSEnrol)},
    {TPH_INT32, "MaxBitRate", TPH_INT32, offsetof(T_Radio, maxBitRate)},
    {TPH_STR, "SupportedFrequencyBands", TPH_INT32, offsetof(T_Radio, supportedFrequencyBands)},
    {TPH_STR, "OperatingFrequencyBand", TPH_INT32, offsetof(T_Radio, operatingFrequencyBand)},
    {TPH_STR, "SupportedStandards", TPH_INT32, offsetof(T_Radio, supportedStandards)},
    {TPH_STR, "OperatingStandards", TPH_INT32, offsetof(T_Radio, operatingStandards)},
    {TPH_STR, "OperatingStandardsFormat", TPH_INT32, offsetof(T_Radio, operatingStandardsFormat)},
    {TPH_UINT32, "OperatingClass", TPH_UINT32, offsetof(T_Radio, operatingClass)},
    {TPH_STR, "ChannelsInUse", TPH_STR, offsetof(T_Radio, channelsInUse)},
    {TPH_BOOL, "AutoChannelSupported", TPH_INT32, offsetof(T_Radio, autoChannelSupported)},
    {TPH_BOOL, "AutoChannelEnable", TPH_BOOL, offsetof(T_Radio, autoChannelEnable)},
    {TPH_INT32, "AutoChannelRefreshPeriod", TPH_INT32, offsetof(T_Radio, autoChannelRefreshPeriod)},
    {TPH_INT32, "Channel", TPH_INT32, offsetof(T_Radio, channel)},
    {TPH_STR, "OperatingChannelBandwidth", TPH_INT32, offsetof(T_Radio, operatingChannelBandwidth)},
    {TPH_STR, "CurrentOperatingChannelBandwidth", TPH_INT32, offsetof(T_Radio, runningChannelBandwidth)},
    {TPH_STR, "MaxChannelBandwidth", TPH_INT32, offsetof(T_Radio, maxChannelBandwidth)},
    {TPH_STR, "ExtensionChannel", TPH_INT32, offsetof(T_Radio, extensionChannel)},
    {TPH_BOOL, "ObssCoexistenceEnable", TPH_INT32, offsetof(T_Radio, obssCoexistenceEnabled)},
    {TPH_STR, "GuardInterval", TPH_INT32, offsetof(T_Radio, guardInterval)},
    {TPH_INT32, "MCS", TPH_INT32, offsetof(T_Radio, MCS)},
    {TPH_INT32, "TransmitPower", TPH_INT32, offsetof(T_Radio, transmitPower)},
    {TPH_INT32, "ActiveAntennaCtrl", TPH_INT32, offsetof(T_Radio, actAntennaCtrl)},
    {TPH_UINT32, "RetryLimit", TPH_UINT8, offsetof(T_Radio, retryLimit)},
    {TPH_UINT32, "LongRetryLimit", TPH_UINT8, offsetof(T_Radio, longRetryLimit)},
    {TPH_UINT32, "BeaconPeriod", TPH_UINT32, offsetof(T_Radio, beaconPeriod)},
    {TPH_UINT32, "DTIMPeriod", TPH_UINT32, offsetof(T_Radio, dtimPeriod)},
    {TPH_BOOL, "TargetWakeTimeEnable", TPH_BOOL, offsetof(T_Radio, twtEnable)},
    {TPH_BOOL, "OfdmaEnable", TPH_BOOL, offsetof(T_Radio, ofdmaEnable)},
    {TPH_STR, "HeCapsSupported", TPH_INT32, offsetof(T_Radio, heCapsSupported)},
    {TPH_STR, "HeCapsEnabled", TPH_INT32, offsetof(T_Radio, heCapsEnabled)},
    {TPH_BOOL, "IEEE80211hSupported", TPH_INT32, offsetof(T_Radio, IEEE80211hSupported)},
    {TPH_BOOL, "IEEE80211hEnabled", TPH_INT32, offsetof(T_Radio, setRadio80211hEnable)},
    {TPH_BOOL, "IEEE80211kSupported", TPH_BOOL, offsetof(T_Radio, IEEE80211kSupported)},
    {TPH_STR, "RegulatoryDomain", TPH_STR, offsetof(T_Radio, regulatoryDomain)},
    {TPH_BOOL, "ImplicitBeamFormingSupported", TPH_INT32, offsetof(T_Radio, implicitBeamFormingSupported)},
    {TPH_BOOL, "ImplicitBeamFormingEnabled", TPH_INT32, offsetof(T_Radio, implicitBeamFormingEnabled)},
    {TPH_BOOL, "ExplicitBeamFormingSupported", TPH_INT32, offsetof(T_Radio, explicitBeamFormingSupported)},
    {TPH_BOOL, "ExplicitBeamFormingEnabled", TPH_INT32, offsetof(T_Radio, explicitBeamFormingEnabled)},
    {TPH_STR, "RIFSEnabled", TPH_INT32, offsetof(T_Radio, RIFSEnabled)},
    {TPH_BOOL, "AirtimeFairnessEnabled", TPH_BOOL, offsetof(T_Radio, airtimeFairnessEnabled)},
    {TPH_BOOL, "RxPowerSaveEnabled", TPH_BOOL, offsetof(T_Radio, rxPowerSaveEnabled)},
    {TPH_BOOL, "MultiUserMIMOSupported", TPH_BOOL, offsetof(T_Radio, multiUserMIMOSupported)},
    {TPH_BOOL, "MultiUserMIMOEnabled", TPH_BOOL, offsetof(T_Radio, multiUserMIMOEnabled)},
    {TPH_INT32, "DFSChannelChangeEventCounter", TPH_UINT32, offsetof(T_Radio, DFSChannelChangeEventCounter)},
    {TPH_STR, "ChannelChangeReason", TPH_STR, offsetof(T_Radio, channelChangeReason)},
    {TPH_STR, "ChannelBandwidthChangeReason", TPH_STR, offsetof(T_Radio, channelBandwidthChangeReason)},
    {TPH_INT32, "ActiveAssociatedDevices", TPH_INT32, offsetof(T_Radio, currentStations)},
    {TPH_INT32, "MaxAssociatedDevices", TPH_INT32, offsetof(T_Radio, maxStations)},
    {TPH_BOOL, "KickRoamingStation", TPH_BOOL, offsetof(T_Radio, kickRoamStaEnabled)},
    {TPH_INT32, "dbgRADEnable", TPH_INT32, offsetof(T_Radio, dbgEnable)},
    {TPH_STR, "dbgRADFile", TPH_PSTR, offsetof(T_Radio, dbgOutput)},
    {0, NULL, 0, 0},
};

T_DEBUG_OBJ_STR VerifyAccessPoint[] = {
    {TPH_STR, "Alias", TPH_STR, offsetof(T_AccessPoint, alias)},
    {TPH_STR, "BridgeInterface", TPH_STR, offsetof(T_AccessPoint, bridgeName)},
    {TPH_STR, "SSIDReference", TPH_INT32, offsetof(T_AccessPoint, SSIDReference)},
    {TPH_BOOL, "SSIDAdvertisementEnabled", TPH_INT32, offsetof(T_AccessPoint, SSIDAdvertisementEnabled)},
    {TPH_INT32, "RetryLimit", TPH_INT32, offsetof(T_AccessPoint, retryLimit)},
    {TPH_BOOL, "WMMEnable", TPH_INT32, offsetof(T_AccessPoint, WMMEnable)},
    {TPH_BOOL, "UAPSDEnable", TPH_INT32, offsetof(T_AccessPoint, UAPSDEnable)},
    {TPH_BOOL, "IEEE80211kEnabled", TPH_BOOL, offsetof(T_AccessPoint, IEEE80211kEnable)},
    {TPH_STR, "Security.WEPKey", TPH_STR, offsetof(T_AccessPoint, WEPKey)},
    {TPH_STR, "Security.PreSharedKey", TPH_STR, offsetof(T_AccessPoint, preSharedKey)},
    {TPH_STR, "Security.KeyPassPhrase", TPH_STR, offsetof(T_AccessPoint, keyPassPhrase)},
    {TPH_STR, "Security.SAEPassphrase", TPH_STR, offsetof(T_AccessPoint, saePassphrase)},
    {TPH_INT32, "Security.RekeyingInterval", TPH_INT32, offsetof(T_AccessPoint, rekeyingInterval)},
    {TPH_STR, "Security.RadiusServerIPAddr", TPH_STR, offsetof(T_AccessPoint, radiusServerIPAddr)},
    {TPH_INT32, "Security.RadiusServerPort", TPH_INT32, offsetof(T_AccessPoint, radiusServerPort)},
    {TPH_STR, "Security.RadiusSecret", TPH_STR, offsetof(T_AccessPoint, radiusSecret)},
    {TPH_INT32, "Security.RadiusDefaultSessionTimeout", TPH_INT32, offsetof(T_AccessPoint, radiusDefaultSessionTimeout)},
    {TPH_STR, "Security.RadiusOwnIPAddress", TPH_STR, offsetof(T_AccessPoint, radiusOwnIPAddress)},
    {TPH_STR, "Security.RadiusNASIdentifier", TPH_STR, offsetof(T_AccessPoint, radiusNASIdentifier)},
    {TPH_STR, "Security.RadiusCalledStationId", TPH_STR, offsetof(T_AccessPoint, radiusCalledStationId)},
    {TPH_INT32, "Security.RadiusChargeableUserId", TPH_INT32, offsetof(T_AccessPoint, radiusChargeableUserId)},
    {TPH_STR, "Security.ModeEnabled", TPH_INT32, offsetof(T_AccessPoint, secModeEnabled)},
    {TPH_STR, "Security.MFPConfig", TPH_INT32, offsetof(T_AccessPoint, mfpConfig)},
    {TPH_INT32, "Security.SPPAmsdu", TPH_INT32, offsetof(T_AccessPoint, sppAmsdu)},
    {TPH_BOOL, "Security.SHA256Enable", TPH_INT32, offsetof(T_AccessPoint, SHA256Enable)},
    {TPH_BOOL, "Security.OWETransitionInterface", TPH_INT32, offsetof(T_AccessPoint, oweTransModeIntf)},
    {TPH_BOOL, "WPS.Enable", TPH_INT32, offsetof(T_AccessPoint, WPS_Enable)},
    {TPH_STR, "WPS.ConfigMethodsEnabled", TPH_INT32, offsetof(T_AccessPoint, WPS_ConfigMethodsEnabled)},
    {TPH_BOOL, "WPS.Configured", TPH_INT32, offsetof(T_AccessPoint, WPS_Configured)},
    {TPH_BOOL, "WPS.PairingInProgress", TPH_BOOL, offsetof(T_AccessPoint, WPS_PairingInProgress)},
    {TPH_BOOL, "Enable", TPH_INT32, offsetof(T_AccessPoint, enable)},
    {TPH_BOOL, "Status", TPH_INT32, offsetof(T_AccessPoint, status)},
    {TPH_INT32, "dbgAPEnable", TPH_INT32, offsetof(T_AccessPoint, dbgEnable)},
    {TPH_BOOL, "WDSEnable", TPH_BOOL, offsetof(T_AccessPoint, wdsEnable)},
    {TPH_STR, "dbgAPFile", TPH_PSTR, offsetof(T_AccessPoint, dbgOutput)},
    {TPH_INT32, "MaxAssociatedDevices", TPH_INT32, offsetof(T_AccessPoint, MaxStations)},
    {TPH_INT32, "ActiveAssociatedDeviceNumberOfEntries", TPH_INT32, offsetof(T_AccessPoint, ActiveAssociatedDeviceNumberOfEntries)},
    {TPH_SEL_SSID, "SWAP POINTER TO SSID", TPH_SEL_SSID, 0},
    {TPH_STR, "BSSID", TPH_BSTR, offsetof(T_SSID, BSSID)},
    {TPH_STR, "SSID", TPH_STR, offsetof(T_SSID, SSID)},
    {TPH_BOOL, "Enable", TPH_BOOL, offsetof(T_SSID, enable)},
    {TPH_STR, "Status", TPH_INT32, offsetof(T_SSID, status)},
    {0, NULL, 0, 0},
};


/* Get the data from object and structure... (without changing any value)*/
int DebugObjStructPrint(void* pData) {
    amxd_object_t* selObj = NULL;
    T_Radio* pR = NULL;
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;
    char* pHR = NULL;
    char* pTmp = NULL;
    int i, val_int;
    bool valBool;
    uint32_t val_uint = 0;
    char fullbuffer[128];
    char objbuf[256];
    char structbuf[256];

    pR = (T_Radio*) pData;

    /* Debug Radio part...*/
    for(i = 0; VerifyRadio[i].ObjStr; i++) {
        pHR = (char*) pData;    // Need it for offsetoff macro
        switch(VerifyRadio[i].stucttype) {
        case TPH_STR:
            sprintf(structbuf, "T_Radio = %32s", (char*) &pHR[VerifyRadio[i].OffSetOf]);
            break;
        case TPH_PSTR:
            sprintf(structbuf, "T_Radio = %32s", *((char**) &pHR[VerifyRadio[i].OffSetOf]));
            break;
        case TPH_INT32:
            val_int = *((long*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %i", val_int);
            break;
        case TPH_INT16:
            val_int = *((int16_t*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %i", val_int);
            break;
        case TPH_INT8:
            val_int = *((int8_t*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %i", val_int);
            break;
        case TPH_UINT32:
            val_uint = *((long*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %u", val_uint);
            break;
        case TPH_UINT16:
            val_uint = *((uint16_t*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %u", val_uint);
            break;
        case TPH_UINT8:
            val_uint = *((uint8_t*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %u", val_uint);
            break;
        case TPH_BOOL:
            val_int = *((bool*) &pHR[VerifyRadio[i].OffSetOf]);
            sprintf(structbuf, "T_Radio = %u", val_int);
            break;
        default:
            sprintf(structbuf, "T_Radio = ERROR %u", VerifyRadio[i].stucttype);
            break;
        }

        switch(VerifyRadio[i].objtype) {
        case TPH_BOOL:
            valBool = amxd_object_get_bool(pR->pBus, VerifyRadio[i].ObjStr, NULL);
            sprintf(objbuf, "Object %s = %u", VerifyRadio[i].ObjStr, valBool);
            break;
        case TPH_INT32:
            val_int = amxd_object_get_int32_t(pR->pBus, VerifyRadio[i].ObjStr, NULL);
            sprintf(objbuf, "Object %s = %i", VerifyRadio[i].ObjStr, val_int);
            break;
        case TPH_UINT32:
            val_uint = amxd_object_get_uint32_t(pR->pBus, VerifyRadio[i].ObjStr, NULL);
            sprintf(objbuf, "Object %s = %u", VerifyRadio[i].ObjStr, val_uint);
            break;
        case TPH_STR:
            wldu_copyStr(fullbuffer,
                         amxd_object_get_cstring_t(pR->pBus, VerifyRadio[i].ObjStr, NULL),
                         sizeof(fullbuffer));
            sprintf(objbuf, "Object %s = %s", VerifyRadio[i].ObjStr, fullbuffer);
            break;
        default:
            sprintf(objbuf, "Object %s = ERROR %u", VerifyRadio[i].ObjStr, VerifyRadio[i].objtype);
            break;
        }
        SAH_TRACEZ_ERROR(ME, "%s:%s ; %s", pR->Name, objbuf, structbuf);
    }
    /* FSM Radio data */
    SAH_TRACEZ_ERROR(ME, "Radio FSM_BitActionArray     %s - %d - %08x | %08x",
                     pR->Name,
                     pR->fsm_radio_st,
                     (unsigned int) pR->fsmRad.FSM_BitActionArray[0],
                     (unsigned int) pR->fsmRad.FSM_BitActionArray[1]);
    SAH_TRACEZ_ERROR(ME, "Radio FSM_AC_BitActionArray  %s - %d - %08x | %08x",
                     pR->Name,
                     pR->fsm_radio_st,
                     (unsigned int) pR->fsmRad.FSM_AC_BitActionArray[0],
                     (unsigned int) pR->fsmRad.FSM_AC_BitActionArray[1]);
    SAH_TRACEZ_ERROR(ME, "FSM_State/Loop/ComPend/TODC/timer %s - %08x | %08x | %08x | %08x | %08x | %u",
                     pR->Name,
                     pR->fsmRad.FSM_State,
                     pR->fsmRad.FSM_Loop,
                     pR->fsmRad.FSM_ComPend,
                     pR->fsmRad.TODC,
                     pR->fsmRad.timeout_msec,
                     pR->fsmRad.FSM_SyncAll);
    SAH_TRACEZ_ERROR(ME, "FSM_Delay/Retry/SrcVAP/DC/timer %s - %08x | %08x | %08x | %p | %p",
                     pR->Name,
                     pR->fsmRad.FSM_Delay,
                     pR->fsmRad.FSM_Retry,
                     pR->fsmRad.FSM_SrcVAP,
                     pR->fsmRad.obj_DelayCommit,
                     pR->fsmRad.timer);
    SAH_TRACEZ_ERROR(ME, "BloCom %u State %u",
                     pR->blockCommit, pR->pFA->mfn_wrad_fsm_state(pR));

    /* Do this also for the VAP's */
    for(it = (amxc_llist_it_t*) amxc_llist_get_first(&pR->llAP); it; it = (amxc_llist_it_t*) amxc_llist_it_get_next(it)) {
        pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
        pHR = (char*) pAP;
        selObj = pAP->pBus;

        /* FSM VAP data */
        SAH_TRACEZ_ERROR(ME, "VAP FSM_BitActionArray     %s - %08x | %08x",
                         pAP->alias,
                         (unsigned int) pAP->fsm.FSM_BitActionArray[0],
                         (unsigned int) pAP->fsm.FSM_BitActionArray[1]);
        SAH_TRACEZ_ERROR(ME, "VAP FSM_AC_BitActionArray  %s - %08x | %08x",
                         pAP->alias,
                         (unsigned int) pAP->fsm.FSM_AC_BitActionArray[0],
                         (unsigned int) pAP->fsm.FSM_AC_BitActionArray[1]);
        SAH_TRACEZ_ERROR(ME, "FSM_State/Loop/ComPend/timer %s - %08x | %08x | %p",
                         pAP->alias,
                         pAP->fsm.FSM_State,
                         pAP->fsm.FSM_Loop,
                         pAP->fsm.timer);

        for(i = 0; VerifyAccessPoint[i].ObjStr; i++) {
            switch(VerifyAccessPoint[i].stucttype) {
            case TPH_STR:
                sprintf(structbuf, "T_AP = %32s", (char*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                break;
            case TPH_BSTR:
                pTmp = (char*) &pHR[VerifyAccessPoint[i].OffSetOf];
                sprintf(structbuf, "T_AP = %02x%02x%02x%02x...", pTmp[0], pTmp[1], pTmp[2], pTmp[3]);
                break;
            case TPH_PSTR:
                sprintf(structbuf, "T_Radio = %32s", *((char**) &pHR[VerifyAccessPoint[i].OffSetOf]));
                break;
            case TPH_INT32:
                val_int = *((int32_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_AP = %i", val_int);
                break;
            case TPH_INT16:
                val_int = *((int16_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_Radio = %i", val_int);
                break;
            case TPH_INT8:
                val_int = *((int8_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_Radio = %i", val_int);
                break;
            case TPH_UINT32:
                val_uint = *((uint32_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_AP = %u", val_uint);
                break;
            case TPH_UINT16:
                val_uint = *((int16_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_Radio = %u", val_uint);
                break;
            case TPH_UINT8:
                val_uint = *((int8_t*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_Radio = %u", val_uint);
                break;
            case TPH_BOOL:
                val_int = *((bool*) &pHR[VerifyAccessPoint[i].OffSetOf]);
                sprintf(structbuf, "T_AP = %u", val_int);
                break;
            case TPH_SEL_SSID:
                pHR = (char*) pAP->pSSID;
                selObj = ((T_SSID*) (pAP->pSSID))->pBus;
                continue;   // Go to next item...
                break;
            default:
                sprintf(structbuf, "T_AP = ERROR %u", VerifyAccessPoint[i].stucttype);
                break;

            }

            switch(VerifyAccessPoint[i].objtype) {
            case TPH_BOOL:
                valBool = amxd_object_get_bool(selObj, VerifyAccessPoint[i].ObjStr, NULL);
                sprintf(objbuf, "Object %s = %u", VerifyAccessPoint[i].ObjStr, valBool);
                break;
            case TPH_INT32:
                val_int = amxd_object_get_int32_t(selObj, VerifyAccessPoint[i].ObjStr, NULL);
                sprintf(objbuf, "Object %s = %i", VerifyAccessPoint[i].ObjStr, val_int);
                break;
            case TPH_UINT32:
                val_int = amxd_object_get_int32_t(selObj, VerifyAccessPoint[i].ObjStr, NULL);
                sprintf(objbuf, "Object %s = %u", VerifyAccessPoint[i].ObjStr, val_int);
                break;
            case TPH_STR:
                wldu_copyStr(fullbuffer,
                             amxd_object_get_cstring_t(selObj, VerifyAccessPoint[i].ObjStr, NULL),
                             sizeof(fullbuffer));
                sprintf(objbuf, "Object %s = %s", VerifyAccessPoint[i].ObjStr, fullbuffer);
                break;
            default:
                sprintf(objbuf, "Object = ERROR %u", VerifyAccessPoint[i].objtype);
                break;
            }
            SAH_TRACEZ_ERROR(ME, "%s:%s ; %s", pAP->alias, objbuf, structbuf);
        }

    }

    return 0;
}

amxd_status_t _verify(amxd_object_t* object _UNUSED,
                      amxd_function_t* func _UNUSED,
                      amxc_var_t* args _UNUSED,
                      amxc_var_t* ret _UNUSED) {
    return amxd_status_ok;
}

void Radio_Commit_Abort(uint64_t call_id _UNUSED, void* userdata) {
    T_Radio* pR = (T_Radio*) userdata;
    pR->call_id = 0;
}

static void doRadioReset(T_Radio* pRad) {
    /* Looks we're hanging? free all timer/callbacks ! */
    SAH_TRACEZ_ERROR(ME, "Radio reset %s ", pRad->Name);
    int fsmState = pRad->pFA->mfn_wrad_fsm_state(pRad);
    SAH_TRACEZ_ERROR(ME, "BloCom %u State %u", pRad->blockCommit, fsmState);
    amxc_llist_it_t* it;
    T_AccessPoint* pAP;
    /* collect all dependencies on all attached ep interfaces, mirror it on the RAD interface */
    amxc_llist_for_each(it, &pRad->llEndPoints) {
        T_EndPoint* pEP = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);
        SAH_TRACEZ_ERROR(ME, "Reset ap %s: 0x%lx 0x%lx // 0x%lx 0x%lx ",
                         pEP->alias,
                         pEP->fsm.FSM_BitActionArray[0], pEP->fsm.FSM_BitActionArray[1],
                         pEP->fsm.FSM_AC_BitActionArray[0], pEP->fsm.FSM_AC_BitActionArray[1]);
    }

    /* collect all dependencies on all attached vap interfaces, mirror it on the RAD interface */
    for(it = (amxc_llist_it_t*) amxc_llist_get_first(&pRad->llAP); it; it = (amxc_llist_it_t*) amxc_llist_it_get_next(it)) {
        pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(pAP && debugIsVapPointer(pAP)) {
            SAH_TRACEZ_ERROR(ME, "Reset ap %s: 0x%lx 0x%lx // 0x%lx 0x%lx",
                             pAP->alias,
                             pAP->fsm.FSM_BitActionArray[0], pAP->fsm.FSM_BitActionArray[1],
                             pAP->fsm.FSM_AC_BitActionArray[0], pAP->fsm.FSM_AC_BitActionArray[1]);
        }
    }

    if(pRad->call_id != 0) {
        fsm_delay_reply(pRad->call_id, amxd_status_unknown_error, NULL);
        pRad->call_id = 0;
    }
    if(pRad->fsmRad.timer) {
        amxp_timer_delete(&pRad->fsmRad.timer);
    }
    pRad->pFA->mfn_wrad_fsm_reset(pRad);
    pRad->fsmRad.timer = 0;
    /* Reset full FSM */
    pRad->fsm_radio_st = FSM_IDLE;
    pRad->fsmRad.FSM_Loop = 0;
    pRad->fsmRad.FSM_Delay = 0;
    pRad->fsmRad.FSM_State = 0;
    pRad->fsmRad.FSM_ComPend = 0;
    // Force full sync
    pRad->fsmRad.FSM_SyncAll = 1;
    // Start the fsm again, so we can start working again
    pRad->pFA->mfn_wrad_fsm(pRad);

    //increment counter to show we reset
    wld_rad_incrementCounterStr(pRad, &pRad->genericCounters, WLD_RAD_EV_FSM_RESET, "0x%x", fsmState);
}


static void doAllRadioReset() {
    amxc_llist_it_t* it;
    T_Radio* pRad = NULL;
    amxc_llist_for_each(it, &g_radios) {
        pRad = amxc_llist_it_get_data(it, T_Radio, it);
        if(pRad->fsm_radio_st != FSM_IDLE) {
            doRadioReset(pRad);
        }
    }
}

/**
 * The function becomes active after a commit and will be called
 * for updating some object values that could changed in time! The
 * choice of parameters that must be updated is fully controlled
 * by the vendor plugin.
 */
static void rad_delayed_commit_time_handler(amxp_timer_t* timer, void* userdata) {
    T_Radio* pR = (T_Radio*) userdata;
    (void) timer;

    SAH_TRACEZ_IN(ME);
    // By every cycle inc the delay till 2 minutes delay... we keep running.
    pR->fsmRad.TODC += (pR->fsmRad.TODC >= 120000) ? 0 : +10000;

    // Call the function...
    if(pR->pFA->mfn_wrad_fsm_delay_commit) {
        pR->pFA->mfn_wrad_fsm_delay_commit(pR);
        if(pR->fsmRad.TODC) {
            amxp_timer_start(pR->fsmRad.obj_DelayCommit, pR->fsmRad.TODC);
        }
    } else {
        pR->fsmRad.TODC = 0; // No use for updates
    }
    // Clearup timer when done!
    if(!pR->fsmRad.TODC) {
        amxp_timer_delete(&pR->fsmRad.obj_DelayCommit);
        pR->fsmRad.obj_DelayCommit = NULL;
    }
    SAH_TRACEZ_OUT(ME);
}

void wld_rad_doSync(T_Radio* pRad) {
    pRad->pFA->mfn_wrad_sync(pRad, SET);
    wld_autoCommitMgr_notifyRadEdit(pRad);
}

int wld_rad_doRadioCommit(T_Radio* pRad) {
    int ret = -1;
    if(!pRad || !debugIsRadPointer(pRad)) {
        SAH_TRACEZ_ERROR(ME, "Invalid commit");
        return -1;
    }

    /* Start timer for late commit update... */
    if(pRad->pFA->mfn_wrad_fsm_delay_commit) {
        pRad->fsmRad.TODC = 10000; /* 10 seconds */
        if(!pRad->fsmRad.obj_DelayCommit) {
            amxp_timer_new(&pRad->fsmRad.obj_DelayCommit, rad_delayed_commit_time_handler, pRad);
            if(pRad->fsmRad.obj_DelayCommit != NULL) {
                SAH_TRACEZ_INFO(ME, "Create delayCommit timer %s", pRad->Name);
            } else {
                SAH_TRACEZ_ERROR(ME, "Failed to create delay commit timer %s", pRad->Name);
            }
        }
        SAH_TRACEZ_INFO(ME, "Start delayCommit callback %s - %d", pRad->Name, pRad->fsmRad.TODC);
        amxp_timer_start(pRad->fsmRad.obj_DelayCommit, pRad->fsmRad.TODC);
    }

    if((pRad->fsm_radio_st == FSM_IDLE) && !pRad->fsmRad.timer) {
        ret = pRad->pFA->mfn_wrad_fsm(pRad);
    } else {
        /* Mark as Commit Pending */
        time_t now = wld_util_time_monotonic_sec();

        if(pRad->fsmRad.FSM_ComPend == 0) {
            pRad->fsmRad.FSM_ComPend_Start = now;
        }

        time_t diff = now - pRad->fsmRad.FSM_ComPend_Start;

        pRad->fsmRad.FSM_ComPend++;
        SAH_TRACEZ_INFO(ME, "%s commit pending (%d)!", pRad->Name, pRad->fsmRad.FSM_ComPend);
        // We have 50 pending commits, and waited for longer as 30 seconds.
        if((pRad->fsmRad.FSM_ComPend > 50) && (diff > 30)) {
            SAH_TRACEZ_WARNING(ME, "%s FSM RESET!", pRad->Name);
            doAllRadioReset();
        }
        ret = 0;
    }

    if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed commit on %s = %d", pRad->Name, ret);
    } else {
        SAH_TRACEZ_INFO(ME, "Success commit on %s = %d", pRad->Name, ret);
    }

    return ret;
}

#define BLOCK_COMMIT_TIMEOUT 60

int wld_rad_doCommitIfUnblocked(T_Radio* pRad) {
    int ret = 0;
    if(!pRad->blockCommit) {
        ret = wld_rad_doRadioCommit(pRad);
    } else {
        time_t now = wld_util_time_monotonic_sec();
        uint32_t diff = (now - pRad->blockStart);
        if(diff > BLOCK_COMMIT_TIMEOUT) {
            SAH_TRACEZ_ERROR(ME, "Ignore commit block @ %s because time expired %u", pRad->Name, diff);
            ret = wld_rad_doRadioCommit(pRad);
        } else if(now < pRad->blockStart) {
            SAH_TRACEZ_ERROR(ME, "Block start in future %s, ignore", pRad->Name);
            ret = wld_rad_doRadioCommit(pRad);
        } else {
            SAH_TRACEZ_NOTICE(ME, "Skipping commit due to block %s, blocked for %u", pRad->Name, diff);
        }
    }
    return ret;
}

amxd_status_t _commit(amxd_object_t* object,
                      amxd_function_t* func _UNUSED,
                      amxc_var_t* args _UNUSED,
                      amxc_var_t* retval) {

    T_Radio* pR = NULL;
    int ret = -1;

    pR = object->priv;

    ret = wld_rad_doRadioCommit(pR);

    if(ret == FSM_WAIT) {
        amxc_var_set(int32_t, retval, ret);
        return amxd_status_deferred;
    }

    amxc_var_set(int32_t, retval, ret);
    return amxd_status_ok;
}



amxd_status_t _startAutoChannelSelection(amxd_object_t* object,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args _UNUSED,
                                         amxc_var_t* ret _UNUSED) {

    T_Radio* pR = object->priv;
    if(!pR || !debugIsRadPointer(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s is not valid Radio", amxd_object_get_name(object, AMXD_OBJECT_NAMED));
        return amxd_status_unknown_error;
    }
    if(!amxd_object_get_bool(object, "Enable", NULL)) {
        SAH_TRACEZ_ERROR(ME, "%s: Radio is disabled", pR->Name);
        return amxd_status_unknown_error;
    }
    if(!amxd_object_get_bool(object, "AutoChannelSupported", NULL)) {
        SAH_TRACEZ_ERROR(ME, "%s: AutoChannel is not supported", pR->Name);
        return amxd_status_unknown_error;
    }
    if(!amxd_object_get_bool(object, "AutoChannelEnable", NULL)) {
        SAH_TRACEZ_ERROR(ME, "%s: AutoChannel is not enabled", pR->Name);
        return amxd_status_unknown_error;
    }
    if(amxd_object_get_bool(object, "AutoChannelSelecting", NULL)) {
        SAH_TRACEZ_ERROR(ME, "%s: AutoChannel selection is already running", pR->Name);
        return amxd_status_unknown_error;
    }
    // Try first 'direct' path to start AutoChannel.
    if(pR->pFA->mfn_wrad_startacs(pR, SET | MVRADIO) < 0) {
        // Use emulation approach by kicking the FSM again.
        pR->pFA->mfn_wrad_autochannelenable(pR, true, SET);
        wld_rad_doRadioCommit(pR);
    }
    return amxd_status_ok;
}

amxd_status_t _startACS(amxd_object_t* obj,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args,
                        amxc_var_t* ret _UNUSED) {
    return _startAutoChannelSelection(obj, func, args, ret);
}

amxd_status_t _checkWPSPIN(amxd_object_t* obj _UNUSED,
                           amxd_function_t* func _UNUSED,
                           amxc_var_t* args,
                           amxc_var_t* retval) {
    unsigned long PIN = 0;

    int32_t ret = -1;

    /*     bool checkWPSPIN(string PIN); */
    const char* pin = GET_CHAR(args, "PIN");
    char pinModified[16] = {'\0'};
    snprintf(pinModified, sizeof(pinModified), "%s", pin);

    SAH_TRACEZ_INFO(ME, "pin:%s", pinModified);
    stripOutToken(pinModified, "-");
    stripOutToken(pinModified, " ");
    if(sscanf(pinModified, "%lu", &PIN) == 1) {
        SAH_TRACEZ_INFO(ME, "PIN: %s %lu", pinModified, PIN);
        ret = wldu_checkWpsPinStr(pinModified);
    } else {
        SAH_TRACEZ_ERROR(ME, "PIN: '%s' sscanf did not return 1", pinModified);
    }

    amxc_var_set(int32_t, retval, ret);

    return (ret < 0) ? amxd_status_unknown_error : amxd_status_ok;
}

/* Find first vap from radio */
T_AccessPoint* wld_rad_getFirstVap(T_Radio* pR) {
    ASSERT_NOT_NULL(pR, NULL, ME, "NULL");
    amxc_llist_it_t* it = (amxc_llist_it_t*) amxc_llist_get_first(&pR->llAP);
    ASSERTI_NOT_NULL(it, NULL, ME, "Empty");
    return (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
}

T_EndPoint* wld_rad_getFirstEp(T_Radio* pR) {
    ASSERT_NOT_NULL(pR, NULL, ME, "NULL");
    amxc_llist_it_t* it = (amxc_llist_it_t*) amxc_llist_get_first(&pR->llEndPoints);
    ASSERTI_NOT_NULL(it, NULL, ME, "Empty");
    return (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);
}

/* find VAP with matching name */
T_AccessPoint* wld_rad_vap_from_name(T_Radio* pR, const char* ifname) {
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &pR->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(!strcmp((const char*) pAP->alias, ifname)) {
            return pAP;
        }
    }
    return NULL;
}

/* find EP with matching name */
T_EndPoint* wld_rad_ep_from_name(T_Radio* pR, const char* ifname) {
    T_EndPoint* pEP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &pR->llEndPoints) {
        pEP = amxc_llist_it_get_data(it, T_EndPoint, it);
        if(!strcmp((const char*) pEP->Name, ifname)) {
            return pEP;
        }
    }
    return NULL;
}

T_Radio* wld_rad_from_name(const char* ifname) {
    T_Radio* pR;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &g_radios) {
        pR = amxc_llist_it_get_data(it, T_Radio, it);
        if(!strcmp((const char*) pR->Name, ifname)) {
            return pR;
        }
    }

    return NULL;
}

/* find VAP with matching name over all the radios */
T_AccessPoint* wld_vap_from_name(const char* ifname) {
    T_Radio* pR;
    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &g_radios) {
        pR = amxc_llist_it_get_data(it, T_Radio, it);
        if((pAP = wld_rad_vap_from_name(pR, ifname))) {
            break;
        }
    }

    return pAP;
}

/* find VAP with matching name over all the radios */
T_EndPoint* wld_vep_from_name(const char* ifname) {
    T_Radio* pR;
    T_EndPoint* pEP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &g_radios) {
        pR = amxc_llist_it_get_data(it, T_Radio, it);
        if((pEP = wld_rad_ep_from_name(pR, ifname))) {
            break;
        }
    }

    return pEP;
}
bool wld_rad_SupFreqBands_mask_to_string(amxc_string_t* output, uint32_t supportedFrequencyBands) {
    return bitmask_to_string(output, swl_freqBandExt_str, ',', supportedFrequencyBands);
}

bool wld_radio_notify_scanresults(amxd_object_t* obj) {
    const uint32_t type = NOTIFY_SCAN_DONE;
    const char* const name = "ScanComplete";
    SAH_TRACEZ_INFO(ME, "send notification [%d|%s]", type, name);
    amxd_object_trigger_signal(obj, name, NULL);
    return true;
}

amxd_status_t _getRadioAirStats(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args _UNUSED,
                                amxc_var_t* retval_map) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = object->priv;

    if(pR->enable == 0) {
        SAH_TRACEZ_INFO(ME, "No air statistics when disabled");
        return amxd_status_ok;
    }

    T_Airstats stats;
    bzero(&stats, sizeof(stats));

    amxc_var_t vendorStats;
    amxc_var_init(&vendorStats);
    stats.vendorStats = &vendorStats;

    int ret = pR->pFA->mfn_wrad_airstats(pR, &stats);

    if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to get Air statistics");
        amxc_var_clean(&vendorStats);
        return amxd_status_unknown_error;
    }

    amxc_var_set_type(retval_map, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(uint16_t, retval_map, "Load", stats.load);
    amxc_var_add_key(int32_t, retval_map, "Noise", stats.noise);
    amxc_var_add_key(uint16_t, retval_map, "TxTime", stats.bss_transmit_time);
    amxc_var_add_key(uint16_t, retval_map, "RxTime", stats.bss_receive_time);
    amxc_var_add_key(uint16_t, retval_map, "IntTime", stats.other_bss_time + stats.other_time);
    amxc_var_add_key(uint16_t, retval_map, "ObssTime", stats.other_bss_time);
    amxc_var_add_key(uint16_t, retval_map, "NoiseTime", stats.other_time);
    amxc_var_add_key(uint16_t, retval_map, "FreeTime", stats.free_time);
    amxc_var_add_key(uint16_t, retval_map, "TotalTime", stats.total_time);
    amxc_var_add_key(uint32_t, retval_map, "Timestamp", stats.timestamp);
    amxc_var_add_key(uint8_t, retval_map, "ShortPreambleErrorPercentage", stats.short_preamble_error_percentage);
    amxc_var_add_key(uint8_t, retval_map, "LongPreambleErrorPercentage", stats.long_preamble_error_percentage);

    //Update parameters in data model
    amxd_object_t* radio = (amxd_object_t*) pR->pBus;
    amxd_object_set_uint16_t(radio, "ChannelLoad", stats.load);
    amxd_object_set_int32_t(radio, "Noise", stats.noise);
    if(stats.total_time != 0) {
        amxd_object_set_uint16_t(radio, "Interference", (stats.other_bss_time * 100) / stats.total_time);
    } else {
        amxd_object_set_uint16_t(radio, "Interference", 0);
    }
    amxc_var_add_key(amxc_htable_t, retval_map, "VendorStats", amxc_var_get_const_amxc_htable_t(stats.vendorStats));

    amxc_var_clean(&vendorStats);

    return amxd_status_ok;
}

/**
 * Retrieves the rssi on a per antenna basis. Currently we offer enough
 * space for up to 8 antenna's.
 */
amxd_status_t _getPerAntennaRssi(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_ok;
    T_Radio* pR = object->priv;

    if(pR->enable == 0) {
        SAH_TRACEZ_INFO(ME, "No air statistics when disabled");

        return amxd_status_ok;
    }

    T_ANTENNA_RSSI antenna_stats;
    int ret = pR->pFA->mfn_wrad_per_ant_rssi(pR, &antenna_stats);

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    if(ret == WLD_ERROR_NOT_IMPLEMENTED) {
        SAH_TRACEZ_INFO(ME, "Function not supported");
        status = amxd_status_ok;
    } else if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to get per antenna rssi info");
        status = amxd_status_unknown_error;
    }

    return status;
}

amxd_status_t _getLatestPower(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = object->priv;

    if((pR->enable == 0) || (pR->status == RST_DOWN)) {
        SAH_TRACEZ_INFO(ME, "No power antenna when disabled");
        return amxd_status_unknown_error;
    }

    T_ANTENNA_POWER antenna_power;

    int ret = pR->pFA->mfn_wrad_latest_power(pR, &antenna_power);

    if(ret < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to get latest power info");
        return amxd_status_unknown_error;
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    uint8_t i = 0;
    char value[16];

    for(i = 0; i < antenna_power.nr_antenna; i++) {
        snprintf(value, 16, "Antenna %u", i);
        amxc_var_add_key(double, retval, value, antenna_power.power_value[i]);
    }

    return amxd_status_ok;
}

/*
   Old and New rules... (( WIKI source )) In 2007 the FCC & ETSI began requiring that devices
   operating on 5.250–5.350 GHz and 5.470–5.725 GHz must employ dynamic frequency selection
   (DFS) and transmit power control (TPC) capabilities. This is to avoid interference with
   weather-radar and military applications. In 2010, the FCC further clarified the use of channels
   in the 5.470–5.725 GHz band to avoid interference with Terminal Doppler Weather Radar (TDWR)
   systems. These restrictions are now referred to collectively as the "Old Rules".

   On June 10, 2015, the FCC approved a new ruleset for 5 GHz device operation (called the "New
   Rules"), which adds 160 and 80 GHz channel identifiers, and re-enables previously prohibited
   DFS channels, in Publication Number 905462. This FCC publication eliminates the ability for
   manufacturers to have devices approved or modified under the Old Rules in phases.
   The New Rules apply in all circumstances as of June 2, 2016.[21]

   For this, we must check the drivers "channel list" in more detail to support also those extra
   channels with their supported channel bandwitdth when they become free.
   The extra functions introduced here are to simplify the task and they should be common for all
   our plugin versions!
 */
bool wld_rad_hasWeatherChannels(T_Radio* pRad) {
    for(int i = 0; pRad && pRad->possibleChannels[i]; i++) {
        if((pRad->possibleChannels[i] == 120) ||
           ( pRad->possibleChannels[i] == 124) ||
           ( pRad->possibleChannels[i] == 128)) {
            return TRUE;
        }
    }
    return FALSE;
}

bool wld_rad_hasChannel(T_Radio* pRad, int chan) {
    for(int i = 0; pRad && pRad->possibleChannels[i]; i++) {
        if(pRad->possibleChannels[i] == chan) {
            return TRUE;
        }
    }
    return FALSE;
}

bool wld_rad_hasChannelWidthCovered(T_Radio* pRad, swl_bandwidth_e chW) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    return (swl_chanspec_bwToInt(pRad->runningChannelBandwidth) >= swl_chanspec_bwToInt(chW));
}

wld_channel_extensionPos_e wld_rad_getExtensionChannel(T_Radio* pRad) {
    ASSERTI_TRUE(wld_rad_hasChannelWidthCovered(pRad, SWL_BW_40MHZ), WLD_CHANNEL_EXTENTION_POS_NONE, ME, "not supported");
    if(pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_2_4GHZ) {
        if((pRad->channel / 4) % 2) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    if(pRad->extensionChannel == WLD_CHANNEL_EXTENTION_POS_AUTO) {
        if(pRad->channel < 7) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    return pRad->extensionChannel;
}

bool wld_rad_hasEnabledEp(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    amxc_llist_it_t* it = NULL;
    T_EndPoint* pEP = NULL;

    /* Check if NO AP is active */
    amxc_llist_for_each(it, &pRad->llEndPoints) {
        pEP = amxc_llist_it_get_data(it, T_EndPoint, it);
        if(pEP->enable) {
            return true;
        }
    }
    return false;
}

bool wld_rad_hasConnectedEp(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    amxc_llist_it_t* it = NULL;
    T_EndPoint* pEP = NULL;

    /* Check if NO AP is active */
    amxc_llist_for_each(it, &pRad->llEndPoints) {
        pEP = amxc_llist_it_get_data(it, T_EndPoint, it);
        if(pEP->connectionStatus == EPCS_CONNECTED) {
            return true;
        }
    }
    return false;
}

bool wld_rad_hasEnabledVap(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    amxc_llist_it_t* it = NULL;
    T_AccessPoint* pAP = NULL;

    /* Check if NO AP is active */
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(pAP->enable) {
            return true;
        }
    }
    return false;
}

bool wld_rad_hasActiveVap(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    amxc_llist_it_t* it = NULL;
    T_AccessPoint* pAP = NULL;

    /* Check if NO AP is active */
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(pAP->status == APSTI_ENABLED) {
            return true;
        }
    }
    return false;
}

uint32_t wld_rad_getFirstEnabledIfaceIndex(T_Radio* pRad) {
    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pRad) {
        if((pAP->index > 0) &&
           (wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pAP->alias) > 0)) {
            return pAP->index;
        }
    }
    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, pRad) {
        if((pEP->index > 0) &&
           (wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pEP->Name) > 0)) {
            return pEP->index;
        }
    }
    return 0;
}

bool wld_rad_hasEnabledIface(T_Radio* pRad) {
    return (wld_rad_getFirstEnabledIfaceIndex(pRad) > 0);
}

uint32_t wld_rad_getFirstActiveIfaceIndex(T_Radio* pRad) {
    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pRad) {
        if((pAP->index > 0) && (pRad->pFA->mfn_wvap_status(pAP) > 0)) {
            return pAP->index;
        }
    }
    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, pRad) {
        if((pEP->index > 0) && (pRad->pFA->mfn_wendpoint_status(pEP) >= SWL_RC_OK)) {
            return pEP->index;
        }
    }
    return 0;
}

bool wld_rad_hasActiveIface(T_Radio* pRad) {
    return (wld_rad_getFirstActiveIfaceIndex(pRad) > 0);
}

bool wld_rad_isChannelSubset(T_Radio* pRad, uint8_t* chanlist, int size) {
    int i = 0;
    int j = 0;

    for(i = 0; i < size; i++) {
        for(j = 0; j < pRad->nrPossibleChannels; j++) {
            if(pRad->possibleChannels[j] == chanlist[i]) {
                break;
            }
        }
        if(j == pRad->nrPossibleChannels) {
            return false;
        }
    }
    return true;
}

/**
 * Function to check if ONLY an active EndPoint is present.
 * In that case we can ignore some (AP) services as the Radio
 * initial act as a passive device.
 *
 * @param pRad - Raido handler
 *
 * @return bool - TRUE when no active AP are found on this Radio
 *         wow pure EP, else FALSE.
 */
bool wld_rad_hasOnlyActiveEP(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");

    T_EndPoint* pEP = NULL;
    amxc_llist_it_t* it = NULL;


    /* Do we've an active EP? */
    it = amxc_llist_get_first(&pRad->llEndPoints);
    if(it) {
        pEP = amxc_llist_it_get_data(it, T_EndPoint, it);
        if(!(pRad->isSTA && pRad->enable && pEP->enable)) {
            return false;
        }
    }

    return !wld_rad_hasActiveVap(pRad);
}

/*
   Some channels are not 11AC channels!
   To handle the trouble channels we must check what we have and if we can support them.
   When the Weather channels are included, we've an extra 11AC group. (116,120,124 and 128).
   When channel 144 is freeed (already in US), this also enables one (132,136,140 and 144)!

   !!! IMPORTANT !!!
   Following channel rule is implemented: The user can ALWAYS select the CHANNEL! If selected
   channel can't be set due current "USER BANDWIDTH" mode, we set the next highest possible
   BANDWIDTH that allows us to SET the requested channel!
   It's up to the caller to keep track of the USER BANDWIDTH and restore it back! This is
   needed to switch back to initial user bandwidth when old (or new) possible channel is
   set/restored.
 */

void do_updateOperatingChannelBandwidth5GHz(T_Radio* pRad) {
    switch(pRad->channel) {
    case 116:
        if(!wld_rad_hasWeatherChannels(pRad)) {
            // We've only Channel 116!
            pRad->runningChannelBandwidth = SWL_BW_20MHZ;
        }
        break;
    case 132:
    case 136:
    case 140:
    case 144: // Future, is already freed in US!
        if(!wld_rad_hasChannel(pRad, 144)) {
            if((pRad->channel == 140) || (pRad->operatingChannelBandwidth == SWL_BW_20MHZ)) {
                pRad->runningChannelBandwidth = SWL_BW_20MHZ;
            } else {
                pRad->runningChannelBandwidth = SWL_BW_40MHZ;
            }
        }
        break;
    case 165:
        pRad->runningChannelBandwidth = SWL_BW_20MHZ;
        break;
    default:
        /* No need to overrule user ChannelBandwidth setting ALL combinations are allowed! */
        break;
    }
}

/*
   Based on a selected channel and BandWidth, we create a string that presents the used channels.
   In 2.4 if you practically cover multiple channels. Ex channel 3 will cover 1,2,3,4,5!
   Still we present it as channel 3. When 40MHz BandWidth is selected we will show the string
   3..7. Still in reallity we've 1,2,3,4,5,6,7,8,9 used. But it's hard to show channel 0 and -1?
   or 14 and 15? just to present the freq spectrum.

   String we will return:
   20 MHz : 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 (, 14)
   40 MHz : 1..5, 2..6, 3..7, 4..8, 5..9, 6..10, 7..11, 8..12, 9..13, 10..14

   In the 5GHz we can clearly show the selected channels used When BW mode
   20 MHz: 36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144
   40 MHz: 36-40, 40-48, 52-56, 60-64, 100-104, 108-112, 116-120, 124-128, 132-136, 140-144
   80 MHz: 36-40-44-48, 52-56-60-64, 100-104-108-112, 116-120-124-128, 132-136-140-144
   160 MHz: 36-40-44-48-52-56-60-64, 100-104-108-112-116-120-124-128

   Start thinking how to present futures 160 MHz? And 80+80 modes... Due country restrictions not
   all channels are aviable!
 */

char* getChannelsInUseStr(T_Radio* pRad) {
    int chidx;

    if(pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_2_4GHZ) {
        /* 5GHz and 6GHz, find channel index */
        for(chidx = 0;
            pRad->possibleChannels[chidx] && pRad->possibleChannels[chidx] != pRad->channel;
            chidx++) {
        }

        if(!pRad->possibleChannels[chidx]) {
            SAH_TRACEZ_ERROR(ME, "%s has an unsupported fix channel %d",
                             pRad->Name,
                             pRad->channel);
            return "ERROR";
        }

        int index = pRad->runningChannelBandwidth;
        if((index == SWL_BW_AUTO) && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ)) {
            index = SWL_BW_80MHZ;
        }
        if((index == SWL_BW_AUTO) && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ)) {
            index = SWL_BW_160MHZ;
        }
        int val = 1;
        if(index > 1) {
            val = 2 << (index - 2);
            chidx -= chidx % val;
        }
        SAH_TRACEZ_INFO(ME, "%s: index <%d> cbw <%d> val <%d>", pRad->Name, index, pRad->runningChannelBandwidth, val);

        int i = 0;
        pRad->channelsInUse[0] = 0;
        for(i = 0; i < val; i++) {
            wldu_catFormat(pRad->channelsInUse, sizeof(pRad->channelsInUse), "%d,", pRad->possibleChannels[chidx + i]);
        }
    } else {
        // 2.4
        switch(pRad->runningChannelBandwidth) {
        case SWL_BW_40MHZ:
            if(pRad->channel <= 6) {
                /* Lower channel has been used as primary */
                snprintf(pRad->channelsInUse, sizeof(pRad->channelsInUse),
                         "%d..%d", pRad->channel, pRad->channel + 4);
            } else {
                /* Upper channels has been used as primary */
                snprintf(pRad->channelsInUse, sizeof(pRad->channelsInUse),
                         "%d..%d", pRad->channel - 4, pRad->channel);
            }
            break;
        case SWL_BW_AUTO:
        case SWL_BW_20MHZ:
            snprintf(pRad->channelsInUse, sizeof(pRad->channelsInUse),
                     "%d", pRad->channel);
            break;
        default:
            SAH_TRACEZ_ERROR(ME, "%s 2.4GHz don't know BandWidth mode %x",
                             pRad->Name,
                             pRad->runningChannelBandwidth);
            break;
        }
    }
    return pRad->channelsInUse;
}

bool wld_rad_is_6ghz(const T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");

    return (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ);
}

bool wld_rad_is_5ghz(const T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");

    if((pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ||
       ((pRad->supportedFrequencyBands & M_SWL_FREQ_BAND_EXT_5GHZ) &&
        ((pRad->channel >= 36) && (pRad->channel % 4 == 0)))) {
        return true;
    } else {
        return false;
    }
}


bool wld_rad_is_24ghz(const T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");

    if((pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) ||
       ((pRad->supportedFrequencyBands & M_SWL_FREQ_BAND_EXT_2_4GHZ) &&
        (pRad->channel < 36))) {
        return true;
    } else {
        return false;
    }
}

bool wld_rad_is_on_dfs(T_Radio* pRad) {
    if(pRad == NULL) {
        SAH_TRACEZ_ERROR(ME, "req dfs of null rad");
        return false;
    }
    return wld_channel_is_dfs_band(pRad->channel, pRad->runningChannelBandwidth);
}

bool wld_rad_isDoingDfsScan(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    return ((pRad->detailedState == CM_RAD_FG_CAC)
            || (pRad->detailedState == CM_RAD_BG_CAC)
            || (pRad->detailedState == CM_RAD_BG_CAC_NS)
            || (pRad->bgdfs_config.channel != 0));
}

/**
 * Return whether the radio is up and not doing any scanning itself.
 * External scanning is allowed.
 */
bool wld_rad_isUpAndReady(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    return ((pRad->detailedState == CM_RAD_UP)
            || (pRad->detailedState == CM_RAD_BG_CAC_EXT)
            || (pRad->detailedState == CM_RAD_BG_CAC_EXT_NS)
            || (pRad->detailedState == CM_RAD_DELAY_AP_UP))
           && !wld_scan_isRunning(pRad);
}


/**
 * Return whether the radio is up
 * Need to retrieve current operating status of the radio based on driver
 * status to avoid any undesirable states due to configuration process which
 * could not be reflected in radio global status
 */
bool wld_rad_isUpExt(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    return ((pRad->detailedState == CM_RAD_UP)
            || (pRad->detailedState == CM_RAD_FG_CAC)
            || (pRad->detailedState == CM_RAD_BG_CAC)
            || (pRad->detailedState == CM_RAD_BG_CAC_EXT)
            || (pRad->detailedState == CM_RAD_BG_CAC_EXT_NS)
            || (pRad->detailedState == CM_RAD_DELAY_AP_UP));
}

swl_bandwidth_e wld_rad_verify_bandwidth(T_Radio* pRad, swl_bandwidth_e targetbandwidth) {
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(pRad->channel, targetbandwidth, pRad->operatingFrequencyBand);
    while(!wld_channel_is_band_available(chanspec) && targetbandwidth > SWL_BW_20MHZ) {
        SAH_TRACEZ_INFO(ME, "band not avail %u %u %s", pRad->channel, targetbandwidth, pRad->Name);
        targetbandwidth--;
        chanspec.bandwidth = targetbandwidth;
    }
    if(pRad->obssCoexistenceEnabled && pRad->obssCoexistenceActive && (targetbandwidth == SWL_BW_40MHZ)) {
        targetbandwidth = SWL_BW_20MHZ;
    }

    if(!wld_channel_is_band_available(chanspec)) {
        SAH_TRACEZ_ERROR(ME, "No band available on channel!! %u %u %u",
                         pRad->channel,
                         pRad->runningChannelBandwidth,
                         pRad->operatingChannelBandwidth);
        return SWL_BW_AUTO;
    }
    return targetbandwidth;
}

swl_bandwidth_e wld_rad_getBaseConfigBw(T_Radio* pRad) {
    if(pRad->operatingChannelBandwidth != SWL_BW_AUTO) {
        return pRad->operatingChannelBandwidth;
    }
    if(wld_rad_is_5ghz(pRad)) {
        return SWL_BW_80MHZ;
    } else if(wld_rad_is_6ghz(pRad)) {
        return SWL_BW_160MHZ;
    } else {
        return SWL_BW_20MHZ;
    }
}

swl_bandwidth_e wld_rad_get_target_bandwidth(T_Radio* pRad) {
    int targetBandwidth;
    if(pRad->operatingChannelBandwidth != SWL_BW_AUTO) {
        if(pRad->obssCoexistenceActive) {
            if(pRad->operatingChannelBandwidth != SWL_BW_20MHZ) {
                SAH_TRACEZ_INFO(ME, "%s : coexistence activated--> forcing targetBandwidth to 20MHz", pRad->Name);
                pRad->channelBandwidthChangeReason = CHAN_REASON_OBSS_COEX; // forcing 20MHz--> even if the reason is manual in origin-->becomes CHAN_REASON_OBSS_COEX
            }
            targetBandwidth = SWL_BW_20MHZ;
        } else {
            targetBandwidth = pRad->operatingChannelBandwidth;
            if(!pRad->autoChannelEnable) {
                pRad->channelBandwidthChangeReason = CHAN_REASON_MANUAL;
            }
        }
    } else {
        if(!pRad->autoChannelEnable) {
            if(!wld_rad_is_24ghz(pRad)) {
                swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(pRad->channel, pRad->maxChannelBandwidth, pRad->operatingFrequencyBand);
                if((pRad->autoBwSelectMode == BW_SELECT_MODE_MAXAVAILABLE)
                   || ((pRad->autoBwSelectMode == BW_SELECT_MODE_MAXCLEARED)
                       && (wld_channel_is_band_usable(chanspec)))) {
                    targetBandwidth = pRad->maxChannelBandwidth;
                } else if(wld_rad_is_6ghz(pRad)) {
                    targetBandwidth = SWL_BW_160MHZ;
                } else {
                    targetBandwidth = SWL_BW_80MHZ;
                }
            } else {
                targetBandwidth = SWL_BW_20MHZ;
            }
        } else {
            targetBandwidth = pRad->runningChannelBandwidth;
        }
        pRad->channelBandwidthChangeReason = CHAN_REASON_AUTO;
    }
    targetBandwidth = wld_rad_verify_bandwidth(pRad, targetBandwidth);
    return targetBandwidth;
}

void wld_rad_get_update_running_bandwidth(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "Radio NULL");
    swl_bandwidth_e targetBandwidth = wld_rad_get_target_bandwidth(pRad);
    if(targetBandwidth != SWL_BW_AUTO) {
        if(pRad->runningChannelBandwidth != targetBandwidth) {
            wld_rad_chan_notification(pRad, pRad->channel, targetBandwidth);
            wld_rad_setRunningChannelBandwidth(pRad, targetBandwidth);
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "Requested bw %u for rad %s not available on current channel %u (%u, %u)"
                         , pRad->operatingChannelBandwidth, pRad->Name, pRad->channel, pRad->runningChannelBandwidth, targetBandwidth);
    }
}

void wld_rad_setRunningChannelBandwidth(T_Radio* pRad, swl_bandwidth_e newBw) {
    ASSERTI_NOT_EQUALS(newBw, pRad->runningChannelBandwidth, , ME, "%s : bw same %u", pRad->Name, newBw);

    SAH_TRACEZ_INFO(ME, "%s set runBw from %u to %u", pRad->Name, pRad->runningChannelBandwidth, newBw);
    if(wld_rad_is_24ghz(pRad)
       && pRad->obssCoexistenceEnabled
       && (pRad->operatingChannelBandwidth == SWL_BW_40MHZ)
       && (pRad->runningChannelBandwidth == SWL_BW_40MHZ)
       && (newBw == SWL_BW_20MHZ)) {
        pRad->obssCoexistenceActive = true;
        SAH_TRACEZ_WARNING(ME, "%s : enabling obss coexistence %u / %u => %u", pRad->Name, pRad->channel,
                           pRad->runningChannelBandwidth, newBw);
    }
    pRad->runningChannelBandwidth = newBw;

    wld_rad_update_operating_standard(pRad);
}

void wld_rad_write_possible_channels(T_Radio* pRad) {
    int idx = 0;
    char TBuf[320] = {'\0'};
    char ValBuf[32] = {'\0'};
    for(idx = 0; pRad->possibleChannels[idx]; idx++) {
        if(idx && TBuf[0]) {
            wldu_catStr(TBuf, ",", sizeof(TBuf));
        }
        wldu_catStr(TBuf, itoa(pRad->possibleChannels[idx], ValBuf, 10), sizeof(TBuf));
    }

    amxd_object_set_cstring_t(pRad->pBus, "PossibleChannels", TBuf);
    amxd_object_set_cstring_t(pRad->pBus, "MaxChannelBandwidth", Rad_SupBW[pRad->maxChannelBandwidth]);
}

bool wld_rad_has_endpoint_enabled(T_Radio* rad) {
    return wld_rad_getEnabledEndpoint(rad) != NULL;
}

T_EndPoint* wld_rad_getEnabledEndpoint(T_Radio* rad) {
    ASSERT_NOT_NULL(rad, NULL, ME, "NULL");
    T_EndPoint* pEndpoint;
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &rad->llEndPoints) {
        pEndpoint = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);

        if(pEndpoint && pEndpoint->enable) {
            return pEndpoint;
        }
    }
    return NULL;
}

bool wld_rad_hasWpsActiveEndpoint(T_Radio* rad) {
    return wld_rad_getWpsActiveEndpoint(rad) != NULL;
}

T_EndPoint* wld_rad_getWpsActiveEndpoint(T_Radio* rad) {
    ASSERT_NOT_NULL(rad, NULL, ME, "NULL");
    T_EndPoint* pEndpoint;
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &rad->llEndPoints) {
        pEndpoint = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);

        if(pEndpoint && (pEndpoint->connectionStatus == EPCS_WPS_PAIRING)) {
            return pEndpoint;
        }
    }
    return NULL;
}

void wld_rad_updateActiveDevices(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    amxc_llist_it_t* it = NULL;
    T_AccessPoint* pAP;

    int nrActiveDevices = 0;

    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        nrActiveDevices += pAP->ActiveAssociatedDeviceNumberOfEntries;
    }
    pRad->currentStations = nrActiveDevices;
    amxd_object_set_int32_t(pRad->pBus, "ActiveAssociatedDevices", pRad->currentStations);
}

T_Radio* wld_rad_get_radio(const char* ifname) {
    T_Radio* pRad = NULL;
    amxc_llist_it_t* llit = NULL;
    amxc_llist_for_each(llit, &g_radios) {
        pRad = amxc_llist_it_get_data(llit, T_Radio, it);
        if(pRad && !strcmp(pRad->Name, ifname)) {
            return pRad;
        }
    }

    return NULL;
}

void wld_rad_chan_update_model(T_Radio* pRad) {
    /* Update object fields directly if changed */
    amxd_object_set_uint32_t(pRad->pBus, "Channel", pRad->channel);
    amxd_object_set_cstring_t(pRad->pBus, "ChannelsInUse", (void*) getChannelsInUseStr(pRad));

    amxc_string_t operatingStandardsText = swl_radStd_toStr(pRad->operatingStandards, pRad->operatingStandardsFormat, pRad->supportedStandards);
    amxd_object_set_cstring_t(pRad->pBus, "OperatingStandards", operatingStandardsText.buffer);
    amxc_string_clean(&operatingStandardsText);
    amxd_object_set_cstring_t(pRad->pBus, "CurrentOperatingChannelBandwidth", Rad_SupBW[pRad->runningChannelBandwidth]);

}

void wld_rad_updateOperatingClass(T_Radio* pRad) {
    swl_chanspec_t chanspec = wld_rad_getSwlChanspec(pRad);
    swl_opClassCountry_e countryZone = getCountryZone(pRad->regulatoryDomainIdx);
    pRad->operatingClass = swl_chanspec_getLocalOperClass(&chanspec, countryZone);
    amxd_object_set_uint32_t(pRad->pBus, "OperatingClass", pRad->operatingClass);
    SAH_TRACEZ_INFO(ME, "%s: set operatingClass to %d", pRad->Name, pRad->operatingClass);
}

void _wld_rad_setOperatingClass(const char* const sig_name _UNUSED,
                                const amxc_var_t* const data,
                                void* const priv _UNUSED) {
    amxd_object_t* object = amxd_dm_signal_get_object(get_wld_plugin_dm(), data);
    ASSERTS_NOT_NULL(object, , ME, "NULL");
    T_Radio* pRad = (T_Radio*) object->priv;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, , ME, "NULL");
    wld_rad_updateOperatingClass(pRad);
}

void s_selectRadio(amxd_object_t* const object, int32_t depth _UNUSED, void* priv) {
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, , ME, "Not instance");
    amxd_object_t* parent = amxd_object_get_parent(object);
    ASSERTS_TRUE(swl_str_matches(amxd_object_get_name(parent, AMXD_OBJECT_NAMED), "Radio"), , ME, "Not under Radio");
    ASSERTS_EQUALS(amxd_object_get_parent(parent), get_wld_object(), , ME, "Not under WiFi");
    T_Radio** ppRad = (T_Radio**) priv;
    ASSERTS_NOT_NULL(ppRad, , ME, "NULL");
    *ppRad = object->priv;
}
void _wld_rad_setVendorData_ocf(const char* const sig_name _UNUSED,
                                const amxc_var_t* const data,
                                void* const priv _UNUSED) {
    amxd_dm_t* dm = get_wld_plugin_dm();
    amxd_object_t* object = amxd_dm_signal_get_object(dm, data);
    T_Radio* pR = NULL;
    amxd_object_hierarchy_walk(object, amxd_direction_up, NULL, s_selectRadio, INT32_MAX, &pR);
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "NO radio Ctx");
    SAH_TRACEZ_INFO(ME, "%s: %s rad vendor data (%s)", pR->Name, sig_name, GET_CHAR(data, "object"));
    pR->pFA->mfn_wifi_supvend_modes(pR, NULL, object, GET_ARG(data, "parameters"));
}

void wld_rad_chan_notification(T_Radio* pRad, int newChannelValue, int newBandwidthValue) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    amxc_var_t* my_map = amxc_var_add_key(amxc_htable_t, &params, "Updates", NULL);

    time_t now = time(NULL);
    struct tm now_timestamp;
    gmtime_r(&now, &now_timestamp);
    amxc_ts_t ts;
    amxc_var_add_key(amxc_ts_t, my_map, "TimeStamp", &ts);

    amxc_var_add_key(uint32_t, my_map, "OldChannel", pRad->channel);
    amxc_var_add_key(uint32_t, my_map, "NewChannel", newChannelValue);
    amxc_var_add_key(cstring_t, my_map, "ChannelChangeReason", g_wld_channelChangeReason_str[pRad->channelChangeReason]);
    amxc_var_add_key(cstring_t, my_map, "OldBandwidth", Rad_SupBW[pRad->runningChannelBandwidth]);
    amxc_var_add_key(cstring_t, my_map, "NewBandwidth", Rad_SupBW[newBandwidthValue]);
    amxc_var_add_key(cstring_t, my_map, "ChannelBandwidthChangeReason", g_wld_channelChangeReason_str[pRad->channelBandwidthChangeReason]);

    amxc_var_add_key(uint32_t, my_map, "NrAssociatedStations", wld_rad_get_nb_active_stations(pRad));
    amxc_var_add_key(uint32_t, my_map, "NrAssociatedVideoStations", wld_rad_get_nb_active_video_stations(pRad));

    amxd_object_trigger_signal(pRad->pBus, "Channel change event", &params);

    SAH_TRACEZ_INFO(ME, "Send channel change notification");

    // Switch values
    pRad->channel = newChannelValue;
    pRad->runningChannelBandwidth = newBandwidthValue;

    amxc_var_clean(&params);
}

void wld_rad_init_counters(T_Radio* pRad, T_EventCounterList* counters, const char** defaults) {
    amxd_object_t* counters_template = amxd_object_findf(pRad->pBus, "EventCounter");
    uint32_t i = 0;
    for(i = 0; i < counters->nrCounters; i++) {
        amxd_object_t* object = NULL;
        amxd_object_new_instance(&object, counters_template, counters->names[i], 0, NULL);
        ASSERT_NOT_NULL(object, , ME, "%s object create error", pRad->Name);

        counters->values[i].object = object;
        amxd_object_set_cstring_t(object, "Key", counters->names[i]);
        if((defaults != NULL) && (defaults[i] != NULL)) {
            amxd_object_set_cstring_t(object, "Info", defaults[i]);
        }
    }
}

void wld_rad_increment_counter(T_Radio* pRad, T_EventCounterList* counters, uint32_t index, const char* info) {
    (void) pRad;
    ASSERTI_TRUE(index < counters->nrCounters, , ME, "VALUE");
    T_EventCounter* value = &(counters->values[index]);
    value->counter++;
    const char* oldInfo = amxd_object_get_cstring_t(value->object, "Info", NULL);
    SAH_TRACEZ_WARNING(ME, "EVENT %s: %s : %u : %s (was %s)", pRad->Name, counters->names[index], value->counter, info, oldInfo);

    value->lastEventTime = wld_util_time_monotonic_sec();
    amxd_object_set_uint32_t(value->object, "Value", value->counter);
    struct tm lastTime;
    wld_util_time_monotonic_to_tm(value->lastEventTime, &lastTime);
    amxc_ts_t ts;
    amxd_object_set_value(amxc_ts_t, value->object, "LastOccurrence", &ts);
    amxd_object_set_cstring_t(value->object, "Info", info);
}

void wld_rad_incrementCounterStr(T_Radio* pRad, T_EventCounterList* counters, uint32_t index, const char* template, ...) {
    va_list args;
    va_start(args, template);

    char buffer[250];
    vsnprintf(buffer, sizeof(buffer), template, args);
    va_end(args);
    wld_rad_increment_counter(pRad, counters, index, buffer);
}

/**
 * Whether radio is available for commands
 */
bool wld_rad_isAvailable(T_Radio* pRad) {
    if((pRad->status == RST_ERROR)
       || (pRad->status == RST_UNKNOWN)
       || (pRad->status == RST_NOTPRESENT)
       || (pRad->detailedState == CM_RAD_DEEP_POWER_DOWN)) {
        return false;
    }
    return true;
}

T_AccessPoint* wld_radio_getVapFromRole(T_Radio* pRad, wld_apRole_e role) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    amxc_llist_it_t* it = NULL;
    amxc_llist_for_each(it, &pRad->llAP) {
        T_AccessPoint* pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(pAP->apRole == role) {
            return pAP;
        }
    }

    return NULL;
}

T_AccessPoint* wld_rad_firstAp(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_get_first(&pRad->llAP);
    ASSERTS_NOT_NULL(it, NULL, ME, "Empty");
    T_AccessPoint* pAP = wld_ap_fromIt(it);
    ASSERTS_TRUE(debugIsVapPointer(pAP), NULL, ME, "%s: invalid first vap", pRad->Name);
    return pAP;
}
T_AccessPoint* wld_rad_nextAp(T_Radio* pRad _UNUSED, T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_it_get_next(&pAP->it);
    ASSERTS_NOT_NULL(it, NULL, ME, "Last");
    T_AccessPoint* nextAp = wld_ap_fromIt(it);
    ASSERTS_TRUE(debugIsVapPointer(nextAp), NULL, ME, "%s: invalid next vap of %s", pRad->Name, pAP->name);
    return nextAp;
}

T_EndPoint* wld_rad_firstEp(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_get_first(&pRad->llEndPoints);
    ASSERTS_NOT_NULL(it, NULL, ME, "Empty");
    T_EndPoint* pEP = wld_endpoint_fromIt(it);
    ASSERTS_TRUE(debugIsEpPointer(pEP), NULL, ME, "%s: invalid first ep", pRad->Name);
    return pEP;
}

T_EndPoint* wld_rad_nextEp(T_Radio* pRad _UNUSED, T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_it_get_next(&pEP->it);
    ASSERTS_NOT_NULL(it, NULL, ME, "Last");
    T_EndPoint* nextEp = wld_endpoint_fromIt(it);
    ASSERTS_TRUE(debugIsEpPointer(nextEp), NULL, ME, "%s: invalid next ep", pRad->Name, pEP->Name);
    return nextEp;
}

T_AccessPoint* wld_rad_getVapByIndex(T_Radio* pRad, int index) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->index == index) {
            return pAP;
        }
    }
    return NULL;
}

T_EndPoint* wld_rad_getEpByIndex(T_Radio* pRad, int index) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_EndPoint* pEP = NULL;
    wld_rad_forEachEp(pEP, pRad) {
        if(pEP->index == index) {
            return pEP;
        }
    }
    return NULL;
}

T_Radio* wld_rad_fromIt(amxc_llist_it_t* it) {
    if(it == NULL) {
        return NULL;
    }
    return amxc_llist_it_get_data(it, T_Radio, it);
}

/**
 * Radio is enabled, and likely available for commands.
 */
bool wld_rad_isActive(T_Radio* pRad) {
    return pRad->status == RST_UP || pRad->status == RST_DORMANT;
}

/**
 * Radio is up and broadcasting signal.
 */
bool wld_rad_isUp(T_Radio* pRad) {
    return pRad->status == RST_UP;
}

/* Set the socket on the Radio structure. */
int wld_rad_getSocket(T_Radio* rad) {
    ASSERT_NOT_NULL(rad, -1, ME, "NULL");
    ASSERT_TRUE(rad->wlRadio_SK > 0, -1, ME, "INVALID");

    return rad->wlRadio_SK;
}

void wld_rad_resetStatusHistogram(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    memset(&pRad->changeInfo.statusHistogram.data, 0, sizeof(pRad->changeInfo.statusHistogram.data));
}


void wld_rad_updateState(T_Radio* pRad, bool forceVapUpdate) {
    ASSERTI_NOT_NULL(pRad, , ME, "Radio null");
    if(pRad->detailedState >= CM_RAD_MAX) {
        SAH_TRACEZ_ERROR(ME, "Detailed rad state wrong %s : %u, entering error", pRad->Name, pRad->detailedState);
        pRad->detailedState = CM_RAD_ERROR;
    }
    wld_util_updateStatusChangeInfo(&pRad->changeInfo, pRad->status);

    int curState = pRad->pFA->mfn_wrad_radio_status(pRad);
    wld_status_e oldStatus = pRad->status;

    if((pRad->enable && wld_util_areAllVapsDisabled(pRad) && wld_util_areAllEndpointsDisabled(pRad)) || (pRad->detailedState == CM_RAD_FG_CAC)) {
        pRad->status = RST_DORMANT;
    } else if((pRad->detailedState == CM_RAD_DOWN) || (curState == 0) || (pRad->detailedState == CM_RAD_DEEP_POWER_DOWN)) {
        pRad->status = RST_DOWN;
    } else if(pRad->detailedState == CM_RAD_UNKNOWN) {
        pRad->status = RST_UNKNOWN;
    } else if(pRad->detailedState == CM_RAD_ERROR) {
        pRad->status = RST_ERROR;
    } else {
        // In case of BG_CAC or plain UP -> just set up
        pRad->status = RST_UP;
    }

    //prepare radio status change event to all subscribers
    wld_radio_status_change_event_t event;
    event.radio = pRad;
    event.oldStatus = oldStatus;
    event.oldDetailedState = pRad->detailedStatePrev;

    //Update object
    if(pRad->detailedState != pRad->detailedStatePrev) {
        pRad->detailedStatePrev = pRad->detailedState;
        amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
        amxd_object_set_cstring_t(chanObject, "RadioStatus", cstr_chanmgt_rad_state[pRad->detailedState]);

    }

    if((event.oldDetailedState != pRad->detailedState) || (event.oldStatus != pRad->status)) {
        //send radio status change event to all subscribers
        wld_event_trigger_callback(gWld_queue_rad_onStatusChange, &event);
        SAH_TRACEZ_INFO(ME, "%s update status from %u/%u to %u/%u at %s", pRad->Name,
                        event.oldStatus, event.oldDetailedState,
                        pRad->status, pRad->detailedState,
                        swl_typeTimeMono_toBuf32(pRad->changeInfo.lastStatusChange).buf);
    }

    if(oldStatus != pRad->status) {
        pRad->changeInfo.lastStatusChange = swl_time_getMonoSec();
        pRad->changeInfo.nrStatusChanges++;

        if(pRad->pBus != NULL) {
            amxd_trans_t trans;
            ASSERT_TRANSACTION_INIT(pRad->pBus, &trans, , ME, "%s : trans init failure", pRad->Name);
            amxd_trans_set_value(cstring_t, &trans, "Status", Rad_SupStatus[pRad->status]);
            swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pRad->changeInfo.lastStatusChange);
            wld_dm_transaction_apply(&trans, get_wld_plugin_dm());
            ASSERT_TRANSACTION_END(&trans, get_wld_plugin_dm(), , ME, "%s : trans apply failure", pRad->Name);
        }
    }

    if((oldStatus == pRad->status) && !forceVapUpdate) {
        SAH_TRACEZ_INFO(ME, "%s : Same state %u / %u", pRad->Name, pRad->status, pRad->detailedState);
        return;
    }

    if((pRad->status == RST_DOWN) && pRad->obssCoexistenceActive) {
        SAH_TRACEZ_INFO(ME, "%s : disabling obssCoexistence", pRad->Name);
        pRad->obssCoexistenceActive = false;
    }

    //Update VAPs

    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;

    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        wld_vap_updateState(pAP);
    }

    wld_radStaMon_updateActive(pRad);
}

void wld_rad_triggerChangeEvent(T_Radio* pRad, wld_rad_changeEvent_e event, void* data) {
    wld_rad_changeEvent_t changeEv;
    changeEv.pRad = pRad;
    changeEv.changeType = event;
    changeEv.changeData = data;

    wld_event_trigger_callback(gWld_queue_rad_onChangeEvent, &changeEv);
}

uint32_t wld_rad_getCurrentFreq(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, 0, ME, "NULL");
    swl_chanspec_t spec = wld_rad_getSwlChanspec(pRad);
    return swl_chanspec_toMHzDef(&spec, 0);
}

static void s_setStats(amxc_var_t* pRetMap, T_Stats* pStats) {
    amxc_var_add_key(uint64_t, pRetMap, "BytesSent", pStats->BytesSent);
    amxc_var_add_key(uint64_t, pRetMap, "BytesSent", pStats->BytesSent);
    amxc_var_add_key(uint64_t, pRetMap, "BytesReceived", pStats->BytesReceived);
    amxc_var_add_key(uint64_t, pRetMap, "PacketsSent", pStats->PacketsSent);
    amxc_var_add_key(uint64_t, pRetMap, "PacketsReceived", pStats->PacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "ErrorsSent", pStats->ErrorsSent);
    amxc_var_add_key(uint32_t, pRetMap, "ErrorsReceived", pStats->ErrorsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "RetransCount", pStats->RetransCount);
    amxc_var_add_key(uint32_t, pRetMap, "DiscardPacketsSent", pStats->DiscardPacketsSent);
    amxc_var_add_key(uint32_t, pRetMap, "DiscardPacketsReceived", pStats->DiscardPacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "UnicastPacketsSent", pStats->UnicastPacketsSent);
    amxc_var_add_key(uint32_t, pRetMap, "UnicastPacketsReceived", pStats->UnicastPacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "MulticastPacketsSent", pStats->MulticastPacketsSent);
    amxc_var_add_key(uint32_t, pRetMap, "MulticastPacketsReceived", pStats->MulticastPacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "BroadcastPacketsSent", pStats->BroadcastPacketsSent);
    amxc_var_add_key(uint32_t, pRetMap, "BroadcastPacketsReceived", pStats->BroadcastPacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "UnknownProtoPacketsReceived", pStats->UnknownProtoPacketsReceived);
    amxc_var_add_key(uint32_t, pRetMap, "FailedRetransCount", pStats->FailedRetransCount);
    amxc_var_add_key(uint32_t, pRetMap, "RetryCount", pStats->RetryCount);
    amxc_var_add_key(uint32_t, pRetMap, "MultipleRetryCount", pStats->MultipleRetryCount);
}

void wld_rad_notifyPublicAction(T_Radio* pRad, swl_macChar_t* macStr, swl_oui_t oui, uint8_t type, uint8_t subtype, uint8_t* data) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);

    amxc_var_t notifMap;
    amxc_var_init(&notifMap);
    amxc_var_set_type(&notifMap, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, &notifMap, "MACAddress", macStr->cMac);
    char ouiStr[SWL_OUI_STR_LEN] = {0};
    swl_hex_fromBytesSep(ouiStr, SWL_OUI_STR_LEN, oui.ouiBytes, SWL_OUI_BYTE_LEN, true, ':', NULL);
    amxc_var_add_key(cstring_t, &notifMap, "OUI", ouiStr);
    amxc_var_add_key(uint8_t, &notifMap, "Type", type);
    amxc_var_add_key(uint8_t, &notifMap, "SubType", subtype);
    amxc_var_add_key(cstring_t, &notifMap, "Data", (char*) data);
    amxc_var_add_key(amxc_htable_t, &map, "Vendor", amxc_var_get_amxc_htable_t(&notifMap));
    amxc_var_clean(&notifMap);

    amxd_object_trigger_signal(pRad->pBus, "PublicAction", &map);
    amxc_var_clean(&map);
}

amxd_object_t* wld_rad_getObject(T_Radio* pRad) {
    return pRad->pBus;
}



amxd_status_t _Radio_getStatusHistogram(amxd_object_t* object,
                                        amxd_function_t* func _UNUSED,
                                        amxc_var_t* args _UNUSED,
                                        amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = object->priv;
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_updateStatusChangeInfo(&pRad->changeInfo, pRad->status);
    swl_type_toVariant((swl_type_t*) &gtWld_status_changeInfo, retval, &pRad->changeInfo);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _Radio_debug(amxd_object_t* object,
                           amxd_function_t* func _UNUSED,
                           amxc_var_t* args,
                           amxc_var_t* retval) {

    T_Radio* pR = object->priv;

    const char* feature = GET_CHAR(args, "op");
    ASSERT_NOT_NULL(feature, amxd_status_unknown_error, ME, "No argument given");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    if(!strcasecmp(feature, "RssiMon")) {
        wld_radStaMon_debug(pR, retval);
    } else if(!strcasecmp(feature, "DriverConfig")) {
        amxc_var_add_key(int32_t, retval, "TxBurst", pR->driverCfg.txBurst);
        amxc_var_add_key(int32_t, retval, "Amsdu", pR->driverCfg.amsdu);
        amxc_var_add_key(int32_t, retval, "Ampdu", pR->driverCfg.ampdu);
        amxc_var_add_key(int32_t, retval, "FragThreshold", pR->driverCfg.fragThreshold);
        amxc_var_add_key(int32_t, retval, "RtsThreshold", pR->driverCfg.rtsThreshold);
        amxc_var_add_key(int32_t, retval, "TxBeamforming", pR->driverCfg.txBeamforming);
        amxc_var_add_key(bool, retval, "VhtOmnEnabled", pR->driverCfg.vhtOmnEnabled);
        amxc_var_add_key(int32_t, retval, "BroadcastMaxBwCapability", pR->driverCfg.broadcastMaxBwCapability);
        amxc_var_add_key(uint32_t, retval, "TPCMode", pR->driverCfg.tpcMode);
    } else if(!strcasecmp(feature, "MacConfig")) {
        amxc_var_add_key(bool, retval, "UseBaseMacOffset", pR->macCfg.useBaseMacOffset);
        amxc_var_add_key(bool, retval, "UseLocalBitForGuest", pR->macCfg.useLocalBitForGuest);
        amxc_var_add_key(int64_t, retval, "BaseMacOffset", pR->macCfg.baseMacOffset);
        amxc_var_add_key(int64_t, retval, "LocalGuestMacOffset", pR->macCfg.localGuestMacOffset);
        amxc_var_add_key(uint32_t, retval, "NrBssRequired", pR->macCfg.nrBssRequired);
    } else if(!strcasecmp(feature, "DelayCommit")) {
        if(pR->pFA->mfn_wrad_fsm_delay_commit) {
            pR->pFA->mfn_wrad_fsm_delay_commit(pR);
        } else {
            amxc_var_add_key(cstring_t, retval, "Error", "No delay commit function");
        }
    } else if(!strcasecmp(feature, "hapdCfg")) {
        char tmpName[128];
        snprintf(tmpName, sizeof(tmpName), "%s-%s.tmp.txt", "/tmp/hostapd", pR->Name);
        wld_hostapd_cfgFile_create(pR, tmpName);
    } else if(!strcasecmp(feature, "nl80211IfaceInfo")) {
        wld_nl80211_ifaceInfo_t ifaceInfo;
        if(wld_rad_nl80211_getInterfaceInfo(pR, &ifaceInfo) < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retval, "Error", "Fail to get nl80211 iface info");
        } else {
            wld_nl80211_dumpIfaceInfo(&ifaceInfo, retval);
        }
    } else if(!strcasecmp(feature, "nl80211WiphyInfo")) {
        wld_nl80211_wiphyInfo_t wiphyInfo;
        if(wld_rad_nl80211_getWiphyInfo(pR, &wiphyInfo) < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retval, "Error", "Fail to get nl80211 wiphy info");
        } else {
            wld_nl80211_dumpWiphyInfo(&wiphyInfo, retval);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "RadioLinuxStats")) {

        T_Stats radioStats;
        memset(&radioStats, 0, sizeof(radioStats));

        if(wld_linuxIfStats_getRadioStats(pR, &radioStats)) {
            s_setStats(retval, &radioStats);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "AllEpLinuxStats")) {

        T_Stats epStats;
        memset(&epStats, 0, sizeof(epStats));

        if(wld_linuxIfStats_getAllEpStats(pR, &epStats)) {
            s_setStats(retval, &epStats);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "AllVapLinuxStats")) {

        T_Stats vapStats;
        memset(&vapStats, 0, sizeof(vapStats));

        if(wld_linuxIfStats_getAllVapStats(pR, &vapStats)) {
            s_setStats(retval, &vapStats);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211SetAntennas")) {
        uint32_t txMapAnt = GET_UINT32(args, "txMapAnt");
        uint32_t rxMapAnt = GET_UINT32(args, "rxMapAnt");
        swl_rc_ne ret = wld_rad_nl80211_setAntennas(pR, txMapAnt, rxMapAnt);
        amxc_var_add_key(cstring_t, retval, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211SetTxPowerAuto")) {
        swl_rc_ne ret = wld_rad_nl80211_setTxPowerAuto(pR);
        amxc_var_add_key(cstring_t, retval, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211SetTxPowerFixed")) {
        int32_t mbm = GET_INT32(args, "mbm");
        swl_rc_ne ret = wld_rad_nl80211_setTxPowerFixed(pR, mbm);
        amxc_var_add_key(cstring_t, retval, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211SetTxPowerLimited")) {
        int32_t mbm = GET_INT32(args, "mbm");
        swl_rc_ne ret = wld_rad_nl80211_setTxPowerLimited(pR, mbm);
        amxc_var_add_key(cstring_t, retval, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211GetTxPower")) {
        int32_t dbm;
        swl_rc_ne ret = wld_rad_nl80211_getTxPower(pR, &dbm);
        amxc_var_add_key(int32_t, retval, "txPwr", dbm);
        amxc_var_add_key(cstring_t, retval, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "getFreq")) {
        amxc_var_add_key(uint32_t, retval, "freq", wld_rad_getCurrentFreq(pR));
    } else {
        amxc_var_add_key(cstring_t, retval, "Error", "Unknown command");
    }
    return amxd_status_ok;
}
