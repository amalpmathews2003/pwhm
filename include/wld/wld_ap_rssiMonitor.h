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

#ifndef SRC_INCLUDE_WLD_WLD_AP_RSSIMONITOR_H_
#define SRC_INCLUDE_WLD_WLD_AP_RSSIMONITOR_H_

#include "wld.h"

typedef struct {
    int32_t minRssi;
    int32_t maxRssi;
    int32_t minNoise;
    int32_t maxNoise;
    int32_t minSNR;
    int32_t maxSNR;
} wld_apRssiMon_signalRange_t;

void wld_apRssiMon_updateEnable(T_AccessPoint* pAP);
void wld_ap_rssiEv_debug(T_AccessPoint* pAP, amxc_var_t* retMap);
void wld_ap_rssiMonInit(T_AccessPoint* pAP);
void wld_ap_rssiMonDestroy(T_AccessPoint* pAP);
void wld_apRssiMon_createStaHistory(T_AssociatedDevice* pAD, uint32_t historyLen);
void wld_apRssiMon_destroyStaHistory(T_AssociatedDevice* pAD);
void wld_apRssiMon_updateHistoryLen(T_AccessPoint* pAP);
void wld_apRssiMon_cleanStaHistory(wld_assocDev_history_t* staHistory, uint32_t historyLen);
void wld_apRssiMon_cleanStaHistoryAll(T_AccessPoint* pAP);
wld_staHistory_t* wld_apRssiMon_getOldestStaSample(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
wld_staHistory_t* wld_apRssiMon_getStaSampleIndexed(T_AccessPoint* pAP, T_AssociatedDevice* pAD, int32_t index);
void wld_apRssiMon_getStaHistory(T_AccessPoint* pAP, const unsigned char macAddress[ETHER_ADDR_LEN], amxc_var_t* myMap);
void wld_apRssiMon_updateStaHistory(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_apRssiMon_sendHistoryOnAssocEvent(T_AccessPoint* pAP, T_AssociatedDevice* pAD, amxc_var_t* myVar);
void wld_apRssiMon_sendHistoryOnDisassocEvent(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_apRssiMon_sendStaHistoryAll(T_AccessPoint* pAP);
void wld_apRssiMon_clearSendEventOnAssoc(T_AccessPoint* pAP);
bool wld_apRssiMon_isReadyStaHistory(T_AssociatedDevice* pAD);
void wld_apRssiMon_updateStaHistoryAll(T_AccessPoint* pAP);
bool wld_apRssiMon_getMinMaxSignal(T_AccessPoint* pAP, T_AssociatedDevice* pAD, wld_apRssiMon_signalRange_t* range);
#endif /* SRC_INCLUDE_WLD_WLD_AP_RSSIMONITOR_H_ */
