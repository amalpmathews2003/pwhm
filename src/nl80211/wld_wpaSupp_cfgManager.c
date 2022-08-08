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
#include <errno.h>
#include <swl/fileOps/swl_fileUtils.h>
#include <swl/fileOps/swl_mapWriterKVP.h>
#include "wld.h"
#include "wld_util.h"
#include "wld_wpaSupp_cfgManager.h"
#include "wld_wpaSupp_cfgManager_priv.h"

#define ME "fileMgr"

/**
 * @brief create wld_wpaSupp_config_t structure
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 *
 * @return true on success.
 * Otherwise, false:
 *      - conf is NULL
 *      - lack of memory
 */
bool wld_wpaSupp_createConfig(wld_wpaSupp_config_t** conf) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    wld_wpaSupp_config_t* config = calloc(1, sizeof(wld_wpaSupp_config_t));
    ASSERT_NOT_NULL(config, false, ME, "Config allocation failed");
    *conf = config;
    // Init the global map
    swl_mapChar_init(&(config->global));
    // Init the network map
    swl_mapChar_init(&(config->network));
    return true;
}

/**
 * @brief load the wpaSupplicant configuration from wpaSupplicant config file into wld_wpaSupp_config_t structure
 *
 * @param conf the structure mapping the content of wpaSupplicant configuration file
 * @param path the path of wpaSupplicant config file
 *
 * @return true on success.
 * Otherwise, false:
 *      - conf is NULL or path is NULL
 *      - config file not found
 */
bool wld_wpaSupp_loadConfig(wld_wpaSupp_config_t** conf, char* path) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    ASSERT_STR(path, false, ME, "Empty path");
    // Read the hostapd config file line by line and fill the config structure
    // Open the file
    FILE* fp = fopen(path, "r");
    ASSERTS_NOT_NULL(fp, false, ME, "NULL");

    wld_wpaSupp_config_t* config = calloc(1, sizeof(wld_wpaSupp_config_t));
    ASSERT_NOT_NULL(config, false, ME, "Config allocation of %s failed", path);
    *conf = config;

    char line[4096];
    char* pos;
    char* start;
    char* end;
    char* key;
    char* value;
    bool isGlobalParsing = true;

    // Init both global and network maps
    swl_mapChar_init(&(config->global));
    swl_mapChar_init(&(config->network));

    while(fgets(line, sizeof(line), fp) != NULL) {
        pos = line;

        // skip white space from the beginning of line
        while((*pos == ' ') || (*pos == '\t') || (*pos == '\r')) {
            pos++;
        }
        // skip comment lines and empty lines
        if((*pos == '#') || (*pos == '\n') || (*pos == '\0')) {
            continue;
        }

        start = pos;
        end = strchr(start, '\n');
        if(end != NULL) {
            *end = '\0';
        }
        pos = strchr(start, '=');
        if(pos == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid line %s", path, line);
            continue;
        }
        *pos = '\0';

        key = start;
        value = pos + 1;

        if(swl_str_matches(key, "network")) {
            isGlobalParsing = false;
            continue;
        }

        if(isGlobalParsing) {
            swl_mapChar_add(&(config->global), key, value);
        } else {
            swl_mapChar_add(&(config->network), key, value);
        }
    }
    fclose(fp);
    return true;
}

/**
 * @brief delete a wld_wpaSupp_config_t map already loaded from wpa_supplicant file
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 *
 * @return true on success.
 * Otherwise, false
 *      - conf is NULL
 */
bool wld_wpaSupp_deleteConfig(wld_wpaSupp_config_t* conf) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    swl_mapChar_cleanup(&(conf->global));
    swl_mapChar_cleanup(&(conf->network));
    free(conf);
    return true;
}

/**
 * @brief write the content of wpaSupp_config_t into a file
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 * @param path the filename where the conf is written
 *
 * @return true if the conf map is successfully written in the file
 * Otherwise, false:
 *      - conf is NULL or path is NULL
 *      - if writing of the conf in the file "path" failed
 */
