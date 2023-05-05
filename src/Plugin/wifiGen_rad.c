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

#include <net/if.h>
#include "swl/swl_common.h"
#include "swla/swla_chanspec.h"
#include "wld/wld_radio.h"
#include "wld/wld_channel.h"
#include "wld/wld_chanmgt.h"
#include "wld/wld_util.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_linuxIfStats.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wifiGen_rad.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"

#define ME "genHapd"



SWL_TABLE(sPowerTable,
          ARR(int8_t powPct; int32_t powReduction; ),
          ARR(swl_type_int8, swl_type_int32, ),
          ARR(
              {6, 12},
              {12, 9},
              {25, 6},
              {50, 3},
              {100, 0},
              {-1, 0},
              ));

static const char* s_defaultRegDomain = "DE";

int wifiGen_rad_miscHasSupport(T_Radio* pRad, T_AccessPoint* pAp, char* buf, int bufsize) {
    ASSERTS_NOT_NULL(buf, 0, ME, "NULL");
    uint32_t band = 0;
    if((pRad != NULL) || ((pAp != NULL) && ((pRad = pAp->pRadio) != NULL))) {
        band = pRad->operatingFrequencyBand;
    }
    if(!buf[0]) {
        /* Get full list */
        swl_str_copy(buf, bufsize, wld_rad_getSuppDrvCaps(pRad, band));
        return 1;
    }
    return wld_rad_findSuppDrvCap(pRad, band, buf);
}

int wifiGen_rad_createHook(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->wlRadio_SK = wld_linuxIfUtils_getNetSock();
    if(pRad->wlRadio_SK < 0) {
        SAH_TRACE_ERROR("Unable to create socket");
        return SWL_RC_ERROR;
    }
    wifiGen_hapd_init(pRad);
    wifiGen_setRadEvtHandlers(pRad);
    return SWL_RC_OK;
}

void wifiGen_rad_destroyHook(T_Radio* pRad) {
    wifiGen_hapd_cleanup(pRad);
    wld_rad_nl80211_delEvtListener(pRad);
    free(pRad->pLastSurvey);
    if(pRad->wlRadio_SK > 0) {
        close(pRad->wlRadio_SK);
        pRad->wlRadio_SK = -1;
    }
}

static void s_updateNrAntenna(T_Radio* pRad, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    if(pWiphyInfo->nSSMax > 0) {
        pRad->nrAntenna[COM_DIR_TRANSMIT] = pRad->nrAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nSSMax;
    } else {
        pRad->nrAntenna[COM_DIR_TRANSMIT] = pWiphyInfo->nrAntenna[COM_DIR_TRANSMIT];
        pRad->nrAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nrAntenna[COM_DIR_RECEIVE];
    }
    pRad->nrActiveAntenna[COM_DIR_TRANSMIT] = pWiphyInfo->nrActiveAntenna[COM_DIR_TRANSMIT];
    pRad->nrActiveAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nrActiveAntenna[COM_DIR_RECEIVE];
}

#define SUITE(oui, id)  (((oui) << 8) | (id))
/* cipher suite selectors */
#define WLAN_CIPHER_SUITE_USE_GROUP     SUITE(0x000FAC, 0)
#define WLAN_CIPHER_SUITE_WEP40         SUITE(0x000FAC, 1)
#define WLAN_CIPHER_SUITE_TKIP          SUITE(0x000FAC, 2)
/* reserved:                            SUITE(0x000FAC, 3) */
#define WLAN_CIPHER_SUITE_CCMP          SUITE(0x000FAC, 4)
#define WLAN_CIPHER_SUITE_WEP104        SUITE(0x000FAC, 5)
#define WLAN_CIPHER_SUITE_AES_CMAC      SUITE(0x000FAC, 6)
#define WLAN_CIPHER_SUITE_GCMP          SUITE(0x000FAC, 8)
#define WLAN_CIPHER_SUITE_GCMP_256      SUITE(0x000FAC, 9)
#define WLAN_CIPHER_SUITE_CCMP_256      SUITE(0x000FAC, 10)
#define WLAN_CIPHER_SUITE_BIP_GMAC_128  SUITE(0x000FAC, 11)
#define WLAN_CIPHER_SUITE_BIP_GMAC_256  SUITE(0x000FAC, 12)
#define WLAN_CIPHER_SUITE_BIP_CMAC_256  SUITE(0x000FAC, 13)
SWL_TABLE(sCiphersMap,
          ARR(uint32_t cipherSuite; char* cipherName; ),
          ARR(swl_type_uint32, swl_type_charPtr, ),
          ARR({WLAN_CIPHER_SUITE_WEP40, "WEP"},
              {WLAN_CIPHER_SUITE_WEP104, "WEP"},
              {WLAN_CIPHER_SUITE_TKIP, "TKIP"},
              {WLAN_CIPHER_SUITE_CCMP, "AES"},
              {WLAN_CIPHER_SUITE_AES_CMAC, "AES_CCM"},
              {WLAN_CIPHER_SUITE_GCMP, "SAE"},
              {WLAN_CIPHER_SUITE_GCMP_256, "SAE"},
              ));

