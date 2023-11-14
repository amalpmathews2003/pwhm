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
#include "wld_ssid.h"
#include "wld_eventing.h"
#include "swla/swla_lib.h"
#include "swl/fileOps/swl_fileUtils.h"
#include "test-toolbox/ttb_amx.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "test-toolbox/ttb_mockClock.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"

#include "wld.h"
#include "wld_assocdev.h"
#include "wld_util.h"
#include "Plugin/wld_plugin.h"


bool wld_th_dm_initFreq(wld_th_dm_t* dm, swl_freqBand_m initMask _UNUSED) {
    assert_non_null(dm);
    return wld_th_dm_init(dm);
}

bool wld_th_dm_init(wld_th_dm_t* dm) {
    assert_non_null(dm);
    ttb_mockClock_initDefault();
    dm->ttbBus = ttb_amx_init();

    amxc_var_t* lib_dirs = amxo_parser_get_config(&dm->ttbBus->parser, "import-dirs");
    amxc_var_add(cstring_t, lib_dirs, "../../src/");
    amxc_var_add(cstring_t, lib_dirs, "../../src/Plugin/");

    amxc_var_t* incDirs = amxo_parser_get_config(&dm->ttbBus->parser, "include-dirs");
    amxc_var_add(cstring_t, incDirs, "../commonData");
    amxc_var_add(cstring_t, incDirs, "../../odl/");
    amxc_var_dump(&dm->ttbBus->parser.config, STDOUT_FILENO);

    setenv("WAN_ADDR", "AA:BB:CC:DD:EE:FF", 0);


    ttb_amx_loadDm(dm->ttbBus, "../../test/commonData/wld.odl", "WiFi");
    //init swla dm bus ctx, first time, before wld_main, for any early dm action
    swl_lib_initialize(dm->ttbBus->bus_ctx);

    assert_int_equal(_wld_main(AMXO_START, &dm->ttbBus->dm, &dm->ttbBus->parser), 0);

    //re-init swla dm bus ctx, second time, after wld_main, to restore ttbBus emulated connection
    swl_lib_initialize(dm->ttbBus->bus_ctx);

    dm->mockVendor = wld_th_mockVendor_create("MockVendor");
    wld_th_mockVendor_register(dm->mockVendor);

    amxc_var_t ret;
    amxc_var_init(&ret);

    assert_int_equal(amxb_get(dm->ttbBus->bus_ctx, "WiFi.", INT32_MAX, &ret, 5), AMXB_STATUS_OK);
    amxc_var_clean(&ret);

    char* radNames[3] = {"wifi0", "wifi1", "wifi2"};
    char* vapNames[3] = {"wlan0", "wlan1", "wlan2"};
    char* epNames[3] = {"sta0", "sta1", "sta2"};

    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        wld_th_dmBand_t* band = &dm->bandList[i];
        band->rad = wld_th_radio_create(dm->ttbBus->bus_ctx, dm->mockVendor, radNames[i]);
        assert_non_null(band->rad);
    }

    swl_timeMono_t createTime = swl_time_getMonoSec();

    amxp_sigmngr_trigger_signal(&dm->ttbBus->dm.sigmngr, "app:start", NULL);
    ttb_mockTimer_goToFutureMs(10000);

    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        printf("** INIT BAND %s, rad %s, vap %s, sta %s\n", swl_freqBand_str[i],
               radNames[i], vapNames[i], epNames[i]);
        wld_th_dmBand_t* band = &dm->bandList[i];
        assert_true(band->rad->enable);
        assert_int_equal(band->rad->channel, swl_channel_defaults[i]);


        band->radObj = band->rad->pBus;
        assert_non_null(band->radObj);

        band->vapPriv = wld_getAccesspointByAlias(vapNames[i]);
        assert_non_null(band->vapPriv);
        assert_true(band->vapPriv->enable);

        band->vapPrivObj = band->vapPriv->pBus;
        assert_non_null(band->vapPrivObj);

        band->vapPrivSSID = band->vapPriv->pSSID;
        band->vapPrivSSIDObj = band->vapPrivSSID->pBus;
        //assert_true(band->vapPrivSSID->enable);

        band->ep = wld_getEndpointByAlias(epNames[i]);


        band->radCreateTime = createTime;
        band->epCreateTime = createTime;
        band->vapCreateTime = createTime;

        assert_non_null(band->ep);

        assert_int_equal(amxc_llist_size(&band->ep->llProfiles), 2);

        assert_non_null(band->ep->currentProfile);

    }

    return true;
}

void wld_th_dm_destroy(wld_th_dm_t* dm) {
    assert_non_null(dm);
    assert_int_equal(_wld_main(AMXO_STOP, &dm->ttbBus->dm, &dm->ttbBus->parser), 0);
    wld_mockVendor_destroy(dm->mockVendor);

    swl_lib_cleanup();

    ttb_amx_cleanup(dm->ttbBus);
}

void wld_th_dm_handleEvents(wld_th_dm_t* dm _UNUSED) {
    assert_non_null(dm);
}
