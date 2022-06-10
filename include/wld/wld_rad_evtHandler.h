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
#ifndef SRC_INCLUDE_WLD_WLD_RAD_EVTHANDLER_H_
#define SRC_INCLUDE_WLD_WLD_RAD_EVTHANDLER_H_

#include <stdbool.h>

typedef void (* wld_radEvtHandler_rad)(T_Radio* rad, uint32_t eventId, void* data);
typedef void (* wld_radEvtHandler_vap)(T_Radio* pRad, T_AccessPoint* pAP, uint32_t eventId, void* data);
typedef void (* wld_radEvtHandler_ep)(T_Radio* pRad, T_EndPoint* pEP, uint32_t eventId, void* data);

typedef struct {
    uint32_t eventId;
    wld_radEvtHandler_rad handleEvent;
} wld_radEvtHandler_rad_t;

typedef struct {
    uint32_t eventId;
    wld_radEvtHandler_vap handleEvent;
} wld_radEvtHandler_vap_t;

typedef struct {
    uint32_t eventId;
    wld_radEvtHandler_ep handleEvent;
} wld_radEvtHandler_ep_t;

#define WLD_RAD_EV_HANDLER_RAD_ENTRY(ev, fun) { .eventId = ev, .handleEvent = (wld_radEvtHandler_rad) fun }
#define WLD_RAD_EV_HANDLER_VAP_ENTRY(ev, fun) { .eventId = ev, .handleEvent = (wld_radEvtHandler_vap) fun }
#define WLD_RAD_EV_HANDLER_EP_ENTRY(ev, fun) { .eventId = ev, .handleEvent = (wld_radEvtHandler_ep) fun }

typedef struct {
    wld_radEvtHandler_vap_t* vapHandlers;
    uint32_t nrVapHandlers;
    wld_radEvtHandler_ep_t* epHandlers;
    uint32_t nrEpHandlers;
    wld_radEvtHandler_rad_t* radHanders;
    uint32_t nrRadHandlers;
    bool cascade;
} wld_radEvtHandler_t;

void wld_radEvtHandler_handleEventByName(wld_radEvtHandler_t* handlers, T_Radio* pRad, uint32_t event, const char* ifname, void* data);
void wld_radEvtHandler_handleEventByIndex(wld_radEvtHandler_t* handlers, T_Radio* pRad, uint32_t event, uint32_t ifindex, void* data);

#define WLD_RAD_EVT_HANDLER_INIT(_handlerName, _vapHandlerName, _epHandlerName, _radHandlerName) \
    wld_radEvtHandler_t _handlerName = { \
        .vapHandlers = _vapHandlerName, \
        .nrVapHandlers = SWL_ARRAY_SIZE(_vapHandlerName), \
        .epHandlers = _epHandlerName, \
        .nrEpHandlers = SWL_ARRAY_SIZE(_epHandlerName), \
        .radHanders = _radHandlerName, \
        .nrRadHandlers = SWL_ARRAY_SIZE(_radHandlerName), \
    }

#endif /* SRC_INCLUDE_WLD_WLD_RAD_EVTHANDLER_H_ */
