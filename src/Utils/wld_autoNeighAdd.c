/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <swl/swl_common.h>
#include "wld.h"
#include "wld_accesspoint.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "wld/Utils/wld_autoNeighAdd.h"

#define ME "wld_autoNeigh"

static wld_autoNeighAddConfig_t s_neighAddConfig;

/**
 * Add a specific accesspoint to the neighbour list of target accesspoint
 * @return
 *  - Success: SWL_RC_OK
 *  - Failure: SWL_RC_ERROR
 */
static swl_rc_ne s_setNeighbourAP(T_AccessPoint* pAP, T_AccessPoint* targetAP) {
    ASSERTI_TRUE(pAP->enable, SWL_RC_ERROR, ME, "%s not enabled", pAP->alias);
    ASSERTI_TRUE(pAP->SSIDAdvertisementEnabled, SWL_RC_ERROR, ME, "%s ssid advertisment is disabled", pAP->alias);

    amxd_trans_t trans;
    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    SWL_MAC_BIN_TO_CHAR(&macChar, (swl_macBin_t*) pAP->pSSID->MACAddress);
    T_ApNeighbour* neigh = wld_ap_findNeigh(targetAP, macChar.cMac);
    if(neigh != NULL) {
        SAH_TRACEZ_INFO(ME, "%s: neighbour AP with BSSID [%s] found in Neighbour list", targetAP->alias, macChar.cMac);
        ASSERT_TRANSACTION_INIT(neigh->obj, &trans, SWL_RC_ERROR, ME, "%s: trans init failure", targetAP->alias);
    } else {
        SAH_TRACEZ_INFO(ME, "%s: create new Neighbour AP with with BSSID [%s]", targetAP->alias, macChar.cMac);
        amxd_object_t* template = amxd_object_findf(targetAP->pBus, "Neighbour");
        ASSERT_NOT_NULL(template, SWL_RC_ERROR, ME, "%s: neighbour object not found", targetAP->alias);
        ASSERT_TRANSACTION_INIT(template, &trans, SWL_RC_ERROR, ME, "%s: trans init failure", targetAP->alias);
        uint32_t index = swla_object_getFirstAvailableIndex(template);
        amxd_trans_add_inst(&trans, index, NULL);
        amxd_trans_set_bool(&trans, "ColocatedAP", true);
    }

    amxd_trans_set_cstring_t(&trans, "BSSID", macChar.cMac);
    amxd_trans_set_cstring_t(&trans, "SSID", pAP->pSSID->SSID);
    amxd_trans_set_uint8_t(&trans, "Channel", pAP->pRadio->channel);
    amxd_trans_set_uint8_t(&trans, "OperatingClass", pAP->pRadio->operatingClass);

    amxd_status_t status = swl_object_finalizeTransactionOnLocalDm(&trans);
    return (status == amxd_status_ok) ? SWL_RC_OK : SWL_RC_ERROR;
}

/**
 * Delete a specific accesspoint from the the neighbour list of target accesspoint
 * @return
 *  - Success: SWL_RC_OK
 *  - Failure: SWL_RC_ERROR
 */
static swl_rc_ne s_delNeighbourAP(T_AccessPoint* pAP, T_AccessPoint* targetAP) {
    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    SWL_MAC_BIN_TO_CHAR(&macChar, (swl_macBin_t*) pAP->pSSID->MACAddress);
    T_ApNeighbour* neigh = wld_ap_findNeigh(targetAP, macChar.cMac);
    ASSERT_NOT_NULL(neigh, SWL_RC_OK, ME, "NULL");
    amxd_status_t status = swl_object_delInstWithTransOnLocalDm(neigh->obj);
    return ((status == amxd_status_ok) ? SWL_RC_OK : SWL_RC_ERROR);
}

