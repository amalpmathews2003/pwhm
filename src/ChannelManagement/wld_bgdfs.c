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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "wld.h"
#include "wld_util.h"
#include "swl/swl_assert.h"
#include "wld_radio.h"
#include "wld_channel_types.h"
#include "wld_chanmgt.h"
#include "wld_bgdfs.h"
#include "wld_channel.h"

#define ME "wld"

const char* wld_bgdfsStatus_str[BGDFS_STATUS_MAX] = {
    "Off",
    "Idle",
    "Clearing",
    "ExtClearing"
};

const char* wld_dfsResult_str[DFS_RESULT_MAX] = {
    "Success",
    "Radar",
    "Fail"
};

static void wld_bgdfs_writeChannel(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pBus, , ME, "NULL");

    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    amxd_object_t* bgDfsObject = amxd_object_findf(chanObject, "BgDfs");
    amxd_object_set_int32_t(bgDfsObject, "Channel", pRad->bgdfs_config.channel);
    amxd_object_set_cstring_t(bgDfsObject, "Bandwidth", Rad_SupBW[pRad->bgdfs_config.bandwidth]);
    amxd_object_set_cstring_t(bgDfsObject, "Status", wld_bgdfsStatus_str[pRad->bgdfs_config.status]);
}

static void wld_bgdfs_writeStatistics(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pBus, , ME, "NULL");

    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    amxd_object_t* bgDfsObject = amxd_object_findf(chanObject, "BgDfs");
    amxd_object_set_int32_t(bgDfsObject, "NrClearSuccess", pRad->bgdfs_stats.nrClearSuccess);
    amxd_object_set_int32_t(bgDfsObject, "NrClearFailRadar", pRad->bgdfs_stats.nrClearFailRadar);
    amxd_object_set_int32_t(bgDfsObject, "NrClearFailOther", pRad->bgdfs_stats.nrClearFailOther);
    amxd_object_set_int32_t(bgDfsObject, "NrClearSuccessExt", pRad->bgdfs_stats.nrClearSuccessExt);
    amxd_object_set_int32_t(bgDfsObject, "NrClearFailRadarExt", pRad->bgdfs_stats.nrClearFailRadarExt);
    amxd_object_set_int32_t(bgDfsObject, "NrClearFailOtherExt", pRad->bgdfs_stats.nrClearFailOtherExt);
}


void wld_bgdfs_notifyClearStarted(T_Radio* pRad, swl_channel_t channel, swl_bandwidth_e bandwidth, bool externalClear) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");


    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(channel, bandwidth, pRad->operatingFrequencyBand);
    pRad->bgdfs_config.channel = channel;
    pRad->bgdfs_config.bandwidth = bandwidth;
    pRad->bgdfs_config.status = externalClear ? BGDFS_STATUS_CLEAR_EXT : BGDFS_STATUS_CLEAR;
    pRad->bgdfs_config.clearStartTime = swl_timespec_getMonoVal();
    pRad->bgdfs_config.estimatedClearTime = wld_channel_get_band_clear_time(chanspec);

    SAH_TRACEZ_WARNING(ME, "%s : startClear %u/%u current %u/%u, dur %u, ext %u", pRad->Name,
                       channel, swl_bandwidth_int[bandwidth],
                       pRad->channel, swl_bandwidth_int[pRad->runningChannelBandwidth],
                       pRad->bgdfs_config.estimatedClearTime, externalClear);
    wld_bgdfs_writeChannel(pRad);
}

bool wld_bgdfs_isRunning(T_Radio* pRad) {
    return (pRad->bgdfs_config.status != BGDFS_STATUS_IDLE) && (pRad->bgdfs_config.status != BGDFS_STATUS_OFF);
}

static void s_sendNotification(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pBus, , ME, "NULL");

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    amxc_var_t* my_map = amxc_var_add_key(amxc_htable_t, &params, "Updates", NULL);

    amxc_var_add_key(uint32_t, my_map, "StartChannel", pRad->channel);
    amxc_var_add_key(cstring_t, my_map, "StartBandwidth", swl_bandwidth_str[pRad->runningChannelBandwidth]);
    amxc_var_add_key(uint32_t, my_map, "ClearChannel", pRad->bgdfs_config.channel);
    amxc_var_add_key(cstring_t, my_map, "ClearBandwidth", swl_bandwidth_str[pRad->bgdfs_config.bandwidth]);
    amxc_var_add_key(cstring_t, my_map, "Result", wld_dfsResult_str[pRad->bgdfs_config.lastResult]);
    int64_t clearTime = swl_timespec_diffToMillisec(&pRad->bgdfs_config.clearStartTime, &pRad->bgdfs_config.clearEndTime);
    amxc_var_add_key(uint64_t, my_map, "RunTime", clearTime);

    amxc_var_add_key(cstring_t, my_map, "Mode", pRad->bgdfs_config.status == BGDFS_STATUS_CLEAR_EXT ?
                     "Provider" : "Background");
    amxd_object_trigger_signal(pRad->pBus, "DFS Done", &params);

    SAH_TRACEZ_INFO(ME, "Send DFS done notification");

    amxc_var_clean(&params);
}

