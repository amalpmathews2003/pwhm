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

/*
 * Refer to IEEE Std 802.11-2020 : 9.4.2.45 Multiple BSSID element
 * for details regarding the procedure and required MAC address assignment:
 * All vaps must share same base mac bits except
 * the variable bitmask of the BSS base mac address,
 * that shall be sized with the number of BSS to be created.
 * egs: with 1 or 2 VAPs, only 1 LSb can change
 *      with 3 or 4 VAPS, only 2 LSb can change
 *      with 16 VAPS, only 4 LSb can change
 */
static bool s_useMultiBssidMask(T_Radio* pRad) {
    return (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ);
}

static uint32_t s_getNrReqBss(T_Radio* pRad) {
    uint32_t radNrBssReq = pRad->macCfg.nrBssRequired;
    if(s_useMultiBssidMask(pRad)) {
        uint32_t radNrBssMapped = wld_rad_countMappedAPs(pRad);
        SAH_TRACEZ_INFO(ME, "%s: max %d req %d mapped %d",
                        pRad->Name, pRad->maxNrHwBss, radNrBssReq, radNrBssMapped);
        radNrBssReq = SWL_MAX(radNrBssReq, radNrBssMapped);
    }
    return radNrBssReq;
}

static uint32_t s_getNrCfgBss(T_Radio* pRad) {
    uint32_t radMaxBssConfig = pRad->maxNrHwBss;
    uint32_t radNrBssReq = s_getNrReqBss(pRad);

    if(radNrBssReq == 0) {
        SAH_TRACEZ_INFO(ME, "%s: prev no bss req known, assume max", pRad->Name);
        return radMaxBssConfig;
    }
    return SWL_MIN(radMaxBssConfig, radNrBssReq);
}

static int32_t s_getMacOffset(T_Radio* pRad, T_Radio* prevRad) {
    if(prevRad->macCfg.useLocalBitForGuest) {
        SAH_TRACEZ_INFO(ME, "%s: guest, use 1 offset for base", pRad->Name);
        return 1;
    }

    return s_getNrCfgBss(prevRad);
}

static uint32_t s_getNrCfgBssRoundedPow2(T_Radio* pRad) {
    uint32_t nrCfgBss = s_getNrCfgBss(pRad);
    // ensure rounding into pow2 values
    if((nrCfgBss > 0) && (swl_bit32_getNrSet(nrCfgBss) > 1)) {
        return (1 << (swl_bit32_getHighest(nrCfgBss) + 1));
    }
    return nrCfgBss;
}

/*
 * shift Mbss base mac, if there is not enough available macs (starting from radio base mac) for secondary bss
 */