swl_rc_ne wld_autoNeighAdd_vapSetDelNeighbourAP(T_AccessPoint* pAP, bool set) {
    ASSERT_NOT_NULL(pAP, SWL_RC_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pRadio, SWL_RC_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pSSID, SWL_RC_ERROR, ME, "NULL");

    ASSERTS_TRUE(s_neighAddConfig.enable, SWL_RC_ERROR, ME, "auto neighbour addition feature disabled");
    ASSERTS_TRUE(SWL_BIT_IS_SET(s_neighAddConfig.neighBands, pAP->pRadio->operatingFrequencyBand), SWL_RC_ERROR, ME,
                 "%s: radio operatingFrequencyBand %s not in NeighbourBands",
                 pAP->alias, Rad_SupFreqBands[pAP->pRadio->operatingFrequencyBand]);

    T_Radio* pOtherRad;
    wld_for_eachRad(pOtherRad) {
        if((swl_str_matches(pAP->pRadio->Name, pOtherRad->Name)) ||
           (pOtherRad->operatingFrequencyBand == pAP->pRadio->operatingFrequencyBand)) {
            continue;
        }
        T_AccessPoint* pOtherAp;
        wld_rad_forEachAp(pOtherAp, pOtherRad) {
            if(pAP->pBus && pOtherAp->pBus && (pAP->defaultDeviceType == pOtherAp->defaultDeviceType)) {
                if(set) {
                    s_setNeighbourAP(pAP, pOtherAp);
                } else {
                    s_delNeighbourAP(pAP, pOtherAp);
                }
            } else {
                continue;
            }
        }
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_autoNeighAdd_radioSetDelNeighbourAP(T_Radio* pRad, bool set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");

    ASSERTS_TRUE(s_neighAddConfig.enable, SWL_RC_ERROR, ME, "auto neighbour addition feature disabled");
    ASSERTS_TRUE(SWL_BIT_IS_SET(s_neighAddConfig.neighBands, pRad->operatingFrequencyBand), SWL_RC_ERROR, ME,
                 "%s: radio operatingFrequencyBand %s not in NeighbourBands",
                 pRad->Name, Rad_SupFreqBands[pRad->operatingFrequencyBand]);

    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pRad) {
        wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, set);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_autoNeighAdd_vapReloadNeighbourAP(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_EQUALS(pAP->status, APSTI_ENABLED, SWL_RC_ERROR, ME, "%s deactivated", pAP->alias);
    if(!swl_rc_isOk(wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, false))) {
        SAH_TRACEZ_NOTICE(ME, "failed deleting AP from neighbor list of other APs");
        return SWL_RC_ERROR;
    }
    if(!swl_rc_isOk(wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, true))) {
        SAH_TRACEZ_NOTICE(ME, "failed adding AP from neighbor list of other APs");
        return SWL_RC_ERROR;
    }
    return SWL_RC_OK;
}

static void s_setNeighbourBands_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED,
                                    amxd_param_t* param _UNUSED,
                                    const amxc_var_t* const newValue) {
    char* bands = amxc_var_dyncast(cstring_t, newValue);
    swl_freqBandExt_m newNeighBands = swl_conv_charToMask(bands, Rad_SupFreqBands, SWL_FREQ_BAND_EXT_MAX);
    free(bands);
    ASSERTI_NOT_EQUALS(newNeighBands, 0, , ME, "invalid parameters");
    ASSERTI_NOT_EQUALS(newNeighBands, s_neighAddConfig.neighBands, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "update NeighbourBands %u to %u", s_neighAddConfig.neighBands, newNeighBands);
    s_neighAddConfig.neighBands = newNeighBands;
}

static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED,
                            amxd_param_t* param _UNUSED,
                            const amxc_var_t* const newValue) {
    bool newEnable = amxc_var_dyncast(bool, newValue);
    ASSERTI_NOT_EQUALS(newEnable, s_neighAddConfig.enable, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "update Enable %u to %u", s_neighAddConfig.enable, newEnable);
    s_neighAddConfig.enable = newEnable;
}

SWLA_DM_HDLRS(sAutoNeighAddDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("NeighbourBands", s_setNeighbourBands_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf)));

void _wld_autoNeighAddition_setConf_ocf(const char* const sig_name,
                                        const amxc_var_t* const data,
                                        void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAutoNeighAddDmHdlrs, sig_name, data, priv);
}
