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
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <string.h>

#include <amxc/amxc_macros.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object_event.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#include <amxb/amxb.h>


#include <amxb/amxb_register.h>

#include "wld.h"
#include "wld_assocdev.h"
#include "wld_util.h"

#include "../mocks/dummy_be.h"
#include "../mocks/test_common.h"
#include "../mocks/wld_th_mockVendor.h"
#include "../mocks/wld_th_radio.h"
#include "../mocks/wld_th_vap.h"

#include "Plugin/wld_plugin.h"

static amxo_parser_t parser;
static amxd_dm_t dm;
static amxb_bus_ctx_t* bus_ctx = NULL;

static wld_th_mockVendor_t* mockVendor;


static T_AccessPoint* s_vapList[2];

static amxd_status_t func(_UNUSED amxd_object_t* obj,
                          _UNUSED amxd_function_t* func,
                          _UNUSED amxc_var_t* args,
                          _UNUSED amxc_var_t* ret) {
    char* obj_path = amxd_object_get_path(obj, AMXD_OBJECT_TERMINATE);
    printf("Func called on %s\n", obj_path);
    free(obj_path);
    return amxd_status_ok;
}

int test_dm_proxy_setup(void** state _UNUSED) {
    test_common_setup();
    amxd_object_t* root_obj = NULL;

    amxo_parser_init(&parser);
    amxd_dm_init(&dm);

    amxc_var_t* lib_dirs = amxo_parser_get_config(&parser, "import-dirs");
    amxc_var_add(cstring_t, lib_dirs, "../src/");
    amxc_var_add(cstring_t, lib_dirs, "../src/Plugin/");
    amxc_var_dump(&parser.config, STDOUT_FILENO);

    setenv("WAN_ADDR", "AA:BB:CC:DD:EE:FF", 0);

    sahTraceOpen("testApp", TRACE_TYPE_STDOUT);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }

    sahTraceSetLevel(TRACE_LEVEL_NOTICE);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);

    amxo_resolver_ftab_add(&parser, "func", AMXO_FUNC(func));

    root_obj = amxd_dm_get_root(&dm);
    amxo_parser_parse_file(&parser, "../../odl/wld.odl", root_obj);

    assert_int_equal(test_register_dummy_be(), 0);
    assert_int_equal(amxb_connect(&bus_ctx, "dummy:/tmp/dummy.sock"), 0);
    amxb_register(bus_ctx, &dm);
    amxb_set_access(bus_ctx, amxd_dm_access_public);

    handle_events();

    assert_int_equal(_wld_main(AMXO_START, &dm, &parser), 0);

    mockVendor = wld_th_mockVendor_create("MockVendor");
    wld_th_mockVendor_register(mockVendor);

    amxc_var_t ret;
    amxc_var_init(&ret);

    assert_int_equal(amxb_get(bus_ctx, "WiFi.", INT32_MAX, &ret, 5), AMXB_STATUS_OK);
    amxc_var_clean(&ret);

    T_Radio* rad2 = wld_th_radio_create(bus_ctx, mockVendor, "wifi0");
    assert_non_null(rad2);
    T_Radio* rad5 = wld_th_radio_create(bus_ctx, mockVendor, "wifi1");
    assert_non_null(rad5);
    amxo_parser_parse_file(&parser, "../../examples/defaults/wld_defaults.odl", root_obj);

    s_vapList[0] = wld_getAccesspointByAlias("wlan0");
    assert_non_null(s_vapList[0]);
    s_vapList[1] = wld_getAccesspointByAlias("wlan1");
    assert_non_null(s_vapList[1]);

    return 0;
}

int test_dm_proxy_teardown(void** state _UNUSED) {
    assert_int_equal(_wld_main(AMXO_STOP, &dm, &parser), 0);

    amxd_dm_remove_root_object(&dm, "WiFi");
    handle_events();

    amxo_parser_clean(&parser);
    amxd_dm_clean(&dm);

    handle_events();

    test_unregister_dummy_be();

    wld_mockVendor_destroy(mockVendor);

    sahTraceClose();
    test_common_teardown();
    return 0;
}

void s_checkDevEntries(T_AccessPoint* pAP, uint32_t nrAssoc, uint32_t nrActive) {
    amxc_var_t retval;
    amxc_var_init(&retval);

    assert_int_equal(amxd_object_get_param(pAP->pBus, "AssociatedDeviceNumberOfEntries", &retval), 0);
    uint32_t dmNrAssoc = amxc_var_get_const_uint32_t(&retval);
    assert_int_equal(dmNrAssoc, nrAssoc);
    assert_int_equal(pAP->AssociatedDeviceNumberOfEntries, nrAssoc);

    assert_int_equal(amxd_object_get_param(pAP->pBus, "ActiveAssociatedDeviceNumberOfEntries", &retval), 0);

    uint32_t dmNrActive = amxc_var_get_const_uint32_t(&retval);
    assert_int_equal(dmNrActive, nrActive);
    assert_int_equal(pAP->ActiveAssociatedDeviceNumberOfEntries, nrActive);

    amxc_var_clean(&retval);
}

void test_dm_proxy_get(_UNUSED void** state) {

#define NR_TEST_DEV 5

    T_AssociatedDevice* devList[NR_TEST_DEV];

    for(int i = 0; i < NR_TEST_DEV; i++) {
        char devName[18];
        snprintf(devName, sizeof(devName), "AA:BB:CC:DD:EE:%02x", i);
        swl_macBin_t binAddr;
        swl_mac_charToBin(&binAddr, (swl_macChar_t*) devName);

        s_checkDevEntries(s_vapList[0], i, i);

        devList[i] = wld_ad_create_associatedDevice(s_vapList[0], &binAddr);
        wld_ad_add_connection_try(s_vapList[0], devList[i]);

        s_checkDevEntries(s_vapList[0], i + 1, i + 1);

        wld_ad_add_connection_success(s_vapList[0], devList[i]);
    }

    for(int i = 0; i < NR_TEST_DEV; i++) {


        s_checkDevEntries(s_vapList[0], SWL_MIN(NR_TEST_DEV, NR_TEST_DEV - i + 1), (NR_TEST_DEV - i));

        wld_ad_add_disconnection(s_vapList[0], devList[i]);

        s_checkDevEntries(s_vapList[0], NR_TEST_DEV - i, SWL_MAX(0, NR_TEST_DEV - i - 1));

    }

}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dm_proxy_get),
    };
    int rv = cmocka_run_group_tests(tests, test_dm_proxy_setup, test_dm_proxy_teardown);
    return rv;
}



