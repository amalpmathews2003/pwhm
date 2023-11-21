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
 * Extention module handling.
 *
 * This module is used to register extention modules that are not doing the "chipset integration"
 *
 * This allows multiple modules to directly interact locally with the data model, without going over the bus.
 */

#include <stdlib.h>
#include <swl/swl_string.h>

#include "wld_extMod.h"


#define ME "wldExtM"

typedef struct {
    amxc_llist_it_t it;
    uint32_t index;
    const char* vendorName;
    const char* moduleName;
} wld_extModuleInfo_t;

// List of generic extention modules
static amxc_llist_t s_extModules = {NULL, NULL};


static wld_extModuleInfo_t* s_getExtModByNames(const char* vendorName, const char* extModName) {
    amxc_llist_for_each(it, &s_extModules) {
        wld_extModuleInfo_t* mod = amxc_llist_it_get_data(it, wld_extModuleInfo_t, it);
        if(swl_str_matches(vendorName, mod->vendorName) && swl_str_matches(extModName, mod->moduleName)) {
            return mod;
        }
    }
    return NULL;
}


static wld_extModuleInfo_t* s_getExtModById(uint32_t id) {
    amxc_llist_for_each(it, &s_extModules) {
        wld_extModuleInfo_t* mod = amxc_llist_it_get_data(it, wld_extModuleInfo_t, it);
        if(mod->index == id) {
            return mod;
        }
    }
    return NULL;
}


uint32_t wld_extMod_register(const char* vendorName, const char* extModName) {
    wld_extModuleInfo_t* oldMod = s_getExtModByNames(vendorName, extModName);
    ASSERT_NULL(oldMod, 0, ME, "Module %s : %s already registered", extModName, vendorName);

    wld_extModuleInfo_t* newMod = calloc(1, sizeof(wld_extModuleInfo_t));
    ASSERT_NOT_NULL(newMod, 0, ME, "ALLOC FAIL");

    uint32_t index = 0;
    if(amxc_llist_size(&s_extModules) == 0) {
        index = 0;
    } else {
        amxc_llist_it_t* it = amxc_llist_get_last(&s_extModules);
        wld_extModuleInfo_t* prevMod = amxc_llist_it_get_data(it, wld_extModuleInfo_t, it);
        index = prevMod->index;
    }
    newMod->index = index + 1;
    newMod->moduleName = extModName;
    newMod->vendorName = vendorName;

    amxc_llist_append(&s_extModules, &newMod->it);
    return newMod->index;
}


void wld_extMod_unregister(uint32_t extModId) {
    wld_extModuleInfo_t* oldMod = s_getExtModById(extModId);
    ASSERT_NOT_NULL(oldMod, , ME, "NULL mod %u", extModId);
    amxc_llist_it_take(&oldMod->it);
    free(oldMod);
}

bool wld_extMod_isValidId(uint32_t extModId) {
    return (s_getExtModById(extModId) != NULL);
}

typedef struct {
    amxc_llist_it_t it;
    uint32_t extModId;
    void* data;
    wld_extMod_deleteData_dcf deleteHandler;
} wld_extMod_dataEntry_t;


swl_rc_ne wld_extMod_initDataList(wld_extMod_dataList_t* list) {
    ASSERT_NOT_NULL(list, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_llist_init(&list->list);
    return SWL_RC_OK;
}

wld_extMod_dataEntry_t* s_getData(wld_extMod_dataList_t* list, uint32_t extModId) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");

    amxc_llist_for_each(it, &list->list) {
        wld_extMod_dataEntry_t* reg = amxc_llist_it_get_data(it, wld_extMod_dataEntry_t, it);
        if(reg->extModId == extModId) {
            return reg;
        }
    }
    return NULL;
}



swl_rc_ne wld_extMod_registerData(wld_extMod_dataList_t* list, uint32_t extModId, void* extModData, wld_extMod_deleteData_dcf deleteHandler) {
    ASSERT_NOT_NULL(list, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(extModData, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(wld_extMod_isValidId(extModId), SWL_RC_INVALID_PARAM, ME, "ZERO");
    ASSERT_NULL(s_getData(list, extModId), SWL_RC_INVALID_STATE, ME, "NULL");

    wld_extMod_dataEntry_t* handler = calloc(1, sizeof(wld_extMod_dataEntry_t));
    ASSERT_NOT_NULL(handler, SWL_RC_NOT_AVAILABLE, ME, "ALLOC");

    handler->extModId = extModId;
    handler->data = extModData;
    handler->deleteHandler = deleteHandler;

    amxc_llist_append(&list->list, &handler->it);

    return SWL_RC_OK;
}


void* wld_extMod_getData(wld_extMod_dataList_t* extDataList, uint32_t extModId) {
    wld_extMod_dataEntry_t* entry = s_getData(extDataList, extModId);
    if(entry == NULL) {
        return NULL;
    }
    return entry->data;
}

swl_rc_ne wld_extMod_unregisterData(wld_extMod_dataList_t* extDataList, uint32_t extModId) {
    wld_extMod_dataEntry_t* entry = s_getData(extDataList, extModId);
    ASSERT_NOT_NULL(entry, SWL_RC_INVALID_PARAM, ME, "NULL");

    amxc_llist_it_take(&entry->it);
    free(entry);
    return SWL_RC_OK;
}


swl_rc_ne wld_extMod_cleanupDataList(wld_extMod_dataList_t* list, void* coreObject) {
    ASSERT_NOT_NULL(list, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(coreObject, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_llist_for_each(it, &list->list) {
        wld_extMod_dataEntry_t* entry = amxc_llist_it_get_data(it, wld_extMod_dataEntry_t, it);
        if(entry->deleteHandler != NULL) {
            entry->deleteHandler(coreObject, entry->data);
        }
        amxc_llist_it_take(&entry->it);
        free(entry);
    }

    amxc_llist_clean(&list->list, NULL);

    return SWL_RC_OK;
}


