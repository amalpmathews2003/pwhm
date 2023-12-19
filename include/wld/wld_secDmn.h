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

#ifndef INCLUDE_WLD_WLD_SECDMN_H_
#define INCLUDE_WLD_WLD_SECDMN_H_

#include "wld_wpaCtrlMngr.h"
#include "wld_daemon.h"

typedef struct wld_secDmn wld_secDmn_t;
typedef void (* wld_secDmn_restartHandler)(wld_secDmn_t* pSecDmn, void* userdata);
typedef void (* wld_secDmn_stopHandler)(wld_secDmn_t* pSecDmn, void* userdata);

typedef struct {
    wld_secDmn_restartHandler restartCb; // optional handler to manage security daemon restarting
    wld_secDmn_stopHandler stopCb;       // optional handler to get notification for security daemon process end
} wld_secDmnEvtHandlers;

struct wld_secDmn {
    wld_wpaCtrlMngr_t* wpaCtrlMngr; /* wpaCtrlMngr */
    wld_process_t* dmnProcess;      /* daemon context. */
    char* cfgFile;                  /* config file path*/
    char* ctrlIfaceDir;             /* ctrl_interface directory: /var/run/xxx/*/
    wld_secDmnEvtHandlers handlers; /* optional handlers of secDmn events*/
    void* userData;                 /* optional user data available in restart handler. */
};

swl_rc_ne wld_secDmn_init(wld_secDmn_t** ppSecDmn, char* cmd, char* startArgs, char* cfgFile, char* ctrlIfaceDir);
swl_rc_ne wld_secDmn_setEvtHandlers(wld_secDmn_t* pSecDmn, wld_secDmnEvtHandlers* pHandlers, void* userData);
swl_rc_ne wld_secDmn_cleanup(wld_secDmn_t** ppSecDmn);
swl_rc_ne wld_secDmn_start(wld_secDmn_t* pSecDmn);
swl_rc_ne wld_secDmn_stop(wld_secDmn_t* pSecDmn);
swl_rc_ne wld_secDmn_reload(wld_secDmn_t* pSecDmn);
swl_rc_ne wld_secDmn_setArgs(wld_secDmn_t* pSecDmn, char* startArgs);
void wld_secDmn_restartCb(wld_secDmn_t* pSecDmn);
bool wld_secDmn_isRunning(wld_secDmn_t* pSecDmn);
bool wld_secDmn_isAlive(wld_secDmn_t* pSecDmn);

#define CALL_SECDMN_MGR_EXT(pSecDmn, fName, ifName, ...) \
    if(pSecDmn != NULL) { \
        CALL_MGR_EXT(pSecDmn->wpaCtrlMngr, fName, ifName, __VA_ARGS__) \
    }

#endif /* INCLUDE_WLD_WLD_SECDMN_H_ */
