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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_wps.h"
#include "wld_assocdev.h"
#include "wld_apMld.h"
#include "swl/swl_common.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_hex.h"
#include "swl/swl_genericFrameParser.h"
#include "swl/swl_staCap.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_eventing.h"
#include "Utils/wld_autoCommitMgr.h"
#include "Utils/wld_autoNeighAdd.h"
#include "wld_ap_nl80211.h"
#include "wld_hostapd_ap_api.h"
#include "wld_dm_trans.h"
#include "Features/wld_persist.h"
#include "wld_extMod.h"

#define ME "mld"

/**
 * Retrieve the affiliatedStaInfo for a given MAC Address associated to a given MLD unit.
 *
 * @param info: the struct in which the affiliated sta info will be written
 * @param mldUnit: the mld unit ID of the ap MLD.
 * @param mac: the mac address for which to do a lookup for a matching affiliated sta.
 *
 * @return
 *  - true if an affiliated sta with the given mac address has been found. The info struct
 * shall be properly filled in with the affiliated sta, the associated device to which the affiliated sta belongs,
 * and the access point to which the associated device is associated.
 * - false if no matching affiliated sta can be found, or some error took place. In this case, the info
 * parameter will not be touched.
 */
bool wld_apMld_fetchAffiliatedStaInfo(wld_apMld_afStaInfo_t* info, int32_t mldUnit, swl_macBin_t* mac) {
    ASSERT_NOT_NULL(info, false, ME, "NULL");
    ASSERT_NOT_NULL(mac, false, ME, "NULL");

    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* tmpAp = NULL;
        wld_rad_forEachAp(tmpAp, pRad) {
            if(tmpAp == NULL) {
                continue;
            }

            if(tmpAp->pSSID == NULL) {
                continue;
            }

            if(tmpAp->pSSID->mldUnit != mldUnit) {
                continue;
            }

            for(int i = 0; i < tmpAp->AssociatedDeviceNumberOfEntries; i++) {
                T_AssociatedDevice* pAD = tmpAp->AssociatedDevice[i];
                amxc_llist_for_each(it, &pAD->affiliatedStaList) {
                    wld_affiliatedSta_t* afSta = amxc_llist_it_get_data(it, wld_affiliatedSta_t, it);
                    if(swl_mac_binMatches(&afSta->mac, mac)) {
                        info->afSta = afSta;
                        info->pAD = pAD;
                        info->mainAp = tmpAp;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}
