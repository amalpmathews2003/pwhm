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
/*
 * This file includes nl80211 debug function (dump) definitions
 */

#ifndef INCLUDE_WLD_WLD_NL80211_DEBUG_H_
#define INCLUDE_WLD_WLD_NL80211_DEBUG_H_

#include "wld_nl80211_types.h"
#include "swl/swl_returnCode.h"

/*
 * @brief dump all wiphy info into a variant map (for debug purpose)
 *
 * @param pIfaceInfo pointer to already filled iface info struct
 * @param retMap pointer to variant map to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_dumpIfaceInfo(wld_nl80211_ifaceInfo_t* pIfaceInfo, amxc_var_t* retMap);

/*
 * @brief dump all wiphy info into a variant map (for debug purpose)
 *
 * @param pWiphyInfo pointer to already filled wiphy info struct
 * @param retMap pointer to variant map to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_dumpWiphyInfo(wld_nl80211_wiphyInfo_t* pWiphyInfo, amxc_var_t* retMap);

/*
 * @brief dump all station info into a variant map (for debug purpose)
 *
 * @param pStationInfo pointer to already filled station info struct
 * @param retMap pointer to variant map to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_dumpStationInfo(wld_nl80211_stationInfo_t* pStationInfo, amxc_var_t* retMap);

/*
 * @brief dump all stations info into a map tree (for debug purpose)
 *
 * @param pAllStationInfo pointer to list of filled stations info struct
 * @param nrStation number of stations
 * @param retMap pointer to variant map to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_dumpAllStationInfo(wld_nl80211_stationInfo_t* pAllStationInfo, uint32_t nrStation, amxc_var_t* retMap);

#endif /* INCLUDE_WLD_WLD_NL80211_DEBUG_H_ */
