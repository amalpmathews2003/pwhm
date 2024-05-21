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
#include "wld_nl80211_utils.h"
#include "swl/swl_common.h"
#include "swl/swl_common_time.h"
#include "wld_radio.h"
#include "wld_util.h"

#define ME "nlRad"

swl_rc_ne wld_rad_nl80211_setEvtListener(T_Radio* pRadio, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = wld_nl80211_delEvtListener(&pRadio->nl80211Listener);
    if((rc == SWL_RC_OK) && (handlers == NULL)) {
        return rc;
    }
    uint32_t wiphy = pRadio->wiphy;
    ASSERTI_NOT_EQUALS(wiphy, WLD_NL80211_ID_UNDEF, SWL_RC_INVALID_PARAM, ME, "%s: undefined wiphy id", pRadio->Name);
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
        swl_macBin_t* pMac = NULL;
        if((pAP->pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->MACAddress))) {
            pMac = (swl_macBin_t*) pAP->pSSID->MACAddress;
        }
        rc = wld_nl80211_newInterface(wld_nl80211_getSharedState(), pRadio->index, pAP->alias, pMac, true, false, &ifaceInfo);
    }
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add VAP(%s) on radio(%s)", pAP->alias, pRadio->Name);
    pAP->index = ifaceInfo.ifIndex;
    pAP->wDevId = ifaceInfo.wDevId;
    return rc;
}

swl_rc_ne wld_rad_nl80211_addEpInterface(T_Radio* pRadio, T_EndPoint* pEP) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    ASSERT_NOT_NULL(pEP, rc, ME, "NULL");
    ASSERT_NOT_EQUALS(pEP->alias[0], 0, rc, ME, "Empty alias");

    wld_nl80211_ifaceInfo_t ifaceInfo;
    if(pEP->index > 0) {
        rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pEP->index, &ifaceInfo);
        if((rc < SWL_RC_OK) || (!swl_str_matches(pEP->alias, ifaceInfo.name))) {
            SAH_TRACEZ_ERROR(ME, "unmatched ep(%s) ifIndex(%d)", pEP->alias, pEP->index);
            return SWL_RC_ERROR;
        }
        return SWL_RC_OK;
    }
    rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pRadio->index, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get radio(%s) main iface info", pRadio->Name);
    if(swl_str_matches(ifaceInfo.name, pEP->alias)) {
        rc = wld_rad_nl80211_setSta(pRadio);
    } else {
        swl_macBin_t* pMac = NULL;
        if((pEP->pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pEP->pSSID->MACAddress))) {
            pMac = (swl_macBin_t*) pEP->pSSID->MACAddress;
        }
        rc = wld_nl80211_newInterface(wld_nl80211_getSharedState(), pRadio->index, pEP->Name, pMac, false, true, &ifaceInfo);
    }
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add EP(%s) on radio(%s)", pEP->alias, pRadio->Name);
    pEP->index = ifaceInfo.ifIndex;
    pEP->wDevId = ifaceInfo.wDevId;
    return rc;
}

uint8_t wld_rad_nl80211_addRadios(vendor_t* vendor,
                                  const uint32_t maxWiphys, const uint32_t maxWlIfaces,
                                  wld_nl80211_ifaceInfo_t wlIfacesInfo[maxWiphys][maxWlIfaces]) {
    uint8_t index = 0;
    ASSERT_NOT_NULL(vendor, index, ME, "NULL");
    for(uint32_t i = 0; i < maxWlIfaces && index < maxWiphys; i++) {
        for(uint32_t j = 0; j < maxWiphys && index < maxWiphys; j++) {
            wld_nl80211_ifaceInfo_t* pIface = &wlIfacesInfo[j][i];
            if(pIface->ifIndex <= 0) {
                continue;
            }
            T_Radio* pRad = wld_rad_get_radio(pIface->name);
            if(pRad == NULL) {
                SAH_TRACEZ_WARNING(ME, "Interface %s handled by %s", pIface->name, vendor->name);
                wld_addRadio(pIface->name, vendor, -1);
            } else {
                SAH_TRACEZ_WARNING(ME, "Interface %s already handled by %s", pIface->name, pRad->vendor->name);
            }
            index++;
        }
    }
    ASSERTW_NOT_EQUALS(index, 0, index, ME, "NO nl80211 Wireless interfaces found");
    return index;
}

