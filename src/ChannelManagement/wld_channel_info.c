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


#include "wld_channel.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <swl/swl_common.h>
#include "wld_channel_lib.h"
#include "wld_radio.h"

#include <debug/sahtrace.h>

typedef struct {
    swl_bandwidth_e wldBw;
    int chanWidth;
} chanwidthData_t;

const chanwidthData_t nr_channels_ahead[] = {
    {SWL_BW_AUTO, 1},
    {SWL_BW_20MHZ, 1},
    {SWL_BW_40MHZ, 2},
    {SWL_BW_80MHZ, 4},
    {SWL_BW_160MHZ, 8},
    {SWL_BW_320MHZ, 16}
};

SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(nr_channels_ahead) == SWL_BW_RAD_MAX, "nr_channels_ahead not correctly defined");

#define ME "chanInf"

/*
 * Operating class table for regulatory region to operating class conversion.
 */
struct operating_class_table {
    uint8_t index;
    uint8_t bandwidth;
    uint8_t chan_set[64];
};

/**
 * Fill in the list with the amount of channels in the band
 */
int wld_channel_get_channels_in_band(swl_chanspec_t chanspec, int* list, int list_size) {
    swl_chanspec_t baseChanspec;
    memcpy(&baseChanspec, &chanspec, sizeof(swl_chanspec_t));
    if(chanspec.band != SWL_FREQ_BAND_EXT_2_4GHZ) {
        baseChanspec.channel = wld_channel_get_base_channel(chanspec);
        int nr_channels = wld_get_nr_channels_in_band(baseChanspec);
        int i;
        for(i = 0; i < nr_channels && i < list_size; i++) {
            list[i] = baseChanspec.channel + i * CHANNEL_INCREMENT;
        }
        if((i == list_size) && (i < nr_channels)) {
            SAH_TRACEZ_ERROR(ME, "Could not put all channels of %u/%u in list of size %u",
                             chanspec.channel,
                             chanspec.bandwidth,
                             list_size);
        }
        return i;
    } else {
        if((chanspec.bandwidth == SWL_BW_20MHZ) && (list_size >= 1)) {
            list[0] = chanspec.channel;
            return 1;
        } else if((chanspec.bandwidth == SWL_BW_40MHZ) && (list_size >= 5)) {
            int start_channel = chanspec.channel;
            //when the operating channel is greater than 6, we use it as top channel, otherwise bottem channel
            if(start_channel > 6) {
                start_channel -= 4;
            }
            for(int i = 0; i < 5; i++) {
                list[i] = start_channel + i;
            }
            return 5;
        } else {
            SAH_TRACEZ_ERROR(ME, "Could not put all channels of %u/%u @ %u in list of size %u",
                             chanspec.channel,
                             chanspec.bandwidth,
                             chanspec.band,
                             list_size);
            return 0;
        }
    }

}

/**
 * Returns whether or not the given channel is a dfs channel
 */
bool wld_channel_is_dfs(int channel) {
    // non-DFS chans in ETSI: 36..48
    // DFS chans in ETSI: 52..64 , 100..140
    // non-DFS chans in FCC: 36..48 , 149..165
    // DFS chans in FCC:  52..64 , 100..144
    return ((channel >= WLD_CHAN_START_DFS) && (channel <= WLD_CHAN_END_DFS));
}

/**
 * Returns whether a given band contains DFS channels.
 */
bool wld_channel_is_dfs_band(int channel, swl_bandwidth_e bandwidth) {
    if(wld_channel_is_dfs(channel)) {
        return true;
    }
    if(bandwidth == SWL_BW_160MHZ) {
        return true;
    }

    return false;
}

/**
 * Return by which percentage a band is dfs.
 */
uint32_t wld_channel_getDfsPercentage(int channel, swl_bandwidth_e bandwidth) {
    if(bandwidth <= SWL_BW_80MHZ) {
        return wld_channel_is_dfs(channel) ? 100 : 0;
    }
    if(bandwidth == SWL_BW_160MHZ) {
        return wld_channel_is_hp_dfs(channel) ? 100 : 50;
    }
    return 0;
}

bool wld_channel_is_hp_dfs(int channel) {
    // hp DFS chans in ETSI: 100..140
    // hp DFS chans in FCC: 100..144
    // hp non-DFS chans in FCC: 149..165
    return ((channel >= WLD_CHAN_START_HP_DFS) && (channel <= WLD_CHAN_END_DFS));
}

