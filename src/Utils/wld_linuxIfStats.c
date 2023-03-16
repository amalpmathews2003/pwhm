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


#include "wld_linuxIfStats.h"
#include "swl/swl_assert.h"
#include "wld_rad_nl80211.h"

#include <debug/sahtrace.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define ME "linuxIfStats"


/**
 * PRIVATE MACROS
 */

#define IFLIST_REPLY_BUFFER 8192


/**
 * PRIVATE STURCTURS
 */

// @brief Netlink request type.
typedef struct {
    struct nlmsghdr hdr; // Netlink message
    struct rtgenmsg gen; // General form of address family dependent message
} nl_req_t;


/**
 * FUNCTIONS
 */


/**
 * @brief Gets interface statistics for the given network interface.
 *
 * Gets interface stats for given network interface from the specified Netlink message of type
 * RTM_NEWLINK.
 *
 * @param[in] pMsg Pointer to the Netlink message containing the data.
 * @param[in] pIfaceName Name of the network interface.
 * @param[in, out] pInterfaceStats Interface statistics structure with values read.
 *
 * @return True on success and false otherwise.
 */
static bool s_getRtmNewLinkInterfaceStats(const struct nlmsghdr* pMsg, const char* pIfaceName, T_Stats* pInterfaceStats) {

    size_t length = 0;
    if(pMsg->nlmsg_len <= NLMSG_LENGTH(sizeof(struct ifinfomsg))) {
        SAH_TRACEZ_ERROR(ME, "pMsg->nlmsg_len not correct");
        return false;
    }

    length = pMsg->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));


    // Loop over all attributes of the RTM_NEWLINK message
    bool ifaceNameMatched = false;
    struct ifinfomsg* iface = (struct ifinfomsg*) (NLMSG_DATA(pMsg));
    struct rtattr* attribute = NULL;

    for(attribute = IFLA_RTA(iface); RTA_OK(attribute, length); attribute = RTA_NEXT(attribute, length)) {
        switch(attribute->rta_type) {
        case IFLA_IFNAME:
            // This message contains the stats for the interface we are interested in
            if(swl_str_nmatches(pIfaceName, (char*) (RTA_DATA(attribute)), strlen(pIfaceName) + 1)) {
                ifaceNameMatched = true;
            }
            break;

        case IFLA_STATS:
            if(ifaceNameMatched) {
                struct rtnl_link_stats* stats = (struct rtnl_link_stats*) (RTA_DATA(attribute));

                pInterfaceStats->BytesSent = stats->tx_bytes;
                pInterfaceStats->BytesReceived = stats->rx_bytes;
                pInterfaceStats->PacketsSent = stats->tx_packets;
                pInterfaceStats->PacketsReceived = stats->rx_packets;
                pInterfaceStats->ErrorsSent = stats->tx_errors;
                pInterfaceStats->ErrorsReceived = stats->rx_errors;
                pInterfaceStats->RetransCount = 0;
                pInterfaceStats->DiscardPacketsSent = stats->tx_dropped;
                pInterfaceStats->DiscardPacketsReceived = stats->rx_dropped;
                pInterfaceStats->UnicastPacketsSent = 0;
                pInterfaceStats->UnicastPacketsReceived = 0;
                pInterfaceStats->MulticastPacketsSent = 0;
                pInterfaceStats->MulticastPacketsReceived = stats->multicast;
                pInterfaceStats->BroadcastPacketsSent = 0;
                pInterfaceStats->BroadcastPacketsReceived = 0;
                pInterfaceStats->UnknownProtoPacketsReceived = 0;
                pInterfaceStats->FailedRetransCount = 0;
                pInterfaceStats->RetryCount = 0;
                pInterfaceStats->MultipleRetryCount = 0;

                return true;
            }
            break;
        }
    }

    return false;
}

/**
 * @brief accumulate statistics for all VAP/EP of one radio
 *
 * @param[in] pInterfaceStats, statistics a one single VAR or EP
 * @param[in, out] pAllStats the accumelated statistics
 *
 */

