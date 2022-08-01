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
 * This file implement nl80211 msg parsing
 */

#include "wld_nl80211_parser.h"

#define ME "nlParser"

#define NLA_GET_VAL(target, api, param) \
    if(param) { \
        target = api(param); \
    }

#define NLA_GET_DATA(target, param, maxLen) \
    if(param) { \
        int len = SWL_MIN((int) (maxLen), nla_len(param)); \
        memcpy(target, nla_data(param), len); \
    }

uint32_t wld_nl80211_getWiphy(struct nlattr* tb[]) {
    ASSERTS_NOT_NULL(tb, WLD_NL80211_ID_UNDEF, ME, "NULL");
    if(tb[NL80211_ATTR_WIPHY]) {
        return nla_get_u32(tb[NL80211_ATTR_WIPHY]);
    }
    /*
     * in some msg, wiphy attribute is missing, but wider wdev identifier
     * is provided, which includes the id
     */
    if(tb[NL80211_ATTR_WDEV]) {
        uint64_t wdev_id = nla_get_u64(tb[NL80211_ATTR_WDEV]);
        return (uint32_t) (wdev_id >> 32);
    }
    return WLD_NL80211_ID_UNDEF;
}

uint32_t wld_nl80211_getIfIndex(struct nlattr* tb[]) {
    uint32_t ifIndex = WLD_NL80211_ID_UNDEF;
    ASSERTS_NOT_NULL(tb, ifIndex, ME, "NULL");
    NLA_GET_VAL(ifIndex, nla_get_u32, tb[NL80211_ATTR_IFINDEX]);
    return ifIndex;
}

SWL_TABLE(sChannelWidthMap,
          ARR(uint32_t nl80211Bw; uint32_t bw; swl_bandwidth_e swlBw; ),
          ARR(swl_type_uint32, swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_CHAN_WIDTH_20, 20, SWL_BW_20MHZ},
              {NL80211_CHAN_WIDTH_40, 40, SWL_BW_40MHZ},
              {NL80211_CHAN_WIDTH_80, 80, SWL_BW_80MHZ},
              {NL80211_CHAN_WIDTH_160, 160, SWL_BW_160MHZ},
              {NL80211_CHAN_WIDTH_10, 10, SWL_BW_10MHZ},
              {NL80211_CHAN_WIDTH_5, 5, SWL_BW_5MHZ},
              {NL80211_CHAN_WIDTH_2, 2, SWL_BW_2MHZ},
              //specific cases
              {NL80211_CHAN_WIDTH_20_NOHT, 20, SWL_BW_20MHZ},
              {NL80211_CHAN_WIDTH_80P80, 160, SWL_BW_AUTO},
              ));

swl_rc_ne wld_nl80211_parseChanSpec(struct nlattr* tb[], wld_nl80211_chanSpec_t* pChanSpec) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERTS_NOT_NULL(tb, rc, ME, "NULL");
    ASSERTS_NOT_NULL(pChanSpec, rc, ME, "NULL");
    rc = SWL_RC_OK;
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_WIPHY_FREQ], rc, ME, "no freq");
    pChanSpec->ctrlFreq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_CHANNEL_WIDTH], rc, ME, "no chan width");
    uint32_t bwId = nla_get_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]);
    uint32_t* pBwVal = (uint32_t*) swl_table_getMatchingValue(&sChannelWidthMap, 1, 0, &bwId);
    if(pBwVal) {
        pChanSpec->chanWidth = *pBwVal;
    }
    NLA_GET_VAL(pChanSpec->centerFreq1, nla_get_u32, tb[NL80211_ATTR_CENTER_FREQ1]);
    NLA_GET_VAL(pChanSpec->centerFreq2, nla_get_u32, tb[NL80211_ATTR_CENTER_FREQ2]);
    return SWL_RC_DONE;
}

