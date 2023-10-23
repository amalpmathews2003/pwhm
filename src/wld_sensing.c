/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include "swl/swl_assert.h"
#include "wld.h"
#include "wld_radio.h"

#define ME "csi"

static wld_csiClient_t* s_findClient(amxd_object_t* templateObj, const char* macAddr) {
    amxd_object_for_each(instance, it, templateObj) {
        amxd_object_t* clientObj = amxc_container_of(it, amxd_object_t, it);
        ASSERT_NOT_NULL(clientObj, NULL, ME, "NULL");
        char* entryMacAddr = amxd_object_get_cstring_t(clientObj, "MACAddress", NULL);
        bool match = (!swl_str_isEmpty(entryMacAddr) && SWL_MAC_CHAR_MATCHES(entryMacAddr, macAddr));
        free(entryMacAddr);
        if(match) {
            return clientObj->priv;
        }
    }
    return NULL;
}

static swl_rc_ne s_updateClient(T_Radio* pRad, wld_csiClient_t* client) {
    swl_rc_ne rc = pRad->pFA->mfn_wrad_sensing_addClient(pRad, client);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: Driver failed to add client %s",
                         pRad->Name, client->macAddr.cMac);
    }
    return rc;
}

static swl_rc_ne s_removeClient(T_Radio* pRad, wld_csiClient_t* client) {
    swl_rc_ne rc = pRad->pFA->mfn_wrad_sensing_delClient(pRad, client->macAddr);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: Driver failed to delete client %s",
                         pRad->Name, client->macAddr.cMac);
    }
    return rc;
}

static bool s_clientMACAddressIsValid(const char* macAddr, swl_macChar_t* macChar) {
    return swl_mac_charToStandard(macChar, macAddr) && swl_mac_charIsValidStaMac(macChar);
}

/**
 * Add one client with specified MAC address
 * @return
 *  - Success: amxd_status_ok
 *  - Failure: amxd_status_unknown_error
 */
amxd_status_t _Sensing_addClient(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    // Get MAC address and monitoring interval input data
    const char* macAddr = GET_CHAR(args, "MACAddress");
    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    ASSERT_TRUE(s_clientMACAddressIsValid(macAddr, &macChar),
                amxd_status_invalid_value, ME, "%s: Invalid client MACAddress [%s]", pRad->Name, macAddr);

    uint32_t interval = GET_UINT32(args, "MonitorInterval");

    amxd_trans_t trans;
    amxd_object_t* clientTemplate = amxd_object_findf(object, "CSIClient");
    ASSERT_NOT_NULL(clientTemplate, amxd_status_unknown_error, ME, "%s: CSIClient object not found", pRad->Name);
    wld_csiClient_t* client = s_findClient(clientTemplate, macAddr);
    if(client != NULL) {
        SAH_TRACEZ_INFO(ME, "%s: Client with MACAddress [%s] found in client list", pRad->Name, macAddr);
        ASSERT_TRANSACTION_INIT(client->obj, &trans, amxd_status_unknown_error, ME,
                                "%s: trans init failure", pRad->Name);
    } else {
        SAH_TRACEZ_INFO(ME, "%s: Create new client with MACAddress [%s] MonitorInterval %u",
                        pRad->Name, macAddr, interval);
        ASSERT_TRANSACTION_INIT(clientTemplate, &trans, amxd_status_unknown_error, ME,
                                "%s: trans init failure", pRad->Name);
        uint32_t index = swla_object_getFirstAvailableIndex(clientTemplate);
        char alias[SWL_MAC_CHAR_LEN + 1];
        snprintf(alias, sizeof(alias), "_%s", macChar.cMac);
        amxd_trans_add_inst(&trans, index, alias);
    }

    amxd_trans_set_cstring_t(&trans, "MACAddress", macChar.cMac);
    amxd_trans_set_uint32_t(&trans, "MonitorInterval", interval);

    SAH_TRACEZ_OUT(ME);
    return swl_object_finalizeTransactionOnLocalDm(&trans);
}

/**
 *  Called to add CSI client datamodel entry
 */
amxd_status_t _wld_wifiSensing_addClientInst_oaf(amxd_object_t* object,
                                                 amxd_param_t* param,
                                                 amxd_action_t reason,
                                                 const amxc_var_t* const args,
                                                 amxc_var_t* const retval,
                                                 void* priv) {
    amxd_status_t status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "CSIClient instance creation failed with error: %d", status);

    uint32_t instIndex = GET_UINT32(retval, "index");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, instIndex);
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "No instance found at index %u", instIndex);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    amxc_var_t* params = amxc_var_get_key(args, "parameters", AMXC_VAR_FLAG_DEFAULT);
    const char* macAddr = GET_CHAR(params, "MACAddress");
    uint32_t interval = GET_UINT32(params, "MonitorInterval");

    SAH_TRACEZ_INFO(ME, "%s: Add new instance, MACAddress [%s] MonitorInterval %u", pRad->Name, macAddr, interval);

    wld_csiClient_t* client = calloc(1, sizeof(wld_csiClient_t));
    ASSERT_NOT_NULL(client, amxd_status_unknown_error, ME, "NULL");
    memset(client, 0, sizeof(wld_csiClient_t));
    swl_mac_charToStandard(&client->macAddr, macAddr);
    client->monitorInterval = interval;

    if(s_updateClient(pRad, client) != SWL_RC_OK) {
        free(client);
        return amxd_status_unknown_error;
    }

    amxc_llist_it_init(&client->it);
    amxc_llist_append(&pRad->csiClientList, &client->it);
    instance->priv = client;
    client->obj = instance;

    return status;
}