bool wld_wpaSupp_writeConfig(wld_wpaSupp_config_t* conf, char* path) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    ASSERT_STR(path, false, ME, "Empty path");
    char tmpName[128];
    bool ret;

    // create a temporary file in write mode
    snprintf(tmpName, sizeof(tmpName), "%s.tmp.txt", path);
    FILE* fptr = fopen(tmpName, "w");
    ASSERTS_NOT_NULL(fptr, false, ME, "NULL");

    // Write the global config into a temporary file
    ret = swl_mapWriterKVP_writeToFptrEsc(&(conf->global), fptr, "\\", '\\');
    if(!ret) {
        SAH_TRACEZ_ERROR(ME, "writing global config failed");
        fclose(fptr);
        unlink(tmpName);
        return false;
    }

    // Write the network config into a temporary file
    fprintf(fptr, "\nnetwork={\n");
    ret = swl_mapWriterKVP_writeToFptrEsc(&(conf->network), fptr, "\\", '\\');
    if(!ret) {
        SAH_TRACEZ_ERROR(ME, "writing network config failed");
    }
    fprintf(fptr, "}\n");

    // close the temporary
    fclose(fptr);

    // Rename the file
    if(ret) {
        ret = rename(tmpName, path) >= 0 ? true : false;
    }
    unlink(tmpName);

    return ret;
}

/**
 * @brief add/update a key value in the config map
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 * @param isGlobal if the key to add/update is a global parameter or a network one
 * @param key the key to add/update
 * @param key the new key value
 *
 * @return true if the parameter is added/updated.
 * Otherwise false:
 *      - conf = NULL or key = NULL or value = NULL
 */
bool wld_wpaSupp_addConfigParam(wld_wpaSupp_config_t* conf, const char* key, const char* value, bool isGlobal) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    ASSERTS_NOT_NULL(value, false, ME, "NULL");
    bool ret;

    if(isGlobal) {
        ret = swl_map_addOrSet(&conf->global, (char*) key, (char*) value);
    } else {
        ret = swl_map_addOrSet(&conf->network, (char*) key, (char*) value);
    }
    return ret;
}


/**
 * @brief delete a key/value from the config map
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 * @param key the key to delete
 *
 * @return true if the parameter is deleted.
 * Otherwise false:
 *      - conf = NULL or key = NULL or value = NULL
 *      - key is NULL or doesn't exist
 */
bool wld_wpaSupp_delConfigParam(wld_wpaSupp_config_t* conf, char* key) {
    ASSERTS_NOT_NULL(conf, false, ME, "NULL");
    ASSERTS_NOT_NULL(key, false, ME, "NULL");

    swl_mapChar_t* section = &conf->network;
    bool ret = swl_map_has(&conf->global, key);
    if(ret) {
        section = &conf->global;
    } else {
        ret = swl_map_has(&conf->network, key);
    }

    if(ret) {
        swl_map_delete(section, key);
    }
    return ret;
}

/**
 * @brief get global config
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 *
 * @return a pointer to the global config map
 * Otherwise NULL:
 *      - conf = NULL
 */
swl_mapChar_t* wld_wpaSupp_getGlobalConfig(wld_wpaSupp_config_t* conf) {
    ASSERTS_NOT_NULL(conf, NULL, ME, "NULL");
    return &(conf->global);
}

/**
 * @brief get network config
 *
 * @param conf the structure mapping the content of wpa_supplicant configuration file
 *
 * @return a pointer to the network config map
 * Otherwise NULL:
 *      - conf = NULL
 */
swl_mapChar_t* wld_wpaSupp_getNetworkConfig(wld_wpaSupp_config_t* conf) {
    ASSERTS_NOT_NULL(conf, NULL, ME, "NULL");
    return &(conf->network);
}