swl_rc_ne wld_nl80211_parseInterfaceInfo(struct nlattr* tb[], wld_nl80211_ifaceInfo_t* pWlIface) {
    ASSERT_NOT_NULL(pWlIface, SWL_RC_INVALID_PARAM, ME, "NULL");
    memset(pWlIface, 0, sizeof(*pWlIface));
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pWlIface->wiphy = wld_nl80211_getWiphy(tb);
    pWlIface->ifIndex = wld_nl80211_getIfIndex(tb);
    NLA_GET_DATA(pWlIface->name, tb[NL80211_ATTR_IFNAME], (sizeof(pWlIface->name) - 1));
    uint32_t ifType = NL80211_IFTYPE_UNSPECIFIED;
    NLA_GET_VAL(ifType, nla_get_u32, tb[NL80211_ATTR_IFTYPE]);
    pWlIface->isAp = (ifType == NL80211_IFTYPE_AP);
    pWlIface->isSta = (ifType == NL80211_IFTYPE_STATION);
    uint64_t wdev_id = 0;
    NLA_GET_VAL(wdev_id, nla_get_u64, tb[NL80211_ATTR_WDEV]);
    pWlIface->isMain = ((wdev_id & 0xffffffff) == 1);
    NLA_GET_DATA(pWlIface->mac.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
    wld_nl80211_parseChanSpec(tb, &pWlIface->chanSpec);
    NLA_GET_VAL(pWlIface->txPower, nla_get_u32, tb[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);
    NLA_GET_VAL(pWlIface->use4Mac, nla_get_u8, tb[NL80211_ATTR_4ADDR]);
    NLA_GET_DATA(pWlIface->ssid, tb[NL80211_ATTR_SSID], (sizeof(pWlIface->ssid) - 1));
    return SWL_RC_OK;
}

swl_rc_ne s_parseIfTypes(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_SUPPORTED_IFTYPES], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlMode;
    int rem;
    nla_for_each_nested(nlMode, tb[NL80211_ATTR_SUPPORTED_IFTYPES], rem) {
        if(nla_type(nlMode) == NL80211_IFTYPE_STATION) {
            pWiphy->suppEp = true;
        } else if(nla_type(nlMode) == NL80211_IFTYPE_AP) {
            pWiphy->suppAp = true;
        }
    }
    return SWL_RC_OK;
}
swl_rc_ne s_parseIfCombi(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_INTERFACE_COMBINATIONS], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlComb;
    int remComb;
    nla_for_each_nested(nlComb, tb[NL80211_ATTR_INTERFACE_COMBINATIONS], remComb) {
        static struct nla_policy ifaceCombinationPolicy[NUM_NL80211_IFACE_COMB] = {
            [NL80211_IFACE_COMB_LIMITS] = { .type = NLA_NESTED },
            [NL80211_IFACE_COMB_MAXNUM] = { .type = NLA_U32 },
            [NL80211_IFACE_COMB_NUM_CHANNELS] = { .type = NLA_U32 },
        };
        struct nlattr* tbComb[NUM_NL80211_IFACE_COMB];
        static struct nla_policy ifaceLimitPolicy[NUM_NL80211_IFACE_LIMIT] = {
            [NL80211_IFACE_LIMIT_TYPES] = { .type = NLA_NESTED },
            [NL80211_IFACE_LIMIT_MAX] = { .type = NLA_U32 },
        };
        int err = nla_parse_nested(tbComb, MAX_NL80211_IFACE_COMB,
                                   nlComb, ifaceCombinationPolicy);
        if(err || !tbComb[NL80211_IFACE_COMB_LIMITS] || !tbComb[NL80211_IFACE_COMB_MAXNUM]) {
            continue;
        }
        struct nlattr* tbLimit[NUM_NL80211_IFACE_LIMIT];
        struct nlattr* nlLimit;
        int remLimit;
        nla_for_each_nested(nlLimit, tbComb[NL80211_IFACE_COMB_LIMITS], remLimit) {
            err = nla_parse_nested(tbLimit, MAX_NL80211_IFACE_LIMIT,
                                   nlLimit, ifaceLimitPolicy);
            if(err || !tbLimit[NL80211_IFACE_LIMIT_TYPES]) {
                break;
            }
            struct nlattr* nlMode;
            int remMode;
            nla_for_each_nested(nlMode, tbLimit[NL80211_IFACE_LIMIT_TYPES], remMode) {
                uint32_t limit = nla_get_u32(tbLimit[NL80211_IFACE_LIMIT_MAX]);
                if(nla_type(nlMode) == NL80211_IFTYPE_AP) {
                    pWiphy->nApMax = limit;
                } else if(nla_type(nlMode) == NL80211_IFTYPE_STATION) {
                    pWiphy->nEpMax = limit;
                }
            }
        }
    }
    return SWL_RC_OK;
}

typedef struct {
    uint16_t b_0 : 1;
    uint16_t support_ch_width_set : 1;
    uint16_t sm_power_save : 2;
    uint16_t ht_greenfield : 1;
    uint16_t short_gi20mhz : 1;
    uint16_t short_gi40mhz : 1;
    uint16_t b_7_15 : 9;
} __attribute__((packed)) htCapabilityInfo_t;

swl_rc_ne s_parseHtAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_HT_CAPA], SWL_RC_OK, ME, "no ht cap");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_HT_MCS_SET], SWL_RC_OK, ME, "no ht mcs set");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_STANDARD_HT];
    htCapabilityInfo_t htCapInfo;
    NLA_GET_DATA(&htCapInfo, tbBand[NL80211_BAND_ATTR_HT_CAPA], sizeof(htCapInfo));
    pMcsStd->standard = SWL_STANDARD_HT;
    //HT20 / HT40
    pMcsStd->bandwidth = SWL_BW_20MHZ;
    if(htCapInfo.support_ch_width_set) {
        pMcsStd->bandwidth = SWL_BW_40MHZ;
        pBand->chanWidthMask |= M_SWL_BW_40MHZ;
    }
    //RX HT20 SGI / RX HT40 SGI
    pMcsStd->guardInterval = SWL_SGI_800;
    if(htCapInfo.short_gi20mhz || htCapInfo.short_gi40mhz) {
        pMcsStd->guardInterval = SWL_SGI_400;
    }
    /*
     * As defined in 7.3.2.57.4 Supported MCS Set field:
     * BitMask: B0..B76 + 3 reserved bits => 10 Bytes
     */
    uint32_t htRxMcsBitMask[3] = {0};
    NLA_GET_DATA(htRxMcsBitMask, tbBand[NL80211_BAND_ATTR_HT_MCS_SET], 10);
    int32_t highestIdx = swl_bitArr32_getHighest(htRxMcsBitMask, SWL_ARRAY_SIZE(htRxMcsBitMask));
    pMcsStd->mcsIndex = SWL_MAX(0, highestIdx);
    pMcsStd->numberOfSpatialStream = (pMcsStd->mcsIndex / 8) + 1;

    pBand->radStdsMask |= M_SWL_RADSTD_N;
    pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
    return SWL_RC_DONE;
}

