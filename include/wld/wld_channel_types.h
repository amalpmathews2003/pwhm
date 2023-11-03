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

#ifndef CHANNELMANAGEMENT_WLD_CHANNEL_TYPES_H_
#define CHANNELMANAGEMENT_WLD_CHANNEL_TYPES_H_

#include <stdbool.h>

#include "swla/swla_chanspec.h"
#include "swla/swla_time.h"

typedef enum {
    WLD_CHANNEL_EXTENTION_POS_AUTO,
    WLD_CHANNEL_EXTENTION_POS_ABOVE,
    WLD_CHANNEL_EXTENTION_POS_BELOW,
    WLD_CHANNEL_EXTENTION_POS_NONE,
    WLD_CHANNEL_EXTENTION_POS_MAX
} wld_channel_extensionPos_e;

/**
 * Obsolete channel extension position definitions, please use enum
 */
#define REXT_AUTO WLD_CHANNEL_EXTENTION_POS_AUTO
#define REXT_ABOVE_CC WLD_CHANNEL_EXTENTION_POS_ABOVE
#define REXT_BELOW_CC WLD_CHANNEL_EXTENTION_POS_BELOW


/**
 * Obsolete frequency band definitions, please use SWL notation
 */
#define RFBI_NONE SWL_FREQ_BAND_EXT_NONE
#define RFBI_AUTO SWL_FREQ_BAND_EXT_AUTO
#define RFBI_2_4_GHZ SWL_FREQ_BAND_EXT_2_4GHZ
#define RFBI_5_GHZ SWL_FREQ_BAND_EXT_5GHZ
#define RFBI_6_GHZ SWL_FREQ_BAND_EXT_6GHZ
#define RFBI_MAX SWL_FREQ_BAND_EXT_MAX
typedef swl_freqBandExt_e wld_radiofb_e;

/**
 * Obsolete frequency band mask definitions, please use SWL notation
 */
#define RFB_NONE M_SWL_FREQ_BAND_EXT_NONE
#define RFB_AUTO M_SWL_FREQ_BAND_EXT_AUTO
#define RFB_2_4_GHZ M_SWL_FREQ_BAND_EXT_2_4GHZ
#define RFB_5_GHZ M_SWL_FREQ_BAND_EXT_5GHZ
#define RFB_6_GHZ M_SWL_FREQ_BAND_EXT_6GHZ
#define RFB_MAX M_SWL_FREQ_BAND_EXT_MAX
typedef swl_freqBandExt_m wld_radiofb_m;

typedef struct {
    swl_channel_t channel;
    swl_bandwidth_e bandwidth;
} wld_chanspec_t;

/**
 * An element of an interference list, to be requested for a specific channel
 */
typedef struct wld_chan_interference {
    /**
     * The channel for which the interference level applies
     */
    int channel;
    /**
     * The interference level
     */
    int interference_level;
} wld_chan_interference_t;



#endif /* CHANNELMANAGEMENT_WLD_CHANNEL_TYPES_H_ */
