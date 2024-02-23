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
#include "wld_assocdev.h"
#include "swla/swla_delayExec.h"
#include "swla/swla_trans.h"
#include "wld_dm_trans.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "chanMgt"

/* timeout in msec for the setChanspec */
#define WLD_CHANMGT_REQ_CHANGE_CS_TIMEOUT 10000

static void s_saveClearedChannels(T_Radio* pRad, amxd_trans_t* pTrans) {
    swl_channel_t clearList[50];
    memset(clearList, 0, sizeof(clearList));
    size_t nr_chans = wld_channel_get_cleared_channels(pRad, clearList, sizeof(clearList));
    char buf[256] = {0};
    swl_conv_uint8ArrayToChar(buf, sizeof(buf), clearList, nr_chans);
    amxd_trans_set_value(cstring_t, pTrans, "ClearedDfsChannels", buf);
}

static void s_saveRadarChannels(T_Radio* pRad, amxd_trans_t* pTrans) {
    char buf[256] = {0};
    swl_conv_uint8ArrayToChar(buf, sizeof(buf), pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels);
    amxd_trans_set_value(cstring_t, pTrans, "RadarTriggeredDfsChannels", buf);
}

static void s_writeRadarChannels(T_Radio* pRad) {
    uint8_t nrLastChannels = pRad->nrRadarDetectedChannels;
    swl_channel_t currDfsChannels[WLD_MAX_POSSIBLE_CHANNELS];
    memcpy(currDfsChannels, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    memset(pRad->radarDetectedChannels, 0, WLD_MAX_POSSIBLE_CHANNELS);

    pRad->nrRadarDetectedChannels = wld_channel_get_radartriggered_channels(pRad, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    if(!swl_typeUInt8_arrayEquals(currDfsChannels, nrLastChannels, pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels)) {
        pRad->nrLastRadarChannelsAdded = swl_typeUInt8_arrayDiff(pRad->lastRadarChannelsAdded, SWL_BW_CHANNELS_MAX,
                                                                 pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels, currDfsChannels, nrLastChannels);
    }
}

static void s_saveDfsChanInfo(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTI_TRUE(pRad->hasDmReady, , ME, "%s: radio dm obj not ready for updates", pRad->Name);
    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    ASSERTI_NOT_NULL(chanObject, , ME, "NULL");
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(chanObject, &trans, , ME, "%s : trans init failure", pRad->Name);
    s_saveClearedChannels(pRad, &trans);
    s_saveRadarChannels(pRad, &trans);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pRad->Name);
}

void wld_chanmgt_writeDfsChannels(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, , ME, "Radio null");
    s_writeRadarChannels(pRad);
    s_saveDfsChanInfo(pRad);
}

static void s_sendNotification(wld_rad_chanChange_t* change) {
    ASSERTS_NOT_NULL(change, , ME, "NULL");
    T_Radio* pRad = change->pRad;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, , ME, "NULL");

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    amxc_var_t* my_map = amxc_var_add_key(amxc_htable_t, &params, "Updates", NULL);

    swl_type_addToMap(&gtSwl_type_timeMono, my_map, "TimeStamp", &change->changeTime);
    amxc_var_add_key(uint32_t, my_map, "OldChannel", change->old.channel);
    amxc_var_add_key(uint32_t, my_map, "NewChannel", change->new.channel);
    amxc_var_add_key(cstring_t, my_map, "ChannelChangeReason", g_wld_channelChangeReason_str[change->reason]);
    amxc_var_add_key(cstring_t, my_map, "OldBandwidth", Rad_SupBW[change->old.bandwidth]);
    amxc_var_add_key(cstring_t, my_map, "NewBandwidth", Rad_SupBW[change->new.bandwidth]);
    amxc_var_add_key(uint32_t, my_map, "NrAssociatedStations", change->nrSta);
    amxc_var_add_key(uint32_t, my_map, "NrAssociatedVideoStations", change->nrVid);

    amxd_object_trigger_signal(pRad->pBus, "Channel change event", &params);

    SAH_TRACEZ_INFO(ME, "Send channel change notification");

    amxc_var_clean(&params);
}


