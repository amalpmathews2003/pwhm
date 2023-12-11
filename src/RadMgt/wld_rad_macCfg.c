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

#include <debug/sahtrace.h>

#include "wld/wld.h"
#include "wld/wld_radio.h"
#include "wld/wld_util.h"
#include "wld/wld_ssid.h"
#include "swl/swl_common.h"
#include "swl/swl_common_mac.h"

#define ME "radMacC"

static int32_t s_getMacOffset(T_Radio* pRad, T_Radio* prevRad) {
    uint32_t prevRadMaxBssConfig = prevRad->maxNrHwBss;
    uint32_t prevRadNrBssReq = prevRad->macCfg.nrBssRequired;

    if(prevRad->macCfg.useLocalBitForGuest) {
        SAH_TRACEZ_INFO(ME, "%s: guest, use 1 offset for base", pRad->Name);
        return 1;
    }
    if(prevRadNrBssReq == 0) {
        SAH_TRACEZ_INFO(ME, "%s: prev no bss req known, assume max", pRad->Name);
        return prevRadMaxBssConfig;
    }
    return SWL_MIN(prevRadMaxBssConfig, prevRadNrBssReq);
}

/*
 * shift Mbss base mac, if there is not enough available macs (starting from radio base mac) for secondary bss
 */
bool wld_rad_macCfg_shiftMbssIfNotEnoughVaps(T_Radio* pRad, uint32_t reqBss) {
    uint8_t maxHwBss = pRad->maxNrHwBss;

    if((reqBss == 0) || pRad->macCfg.useLocalBitForGuest) {
        // No requirement or using guest, all is fine.
        return false;
    }

    // calculate nr bss available, by checking last bit mask
    uint8_t* mbssBaseMACAddr = pRad->mbssBaseMACAddr.bMac;
    uint8_t bitMask = mbssBaseMACAddr[5] % maxHwBss;

    uint32_t nrAvailable = maxHwBss - bitMask;
    bool nextToCycle = (bitMask == maxHwBss - 1);
    if((nrAvailable >= reqBss) || nextToCycle) {
        return false;
    }
    uint8_t orBitMask = maxHwBss - 1; // MaxHwBss is always multiple of 2
    mbssBaseMACAddr[5] |= orBitMask;
    SAH_TRACEZ_WARNING(ME, "%s : shifting MBSS BASE MAC so max nr of BSS available, maxBss %u mask 0x%.2x " MAC_PRINT_FMT, pRad->Name, maxHwBss, orBitMask,
                       MAC_PRINT_ARG(mbssBaseMACAddr));
    return true;
}

/*
 * update radio base mac address, considering required BSSs, and previous radio number of child BSSs
 */
bool wld_rad_macCfg_updateRadBaseMac(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    bool change = false;
    amxc_llist_it_t* prevListIt = pRad->it.prev;
    swl_macBin_t prevMacAddr;
    memcpy(prevMacAddr.bMac, pRad->MACAddr, SWL_MAC_BIN_LEN);

    if(pRad->macCfg.useBaseMacOffset) {
        memcpy(pRad->MACAddr, wld_getWanAddr()->bMac, ETHER_ADDR_LEN);
        swl_mac_binAddVal((swl_macBin_t*) pRad->MACAddr, pRad->macCfg.baseMacOffset, -1);
    } else if(prevListIt != NULL) {
        T_Radio* prevRad = amxc_llist_it_get_data(prevListIt, T_Radio, it);
        memcpy(pRad->MACAddr, prevRad->mbssBaseMACAddr.bMac, SWL_MAC_BIN_LEN);
        swl_mac_binAddVal((swl_macBin_t*) pRad->MACAddr, s_getMacOffset(pRad, prevRad), -1);
    }
    memcpy(pRad->mbssBaseMACAddr.bMac, pRad->MACAddr, SWL_MAC_BIN_LEN);

    wld_rad_macCfg_shiftMbssIfNotEnoughVaps(pRad, pRad->macCfg.nrBssRequired);
    if(memcmp(pRad->MACAddr, prevMacAddr.bMac, SWL_MAC_BIN_LEN)) {
        pRad->pFA->mfn_sync_radio(pRad->pBus, pRad, SET);
        change = true;
    }

    SAH_TRACEZ_INFO(ME, "%s: set MAC "SWL_MAC_FMT " %u %p", pRad->Name, SWL_MAC_ARG(pRad->MACAddr), pRad->macCfg.useBaseMacOffset, prevListIt);
    return change;
}

