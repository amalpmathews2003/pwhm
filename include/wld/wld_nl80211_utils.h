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
/*
 * This file includes nl80211 utility apis for:
 * - type conversion
 * - common attributes fetching
 * - ...
 */

#ifndef INCLUDE_WLD_WLD_NL80211_UTILS_H_
#define INCLUDE_WLD_WLD_NL80211_UTILS_H_

#include "wld_nl80211_compat.h"
#include "swl/swl_common.h"
#include "swl/swl_common_chanspec.h"

/*
 * @brief macro to get nl attribute typed value, when it is found
 */
#define NLA_GET_VAL(target, api, param) \
    if(param) { \
        target = api(param); \
    }

/*
 * @brief macro to get nl attribute data buffer, when it is found
 */
#define NLA_GET_DATA(target, param, maxLen) \
    if(param) { \
        int len = SWL_MIN((int) (maxLen), nla_len(param)); \
        memcpy(target, nla_data(param), len); \
    }

/*
 * @brief fetch Wiphy ID from nl80211 attributes
 *
 * @param tb nl attributes array
 *
 * @return uint32 valid wiphy ID if found
 *         WLD_NL80211_ID_UNDEF (-1) in case of error
 */
uint32_t wld_nl80211_getWiphy(struct nlattr* tb[]);

/*
 * @brief fetch interface netdev index from nl80211 attributes
 *
 * @param tb nl attributes array
 *
 * @return uint32 valid netdev index if found
 *         WLD_NL80211_ID_UNDEF (-1) in case of error
 */
uint32_t wld_nl80211_getIfIndex(struct nlattr* tb[]);

/*
 * @brief convert NL80211_CHAN_WIDTH_xxx into swl bandwidth enum ID
 *
 * @param nl80211Bw nl80211 bandwidth id
 *
 * @return valid SWL_BW_ or SWL_BW_MAX if not match is found
 */
swl_bandwidth_e wld_nl80211_bwNlToSwl(uint32_t nl80211Bw);

/*
 * @brief convert swl bandwidth enum ID into NL80211_CHAN_WIDTH_xxx
 *
 * @param swlBw swl bandwidth enum id
 *
 * @return valid NL80211_CHAN_WIDTH_xxx or -1 if not match is found
 */
int32_t wld_nl80211_bwSwlToNl(swl_bandwidth_e swlBw);

/*
 * @brief convert NL80211_CHAN_WIDTH_xxx into channel bandwidth value in MHz
 *
 * @param nl80211Bw nl80211 bandwidth id
 *
 * @return valid channel bandwidth value or 0 if not match is found
 */
uint32_t wld_nl80211_bwNlToVal(uint32_t nl80211Bw);

/*
 * @brief convert channel bandwidth value in MHz into NL80211_CHAN_WIDTH_xxx
 *
 * @param bwVal channel bandwidth value in MHz
 *
 * @return valid NL80211_CHAN_WIDTH_xxx enum or -1 if not match is found
 */
int32_t wld_nl80211_bwValToNl(uint32_t bwVal);

/*
 * @brief convert swl frequency band enum ID into NL80211_BAND_xxx
 *
 * @param swlFb swl frequency band  enum id
 *
 * @return valid NL80211_BAND_xxx or -1 if not match is found
 */
int32_t wld_nl80211_freqBandSwlToNl(swl_freqBand_e swlFb);

/*
 * @brief convert nl frequency band enum ID NL80211_BAND_xxx into swl freq band enum id
 *
 * @param swlFb nl frequency band enum id NL80211_BAND_xxx
 *
 * @return valid swl freq band enum id or SWL_FREQ_BAND_MAX if not match is found
 */
swl_freqBand_e wld_nl80211_freqBandNlToSwl(uint32_t nl80211Fb);

#endif /* INCLUDE_WLD_WLD_NL80211_UTILS_H_ */
