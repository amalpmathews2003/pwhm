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
#include "swl/fileOps/swl_fileUtils.h"

#define ME "wldPst"

static bool s_getSavedConfPath(amxc_string_t* path) {
    ASSERT_NOT_NULL(path, false, ME, "NULL");
    amxc_string_reset(path);
    amxc_var_t* config = amxo_parser_get_config(get_wld_plugin_parser(), "odl.directory");
    const char* saveDir = amxc_var_constcast(cstring_t, config);
    ASSERTS_STR(saveDir, false, ME, "Empty");
    config = amxo_parser_get_config(get_wld_plugin_parser(), "name");
    const char* wldName = amxc_var_constcast(cstring_t, config);
    ASSERTS_STR(wldName, false, ME, "Empty");
    int ret = amxc_string_appendf(path, "%s/%s.odl", saveDir, wldName);
    ASSERT_EQUALS(ret, 0, false, ME, "fail to format save path");
    return true;
}

bool wld_persist_onStart() {
    SAH_TRACEZ_IN(ME);
    // Defaults or saved odl will be loaded here:
    // This makes sure that the events for the defaults are only called when the plugin is started
    while(amxp_signal_read() == 0) {
    }
    int rv;
    amxc_string_t confPath;
    amxc_string_init(&confPath, 0);
    amxd_object_t* rootObj = amxd_dm_get_root(get_wld_plugin_dm());
    amxo_parser_t* parser = get_wld_plugin_parser();
    if(s_getSavedConfPath(&confPath) && swl_fileUtils_existsFile(amxc_string_get(&confPath, 0))) {
        rv = amxo_parser_parse_file(parser, amxc_string_get(&confPath, 0), rootObj);
    } else {
        rv = amxo_parser_parse_string(parser, "include '${odl.dm-defaults}';", rootObj);
        wld_vendorModuleMgr_loadDefaultsAll();
    }
    amxc_string_clean(&confPath);
    while(amxp_signal_read() == 0) {
    }
    if(rv != 0) {
        SAH_TRACEZ_ERROR(ME, "Fail to load conf: (status:%d) (msg:%s)",
                         amxo_parser_get_status(parser),
                         amxo_parser_get_message(parser));
        return false;
    }
    SAH_TRACEZ_OUT(ME);
    return true;
}

bool wld_persist_isRadInitConfigPending(T_Radio* pRad _UNUSED) {
    return false;
}


void wld_persist_onRadioCreation(T_Radio* pRad _UNUSED) {
    // when persistence is on, radios are loaded from the data model.
}


bool wld_persist_writeApAtCreation() {
    return true;
}
