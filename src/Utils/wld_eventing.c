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


#include <debug/sahtrace.h>
#include "wld_eventing.h"
#include "swl/swl_assert.h"

#define ME "utilEvt"

/**
 * Eventing
 *
 * This file provides some event queues for the easy registration of different modules to events in the system.
 * Current supported events are
 * * RAD change event
 */

/**
 * The queue of callbacks after an rad gets changed
 */
static wld_event_queue_t rqueue_rad_onStatus_change = {.name = "evRadStatus"};
static wld_event_queue_t rqueue_ep_onStatus_change = {.name = "evEpStatus"};
static wld_event_queue_t rqueue_vap_onStatus_change = {.name = "evVapStatus"};
static wld_event_queue_t rqueue_rad_onScan_change = {.name = "evScan"};
static wld_event_queue_t rqueue_rad_onChange = {.name = "evRadChange"};
static wld_event_queue_t rqueue_rad_onFrame = {.name = "evRadFrame"};

/**
 * General lifecycle changes of AP. Does not include status changes => rqueue_vap_onStatus_change
 */
static wld_event_queue_t rqueue_vap_onChange = {.name = "evApChange"};
static wld_event_queue_t rqueue_vap_onAction = {.name = "evApAction"};
static wld_event_queue_t rqueue_ep_onChange = {.name = "evEpChange"};
static wld_event_queue_t rqueue_sta_onChange = {.name = "evStaChange"};
static wld_event_queue_t rqueue_lifecycleEvent = {.name = "evLifecycle"};

wld_event_queue_t* gWld_queue_rad_onStatusChange = NULL;
wld_event_queue_t* gWld_queue_ep_onStatusChange = NULL;
wld_event_queue_t* gWld_queue_vap_onStatusChange = NULL; // Called when the status of the vap changes. @type wld_vap_status_change_event_t
wld_event_queue_t* gWld_queue_vap_onChangeEvent = NULL;  // Called when vap structural or config changes, i.e. create / destroy. @type wld_vap_changeEvent_t
wld_event_queue_t* gWld_queue_vap_onAction = NULL;       // General event queue, for vap actions, e.g. rssi eventing sample cation, sta kick, sta steer
wld_event_queue_t* gWld_queue_ep_onChangeEvent = NULL;   // Called when ep structural or config changes, i.e. create / destroy. @type wld_ep_changeEvent_t
wld_event_queue_t* gWld_queue_rad_onScan_change = NULL;
wld_event_queue_t* gWld_queue_rad_onChangeEvent = NULL;
wld_event_queue_t* gWld_queue_rad_onFrameEvent = NULL;

wld_event_queue_t* gWld_queue_sta_onChangeEvent = NULL; // Called on station lifecycle changes. @type wld_ad_changeEvent_t.


wld_event_queue_t* gWld_queue_lifecycleEvent = NULL;

/**
 * Initialize the eventing module
 * Create all queues, and set the pointers correctly
 */
void wld_event_init() {
    SAH_TRACEZ_INFO(ME, "Init eventing");
    gWld_queue_rad_onStatusChange = &rqueue_rad_onStatus_change;
    amxc_llist_init(&gWld_queue_rad_onStatusChange->subscribers);

    gWld_queue_ep_onStatusChange = &rqueue_ep_onStatus_change;
    amxc_llist_init(&gWld_queue_ep_onStatusChange->subscribers);

    gWld_queue_vap_onStatusChange = &rqueue_vap_onStatus_change;
    amxc_llist_init(&gWld_queue_vap_onStatusChange->subscribers);

    gWld_queue_vap_onAction = &rqueue_vap_onAction;
    amxc_llist_init(&gWld_queue_vap_onAction->subscribers);

    gWld_queue_rad_onScan_change = &rqueue_rad_onScan_change;
    amxc_llist_init(&gWld_queue_rad_onScan_change->subscribers);

    gWld_queue_rad_onChangeEvent = &rqueue_rad_onChange;
    amxc_llist_init(&gWld_queue_rad_onChangeEvent->subscribers);

    gWld_queue_rad_onFrameEvent = &rqueue_rad_onFrame;
    amxc_llist_init(&gWld_queue_rad_onFrameEvent->subscribers);

    gWld_queue_lifecycleEvent = &rqueue_lifecycleEvent;
    amxc_llist_init(&gWld_queue_lifecycleEvent->subscribers);

    gWld_queue_vap_onChangeEvent = &rqueue_vap_onChange;
    amxc_llist_init(&gWld_queue_vap_onChangeEvent->subscribers);

    gWld_queue_ep_onChangeEvent = &rqueue_ep_onChange;
    amxc_llist_init(&gWld_queue_ep_onChangeEvent->subscribers);

    gWld_queue_sta_onChangeEvent = &rqueue_sta_onChange;
    amxc_llist_init(&gWld_queue_sta_onChangeEvent->subscribers);
}

