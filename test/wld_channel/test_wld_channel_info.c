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
        cmocka_unit_test(test_wld_channel_is_dfs_band),
        cmocka_unit_test(test_wld_channel_getDfsPercentage),
        cmocka_unit_test(test_wld_channel_get_center_channel),
        cmocka_unit_test(test_wld_channel_is_long_wait_band),
        cmocka_unit_test(test_wld_channel_is_chan_in_band),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
