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

#define ME "radIfM"

#include <stdlib.h>
#include <debug/sahtrace.h>



#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <swl/swl_commonLib.h>
#include <swl/swl_conversion.h>
#include <swl/swl_string.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_endpoint.h"
#include "wld_wps.h"
#include "wld_channel.h"
#include "swl/swl_assert.h"
#include "wld_statsmon.h"
#include "wld_channel_types.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_stamon.h"
#include "wld_assocdev.h"
#include "wld_tinyRoam.h"

amxd_status_t _addVAPIntf(amxd_object_t* obj _UNUSED,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval) {
    int ret = WLD_ERROR;

    const char* radioname = GET_CHAR(args, "radio");
    const char* vapname = GET_CHAR(args, "vap");
    /* uint32 addVAPIntf(string vap, string radio); */

    /* Get our T_Radio pointer of the selected radio */
    T_Radio* pR = wld_getRadioDataHandler(get_wld_object(), radioname);

    if(pR == NULL) {
        goto leave;
    }

    T_AccessPoint* pAP = wld_ap_getVapByName(vapname);
    if(pAP != NULL) {
        SAH_TRACEZ_ERROR(ME, "vap with name %s is already created", vapname);
        goto leave;
    }

    amxd_object_t* apParentObject = amxd_object_get_child(get_wld_object(), "AccessPoint");
    amxd_object_t* apObj;
    amxd_object_add_instance(&apObj, apParentObject, vapname, 0, NULL);
    if(apObj == NULL) {
        SAH_TRACEZ_ERROR(ME, "amxd_object_add_instance() failed");
        goto leave;
    }

    char* apObjPath = amxd_object_get_path(pR->pBus, AMXD_OBJECT_NAMED);
    if(apObjPath != NULL) {
        amxd_object_set_cstring_t(apObj, "RadioReference", apObjPath);
        free(apObjPath);
    }

    if(!(apParentObject)) {
        SAH_TRACEZ_ERROR(ME, "Failed to commit");
        goto leave;
    }

    amxd_object_t* ssidParentObject = amxd_object_get_child(get_wld_object(), "SSID");
    amxd_object_t* ssidObj;
    amxd_object_add_instance(&ssidObj, ssidParentObject, vapname, 0, NULL);
    if(ssidObj == NULL) {
        SAH_TRACEZ_ERROR(ME, "amxd_object_add_instance() failed");
        goto leave;
    }

    char ssid[SSID_NAME_LEN] = {'\0'};
    snprintf(ssid, SSID_NAME_LEN, "NeMo_SSID%d", amxd_object_get_instance_count(ssidParentObject));
    amxd_object_set_cstring_t(ssidObj, "SSID", ssid);

    if(!(ssidParentObject)) {
        SAH_TRACEZ_ERROR(ME, "Failed to commit");
        goto leave;
    }

    ret = WLD_OK;

leave:
    if(ret < 0) {
        /* ERROR! */
        SAH_TRACEZ_ERROR(ME, "addVAPIntf failed");
    }
    amxc_var_set(int32_t, retval, ret);

    return (ret < 0) ? amxd_status_unknown_error : amxd_status_ok;
}

amxd_status_t _delVAPIntf(amxd_object_t* wifi,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval _UNUSED) {
    const char* vapname = GET_CHAR(args, "vap");

    if(vapname == NULL) {
        SAH_TRACEZ_ERROR(ME, "no vapname found");
        return amxd_status_unknown_error;
    }
    /* remove our object entry out of the SSID and ACCESSPOINT table */
    amxd_object_t* apChild = amxd_object_get_child(wifi, "AccessPoint");
    amxd_object_t* apObj = amxd_object_get_instance(apChild, vapname, 0);
    amxd_object_t* ssidChild = amxd_object_get_child(wifi, "SSID");
    amxd_object_t* ssidObj = amxd_object_get_instance(ssidChild, vapname, 0);

    if(!apObj || !ssidObj) {
        SAH_TRACEZ_ERROR(ME, "%s: no vap object found", vapname);
        return amxd_status_unknown_error;
    }

    T_AccessPoint* pAP = (T_AccessPoint*) apObj->priv;
    if(pAP) {
        wld_ap_destroy(pAP);
    } else {
        SAH_TRACEZ_ERROR(ME, "%s: no vap data found", vapname);
    }

    amxd_object_delete(&apObj);
    amxd_object_delete(&ssidObj);
    return amxd_status_ok;
}

