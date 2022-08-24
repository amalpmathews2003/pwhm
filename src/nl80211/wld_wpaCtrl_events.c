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
#include "swl/swl_table.h"
#include "swl/swl_string.h"
#include "wld_channel.h"
#include "wld_wpaCtrlInterface_priv.h"
#include "wld_wpaCtrlMngr_priv.h"
#include "wld_wpaCtrl_events.h"

#define ME "wpaCtrl"

static int s_getValueStr(const char* pData, const char* pKey, char* pValue, int* length) {
    ASSERTS_STR(pData, 0, ME, "Empty data");
    ASSERTS_STR(pKey, 0, ME, "Empty key");
    char* paramStart = strstr(pData, pKey);
    ASSERTI_NOT_NULL(paramStart, 0, ME, "Failed to find token %s", pKey);
    char* paramEnd = &paramStart[strlen(pKey)];
    ASSERTI_EQUALS(paramEnd[0], '=', 0, ME, "partial token %s", pKey);
    char* valueStart = &paramEnd[1];
    char* valueEnd = &paramEnd[strlen(paramEnd)];

    char* p;
    // locate value string end, by looking for next parameter
    // as value string may include spaces
    for(p = strchr(valueStart, '='); (p != NULL) && (p > valueStart); p--) {
        if(isspace(p[0])) {
            break;
        }
    }
    if(p != NULL) {
        valueEnd = p;
    }
    // clear the '\n' and spaces at the end of value string
    for(p = valueEnd; (p != NULL) && (p > valueStart); p--) {
        if(isspace(p[0])) {
            valueEnd = p;
            continue;
        }
        if(p[0] != 0) {
            break;
        }
    }
    ASSERTI_NOT_NULL(valueEnd, 0, ME, "Can locate string value end of token %s", pKey);

    int valueLen = valueEnd - valueStart;
    if(pValue == NULL) {
        if(length) {
            *length = 0;
        }
        return valueLen;
    }
    pValue[0] = 0;
    ASSERTS_NOT_NULL(length, valueLen, ME, "null dest len");
    ASSERTS_TRUE(*length > 0, valueLen, ME, "null dest len");
    if(*length > valueLen) {
        *length = valueLen;
    }
    strncpy(pValue, valueStart, *length);
    pValue[*length] = 0;

    return valueLen;
}

/* Extension functions We don't care about the length string.*/
int wld_wpaCtrl_getValueStr(const char* pData, const char* pKey, char* pValue, int length) {
    return s_getValueStr(pData, pKey, pValue, &length);
}
/* Extension function get an integer value */
bool wld_wpaCtrl_getValueIntExt(const char* pData, const char* pKey, int32_t* pVal) {
    char strbuf[64] = {0};
    ASSERTS_TRUE(wld_wpaCtrl_getValueStr(pData, pKey, strbuf, sizeof(strbuf)) > 0, false,
                 ME, "key (%s) not found", pKey);
    ASSERT_TRUE(swl_typeInt32_fromChar(pVal, strbuf), false,
                ME, "key (%s) val (%s) num conversion failed", pKey, strbuf);
    return true;
}

/* Extension function get an integer value , returns 0 as default value*/
int wld_wpaCtrl_getValueInt(const char* pData, const char* pKey) {
    int32_t val = 0;
    wld_wpaCtrl_getValueIntExt(pData, pKey, &val);
    return val;
}

#define NOTIFY(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        /* for VAP/EP events */ \
        if(PIFACE->handlers.FNAME != NULL) { \
            PIFACE->handlers.FNAME(PIFACE->userData, PIFACE->name, __VA_ARGS__); \
        } \
        /* for RAD/Glob events */ \
        if((PIFACE->pMgr != NULL) && (PIFACE->pMgr->handlers.FNAME != NULL)) { \
            PIFACE->pMgr->handlers.FNAME(PIFACE->pMgr->userData, PIFACE->name, __VA_ARGS__); \
        } \
    }

typedef void (* evtParser_f)(wld_wpaCtrlInterface_t* pInterface, char* event, char* params);

#define CALL_MGR_I(pIntf, fName, ...) \
    if(pIntf != NULL) { \
        CALL_MGR(pIntf->pMgr, pIntf->name, fName, __VA_ARGS__); \
    }

#define CALL_MGR_I_NA(pIntf, fName) \
    if(pIntf != NULL) { \
        CALL_MGR_NA(pIntf->pMgr, pIntf->name, fName); \
    }

static void s_cancelEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS CANCEL", pInterface->name);
    CALL_INTF_NA(pInterface, fWpsCancelMsg);
}

static void s_timeoutEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS TIMEOUT", pInterface->name);
    CALL_INTF_NA(pInterface, fWpsTimeoutMsg);
}

