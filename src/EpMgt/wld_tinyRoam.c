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

#include "wld_tinyRoam.h"
#include <stdbool.h>
#include "wld.h"
#include "wld_endpoint.h"
#include "swl/swl_common.h"
#include "wld_eventing.h"

#define ME "tyRoam"

/**
 * Tiny Roam allows other components to orchestrate AP roaming.
 *
 * AP roaming is (re)connecting an endpoint to another accesspoint, based on whether this
 * is estimated to yield a better path in the meshnetwork from the repeater to the homegateway.
 *
 * Tiny Roam provides the functionality to do the actual roam while keeping the responsibility
 * of choosing where to roam to to the caller, but still providing the robustness of reconnecting
 * to the network if the roam fails. The latter is important in case the decision to roam
 * is communicated over the network.
 *
 * Note: Counters like #T_roaming_data.nrRoamFail are kept there and updated.
 */
struct wld_tinyRoam {
    /** How many times to try to roam until it is considered failed. Invariant: at least 1. */
    int32_t maxNumberOfAttempts;

    /** How many times an attempt to roam has started. Invariant: at least 0. */
    int32_t numberOfAttemptsStarted;

    /**
     * Number of seconds after an attempt has started but not succeeded yet, to try again
     * or give up. Invariant: at least 1. */
    int32_t attemptTimeoutInSec;

    /**
     * Timer to start first/next roam attempt and check result of previous (if any) attempt.
     * Can be NULL.
     */
    amxp_timer_t* roamAttemptTimer;

    /** BSSID to roam to, or the null-mac if not roaming or if reconnecting after roam failure */
    swl_macBin_t targetBssid;

    /**
     * Callback called when a started roaming stops. Can be NULL.
     * Invariant: if non-null, then targetBssid is not the null-mac. */
    wld_tinyRoam_roamEndedCb_t endedCb;

    /** userdata to pass to #endedCb. Can be NULL. */
    void* endedCbUserData;
};

/** Human-readable version of #wld_tinyRoam_roamResult_e */
const char* const wld_tinyRoam_roamResultChar[] = {
    "Success",
    "Bug encountered. See logs for details.",
    "Already connected to the bssid we want to roam to",
    "Cannot perform tiny roam while (big)roam is enabled",
    "Invalid argument",
    "Cannot roam to myself",
    "Roaming was cancelled because another roam was started",
    "All attempts failed",
};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(wld_tinyRoam_roamResultChar) == WLD_TINYROAM_ROAMRESULT_MAX,
                  "wld_tinyRoam_roamResultChar not correctly defined");


typedef enum {
    ROAMATTEMPTRESULT_NOTROAMING,
    ROAMATTEMPTRESULT_SUCCESS,
    ROAMATTEMPTRESULT_WRONGBSSID,
    ROAMATTEMPTRESULT_NOTCONNECTED,
    ROAMATTEMPTRESULT_MAX,
} roamAttemptResult_e;

static void s_roamTimerHandle(amxp_timer_t* timer _UNUSED, void* userdata);

static bool s_goodEp(T_EndPoint* ep) {
    ASSERT_NOT_NULL(ep, false, ME, "NULL");
    ASSERT_NOT_NULL(ep->tinyRoam, false, ME, "NULL");
    ASSERT_NOT_NULL(ep->pFA, false, ME, "NULL");
    return true;
}

/** basically private read-write version of #wld_tinyRoam_targetBssid */
static swl_macBin_t* s_targetBssid(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), NULL, ME, "NULL");
    return &ep->tinyRoam->targetBssid;
}

/**
 * Where tinyRoam is trying to roam to.
 *
 * Returns null-mac if not roaming.
 */
const swl_macBin_t* wld_tinyRoam_targetBssid(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), &g_swl_macBin_null, ME, "NULL");
    return s_targetBssid(ep);
}

bool wld_tinyRoam_isRoaming(T_EndPoint* ep) {
    return !swl_mac_binIsNull(wld_tinyRoam_targetBssid(ep));
}

/**
 *
 * @param doReconnect: whether to reconnect to arbitrary AP in the network.
 *   You probably want this after giving up attempting to roam to a specific AP.
 */
static void s_stopRoaming(T_EndPoint* ep, bool doReconnect, wld_tinyRoam_roamResult_e roamResult) {
    ASSERT_TRUE(s_goodEp(ep), , ME, "NULL");

    ASSERT_TRUE(wld_tinyRoam_isRoaming(ep), , ME, "Stopping but was not running.");

    *s_targetBssid(ep) = g_swl_macBin_null;

    if(doReconnect) {
        endpointPerformConnectCommit(ep, true);
    }

    if(ep->tinyRoam->roamAttemptTimer != NULL) {
        amxp_timer_stop(ep->tinyRoam->roamAttemptTimer);
    }

    if(ep->tinyRoam->endedCb != NULL) {
        ep->tinyRoam->endedCb(ep, roamResult, ep->tinyRoam->endedCbUserData);
        ep->tinyRoam->endedCb = NULL;
        ep->tinyRoam->endedCbUserData = NULL;
    }
}

