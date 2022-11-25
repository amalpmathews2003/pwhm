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
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include "wld.h"
#include "wld_wpaCtrl_api.h"
#include "wld_wpaCtrlInterface_priv.h"

#define ME "wpaCtrl"

#define MSG_LENGTH          (4096 * 3)
#define CTRL_IFACE_CLIENT "/var/lib/wld/wpactrl-"

typedef enum {
    WPA_CONNECTION_CMD = 0,
    WPA_CONNECTION_EVENT,
    WPA_CONNECTION_MAX
} wld_wpaCtrlConnectionType_e;

static const char* s_getConnCliPath(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    return pConn->clientAddr.sun_path;
}

static const char* s_getConnSrvPath(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    return pConn->serverAddr.sun_path;
}

static bool s_sendCmd(wpaCtrlConnection_t* pConn, const char* cmd) {
    ASSERTS_NOT_NULL(pConn, false, ME, "NULL");
    ASSERTS_NOT_EQUALS(pConn->wpaPeer, 0, false, ME, "NULL");
    int fd = pConn->wpaPeer;
    ASSERTS_TRUE(fd > 0, false, ME, "fd <= 0");

    const char* srvPath = s_getConnSrvPath(pConn);
    size_t len = strlen(cmd);
    ssize_t ret = send(fd, cmd, len, 0);
    SAH_TRACEZ_INFO(ME, "send cmd (%s) to (%s)", cmd, srvPath);
    ASSERT_EQUALS(ret, (ssize_t) len, false, ME, "Failed to send cmd (%s) to (%s): ret(%d), err(%d:%s)",
                  cmd, srvPath,
                  (int32_t) ret, errno, strerror(errno));

    return true;
}

/**
 * @brief send command to wap_ctrl server
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd :string command to be sent
 *
 * @return true if the command is successfully sent.
 *         false Otherwise (invalid param/state, or partial sending)
 */
bool wld_wpaCtrl_sendCmd(wld_wpaCtrlInterface_t* pIface, const char* cmd) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return s_sendCmd(pIface->cmdConn, cmd);
}

static bool s_sendCmdSynced(wpaCtrlConnection_t* pConn, const char* cmd, char* reply, size_t reply_len) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pConn, false, ME, "NULL");
    int fd = pConn->wpaPeer;
    const char* ifName = wld_wpaCtrlInterface_getName(pConn->pInterface);
    ASSERT_TRUE(fd > 0, false, ME, "%s: invalid fd for cmd (%s)", ifName, cmd);

    struct timeval tv;
    int res;
    fd_set rfds;

    /* clear pending replies to avoid getting answer of timeouted request */
    char dropBuf[reply_len];
    while(recv(fd, dropBuf, reply_len, 0) > 0) {
    }

    ASSERT_TRUE(s_sendCmd(pConn, cmd), false, ME, "%s: fail to send sync cmd(%s)", ifName, cmd);

    do {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        res = select(fd + 1, &rfds, NULL, NULL, &tv);
        if((res < 0) && (errno == EINTR)) {
            continue;
        }
        ASSERT_FALSE(res < 0, false, ME, "%s: select err(%d:%s)", ifName, errno, strerror(errno));
        ASSERT_NOT_EQUALS(res, 0, false, ME, "%s: cmd(%s) timed out", ifName, cmd);
        ASSERT_NOT_EQUALS(FD_ISSET(fd, &rfds), 0, false, ME, "%s: missing reply on fd(%d)", ifName, fd);
        ssize_t nr = recv(fd, reply, reply_len, 0);
        ASSERT_FALSE(nr < 0, false, ME, "%s: recv err(%d:%s)", ifName, errno, strerror(errno));
        /* ignore reply when looking like a wpactrl event */
        if(((nr > 0) && (reply[0] == '<')) ||
           ((nr > 6) && ( strncmp(reply, "IFNAME=", 7) == 0))) {
            continue;
        }
        nr = SWL_MIN(nr, (ssize_t) reply_len - 1);
        reply[nr] = '\0';
        /* remove systematic carriage return at end of buffer */
        if((nr > 0) && (reply[nr - 1] == '\n')) {
            reply[nr - 1] = '\0';
        }
        break;
    } while(1);

    SAH_TRACEZ_OUT(ME);
    return true;
}