const char* chanspecShowing_str[CHANNEL_INTERNAL_STATUS_MAX] = {"Current", "Target", "Sync"};

static bool s_isChanspecSync(T_Radio* pR) {
    return swl_typeChanspecExt_equals(pR->targetChanspec.chanspec, pR->currentChanspec.chanspec);
}

static void s_updateChannelShowing(T_Radio* pR) {
    amxd_object_t* object = amxd_object_get_child(pR->pBus, "ChannelMgt");
    swl_conv_objectParamSetEnum(object, "ChanspecShowing", pR->channelShowing, chanspecShowing_str, CHANNEL_INTERNAL_STATUS_MAX);
}

static void s_updateChanDetailed(wld_rad_detailedChanState_t chanDet, amxd_object_t* object) {

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, , ME, "trans init failure");

    amxd_trans_set_uint16_t(&trans, "Channel", chanDet.chanspec.channel);
    amxd_trans_set_cstring_t(&trans, "Bandwidth", swl_radBw_str[swl_chanspec_toRadBw(&chanDet.chanspec)]);
    amxd_trans_set_cstring_t(&trans, "Frequency", swl_freqBandExt_str[chanDet.chanspec.band]);
    amxd_trans_set_cstring_t(&trans, "Reason", g_wld_channelChangeReason_str[chanDet.reason]);
    amxd_trans_set_cstring_t(&trans, "ReasonExt", chanDet.reasonExt);
    swl_type_toObjectParam(&gtSwl_type_timeMono, object, "LastChangeTime", &chanDet.changeTime);


    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");

}

static void s_updateTargetChanspec(T_Radio* pR) {
    s_updateChanDetailed(pR->targetChanspec, amxd_object_findf(pR->pBus, "ChannelMgt.TargetChanspec"));
}

static void s_updateCurrentChanspec(T_Radio* pR) {
    s_updateChanDetailed(pR->currentChanspec, amxd_object_findf(pR->pBus, "ChannelMgt.CurrentChanspec"));
}

static void s_cleanChannelListTillSize(T_Radio* pRad, size_t maxSize) {

    amxc_llist_t* changeList = &(pRad->channelChangeList);
    amxc_llist_it_t* it = amxc_llist_get_first(changeList);
    while(amxc_llist_size(changeList) > maxSize && it != NULL) {
        amxc_llist_it_t* nextIt = it->next;
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        amxc_llist_it_take(&change->it);
        amxd_object_delete(&change->object);
        free(change);
        it = nextIt;
    }
}

static swl_rc_ne s_writeChangeToOdl(wld_rad_chanChange_t* change) {
    ASSERTS_NOT_NULL(change, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = change->pRad;
    ASSERTS_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, SWL_RC_ERROR, ME, "NULL");
    amxd_object_t* template = amxd_object_findf(pRad->pBus, "ChannelMgt.ChannelChanges");

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(template, &trans, SWL_RC_ERROR, ME, "%s : trans init failure", pRad->Name);

    amxd_trans_add_inst(&trans, pRad->totalNrCurrentChanspecChanges, NULL);

    swl_typeTimeMono_toTransParam(&trans, "TimeStamp", change->changeTime);

    amxd_trans_set_uint8_t(&trans, "OldChannel", change->old.channel);
    amxd_trans_set_cstring_t(&trans, "OldBandwidth", Rad_SupBW[change->old.bandwidth]);
    amxd_trans_set_uint32_t(&trans, "NewChannel", change->new.channel);
    amxd_trans_set_cstring_t(&trans, "NewBandwidth", Rad_SupBW[change->new.bandwidth]);
    amxd_trans_set_cstring_t(&trans, "ChannelChangeReason", g_wld_channelChangeReason_str[change->reason]);
    amxd_trans_set_cstring_t(&trans, "ChannelChangeReasonExt", change->reasonExt);
    amxd_trans_set_uint16_t(&trans, "NrSta", change->nrSta);
    amxd_trans_set_uint16_t(&trans, "NrVideoSta", change->nrVid);

    amxd_trans_set_uint32_t(&trans, "TargetChannel", change->target.channel);
    amxd_trans_set_cstring_t(&trans, "TargetBandwidth", Rad_SupBW[change->target.bandwidth]);
    swl_typeTimeMono_toTransParam(&trans, "TargetChangeTime", change->targetChangeTime);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "%s : trans apply failure", pRad->Name);


    change->object = amxd_object_get_instance(template, NULL, pRad->totalNrCurrentChanspecChanges);
    return SWL_RC_OK;
}