/**
 * Returns whether or not the given channel is a channel on 5GHz
 */
bool wld_channel_is_5ghz(int channel) {
    return channel >= WLD_CHAN_START_5GHZ;
}



/**
 * Return the amount of 20Mhz channels in a given band.
 */
int wld_get_nr_channels_in_band(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_2_4GHZ) {
        int i = 0;
        for(i = 0; i < SWL_BW_RAD_MAX; i++) {
            if(chanspec.bandwidth == nr_channels_ahead[i].wldBw) {
                return nr_channels_ahead[i].chanWidth;
            }
        }
    } else {
        if((chanspec.bandwidth == SWL_BW_20MHZ) || (chanspec.bandwidth == SWL_BW_AUTO)) {
            return 1;
        } else if(chanspec.bandwidth == SWL_BW_40MHZ) {
            return 5;
        }
    }
    SAH_TRACEZ_ERROR(ME, "requested unknown band %d for chan %d", chanspec.bandwidth, chanspec.channel);
    return 0;
}

/**
 * Get the frequency from a channel (in Hertz).
 */
uint32_t wld_channel_getFrequencyOfChannel(swl_chanspec_t chanspec) {
    if(chanspec.band == SWL_FREQ_BAND_EXT_6GHZ) {
        return 5950 + 5 * chanspec.channel;
    } else if(chanspec.band == SWL_FREQ_BAND_EXT_5GHZ) {
        return 5000 + 5 * chanspec.channel;
    } else {
        if(chanspec.channel == 14) {
            return 2484;
        } else {
            return 2407 + 5 * chanspec.channel;
        }
    }
}

/**
 * Get the base channel of a band. This is the first main channel in the band.
 */
int wld_channel_get_base_channel(swl_chanspec_t chanspec) {
    int channel = chanspec.channel;
    if(chanspec.band == SWL_FREQ_BAND_EXT_6GHZ) {
        int nr_chans = wld_get_nr_channels_in_band(chanspec);
        if(nr_chans == 0) {
            SAH_TRACEZ_ERROR(ME, "can't get base %u %u", channel, chanspec.bandwidth);
            return channel;
        }
        if(channel >= 81) {
            return channel - ((channel - 81) % (nr_chans * CHANNEL_INCREMENT));
        } else {
            return channel - ((channel - 1) % (nr_chans * CHANNEL_INCREMENT));
        }
    } else if(chanspec.band == SWL_FREQ_BAND_EXT_5GHZ) {
        int nr_chans = wld_get_nr_channels_in_band(chanspec);
        if(nr_chans == 0) {
            SAH_TRACEZ_ERROR(ME, "can't get base %u %u", chanspec.channel, chanspec.bandwidth);
            return channel;
        }
        if(channel >= 149) {
            return channel - ((channel - 149) % (nr_chans * CHANNEL_INCREMENT));
        } else {
            return channel - ((channel - 36) % (nr_chans * CHANNEL_INCREMENT));
        }
    } else {
        //2.4GHz
        if((chanspec.bandwidth == SWL_BW_40MHZ) && (chanspec.channel > 6)) {
            return channel - 4;
        } else {
            return channel;
        }
    }
}

/**
 * Get the center channel of a band
 */
int wld_channel_get_center_channel(swl_chanspec_t chanspec) {
    int centerChannel;
    int baseChannel = wld_channel_get_base_channel(chanspec);
    int nrChannelsInBand = wld_get_nr_channels_in_band(chanspec);
    if(chanspec.band != SWL_FREQ_BAND_EXT_2_4GHZ) {
        if(nrChannelsInBand > 1) {
            centerChannel = baseChannel + (nrChannelsInBand / 2 * 4) - 2;
        } else {
            centerChannel = baseChannel;
        }
    } else {
        if(chanspec.bandwidth == SWL_BW_40MHZ) {
            centerChannel = baseChannel + 2;
        } else {
            centerChannel = baseChannel;
        }
    }
    return centerChannel;
}

/**
 * Returns the time in milliseconds that a channel should be cleared.
 */