static void s_accumulateStats(T_Stats* pAllStats, T_Stats* pInterfaceStats) {

    ASSERT_NOT_NULL(pAllStats, , ME, "NULL");
    ASSERT_NOT_NULL(pInterfaceStats, , ME, "NULL");

    pAllStats->BytesSent += pInterfaceStats->BytesSent;
    pAllStats->BytesReceived += pInterfaceStats->BytesReceived;
    pAllStats->PacketsSent += pInterfaceStats->PacketsSent;
    pAllStats->PacketsReceived += pInterfaceStats->PacketsReceived;
    pAllStats->ErrorsSent += pInterfaceStats->ErrorsSent;
    pAllStats->ErrorsReceived += pInterfaceStats->ErrorsReceived;
    pAllStats->RetransCount += pInterfaceStats->RetransCount;
    pAllStats->DiscardPacketsSent += pInterfaceStats->DiscardPacketsSent;
    pAllStats->DiscardPacketsReceived += pInterfaceStats->DiscardPacketsReceived;
    pAllStats->UnicastPacketsSent += pInterfaceStats->UnicastPacketsSent;
    pAllStats->UnicastPacketsReceived += pInterfaceStats->UnicastPacketsReceived;
    pAllStats->MulticastPacketsSent += pInterfaceStats->MulticastPacketsSent;
    pAllStats->MulticastPacketsReceived += pInterfaceStats->MulticastPacketsReceived;
    pAllStats->BroadcastPacketsSent += pInterfaceStats->BroadcastPacketsSent;
    pAllStats->BroadcastPacketsReceived += pInterfaceStats->BroadcastPacketsReceived;
    pAllStats->UnknownProtoPacketsReceived += pInterfaceStats->UnknownProtoPacketsReceived;
    pAllStats->FailedRetransCount += pInterfaceStats->FailedRetransCount;
    pAllStats->RetryCount += pInterfaceStats->RetryCount;
    pAllStats->MultipleRetryCount += pInterfaceStats->MultipleRetryCount;
}

/**
 * @brief Gets interface statistics for the given network interface.
 *
 * Gets interface stats for given network interface by sending a RTM_GETLINK Netlink request
 * through the specified Netlink socket and parsing received response.
 *
 * @param[in] pIfaceName Name of the network interface.
 * @param[in, out] pInterfaceStats Interface statistics structure with values read.
 *
 * @return True on success and false otherwise.
 */
bool wld_linuxIfStats_getInterfaceStats(const char* pIfaceName, T_Stats* pInterfaceStats) {

    ASSERT_NOT_NULL(pIfaceName, false, ME, "NULL");
    ASSERT_NOT_NULL(pInterfaceStats, false, ME, "NULL");

    bool result = false;
    bool end = false;                // flag to end loop parsing, equal to true when NLMSG_DONE message is received

    struct sockaddr_nl kernel;       // the remote (kernel space) side of the communication
    struct msghdr rtnl_msg;          // generic msghdr struct for use with sendmsg
    struct iovec io;                 // IO vector for sendmsg
    nl_req_t req;                    // structure that describes the Netlink packet itself

    char reply[IFLIST_REPLY_BUFFER]; // a large buffer to receive lots of link information

    memset(&kernel, 0, sizeof(kernel));
    memset(&rtnl_msg, 0, sizeof(rtnl_msg));
    memset(&io, 0, sizeof(io));
    memset(&req, 0, sizeof(req));
    memset(reply, 0, sizeof(reply));

    // Netlink socket is ready for use, prepare and send request
    kernel.nl_family = AF_NETLINK; // fill-in kernel address (destination of our message)
    kernel.nl_groups = 0;

    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.hdr.nlmsg_seq = 1;
    req.gen.rtgen_family = AF_PACKET; // no preferred AF, we will get *all* interfaces

    io.iov_base = &req;
    io.iov_len = req.hdr.nlmsg_len;
    rtnl_msg.msg_iov = &io;
    rtnl_msg.msg_iovlen = 1;
    rtnl_msg.msg_name = &kernel;
    rtnl_msg.msg_namelen = sizeof(kernel);


    /**
     * Create Netlink socket for kernel/user-space communication.
     * No need to call bind() as packets are sent only between the kernel and the originating
     * process (no multicasting).
     */
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(fd < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed creating Netlink socket");
        return false;
    }

    if(bind(fd, (struct sockaddr*) &kernel, sizeof(kernel)) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed binding socket");
        return false;
    }

    if(sendmsg(fd, (struct msghdr*) (&rtnl_msg), 0) < 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to send message through Netlink socket");

        close(fd);
        return false;
    }

    // Parse reply until message is done
    while(!end) {

        uint32_t length;
        struct msghdr rtnl_reply;     // generic msghdr structure for use with recvmsg
        struct nlmsghdr* pMsg;        // pointer to current part
        struct iovec io_reply;

        memset(&io_reply, 0, sizeof(io_reply));
        memset(&rtnl_reply, 0, sizeof(rtnl_reply));

        io.iov_base = reply;
        io.iov_len = IFLIST_REPLY_BUFFER;
        rtnl_reply.msg_iov = &io;
        rtnl_reply.msg_iovlen = 1;
        rtnl_reply.msg_name = &kernel;
        rtnl_reply.msg_namelen = sizeof(kernel);

        // Read as much data as fits in the receive buffer
        length = recvmsg(fd, &rtnl_reply, 0);

        if(length) {

            for(pMsg = (struct nlmsghdr*) reply;
                NLMSG_OK(pMsg, length);
                pMsg = NLMSG_NEXT(pMsg, length)) {
                switch(pMsg->nlmsg_type) {
                case NLMSG_DONE:
                    // This is the special meaning NLMSG_DONE message we asked for by using NLM_F_DUMP flag
                    end = true;
                    break;

                case RTM_NEWLINK:
                    // This is a RTM_NEWLINK message, which contains lots of information about a link
                    if(s_getRtmNewLinkInterfaceStats(pMsg, pIfaceName, pInterfaceStats)) {
                        end = true;
                        result = true;
                    }
                    break;
                }
            }
        } else {
            close(fd);
            return false;
        }
    }

    // Clean up and finish properly
    close(fd);
    return result;
}


