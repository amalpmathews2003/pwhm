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
#include "swl/swl_genericFrameParser.h"

#define ME "genStaC"

static void s_copyAssocDevInfoFromIEs(T_AssociatedDevice* pDev, wld_assocDev_capabilities_t* cap, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    pDev->capabilities = pWirelessDevIE->capabilities;
    pDev->uniiBandsCapabilities = pWirelessDevIE->uniiBandsCapabilities;
    cap->freqCapabilities = pWirelessDevIE->freqCapabilities;
    memcpy(&cap->vendorOUI, &pWirelessDevIE->vendorOUI, sizeof(swl_oui_list_t));
    cap->htCapabilities = pWirelessDevIE->htCapabilities;
    cap->vhtCapabilities = pWirelessDevIE->vhtCapabilities;
    cap->heCapabilities = pWirelessDevIE->heCapabilities;
}

static swl_rc_ne s_processAssocFrame(T_AccessPoint* pAP, T_AssociatedDevice* pAD, char* data, size_t len) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAD, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    size_t binLen = len / 2;
    swl_bit8_t binData[binLen];
    bool success = swl_hex_toBytes(binData, binLen, data, len);
    ASSERT_TRUE(success, SWL_RC_ERROR, ME, "HEX CONVERT FAIL");

    //skip Mgmt Frame header and jump to tagged parameters (IEs)
    size_t iesPos = SWL_80211_MGMT_FRAME_TAGGED_IES_OFFSET;
    ssize_t iesLen = binLen - iesPos;
    ASSERT_FALSE(iesLen < 0, SWL_RC_ERROR, ME, "Too short");

    swl_bit8_t* iesData = &binData[iesPos];
    swl_parsingArgs_t parsingArgs = {
        .seenOnChanspec = SWL_CHANSPEC_NEW(pAP->pRadio->channel, pAP->pRadio->runningChannelBandwidth, pAP->pRadio->operatingFrequencyBand),
    };
    swl_wirelessDevice_infoElements_t wirelessDevIE;
    ssize_t parsedLen = swl_80211_parseInfoElementsBuffer(&wirelessDevIE, &parsingArgs, iesLen, iesData);
    ASSERT_FALSE(parsedLen < (ssize_t) iesLen, SWL_RC_ERROR, ME, "Partial IEs parsing");

    s_copyAssocDevInfoFromIEs(pAD, &pAD->assocCaps, &wirelessDevIE);

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
