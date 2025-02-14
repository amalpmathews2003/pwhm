/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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

#define ME "mldCfg"

#include <stdlib.h>
#include <debug/sahtrace.h>


#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <swla/swla_commonLib.h>
#include <swla/swla_conversion.h>
#include <swl/swl_string.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"

#include <swl/swl_base64.h>
#include "swla/swla_dm.h"


static void s_setApMldConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    SAH_TRACEZ_IN(ME);
    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        int8_t newEnable;
        if(swl_str_matches(pname, "EMLMREnable")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%s: EMLMREnable oldValue:%d newValue:%d", pAP->alias, pAP->mldCfg.emlmrEnable, newEnable);
            pAP->mldCfg.emlmrEnable = newEnable;
        } else if(swl_str_matches(pname, "EMLSREnable")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%s: EMLSREnable oldValue:%d newValue:%d", pAP->alias, pAP->mldCfg.emlsrEnable, newEnable);
            pAP->mldCfg.emlsrEnable = newEnable;
        } else if(swl_str_matches(pname, "STREnable")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%s: STREnable oldValue:%d newValue:%d", pAP->alias, pAP->mldCfg.strEnable, newEnable);
            pAP->mldCfg.strEnable = newEnable;
        } else if(swl_str_matches(pname, "NSTREnable")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%s: NSTREnable oldValue:%d newValue:%d", pAP->alias, pAP->mldCfg.nstrEnable, newEnable);
            pAP->mldCfg.nstrEnable = newEnable;
        }
    }

    pAP->pFA->mfn_wvap_setMldCfg(pAP);
    SAH_TRACEZ_OUT(ME);
}


SWLA_DM_HDLRS(sApMldCfgHdlrs, ARR(), .objChangedCb = s_setApMldConfig_ocf);



void _wld_ap_setMLDConfig_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApMldCfgHdlrs, sig_name, data, priv);
}