swl_rc_ne wld_rad_nl80211_getWiphyInfo(T_Radio* pRadio, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getWiphyInfo(wld_nl80211_getSharedState(), pRadio->index, pWiphyInfo);
}

swl_rc_ne wld_rad_nl80211_getSurveyInfo(T_Radio* pRadio, wld_nl80211_channelSurveyInfo_t** ppChanSurveyInfo, uint32_t* pnrChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t ifIndex = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    return wld_nl80211_getSurveyInfo(wld_nl80211_getSharedState(), ifIndex, ppChanSurveyInfo, pnrChanSurveyInfo);
}

/*
 * lowest period since last air stats calculation in ms
 * to avoid too small diff counters
 */
#define MIN_AIR_STATS_REFRESH_PERIOD_MS 100
swl_rc_ne wld_rad_nl80211_getAirStatsFromSurveyInfo(T_Radio* pRadio, wld_airStats_t* pStats, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSurveyInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!pChanSurveyInfo->inUse) {
        return SWL_RC_CONTINUE;
    }
    if((pChanSurveyInfo->timeOn == 0) ||
       (pChanSurveyInfo->timeBusy > pChanSurveyInfo->timeOn)) {
        SAH_TRACEZ_ERROR(ME, "\t%s: Center Freq %dMHz: wrong time activity values",
                         pRadio->Name, pChanSurveyInfo->frequencyMHz);
        return SWL_RC_ERROR;
    }

    swl_timeSpecMono_t nowTs;
    swl_timespec_getMono(&nowTs);
    uint32_t nowTsMsU32 = swl_timespec_toMs(&nowTs);

    if(pRadio->pLastAirStats == NULL) {
        pRadio->pLastAirStats = calloc(1, sizeof(wld_airStats_t));
        if(pRadio->pLastAirStats == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: fail to alloc air stats cache", pRadio->Name);
            return SWL_RC_ERROR;
        }
    } else if((nowTsMsU32 - pRadio->pLastAirStats->timestamp) <= MIN_AIR_STATS_REFRESH_PERIOD_MS) {
        SAH_TRACEZ_INFO(ME, "%s: too frequent polling: return cached air stats", pRadio->Name);
        memcpy(pStats, pRadio->pLastAirStats, sizeof(*pStats));
        return SWL_RC_OK;
    }

    /*
     * cache last active chan survey info, per radio
     * as nl80211 survey info include cumulative time values
     * => for a proper runtime channel load calculation,
     * we need to diff with last meas before
     */
    if(pRadio->pLastSurvey == NULL) {
        pRadio->pLastSurvey = calloc(1, sizeof(wld_nl80211_channelSurveyInfo_t));
        if(pRadio->pLastSurvey == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: fail to alloc survey cache", pRadio->Name);
            return SWL_RC_ERROR;
        }
    }
    wld_nl80211_channelSurveyInfo_t* pLastActiveChanSurveyInfo = pRadio->pLastSurvey;
    wld_nl80211_channelSurveyInfo_t currChanSurveyInfo;
    memcpy(&currChanSurveyInfo, pChanSurveyInfo, sizeof(*pChanSurveyInfo));
    if((pLastActiveChanSurveyInfo->frequencyMHz == currChanSurveyInfo.frequencyMHz) &&
       (pLastActiveChanSurveyInfo->timeOn > 0) &&
       (pLastActiveChanSurveyInfo->timeOn < currChanSurveyInfo.timeOn)) {
        currChanSurveyInfo.timeOn -= pLastActiveChanSurveyInfo->timeOn;
        currChanSurveyInfo.timeBusy -= pLastActiveChanSurveyInfo->timeBusy;
        currChanSurveyInfo.timeExtBusy -= pLastActiveChanSurveyInfo->timeExtBusy;
        currChanSurveyInfo.timeRx -= pLastActiveChanSurveyInfo->timeRx;
        currChanSurveyInfo.timeTx -= pLastActiveChanSurveyInfo->timeTx;
        currChanSurveyInfo.timeScan -= pLastActiveChanSurveyInfo->timeScan;
        currChanSurveyInfo.timeRxInBss -= pLastActiveChanSurveyInfo->timeRxInBss;
    }
    memcpy(pLastActiveChanSurveyInfo, pChanSurveyInfo, sizeof(*pChanSurveyInfo));

    pChanSurveyInfo = &currChanSurveyInfo;
    pStats->timestamp = nowTsMsU32;
    pStats->noise = pChanSurveyInfo->noiseDbm;
    //use ratio, when timeOn value is beyond total_time variable max type value
    uint64_t base = SWL_MIN(pChanSurveyInfo->timeOn, SWL_BIT_SHIFT(SWL_BIT_SIZE(pStats->total_time)) - 1);

    pStats->total_time = base; // eqv. all pChanSurveyInfo->timeOn;
    pStats->bss_transmit_time = (pChanSurveyInfo->timeTx * base) / pChanSurveyInfo->timeOn;
    pStats->bss_receive_time = (pChanSurveyInfo->timeRx * base) / pChanSurveyInfo->timeOn;
    if(pChanSurveyInfo->timeRxInBss > 0) {
        pStats->other_bss_time = ((pChanSurveyInfo->timeRx - pChanSurveyInfo->timeRxInBss) * base) / pChanSurveyInfo->timeOn;
    }
    uint64_t wifiTime = pChanSurveyInfo->timeTx + pChanSurveyInfo->timeRx + pChanSurveyInfo->timeScan;
    if(pChanSurveyInfo->timeBusy > wifiTime) {
        pStats->other_time = ((pChanSurveyInfo->timeBusy - wifiTime) * base) / pChanSurveyInfo->timeOn;
    }
    pStats->free_time = ((pChanSurveyInfo->timeOn - pChanSurveyInfo->timeBusy) * base) / pChanSurveyInfo->timeOn;
    pStats->load = SWL_MAX((bool) (pChanSurveyInfo->timeBusy > 0), SWL_MIN((pChanSurveyInfo->timeBusy * 100) / pChanSurveyInfo->timeOn, 100U));
    memcpy(pRadio->pLastAirStats, pStats, sizeof(*pStats));
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_getAirstats(T_Radio* pRadio, wld_airStats_t* pStats) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");

    memset(pStats, 0, sizeof(*pStats));
    uint32_t nChanSurveyInfo = 0;
    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList = NULL;
    swl_rc_ne retVal = wld_rad_nl80211_getSurveyInfo(pRadio, &pChanSurveyInfoList, &nChanSurveyInfo);
    if(retVal < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to get survey info info", pRadio->Name);
        free(pChanSurveyInfoList);
        return retVal;
    }

    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo;
    for(uint32_t id = 0; id < nChanSurveyInfo; id++) {
        pChanSurveyInfo = &pChanSurveyInfoList[id];
        retVal = wld_rad_nl80211_getAirStatsFromSurveyInfo(pRadio, pStats, pChanSurveyInfo);
        if(retVal <= SWL_RC_OK) {
            break;
        }
    }
    free(pChanSurveyInfoList);
    return retVal;
}

