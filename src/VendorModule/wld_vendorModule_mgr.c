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
#include <dirent.h>
#include <dlfcn.h>

#include <amxc/amxc_variant.h>
#include <amxm/amxm.h>

#include <sys/types.h>

#include "swl/swl_common.h"
#include "wld.h"
#include "wld_vendorModule_mgr.h"
#include "wld_vendorModule_priv.h"

#define ME "wld"

typedef struct {
    amxc_llist_it_t it;
    amxm_shared_object_t* so; // shared object from which the module is loaded
    char* name;
} wld_vendorModule_t;

static amxc_llist_t sVendorModulesList = {NULL, NULL};

// All vendor modules are loaded in the local namespace
static amxm_shared_object_t* s_selfSo() {
    return amxm_get_so(wld_vendorModule_nameSpace());
}

// Vendor module name is a unique key
static wld_vendorModule_t* s_findVendorModule(const char* modName) {
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &sVendorModulesList) {
        wld_vendorModule_t* pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        if(swl_str_matches(pVendorModule->name, modName)) {
            return pVendorModule;
        }
    }
    return NULL;
}

// returns counter of vendor modules per source shared object.
static int s_countVendorModules(amxm_shared_object_t* pSoSrc) {
    int count = 0;
    amxc_llist_it_t* it;
    amxc_llist_for_each(it, &sVendorModulesList) {
        wld_vendorModule_t* pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        if(pVendorModule->so == pSoSrc) {
            count++;
        }
    }
    return count;
}

