/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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

#include "wld.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "Utils/wld_dmnMgt.h"
#include "wld_secDmnGrp.h"

#define ME "wldDMgt"

static void s_addDmnInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const initialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);
    char* name = amxd_object_get_cstring_t(object, "Name", NULL);
    ASSERT_STR(name, , ME, "daemon name is empty");
    wld_dmnMgt_dmnCtx_t* pDmnCtx = calloc(1, sizeof(*pDmnCtx));
    if(pDmnCtx == NULL) {
        SAH_TRACEZ_ERROR(ME, "fail to alloc dmn ctx");
        free(name);
        return;
    }
    pDmnCtx->name = name;
    object->priv = pDmnCtx;
    pDmnCtx->object = object;
    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sDmnDmHdlrs, ARR(), .instAddedCb = s_addDmnInst_oaf, );

void _wld_dmnMgt_setDaemonEntry_ocf(const char* const sig_name,
                                    const amxc_var_t* const data,
                                    void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sDmnDmHdlrs, sig_name, data, priv);
}

amxd_status_t _wld_dmnMgt_delDaemonEntry_odf(amxd_object_t* object,
                                             amxd_param_t* param,
                                             amxd_action_t reason,
                                             const amxc_var_t* const args,
                                             amxc_var_t* const retval,
                                             void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: destroy instance object(%p)", name, object);
    wld_dmnMgt_dmnCtx_t* pDmnCtx = (wld_dmnMgt_dmnCtx_t*) object->priv;
    if(pDmnCtx != NULL) {
        object->priv = NULL;
        pDmnCtx->object = NULL;
        W_SWL_FREE(pDmnCtx->name);
        free(pDmnCtx);
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setDmnExecSettings_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    amxd_object_t* pDmnObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(pDmnObj, , ME, "NULL");
    wld_dmnMgt_dmnCtx_t* pDmnCtx = (wld_dmnMgt_dmnCtx_t*) pDmnObj->priv;
    ASSERT_NOT_NULL(pDmnCtx, , ME, "NULL");
    bool needSync = false;

    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "UseGlobalInstance")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_trl_e valTrl;
            swl_trl_fromChar(&valTrl, valStr, SWL_TRL_FORMAT_AUTO);
            if(pDmnCtx->exec.useGlobalInstance == valTrl) {
                continue;
            }
            pDmnCtx->exec.useGlobalInstance = valTrl;
        } else {
            continue;
        }
        char* pvalue = swl_typeCharPtr_fromVariantDef(newValue, NULL);
        SAH_TRACEZ_INFO(ME, "%s: setting exec settings %s = %s", pDmnCtx->name, pname, pvalue);
        free(pvalue);
        free(valStr);
        needSync = true;
    }

    if(needSync) {
        vendor_t* pVdr;
        wld_for_eachVdr(pVdr) {
            SAH_TRACEZ_INFO(ME, "apply dmn %s exec setting to vdr %s", pDmnCtx->name, pVdr->name);
            pVdr->fta.mfn_wvdr_setDmnExecSettings(pVdr, pDmnCtx->name, &pDmnCtx->exec);
        }
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sDmnExecSettingsDmHdlrs, ARR(), .objChangedCb = s_setDmnExecSettings_ocf);

void _wld_dmnMgt_setDaemonExecSettings_ocf(const char* const sig_name,
                                           const amxc_var_t* const data,
                                           void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sDmnExecSettingsDmHdlrs, sig_name, data, priv);
}

const wld_dmnMgt_dmnCtx_t* wld_dmnMgt_getDmnCtx(const char* dmnName) {
    ASSERT_STR(dmnName, NULL, ME, "Empty");
    amxd_object_t* dmnObj = amxd_object_findf(get_wld_object(), "DaemonMgt.Daemon.[Name == '%s']", dmnName);
    ASSERT_NOT_NULL(dmnObj, NULL, ME, "No dmn with name %s", dmnName);
    return dmnObj->priv;
}

swl_rc_ne wld_dmnMgt_initDmnExecInfo(wld_dmnMgt_dmnExecInfo_t** ppDmnExecInfo) {
    ASSERTS_NOT_NULL(ppDmnExecInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* pDmnExecInfo = *ppDmnExecInfo;
    if(pDmnExecInfo == NULL) {
        pDmnExecInfo = calloc(1, sizeof(*pDmnExecInfo));
        ASSERT_NOT_NULL(pDmnExecInfo, SWL_RC_ERROR, ME, "NULL");
        pDmnExecInfo->globalDmnSupported = SWL_TRL_UNKNOWN;
        *ppDmnExecInfo = pDmnExecInfo;
    }
    return SWL_RC_OK;
}

void wld_dmnMgt_cleanupDmnExecInfo(wld_dmnMgt_dmnExecInfo_t** ppDmnExecInfo) {
    ASSERTS_NOT_NULL(ppDmnExecInfo, , ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* pDmnExecInfo = *ppDmnExecInfo;
    ASSERTS_NOT_NULL(pDmnExecInfo, , ME, "NULL");
    wld_secDmnGrp_cleanup(&pDmnExecInfo->pGlobalDmnGrp);
    free(pDmnExecInfo);
    *ppDmnExecInfo = NULL;
}