swl_rc_ne wld_rad_nl80211_updateUsageStatsFromSurveyInfo(T_Radio* pRadio, amxc_llist_t* pOutSpectrumResults,
                                                         wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pOutSpectrumResults, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSurveyInfoList, SWL_RC_OK, ME, "Empty");

    for(uint32_t i = 0; i < nChanSurveyInfo; i++) {
        wld_spectrumChannelInfoEntry_t chanInfo;
        memset(&chanInfo, 0, sizeof(chanInfo));
        swl_chanspec_t chanSpec;
        wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo = &pChanSurveyInfoList[i];
        if(swl_chanspec_channelFromMHz(&chanSpec, pChanSurveyInfo->frequencyMHz) < SWL_RC_OK) {
            continue;
        }

        chanInfo.channel = chanSpec.channel;
        chanInfo.bandwidth = SWL_BW_20MHZ;
        chanInfo.noiselevel = pChanSurveyInfo->noiseDbm;
        wld_airStats_t airStats;
        memset(&airStats, 0, sizeof(airStats));
        if(!pChanSurveyInfo->inUse) {
            if((pChanSurveyInfo->timeOn > 0) && (pChanSurveyInfo->timeBusy <= pChanSurveyInfo->timeOn)) {
                chanInfo.availability = SWL_MIN((((pChanSurveyInfo->timeOn - pChanSurveyInfo->timeBusy) * 100) / pChanSurveyInfo->timeOn), 100LLU);
            }
        } else if(wld_rad_nl80211_getAirStatsFromSurveyInfo(pRadio, &airStats, pChanSurveyInfo) == SWL_RC_OK) {
            if(airStats.total_time > 0) {
                uint64_t ourTime = airStats.bss_receive_time + airStats.bss_transmit_time + airStats.other_bss_time;
                chanInfo.ourUsage = SWL_MAX((bool) (ourTime > 0), SWL_MIN(((ourTime * 100) / airStats.total_time), 100U));
                chanInfo.availability = SWL_MIN(((airStats.free_time * 100) / airStats.total_time), 100);
            }
        }

        wld_util_addorUpdateSpectrumEntry(pOutSpectrumResults, &chanInfo);
    }

    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_setAntennas(T_Radio* pRadio, uint32_t txMapAnt, uint32_t rxMapAnt) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setWiphyAntennas(wld_nl80211_getSharedState(), pRadio->index, txMapAnt, rxMapAnt);
}

