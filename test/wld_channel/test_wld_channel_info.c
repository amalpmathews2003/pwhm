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

#include "wld_channel.h"


static void test_wld_channel_is_dfs(void** state) {
    (void) state;
    assert_false(wld_channel_is_dfs(36));
    assert_true(wld_channel_is_dfs(52));
    assert_true(wld_channel_is_dfs(100));
    assert_false(wld_channel_is_dfs(149));

    assert_false(wld_channel_is_dfs(1));
    assert_false(wld_channel_is_dfs(6));
    assert_false(wld_channel_is_dfs(11));
}

static void test_wld_channel_is_dfs_band(void** state) {
    (void) state;
    assert_false(wld_channel_is_dfs_band(36, SWL_BW_80MHZ));
    assert_true(wld_channel_is_dfs_band(36, SWL_BW_160MHZ));
    assert_true(wld_channel_is_dfs_band(52, SWL_BW_80MHZ));
    assert_true(wld_channel_is_dfs_band(100, SWL_BW_80MHZ));
    assert_true(wld_channel_is_dfs_band(100, SWL_BW_160MHZ));

    assert_false(wld_channel_is_dfs_band(1, SWL_BW_20MHZ));
    assert_false(wld_channel_is_dfs_band(6, SWL_BW_20MHZ));
    assert_false(wld_channel_is_dfs_band(11, SWL_BW_20MHZ));
}

static void test_wld_channel_getDfsPercentage(void** state) {
    (void) state;

    assert_int_equal(wld_channel_getDfsPercentage(36, SWL_BW_80MHZ), 0);
    assert_int_equal(wld_channel_getDfsPercentage(36, SWL_BW_160MHZ), 50);
    assert_int_equal(wld_channel_getDfsPercentage(52, SWL_BW_80MHZ), 100);
    assert_int_equal(wld_channel_getDfsPercentage(52, SWL_BW_160MHZ), 50);
    assert_int_equal(wld_channel_getDfsPercentage(100, SWL_BW_80MHZ), 100);
    assert_int_equal(wld_channel_getDfsPercentage(100, SWL_BW_160MHZ), 100);
    assert_int_equal(wld_channel_getDfsPercentage(116, SWL_BW_80MHZ), 100);
    assert_int_equal(wld_channel_getDfsPercentage(116, SWL_BW_160MHZ), 100);

    assert_int_equal(wld_channel_is_dfs_band(1, SWL_BW_20MHZ), 0);
    assert_int_equal(wld_channel_is_dfs_band(6, SWL_BW_20MHZ), 0);
    assert_int_equal(wld_channel_is_dfs_band(11, SWL_BW_20MHZ), 0);
}

static void test_wld_channel_is_hp_dfs(void** state) {
    (void) state;
    assert_false(wld_channel_is_hp_dfs(1));
    assert_false(wld_channel_is_hp_dfs(6));
    assert_false(wld_channel_is_hp_dfs(11));

    assert_false(wld_channel_is_hp_dfs(36));
    assert_false(wld_channel_is_hp_dfs(52));
    assert_true(wld_channel_is_hp_dfs(100));
    assert_false(wld_channel_is_hp_dfs(149));
}

static void test_wld_channel_is_5ghz(void** state) {
    (void) state;

    assert_false(wld_channel_is_5ghz(1));
    assert_false(wld_channel_is_5ghz(6));
    assert_false(wld_channel_is_5ghz(11));

    assert_true(wld_channel_is_5ghz(36));
    assert_true(wld_channel_is_5ghz(52));
    assert_true(wld_channel_is_5ghz(100));
    assert_true(wld_channel_is_5ghz(149));
}

static void test_wld_get_nr_channels_in_band(void** state) {
    (void) state;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(1, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_2_4GHZ);
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.channel = 11;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.channel = 1;
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.channel = 11;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.channel = 1;
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 5);
    chanspec.channel = 11;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 5);

    chanspec.channel = 36;
    chanspec.band = SWL_FREQ_BAND_EXT_5GHZ;
    chanspec.bandwidth = SWL_BW_AUTO;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 2);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 4);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 8);

    chanspec.channel = 100;
    chanspec.band = SWL_FREQ_BAND_EXT_5GHZ;
    chanspec.bandwidth = SWL_BW_AUTO;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 1);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 2);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 4);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_get_nr_channels_in_band(chanspec), 8);
}

