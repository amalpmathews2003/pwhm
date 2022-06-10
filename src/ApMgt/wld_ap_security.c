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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_wps.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "wld_ap_rssiMonitor.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "apSec"

const char* cstr_AP_ModesSupported[APMSI_MAX + 1] = {"None", "OWE",
    "WEP-64", "WEP-128", "WEP-128iv",
    "WPA-Personal", "WPA2-Personal", "WPA-WPA2-Personal",
    "WPA3-Personal", "WPA2-WPA3-Personal",
    "E-None",
    "WPA-Enterprise", "WPA2-Enterprise", "WPA-WPA2-Enterprise",
    "WPA3-Enterprise", "WPA2-WPA3-Enterprise",
    "Auto", "Unsupported", "Unknown",
    0};

const char* cstr_AP_EncryptionMode[] = {"Default", "AES", "TKIP", "TKIP-AES", 0};

const char* wld_mfpConfig_str[WLD_MFP_MAX] = {"Disabled", "Optional", "Required"};

const char* g_str_wld_ap_td[AP_TD_MAX] = {"WPA3-Personal", "SAE-PK", "WPA3-Enterprise", "EnhancedOpen"};


void wld_ap_sec_doSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_sec_sync(pAP, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

/**
 * Check that the security settings are valid.
 **/
amxd_status_t _validateSecurity(amxd_object_t* obj, void* validationData) {
    _UNUSED_(validationData);
    amxd_param_t* pModeParam = NULL;
    amxd_param_t* pParam = NULL;
    const char* preSharedKey = NULL;
    const char* keyPassPhrase = NULL;
    const char* saePassphrase = NULL;
    const char* wepKey = NULL;
    const char* radiusSecret = NULL;
    const char* modeEnabled = NULL;
    const char* s_modesAvailable = NULL;
    const char* s_modesSupported = NULL;
    const char* encryptionMode = NULL;
    int idx;
    int ret;
    wld_securityMode_e iModeEnabled;
    uint32_t m_modesAvailable = 0;
    uint32_t m_modesSupported = 0;

    amxc_var_t pVar;
    amxc_var_init(&pVar);

    pModeParam = amxd_object_get_param_def(obj, "ModesAvailable");
    amxd_param_get_value(pModeParam, &pVar);
    s_modesAvailable = amxc_var_get_cstring_t(&pVar);
    assert(s_modesAvailable);

    pModeParam = amxd_object_get_param_def(obj, "ModeEnabled");
    amxd_param_get_value(pModeParam, &pVar);
    modeEnabled = amxc_var_get_cstring_t(&pVar);
    assert(modeEnabled);

    pModeParam = amxd_object_get_param_def(obj, "ModesSupported");
    amxd_param_get_value(pModeParam, &pVar);
    s_modesSupported = amxc_var_get_cstring_t(&pVar);
    assert(s_modesSupported);

    pModeParam = amxd_object_get_param_def(obj, "EncryptionMode");
    amxd_param_get_value(pModeParam, &pVar);
    encryptionMode = amxc_var_get_cstring_t(&pVar);
    assert(encryptionMode);

    pParam = amxd_object_get_param_def(obj, "WEPKey");
    amxd_param_get_value(pParam, &pVar);
    wepKey = amxc_var_get_cstring_t(&pVar);
    assert(wepKey);
    if(wepKey && wepKey[0] && !isValidWEPKey(wepKey)) {
        SAH_TRACEZ_ERROR(ME, "isValidWEPKey failed! %s", wepKey);
        return amxd_status_unknown_error;
    }

    pParam = amxd_object_get_param_def(obj, "PreSharedKey");
    amxd_param_get_value(pParam, &pVar);
    preSharedKey = amxc_var_get_cstring_t(&pVar);
    assert(preSharedKey);

    pParam = amxd_object_get_param_def(obj, "KeyPassPhrase");
    amxd_param_get_value(pParam, &pVar);
    keyPassPhrase = amxc_var_get_cstring_t(&pVar);
    assert(keyPassPhrase);

    pParam = amxd_object_get_param_def(obj, "SAEPassphrase");
    amxd_param_get_value(pParam, &pVar);
    saePassphrase = amxc_var_get_cstring_t(&pVar);
    assert(saePassphrase);

    iModeEnabled = swl_conv_charToEnum(modeEnabled, cstr_AP_ModesSupported, APMSI_MAX, APMSI_UNKNOWN);

    // Double check when in WPAx security mode... one of the 2 must be OK!
    if(isModeWPAPersonal(iModeEnabled)) {
        ret = ((preSharedKey && preSharedKey[0] &&
                isValidPSKKey(preSharedKey)) ||
               (keyPassPhrase && keyPassPhrase[0] &&
                isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)));
        if(!ret) {
            SAH_TRACEZ_ERROR(ME, "isValidPSKKey AND isValidAESKey failed for!");
            if(preSharedKey) {
                SAH_TRACEZ_ERROR(ME, "isValidPSKKey = %s!", preSharedKey);
            }
            if(keyPassPhrase) {
                SAH_TRACEZ_ERROR(ME, "isValidAESKey = %s!", keyPassPhrase);
            }
            return amxd_status_unknown_error;
        }
    }

    pParam = amxd_object_get_param_def(obj, "RadiusSecret");
    amxd_param_get_value(pParam, &pVar);
    radiusSecret = amxc_var_get_cstring_t(&pVar);
    assert(radiusSecret);

    findStrInArray(modeEnabled, cstr_AP_ModesSupported, &idx);

    m_modesSupported = swl_conv_charToMask(s_modesSupported, cstr_AP_ModesSupported, APMSI_MAX);
    m_modesAvailable = swl_conv_charToMask(s_modesAvailable, cstr_AP_ModesSupported, APMSI_MAX);

    T_AccessPoint* pAP = amxd_object_get_parent(obj)->priv;
    if((m_modesSupported != 0) && (m_modesAvailable == 0)) {
        m_modesAvailable = m_modesSupported;
        amxc_string_t TBufStr;
        amxc_string_init(&TBufStr, 0);
        wld_ap_ModesSupported_mask_to_string(&TBufStr, m_modesAvailable);
        if(pAP && debugIsVapPointer(pAP)) {
            amxc_var_t value;
            amxc_var_init(&value);
            amxc_var_set(cstring_t, &value, amxc_string_get(&TBufStr, 0));
            amxd_param_t* parameter = amxd_object_get_param_def(obj, "ModesAvailable");
            amxd_param_set_value(parameter, &value);
            amxc_var_clean(&value);
        }
        amxc_string_clean(&TBufStr);
    }
    if((m_modesSupported != 0) && !(m_modesSupported & m_modesAvailable)) {
        SAH_TRACEZ_ERROR(ME, "ModesAvailable not supported");
        return amxd_status_unknown_error;
    }


    if((m_modesAvailable != 0) && !(m_modesAvailable & (1 << idx))) {
        if(SWL_BIT_IS_SET(m_modesAvailable, APMSI_WPA3_P) && (iModeEnabled == APMSI_WPA2_P)) {
            amxc_var_t value;
            amxc_var_init(&value);
            amxc_var_set(cstring_t, &value, cstr_AP_ModesSupported[APMSI_WPA3_P]);
            amxd_param_t* parameter = amxd_object_get_param_def(obj, "ModeEnabled");
            amxd_param_set_value(parameter, &value);
            SAH_TRACEZ_WARNING(ME, "%s: Default WPA2 set but not available, setting to available WPA3", pAP->alias);
            amxc_var_clean(&value);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: ModeEnabled %s not available 0x%02x", pAP->alias, cstr_AP_ModesSupported[idx], m_modesAvailable);
            if(pAP && debugIsVapPointer(pAP)) {
                if(SWL_BIT_IS_SET(m_modesAvailable, APMSI_WPA2_P)) {
                    SAH_TRACEZ_ERROR(ME, "%s: reverting to WPA2 from ", pAP->alias);
                    amxc_var_t value;
                    amxc_var_init(&value);
                    amxc_var_set(cstring_t, &value, cstr_AP_ModesSupported[APMSI_WPA2_P]);
                    amxd_param_t* parameter = amxd_object_get_param_def(obj, "ModeEnabled");
                    amxd_param_set_value(parameter, &value);
                }
                if(SWL_BIT_IS_SET(m_modesAvailable, APMSI_WPA3_P)) {
                    amxc_var_t value;
                    amxc_var_init(&value);
                    amxc_var_set(cstring_t, &value, cstr_AP_ModesSupported[APMSI_WPA3_P]);
                    amxd_param_t* parameter = amxd_object_get_param_def(obj, "ModeEnabled");
                    amxd_param_set_value(parameter, &value);
                } else {
                    SAH_TRACEZ_ERROR(ME, "%s: unable to revert to WPA2, not available", pAP->alias);
                }
            }
            return amxd_status_unknown_error;
        }
    }

    switch(idx) {
    case APMSI_NONE:
    case APMSI_OWE:
        return amxd_status_ok;
    case APMSI_WEP64:        /* 5/10 */
    case APMSI_WEP128:       /* 13/26 */
    case APMSI_WEP128IV:     /* 16/32 */
        /* BUG 49483 - SOP screwed WEP, result many targets fail
           Our WebUI can't change the WEP key if there's no Key filled in.
           See this as a HACK to get around most of the trouble!
         */
        if(wepKey && !wepKey[0]) {
            /* No WEP key filled in... Generate a valid WEP-128 key! */
            T_AccessPoint* pAP = NULL;

            pAP = amxd_object_get_parent(obj)->priv;
            if(pAP && debugIsVapPointer(pAP)) {
                get_randomstr((unsigned char*) pAP->WEPKey, 13);

                /* push the WEP key directly to the datamodel! */
                amxc_var_t value;
                amxc_var_init(&value);
                amxc_var_set(cstring_t, &value, (const char*) pAP->WEPKey);
                amxd_param_t* parameter = amxd_object_get_param_def(obj, "WEPKey");
                amxd_param_set_value(parameter, &value);

                return amxd_status_ok;     /* It's OK, continue! */
            }
        }
        return (isValidWEPKey(wepKey)) ? true : false;

    case APMSI_WPA_P:
        if(isValidPSKKey(preSharedKey)) {
            return amxd_status_ok;
        }
        return isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
    case APMSI_WPA2_P:
    case APMSI_WPA_WPA2_P:
    case APMSI_WPA2_WPA3_P:
        return isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
    case APMSI_WPA3_P:
        if(swl_str_matches(saePassphrase, "")) {
            return isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
        } else {
            return isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN);
        }
    case APMSI_WPA_E:
        if(isValidPSKKey(radiusSecret)) {
            return amxd_status_ok;
        }
        return isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
    case APMSI_NONE_E:
    case APMSI_WPA2_E:
    case APMSI_WPA_WPA2_E:
    case APMSI_WPA2_WPA3_E:
    case APMSI_WPA3_E:
        // The only technical limitation is that shared secrets must be greater than 0 in length!
        ret = strlen(radiusSecret);
        return (ret >= 1 && ret < 64) ? true : false;

    default:
        SAH_TRACEZ_ERROR(ME, "Unknown mode %s", modeEnabled);
        return amxd_status_unknown_error;
    }

    /* Unknown mode? */
    return amxd_status_unknown_error;
}

