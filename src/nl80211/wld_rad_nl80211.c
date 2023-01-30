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
/*
 * This file implements wrapper functions to use nl80211 generic apis with T_Radio context
 */

#include "wld_rad_nl80211.h"
#include "swl/swl_common.h"
#include "wld_radio.h"

#define ME "nlRad"

swl_rc_ne wld_rad_nl80211_setEvtListener(T_Radio* pRadio, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = wld_nl80211_delEvtListener(&pRadio->nl80211Listener);
    if((rc == SWL_RC_OK) && (handlers == NULL)) {
        return rc;
    }
    uint32_t wiphy = pRadio->ref_index;
    uint32_t ifIndex = WLD_NL80211_ID_ANY;
    SAH_TRACEZ_INFO(ME, "rad(%s): add evt listener wiphy(%d)/ifIndex(%d)", pRadio->Name, wiphy, ifIndex);
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    pRadio->nl80211Listener = wld_nl80211_addEvtListener(state, wiphy, ifIndex, pRadio, pData, handlers);
    ASSERT_NOT_NULL(pRadio->nl80211Listener, SWL_RC_ERROR,
                    ME, "rad(%s): fail to add evt listener wiphy(%d)/ifIndex(%d)", pRadio->Name, wiphy, ifIndex);
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_delEvtListener(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_delEvtListener(&pRadio->nl80211Listener);
}

swl_rc_ne wld_rad_nl80211_getInterfaceInfo(T_Radio* pRadio, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pRadio->index, pIfaceInfo);
}

swl_rc_ne wld_rad_nl80211_setAp(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceType(wld_nl80211_getSharedState(), pRadio->index, true, false);
}

swl_rc_ne wld_rad_nl80211_setSta(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceType(wld_nl80211_getSharedState(), pRadio->index, false, true);
}

swl_rc_ne wld_rad_nl80211_set4Mac(T_Radio* pRadio, bool use4Mac) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceUse4Mac(wld_nl80211_getSharedState(), pRadio->index, use4Mac);
}

swl_rc_ne wld_rad_nl80211_addVapInterface(T_Radio* pRadio, T_AccessPoint* pAP) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");
    ASSERT_NOT_EQUALS(pAP->alias[0], 0, rc, ME, "Empty alias");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    if(pAP->index > 0) {
        rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pAP->index, &ifaceInfo);
        if((rc < SWL_RC_OK) || (!swl_str_matches(pAP->alias, ifaceInfo.name))) {
            SAH_TRACEZ_ERROR(ME, "unmatched vap(%s) ifIndex(%d)", pAP->alias, pAP->index);
            return SWL_RC_ERROR;
        }
        return SWL_RC_OK;

    }
    rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pRadio->index, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get radio(%s) main iface info", pRadio->Name);
    if(swl_str_matches(ifaceInfo.name, pAP->alias)) {
        rc = wld_rad_nl80211_setAp(pRadio);
    } else {
        rc = wld_nl80211_newInterface(wld_nl80211_getSharedState(), pRadio->index, pAP->alias, NULL, true, false, &ifaceInfo);
    }
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add VAP(%s) on radio(%s)", pAP->alias, pRadio->Name);
    pAP->index = ifaceInfo.ifIndex;
    return rc;
}

swl_rc_ne wld_rad_nl80211_getWiphyInfo(T_Radio* pRadio, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getWiphyInfo(wld_nl80211_getSharedState(), pRadio->index, pWiphyInfo);
}

swl_rc_ne wld_rad_nl80211_getNoise(T_Radio* pRadio, int32_t* noise) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getNoise(wld_nl80211_getSharedState(), pRadio->index, noise);
}
swl_rc_ne wld_rad_nl80211_setAntennas(T_Radio* pRadio, uint32_t txMapAnt, uint32_t rxMapAnt) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setWiphyAntennas(wld_nl80211_getSharedState(), pRadio->index, txMapAnt, rxMapAnt);
}

swl_rc_ne wld_rad_nl80211_setTxPowerAuto(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerAuto(wld_nl80211_getSharedState(), pRadio->index);
}

swl_rc_ne wld_rad_nl80211_setTxPowerFixed(T_Radio* pRadio, int32_t mbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerFixed(wld_nl80211_getSharedState(), pRadio->index, mbm);
}

