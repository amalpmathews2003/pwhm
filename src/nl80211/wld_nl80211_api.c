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
 * This file implements nl80211 api (requests)
 */

#include "wld_nl80211_api_priv.h"
#include "wld_nl80211_api.h"
#include "wld_nl80211_parser.h"

#include "swl/swl_common.h"
#include "swl/swl_assert.h"
#include "swla/swla_mac.h"

#define ME "nlApi"
#define NL80211_WLD_VENDOR_NAME "nl80211"

const T_CWLD_FUNC_TABLE* wld_nl80211_getVendorTable() {
    vendor_t* pVendor = wld_getVendorByName(NL80211_WLD_VENDOR_NAME);
    ASSERTS_NOT_NULL(pVendor, NULL, ME, "NULL");
    return &pVendor->fta;
}

const wld_fsmMngr_t* wld_nl80211_getFsmMngr() {
    vendor_t* pVendor = wld_getVendorByName(NL80211_WLD_VENDOR_NAME);
    ASSERTS_NOT_NULL(pVendor, NULL, ME, "NULL");
    return pVendor->fsmMngr;
}

vendor_t* wld_nl80211_registerVendor(T_CWLD_FUNC_TABLE* fta) {
    return wld_registerVendor(NL80211_WLD_VENDOR_NAME, fta);
}

struct getWirelessIfacesData_s {
    const uint32_t nrIfacesMax;
    uint32_t nrIfaces;
    wld_nl80211_ifaceInfo_t* pIfaces;
};
static swl_rc_ne s_getInterfaceInfoCb(swl_rc_ne rc, struct nlmsghdr* nlh, void* priv) {
    ASSERTS_FALSE((rc <= SWL_RC_ERROR), rc, ME, "Request error");
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_EQUALS(nlh->nlmsg_type, g_nl80211DriverIDs.family_id, SWL_RC_OK, ME, "skip msgtype %d", nlh->nlmsg_type);
    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
    ASSERTI_EQUALS(gnlh->cmd, NL80211_CMD_NEW_INTERFACE, SWL_RC_OK, ME, "unexpected cmd %d", gnlh->cmd);
    struct getWirelessIfacesData_s* requestData = (struct getWirelessIfacesData_s*) priv;
    ASSERT_NOT_NULL(requestData, SWL_RC_ERROR, ME, "No request data");
    // Parse the netlink message
    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};
    if(nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse netlink message");
        return SWL_RC_ERROR;
    }
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_nl80211_parseInterfaceInfo(tb, &ifaceInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    ASSERT_NOT_EQUALS(ifaceInfo.wiphy, WLD_NL80211_ID_UNDEF, SWL_RC_ERROR, ME, "missing wiphy");
    ASSERT_NOT_EQUALS(ifaceInfo.ifIndex, WLD_NL80211_ID_UNDEF, SWL_RC_ERROR, ME, "missing ifIndex");
    ASSERT_NOT_EQUALS(ifaceInfo.ifIndex, 0, SWL_RC_ERROR, ME, "Invalid net dev index");
    ASSERT_NOT_EQUALS(ifaceInfo.name[0], 0, SWL_RC_ERROR, ME, "missing interface name");
    if((!ifaceInfo.isSta) && (!ifaceInfo.isAp)) {
        SAH_TRACEZ_INFO(ME, "interface %s is not AP/Station", ifaceInfo.name);
        return SWL_RC_OK;
    }
    if((requestData->pIfaces == NULL) || (requestData->nrIfacesMax == 0)) {
        SAH_TRACEZ_INFO(ME, "interface %s skipped: no storage available", ifaceInfo.name);
        return SWL_RC_OK;
    }
    if(requestData->nrIfaces >= requestData->nrIfacesMax) {
        SAH_TRACEZ_INFO(ME, "interface %s skipped: maxIfaces %d reached", ifaceInfo.name, requestData->nrIfacesMax);
        return SWL_RC_DONE;
    }

    wld_nl80211_ifaceInfo_t* pIface = &requestData->pIfaces[requestData->nrIfaces];
    memcpy(pIface, &ifaceInfo, sizeof(*pIface));
    requestData->nrIfaces++;
    return SWL_RC_OK;
}