amxd_status_t _wld_ap_setWEPKey_pwf(amxd_object_t* object _UNUSED,
                                    amxd_param_t* parameter,
                                    amxd_action_t reason _UNUSED,
                                    const amxc_var_t* const args _UNUSED,
                                    amxc_var_t* const retval _UNUSED,
                                    void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);

    const char* pname = amxc_var_constcast(cstring_t, args); // Get the Key
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    int ret = isValidWEPKey(pname);                          // Check if valid
    SAH_TRACEZ_INFO(ME, "WEPKey %s -  %d", pname, ret);
    ASSERTI_TRUE(ret != 0, false, ME, "%s : invalid WEP key %s", pAP->alias, pname);

    /* We got as return the correct WEP key format used... */
    wldu_copyStr(pAP->WEPKey, pname, sizeof(pAP->WEPKey));

    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setMFPConfig_pwf(amxd_object_t* object _UNUSED,
                                       amxd_param_t* parameter,
                                       amxd_action_t reason _UNUSED,
                                       const amxc_var_t* const args _UNUSED,
                                       amxc_var_t* const retval _UNUSED,
                                       void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    const char* mfpStr = amxc_var_constcast(cstring_t, args);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    pAP->mfpConfig = conv_strToEnum(wld_mfpConfig_str, mfpStr, WLD_MFP_MAX, WLD_MFP_DISABLED);
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setModesAvailable_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    const char* modesAvailable = amxc_var_constcast(cstring_t, args);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    if(modesAvailable && modesAvailable[0]) {
        pAP->secModesAvailable = swl_conv_charToMask(modesAvailable, cstr_AP_ModesSupported, APMSI_MAX);
    } else {
        pAP->secModesAvailable = pAP->secModesSupported;
    }
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setModeEnabled_pwf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* parameter,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args _UNUSED,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    int ret = 0;
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }
    SAH_TRACEZ_IN(ME);
    const char* pname = amxc_var_constcast(cstring_t, args);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");

    wld_securityMode_e idx = conv_strToEnum(cstr_AP_ModesSupported, pname, APMSI_MAX, APMSI_UNKNOWN);

    switch(idx) {
    case APMSI_NONE:
    case APMSI_OWE:
        pAP->secModeEnabled = idx;  // If this is what the user wants?
        ret = true;
        SAH_TRACEZ_INFO(ME, "%s %d", idx == APMSI_OWE ? "OWE" : "None", ret);
        /* If we must enable MAC filter... it must be here */
        break;
    case APMSI_WEP64:    /* 5/10 */
    case APMSI_WEP128:   /* 13/26 */
    case APMSI_WEP128IV: /* 16/32 */
        /* Before changing... check if we've a valid WEP key */
        ret = isValidWEPKey(pAP->WEPKey);
        if(ret) {
            pAP->secModeEnabled = ret;
        }
        SAH_TRACEZ_INFO(ME, "WEP %d", ret);
        break;
    case APMSI_WPA_P:
        ret = isValidPSKKey(pAP->preSharedKey);
        if(!ret) {
            ret = (isValidAESKey(pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) ? idx : 0;
        }
        if(ret) {
            pAP->secModeEnabled = ret;
        }
        SAH_TRACEZ_INFO(ME, "WPA-TKIP %d", ret);
        break;
    case APMSI_WPA2_P:
    case APMSI_WPA_WPA2_P:
    case APMSI_WPA2_WPA3_P:
        ret = isValidAESKey((char*) pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
        if(ret) {
            pAP->secModeEnabled = idx;
        }
        SAH_TRACEZ_INFO(ME, "WPA(x)-AES %d", ret);
        break;
    case APMSI_WPA3_P:
        if(swl_str_matches(pAP->saePassphrase, "")) {
            ret = isValidAESKey(pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
        } else {
            ret = isValidAESKey(pAP->saePassphrase, SAE_KEY_SIZE_LEN);
        }
        SAH_TRACEZ_INFO(ME, "WPA3 %d", ret);
        break;
    case APMSI_WPA_E:
        ret = isValidPSKKey(pAP->radiusSecret);
        if(!ret) {
            ret = isValidAESKey(pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1);
        }
        if(ret) {
            pAP->secModeEnabled = idx;
        }
        SAH_TRACEZ_INFO(ME, "WPA-TKIP  %d", ret);
        break;
    case APMSI_NONE_E:
    case APMSI_WPA2_E:
    case APMSI_WPA_WPA2_E:
    case APMSI_WPA3_E:
    case APMSI_WPA2_WPA3_E:
        ret = strlen(pAP->radiusSecret);
        if((ret >= 8) && (ret < 64)) {
            pAP->secModeEnabled = idx;
        }
        break;
    case APMSI_UNKNOWN:
        SAH_TRACEZ_ERROR(ME, "%s : unknown security mode %s", pAP->alias, pname);
        break;
    default:
        break;
    }

    if(ret) {
        wld_ap_sec_doSync(pAP);
        return amxd_status_ok;
    } else {
        return amxd_status_unknown_error;
    }
}

amxd_status_t _wld_ap_setPreSharedKey_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    const char* pname = amxc_var_constcast(cstring_t, args);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    int ret = isValidPSKKey(pname);                // Check if valid
    ASSERTI_TRUE(ret != 0, false, ME, "PreSharedKey %s Not VALID", (char*) pname);

    SAH_TRACEZ_INFO(ME, "WPA-TKIP %s -  %d", pname, ret);
    /* We got as return the correct WPA key format used... */
    wldu_copyStr(pAP->preSharedKey, pname, sizeof(pAP->preSharedKey));
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

static amxd_status_t _setApPassphrase(amxd_param_t* parameter,
                                      amxd_action_t reason _UNUSED,
                                      const amxc_var_t* const args _UNUSED,
                                      amxc_var_t* const retval _UNUSED,
                                      void* priv, bool isSae) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    const char* newPassphrase = amxc_var_constcast(cstring_t, args);

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    ASSERTS_NOT_NULL(newPassphrase, amxd_status_unknown_error, ME, "NULL");
    bool sanitized = false;
    if(isSae) {
        sanitized = isValidAESKey(newPassphrase, SAE_KEY_SIZE_LEN);
    } else {
        sanitized = isValidAESKey(newPassphrase, PSK_KEY_SIZE_LEN - 1);
    }

    SAH_TRACEZ_INFO(ME, "WPA-AES %s -  %u", newPassphrase, sanitized);

    if(!sanitized) {
        if((pAP->secModeEnabled != APMSI_NONE) && !isSae) {
            SAH_TRACEZ_ERROR(ME, "%s : set unsanitized PSK %u %u", pAP->alias, sanitized, pAP->secModeEnabled);
        } else {
            SAH_TRACEZ_INFO(ME, "%s : set unsanitized PSK %u %u %u", pAP->alias, sanitized, pAP->secModeEnabled, isSae);
        }
        return amxd_status_unknown_error;
    }

    T_SSID* pSSID = pAP->pSSID;
    bool needSync = true;
    bool passUpdated = false;
    if(isSae) {
        passUpdated = pSSID && !wldu_key_matches(pSSID->SSID, newPassphrase, pAP->saePassphrase);
    } else {
        passUpdated = pSSID && !wldu_key_matches(pSSID->SSID, newPassphrase, pAP->keyPassPhrase);
    }
    if(!passUpdated) {
        needSync = false;
        SAH_TRACEZ_INFO(ME, "Same key is used, no need to sync %s", newPassphrase);
    }
    if(isSae) {
        wldu_copyStr(pAP->saePassphrase, newPassphrase, sizeof(pAP->saePassphrase));
    } else {
        wldu_copyStr(pAP->keyPassPhrase, newPassphrase, sizeof(pAP->keyPassPhrase));
    }

    if(needSync) {
        wld_ap_sec_doSync(pAP);
    }
    return amxd_status_ok;

}

amxd_status_t _wld_ap_setKeyPassPhrase_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    return _setApPassphrase(parameter, reason, args, retval, priv, false);
}