swl_rc_ne wld_rad_nl80211_setTxPowerLimited(T_Radio* pRadio, int32_t mbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerLimited(wld_nl80211_getSharedState(), pRadio->index, mbm);
}

swl_rc_ne wld_rad_nl80211_getTxPower(T_Radio* pRadio, int32_t* mbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getTxPower(wld_nl80211_getSharedState(), pRadio->index, mbm);
}

swl_rc_ne wld_rad_nl80211_getChanSpecFromIfaceInfo(swl_chanspec_t* pChanSpec, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pIfaceInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = swl_chanspec_channelFromMHz(pChanSpec, pIfaceInfo->chanSpec.ctrlFreq);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get channel");
    pChanSpec->bandwidth = swl_chanspec_intToBw(pIfaceInfo->chanSpec.chanWidth);
    return SWL_RC_OK;
}

static swl_rc_ne s_getChanSpec(int ifIndex, swl_chanspec_t* pChanSpec) {
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(ifIndex > 0, SWL_RC_INVALID_PARAM, ME, "invalid ifindex");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_rc_ne rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &ifaceInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "no iface info");
    rc = wld_rad_nl80211_getChanSpecFromIfaceInfo(pChanSpec, &ifaceInfo);
    return rc;
}

swl_rc_ne wld_rad_nl80211_getChannel(T_Radio* pRadio, swl_chanspec_t* pChanSpec) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc;
    if((rc = s_getChanSpec(pRadio->index, pChanSpec)) >= SWL_RC_OK) {
        return rc;
    }
    /*
     * disabled main iface (usually matching radio iface) may not carry channel info
     * so fetch first vap iface having channel info
     */
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRadio) {
        if((rc = s_getChanSpec(pAP->index, pChanSpec)) >= SWL_RC_OK) {
            return rc;
        }
    }
    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, pRadio) {
        if((rc = s_getChanSpec(pEP->index, pChanSpec)) >= SWL_RC_OK) {
            return rc;
        }
    }
    return rc;
}

swl_rc_ne wld_rad_nl80211_startScan(T_Radio* pRadio, T_ScanArgs* args) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    wld_nl80211_scanParams_t params;
    swl_unLiList_init(&params.ssids, sizeof(char*));
    swl_unLiList_init(&params.freqs, sizeof(uint32_t));
    params.iesLen = 0;
    if(args) {
        int i;
        for(i = 0; i < args->chanCount; i++) {
            if(!wld_rad_hasChannel(pRadio, args->chanlist[i])) {
                SAH_TRACEZ_ERROR(ME, "rad(%s) does not support scan chan(%d)",
                                 pRadio->Name, args->chanlist[i]);
                goto scan_error;
            }
            swl_chanspec_t chanspec = {
                .band = pRadio->operatingFrequencyBand,
                .bandwidth = pRadio->operatingChannelBandwidth,
                .channel = args->chanlist[i],
            };
            chanspec.band = pRadio->operatingFrequencyBand;
            chanspec.bandwidth = pRadio->operatingChannelBandwidth;
            uint32_t freq = wld_channel_getFrequencyOfChannel(chanspec);
            swl_unLiList_add(&params.freqs, &freq);
        }
        if(args->ssid[0] && (args->ssidLen > 0)) {
            char* ssid = args->ssid;
            swl_unLiList_add(&params.ssids, &ssid);
        }
        if(swl_mac_binIsNull(&args->bssid) == false) {
            params.bssid = args->bssid;
        }
    }
    /*
     * start_scan command has to be sent to enabled interface (UP)
     * (even when secondary VAP, while primary is disabled)
     */
    int index = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    rc = wld_nl80211_startScan(state, index, &params);
scan_error:
    swl_unLiList_destroy(&params.ssids);
    swl_unLiList_destroy(&params.freqs);
    return rc;
}

swl_rc_ne wld_rad_nl80211_abortScan(T_Radio* pRadio) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    /*
     * abort_scan command has to be sent to enabled interface (UP)
     * (even when secondary VAP, while primary is disabled)
     */
    int index = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    return wld_nl80211_abortScan(wld_nl80211_getSharedState(), index);
}

swl_rc_ne wld_rad_nl80211_getScanResults(T_Radio* pRadio, void* priv, scanResultsCb_f fScanResultsCb) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    return wld_nl80211_getScanResults(wld_nl80211_getSharedState(), pRadio->index, priv, fScanResultsCb);
}

