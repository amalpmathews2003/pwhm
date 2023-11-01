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
#include <arpa/inet.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_wps.h"
#include "swl/swl_hex.h"
#include "swl/swl_tlv.h"

#define ME "wSupPsr"

/* wps specification */
enum wps_wpasupplicant_attr {
    WPS_ATTR_AP_CHANNEL = 0x1001,
    WPS_ATTR_ASSOC_STATE = 0x1002,
    WPS_ATTR_AUTH_TYPE = 0x1003,
    WPS_ATTR_AUTH_TYPE_FLAGS = 0x1004,
    WPS_ATTR_AUTHENTICATOR = 0x1005,
    WPS_ATTR_CONFIG_METHODS = 0x1008,
    WPS_ATTR_CONFIG_ERROR = 0x1009,
    WPS_ATTR_CONFIRM_URL4 = 0x100a,
    WPS_ATTR_CONFIRM_URL6 = 0x100b,
    WPS_ATTR_CONN_TYPE = 0x100c,
    WPS_ATTR_CONN_TYPE_FLAGS = 0x100d,
    WPS_ATTR_CRED = 0x100e,
    WPS_ATTR_ENCR_TYPE = 0x100f,
    WPS_ATTR_ENCR_TYPE_FLAGS = 0x1010,
    WPS_ATTR_DEV_NAME = 0x1011,
    WPS_ATTR_DEV_PASSWORD_ID = 0x1012,
    WPS_ATTR_E_HASH1 = 0x1014,
    WPS_ATTR_E_HASH2 = 0x1015,
    WPS_ATTR_E_SNONCE1 = 0x1016,
    WPS_ATTR_E_SNONCE2 = 0x1017,
    WPS_ATTR_ENCR_SETTINGS = 0x1018,
    WPS_ATTR_ENROLLEE_NONCE = 0x101a,
    WPS_ATTR_FEATURE_ID = 0x101b,
    WPS_ATTR_IDENTITY = 0x101c,
    WPS_ATTR_IDENTITY_PROOF = 0x101d,
    WPS_ATTR_KEY_WRAP_AUTH = 0x101e,
    WPS_ATTR_KEY_ID = 0x101f,
    WPS_ATTR_MAC_ADDR = 0x1020,
    WPS_ATTR_MANUFACTURER = 0x1021,
    WPS_ATTR_MSG_TYPE = 0x1022,
    WPS_ATTR_MODEL_NAME = 0x1023,
    WPS_ATTR_MODEL_NUMBER = 0x1024,
    WPS_ATTR_NETWORK_INDEX = 0x1026,
    WPS_ATTR_NETWORK_KEY = 0x1027,
    WPS_ATTR_NETWORK_KEY_INDEX = 0x1028,
    WPS_ATTR_NEW_DEVICE_NAME = 0x1029,
    WPS_ATTR_NEW_PASSWORD = 0x102a,
    WPS_ATTR_OOB_DEVICE_PASSWORD = 0x102c,
    WPS_ATTR_OS_VERSION = 0x102d,
    WPS_ATTR_POWER_LEVEL = 0x102f,
    WPS_ATTR_PSK_CURRENT = 0x1030,
    WPS_ATTR_PSK_MAX = 0x1031,
    WPS_ATTR_PUBLIC_KEY = 0x1032,
    WPS_ATTR_RADIO_ENABLE = 0x1033,
    WPS_ATTR_REBOOT = 0x1034,
    WPS_ATTR_REGISTRAR_CURRENT = 0x1035,
    WPS_ATTR_REGISTRAR_ESTABLISHED = 0x1036,
    WPS_ATTR_REGISTRAR_LIST = 0x1037,
    WPS_ATTR_REGISTRAR_MAX = 0x1038,
    WPS_ATTR_REGISTRAR_NONCE = 0x1039,
    WPS_ATTR_REQUEST_TYPE = 0x103a,
    WPS_ATTR_RESPONSE_TYPE = 0x103b,
    WPS_ATTR_RF_BANDS = 0x103c,
    WPS_ATTR_R_HASH1 = 0x103d,
    WPS_ATTR_R_HASH2 = 0x103e,
    WPS_ATTR_R_SNONCE1 = 0x103f,
    WPS_ATTR_R_SNONCE2 = 0x1040,
    WPS_ATTR_SELECTED_REGISTRAR = 0x1041,
    WPS_ATTR_SERIAL_NUMBER = 0x1042,
    WPS_ATTR_WPS_STATE = 0x1044,
    WPS_ATTR_SSID = 0x1045,
    WPS_ATTR_TOTAL_NETWORKS = 0x1046,
    WPS_ATTR_UUID_E = 0x1047,
    WPS_ATTR_UUID_R = 0x1048,
    WPS_ATTR_VENDOR_EXT = 0x1049,
    WPS_ATTR_VERSION = 0x104a,
    WPS_ATTR_X509_CERT_REQ = 0x104b,
    WPS_ATTR_X509_CERT = 0x104c,
    WPS_ATTR_EAP_IDENTITY = 0x104d,
    WPS_ATTR_MSG_COUNTER = 0x104e,
    WPS_ATTR_PUBKEY_HASH = 0x104f,
    WPS_ATTR_REKEY_KEY = 0x1050,
    WPS_ATTR_KEY_LIFETIME = 0x1051,
    WPS_ATTR_PERMITTED_CFG_METHODS = 0x1052,
    WPS_ATTR_SELECTED_REGISTRAR_CONFIG_METHODS = 0x1053,
    WPS_ATTR_PRIMARY_DEV_TYPE = 0x1054,
    WPS_ATTR_SECONDARY_DEV_TYPE_LIST = 0x1055,
    WPS_ATTR_PORTABLE_DEV = 0x1056,
    WPS_ATTR_AP_SETUP_LOCKED = 0x1057,
    WPS_ATTR_APPLICATION_EXT = 0x1058,
    WPS_ATTR_EAP_TYPE = 0x1059,
    WPS_ATTR_IV = 0x1060,
    WPS_ATTR_KEY_PROVIDED_AUTO = 0x1061,
    WPS_ATTR_802_1X_ENABLED = 0x1062,
    WPS_ATTR_APPSESSIONKEY = 0x1063,
    WPS_ATTR_WEPTRANSMITKEY = 0x1064,
    WPS_ATTR_REQUESTED_DEV_TYPE = 0x106a,
} wps_wpasupplicant_attr_ne;

