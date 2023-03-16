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
#ifndef __WLD_WPS_H__
#define __WLD_WPS_H__

#include <stdint.h>

#include "wld.h"

//String state
#define WPS_PAIRING_READY   SWL_WPS_NOTIF_PAIRING_STARTED
#define WPS_PAIRING_ERROR   SWL_WPS_NOTIF_PAIRING_STARTERROR
#define WPS_PAIRING_DONE    SWL_WPS_NOTIF_PAIRING_ENDED
#define WPS_PAIRING_NO_MESSAGE  "pairingNoMessage"
//WPS Reason
#define WPS_CAUSE_TIMEOUT SWL_WPS_NOTIF_ENDED_REASON_TIMEOUT
#define WPS_CAUSE_SUCCESS SWL_WPS_NOTIF_ENDED_REASON_SUCCESS
#define WPS_CAUSE_FAILURE SWL_WPS_NOTIF_ENDED_REASON_FAILURE
#define WPS_CAUSE_CANCELLED SWL_WPS_NOTIF_ENDED_REASON_CANCELLED
#define WPS_CAUSE_LOCKED SWL_WPS_NOTIF_ENDED_REASON_LOCKED
#define WPS_CAUSE_UNLOCKED SWL_WPS_NOTIF_ENDED_REASON_UNLOCKED
#define WPS_CAUSE_START_WPS_PBC SWL_WPS_NOTIF_STARTED_REASON_START_WPS_PBC
#define WPS_CAUSE_START_WPS_PIN SWL_WPS_NOTIF_STARTED_REASON_START_WPS_PIN
#define WPS_CAUSE_CONFIGURING SWL_WPS_NOTIF_ENDED_REASON_CONFIGURING
#define WPS_CAUSE_CONFIG SWL_WPS_NOTIF_ENDED_REASON_BAD_CONFIG
#define WPS_CAUSE_OVERLAP SWL_WPS_NOTIF_ENDED_REASON_OVERLAP
#define WPS_FAILURE_START_PBC SWL_WPS_NOTIF_STARTERROR_REASON_START_PBC
#define WPS_FAILURE_START_PIN SWL_WPS_NOTIF_STARTERROR_REASON_START_PIN
#define WPS_FAILURE_NO_SELF_PIN SWL_WPS_NOTIF_STARTERROR_REASON_NO_SELF_PIN
#define WPS_FAILURE_INVALID_PIN SWL_WPS_NOTIF_STARTERROR_REASON_INVALID_PIN
#define WPS_FAILURE_CREDENTIALS SWL_WPS_NOTIF_STARTERROR_REASON_CREDENTIALS

// Default WPS session walk time
#define WPS_WALK_TIME_DEFAULT 120
//Safety WPS session timer: only used if session timeout is not notified by backend (driver).
//expiration time: added margin to default WPS walk time
#define WPS_SESSION_TIMEOUT (WPS_WALK_TIME_DEFAULT + 10)

typedef struct {
    char ssid[SSID_NAME_LEN];
    swl_security_apMode_e secMode;
    char key[PSK_KEY_SIZE_LEN];
} T_WPSCredentials;

const char* wld_wps_ConfigMethod_to_string(wld_wps_cfgMethod_e value);
bool wld_wps_ConfigMethods_mask_to_string(amxc_string_t* output, const wld_wps_cfgMethod_m configMethods);
bool wld_wps_ConfigMethods_mask_to_charBuf(char* string, size_t stringsize, const wld_wps_cfgMethod_m configMethods);
bool wld_wps_ConfigMethods_string_to_mask(wld_wps_cfgMethod_m* output, const char* input, const char separator);
void wld_wps_pushButton_reply(uint64_t call_id, swl_usp_cmdStatus_ne cmdStatus);

//FOR INTERNAL USE ONLY
void wld_wps_sendPairingNotification(amxd_object_t* object, uint32_t type, const char* reason, const char* macAddress, T_WPSCredentials* credentials);
void wld_wps_clearPairingTimer(wld_wpsSessionInfo_t* pCtx);
//DEPRECATED
void wld_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress);

void genSelfPIN();

amxd_status_t _generateSelfPIN(amxd_object_t* object,
                               amxd_function_t* func,
                               amxc_var_t* args,
                               amxc_var_t* retval);

amxd_status_t _pushButton(amxd_object_t* object,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* retval);
#endif // __WLD_WPS_H__