static int s_ifaceInfoCmp(const void* e1, const void* e2) {
    wld_nl80211_ifaceInfo_t* pIface1 = (wld_nl80211_ifaceInfo_t*) e1;
    wld_nl80211_ifaceInfo_t* pIface2 = (wld_nl80211_ifaceInfo_t*) e2;
    if(pIface1->wiphy == pIface2->wiphy) {
        return pIface1->ifIndex - pIface2->ifIndex;
    }
    return (pIface1->wiphy - pIface2->wiphy);
}
swl_rc_ne wld_nl80211_getInterfaces(const uint32_t nrWiphyMax, const uint32_t nrWifaceMax,
                                    wld_nl80211_ifaceInfo_t wlIfaces[nrWiphyMax][nrWifaceMax]) {
    memset(wlIfaces, 0, nrWiphyMax * nrWifaceMax * sizeof(wld_nl80211_ifaceInfo_t));
    struct getWirelessIfacesData_s requestData = {
        .nrIfacesMax = nrWiphyMax * nrWifaceMax,
        .nrIfaces = 0,
        .pIfaces = calloc(nrWiphyMax * nrWifaceMax, sizeof(wld_nl80211_ifaceInfo_t)),
    };
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    swl_rc_ne rc = wld_nl80211_sendCmdSync(state,
                                           NL80211_CMD_GET_INTERFACE, NLM_F_DUMP,
                                           0, NULL, s_getInterfaceInfoCb, &requestData);
    //sort wlifaces by wiphy then ifIndex, to get ordered vaps per radio
    qsort(requestData.pIfaces, requestData.nrIfaces, sizeof(wld_nl80211_ifaceInfo_t), s_ifaceInfoCmp);
    wld_nl80211_ifaceInfo_t* pWlIface;
    uint32_t lastWiphy = WLD_NL80211_ID_UNDEF;
    uint32_t curWiphyPos = 0;
    uint32_t curIfacePos = 0;
    for(uint32_t i = 0; i < requestData.nrIfaces; i++) {
        if((curWiphyPos >= nrWiphyMax) || (curIfacePos >= nrWifaceMax)) {
            break;
        }
        if(lastWiphy != requestData.pIfaces[i].wiphy) {
            if(curIfacePos > 0) {
                curWiphyPos++;
                curIfacePos = 0;
            }
            lastWiphy = requestData.pIfaces[i].wiphy;
        }
        pWlIface = &wlIfaces[curWiphyPos][curIfacePos];
        memcpy(pWlIface, &requestData.pIfaces[i], sizeof(*pWlIface));
        SAH_TRACEZ_INFO(ME, "Wireless[%d][%d] iface (name:%s,index:%d) %s %s%s",
                        curWiphyPos, curIfacePos,
                        pWlIface->name, pWlIface->ifIndex,
                        (pWlIface->isMain ? "main" : "virtual"),
                        (pWlIface->isAp ? "AP" : ""),
                        (pWlIface->isSta ? "EP" : ""));
        curIfacePos++;
    }
    free(requestData.pIfaces);
    return rc;
}

swl_rc_ne wld_nl80211_getInterfaceInfo(wld_nl80211_state_t* state, uint32_t ifIndex, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    struct getWirelessIfacesData_s requestData = {
        .nrIfacesMax = 1,
        .nrIfaces = 0,
        .pIfaces = calloc(1, sizeof(wld_nl80211_ifaceInfo_t)),
    };
    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_INTERFACE, 0,
                                           ifIndex, NULL, s_getInterfaceInfoCb, &requestData);
    if(requestData.nrIfaces == 0) {
        SAH_TRACEZ_ERROR(ME, "no AP/Station interface with ifIndex(%d)", ifIndex);
        rc = SWL_RC_ERROR;
    } else if(pIfaceInfo) {
        memcpy(pIfaceInfo, requestData.pIfaces, sizeof(wld_nl80211_ifaceInfo_t));
    }
    free(requestData.pIfaces);
    return rc;
}

swl_rc_ne wld_nl80211_newInterface(wld_nl80211_state_t* state, uint32_t ifIndex, const char* ifName,
                                   const swl_macBin_t* pMac, bool isAp, bool isSta,
                                   wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(ifName, rc, ME, "NULL");
    ASSERT_NOT_EQUALS(ifName[0], 0, rc, ME, "empty name");
    ASSERT_TRUE((isAp || isSta), rc, ME, "invalid type");
    //no nl80211 interface type for mixed apsta mode
    uint32_t ifType = (isAp ? NL80211_IFTYPE_AP : NL80211_IFTYPE_STATION);
    NL_ATTRS(attribs,
             ARR(NL_ATTR_DATA(NL80211_ATTR_IFNAME, strlen(ifName) + 1, ifName),
                 NL_ATTR_VAL(NL80211_ATTR_IFTYPE, ifType),
                 //make the socket owning the interface
                 //to make the interface being destroyed when the socket is closed
                 NL_ATTR(NL80211_ATTR_SOCKET_OWNER)));
    //add mac if provided and valid; driver may support setting mac on interface creation
    if(pMac && !swl_mac_binIsBroadcast(pMac) && !swl_mac_binIsNull(pMac)) {
        NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_MAC, SWL_MAC_BIN_LEN, pMac->bMac));
    }
    struct getWirelessIfacesData_s requestData = {
        .nrIfacesMax = 1,
        .nrIfaces = 0,
        .pIfaces = calloc(1, sizeof(wld_nl80211_ifaceInfo_t)),
    };
    rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_NEW_INTERFACE, 0,
                                 ifIndex, &attribs, s_getInterfaceInfoCb, &requestData);
    NL_ATTRS_CLEAR(&attribs);
    if((requestData.nrIfaces > 0) && pIfaceInfo) {
        memcpy(pIfaceInfo, requestData.pIfaces, sizeof(wld_nl80211_ifaceInfo_t));
    }
    free(requestData.pIfaces);
    return rc;
}

