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
#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "swl/fileOps/swl_fileUtils.h"
#include "test-toolbox/ttb_amx.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"

#include "wld.h"
#include "wld_assocdev.h"
#include "wld_util.h"
#include "Plugin/wld_plugin.h"

bool wld_th_dm_init(wld_th_dm_t* dm) {
    dm->ttbAmx = ttb_amx_init();

    amxc_var_t* lib_dirs = amxo_parser_get_config(&dm->ttbAmx->parser, "import-dirs");
    amxc_var_add(cstring_t, lib_dirs, "../src/");
    amxc_var_add(cstring_t, lib_dirs, "../src/Plugin/");

    amxc_var_t* incDirs = amxo_parser_get_config(&dm->ttbAmx->parser, "include-dirs");
    amxc_var_add(cstring_t, incDirs, "../commonData");
    amxc_var_dump(&dm->ttbAmx->parser.config, STDOUT_FILENO);

    setenv("WAN_ADDR", "AA:BB:CC:DD:EE:FF", 0);

    ttb_amx_loadDm(dm->ttbAmx, "../../odl/wld.odl", "WiFi");


    assert_int_equal(_wld_main(AMXO_START, &dm->ttbAmx->dm, &dm->ttbAmx->parser), 0);

    dm->mockVendor = wld_th_mockVendor_create("MockVendor");
    wld_th_mockVendor_register(dm->mockVendor);

    amxc_var_t ret;
    amxc_var_init(&ret);

    assert_int_equal(amxb_get(dm->ttbAmx->bus_ctx, "WiFi.", INT32_MAX, &ret, 5), AMXB_STATUS_OK);
    amxc_var_clean(&ret);

    char* radNames[2] = {"wifi0", "wifi1"};
    char* vapNames[2] = {"wlan0", "wlan1"};
    char* epNames[2] = {"sta0", "sta1"};
    swl_channel_t channels[2] = {1, 36};

    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        wld_th_dmBand_t* band = &dm->bandList[i];
        band->rad = wld_th_radio_create(dm->ttbAmx->bus_ctx, dm->mockVendor, radNames[i]);
        assert_non_null(band->rad);
    }
    //amxd_object_t* root_obj = amxd_dm_get_root(&dm->ttbAmx->dm);

    //amxo_parser_parse_file(&dm->ttbAmx->parser, "../commonData/wld_testDefaults.odl", root_obj);

    ttb_mockTimer_goToFutureMs(1000);

    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        printf("** INIT BAND %s, rad %s, vap %s, sta %s\n", swl_freqBand_str[i],
               radNames[i], vapNames[i], epNames[i]);
        wld_th_dmBand_t* band = &dm->bandList[i];
        assert_true(band->rad->enable);
        assert_int_equal(band->rad->channel, channels[i]);

        band->radObj = band->rad->pBus;
        assert_non_null(band->radObj);

        band->vapPriv = wld_getAccesspointByAlias(vapNames[i]);
        assert_non_null(band->vapPriv);
        assert_true(band->vapPriv->enable);

        band->vapPrivObj = band->vapPriv->pBus;
        assert_non_null(band->vapPrivObj);

        band->ep = wld_getEndpointByAlias(epNames[i]);
        assert_non_null(band->ep);

        assert_int_equal(amxc_llist_size(&band->ep->llProfiles), 1);

        assert_non_null(band->ep->currentProfile);

    }

    return true;
}

void wld_th_dm_destroy(wld_th_dm_t* dm) {

    assert_int_equal(_wld_main(AMXO_STOP, &dm->ttbAmx->dm, &dm->ttbAmx->parser), 0);
    wld_mockVendor_destroy(dm->mockVendor);

    ttb_amx_cleanup(dm->ttbAmx);
}

void wld_th_dm_handleEvents(wld_th_dm_t* dm _UNUSED) {

}