/**
 * @brief ssid_create
 *
 * Create a new SSID instance based on the SSID name
 * and set default SSID parameters like SSID and MAC
 *
 * @param pBus root Wifi object
 * @param pR pointer to the radio struct
 * @param name the name of the ssid we want to create
 * @return ssid object
 */
amxd_object_t* ssid_create(amxd_object_t* pBus, T_Radio* pR, const char* name) {
    if(!pBus || !pR || !name) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : !pBus||!pR||!name");
        return NULL;
    }

    SAH_TRACEZ_INFO(ME, "Creating a new SSID instance - ssid_name:[%s]", name);

    /* Create a new SSID instance */
    amxd_object_t* ssidobject = amxd_object_get_child(pBus, "SSID");
    if(!ssidobject) {
        SAH_TRACEZ_ERROR(ME, "SSID object not found");
        return NULL;
    }
    amxd_object_t* ssidinstance;
    amxd_object_add_instance(&ssidinstance, ssidobject, name, 0, NULL);
    if(!ssidinstance) {
        SAH_TRACEZ_ERROR(ME, "Unable to create SSID instance");
        return NULL;
    }

    /* Set the T_SSID struct to the new instance */
    ssidinstance->priv = calloc(1, sizeof(T_SSID));
    T_SSID* pSSID = (T_SSID*) ssidinstance->priv;
    if(!pSSID) {
        amxd_object_delete(&ssidinstance);
        SAH_TRACEZ_ERROR(ME, "Unable to set T_SSID struct to the new instance");
        return NULL;
    }

    /* Interlinking */
    pSSID->debug = SSID_POINTER;
    pSSID->pBus = ssidinstance;
    pSSID->RADIO_PARENT = pR;
    pSSID->status = RST_DOWN;

    /* Set the SSID MACAddress as the Radio Base MAC */
    memcpy(pSSID->MACAddress, pR->MACAddr, sizeof(pR->MACAddr));

    /* Set a custom SSID name */
    sprintf(pSSID->SSID, "NeMo_SSID%d", amxd_object_get_instance_count(ssidobject));

    /* Sync constant parameters : name -> Datamodel */
    amxd_object_set_cstring_t(ssidinstance, "Name", name);
    amxd_object_set_cstring_t(ssidinstance, "Alias", name);
    swl_str_copy(pSSID->Name, sizeof(pSSID->Name), name);

    /* Sync dynamic parameters : T_SSID -> Datamodel */
    syncData_SSID2OBJ(ssidinstance, pSSID, SET | NO_COMMIT);
    return ssidinstance;
}

/**
 * @brief endpoint_create
 *
 * Create a new Endpoint instance based on the Endpoint name
 * and set default Endpoint parameters
 *
 * @param pBus root Wifi object
 * @param pR pointer to the radio struct
 * @param endpointname the name of the endpoint we want to create
 * @param intfname the name of the interface for the endpoint
 * @param intfNr interface number
 * @return endpoint object
 */
amxd_object_t* endpoint_create(amxd_object_t* pBus, T_Radio* pR, const char* endpointname, const char* intfname, int intfNr) {
    if(!pBus || !pR || !endpointname) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : !pBus||!pR||!endpointname");
        return NULL;
    }

    SAH_TRACEZ_INFO(ME, "Creating a new Endpoint instance - endpointname:[%s]", endpointname);

    /* Create a new SSID instance */
    amxd_object_t* endpointobject = amxd_object_get_child(pBus, "EndPoint");
    if(!endpointobject) {
        SAH_TRACEZ_ERROR(ME, "Endpoint object not found");
        return NULL;
    }
    amxd_object_t* endpointinstance;
    amxd_object_add_instance(&endpointinstance, endpointobject, endpointname, 0, NULL);
    if(!endpointinstance) {
        SAH_TRACEZ_ERROR(ME, "Unable to create Endpoint instance");
        return NULL;
    }

    /* Set the T_Endpoint struct to the new instance */
    endpointinstance->priv = calloc(1, sizeof(T_EndPoint));
    T_EndPoint* endpoint = (T_EndPoint*) endpointinstance->priv;
    if(!endpoint) {
        amxd_object_delete(&endpointinstance);
        SAH_TRACEZ_ERROR(ME, "Unable to set T_Endpoint struct to the new instance");
        return NULL;
    }

    /* Interlinking */
    endpoint->debug = ENDP_POINTER;
    endpoint->pBus = endpointinstance;
    endpoint->pRadio = pR;
    endpoint->pFA = pR->pFA;
    amxc_llist_append(&pR->llEndPoints, &endpoint->it);

    /* Set some defaults to the endpoint struct */
    setEndPointDefaults(endpoint, endpointname, intfname, intfNr);

    wld_endpoint_create_reconnect_timer(endpoint);

    /* Sync dynamic parameters : T_Endpoint -> Datamodel */
    syncData_EndPoint2OBJ(endpoint);

    wld_tinyRoam_init(endpoint);
    return endpointinstance;
}