typedef struct {
    uint32_t max_mpdu_len : 2;
    uint32_t support_ch_width_set : 2;
    uint32_t rx_ldpc : 1;
    uint32_t short_gi80mhz_tvht_mode4c : 1;
    uint32_t short_gi160mhz80_80mhz : 1;
    uint32_t tx_stbc : 1;
    uint32_t rx_stbc : 3;
    uint32_t su_beamformer : 1;           // SU beamformer capable
    uint32_t su_beamformee : 1;           // SU beamformee capable
    uint32_t beamformee_sts : 3;          // Beamformee STS capability
    uint32_t sound_dimensions : 3;        // Number of sounding dimensions
    uint32_t mu_beamformer : 1;           // MU beamformer capable
    uint32_t mu_beamformee : 1;           // MU beamformee capable
    uint32_t b_22_32 : 11;
} __attribute__((packed)) vhtCapabilityInfo_t;

swl_rc_ne s_parseVhtAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_VHT_CAPA], SWL_RC_OK, ME, "no vht cap");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_VHT_MCS_SET], SWL_RC_OK, ME, "no vht mcs set");
    ASSERTW_NOT_EQUALS(pBand->freqBand, SWL_FREQ_BAND_2_4GHZ, SWL_RC_OK, ME, "invalid vht cap in 2.4g band");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_STANDARD_VHT];
    vhtCapabilityInfo_t vhtCapInfo;
    NLA_GET_DATA(&vhtCapInfo, tbBand[NL80211_BAND_ATTR_VHT_CAPA], sizeof(vhtCapInfo));
    pMcsStd->standard = SWL_STANDARD_VHT;
    //Supported Channel Width
    pMcsStd->bandwidth = SWL_BW_80MHZ;
    pBand->chanWidthMask |= (M_SWL_BW_40MHZ | M_SWL_BW_80MHZ);
    switch(vhtCapInfo.support_ch_width_set) {
    //(160 MHz )or (160 MHz/80+80 MHz)
    case 1:
    case 2: {
        pMcsStd->bandwidth = SWL_BW_160MHZ;
        pBand->chanWidthMask |= M_SWL_BW_160MHZ;
        break;
    }
    default: break;
    }
    //short GI (80 MHz) / short GI (160/80+80 MHz)
    pMcsStd->guardInterval = SWL_SGI_800;
    if(vhtCapInfo.short_gi80mhz_tvht_mode4c || vhtCapInfo.short_gi160mhz80_80mhz) {
        pMcsStd->guardInterval = SWL_SGI_400;
    }
    //SU Beamformer
    if(vhtCapInfo.su_beamformer) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_VHT_SU;
    }
    //SU Beamformee
    if(vhtCapInfo.su_beamformee) {
        pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_VHT_SU;
    }
    //MU Beamformer
    if(vhtCapInfo.mu_beamformer) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_VHT_MU;
    }
    //MU Beamformee
    if(vhtCapInfo.mu_beamformee) {
        pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_VHT_MU;
    }

    /*
     * As defined in 9.4.2.157.3 Supported VHT-MCS and NSS Set field: B0..B63
     * Rx VHT-MCS Map: Figure 9-611—Rx VHT-MCS Map: B0..B16
     * tweak: only parse first Rx VHT-MCS Map
     * (same values are commonly applied for TX maps)
     */
    uint16_t mcsMapRx;
    NLA_GET_DATA(&mcsMapRx, tbBand[NL80211_BAND_ATTR_VHT_MCS_SET], sizeof(mcsMapRx));
    pMcsStd->numberOfSpatialStream = 1;
    pMcsStd->mcsIndex = 0;
    for(uint8_t ssId = 0; ssId < 8; ssId++) {  // up to 8ss
        uint8_t mcsMapPerSS = (mcsMapRx >> (2 * ssId)) & 0x03;
        if(mcsMapPerSS < 3) {
            pMcsStd->numberOfSpatialStream = SWL_MAX((int32_t) pMcsStd->numberOfSpatialStream, (ssId + 1));
            pMcsStd->mcsIndex = SWL_MAX((int32_t) pMcsStd->mcsIndex, (mcsMapPerSS + 7));
        }
    }

    pBand->radStdsMask |= M_SWL_RADSTD_AC;
    pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
    return SWL_RC_DONE;
}