uint32_t wld_bgdfs_clearTimeEllapsed(T_Radio* pRad) {
    ASSERTS_TRUE(wld_bgdfs_isRunning(pRad), 0, ME, "INVALID");
    swl_timeSpecMono_t now = swl_timespec_getMonoVal();
    return (uint32_t) swl_timespec_diffToSec(&pRad->bgdfs_config.clearStartTime, &now);
}

void wld_bgdfs_notifyClearEnded(T_Radio* pRad,
                                wld_dfsResult_e result) {

    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    pRad->bgdfs_config.clearEndTime = swl_timespec_getMonoVal();
    pRad->bgdfs_config.lastResult = result;

    SAH_TRACEZ_WARNING(ME, "%s : end clear %u / %u : %u after %" PRId64 " ms",
                       pRad->Name, pRad->bgdfs_config.channel, swl_bandwidth_int[pRad->bgdfs_config.bandwidth],
                       result,
                       swl_timespec_diffToMillisec(&pRad->bgdfs_config.clearStartTime, &pRad->bgdfs_config.clearEndTime));
    bool isExternal = (pRad->bgdfs_config.status == BGDFS_STATUS_CLEAR_EXT);
    if(result == DFS_RESULT_OK) {
        if(isExternal) {
            pRad->bgdfs_stats.nrClearSuccessExt++;
        } else {
            pRad->bgdfs_stats.nrClearSuccess++;
        }
    } else if(result == DFS_RESULT_RADAR) {
        if(isExternal) {
            pRad->bgdfs_stats.nrClearFailRadarExt++;
        } else {
            pRad->bgdfs_stats.nrClearFailRadar++;
        }
    } else {
        if(isExternal) {
            pRad->bgdfs_stats.nrClearFailOtherExt++;
        } else {
            pRad->bgdfs_stats.nrClearFailOther++;
        }
    }
    wld_bgdfs_writeStatistics(pRad);

    s_sendNotification(pRad);

    pRad->bgdfs_config.channel = 0;
    pRad->bgdfs_config.bandwidth = SWL_BW_AUTO;
    bool running = pRad->bgdfs_config.enable && pRad->bgdfs_config.available;
    pRad->bgdfs_config.status = running ? BGDFS_STATUS_IDLE : BGDFS_STATUS_OFF;
    wld_bgdfs_writeChannel(pRad);
}

void wld_bgdfs_setAvailable(T_Radio* pRad, bool available) {
    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    amxd_object_t* bgDfsObject = amxd_object_findf(chanObject, "BgDfs");
    pRad->bgdfs_config.available = available;
    amxd_object_set_bool(bgDfsObject, "Available", available);
    if(!pRad->bgdfs_config.available && wld_bgdfs_isRunning(pRad)) {
        pRad->pFA->mfn_wrad_bgdfs_stop(pRad);
        SAH_TRACEZ_ERROR(ME, "%s : end bgdfs due to available end", pRad->Name);
    }

    pRad->bgdfs_config.status = pRad->bgdfs_config.available ? BGDFS_STATUS_IDLE : BGDFS_STATUS_OFF;
    amxd_object_set_cstring_t(bgDfsObject, "Status", wld_bgdfsStatus_str[pRad->bgdfs_config.status]);
}

swl_rc_ne wld_bgdfs_startExt(T_Radio* pRad, wld_startBgdfsArgs_t* args) {
    bool legacy = false;
    swl_rc_ne ret = pRad->pFA->mfn_wrad_bgdfs_start_ext(pRad, args);
    if(ret < 0) {
        if(ret == SWL_RC_NOT_IMPLEMENTED) {
            // if not implemented, try normal one
            ret = pRad->pFA->mfn_wrad_bgdfs_start(pRad, args->channel);
            legacy = true;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s : Starting BG_DFS %u/%s legacy %u",
                    pRad->Name, args->channel, swl_bandwidth_str[args->bandwidth], legacy);
    return ret;
}


static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    bool enabled = amxc_var_dyncast(bool, newValue);
    ASSERTI_NOT_EQUALS(pR->bgdfs_config.enable, enabled, , ME, "EQUALS");

    if(wld_bgdfs_isRunning(pR)) {
        wld_bgdfs_notifyClearEnded(pR, DFS_RESULT_OTHER);
    }

    pR->bgdfs_config.enable = enabled;

    SAH_TRACEZ_INFO(ME,
                    "%s: BgDfs preclear enable changed to %s",
                    pR->Name,
                    pR->bgdfs_config.enable ? "true" : "false");

    SAH_TRACEZ_OUT(ME);
}

