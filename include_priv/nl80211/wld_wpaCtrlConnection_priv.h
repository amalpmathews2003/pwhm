/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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

#ifndef INCLUDE_PRIV_NL80211_WLD_WPACTRLCONNECTION_PRIV_H_
#define INCLUDE_PRIV_NL80211_WLD_WPACTRLCONNECTION_PRIV_H_

#include <sys/un.h>
#include "swl/swl_common.h"

#define CTRL_IFACE_CLIENT "/var/lib/wld/wpactrl-"
#define DFLT_SYNC_CMD_TMOUT_MS 1000

typedef void (* wld_wpaCtrlConnection_readDataCb_f)(void* userData, char* msgData, size_t msgLen);

typedef struct {
    wld_wpaCtrlConnection_readDataCb_f fReadDataCb;
} wld_wpaCtrlConnection_evtHandlers_cb;

typedef struct wpaCtrlConnection {
    struct sockaddr_un clientAddr;
    struct sockaddr_un serverAddr;
    char* sockName;
    int wpaPeer;
    void* userData;
    wld_wpaCtrlConnection_evtHandlers_cb evtHdlrs;
} wpaCtrlConnection_t;

swl_rc_ne wld_wpaCtrlConnection_init(wpaCtrlConnection_t** ppConn, uint32_t connId, const char* sockName, const char* serverPath);
swl_rc_ne wld_wpaCtrlConnection_setEvtHandlers(wpaCtrlConnection_t* pConn, void* userData, wld_wpaCtrlConnection_evtHandlers_cb* pEvtHdlrs);
swl_rc_ne wld_wpaCtrlConnection_open(wpaCtrlConnection_t* pConn);
swl_rc_ne wld_wpaCtrlConnection_sendCmd(wpaCtrlConnection_t* pConn, const char* cmd);
swl_rc_ne wld_wpaCtrlConnection_sendCmdSyncedExt(wpaCtrlConnection_t* pConn, const char* cmd, char* reply, size_t replyLen, uint32_t tmOutMSec);
swl_rc_ne wld_wpaCtrlConnection_sendCmdSynced(wpaCtrlConnection_t* pConn, const char* cmd, char* reply, size_t replyLen);
swl_rc_ne wld_wpaCtrlConnection_sendCmdCheckResponseExt(wpaCtrlConnection_t* pConn, char* cmd, char* expectedResponse, uint32_t tmOutMSec);
swl_rc_ne wld_wpaCtrlConnection_sendCmdCheckResponse(wpaCtrlConnection_t* pConn, char* cmd, char* expectedResponse);
swl_rc_ne wld_wpaCtrlConnection_close(wpaCtrlConnection_t* pConn);
swl_rc_ne wld_wpaCtrlConnection_cleanup(wpaCtrlConnection_t** ppConn);
const char* wld_wpaCtrlConnection_getConnCliPath(wpaCtrlConnection_t* pConn);
const char* wld_wpaCtrlConnection_getConnSrvPath(wpaCtrlConnection_t* pConn);

#endif /* INCLUDE_PRIV_NL80211_WLD_WPACTRLCONNECTION_PRIV_H_ */
