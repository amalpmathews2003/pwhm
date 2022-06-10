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

#ifndef INCLUDE_PRIV_NL80211_WLD_NL80211_EVENTS_PRIV_H_
#define INCLUDE_PRIV_NL80211_WLD_NL80211_EVENTS_PRIV_H_

#include "wld_nl80211_core_priv.h"
#include "wld_nl80211_events.h"
#include "swl/swl_unLiList.h"

/*
 * @brief nl80211 event listener context
 */
struct wld_nl80211_listener {
    amxc_llist_it_t it;                  //iterator for list
    wld_nl80211_state_t* state;          //manager ctx, receiving nl events
    uint32_t wiphy;                      //watched wiphy: >=0 or WLD_NL80211_ID_ANY or WLD_NL80211_ID_UNDEF
    uint32_t ifIndex;                    //watched iface netdev index: >0 or WLD_NL80211_ID_ANY
    void* pRef;                          //user private reference to provide when invoking handlers
    void* pData;                         //user private data to provide when invoking handlers
    wld_nl80211_evtHandlers_cb handlers; //event handler struct: evts with null clbks are ignored
};

/*
 * @brief prototype of nl event parser: gets attributes from nl msg then forward event
 * to the listener side
 *
 * @param pListener pointer to event listener
 * @param nlh pointer to netlink msg header
 * @param tb array of netlink message attributes
 *
 * @return SWL_RC_OK in case of parsing success
 *         SWL_RC_CONTINUE when msg is valid but ignored by the parser
 *         SWL_RC_DONE msg parsed and processed by listener event handler
 *         SWL_RC_ERROR in case of error
 */
typedef swl_rc_ne (* wld_nl80211_evtParser_f) (swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]);

/*
 * @brief get parser (callback) for nl80211 event
 *
 * @param eventId id of nl80211 event (NL80211_CMD_xxx)
 *
 * @return pointer to parser when event is supported, null otherwise
 */
wld_nl80211_evtParser_f wld_nl80211_getEventParser(uint32_t eventId);

/*
 * @brief check if listener has handler callback for received event
 *
 * @param pListener pointer to listener context
 * @param eventId id of received nl80211 event (NL80211_CMD_xxx)
 *
 * @return true if listener has handler for received event
 *         false otherwise (or in case of error)
 */
bool wld_nl80211_hasEventHandler(wld_nl80211_listener_t* pListener, uint32_t eventId);

/*
 * @brief update event handlers of listener, when it defines
 * monitored wiphy Id and/or iface netId.
 *
 * @param pListener pointer to listener context
 * @param handlers pointer to structure of handler callbacks
 * unwanted events shall remain null.
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_updateEventHandlers(wld_nl80211_listener_t* pListener, const wld_nl80211_evtHandlers_cb* const handlers);

#endif /* INCLUDE_PRIV_NL80211_WLD_NL80211_EVENTS_PRIV_H_ */