static void s_saveChannelChange(wld_rad_chanChange_t* change) {
    ASSERTS_FALSE(s_writeChangeToOdl(change) < SWL_RC_OK, , ME, "Fail to save channel change");
    s_sendNotification(change);
}

static void s_saveCurrentChanspec(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    s_updateCurrentChanspec(pRad);
    s_updateChannelShowing(pRad);
    wld_rad_updateChannelsInUse(pRad);
    wld_rad_chan_update_model(pRad, NULL);
}

static wld_rad_chanChange_t* s_logChannelChange(T_Radio* pRad, swl_chanspec_t oldSpec) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");

    uint32_t maxChangelistSize = pRad->channelChangeListSize;
    ASSERTI_NOT_EQUALS(maxChangelistSize, 0, NULL, ME, "Log size 0");

    wld_rad_chanChange_t* change = calloc(1, sizeof(wld_rad_chanChange_t));
    ASSERTS_NOT_NULL(change, NULL, ME, "NULL");

    change->pRad = pRad;
    change->old = oldSpec;
    change->new = pRad->currentChanspec.chanspec;
    change->target = pRad->targetChanspec.chanspec;
    change->changeTime = swl_time_getMonoSec();
    change->targetChangeTime = pRad->targetChanspec.changeTime;
    change->reason = pRad->currentChanspec.reason;
    snprintf(change->reasonExt, sizeof(change->reasonExt), "%s", pRad->currentChanspec.reasonExt);
    change->nrSta = wld_rad_get_nb_active_stations(pRad);
    change->nrVid = wld_rad_get_nb_active_video_stations(pRad);

    amxc_llist_t* changeList = &(pRad->channelChangeList);
    amxc_llist_append(changeList, &change->it);

    s_cleanChannelListTillSize(pRad, maxChangelistSize);

    return change;
}

static void s_onUpdateChannelChangeConfig(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, , ME, "NULL");
    s_cleanChannelListTillSize(pRad, pRad->channelChangeListSize);
}

static const char* s_isTargetChanspecValid(T_Radio* pR, swl_chanspec_t chanspec) {
    bool isValid = ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) && swl_channel_is2g(chanspec.channel)) ||
        ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) && swl_channel_is5g(chanspec.channel)) ||
        ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && swl_channel_is6g(chanspec.channel));
    if(!isValid) {
        return "Illegal channel for band";
    }

    if(swl_typeChanspec_equals(pR->targetChanspec.chanspec, chanspec)) {
        return "same chanspec";
    }

    if(wld_channel_is_band_radar_detected(chanspec)) {
        return "Chanspec not available due to radar";
    }

    return NULL;
}

static void s_setChanspecDone(T_Radio* pR, amxd_status_t status) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    if((pR->callIdReqChanspec.status != SWL_FUNCTION_DEFERRED_STATUS_STARTED) &&
       (pR->callIdReqChanspec.status != SWL_FUNCTION_DEFERRED_STATUS_CANCELLED)) {
        return;
    }

    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, &ret, "Result", (status == amxd_status_ok));

    SAH_TRACEZ_INFO(ME, "%s: setChanspec() call end: %u", pR->Name, status);
    swl_function_deferDone(&pR->callIdReqChanspec, status, NULL, &ret);

    SAH_TRACEZ_INFO(ME, "%s: notify channel switch done, Result=%u",
                    pR->Name, (status == amxd_status_ok));
    amxd_object_trigger_signal(pR->pBus, "ChannelSwitchComplete", &ret);

    amxc_var_clean(&ret);
    amxp_timer_delete(&pR->timerReqChanspec);
    pR->timerReqChanspec = NULL;
}