swl_rc_ne wld_nl80211_delInterface(wld_nl80211_state_t* state, uint32_t ifIndex) {
    return wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_DEL_INTERFACE, 0, ifIndex, NULL);
}

swl_rc_ne wld_nl80211_registerFrame(wld_nl80211_state_t* state, uint32_t ifIndex, uint16_t type, const char* pattern, size_t patternLen) {
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_FRAME_TYPE, type),
                 NL_ATTR_DATA(NL80211_ATTR_FRAME_MATCH, patternLen, pattern)));
    swl_rc_ne rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_REGISTER_ACTION, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

swl_rc_ne wld_nl80211_setInterfaceType(wld_nl80211_state_t* state, uint32_t ifIndex, bool isAp, bool isSta) {
    ASSERT_TRUE((isAp || isSta), WLD_ERROR, ME, "invalid type");
    //no nl80211 interface type for mixed apsta mode
    uint32_t attVal = (isAp ? NL80211_IFTYPE_AP : NL80211_IFTYPE_STATION);
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_IFTYPE, attVal)));
    swl_rc_ne rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_SET_INTERFACE, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

swl_rc_ne wld_nl80211_setInterfaceUse4Mac(wld_nl80211_state_t* state, uint32_t ifIndex, bool use4Mac) {
    uint8_t attVal = use4Mac;
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_4ADDR, attVal)));
    swl_rc_ne rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_SET_INTERFACE, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

struct getWiphyData_s {
    const uint32_t nrWiphyMax;
    uint32_t nrWiphy;
    wld_nl80211_wiphyInfo_t* pWiphys;
    uint32_t ifIndex;
};
static swl_rc_ne s_getWiphyInfoCb(swl_rc_ne rc, struct nlmsghdr* nlh, void* priv) {
    ASSERTS_FALSE((rc <= SWL_RC_ERROR), rc, ME, "Request error");
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_EQUALS(nlh->nlmsg_type, g_nl80211DriverIDs.family_id, SWL_RC_OK, ME, "skip msgtype %d", nlh->nlmsg_type);
    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
    ASSERTI_EQUALS(gnlh->cmd, NL80211_CMD_NEW_WIPHY, SWL_RC_OK, ME, "unexpected cmd %d", gnlh->cmd);
    struct getWiphyData_s* requestData = (struct getWiphyData_s*) priv;
    ASSERT_NOT_NULL(requestData, SWL_RC_ERROR, ME, "No request data");
    // Parse the netlink message
    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};
    if(nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse netlink message");
        return SWL_RC_ERROR;
    }
    ASSERT_NOT_NULL(tb[NL80211_ATTR_GENERATION], SWL_RC_ERROR, ME, "missing genId");
    uint32_t genId = nla_get_u32(tb[NL80211_ATTR_GENERATION]);
    wld_nl80211_wiphyInfo_t* pWiphy = &requestData->pWiphys[0];
    if(requestData->nrWiphy > 0) {
        pWiphy = &requestData->pWiphys[requestData->nrWiphy - 1];
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    if((pWiphy->genId > 0) &&
       (((pWiphy->genId != genId) && (pWiphy->wiphy == wiphy)) ||
        ((pWiphy->genId == genId) && (pWiphy->wiphy != wiphy) && (requestData->ifIndex != 0)))) {
        SAH_TRACEZ_ERROR(ME, "invalid genId(%d) for received msg of wiphy(%d)", genId, wiphy);
        return SWL_RC_ERROR;
    }
    if((pWiphy->genId != genId) || ((requestData->ifIndex == 0) && (pWiphy->wiphy != wiphy))) {
        if(requestData->nrWiphy >= requestData->nrWiphyMax) {
            SAH_TRACEZ_INFO(ME, "wiphy(%d) skipped: maxWiphys %d reached", wiphy, requestData->nrWiphyMax);
            return SWL_RC_DONE;
        }
        pWiphy = &requestData->pWiphys[requestData->nrWiphy++];
        pWiphy->genId = genId;
    }
    rc = wld_nl80211_parseWiphyInfo(tb, pWiphy);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    return SWL_RC_OK;
}
swl_rc_ne wld_nl80211_getWiphyInfo(wld_nl80211_state_t* state, uint32_t ifIndex, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    NL_ATTRS(attribs,
             ARR(NL_ATTR(NL80211_ATTR_SPLIT_WIPHY_DUMP)));
    struct getWiphyData_s requestData = {
        .nrWiphyMax = 1,
        .nrWiphy = 0,
        .pWiphys = calloc(1, sizeof(wld_nl80211_wiphyInfo_t)),
        .ifIndex = ifIndex,
    };
    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_WIPHY, NLM_F_DUMP,
                                           ifIndex, &attribs, s_getWiphyInfoCb, &requestData);
    NL_ATTRS_CLEAR(&attribs);
    if(requestData.nrWiphy == 0) {
        SAH_TRACEZ_ERROR(ME, "no Wiphy found for ifIndex(%d)", ifIndex);
        rc = SWL_RC_ERROR;
    } else if(pWiphyInfo) {
        memcpy(pWiphyInfo, &requestData.pWiphys[0], sizeof(wld_nl80211_wiphyInfo_t));
    }
    free(requestData.pWiphys);
    return rc;
}

