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


#ifndef INCLUDE_WLD_WLD_EXTMOD_H_
#define INCLUDE_WLD_WLD_EXTMOD_H_

#include <amxc/amxc.h>
#include <swl/swl_common.h>

/**
 * Library to manage data for "extension modules", meaning modules that are not actively providing
 * chipset integration services, but would like to be able to directly interact with the wifi layer
 * without having to go around the data model.
 */

/**
 * @brief Register an extension module. Provide pointers to vendorName and extModName.
 * These pointers shall be stored internally, so please ensure they are persistent.
 *
 * @param vendorName: name of the extension module vendor company
 * @param extModName: the name of the extension module, should be unique per company
 *
 * @return a non-zero extended module index if successful, zero if failed. This
 * registered extension module index shall be used for extModData registration,
 * and shall be referred there as extModId.
 */
uint32_t wld_extMod_register(const char* vendorName, const char* extModName);

/**
 * @brief Unregister extension module with extModId.
 * This should only be done after cleaning up all extention module data from relevant objects.
 * unregistered an extension module shall NOT call the delete handlers linked with this extension module.
 *
 * @param extModId the registered extension module index
 */
void wld_extMod_unregister(uint32_t extModId);

/*
 * @brief check whether a extMod is registered with given id
 * @param extModId the registered extension module index
 * @return true if an extMod is currently registered with given id, false otherwsie.
 */
bool wld_extMod_isValidId(uint32_t extModId);

/**
 * Struct for handling data. Please do not use its internals.
 * Define is needed so that can be embedded natively in encompassing data structure.
 */
typedef struct {
    amxc_llist_t list;
} wld_extMod_dataList_t;

/**
 * Delete handler
 * @param coreObject: the radio / ap / ep / sta object that is being deleted, causing this delete handler to be called
 * @param extModData: the extModData provided upon registration.
 */
typedef void (* wld_extMod_deleteData_dcf)(void* coreObject, void* extModData);

/**
 * @brief initialize the ext mod data list
 * @param list the list to initialize
 * @return SWL_RC_OK if initialisation was successful, false otherwise
 */
swl_rc_ne wld_extMod_initDataList(wld_extMod_dataList_t* list);

/**
 * @brief register extModData for a given extenion module
 * @param list the data list to register the extModData with
 * @param extModId the registered extension module index
 * @param extModData the data being registered
 * @param deleteHandler the handler which shall be called if the list is cleaned up. Handler will not be called in case of unregister
 * @return
 *  * SWL_RC_OK if registration was successfull,
 *  * SWL_RC_INVALID_STATE if a previous registration was found with the given extModId
 *  * SWL_RC_INVALID_PARAM if for some other reason registration failed.
 */
swl_rc_ne wld_extMod_registerData(wld_extMod_dataList_t* list, uint32_t extModId, void* extModData, wld_extMod_deleteData_dcf deleteHandler);


/**
 * @brief retrieve the extModData that was registered for a given extModId
 * @param list the data list to retrieve the extModData from
 * @param extModId the registered extension module index
 * @return the extModData that was previously registered with the given extModId
 */
void* wld_extMod_getData(wld_extMod_dataList_t* list, uint32_t extModId);

/**
 * @brief unregister the extModData that was registered for a given extModId
 *
 * The extModData shall NOT be freed. This is still up to the extension module to do themselves.
 * This shall also NOT call the delete handler for the relevant object. This shall purely unlink the extension module data
 * for the given extModId.
 *
 * @param list the data list to unregister the extModData from
 * @param extModId the registered extension module index
 * @return SWL_RC_OK if unregistration was successfull, i.e. the data existed. SWL_RC_INVALID_PARAM otherwise.
 */
swl_rc_ne wld_extMod_unregisterData(wld_extMod_dataList_t* list, uint32_t extModId);


/**
 * @brief clean up the extModDataList for a given core object. This will call the delete handlers
 * for all actively registered extModData entries. It is not expected that these entries then unregister themselves
 * @param list the data list to clean up
 * @param coreObject the object that will be provided to the delete handlers
 * @return SWL_RC_OK if cleanup was successful, SWL_RC_INVALID_PARAM otherwise.
 */
swl_rc_ne wld_extMod_cleanupDataList(wld_extMod_dataList_t* list, void* coreObject);


#endif /* INCLUDE_WLD_WLD_EXTMOD_H_ */
