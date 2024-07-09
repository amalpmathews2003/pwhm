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
#include "swl/swl_common.h"
#include "wld/wld_fsm.h"
#include "wld_th_fsm.h"
#include "test-toolbox/ttb.h"

#include "wld_th_radio.h"
#include "wld_th_vap.h"


static bool s_testFsm = false;



static bool s_doRadActionEnableDown(T_Radio* pRad) {
    printf("ACTION RAD -%s-  WLD_TH_FSM_SET_RAD_ENABLE_DOWN\n", pRad->Name);
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(pRad);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_RAD_ENABLE_DOWN]++;
    return true;
}


static bool s_doRadActionIwPriv(T_Radio* pRad) {
    printf("ACTION RAD -%s-  WLD_TH_FSM_SET_IWPRIV_CMDS\n", pRad->Name);
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(pRad);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_IWPRIV_CMDS]++;
    return true;
}

static bool s_doRadActionEnable(T_Radio* pRad) {
    printf("ACTION RAD -%s-  WLD_TH_FSM_SET_RAD_ENABLE\n", pRad->Name);
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(pRad);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_RAD_ENABLE]++;
    return true;
}

static void s_callVap(T_AccessPoint* pAP, T_Radio* pRad, wld_th_fsmStates_e state) {
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    SWL_CALL(vd->fsmCallback[state], pAP, pRad, state);
}

static bool s_doVapActionDown(T_AccessPoint* pAP, T_Radio* pRad) {
    printf("ACTION RAD -%s- VAP -%s-  WLD_TH_FSM_SET_VAP_ENABLE_DOWN\n", pRad->Name, pAP->name);
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_VAP_ENABLE_DOWN]++;
    s_callVap(pAP, pRad, WLD_TH_FSM_SET_VAP_ENABLE_DOWN);
    return true;
}


static bool s_doVapActionIwPriv(T_AccessPoint* pAP, T_Radio* pRad) {
    printf("ACTION RAD -%s- VAP -%s-  WLD_TH_FSM_SET_IWPRIV_CMDS\n", pRad->Name, pAP->name);
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_IWPRIV_CMDS]++;
    s_callVap(pAP, pRad, WLD_TH_FSM_SET_IWPRIV_CMDS);
    return true;
}


static bool s_doVapActionUp(T_AccessPoint* pAP, T_Radio* pRad) {
    printf("ACTION RAD -%s- VAP -%s-  WLD_TH_FSM_SET_VAP_ENABLE\n", pRad->Name, pAP->name);
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);
    vd->fsmCommits[WLD_TH_FSM_SET_VAP_ENABLE]++;
    s_callVap(pAP, pRad, WLD_TH_FSM_SET_VAP_ENABLE);
    return true;
}


static bool s_doEpActionDown(T_EndPoint* pEP, T_Radio* pRad) {
    printf("ACTION RAD -%s- pEP -%s-  WLD_TH_FSM_EP_DISCONNECT_AP\n", pRad->Name, pEP->alias);
    return true;
}

static bool s_doEpActionIwPriv(T_EndPoint* pEP, T_Radio* pRad) {
    printf("ACTION RAD -%s- pEP -%s-  WLD_TH_FSM_SET_IWPRIV_CMDS\n", pRad->Name, pEP->alias);
    return true;
}

static bool s_doEpActionUp(T_EndPoint* pEP, T_Radio* pRad) {
    printf("ACTION RAD -%s- pEP -%s-  WLD_TH_FSM_SET_EP_ENABLE\n", pRad->Name, pEP->alias);
    return true;
}


wld_fsmMngr_action_t wld_th_fsm_actions[] = {
    {FSM_ACTION(WLD_TH_FSM_EP_DISCONNECT_AP), .doEpFsmAction = s_doEpActionDown},
    {FSM_ACTION(WLD_TH_FSM_SET_VAP_ENABLE_DOWN), .doVapFsmAction = s_doVapActionDown},
    {FSM_ACTION(WLD_TH_FSM_SET_RAD_ENABLE_DOWN), .doRadFsmAction = s_doRadActionEnableDown},
    {FSM_ACTION(WLD_TH_FSM_SET_IWPRIV_CMDS), .doRadFsmAction = s_doRadActionIwPriv,
        .doVapFsmAction = s_doVapActionIwPriv,
        .doEpFsmAction = s_doEpActionIwPriv},
    {FSM_ACTION(WLD_TH_FSM_SET_RAD_ENABLE), .doRadFsmAction = s_doRadActionEnable},
    {FSM_ACTION(WLD_TH_FSM_SET_VAP_ENABLE), .doVapFsmAction = s_doVapActionUp},
    {FSM_ACTION(WLD_TH_FSM_SET_EP_ENABLE), .doEpFsmAction = s_doEpActionUp},
};

wld_fsmMngr_t wld_th_fsm_mngr = {
    .doLock = NULL,
    .doUnlock = NULL,
    .ensureLock = NULL,
    .waitGlobalSync = NULL,
    .doRestart = NULL,
    .checkPreDependency = NULL,
    .checkEpDependency = NULL,
    .checkVapDependency = NULL,
    .checkRadDependency = NULL,
    .doFinish = NULL,
    .actionList = wld_th_fsm_actions,
    .nrFsmBits = WLD_TH_FSM_MAX,
};

void whm_th_fsm_useMgr(bool testFsm) {
    s_testFsm = testFsm;
}

void whm_th_fsm_doInit(vendor_t* vendor) {
    if(s_testFsm) {
        wld_fsm_init(vendor, &wld_th_fsm_mngr);
    }
}