swl_rc_ne wld_nl80211_getAllWiphyInfo(wld_nl80211_state_t* state, const uint32_t nrWiphyMax, wld_nl80211_wiphyInfo_t pWiphyIfs[nrWiphyMax], uint32_t* pNrWiphy) {
    memset(pWiphyIfs, 0, nrWiphyMax * sizeof(wld_nl80211_wiphyInfo_t));
    NL_ATTRS(attribs,
             ARR(NL_ATTR(NL80211_ATTR_SPLIT_WIPHY_DUMP)));
    struct getWiphyData_s requestData = {
        .nrWiphyMax = nrWiphyMax,
        .nrWiphy = 0,
        .pWiphys = calloc(nrWiphyMax, sizeof(wld_nl80211_wiphyInfo_t)),
        .ifIndex = 0,
    };
    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_WIPHY, NLM_F_DUMP,
                                           0, &attribs, s_getWiphyInfoCb, &requestData);
    NL_ATTRS_CLEAR(&attribs);
    if(pNrWiphy != NULL) {
        *pNrWiphy = requestData.nrWiphy;
    }
    if(requestData.nrWiphy == 0) {
        SAH_TRACEZ_ERROR(ME, "no Wiphy found");
        rc = SWL_RC_ERROR;
    } else if(nrWiphyMax > 0) {
        //reverse copy to restore proper detection order
        for(uint32_t i = 0; i < requestData.nrWiphy; i++) {
            pWiphyIfs[i] = requestData.pWiphys[requestData.nrWiphy - i - 1];
        }
    }
    free(requestData.pWiphys);
    return rc;
}

struct getStationData_s {
    const uint32_t nrStationMax;
    uint32_t nrStation;
    wld_nl80211_stationInfo_t* pStationInfo;
};

static swl_rc_ne s_getStationInfoCb(swl_rc_ne rc, struct nlmsghdr* nlh, void* priv) {
    ASSERTS_FALSE((rc <= SWL_RC_ERROR), rc, ME, "Request error");
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_EQUALS(nlh->nlmsg_type, g_nl80211DriverIDs.family_id, SWL_RC_OK, ME, "skip msgtype %d", nlh->nlmsg_type);

    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
    ASSERTI_EQUALS(gnlh->cmd, NL80211_CMD_NEW_STATION, SWL_RC_OK, ME, "unexpected cmd %d", gnlh->cmd);

    struct getStationData_s* requestData = (struct getStationData_s*) priv;
    ASSERT_NOT_NULL(requestData, SWL_RC_ERROR, ME, "No request data");

    // Parse the netlink message
    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};
    if(nla_parse(tb,
                 NL80211_ATTR_MAX,
                 genlmsg_attrdata(gnlh, 0),
                 genlmsg_attrlen(gnlh, 0),
                 NULL)
       ) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse netlink message");
        return SWL_RC_ERROR;
    }

    wld_nl80211_stationInfo_t stationInfo;
    memset(&stationInfo, 0, sizeof(stationInfo));
    rc = wld_nl80211_parseStationInfo(tb, &stationInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing station info failed");

    if((requestData->pStationInfo == NULL) || (requestData->nrStationMax == 0)) {
        SAH_TRACEZ_INFO(ME, "No device associated available");
        return SWL_RC_OK;
    }
    if(requestData->nrStation >= requestData->nrStationMax) {
        SAH_TRACEZ_INFO(ME, "Device skipped: maxStation %d reached", requestData->nrStationMax);
        return SWL_RC_DONE;
    }

    wld_nl80211_stationInfo_t* pStationInfo = &requestData->pStationInfo[requestData->nrStation];
    memcpy(pStationInfo, &stationInfo, sizeof(*pStationInfo));
    requestData->nrStation++;

    return rc;
}

