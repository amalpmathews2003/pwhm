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
#include "wld/wld.h"
#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "swl/swl_bit.h"
#include "swl/swl_hex.h"
#include "swl/swl_80211.h"

#define ME "genStaC"

static void s_parseHtCap(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap,
                         swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_htCapIE_t* htCap = (swl_80211_htCapIE_t*) (frm);
    ASSERTS_NOT_NULL(htCap, , ME, "NULL");
    ASSERTS_NOT_NULL(cap, , ME, "NULL");
    if(htCap->htCapInfo & M_SWL_80211_HTCAPINFO_CAP_40) {
        cap->htCapabilities |= M_SWL_STACAP_HT_40MHZ;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HT_40MHZ cap");
    }
    if(htCap->htCapInfo & M_SWL_80211_HTCAPINFO_SGI_20) {
        cap->htCapabilities |= M_SWL_STACAP_HT_SGI20;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HT_SGI20 cap");
    }
    if(htCap->htCapInfo & M_SWL_80211_HTCAPINFO_SGI_40) {
        cap->htCapabilities |= M_SWL_STACAP_HT_SGI40;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HT_SGI40 cap");
    }
    if(htCap->htCapInfo & M_SWL_80211_HTCAPINFO_INTOL_40) {
        cap->htCapabilities |= M_SWL_STACAP_HT_40MHZ_INTOL;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HT_40MHZ_INTOL cap");
    }
    SAH_TRACEZ_INFO(ME, "Parsing HT Capabilities from %02x to %x", htCap->htCapInfo, cap->htCapabilities);
}


static void s_parseVhtCap(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap,
                          swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_vhtCapIE_t* vhtCap = (swl_80211_vhtCapIE_t*) (frm);
    ASSERTS_NOT_NULL(vhtCap, , ME, "NULL");
    ASSERTS_NOT_NULL(cap, , ME, "NULL");
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_SGI_80) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_SGI80;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_SGI80 cap");
    }
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_SGI_160) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_SGI160;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_SGI160 cap");
    }
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_SU_BFR) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_SU_BFR;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_SU_BFR cap");
    }
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_SU_BFE) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_SU_BFE;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_SU_BFE cap");
    }
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_MU_BFR) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_MU_BFR;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_MU_BFR cap");
    }
    if(vhtCap->vhtCapInfo & M_SWL_80211_VHTCAPINFO_MU_BFE) {
        cap->vhtCapabilities |= M_SWL_STACAP_VHT_MU_BFE;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_VHT_MU_BFE cap");
    }
    SAH_TRACEZ_INFO(ME, "Parsing VHT Capabilities from %04x to %x", vhtCap->vhtCapInfo, cap->vhtCapabilities);
}


static void s_parseExtHeOp(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap _UNUSED,
                           swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_heOpIE_t* heopcap = (swl_80211_heOpIE_t*) frm;
    ASSERTS_NOT_NULL(heopcap, , ME, "NULL");
}

static void s_parseExtHeCap(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap,
                            swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_heCapIE_t* hecap = (swl_80211_heCapIE_t*) frm;
    ASSERTS_NOT_NULL(hecap, , ME, "NULL");
    char phy_cap_hex[64] = {'\0'};
    for(uint8_t i = 0; i < SWL_80211_HECAP_PHY_CAP_INFO_SIZE; i++) {
        char hex_dump[3] = {'\0'};
        snprintf(hex_dump, sizeof(hex_dump), "%02x", hecap->phyCap.cap[i]);
        swl_str_cat(phy_cap_hex, sizeof(phy_cap_hex), hex_dump);
    }
    if(hecap->phyCap.cap[3] & 0x40) {
        cap->heCapabilities |= M_SWL_STACAP_HE_SU_BFR;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HE_SU_BFR cap");
    }
    if(hecap->phyCap.cap[4] & 0x01) {
        cap->heCapabilities |= M_SWL_STACAP_HE_SU_AND_MU_BFE;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HE_SU_AND_MU_BFE cap");
    }
    if((hecap->phyCap.cap[4] & 0x02) >> 1) {
        cap->heCapabilities |= M_SWL_STACAP_HE_MU_BFR;
        SAH_TRACEZ_INFO(ME, "Adding M_SWL_STACAP_HE_MU_BFR cap");
    }
    SAH_TRACEZ_INFO(ME, "Parsing HE Capabilities from %s to %x", phy_cap_hex, cap->heCapabilities);
}

SWL_TABLE(ieParseExtTable,
          ARR(uint8_t index; void* val; ),
          ARR(swl_type_uint8, swl_type_voidPtr),
          ARR({SWL_80211_EL_IDEXT_HE_OP, (void*) s_parseExtHeOp},
              {SWL_80211_EL_IDEXT_HE_CAP, (void*) s_parseExtHeCap}, )
          );
typedef void (* elementParseExtFun_f) (T_AccessPoint* pAP, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap, swl_80211_elIdExt_ne elId, uint8_t len, swl_bit8_t* frm);



