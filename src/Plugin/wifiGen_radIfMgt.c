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

#include <errno.h>
#include <net/if.h>
#include "wld/wld.h"
#include "wld/wld_radio.h"
#include "wld/wld_util.h"
#include "wld/wld_ssid.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_rad_nl80211.h"
#include "swl/swl_common.h"
#include "wifiGen_rad.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"

#define ME "genRadI"

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
static bool s_shiftMbssIfNotEnoughVaps(T_Radio* pRad, uint32_t reqBss) {
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

static void s_updateRadBaseMac(T_Radio* pRad) {
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

    s_shiftMbssIfNotEnoughVaps(pRad, pRad->macCfg.nrBssRequired);
    if(memcmp(pRad->MACAddr, prevMacAddr.bMac, SWL_MAC_BIN_LEN)) {
        pRad->pFA->mfn_sync_radio(pRad->pBus, pRad, SET);
    }

    SAH_TRACEZ_INFO(ME, "%s: set MAC "SWL_MAC_FMT " %u %p", pRad->Name, SWL_MAC_ARG(pRad->MACAddr), pRad->macCfg.useBaseMacOffset, prevListIt);
}

int wifiGen_vap_setBssid(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERT_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Set mac address "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(pSSID->BSSID));
    int ret = wld_linuxIfUtils_updateMac(wld_rad_getSocket(pAP->pRadio), pAP->alias, (swl_macBin_t*) pSSID->BSSID);
    ASSERT_EQUALS(ret, SWL_RC_OK, ret, ME, "%s: fail to apply mac ["SWL_MAC_FMT "]", pAP->alias, SWL_MAC_ARG(pSSID->BSSID));
    return ret;
}

static bool s_updateVapAlias(T_Radio* pRad, T_AccessPoint* pAP, int if_count) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    if(!if_count) {    // yes
        snprintf(pAP->alias, sizeof(pAP->alias), "%s", pRad->Name);
    } else {
        snprintf(pAP->alias, sizeof(pAP->alias), "%s.%d", pRad->Name, if_count);
    }
    return true;
}

static int s_createAp(T_Radio* pRad, T_AccessPoint* pAP, uint32_t apIndex) {
    swl_rc_ne rc;
    if(!apIndex) {
        rc = wld_rad_nl80211_setAp(pRad);
        pAP->index = pRad->index;
        pAP->wDevId = pRad->wDevId;
    } else {
        rc = wld_rad_nl80211_addVapInterface(pRad, pAP);
    }
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to create ap(%s)id(%d) on rad(%s)", pAP->alias, apIndex, pRad->Name);
    rc = wifiGen_hapd_initVAP(pAP);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to init hapd interface", pAP->alias);
    rc = pAP->pFA->mfn_wvap_setEvtHandlers(pAP);
    return rc;
}

static int s_bssIndex(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    ASSERTS_NOT_NULL(pAP->pRadio, pAP->ref_index, ME, "NULL");
    return (pAP->pRadio->isSTASup + pAP->ref_index);
}

int wifiGen_rad_addVapExt(T_Radio* pRad, T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    // Ap index is number of AP already initialized plus 1 if sta is supported, 0 otherwise.
    int32_t apIndex = s_bssIndex(pAP);
    SAH_TRACEZ_INFO(ME, "%s: add vap - [%s] %u", pRad->Name, pAP->name, apIndex);

    int maxNrAp = pRad->maxNrHwBss;
    ASSERT_FALSE(apIndex >= maxNrAp, WLD_ERROR_INVALID_STATE, ME, "%s: Max Number of BSS reached : %d", pRad->Name, maxNrAp);
    ASSERT_FALSE(wld_rad_getSocket(pRad) < 0, WLD_ERROR_INVALID_STATE, ME, "%s: Unable to create socket", pRad->Name);

    pRad->pFA->mfn_wrad_enable(pRad, 0, SET | DIRECT);

    //check need for updating base mac when adding next interface
    if(apIndex > 0) {
        bool changed = s_shiftMbssIfNotEnoughVaps(pRad, apIndex + 1);
        if(changed) {
            SAH_TRACEZ_WARNING(ME, "%s: Shifting previous %u vap interfaces after mbss mac shift ", pRad->Name, (apIndex - pRad->isSTASup));
            //Brcm requires to re-apply the primary config's cur_etheraddr,
            //in order to clear all previously set secondary config ethernet addresses
            //and allow setting them again with shifted MACs
            //1) if the first interface is the primary vap, then the mac restore is implicitly done, when re-writing bssid.
            //2) otherwise (i.e endpoint is first interface), re-apply mac on radio (as endpoint object may be not yet created on startup)
            if(pRad->isSTASup) {
                wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRad), pRad->Name, (swl_macBin_t*) pRad->MACAddr);
            }
            // update vaps with new shifted mac
            // actually: only secondary interfaces are impacted (i.e: the radio base mac addr does not change)
            T_AccessPoint* tmpAp = NULL;
            wld_rad_forEachAp(tmpAp, pRad) {
                swl_macBin_t macBin = SWL_MAC_BIN_NEW();
                wld_ssid_generateBssid(pRad, tmpAp, s_bssIndex(tmpAp), &macBin);
                wld_ssid_setBssid(tmpAp->pSSID, &macBin);
                tmpAp->pFA->mfn_sync_ssid(tmpAp->pSSID->pBus, tmpAp->pSSID, SET);
                wifiGen_vap_setBssid(tmpAp);
                //sched to reapply via fsm
                tmpAp->pFA->mfn_wvap_bssid(pRad, tmpAp, (uint8_t*) swl_typeMacBin_toBuf32Ref(&macBin).buf, SWL_MAC_CHAR_LEN, SET);
            }
        }
    }

    //retrieve interface name and update pAP->alias
    s_updateVapAlias(pRad, pAP, apIndex);
    // actually create AP, and update netdev index.
    s_createAp(pRad, pAP, apIndex);

    swl_macBin_t macBin = SWL_MAC_BIN_NEW();
    wld_ssid_generateBssid(pRad, pAP, apIndex, &macBin);
    wld_ssid_setBssid(pAP->pSSID, &macBin);

    /* Set network address! */
    wifiGen_vap_setBssid(pAP);

    pRad->pFA->mfn_wrad_enable(pRad, pRad->enable, SET);

    SAH_TRACEZ_INFO(ME, "%s: created interface %s (%s) index %d - %u",
                    pRad->Name, pAP->name, pAP->alias, pAP->index, apIndex);

    return SWL_RC_OK;
}

