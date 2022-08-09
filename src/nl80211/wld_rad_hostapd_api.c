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
#include "wld_channel.h"
#include "wld_radio.h"
#include "wld_rad_hostapd_api.h"
#include "wld_hostapd_ap_api.h"
#include "wld_hostapd_cfgFile.h"

#define ME "hapdRad"

/** @brief update the the Operating Standard in the hostapd
 *
 * @param pRad radio
 * @return - SECDMN_ACTION_OK_NEED_SIGHUP when Operating Standard is updated in the hostapd config.
 *         - Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_rad_hostapd_setOperatingStandard(T_Radio* pRadio) {
    ASSERTS_NOT_NULL(pRadio, SECDMN_ACTION_ERROR, ME, "NULL");

    // overwrite the current hostapd config file
    wld_hostapd_cfgFile_createExt(pRadio);

    return SECDMN_ACTION_OK_NEED_SIGHUP;
}

/** @brief calculate the sec_channel_offset parameter of CHAN_SWITCH hostapd command
 * the function code is inspired from hostapd_ctrl_check_freq_params function in hostapd
 *
 * @param bandwidth bandwidht 20/40/80/160MHz
 * @param freq frequency
 * @param center_freq center frequency
 * @param sec_channel_offset a parameter in the CHAN_SWITCH command
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
static swl_rc_ne s_calculateSecChannelOffset(swl_bandwidth_e bandwidth, uint32_t freq, uint32_t center_freq, int* sec_channel_offset) {
    swl_rc_ne ret = SWL_RC_OK;

    switch(bandwidth) {
    case SWL_BW_AUTO:
    case SWL_BW_20MHZ:
        *sec_channel_offset = 0;
        break;
    case SWL_BW_40MHZ:
        if(freq + 10 == center_freq) {
            *sec_channel_offset = 1;
        } else if(freq - 10 == center_freq) {
            *sec_channel_offset = -1;
        } else {
            ret = SWL_RC_ERROR;
        }
        break;
    case SWL_BW_80MHZ:
        if((freq - 10 == center_freq) || (freq + 30 == center_freq)) {
            *sec_channel_offset = 1;
        } else if((freq + 10 == center_freq) || (freq - 30 == center_freq)) {
            *sec_channel_offset = -1;
        } else {
            ret = SWL_RC_ERROR;
        }
        break;
    case SWL_BW_160MHZ:
        if((freq + 70 == center_freq) || (freq + 30 == center_freq) ||
           (freq - 10 == center_freq) || (freq - 50 == center_freq)) {
            *sec_channel_offset = 1;
        } else if((freq + 50 == center_freq) || (freq + 10 == center_freq) ||
                  (freq - 30 == center_freq) || (freq - 70 == center_freq)) {
            *sec_channel_offset = -1;
        } else {
            ret = SWL_RC_ERROR;
        }
        break;
    default:
        ret = SWL_RC_ERROR;
        break;
    }
    return ret;
}
/**
 * @brief switch the channel
 *
 * @param pRad radio
 *
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR or SWL_RC_INVALID_PARAM
 */