static void s_initialiseCapabilities(T_Radio* pRad, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "init cap %s", pRad->Name);
    wld_rad_init_cap(pRad);

    ASSERT_NOT_NULL(pWiphyInfo, , ME, "NULL");
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        wld_nl80211_bandDef_t* pBand = &pWiphyInfo->bands[i];
        if(pBand->nChans > 0) {
            SAH_TRACEZ_INFO(ME, "process band(%d) nchan(%d)", pBand->freqBand, pBand->nChans);
            if(SWL_BIT_IS_SET(pBand->chanWidthMask, SWL_BW_160MHZ)) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "160MHz");
            }
            if(pWiphyInfo->suppUAPSD) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "UAPSD");
            }
            for(uint32_t j = 0; j < pWiphyInfo->nCipherSuites; j++) {
                uint32_t cipherSuite = pWiphyInfo->cipherSuites[j];
                if((pBand->freqBand == SWL_FREQ_BAND_6GHZ) && (cipherSuite < WLAN_CIPHER_SUITE_GCMP)) {
                    continue;
                }
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, (char*) swl_table_getMatchingValue(&sCiphersMap, 1, 0, &cipherSuite));
            }
            if(((pBand->freqBand == SWL_FREQ_BAND_5GHZ) &&
                (pBand->bfCapsSupported[COM_DIR_TRANSMIT] & (M_RAD_BF_CAP_VHT_SU | M_RAD_BF_CAP_VHT_MU))) ||
               (pBand->bfCapsSupported[COM_DIR_TRANSMIT] & (M_RAD_BF_CAP_HE_SU | M_RAD_BF_CAP_HE_MU))) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "EXPL_BF");
            }
            if(((pBand->freqBand == SWL_FREQ_BAND_5GHZ) &&
                (pBand->bfCapsSupported[COM_DIR_RECEIVE] & (M_RAD_BF_CAP_VHT_SU | M_RAD_BF_CAP_VHT_MU))) ||
               (pBand->bfCapsSupported[COM_DIR_RECEIVE] & (M_RAD_BF_CAP_HE_SU | M_RAD_BF_CAP_HE_MU))) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "IMPL_BF");
            }
            if(pBand->bfCapsSupported[COM_DIR_TRANSMIT] & (M_RAD_BF_CAP_VHT_MU | M_RAD_BF_CAP_HE_MU)) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "MU_MIMO");
            }
            if(pWiphyInfo->suppFeatures.dfsOffload) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "DFS_OFFLOAD");
            }
            if(pWiphyInfo->suppFeatures.sae) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SAE");
            }
            if(pWiphyInfo->suppFeatures.sae_pwe) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SAE_PWE");
            }
            if(pWiphyInfo->suppCmds.channelSwitch) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "CSA");
            }
            if(pWiphyInfo->suppCmds.survey) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SURVEY");
            }
            if(pWiphyInfo->suppCmds.WMMCapability) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "WME");
            }
            if(pWiphyInfo->suppFeatures.scanDwell) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SCAN_DWELL");
            }
            SAH_TRACEZ_INFO(ME, "%s: Caps[%s]={%s}", pRad->Name, swl_freqBand_str[pBand->freqBand], wld_rad_getSuppDrvCaps(pRad, pBand->freqBand));
        }
    }
    SAH_TRACEZ_OUT(ME);
}