amxd_status_t _wld_ap_setSaePassphrase_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    return _setApPassphrase(parameter, reason, args, retval, priv, true);
}

amxd_status_t _wld_ap_setEncryptionMode_pwf(amxd_object_t* object _UNUSED,
                                            amxd_param_t* parameter,
                                            amxd_action_t reason _UNUSED,
                                            const amxc_var_t* const args _UNUSED,
                                            amxc_var_t* const retval _UNUSED,
                                            void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");

    const char* newMode = amxc_var_constcast(cstring_t, args);

    pAP->encryptionModeEnabled = conv_strToEnum(cstr_AP_EncryptionMode, newMode, APEMI_MAX, APEMI_DEFAULT);

    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setRekeyInterval_pwf(amxd_object_t* object _UNUSED,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    // They are handled in SyncData_AP2OBJ()
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setRadiusInfo_pwf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* parameter,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args _UNUSED,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    // They are handled in SyncData_AP2OBJ()
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setSHA256Enable_pwf(amxd_object_t* object _UNUSED,
                                          amxd_param_t* parameter,
                                          amxd_action_t reason _UNUSED,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval _UNUSED,
                                          void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");
    pAP->SHA256Enable = amxc_var_dyncast(bool, args);

    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setOweTransitionIntf_pwf(amxd_object_t* object _UNUSED,
                                               amxd_param_t* parameter,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args _UNUSED,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {
    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");

    const char* oweTransIntf = amxc_var_constcast(cstring_t, args);

    swl_str_copy(pAP->oweTransModeIntf, sizeof(pAP->oweTransModeIntf), oweTransIntf);
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}

amxd_status_t _wld_ap_setTransitionDisable_pwf(amxd_object_t* object _UNUSED,
                                               amxd_param_t* parameter,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args _UNUSED,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {

    amxd_status_t rv = amxd_status_ok;
    amxd_object_t* wifiVap = amxd_param_get_owner(parameter);
    if(amxd_object_get_type(wifiVap) != amxd_object_instance) {
        return rv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) wifiVap->priv;
    rv = amxd_action_param_write(wifiVap, parameter, reason, args, retval, priv);
    if(rv != amxd_status_ok) {
        return rv;
    }

    SAH_TRACEZ_IN(ME);
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "INVALID");

    const char* tdStr = amxc_var_constcast(cstring_t, args);

    wld_ap_td_m transitionDisable = swl_conv_charToMask(tdStr, g_str_wld_ap_td, AP_TD_MAX);
    ASSERTI_NOT_EQUALS(transitionDisable, pAP->transitionDisable, amxd_status_unknown_error, ME, "%s: no change", pAP->alias);
    pAP->transitionDisable = transitionDisable;
    wld_ap_sec_doSync(pAP);
    return amxd_status_ok;
}
