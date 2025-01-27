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

#include "swl/swl_common.h"
#include "swl/swl_common_conversion.h"
#include "swla/swla_dm.h"

#include "Utils/wld_config.h"

#define ME "wldCfg"

typedef enum {
    WLD_CONFIG_ENABLE_SYNC_MODE_OFF,
    WLD_CONFIG_ENABLE_SYNC_MODE_TO_INTF,
    WLD_CONFIG_ENABLE_SYNC_MODE_FROM_INTF,
    WLD_CONFIG_ENABLE_SYNC_MODE_MIRRORED,
    WLD_CONFIG_ENABLE_SYNC_MODE_MAX,
} wld_config_enableSyncMode_e;


const char* wld_config_enableSyncMode_str[] =
{"Off", "ToIntf", "FromIntf", "Mirrored", NULL};

static wld_config_enableSyncMode_e s_curSyncMode = WLD_CONFIG_ENABLE_SYNC_MODE_MIRRORED;

static uint32_t s_defaultFsmWaitTime = 10;

wld_config_enableSyncMode_e wld_config_getEnableSyncMode() {
    return s_curSyncMode;
}
const char* wld_config_getEnableSyncModeStr() {
    return wld_config_enableSyncMode_str[s_curSyncMode];
}

bool wld_config_isEnableSyncNeeded(bool toIntf) {
    if(s_curSyncMode == WLD_CONFIG_ENABLE_SYNC_MODE_MIRRORED) {
        return true;
    }
    if(toIntf) {
        return s_curSyncMode == WLD_CONFIG_ENABLE_SYNC_MODE_TO_INTF;
    } else {
        return s_curSyncMode == WLD_CONFIG_ENABLE_SYNC_MODE_FROM_INTF;
    }
}


uint32_t wld_config_getDefaultFsmStageTime() {
    return s_defaultFsmWaitTime;
}

static void s_setEnableSyncMode_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED,
                                    amxd_param_t* param _UNUSED,
                                    const amxc_var_t* const newValue) {
    char* newEnableSyncModeStr = amxc_var_get_cstring_t(newValue);
    ASSERTI_NOT_NULL(newEnableSyncModeStr, , ME, "NULL");
    wld_config_enableSyncMode_e newMode = swl_conv_charToEnum(newEnableSyncModeStr, wld_config_enableSyncMode_str,
                                                              WLD_CONFIG_ENABLE_SYNC_MODE_MAX, WLD_CONFIG_ENABLE_SYNC_MODE_MIRRORED);
    free(newEnableSyncModeStr);

    ASSERTI_NOT_EQUALS(newMode, s_curSyncMode, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "update Enable %u to %u", s_curSyncMode, newMode);
    s_curSyncMode = newMode;
}

static void s_setDefaultFSMWaitTime_pwf(void* priv _UNUSED, amxd_object_t* object _UNUSED,
                                        amxd_param_t* param _UNUSED,
                                        const amxc_var_t* const newValue) {
    uint32_t newDefaultFsmWaitTime = amxc_var_get_uint32_t(newValue);
    ASSERTI_NOT_EQUALS(newDefaultFsmWaitTime, 0, , ME, "Zero");


    ASSERTI_NOT_EQUALS(newDefaultFsmWaitTime, s_defaultFsmWaitTime, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "update defaultFsmSyncType %u to %u", s_defaultFsmWaitTime, newDefaultFsmWaitTime);
    s_defaultFsmWaitTime = newDefaultFsmWaitTime;
}

SWLA_DM_HDLRS(sWldConfigParamChangeHandlers,
              ARR(SWLA_DM_PARAM_HDLR("EnableSyncMode", s_setEnableSyncMode_pwf),
                  SWLA_DM_PARAM_HDLR("DefaultFSMWaitTime", s_setDefaultFSMWaitTime_pwf)));

void _wld_config_setConf_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sWldConfigParamChangeHandlers, sig_name, data, priv);
}