static void s_wpsSuccess(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    char buf[SWL_MAC_CHAR_LEN] = {0};
    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);

    swl_macChar_t mac;
    swl_mac_charToStandard(&mac, buf);

    SAH_TRACEZ_INFO(ME, "%s WPS SUCCESS %s", pInterface->name, mac.cMac);
    CALL_INTF(pInterface, fWpsSuccessMsg, &mac);
}

static void s_apStationConnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-CONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationConnectedCb, &bStationMac);
}

static void s_apStationDisconnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-DISCONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationDisconnectedCb, &bStationMac);
}

static void s_btmResponse(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: BSS-TM-RESP <sta_mac_adr> dialog_token=x status_code=x bss_termination_delay=x target_bssid=<bsssid_mac_adr>
    char buf[SWL_MAC_CHAR_LEN] = {0};
    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);

    swl_macChar_t mac;
    swl_mac_charToStandard(&mac, buf);

    uint8_t replyCode = wld_wpaCtrl_getValueInt(params, "status_code");

    SAH_TRACEZ_INFO(ME, "%s BSS-TM-RESP %s status_code=%d", pInterface->name, mac.cMac, replyCode);

    CALL_INTF(pInterface, fBtmReplyCb, &mac, replyCode);
}

SWL_TABLE(sChWidthMaps,
          ARR(uint32_t chWidthId; char* chWidthDesc; swl_bandwidth_e swlBw; ),
          ARR(swl_type_uint32, swl_type_charPtr, swl_type_uint32, ),
          ARR({0, "20 MHz (no HT)", SWL_BW_20MHZ}, //CHAN_WIDTH_20_NOHT
              {1, "20 MHz", SWL_BW_20MHZ},         //CHAN_WIDTH_20
              {2, "40 MHz", SWL_BW_40MHZ},         //CHAN_WIDTH_40
              {3, "80 MHz", SWL_BW_80MHZ},         //CHAN_WIDTH_80
              {4, "80+80 MHz", SWL_BW_160MHZ},     //CHAN_WIDTH_80P80
              {5, "160 MHz", SWL_BW_160MHZ},       //CHAN_WIDTH_160
              ));
static swl_rc_ne s_freqParamToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t ctrlFreq = 0;
    ASSERTS_TRUE(wld_wpaCtrl_getValueIntExt(params, key, (int32_t*) &ctrlFreq), SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_chanspec_t chanSpec;
    swl_rc_ne rc = swl_chanspec_channelFromMHz(&chanSpec, ctrlFreq);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get chanspec for freq(%d)", ctrlFreq);
    pChanSpec->channel = chanSpec.channel;
    pChanSpec->band = chanSpec.band;
    return SWL_RC_OK;
}
static swl_rc_ne s_chWidthDescToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    char chWidthStr[64] = {0};
    ASSERTS_TRUE(wld_wpaCtrl_getValueStr(params, key, chWidthStr, sizeof(chWidthStr)) > 0, SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_bandwidth_e* pBwEnu = (swl_bandwidth_e*) swl_table_getMatchingValue(&sChWidthMaps, 2, 1, chWidthStr);
    ASSERTS_NOT_NULL(pBwEnu, SWL_RC_ERROR, ME, "unknown channel width desc (%s)", chWidthStr);
    pChanSpec->bandwidth = *pBwEnu;
    return SWL_RC_OK;
}
static swl_rc_ne s_chWidthIdToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t chWId = 0;
    ASSERTS_TRUE(wld_wpaCtrl_getValueIntExt(params, key, (int32_t*) &chWId), SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_bandwidth_e* pBwEnu = (swl_bandwidth_e*) swl_table_getMatchingValue(&sChWidthMaps, 2, 0, &chWId);
    ASSERTS_NOT_NULL(pBwEnu, SWL_RC_ERROR, ME, "unknown channel width id (%d)", chWId);
    pChanSpec->bandwidth = *pBwEnu;
    return SWL_RC_OK;
}

static void s_chanSwitchStartedEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: CTRL-EVENT-STARTED-CHANNEL-SWITCH freq=5260 ht_enabled=1 ch_offset=1 ch_width=80 MHz cf1=5290 cf2=0 dfs=1
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthDescToChanSpec(params, "ch_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: channel=%d width=%d band=%d", pInterface->name, chanSpec.channel,
                    chanSpec.bandwidth, chanSpec.band);

    CALL_MGR_I(pInterface, fChanSwitchStartedCb, &chanSpec);
}

