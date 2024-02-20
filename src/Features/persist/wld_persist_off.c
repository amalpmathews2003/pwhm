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
#include "swl/swl_returnCode.h"
#include "wld_dm_trans.h"

#define ME "wldPst"

bool wld_persist_onStart() {
    SAH_TRACEZ_IN(ME);
    // Defaults or saved odl will be loaded here:
    // This makes sure that the events for the defaults are only called when the plugin is started
    while(amxp_signal_read() == 0) {
    }
    int rv;

    amxd_object_t* rootObj = amxd_dm_get_root(get_wld_plugin_dm());
    amxo_parser_t* parser = get_wld_plugin_parser();

    rv = amxo_parser_parse_string(parser, "include '${odl.dm-defaults}';", rootObj);
    wld_vendorModuleMgr_loadDefaultsAll();

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


void wld_persist_onRadioCreation(T_Radio* pR) {
    amxd_object_t* templateObject = amxd_object_get(get_wld_object(), "Radio");

    if(!templateObject) {
        SAH_TRACEZ_ERROR(ME, "%s: Could not get template: %p", pR->Name, templateObject);
        return;
    }

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(templateObject, &trans, , ME, "%s : trans init failure", pR->Name);

    amxd_trans_add_inst(&trans, pR->index, pR->instanceName);
    SAH_TRACEZ_INFO(ME, "Add radio %s", pR->instanceName);
    amxd_trans_set_value(cstring_t, &trans, "OperatingFrequencyBand",
                         Rad_SupFreqBands[pR->operatingFrequencyBand]);
    amxd_trans_set_value(cstring_t, &trans, "Alias", pR->instanceName);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pR->Name);

    pR->pBus = amxd_object_get_instance(templateObject, NULL, pR->index);
    pR->pBus->priv = pR;
}

bool wld_persist_writeApAtCreation() {
    return false;
}