swl_rc_ne wld_nl80211_getStationInfo(wld_nl80211_state_t* state, uint32_t ifIndex, const swl_macBin_t* pMac, wld_nl80211_stationInfo_t* pStationInfo) {

    ASSERT_NOT_NULL(pMac, SWL_RC_INVALID_PARAM, ME, "NULL");
    NL_ATTRS(attribs, ARR(NL_ATTR_DATA(NL80211_ATTR_MAC, SWL_MAC_BIN_LEN, pMac)));

    struct getStationData_s requestData = {
        .nrStationMax = 1,
        .nrStation = 0,
        .pStationInfo = calloc(1, sizeof(wld_nl80211_stationInfo_t)),
    };

    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_STATION, 0, ifIndex, &attribs, s_getStationInfoCb, &requestData);
    NL_ATTRS_CLEAR(&attribs);

    if(requestData.nrStation == 0) {
        SAH_TRACEZ_NOTICE(ME, "no Station " SWL_MAC_FMT " with ifIndex(%d)", SWL_MAC_ARG(pMac->bMac), ifIndex);
        rc = SWL_RC_INVALID_PARAM;
    } else if(pStationInfo) {
        memcpy(pStationInfo, requestData.pStationInfo, sizeof(wld_nl80211_stationInfo_t));
    }
    free(requestData.pStationInfo);
    return rc;
}

swl_rc_ne wld_nl80211_getAllStationsInfo(wld_nl80211_state_t* state, uint32_t ifIndex, wld_nl80211_stationInfo_t** ppStationInfo, uint32_t* pnStation) {

    struct getStationData_s requestData = {
        .nrStationMax = MAXNROF_STAENTRY,
        .nrStation = 0,
        .pStationInfo = calloc(MAXNROF_STAENTRY, sizeof(wld_nl80211_stationInfo_t)),
    };

    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_STATION, NLM_F_DUMP, ifIndex, NULL, s_getStationInfoCb, &requestData);

    if(requestData.nrStation == 0) {
        SAH_TRACEZ_NOTICE(ME, "no Station device with ifIndex(%d)", ifIndex);
        rc = SWL_RC_OK;
    } else if((ppStationInfo != NULL) && (pnStation != NULL)) {
        *ppStationInfo = calloc(requestData.nrStation, sizeof(wld_nl80211_stationInfo_t));
        if(*ppStationInfo == NULL) {
            SAH_TRACEZ_ERROR(ME, "Fail to allocate memory for station info results");
            rc = SWL_RC_ERROR;
        } else {
            *pnStation = requestData.nrStation;
            memcpy(*ppStationInfo, requestData.pStationInfo, requestData.nrStation * sizeof(wld_nl80211_stationInfo_t));
        }
    }
    free(requestData.pStationInfo);
    return rc;
}

struct getSurveyInfoData_s {
    const uint32_t nrChanSurveyInfoMax;
    uint32_t nrChanSurveyInfo;
    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo;
};

static swl_rc_ne s_getChanSurveyInfoCb(swl_rc_ne rc, struct nlmsghdr* nlh, void* priv) {
    ASSERTS_FALSE((rc <= SWL_RC_ERROR), rc, ME, "Request error");
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_EQUALS(nlh->nlmsg_type, g_nl80211DriverIDs.family_id, SWL_RC_OK, ME, "skip msgtype %d", nlh->nlmsg_type);
    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
    ASSERTI_EQUALS(gnlh->cmd, NL80211_CMD_NEW_SURVEY_RESULTS, SWL_RC_OK, ME, "unexpected cmd %d", gnlh->cmd);

    struct getSurveyInfoData_s* requestData = (struct getSurveyInfoData_s*) priv;
    ASSERT_NOT_NULL(requestData, SWL_RC_ERROR, ME, "No request data");

    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};

    if(nla_parse(tb,
                 NL80211_ATTR_MAX,
                 genlmsg_attrdata(gnlh, 0),
                 genlmsg_attrlen(gnlh, 0),
                 NULL)
       ) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse netlink message");
        return SWL_RC_ERROR;
    }

    // Parse the netlink message
    wld_nl80211_channelSurveyInfo_t chanSurveyInfo;
    memset(&chanSurveyInfo, 0, sizeof(chanSurveyInfo));
    rc = wld_nl80211_parseChanSurveyInfo(tb, &chanSurveyInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing channel survey info failed");

    if((requestData->pChanSurveyInfo == NULL) || (requestData->nrChanSurveyInfoMax == 0)) {
        SAH_TRACEZ_INFO(ME, "No memory available for saving channel survey info");
        return SWL_RC_OK;
    }
    if(requestData->nrChanSurveyInfo >= requestData->nrChanSurveyInfoMax) {
        SAH_TRACEZ_INFO(ME, "Result skipped: maxChanInfo %d reached", requestData->nrChanSurveyInfoMax);
        return SWL_RC_DONE;
    }

    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo = &requestData->pChanSurveyInfo[requestData->nrChanSurveyInfo];
    memcpy(pChanSurveyInfo, &chanSurveyInfo, sizeof(*pChanSurveyInfo));
    requestData->nrChanSurveyInfo++;

    return rc;
}