typedef struct {
    uint8_t b_0 : 1;
    uint8_t supp_40mhz_in_2_4ghz : 1;
    uint8_t supp_40_80mhz_in_5ghz : 1;
    uint8_t supp_160mhz_in_5ghz : 1;
    uint8_t supp_160_80p80mhz_in_5ghz : 1;
    uint8_t b_5_7 : 3;
    uint16_t b_8_23;
    uint16_t b_24_30 : 7;
    uint16_t su_beamformer : 1;
    uint16_t su_beamformee : 1;
    uint16_t mu_beamformer : 1;
    uint16_t beamformee_sts_le_80mhz : 3;
    uint16_t beamformee_sts_gt_80mhz : 3;
    uint16_t b_40_55;
    uint16_t b_56_71;
    uint16_t b_72_87;
} __attribute__((packed)) hePhyCapInfo_t;

swl_rc_ne s_parseHeAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], SWL_RC_OK, ME, "no iftype data");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_STANDARD_HE];
    struct nlattr* nlIfType;
    int rem;
    int bandIftypeAttrMax = NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET;
    struct nlattr* tbIfType[bandIftypeAttrMax + 1];
    nla_for_each_nested(nlIfType, tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], rem) {
        nla_parse(tbIfType, bandIftypeAttrMax, nla_data(nlIfType), nla_len(nlIfType), NULL);
        struct nlattr* heCapPhyAttr = tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY];
        struct nlattr* heCapMcsSetAttr = tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET];
        if(!heCapPhyAttr || !heCapMcsSetAttr) {
            continue;
        }
        /*
         * As defined in 9.4.2.248.3 HE PHY Capabilities Information field: B0..B87
         */
        hePhyCapInfo_t htPhyCapInfo;
        NLA_GET_DATA(&htPhyCapInfo, heCapPhyAttr, sizeof(htPhyCapInfo));
        uint64_t phyCap[2] = { 0 };
        NLA_GET_DATA(phyCap, heCapPhyAttr, 11);
        pMcsStd->standard = SWL_STANDARD_HE;
        pMcsStd->bandwidth = SWL_BW_20MHZ;
        if(pBand->freqBand == SWL_FREQ_BAND_2_4GHZ) {
            if(htPhyCapInfo.supp_40mhz_in_2_4ghz) {
                pMcsStd->bandwidth = SWL_BW_40MHZ;
                pBand->chanWidthMask |= M_SWL_BW_40MHZ;
            }
        } else {
            if(htPhyCapInfo.supp_40_80mhz_in_5ghz) {
                pMcsStd->bandwidth = SWL_BW_80MHZ;
                pBand->chanWidthMask |= M_SWL_BW_40MHZ | M_SWL_BW_80MHZ;
            }
            if(htPhyCapInfo.supp_160mhz_in_5ghz || htPhyCapInfo.supp_160_80p80mhz_in_5ghz) {
                pMcsStd->bandwidth = SWL_BW_160MHZ;
                pBand->chanWidthMask |= M_SWL_BW_160MHZ;
            }
        }
        /*
         * GI 800ns mandatory in HE
         * Cf: IEEE80211 27.3.12 Data field
         * An HE STA shall support Data field OFDM symbols in an HE PPDU with guard interval
         * durations of 0.8 µs, 1.6 µs, and 3.2 µs.
         */
        pMcsStd->guardInterval = SWL_SGI_800;
        //SU Beamformer
        if(htPhyCapInfo.su_beamformer) {
            pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_HE_SU;
        }
        //SU Beamformee
        if(htPhyCapInfo.su_beamformee) {
            pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_HE_SU;
        }
        //MU Beamformer
        if(htPhyCapInfo.mu_beamformer) {
            pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_HE_MU;
        }
        //Beamformee STS (<= 80Mhz or > 80MHz)
        if(htPhyCapInfo.beamformee_sts_le_80mhz || htPhyCapInfo.beamformee_sts_gt_80mhz) {
            pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_HE_MU;
        }
        /*
         * As defined in 9.4.2.248.4 Supported HE-MCS And NSS Set field:
         * RX/TX MCS Maps, per supported chan width group
         * tweak: only parse "permanent" first Rx HE-MCS Map ≤ 80 MHz
         * (same values are commonly applied for next maps (TX, and other bw groups)
         */
        uint16_t mcsMapRxLe80;
        NLA_GET_DATA(&mcsMapRxLe80, heCapMcsSetAttr, sizeof(mcsMapRxLe80));
        pMcsStd->numberOfSpatialStream = 1;
        pMcsStd->mcsIndex = 0;
        for(uint8_t ssId = 0; ssId < 8; ssId++) {  // up to 8ss
            uint8_t mcsMapPerSS = (mcsMapRxLe80 >> (2 * ssId)) & 0x03;
            if(mcsMapPerSS < 3) {
                pMcsStd->numberOfSpatialStream = SWL_MAX((int32_t) pMcsStd->numberOfSpatialStream, (ssId + 1));
                pMcsStd->mcsIndex = SWL_MAX((int32_t) pMcsStd->mcsIndex, (7 + (mcsMapPerSS * 2)));
            }
        }

        pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
        pBand->radStdsMask |= M_SWL_RADSTD_AX;
    }
    ASSERTS_EQUALS(pMcsStd->standard, SWL_STANDARD_HE, SWL_RC_OK, ME, "Missing HE caps");
    return SWL_RC_DONE;
}