swl_rc_ne wld_rad_nl80211_setTxPowerAuto(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerAuto(wld_nl80211_getSharedState(), pRadio->index);
}

swl_rc_ne wld_rad_nl80211_setTxPowerFixed(T_Radio* pRadio, int32_t dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerFixed(wld_nl80211_getSharedState(), pRadio->index, dbm);
}

swl_rc_ne wld_rad_nl80211_setTxPowerLimited(T_Radio* pRadio, int32_t dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerLimited(wld_nl80211_getSharedState(), pRadio->index, dbm);
}

swl_rc_ne wld_rad_nl80211_getTxPower(T_Radio* pRadio, int32_t* dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getTxPower(wld_nl80211_getSharedState(), pRadio->index, dbm);
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

static void s_setScanDuration(T_Radio* pRadio, wld_nl80211_scanParams_t* params) {
    ASSERTS_NOT_NULL(params, , ME, "NULL");
    params->measDuration = 0;
    params->measDurationMandatory = false;
    ASSERTS_NOT_NULL(pRadio, , ME, "NULL");
    wld_scan_config_t* pCfg = &pRadio->scanState.cfg;
    bool suppScanDwell = pRadio->pFA->mfn_misc_has_support(pRadio, NULL, "SCAN_DWELL", 0);
    ASSERTI_TRUE(suppScanDwell, , ME, "%s: nl80211 driver does not support setting scan dwell", pRadio->Name);

    //consider passive scan for freqBand 6GHz
    if((pRadio->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (swl_unLiList_size(&params->ssids) > 0)) {
        if((pCfg->activeChannelTime >= SCAN_ACTIVE_DWELL_MIN) && (pCfg->activeChannelTime <= SCAN_ACTIVE_DWELL_MAX)) {
            params->measDuration = pCfg->activeChannelTime;
            params->measDurationMandatory = true;
        } else if(pCfg->activeChannelTime == -1) {
            params->measDuration = SCAN_ACTIVE_DWELL_DEFAULT;
        }
    } else if((pCfg->passiveChannelTime >= SCAN_PASSIVE_DWELL_MIN) && (pCfg->passiveChannelTime <= SCAN_PASSIVE_DWELL_MAX)) {
        params->measDuration = pCfg->passiveChannelTime;
        params->measDurationMandatory = true;
    } else if(pCfg->passiveChannelTime == -1) {
        params->measDuration = SCAN_PASSIVE_DWELL_DEFAULT;
    }
}

swl_rc_ne wld_rad_nl80211_startScanExt(T_Radio* pRadio, wld_nl80211_scanFlags_t* pFlags) {
    wld_scanArgs_t* args = &pRadio->scanState.cfg.scanArguments;
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    wld_nl80211_scanParams_t params;
    memset(&params, 0, sizeof(wld_nl80211_scanParams_t));
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
            swl_chanspec_t chanspec = swl_chanspec_fromDm(args->chanlist[i],
                                                          pRadio->operatingChannelBandwidth,
                                                          pRadio->operatingFrequencyBand);
            uint32_t freq = wld_channel_getFrequencyOfChannel(chanspec);
            swl_unLiList_add(&params.freqs, &freq);
        }
        if(args->ssid[0] && (args->ssidLen > 0)) {
            char* ssid = args->ssid;
            swl_unLiList_add(&params.ssids, &ssid);
        }
        if(swl_mac_binIsNull(&args->bssid) == false) {
            memcpy(&params.bssid, &args->bssid, SWL_MAC_BIN_LEN);
        }
    }
    if(pFlags != NULL) {
        memcpy(&params.flags, pFlags, sizeof(wld_nl80211_scanFlags_t));
    }
    s_setScanDuration(pRadio, &params);

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

swl_rc_ne wld_rad_nl80211_startScan(T_Radio* pRadio) {
    wld_nl80211_scanFlags_t flags = {.flush = true};
    return wld_rad_nl80211_startScanExt(pRadio, &flags);
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

struct getScanResultsData_s {
    T_Radio* pRadio;
    scanResultsCb_f fScanResultsCb;
    void* priv;
};
static void s_scanResultsCb(void* priv, swl_rc_ne rc, wld_scanResults_t* results) {
    struct getScanResultsData_s* pScanResultsData = (struct getScanResultsData_s*) priv;
    ASSERTS_NOT_NULL(pScanResultsData, , ME, "NULL");
    if(pScanResultsData->fScanResultsCb != NULL) {
        pScanResultsData->fScanResultsCb(pScanResultsData->priv, rc, results);
    }
    free(pScanResultsData);
}
swl_rc_ne wld_rad_nl80211_getScanResults(T_Radio* pRadio, void* priv, scanResultsCb_f fScanResultsCb) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    struct getScanResultsData_s* pScanResultsData = calloc(1, sizeof(struct getScanResultsData_s));
    if(pScanResultsData == NULL) {
        SAH_TRACEZ_ERROR(ME, "Fail to alloc getScanResults req data");
        rc = SWL_RC_ERROR;
        if(fScanResultsCb) {
            fScanResultsCb(priv, rc, NULL);
        }
        return rc;
    }
    pScanResultsData->pRadio = pRadio;
    pScanResultsData->fScanResultsCb = fScanResultsCb;
    pScanResultsData->priv = priv;
    return wld_nl80211_getScanResults(wld_nl80211_getSharedState(), pRadio->index, pScanResultsData, s_scanResultsCb);
}

swl_rc_ne wld_rad_nl80211_setRegDomain(T_Radio* pRadio, const char* alpha2) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: setting reg domain %s", pRadio->Name, alpha2);
    rc = wld_nl80211_setRegDomain(wld_nl80211_getSharedState(), pRadio->wiphy, alpha2);
    return rc;
}