static void s_updateBandAndStandard(T_Radio* pRad, wld_nl80211_bandDef_t bands[], wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pRad->supportedFrequencyBands = M_SWL_FREQ_BAND_EXT_NONE;
    pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_NONE;
    pRad->supportedStandards = 0;
    pRad->runningChannelBandwidth = SWL_BW_AUTO;
    pRad->maxChannelBandwidth = SWL_BW_AUTO;
    ASSERT_NOT_NULL(bands, , ME, "NULL");
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        wld_nl80211_bandDef_t* pBand = &bands[i];
        if(pBand->nChans > 0) {
            W_SWL_BIT_CLEAR(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_NONE);
            W_SWL_BIT_SET(pRad->supportedFrequencyBands, pBand->freqBand);
            pRad->supportedStandards |= pBand->radStdsMask;
        }
    }
    if(pIfaceInfo) {
        swl_chanspec_t chanSpec;
        SAH_TRACEZ_INFO(ME, "convert freq:%d", pIfaceInfo->chanSpec.ctrlFreq);
        if(swl_chanspec_channelFromMHz(&chanSpec, pIfaceInfo->chanSpec.ctrlFreq) >= SWL_RC_OK) {
            SAH_TRACEZ_INFO(ME, "get band:%d chan:%d", chanSpec.band, chanSpec.channel);
            pRad->channel = chanSpec.channel;
            pRad->operatingFrequencyBand = chanSpec.band;
            W_SWL_BIT_CLEAR(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_NONE);
            W_SWL_BIT_SET(pRad->supportedFrequencyBands, pRad->operatingFrequencyBand);
        }
        if(pIfaceInfo->chanSpec.chanWidth > 0) {
            pRad->runningChannelBandwidth = swl_chanspec_intToBw(pIfaceInfo->chanSpec.chanWidth);
        }
    }
    if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_NONE) {
        if(pRad->supportedFrequencyBands < M_SWL_FREQ_BAND_EXT_NONE) {
            pRad->operatingFrequencyBand = swl_bit32_getHighest(pRad->supportedFrequencyBands);
        } else {
            pRad->operatingFrequencyBand = (pRad->nrAntenna[COM_DIR_TRANSMIT] > 2) ? SWL_FREQ_BAND_EXT_5GHZ : SWL_FREQ_BAND_EXT_2_4GHZ;
            pRad->supportedFrequencyBands = SWL_BIT_SHIFT(pRad->operatingFrequencyBand);
        }
    }
    if(pRad->supportedStandards == 0) {
        if(SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_2_4GHZ)) {
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_B);
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_G);
        }
        if(SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_5GHZ)) {
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_A);
        }
    }
    wld_nl80211_bandDef_t* pOperBand = &bands[pRad->operatingFrequencyBand];
    if(pOperBand->chanWidthMask > 0) {
        pRad->maxChannelBandwidth = swl_bit32_getHighest(pOperBand->chanWidthMask);
        if(pRad->runningChannelBandwidth == SWL_BW_AUTO) {
            pRad->runningChannelBandwidth = pRad->maxChannelBandwidth;
        }
    } else if(pRad->runningChannelBandwidth == SWL_BW_AUTO) {
        switch(pRad->operatingFrequencyBand) {
        case SWL_FREQ_BAND_EXT_5GHZ:
            pRad->runningChannelBandwidth = SWL_BW_80MHZ;
            break;
        case SWL_FREQ_BAND_EXT_6GHZ:
            pRad->runningChannelBandwidth = SWL_BW_160MHZ;
            break;
        default:
            pRad->runningChannelBandwidth = SWL_BW_20MHZ;
            break;
        }
        pRad->maxChannelBandwidth = pRad->runningChannelBandwidth;
    }
    pRad->IEEE80211hSupported = (pRad->supportedFrequencyBands & (M_SWL_FREQ_BAND_5GHZ | M_SWL_FREQ_BAND_6GHZ)) ? 1 : 0;
    pRad->IEEE80211kSupported = true;
    pRad->IEEE80211rSupported = true;
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX)) {
        pRad->heCapsSupported = M_HE_CAP_DL_OFDMA | M_HE_CAP_UL_OFDMA | M_HE_CAP_DL_MUMIMO;
    }
    //updating capabilities
    pRad->htCapabilities = pOperBand->htCapabilities;
    pRad->vhtCapabilities = pOperBand->vhtCapabilities;
    pRad->hePhyCapabilities = pOperBand->hePhyCapabilities;

}

void s_readChanInfo(T_Radio* pRad, wld_nl80211_bandDef_t* pOperBand) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_clear_flags(pRad);
    ASSERT_NOT_NULL(pOperBand, , ME, "NULL");
    for(uint32_t i = 0; i < pOperBand->nChans; i++) {
        wld_nl80211_chanDesc_t* pChan = &pOperBand->chans[i];
        swl_chanspec_t chanSpec;
        if(swl_chanspec_channelFromMHz(&chanSpec, pChan->ctrlFreq) < SWL_RC_OK) {
            continue;
        }
        wld_channel_mark_available_channel(chanSpec);
        if(pChan->isDfs) {
            wld_channel_mark_radar_req_channel(chanSpec);
        }
        switch(pChan->status) {
        case WLD_NL80211_CHAN_AVAILABLE: wld_channel_clear_passive_channel(chanSpec); break;
        case WLD_NL80211_CHAN_USABLE: wld_channel_mark_passive_channel(chanSpec); break;
        case WLD_NL80211_CHAN_UNAVAILABLE: wld_channel_mark_radar_detected_channel(chanSpec); break;
        default: break;
        }
    }
}

