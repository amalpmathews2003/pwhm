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

#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_vendorModule_mgr.h"
#include "swl/swl_assert.h"
#include "wifiGen.h"

#define ME "wld"

static void s_delayLoadDmConf(amxp_timer_t* timer, void* userdata _UNUSED) {
    amxp_timer_delete(&timer);
    wld_plugin_loadDmConf();
}

int _wld_main(int reason,
              amxd_dm_t* dm,
              amxo_parser_t* parser) {
    switch(reason) {
    case 0:     // START
        SAH_TRACEZ_WARNING(ME, "WLD plugin started");
        wld_plugin_init(dm, parser);
        wld_plugin_start();
        wld_vendorModuleMgr_loadInternal();

        //Init info struct to be filled with need info
        wld_vendorModule_initInfo_t initInfo;
        memset(&initInfo, 0, sizeof(initInfo));

        wifiGen_init();
        wld_vendorModuleMgr_initAll(&initInfo);
        wifiGen_addRadios();

        /*
         * Delay loading DM config to let plugin enter event loop
         * and be ready for timer signals
         */
        amxp_timer_t* timer = NULL;
        amxp_timer_new(&timer, s_delayLoadDmConf, NULL);
        amxp_timer_start(timer, 100);
        break;
    case 1:     // STOP
        SAH_TRACEZ_WARNING(ME, "WLD plugin stopped");
        wld_vendorModuleMgr_deinitAll();
        wld_vendorModuleMgr_unloadAll();
        wld_cleanup();
        break;
    default:
        break;
    }
    return 0;
}