static swl_rc_ne s_unloadVendorModule(wld_vendorModule_t* pVendorModule) {
    ASSERTS_NOT_NULL(pVendorModule, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_llist_it_take(&pVendorModule->it);
    amxm_module_t* pMod = amxm_so_get_module(s_selfSo(), pVendorModule->name);
    if(pMod != NULL) {
        SAH_TRACEZ_INFO(ME, "Unloading module %s", pMod->name);
        amxm_module_deregister(&pMod);
    }
    if((pVendorModule->so) &&
       (pVendorModule->so != s_selfSo()) &&
       (s_countVendorModules(pVendorModule->so) == 0)) {
        SAH_TRACEZ_INFO(ME, "Closing shared object %s", pVendorModule->so->name);
        amxm_so_close(&pVendorModule->so);
    }
    free(pVendorModule->name);
    free(pVendorModule);
    return SWL_RC_OK;
}

static wld_vendorModule_t* s_loadVendorModule(amxm_shared_object_t* pSoSrc, const char* modName) {
    wld_vendorModule_t* pVendorModule = s_findVendorModule(modName);
    ASSERTS_NULL(pVendorModule, pVendorModule, ME, "Loaded");
    ASSERT_NOT_NULL(pSoSrc, NULL, ME, "NULL");
    // vendor module shall be registered at this moment, in self namespace
    amxm_module_t* pMod = amxm_so_get_module(s_selfSo(), modName);
    ASSERT_NOT_NULL(pMod, NULL, ME, "Module %s not registered", modName);
    SAH_TRACEZ_INFO(ME, "Loading module %s", modName);
    pVendorModule = calloc(1, sizeof(wld_vendorModule_t));
    ASSERT_NOT_NULL(pVendorModule, NULL, ME, "NULL");
    pVendorModule->name = strdup(modName);
    if(pVendorModule->name == NULL) {
        SAH_TRACEZ_ERROR(ME, "Name alloc failure");
        s_unloadVendorModule(pVendorModule);
        return NULL;
    }
    //checking mandatory vendor module APIs
    if(!amxm_module_has_function(pMod, wld_vendorModule_apiName(WLD_VENDORMODULE_API_INIT)) ||
       !amxm_module_has_function(pMod, wld_vendorModule_apiName(WLD_VENDORMODULE_API_DEINIT))) {
        SAH_TRACEZ_ERROR(ME, "Missing mandatory APIs in module %s", modName);
        s_unloadVendorModule(pVendorModule);
        return NULL;
    }
    //setting source shared object
    pVendorModule->so = pSoSrc;
    amxc_llist_append(&sVendorModulesList, &pVendorModule->it);
    SAH_TRACEZ_INFO(ME, "Vendor Module %s loaded", pVendorModule->name);
    return pVendorModule;
}

static swl_rc_ne s_execVendorModuleApi(wld_vendorModule_t* pVendorModule, wld_vendorModule_apis_e apiId, amxc_var_t* pArgs, amxc_var_t* pRet) {
    ASSERT_NOT_NULL(pVendorModule, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxm_module_t* pMod = amxm_so_get_module(s_selfSo(), pVendorModule->name);
    ASSERT_NOT_NULL(pMod, SWL_RC_NOT_AVAILABLE, ME, "Module %s not registered", pVendorModule->name);
    const char* funcName = wld_vendorModule_apiName(apiId);
    ASSERTW_STR(funcName, SWL_RC_NOT_IMPLEMENTED, ME, "Module %s has not function (%d)", pVendorModule->name, apiId);
    SAH_TRACEZ_INFO(ME, "Executing %s.%s", pVendorModule->name, funcName);
    amxc_var_t args;
    amxc_var_t ret;
    amxc_var_init(&args);
    amxc_var_init(&ret);
    if(pArgs == NULL) {
        pArgs = &args;
    }
    if(pRet == NULL) {
        pRet = &ret;
    }
    int error = amxm_module_execute_function(pMod, funcName, pArgs, pRet);
    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    ASSERT_EQUALS(error, 0, SWL_RC_ERROR, ME, "fail to execute %s.%s", pVendorModule->name, funcName);
    return SWL_RC_OK;
}

static swl_rc_ne s_initVendorModule(wld_vendorModule_t* pVendorModule, wld_vendorModule_initInfo_t* pInfo) {
    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set_uint64_t(&args, (uintptr_t) pInfo);
    swl_rc_ne rc = s_execVendorModuleApi(pVendorModule, WLD_VENDORMODULE_API_INIT, &args, NULL);
    amxc_var_clean(&args);
    return rc;
}

static swl_rc_ne s_deinitVendorModule(wld_vendorModule_t* pVendorModule) {
    return s_execVendorModuleApi(pVendorModule, WLD_VENDORMODULE_API_DEINIT, NULL, NULL);
}

static swl_rc_ne s_loadDefaultsVendorModule(wld_vendorModule_t* pVendorModule) {
    return s_execVendorModuleApi(pVendorModule, WLD_VENDORMODULE_API_LOAD_DEFAULTS, NULL, NULL);
}

static int s_loadSharedObj(amxm_shared_object_t* pSoSrc) {
    ASSERT_NOT_NULL(pSoSrc, SWL_RC_ERROR, ME, "NULL");
    size_t modCount = amxm_so_count_modules(s_selfSo());
    ASSERTW_TRUE((modCount > 0), SWL_RC_ERROR, ME, "no modules registered");
    char* modName = NULL;
    while(modCount > 0) {
        modCount--;
        modName = amxm_so_probe(s_selfSo(), modCount);
        s_loadVendorModule(pSoSrc, modName);
        free(modName);
    }
    int loaded = s_countVendorModules(pSoSrc);
    SAH_TRACEZ_INFO(ME, "%d Vendor Modules loaded from %s", loaded, pSoSrc->name);
    return loaded;
}

int wld_vendorModuleMgr_loadInternal() {
    return s_loadSharedObj(s_selfSo());
}

int wld_vendorModuleMgr_loadExternal(const char* soFilePath) {
    amxm_shared_object_t* pSoSrc = NULL;
    int ret = -1;
    dlerror();
    ret = amxm_so_open(&pSoSrc, soFilePath, soFilePath);
    char* loadError = dlerror();
    ASSERT_EQUALS(ret, 0, SWL_RC_ERROR, ME, "fail to open %s (%s)", soFilePath, loadError);
    ret = s_loadSharedObj(pSoSrc);
    if(ret <= 0) {
        SAH_TRACEZ_ERROR(ME, "No vendor modules loaded from %s", soFilePath);
        amxm_so_close(&pSoSrc);
    }
    return ret;
}

static int s_filterModNames(const struct dirent* pEntry) {
    const char* fname = pEntry->d_name;
    if(swl_str_matches(fname, ".") || swl_str_matches(fname, "..")) {
        return 0;
    }
    if(!swl_str_startsWith(fname, WLD_VENDOR_MODULE_PREFIX) || !strstr(fname, ".so")) {
        return 0;
    }
    return 1;
}

int wld_vendorModuleMgr_loadExternalDir(const char* soDirPath) {
    ASSERTS_STR(soDirPath, SWL_RC_INVALID_PARAM, ME, "NULL");
    struct dirent** namelist;
    int count = 0;
    int n = scandir(soDirPath, &namelist, s_filterModNames, alphasort);
    ASSERT_NOT_EQUALS(n, -1, SWL_RC_ERROR, ME, "fail to scan dir %s", soDirPath);
    while(n--) {
        const char* fname = namelist[n]->d_name;
        size_t fullPathLen = swl_str_len(soDirPath) + swl_str_len(fname) + 2;
        char fullPath[fullPathLen];
        memset(fullPath, 0, sizeof(fullPath));
        snprintf(fullPath, sizeof fullPath, "%s/%s", soDirPath, fname);
        int ret = wld_vendorModuleMgr_loadExternal(fullPath);
        if(ret < 0) {
            SAH_TRACEZ_ERROR(ME, "Fail to load file %s", fullPath);
        } else {
            count += ret;
        }
        free(namelist[n]);
    }
    free(namelist);
    return count;
}

int wld_vendorModuleMgr_unloadAll() {
    amxc_llist_it_t* it = NULL;
    wld_vendorModule_t* pVendorModule = NULL;
    while((it = amxc_llist_get_first(&sVendorModulesList))) {
        pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        s_unloadVendorModule(pVendorModule);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_vendorModuleMgr_initAll(wld_vendorModule_initInfo_t* pInfo) {
    amxc_llist_it_t* it = NULL;
    wld_vendorModule_t* pVendorModule = NULL;
    amxc_llist_for_each(it, &sVendorModulesList) {
        pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        s_initVendorModule(pVendorModule, pInfo);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_vendorModuleMgr_deinitAll() {
    amxc_llist_it_t* it = NULL;
    wld_vendorModule_t* pVendorModule = NULL;
    amxc_llist_for_each(it, &sVendorModulesList) {
        pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        s_deinitVendorModule(pVendorModule);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_vendorModuleMgr_loadDefaultsAll() {
    amxc_llist_it_t* it = NULL;
    wld_vendorModule_t* pVendorModule = NULL;
    amxc_llist_for_each(it, &sVendorModulesList) {
        pVendorModule = amxc_llist_it_get_data(it, wld_vendorModule_t, it);
        s_loadDefaultsVendorModule(pVendorModule);
    }
    return SWL_RC_OK;
}
