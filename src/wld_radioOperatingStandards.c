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

#define ME "ROpStd"

#include <stdlib.h>
#include <debug/sahtrace.h>



#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <components.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "Utils/wld_autoCommitMgr.h"
#include "swl/swl_assert.h"

#include "wld_radioOperatingStandards.h"

// Checks whether given value is allowed for `OperatingStandards`.
// Callback called by the datamodel, see `.odl` file.
amxd_status_t validateRadioOperatingStandards(amxd_param_t* parameter, void* oldValue _UNUSED) {
    T_Radio* pRadio = amxd_param_get_owner(parameter)->priv;
    ASSERTS_NOT_NULL(pRadio, amxd_status_ok, ME, "NULL");

    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(parameter, &value);
    const char* radioStandardsString = amxc_var_get_cstring_t(&value);
    amxc_var_clean(&value);

    swl_radioStandard_m radioStandards;

    return swl_radStd_fromCharAndValidate(&radioStandards, radioStandardsString,
                                          pRadio->operatingStandardsFormat, pRadio->supportedStandards,
                                          "validateRadioOperatingStandards");
}

static void s_processOperatingStandards(T_Radio* pR, const char* newVal) {
    ASSERTS_NOT_NULL(pR, , ME, "NULL"); // silent fail if userdata not yet set
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "INVALID");
    ASSERT_STR(newVal, , ME, "Empty");
    swl_radioStandard_m newStandards;
    bool validOperatingStandards = swl_radStd_fromCharAndValidate(&newStandards, newVal,
                                                                  pR->operatingStandardsFormat, pR->supportedStandards, "setRadioOperatingStandards");
    ASSERT_TRUE(validOperatingStandards, , ME, "%s setRadioOperatingStandards : invalid operatingStandards", pR->Name);

    SAH_TRACEZ_INFO(ME, "%s setRadioOperatingStandards : set %s (%#x) was (%#x)",
                    pR->Name,
                    newVal, newStandards, pR->operatingStandards);


    if(pR->operatingStandards != newStandards) {
        pR->pFA->mfn_wrad_supstd(pR, newStandards);
        wld_autoCommitMgr_notifyRadEdit(pR);
    }
}

// Callback called when `OperatingStandardsFormat` is set.
amxd_status_t _wld_rad_setOperatingStandardsFormat_pwf(amxd_object_t* object,
                                                       amxd_param_t* parameter,
                                                       amxd_action_t reason,
                                                       const amxc_var_t* const args,
                                                       amxc_var_t* const retval,
                                                       void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return rv;
    }
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NO radio Ctx");

    const char* newValChar = amxc_var_constcast(cstring_t, args);
    ASSERTW_STR(newValChar, amxd_status_unknown_error, ME, "Missing param");
    const swl_radStd_format_e newVal = swl_radStd_charToFormat(newValChar);
    ASSERTI_NOT_EQUALS(newVal, pR->operatingStandardsFormat, amxd_status_ok, ME, "same value");
    pR->operatingStandardsFormat = newVal;

    // How to interpret the argument value of the "OperatingStandards" parameter could have changed,
    // so parse it again.
    amxc_var_t value;
    amxc_var_init(&value);
    amxd_object_get_param(object, "OperatingStandards", &value);
    s_processOperatingStandards(pR, amxc_var_constcast(cstring_t, &value));
    amxc_var_clean(&value);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

// Callback called when `OperatingStandards` is set.
amxd_status_t _wld_rad_setOperatingStandards_pwf(amxd_object_t* object,
                                                 amxd_param_t* parameter,
                                                 amxd_action_t reason,
                                                 const amxc_var_t* const args,
                                                 amxc_var_t* const retval,
                                                 void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t rv = amxd_status_ok;
    if(amxd_object_get_type(object) != amxd_object_instance) {
        return rv;
    }
    rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_ok, ME, "NULL");
    ASSERT_TRUE(debugIsRadPointer(pR), amxd_status_unknown_error, ME, "NO radio Ctx");

    const char* newValChar = amxc_var_constcast(cstring_t, args);
    s_processOperatingStandards(pR, newValChar);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

// In case `operatingStandards` is "auto", set it to the default (which is the `supportedStandards`).
// This also updates the value in the datamodel, if change needed.
void wld_rad_update_operating_standard(T_Radio* pRad) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pRad, , ME, "Radio NULL");
    ASSERT_FALSE(pRad->operatingStandards != M_SWL_RADSTD_AUTO && (pRad->operatingStandards & (~pRad->supportedStandards)), , ME,
                 "%s update_operating_standard : OperatingStandards %#x contains a standard not in SupportedStandards %#x",
                 pRad->Name, pRad->operatingStandards, pRad->supportedStandards);
    char* oldStandardsText = amxd_object_get_cstring_t(pRad->pBus, "OperatingStandards", NULL);
    ASSERT_NOT_NULL(oldStandardsText, , ME, "OperatingStandards not found in pRad object");
    swl_radioStandard_m oldStandards = pRad->operatingStandards;

    //If operatingStandards not yet set, ignore update.
    ASSERTI_NOT_EQUALS(pRad->operatingStandards, 0, , ME,
                       "%s opstd=0, ignoring update", pRad->Name);

    ASSERTS_EQUALS(pRad->operatingStandards, M_SWL_RADSTD_AUTO, , ME,
                   "%s opstd not 'auto', so no need to convert it", pRad->Name);

    pRad->operatingStandards = pRad->supportedStandards;

    amxc_string_t operatingStandardsText = swl_radStd_toStr(pRad->operatingStandards,
                                                            pRad->operatingStandardsFormat, pRad->supportedStandards);

    SAH_TRACEZ_INFO(ME, "%s update_operating_standard: update from %#x (%s) to %#x (%s)",
                    pRad->Name,
                    oldStandards, oldStandardsText,
                    pRad->operatingStandards, operatingStandardsText.buffer);
    amxd_object_set_cstring_t(pRad->pBus, "OperatingStandards", operatingStandardsText.buffer);
    amxc_string_clean(&operatingStandardsText);

    SAH_TRACEZ_OUT(ME);
}