/**
 * Update one client monitoring interval
 * @return amxd_status_ok if success, error code otherwise
 */
amxd_status_t _wld_wifiSensing_setMonitorInterval_pwf(amxd_object_t* object,
                                                      amxd_param_t* parameter,
                                                      amxd_action_t reason,
                                                      const amxc_var_t* const args,
                                                      amxc_var_t* const retval,
                                                      void* priv) {
    uint32_t oldInterval = amxc_var_get_uint32_t(&parameter->value);
    uint32_t newInterval = amxc_var_get_uint32_t(args);

    amxd_status_t status = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Set MonitorInterval failed with error: %d", status);

    wld_csiClient_t* client = (wld_csiClient_t*) object->priv;
    ASSERTS_NOT_NULL(client, status, ME, "NULL");

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    s_removeClient(pRad, client);

    SAH_TRACEZ_INFO(ME, "%s: Update MonitorInterval %u to %u for client [%s]",
                    pRad->Name, oldInterval, newInterval, client->macAddr.cMac);

    client->monitorInterval = newInterval;
    if(s_updateClient(pRad, client) != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: Update MonitorInterval failed for client [%s] -> Back to old interval %u",
                         pRad->Name, client->macAddr.cMac, oldInterval);
        client->monitorInterval = oldInterval;
        s_updateClient(pRad, client);
        return amxd_status_unknown_error;
    }

    return status;
}

/**
 * Delete one client with specified MAC address
 * @return
 *  - Success: amxd_status_ok
 *  - Failure: amxd_status_unknown_error
 */
amxd_status_t _Sensing_delClient(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_unknown_error;
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, status, ME, "No radio ctx");

    // Get MAC address input data
    const char* macAddr = GET_CHAR(args, "MACAddress");

    SAH_TRACEZ_INFO(ME, "%s: Delete client with MACAddress [%s]", pRad->Name, macAddr);

    swl_macBin_t macBin = SWL_MAC_BIN_NEW();
    ASSERT_TRUE(swl_typeMacBin_fromChar(&macBin, macAddr),
                status, ME, "%s: Invalid client MACAddress [%s]", pRad->Name, macAddr);

    if(wld_sensing_delCsiClientEntry(pRad, &macBin) != SWL_RC_OK) {
        return status;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Called to delete CSI client datamodel entry
 */
swl_rc_ne wld_sensing_delCsiClientEntry(T_Radio* pRad, swl_macBin_t* macAddr) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "No radio ctx");
    ASSERT_NOT_NULL(pRad->pBus, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    SWL_MAC_BIN_TO_CHAR(&macChar, macAddr);

    amxd_object_t* sensingTemplate = amxd_object_findf(pRad->pBus, "Sensing");
    ASSERT_NOT_NULL(sensingTemplate, SWL_RC_ERROR, ME, "%s: Sensing object not found", pRad->Name);
    amxd_object_t* clientTemplate = amxd_object_findf(sensingTemplate, "CSIClient");
    ASSERT_NOT_NULL(clientTemplate, SWL_RC_ERROR, ME, "%s: CSIClient object not found", pRad->Name);
    wld_csiClient_t* client = s_findClient(clientTemplate, macChar.cMac);
    ASSERTI_NOT_NULL(client, SWL_RC_OK, ME, "%s: Client with MACAddress [%s] not found",
                     pRad->Name, macChar.cMac);

    SAH_TRACEZ_INFO(ME, "%s: Delete CSI client instance %d", pRad->Name, client->obj->index);

    amxd_status_t status = swl_object_delInstWithTransOnLocalDm(client->obj);
    return ((status == amxd_status_ok) ? SWL_RC_OK : SWL_RC_ERROR);
}

/**
 * Called when a client instance is destroyed
 */