SWL_TABLE(sChanDfsStatusMap,
          ARR(uint32_t nl80211ChanStatus; wld_nl80211_chanStatus_e chanStatus; ),
          ARR(swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_DFS_USABLE, WLD_NL80211_CHAN_USABLE},
              {NL80211_DFS_UNAVAILABLE, WLD_NL80211_CHAN_UNAVAILABLE},
              {NL80211_DFS_AVAILABLE, WLD_NL80211_CHAN_AVAILABLE},
              ));
const char* g_str_wld_nl80211_chan_status_bf_cap[WLD_NL80211_CHAN_STATUS_MAX] = {
    "Unknown", "Disabled", "Usable", "Unavailable", "Available",
};
swl_rc_ne s_parseChans(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_FREQS], SWL_RC_OK, ME, "No freqs to parse");
    struct nlattr* nlFreq;
    int remFreq;
    struct nlattr* tbFreq[NL80211_FREQUENCY_ATTR_MAX + 1];
    static struct nla_policy freqPolicy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
        [NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
        [NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_NO_IR] = { .type = NLA_FLAG },
        [__NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
    };
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_STATE].type = NLA_U32;
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_TIME].type = NLA_U32;
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME].type = NLA_U32;
    wld_nl80211_chanDesc_t* pChan = &pBand->chans[0];
    if(pBand->nChans > 0) {
        pChan = &pBand->chans[pBand->nChans - 1];
    }
    nla_for_each_nested(nlFreq, tbBand[NL80211_BAND_ATTR_FREQS], remFreq) {
        uint32_t freq;
        nla_parse(tbFreq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nlFreq), nla_len(nlFreq), freqPolicy);
        // Ignore "channels" that are disabled or with no frequency.
        if(!tbFreq[NL80211_FREQUENCY_ATTR_FREQ] || tbFreq[NL80211_FREQUENCY_ATTR_DISABLED]) {
            continue;
        }
        freq = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_FREQ]);
        bool isDfs = tbFreq[NL80211_FREQUENCY_ATTR_RADAR];
        bool noIr = (tbFreq[NL80211_FREQUENCY_ATTR_NO_IR] || tbFreq[__NL80211_FREQUENCY_ATTR_NO_IBSS]);
        //Filter out chanels that are not DFS but still have the NO_IR flag
        if(!isDfs && noIr) {
            continue;
        }
        if(pChan->ctrlFreq != freq) {
            if(pBand->nChans >= WLD_MAX_POSSIBLE_CHANNELS) {
                continue;
            }
            pChan = &pBand->chans[pBand->nChans++];
            pChan->ctrlFreq = freq;
            pChan->isDfs = isDfs;
        }
        if(!(pChan->isDfs)) {
            pChan->status = WLD_NL80211_CHAN_AVAILABLE;
        } else {
            if(tbFreq[NL80211_FREQUENCY_ATTR_DFS_STATE]) {
                uint32_t state = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_DFS_STATE]);
                uint32_t* pStateVal = (uint32_t*) swl_table_getMatchingValue(&sChanDfsStatusMap, 1, 0, &state);
                if(pStateVal) {
                    pChan->status = *pStateVal;
                }
            }
            NLA_GET_VAL(pChan->dfsTime, nla_get_u32, tbFreq[NL80211_FREQUENCY_ATTR_DFS_TIME]);
            NLA_GET_VAL(pChan->dfsCacTime, nla_get_u32, tbFreq[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME]);
        }
        if(tbFreq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]) {
            //convert from mbm to dbm
            pChan->maxTxPwr = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]) / 100;
        }
    }
    return SWL_RC_OK;
}
SWL_TABLE(sBandsMap,
          ARR(uint32_t nl80211Band; swl_freqBand_e swlBand; ),
          ARR(swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_BAND_2GHZ, SWL_FREQ_BAND_2_4GHZ},
              {NL80211_BAND_5GHZ, SWL_FREQ_BAND_5GHZ},
              {NL80211_BAND_6GHZ, SWL_FREQ_BAND_6GHZ},
              ));
