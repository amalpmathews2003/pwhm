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
*/

#ifndef INCLUDE_WLD_UTILS_WLD_DMNMGT_H_
#define INCLUDE_WLD_UTILS_WLD_DMNMGT_H_

#include "swl/swl_common.h"
#include "wld/wld_types.h"
#include "wld/wld_secDmn_types.h"

typedef struct {
    swl_trl_e globalDmnSupported;             //global daemon is platform dependent, indicated by genPlugin/VdrMod, 0:No, 1:Yes, 2:Unknown
    bool globalDmnRequired;                   //global daemon may be required to manage wifi 6e/7 features
    wld_secDmnGrp_t* pGlobalDmnGrp;           //execution group when global instance is enabled
} wld_dmnMgt_dmnExecInfo_t;

typedef struct {
    swl_trl_e useGlobalInstance;              //use global daemon: 0:Off, 1:On, 2:auto ie enabled when required
} wld_dmnMgt_dmnExecSettings_t;

typedef struct {
    char* name;
    amxd_object_t* object;
    wld_dmnMgt_dmnExecSettings_t exec;
} wld_dmnMgt_dmnCtx_t;

const wld_dmnMgt_dmnCtx_t* wld_dmnMgt_getDmnCtx(const char* dmnName);
swl_rc_ne wld_dmnMgt_initDmnExecInfo(wld_dmnMgt_dmnExecInfo_t** ppDmnExecInfo);
void wld_dmnMgt_cleanupDmnExecInfo(wld_dmnMgt_dmnExecInfo_t** ppDmnExecInfo);

#endif /* INCLUDE_WLD_UTILS_WLD_DMNMGT_H_ */
