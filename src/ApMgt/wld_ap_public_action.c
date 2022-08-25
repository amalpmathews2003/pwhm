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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "swl/swl_assert.h"
#include "swl/swl_hex.h"
#include "swl/swl_oui.h"

#define ME "apPub"

amxd_status_t _sendPublicAction(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args,
                                amxc_var_t* ret _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");

    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_ok;

    const char* macStr = GET_CHAR(args, "mac");
    swl_macBin_t mac = SWL_MAC_BIN_NEW();
    bool ok = SWL_MAC_CHAR_TO_BIN(&mac, macStr);
    if(!ok) {
        SAH_TRACEZ_ERROR(ME, "Fail to convert mac");
        return amxd_status_invalid_function_argument;
    }

    const char* data = GET_CHAR(args, "data");
    const char* ouiStr = GET_CHAR(args, "oui");

    uint8_t type = 0;
    uint8_t subtype = 0;
    amxc_var_t* typeVar = amxc_var_get_key(args, "type", AMXC_VAR_FLAG_DEFAULT);
    type = (typeVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, typeVar);
    amxc_var_t* subTypeVar = amxc_var_get_key(args, "subtype", AMXC_VAR_FLAG_DEFAULT);
    subtype = (subTypeVar == NULL) ? 0 : amxc_var_dyncast(uint8_t, subTypeVar);

    if(!data || !ouiStr) {
        SAH_TRACEZ_ERROR(ME, "Invalid argument");
        return amxd_status_invalid_function_argument;
    }

    swl_oui_t oui;
    memset(&oui, 0, sizeof(swl_oui_t));
    ok = swl_hex_toBytesSep(oui.ouiBytes, SWL_ARRAY_SIZE(oui.ouiBytes), ouiStr, SWL_OUI_STR_LEN, ':', NULL);
    if(!ok) {
        SAH_TRACEZ_ERROR(ME, "Invalid OUI");
        return amxd_status_invalid_function_argument;
    }

    swl_rc_ne res = pAP->pFA->mfn_wvap_sendPublicAction(pAP, &mac, oui, type, subtype, (char*) data);
    if(res == SWL_RC_NOT_IMPLEMENTED) {
        SAH_TRACEZ_ERROR(ME, "Function not supported");
        status = amxd_status_function_not_implemented;
    } else if(res < 0) {
        SAH_TRACEZ_ERROR(ME, "Error during execution");
        status = amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}