swl_rc_ne wld_chanmgt_reportCurrentChanspec(T_Radio* pR, swl_chanspec_t chanspec, wld_channelChangeReason_e reason) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");

    if(swl_typeChanspec_equals(chanspec, pR->currentChanspec.chanspec)) {
        SAH_TRACEZ_INFO(ME, "%s: report same chanspec, %s (reason %s)", pR->Name,
                        swl_typeChanspecExt_toBuf32(chanspec).buf,
                        g_wld_channelChangeReason_str[reason]);
        return SWL_RC_INVALID_STATE;
    }

    swl_chanspec_t oldSpec = pR->currentChanspec.chanspec;

    if(pR->currentChanspec.chanspec.channel != chanspec.channel) {
        pR->channelChangeReason = reason;
    }
    if(pR->currentChanspec.chanspec.bandwidth != chanspec.bandwidth) {
        pR->channelBandwidthChangeReason = reason;
    }

    SAH_TRACEZ_WARNING(ME, "%s: set cur chanspec %s -> %s (reason <%s>, target %s, nrSta %u/%u)",
                       pR->Name,
                       swl_typeChanspecExt_toBuf32(pR->currentChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(chanspec).buf,
                       g_wld_channelChangeReason_str[reason],
                       swl_typeChanspecExt_toBuf32(pR->targetChanspec.chanspec).buf,
                       wld_rad_get_nb_active_stations(pR), wld_rad_get_nb_active_video_stations(pR)
                       );

    pR->totalNrCurrentChanspecChanges++;
    pR->channelChangeCounters[reason]++;

    if(swl_function_deferIsActive(&pR->callIdReqChanspec)) {
        bool success = swl_type_equals(swl_type_chanspec, &pR->targetChanspec.chanspec, &chanspec);
        success &= (pR->targetChanspec.reason == reason);
        amxd_status_t status = success ? amxd_status_ok : amxd_status_unknown_error;
        s_setChanspecDone(pR, status);
    }

    if(pR->operatingChannelBandwidth != SWL_RAD_BW_AUTO) {
        pR->operatingChannelBandwidth = swl_chanspec_toRadBw(&chanspec);
    }

    pR->currentChanspec.chanspec = chanspec;
    pR->currentChanspec.reason = reason;
    pR->currentChanspec.changeTime = swl_time_getMonoSec();
    pR->channel = chanspec.channel;
    pR->runningChannelBandwidth = swl_chanspec_toRadBw(&chanspec);

    if((reason == pR->targetChanspec.reason) && swl_typeChanspec_equals(pR->targetChanspec.chanspec, pR->currentChanspec.chanspec)) {
        swl_str_cat(pR->currentChanspec.reasonExt, sizeof(pR->currentChanspec.reasonExt), pR->targetChanspec.reasonExt);
    } else {
        memset(pR->targetChanspec.reasonExt, 0, sizeof(pR->targetChanspec.reasonExt));
    }


    wld_rad_chanChange_t* change = s_logChannelChange(pR, oldSpec);

    if(s_isChanspecSync(pR)) {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_SYNC;
        swl_str_copy(pR->currentChanspec.reasonExt, sizeof(pR->currentChanspec.reasonExt), pR->targetChanspec.reasonExt);
    } else {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_CURRENT;
    }

    if(pR->hasDmReady) {
        s_saveChannelChange(change);
        s_saveCurrentChanspec(pR);
    }

    /* notify about channel change status */
    wld_rad_triggerChangeEvent(pR, WLD_RAD_CHANGE_CHANSPEC, NULL);

    /* Reset to default channel if an invalid channel switch occurred */
    if(reason == CHAN_REASON_INVALID) {
        swl_chanspec_t resetChanspec = SWL_CHANSPEC_NEW(swl_channel_defaults[pR->operatingFrequencyBand],
                                                        pR->targetChanspec.chanspec.bandwidth,
                                                        pR->operatingFrequencyBand);
        SAH_TRACEZ_ERROR(ME, "%s: Invalid channel switch <%u>, reset to default channel <%u>", pR->Name, pR->channel, resetChanspec.channel);
        wld_chanmgt_setTargetChanspec(pR, resetChanspec, true, CHAN_REASON_RESET, NULL);
    }
    if((reason == CHAN_REASON_INITIAL) && (pR->targetChanspec.chanspec.channel == 0)) {
        wld_chanmgt_setTargetChanspec(pR, chanspec, false, CHAN_REASON_INITIAL, NULL);
    }

    return SWL_RC_OK;
}