swl_rc_ne s_parseWiphyBands(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_WIPHY_BANDS], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlBand;
    struct nlattr* nlFreq;
    int remBand;
    int remFreq;
    struct nlattr* tbBand[NL80211_BAND_ATTR_MAX + 1];
    nla_for_each_nested(nlBand, tb[NL80211_ATTR_WIPHY_BANDS], remBand) {
        uint32_t nlFreqBand = nlBand->nla_type;
        swl_freqBand_e* pSwlFreqBand = (swl_freqBand_e*) swl_table_getMatchingValue(&sBandsMap, 1, 0, &nlFreqBand);
        if(!pSwlFreqBand) {
            continue;
        }
        swl_freqBand_e freqBand = *pSwlFreqBand;
        wld_nl80211_bandDef_t* pBand = &pWiphy->bands[freqBand];
        pBand->freqBand = freqBand;
        switch(freqBand) {
        case SWL_FREQ_BAND_2_4GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_B | M_SWL_RADSTD_G;
            break;
        }
        case SWL_FREQ_BAND_5GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_A;
            break;
        }
        case SWL_FREQ_BAND_6GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_AX;
            break;
        }
        default:
            break;
        }
        pBand->nSSMax = SWL_MAX((int) pBand->nSSMax, 1);
        pBand->chanWidthMask |= M_SWL_BW_20MHZ;
        nla_parse(tbBand, NL80211_BAND_ATTR_MAX, nla_data(nlBand), nla_len(nlBand), NULL);
        s_parseHtAttrs(tbBand, pBand);
        s_parseVhtAttrs(tbBand, pBand);
        s_parseHeAttrs(tbBand, pBand);
        s_parseChans(tbBand, pBand);
    }
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(pWiphy->bands); i++) {
        if(pWiphy->bands[i].nChans > 0) {
            pWiphy->freqBandsMask |= SWL_BIT_SHIFT(pWiphy->bands[i].freqBand);
            pWiphy->nSSMax = SWL_MAX(pWiphy->nSSMax, pWiphy->bands[i].nSSMax);
        }
    }
    return SWL_RC_OK;
}

static swl_rc_ne s_parseAntMask(struct nlattr* pAttr, int32_t* pNAnt) {
    ASSERTS_NOT_NULL(pNAnt, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!pAttr) {
        if(*pNAnt == 0) {
            *pNAnt = -1;
        }
        return SWL_RC_ERROR;
    }
    *pNAnt = swl_bit32_getNrSet(nla_get_u32(pAttr));
    return SWL_RC_OK;
}
swl_rc_ne wld_nl80211_parseWiphyInfo(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERT_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pWiphy->wiphy = wld_nl80211_getWiphy(tb);
    NLA_GET_DATA(pWiphy->name, tb[NL80211_ATTR_WIPHY_NAME], (sizeof(pWiphy->name) - 1));
    NLA_GET_VAL(pWiphy->maxScanSsids, nla_get_u8, tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]);
    NLA_GET_VAL(pWiphy->maxScanIeLen, nla_get_u16, tb[NL80211_ATTR_MAX_SCAN_IE_LEN]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX], &pWiphy->nrAntenna[COM_DIR_TRANSMIT]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX], &pWiphy->nrAntenna[COM_DIR_RECEIVE]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_TX], &pWiphy->nrActiveAntenna[COM_DIR_TRANSMIT]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_RX], &pWiphy->nrActiveAntenna[COM_DIR_RECEIVE]);
    if(tb[NL80211_ATTR_SUPPORT_AP_UAPSD]) {
        pWiphy->suppUAPSD = true;
    }
    if(tb[NL80211_ATTR_CIPHER_SUITES]) {
        pWiphy->nCipherSuites = SWL_MIN(WLD_NL80211_CIPHERS_MAX, (int) (nla_len(tb[NL80211_ATTR_CIPHER_SUITES]) / sizeof(uint32_t)));
        memcpy(pWiphy->cipherSuites, nla_data(tb[NL80211_ATTR_CIPHER_SUITES]), pWiphy->nCipherSuites * sizeof(uint32_t));
    }
    NLA_GET_VAL(pWiphy->nStaMax, nla_get_u32, tb[NL80211_ATTR_MAX_AP_ASSOC_STA]);
    s_parseIfTypes(tb, pWiphy);
    s_parseIfCombi(tb, pWiphy);
    s_parseWiphyBands(tb, pWiphy);
    return SWL_RC_OK;
}

