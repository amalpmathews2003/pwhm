/*
 * Copyright (c) 2011 SoftAtHome
 *
 * The information and source code contained herein is the exclusive
 * property of SoftAtHome and may not be disclosed, examined, or
 * reproduced in whole or in part without explicit written authorization
 * of the copyright owner.
 */
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