static void s_updateTargetDm(void* data) {
    T_Radio* pR = (T_Radio*) data;
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "ERR");

    s_updateChannelShowing(pR);
    s_updateTargetChanspec(pR);
}

/**
 * Set target chanspec for a specific radio
 *
 * Notes:
 * - frequency band switching is not supported
 * - reasonExt is not used for the moment
 */
swl_rc_ne wld_chanmgt_setTargetChanspec(T_Radio* pR, swl_chanspec_t chanspec, bool direct, wld_channelChangeReason_e reason, const char* reasonExt) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");
    ASSERT_TRUE(pR->isReady || reason == CHAN_REASON_INITIAL, SWL_RC_ERROR, ME, "%s: radio is not configured yet", pR->Name)

    swl_rc_ne rc = SWL_RC_OK;

    swl_chanspec_t tgtChanspec = chanspec;

    if(tgtChanspec.bandwidth == SWL_BW_AUTO) {
        tgtChanspec.bandwidth = swl_bandwidth_defaults[wld_rad_getFreqBand(pR)];
    }
    while(!wld_channel_is_band_available(tgtChanspec) && tgtChanspec.bandwidth > 0) {
        tgtChanspec.bandwidth--;
    }
    ASSERT_TRUE(tgtChanspec.bandwidth > 0, SWL_RC_ERROR, ME, "%s: channel / bw not available %s",
                pR->Name, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf);

    const char* checkReason = s_isTargetChanspecValid(pR, tgtChanspec);
    ASSERT_NULL(checkReason, SWL_RC_ERROR, ME, "%s: %s <%s>", pR->Name, checkReason, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf);

    if(!wld_rad_firstCommitFinished(pR) && (reason == CHAN_REASON_MANUAL) && (pR->totalNrTargetChanspecChanges < 2)) {
        reason = CHAN_REASON_PERSISTANCE;
    }

    SAH_TRACEZ_WARNING(ME, "%s: set tgt chanspec %s -> %s (reason <%s>, direct <%d>, current %s, in %s)",
                       pR->Name,
                       swl_typeChanspecExt_toBuf32(pR->targetChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(tgtChanspec).buf,
                       g_wld_channelChangeReason_str[reason], direct,
                       swl_typeChanspecExt_toBuf32(pR->currentChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(chanspec).buf
                       );

    pR->totalNrTargetChanspecChanges++;

    pR->channel = tgtChanspec.channel;

    memset(&pR->targetChanspec.chanspec, 0, sizeof(swl_chanspec_t));
    pR->targetChanspec.chanspec = tgtChanspec;
    pR->targetChanspec.reason = reason;
    pR->targetChanspec.changeTime = swl_time_getMonoSec();
    pR->channelChangeReason = reason;
    if(reasonExt != NULL) {
        swl_str_copy(pR->targetChanspec.reasonExt, sizeof(pR->targetChanspec.reasonExt), reasonExt);
    } else {
        memset(pR->targetChanspec.reasonExt, 0, sizeof(pR->targetChanspec.reasonExt));
    }

    if(tgtChanspec.channel == amxd_object_get_uint8_t(pR->pBus, "Channel", NULL)) {
        /* operating channel did not change yet, exposed channel in DM is the target */
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_TARGET;
    } else {
        /* exposed channel in DM is not up to date and still print current value */
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_CURRENT;
    }

    /* Change channel */
    if(reason != CHAN_REASON_INITIAL) {
        rc = pR->pFA->mfn_wrad_setChanspec(pR, direct);
    }

    if(s_isChanspecSync(pR)) {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_SYNC;
    }

    swla_delayExec_add(s_updateTargetDm, pR);

    return rc;
}

static void s_setChanspecCanceled(swl_function_deferredInfo_t* info, void* const priv) {
    T_Radio* pR = (T_Radio*) priv;
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_EQUALS(&pR->callIdReqChanspec, info, , ME, "not matching callInfo");
    s_setChanspecDone(pR, amxd_status_unknown_error);
}

static void s_setChanspecTimeout(amxp_timer_t* timer, void* userdata) {
    T_Radio* pR = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_EQUALS(pR->timerReqChanspec, timer, , ME, "INVALID");
    SAH_TRACEZ_WARNING(ME, "%s: chanspec timeout", pR->Name);
    s_setChanspecDone(pR, amxd_status_unknown_error);
}

amxd_status_t _Radio_setChanspec(amxd_object_t* obj,
                                 amxd_function_t* func,
                                 amxc_var_t* args,
                                 amxc_var_t* ret) {
    T_Radio* pR = obj->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    uint16_t channel = GET_UINT32(args, "channel");
    const char* bandwidth_str = GET_CHAR(args, "bandwidth");
    const char* reason_str = GET_CHAR(args, "reason");
    const char* reasonExt = GET_CHAR(args, "reasonExt");
    bool direct = GET_BOOL(args, "direct");

    if(!wld_rad_hasChannel(pR, channel)) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid channel %d", pR->Name, channel);
        return amxd_status_invalid_arg;
    }

    swl_freqBandExt_e freqBand = pR->operatingFrequencyBand;

    swl_radBw_e radBw = swl_conv_charToEnum(bandwidth_str, swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    if(radBw > SWL_RAD_BW_MAX) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid bandwidth %s - %d (max: %d)", pR->Name, bandwidth_str, radBw, SWL_RAD_BW_MAX);
        return amxd_status_invalid_arg;
    }
    swl_bandwidth_e bandwidth = swl_radBw_toBw[radBw];

    if(swl_chanspec_bwToInt(bandwidth) > swl_chanspec_bwToInt(pR->maxChannelBandwidth)) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid bandwidth %s (max: %s)", pR->Name,
                         swl_bandwidth_str[bandwidth], swl_bandwidth_str[pR->maxChannelBandwidth]);
        return amxd_status_invalid_arg;
    }

    swl_chanspec_t chanspec = swl_chanspec_fromDm(channel, radBw, freqBand);
    wld_channelChangeReason_e reason = swl_conv_charToEnum(reason_str, g_wld_channelChangeReason_str, CHAN_REASON_MAX, CHAN_REASON_MANUAL);

    SAH_TRACEZ_INFO(ME, "%s: request chanspec <%s> reason <%s> direct <%d>",
                    pR->Name, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf, g_wld_channelChangeReason_str[reason], direct);

    swl_rc_ne rc = wld_chanmgt_setTargetChanspec(pR, chanspec, direct, reason, reasonExt);
    ASSERT_FALSE(rc < SWL_RC_OK, amxd_status_unknown_error,
                 ME, "%s: fail to set channel %d", pR->Name, channel);
    chanspec = pR->targetChanspec.chanspec;

    /* release/notify old call, if any.*/
    if(swl_function_deferIsActive(&pR->callIdReqChanspec)) {
        amxd_function_deferred_remove(pR->callIdReqChanspec.callId);
    }

    /* register callId & cancel callback */
    swl_function_deferCb(&pR->callIdReqChanspec, func, ret, s_setChanspecCanceled, pR);

    /* set timeout for the call
     * if the band is cleared use default timeout
     * if not, wait clear time in addition */
    uint32_t timeout = WLD_CHANMGT_REQ_CHANGE_CS_TIMEOUT;
    timeout += wld_channel_is_band_passive(chanspec) ? wld_channel_get_band_clear_time(chanspec) : 0;
    amxp_timer_new(&pR->timerReqChanspec, s_setChanspecTimeout, pR);
    amxp_timer_start(pR->timerReqChanspec, timeout);

    if(!direct) {
        /* schedule action */
        wld_autoCommitMgr_notifyRadEdit(pR);
    }

    /* Used for extra logging */
    if(reasonExt) {
        SAH_TRACEZ_INFO(ME, "%s: Reason detailed:", pR->Name);
        SAH_TRACEZ_INFO(ME, "\t%s", reasonExt);
    }

    return amxd_status_deferred;
}

