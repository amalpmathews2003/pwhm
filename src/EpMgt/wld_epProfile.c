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

#include <string.h>
#include <swla/swla_mac.h>

#include "wld.h"
#include "wld_radio.h"
#include "wld_endpoint.h"
#include "wld_epProfile.h"
#include "wld_tinyRoam.h"
#include "wld_ssid.h"
#include "wld_wps.h"
#include "wld_util.h"
#include "swl/swl_assert.h"
#include "wld_eventing.h"
#include "wld_assocdev.h"
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_cfgManager.h"

#define ME "wldEPrf"



/**
 * @brief setEndPointProfileDefaults
 *
 * Set some default settings for the Endpoint Profile
 * Used when the profile is just created
 *
 * @param profile Endpoint profile
 */
static void s_setDefaults(T_EndPointProfile* profile) {
    SAH_TRACEZ_INFO(ME, "Setting EndpointProfile Defaults");

    profile->enable = 0;
    profile->priority = 0;
    profile->status = EPPS_DISABLED;
    memset(profile->SSID, 0, sizeof(profile->SSID));
    memset(profile->BSSID, 0, sizeof(profile->BSSID));
    memset(profile->keyPassPhrase, 0, sizeof(profile->keyPassPhrase));
    memset(profile->saePassphrase, 0, sizeof(profile->saePassphrase));
    memset(profile->alias, 0, sizeof(profile->alias));
    memset(profile->location, 0, sizeof(profile->location));
    memset(profile->WEPKey, 0, sizeof(profile->WEPKey));
    memset(profile->preSharedKey, 0, sizeof(profile->preSharedKey));
    profile->secModeEnabled = APMSI_NONE;
}


/**
 * @brief wld_endpoint_addProfileInstance_ocf
 *
 * Endpoint Profile instance Add Handler
 * Create a new EndpointProfile Instance
 * and set its defaults
 *
 * @param template_object EndpointProfile template object
 * @param instance_object EndpointProfile instance object
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_addProfileInstance_ocf(amxd_object_t* object,
                                                   amxd_param_t* param,
                                                   amxd_action_t reason,
                                                   const amxc_var_t* const args,
                                                   amxc_var_t* const retval,
                                                   void* priv) {
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    amxd_object_t* endpointObject = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(endpointObject, amxd_status_unknown_error, ME, "NULL");
    T_EndPoint* pEP = (T_EndPoint*) endpointObject->priv;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Endpoint structure");
        return amxd_status_unknown_error;
    }
    /* Set the T_EndPointProfile struct to the new instance */
    T_EndPointProfile* profile = (T_EndPointProfile*) calloc(1, sizeof(T_EndPointProfile));
    if(profile == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to allocate T_EndpointProfile structure");
        return amxd_status_unknown_error;
    }
    instance->priv = profile;
    profile->pBus = instance;
    /* Interlinking */
    amxc_llist_append(&pEP->llProfiles, &profile->it);
    profile->endpoint = pEP;

    /* Set some defaults to the endpoint profile struct */
    s_setDefaults(profile);
    /* Set the current profile when a matching profile reference is set */
    wld_endpoint_setCurrentProfile(endpointObject, profile);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


/**
 * @brief wld_endpoint_deleteProfileInstance_odf
 *
 * Delete handler on the Profile template object
 * Remove the linked profile data
 *
 * @param template_object
 * @param instance_object
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_deleteProfileInstance_odf(amxd_object_t* template_object, amxd_object_t* instance_object) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* endpointObject = amxd_object_get_parent(template_object);
    T_EndPointProfile* pProf = (T_EndPointProfile*) instance_object->priv;
    T_EndPoint* pEP = (T_EndPoint*) endpointObject->priv;

    if(pProf == NULL) {
        //If no priv data, internal clean already done.
        return amxd_status_ok;
    }
    instance_object->priv = NULL;
    pProf->pBus = NULL;

    wld_epProfile_delete(pEP, pProf);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}


swl_rc_ne wld_epProfile_delete(T_EndPoint* pEP, T_EndPointProfile* pProfile) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pProfile, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(((void*) pEP) != ((void*) pProfile), SWL_RC_INVALID_PARAM, ME, "EQUALS");

    if(pEP->currentProfile == pProfile) {
        pEP->currentProfile = NULL;
        SAH_TRACEZ_WARNING(ME, "Current Active EndpointProfile gets deleted !!!");
        pEP->internalChange = true;
        amxd_object_set_cstring_t(pEP->pBus, "ProfileReference", "");
        pEP->internalChange = false;
        wld_endpoint_reconfigure(pEP);
    }

    if(pProfile->pBus != NULL) {
        pProfile->pBus->priv = NULL;
        amxd_object_delete(&pProfile->pBus);
    }

    amxc_llist_it_take(&pProfile->it);
    free(pProfile);

    return SWL_RC_OK;
}


T_EndPointProfile* wld_epProfile_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");

    return amxc_llist_it_get_data(it, T_EndPointProfile, it);
}


