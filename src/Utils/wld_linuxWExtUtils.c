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
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef __USE_MISC
#include <linux/wireless.h>
#undef __USE_MISC
#include <net/if.h>
#else
#include <net/if.h>
#define __USE_MISC
#include <linux/wireless.h>
#endif //__USE_MISC

#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "wld_linuxWExtUtils.h"

#define ME "netUtil"

struct wlIfaceInfo {
    wld_linuxIface_t info;
    char protocol[IFNAMSIZ];
    int type;
};
static int s_ifnameCmp(const void* e1, const void* e2) {
    return strcmp(((struct wlIfaceInfo*) e1)->info.ifName, ((struct wlIfaceInfo*) e2)->info.ifName);
}
swl_rc_ne wld_linuxWExtUtils_getWirelessIfaces(const uint32_t maxRad, const uint32_t maxVap, wld_linuxIface_t grpWlIfaces[maxRad][maxVap]) {
    // ifaces are ordered with their if_index (i.e creation order)
    struct if_nameindex* ifaces = if_nameindex();
    ASSERT_NOT_NULL(ifaces, SWL_RC_ERROR, ME, "NULL");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        SAH_TRACEZ_ERROR(ME, "fail to open socket (errno:%d:%s))", errno, strerror(errno));
        if_freenameindex(ifaces);
        return SWL_RC_ERROR;
    }

    // keep only wireless interfaces
    uint32_t i = 0;
    uint32_t ifaceCnt = 0;
    uint32_t ifaceMax = maxRad * maxVap;
    struct wlIfaceInfo wlIfaces[ifaceMax];
    struct iwreq pwrq;
    for(; ifaces[i].if_index > 0 && ifaces[i].if_name != NULL && ifaceCnt < ifaceMax; i++) {
        memset(&pwrq, 0, sizeof(pwrq));
        swl_str_copy(pwrq.ifr_name, IFNAMSIZ, ifaces[i].if_name);
        if(ioctl(sock, SIOCGIWNAME, &pwrq) == -1) {
            continue;
        }
        swl_str_copy(wlIfaces[ifaceCnt].protocol, IFNAMSIZ, pwrq.u.name);
        if(ioctl(sock, SIOCGIWMODE, &pwrq) == -1) {
            continue;
        }
        if((pwrq.u.mode != IW_MODE_INFRA) && (pwrq.u.mode != IW_MODE_MASTER)) {
            continue;
        }
        wlIfaces[ifaceCnt].type = pwrq.u.mode;
        swl_str_copy(wlIfaces[ifaceCnt].info.ifName, IFNAMSIZ, ifaces[i].if_name);
        wlIfaces[ifaceCnt].info.ifIndex = ifaces[i].if_index;
        ifaceCnt++;
    }
    if_freenameindex(ifaces);
    close(sock);

    ASSERTI_NOT_EQUALS(ifaceCnt, 0, SWL_RC_ERROR, ME, "NO Wireless interface found");

    //sort wlifaces by ifname to get successive vap names for same radio
    qsort(wlIfaces, ifaceCnt, sizeof(struct wlIfaceInfo), s_ifnameCmp);

    uint32_t radId = 0;
    uint32_t vapId = 0;
    struct wlIfaceInfo* prevWlIface = NULL;
    for(i = 0; i < ifaceCnt; i++) {
        if((radId >= maxRad) || (vapId >= maxVap)) {
            SAH_TRACEZ_INFO(ME, "skip wireless iface (name:%s,index:%d): max reached",
                            wlIfaces[i].info.ifName, wlIfaces[i].info.ifIndex);
            continue;
        }
        if(prevWlIface != NULL) {
            vapId++;
            /*
             * group id (i.e radio) changes, when :
             * 1) getting infra iface (station)
             * 2) getting master iface (vap) with lower ifindex than previous iface
             */
            if((wlIfaces[i].type == IW_MODE_INFRA) || (wlIfaces[i].info.ifIndex < prevWlIface->info.ifIndex)) {
                radId++;
                vapId = 0;
            }
        }
        SAH_TRACEZ_INFO(ME, "Wireless[%d][%d] %s iface (name:%s,index:%d) found : proto %s",
                        radId, vapId,
                        (wlIfaces[i].type == IW_MODE_MASTER) ? "AP" : "RADIO",
                        wlIfaces[i].info.ifName, wlIfaces[i].info.ifIndex,
                        wlIfaces[i].protocol);
        swl_str_copy(grpWlIfaces[radId][vapId].ifName, IFNAMSIZ, wlIfaces[i].info.ifName);
        grpWlIfaces[radId][vapId].ifIndex = wlIfaces[i].info.ifIndex;
        prevWlIface = &wlIfaces[i];
    }
    return SWL_RC_OK;
}

