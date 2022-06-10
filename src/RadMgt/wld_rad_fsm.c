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
#include "swl/swl_common.h"
#include "wld.h"
#include "wld_fsm.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "debug/sahtrace.h"

#define ME "wldFsm"

#define RADIO_LOCK          0x0100
#define RADIO_INDEX_MASK    0x000F

static int s_radioFSMLock = 0;
static int s_radioFSMWaiting = 0;


static wld_fsmMngr_t* s_getMngr(T_Radio* rad) {
    return rad->vendor->fsmMngr;
}

static void s_printFsmBits(T_Radio* rad) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, rad) {
        if(areBitsSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW)) {
            SAH_TRACEZ_WARNING(ME, "FSM VAP %8s: %u 0x%08lx 0x%08lx // 0x%08lx 0x%08lx",
                               pAP->alias,
                               pAP->enable,
                               pAP->fsm.FSM_BitActionArray[0], pAP->fsm.FSM_BitActionArray[1],
                               pAP->fsm.FSM_AC_BitActionArray[0], pAP->fsm.FSM_AC_BitActionArray[1]);
        }
    }

    T_EndPoint* pEP = NULL;
    wld_rad_forEachEp(pEP, rad) {
        if(areBitsSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW)) {
            SAH_TRACEZ_WARNING(ME, "FSM  EP %8s: %u 0x%08lx 0x%08lx // 0x%08lx 0x%08lx",
                               pEP->alias,
                               pEP->enable,
                               pEP->fsm.FSM_BitActionArray[0], pEP->fsm.FSM_BitActionArray[1],
                               pEP->fsm.FSM_AC_BitActionArray[0], pEP->fsm.FSM_AC_BitActionArray[1]);
        }
    }

    SAH_TRACEZ_WARNING(ME, "FSM RAD %8s: %u 0x%08lx 0x%08lx // 0x%08lx 0x%08lx // 0x%08lx 0x%08lx",
                       rad->Name,
                       rad->enable,
                       rad->fsmRad.FSM_BitActionArray[0], rad->fsmRad.FSM_BitActionArray[1],
                       rad->fsmRad.FSM_AC_BitActionArray[0], rad->fsmRad.FSM_AC_BitActionArray[1],
                       rad->fsmRad.FSM_CSC[0], rad->fsmRad.FSM_CSC[1]);

}

static void s_cleanFsmBits(T_Radio* rad) {
    longArrayClean(rad->fsmRad.FSM_BitActionArray, FSM_BW);
    amxc_llist_it_t* it;

    amxc_llist_for_each(it, &rad->llAP) {
        T_AccessPoint* pAP = amxc_llist_it_get_data(it, T_AccessPoint, it);
        longArrayClean(pAP->fsm.FSM_BitActionArray, FSM_BW);
    }

    amxc_llist_for_each(it, &rad->llEndPoints) {
        T_EndPoint* pEP = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);
        longArrayClean(pEP->fsm.FSM_BitActionArray, FSM_BW);
    }
}

/**
 * This function shall update the radio FSM_CSC bit array. It shall set it
 * to the OR of the Radio FSM_BA, VAP FSM_BA and Endpoint FSM_BA. If there are then
 * any bits in the FSM_CSC, we should do a commit. If not, no commit is necessary, since there is not
 * a single instance requiring any FSM change.
 */