/**
 * @brief send command to wpa_ctrl server and wait for the reply
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param reply : buffer where to store the received answer
 * @param reply_len : max length of reply
 *
 * @return true if the command is answered, false otherwise
 */
bool wld_wpaCtrl_sendCmdSynced(wld_wpaCtrlInterface_t* pIface, const char* cmd, char* reply, size_t reply_len) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return s_sendCmdSynced(pIface->cmdConn, cmd, reply, reply_len);
}

static bool s_sendCmdCheckResponse(wpaCtrlConnection_t* pConn, char* cmd, char* expectedResponse) {
    ASSERT_NOT_NULL(pConn, false, ME, "NULL");
    SAH_TRACEZ_IN(ME);
    char reply[MSG_LENGTH] = {'\0'};

    // send the command
    ASSERTS_TRUE(s_sendCmdSynced(pConn, cmd, reply, sizeof(reply) - 1), false, ME, "sending cmd %s failed", cmd);
    // check the response
    ASSERT_TRUE(swl_str_matches(reply, expectedResponse), false, ME, "cmd(%s) reply(%s): unmatch expect(%s)", cmd, reply, expectedResponse);

    return true;
}

/**
 * @brief send command to wpa_ctrl server and check the received reply
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param expectedResponse : string expected reply
 *
 * @return true when the command is answered as expected, false otherwise
 */
bool wld_wpaCtrl_sendCmdCheckResponse(wld_wpaCtrlInterface_t* pIface, char* cmd, char* expectedResponse) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return s_sendCmdCheckResponse(pIface->cmdConn, cmd, expectedResponse);
}

/**
 * @brief callback for received data from wpa_ctrl server
 *
 * @param peer : pointer to source context
 *
 * @return false on error. Otherwise, true
 */
static void s_readCtrl(int fd, void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    char msgData[2048] = {'\0'};
    ssize_t msgDataLen;
    ASSERTS_TRUE(fd > 0, , ME, "fd <= 0");

    msgDataLen = recv(fd, msgData, (sizeof(msgData) - 1), MSG_DONTWAIT);
    ASSERTS_FALSE(msgDataLen <= 0, , ME, "recv() failed (%d:%s)", errno, strerror(errno));
    msgData[msgDataLen] = '\0';
    amxo_connection_t* con = amxo_connection_get(get_wld_plugin_parser(), fd);
    ASSERT_NOT_NULL(con, , ME, "con NULL");
    wpaCtrlConnection_t* pConn = (wpaCtrlConnection_t*) con->priv;
    const char* srvPath = s_getConnSrvPath(pConn);
    SAH_TRACEZ_INFO(ME, "received data(%s) from (%s)", msgData, srvPath);
    ASSERTS_NOT_NULL(pConn, , ME, "NULL");
    wld_wpaCtrl_processMsg(pConn->pInterface, msgData, msgDataLen);
    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief close wpa_connection to wpa_ctrl server
 *
 * @param ppConn connection to close and reset
 *
 * @return void
 */
static void s_wpaCtrlCloseConnection(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, , ME, "NULL");
    ASSERTS_TRUE(pConn->wpaPeer > 0, , ME, "NULL");
    unlink(pConn->clientAddr.sun_path);
    amxo_connection_remove(get_wld_plugin_parser(), pConn->wpaPeer);
    pConn->wpaPeer = 0;
}

static void s_wpaCtrlCleanConnection(wpaCtrlConnection_t** ppConn) {
    ASSERTS_NOT_NULL(ppConn, , ME, "NULL");
    s_wpaCtrlCloseConnection(*ppConn);
    free(*ppConn);
    *ppConn = NULL;
}

/**
 * @brief initialize connection to wpa_ctrl server
 *
 * @param ppConn: connection to allocate and establish
 * @param interfaceName interface name
 * @param type WPA_CONNECTION_CMD, WPA_CONNECTION_EVENT
 * @param userdata the data used by the callback
 * @param handler the callback function is triggered when data is received on the opened connection
 *
 * @return true on success. Otherwise, false.
 */
