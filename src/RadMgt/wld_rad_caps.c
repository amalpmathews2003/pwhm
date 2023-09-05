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


#include <stdlib.h>
#include <debug/sahtrace.h>



#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_channel.h"
#include "swl/swl_assert.h"

#define ME "radCaps"

int wld_rad_init_cap(T_Radio* pR) {
    ASSERTS_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");
    ASSERTS_TRUE(pR->nrCapabilities > 0, SWL_RC_OK, ME, "NULL");
    ASSERTS_NOT_NULL(pR->capabilities, SWL_RC_ERROR, ME, "NULL");
    int i = 0;
    for(i = 0; i < pR->nrCapabilities; i++) {
        pR->cap_status[i].capable = 0;
        pR->cap_status[i].enable = 0;
        pR->cap_status[i].status = 0;
        if(pR->capabilities[i].Name == NULL) {
            SAH_TRACEZ_ERROR(ME, "capability %u has NULL name, aborting", i);
            return -1;
        }
    }
    if(pR->capabilities[pR->nrCapabilities].Name != NULL) {
        SAH_TRACEZ_ERROR(ME, "capability MAX %u has NON NULL name, aborting", pR->nrCapabilities);
        return -1;
    }
    return 0;
}

bool wld_rad_is_cap_enabled(T_Radio* pR, int capability) {
    return pR->cap_status[capability].enable;
}

bool wld_rad_is_cap_active(T_Radio* pR, int capability) {
    return pR->cap_status[capability].status;
}

/**
 * Update the capabilities status string of the radio.
 * @param pR
 *  The radio to update.
 */
void wld_rad_parse_status(T_Radio* pR) {
    int size = 1 + ((MAX_CAP_SIZE + 1) * pR->nrCapabilities);
    char cap_string[size];
    cap_string[0] = 0;
    int i = 0;
    for(i = 0; i < pR->nrCapabilities; i++) {
        if(pR->cap_status[i].status) {
            swl_strlst_cat(cap_string, sizeof(cap_string), " ", pR->capabilities[i].Name);
        }
    }
    amxd_object_t* featureObject = amxd_object_findf(pR->pBus, "RadCaps");
    amxd_object_set_cstring_t(featureObject, "Status", cap_string);
}

/**
 * Update the enable status string of the radio.
 * @param pR
 *  The radio to update.
 */
static void wld_rad_parse_enable(T_Radio* pR, int includeIndex, int excludeIndex) {
    int size = 1 + ((MAX_CAP_SIZE + 1) * pR->nrCapabilities);
    char cap_string[size];
    cap_string[0] = 0;
    for(int i = 0; i < pR->nrCapabilities; i++) {
        if((i == includeIndex) || (pR->cap_status[i].enable && (i != excludeIndex))) {
            swl_strlst_cat(cap_string, sizeof(cap_string), " ", pR->capabilities[i].Name);
        }
    }
    amxd_object_t* featureObject = amxd_object_findf(pR->pBus, "RadCaps");
    amxd_object_set_cstring_t(featureObject, "Enabled", cap_string);
}

/**
 * Update the capabilities available string of the radio
 * It will also update the current status.
 *
 * @param pR
 *  The radio of which to update the capabilities.
 */
void wld_rad_parse_cap(T_Radio* pR) {
    int size = 1 + ((MAX_CAP_SIZE + 1) * pR->nrCapabilities);
    char cap_string[size];
    cap_string[0] = 0;
    int i = 0;
    for(i = 0; i < pR->nrCapabilities; i++) {
        if(pR->cap_status[i].capable) {
            swl_strlst_cat(cap_string, sizeof(cap_string), " ", pR->capabilities[i].Name);
        }
    }
    amxd_object_t* featureObject = amxd_object_findf(pR->pBus, "RadCaps");
    amxd_object_set_cstring_t(featureObject, "Available", cap_string);
    wld_rad_parse_enable(pR, -1, -1);
    wld_rad_parse_status(pR);
}