static void s_collectDependencies(T_Radio* rad) {
    T_AccessPoint* pAP;
    T_EndPoint* pEP;

    //Initialize CSC cache to radio bits
    longArrayCopy(rad->fsmRad.FSM_CSC, rad->fsmRad.FSM_BitActionArray, FSM_BW);

    /* collect all dependencies on all attached vap interfaces, mirror it on the RAD interface */
    wld_rad_forEachAp(pAP, rad) {
        if(rad->fsmRad.FSM_SyncAll || pAP->fsm.FSM_SyncAll) {
            /* This is the correct place todo this.
               During dependency check some overlapping states can be cleared */
            markAllBitsLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, s_getMngr(rad)->nrFsmBits);
            pAP->fsm.FSM_SyncAll = FALSE;
        }

        longArrayBitOr(rad->fsmRad.FSM_CSC, pAP->fsm.FSM_BitActionArray, FSM_BW);
    }

    wld_rad_forEachEp(pEP, rad) {
        if(rad->fsmRad.FSM_SyncAll || pEP->fsm.FSM_SyncAll) {
            /* This is the correct place todo this.
               During dependency check some overlapping states can be cleared */
            markAllBitsLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, s_getMngr(rad)->nrFsmBits);
            pEP->fsm.FSM_SyncAll = FALSE;
        }
        longArrayBitOr(rad->fsmRad.FSM_CSC, pEP->fsm.FSM_BitActionArray, FSM_BW);
    }

    rad->fsmRad.FSM_SyncAll = FALSE;
}

static bool s_checkCommitPending(T_Radio* rad, FSM_STATE targetState) {

    s_collectDependencies(rad);
    if(rad->fsmRad.FSM_ComPend && areBitsSetLongArray(rad->fsmRad.FSM_CSC, FSM_BW)) {
        /* We've a extra COMMIT pending! */
        SAH_TRACEZ_WARNING(ME, "%s: restarting FSM commit", rad->Name);
        rad->fsmRad.FSM_State = targetState;
        return true;
    } else if(rad->fsmRad.FSM_ComPend) {
        SAH_TRACEZ_WARNING(ME, "%s: Commit pending without bits, ignore", rad->Name);
        rad->fsmRad.FSM_ComPend = 0;
    } else if(areBitsSetLongArray(rad->fsmRad.FSM_CSC, FSM_BW)) {
        SAH_TRACEZ_WARNING(ME, "%s: Bits pending 0x%08lx // 0x%08lx, ignore since no commit",
                           rad->Name,
                           rad->fsmRad.FSM_CSC[0], rad->fsmRad.FSM_CSC[1]);
    } else {
        SAH_TRACEZ_INFO(ME, "%s check ok", rad->Name);
    }
    return false;
}

/**
 * Let fsm try to get the lock
 * Return true if has lock
 * Return false if doesn't have lock. Will put radio in waiting list.
 */
bool s_tryGetLock(T_Radio* rad) {
    int bitmask = (1 << rad->ref_index);
    if((s_radioFSMLock & RADIO_INDEX_MASK) == bitmask) {
        SAH_TRACEZ_ERROR(ME, "%s: requesting lock while has lock 0x%x", rad->Name, s_radioFSMLock);
        return true;
    }

    if(s_radioFSMLock) {
        s_radioFSMWaiting |= bitmask;
        return false;
    } else {
        s_radioFSMLock = RADIO_LOCK | bitmask;
        s_radioFSMWaiting &= ~(bitmask);
        return true;
    }
}

/**
 * Free the fsm lock.
 * Return true if you're the last one, i.e. no others are waiting.
 * Return false if others are waiting.
 */
void s_freeLock(T_Radio* rad) {
    int bitmask = (1 << rad->ref_index);
    if((s_radioFSMLock & RADIO_INDEX_MASK) != bitmask) {
        SAH_TRACEZ_ERROR(ME, "%s: freeing lock while not has lock %u", rad->Name, s_radioFSMLock);
        return;
    }

    s_radioFSMLock = 0;
}

bool s_areAnyWaiting() {
    return s_radioFSMWaiting == 0;
}

void s_ensureHasLock(T_Radio* rad) {
    int bitmask = (1 << rad->ref_index);
    if((s_radioFSMLock & RADIO_INDEX_MASK) != bitmask) {
        SAH_TRACEZ_ERROR(ME, "%s: Checking lock while not has lock %u", rad->Name, s_radioFSMLock);
    }
}