int wld_channel_get_channel_clear_time(int channel) {
    if(!wld_channel_is_dfs(channel)) {
        return 0;
    }
    if((channel >= WLD_CHAN_START_EXTENDED_CLEARTIME)
       && (channel <= WLD_CHAN_END_EXTENDED_CLEARTIME)) {
        return WLD_CHAN_DFS_EXTENDED_CLEAR_TIME_MS;
    } else {
        return WLD_CHAN_DFS_CLEAR_TIME_MS;
    }
}

/**
 * Returns the time in milliseconds that it takes to clear this band.
 */
int wld_channel_get_band_clear_time(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return 0;
    }
    int nrChannels = wld_get_nr_channels_in_band(chanspec);
    int time = 0;
    int temp_time = 0;
    int i = 0;
    int temp_channel = wld_channel_get_base_channel(chanspec);
    for(i = 0; i < nrChannels; i++) {
        temp_time = wld_channel_get_channel_clear_time(temp_channel);
        if(temp_time > time) {
            time = temp_time;
        }
        temp_channel += CHANNEL_INCREMENT;
    }
    return time;
}

//Deprecated
bool wld_channel_is_long_wait(swl_chanspec_t chanspec) {
    return wld_channel_is_long_wait_band(chanspec);
}

/**
 * Returns true if a band is a long wait channel.
 * A band is a long wait band if any channel is the band is in the extend cleartime band.
 */
bool wld_channel_is_long_wait_band(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return false;
    }

    int nrChannels = wld_get_nr_channels_in_band(chanspec);
    swl_chanspec_t tmpChanspec;
    memcpy(&tmpChanspec, &chanspec, sizeof(swl_chanspec_t));
    tmpChanspec.channel = wld_channel_get_base_channel(chanspec);

    int i = 0;
    for(i = 0; i < nrChannels; i++) {

        if(wld_channel_is_long_wait_channel(tmpChanspec)) {
            return true;
        } else {
            tmpChanspec.channel += CHANNEL_INCREMENT;
        }
    }
    return false;
}

bool wld_channel_is_long_wait_channel(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return false;
    }
    return ((chanspec.channel >= WLD_CHAN_START_EXTENDED_CLEARTIME)
            && (chanspec.channel <= WLD_CHAN_END_EXTENDED_CLEARTIME));
}



/**
 * Returns true if the testChannel is in the band determing by the input channel and bandwidth.
 * Returns false otherwise.
 */
bool wld_channel_is_chan_in_band(swl_chanspec_t chanspec, int testChannel) {
    int chan_list[WLD_CHAN_MAX_NR_CHANS_IN_USE];
    int nr_channels = wld_channel_get_channels_in_band(chanspec, chan_list, SWL_ARRAY_SIZE(chan_list));
    int i = 0;
    for(i = 0; i < nr_channels; i++) {
        if(testChannel == chan_list[i]) {
            return true;
        }
    }
    return false;
}

int wld_channel_getComplementaryBaseChannel(swl_chanspec_t chanspec) {
    if((chanspec.band == SWL_FREQ_BAND_EXT_2_4GHZ) || (chanspec.bandwidth <= SWL_BW_20MHZ)) {
        return -1;
    }
    swl_chanspec_t baseChanspec = SWL_CHANSPEC_NEW(wld_channel_get_base_channel(chanspec), chanspec.bandwidth - 1, chanspec.band);
    if(wld_channel_is_chan_in_band(baseChanspec, chanspec.channel)) {
        int nr_channels = wld_get_nr_channels_in_band(baseChanspec);
        return baseChanspec.channel + nr_channels * CHANNEL_INCREMENT;
    } else {
        return baseChanspec.channel;
    }
}

bool wld_channel_hasChannelWidthCovered(swl_chanspec_t chspec, swl_bandwidth_e chW) {
    return (swl_chanspec_bwToInt(chspec.bandwidth) >= swl_chanspec_bwToInt(chW));
}

wld_channel_extensionPos_e wld_channel_getExtensionChannel(swl_chanspec_t chspec, wld_channel_extensionPos_e currExtChanPos) {
    ASSERTI_TRUE(wld_channel_hasChannelWidthCovered(chspec, SWL_BW_40MHZ), WLD_CHANNEL_EXTENTION_POS_NONE, ME, "not supported");
    if(chspec.band != SWL_FREQ_BAND_EXT_2_4GHZ) {
        if((chspec.channel / 4) % 2) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    if(currExtChanPos == WLD_CHANNEL_EXTENTION_POS_AUTO) {
        if(chspec.channel < 7) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    return currExtChanPos;
}
