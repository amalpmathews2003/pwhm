/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmocka.h>

#include <debug/sahtrace.h>

#include "wld.h"
#include "wld_hostapd_ap_api.h"
#include "wld_wpaCtrl_events.h"

static void test_wld_ap_hostapd_getParamAction(void** state) {
    (void) state;

    wld_secDmn_action_rc_ne pMappedAction;
    swl_rc_ne ret = wld_ap_hostapd_getParamAction(&pMappedAction, "param_invalid");
    assert_int_equal(SWL_RC_ERROR, ret);

    ret = wld_ap_hostapd_getParamAction(&pMappedAction, "ssid");
    assert_int_equal(SWL_RC_OK, ret);
}

static void test_wld_ap_hostapd_setParamAction(void** state) {
    (void) state;

    swl_rc_ne ret = wld_ap_hostapd_setParamAction("param_invalid", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY);
    assert_int_equal(SWL_RC_ERROR, ret);

    ret = wld_ap_hostapd_setParamAction("ssid", SECDMN_ACTION_OK_NEED_RELOAD_SECKEY);
    assert_int_equal(SWL_RC_OK, ret);

    wld_secDmn_action_rc_ne pMappedAction = SECDMN_ACTION_OK_DONE;
    ret = wld_ap_hostapd_getParamAction(&pMappedAction, "ssid");
    assert_int_equal(SWL_RC_OK, ret);
    assert_int_equal(pMappedAction, SECDMN_ACTION_OK_NEED_RELOAD_SECKEY);

    ret = wld_ap_hostapd_setParamAction("ssid", SECDMN_ACTION_OK_NEED_SIGHUP);
    assert_int_equal(SWL_RC_OK, ret);

    ret = wld_ap_hostapd_getParamAction(&pMappedAction, "ssid");
    assert_int_equal(SWL_RC_OK, ret);
    assert_int_equal(pMappedAction, SECDMN_ACTION_OK_NEED_SIGHUP);
}

static void test_wld_parse_wpactrl_event(void** state) {
    (void) state;
    struct testInfo {
        const char* msgData;
        const char* msgEvtNamePfx;
        const char* msgEvtArgsSep;
        bool expecRes;
        const char* expecEvtName;
        const char* expecEvtArgs;
    } tests[] = {
        {
            "<3>CTRL-EVENT-CONNECTED - Connection to 98:42:65:2d:27:b0 completed [id=0 id_str=]",
            "<3>", " ",
            true, "CTRL-EVENT-CONNECTED", "- Connection to 98:42:65:2d:27:b0 completed [id=0 id_str=]"
        },
        {
            "<3>WDS-STA-INTERFACE-REMOVED ifname=wlan0.sta1 sta_addr=d6:ee:57:0d:f2:aa",
            "<3>", " ",
            true, "WDS-STA-INTERFACE-REMOVED", "ifname=wlan0.sta1 sta_addr=d6:ee:57:0d:f2:aa"
        },
        {
            "<3>AP-ENABLED",
            "<3>", " ",
            true, "AP-ENABLED", NULL
        },
        {
            "zzzzzzzz<3>AP-ENABLED",
            "<3>", NULL,
            true, "AP-ENABLED", NULL
        },
        {
            "<3>AP-MGMT-FRAME-RECEIVED buf=b0003c0000000000001012d4159a59e7000000000010a082000001000000",
            "<3>", NULL,
            true, "AP-MGMT-FRAME-RECEIVED buf=b0003c0000000000001012d4159a59e7000000000010a082000001000000", NULL
        },
        {
            "CTRL-EVENT-STARTED-CHANNEL-SWITCH freq=5260 ht_enabled=1 ch_offset=1 ch_width=80 MHz cf1=5290 cf2=0 dfs=1",
            NULL, " ",
            true, "CTRL-EVENT-STARTED-CHANNEL-SWITCH", "freq=5260 ht_enabled=1 ch_offset=1 ch_width=80 MHz cf1=5290 cf2=0 dfs=1"
        },
        {
            "CTRL-EVENT-STARTED-CHANNEL-SWITCH freq=5260 ht_enabled=1 ch_offset=1 ch_width=80 MHz cf1=5290 cf2=0 dfs=1",
            "<3>", " ",
            false, NULL, NULL
        },
        {
            "<3>",
            "<3>", " ",
            false, NULL, NULL
        },
        {
            NULL,
            "<3>", " ",
            false, NULL, NULL
        },
    };

    char* eventName = NULL;
    char* pParams = NULL;
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        bool ret = wld_wpaCtrl_parseMsg(tests[i].msgData, tests[i].msgEvtNamePfx, tests[i].msgEvtArgsSep, &eventName, &pParams);
        assert_int_equal(ret, tests[i].expecRes);
        assert_true(swl_str_matches(eventName, tests[i].expecEvtName));
        assert_true(swl_str_matches(pParams, tests[i].expecEvtArgs));
    }

    W_SWL_FREE(eventName);
    W_SWL_FREE(pParams);
}

