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

#include <swl/swl_common.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_util.h"

#define ME "wld_acm"

typedef struct {
    bool enable;
    uint32_t delay;
    uint32_t bootDelay;
} wld_autoCommitMgr_t;

static wld_autoCommitMgr_t s_mgr;

void s_startRadioAutoCommit(T_Radio* pRad) {
    swl_timeSpecMono_t time;
    swl_timespec_getMono(&time);
    pRad->autoCommitData.lastCommitCallTime = time;

    ASSERTI_TRUE(s_mgr.enable, , ME, "Disabled");
    ASSERT_NOT_NULL(pRad->autoCommitData.timer, , ME, "NULL");
    amxp_timer_state_t state = pRad->autoCommitData.timer->state;

    ASSERTI_TRUE(state != amxp_timer_running && state != amxp_timer_started, , ME, "%s: timer started", pRad->Name);
    ASSERTI_TRUE(pRad->fsmRad.FSM_ComPend == 0, , ME, "%s: FSM commit pending (cnt: %d)", pRad->Name, pRad->fsmRad.FSM_ComPend);


    swl_timeSpecMono_t* initTime = wld_getInitTime();
    int64_t msDiff = swl_timespec_diffToNanosec(initTime, &time) / 1000000;
    uint32_t minDelay = 0;
    if((msDiff >= 0) && (msDiff < s_mgr.bootDelay)) {
        minDelay = s_mgr.bootDelay - (uint32_t) msDiff;
    }
    uint32_t finalDelay = SWL_MAX(s_mgr.delay, minDelay);
    SAH_TRACEZ_INFO(ME, "%s: delay %u (%u / %u), time (%s / %s / %s)", pRad->Name, finalDelay, s_mgr.delay, minDelay,
                    swl_typeTimeSpecMono_toBuf32(time).buf, swl_typeTimeSpecMono_toBuf32(*initTime).buf, swl_typeInt64_toBuf32(msDiff).buf);

    amxp_timer_start(pRad->autoCommitData.timer, finalDelay);
}

/**
 * Notify rad FSM action was taken.
 */
void wld_autoCommitMgr_notifyRadEdit(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    bool anyChange = pRad->fsmRad.FSM_SyncAll || areBitsSetLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW);
    ASSERTI_TRUE(anyChange, , ME, "%s: not pending", pRad->Name);
    s_startRadioAutoCommit(pRad);
}

static void s_autoCommit_th(amxp_timer_t* timer _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_rad_doCommitIfUnblocked(pRad);
}


static void s_radInit(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NULL(pRad->autoCommitData.timer, , ME, "NOT NULL");
    amxp_timer_new(&pRad->autoCommitData.timer, s_autoCommit_th, pRad);
}

static void s_radDestroy(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->autoCommitData.timer, , ME, "NULL");
    amxp_timer_delete(&pRad->autoCommitData.timer);
    pRad->autoCommitData.timer = NULL;
}

/**
 * Notify vap FSM action was taken.
 */
void wld_autoCommitMgr_notifyVapEdit(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    bool anyChange = pAP->fsm.FSM_SyncAll || areBitsSetLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW);
    ASSERTI_TRUE(anyChange, , ME, "%s: not pending", pAP->alias);
    s_startRadioAutoCommit(pAP->pRadio);
}

/**
 * Notify ep FSM action was taken.
 */
void wld_autoCommitMgr_notifyEpEdit(T_EndPoint* pEp) {
    ASSERT_NOT_NULL(pEp, , ME, "NULL");
    bool anyChange = pEp->fsm.FSM_SyncAll || areBitsSetLongArray(pEp->fsm.FSM_BitActionArray, FSM_BW);
    ASSERTI_TRUE(anyChange, , ME, "%s: not pending", pEp->alias);
    s_startRadioAutoCommit(pEp->pRadio);
}

/**
 * Initialize autoCommit on radio create
 */
void wld_autoCommitMgr_init(T_Radio* pRad) {
    if(s_mgr.enable) {
        s_radInit(pRad);
    }
}

/**
 * Destroy autoCommit on radio destroy
 */
void wld_autoCommitMgr_destroy(T_Radio* pRad) {
    if(s_mgr.enable) {
        s_radDestroy(pRad);
    }

}

static void s_initAutoCommit() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        s_radInit(pRad);
        //stop potentially started timer to allow reconfiguring delay
        amxp_timer_stop(pRad->autoCommitData.timer);
        wld_autoCommitMgr_notifyRadEdit(pRad);
    }
}

static void s_cleanAutoCommit() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        s_radDestroy(pRad);
    }
}

static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    bool newEnable = amxc_var_dyncast(bool, newValue);
    ASSERTI_NOT_EQUALS(newEnable, s_mgr.enable, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "Update enable %u to %u", s_mgr.enable, newEnable);
    s_mgr.enable = newEnable;

    if(newEnable) {
        s_initAutoCommit();
    } else {
        s_cleanAutoCommit();
    }
}

static void s_setDelayTime_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    uint32_t newDelay = amxc_var_dyncast(uint32_t, newValue);
    ASSERTI_NOT_EQUALS(newDelay, s_mgr.delay, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "Update delay %u to %u", s_mgr.delay, newDelay);
    s_mgr.delay = newDelay;
    if(s_mgr.enable) {
        s_initAutoCommit();
    }
}

static void s_setBootDelayTime_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    uint32_t newDelay = amxc_var_dyncast(uint32_t, newValue);
    ASSERTI_NOT_EQUALS(newDelay, s_mgr.bootDelay, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "Update BootDelay %u to %u", s_mgr.bootDelay, newDelay);
    s_mgr.bootDelay = newDelay;
    if(s_mgr.enable) {
        s_initAutoCommit();
    }
}

SWLA_DM_HDLRS(sAutoCommitMgrDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("DelayTime", s_setDelayTime_pwf),
                  SWLA_DM_PARAM_HDLR("BootDelayTime", s_setBootDelayTime_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf)));

void _wld_autoCommitMgr_setConf_ocf(const char* const sig_name,
                                    const amxc_var_t* const data,
                                    void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAutoCommitMgrDmHdlrs, sig_name, data, priv);
}

amxd_status_t _AutoCommitMgr_debug(amxd_object_t* obj _UNUSED,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retMap) {

    const char* feature = GET_CHAR(args, "op");
    amxc_var_init(retMap);
    amxc_var_set_type(retMap, AMXC_VAR_ID_HTABLE);

    if(swl_str_isEmpty(feature)) {
        amxc_var_add_key(bool, retMap, "Enable", s_mgr.enable);
        amxc_var_add_key(uint32_t, retMap, "Delay", s_mgr.delay);
        amxc_var_add_key(uint32_t, retMap, "BootDelay", s_mgr.bootDelay);
        amxc_var_t* myList = amxc_var_add_key(amxc_llist_t, retMap, "RadioInfo", NULL);
        T_Radio* pRad;
        wld_for_eachRad(pRad) {
            amxc_var_t* myMap = amxc_var_add(amxc_htable_t, myList, NULL);
            amxc_var_add_key(cstring_t, myMap, "RadioName", pRad->Name);
            amxc_var_add_key(bool, myMap, "HasTimer", pRad->autoCommitData.timer != NULL);
            amxc_var_add_key(uint32_t, myMap, "Timer", amxp_timer_remaining_time(pRad->autoCommitData.timer));
        }
    } else {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Feature unknown -%s-", feature);
        amxc_var_add_key(cstring_t, retMap, "Error", buffer);
    }

    return amxd_status_ok;
}