static void s_setChannelChangeLogSize_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    pR->channelChangeListSize = amxc_var_dyncast(uint32_t, newValue);
    s_onUpdateChannelChangeConfig(pR);
    SAH_TRACEZ_WARNING(ME, "%s: Update channel changes list size to %d", pR->Name, pR->channelChangeListSize);

    SAH_TRACEZ_OUT(ME);
}

static void s_setAcsBootChannel_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    pR->acsBootChannel = amxc_var_dyncast(int32_t, newValue);
    SAH_TRACEZ_WARNING(ME, "%s: Update ACS Boot channel to %d", pR->Name, pR->acsBootChannel);
    SAH_TRACEZ_OUT(ME);
}

/**
 * Ensure current channels is properly filled in with default.
 */
void wld_chanmgt_checkInitChannel(T_Radio* pRad) {
    if(pRad->currentChanspec.chanspec.channel != 0) {
        return;
    }
    pRad->channelChangeListSize = 10;

    swl_freqBand_e freqB = wld_rad_getFreqBand(pRad);
    swl_chanspec_t defaultChanspec = SWL_CHANSPEC_NEW(swl_channel_defaults[freqB],
                                                      swl_bandwidth_defaults[freqB], pRad->operatingFrequencyBand);
    //consider the default chanspec from the driver
    //otherwise fall back to implicit default chanspec per freqBand
    swl_chanspec_t radChanspec = wld_rad_getSwlChanspec(pRad);
    if(swl_chanspec_channelToMHzDef(&radChanspec, 0) != 0) {
        defaultChanspec = radChanspec;
    }
    wld_chanmgt_reportCurrentChanspec(pRad, defaultChanspec, CHAN_REASON_INITIAL);
}