swl_rc_ne s_updateChannels(T_Radio* pRad, wld_nl80211_bandDef_t* pOperBand) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pOperBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->nrPossibleChannels = 0;
    for(uint32_t i = 0; i < pOperBand->nChans; i++) {
        wld_nl80211_chanDesc_t* pChan = &pOperBand->chans[i];
        swl_chanspec_t chanSpec;
        if((swl_chanspec_channelFromMHz(&chanSpec, pChan->ctrlFreq) < SWL_RC_OK) ||
           (chanSpec.channel == 0)) {
            SAH_TRACEZ_WARNING(ME, "%s: skip unknown freq %d", pRad->Name, pChan->ctrlFreq);
            continue;
        }
        if(pRad->nrPossibleChannels == (int) SWL_ARRAY_SIZE(pRad->possibleChannels)) {
            SAH_TRACEZ_WARNING(ME, "%s: skip saving supported chan %d freq %d (out of limit)",
                               pRad->Name, chanSpec.channel, pChan->ctrlFreq);
            continue;
        }
        pRad->possibleChannels[pRad->nrPossibleChannels] = chanSpec.channel;
        pRad->nrPossibleChannels++;
    }
    wld_channel_init_channels(pRad);
    s_readChanInfo(pRad, pOperBand);
    return SWL_RC_OK;
}

int wifiGen_rad_poschans(T_Radio* pRad, uint8_t* buf _UNUSED, int bufsize _UNUSED) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(pRad->operatingFrequencyBand < (uint32_t) SWL_FREQ_BAND_MAX, SWL_RC_INVALID_PARAM, ME, "Invalid");
    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc = wld_rad_nl80211_getWiphyInfo(pRad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[pRad->operatingFrequencyBand];
    rc = s_updateChannels(pRad, pOperBand);
    wld_rad_write_possible_channels(pRad);
    wld_chanmgt_writeDfsChannels(pRad);
    return rc;
}

int wifiGen_rad_supports(T_Radio* pRad, char* buf _UNUSED, int bufsize _UNUSED) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->index = if_nametoindex(pRad->Name);
    pRad->runningChannelBandwidth = SWL_BW_AUTO;
    pRad->operatingStandards = M_SWL_RADSTD_AUTO;
    /* Fill in all our RO fields... */
    pRad->autoChannelSupported = 1;
    pRad->enable = 1;            // By default it's on
    pRad->status = RST_UNKNOWN;  // It's hard to track odl is settings his default here.
    pRad->detailedState = CM_RAD_UNKNOWN;
    pRad->maxBitRate = 54;

    //toggle to force loading nl80211 wiphy caps
    int radState = wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pRad->Name);
    if(radState == 0) {
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 1);
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 0);
    }

    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc = wld_rad_nl80211_getWiphyInfo(pRad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_rad_nl80211_getInterfaceInfo(pRad, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 rad iface info");
    pRad->wiphy = ifaceInfo.wiphy;
    pRad->wDevId = ifaceInfo.wDevId;
    wifiGen_setRadEvtHandlers(pRad);

    pRad->isAP = wiphyInfo.suppAp;
    if(pRad->isAP) {
        if((pRad->maxNrHwBss = wiphyInfo.nApMax) == 0) {
            pRad->maxNrHwBss = MAXNROF_ACCESSPOINT;
        }
    }
    //memcpy(pRad->MACAddr, ifaceInfo.mac.bMac, ETHER_ADDR_LEN);

    s_updateNrAntenna(pRad, &wiphyInfo);
    s_updateBandAndStandard(pRad, wiphyInfo.bands, &ifaceInfo);

    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[pRad->operatingFrequencyBand];
    pRad->bfCapsSupported[COM_DIR_TRANSMIT] = pOperBand->bfCapsSupported[COM_DIR_TRANSMIT];
    pRad->bfCapsSupported[COM_DIR_RECEIVE] = pOperBand->bfCapsSupported[COM_DIR_RECEIVE];
    s_initialiseCapabilities(pRad, &wiphyInfo);
    SAH_TRACEZ_INFO(ME, "%s: band:%d, chan:%d", pRad->Name, pRad->operatingFrequencyBand, pRad->channel);

    //rad->channelInUse;      /* 32 Bit pattern that will mark all channels? */
    if(pRad->channel == 0) {
        pRad->channel = swl_channel_defaults[wld_rad_getFreqBand(pRad)];
    }
    s_updateChannels(pRad, pOperBand);

    pRad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    pRad->channelChangeReason = CHAN_REASON_INITIAL;
    pRad->autoChannelSupported = TRUE;
    pRad->autoChannelEnable = FALSE;
    pRad->autoChannelRefreshPeriod = FALSE;
    pRad->extensionChannel = REXT_AUTO;          /* Auto */
    pRad->guardInterval = SWL_SGI_AUTO;          /* Auto == 400NSec*/
    pRad->MCS = 10;
    if(pOperBand->radStdsMask & M_SWL_RADSTD_AX) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_HE].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_AC) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_VHT].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_N) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_HT].mcsIndex;
    }

    swl_table_columnToArrayOffset(pRad->transmitPowerSupported, 64, &sPowerTable, 0, 0);

    pRad->transmitPower = 100;
    pRad->setRadio80211hEnable = (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ || pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) ? TRUE : FALSE;
    swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain), s_defaultRegDomain);
    getCountryParam(pRad->regulatoryDomain, 0, &pRad->regulatoryDomainIdx);
    pRad->m_multiAPTypesSupported = M_MULTIAP_ALL;

    /* First time force full config */
    pRad->fsmRad.FSM_SyncAll = TRUE;

    /* Enable WPS 2.0 -- Do this only when you know that WPS 2.0 is supported & needed! */
    pRad->wpsConst->wpsSupVer = 2;

    SAH_TRACEZ_OUT(ME);
    return SWL_RC_OK;
}

