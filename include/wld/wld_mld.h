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

#ifndef SRC_INCLUDE_WLD_WLD_MLD_H_
#define SRC_INCLUDE_WLD_WLD_MLD_H_

#include "wld_types.h"

swl_rc_ne wld_mld_initMgr(wld_mldMgr_t** ppMgr);
swl_rc_ne wld_mld_deinitMgr(wld_mldMgr_t** ppMgr);

wld_mldLink_t* wld_mld_registerLink(T_SSID* pSSID, int32_t unit);
swl_rc_ne wld_mld_unregisterLink(T_SSID* pSSID);
wld_mldLink_t* wld_mld_selectPrimaryLink(wld_mldLink_t* pLink);
swl_rc_ne wld_mld_setPrimaryLink(wld_mldLink_t* pLink);
wld_mldLink_t* wld_mld_getPrimaryLink(wld_mldLink_t* pLink);
T_SSID* wld_mld_getPrimaryLinkSsid(wld_mldLink_t* pLink);
const char* wld_mld_getPrimaryLinkIfName(wld_mldLink_t* pLink);
int32_t wld_mld_getPrimaryLinkIfIndex(wld_mldLink_t* pLink);
swl_rc_ne wld_mld_setLinkId(wld_mldLink_t* pLink, int32_t linkId);
int16_t wld_mld_getLinkId(const wld_mldLink_t* pLink);
const char* wld_mld_getLinkIfName(wld_mldLink_t* pLink);
bool wld_mld_isLinkActive(wld_mldLink_t* pLink);
bool wld_mld_isLinkEnabled(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighLinks(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighActiveLinks(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighEnabledLinks(wld_mldLink_t* pLink);

#endif /* SRC_INCLUDE_WLD_WLD_MLD_H_ */
