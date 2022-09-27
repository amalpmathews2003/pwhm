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
#include <stdlib.h>
#include <debug/sahtrace.h>
#include <errno.h>

#include <amxc/amxc_variant.h>
#include <amxm/amxm.h>

#include <sys/types.h>

#include "wld.h"
#include "swl/swl_common.h"
#include "swla/swla_table.h"
#include "wld_vendorModule_priv.h"
#include "wld_vendorModule.h"

#define ME "wld"

typedef int (* funcCallForward_f) (amxc_var_t* args, amxc_var_t* ret, const wld_vendorModule_handlers_cb* pModCbs);

// Vendor Modules NameSpace
static const char* sVendorModuleNS = "self";

static int s_forwardInit(amxc_var_t* args, amxc_var_t* ret _UNUSED, const wld_vendorModule_handlers_cb* pModCbs) {
    ASSERT_NOT_NULL(pModCbs, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pModCbs->fInitCb, WLD_ERROR_NOT_IMPLEMENTED, ME, "No init handler");
    wld_vendorModule_initInfo_t* initData =
        (wld_vendorModule_initInfo_t*) ((uintptr_t) amxc_var_get_const_uint64_t(args));
    bool retCode = pModCbs->fInitCb(initData);
    ASSERT_TRUE(retCode, WLD_ERROR, ME, "init failure");
    return WLD_OK;
}

static int s_forwardDeInit(amxc_var_t* args _UNUSED, amxc_var_t* ret _UNUSED, const wld_vendorModule_handlers_cb* pModCbs) {
    ASSERT_NOT_NULL(pModCbs, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(pModCbs->fDeinitCb, WLD_ERROR_NOT_IMPLEMENTED, ME, "No deinit handler");
    bool retCode = pModCbs->fDeinitCb();
    ASSERT_TRUE(retCode, WLD_ERROR, ME, "deinit failure");
    return WLD_OK;
}

SWL_TABLE(sVendorModuleApis,
          ARR(uint32_t apidId; char* apiName; void* apiFwdFunc; ),
          ARR(swl_type_uint32, swl_type_charPtr, swl_type_voidPtr, ),
          ARR({WLD_VENDORMODULE_API_INIT, "init", s_forwardInit},
              {WLD_VENDORMODULE_API_DEINIT, "deinit", s_forwardDeInit})
          );

const char* wld_vendorModule_nameSpace() {
    return sVendorModuleNS;
}

wld_vendorModule_apis_e wld_vendorModule_apiId(const char* apiName) {
    void* pApiId = swl_table_getMatchingValue(&sVendorModuleApis, 0, 1, apiName);
    if(pApiId) {
        return *((wld_vendorModule_apis_e*) pApiId);
    }
    return WLD_VENDORMODULE_API_MAX;
}

const char* wld_vendorModule_apiName(wld_vendorModule_apis_e apiId) {
    return (const char*) swl_table_getMatchingValue(&sVendorModuleApis, 1, 0, &apiId);
}

int wld_vendorModule_unregister(const char* modName) {
    ASSERT_NOT_NULL(modName, WLD_ERROR_INVALID_PARAM, ME, "invalid module name");
    amxm_shared_object_t* pSo = amxm_get_so(wld_vendorModule_nameSpace());
    ASSERT_NOT_NULL(pSo, WLD_ERROR, ME, "No local namespace available");
    amxm_module_t* pMod = amxm_so_get_module(pSo, ME);
    ASSERTI_NOT_NULL(pMod, WLD_ERROR_INVALID_PARAM, ME, "Module %s not found", modName);
    SAH_TRACEZ_INFO(ME, "Unregistering module %s", modName);
    int ret = amxm_module_deregister(&pMod);
    ASSERT_EQUALS(ret, 0, WLD_ERROR, ME, "Fail to unregister module %s", modName);
    return WLD_OK;
}

int wld_vendorModule_register(const char* modName, const wld_vendorModule_config_t* pConfig) {
    ASSERT_TRUE(modName && modName[0], WLD_ERROR_INVALID_PARAM, ME, "invalid module name");
    ASSERT_NOT_NULL(pConfig, WLD_ERROR_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pConfig->fGlobalCb, WLD_ERROR_INVALID_PARAM, ME, "Missing global handler");
    amxm_shared_object_t* pSo = amxm_get_so(wld_vendorModule_nameSpace());
    ASSERT_NOT_NULL(pSo, WLD_ERROR, ME, "No local namespace available");
    SAH_TRACEZ_INFO(ME, "Registering module %s", modName);
    amxm_module_t* pMod = amxm_so_get_module(pSo, modName);
    ASSERTI_NULL(pMod, WLD_OK, ME, "module %s already registered", modName);
    int ret = amxm_module_register(&pMod, pSo, modName);
    ASSERT_EQUALS(ret, 0, WLD_ERROR, ME, "Fail to register module %s", modName);
    uint32_t i;
    for(i = 0; i < sVendorModuleApis.nrTuples; i++) {
        const char* funcName = (const char*) swl_table_getValue(&sVendorModuleApis, i, 1);
        SAH_TRACEZ_INFO(ME, "Adding function %s.%s", modName, funcName);
        /*
         * Here, we are setting global handler for all added functions:
         * It is up to vendor module to require forwarding amx func call
         * to vendor module specific handlers
         */
        ret = amxm_module_add_function(pMod, funcName, pConfig->fGlobalCb);
        if(ret != 0) {
            SAH_TRACEZ_ERROR(ME, "Fail to add function %s.%s", modName, funcName);
            wld_vendorModule_unregister(modName);
            return WLD_ERROR;
        }
    }
    return WLD_OK;
}

int wld_vendorModule_forwardCall(const char* const funcName, amxc_var_t* args, amxc_var_t* ret,
                                 const wld_vendorModule_handlers_cb* const handlers) {
    ASSERTI_NOT_NULL(funcName, WLD_ERROR, ME, "Null function name");
    funcCallForward_f* pfForward = (funcCallForward_f*) swl_table_getMatchingValue(&sVendorModuleApis, 2, 1, funcName);
    ASSERTI_NOT_NULL(pfForward, WLD_ERROR_NOT_IMPLEMENTED, ME, "Api %s can not be forwarded", funcName);
    funcCallForward_f fForward = *pfForward;
    ASSERTI_NOT_NULL(fForward, WLD_ERROR, ME, "Api %s has invalid forward handler", funcName);
    return fForward(args, ret, handlers);
}
