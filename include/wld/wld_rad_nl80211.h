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
 * This file defines wrapper functions to use nl80211 generic apis with T_Radio context
 */

#ifndef INCLUDE_WLD_WLD_RAD_NL80211_H_
#define INCLUDE_WLD_WLD_RAD_NL80211_H_

#include "wld.h"
#include "wld_nl80211_api.h"
#include "wld_nl80211_events.h"
#include "wld_nl80211_debug.h"

/*
 * @brief set a listener for all received nl80211 events related to a radio device.
 *
 * @param pRadio pointer to radio context
 * @param pData user private data to provide when invoking handlers
 * @param handlers Pointer to structure of callback functions, handling the received events
 *                 Callbacks of unmanaged events shall remain null.
 *                 Handlers will be called with pRef equal to pRadio.
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setEvtListener(T_Radio* pRadio, void* pData, const wld_nl80211_evtHandlers_cb* const handlers);

/*
 * @brief delete listener of radio device's nl80211 events
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_delEvtListener(T_Radio* pRadio);

/*
 * @brief get info of main radio interface (AP/EP)
 * This includes some :
 * - physical info like current channel and tx power
 * - logical info: interface name, mac, type, ssid
 *
 * @param pRadio pointer to radio context
 * @param pIfaceInfo pointer to interface info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getInterfaceInfo(T_Radio* pRadio, wld_nl80211_ifaceInfo_t* pIfaceInfo);

/*
 * @brief configure radio's main interface type as AP
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setAp(T_Radio* pRadio);

/*
 * @brief configure radio's main interface type as Station (endpoint)
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setSta(T_Radio* pRadio);

/*
 * @brief configure radio's main interface 4Mac option
 * (With NL80211, it is only available in STATION and AP_VLAN types)
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_set4Mac(T_Radio* pRadio, bool use4Mac);

/*
 * @brief create/set AP interface on top of radio device
 * AP.alias (expected net dev name) must be provided as input.
 *
 * @param pAP pointer to accesspoint context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_addVapInterface(T_Radio* pRadio, T_AccessPoint* pAP);

/*
 * @brief get info of radio device
 * This includes:
 * - supported radio standards, freq Bands (+chanList), channel widths
 *
 * @param pRadio pointer to radio context
 * @param pWiphyInfo pointer to radio info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getWiphyInfo(T_Radio* pRadio, wld_nl80211_wiphyInfo_t* pWiphyInfo);

/*
 * @brief get survey info of all radio channels
 *
 * @param pRadio pointer to radio context
 * @param ppChanSurveyInfo (output) array of channel survey info, dynamically allocated
 * (need to be freed by user)
 * @param pnChanSurveyInfo (output) number of available survey info
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getSurveyInfo(T_Radio* pRadio, wld_nl80211_channelSurveyInfo_t** ppChanSurveyInfo, uint32_t* pnrChanSurveyInfo);

/*
 * @brief get radio air statistics of the central channel being used
 * The channel load is defined as the percentage of time that the AP sensed the medium was busy.
 *
 * @param pRadio pointer to radio context
 * @param stats pointer to result
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getAirstats(T_Radio* pRad, T_Airstats* stats);

/*
 * @brief configure radio's tx/rx antennas
 *
 * @param pRadio pointer to radio context
 * @param txMapAnt Bitmap of allowed antennas for transmitting
 * @param rxMapAnt Bitmap of allowed antennas for receiving
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setAntennas(T_Radio* pRadio, uint32_t txMapAnt, uint32_t rxMapAnt);

/*
 * @brief configure auto transmit power level
 *
 * @param pRadio pointer to radio context
 * @param type TX power adjustment
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerAuto(T_Radio* pRadio);

/*
 * @brief configure fixed transmit power level
 *
 * @param pRadio pointer to radio context
 * @param mbm transmit power level where mbm = (dbm * 100)
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerFixed(T_Radio* pRadio, int32_t mbm);

/*
 * @brief configure limited transmit power level
 *
 * @param pRadio pointer to radio context
 * @param mbm transmit power level where mbm = (dbm * 100)
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerLimited(T_Radio* pRadio, int32_t mbm);

/*
 * @brief get transmit power level
 *
 * @param pRadio pointer to radio context
 * @param dbm current tx power in dbm
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getTxPower(T_Radio* pRadio, int32_t* dbm);

/*
 * @brief get channel specification from nl80211 interface info
 *
 * @param pChanSpec pointer to chanspec info struct to be filled
 * @param pIfaceInfo source pointer to interface info
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getChanSpecFromIfaceInfo(swl_chanspec_t* pChanSpec, wld_nl80211_ifaceInfo_t* pIfaceInfo);

/*
 * @brief get current radio channel info
 *
 * @param pRadio pointer to radio context
 * @param pChanSpec pointer to chanspec info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getChannel(T_Radio* pRadio, swl_chanspec_t* pChanSpec);

/*
 * @brief initiate a neighbor scan
 * It is possible to indicate scan options:
 * - ssids to fetch
 * - frequencies to use
 *
 * @param pRadio pointer to radio context
 * @param args scan options (ssid, channels)
 *
 * @return SWL_RC_OK in case of success (scan trigger acknowledged)
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_startScan(T_Radio* pRadio, T_ScanArgs* args);

/*
 * @brief abort a running neighbor scan
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success (scan abort acknowledged)
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_abortScan(T_Radio* pRadio);

/*
 * @brief subscribe to get asynchronous scan results
 *
 * @param pRadio pointer to radio context
 * @param priv user data that will returned in the result handler
 * @param fScanResultsCb handler that will be called when results are ready
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getScanResults(T_Radio* pRadio, void* priv, scanResultsCb_f fScanResultsCb);

#endif /* INCLUDE_WLD_WLD_RAD_NL80211_H_ */