static void test_wld_channel_get_base_channel(void** state) {
    (void) state;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(64, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ);
    assert_int_equal(wld_channel_get_base_channel(chanspec), 64);
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_channel_get_base_channel(chanspec), 64);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_channel_get_base_channel(chanspec), 60);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_channel_get_base_channel(chanspec), 52);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_channel_get_base_channel(chanspec), 36);
}

static void test_wld_channel_get_center_channel(void** state) {
    (void) state;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(36, SWL_BW_AUTO, SWL_FREQ_BAND_EXT_5GHZ);
    assert_int_equal(wld_channel_get_center_channel(chanspec), 36);
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 36);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 38);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 42);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 50);

    chanspec.channel = 100;
    chanspec.bandwidth = SWL_BW_AUTO;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 100);
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 100);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 102);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 106);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_channel_get_center_channel(chanspec), 114);
}

static void test_wld_channel_is_long_wait_band(void** state) {
    (void) state;

    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(36, SWL_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.channel = 52;
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.channel = 100;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));

    chanspec.channel = 116;
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));


    chanspec.channel = 128;
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_true(wld_channel_is_long_wait_band(chanspec));

    chanspec.channel = 1;
    chanspec.bandwidth = SWL_BW_20MHZ;
    chanspec.band = SWL_FREQ_BAND_EXT_2_4GHZ;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.channel = 6;
    assert_false(wld_channel_is_long_wait_band(chanspec));
    chanspec.channel = 11;
    assert_false(wld_channel_is_long_wait_band(chanspec));
}

static void test_wld_channel_is_chan_in_band(void** state) {
    (void) state;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(36, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    assert_true(wld_channel_is_chan_in_band(chanspec, 36));
    assert_false(wld_channel_is_chan_in_band(chanspec, 40));
    assert_false(wld_channel_is_chan_in_band(chanspec, 44));
    assert_false(wld_channel_is_chan_in_band(chanspec, 52));
    assert_false(wld_channel_is_chan_in_band(chanspec, 100));

    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_is_chan_in_band(chanspec, 36));
    assert_true(wld_channel_is_chan_in_band(chanspec, 40));
    assert_false(wld_channel_is_chan_in_band(chanspec, 44));
    assert_false(wld_channel_is_chan_in_band(chanspec, 52));
    assert_false(wld_channel_is_chan_in_band(chanspec, 100));

    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_true(wld_channel_is_chan_in_band(chanspec, 36));
    assert_true(wld_channel_is_chan_in_band(chanspec, 40));
    assert_true(wld_channel_is_chan_in_band(chanspec, 44));
    assert_false(wld_channel_is_chan_in_band(chanspec, 52));
    assert_false(wld_channel_is_chan_in_band(chanspec, 100));

    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_true(wld_channel_is_chan_in_band(chanspec, 36));
    assert_true(wld_channel_is_chan_in_band(chanspec, 40));
    assert_true(wld_channel_is_chan_in_band(chanspec, 44));
    assert_true(wld_channel_is_chan_in_band(chanspec, 52));
    assert_false(wld_channel_is_chan_in_band(chanspec, 100));
}

static void test_wld_channel_getComplementaryBaseChannel(void** state) {
    (void) state;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(36, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), -1);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 40);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 44);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 52);

    chanspec.channel = 100;
    chanspec.bandwidth = SWL_BW_20MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), -1);
    chanspec.bandwidth = SWL_BW_40MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 104);
    chanspec.bandwidth = SWL_BW_80MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 108);
    chanspec.bandwidth = SWL_BW_160MHZ;
    assert_int_equal(wld_channel_getComplementaryBaseChannel(chanspec), 116);
}

