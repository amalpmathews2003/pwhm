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


/**
 * @file Translates between bus and C API provided by `wld_tinyRoam.c`. No logic.
 */


#include "wld_tinyRoam.h"
#include "wld.h"
#define ME "tyRoam"


#define WLD_TINY_ROAM_DEFAULT_NR_TRIES 1
#define WLD_TINY_ROAM_DEFAULT_TIMEOUT_SEC 60

static void s_writeReply(amxc_var_t* retval, wld_tinyRoam_roamResult_e roamResult) {
    uint64_t call_id = amxc_var_dyncast(uint64_t, retval);

    amxc_var_t retvalMap;
    amxc_var_init(&retvalMap);
    amxc_var_set_type(&retvalMap, AMXC_VAR_ID_HTABLE);

    if(roamResult != WLD_TINYROAM_ROAMRESULT_SUCCESS) {
        const char* message = wld_tinyRoam_roamResultChar[roamResult];
        amxc_var_add_key(cstring_t, &retvalMap, "Roam error", message);
    }

    amxc_var_add_key(uint32_t, &retvalMap, "Result", roamResult);
    amxd_function_deferred_done(call_id, amxd_status_ok, NULL, &retvalMap);
    amxc_var_clean(&retvalMap);
}

/** Implements #wld_tinyRoam_roamEndedCb_t */
static void s_roamEndedCb(T_EndPoint* ep _UNUSED, wld_tinyRoam_roamResult_e roamResult, void* userData) {
    amxc_var_t* retval = userData;
    ASSERT_NOT_NULL(userData, , ME, "NULL");
    s_writeReply(retval, roamResult);
}

/**
 * Callback function, only translates between bus<->C API, no logic.
 */
amxd_status_t _EndPoint_roamTo(amxd_object_t* objEp,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {

    T_EndPoint* ep = objEp->priv;
    ASSERT_NOT_NULL(ep, amxd_status_unknown_error, ME, "NULL");

    // parse parameter "bssid"
    const char* bssidChar = GET_CHAR(args, "bssid");
    if(bssidChar == NULL) {
        return amxd_status_unknown_error;
    }
    swl_macBin_t bssid = SWL_MAC_BIN_NEW();
    bool ok = SWL_MAC_CHAR_TO_BIN(&bssid, bssidChar);
    if(!ok) {
        SAH_TRACEZ_ERROR(ME, "%s bssid parameter missing or unparseable", ep->Name);
        return amxd_status_unknown_error;
    }

    amxc_var_t* triesVar = amxc_var_get_key(args, "tries", AMXC_VAR_FLAG_DEFAULT);
    int32_t maxNumberOfAttempts = WLD_TINY_ROAM_DEFAULT_NR_TRIES;
    if(triesVar != NULL) {
        maxNumberOfAttempts = amxc_var_get_int32_t(triesVar);
    }

    amxc_var_t* timeoutInSecVar = amxc_var_get_key(args, "timeoutInSec", AMXC_VAR_FLAG_DEFAULT);
    int32_t timeoutInSec = WLD_TINY_ROAM_DEFAULT_TIMEOUT_SEC;
    if(triesVar != NULL) {
        timeoutInSec = amxc_var_get_int32_t(timeoutInSecVar);
    }

    // call C API
    ok = wld_tinyRoam_roamTo(ep, &bssid, maxNumberOfAttempts, timeoutInSec, s_roamEndedCb, retval);

    if(ok) {
        return amxd_status_deferred;
    } else {
        return amxd_status_unknown_error;
    }
}