/*
 * @brief parse nl msg rate attributes into station info struct
 *
 * @param pBitrateAttributre pointer to rate info struct
 * @param pRate pointer to rateInfo structure to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
static swl_rc_ne wld_nl80211_parseRateInfo(struct nlattr* pBitrateAttributre, wld_nl80211_rateInfo_t* pRate) {
    ASSERTS_NOT_NULL(pBitrateAttributre, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRate, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pRinfo[NL80211_RATE_INFO_MAX + 1];

    static struct nla_policy ratePolicy[NL80211_RATE_INFO_MAX + 1] = {
        [NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
        [NL80211_RATE_INFO_MCS] = { .type = NLA_U8 },
        [NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG },
        [NL80211_RATE_INFO_SHORT_GI] = { .type = NLA_FLAG },
        [NL80211_RATE_INFO_BITRATE32] = { .type = NLA_U32 },

    };
    ratePolicy[NL80211_RATE_INFO_VHT_MCS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_VHT_NSS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_MCS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_NSS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_GI].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_DCM].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_80_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_80P80_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_160_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_10_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_5_MHZ_WIDTH].type = NLA_FLAG;


    rc = nla_parse_nested(pRinfo, NL80211_RATE_INFO_MAX, pBitrateAttributre, ratePolicy);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested RATE attributes!");
        return SWL_RC_ERROR;
    }

    // Get bitrate value in Kbps
    if(pRinfo[NL80211_RATE_INFO_BITRATE32]) {
        pRate->bitrate = 100 * nla_get_u32(pRinfo[NL80211_RATE_INFO_BITRATE32]);
    } else if(pRinfo[NL80211_RATE_INFO_BITRATE]) {
        pRate->bitrate = 100 * nla_get_u16(pRinfo[NL80211_RATE_INFO_BITRATE]);
    } else {
        SAH_TRACEZ_ERROR(ME, "NL80211_RATE_INFO_BITRATE attribute is missing");
        rc = SWL_RC_ERROR;
    }

    // Get protocol info
    swl_mcs_clean(&pRate->mcsInfo);
    pRate->mcsInfo.numberOfSpatialStream = 1;
    // legacy: to allow default stats for a/b/g std and passive rx devices
    pRate->mcsInfo.standard = SWL_STANDARD_LEGACY;

    if(pRinfo[NL80211_RATE_INFO_HE_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_MCS]);
        pRate->mcsInfo.standard = SWL_STANDARD_HE;
        if(pRinfo[NL80211_RATE_INFO_HE_NSS]) {
            pRate->mcsInfo.numberOfSpatialStream = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_NSS]);
        }
        pRate->mcsInfo.guardInterval = SWL_SGI_3200;
    } else if(pRinfo[NL80211_RATE_INFO_VHT_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_VHT_MCS]);
        pRate->mcsInfo.standard = SWL_STANDARD_VHT;

        if(pRinfo[NL80211_RATE_INFO_VHT_NSS]) {
            pRate->mcsInfo.numberOfSpatialStream = nla_get_u8(pRinfo[NL80211_RATE_INFO_VHT_NSS]);
        }
        pRate->mcsInfo.guardInterval = SWL_SGI_800;
    } else if(pRinfo[NL80211_RATE_INFO_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_MCS]);
        pRate->mcsInfo.standard = SWL_STANDARD_HT;
        pRate->mcsInfo.numberOfSpatialStream = 1 + (pRate->mcsInfo.mcsIndex / 8);
        pRate->mcsInfo.guardInterval = SWL_SGI_800;
    }

    // Get guard interval
    if(pRinfo[NL80211_RATE_INFO_HE_GI]) {
        uint8_t sgi = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_GI]);
        if(sgi == NL80211_RATE_INFO_HE_GI_0_8) {
            pRate->mcsInfo.guardInterval = SWL_SGI_800;
        } else if(sgi == NL80211_RATE_INFO_HE_GI_1_6) {
            pRate->mcsInfo.guardInterval = SWL_SGI_1600;
        } else if(sgi == NL80211_RATE_INFO_HE_GI_3_2) {
            pRate->mcsInfo.guardInterval = SWL_SGI_3200;
        }
    } else if(pRinfo[NL80211_RATE_INFO_SHORT_GI]) {
        pRate->mcsInfo.guardInterval = SWL_SGI_400;
    }

    // Get HE DCM (u8, 0/1)
    if(pRinfo[NL80211_RATE_INFO_HE_DCM]) {
        pRate->heDcm = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_DCM]);
    }

    // Get Bandwidth value
    if((pRinfo[NL80211_RATE_INFO_80P80_MHZ_WIDTH]) || (pRinfo[NL80211_RATE_INFO_160_MHZ_WIDTH])) {
        pRate->mcsInfo.bandwidth = SWL_BW_160MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_80_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_80MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_40_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_40MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_10_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_10MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_5_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_5MHZ;
    } else {
        pRate->mcsInfo.bandwidth = SWL_BW_20MHZ;
    }

    swl_mcs_checkMcsIndexes(&pRate->mcsInfo);

    return rc;
}

static swl_rc_ne wld_nl80211_parseSignalPerChain(struct nlattr* pSigPChain, int8_t* sigArray, uint32_t maxNChain, uint32_t* pNChains) {
    ASSERTS_NOT_NULL(pSigPChain, SWL_RC_INVALID_PARAM, ME, "NULL");
    struct nlattr* attr;
    uint32_t i = 0;
    int rem = 0;
    nla_for_each_nested(attr, pSigPChain, rem) {
        if(i >= maxNChain) {
            break;
        }
        sigArray[i++] = (int8_t) nla_get_u8(attr);
    }
    if(pNChains) {
        *pNChains = i;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_parseStationInfo(struct nlattr* tb[], wld_nl80211_stationInfo_t* pStation) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pStation, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pSinfo[NL80211_STA_INFO_MAX + 1];

    static struct nla_policy statsPolicy[NL80211_STA_INFO_MAX + 1] = {
        [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32},
        [NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_RETRIES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
        [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8  },
        [NL80211_STA_INFO_SIGNAL_AVG] = { .type = NLA_U8 },
        [NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
        [NL80211_STA_INFO_RX_BITRATE] = { .type = NLA_NESTED },
        [NL80211_STA_INFO_CONNECTED_TIME] = { .type = NLA_U32},
        [NL80211_STA_INFO_STA_FLAGS] = { .minlen = sizeof(struct nl80211_sta_flag_update) },

    };
    statsPolicy[NL80211_STA_INFO_RX_BYTES64].type = NLA_U64;
    statsPolicy[NL80211_STA_INFO_TX_BYTES64].type = NLA_U64;
    statsPolicy[NL80211_STA_INFO_CHAIN_SIGNAL].type = NLA_NESTED;
    statsPolicy[NL80211_STA_INFO_CHAIN_SIGNAL_AVG].type = NLA_NESTED;

    if(nla_parse_nested(pSinfo, NL80211_STA_INFO_MAX, tb[NL80211_ATTR_STA_INFO], statsPolicy) != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested STA attributes!");
        return SWL_RC_ERROR;
    }
    NLA_GET_DATA(pStation->macAddr.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
    if(pSinfo[NL80211_STA_INFO_INACTIVE_TIME]) {
        pStation->inactiveTime = nla_get_u32(pSinfo[NL80211_STA_INFO_INACTIVE_TIME]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_BYTES64]) {
        pStation->rxBytes = nla_get_u64(pSinfo[NL80211_STA_INFO_RX_BYTES64]);
    } else if(pSinfo[NL80211_STA_INFO_RX_BYTES]) {
        pStation->rxBytes = nla_get_u32(pSinfo[NL80211_STA_INFO_RX_BYTES]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_BYTES64]) {
        pStation->txBytes = nla_get_u64(pSinfo[NL80211_STA_INFO_RX_BYTES64]);
    } else if(pSinfo[NL80211_STA_INFO_TX_BYTES]) {
        pStation->txBytes = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_BYTES]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_PACKETS]) {
        pStation->rxPackets = nla_get_u32(pSinfo[NL80211_STA_INFO_RX_PACKETS]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_PACKETS]) {
        pStation->txPackets = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_PACKETS]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_RETRIES]) {
        pStation->txRetries = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_RETRIES]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_FAILED]) {
        pStation->txFailed = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_FAILED]);
    }
    if(pSinfo[NL80211_STA_INFO_SIGNAL]) {
        pStation->rssiDbm = nla_get_u8(pSinfo[NL80211_STA_INFO_SIGNAL]);
    }
    if(pSinfo[NL80211_STA_INFO_SIGNAL_AVG]) {
        pStation->rssiAvgDbm = nla_get_u8(pSinfo[NL80211_STA_INFO_SIGNAL_AVG]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_BITRATE]) {
        wld_nl80211_parseRateInfo(pSinfo[NL80211_STA_INFO_TX_BITRATE], &pStation->txRate);
    }
    if(pSinfo[NL80211_STA_INFO_RX_BITRATE]) {
        wld_nl80211_parseRateInfo(pSinfo[NL80211_STA_INFO_RX_BITRATE], &pStation->rxRate);
    }
    if(pSinfo[NL80211_STA_INFO_CONNECTED_TIME]) {
        pStation->connectedTime = nla_get_u32(pSinfo[NL80211_STA_INFO_CONNECTED_TIME]);
    }
    struct nl80211_sta_flag_update* pSFlags = NULL;
    pStation->flags.authorized = SWL_TRL_UNKNOWN;
    pStation->flags.authenticated = SWL_TRL_UNKNOWN;
    pStation->flags.associated = SWL_TRL_UNKNOWN;
    pStation->flags.wme = SWL_TRL_UNKNOWN;
    pStation->flags.mfp = SWL_TRL_UNKNOWN;
    if(pSinfo[NL80211_STA_INFO_STA_FLAGS]) {
        pSFlags = (struct nl80211_sta_flag_update*) nla_data(pSinfo[NL80211_STA_INFO_STA_FLAGS]);
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_AUTHORIZED)) {
            pStation->flags.authorized = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_AUTHORIZED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_AUTHENTICATED)) {
            pStation->flags.authenticated = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_AUTHENTICATED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_ASSOCIATED)) {
            pStation->flags.associated = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_ASSOCIATED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_WME)) {
            pStation->flags.wme = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_WME);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_MFP)) {
            pStation->flags.mfp = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_MFP);
        }
    }
    wld_nl80211_parseSignalPerChain(pSinfo[NL80211_STA_INFO_CHAIN_SIGNAL], pStation->rssiDbmByChain, MAX_NR_ANTENNA, &pStation->nrSignalChains);
    wld_nl80211_parseSignalPerChain(pSinfo[NL80211_STA_INFO_CHAIN_SIGNAL_AVG], pStation->rssiAvgDbmByChain, MAX_NR_ANTENNA, NULL);

    return rc;
}