/**
 * BSSID/MAC of pAP/pEP will be generated from interface Index (i.e rank) and from the number of MACAddresses
 * being available per configuration.
 */
swl_rc_ne wld_rad_macCfg_generateMac(T_Radio* pRad, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(macBin, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "No mapped radio");

    // Sync with the MAC created by BSSID!
    // Create our Virtual MAC
    uint32_t nrMaskBit = swl_bit32_getHighest(pRad->maxNrHwBss);

    unsigned char* baseMacAddr = pRad->MACAddr;
    if(index > 0) {
        baseMacAddr = pRad->mbssBaseMACAddr.bMac;
    }
    memcpy(macBin->bMac, baseMacAddr, ETHER_ADDR_LEN);

    uint8_t bitMask = baseMacAddr[5] % pRad->maxNrHwBss;

    if((baseMacAddr[5] % pRad->maxNrHwBss) == pRad->maxNrHwBss - 1) {
        nrMaskBit = -1;
    } else if(bitMask + index >= pRad->maxNrHwBss) {
        SAH_TRACEZ_ERROR(ME, "%s error has not enough iface MACs %u + %u >= %u. Cycling BSS",
                         pRad->Name, index, bitMask, pRad->maxNrHwBss);
    }

    swl_mac_binAddVal(macBin, index, nrMaskBit);
    //if mac shift occured, then skip generated mac matching the mbss base mac bitmask
    //as bcrm drv verifies that there isn't a collision with any other bss configs (including primary bss).
    if(memcmp(baseMacAddr, pRad->MACAddr, ETHER_ADDR_LEN) &&
       ((macBin->bMac[5] % pRad->maxNrHwBss) >= (pRad->MACAddr[5] % pRad->maxNrHwBss))) {
        swl_mac_binAddVal(macBin, 1, nrMaskBit);
    }

    /* IEEE standardized MAC address assignation on 6GHz and 5 1/2 bytes must be the same */
    if(index && pRad->macCfg.useLocalBitForGuest && (pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ)) {
        macBin->bMac[0] |= 0x02;    // Set on guest interfaces the locally administered bit.
        // Only allow offset when using local mac
        swl_mac_binAddVal(macBin, pRad->macCfg.localGuestMacOffset * (1 + pRad->ref_index), -1);
    }

    SAH_TRACEZ_INFO(ME, "RADIO %s gen iface rank(%d) MAC "SWL_MAC_FMT " Base "SWL_MAC_FMT " maskBit %u, supBss %u",
                    pRad->Name, index,
                    SWL_MAC_ARG(macBin->bMac), SWL_MAC_ARG(baseMacAddr), nrMaskBit, pRad->maxNrHwBss);
    return SWL_RC_OK;
}

static void s_setMACConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: update mac config", pRad->Name);

    pRad->macCfg.useBaseMacOffset = amxd_object_get_bool(object, "UseBaseMacOffset", NULL);
    pRad->macCfg.useLocalBitForGuest = amxd_object_get_bool(object, "UseLocalBitForGuest", NULL);
    pRad->macCfg.baseMacOffset = amxd_object_get_int64_t(object, "BaseMacOffset", NULL);
    pRad->macCfg.localGuestMacOffset = amxd_object_get_int64_t(object, "LocalGuestMacOffset", NULL);
    pRad->macCfg.nrBssRequired = amxd_object_get_uint32_t(object, "NrBssRequired", NULL);

    //Update base mac when adding first interface, as we have all needed macConfig details
    if(!wld_rad_countIfaces(pRad)) {
        char macStr[SWL_MAC_CHAR_LEN] = {0};
        // Apply the MAC address on the radio, update if needed inside vendor
        SWL_MAC_BIN_TO_CHAR(macStr, pRad->MACAddr);
        pRad->pFA->mfn_wvap_bssid(pRad, NULL, (unsigned char*) macStr, SWL_MAC_CHAR_LEN, SET);
        // update macStr as macBin may be shifted inside vendor
        SWL_MAC_BIN_TO_CHAR(macStr, pRad->MACAddr);
        SAH_TRACEZ_WARNING(ME, "%s: vendor %s updated baseMac %s", pRad->Name, pRad->vendor->name, macStr);
    }

    wld_rad_doSync(pRad);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sMACConfigDmHdlrs, ARR(), .objChangedCb = s_setMACConfig_ocf);

void _wld_rad_setMACConfig_ocf(const char* const sig_name,
                               const amxc_var_t* const data,
                               void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sMACConfigDmHdlrs, sig_name, data, priv);
}