static void test_wld_channel_get_interference_list(void** state) {
    (void) state;

    wld_chan_interference_t interferenceList[WLD_CHAN_MAX_NR_INTERF_CHAN];
    memset(interferenceList, 0, sizeof(interferenceList));
    int list_size = WLD_CHAN_MAX_NR_INTERF_CHAN;

    // chan: 1 bw: 20 fq: 2.4
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);
    wld_chan_interference_t chan_1_bw_20_2g[NR_INTERFERENCE_ELEMENT_24_20] = {
        {.channel = 1, .interference_level = 1},
        {.channel = 2, .interference_level = 2},
        {.channel = 3, .interference_level = 3},
        {.channel = 4, .interference_level = 1},
    };
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_1_bw_20_2g, interferenceList, SWL_ARRAY_SIZE(chan_1_bw_20_2g));

    // chan: 7 bw: 20 fq: 2.4
    chanspec.channel = 7;
    wld_chan_interference_t chan_7_bw_20_2g[NR_INTERFERENCE_ELEMENT_24_20] = {
        {.channel = 4, .interference_level = 1},
        {.channel = 5, .interference_level = 3},
        {.channel = 6, .interference_level = 2},
        {.channel = 7, .interference_level = 1},
        {.channel = 8, .interference_level = 2},
        {.channel = 9, .interference_level = 3},
        {.channel = 10, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_7_bw_20_2g, interferenceList, SWL_ARRAY_SIZE(chan_7_bw_20_2g));

    // chan: 11 bw: 20 fq: 2.4
    chanspec.channel = 11;
    wld_chan_interference_t chan_11_bw_20_2g[NR_INTERFERENCE_ELEMENT_24_20] = {
        {.channel = 8, .interference_level = 1},
        {.channel = 9, .interference_level = 3},
        {.channel = 10, .interference_level = 2},
        {.channel = 11, .interference_level = 1},
        {.channel = 12, .interference_level = 2},
        {.channel = 13, .interference_level = 3},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_11_bw_20_2g, interferenceList, SWL_ARRAY_SIZE(chan_11_bw_20_2g));

    // chan: 1 bw: 40 fq: 2.4
    chanspec.channel = 1;
    chanspec.bandwidth = SWL_BW_40MHZ;
    wld_chan_interference_t chan_1_bw_40_2g[] = {
        {.channel = 1, .interference_level = 4},
        {.channel = 2, .interference_level = 4},
        {.channel = 3, .interference_level = 4},
        {.channel = 4, .interference_level = 4},
        {.channel = 5, .interference_level = 4},
        {.channel = 6, .interference_level = 3},
        {.channel = 7, .interference_level = 2},
        {.channel = 8, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_1_bw_40_2g, interferenceList, SWL_ARRAY_SIZE(chan_1_bw_40_2g));

    // chan: 5 bw: 40 fq: 2.4
    chanspec.channel = 5;
    wld_chan_interference_t chan_5_bw_40_2g[] = {
        {.channel = 2, .interference_level = 1},
        {.channel = 3, .interference_level = 2},
        {.channel = 4, .interference_level = 3},
        {.channel = 5, .interference_level = 4},
        {.channel = 6, .interference_level = 4},
        {.channel = 7, .interference_level = 4},
        {.channel = 8, .interference_level = 4},
        {.channel = 9, .interference_level = 4},
        {.channel = 10, .interference_level = 3},
        {.channel = 11, .interference_level = 2},
        {.channel = 12, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_5_bw_40_2g, interferenceList, SWL_ARRAY_SIZE(chan_5_bw_40_2g));

    // chan: 13 bw: 40 fq: 2.4
    chanspec.channel = 13;
    wld_chan_interference_t chan_13_bw_40_2g[] = {
        {.channel = 6, .interference_level = 1},
        {.channel = 7, .interference_level = 2},
        {.channel = 8, .interference_level = 3},
        {.channel = 9, .interference_level = 4},
        {.channel = 10, .interference_level = 4},
        {.channel = 11, .interference_level = 4},
        {.channel = 12, .interference_level = 4},
        {.channel = 13, .interference_level = 4},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_13_bw_40_2g, interferenceList, SWL_ARRAY_SIZE(chan_13_bw_40_2g));

    // chan: 36 bw: 20 fq: 5
    chanspec.channel = 36;
    chanspec.bandwidth = SWL_BW_20MHZ;
    chanspec.band = SWL_FREQ_BAND_EXT_5GHZ;
    wld_chan_interference_t chan_36_bw_20_5g[] = {
        {.channel = 36, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_36_bw_20_5g, interferenceList, SWL_ARRAY_SIZE(chan_36_bw_20_5g));

    // chan: 44 bw: 40 fq: 5
    chanspec.channel = 44;
    chanspec.bandwidth = SWL_BW_40MHZ;
    wld_chan_interference_t chan_44_bw_40_5g[] = {
        {.channel = 44, .interference_level = 1},
        {.channel = 48, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_44_bw_40_5g, interferenceList, SWL_ARRAY_SIZE(chan_44_bw_40_5g));

    // chan: 112 bw: 80 fq: 5
    chanspec.channel = 112;
    chanspec.bandwidth = SWL_BW_80MHZ;
    wld_chan_interference_t chan_112_bw_80_5g[] = {
        {.channel = 100, .interference_level = 1},
        {.channel = 104, .interference_level = 1},
        {.channel = 108, .interference_level = 1},
        {.channel = 112, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_112_bw_80_5g, interferenceList, SWL_ARRAY_SIZE(chan_112_bw_80_5g));

    // chan: 52 bw: 160 fq: 5
    chanspec.channel = 52;
    chanspec.bandwidth = SWL_BW_160MHZ;
    wld_chan_interference_t chan_52_bw_160_5g[] = {
        {.channel = 36, .interference_level = 1},
        {.channel = 40, .interference_level = 1},
        {.channel = 44, .interference_level = 1},
        {.channel = 48, .interference_level = 1},
        {.channel = 52, .interference_level = 1},
        {.channel = 56, .interference_level = 1},
        {.channel = 60, .interference_level = 1},
        {.channel = 64, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_52_bw_160_5g, interferenceList, SWL_ARRAY_SIZE(chan_52_bw_160_5g));

    // chan: 5 bw: 20 fq: 6
    chanspec.channel = 5;
    chanspec.bandwidth = SWL_BW_20MHZ;
    chanspec.band = SWL_FREQ_BAND_EXT_6GHZ;
    wld_chan_interference_t chan_5_bw_20_6g[] = {
        {.channel = 5, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_5_bw_20_6g, interferenceList, SWL_ARRAY_SIZE(chan_5_bw_20_6g));

    // chan: 5 bw: 40 fq: 6
    chanspec.bandwidth = SWL_BW_40MHZ;
    wld_chan_interference_t chan_5_bw_40_6g[] = {
        {.channel = 1, .interference_level = 1},
        {.channel = 5, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_5_bw_40_6g, interferenceList, SWL_ARRAY_SIZE(chan_5_bw_40_6g));

    // chan: 37 bw: 80 fq: 6
    chanspec.channel = 37;
    chanspec.bandwidth = SWL_BW_80MHZ;
    wld_chan_interference_t chan_37_bw_80_6g[] = {
        {.channel = 33, .interference_level = 1},
        {.channel = 37, .interference_level = 1},
        {.channel = 41, .interference_level = 1},
        {.channel = 45, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_37_bw_80_6g, interferenceList, SWL_ARRAY_SIZE(chan_37_bw_80_6g));

    // chan: 21 bw: 160 fq: 6
    chanspec.channel = 21;
    chanspec.bandwidth = SWL_BW_160MHZ;
    wld_chan_interference_t chan_21_bw_160_6g[] = {
        {.channel = 1, .interference_level = 1},
        {.channel = 5, .interference_level = 1},
        {.channel = 9, .interference_level = 1},
        {.channel = 13, .interference_level = 1},
        {.channel = 17, .interference_level = 1},
        {.channel = 21, .interference_level = 1},
        {.channel = 25, .interference_level = 1},
        {.channel = 29, .interference_level = 1},
    };
    memset(interferenceList, 0, sizeof(interferenceList));
    wld_channel_get_interference_list(chanspec, interferenceList, list_size);
    assert_memory_equal(chan_21_bw_160_6g, interferenceList, SWL_ARRAY_SIZE(chan_21_bw_160_6g));
}

static void test_wld_channel_bands_overlapping(void** state) {
    (void) state;
    swl_chanspec_t chanspec1 = SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);
    swl_chanspec_t chanspec2 = SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);

    // 1/20 and 1/20
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 4/20 and 1/20
    chanspec1.channel = 4;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 6/20 and 1/20
    chanspec1.channel = 6;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 6/40 and 1/20
    chanspec1.bandwidth = SWL_BW_40MHZ;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 11/40 and 1/40
    chanspec1.channel = 11;
    chanspec2.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 13/40 and 1/40
    chanspec1.channel = 13;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 1/20 and 36/20
    chanspec1.channel = 1;
    chanspec2.channel = 36;
    chanspec2.bandwidth = SWL_BW_20MHZ;
    chanspec2.band = SWL_FREQ_BAND_EXT_5GHZ;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 36/20 and 36/20
    chanspec1.channel = 36;
    chanspec1.bandwidth = SWL_BW_20MHZ;
    chanspec1.band = SWL_FREQ_BAND_EXT_5GHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 36/20 and 40/20
    chanspec2.channel = 40;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 36/20 and 40/40
    chanspec2.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 36/20 and 40/80
    chanspec2.bandwidth = SWL_BW_80MHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 36/20 and 40/160
    chanspec2.bandwidth = SWL_BW_160MHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 100/20 and 40/160
    chanspec1.channel = 100;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 37/20 and 37/20
    chanspec1.channel = 37;
    chanspec1.bandwidth = SWL_BW_20MHZ;
    chanspec1.band = SWL_FREQ_BAND_EXT_6GHZ;
    chanspec2.channel = 37;
    chanspec2.bandwidth = SWL_BW_20MHZ;
    chanspec2.band = SWL_FREQ_BAND_EXT_6GHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 33/20 and 37/20
    chanspec1.channel = 33;
    assert_false(wld_channel_areBandsOverlapping(chanspec1, chanspec2));

    // 33/20 and 37/80
    chanspec2.bandwidth = SWL_BW_80MHZ;
    assert_true(wld_channel_areBandsOverlapping(chanspec1, chanspec2));
}

static void test_wld_channel_bands_adjacent(void** state) {
    (void) state;
    swl_chanspec_t chanspec1 = SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);
    swl_chanspec_t chanspec2 = SWL_CHANSPEC_NEW(1, SWL_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);

    // 1/20 and 1/20
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 3/20 and 1/20
    chanspec1.channel = 3;
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 6/20 and 1/20
    chanspec1.channel = 6;
    assert_false(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 6/20 and 1/40
    chanspec2.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 6/40 and 1/40
    chanspec1.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 11/40 and 1/40
    chanspec1.channel = 11;
    assert_false(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 11/20 and 36/20
    chanspec1.bandwidth = SWL_BW_20MHZ;
    chanspec2.channel = 36;
    chanspec2.bandwidth = SWL_BW_20MHZ;
    chanspec2.band = SWL_FREQ_BAND_EXT_5GHZ;
    assert_false(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 36/20 and 36/20
    chanspec1.channel = 36;
    chanspec1.band = SWL_FREQ_BAND_EXT_5GHZ;
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 36/20 and 40/40
    chanspec2.channel = 40;
    chanspec2.bandwidth = SWL_BW_40MHZ;
    assert_true(wld_channel_isBandAdjacentTo(chanspec1, chanspec2));

    // 40/40 and 36/20
    assert_false(wld_channel_isBandAdjacentTo(chanspec2, chanspec1));
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
    sahTraceAddZone(sahTraceLevel(), "pcb");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_channel_is_dfs),
        cmocka_unit_test(test_wld_channel_is_dfs_band),
        cmocka_unit_test(test_wld_channel_getDfsPercentage),
        cmocka_unit_test(test_wld_channel_is_hp_dfs),
        cmocka_unit_test(test_wld_channel_is_5ghz),
        cmocka_unit_test(test_wld_get_nr_channels_in_band),
        cmocka_unit_test(test_wld_channel_get_base_channel),
        cmocka_unit_test(test_wld_channel_get_center_channel),
        cmocka_unit_test(test_wld_channel_is_long_wait_band),
        cmocka_unit_test(test_wld_channel_is_chan_in_band),
        cmocka_unit_test(test_wld_channel_getComplementaryBaseChannel),
        cmocka_unit_test(test_wld_channel_get_interference_list),
        cmocka_unit_test(test_wld_channel_bands_overlapping),
        cmocka_unit_test(test_wld_channel_bands_adjacent),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