/**
 * Cleanup the given queue
 */
static void wld_event_cleanup_queue(wld_event_queue_t* queue) {
    ASSERT_NOT_NULL(queue, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Cleanup %s", queue->name);
    amxc_llist_it_t* it = amxc_llist_take_first(&queue->subscribers);
    while(it) {
        it = amxc_llist_take_first(&queue->subscribers);
    }
    amxc_llist_clean(&queue->subscribers, NULL);
}

/**
 * Destroy the different event queues
 * Note that we DO NOT RELEASE the iterators, we just unlink them
 */
void wld_event_destroy() {
    SAH_TRACEZ_INFO(ME, "Destroy eventing");
    wld_event_cleanup_queue(gWld_queue_rad_onStatusChange);
    wld_event_cleanup_queue(gWld_queue_ep_onStatusChange);
    wld_event_cleanup_queue(gWld_queue_vap_onStatusChange);
    wld_event_cleanup_queue(gWld_queue_vap_onChangeEvent);
    wld_event_cleanup_queue(gWld_queue_ep_onChangeEvent);
    wld_event_cleanup_queue(gWld_queue_rad_onScan_change);
    wld_event_cleanup_queue(gWld_queue_lifecycleEvent);

    wld_event_cleanup_queue(gWld_queue_sta_onChangeEvent);
}

/**
 * Add a callback to the given queue
 */
void wld_event_add_callback(wld_event_queue_t* queue, wld_event_callback_t* callback) {
    ASSERT_NOT_NULL(queue, , ME, "NULL");
    ASSERT_NOT_NULL(callback, , ME, "NULL");
    ASSERT_NOT_NULL(callback->callback, , ME, "NULL");

    ASSERT_FALSE(callback->it.llist != NULL && callback->it.llist != &queue->subscribers, ,
                 ME, "Callback is already in another queue");

    if(callback->refCount == 0) {
        amxc_llist_append(&queue->subscribers, &callback->it);
    }

    callback->refCount += 1;
    SAH_TRACEZ_INFO(ME, "Add callback to queue %s %zu => %u %p",
                    queue->name, amxc_llist_size(&queue->subscribers), callback->refCount, callback);
}

/**
 * Remove the callback from the given queue
 */
void wld_event_remove_callback(wld_event_queue_t* queue, wld_event_callback_t* callback) {
    ASSERT_NOT_NULL(queue, , ME, "NULL");
    ASSERT_NOT_NULL(callback, , ME, "NULL");

    amxc_llist_it_t* iter = &callback->it;
    if((iter->llist != NULL) && (iter->llist != &queue->subscribers)) {
        SAH_TRACEZ_ERROR(ME, "removing callback from invalid queue %s %p", queue->name, callback);
    } else {
        SAH_TRACEZ_INFO(ME, "Remove callback for queue %s %zu => %u %p",
                        queue->name, amxc_llist_size(&queue->subscribers), callback->refCount, callback);

        callback->refCount -= 1;
        if(callback->refCount == 0) {
            amxc_llist_it_take(&callback->it);
        }

    }
}

/**
 * Notify all callbacks with the given data structure
 * @param queue
 *  The queue to which the trigger has to be sent
 * @param data
 *  The data that has to be delivered to each queue
 *
 *  Note that we do not isolate the data, so in theory each callback can manipulate the data field
 *  Please refrain from doing so.
 */
void wld_event_trigger_callback(wld_event_queue_t* queue, const void* data) {
    ASSERT_NOT_NULL(queue, , ME, "NULL");

    wld_event_callback_t* callback;
    SAH_TRACEZ_INFO(ME, "Trigger callback to queue %s %zu", queue->name, amxc_llist_size(&queue->subscribers));
    amxc_llist_for_each(it, &queue->subscribers) {
        callback = amxc_llist_it_get_data(it, wld_event_callback_t, it);

        SAH_TRACEZ_INFO(ME, " * trigger %p", callback);
        callback->callback(data);
    }

}