int wifiGen_rad_status(T_Radio* pRad) {
    swl_chanspec_t currChanSpec;
    if(!wld_rad_hasEnabledIface(pRad)) {
        SAH_TRACEZ_INFO(ME, "%s: has no enabled interface", pRad->Name);
        pRad->detailedState = CM_RAD_DOWN;
    } else if(wld_rad_nl80211_getChannel(pRad, &currChanSpec) < SWL_RC_OK) {
        SAH_TRACEZ_INFO(ME, "%s: has no available channel", pRad->Name);
        pRad->detailedState = CM_RAD_DOWN;
    } else if(!wld_channel_is_band_usable(currChanSpec)) {
        /*
         * In this situation, some channel clearing must be done, or is on going
         * The radio detailed status is updated through the driver/hostapd eventing.
         */
        SAH_TRACEZ_INFO(ME, "%s: curr band(c:%d/b:%d) is not yet usable",
                        pRad->Name, currChanSpec.channel, swl_chanspec_bwToInt(currChanSpec.bandwidth));
    } else if(wld_rad_hasActiveIface(pRad)) {
        /*
         * radio is considered up when:
         * 1) channel info available
         * 2) radio's main/child interface is operational (i.e running with lower up)
         * 3) current chanspec is usable (it is safety check, as net link may remain RUNNING even
         *    after a dfs radar detection)
         */
        SAH_TRACEZ_INFO(ME, "%s: radio is up", pRad->Name);
        pRad->detailedState = CM_RAD_UP;
    }
    // All other intermediate states are handled with eventing (nl80211/wpactrl)
    return (pRad->detailedState != CM_RAD_DOWN);
}

static bool s_doRadDisable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: disable rad", pRad->Name);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, false);
    return true;
}

static bool s_doRadEnable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: Enable rad", pRad->Name);
    if(pRad->isSTA) {
        wld_rad_nl80211_setSta(pRad);
    } else if(pRad->isAP) {
        wld_rad_nl80211_setAp(pRad);
    }
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, true);
    return true;
}

