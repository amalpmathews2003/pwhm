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
 * This file implements nl80211 api debug functions (dump)
 */

#include "wld_nl80211_debug.h"
#include "wld_channel.h"
#include "swl/swl_common.h"

#define ME "nlDbg"

static void s_dumpVar(amxc_var_t* retVar) {
    amxc_string_t retStr;
    amxc_string_init(&tmpString, 0);
    amxc_string_csv_join_var(&tmpString, retVar);
    SAH_TRACEZ_INFO(ME, "%s", amxc_string_get(&retStr, 0));
    amxc_string_clean(&retStr);
}

static void s_dumpMap(amxc_var_t* retMap) {
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_copy(&localVar, retMap);
    s_dumpVar(&localVar);
    amxc_var_clean(&localVar);
}

swl_rc_ne wld_nl80211_dumpIfaceInfo(wld_nl80211_ifaceInfo_t* pIfaceInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pIfaceInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");
    amxc_var_add_key(cstring_t, pMap, "name", pIfaceInfo->name);
    amxc_var_add_key(uint32_t, pMap, "wiphy", pIfaceInfo->wiphy);
    amxc_var_add_key(uint32_t, pMap, "ifIndex", pIfaceInfo->ifIndex);
    swl_macChar_t macChar;
    swl_mac_binToChar(&macChar, &pIfaceInfo->mac);
    amxc_var_add_key(cstring_t, pMap, "mac", macChar.cMac);
    amxc_string_t typeStr;
    amxc_string_init(&typeStr, 0);
    amxc_string_append(&typeStr, (pIfaceInfo->isMain ? "main" : "virtual"), amxc_string_buffer_length(&typeStr));
    amxc_string_append(&typeStr, (pIfaceInfo->isAp ? "AP" : ""), amxc_string_buffer_length(&typeStr));
    amxc_string_append(&typeStr, (pIfaceInfo->isSta ? "STA" : ""), amxc_string_buffer_length(&typeStr));
    amxc_var_add_key(cstring_t, pMap, "type", amxc_string_get(&typeStr, 0));
    amxc_string_clean(&typeStr);
    amxc_var_add_key(uint32_t, pMap, "freq", pIfaceInfo->chanSpec.ctrlFreq);
    amxc_var_add_key(uint32_t, pMap, "bw", pIfaceInfo->chanSpec.chanWidth);
    amxc_var_add_key(uint32_t, pMap, "txPwr", pIfaceInfo->txPower);
    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_dumpWiphyInfo(wld_nl80211_wiphyInfo_t* pWiphyInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pWiphyInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");
    amxc_var_add_key(cstring_t, pMap, "name", pWiphyInfo->name);
    amxc_var_add_key(uint32_t, pMap, "wiphy", pWiphyInfo->wiphy);
    amxc_var_t* nAntMap = amxc_var_add_key(amxc_htable_t, pMap, "nrAntenna", NULL);
    for(uint32_t i = 0; i < COM_DIR_MAX; i++) {
        amxc_var_add_key(int32_t, nAntMap, com_dir_char[i], pWiphyInfo->nrAntenna[i]);
    }
    amxc_var_t* nActAntMap = amxc_var_add_key(amxc_htable_t, pMap, "nrActiveAntenna", NULL);
    for(uint32_t i = 0; i < COM_DIR_MAX; i++) {
        amxc_var_add_key(int32_t, nActAntMap, com_dir_char[i], pWiphyInfo->nrActiveAntenna[i]);
    }
    amxc_var_add_key(uint32_t, pMap, "nrApMax", pWiphyInfo->nApMax);
    amxc_var_add_key(uint32_t, pMap, "nrEpMax", pWiphyInfo->nEpMax);
    amxc_var_add_key(uint32_t, pMap, "nrStaMax", pWiphyInfo->nStaMax);
    amxc_var_add_key(bool, pMap, "suppUAPSD", pWiphyInfo->suppUAPSD);

    amxc_var_t* cipherSuitesList = amxc_var_add_key(amxc_llist_t, pMap, "suppCiphers", NULL);

    amxc_string_t cipherStr;
    amxc_string_init(&cipherStr, 0);
    for(uint32_t i = 0; i < pWiphyInfo->nCipherSuites; i++) {
        amxc_string_clean(&cipherStr);
        amxc_string_appendf(&cipherStr, "%08x", pWiphyInfo->cipherSuites[i]);
        amxc_var_add(cstring_t, cipherSuitesList, amxc_string_get(&cipherStr, 0));
    }
    amxc_string_clean(&cipherStr);

    char buffer[128] = {0};
    swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->freqBandsMask, swl_freqBand_str, SWL_FREQ_BAND_MAX);
    amxc_var_add_key(cstring_t, pMap, "freqBands", buffer);
    amxc_var_t* bandsMap = amxc_var_add_key(amxc_htable_t, pMap, "Bands", NULL);
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        if(pWiphyInfo->bands[i].nChans == 0) {
            continue;
        }
        amxc_var_t* bandMap = amxc_var_add_key(amxc_htable_t, bandsMap, swl_freqBand_str[pWiphyInfo->bands[i].freqBand], NULL);
        swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].radStdsMask, swl_radStd_str, SWL_RADSTD_MAX);
        amxc_var_add_key(cstring_t, bandMap, "suppRadStds", buffer);
        swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].chanWidthMask, swl_bandwidth_str, SWL_BW_MAX);
        amxc_var_add_key(cstring_t, bandMap, "suppChanWidth", buffer);
        amxc_var_add_key(uint32_t, bandMap, "nrSSMax", pWiphyInfo->bands[i].nSSMax);
        uint32_t j;
        amxc_var_t* mcsStdsList = amxc_var_add_key(amxc_llist_t, bandMap, "suppMcsStds", NULL);
        for(j = 0; j < SWL_STANDARD_MAX; j++) {
            if(pWiphyInfo->bands[i].mcsStds[j].standard == SWL_STANDARD_UNKNOWN) {
                continue;
            }
            if(swl_mcs_toChar(buffer, sizeof(buffer), &pWiphyInfo->bands[i].mcsStds[j]) > 0) {
                amxc_var_add(cstring_t, mcsStdsList, buffer);
            }
        }
        amxc_var_t* bfMap = amxc_var_add_key(amxc_htable_t, bandMap, "suppBeamForming", NULL);
        for(j = 0; j < COM_DIR_MAX; j++) {
            swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].bfCapsSupported[j],
                                g_str_wld_rad_bf_cap, RAD_BF_CAP_MAX);
            amxc_var_add_key(cstring_t, bfMap, com_dir_char[j], buffer);
        }
        amxc_var_t* chanList = amxc_var_add_key(amxc_llist_t, bandMap, "suppChannels", NULL);
        for(j = 0; j < pWiphyInfo->bands[i].nChans; j++) {
            amxc_var_t* chanMap = amxc_var_add(amxc_htable_t, chanList, NULL);
            amxc_var_add_key(uint32_t, chanMap, "ctrlFreq", pWiphyInfo->bands[i].chans[j].ctrlFreq);
            amxc_var_add_key(uint32_t, chanMap, "maxTxPwr", pWiphyInfo->bands[i].chans[j].maxTxPwr);
            if(pWiphyInfo->bands[i].chans[j].isDfs) {
                amxc_var_t* dfsInfoMap = amxc_var_add_key(amxc_htable_t, chanMap, "dfs", NULL);
                amxc_var_add_key(cstring_t, dfsInfoMap, "status",
                                 g_str_wld_nl80211_chan_status_bf_cap[pWiphyInfo->bands[i].chans[j].status]);
                amxc_var_add_key(uint32_t, dfsInfoMap, "stateTime", pWiphyInfo->bands[i].chans[j].dfsTime);
                amxc_var_add_key(uint32_t, dfsInfoMap, "cacTime", pWiphyInfo->bands[i].chans[j].dfsCacTime);
            }
        }
    }

    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

