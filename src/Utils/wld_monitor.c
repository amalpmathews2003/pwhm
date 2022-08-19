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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>


#include <net/if.h>
#include <sys/ioctl.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "wld_monitor.h"

#define ME "utilMon"

void wld_mon_sendUpdateNotification(amxd_object_t* eventObject, const char* name, amxc_var_t* variant) {
    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(amxc_htable_t, &map, "Updates", amxc_var_get_amxc_htable_t(variant));
    amxd_object_trigger_signal(eventObject, name, &map);
    amxc_var_clean(&map);
}

static void timeHandler(amxp_timer_t* timer, void* data) {
    (void) timer;
    T_Monitor* pMon = (T_Monitor*) data;
    pMon->callback_fn(pMon->userData);
}

static void startMonitor(T_Monitor* pMon) {
    ASSERT_NOT_NULL(pMon, , ME, "NULL");
    ASSERT_NOT_NULL(pMon->timer, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "Start mon %s", pMon->name);

    ASSERT_NOT_EQUALS(pMon->interval, 0, , ME, "Zero");

    amxp_timer_set_interval(pMon->timer, pMon->interval);
    amxp_timer_start(pMon->timer, pMon->interval);

}

static void stopMonitor(T_Monitor* pMon) {
    SAH_TRACEZ_INFO(ME, "Stop mon %s", pMon->name);
    amxp_timer_stop(pMon->timer);
}

static void wld_mon_updateEnabled(T_Monitor* pMon) {
    bool targetRunning = (pMon->active && pMon->enabled);
    if(targetRunning != pMon->running) {
        pMon->running = targetRunning;
        if(targetRunning) {
            startMonitor(pMon);
        } else {
            stopMonitor(pMon);
        }
    }
}

amxd_status_t _mon_enableWriteHandler(amxd_object_t* object _UNUSED,
                                      amxd_param_t* parameter,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* topObject = amxd_param_get_owner(parameter);
    ASSERTS_NOT_NULL(topObject, amxd_status_unknown_error, ME, "NULL");
    ASSERTS_FALSE(amxd_object_get_type(topObject) == amxd_object_template, amxd_status_unknown_error, ME, "Initial template run, skip");
    T_Monitor* pMon = (T_Monitor*) topObject->priv;
    ASSERTS_NOT_NULL(pMon, amxd_status_unknown_error, ME, "NULL");
    rv = amxd_action_param_write(topObject, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    pMon->enabled = amxc_var_dyncast(bool, args);

    SAH_TRACEZ_INFO(ME, "Update enable %s %u", pMon->name, pMon->enabled);

    wld_mon_updateEnabled(pMon);
    return amxd_status_ok;
}

amxd_status_t _mon_intervalWriteHandler(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args _UNUSED,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* topObject = amxd_param_get_owner(parameter);
    ASSERTS_NOT_NULL(topObject, amxd_status_unknown_error, ME, "NULL");
    ASSERTS_FALSE(amxd_object_get_type(topObject) == amxd_object_template, amxd_status_unknown_error, ME, "Initial template run, skip");
    T_Monitor* pMon = (T_Monitor*) topObject->priv;
    ASSERTS_NOT_NULL(pMon, amxd_status_unknown_error, ME, "NULL");
    rv = amxd_action_param_write(topObject, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    pMon->interval = amxc_var_dyncast(uint32_t, args);

    SAH_TRACEZ_INFO(ME, "Update interval %s %u", pMon->name, pMon->interval);

    if(pMon->running) {
        amxp_timer_set_interval(pMon->timer, pMon->interval);
    }
    return amxd_status_ok;
}

void wld_mon_initMon(T_Monitor* pMon, amxd_object_t* object, char* name, void* userData, void (* callback_fn)(void* userData)) {
    ASSERT_NOT_NULL(pMon, , ME, "NULL");
    ASSERT_NOT_NULL(object, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "Init mon %s", name);

    object->priv = pMon;
    pMon->object = object;

    amxc_var_t value;
    amxc_var_init(&value);

    amxd_param_t* enable_param = amxd_object_get_param_def(object, "Enable");
    amxd_param_get_value(enable_param, &value);
    pMon->enabled = amxc_var_get_bool(&value);

    amxd_param_t* interval_param = amxd_object_get_param_def(object, "Interval");
    amxd_param_get_value(interval_param, &value);
    pMon->interval = amxc_var_get_uint32_t(&value);

    amxp_timer_new(&pMon->timer, timeHandler, pMon);
    pMon->name = strdup(name);
    pMon->userData = userData;
    pMon->callback_fn = callback_fn;
    amxc_var_clean(&value);
}

void wld_mon_destroyMon(T_Monitor* pMon) {
    SAH_TRACEZ_INFO(ME, "%s: destroy mon", pMon->name);

    pMon->object->priv = NULL;
    amxp_timer_delete(&pMon->timer);
    free(pMon->name);
    pMon->name = NULL;
    pMon->userData = NULL;
    pMon->callback_fn = NULL;
}

void wld_mon_writeActive(T_Monitor* pMon, bool active) {
    pMon->active = active;
    wld_mon_updateEnabled(pMon);
}


void wld_mon_debug(T_Monitor* pMon, amxc_var_t* retMap) {

    amxc_var_add_key(bool, retMap, "Enabled", pMon->enabled);
    amxc_var_add_key(bool, retMap, "Active", pMon->active);
    amxc_var_add_key(bool, retMap, "Running", pMon->running);
    amxc_var_add_key(uint32_t, retMap, "Interval", pMon->interval);

    if(pMon->timer != NULL) {
        amxc_var_add_key(int32_t, retMap, "TimerState", amxp_timer_get_state(pMon->timer));
        amxc_var_add_key(uint32_t, retMap, "TimeRemaining", amxp_timer_remaining_time(pMon->timer));
    } else {
        amxc_var_add_key(int32_t, retMap, "TimerState", -1);
    }
}

void wld_mon_stop(T_Monitor* pMon) {
    amxp_timer_stop(pMon->timer);
}