static void s_setBgdfsProvider_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    pR->bgdfs_config.useProvider = amxc_var_dyncast(bool, newValue);

    SAH_TRACEZ_INFO(ME,
                    "%s: BgDfs useProvider changed to %s",
                    pR->Name,
                    pR->bgdfs_config.useProvider ? "true" : "false");

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sRadBgDfsDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf),
                  SWLA_DM_PARAM_HDLR("AllowProvider", s_setBgdfsProvider_pwf),
                  ));

void _wld_radBgDfs_setConf_ocf(const char* const sig_name,
                               const amxc_var_t* const data,
                               void* const priv _UNUSED) {
    swla_dm_procObjEvtOfLocalDm(&sRadBgDfsDmHdlrs, sig_name, data, priv);
}

amxd_status_t bgdfs_startClear(T_Radio* pR,
                               uint32_t bandwidth,
                               swl_channel_t channel,
                               wld_startBgdfsArgs_t* dfsArgs) {
    ASSERTI_NOT_NULL(pR, amxd_status_unknown_error, ME, "INVALID");

    if(!pR->bgdfs_config.enable || !pR->bgdfs_config.available || (pR->bgdfs_config.channel != 0)) {
        SAH_TRACEZ_ERROR(ME, "%s config error %d %d %d", pR->Name,
                         pR->bgdfs_config.enable, pR->bgdfs_config.available, pR->bgdfs_config.channel);
        return amxd_status_invalid_function_argument;
    }

    dfsArgs->channel = channel;
    /**
     * bandwidth is also set to 0 if the argument is not provided
     */
    if(bandwidth == 0) {
        dfsArgs->bandwidth = SWL_BW_AUTO;
    } else {
        dfsArgs->bandwidth = wld_channel_getBandwidthEnumFromVal(bandwidth);
    }
    SAH_TRACEZ_INFO(ME, "Dfs arguments: channel= %i, bandwidth= %i", channel, bandwidth);

    ASSERT_CMD_SUCCESS(wld_bgdfs_startExt(pR, dfsArgs),
                       amxd_status_unknown_error, ME, "%s start bgdfs error %i", pR->Name, _errNo);
    return amxd_status_ok;
}

amxd_status_t bgdfs_stopClear(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, amxd_status_unknown_error, ME, "INVALID");

    if(!pRad->bgdfs_config.enable || !pRad->bgdfs_config.available || (pRad->bgdfs_config.channel == 0)) {
        SAH_TRACEZ_ERROR(ME, "%s config error %d %d %d", pRad->Name,
                         pRad->bgdfs_config.enable, pRad->bgdfs_config.available, pRad->bgdfs_config.channel);
        return amxd_status_invalid_function_argument;
    }

    ASSERT_CMD_SUCCESS(pRad->pFA->mfn_wrad_bgdfs_stop(pRad),
                       amxd_status_unknown_error, ME, "%s stop bgdfs error %i", pRad->Name, _errNo);

    return amxd_status_ok;
}


amxd_status_t _startBgDfsClear(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "No mapped radio ctx");

    swl_channel_t channel = (swl_channel_t) GET_UINT32(args, "channel");
    uint32_t bandwidth = GET_UINT32(args, "bandwidth");
    wld_startBgdfsArgs_t dfsArgs;

    memset(&dfsArgs, 0, sizeof(wld_startBgdfsArgs_t));

    SAH_TRACEZ_INFO(ME, "startBgDfsClear called with channel: %i, bandwidth: %i", channel, bandwidth);
    amxd_status_t status = bgdfs_startClear(pR, bandwidth, channel, &dfsArgs);

    SAH_TRACEZ_OUT(ME);

    return status;
}

amxd_status_t _stopBgDfsClear(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args _UNUSED,
                              amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "No mapped radio ctx");

    amxd_status_t status = bgdfs_stopClear(pR);

    SAH_TRACEZ_OUT(ME);

    return status;
}
