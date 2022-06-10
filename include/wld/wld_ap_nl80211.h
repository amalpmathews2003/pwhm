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
/*
 * This file defines wrapper functions to use nl80211 generic apis with T_AccessPoint context
 */

#ifndef INCLUDE_WLD_WLD_AP_NL80211_H_
#define INCLUDE_WLD_WLD_AP_NL80211_H_

#include "wld.h"
#include "wld_nl80211_api.h"
#include "wld_nl80211_events.h"
#include "wld_nl80211_debug.h"

/*
 * @brief set a listener for all received nl80211 events from an AP interface.
 *
 * @param state pointer to nl80211 socket manager, on which socket the event would be received
 * @param pAP pointer to accesspoint context
 * @param pData user private data to provide when invoking handlers
 * @param handlers Pointer to structure of callback functions, handling the received events
 *                 Callbacks of unmanaged events shall remain null.
 *                 Handlers will be called with pRef equal to pAP.
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_nl80211_setEvtListener(T_AccessPoint* pAP, void* pData, const wld_nl80211_evtHandlers_cb* const handlers);

/*
 * @brief delete listener for AP interface's nl80211 events
 *
 * @param pAP pointer to accesspoint context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_nl80211_delEvtListener(T_AccessPoint* pAP);

/*
 * @brief get info of main/secondary AP interface
 * This includes some :
 * - physical info like relative radio current channel and tx power
 * - logical info: interface name, mac, type, ssid
 *
 * @param pAP pointer to accesspoint context
 * @param pIfaceInfo pointer to interface info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_nl80211_getInterfaceInfo(T_AccessPoint* pAP, wld_nl80211_ifaceInfo_t* pIfaceInfo);

/*
 * @brief delete AP interface
 * If the interface matches the radio's main interface, then it will set as Station interface.
 *
 * @param pAP pointer to accesspoint context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_nl80211_delVapInterface(T_AccessPoint* pAP);

#endif /* INCLUDE_WLD_WLD_AP_NL80211_H_ */