swl_rc_ne wld_rad_nl80211_bgDfsStart(T_Radio* pRadio, wld_startBgdfsArgs_t* args) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(args, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Starting BG_DFS %u/%s",
                    pRadio->Name, args->channel, swl_bandwidth_str[args->bandwidth]);

    swl_chanspec_t bgDfsChanspec = SWL_CHANSPEC_NEW(args->channel, args->bandwidth, pRadio->operatingFrequencyBand);
    uint32_t index = wld_rad_getFirstEnabledIfaceIndex(pRadio);

    return wld_nl80211_bgDfsStart(wld_nl80211_getSharedState(), index, MLO_LINK_ID_UNKNOWN, bgDfsChanspec);
}

swl_rc_ne wld_rad_nl80211_bgDfsStop(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Stopping BG_DFS", pRadio->Name);

    uint32_t index = wld_rad_getFirstEnabledIfaceIndex(pRadio);

    return wld_nl80211_bgDfsStop(wld_nl80211_getSharedState(), index, MLO_LINK_ID_UNKNOWN);
}

swl_rc_ne wld_rad_nl80211_registerFrame(T_Radio* pRadio, uint16_t type, const char* pattern, size_t patternLen) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    uint32_t ifIndex = wld_rad_getFirstActiveIfaceIndex(pRadio);
    return wld_nl80211_registerFrame(wld_nl80211_getSharedState(), ifIndex, type, pattern, patternLen);
}

swl_rc_ne wld_rad_nl80211_sendVendorSubCmd(T_Radio* pRadio, uint32_t oui, int subcmd, void* data, int dataLen,
                                           bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmd(wld_nl80211_getSharedState(), oui, subcmd, data, dataLen, isSync, withAck,
                                      flags, pRadio->index, pRadio->wDevId, handler, priv);

    return rc;
}

swl_rc_ne wld_rad_nl80211_sendVendorSubCmdAttr(T_Radio* pRadio, uint32_t oui, int subcmd, wld_nl80211_nlAttr_t* vendorAttr,
                                               bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmdAttr(wld_nl80211_getSharedState(), oui, subcmd, vendorAttr, isSync, withAck,
                                          flags, pRadio->index, pRadio->wDevId, handler, priv);

    return rc;
}