static bool s_wpaCtrlInitConnection(wpaCtrlConnection_t** ppConn, wld_wpaCtrlInterface_t* pIface, uint32_t connId, char* serverPath) {
    ASSERT_NOT_NULL(ppConn, false, ME, "NULL");
    ASSERT_NOT_NULL(pIface, false, ME, "NULL");
    char* interfaceName = pIface->name;
    ASSERT_TRUE(interfaceName && interfaceName[0], false, ME, "invalid interfaceName");
    ASSERT_TRUE(serverPath && serverPath[0], false, ME, "invalid serverPath");

    wpaCtrlConnection_t* pConn = *ppConn;
    if(pConn == NULL) {
        pConn = calloc(1, sizeof(wpaCtrlConnection_t));
        ASSERT_NOT_NULL(pConn, false, ME, "%s: fail to alloc wpa_ctrl connection", interfaceName);
        pConn->pInterface = pIface;
        *ppConn = pConn;
    } else if(pConn->wpaPeer != 0) {
        s_wpaCtrlCloseConnection(pConn);
    }

    pConn->serverAddr.sun_family = AF_UNIX;
    snprintf(pConn->serverAddr.sun_path, sizeof(pConn->clientAddr.sun_path), "%s/%s", serverPath, interfaceName);
    pConn->clientAddr.sun_family = AF_UNIX;
    snprintf(pConn->clientAddr.sun_path, sizeof(pConn->serverAddr.sun_path), CTRL_IFACE_CLIENT "%s-%u", interfaceName, connId);

    SAH_TRACEZ_INFO(ME, "%s: init connection (%s) to (%s)", interfaceName,
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));
    return true;
}

/**
 * @brief open connection to wpa_ctrl server
 *
 * @param pConn: connection to be established
 *
 * @return true on success. Otherwise, false.
 */
static bool s_wpaCtrlOpenConnection(wpaCtrlConnection_t* pConn) {
    ASSERT_NOT_NULL(pConn, false, ME, "NULL");
    ASSERTS_FALSE(pConn->wpaPeer > 0, true, ME, "already connected");
    int fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(fd < 0, false, ME, "socket(PF_UNIX, SOCK_DGRAM, 0) failed");
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    int ret = bind(fd, (struct sockaddr*) &(pConn->clientAddr), sizeof(struct sockaddr_un));
    if((ret < 0) && (errno == EADDRINUSE)) {
        unlink(pConn->clientAddr.sun_path);
        ret = bind(fd, (struct sockaddr*) &(pConn->clientAddr), sizeof(struct sockaddr_un));
    }
    if((ret < 0) ||
       (connect(fd, (struct sockaddr*) &(pConn->serverAddr), sizeof(struct sockaddr_un)) < 0) ||
       (amxo_connection_add(get_wld_plugin_parser(), fd, s_readCtrl, "readCtrl", AMXO_CUSTOM, pConn) != 0)) {
        SAH_TRACEZ_INFO(ME, "failed to open connection (%s) to (%s) err:%d:%s",
                        s_getConnCliPath(pConn),
                        s_getConnSrvPath(pConn),
                        errno, strerror(errno));
        close(fd);
        s_wpaCtrlCloseConnection(pConn);
        return false;
    }
    pConn->wpaPeer = fd;

    SAH_TRACEZ_INFO(ME, "open connection (%s) to (%s)",
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));
    return true;
}

/**
 * @brief close the wpaCtrl interface to wpa_ctrl server
 * This function closes all relative connections (cmd and event)
 *
 * @param wpaCtrlInterface the interface to close
 *
 * @return void
 */
void wld_wpaCtrlInterface_close(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, , ME, "NULL");
    // Send DETACH before closing connection
    if(wld_wpaCtrlMngr_isConnected(pIface->pMgr) && pIface->isReady && (pIface->eventConn != NULL)) {
        bool ret = s_sendCmdCheckResponse(pIface->eventConn, "DETACH", "OK");
        if(ret == false) {
            SAH_TRACEZ_ERROR(ME, "detach failed from (%s)", s_getConnSrvPath(pIface->eventConn));
        }
    }

    // Close command connection socket
    s_wpaCtrlCloseConnection(pIface->cmdConn);
    // Close event socket
    s_wpaCtrlCloseConnection(pIface->eventConn);
    pIface->isReady = false;
}

