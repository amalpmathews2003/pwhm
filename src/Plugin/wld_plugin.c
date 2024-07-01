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
#include "swl/fileOps/swl_fileUtils.h"
#include "wld/Features/wld_persist.h"
#include "wld/wld_eventing.h"

#define ME "wld"


static void s_sendLifecycleEvent(wld_lifecycleEvent_e change) {
    wld_lifecycleEvent_t event = {
        .event = change,
    };
    wld_event_trigger_callback(gWld_queue_lifecycleEvent, &event);

}

static void s_addGenRadiosAndLoadConf(void* userdata _UNUSED) {
    wifiGen_addRadios();
    wld_persist_onStart();
}

void _app_start(const char* const event_name _UNUSED,
                const amxc_var_t* const event_data _UNUSED,
                void* const priv _UNUSED) {
    SAH_TRACEZ_WARNING(ME, "data model is loaded");
    wifiGen_waitForGenRadios(NULL, s_addGenRadiosAndLoadConf);
    s_sendLifecycleEvent(WLD_LIFECYCLE_EVENT_DATAMODEL_LOADED);
}

static bool s_initPluginConf() {
    const char* confFile = amxc_var_constcast(cstring_t, amxo_parser_get_config(get_wld_plugin_parser(), "config-storage-file"));
    ASSERTS_STR(confFile, false, ME, "empty");
    ASSERTS_FALSE(swl_fileUtils_existsFile(confFile), true, ME, "%s exists", confFile);
    const char* confDir = amxc_var_constcast(cstring_t, amxo_parser_get_config(get_wld_plugin_parser(), "config-storage-dir"));
    ASSERT_TRUE(swl_fileUtils_mkdirRecursive(confDir), false, ME, "Fail to create conf dir %s", confDir);
    return swl_fileUtils_writeFile("", confFile);
}

static char* s_getModDir(amxo_parser_t* parser) {
    //1) get conf: pwhm vendor modules path
    amxc_var_t* cfgVar = amxo_parser_get_config(parser, "wld.mod-dir");
    char* modDir = amxc_var_get_cstring_t(cfgVar);
    amxc_var_delete(&cfgVar);
    if(!swl_str_isEmpty(modDir)) {
        return modDir;
    }
    //2) if not available, build alternative vdr module path from the plugin path
    W_SWL_FREE(modDir);
    cfgVar = amxo_parser_get_config(parser, "plugin-dir");
    const char* pluginDir = amxc_var_get_const_cstring_t(cfgVar);
    if(!swl_str_isEmpty(pluginDir)) {
        amxc_string_t modDirStr;
        amxc_string_init(&modDirStr, swl_str_len(pluginDir));
        amxc_string_setf(&modDirStr, "%s/%s", pluginDir, "/wld/modules");
        modDir = amxc_string_take_buffer(&modDirStr);
        amxc_string_clean(&modDirStr);
    }
    amxc_var_delete(&cfgVar);
    return modDir;
}

int _wld_main(int reason,
              amxd_dm_t* dm,
              amxo_parser_t* parser) {
    switch(reason) {
    case 0:     // START
        SAH_TRACEZ_WARNING(ME, "WLD plugin started");
        wld_plugin_init(dm, parser);
        wld_plugin_start();
        char* modDir = s_getModDir(parser);
        if(swl_str_isEmpty(modDir) || (wld_vendorModuleMgr_loadExternalDir(modDir) < 0)) {
            wld_vendorModuleMgr_loadInternal();
        }
        free(modDir);

        wifiGen_init();
        s_initPluginConf();

        //Init info struct to be filled with need info
        wld_vendorModule_initInfo_t initInfo;
        memset(&initInfo, 0, sizeof(initInfo));
        wld_vendorModuleMgr_initAll(&initInfo);

        s_sendLifecycleEvent(WLD_LIFECYCLE_EVENT_INIT_DONE);

        break;
    case 1:     // STOP
        SAH_TRACEZ_WARNING(ME, "WLD plugin stopped");
        s_sendLifecycleEvent(WLD_LIFECYCLE_EVENT_CLEANUP_START);

        wld_vendorModuleMgr_deinitAll();
        wld_cleanup();
        wld_vendorModuleMgr_unloadAll();
        wifiGen_deinit();
        break;
    default:
        break;
    }
    return 0;
}
