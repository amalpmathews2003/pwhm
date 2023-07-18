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


#include "wld.h"
#include "wld_util.h"
#include "wld_th_ep.h"
#include "wld_th_mockVendor.h"
#include "test-toolbox/ttb_mockTimer.h"

#define ME "TH_EP"

T_EndPoint* wld_th_ep_createEp(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor _UNUSED, T_Radio* radio, const char* name) {
    assert_non_null(bus_ctx);
    assert_non_null(mockVendor);
    assert_non_null(radio);
    assert_non_null(name);

    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "radio", radio->Name);
    amxc_var_add_key(cstring_t, &args, "endpoint", name);

    assert_int_equal(amxb_call(bus_ctx, "WiFi.", "addEndPointIntf", &args, NULL, 5), AMXB_STATUS_OK);
    amxc_var_clean(&args);

    T_EndPoint* ep = wld_getEndpointByAlias(name);
    return ep;
}

void wld_th_ep_deleteEp(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor _UNUSED, const char* name) {
    assert_non_null(bus_ctx);
    assert_non_null(mockVendor);
    assert_non_null(name);
    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "endpoint", name);

    assert_int_equal(amxb_call(bus_ctx, "WiFi.", "delEndPointIntf", &args, NULL, 5), AMXB_STATUS_OK);
    amxc_var_clean(&args);
}

amxd_object_t* wld_th_ep_createProfile(T_EndPoint* ep, const char* name) {
    assert_non_null(ep);
    assert_non_null(name);
    amxd_object_t* epObj = ep->pBus;
    assert_non_null(epObj);
    amxd_object_t* profilesObj = amxd_object_findf(epObj, "Profile");
    amxd_object_t* obj;
    amxd_object_add_instance(&obj, profilesObj, name, 0, NULL);
    return obj;
}

void wld_th_ep_doFsmClean(T_EndPoint* pEP) {
    assert_non_null(pEP);
    clearAllBitsLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW);
}

void wld_th_ep_setEnable(T_EndPoint* pEP, bool enable, bool commit) {
    assert_non_null(pEP);
    if(commit) {
        swl_typeUInt8_commitObjectParam(pEP->pBus, "Enable", enable);
        ttb_mockTimer_goToFutureMs(10);
    } else {
        swl_typeUInt8_toObjectParam(pEP->pBus, "Enable", enable);
    }
}

swl_rc_ne wld_th_ep_getStats(T_EndPoint* pEP _UNUSED, T_EndPointStats* stats) {
    assert_non_null(pEP);
    stats->txbyte = 12991;
    stats->txPackets = 65;
    stats->assocCaps.currentSecurity = SWL_SECURITY_APMODE_WPA2_P;
    return SWL_RC_OK;
}