int wifiGen_rad_delvapif(T_Radio* pRad, char* vapName) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(vapName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTW_FALSE(swl_str_matches(pRad->Name, vapName), SWL_RC_OK, ME, "%s: avoid deleting main rad iface", vapName);
    int ifIndex = if_nametoindex(vapName);
    ASSERT_TRUE(ifIndex > 0, SWL_RC_INVALID_PARAM, ME, "unknown iface (%s)", vapName);
    swl_rc_ne rc = wld_nl80211_delInterface(wld_nl80211_getSharedState(), ifIndex);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to del vap (%s)", vapName);
    return SWL_RC_OK;
}

int wifiGen_rad_addEndpointIf(T_Radio* pRad, char* buf, int bufsize) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(buf, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(bufsize > 0, SWL_RC_INVALID_PARAM, ME, "null size");

    // assumption: endpoint interface is the MAIN radio interface
    const char* epIfname = pRad->Name;
    T_EndPoint* pEpExist = wld_rad_ep_from_name(pRad, epIfname);
    ASSERT_NULL(pEpExist, SWL_RC_ERROR, ME, "%s: already created EP[%s] with ifname(%s)", pRad->Name, pEpExist->alias, pEpExist->Name);
    swl_str_copy(buf, bufsize, epIfname);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), (char*) epIfname, false);
    wld_rad_nl80211_setSta(pRad);
    wld_rad_nl80211_set4Mac(pRad, true);

    /* Return the radio index number */
    return pRad->index;
}

int wifiGen_vap_bssid(T_Radio* pRad, T_AccessPoint* pAP, unsigned char* buf, int bufsize, int set) {
    ASSERT_FALSE((pRad == NULL) && (pAP == NULL), SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = NULL;
    SAH_TRACEZ_INFO(ME, "%p - %p - %p - %d - %d", pRad, pAP, buf, bufsize, set);
    if(set & SET) {
        if(pAP) {
            pSSID = (T_SSID*) pAP->pSSID;
            ASSERT_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
            ASSERT_STR(buf, SWL_RC_INVALID_PARAM, ME, "Empty bssid");
            ASSERT_TRUE(SWL_MAC_CHAR_TO_BIN(pSSID->BSSID, buf),
                        SWL_RC_ERROR, ME, "fail to convert mac(%s) len(%d)", buf, bufsize);
            setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID);
            if(s_bssIndex(pAP) == 0) {
                // if changing first VAP, all secondary vaps also must have their BSSID set.
                setBitLongArray(pAP->pRadio->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID);
                return SWL_RC_OK;
            }
        } else {
            s_updateRadBaseMac(pRad);
            wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRad), pRad->Name, (swl_macBin_t*) pRad->MACAddr);
        }
    } else {
        ASSERTS_NOT_NULL(buf, SWL_RC_INVALID_PARAM, ME, "NULL");
        ASSERTS_TRUE((bufsize >= ETHER_ADDR_STR_LEN), SWL_RC_INVALID_PARAM, ME, "Not enough space");
        swl_macBin_t* srcBMac = NULL;
        if(pAP) {
            pSSID = (T_SSID*) pAP->pSSID;
            ASSERT_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
            srcBMac = (swl_macBin_t*) pSSID->BSSID;
        } else {
            srcBMac = (swl_macBin_t*) pRad->MACAddr;
        }
        // String formatted or octet based ?
        swl_mac_binToCharSep((swl_macChar_t*) buf, srcBMac, false, ':');
        SAH_TRACEZ_INFO(ME, "%s", buf);
    }
    return SWL_RC_OK;
}

