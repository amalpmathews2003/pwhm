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
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_arp.h>

#include "swl/swl_common.h"
#include "swl/swl_mac.h"
#include "swl/swl_string.h"

#define ME "netUtil"

int wld_linuxIfUtils_getNetSock() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

int wld_linuxIfUtils_getState(int sock, char* intfName) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFFLAGS failed (errno:%d:%s))", intfName, errno, strerror(errno));
    return ((ifr.ifr_flags & IFF_UP) == IFF_UP);
}

int wld_linuxIfUtils_getStateExt(char* intfName) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getState(sock, intfName);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_setState(int sock, char* intfName, int state) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFFLAGS failed (errno:%d:%s))", intfName, errno, strerror(errno));
    ifr.ifr_flags &= ~IFF_UP;
    if(state) {
        ifr.ifr_flags |= IFF_UP;
    }
    ret = ioctl(sock, SIOCSIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCSIFFLAGS state (%d) failed (errno:%d:%s))", intfName, state, errno, strerror(errno));
    return SWL_RC_OK;
}

int wld_linuxIfUtils_setStateExt(char* intfName, int state) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_setState(sock, intfName, state);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_setMac(int sock, char* intfName, swl_macBin_t* macInfo) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof ifr);
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data, macInfo->bMac, SWL_MAC_BIN_LEN);
    int ret = ioctl(sock, SIOCSIFHWADDR, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCSIFHWADDR mac ["SWL_MAC_FMT "] (errno:%d:%s))",
                 intfName, SWL_MAC_ARG(macInfo->bMac), errno, strerror(errno));
    return SWL_RC_OK;
}

int wld_linuxIfUtils_setMacExt(char* intfName, swl_macBin_t* macInfo) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_setMac(sock, intfName, macInfo);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_getMac(int sock, char* intfName, swl_macBin_t* macInfo) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFHWADDR, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFHWADDR (errno:%d:%s))",
                 intfName, errno, strerror(errno));
    memcpy(macInfo->bMac, ifr.ifr_hwaddr.sa_data, SWL_MAC_BIN_LEN);
    return SWL_RC_OK;
}

int wld_linuxIfUtils_getMacExt(char* intfName, swl_macBin_t* macInfo) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getMac(sock, intfName, macInfo);
    close(sock);
    return ret;
}