static void s_performSync(T_Radio* rad) {
    T_AccessPoint* pAP;
    T_EndPoint* pEP;
    T_SSID* pSSID;
    // Entry point to actually start configuring.
    // dependency, rad and vap should follow after.
    s_ensureHasLock(rad);
    rad->fsmRad.FSM_ComPend = 0;    // Clear PENDING marker!

    /* We're passing here if we've a pending commit */
    wld_rad_forEachAp(pAP, rad) {
        if(rad->fsmRad.FSM_SyncAll || pAP->fsm.FSM_SyncAll) {
            markAllBitsLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, s_getMngr(rad)->nrFsmBits);
            pAP->fsm.FSM_SyncAll = FALSE;
        }
    }

    wld_rad_forEachEp(pEP, rad) {
        if(rad->fsmRad.FSM_SyncAll || pEP->fsm.FSM_SyncAll) {
            markAllBitsLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, s_getMngr(rad)->nrFsmBits);
            pEP->fsm.FSM_SyncAll = false;
        }
    }

    rad->fsmRad.FSM_SyncAll = FALSE;

    rad->pFA->mfn_sync_radio(rad->pBus, rad, GET);

    wld_rad_forEachAp(pAP, rad) {
        pSSID = (T_SSID*) pAP->pSSID;
        rad->pFA->mfn_sync_ap(pAP->pBus, pAP, GET);
        rad->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, GET);
    }
}


static void s_syncVap(T_Radio* rad) {
    T_AccessPoint* pAP;

    /* collect all dependencies on all attached vap interfaces, mirror it on the RAD interface */
    wld_rad_forEachAp(pAP, rad) {
        longArrayCopy(pAP->fsm.FSM_AC_BitActionArray, pAP->fsm.FSM_BitActionArray, FSM_BW);
        longArrayBitOr(pAP->fsm.FSM_AC_BitActionArray, rad->fsmRad.FSM_BitActionArray, FSM_BW);
        SWL_CALL(s_getMngr(rad)->checkVapDependency, pAP, rad);
        longArrayBitOr(rad->fsmRad.FSM_AC_BitActionArray, pAP->fsm.FSM_AC_BitActionArray, FSM_BW);
    }
}

static void s_syncEp(T_Radio* rad) {
    T_EndPoint* pEP;
    /* collect all dependencies on all attached ep interfaces, mirror it on the RAD interface */
    wld_rad_forEachEp(pEP, rad) {
        longArrayCopy(pEP->fsm.FSM_AC_BitActionArray, pEP->fsm.FSM_BitActionArray, FSM_BW);
        longArrayBitOr(pEP->fsm.FSM_AC_BitActionArray, rad->fsmRad.FSM_BitActionArray, FSM_BW);
        SWL_CALL(s_getMngr(rad)->checkEpDependency, pEP, rad);
        longArrayBitOr(rad->fsmRad.FSM_AC_BitActionArray, pEP->fsm.FSM_AC_BitActionArray, FSM_BW);
    }
}

static void s_fsmRun_thf(amxp_timer_t* timer _UNUSED, void* userdata) {
    SAH_TRACEZ_IN(ME);
    T_Radio* rad = (T_Radio*) userdata;
    wld_rad_fsm(rad);

    if(rad->fsmRad.timer && (rad->fsmRad.timeout_msec > 0)) {
        SAH_TRACEZ_INFO(ME, "timeout %s %d - %p",
                        rad->Name,
                        rad->fsmRad.timeout_msec,
                        rad->fsmRad.timer);
        amxp_timer_start(rad->fsmRad.timer, rad->fsmRad.timeout_msec);
    }
    SAH_TRACEZ_OUT(ME);
}