int wifiGen_rad_enable(T_Radio* rad, int val, int flag) {
    int ret;
    SAH_TRACEZ_INFO(ME, "%d --> %d -  %d", rad->enable, val, flag);
    if(flag & SET) {
        ret = val;
        if(flag & DIRECT) {
            if(val) {
                s_doRadEnable(rad);
            } else {
                s_doRadDisable(rad);
            }
        } else {
            setBitLongArray(rad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
        }
    } else {
        /* GET */
        if(flag & DIRECT) {
            ret = (wld_linuxIfUtils_getState(wld_rad_getSocket(rad), rad->Name) == true);
        } else {
            ret = rad->enable;
        }
    }
    return ret;
}

int wifiGen_rad_sync(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Get Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_sync", pRad->Name);
    // Rad sync: just toggles and re-apply current config with secDmn
    ASSERTI_FALSE(set & DIRECT, SWL_RC_OK, ME, "%s: no rad conf can be directly applied/synced out of secDmn", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD);
    return SWL_RC_OK;
}

int wifiGen_rad_regDomain(T_Radio* pRad, char* val, int bufsize, int set) {
    if(set & SET) {
        const char* countryName = getShortCountryName(pRad->regulatoryDomainIdx);
        if(!swl_str_isEmpty(countryName)) {
            swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain), countryName);
            if(set & DIRECT) {
                wld_rad_nl80211_setRegDomain(pRad, countryName);
            } else {
                setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_COUNTRYCODE);
            }
        } else {
            SAH_TRACEZ_ERROR(ME, "regulatoryDomainIdx %d not valid!", pRad->regulatoryDomainIdx);
        }
    } else {
        swl_str_copy(val, bufsize, pRad->regulatoryDomain);
    }
    return true;
}

int32_t s_getMaxPow(T_Radio* pRad) {
    wld_nl80211_wiphyInfo_t pWiphyInfo;
    memset(&pWiphyInfo, 0, sizeof(wld_nl80211_wiphyInfo_t));

    swl_rc_ne ret = wld_rad_nl80211_getWiphyInfo(pRad, &pWiphyInfo);
    ASSERT_TRUE(ret == SWL_RC_OK, 0, ME, "Err %i", ret);
    uint32_t curFreq = 0;
    swl_chanspec_t spec = wld_rad_getSwlChanspec(pRad);
    swl_chanspec_channelToMHz(&spec, &curFreq);

    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        if(pWiphyInfo.bands[i].nChans == 0) {
            continue;
        }
        for(uint32_t j = 0; j < pWiphyInfo.bands[i].nChans; j++) {
            if(pWiphyInfo.bands[i].chans[j].ctrlFreq == curFreq) {
                return (int32_t) pWiphyInfo.bands[i].chans[j].maxTxPwr;
            }
        }
    }
    return 0;

}

