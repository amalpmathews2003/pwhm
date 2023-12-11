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


#ifndef INCLUDE_WLD_WLD_EVENTING_H_
#define INCLUDE_WLD_WLD_EVENTING_H_



#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_parameter.h>
#include <amxd/amxd_action.h>
#include <amxd/amxd_object_parameter.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_function.h>

typedef enum {
    WLD_LIFECYCLE_EVENT_INIT_DONE,
    WLD_LIFECYCLE_EVENT_DATAMODEL_LOADED,
    WLD_LIFECYCLE_EVENT_CLEANUP_START,
} wld_lifecycleEvent_e;

typedef struct {
    wld_lifecycleEvent_e event;
} wld_lifecycleEvent_t;


typedef void (* wld_event_callback_fun)(const void* data);

typedef struct {
    uint32_t refCount; //count for nr of registrations.
    amxc_llist_it_t it;
    wld_event_callback_fun callback;
} wld_event_callback_t;

typedef struct {
    amxc_llist_t subscribers;
    char* name;
} wld_event_queue_t;

extern wld_event_queue_t* gWld_queue_ep_onStatusChange;

extern wld_event_queue_t* gWld_queue_vap_onStatusChange;
extern wld_event_queue_t* gWld_queue_vap_onChangeEvent;
extern wld_event_queue_t* gWld_queue_vap_onAction;

extern wld_event_queue_t* gWld_queue_rad_onStatusChange;
extern wld_event_queue_t* gWld_queue_rad_onScan_change;
extern wld_event_queue_t* gWld_queue_rad_onChangeEvent;
extern wld_event_queue_t* gWld_queue_rad_onFrameEvent;

extern wld_event_queue_t* gWld_queue_sta_onChangeEvent;

extern wld_event_queue_t* gWld_queue_lifecycleEvent;



void wld_event_add_callback(wld_event_queue_t* queue, wld_event_callback_t* callback);
void wld_event_remove_callback(wld_event_queue_t* queue, wld_event_callback_t* callback);
void wld_event_trigger_callback(wld_event_queue_t* queue, const void* data);


void wld_event_init();
void wld_event_destroy();

#endif /* INCLUDE_WLD_WLD_EVENTING_H_ */