static void s_chanSwitchEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: CTRL-EVENT-CHANNEL-SWITCH freq=2427 ht_enabled=0 ch_offset=0 ch_width=20 MHz (no HT) cf1=2427 cf2=0 dfs=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthDescToChanSpec(params, "ch_width", &chanSpec);

    SAH_TRACEZ_INFO(ME, "%s: channel=%d width=%d band=%d", pInterface->name, chanSpec.channel,
                    chanSpec.bandwidth, chanSpec.band);

    CALL_MGR_I(pInterface, fChanSwitchCb, &chanSpec);
}

static void s_apCsaFinishedEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: AP-CSA-FINISHED freq=2427 dfs=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");

    SAH_TRACEZ_INFO(ME, "%s: channel=%d", pInterface->name, chanSpec.channel);

    CALL_MGR_I(pInterface, fApCsaFinishedCb, &chanSpec);
}

static void s_apEnabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: AP-ENABLED
    // This event notifies that hapd main interface has setup completed
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    if(pInterface == wld_wpaCtrlMngr_getInterface(pInterface->pMgr, 0)) {
        CALL_MGR_I_NA(pInterface, fMainApSetupCompletedCb);
    }
}

static void s_apDisabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: AP-DISABLED
    // This event notifies that hapd main interface is disabled, or that secondary bss is removed from conf
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    if(pInterface == wld_wpaCtrlMngr_getInterface(pInterface->pMgr, 0)) {
        CALL_MGR_I_NA(pInterface, fMainApDisabledCb);
    }
}

static void s_ifaceTerminatingEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: CTRL-EVENT-TERMINATING
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    pInterface->isReady = false;
    wld_wpaCtrlInterface_close(pInterface);
    if(pInterface == wld_wpaCtrlMngr_getInterface(pInterface->pMgr, 0)) {
        CALL_MGR_I(pInterface, fMngrReadyCb, false);
    }
}

static void s_radDfsCacStartedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-CAC-START freq=5500 chan=100 sec_chan=1, width=1, seg0=106, seg1=0, cac_time=60s
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "width", &chanSpec);
    uint32_t cac_time = wld_wpaCtrl_getValueInt(params, "cac_time");
    SAH_TRACEZ_INFO(ME, "%s: cac started on channel=%d for %d sec",
                    pInterface->name, chanSpec.channel, cac_time);
    CALL_MGR_I(pInterface, fDfsCacStartedCb, &chanSpec, cac_time);
}
static void s_radDfsCacDoneEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-CAC-COMPLETED success=1 freq=5500 ht_enabled=0 chan_offset=0 chan_width=3 cf1=5530 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    bool success = wld_wpaCtrl_getValueInt(params, "success");
    SAH_TRACEZ_INFO(ME, "%s: CAC done channel=%d width=%d band=%d result=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band, success);
    CALL_MGR_I(pInterface, fDfsCacDoneCb, &chanSpec, success);
}
static void s_radDfsCacExpiredEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-PRE-CAC-EXPIRED freq=5500 ht_enabled=0 chan_offset=0 chan_width=0 cf1=5500 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: previous CAC expired on channel=%d width=%d band=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band);
    CALL_MGR_I(pInterface, fDfsCacExpiredCb, &chanSpec);
}
static void s_radDfsRadarDetectedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-RADAR-DETECTED freq=5540 ht_enabled=0 chan_offset=0 chan_width=3 cf1=5530 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: DFS radar detected on channel=%d width=%d band=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band);
    CALL_MGR_I(pInterface, fDfsRadarDetectedCb, &chanSpec);
}
static void s_radDfsNopFinishedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-NOP-FINISHED freq=5500 ht_enabled=0 chan_offset=0 chan_width=0 cf1=5500 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: DFS Non-Occupancy Period is over on channel=%d width=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth);
    CALL_MGR_I(pInterface, fDfsNopFinishedCb, &chanSpec);
}

static void s_radDfsNewChannelEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-NEW-CHANNEL freq=5180 chan=36 sec_chan=1
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    SAH_TRACEZ_INFO(ME, "%s: DFS New channel switch expected to channel=%d, after radar detection",
                    pInterface->name, chanSpec.channel);
    CALL_MGR_I(pInterface, fDfsNewChannelCb, &chanSpec);
}

static void s_mgtFrameEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: <3>AP-MGMT-FRAME-RECEIVED buf=b0003c0000000000001012d4159a59e7000000000010a082000001000000
    char frameControl[4];
    int fc;
    memcpy(frameControl, &params[4], 4);
    sscanf(frameControl, "%x", &fc);
    uint16_t stype = (fc & 0xF000) >> 12;
    SAH_TRACEZ_INFO(ME, "%s MGMT-FRAME stype=%x", pInterface->name, stype);
    CALL_INTF(pInterface, fMgtFrameReceivedCb, stype, params);
}