/**
 * @brief _addEndPointIntf
 *
 * Creates an EndPoint interface on the RADIO and updates the EndPoint and SSID object fields
 *
 * @param fcall function call context
 * @param args argument list
 *     - radio : Name of the RADIO interface that derrives the endpoint interface. (wifi0, wifi1,...)
 *     - endpoint : Name of the EndPoint interface that will be created. (wln0, wln1, ...)
 * @param retval variant that must contain the return value
 * @return
 *     - function execution state
 */
amxd_status_t _addEndPointIntf(amxd_object_t* wifi,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    const char* radioname = GET_CHAR(args, "radio");
    const char* endpointname = GET_CHAR(args, "endpoint");
    char* objpath = NULL;
    amxd_object_t* endpointinstance = NULL;
    amxd_object_t* ssidinstance = NULL;
    amxd_object_t* wpsinstance = NULL;

    T_Radio* pR = NULL;
    T_EndPoint* endpoint = NULL;
    T_SSID* pSSID = NULL;

    char endpointnamebuf[64];
    int32_t intfNr;

    bool ret = false;

    SAH_TRACEZ_IN(ME);

    /* Use a fix sized buffer... */
    wldu_copyStr(endpointnamebuf, endpointname, sizeof(endpointnamebuf));

    /* Get our T_Radio pointer of the selected radio */
    pR = wld_getRadioDataHandler(wifi, radioname);
    if(!pR) {
        SAH_TRACEZ_ERROR(ME, "Radio structure for radio [%s] is missing", radioname);
        goto leave;
    }

    /* Try to create the requested interface by calling the HW function */
    SAH_TRACEZ_INFO(ME, "Try to execute HW implementation of addEndPointIntf - radio:[%s] endpoint:[%s]",
                    radioname, endpointname);
    intfNr = pR->pFA->mfn_wrad_addendpointif(pR, endpointnamebuf, sizeof(endpointnamebuf));

    /* Note that we must use the updated endpointnamebuf - Some vendors
     * don't allow us to change the interface name as we request!
     * The intfNr value contains the system interface index value.
     * ((ip link)) */
    if(intfNr < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute HW implementation of addEndPointIntf - radio:[%s] endpoint:[%s]",
                         radioname, endpointname);
        goto leave;
    }
    /* If we succeed on this one, we must also create an SSID
    *  and AccessPoint entry in our data model (see TR181) */

    /* Reserve the VAP interface + SSID */
    endpointinstance = endpoint_create(wifi, pR, endpointname, endpointnamebuf, intfNr);
    if(!endpointinstance) {
        SAH_TRACEZ_ERROR(ME, "Failed to create endpoint instance");
        goto leave;
    }
    endpoint = (T_EndPoint*) endpointinstance->priv;

    wpsinstance = amxd_object_get(endpointinstance, "WPS");
    endpoint->wpsSessionInfo.intfObj = endpointinstance;
    wpsinstance->priv = &endpoint->wpsSessionInfo;
    //function_setHandler(object_getFunction(wpsinstance, "pushButton"), __EndPoint_WPS_pushButton);
    //function_setHandler(object_getFunction(wpsinstance, "cancelPairing"), __EndPoint_WPS_cancelPairing);

    ssidinstance = ssid_create(wifi, pR, endpointname);
    if(!ssidinstance) {
        SAH_TRACEZ_ERROR(ME, "Failed to create ssid instance");
        goto leave;
    }

    pSSID = (T_SSID*) ssidinstance->priv;

    /* Interlink */
    endpoint->pSSID = pSSID;
    pSSID->ENDP_HOOK = endpoint;

    /* Relate both objects with eachother */
    objpath = amxd_object_get_path(ssidinstance, AMXD_OBJECT_NAMED);
    if(!objpath) {
        SAH_TRACEZ_ERROR(ME, "Failed to get object path from ssidinstance");
        goto leave;
    }
    amxd_object_set_cstring_t(endpointinstance, "SSIDReference", objpath);
    free(objpath);

    objpath = amxd_object_get_path(pR->pBus, AMXD_OBJECT_NAMED);
    if(objpath) {
        amxd_object_set_cstring_t(endpointinstance, "RadioReference", objpath);
        free(objpath);
    }

    if((ret = pR->pFA->mfn_wendpoint_create_hook(endpoint))) {
        SAH_TRACEZ_ERROR(ME, "%s: pEP create hook failed", endpoint->Name);
    }

    if(!(wifi)) {
        SAH_TRACEZ_ERROR(ME, "Failed to commit");
        goto leave;
    }

    ret = true;
