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

#include "wld.h"
#include "wld_util.h"
#include "wld_channel.h"
#include "swl/swl_assert.h"
#include "wld_channel_lib.h"
#include <stdlib.h>
#include <string.h>
#include "wld_chanmgt.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_stamon.h"
#include "wld_eventing.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"

#define ME "chanMgt"

static void s_writeClearedChannels(T_Radio* pRad, amxd_object_t* chanObject) {
    swl_channel_t clearList[50];
    memset(clearList, 0, sizeof(clearList));
    size_t nr_chans = wld_channel_get_cleared_channels(pRad, clearList, sizeof(clearList));
    swl_typeUInt8_arrayObjectParamSetChar(chanObject, "ClearedDfsChannels", clearList, nr_chans);

}

static void s_writeRadarChannels(T_Radio* pRad, amxd_object_t* chanObject) {
    uint8_t nrLastChannels = pRad->nrRadarDetectedChannels;
    swl_channel_t currDfsChannels[WLD_MAX_POSSIBLE_CHANNELS];
    memcpy(currDfsChannels, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    memset(pRad->radarDetectedChannels, 0, WLD_MAX_POSSIBLE_CHANNELS);

    pRad->nrRadarDetectedChannels = wld_channel_get_radartriggered_channels(pRad, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    swl_typeUInt8_arrayObjectParamSetChar(chanObject, "RadarTriggeredDfsChannels", pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels);

    if(!swl_typeUInt8_arrayEquals(currDfsChannels, nrLastChannels, pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels)) {
        pRad->nrLastRadarChannelsAdded = swl_typeUInt8_arrayDiff(pRad->lastRadarChannelsAdded, SWL_BW_CHANNELS_MAX,
                                                                 pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels, currDfsChannels, nrLastChannels);
    }
}

void wld_chanmgt_writeDfsChannels(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, , ME, "Radio null");
    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    s_writeClearedChannels(pRad, chanObject);
    s_writeRadarChannels(pRad, chanObject);
}