swl_rc_ne wld_rad_hostapd_switchChannel(T_Radio* pR) {
    T_AccessPoint* primaryVap = wld_rad_firstAp(pR);
    ASSERT_NOT_NULL(primaryVap, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_chanspec_t chanspec = {
        .channel = pR->channel,
        .bandwidth = pR->runningChannelBandwidth,
        .band = pR->operatingFrequencyBand
    };

    uint32_t freq;
    uint32_t center_freq;

    swl_rc_ne res = swl_chanspec_channelToMHz(&chanspec, &freq);
    ASSERT_TRUE(res == SWL_RC_OK, SWL_RC_ERROR, ME, "Invalid band");

    int32_t bw = swl_chanspec_bwToInt(chanspec.bandwidth);

    swl_channel_t center_channel = swl_chanspec_getCentreChannel(&chanspec);
    chanspec.channel = center_channel;

    res = swl_chanspec_channelToMHz(&chanspec, &center_freq);
    ASSERT_TRUE(res == SWL_RC_OK, SWL_RC_ERROR, ME, "Invalid band");

    // Extension Channel
    int sec_channel_offset = 0;
    res = s_calculateSecChannelOffset(chanspec.bandwidth, freq, center_freq, &sec_channel_offset);
    ASSERT_TRUE(res == SWL_RC_OK, SWL_RC_ERROR, ME, "invalid frequencies");

    char cmd[256] = {'\0'};
    swl_str_catFormat(cmd, sizeof(cmd), "CHAN_SWITCH");
    swl_strlst_catFormat(cmd, sizeof(cmd), " ", "5"); // beacons count before switch
    swl_strlst_catFormat(cmd, sizeof(cmd), " ", "%d", freq);

    swl_strlst_catFormat(cmd, sizeof(cmd), " ", "bandwidth=%d", bw);
    if(bw > 20) {
        swl_strlst_catFormat(cmd, sizeof(cmd), " ", "center_freq1=%d", center_freq);
        swl_strlst_catFormat(cmd, sizeof(cmd), " ", "sec_channel_offset=%d", sec_channel_offset);
    }

    // standard_mode
    swl_radioStandard_m operStd = pR->operatingStandards;
    if(SWL_BIT_IS_ONLY_SET(operStd, SWL_RADSTD_AUTO)) {
        operStd = pR->supportedStandards;
    }
    if(SWL_BIT_IS_SET(operStd, SWL_RADSTD_N)) {
        swl_strlst_catFormat(cmd, sizeof(cmd), " ", "ht");
    }
    //hostapd complains when setting vht cap in chan_switch with bw 20MHz
    if((bw > 20) && (SWL_BIT_IS_SET(operStd, SWL_RADSTD_AC))) {
        swl_strlst_catFormat(cmd, sizeof(cmd), " ", "vht");
    }
    if(SWL_BIT_IS_SET(operStd, SWL_RADSTD_AX)) {
        swl_strlst_catFormat(cmd, sizeof(cmd), " ", "he");
    }

    //send command
    bool ret = wld_ap_hostapd_sendCommand(primaryVap, cmd, "switch channel");
    ASSERTS_TRUE(ret, SWL_RC_ERROR, ME, "NULL");
    return SWL_RC_OK;
}

/**
 * @brief reload the hostapd
 *
 * @param pR radio context
 * @return SWL_RC_OK when the Hostapd config is reloaded. Otherwise error code.
 */
swl_rc_ne wld_rad_hostapd_reload(T_Radio* pR) {
    T_AccessPoint* primaryVap = wld_rad_firstAp(pR);
    bool ret = wld_ap_hostapd_sendCommand(primaryVap, "RELOAD", "reload conf");
    ASSERTS_TRUE(ret, SWL_RC_ERROR, ME, "%s: reload fail", pR->Name);
    return SWL_RC_OK;
}

/**
 * @brief set main interface channel parameters
 *
 * @param pR radio context
 *
 * @return SECDMN_ACTION_OK_NEED_TOGGLE when the Hostapd config is set. Otherwise SECDMN_ACTION_ERROR.
 */
wld_secDmn_action_rc_ne wld_rad_hostapd_setChannel(T_Radio* pR) {
    T_AccessPoint* primaryVap = wld_rad_firstAp(pR);
    ASSERT_NOT_NULL(primaryVap, SECDMN_ACTION_ERROR, ME, "NULL");
    swl_mapChar_t radParams;
    swl_mapChar_init(&radParams);
    wld_hostapd_cfgFile_setRadioConfig(pR, &radParams);
    const char* chanParams[] = {
        "channel", "ht_capab",
        "vht_capab", "vht_oper_centr_freq_seg0_idx", "vht_oper_chwidth",
        "he_oper_centr_freq_seg0_idx", "he_oper_chwidth",
    };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(chanParams); i++) {
        wld_ap_hostapd_setParamValue(primaryVap, chanParams[i], swl_mapChar_get(&radParams, (char*) chanParams[i]), "");
    }
    swl_mapChar_cleanup(&radParams);
    return SECDMN_ACTION_OK_NEED_TOGGLE;
}

/**
 * @brief enable main hostapd interface
 *
 * @param pR radio context
 *
 * @return SWL_RC_OK when the enabling cmd is sent. Otherwise error code.
 */
swl_rc_ne wld_rad_hostapd_enable(T_Radio* pR) {
    T_AccessPoint* primaryVap = wld_rad_firstAp(pR);
    ASSERT_NOT_NULL(primaryVap, SWL_RC_INVALID_PARAM, ME, "NULL");
    bool ret = wld_ap_hostapd_sendCommand(primaryVap, "ENABLE", "enableHapd");
    ASSERTS_TRUE(ret, SWL_RC_ERROR, ME, "%s: enabling failed", primaryVap->alias);
    return SWL_RC_OK;
}

/**
 * @brief disable main hostapd interface
 *
 * @param pR radio context
 *
 * @return SWL_RC_OK when the the disabling cmd is sent. Otherwise error code.
 */
swl_rc_ne wld_rad_hostapd_disable(T_Radio* pR) {
    T_AccessPoint* primaryVap = wld_rad_firstAp(pR);
    ASSERT_NOT_NULL(primaryVap, SWL_RC_INVALID_PARAM, ME, "NULL");
    bool ret = wld_ap_hostapd_sendCommand(primaryVap, "DISABLE", "disableHapd");
    ASSERTS_TRUE(ret, SWL_RC_ERROR, ME, "%s: disabling failed", primaryVap->alias);
    return SWL_RC_OK;
}

