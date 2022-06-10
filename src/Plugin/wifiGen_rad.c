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
#include "swl/swl_chanspec.h"
#include "wld/wld_radio.h"
#include "wld/wld_channel.h"
#include "wld/wld_util.h"
#include "wld/wld_linuxIfUtils.h"
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

static const char* s_defaultRegDomain = "EU";
static const int s_defaultChan[] = {1, 36, 1};

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
    wld_rad_parse_cap(pRad);

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
            if(SWL_BIT_IS_SET(pBand->radStdsMask, SWL_RADSTD_AC)) {
                if(SWL_BIT_IS_SET(pBand->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_VHT_SU)) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "EXPL_BF");
                }
                if(SWL_BIT_IS_SET(pBand->bfCapsSupported[COM_DIR_RECEIVE], RAD_BF_CAP_VHT_SU)) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "IMPL_BF");
                }
                if(SWL_BIT_IS_SET(pBand->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_HE_MU)) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "MU_MIMO");
                }
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
            pRad->operatingChannelBandwidth = swl_chanspec_intToBw(pIfaceInfo->chanSpec.chanWidth);
            pRad->runningChannelBandwidth = pRad->operatingChannelBandwidth;
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
            pRad->runningChannelBandwidth = pRad->operatingChannelBandwidth = pRad->maxChannelBandwidth;
        }
    } else if(pRad->runningChannelBandwidth == SWL_BW_AUTO) {
        switch(pRad->operatingFrequencyBand) {
        case SWL_FREQ_BAND_EXT_5GHZ:
            pRad->operatingChannelBandwidth = SWL_BW_80MHZ;
            pRad->runningChannelBandwidth = SWL_BW_80MHZ;
            break;
        case SWL_FREQ_BAND_EXT_6GHZ:
            pRad->operatingChannelBandwidth = SWL_BW_80MHZ;
            pRad->runningChannelBandwidth = SWL_BW_160MHZ;
            break;
        default:
            pRad->operatingChannelBandwidth = SWL_BW_AUTO;
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

int s_updateChannels(T_Radio* pRad, wld_nl80211_bandDef_t* pOperBand) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pOperBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->nrPossibleChannels = 0;
    for(uint32_t i = 0; i < pOperBand->nChans; i++) {
        wld_nl80211_chanDesc_t* pChan = &pOperBand->chans[i];
        swl_chanspec_t chanSpec;
        if((swl_chanspec_channelFromMHz(&chanSpec, pChan->ctrlFreq) < SWL_RC_OK) ||
           (chanSpec.channel == 0)) {
            continue;
        }
        pRad->possibleChannels[pRad->nrPossibleChannels++] = chanSpec.channel;
    }
    wld_channel_init_channels(pRad);
    s_readChanInfo(pRad, pOperBand);
    wld_rad_write_possible_channels(pRad);
    return SWL_RC_OK;
}

int wifiGen_rad_poschans(T_Radio* pRad, uint8_t* buf _UNUSED, int bufsize _UNUSED) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(pRad->operatingFrequencyBand < (uint32_t) SWL_FREQ_BAND_MAX, SWL_RC_INVALID_PARAM, ME, "Invalid");
    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc = wld_rad_nl80211_getWiphyInfo(pRad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[pRad->operatingFrequencyBand];
    return s_updateChannels(pRad, pOperBand);
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
        pRad->channel = s_defaultChan[pRad->operatingFrequencyBand];
    }
    s_updateChannels(pRad, pOperBand);

    pRad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    pRad->channelChangeReason = CHAN_REASON_INITIAL;

    wld_rad_update_operating_standard(pRad);
    pRad->autoChannelSupported = TRUE;
    pRad->autoChannelEnable = FALSE;
    pRad->autoChannelRefreshPeriod = FALSE;
    pRad->extensionChannel = REXT_AUTO;          /* Auto */
    pRad->guardInterval = RGI_AUTO;              /* Auto == 400NSec*/
    pRad->MCS = 10;
    if(pOperBand->radStdsMask & M_SWL_RADSTD_AX) {
        pRad->MCS = pOperBand->mcsStds[SWL_STANDARD_HE].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_AC) {
        pRad->MCS = pOperBand->mcsStds[SWL_STANDARD_VHT].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_N) {
        pRad->MCS = pOperBand->mcsStds[SWL_STANDARD_HT].mcsIndex;
    }

    swl_table_columnToArrayOffset(pRad->transmitPowerSupported, 64, &sPowerTable, 0, 0);

    pRad->transmitPower = 100;
    pRad->setRadio80211hEnable = (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ || pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) ? TRUE : FALSE;
    wld_rad_chan_notification(pRad, pRad->channel, pRad->runningChannelBandwidth);
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
    int ret = wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pRad->Name);
    ASSERTS_FALSE(ret < 0, ret, ME, "%s: fail to get rad state", pRad->Name);
    if(ret > 0) {
        if((pRad->detailedState != CM_RAD_FG_CAC) || wifiGen_hapd_hasStateEnabled(pRad)) {
            pRad->detailedState = CM_RAD_UP;
        }
    } else {
        pRad->detailedState = CM_RAD_DOWN;
    }
    return ret;
}

