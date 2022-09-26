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

#ifndef INCLUDE_WLD_WLD_VENDORMODULE_MGR_H_
#define INCLUDE_WLD_WLD_VENDORMODULE_MGR_H_

#include "wld_vendorModule.h"

/**
 * @brief Load vendor modules already registered in local "Self" namespace
 *
 * @return positive number of vendor modules loaded from local namespace
 * or WLD_ERROR in case of failure
 */
int wld_vendorModuleMgr_loadInternal();

/**
 * @brief Open shared object file and load included vendor modules that
 * would be registered in local "Self" namespace
 *
 * @return positive number of vendor modules loaded from specific shared object file
 * or WLD_ERROR in case of failure
 */
int wld_vendorModuleMgr_loadExternal(const char* soFilePath);

/**
 * @brief Find and Open vendor modules shared object files located in directory
 * These will be registered in local "Self" namespace
 *
 * @return positive number of vendor modules loaded from all shared object files
 * or SWL_RC_ERROR in case of failure
 */
int wld_vendorModuleMgr_loadExternalDir(const char* soDirPath);

/**
 * @brief initialize all loaded vendor modules in local namespace
 *
 * @param pInfo Pointer of initialization info struct
 *
 * @return WLD_OK in case of success , or WLD_ERROR otherwise
 */
int wld_vendorModuleMgr_initAll(wld_vendorModule_initInfo_t* pInfo);

/**
 * @brief de-initialize all loaded vendor modules in local namespace
 *
 * @return WLD_OK in case of success , or WLD_ERROR otherwise
 */
int wld_vendorModuleMgr_deinitAll();

/**
 * @brief Unload all vendor module from local namespace
 * and close the relative source shared object file if external.
 *
 * @return WLD_OK in case of success , or WLD_ERROR otherwise
 */
int wld_vendorModuleMgr_unloadAll();

#endif /* INCLUDE_WLD_WLD_VENDORMODULE_MGR_H_ */
