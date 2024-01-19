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
#include <stdlib.h>

#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "wld_secDmn.h"

#define ME "secDmn"

void wld_secDmn_restartCb(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    wld_wpaCtrlMngr_disconnect(pSecDmn->wpaCtrlMngr);
    wld_dmn_startDeamon(pSecDmn->dmnProcess);
    wld_wpaCtrlMngr_connect(pSecDmn->wpaCtrlMngr);
}

static void s_restartDeamon(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    if(pSecDmn->handlers.restartCb != NULL) {
        pSecDmn->handlers.restartCb(pSecDmn, pSecDmn->userData);
        return;
    }
    wld_secDmn_restartCb(pSecDmn);
}

static void s_stopDeamon(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    if(pSecDmn->handlers.stopCb) {
        pSecDmn->handlers.stopCb(pSecDmn, pSecDmn->userData);
        return;
    }
    //finalize secDmn cleanup
    wld_secDmn_stop(pSecDmn);
}

swl_rc_ne wld_secDmn_init(wld_secDmn_t** ppSecDmn, char* cmd, char* startArgs, char* cfgFile, char* ctrlIfaceDir) {
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "invalid cmd");
    ASSERT_NOT_NULL(ppSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmn_t* pSecDmn = *ppSecDmn;
    if(pSecDmn == NULL) {
        pSecDmn = calloc(1, sizeof(wld_secDmn_t));
        ASSERT_NOT_NULL(pSecDmn, SWL_RC_ERROR, ME, "NULL");
        *ppSecDmn = pSecDmn;
    }
    ASSERT_NULL(pSecDmn->dmnProcess, SWL_RC_OK, ME, "already initialized");
    pSecDmn->dmnProcess = calloc(1, sizeof(wld_process_t));
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    wld_deamonEvtHandlers handlers = {
        .restartCb = s_restartDeamon,
        .stopCb = s_stopDeamon,
    };
    if((!wld_dmn_initializeDeamon(pSecDmn->dmnProcess, cmd)) ||
       (!wld_dmn_setDeamonEvtHandlers(pSecDmn->dmnProcess, &handlers, pSecDmn)) ||
       (!wld_wpaCtrlMngr_init(&pSecDmn->wpaCtrlMngr, pSecDmn))) {
        SAH_TRACEZ_ERROR(ME, "fail to initialize daemon %s", cmd);
        wld_dmn_cleanupDaemon(pSecDmn->dmnProcess);
        free(pSecDmn->dmnProcess);
        pSecDmn->dmnProcess = NULL;
        return SWL_RC_ERROR;
    }
    wld_dmn_setArgList(pSecDmn->dmnProcess, startArgs);
    swl_str_copyMalloc(&pSecDmn->cfgFile, cfgFile);
    swl_str_copyMalloc(&pSecDmn->ctrlIfaceDir, ctrlIfaceDir);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_setEvtHandlers(wld_secDmn_t* pSecDmn, wld_secDmnEvtHandlers* pHandlers, void* userData) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    pSecDmn->userData = userData;
    if(pHandlers == NULL) {
        memset(&pSecDmn->handlers, 0, sizeof(wld_secDmnEvtHandlers));
    } else {
        memcpy(&pSecDmn->handlers, pHandlers, sizeof(wld_secDmnEvtHandlers));
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_cleanup(wld_secDmn_t** ppSecDmn) {
    ASSERTS_NOT_NULL(ppSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmn_t* pSecDmn = *ppSecDmn;
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_OK, ME, "cleaned up");
    wld_wpaCtrlMngr_cleanup(&pSecDmn->wpaCtrlMngr);
    wld_dmn_cleanupDaemon(pSecDmn->dmnProcess);
    free(pSecDmn->dmnProcess);
    pSecDmn->dmnProcess = NULL;
    free(pSecDmn->cfgFile);
    pSecDmn->cfgFile = NULL;
    free(pSecDmn->ctrlIfaceDir);
    pSecDmn->ctrlIfaceDir = NULL;
    free(pSecDmn);
    *ppSecDmn = NULL;
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_start(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_FALSE(wld_dmn_isRunning(pSecDmn->dmnProcess), SWL_RC_OK, ME, "already running");
    wld_dmn_startDeamon(pSecDmn->dmnProcess);
    wld_wpaCtrlMngr_connect(pSecDmn->wpaCtrlMngr);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_stop(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_TRUE(wld_dmn_isEnabled(pSecDmn->dmnProcess), SWL_RC_ERROR, ME, "not running");
    wld_wpaCtrlMngr_disconnect(pSecDmn->wpaCtrlMngr);
    wld_dmn_stopDeamon(pSecDmn->dmnProcess);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_reload(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERT_TRUE(wld_dmn_isEnabled(pSecDmn->dmnProcess), SWL_RC_ERROR, ME, "not running");
    amxp_subproc_kill(pSecDmn->dmnProcess->process->proc, SIGHUP);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_setArgs(wld_secDmn_t* pSecDmn, char* startArgs) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");

    wld_dmn_setArgList(pSecDmn->dmnProcess, startArgs);

    return SWL_RC_OK;
}

bool wld_secDmn_isRunning(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return wld_dmn_isRunning(pSecDmn->dmnProcess);
}

bool wld_secDmn_isEnabled(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return wld_dmn_isEnabled(pSecDmn->dmnProcess);
}

bool wld_secDmn_isAlive(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return (wld_secDmn_isRunning(pSecDmn) && wld_wpaCtrlMngr_isConnected(pSecDmn->wpaCtrlMngr));
}