int wifiGen_rad_txpow(T_Radio* pRad, int val, int set) {
    if(set & SET) {
        pRad->transmitPower = val;
        ASSERTS_TRUE(wld_rad_hasActiveIface(pRad), SWL_RC_ERROR, ME, "%s not ready", pRad->Name);

        if(val == -1) {
            return wld_rad_nl80211_setTxPowerAuto(pRad);
        }

        int8_t srcVal = val;
        int32_t* tgtVal = (int32_t*) swl_table_getMatchingValue(&sPowerTable, 1, 0, &srcVal);
        ASSERT_NOT_NULL(tgtVal, SWL_RC_ERROR, ME, "%s: unknown txPow percentage %d", pRad->Name, val);


        int32_t maxPow = s_getMaxPow(pRad);
        ASSERT_NOT_EQUALS(maxPow, 0, SWL_RC_ERROR, ME, "%s maxPow unknown", pRad->Name);

        int32_t tgtPow = (*tgtVal > maxPow ? 0 : maxPow - *tgtVal);

        swl_rc_ne retVal = SWL_RC_OK;

        SAH_TRACEZ_INFO(ME, "%s: setPow %i / max %i diff %i => %i", pRad->Name, val, maxPow, *tgtVal, tgtPow);
        // arg in mbm, so * 100
        retVal = wld_rad_nl80211_setTxPowerLimited(pRad, tgtPow * 100);

        return (retVal == SWL_RC_OK ? WLD_OK : WLD_ERROR);
    } else {
        ASSERTS_TRUE(wld_rad_hasActiveIface(pRad), pRad->transmitPower, ME, "%s not ready", pRad->Name);
        int32_t maxPow = s_getMaxPow(pRad);
        ASSERT_NOT_EQUALS(maxPow, 0, SWL_RC_ERROR, ME, "%s: maxPow unknown", pRad->Name);

        int32_t curDbm;
        swl_rc_ne retVal = wld_rad_nl80211_getTxPower(pRad, &curDbm);
        ASSERT_EQUALS(retVal, SWL_RC_OK, SWL_RC_ERROR, ME, "%s: fail to read curr txPow", pRad->Name);

        if(curDbm == 0) {
            //auto set as 0
            return -1;
        }
        int32_t diff = (maxPow - curDbm);

        int8_t* tgtVal = (int8_t*) swl_table_getMatchingValue(&sPowerTable, 0, 1, &diff);
        ASSERTI_NOT_NULL(tgtVal, pRad->transmitPower, ME, "%s: no tgtVal %i (%i/%i)", pRad->Name, diff, curDbm, maxPow);

        return *tgtVal;
    }

}
swl_rc_ne wifiGen_rad_setChanspec(T_Radio* pRad, bool direct) {

    SAH_TRACEZ_INFO(ME, "%s: direct:%d", pRad->Name, direct);
    if(!direct) {
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_CHANNEL);
        return SWL_RC_OK;
    }
    bool needCommit = (pRad->fsmRad.FSM_State != FSM_RUN);
    unsigned long* actionArray = (needCommit ? pRad->fsmRad.FSM_BitActionArray : pRad->fsmRad.FSM_AC_BitActionArray);
    if(wifiGen_hapd_isRunning(pRad)) {
        if(pRad->pFA->mfn_misc_has_support(pRad, NULL, "CSA", 0)) {
            if((wld_channel_is_band_usable(pRad->targetChanspec.chanspec)) ||
               (pRad->pFA->mfn_misc_has_support(pRad, NULL, "DFS_OFFLOAD", 0))) {
                swl_rc_ne rc = wld_rad_hostapd_switchChannel(pRad);
                return (rc < SWL_RC_OK) ? SWL_RC_ERROR : SWL_RC_DONE;
            }
        }
        /*
         * channel can not be warm applied (with CSA)
         * then hostapd must be toggled for cold applying (with DFS clearing if needed)
         */
        wld_rad_hostapd_setChannel(pRad);
        setBitLongArray(actionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        setBitLongArray(actionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD);
    }
    /* update conf file */
    setBitLongArray(actionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    if(needCommit) {
        wld_rad_doCommitIfUnblocked(pRad);
    }
    return SWL_RC_OK;
}

uint32_t s_writeAntennaCtrl(T_Radio* pRad, com_dir_e comdir) {
    uint32_t maxMask = (1 << pRad->nrAntenna[comdir]) - 1;
    uint32_t setVal = (comdir == COM_DIR_TRANSMIT) ? pRad->txChainCtrl : pRad->rxChainCtrl;
    uint32_t val = pRad->actAntennaCtrl;
    if(setVal <= 0) {
        setVal = ((val <= 0) || (val > maxMask)) ? maxMask : val;
    }
    return setVal;
}
int wifiGen_rad_antennactrl(T_Radio* pRad, int val _UNUSED, int set) {
    ASSERT_TRUE(set & SET, WLD_ERROR_INVALID_PARAM, ME, "Get Only");
    uint32_t txMapAnt = s_writeAntennaCtrl(pRad, COM_DIR_TRANSMIT);
    uint32_t rxMapAnt = s_writeAntennaCtrl(pRad, COM_DIR_RECEIVE);

    swl_rc_ne ret = wld_rad_nl80211_setAntennas(pRad, txMapAnt, rxMapAnt);
    ASSERT_TRUE(swl_rc_isOk(ret), WLD_ERROR, ME, "Error");

    pRad->nrActiveAntenna[COM_DIR_TRANSMIT] = swl_bit32_getNrSet(txMapAnt);
    pRad->nrActiveAntenna[COM_DIR_RECEIVE] = swl_bit32_getNrSet(rxMapAnt);
    wld_radio_updateAntenna(pRad);
    SAH_TRACEZ_INFO(ME, "%s: enable %u, ctrl %i %i-%i ; ant %i/%i -%i/%i",
                    pRad->Name, pRad->enable, pRad->actAntennaCtrl, pRad->txChainCtrl, pRad->rxChainCtrl,
                    pRad->nrActiveAntenna[COM_DIR_TRANSMIT], pRad->nrAntenna[COM_DIR_TRANSMIT],
                    pRad->nrActiveAntenna[COM_DIR_RECEIVE], pRad->nrAntenna[COM_DIR_RECEIVE]);
    return WLD_OK;
}

int wifiGen_rad_supstd(T_Radio* pRad, swl_radioStandard_m radioStandards) {
    ASSERTS_NOT_NULL(pRad, -1, ME, "rad is NULL");
    ASSERT_TRUE(swl_radStd_isValid(radioStandards, "rad_supstd"), -1, ME,
                "%s rad_supstd : Invalid operatingStandards %#x",
                pRad->Name, radioStandards);

    SAH_TRACEZ_INFO(ME, "%s : Set standards %#x", pRad->Name, radioStandards);
    pRad->operatingStandards = radioStandards;
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD);
    return 1;
}