int wifiGen_rad_enable(T_Radio* rad, int val, int set) {
    int ret;
    SAH_TRACEZ_INFO(ME, "%d --> %d -  %d", rad->enable, val, set);
    if(set & SET) {
        ret = val;
        setBitLongArray(rad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
    } else {
        /* GET */
        ret = rad->enable;
    }
    return ret;
}

int wifiGen_rad_sync(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Get Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_sync", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
    return 0;
}

int wifiGen_rad_regDomain(T_Radio* pRad, char* val, int bufsize, int set) {
    if(set & SET) {
        if(pRad->regulatoryDomainIdx >= 0) {
            swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain),
                         Rad_CountryCode[pRad->regulatoryDomainIdx].ShortCountryName);
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
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
    uint32_t curFreq = wld_rad_getCurrentFreq(pRad);

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

        if(val == -1) {
            return wld_rad_nl80211_setTxPowerAuto(pRad);
        }

        int8_t srcVal = val;
        int32_t* tgtVal = (int32_t*) swl_table_getMatchingValue(&sPowerTable, 1, 0, &srcVal);
        if(tgtVal == NULL) {
            return WLD_ERROR;
        }


        int32_t maxPow = s_getMaxPow(pRad);
        ASSERT_TRUE(maxPow != 0, WLD_ERROR, ME, "%s maxPow unknown", pRad->Name);

        int32_t tgtPow = (*tgtVal > maxPow ? 0 : maxPow - *tgtVal);

        swl_rc_ne retVal = SWL_RC_OK;

        SAH_TRACEZ_ERROR(ME, "%s: setPow %i / max %i diff %i => %i", pRad->Name, val, maxPow, *tgtVal, tgtPow);
        // arg in mbm, so * 100
        retVal = wld_rad_nl80211_setTxPowerLimited(pRad, tgtPow * 100);

        return (retVal == SWL_RC_OK ? WLD_OK : WLD_ERROR);
    } else {
        int32_t maxPow = s_getMaxPow(pRad) * 100;
        ASSERT_TRUE(maxPow != 0, WLD_ERROR, ME, "ERROR");

        int32_t curDbm;
        swl_rc_ne retVal = wld_rad_nl80211_getTxPower(pRad, &curDbm);
        ASSERT_TRUE(retVal == SWL_RC_OK, WLD_ERROR, ME, "ERROR");

        if(curDbm == 0) {
            //auto set as 0
            return -1;
        }
        int32_t diff = (maxPow - curDbm) / 100;

        int8_t* tgtVal = (int8_t*) swl_table_getMatchingValue(&sPowerTable, 0, 1, &diff);
        ASSERT_NOT_NULL(tgtVal, SWL_RC_ERROR, ME, "%s: no tgtVal %i (%i/%i)", pRad->Name, diff, curDbm, maxPow);


        SAH_TRACEZ_ERROR(ME, "%s: getPow %i max %i diff %i => %i", pRad->Name, curDbm, maxPow, diff, *tgtVal);
        return *tgtVal;
    }

}
int wifiGen_rad_channel(T_Radio* pRad, int val, int set) {

    SAH_TRACEZ_INFO(ME, "%s: val:%d-->%d bw=%d - set:%d", pRad->Name, pRad->channel, val, pRad->runningChannelBandwidth, set);

    if(set & SET) {
        pRad->channel = val;
        if(set & DIRECT) {
            wld_rad_hostapd_switchChannel(pRad);
        } else {
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        }
    }
    return pRad->channel;
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
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    return 1;
}

int wifiGen_rad_ochbw(T_Radio* pRad, int val, int set) {
    if(set & SET) {
        pRad->operatingChannelBandwidth = val;
        wld_rad_get_update_running_bandwidth(pRad);
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    } else {
        return pRad->operatingChannelBandwidth;
    }
    SAH_TRACEZ_INFO(ME, "%p %.8x %d", pRad, val, set);
    return 0;
}