static swl_rc_ne wld_nl80211_dumpRateInfo(wld_nl80211_rateInfo_t* pRateInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pRateInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    variant_t localVar;
    variant_initialize(&localVar, variant_type_map);
    variant_map_t* pMap = (retMap ? retMap : variant_da_map(&localVar));
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");

    char buffer[128] = {0};
    swl_mcs_toChar(buffer, sizeof(buffer), &pRateInfo->mscInfo);

    variant_map_addUInt32(pMap, "Bitrate", pRateInfo->bitrate);
    variant_map_addChar(pMap, "mscInfo", buffer);
    variant_map_addUInt8(pMap, "HeDcm", pRateInfo->heDcm);

    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_dumpStationInfo(wld_nl80211_stationInfo_t* pStationInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pStationInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    variant_t localVar;
    variant_initialize(&localVar, variant_type_map);
    variant_map_t* pMap = (retMap ? retMap : variant_da_map(&localVar));
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");

    swl_macChar_t cMacAddr;
    swl_mac_binToChar(&cMacAddr, &pStationInfo->macAddr);

    variant_map_addChar(pMap, "MacAddress", cMacAddr.cMac);
    variant_map_addUInt32(pMap, "inactiveTimeMs", pStationInfo->inactiveTime);
    variant_map_addUInt32(pMap, "rxBytes", pStationInfo->rxBytes);
    variant_map_addUInt32(pMap, "txBytes", pStationInfo->txBytes);
    variant_map_addUInt32(pMap, "rxPackets", pStationInfo->rxPackets);
    variant_map_addUInt32(pMap, "txPackets", pStationInfo->txPackets);
    variant_map_addUInt32(pMap, "txRetries", pStationInfo->txRetries);
    variant_map_addUInt32(pMap, "txFailed", pStationInfo->txFailed);
    variant_map_addInt8(pMap, "rssiDbm", pStationInfo->rssiDbm);
    variant_map_addInt8(pMap, "rssiAvgDbm", pStationInfo->rssiAvgDbm);
    variant_map_addMap(pMap, "rxRate", NULL);
    variant_map_t* rxRate = variant_map_da_findMap(pMap, "rxRate");
    wld_nl80211_dumpRateInfo(&pStationInfo->rxRate, rxRate);
    variant_map_addMap(pMap, "txRate", NULL);
    variant_map_t* txRate = variant_map_da_findMap(pMap, "txRate");
    wld_nl80211_dumpRateInfo(&pStationInfo->txRate, txRate);

    return SWL_RC_OK;
}