swl_rc_ne wld_nl80211_getSurveyInfo(wld_nl80211_state_t* state, uint32_t ifIndex, wld_nl80211_channelSurveyInfo_t** ppChanSurveyInfo, uint32_t* pnChanSurveyInfo) {

    struct getSurveyInfoData_s requestData = {
        .nrChanSurveyInfoMax = WLD_MAX_POSSIBLE_CHANNELS,
        .nrChanSurveyInfo = 0,
        .pChanSurveyInfo = calloc(WLD_MAX_POSSIBLE_CHANNELS, sizeof(wld_nl80211_channelSurveyInfo_t)),
    };

    swl_rc_ne rc = wld_nl80211_sendCmdSync(state, NL80211_CMD_GET_SURVEY, NLM_F_DUMP, ifIndex, NULL, s_getChanSurveyInfoCb, &requestData);

    if(requestData.nrChanSurveyInfo == 0) {
        SAH_TRACEZ_INFO(ME, "no Channel survey info with ifIndex(%d)", ifIndex);
        rc = SWL_RC_OK;
    } else if((ppChanSurveyInfo != NULL) && (pnChanSurveyInfo != NULL)) {
        *ppChanSurveyInfo = calloc(requestData.nrChanSurveyInfo, sizeof(wld_nl80211_channelSurveyInfo_t));
        if(*ppChanSurveyInfo == NULL) {
            SAH_TRACEZ_ERROR(ME, "Fail to allocate memory for station info results");
            rc = SWL_RC_ERROR;
        } else {
            *pnChanSurveyInfo = requestData.nrChanSurveyInfo;
            memcpy(*ppChanSurveyInfo, requestData.pChanSurveyInfo, requestData.nrChanSurveyInfo * sizeof(wld_nl80211_channelSurveyInfo_t));
        }
    }
    free(requestData.pChanSurveyInfo);
    return rc;
}

swl_rc_ne wld_nl80211_setWiphyAntennas(wld_nl80211_state_t* state, uint32_t ifIndex, uint32_t txMapAnt, uint32_t rxMapAnt) {
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_TX, txMapAnt),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_RX, rxMapAnt)));
    swl_rc_ne rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_SET_WIPHY, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

static swl_rc_ne s_setTxPower(wld_nl80211_state_t* state, uint32_t ifIndex, enum nl80211_tx_power_setting type, int32_t dbm) {
    NL_ATTRS(attribs, ARR(NL_ATTR_VAL(NL80211_ATTR_WIPHY_TX_POWER_SETTING, type)));
    if(type != NL80211_TX_POWER_AUTOMATIC) {
        dbm *= 100;
        NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_WIPHY_TX_POWER_LEVEL, dbm));
    }
    swl_rc_ne rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_SET_WIPHY, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

swl_rc_ne wld_nl80211_setTxPowerAuto(wld_nl80211_state_t* state, uint32_t ifIndex) {
    return s_setTxPower(state, ifIndex, NL80211_TX_POWER_AUTOMATIC, 0);
}

swl_rc_ne wld_nl80211_setTxPowerFixed(wld_nl80211_state_t* state, uint32_t ifIndex, int32_t dbm) {
    return s_setTxPower(state, ifIndex, NL80211_TX_POWER_FIXED, dbm);
}

swl_rc_ne wld_nl80211_setTxPowerLimited(wld_nl80211_state_t* state, uint32_t ifIndex, int32_t dbm) {
    return s_setTxPower(state, ifIndex, NL80211_TX_POWER_LIMITED, dbm);
}