#define WPS_SUPPLICANT_AUTH_OPEN          0x01
#define WPS_SUPPLICANT_AUTH_WPAPSK        0x02
#define WPS_SUPPLICANT_AUTH_WPA2PSK       0x20

#define WPS_SUPPLICANT_ENCR_NONE          0x01
#define WPS_SUPPLICANT_ENCR_TKIP          0x04
#define WPS_SUPPLICANT_ENCR_AES           0x08

SWL_TABLE(sAuthMap,
          ARR(uint16_t auth; uint16_t secMode; ),
          ARR(swl_type_uint16, swl_type_uint16, ),
          ARR({SWL_SECURITY_APMODE_NONE, WPS_SUPPLICANT_AUTH_OPEN},
              {SWL_SECURITY_APMODE_WPA_P, WPS_SUPPLICANT_AUTH_WPAPSK},
              {SWL_SECURITY_APMODE_WPA2_P, WPS_SUPPLICANT_AUTH_WPA2PSK},
              ));

SWL_TABLE(sEncrMap,
          ARR(uint16_t encr; uint16_t secMode; ),
          ARR(swl_type_uint16, swl_type_uint16, ),
          ARR({SWL_SECURITY_APMODE_NONE, WPS_SUPPLICANT_ENCR_NONE},
              {SWL_SECURITY_APMODE_WPA_P, WPS_SUPPLICANT_ENCR_TKIP},
              {SWL_SECURITY_APMODE_WPA2_P, WPS_SUPPLICANT_ENCR_AES},
              ));

static inline uint16_t WPS_SUPPLICANT_GET_BE16(const uint8_t* a) {
    return (a[0] << 8) | a[1];
}

static inline void WPS_SUPPLICANT_SET_BE16(char* a, const uint16_t val) {
    a[0] = val >> 8;
    a[1] = val & 0xff;
}