amxd_status_t _wld_wifiSensing_delClient_odf(amxd_object_t* object,
                                             amxd_param_t* param,
                                             amxd_action_t reason,
                                             const amxc_var_t* const args,
                                             amxc_var_t* const retval,
                                             void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy client entry st:%d", status);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    ASSERT_NOT_NULL(pRad, amxd_status_ok, ME, "No radio ctx");
    wld_csiClient_t* client = (wld_csiClient_t*) object->priv;
    object->priv = NULL;
    ASSERT_NOT_NULL(client, amxd_status_ok, ME, "NULL");

    s_removeClient(pRad, client);

    amxc_llist_it_take(&(client->it));
    free(client);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/**
 * State of the overal CSIMON module
 * @return
 *  - Success: amxd_status_ok
 *  - Failure: amxd_status_unknown_error
 */
amxd_status_t _Sensing_csiStats(amxd_object_t* object,
                                amxd_function_t* func _UNUSED,
                                amxc_var_t* args _UNUSED,
                                amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    wld_csiState_t csimonState;
    memset(&csimonState, 0, sizeof(wld_csiState_t));

    amxc_var_t vendorCounters;
    amxc_var_init(&vendorCounters);
    csimonState.vendorCounters = &vendorCounters;

    if(pRad->pFA->mfn_wrad_sensing_csiStats(pRad, &csimonState) != SWL_RC_OK) {
        amxc_var_clean(&vendorCounters);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    /* Update csimonState values */
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, retval, "NullFrameCounter", csimonState.nullFrameCounter);
    amxc_var_add_key(uint32_t, retval, "M2MTransmitCounter", csimonState.m2mTransmitCounter);
    amxc_var_add_key(uint32_t, retval, "UserTransmitCounter", csimonState.userTransmitCounter);
    amxc_var_add_key(uint32_t, retval, "NullFrameAckFailCounter", csimonState.nullFrameAckFailCounter);
    amxc_var_add_key(uint32_t, retval, "ReceivedOverflowCounter", csimonState.receivedOverflowCounter);
    amxc_var_add_key(uint32_t, retval, "TransmitFailCounter", csimonState.transmitFailCounter);
    amxc_var_add_key(uint32_t, retval, "UserTransmitFailCounter", csimonState.userTransmitFailCounter);
    /* Update vendor specific counters */
    amxc_var_add_key(amxc_htable_t, retval, "VendorCounters", amxc_var_get_const_amxc_htable_t(csimonState.vendorCounters));
    amxc_var_clean(&vendorCounters);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Reset CSIMON counters
 * @return
 *  - Success: amxd_status_ok
 *  - Failure: amxd_status_unknown_error
 */
amxd_status_t _Sensing_resetStats(amxd_object_t* object,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    pRad->pFA->mfn_wrad_sensing_resetStats(pRad);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * For Debug : show every sensing clients
 */
amxd_status_t _Sensing_debug(amxd_object_t* object,
                             amxd_function_t* func _UNUSED,
                             amxc_var_t* args _UNUSED,
                             amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "No radio ctx");

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);
    amxc_llist_for_each(it, &pRad->csiClientList) {
        wld_csiClient_t* client = amxc_llist_it_get_data(it, wld_csiClient_t, it);
        SAH_TRACEZ_INFO(ME, "%s: Client MACAddress [%s], MonitorInterval %u",
                        pRad->Name, client->macAddr.cMac, client->monitorInterval);

        amxc_var_t* clientEntry = amxc_var_add(amxc_htable_t, retval, NULL);
        if(clientEntry == NULL) {
            break;
        }
        amxc_var_add_key(cstring_t, clientEntry, "MACAddress", client->macAddr.cMac);
        amxc_var_add_key(uint32_t, clientEntry, "MonitorInterval", client->monitorInterval);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Enable CSI monitoring
 */
static void s_setSensingEnable_pwf(void* priv _UNUSED,
                                   amxd_object_t* object,
                                   amxd_param_t* param _UNUSED,
                                   const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, , ME, "No radio ctx");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    ASSERTI_NOT_EQUALS(newEnable, pRad->csiEnable, , ME, "EQUAL");
    SAH_TRACEZ_INFO(ME, "Update Sensing Enable %d to %d", pRad->csiEnable, newEnable);

    pRad->csiEnable = newEnable;
    pRad->pFA->mfn_wrad_sensing_cmd(pRad);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sSensingDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setSensingEnable_pwf)));

void _wld_wifiSensing_setConf_ocf(const char* const sig_name,
                                  const amxc_var_t* const data,
                                  void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sSensingDmHdlrs, sig_name, data, priv);
}

void wld_sensing_init(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "No radio ctx");

    SAH_TRACEZ_INFO(ME, "Initialize CSI client list");
    if(amxc_llist_init(&pRad->csiClientList) != 0) {
        SAH_TRACEZ_ERROR(ME, "Client list initialization failed");
    }
}

void wld_sensing_cleanup(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "No radio ctx");

    SAH_TRACEZ_INFO(ME, "Cleanup CSI client list");
    amxc_llist_for_each(it, &pRad->csiClientList) {
        wld_csiClient_t* client = amxc_llist_it_get_data(it, wld_csiClient_t, it);
        amxc_llist_it_take(&client->it);
        free(client);
    }
}

