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

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "wld_apMld.h"

#define ME "mld"

/**
 * Retrieve an affiliatedStaInfo for a given MAC Address associated to a given MLD unit.
 *
 * Because inactive affiliated sta is remembered, and affiliatedSta mac addresses may be reused on other band
 * it is possible there are multiple afSta with the same mac address, although there should never be more than one
 * of them active at the same time.
 *
 * @param info: the struct in which the affiliated sta info will be written
 * @param mldUnit: the mld unit ID of the ap MLD.
 * @param mac: the mac address for which to do a lookup for a matching affiliated sta.
 *
 * @return
 *  - true if an affiliated sta with the given mac address has been found. The info struct
 * shall be properly filled in with the affiliated sta, the associated device to which the affiliated sta belongs,
 * and the access point to which the associated device is associated.
 * - false if no matching affiliated sta can be found, or some error took place. In this case, the info
 * parameter will not be touched.
 */
bool wld_apMld_fetchAffiliatedStaInfo(wld_apMld_afStaInfo_t* info, int32_t mldUnit, swl_macBin_t* mac) {
    ASSERT_NOT_NULL(info, false, ME, "NULL");
    ASSERT_NOT_NULL(mac, false, ME, "NULL");

    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* tmpAp = NULL;
        wld_rad_forEachAp(tmpAp, pRad) {
            if(tmpAp == NULL) {
                continue;
            }

            if(tmpAp->pSSID == NULL) {
                continue;
            }

            if(tmpAp->pSSID->mldUnit != mldUnit) {
                continue;
            }

            for(int i = 0; i < tmpAp->AssociatedDeviceNumberOfEntries; i++) {
                T_AssociatedDevice* pAD = tmpAp->AssociatedDevice[i];
                amxc_llist_for_each(it, &pAD->affiliatedStaList) {
                    wld_affiliatedSta_t* afSta = amxc_llist_it_get_data(it, wld_affiliatedSta_t, it);
                    if(swl_mac_binMatches(&afSta->mac, mac)) {
                        info->afSta = afSta;
                        info->pAD = pAD;
                        info->mainAp = tmpAp;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/**
 * Fetch the active AffiliatedStaInfo for a given MAC afSta connected to a given AP.
 */
bool wld_apMld_getActiveApAffiliatedStaInfo(wld_apMld_afStaInfo_t* info, T_AccessPoint* pAP, swl_macBin_t* mac) {
    ASSERT_NOT_NULL(info, false, ME, "NULL");
    ASSERT_NOT_NULL(mac, false, ME, "NULL");

    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* tmpAp = NULL;
        wld_rad_forEachAp(tmpAp, pRad) {
            if(tmpAp == NULL) {
                continue;
            }

            if(tmpAp->pSSID == NULL) {
                continue;
            }

            if(tmpAp->pSSID->mldUnit != pAP->pSSID->mldUnit) {
                continue;
            }

            for(int i = 0; i < tmpAp->AssociatedDeviceNumberOfEntries; i++) {
                T_AssociatedDevice* pAD = tmpAp->AssociatedDevice[i];
                if(!pAD->Active) {
                    continue;
                }

                amxc_llist_for_each(it, &pAD->affiliatedStaList) {
                    wld_affiliatedSta_t* afSta = amxc_llist_it_get_data(it, wld_affiliatedSta_t, it);
                    if(!afSta->active) {
                        continue;
                    }

                    if(swl_mac_binMatches(&afSta->mac, mac) && (afSta->pAP == pAP)) {
                        info->afSta = afSta;
                        info->pAD = pAD;
                        info->mainAp = tmpAp;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/*
 * @brief check whether one APMLD link have applicable and shared
 * ssid and security configurations (secMode, keypass) values
 * with the other links
 */
bool wld_apMld_hasSharedConnectionConf(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    wld_mldLink_t* pLink = pSSID->pMldLink;
    if(!wld_mld_checkUsableLinkBasicConditions(pLink)) {
        return false;
    }

    uint32_t countUsable = 0;
    uint32_t countSharedConf = 0;
    wld_mldLink_t* pNgLink = NULL;
    wld_for_eachNeighMldLink(pNgLink, pLink) {
        if(!wld_mld_checkUsableLinkBasicConditions(pNgLink)) {
            continue;
        }
        countUsable++;
        T_SSID* pNgLinkSSID = wld_mld_getLinkSsid(pNgLink);
        if(swl_str_matches(pNgLinkSSID->SSID, pSSID->SSID) &&
           wld_ap_sec_checkSharedSecConfigs(pNgLinkSSID->AP_HOOK, pAP)) {
            countSharedConf++;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: countUsableLinks:%d countSharedConfLinks:%d",
                    pAP->alias, countUsable, countSharedConf);

    /*
     * APMLD links must have same SSID and valid shared security config (mode,psk,saePassPhrase)
     * in order to be usable.
     * otherwise, the link configs are misaligned, so the MLD is split into individual links.
     */
    if((countUsable > 0) && (countSharedConf > 0) &&
       (countUsable == countSharedConf)) {
        return true;
    }
    return false;
}

void wld_ap_mld_notifyChange(wld_mld_t* pMld, wld_mldChangeEvent_e event, const char* reason) {

    SAH_TRACEZ_INFO(ME, "MLD Event %d for unit %d - Reason: %s",
                    event, pMld->unit, reason);

    switch(event) {
    case WLD_MLD_EVT_ADD:
        if(wld_ap_mld_getOrCreateDmObject(pMld->unit, pMld->pGroup->type, pMld) != NULL) {
        } else {
            SAH_TRACEZ_ERROR(ME, "Failed to create/get DM object for MLD unit %d", pMld->unit);
        }
        break;

    default:
        SAH_TRACEZ_INFO(ME, "Unhandled MLD event: %d", event);
        break;
    }
}

amxd_object_t* wld_ap_mld_getOrCreateDmObject(uint32_t mld_unit, wld_ssidType_e mld_type, wld_mld_t* pMld_internal) {
    if(mld_type != WLD_SSID_TYPE_AP) {
        SAH_TRACEZ_ERROR(ME, "Expected AP SSID type for APMLD");
        return NULL;
    }

    amxd_object_t* object = NULL;
    amxd_object_t* rootObj = get_wld_object();
    char path[128];
    uint32_t dm_instance_id = 0;
    dm_instance_id = mld_unit + 1;
    snprintf(path, sizeof(path), "APMLD.%u", dm_instance_id);
    object = amxd_object_findf(rootObj, "%s", path);

    if(object == NULL) {
        SAH_TRACEZ_INFO(ME, "APMLD instance %s not found. Proceeding with creation", path);
        amxd_object_t* parent_obj = get_wld_object();
        ASSERT_NOT_NULL(parent_obj, NULL, ME, "WiFi object not found in DM Cannot create APMLD instance for MLDUnit %u.", mld_unit);
        amxd_object_t* templateObject = amxd_object_get(parent_obj, "APMLD");
        ASSERT_NOT_NULL(templateObject, NULL, ME, "APMLD template object not found, cannot create instance for MLDUnit %u.", mld_unit);
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(templateObject, &trans, NULL, ME, "%s: Failed to init transaction for APMLD %u creation", ME, mld_unit);
        amxd_trans_add_inst(&trans, dm_instance_id, NULL);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, NULL, ME, "%s: Failed to apply transaction for APMLD %u creation", ME, mld_unit);
        object = amxd_object_get_instance(templateObject, NULL, dm_instance_id);
        ASSERT_NOT_NULL(object, NULL, ME, "%s: Failed to retrieve newly created APMLD instance %u after transaction.", ME, mld_unit);
        SAH_TRACEZ_INFO(ME, "New APMLD instance %s created for MLDUnit %u", path, mld_unit);
    } else {
        SAH_TRACEZ_INFO(ME, "Found existing APMLD instance: %s (MLD Unit %u). Linking.", path, mld_unit);
    }
    pMld_internal->object = object;
    object->priv = pMld_internal;
    return object;
}


amxd_object_t* wld_ap_createAffiliatedAPObjects(wld_mldLink_t* pLink, uint32_t dm_instance) {

        uint32_t dm_instance_id = 0;
        dm_instance_id = dm_instance + 1;
        wld_mldLink_t* pTempLink = pLink;
        amxd_object_t* CurrObj = pLink->AffObj;
        if(CurrObj){
                SAH_TRACEZ_INFO(ME, "linkId (%d): current Affiliated object exists so deleting all the instances below", dm_instance);
                wld_ap_deleteAffiliatedAPObjects(pTempLink);
                pLink->linkId = -1;
        }
        else {
                SAH_TRACEZ_INFO(ME, "linkId (%d): current Affiliated object don't exists", dm_instance);
        }
        SAH_TRACEZ_INFO(ME, "linkId (%d): calling create affiliated ap %u", dm_instance, pLink->configured);

        amxd_object_t* parent_obj = pLink->pMld->object;
        if (parent_obj == NULL) {
                SAH_TRACEZ_ERROR(ME, "pMld->object is NULL, cannot create AffiliatedAP instances");
                return NULL;
        }

        amxd_object_t* templateObject = amxd_object_get(parent_obj, "AffiliatedAP");
        ASSERT_NOT_NULL(templateObject, NULL, ME, "%s: Could not get template", pLink->pSSID->Name);
        SAH_TRACEZ_INFO(ME, "%s: Creating template object for ssid", pLink->pSSID->Name);
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(templateObject, &trans, NULL, ME, "%s : trans init failure", pLink->pSSID->Name);

        amxd_trans_add_inst(&trans, dm_instance_id, NULL);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, NULL, ME, "%s : trans apply failure", pLink->pSSID->Name);

        pLink->AffObj = amxd_object_get_instance(templateObject, NULL, dm_instance_id);
        ASSERT_NOT_NULL(pLink->AffObj, NULL, ME, "%s: failure to create object", pLink->pSSID->Name);
        pLink->AffObj->priv = pLink;
        pTempLink = NULL;
        return pLink->AffObj;

}


amxd_status_t wld_ap_deleteAffiliatedAPObjects(wld_mldLink_t* pStartLink) {
        if (pStartLink == NULL || pStartLink->pMld == NULL) {
                SAH_TRACEZ_ERROR(ME, "Invalid start link or parent MLD");
                return amxd_status_unknown_error;
        }

        amxd_status_t status = amxd_status_ok;
        amxc_llist_it_t* it = &pStartLink->it;  // starting point in the list
        amxc_llist_it_t* next_it = NULL;

        // Get template once from the parent MLD
        amxd_object_t* parent_obj = pStartLink->pMld->object;
        if (parent_obj == NULL) {
                SAH_TRACEZ_ERROR(ME, "pMld->object is NULL, cannot retrieve AffiliatedAP instances");
                return amxd_status_unknown_error;
        }

        amxd_object_t* ObjTempl = amxd_object_get(parent_obj, "AffiliatedAP");
        if (ObjTempl == NULL) {
                SAH_TRACEZ_ERROR(ME, "Failed to get AffiliatedAP template from parent object");
                return amxd_status_unknown_error;
        }

        // Loop from current link till end of list
        while (it != NULL) {
                wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
                next_it = amxc_llist_it_get_next(it);  // store next iterator in advance

                if (pLink->AffObj != NULL) {
                        uint32_t instance_id = amxd_object_get_index(pLink->AffObj);
                        SAH_TRACEZ_INFO(ME, "Deleting DM instance %u for %s", instance_id, pLink->pSSID->Name);

                        amxd_trans_t trans;
                        ASSERT_TRUE(instance_id > 0, false, ME, "wrong instance index");
                        ASSERT_TRANSACTION_INIT(ObjTempl, &trans, false, ME, "%s: Failed to init transaction for deleting AffiliatedAP instance %u for %s", ME, instance_id, pLink->pSSID->Name)

                        amxd_trans_del_inst(&trans, instance_id, NULL);
                        // Clear private pointer before applying
                        pLink->AffObj->priv = NULL;
                        if(swl_object_finalizeTransactionOnLocalDm(&trans) != amxd_status_ok) {
                                SAH_TRACEZ_ERROR(ME, "%s : trans apply failure", pLink->pSSID->Name);
                                pLink->AffObj->priv = pLink;
                                return amxd_status_unknown_error;
                        }

                        pLink->AffObj = NULL;
                        pLink->linkId = -1;
                        SAH_TRACEZ_INFO(ME, "DM instance AffiliatedAP.%u deleted successfully", instance_id);

                } else {
                        SAH_TRACEZ_INFO(ME, "MLD unit %d: Link ID %d: No DM object found to delete", pLink->pMld->unit, pLink->linkId);
                }

                it = next_it;  // move to next link
        }

        return status;
}
