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

#ifndef SRC_INCLUDE_WLD_WLD_TYPES_H_
#define SRC_INCLUDE_WLD_WLD_TYPES_H_

#define WLD_MAX_POSSIBLE_CHANNELS 64

typedef struct WLD_RADIO T_Radio;
typedef struct S_ACCESSPOINT T_AccessPoint;
typedef struct S_SSID T_SSID;
typedef struct S_EndPoint T_EndPoint;
typedef struct vendor vendor_t;
typedef struct wld_tinyRoam wld_tinyRoam_t;
/*
 * Deprecated types with old syntax T_xxx , only defined for backward compatibility with legacy code
 * Please use wld_xxx_t types for new development.
 */
typedef struct wld_spectrumChannelInfoEntry spectrumChannelInfoEntry_t;
typedef struct wld_scanArgs T_ScanArgs;
typedef struct wld_scanResultSSID T_ScanResult_SSID;
typedef struct wld_scanResults T_ScanResults;
typedef struct wld_airStats T_Airstats;
typedef struct wld_nasta T_NonAssociatedDevice;
typedef struct wld_nasta T_MonitorDevice;
typedef struct wld_radExt wld_radExt_t;

typedef enum {
    COM_DIR_TRANSMIT,
    COM_DIR_RECEIVE,
    COM_DIR_MAX
} com_dir_e;
extern const char* com_dir_char[];

typedef enum {
    WLD_AC_BE,
    WLD_AC_BK,
    WLD_AC_VI,
    WLD_AC_VO,
    WLD_AC_MAX
} wld_ac_e;


typedef enum {
    FSM_IDLE,       /* Task not yet created */
    FSM_WAIT,       /* Task is active, but waits for extra input */
    FSM_RESTART,    /* Do some extra state checks on VAP & Radio */
    FSM_SYNC_RAD,   /* Sync the Radio states --> T_Radio */
    FSM_SYNC_VAP,   /* Sync VAP interfaces --> T_AccessPoint & T_SSID */
    FSM_DEPENDENCY, /* Task checks dependency on the to-do list */
    FSM_RUN,        /* Run through the to-do list */
    FSM_COMPEND,    /* Check if a new commit is pending? */
    FSM_WAIT_RAD,   /* Wait for other radio's to finish config */
    FSM_FINISH,     /* To-do list is empty, all went fine */
    FSM_ERROR,      /* We've failed to execute the to-do list */
    FSM_UNKNOWN,    /* ? A state we don't like and don't know how to get rid of it.*/
    FSM_FATAL       /* Really hard issue... we can only fix this by restarting the system or driver */
} FSM_STATE;

#endif /* SRC_INCLUDE_WLD_WLD_TYPES_H_ */
