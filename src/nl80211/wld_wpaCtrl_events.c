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
/* Extension functions get an integer value */
int wld_wpaCtrl_getValueInt(const char* pData, const char* pKey) {
    char strbuf[16] = {0};
    wld_wpaCtrl_getValueStr(pData, pKey, strbuf, sizeof(strbuf));
    return atoi(strbuf);
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
#define HDLR_OFFSET(hdlr) offsetof(wld_wpaCtrl_evtHandlers_cb, hdlr)

// Call interface handler protected against null interface and null handler
#define CALL_INTF(pIntf, fName, ...) \
    if(pIntf != NULL) { \
        SWL_CALL(pIntf->handlers.fName, pIntf->userData, pIntf->name, __VA_ARGS__); \
    }

// Call interface handler protected against null interface and null handler. Don't add args
#define CALL_INTF_NA(pIntf, fName) \
    if(pIntf != NULL) { \
        SWL_CALL(pIntf->handlers.fName, pIntf->userData, pIntf->name); \
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

static void s_stationConnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-CONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fStationConnectedCb, &bStationMac);
}

static void s_stationDisconnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-DISCONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fStationDisconnectedCb, &bStationMac);
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

SWL_TABLE(sWpaCtrlEvents,
          ARR(char* evtName; void* evtParser; ),
          ARR(swl_type_charPtr, swl_type_voidPtr),
          ARR(
              {"WPS-CANCEL", &s_cancelEvent},
              {"WPS-TIMEOUT", &s_timeoutEvent},
              {"WPS-REG-SUCCESS", &s_wpsSuccess},
              {"AP-STA-CONNECTED", &s_stationConnected},
              {"AP-STA-DISCONNECTED", &s_stationDisconnected},
              {"BSS-TM-RESP", &s_btmResponse}
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
void wld_wpaCtrl_processMsg(wld_wpaCtrlInterface_t* pInterface, char* msgData) {
    ASSERTS_NOT_NULL(pInterface, , ME, "NULL");
    ASSERTS_NOT_NULL(msgData, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: process event (%s)", wld_wpaCtrlInterface_getName(pInterface), msgData);
    s_processEvent(pInterface, msgData);
    //if custom event handler was registered, then call it
    NOTIFY(pInterface, fProcEvtMsg, msgData);
}