/**
 * update of the radio data model after create
 */
void wld_chanmgt_saveChanges(T_Radio* pRad, amxd_trans_t* trans) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pBus, , ME, "NULL");
    ASSERTI_TRUE(pRad->hasDmReady, , ME, "%s: radio dm obj not ready for updates", pRad->Name);
    s_updateCurrentChanspec(pRad);
    s_updateTargetDm(pRad);

    amxc_llist_for_each(it, &pRad->channelChangeList) {
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        if(change->object == NULL) {
            s_writeChangeToOdl(change);
        }
    }

    swla_trans_t tmpTrans;
    amxd_trans_t* targetTrans = swla_trans_init(&tmpTrans, trans, pRad->pBus);
    ASSERT_NOT_NULL(targetTrans, , ME, "NULL");

    swl_channel_t channel = amxd_object_get_uint32_t(pRad->pBus, "Channel", NULL);
    if((channel == 0) || (channel == pRad->channel)) {
        if(channel == 0) {
            amxd_trans_set_uint32_t(targetTrans, "Channel", pRad->channel);
        }
        amxd_trans_set_cstring_t(targetTrans, "ChannelsInUse", pRad->channelsInUse);
        amxd_trans_set_cstring_t(targetTrans, "ChannelChangeReason", g_wld_channelChangeReason_str[pRad->channelChangeReason]);
    }

    swl_radBw_e radBw = swl_conv_objectParamEnum(pRad->pBus, "OperatingChannelBandwidth", swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    if((radBw == SWL_RAD_BW_AUTO) || (radBw == pRad->operatingChannelBandwidth)) {
        if(radBw == SWL_RAD_BW_AUTO) {
            amxd_trans_set_cstring_t(targetTrans, "OperatingChannelBandwidth", swl_radBw_str[pRad->operatingChannelBandwidth]);
        }
        amxd_trans_set_cstring_t(targetTrans, "CurrentOperatingChannelBandwidth", swl_radBw_str[pRad->runningChannelBandwidth]);
        amxd_trans_set_cstring_t(targetTrans, "ChannelBandwidthChangeReason", g_wld_channelChangeReason_str[pRad->channelBandwidthChangeReason]);
    }

    swla_trans_finalize(&tmpTrans, NULL);
}