static void wld_rad_setCapabilityIndex(T_Radio* pR, int index, bool status) {
    ASSERTS_NOT_NULL(pR, , ME, "NULL");
    ASSERTS_NOT_NULL(pR->capabilities, , ME, "NULL");
    ASSERTS_TRUE((index >= 0) && (index < pR->nrCapabilities), , ME, "out of range");
    const char* name = pR->capabilities[index].Name;
    if(!pR->cap_status[index].capable) {
        SAH_TRACEZ_INFO(ME, "Feature not capable %s, ignore change", name);
        return;
    }
    if(pR->cap_status[index].enable == status) {
        SAH_TRACEZ_INFO(ME, "Item already %s %s", (status ? "enabled" : "disabled"), name);
        return;
    }
    pR->cap_status[index].enable = status;
    if(pR->capabilities[index].changeHandler != NULL) {
        SAH_TRACEZ_INFO(ME, "changing feature %s to %u, calling handler", name, status);
        pR->capabilities[index].changeHandler(pR);
    } else {
        SAH_TRACEZ_INFO(ME, "changing feature %s to %u, no handler", name, status);
    }
}

/**
 * Callback function for writing the enable field.
 * This function will retrieve the enable string, cut it into parts.
 * Then it will go over all capabilities, and check whether or not it's in the requested capabilities list.
 */
static void s_setEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERTI_TRUE(pR->nrCapabilities > 0, , ME, "%s: No specific radio caps", pR->Name);

    const char* capabilities = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(capabilities, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Setting capabilities [%s]", pR->Name, capabilities);

    for(int i = 0; i < pR->nrCapabilities; i++) {
        bool status = swl_strlst_contains(capabilities, " ", pR->capabilities[i].Name);
        if(pR->cap_status[i].enable != status) {
            wld_rad_setCapabilityIndex(pR, i, status);
        }
    }
    wld_rad_parse_status(pR);
}

/**
 * Get the index of a given capability.
 * @param pR
 *  the radio to request the capability.
 *
 * @param item
 *  the item of which we want the capability
 * @return
 *  *if item exists
 *	  return the index of the capability in the capability list
 *  *if item does not exist
 *	  return -1
 */
int wld_rad_get_capability_index(T_Radio* pR, const char* item) {
    ASSERT_NOT_NULL(pR, -1, ME, "NULL");
    ASSERT_NOT_NULL(item, -1, ME, "NULL");
    int i = 0;
    for(i = 0; i < pR->nrCapabilities; i++) {
        if(strncmp(item, pR->capabilities[i].Name, MAX_CAP_SIZE) == 0) {
            return i;
        }
    }
    return -1;
}


/**
 * Callback function to enable a feature
 * It will enable the feature internally, call the feature callback handler, and write the new
 * capabilities enabled field.
 * If a feature is not known, an error is returned.
 */
amxd_status_t _RadCaps_Enable(amxd_object_t* wifi_cap,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* ret _UNUSED) {
    amxd_object_t* wifi_rad = amxd_object_get_parent(wifi_cap);
    T_Radio* pR = (T_Radio*) wifi_rad->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    const char* feature = GET_CHAR(args, "feature");

    int index = wld_rad_get_capability_index(pR, feature);
    ASSERT_FALSE(index == -1, amxd_status_unknown_error, ME, "Unknown feature %s", feature);

    ASSERTS_FALSE(pR->cap_status[index].enable, amxd_status_ok, ME, "already enabled");

    wld_rad_setCapabilityIndex(pR, index, true);
    wld_rad_parse_enable(pR, index, -1);
    wld_rad_parse_status(pR);

    return amxd_status_ok;
}

/**
 * Callback function to disable a feature
 * It will disable the feature internally, call the feature callback handler, and write the new
 * capabilities enabled field.
 * If a feature is not known, an error is returned.
 */