static void test_wld_fetch_wpactrl_event(void** state) {
    (void) state;
    char* evtList[] = {
        "SME: Trying to authenticate",
        "Trying to associate",
        "Associated"
    };
    struct testInfo {
        const char* msgData;
        const char* msgEvtNamePfx;
        const char* msgEvtArgsSep;
        int32_t expectPos;
        const char* expecEvtName;
        const char* expecEvtArgs;
    } tests[] = {
        {
            "<3>SME: Trying to authenticate with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)",
            "<3>", " ",
            0, "SME: Trying to authenticate", "with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)"
        },
        {
            "<3>Trying to associate with SSID 'ssid'",
            "<3>", " ",
            1, "Trying to associate", "with SSID 'ssid'"
        },
        {
            "<3>Trying to associate with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)",
            "<3>", " ",
            1, "Trying to associate", "with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)"
        },
        {
            "<3>Trying to connect to 98:42:65:2d:23:43",
            "<3>", NULL,
            -1, "Trying to connect to 98:42:65:2d:23:43", NULL
        },
        {
            "SME: Trying to connect",
            NULL, " ",
            -1, "SME:", "Trying to connect"
        },
        {
            "Associated to 98:42:65:2d:23:43",
            NULL, " ",
            2, "Associated", "to 98:42:65:2d:23:43"
        },
        {
            "SME:",
            "<3>", " ",
            -1, NULL, NULL
        },
        {
            "<3>",
            "<3>", " ",
            -1, NULL, NULL
        },
        {
            NULL,
            "<3>", " ",
            -1, NULL, NULL
        },
    };

    char* eventName = NULL;
    char* pParams = NULL;
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        int ret = wld_wpaCtrl_fetchEvent(tests[i].msgData, tests[i].msgEvtNamePfx, tests[i].msgEvtArgsSep, evtList, SWL_ARRAY_SIZE(evtList), &eventName, &pParams);
        assert_int_equal(ret, tests[i].expectPos);
        assert_true(swl_str_matches(eventName, tests[i].expecEvtName));
        assert_true(swl_str_matches(pParams, tests[i].expecEvtArgs));
    }

    W_SWL_FREE(eventName);
    W_SWL_FREE(pParams);
}

static int s_setupSuite(void** state) {
    (void) state;
    return 0;
}

static int s_teardownSuite(void** state) {
    (void) state;
    return 0;
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_WARNING);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);
    sahTraceAddZone(sahTraceLevel(), "hapdAP");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_ap_hostapd_getParamAction),
        cmocka_unit_test(test_wld_ap_hostapd_setParamAction),
        cmocka_unit_test(test_wld_parse_wpactrl_event),
        cmocka_unit_test(test_wld_fetch_wpactrl_event),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