/**
 *
 * Not safe to run in bus callback (i.e. too slow and/or calls driver).
 *
 * This is a lower-level function, you probably want to call higher-level
 * #s_doFirstRoamAttempt or #s_processPreviousAttemptResult instead.
 */
static void s_doRoamAttempt(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), , ME, "NULL");

    ep->tinyRoam->numberOfAttemptsStarted++;

    SAH_TRACEZ_WARNING(ME, "%s Attempt (%" PRId32 "/%" PRId32 ") to roam to " SWL_MAC_FMT,
                       ep->Name, ep->tinyRoam->numberOfAttemptsStarted,
                       ep->tinyRoam->maxNumberOfAttempts, SWL_MAC_ARG(s_targetBssid(ep)->bMac));
    // Do this roam attempt:
    endpointPerformConnectCommit(ep, true);

    // Schedule checking result of this roam attempt and potentially starting the next roam attempt.
    if(ep->tinyRoam->roamAttemptTimer != NULL) {
        amxp_timer_delete(&ep->tinyRoam->roamAttemptTimer);
    }
    amxp_timer_new(&ep->tinyRoam->roamAttemptTimer, s_roamTimerHandle, ep);
    amxp_timer_start(ep->tinyRoam->roamAttemptTimer, ep->tinyRoam->attemptTimeoutInSec * 1000);
}

roamAttemptResult_e s_retrieveRoamAttemptResult(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), false, ME, "NULL");

    swl_macBin_t currentBssid = SWL_MAC_BIN_NEW();
    wld_endpoint_getMacAddress(ep, currentBssid.bMac);

    if(!wld_tinyRoam_isRoaming(ep)) {
        return ROAMATTEMPTRESULT_NOTROAMING;
    } else if(swl_mac_binIsNull(&currentBssid)) {
        return ROAMATTEMPTRESULT_NOTCONNECTED;
    } else if(!swl_mac_binMatches(&currentBssid, s_targetBssid(ep))) {
        return ROAMATTEMPTRESULT_WRONGBSSID;
    } else {
        return ROAMATTEMPTRESULT_SUCCESS;
    }
}

static void s_doFirstRoamAttempt(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), , ME, "NULL");
    ASSERT_EQUALS(ep->tinyRoam->numberOfAttemptsStarted, 0, , ME, "Not first attempt");
    ASSERT_FALSE(swl_mac_binIsNull(&ep->tinyRoam->targetBssid), , ME, "Roaming without target");

    swl_macBin_t currentBssid = SWL_MAC_BIN_NEW();
    wld_endpoint_getBssidBin(ep, &currentBssid);
    if(swl_mac_binMatches(&currentBssid, &ep->tinyRoam->targetBssid)) {
        SAH_TRACEZ_ERROR(ME, "%s Already connected to " SWL_MAC_FMT, ep->Name, SWL_MAC_ARG(currentBssid.bMac));
        s_stopRoaming(ep, false, WLD_TINYROAM_ROAMRESULT_ALREADY_CONNECTED);
        return;
    }

    if(wld_isInternalBssidBin(&ep->tinyRoam->targetBssid)) {
        SAH_TRACEZ_ERROR(ME, "%s Cannot roam to myself " SWL_MAC_FMT,
                         ep->Name, SWL_MAC_ARG(ep->tinyRoam->targetBssid.bMac));
        s_stopRoaming(ep, false, WLD_TINYROAM_ROAMRESULT_ROAM_TO_MYSELF);
        return;
    }

    s_doRoamAttempt(ep);
}

static void s_processPreviousAttemptResult(T_EndPoint* ep) {
    ASSERT_TRUE(s_goodEp(ep), , ME, "NULL");

    roamAttemptResult_e attemptResult = s_retrieveRoamAttemptResult(ep);

    ASSERT_NOT_EQUALS(attemptResult, ROAMATTEMPTRESULT_NOTROAMING, ,
                      ME, "roaming and not roaming at the same time");

    if(attemptResult == ROAMATTEMPTRESULT_SUCCESS) {
        SAH_TRACEZ_WARNING(ME, "%s Roam success", ep->Name);
        s_stopRoaming(ep, false, WLD_TINYROAM_ROAMRESULT_SUCCESS);
    } else {
        if(attemptResult == ROAMATTEMPTRESULT_NOTCONNECTED) {
            SAH_TRACEZ_ERROR(ME, "%s Attempt to roam failed", ep->Name);
        } else if(attemptResult == ROAMATTEMPTRESULT_WRONGBSSID) {
            SAH_TRACEZ_ERROR(ME, "%s Roam connected to wrong bssid", ep->Name);
        }

        if(ep->tinyRoam->numberOfAttemptsStarted < ep->tinyRoam->maxNumberOfAttempts) {
            s_doRoamAttempt(ep);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s roam max number of attempts (%" PRId32 ") reached",
                             ep->Name, ep->tinyRoam->maxNumberOfAttempts);
            s_stopRoaming(ep, true, WLD_TINYROAM_ROAMRESULT_ALL_ATTEMPTS_FAILED);
        }
    }
    SWL_ASSERT_STATIC(ROAMATTEMPTRESULT_MAX == 4, "You have to update case analysis");
}