int wld_rad_fsm(T_Radio* rad) {
    T_AccessPoint* pAP;
    T_EndPoint* pEP;
    int delay;

    /* State machine is active but some task take more time before we can continue */
    SAH_TRACEZ_INFO(ME, "%s: run fsm %d", rad->Name, rad->fsmRad.FSM_State);
    switch(rad->fsmRad.FSM_State) {
    case FSM_IDLE:
        s_collectDependencies(rad);

        /* Is there an action bit set ? */
        if(areBitsSetLongArray(rad->fsmRad.FSM_CSC, FSM_BW)) {
            /* Yes, create a timer... if needed so we can shedule the task */
            if(!rad->fsmRad.timer) {
                if((amxp_timer_new(&rad->fsmRad.timer, s_fsmRun_thf, rad))) {
                    SAH_TRACEZ_INFO(ME, "NEW TIMER CREATED for FSM %s %p", rad->Name, rad->fsmRad.timer);
                }
            }
            if(rad->fsmRad.timer) {
                /* In case we don't have the lock keep trying...(if timer runs) */
                rad->fsmRad.timeout_msec = 1000;     // Go for it ASAP!
                SAH_TRACEZ_INFO(ME, "amxp_timer_start %s %p %d", rad->Name, rad->fsmRad.timer, rad->fsmRad.timeout_msec);
                /* We've created our timer - check if we can go to WAIT State? */
                if((rad->fsm_radio_st == FSM_IDLE) &&
                   (0 == amxp_timer_start(rad->fsmRad.timer, rad->fsmRad.timeout_msec))) {
                    SAH_TRACEZ_WARNING(ME, "Starting FSM commit %s", rad->Name);
                    rad->fsm_radio_st = FSM_RUN;              // Lock the RADIO for our FSM
                    rad->fsmRad.FSM_State = FSM_WAIT;
                    rad->fsmRad.FSM_Retry = WLD_FSM_MAX_WAIT; // When failing, try 2 extra rounds ?
                    rad->fsmRad.FSM_Loop = 0;                 // Be sure this is resetted!
                }
            }
        }
        break;

    case FSM_WAIT:
        if(wld_rad_hasWpsActiveEndpoint(rad)) {
            if(rad->fsmRad.FSM_Retry > 0) {
                SAH_TRACEZ_WARNING(ME, "Delay commit %s %i / %i", rad->Name, rad->fsmRad.FSM_Retry, WLD_FSM_MAX_WAIT);
                rad->fsmRad.timeout_msec = WLD_FSM_WAIT_TIME;
                rad->fsmRad.FSM_Retry--;
                break;
            } else {
                SAH_TRACEZ_ERROR(ME, "%s start while EP WPS active", rad->Name);
            }
        }
        rad->fsmRad.FSM_Retry = WLD_FSM_MAX_WAIT;
        /* There's no wait cycle (Can't be used for TR69 & TR98), SPEEDUP */
        rad->fsmRad.timeout_msec = 100;
        bool has_lock = s_tryGetLock(rad);
        if(has_lock) {
            rad->fsmRad.FSM_State = FSM_DEPENDENCY;
        }
        // else just stay and wait.
        break;

    case FSM_RESTART:
        // Restart radio after deep power down
        SAH_TRACEZ_INFO(ME, "%s refresh", rad->Name);

        SWL_CALL(s_getMngr(rad)->doRestart, rad);

        rad->fsmRad.FSM_State = FSM_DEPENDENCY;
        break;
    case FSM_SYNC_RAD:
    case FSM_SYNC_VAP:
    case FSM_DEPENDENCY: {

        s_performSync(rad);

        SWL_CALL(s_getMngr(rad)->checkPreDependency, rad);

        s_syncVap(rad);
        s_syncEp(rad);

        SWL_CALL(s_getMngr(rad)->checkRadDependency, rad);

        s_printFsmBits(rad);
        s_cleanFsmBits(rad);

        rad->fsmRad.FSM_State = FSM_RUN;
        wld_rad_incrementCounterStr(rad, &rad->genericCounters, WLD_RAD_EV_FSM_COMMIT,
                                    "0x%08lx 0x%08lx / 0x%08lx 0x%08lx", rad->fsmRad.FSM_AC_BitActionArray[0], rad->fsmRad.FSM_AC_BitActionArray[1],
                                    rad->fsmRad.FSM_BitActionArray[0], rad->fsmRad.FSM_BitActionArray[1]);
        rad->fsmRad.retryCount = 0;
        break;
    }
    case FSM_RUN:
        delay = 0;
        rad->fsmRad.timeout_msec = 10;
        int radFsmBit = getLowestBitLongArray(rad->fsmRad.FSM_AC_BitActionArray, FSM_BW);
        if(radFsmBit < 0) {
            rad->fsmRad.FSM_State = FSM_COMPEND;
            break;
        }
        if(s_getMngr(rad)->actionList[radFsmBit].index != (uint32_t) radFsmBit) {
            SAH_TRACEZ_ERROR(ME, "Rad %7s: Invalid bit %u - %u name %s", rad->Name, s_getMngr(rad)->actionList[radFsmBit].index,
                             radFsmBit, s_getMngr(rad)->actionList[radFsmBit].name);
        }


        SAH_TRACEZ_INFO(ME, "RAD %7s: bit %u : %s", rad->Name, radFsmBit, s_getMngr(rad)->actionList[radFsmBit].name);
        SWL_CALL(s_getMngr(rad)->doRadFsmRun, rad);
        bool globalSuccess = true;

        if((s_getMngr(rad)->actionList != NULL) && (s_getMngr(rad)->actionList[radFsmBit].doRadFsmAction != NULL)) {
            globalSuccess = s_getMngr(rad)->actionList[radFsmBit].doRadFsmAction(rad);
        }

        wld_rad_forEachAp(pAP, rad) {
            if(pAP->fsm.FSM_Delay) {
                pAP->fsm.FSM_Delay--;
                delay++;
                continue;
            }

            int vapFsmBit = getLowestBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW);
            bool execVap = (radFsmBit >= vapFsmBit) && (vapFsmBit != -1);
            SAH_TRACEZ_INFO(ME, "AP %7s: bit %u => exec %u", pAP->name, vapFsmBit, execVap);
            if(!execVap) {
                continue;
            }

            bool handled = false;

            if(s_getMngr(rad)->doVapFsmRun != NULL) {
                s_getMngr(rad)->doVapFsmRun(pAP, rad);
                handled = true;
            }

            if((s_getMngr(rad)->actionList != NULL) && (s_getMngr(rad)->actionList[vapFsmBit].doVapFsmAction != NULL)) {
                bool success = s_getMngr(rad)->actionList[vapFsmBit].doVapFsmAction(pAP, rad);
                if(success) {
                    clearBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, vapFsmBit);
                } else {
                    globalSuccess = false;
                }
                handled = true;
            }

            if(!handled) {
                clearBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, vapFsmBit);
            }

            /* Check if all VAPS are synced before entering the next stage  */
            int nextVapFsmBit = getLowestBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW);
            if((nextVapFsmBit <= radFsmBit) && (nextVapFsmBit != -1)) {
                delay++;
            }
        }

        wld_rad_forEachEp(pEP, rad) {
            if(pEP->fsm.FSM_Delay) {
                pEP->fsm.FSM_Delay--;
                delay++;
                continue;
            }

            int epFsmBit = getLowestBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW);
            bool execEp = (radFsmBit >= epFsmBit) && (epFsmBit != -1);
            SAH_TRACEZ_INFO(ME, "EP: %7s : bit %u => exec %u ?", pEP->Name, epFsmBit, execEp);
            if(!execEp) {
                continue;
            }
            bool handled = false;
            if(s_getMngr(rad)->doEpFsmRun != NULL) {
                s_getMngr(rad)->doEpFsmRun(pEP, rad);
                handled = true;
            }

            SWL_CALL(s_getMngr(rad)->doEpFsmRun, pEP, rad);
            if((s_getMngr(rad)->actionList != NULL) && (s_getMngr(rad)->actionList[epFsmBit].doEpFsmAction != NULL)) {
                bool success = s_getMngr(rad)->actionList[epFsmBit].doEpFsmAction(pEP, rad);
                if(success) {
                    clearBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, epFsmBit);
                } else {
                    globalSuccess = false;
                }
                handled = true;
            }
            if(!handled) {
                clearBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, epFsmBit);
            }
        }

        if(rad->fsmRad.FSM_Loop > 0) {
            // Get out of the loop!
            rad->fsmRad.FSM_Loop--;
        } else {
            rad->fsmRad.FSM_Loop = 0;
        }

        /* If nothing went wrong... we can clear the BIT on the radio... */
        if(!delay && !rad->fsmRad.FSM_Loop && globalSuccess) {
            // Full loop is done?
            clearBitLongArray(rad->fsmRad.FSM_AC_BitActionArray, FSM_BW, radFsmBit);
        }

        break;

    case FSM_COMPEND:
        // if commit still pending, go to restart. Still have lock.
        if(s_checkCommitPending(rad, FSM_DEPENDENCY)) {
            break;
        }

        rad->fsmRad.FSM_State = FSM_FINISH;
        // Release lock
        bool last = s_areAnyWaiting();
        SAH_TRACEZ_WARNING(ME, "%s: check compend FSM %u %p", rad->Name, last, s_getMngr(rad)->doFinish);
        SWL_CALL(s_getMngr(rad)->doCompendCheck, rad, last);
        s_freeLock(rad);
        break;

    case FSM_WAIT_RAD:
    case FSM_FINISH:
    case FSM_ERROR:
    case FSM_UNKNOWN:
        SAH_TRACEZ_WARNING(ME, "%s: do finish FSM %p", rad->Name, s_getMngr(rad)->doFinish);
        SWL_CALL(s_getMngr(rad)->doFinish, rad);

        /* Update here our wireless STATUS field to the datamodel */
        wld_rad_updateState(rad, false);


        rad->fsmRad.FSM_State = FSM_IDLE;
        rad->fsm_radio_st = FSM_IDLE;    // UnLock the RADIO
        rad->blockCommit = 0;            // Accept again commits by toggle Enable TRUE

        if(rad->fsmRad.timer) {
            amxp_timer_delete(&rad->fsmRad.timer);
        }
        rad->fsmRad.timer = 0;
        if(rad->call_id != 0) {
            fsm_delay_reply(rad->call_id, amxd_status_ok, NULL);
            rad->call_id = 0;
        }
        break;

    case FSM_FATAL:
        break;

    default:
        break;
    }
    SAH_TRACEZ_INFO(ME, "%s done FSM state %u, rad state %u", rad->Name, rad->fsmRad.FSM_State, rad->detailedState);

    return (rad->fsmRad.FSM_State);
}

int wld_rad_fsm_reset(T_Radio* rad) {
    int bitmask = (1 << rad->ref_index);
    if((s_radioFSMLock & RADIO_INDEX_MASK) != bitmask) {
        SAH_TRACEZ_ERROR(ME, "%s: resetting radio which has lock", rad->Name);
        s_freeLock(rad);
    }
    return WLD_OK;
}

// For Broadcom FSM is ready when... both RADIOS are ready!
static int s_fsmState(T_Radio* rad) {
    return rad->fsmRad.FSM_State;
}


void wld_fsm_init(vendor_t* vendor, wld_fsmMngr_t* fsmMngr) {
    SAH_TRACEZ_INFO(ME, "%s init vendor wld fsm", vendor->name);
    vendor->fsmMngr = fsmMngr;
    vendor->fta.mfn_wrad_fsm = wld_rad_fsm;          //ERROR_TRAP_pA;
    vendor->fta.mfn_wrad_fsm_state = s_fsmState;     //ERROR_TRAP_pA;
}