void wld_wpaCtrlInterface_cleanup(wld_wpaCtrlInterface_t** ppIface) {
    ASSERTS_NOT_NULL(ppIface, , ME, "NULL");
    wld_wpaCtrlInterface_t* pIface = *ppIface;
    ASSERTS_NOT_NULL(pIface, , ME, "NULL");
    wld_wpaCtrlInterface_close(pIface);
    s_wpaCtrlCleanConnection(&pIface->cmdConn);
    s_wpaCtrlCleanConnection(&pIface->eventConn);
    wld_wpaCtrlMngr_unregisterInterface(pIface->pMgr, pIface);
    free(pIface);
    *ppIface = NULL;
}

bool wld_wpaCtrlInterface_init(wld_wpaCtrlInterface_t** ppIface, char* interfaceName, char* serverPath) {
    SAH_TRACEZ_IN(ME);
    ASSERTS_NOT_NULL(ppIface, false, ME, "NULL");

    wld_wpaCtrlInterface_t* pIface = *ppIface;
    if(pIface == NULL) {
        pIface = calloc(1, sizeof(wld_wpaCtrlInterface_t));
        ASSERT_NOT_NULL(pIface, false, ME, "%s: fail to allocate context", interfaceName);
        *ppIface = pIface;
    } else {
        wld_wpaCtrlInterface_close(pIface);
    }
    swl_str_copyMalloc(&pIface->name, interfaceName);
    pIface->isReady = false;

    // init connection for events
    bool ret = s_wpaCtrlInitConnection(&(pIface->eventConn), pIface, WPA_CONNECTION_EVENT, serverPath);
    // init connection for synchronous commands
    ret |= s_wpaCtrlInitConnection(&(pIface->cmdConn), pIface, WPA_CONNECTION_CMD, serverPath);

    if(!ret) {
        SAH_TRACEZ_ERROR(ME, "fail to init interface connections");
    }
    return ret;
}

bool wld_wpaCtrlInterface_setEvtHandlers(wld_wpaCtrlInterface_t* pIface, void* userdata, wld_wpaCtrl_evtHandlers_cb* pHandlers) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    pIface->userData = userdata;
    if(pHandlers) {
        pIface->handlers = *pHandlers;
    } else {
        memset(&pIface->handlers, 0, sizeof(pIface->handlers));
    }
    return true;
}

bool wld_wpaCtrlInterface_ping(wld_wpaCtrlInterface_t* pIface) {
    return ((pIface != NULL) &&
            (s_sendCmdCheckResponse(pIface->cmdConn, "PING", "PONG")));
}

bool wld_wpaCtrlInterface_isReady(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return pIface->isReady;
}

const char* wld_wpaCtrlInterface_getPath(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return s_getConnSrvPath(pIface->cmdConn);
}

const char* wld_wpaCtrlInterface_getName(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return pIface->name;
}

/**
 * @brief open the wpaCtrl interface of wpa_ctrl server
 * The first connection is for sending commands and the second one is for receiving unsolicited messages
 *
 * @param wpaCtrlInterface pointer to interface context (to be allocated and established)
 * @param interfaceName interface name
 * @param userdata the data used by the callback
 *
 * @return true on success. Otherwise, error.
 */
bool wld_wpaCtrlInterface_open(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    pIface->isReady = false;

    /*
     * create a socket for event
     * and send attach command to wpa_ctrl server to register for unsolicited msg
     */
    if((!s_wpaCtrlOpenConnection(pIface->eventConn)) ||
       (!s_sendCmdCheckResponse(pIface->eventConn, "ATTACH", "OK"))) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to establish event connection", pIface->name);
        return false;
    }

    /*
     * create a socket for cmd
     */
    if(!s_wpaCtrlOpenConnection(pIface->cmdConn)) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to establish cmd connection", pIface->name);
        return false;
    }

    SAH_TRACEZ_INFO(ME, "%s: Successfully connected to %s", pIface->name, wld_wpaCtrlInterface_getPath(pIface));
    pIface->isReady = true;

    return true;
}