/**
 * Return the current chanspec
 */
swl_chanspec_t wld_chanmgt_getCurChspec(T_Radio* pRad) {
    swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
    ASSERTS_NOT_NULL(pRad, chspec, ME, "NULL");
    return pRad->currentChanspec.chanspec;
}

/**
 * Return the target chanspec
 */
swl_chanspec_t wld_chanmgt_getTgtChspec(T_Radio* pRad) {
    swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
    ASSERTS_NOT_NULL(pRad, chspec, ME, "NULL");
    return pRad->targetChanspec.chanspec;
}

/**
 * Return the current channel
 */
swl_channel_t wld_chanmgt_getCurChannel(T_Radio* pRad) {
    return wld_chanmgt_getCurChspec(pRad).channel;
}

/**
 * Return the current bandwidth
 */
swl_bandwidth_e wld_chanmgt_getCurBw(T_Radio* pRad) {
    return wld_chanmgt_getCurChspec(pRad).bandwidth;
}

/**
 * Return the target channel
 */
swl_channel_t wld_chanmgt_getTgtChannel(T_Radio* pRad) {
    return wld_chanmgt_getTgtChspec(pRad).channel;
}

/**
 * Return the target bandwidth
 */
swl_bandwidth_e wld_chanmgt_getTgtBw(T_Radio* pRad) {
    return wld_chanmgt_getTgtChspec(pRad).bandwidth;
}

void wld_chanmgt_cleanup(T_Radio* pRad) {
    amxc_llist_for_each(it, &pRad->channelChangeList) {
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        amxc_llist_it_take(it);
        free(change);
    }
}

SWLA_DM_HDLRS(sChanMgtDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("ChangeLogSize", s_setChannelChangeLogSize_pwf),
                  SWLA_DM_PARAM_HDLR("AcsBootChannel", s_setAcsBootChannel_pwf)));

void _wld_chanmgt_setConf_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sChanMgtDmHdlrs, sig_name, data, priv);
}

amxd_status_t _wld_chanmgt_validateAcsBootChannel_pvf(amxd_object_t* object,
                                                      amxd_param_t* param _UNUSED,
                                                      amxd_action_t reason _UNUSED,
                                                      const amxc_var_t* const args,
                                                      amxc_var_t* const retval _UNUSED,
                                                      void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_singleton, amxd_status_ok, ME, "obj is not singleton");
    int bootChannel = amxc_var_dyncast(int32_t, args);
    ASSERTI_FALSE((bootChannel < -1) || (bootChannel > 255), amxd_status_invalid_value, ME, "invalid bootChannel %d", bootChannel);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No radio mapped");
    ASSERTI_TRUE(pRad->hasDmReady, amxd_status_ok, ME, "%s: radio config not yet fully loaded", pRad->Name);
    ASSERTI_TRUE(wld_rad_hasChannel(pRad, bootChannel), amxd_status_invalid_value, ME, "%s: not supported bootChannel %d", pRad->Name, bootChannel);
    return amxd_status_ok;
}



void wld_chanmgt_init(T_Radio* pR) {
    swl_function_deferInit(&pR->callIdReqChanspec);
}