bool wld_rad_macCfg_shiftMbssIfNotEnoughVaps(T_Radio* pRad, uint32_t reqBss) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    // MaxHwBss is always power of 2
    uint8_t maxHwBss = s_getNrCfgBssRoundedPow2(pRad);
    ASSERT_TRUE(maxHwBss > 0, false, ME, "%s: no supported BSSs", pRad->Name);

    SAH_TRACEZ_INFO(ME, "%s: maxHwBss %d reqBss %d useLocalBitForGuest %d",
                    pRad->Name, maxHwBss, reqBss, pRad->macCfg.useLocalBitForGuest);

    if((reqBss == 0) || pRad->macCfg.useLocalBitForGuest) {
        // No requirement or using guest, all is fine.
        return false;
    }

    // calculate nr bss available, by checking last byte mask
    uint8_t* mbssBaseMACAddr = pRad->mbssBaseMACAddr.bMac;
    uint8_t bitMask = mbssBaseMACAddr[5] % maxHwBss;

    uint32_t nrAvailable = maxHwBss - bitMask;
    bool nextToCycle = (bitMask == maxHwBss - 1);
    bool useMultiBssidMask = s_useMultiBssidMask(pRad);
    if(nextToCycle && useMultiBssidMask) {
        nextToCycle = false;
        SAH_TRACEZ_WARNING(ME, "%s: mbss mac not allowed to cycle because Multi-BSSID rule", pRad->Name);
    }
    if((nrAvailable >= reqBss) || nextToCycle) {
        return false;
    }
    swl_macBin_t oldBMac;
    memcpy(&oldBMac, mbssBaseMACAddr, sizeof(oldBMac));
    swl_mac_binAddVal((swl_macBin_t*) mbssBaseMACAddr, bitMask, 18);
    SAH_TRACEZ_WARNING(ME, "%s : shifting MBSS BASE MAC from "MAC_PRINT_FMT " to "MAC_PRINT_FMT
                       " because reqBss %u exceeds available %u of max %u => jump %u",
                       pRad->Name, MAC_PRINT_ARG(oldBMac.bMac), MAC_PRINT_ARG(mbssBaseMACAddr),
                       reqBss, nrAvailable, maxHwBss, bitMask);
    /*
     * New mbssBaseMACAddr shall be mirrored to Radio when :
     * - main radio iface is primary vap
     * - strict 11ax MultiBSSID match between primary vap and radio
     */
    if((!pRad->isSTASup) && useMultiBssidMask) {
        SAH_TRACEZ_WARNING(ME, "%s : Force Update radio MAC with MultiBSSID Base mac "MAC_PRINT_FMT,
                           pRad->Name, MAC_PRINT_ARG(mbssBaseMACAddr));
        memcpy(pRad->MACAddr, pRad->mbssBaseMACAddr.bMac, SWL_MAC_BIN_LEN);
    }
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

    wld_rad_macCfg_shiftMbssIfNotEnoughVaps(pRad, s_getNrReqBss(pRad));
    if(memcmp(pRad->MACAddr, prevMacAddr.bMac, SWL_MAC_BIN_LEN)) {
        pRad->pFA->mfn_sync_radio(pRad->pBus, pRad, SET);
        change = true;
    }

    SAH_TRACEZ_INFO(ME, "%s: set MAC "SWL_MAC_FMT " %u %p", pRad->Name, SWL_MAC_ARG(pRad->MACAddr), pRad->macCfg.useBaseMacOffset, prevListIt);
    return change;
}

/**
 * EP MAC is generated based on EP Mac rank and beyond bitmask used for radio's BSSs
 */
swl_rc_ne wld_rad_macCfg_generateEpMac(T_Radio* pRad, const char* ifname, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(macBin, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "No mapped radio");
    if(pRad->isSTASup) {
        if(!index) {
            SAH_TRACEZ_INFO(ME, "%s: use main rad mac as EP rank(%d) mac "SWL_MAC_FMT, pRad->Name, index, SWL_MAC_ARG(pRad->MACAddr));
            memcpy(macBin->bMac, pRad->MACAddr, ETHER_ADDR_LEN);
            return SWL_RC_OK;
        }
        SAH_TRACEZ_WARNING(ME, "%s: force generate EP %s rank(%d) mac", pRad->Name, ifname, index);
    }
    uint32_t nrSuppBss = pRad->maxNrHwBss;
    uint32_t nrMaskBit = swl_bit32_getHighest(nrSuppBss);
    unsigned char* baseMacAddr = pRad->MACAddr;
    memcpy(macBin->bMac, baseMacAddr, ETHER_ADDR_LEN);
    //skip all the bits that may be used for BSSID generation
    swl_mac_binAddVal(macBin, nrSuppBss * (1 + index), 18);
    SAH_TRACEZ_INFO(ME, "RADIO %s gen EP iface %s rank(%d) MAC "SWL_MAC_FMT " Base "SWL_MAC_FMT " jump maskBit %u, supBss %u",
                    pRad->Name, ifname, index,
                    SWL_MAC_ARG(macBin->bMac), SWL_MAC_ARG(baseMacAddr), nrMaskBit, nrSuppBss);
    return SWL_RC_OK;
}

/**
 * BSSID is generated based on AP Bssid rank and radio's available BSSIDs
 */