amxd_status_t _RadCaps_Disable(amxd_object_t* wifi_cap,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* ret _UNUSED) {
    amxd_object_t* wifi_rad = amxd_object_get_parent(wifi_cap);
    T_Radio* pR = (T_Radio*) wifi_rad->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    const char* feature = GET_CHAR(args, "feature");

    int index = wld_rad_get_capability_index(pR, feature);
    ASSERT_FALSE(index == -1, amxd_status_unknown_error, ME, "Unknown feature %s", feature);

    ASSERTS_TRUE(pR->cap_status[index].enable, amxd_status_ok, ME, "already disabled");

    wld_rad_setCapabilityIndex(pR, index, false);
    wld_rad_parse_enable(pR, -1, index);
    wld_rad_parse_status(pR);

    return amxd_status_ok;
}

/**
 * Function to print the full list of capabilities, and whether or not they are enabled.
 */
amxd_status_t _RadCaps_DebugPrint(amxd_object_t* wifi_cap,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_object_t* wifi_rad = amxd_object_get_parent(wifi_cap);
    T_Radio* pR = (T_Radio*) wifi_rad->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    int i = 0;
    for(i = 0; i < pR->nrCapabilities; i++) {
        SAH_TRACEZ_NOTICE(ME, "%s : %u %u %u",
                          pR->capabilities[i].Name,
                          pR->cap_status[i].capable,
                          pR->cap_status[i].enable,
                          pR->cap_status[i].status
                          );
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/*
 * @brief check radio's supported driver capability per band
 *
 * @param pR radio context
 * @param suppBand supported frequency band to check for caps
 * @param suppCap string of supported driver capability to check for it
 *
 * @param return SWL_TRL_TRUE in capability is found
 *               SWL_TRL_FALSE is not found
 *               SWL_TRL_UNKNOWN in case of error
 */
swl_trl_e wld_rad_findSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap) {
    ASSERTS_NOT_NULL(pRad, SWL_TRL_UNKNOWN, ME, "NULL");
    ASSERTS_TRUE((suppBand < (uint32_t) SWL_FREQ_BAND_MAX), SWL_TRL_UNKNOWN, ME, "Invalid");
    ASSERTS_STR(suppCap, SWL_TRL_UNKNOWN, ME, "Empty");
    return swl_strlst_contains(pRad->suppDrvCaps[suppBand], " ", suppCap);
}

void wld_rad_addSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap) {
    ASSERTS_EQUALS(wld_rad_findSuppDrvCap(pRad, suppBand, suppCap), SWL_TRL_FALSE, , ME, "ignored");
    uint32_t len = 2 + strlen(suppCap);
    if(pRad->suppDrvCaps[suppBand]) {
        len += strlen(pRad->suppDrvCaps[suppBand]);
    }
    char suppCaps[len];
    swl_str_copy(suppCaps, len, pRad->suppDrvCaps[suppBand]);
    swl_strlst_cat(suppCaps, len, " ", suppCap);
    swl_str_copyMalloc(&pRad->suppDrvCaps[suppBand], suppCaps);
    SAH_TRACEZ_INFO(ME, "%s: Caps[%s]={%s}", pRad->Name, swl_freqBand_str[suppBand], suppCaps);
}

const char* wld_rad_getSuppDrvCaps(T_Radio* pRad, swl_freqBand_e suppBand) {
    ASSERTS_NOT_NULL(pRad, "", ME, "NULL");
    ASSERTS_TRUE((suppBand < (uint32_t) SWL_FREQ_BAND_MAX), "", ME, "Invalid");
    return pRad->suppDrvCaps[suppBand];
}

void wld_rad_clearSuppDrvCaps(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    for(int32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        free(pRad->suppDrvCaps[i]);
        pRad->suppDrvCaps[i] = NULL;
    }
}

SWLA_DM_HDLRS(sRadCapsDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enabled", s_setEnabled_pwf)));

void _wld_radCaps_setConf_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sRadCapsDmHdlrs, sig_name, data, priv);
}