swl_rc_ne wld_nl80211_getTxPower(wld_nl80211_state_t* state, uint32_t ifIndex, int32_t* dbm) {
    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_rc_ne rc = wld_nl80211_getInterfaceInfo(state, ifIndex, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get radio main iface info");
    *dbm = ifaceInfo.txPower;
    return rc;
}

static uint32_t s_getScanFlags(wld_nl80211_scanFlags_t* pFlags) {
    uint32_t flags = 0;
    ASSERTS_NOT_NULL(pFlags, flags, ME, "NULL");
    if(pFlags->flush) {
        flags |= NL80211_SCAN_FLAG_FLUSH;
    }
    if(pFlags->force) {
        flags |= NL80211_SCAN_FLAG_AP;
    }
    return flags;
}
swl_rc_ne wld_nl80211_startScan(wld_nl80211_state_t* state, uint32_t ifIndex, wld_nl80211_scanParams_t* params) {
    SAH_TRACEZ_IN(ME);
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    NL_ATTRS(attribs, ARR());
    if(params) {
        uint32_t flags = s_getScanFlags(&params->flags);
        if(flags) {
            NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_SCAN_FLAGS, flags));
            SAH_TRACEZ_INFO(ME, "iface:%d, added scan flags attribute(%d)", ifIndex, swl_unLiList_size(&attribs));
        }
        swl_unLiListIt_t it;
        if(swl_unLiList_size(&params->ssids) > 0) {
            NL_ATTR_NESTED(ssidsAttr, NL80211_ATTR_SCAN_SSIDS);
            SAH_TRACEZ_INFO(ME, "Trying to add scan ssids(%d)", swl_unLiList_size(&params->ssids));
            swl_unLiList_for_each(it, &params->ssids) {
                char* ssid = *(swl_unLiList_data(&it, char**));
                if(ssid == NULL) {
                    continue;
                }
                SAH_TRACEZ_INFO(ME, "Scan for ssid %s ", ssid);
                NL_ATTRS_ADD(&ssidsAttr.data.attribs,
                             NL_ATTR_DATA(swl_unLiList_size(&ssidsAttr.data.attribs) + 1, strlen(ssid), ssid));
            }
            if(swl_unLiList_size(&ssidsAttr.data.attribs) > 0) {
                swl_unLiList_add(&attribs, &ssidsAttr);
            } else {
                wld_nl80211_cleanNlAttr(&ssidsAttr);
            }
        }

        if(swl_mac_binIsNull(&params->bssid) == false) {
            NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_MAC, SWL_MAC_BIN_LEN, params->bssid.bMac));//some legacy implementations use NL80211_ATTR_MAC instead of NL80211_ATTR_BSSID
            NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_BSSID, SWL_MAC_BIN_LEN, params->bssid.bMac));
            SAH_TRACEZ_INFO(ME, "Scan for bssid  "SWL_MAC_FMT "", SWL_MAC_ARG(params->bssid.bMac));

        }

        if(swl_unLiList_size(&params->freqs) > 0) {
            NL_ATTR_NESTED(freqsAttr, NL80211_ATTR_SCAN_FREQUENCIES);
            swl_unLiList_for_each(it, &params->freqs) {
                uint32_t* pFreq = swl_unLiList_data(&it, uint32_t*);
                if((pFreq == NULL) || (*pFreq == 0)) {
                    continue;
                }
                SAH_TRACEZ_INFO(ME, "Scan over frequency %u MHz", *pFreq);
                NL_ATTRS_ADD(&freqsAttr.data.attribs,
                             NL_ATTR_DATA(swl_unLiList_size(&freqsAttr.data.attribs) + 1, sizeof(*pFreq), pFreq));
            }
            if(swl_unLiList_size(&freqsAttr.data.attribs) > 0) {
                swl_unLiList_add(&attribs, &freqsAttr);
            } else {
                wld_nl80211_cleanNlAttr(&freqsAttr);
            }
        }

        if((params->iesLen > 0) && (params->ies != NULL)) {
            NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_IE, params->iesLen, params->ies));
            SAH_TRACEZ_INFO(ME, "Scan probe with extra IEs");
        }
    }
    rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_TRIGGER_SCAN, 0, ifIndex, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

swl_rc_ne wld_nl80211_abortScan(wld_nl80211_state_t* state, uint32_t ifIndex) {
    return wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_ABORT_SCAN, 0, ifIndex, NULL);
}

struct getScanResultsData_s {
    scanResultsCb_f fScanResultsCb;
    T_ScanResults results;
    void* priv;
};
static swl_rc_ne s_scanResultCb(swl_rc_ne rc, struct nlmsghdr* nlh, void* priv) {
    T_ScanResult_SSID* pResult = NULL;
    struct getScanResultsData_s* requestData = (struct getScanResultsData_s*) priv;
    if(rc <= SWL_RC_ERROR) {
        goto scanFinish;
    }
    if(nlh->nlmsg_type == NLMSG_DONE) {
        rc = SWL_RC_DONE;
        goto scanFinish;
    }
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.scan_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(nlh);
    ASSERTI_EQUALS(gnlh->cmd, NL80211_CMD_NEW_SCAN_RESULTS, SWL_RC_OK, ME, "unexpected cmd %d", gnlh->cmd);
    T_ScanResult_SSID result;
    memset(&result, 0, sizeof(result));
    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};
    if((nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL) != 0) ||
       ((rc = wld_nl80211_parseScanResult(tb, &result)) < SWL_RC_OK)) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nl msg evt(%d)", gnlh->cmd);
        goto scanFinish;
    }
    if(rc == SWL_RC_CONTINUE) {
        SAH_TRACEZ_INFO(ME, "skip nl msg due to partial parsing");
    } else if(requestData) {
        pResult = calloc(1, sizeof(T_ScanResult_SSID));
        if(pResult == NULL) {
            SAH_TRACEZ_ERROR(ME, "fail to alloc scan result element");
            goto scanFinish;
        }
        memcpy(pResult, &result, sizeof(T_ScanResult_SSID));
        amxc_llist_it_init(&pResult->it);
        amxc_llist_append(&requestData->results.ssids, &pResult->it);
    }
    ASSERTS_FALSE((nlh->nlmsg_flags & NLM_F_MULTI), SWL_RC_OK, ME, "expecting other nl msg");