/**
 * Inspect previous (if any) roam attempt successfulness, and start next (if any) roam attempt.
 */
static void s_roamTimerHandle(amxp_timer_t* timer _UNUSED, void* userdata) {
    T_EndPoint* ep = userdata;
    ASSERT_TRUE(s_goodEp(ep), , ME, "NULL");

    if(ep->tinyRoam->numberOfAttemptsStarted == 0) {
        s_doFirstRoamAttempt(ep);
    } else {
        s_processPreviousAttemptResult(ep);
    }
}

/**
 * Start up roaming attempts to roam to given bssid.
 *
 * Safe to run in bus callback.
 *
 * @param roamEndedCb: if arguments are valid, guaranteed to be called when roaming ends
 *   (both on success and failure).
 *   if arguments are not valid, guaranteed to not be called (for this invocation).
 * @result Returns true on valid arguments, false on invalid arguments.
 */
bool wld_tinyRoam_roamTo(T_EndPoint* ep, const swl_macBin_t* newBssid,
                         int32_t maxNumberOfAttempts, int32_t timeoutInSec, wld_tinyRoam_roamEndedCb_t roamEndedCb,
                         void* userData) {

    ASSERT_TRUE(s_goodEp(ep), false, ME, "NULL");
    ASSERT_NOT_NULL(newBssid, false, ME, "NULL");
    ASSERT_TRUE(maxNumberOfAttempts >= 1, false,
                ME, "max number of attempts must be at least 1");
    ASSERT_FALSE(swl_mac_binIsNull(newBssid), false,
                 ME, "%s Cannot roam to NULL BSSID", ep->Name);
    ASSERT_TRUE(timeoutInSec >= 5, false,
                ME, "%s timeout must be at least 5 seconds", ep->Name);

    if(wld_tinyRoam_isRoaming(ep)) {
        // Note: important because this calls the ended-callback if previous call is still ongoing.
        s_stopRoaming(ep, false, WLD_TINYROAM_ROAMRESULT_REPLACED);
    }

    *s_targetBssid(ep) = *newBssid;
    ep->tinyRoam->maxNumberOfAttempts = maxNumberOfAttempts;
    ep->tinyRoam->numberOfAttemptsStarted = 0;
    ep->tinyRoam->attemptTimeoutInSec = timeoutInSec;
    ep->tinyRoam->endedCb = roamEndedCb;
    ep->tinyRoam->endedCbUserData = userData;

    // start actual work behind timer, so we stay safe to run in bus callback.
    if(ep->tinyRoam->roamAttemptTimer != NULL) {
        amxp_timer_delete(&ep->tinyRoam->roamAttemptTimer);
    }
    amxp_timer_new(&ep->tinyRoam->roamAttemptTimer, s_roamTimerHandle, ep);
    amxp_timer_start(ep->tinyRoam->roamAttemptTimer, 0);

    return true;
}

static void s_epStateUpdateCb(wld_ep_status_change_event_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_EndPoint* ep = event->ep;
    ASSERT_NOT_NULL(ep, , ME, "NULL");

    ASSERTS_TRUE(event->oldConnectionStatus != EPCS_CONNECTED
                 && ep->connectionStatus == EPCS_CONNECTED, ,
                 ME, "Only interested in when connection comes up");

    ASSERTS_TRUE(wld_tinyRoam_isRoaming(ep), , ME, "Not roaming");

    s_processPreviousAttemptResult(ep);
}

static wld_event_callback_t s_epStateUpdateCbContainer = {
    .callback = (wld_event_callback_fun) s_epStateUpdateCb
};

void wld_tinyRoam_init(T_EndPoint* ep) {
    ASSERT_NOT_NULL(ep, , ME, "NULL");
    ASSERT_NULL(ep->tinyRoam, , ME, "Already initialized");

    ep->tinyRoam = calloc(1, sizeof(wld_tinyRoam_t));

    // This already takes care of refcounting.
    wld_event_add_callback(gWld_queue_ep_onStatusChange, &s_epStateUpdateCbContainer);
}

void wld_tinyRoam_cleanup(T_EndPoint* ep) {
    ASSERT_NOT_NULL(ep, , ME, "NULL");
    ASSERT_NOT_NULL(ep->tinyRoam, , ME, "Already cleaned up / never initialized");
    amxp_timer_delete(&ep->tinyRoam->roamAttemptTimer);
    ep->tinyRoam->roamAttemptTimer = NULL;
    free(ep->tinyRoam);
    ep->tinyRoam = NULL;

    // This already takes care of refcounting.
    wld_event_remove_callback(gWld_queue_ep_onStatusChange, &s_epStateUpdateCbContainer);
}