swl_rc_ne wpaSupp_buildWpsCredentials(T_AccessPoint* pAP, char* buf, size_t* bufLen) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(buf, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(bufLen, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(*bufLen > 0, SWL_RC_INVALID_PARAM, ME, "Empty");

    swl_tlvList_t tlvList;
    swl_tlvList_init(&tlvList);

    uint8_t networkAttrVal = 0;
    swl_tlvList_appendType(&tlvList, WPS_ATTR_NETWORK_INDEX, 1, &networkAttrVal, &gtSwl_type_uint8);
    swl_tlvList_append(&tlvList, WPS_ATTR_SSID, strlen(pSSID->SSID), pSSID->SSID);
    swl_tlvList_appendType(&tlvList, WPS_ATTR_AUTH_TYPE, 2, swl_table_getMatchingValue(&sAuthMap, 1, 0, &pAP->secModeEnabled), &gtSwl_type_uint16);
    swl_tlvList_appendType(&tlvList, WPS_ATTR_ENCR_TYPE, 2, swl_table_getMatchingValue(&sEncrMap, 1, 0, &pAP->secModeEnabled), &gtSwl_type_uint16);
    swl_tlvList_append(&tlvList, WPS_ATTR_NETWORK_KEY, strlen(pAP->keyPassPhrase), pAP->keyPassPhrase);
    swl_tlvList_append(&tlvList, WPS_ATTR_MAC_ADDR, SWL_MAC_BIN_LEN, pSSID->BSSID);

    swl_tlvList_prepend(&tlvList, WPS_ATTR_CRED, swl_tlvList_getTotalSize(&tlvList), NULL);

    size_t remainingLen = *bufLen;
    swl_rc_ne ret = swl_tlvList_toBuffer(&tlvList, buf, *bufLen, &remainingLen);
    swl_tlvList_cleanup(&tlvList);

    *bufLen -= remainingLen;

    return ret;
}

static swl_rc_ne s_parseSsid(T_WPSCredentials* creds, uint16_t attr _UNUSED, uint16_t len, uint8_t* data) {
    ASSERT_FALSE(len >= sizeof(creds->ssid), SWL_RC_ERROR, ME, "Invalid ssid size %u", len);
    bool success = swl_str_ncopy(creds->ssid, sizeof(creds->ssid), (const char*) data, len);
    ASSERT_TRUE(success, SWL_RC_ERROR, ME, " FAIL");
    return SWL_RC_OK;
}

static swl_rc_ne s_parseKey(T_WPSCredentials* creds, uint16_t attr _UNUSED, uint16_t len, uint8_t* data) {
    ASSERT_FALSE(len >= sizeof(creds->key), SWL_RC_ERROR, ME, "Invalid key size %u", len);
    bool success = swl_str_ncopy(creds->key, sizeof(creds->key), (const char*) data, len);
    ASSERT_TRUE(success, SWL_RC_ERROR, ME, "FAIL");
    return SWL_RC_OK;
}

static swl_rc_ne s_parseAuthType(T_WPSCredentials* creds, uint16_t attr _UNUSED, uint16_t len, uint8_t* data) {
    ASSERT_FALSE(len != 2, SWL_RC_ERROR, ME, "Invalid auth type size %u", len);
    uint16_t auth = WPS_SUPPLICANT_GET_BE16(data);
    if(auth == WPS_SUPPLICANT_AUTH_WPAPSK) {
        creds->secMode = SWL_SECURITY_APMODE_WPA_P;
    } else if(auth == WPS_SUPPLICANT_AUTH_WPA2PSK) {
        creds->secMode = SWL_SECURITY_APMODE_WPA2_P;
    } else if(auth == (WPS_SUPPLICANT_AUTH_WPA2PSK | WPS_SUPPLICANT_AUTH_WPAPSK)) {
        creds->secMode = SWL_SECURITY_APMODE_WPA_WPA2_P;
    }
    return SWL_RC_OK;
}

SWL_TABLE(attrParseTable,
          ARR(uint16_t index; void* val; ),
          ARR(swl_type_int16, swl_type_voidPtr),
          ARR({WPS_ATTR_SSID, (void*) s_parseSsid},
              {WPS_ATTR_NETWORK_KEY, (void*) s_parseKey},
              {WPS_ATTR_AUTH_TYPE, (void*) s_parseAuthType}, )
          );

typedef swl_rc_ne (* elementParseFun_f) (T_WPSCredentials* creds, uint16_t attr _UNUSED, uint16_t len, uint8_t* data);

swl_rc_ne wpaSup_parseWpsReceiveCredentialsEvt(T_WPSCredentials* creds, char* data, size_t dataLen) {
    ASSERT_NOT_NULL(creds, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "Parse cred %zu -%s-", dataLen, data);
    size_t binLen = dataLen / 2;
    swl_bit8_t binData[binLen];
    bool success = swl_hex_toBytes(binData, binLen, data, dataLen);
    ASSERT_TRUE(success, SWL_RC_ERROR, ME, "HEX CONVERT FAIL");
    size_t index = 0;
    while(index < binLen) {
        size_t remainingSize = binLen - index;
        ASSERT_FALSE(remainingSize < 4, SWL_RC_ERROR, ME, "data buff too small for attr %zu", remainingSize);
        uint16_t attr = WPS_SUPPLICANT_GET_BE16(&binData[index]);
        index += 2;
        uint16_t len = WPS_SUPPLICANT_GET_BE16(&binData[index]);
        ASSERT_TRUE(len != 0, SWL_RC_ERROR, ME, "ZERO LEN");
        index += 2;
        remainingSize = binLen - index;
        ASSERT_FALSE(len > remainingSize, SWL_RC_ERROR, ME, "data buff too small for hex %zu %u", remainingSize, len);
        SAH_TRACEZ_INFO(ME, "parsing attr(%u), len(%u)", attr, len);

        void** func = (void**) swl_table_getMatchingValue(&attrParseTable, 1, 0, &attr);
        if(func != NULL) {
            elementParseFun_f myFun = (elementParseFun_f) (*func);
            swl_rc_ne ret = myFun(creds, attr, len, &binData[index]);
            ASSERT_FALSE(ret < SWL_RC_OK, SWL_RC_ERROR, ME, "Parsing Fail");
        } else {
            SAH_TRACEZ_INFO(ME, "Unsupported element id %d", attr);
        }
        // Skip empty data for WPS_ATTR_CRED attr
        if(attr != WPS_ATTR_CRED) {
            index += len;
        }
    }
    SAH_TRACEZ_INFO(ME, "Retrieved credentials: SSID: %s; security mode: %d; key: %s",
                    creds->ssid, creds->secMode, creds->key);

    return SWL_RC_OK;
}