static void s_parseExt(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap,
                       swl_80211_elId_ne elId _UNUSED, uint8_t len, uint8_t* frm) {
    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERT_TRUE(len != 0, , ME, "ZERO LEN");

    swl_80211_elIdExt_ne elIdExt = frm[0];


    void** func = (void**) swl_table_getMatchingValue(&ieParseExtTable, 1, 0, &elIdExt);
    if(func != NULL) {
        elementParseExtFun_f myFun = (elementParseExtFun_f) (*func);
        myFun(pAP, pDev, cap, elIdExt, len - 1, &frm[1]);
    } else {
        SAH_TRACEZ_INFO(ME, "Unsupported element id ext %d", elIdExt);
    }

}

static void s_parseExtCap(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap _UNUSED,
                          swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Parsing Extra Capabilities %p %d", frm, (int) frm[1]);
    if(frm[4] & 0x01) { // Just check if QoS MAP is set.
        pDev->capabilities |= M_SWL_STACAP_QOS_MAP;
    }
}

static void s_parseVendor(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap,
                          swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_vendorIdEl_t* vendor = (swl_80211_vendorIdEl_t*) frm;
    ASSERTS_NOT_NULL(vendor, , ME, "NULL");
    ASSERTS_NOT_NULL(cap, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Parse associated STA IEs");
    swl_oui_list_t* vendorOUI = &cap->vendorOUI;
    ASSERTS_NOT_NULL(vendorOUI, , ME, "NULL");

    swl_oui_t newOui;
    memcpy(newOui.ouiBytes, vendor->oui, SWL_OUI_BYTE_LEN);
    if(swl_typeOui_arrayContains(vendorOUI->oui, vendorOUI->count, newOui)) {
        return;
    }

    ASSERTS_TRUE(vendorOUI->count < SWL_OUI_MAX_NUM, , ME, "Max number of OUI reached");

    memcpy(vendorOUI->oui[vendorOUI->count].ouiBytes, newOui.ouiBytes, SWL_OUI_BYTE_LEN);

    SAH_TRACEZ_INFO(ME, "Parsing Vendor ID=%d OUI=%s", elId, swl_typeOui_toBuf32(newOui).buf);
    vendorOUI->count++;
}


static void s_parseMobDom(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap _UNUSED,
                          swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    swl_80211_mobDomEl_t* mdie = (swl_80211_mobDomEl_t*) frm;
    ASSERTS_NOT_NULL(mdie, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");
    if((mdie->cap & SWL_80211_MOB_DOM_CAP_FBSS_TRANS_DS) == SWL_80211_MOB_DOM_CAP_FBSS_TRANS_DS) {
        pDev->capabilities |= M_SWL_STACAP_FBT;
    }
}

static void s_parseRegClass(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev _UNUSED, wld_assocDev_capabilities_t* cap,
                            swl_80211_elId_ne elId _UNUSED, uint8_t len, uint8_t* frm) {
    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");
    swl_freqBand_e freqBand;
    for(uint8_t i = 1; i < len; i++) { //Start at 1 to skip current operating class
        freqBand = swl_chanspec_freqBandExtToFreqBand(swl_chanspec_operClassToFreq(frm[i]), SWL_FREQ_BAND_MAX, NULL);
        if(freqBand == SWL_FREQ_BAND_MAX) {
            continue;
        }
        W_SWL_BIT_SET(cap->freqCapabilities, freqBand);
    }
}

static void s_parseSupChan(T_AccessPoint* pAP, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap _UNUSED,
                           swl_80211_elId_ne elId _UNUSED, uint8_t len, uint8_t* frm) {

    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t chanspec;
    chanspec.bandwidth = pRad->operatingChannelBandwidth;
    chanspec.band = pRad->operatingFrequencyBand;

    uint8_t nrTuples = len / 2;
    for(int i = 0; i < nrTuples; i++) {
        chanspec.channel = frm[i * 2];
        pDev->uniiBandsCapabilities |= swl_chanspec_toUniiMask(&chanspec);
    }
}


static void s_parsRsn(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap _UNUSED,
                      swl_80211_elId_ne elId _UNUSED, uint8_t len, uint8_t* frm) {
    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "Parsing RSN IE len %d", len);
    ASSERTS_TRUE(len >= 8, , ME, "RSN too short");
    ASSERTS_TRUE(frm[0] == 1, , ME, "RSN version not supported");
    /* skip len + version + group cipher */
    frm += 6;
    len -= 6;

    /* skip pairwise cipher suite */
    uint8_t n_uciphers = *frm;
    uint32_t size_ciphers = n_uciphers * 4 + 2;
    ASSERTS_TRUE(len >= size_ciphers, , ME, "ucast cipher data too short");
    frm += size_ciphers;
    len -= size_ciphers;

    /* skip authentication key mgt suite*/
    uint8_t n_keymgmt = *frm;
    uint32_t size_keymgmt = n_keymgmt * 4 + 2;
    ASSERTS_TRUE(len >= size_keymgmt, , ME, "keygmt data too short");
    frm += size_keymgmt;
    len -= size_keymgmt;

    /* rsn capabilities */
    ASSERTS_TRUE(len >= 2, , ME, "RSN capabilities  data too short");
    SAH_TRACEZ_INFO(ME, "BCM RSN Capabilites (0x%02x): \n", frm[0]);
    /* PMF capability 1... ....*/
    if(SWL_BIT_IS_SET(frm[0], 7)) {
        pDev->capabilities |= M_SWL_STACAP_PMF;
    }
}

static void s_parseRmCap(T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap _UNUSED,
                         swl_80211_elId_ne elId _UNUSED, uint8_t len _UNUSED, uint8_t* frm) {
    ASSERTS_NOT_NULL(frm, , ME, "NULL");
    ASSERTS_NOT_NULL(pDev, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Parsing RM Enable Capabilities %p", frm);
    /*
     * see Table 9-210—RM Enabled Capabilities definition
     * frm[0] =       bit0        |        bit1       |        bit2       |        bit3       |        bit4       |        bit5        |       bit6        |    bit7
     *          Link Measurement    Neighbor Report     Parallel            Repeated            Beacon Passive      Beacon Active        Beacon Table        Beacon Measurement
     *          Capability Enabled  Capability Enabled  Measurements        Measurements        Measurement         Measurement          Measurement         Reporting Conditions
     *                                                  Capability Enabled  Capability Enabled  Capability Enabled  Capability Enabled   Capability Enabled  Capability Enabled
     *
     */
    // check bit4 (Beacon Passive Measurement Capability Enabled)and bit5 (Beacon Active Measurement Capability Enabled)
    if(SWL_BIT_IS_SET(frm[0], 4) || SWL_BIT_IS_SET(frm[0], 5) || SWL_BIT_IS_SET(frm[0], 6)) {
        pDev->capabilities |= M_SWL_STACAP_RRM;
    }
}

SWL_TABLE(ieParseTable,
          ARR(uint8_t index; void* val; ),
          ARR(swl_type_uint8, swl_type_voidPtr),
          ARR({SWL_80211_EL_ID_HT_CAP, (void*) s_parseHtCap},
              {SWL_80211_EL_ID_VHT_CAP, (void*) s_parseVhtCap},
              {SWL_80211_EL_ID_EXT_CAP, (void*) s_parseExtCap},
              {SWL_80211_EL_ID_VENDOR_SPECIFIC, (void*) s_parseVendor},
              {SWL_80211_EL_ID_MOB_DOM, (void*) s_parseMobDom},
              {SWL_80211_EL_ID_SUP_CHAN, (void*) s_parseSupChan},
              {SWL_80211_EL_ID_SUP_OP_CLASS, (void*) s_parseRegClass},
              {SWL_80211_EL_ID_RSN, (void*) s_parsRsn},
              {SWL_80211_EL_ID_EXT, (void*) s_parseExt},
              {SWL_80211_EL_ID_RM_ENAB_CAP, (void*) s_parseRmCap},
              )
          );

typedef void (* elementParseFun_f) (T_AccessPoint* pAP, T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap, swl_80211_elId_ne elId, uint8_t len, swl_bit8_t* frm);


static swl_rc_ne s_processAssocFrame(T_AccessPoint* pAP, T_AssociatedDevice* pAD, char* data, size_t len) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAD, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    size_t binLen = len / 2;
    swl_bit8_t binData[binLen];
    bool success = swl_hex_toBytes(binData, binLen, data, len);
    ASSERT_TRUE(success, SWL_RC_ERROR, ME, "HEX CONVERT FAIL");

    size_t index = 28;

    while(index < binLen) {

        uint8_t elementLen = binData[index + 1];
        ASSERT_TRUE(elementLen != 0, SWL_RC_ERROR, ME, "ZERO LEN");

        if(binLen - index < binData[index + 1]) {
            SAH_TRACEZ_ERROR(ME, "Too short");
            return SWL_RC_ERROR;
        }
        swl_80211_elId_ne elementId = binData[index];
        SAH_TRACEZ_INFO(ME, "parsing element: id %u, len %u, index %zu", elementId, elementLen, index);

        void** func = (void**) swl_table_getMatchingValue(&ieParseTable, 1, 0, &elementId);
        if(func != NULL) {
            elementParseFun_f myFun = (elementParseFun_f) (*func);
            myFun(pAP, pAD, &pAD->assocCaps, elementId, elementLen - 2, &binData[index + 2]);
        } else {
            SAH_TRACEZ_INFO(ME, "Unsupported element id %d", elementId);
        }
        index += elementLen + 2;
    }

    return SWL_RC_OK;
}

void wifiGen_staCapHandler_receiveAssocMsg(T_AccessPoint* pAP, T_AssociatedDevice* pAD, char* data) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERT_NOT_NULL(data, , ME, "NULL");

    char* pkt = strchr(data, '=');
    ASSERT_NOT_NULL(pkt, , ME, "NULL");
    pkt = pkt + 1;
    size_t len = swl_str_len(pkt);

    SAH_TRACEZ_INFO(ME, "PKT sta:"SWL_MAC_FMT " len:%zu", SWL_MAC_ARG(pAD->MACAddress), len);
    pAD->capabilities = 0;
    s_processAssocFrame(pAP, pAD, pkt, len);
}