leave:
    if(false == ret) {
        /* ERROR! */
    }
    amxc_var_set(bool, retval, ret);
    SAH_TRACEZ_OUT(ME);
    return ret ? amxd_status_ok : amxd_status_unknown_error;
}

/**
 * @brief _delEndPointIntf
 *
 * Deletes an EndPoint interface on the RADIO and its associated structures
 *
 * @param fcall function call context
 * @param args argument list
 *     - endpoint : Name of the EndPoint interface that will be created (wln0, wln1, ...)
 * @param retval : variant that must contain the return value
 * @return
 *     - function execution state
 */
amxd_status_t _delEndPointIntf(amxd_object_t* wifi,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    const char* endpointname = GET_CHAR(args, "endpoint");

    amxd_object_t* epObj = NULL;
    amxd_object_t* ssidObj = NULL;
    T_Radio* pR = NULL;
    T_EndPoint* pEP = NULL;

    bool ret = false;

    SAH_TRACEZ_IN(ME);

    epObj = amxd_object_findf(wifi, "EndPoint.%s", endpointname);
    if(!epObj) {
        SAH_TRACEZ_ERROR(ME, "Endpoint instance not found");
        goto leave;
    }
    ssidObj = amxd_object_findf(wifi, "SSID.%s", endpointname);
    if(!ssidObj) {
        SAH_TRACEZ_ERROR(ME, "SSID instance not found");
        goto leave;
    }

    /* Get our Radio system handler */
    pEP = (T_EndPoint*) epObj->priv;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Endpoint structure for endpoint [%s] is missing", endpointname);
        goto leave;
    }
    pR = (T_Radio*) pEP->pRadio;
    if(!pR) {
        SAH_TRACEZ_ERROR(ME, "Radio structure for endpoint [%s] is missing", endpointname);
        goto leave;
    }

    if(pR->pFA->mfn_wendpoint_destroy_hook(pEP)) {
        SAH_TRACEZ_ERROR(ME, "%s: pEP destroy hook failed", pEP->Name);
    }

    /* Try to delete the requested interface by calling the HW function */
    if(pR->pFA->mfn_wrad_delendpointif(pR, (char*) endpointname) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to execute HW implementation to delete the interface - endpoint:[%s]",
                         endpointname);
        goto leave;
    }

    /* Remove our object entry out of the SSID and ENDPOINT table */
    amxd_object_delete(&epObj);
    amxd_object_delete(&ssidObj);

    /* Take EP also out the Radio */
    amxc_llist_it_take(&pEP->it);

    if(!(wifi)) {
        SAH_TRACEZ_ERROR(ME, "Failed to commit");
        goto leave;
    }

    ret = true;
leave:
    if(false == ret) {
        /* ERROR! */
    }

    amxc_var_set(bool, retval, ret);
    SAH_TRACEZ_OUT(ME);
    return ret ? amxd_status_ok : amxd_status_unknown_error;
}