scanFinish:
    ASSERTS_NOT_NULL(requestData, rc, ME, "no request data");
    SAH_TRACEZ_INFO(ME, "rc:%d nResults:%d", rc, (int) amxc_llist_size(&requestData->results.ssids));
    amxc_llist_for_each(it, &requestData->results.ssids) {
        pResult = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        SAH_TRACEZ_INFO(ME, "scan result entry: bssid("SWL_MAC_FMT ") ssid(%s) signal(%d dbm)",
                        SWL_MAC_ARG(pResult->bssid.bMac), pResult->ssid, pResult->rssi);
    }
    if(requestData->fScanResultsCb) {
        requestData->fScanResultsCb(requestData->priv, rc, &requestData->results);
    }
    SAH_TRACEZ_INFO(ME, "clean request data");
    amxc_llist_for_each(it, &requestData->results.ssids) {
        pResult = amxc_llist_it_get_data(it, T_ScanResult_SSID, it);
        amxc_llist_it_take(&pResult->it);
        free(pResult);
    }
    free(requestData);
    return rc;
}

swl_rc_ne wld_nl80211_getScanResults(wld_nl80211_state_t* state, uint32_t ifIndex, void* priv, scanResultsCb_f fScanResultsCb) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(state, rc, ME, "NULL");
    struct getScanResultsData_s* pScanResultsData = calloc(1, sizeof(struct getScanResultsData_s));
    if(pScanResultsData == NULL) {
        SAH_TRACEZ_ERROR(ME, "Fail to alloc getScanResults req data");
        if(fScanResultsCb) {
            fScanResultsCb(priv, rc, NULL);
        }
        return rc;
    }
    pScanResultsData->fScanResultsCb = fScanResultsCb;
    amxc_llist_init(&pScanResultsData->results.ssids);
    pScanResultsData->priv = priv;
    rc = wld_nl80211_sendCmd(false, state, NL80211_CMD_GET_SCAN, NLM_F_DUMP, ifIndex, NULL, s_scanResultCb, pScanResultsData, NULL);
    return rc;
}

swl_rc_ne wld_nl80211_setRegDomain(wld_nl80211_state_t* state, uint32_t wiphy, const char* alpha2) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_EQUALS(swl_str_len(alpha2), 2, rc, ME, "invalid alpha2");
    NL_ATTRS(attribs, ARR());
    if((wiphy != WLD_NL80211_ID_ANY) && (wiphy != WLD_NL80211_ID_UNDEF)) {
        //No specific wiphy for global regulatory domain
        NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_WIPHY, wiphy));
    }
    NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_REG_ALPHA2, swl_str_len(alpha2) + 1, alpha2));
    rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_REQ_SET_REG, 0, 0, &attribs);
    NL_ATTRS_CLEAR(&attribs);
    return rc;
}

swl_rc_ne wld_nl80211_sendVendorSubCmd(wld_nl80211_state_t* state, uint32_t oui, int subcmd, void* data, int dataLen,
                                       bool isSync, bool withAck, uint32_t flags, uint32_t ifIndex, uint64_t wDevId,
                                       wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(state, rc, ME, "NULL");
    ASSERT_FALSE((wDevId == 0) && (ifIndex == 0), rc, ME, "devices wDevId and index are 0");

    NL_ATTRS(attribs,
             ARR(
                 NL_ATTR_VAL(NL80211_ATTR_VENDOR_ID, oui),
                 NL_ATTR_VAL(NL80211_ATTR_VENDOR_SUBCMD, subcmd)
                 )
             );
    if(wDevId) {
        NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_WDEV, wDevId));
    }
    if(dataLen) {
        NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_VENDOR_DATA, dataLen, data));
    }

    if(withAck) {
        rc = wld_nl80211_sendCmdSyncWithAck(state, NL80211_CMD_VENDOR, flags, ifIndex, &attribs);
    } else {
        rc = wld_nl80211_sendCmd(isSync, state, NL80211_CMD_VENDOR, flags, ifIndex, &attribs, handler, priv, NULL);
    }
    NL_ATTRS_CLEAR(&attribs);

    return rc;
}