static void s_stationDisconnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: <3>CTRL-EVENT-DISCONNECTED bssid=xx:xx:xx:xx:xx:xx reason=3 locally_generated=1
    char bssid[SWL_MAC_CHAR_LEN] = {0};
    wld_wpaCtrl_getValueStr(params, "bssid", bssid, sizeof(bssid));
    swl_macBin_t bBssidMac;
    swl_mac_charToBin(&bBssidMac, (swl_macChar_t*) bssid);
    uint8_t reasonCode = wld_wpaCtrl_getValueInt(params, "reason");
    CALL_INTF(pInterface, fStationDisconnectedCb, &bBssidMac, reasonCode);
}

SWL_TABLE(sWpaCtrlEvents,
          ARR(char* evtName; void* evtParser; ),
          ARR(swl_type_charPtr, swl_type_voidPtr),
          ARR(
              {"WPS-CANCEL", &s_cancelEvent},
              {"WPS-TIMEOUT", &s_timeoutEvent},
              {"WPS-REG-SUCCESS", &s_wpsSuccess},
              {"AP-STA-CONNECTED", &s_apStationConnected},
              {"AP-STA-DISCONNECTED", &s_apStationDisconnected},
              {"BSS-TM-RESP", &s_btmResponse},
              {"CTRL-EVENT-STARTED-CHANNEL-SWITCH", &s_chanSwitchStartedEvtCb},
              {"CTRL-EVENT-CHANNEL-SWITCH", &s_chanSwitchEvtCb},
              {"AP-CSA-FINISHED", &s_apCsaFinishedEvtCb},
              {"AP-MGMT-FRAME-RECEIVED", &s_mgtFrameEvt},
              {"AP-ENABLED", &s_apEnabledEvt},
              {"AP-DISABLED", &s_apDisabledEvt},
              {"CTRL-EVENT-TERMINATING", &s_ifaceTerminatingEvt},
              {"DFS-CAC-START", &s_radDfsCacStartedEvt},
              {"DFS-CAC-COMPLETED", &s_radDfsCacDoneEvt},
              {"DFS-PRE-CAC-EXPIRED", &s_radDfsCacExpiredEvt},
              {"DFS-RADAR-DETECTED", &s_radDfsRadarDetectedEvt},
              {"DFS-NOP-FINISHED", &s_radDfsNopFinishedEvt},
              {"DFS-NEW-CHANNEL", &s_radDfsNewChannelEvt},
              {"CTRL-EVENT-DISCONNECTED", &s_stationDisconnected},
              ));

static evtParser_f s_getEventParser(char* eventName) {
    evtParser_f* pfEvtHdlr = (evtParser_f*) swl_table_getMatchingValue(&sWpaCtrlEvents, 1, 0, eventName);
    ASSERTS_NOT_NULL(pfEvtHdlr, NULL, ME, "no internal hdlr defined for evt(%s)", eventName);
    return *pfEvtHdlr;
}

#define WPA_MSG_LEVEL_INFO "<3>"
void s_processEvent(wld_wpaCtrlInterface_t* pInterface, char* msgData) {
    // All wpa msgs including events are sent with level MSG_INFO (3)
    char* pEvent = strstr(msgData, WPA_MSG_LEVEL_INFO);
    ASSERTS_NOT_NULL(pEvent, , ME, "%s: this is not a wpa_ctrl event %s", wld_wpaCtrlInterface_getName(pInterface), msgData);
    pEvent += strlen(WPA_MSG_LEVEL_INFO);
    // Look for the event
    uint32_t eventNameLen = strlen(pEvent);
    char* pParams = strchr(pEvent, ' ');
    if(pParams) {
        eventNameLen = pParams - pEvent;
        pParams++;
    }
    char eventName[eventNameLen + 1];
    swl_str_copy(eventName, sizeof(eventName), pEvent);
    evtParser_f fEvtParser = s_getEventParser(eventName);
    ASSERTS_NOT_NULL(fEvtParser, , ME, "No parser for evt(%s)", eventName);
    fEvtParser(pInterface, eventName, pParams);
}

/**
 * @brief process msgData received from wpa_ctrl server
 *
 * @param pInterface Pointer to source interface
 * @param msgData the event received
 *
 * @return void
 */
void wld_wpaCtrl_processMsg(wld_wpaCtrlInterface_t* pInterface, char* msgData, size_t len) {
    ASSERTS_NOT_NULL(pInterface, , ME, "NULL");
    ASSERTS_NOT_NULL(msgData, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: process event len: %zu (%s)", wld_wpaCtrlInterface_getName(pInterface), len, msgData);
    s_processEvent(pInterface, msgData);
    //if custom event handler was registered, then call it
    NOTIFY(pInterface, fProcEvtMsg, msgData);
}