void wifiGen_rad_initBands(T_Radio* pRad) {
    int i = 0;
    for(i = 0; i < pRad->nrPossibleChannels; i++) {
        swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(pRad->possibleChannels[i], pRad->runningChannelBandwidth, pRad->operatingFrequencyBand);
        wld_channel_mark_available_channel(chanspec);
        if(wld_channel_is_dfs(pRad->possibleChannels[i])) {
            wld_channel_mark_radar_req_channel(chanspec);
            wld_channel_mark_passive_channel(chanspec);
        }
    }
}

swl_rc_ne wifiGen_rad_stats(T_Radio* pRad) {
    swl_rc_ne ret;
    T_Stats stats;
    memset(&stats, 0, sizeof(stats));
    if(wld_linuxIfStats_getRadioStats(pRad, &stats)) {
        ret = SWL_RC_OK;
        memcpy(&pRad->stats, &stats, sizeof(stats));
    } else {
        SAH_TRACEZ_WARNING(ME, "%s: get stats for radio fail", pRad->Name);
        ret = SWL_RC_ERROR;
    }
    return ret;
}

int wifiGen_rad_delayedCommitUpdate(T_Radio* pRad) {
    if(pRad->fsmRad.FSM_State != FSM_IDLE) {
        // Do nothing... we're still in commit state.
        pRad->fsmRad.TODC = 10000;
        return 0;
    }
    wld_rad_updateState(pRad, false);
    wifiGen_hapd_syncVapStates(pRad);
    pRad->fsmRad.TODC = 0;
    return 0;
}

swl_rc_ne wifiGen_rad_getScanResults(T_Radio* pRad, T_ScanResults* results) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(results, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_scan_cleanupScanResults(results);
    amxc_llist_for_each(it, &pRad->scanState.lastScanResults.ssids) {
        T_ScanResult_SSID* pResult = amxc_container_of(it, T_ScanResult_SSID, it);
        T_ScanResult_SSID* pCopy = calloc(1, sizeof(T_ScanResult_SSID));
        if(pCopy == NULL) {
            wld_scan_cleanupScanResults(results);
            return SWL_RC_ERROR;
        }
        memcpy(pCopy, pResult, sizeof(*pCopy));
        amxc_llist_it_init(&pCopy->it);
        amxc_llist_append(&results->ssids, &pCopy->it);
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_rad_getAirStats(T_Radio* pRad, T_Airstats* pStats) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_rad_nl80211_getAirstats(pRad, pStats);
}


swl_rc_ne wifiGen_rad_getSpectrumInfo(T_Radio* rad, bool update _UNUSED, amxc_llist_t* llSpectrumChannelInfo) {
    ASSERT_NOT_NULL(rad, SWL_RC_INVALID_PARAM, ME, "NULL");

    uint32_t nChanSurveyInfo = 0;
    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList = NULL;
    wld_rad_nl80211_getSurveyInfo(rad, &pChanSurveyInfoList, &nChanSurveyInfo);

    amxd_object_t* obj_scan = amxd_object_get(rad->pBus, "ScanResults");
    amxd_object_t* chanTemplate = amxd_object_get(obj_scan, "SurroundingChannels");
    amxd_object_t* chanObj = NULL;
    uint32_t nrCoChannel = 0;

    for(uint32_t i = 0; i < nChanSurveyInfo; i++) {
        swl_chanspec_t chanSpec;
        swl_chanspec_channelFromMHz(&chanSpec, pChanSurveyInfoList[i].frequencyMHz);
        spectrumChannelInfoEntry_t* pEntry = calloc(1, sizeof(spectrumChannelInfoEntry_t));
        ASSERT_NOT_NULL(pEntry, false, ME, "NULL");

        amxd_object_for_each(instance, it, chanTemplate) {
            chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
            if(amxd_object_get_uint16_t(chanObj, "Channel", NULL) == chanSpec.channel) {
                amxd_object_t* apTemplate = amxd_object_get(chanObj, "Accesspoint");
                nrCoChannel = amxd_object_get_instance_count(apTemplate);
                break;
            }
        }

        pEntry->channel = chanSpec.channel;
        pEntry->nrCoChannelAP = nrCoChannel;
        pEntry->bandwidth = rad->runningChannelBandwidth;
        pEntry->noiselevel = pChanSurveyInfoList[i].noiseDbm;
        amxc_llist_append(llSpectrumChannelInfo, &pEntry->it);

    }

    return SWL_RC_OK;
}
