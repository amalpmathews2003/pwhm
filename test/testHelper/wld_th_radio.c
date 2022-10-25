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

#include "wld_th_radio.h"
#include "wld.h"
#include "wld_th_mockVendor.h"
#include "wld_radio.h"
#include "swl/swl_common_chanspec.h"

static int s_getNextIndex() {
    // this is basically a hidden global, so not very pretty, but ok...
    static int nextIndex = 0;
    int lastNextIndex = nextIndex;
    nextIndex++;
    return lastNextIndex;
}

/** Implements #PFN_WRAD_ADDENDPOINTIF */
int wld_th_radio_vendorCb_addEndpointIf(T_Radio* rad _UNUSED, char* endpoint _UNUSED, int bufsize _UNUSED) {
    return 0;
}

/** Implements #PFN_WRAD_DELENDPOINTIF */
int wld_th_radio_vendorCb_delEndpointIf(T_Radio* radio _UNUSED, char* endpoint _UNUSED) {
    return 0;
}

/** Implements #PFN_WRAD_ADDVAPIF */
int wld_th_radio_vendorCb_addVapIf(T_Radio* rad _UNUSED, char* vap _UNUSED, int bufsize _UNUSED) {
    return 0;
}

swl_channel_t possibleChannels2[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
swl_channel_t possibleChannels5[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128};


void s_readChanInfo(T_Radio* pRad) {

    for(int i = 0; i < pRad->nrPossibleChannels; i++) {
        swl_chanspec_t cs = SWL_CHANSPEC_NEW(pRad->possibleChannels[i], pRad->operatingChannelBandwidth, pRad->operatingFrequencyBand);
        wld_channel_mark_available_channel(cs);
        if((pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) && wld_channel_is_dfs(possibleChannels5[i])) {
            wld_channel_mark_radar_req_channel(cs);
            wld_channel_mark_passive_channel(cs);
        }
    }
}

/** Implements #PFN_WRAD_SUPPORTS */
int wld_th_radio_vendorCb_supports(T_Radio* rad, char* buf _UNUSED, int bufsize _UNUSED) {
    rad->driverCfg.skipSocketIO = true;

    // In wld's radio cration, this callback is called, after which it syncs to PCB,
    // after which it commits the PCB object, which fails if the band is invalid.
    // So write the band here, so it's synced, so the commit succeeds.
    printf("SUPP TEST -%s-\n", rad->Name);
    if(swl_str_matches(rad->Name, "wifi1")) {
        rad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_5GHZ;
        rad->supportedFrequencyBands = M_SWL_FREQ_BAND_5GHZ;
        memcpy(rad->possibleChannels, possibleChannels5, SWL_ARRAY_SIZE(possibleChannels5));
        rad->nrPossibleChannels = SWL_ARRAY_SIZE(possibleChannels5);
        rad->maxChannelBandwidth = SWL_BW_160MHZ;
        rad->operatingChannelBandwidth = SWL_BW_80MHZ;
        rad->channel = 36;
        rad->supportedStandards = M_SWL_RADSTD_A | M_SWL_RADSTD_N | M_SWL_RADSTD_AC | M_SWL_RADSTD_AX;
    } else {
        rad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_2_4GHZ;
        rad->supportedFrequencyBands = M_SWL_FREQ_BAND_2_4GHZ;
        memcpy(rad->possibleChannels, possibleChannels2, SWL_ARRAY_SIZE(possibleChannels2));
        rad->nrPossibleChannels = SWL_ARRAY_SIZE(possibleChannels2);
        rad->maxChannelBandwidth = SWL_BW_40MHZ;
        rad->operatingChannelBandwidth = SWL_BW_20MHZ;
        rad->channel = 1;
        rad->supportedStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G | M_SWL_RADSTD_N | M_SWL_RADSTD_AX;
    }

    wld_channel_init_channels(rad);
    s_readChanInfo(rad);
    wld_rad_write_possible_channels(rad);

    printf("%s: run supports\n", rad->Name);

    return 0;
}

T_Radio* wld_th_radio_create(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor, const char* name) {

    int idx = s_getNextIndex();

    bool ok = ( wld_addRadio(name, wld_th_mockVendor_vendor(mockVendor), idx) >= 0);
    assert_true(ok);

    T_Radio* radio = wld_rad_get_radio(name);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "WiFi.Radio.wifi%d", idx);

    // wld itself chokes when the pcb object does not exist, so fail fast if that happens.
    amxc_var_t ret;
    amxc_var_init(&ret);

    assert_int_equal(amxb_get(bus_ctx, buffer, INT32_MAX, &ret, 5), AMXB_STATUS_OK);
    amxc_var_clean(&ret);

    return radio;
}

int wld_th_wrad_fsm(T_Radio* rad) {
    printf("%s: do commit\n", rad->Name);
    return 0;
}

int wld_th_rad_enable(T_Radio* rad, int val, int set) {
    int ret;
    printf("RAD: %d --> %d -  %d", rad->enable, val, set);
    if(set & SET) {
        rad->enable = val;
        ret = val;
    } else {
        ret = rad->enable;
    }
    return ret;
}


