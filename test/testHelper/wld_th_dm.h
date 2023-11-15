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

#ifndef SRC_TEST_TESTHELPER_WLD_TH_DM_H_
#define SRC_TEST_TESTHELPER_WLD_TH_DM_H_

#include "wld.h"
#include "test-toolbox/ttb_amx.h"
#include "test-toolbox/ttb_object.h"
#include "wld_th_types.h"

#include "swla/swla_chanspec.h"
#include "swl/swl_common.h"

#include "wld_th_radio.h"
#include "wld_th_vap.h"
#include "wld_th_types.h"
#include "wld_th_mockVendor.h"


typedef struct {
    T_Radio* rad;
    swl_timeMono_t radCreateTime;
    ttb_object_t* radObj;
    T_AccessPoint* vapPriv;
    T_SSID* vapPrivSSID;
    swl_timeMono_t vapCreateTime;
    ttb_object_t* vapPrivObj;
    ttb_object_t* vapPrivSSIDObj;
    T_EndPoint* ep;
    swl_timeMono_t epCreateTime;
    ttb_object_t* epObj;
} wld_th_dmBand_t;

typedef struct {
    wld_th_dmBand_t bandList[SWL_FREQ_BAND_MAX];
    ttb_amx_t* ttbBus;
    wld_th_mockVendor_t* mockVendor;
} wld_th_dm_t;

bool wld_th_dmEnv_init(wld_th_dm_t* dm);
bool wld_th_dm_init(wld_th_dm_t* dm);
bool wld_th_dm_initFreq(wld_th_dm_t* dm, swl_freqBand_m initMask);
void wld_th_dm_destroy(wld_th_dm_t* dm);

void wld_th_dm_handleEvents(wld_th_dm_t* dm);

#endif /* SRC_TEST_TESTHELPER_WLD_TH_DM_H_ */