/**
 * Get All virtual accespoints statistics
 */
bool wld_linuxIfStats_getAllVapStats(T_Radio* pRadio, T_Stats* pAllVapStats) {

    ASSERT_NOT_NULL(pRadio, false, ME, "NULL");
    ASSERT_NOT_NULL(pAllVapStats, false, ME, "NULL");

    bool result = false;

    T_AccessPoint* pAP = NULL;
    amxc_llist_it_t* it = NULL;

    T_Stats interfaceStats;
    memset(&interfaceStats, 0, sizeof(interfaceStats));

    // Get APs stats
    for(it = (amxc_llist_it_t*) amxc_llist_get_first(&pRadio->llAP); it; it = (amxc_llist_it_t*) amxc_llist_it_get_next(it)) {
        pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);

        SAH_TRACEZ_INFO(ME, "Radio AP interface = %s", pAP->alias);

        if(pAP->index <= 0) {
            continue;
        }
        if(!wld_linuxIfStats_getInterfaceStats(pAP->alias, &interfaceStats)) {
            SAH_TRACEZ_ERROR(ME, "Failed to get interface statistics for interface %s", pAP->alias);
            result |= false;
            continue;
        }

        s_accumulateStats(pAllVapStats, &interfaceStats);

        result |= true;
    }

    return result;
}

/**
 * Get All EndPoints statistics
 */
bool wld_linuxIfStats_getAllEpStats(T_Radio* pRadio, T_Stats* pAllEpStats) {
    ASSERT_NOT_NULL(pRadio, false, ME, "NULL");
    ASSERT_NOT_NULL(pAllEpStats, false, ME, "NULL");

    bool result = false;

    T_EndPoint* pEP = NULL;
    amxc_llist_it_t* it = NULL;

    T_Stats interfaceStats;
    memset(&interfaceStats, 0, sizeof(interfaceStats));

    // Get EPs Stats
    for(it = (amxc_llist_it_t*) amxc_llist_get_first(&pRadio->llEndPoints); it; it = (amxc_llist_it_t*) amxc_llist_it_get_next(it)) {
        pEP = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);

        SAH_TRACEZ_INFO(ME, "Radio EP interface = %s", pEP->Name);

        if(pEP->index <= 0) {
            continue;
        }
        if(!wld_linuxIfStats_getInterfaceStats(pEP->Name, &interfaceStats)) {
            SAH_TRACEZ_ERROR(ME, "Failed to get interface statistics for interface %s", pEP->Name);
            result |= false;
            continue;
        }

        s_accumulateStats(pAllEpStats, &interfaceStats);

        result |= true;
    }

    return result;
}


/**
 * Get ratio statistics
 */
bool wld_linuxIfStats_getRadioStats(T_Radio* pRadio, T_Stats* pRadioStats) {
    ASSERT_NOT_NULL(pRadio, false, ME, "NULL");
    ASSERT_NOT_NULL(pRadioStats, false, ME, "NULL");

    bool result = false;

    result |= wld_linuxIfStats_getAllVapStats(pRadio, pRadioStats);
    result |= wld_linuxIfStats_getAllEpStats(pRadio, pRadioStats);
    result |= wld_rad_nl80211_getNoise(pRadio, &pRadioStats->noise);
    return result;
}