swl_rc_ne wld_rad_macCfg_generateBssid(T_Radio* pRad, const char* ifname, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(macBin, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "No mapped radio");

    // Sync with the MAC created by BSSID!
    // Create our Virtual MAC
    uint32_t nrSuppBss = s_getNrCfgBssRoundedPow2(pRad);
    ASSERT_TRUE(nrSuppBss > 0, SWL_RC_ERROR, ME, "%s: no supported BSSs", pRad->Name);
    uint32_t nrMaskBit = swl_bit32_getHighest(nrSuppBss);

    unsigned char* baseMacAddr = pRad->MACAddr;
    if(index > 0) {
        baseMacAddr = pRad->mbssBaseMACAddr.bMac;
    }
    memcpy(macBin->bMac, baseMacAddr, ETHER_ADDR_LEN);

    uint8_t bitMask = baseMacAddr[5] % nrSuppBss;
    bool useMultiBssidMask = s_useMultiBssidMask(pRad);

    if((bitMask == nrSuppBss - 1) && (!useMultiBssidMask)) {
        //mac bitmask not allowed to overflow with 11ax MultiBSSID rule
        nrMaskBit = 18; //3 bytes LSB of Mac (the deviceId part)
    } else if(bitMask + index >= nrSuppBss) {
        SAH_TRACEZ_ERROR(ME, "%s error has not enough iface %s MACs %u + %u >= %u. Cycling BSS",
                         pRad->Name, ifname, index, bitMask, nrSuppBss);
    }

    swl_mac_binAddVal(macBin, index, nrMaskBit);
    //if mac shift occured, then skip generated mac matching the mbss base mac bitmask
    //as bcrm drv verifies that there isn't a collision with any other bss configs (including primary bss).
    if(memcmp(baseMacAddr, pRad->MACAddr, ETHER_ADDR_LEN) &&
       ((macBin->bMac[5] % nrSuppBss) >= (pRad->MACAddr[5] % nrSuppBss))) {
        swl_mac_binAddVal(macBin, 1, nrMaskBit);
    }

    /* IEEE standardized MAC address assigning on 6GHz:
     * All BSSs must share same base Mac address: only the bitmask of the created BSSs is allowed to change;
     * no other modifiers, like LAA bit, or guestMacOffset.
     * Eg: with maxHwBss 16 on rad 6GHz, 5 1/2 bytes must be the same on all BSSIDs
     */
    if(index && pRad->macCfg.useLocalBitForGuest && (!useMultiBssidMask)) {
        macBin->bMac[0] |= 0x02;    // Set on guest interfaces the locally administered bit.
        // Only allow offset when using local mac
        swl_mac_binAddVal(macBin, pRad->macCfg.localGuestMacOffset * (1 + pRad->ref_index), -1);
    }

    SAH_TRACEZ_INFO(ME, "RADIO %s gen BSS iface %s rank(%d) MAC "SWL_MAC_FMT " Base "SWL_MAC_FMT " maskBit %u, supBss %u",
                    pRad->Name, ifname, index,
                    SWL_MAC_ARG(macBin->bMac), SWL_MAC_ARG(baseMacAddr), nrMaskBit, nrSuppBss);
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_macCfg_generateDummyBssid(T_Radio* pRad, const char* ifname, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(macBin, SWL_RC_INVALID_PARAM, ME, "NULL");

    memcpy(macBin->bMac, &g_swl_macBin_null, SWL_MAC_BIN_LEN);
    // Set 3 LSB bytes based on radio base mac
    memcpy(&macBin->bMac[3], &pRad->MACAddr[3], 3);
    // Set on local interfaces the locally administered bit.
    macBin->bMac[0] |= 0x02;
    // Use guest offset increment for local interfaces mac
    swl_mac_binAddVal(macBin, (1 << 8) * (1 + pRad->ref_index), -1);
    swl_mac_binAddVal(macBin, index, 18);
    SAH_TRACEZ_WARNING(ME, "RADIO %s dummy BSS iface %s rank(%d) MAC "SWL_MAC_FMT " Rad "SWL_MAC_FMT,
                       pRad->Name, ifname, index,
                       SWL_MAC_ARG(macBin->bMac), SWL_MAC_ARG(pRad->MACAddr));
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
