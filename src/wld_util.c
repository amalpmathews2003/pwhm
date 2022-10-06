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
#include "wld_util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <openssl/evp.h>

#include "swl/swl_assert.h"
#include "swla/swla_conversion.h"
#include "swl/swl_string.h"
#include "swl/swl_hex.h"

#include "wld_linuxIfUtils.h"

#define ME "util"

const char* com_dir_char[COM_DIR_MAX] = {"Tx", "Rx"};

/*
** Country/Region Codes from MS WINNLS.H
** Numbering from ISO 3166 short names from Broadcom driver.
*/
const T_COUNTRYCODE Rad_CountryCode[] = {
    { 250, "European Union", "EU" },
    { 250, "European Union 5G+", "EU2"},
    {   0, "Afghanistan", "AF" },
    {   8, "Albania", "AL" },
    {  12, "Algeria", "DZ" },
    {   0, "American Samoa", "AS" },
    {   0, "Angola", "AO" },
    {   0, "Anguilla", "AI" },
    {   0, "Antigua and Barbuda", "AG" },
    {  32, "Argentina", "AR" },
    {  51, "Armenia", "AM" },
    {   0, "Aruba", "AM" },
    {  36, "Australia", "AU" },
    {  40, "Austria", "AT" },
    {  31, "Azerbaijan", "AZ" },
    {   0, "Bahamas", "BS" },
    {  48, "Bahrain", "BH" },
    {   0, "Baker Island", "OB" },
    {  50, "Bangladesh", "BD" },
    {   0, "Barbados", "BB" },
    { 112, "Belarus", "BY" },
    {  56, "Belgium", "BE" },
    {  84, "Belize", "BZ" },
    {   0, "Benin", "BJ" },
    {   0, "Bermuda", "BM" },
    {   0, "Bhutan", "BT" },
    {  68, "Bolivia", "BO" },
    {  70, "Bosnia and Herzegovina", "BA" },
    {   0, "Botswana", "BW" },
    {  76, "Brazil", "BR" },
    {   0, "British Indian Ocean Territory", "IO" },
    {  96, "Brunei Darussalam", "BN" },
    { 100, "Bulgaria", "BG" },
    {   0, "Burkina Faso", "BF" },
    {   0, "Burundi", "BI" },
    {   0, "Cambodia", "KH" },
    {   0, "Cameroon", "CM" },
    { 124, "Canada", "CA" },
    {   0, "Cape Verde", "CV" },
    {   0, "Cayman Islands", "KY" },
    {   0, "Central African Republic", "CF" },
    {   0, "Chad", "TD" },
    { 152, "Chile", "CL" },
    { 156, "China", "CN" },
    {   0, "Christmas Island", "CX" },
    { 170, "Colombia", "CO" },
    {   0, "Comoros", "KM" },
    {   0, "Congo", "CG" },
    {   0, "Congo, The Democratic Republic Of The", "CD" },
    { 188, "Costa Rica", "CR" },
    {   0, "Cote D'ivoire", "CI" },
    { 191, "Croatia", "HR" },
    {   0, "Cuba", "CU" },
    { 196, "Cyprus", "CY" },
    { 203, "Czech Republic", "CZ" },
    { 208, "Denmark", "DK" },
    {   0, "Djibouti", "DJ" },
    {   0, "Dominica", "DM" },
    { 214, "Dominican Republic", "DO" },
    { 218, "Ecuador", "EC" },
    { 818, "Egypt", "EG" },
    { 222, "El Salvador", "SV" },
    {   0, "Equatorial Guinea", "GQ" },
    {   0, "Eritrea", "ER" },
    { 233, "Estonia", "EE" },
    {   0, "Ethiopia", "ET" },
    {   0, "Falkland Islands (Malvinas)", "FK" },
    { 234, "Faroe Islands", "FO" },
    {   0, "Fiji", "FJ" },
    { 246, "Finland", "FI" },
    { 250, "France", "FR" },
    { 255, "French Guina", "GF" },
    { 255, "French Polynesia", "PF" },
    { 255, "French Southern Territories", "TF" },
    {   0, "Gabon", "GA" },
    {   0, "Gambia", "GM" },
    { 268, "Georgia", "GE" },
    {   0, "Ghana", "GH" },
    {   0, "Gibraltar", "GI" },
    { 276, "Germany", "DE" },
    { 300, "Greece", "GR" },
    {   0, "Grenada", "GD" },
    {   0, "Guadeloupe", "GP" },
    {   0, "Guam", "GU" },
    { 320, "Guatemala", "GT" },
    {   0, "Guernsey", "GG" },
    {   0, "Guinea", "GN" },
    {   0, "Guinea-bissau", "GW" },
    {   0, "Guyana", "GY" },
    {   0, "Haiti", "HT" },
    {   0, "Holy See (Vatican City State)", "VA" },
    { 340, "Honduras", "HN" },
    { 344, "Hong Kong", "HK" },
    { 348, "Hungary", "HU" },
    { 352, "Iceland", "IS" },
    { 256, "India", "IN" },
    { 360, "Indonesia", "ID" },
    { 364, "Iran, Islamic Republic Of", "IR" },
    {   0, "Iraq", "IQ" },
    { 372, "Ireland", "IE" },
    { 376, "Israel", "IL" },
    { 380, "Italy", "IT" },
    { 388, "Jamaica", "JM" },
    { 392, "Japan", "JP" },
    { 393, "Japan_1", "J1" },
    { 394, "Japan_2", "J2" },
    { 395, "Japan_3", "J3" },
    { 396, "Japan_4", "J4" },
    { 397, "Japan_5", "J5" },
    { 399, "Japan_6", "J6" },
    {4007, "Japan_7", "J7" },
    {4008, "Japan_8", "J8" },
    {4009, "Japan_9", "J9" },
    {4010, "Japan_10", "J10"},
    {   0, "Jersey", "JE" },
    { 400, "Jordan", "JO" },
    { 398, "Kazakhstan", "KZ" },
    { 404, "Kenya", "KE" },
    {   0, "Kiribati", "KI" },
    { 410, "Korea, Republic Of", "KR" },
//408,      KOREA_NORTH             /* North Korea */
//410,      KOREA_ROC               /* South Korea */
//411,      KOREA_ROC2              /* South Korea */
    { 414, "Kuwait", "KW" },
    {   0, "Kosovo (No country code, use 0A)", "0A" },
    {   0, "Kyrgyzstan", "KG" },
    {   0, "Lao People's Democratic Repubic", "LA" },
    { 428, "Latvia", "LV" },
    { 422, "Lebanon", "LB" },
    {   0, "Lesotho", "LS" },
    {   0, "Liberia", "LR" },
    { 434, "Libyan Arab Jamahiriya", "LY" },
    { 438, "Liechtenstein", "LI" },
    { 440, "Lithuania", "LT" },
    { 442, "Luxembourg", "LU" },
    { 446, "Macau", "MA" },
    { 807, "Macedonia", "MK" },
    {   0, "Madagascar", "MG" },
    {   0, "Malawi", "MW" },
    {   0, "Macao", "MO" },
    { 458, "Malaysia", "MY" },
    {   0, "Maldives", "MV" },
    {   0, "Mali", "ML" },
    {   0, "Malta", "MT" },
    {   0, "Man, Isle Of", "IM" },
    {   0, "Martinique", "MQ" },
    {   0, "Mauritania", "MR" },
    {   0, "Mauritius", "MU" },
    {   0, "Mayotte", "YT" },
    { 484, "Mexico", "MX" },
    {   0, "Micronesia, Federated States Of", "FM" },
    {   0, "Moldova, Republic Of", "MD" },
    { 492, "Monaco", "MC" },
    {   0, "Mongolia", "MN" },
    {   0, "Montenegro", "ME" },
    {   0, "Montserrat", "MS" },
    { 504, "Morocco", "MA" },
    {   0, "Mozambique", "MZ" },
    {   0, "Myanmar", "MM" },
    {   0, "Namibia", "NA" },
    {   0, "Nauru", "NR" },
    { 524, "Nepal", "NP" },
    { 528, "Netherlands", "NL" },
    {   0, "Netherlands Antilles", "AN" },
    {   0, "New Caledonia", "NC" },
    { 554, "New Zealand", "NZ" },
    { 558, "Nicaragua", "NI" },
    {   0, "Niger", "NE" },
    {   0, "Nigeria", "NG" },
    {   0, "Niue", "NU" },
    {   0, "Norfolk Island", "NF" },
    {   0, "Northern Mariana Islands", "MP" },
    { 578, "Norway", "NO" },
    { 512, "Oman", "OM" },
    { 586, "Pakistan", "PK" },
    {   0, "Palau", "PW" },
    { 591, "Panama", "PA" },
    {   0, "Papua New Guinea", "PG" },
    { 600, "Paraguay", "PY" },
    { 604, "Peru", "PE" },
    { 608, "Philippines", "PH" },
    { 616, "Poland", "PL" },
    { 620, "Portugal", "PT" },
    { 630, "Pueto Rico", "PR" },
    { 634, "Qatar", "QA" },
    {   0, "Reunion", "RE" },
    { 642, "Romania", "RO" },
    {   0, "Rwanda", "RW" },
    { 643, "Russia", "RU" },
    {   0, "Saint Kitts and Nevis", "KN" },
    {   0, "Saint Lucia", "LC" },
    {   0, "Sanit Martin / Sint Marteen", "MF" },
    {   0, "Saint Pierre and Miquelon", "PM" },
    {   0, "Saint Vincent and The Grenadines", "VC" },
    {   0, "Samoa", "WS" },
    {   0, "San Marino", "SM" },
    {   0, "Sao Tome and Principe", "ST" },
    { 682, "Saudi Arabia", "SA" },
    {   0, "Senegal", "SN" },
    {   0, "Serbia", "RS" },
    {   0, "Seychelles", "SC" },
    {   0, "Sierra Leone", "SL" },
    { 702, "Singapore", "SG" },
    { 703, "Slovakia", "SK" },
    { 705, "Slovenia", "SI" },
    {   0, "Solomon Islands", "SB" },
    {   0, "Somalia", "SO" },
    { 710, "South Africa", "ZA" },
    { 724, "Spain", "ES" },
    { 144, "Sri Lanka", "LK" },
    {   0, "Suriname", "SR" },
    {   0, "Swaziland", "SZ" },
    { 752, "Sweden", "SE" },
    { 756, "Switzerland", "CH" },
    { 760, "Syria", "SY" },
    { 158, "Taiwan, Province Of China", "TW" },
    {   0, "Tajikistan", "TJ" },
    {   0, "Tanzania, United Republic Of", "TZ" },
    { 764, "Thailand", "TH" },
    {   0, "Togo", "TG" },
    {   0, "Tonga", "TO" },
    { 780, "Trinidad and Tobago", "TT" },
    { 788, "Tunisia", "TN" },
    { 792, "Turkey", "TR" },
    {   0, "Turkmenistan", "TM" },
    {   0, "Turks and Caicos Islands", "TC" },
    {   0, "Tuvalu", "TV" },
    {   0, "Uganda", "UG" },
    {   0, "Ukraine", "UA" },
    { 784, "United Arab Emirates", "AE" },
    { 826, "United Kingdom", "GB" },
    { 840, "United States", "US" },
    { 842, "United States (No DFS)", "Q2" },
    {   0, "United States Minor Outlying Islands", "UM" },
    { 858, "Uruguay", "UY" },
    { 860, "Uzbekistan", "UZ" },
    {   0, "Vanuatu", "VU" },
    { 862, "Venezuela", "VE" },
    { 704, "Viet Nam", "VN" },
    {   0, "Virgin Islands, British", "VG" },
    {   0, "Virgin Islands, U.S.", "VI" },
    {   0, "Wallis and Futuna", "WF" },
    {   0, "West Bank", "0C" },
    {   0, "Western Sahara", "EH" },
    { 887, "Yemen", "YE" },
    {   0, "Zambia", "ZM" },
    { 716, "Zimbabwe", "ZW" },
    {   0, "Country Z2", "Z2" },
    {   0, "Europe / APAC 2005", "XA" },
    {   0, "North and South America and Taiwan", "XB" },
    {   0, "FCC Worldwide", "X0" },
    {   0, "Worldwide APAC", "X1" },
    {   0, "Worldwide RoW 2", "X2" },
    {   0, "ETSI", "X3" },
    {   0, "Worldwide Safe Enhanced Mode Locale", "XS" },
    {   0, "Worldwide Locale for Linux driver", "XW" },
    {   0, "Worldwide Locale", "XX" },
    {   0, "Fake Country Code", "XY" },
    {   0, "Worldwide Locale", "XZ" },
    {   0, "European Locale 0dBi", "XU" },
    {   0, "Worldwide Safe Mode Locale", "XV" },

    {   0, NULL, NULL }
};

bool __WLD_BUG_ON(bool bug, const char* condition _UNUSED, const char* function _UNUSED,
                  const char* file _UNUSED, int line _UNUSED) {
    if(!bug) {
        return false;
    }

    SAH_TRACEZ_ERROR(ME, "WLD_BUG_ON(\"%s\") in function %s() (%s:%d)\n",
                     condition, function, file, line);
    return true;
}


int debugIsVapPointer(void* p) {
    T_AccessPoint* pAP = (T_AccessPoint*) p;
    if(pAP && (pAP->debug == VAP_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID VAP pointer %p", p);
    return 0;
}

int debugIsEpPointer(void* p) {
    T_EndPoint* pEP = (T_EndPoint*) p;
    if(pEP && (pEP->debug == ENDP_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID EndPoint pointer %p", p);
    return 0;
}

int debugIsRadPointer(void* p) {
    T_Radio* pR = (T_Radio*) p;
    if(pR && (pR->debug == RAD_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID RADIO pointer %p", p);
    return 0;
}

int debugIsSsidPointer(void* p) {
    T_SSID* pS = (T_SSID*) p;

    if(pS && (pS->debug == SSID_POINTER)) {
        return 1;
    }
    SAH_TRACEZ_INFO(ME, "Not a VALID SSID pointer %p", p);
    return 0;
}

/**
 * Function returns true when the string (str) can be found in
 * the string array list (strarray).
 *
 * @param str NULL terminated text string
 * @param strarray List of strings, the list must end with a
 *                 NULL pointer..
 * @param index If given we return here the index value of the
 *              hitted string.
 *
 * @return bool true when str is found in strarray else false.
 */
bool findStrInArray(const char* str, const char** strarray, int* index) {
    int i;

    for(i = 0; strarray && strarray[i]; i++) {
        if(!strcmp(str, strarray[i])) {
            if(index) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

int findStrInArrayN(const char* str, const char** strarray, int size, int default_index) {
    int i = 0;
    for(i = 0; i < size && str && strarray && strarray[i]; i++) {
        if(!strcmp(str, strarray[i])) {
            return i;
        }
    }
    return default_index;
}

/**
 * Convert a list of integers into a comma separated list.
 * The comma separated list will go into str.
 */
bool convIntArrToString(char* str, int str_size, const int* int_list, int list_size) {
    int i = 0;
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(int_list, false, ME, "NULL");
    ASSERT_TRUE(str_size > 0, false, ME, "null str size");
    str[0] = 0;
    ASSERTS_TRUE(list_size > 0, true, ME, "null list size");
    for(i = 0; (i < list_size) && ((int) strlen(str) < (str_size - 1)); i++) {
        wldu_catFormat(str, str_size, "%s%i", (i ? "," : ""), int_list[i]);
    }
    ASSERTI_EQUALS(i, list_size, false, ME, "Int array not fully converted to string: str buf too small");
    return true;
}

bool convStrToIntArray(int* int_list, size_t list_size, const char* str, int* outSize) {
    if(outSize != NULL) {
        *outSize = 0;
    }
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(int_list, false, ME, "NULL");
    ASSERT_TRUE(list_size > 0, false, ME, "INVALID");

    size_t i = 0;
    uint32_t size = strlen(str) + 1;
    char buffer[size];
    swl_str_copy(buffer, sizeof(buffer), str);
    buffer[size - 1] = '\0';
    char* needle = buffer;

    do {
        char* sep = strchr(needle, ',');
        if(sep != NULL) {
            *sep = '\0';
        }
        int_list[i] = atoi(needle);
        i++;
        needle = (sep != NULL) ? sep + 1 : NULL;
    } while(needle != NULL && i < list_size);
    if(outSize != NULL) {
        *outSize = i;
    }
    return needle == NULL;
}

bool convStrArrToStr(char* str, int str_size, const char** strList, int list_size) {
    int i = 0;
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_NOT_NULL(strList, false, ME, "NULL");
    ASSERT_TRUE(str_size > 0, false, ME, "null str size");
    str[0] = 0;
    ASSERTS_TRUE(list_size > 0, true, ME, "null list size");
    for(i = 0; (i < list_size) && ((int) strlen(str) < (str_size - 1)); i++) {
        wldu_catFormat(str, str_size, "%s%s", (i ? "," : ""), strList[i]);
    }
    ASSERTI_EQUALS(i, list_size, false, ME, "Int array not fully converted to string: str buf too small");
    return true;
}

/**
 * Return whether a binary mac is in a given list.
 *
 * @arg macAddr : the mac to check, in binary format
 * @arg macList : the list of macs to check against, in binary format.
 * @arg listSize : the number of entries of the macList
 *
 * if an entry of macList is bitwise identical to macAddr, this function returns true.
 * otherwise it returns false.
 */
bool wldu_is_mac_in_list(unsigned char macAddr[ETHER_ADDR_LEN], unsigned char macList[][ETHER_ADDR_LEN], int listSize) {
    int i = 0;
    for(i = 0; i < listSize; i++) {
        if(memcmp(macAddr, macList[i], ETHER_ADDR_LEN) == 0) {
            return true;
        }
    }
    return false;
}


/**
 * Internal function to free all elements in a linked listed. Preferably to be called with the
 * macro llist_freeData
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_freeDataInternal(amxc_llist_t* list, size_t offset) {
    while(!amxc_llist_is_empty(list)) {
        amxc_llist_it_t* it = amxc_llist_take_first(list);
        void* object = (((void*) it) - offset);
        free(object);
    }
}

/**
 * Internal function to destroy all elements in a linked listed, i.e. free them by calling the destroy function
 * Preferably to be called with the macro llist_freeData.
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_freeDataFunInternal(amxc_llist_t* list, size_t offset, void (* destroy)(void* val)) {
    ASSERT_NOT_NULL(destroy, , ME, "NULL");
    amxc_llist_it_t* it;
    while(!amxc_llist_is_empty(list)) {
        it = amxc_llist_take_first(list);
        void* object = (((void*) it) - offset);
        destroy(object);
    }
}


/**
 * Internal function to map a function all elements in a linked listed.
 * Preferably to be called with the macro llist_map.
 * Offset needs to be the offset of iterator in the list object structure
 */
void wldu_llist_mapInternal(amxc_llist_t* list, size_t offset, void (* map)(void* val, void* data), void* data) {
    ASSERT_NOT_NULL(map, , ME, "NULL");
    amxc_llist_for_each(it, list) {
        void* object = (((void*) it) - offset);
        map(object, data);
    }
}

unsigned long conv_ModeBitStr(char** enumStrList, char* str) {
    unsigned long val, i;
    char* pCh = NULL;

    for(i = 0, val = 0; enumStrList[i]; i++) {
        pCh = strstr(str, enumStrList[i]);
        if(pCh) {
            val |= (1 << i);
        }
    }
    return val;
}

long conv_ModeIndexStr(const char** enumStrList, const char* str) {
    ASSERT_NOT_NULL(str, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(enumStrList, WLD_ERROR, ME, "NULL");

    long i = 0;

    for(i = 0; enumStrList[i]; i++) {
        if(!strcmp(str, enumStrList[i])) {
            return i;
        }
    }

    return WLD_ERROR;
}

uint32_t conv_strToMaskSep(const char* str, const char** enumStrList, uint32_t maxVal, char separator) {
    return swl_conv_charToMaskSep(str, enumStrList, maxVal, separator, NULL);
}

/** @deprecated in favor of #swl_conv_charToMask */
uint32_t conv_strToMask(const char* str, const char** enumStrList, uint32_t maxVal) {
    return conv_strToMaskSep(str, enumStrList, maxVal, ',');
}

bool conv_maskToStrSep(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize, char separator) {
    ASSERT_NOT_NULL(enumStrList, false, ME, "NULL");
    ASSERT_NOT_NULL(str, false, ME, "NULL");
    ASSERT_FALSE(strSize == 0, false, ME, "NULL");
    ASSERT_NOT_EQUALS(separator, '\0', false, ME, "Zero separator");
    uint32_t i = 0;
    uint32_t index = 0;
    int printed = 0;
    str[0] = '\0';

    for(i = 0; i < maxVal; i++) {
        if(mask & (1 << i)) {
            if(index) {
                printed = snprintf(&str[index], strSize - index, "%c%s", separator, enumStrList[i]);
            } else {
                printed = snprintf(&str[index], strSize - index, "%s", enumStrList[i]);
            }
            index += printed;

            ASSERT_TRUE(index < strSize, false, ME, "Buff too small");
        }
    }
    return true;
}

bool conv_maskToStr(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize) {
    return conv_maskToStrSep(mask, enumStrList, maxVal, str, strSize, ',');
}

uint32_t conv_strToEnum(const char** enumStrList, const char* str, uint32_t maxVal, uint32_t defaultVal) {
    return swl_conv_charToEnum(str, enumStrList, maxVal, defaultVal);
}


/**
 * @name conv_ModeIndexStrEx
 *
 * @details Function looks for a given string in a string array buffer.
 * The number of items is limited by the string array or the min-maxIndex range.
 *
 *
 * @param str NULL terminated text string
 * @param strarray List of strings, the list must end with a NULL pointer.
 * @param minIndex Starting index number
 * @param maxIndex Ending index number
 *
 * @return int index value that contains the matching string in the strarray ELSE -1!
 */
int conv_ModeIndexStrEx(const char** enumStrList, const char* str, int minIdx, int maxIdx) {
    int i, ret = -1;

    if(str && enumStrList) {
        for(i = minIdx; i <= maxIdx && enumStrList[i]; i++) {
            if(strcmp(enumStrList[i], str)) {
                continue;
            }
            ret = i;
        }
    }
    return ret;
}

/** Try to reduce the use of this function, % and / are
 *  slowing down the process a bit. */
char* itoa(int value, char* result, int base) {
    char aux;
    char* begin, * end;
    char* out;
    unsigned int quotient;

    if((base < 2) || (base > 16)) {
        *result = 0; return result;
    }

    out = result;
    begin = out;

    quotient = ( value < 0 && base == 10) ? (unsigned int) (-value) : (unsigned int) value;

    do {
        *out = "0123456789ABCDEF"[quotient % base];
        ++out;
        quotient /= base;
    } while(quotient);

    // Only apply negative sign for base 10
    if((value < 0) && (base == 10)) {
        *out++ = '-';
    }

    end = out;
    *out = 0;

    /* reverse string */
    end--;
    while(end > begin) {
        aux = *end, *end-- = *begin, *begin++ = aux;
    }

    return result;
}

/**
 * @brief find the lowest bit in an array of longs. (used
 *       in our FSM)
 *
 * @param pval  pointer to an unsigned long array
 * @param elem The amount of longs stored int the array
 *
 * @return int The lowest bit that's set. In case no bit is set
 *         the return is < 0. (-1)
 */
int getLowestBitLongArray(unsigned long* pval, int elem) {
    int ret = 0, i;
    unsigned long sv;

    for(i = 0; pval && i < elem; i++) {
        sv = pval[i];
        if(sv) {
            ret += (i << 5);
            /* Filter out lowest active bit */
            sv &= -sv;
            /* Calculate the bit index */
            if((sv & 0x0000ffff) == 0) {
                ret += 16;
            }
            if((sv & 0x00ff00ff) == 0) {
                ret += 8;
            }
            if((sv & 0x0f0f0f0f) == 0) {
                ret += 4;
            }
            if((sv & 0x33333333) == 0) {
                ret += 2;
            }
            if((sv & 0x55555555) == 0) {
                ret += 1;
            }
            return ret;
        }
    }
    return -1;
}

/**
 * @brief find the highest bit in an array of longs.
 *
 * @param pval  pointer to an unsigned long array
 * @param elem The amount of longs stored int the array
 *
 * @return int The highest bit that's set. In case no bit is set
 *         the return is < 0. (-1)
 */
int getHighestBitLongArray(unsigned long* pval, int elem) {
    int ret, i;
    unsigned long val, sv;

    for(i = (elem - 1); pval && i >= 0; i--) {
        sv = pval[i];
        if(sv) {
            ret = (i << 5);
            val = sv >> 16; if(val) {
                sv = val; ret += 16;
            }
            val = sv >> 8;  if(val) {
                sv = val; ret += 8;
            }
            val = sv >> 4;  if(val) {
                sv = val; ret += 4;
            }
            val = sv >> 2;  if(val) {
                sv = val; ret += 2;
            }
            val = sv >> 1;  if(val) {
                ret += 1;
            }
            return ret;
        }
    }
    return -1;
}

int areBitsSetLongArray(unsigned long* pval, int elem) {
    int i;
    unsigned long sv;

    for(i = 0, sv = 0; i < elem && pval; i++) {
        sv |= pval[i];
    }
    return (sv) ? 1 : 0;
}

int clearAllBitsLongArray(unsigned long* pval, int elem) {
    int i;

    for(i = 0; i < elem && pval; i++) {
        pval[i] = 0;
    }
    return 0;
}

/**
 * Returns 1 if the given bit number, and only the given bitnr is set in the long array.
 * Returns 0 otherwise.
 */
int isBitOnlySetLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long test = 0;
    for(i = 0; i < elem && pval; i++) {

        if((bitNr < 32) && (bitNr >= 0)) {
            test = 1 << bitNr;
        } else {
            test = 0;
        }

        if(pval[i] != test) {
            return 0;
        }
        bitNr -= 32;
    }
    return 1;
}

int areBitsOnlySetLongArray(unsigned long* pval, int elem, int* bitNrArray, int nrBitElem) {
    int i = 0;
    int j = 0;
    unsigned long mask;

    for(i = 0; i < elem && pval; i++) {
        mask = 0xffffffff;

        for(j = 0; j < nrBitElem; j++) {
            int bitNr = bitNrArray[j] - (i * 32);
            if((bitNr & ~0x1f) == 0) {
                mask &= ~(1 << bitNr);
            }
        }

        if((pval[i] & mask) > 0) {
            return 0;
        }
    }
    return 1;
}

int isBitSetLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;

    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }
    if((i >= elem) || !pval) {
        return 0;
    }
    sv = 1 << (bitNr & 0x1f);
    return (pval[i] & sv) ? 1 : 0;
}

int setBitLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }
    if((i >= elem) || !pval) {
        return 0;
    }
    sv = 1 << (bitNr & 0x1f);
    pval[i] |= sv;

    return 1;
}

int clearBitLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        bitNr -= 0x20;
        i++;
    }

    if((bitNr < 0) || (i >= elem) || !pval) {
        return 0;
    }

    sv = 1 << (bitNr & 0x1f);
    pval[i] &= ~sv;

    return 1;
}

int markAllBitsLongArray(unsigned long* pval, int elem, int bitNr) {
    int i;
    unsigned long sv;

    i = 0;
    while(bitNr > 0x1f) {
        pval[i] = 0xffffffff;
        i++;
        bitNr -= 0x20;
    }
    if((bitNr < 0) || (i >= elem) || !pval) {
        return 0;
    }

    sv = (1 << bitNr) - 1;
    pval[i] = sv;

    return 1;
}

/*
   Active Commit FSM Bits,
   Shield ongoing vendor commit activities from new/inbetween configuration changes.
 */
void moveFSM_VAPBit2ACFSM(T_AccessPoint* pAP) {
    int i;

    for(i = 0; pAP && i < FSM_BW && pAP->fsm.FSM_BitActionArray[i]; i++) {
        pAP->fsm.FSM_AC_BitActionArray[i] = pAP->fsm.FSM_BitActionArray[i];
        pAP->fsm.FSM_BitActionArray[i] = 0;
    }
}

void moveFSM_RADBit2ACFSM(T_Radio* pR) {
    int i;

    for(i = 0; pR && i < FSM_BW && pR->fsmRad.FSM_BitActionArray[i]; i++) {
        pR->fsmRad.FSM_AC_BitActionArray[i] = pR->fsmRad.FSM_BitActionArray[i];
        pR->fsmRad.FSM_BitActionArray[i] = 0;
    }
}

void moveFSM2ACFSM(T_FSM* fsm) {
    int i;

    for(i = 0; fsm && i < FSM_BW && fsm->FSM_BitActionArray[i]; i++) {
        fsm->FSM_AC_BitActionArray[i] = fsm->FSM_BitActionArray[i];
        fsm->FSM_BitActionArray[i] = 0;
    }
}

void longArrayCopy(unsigned long* to, unsigned long* from, int len) {
    int i;
    if(!to || !from) {
        return;
    }

    for(i = 0; i < len; i++) {
        to[i] = from[i];
    }
}

void longArrayBitOr(unsigned long* to, unsigned long* from, int len) {
    int i;
    if(!to || !from) {
        return;
    }

    for(i = 0; i < len; i++) {
        to[i] |= from[i];
    }
}

void longArrayClean(unsigned long* array, int len) {
    int i;
    if(!array) {
        return;
    }

    for(i = 0; i < len; i++) {
        array[i] = 0;
    }
}

/**
 * @brief isModeWEP
 *
 * Check if given mode is one of the WEP modes.
 *
 * @param mode The security mode.
 * @return true if the mode is a WEP mode, false otherwise.
 */
bool isModeWEP(wld_securityMode_e mode) {
    if((mode == APMSI_WEP64) || (mode == APMSI_WEP128) || (mode == APMSI_WEP128IV)) {
        return true;
    }

    return false;
}

/**
 * @brief isModeWPAPersonal
 *
 * Check if given mode is one of the WPA personal modes.
 *
 * @param mode The security mode.
 * @return true if the mode is a WPA personal mode, false otherwise.
 */
bool isModeWPAPersonal(wld_securityMode_e mode) {
    if((mode == APMSI_WPA_P) || (mode == APMSI_WPA2_P) || (mode == APMSI_WPA_WPA2_P)
       || (mode == APMSI_WPA3_P) || (mode == APMSI_WPA2_WPA3_P)) {
        return true;
    }

    return false;
}

bool isModeWPA3Personal(wld_securityMode_e mode) {
    if((mode == APMSI_WPA3_P) || (mode == APMSI_WPA2_WPA3_P)) {
        return true;
    }

    return false;
}

/**
 * @details isValidWEPKey check if given string is a valid WEP
 *          key. We support 5/13/16 ASCII WEP key or 10/26/32
 *          Hex WEP key
 *
 * @param key NULL terminated WEP key string.
 *
 * @return due to legacy reasons, this returns the WEP standard for which this
 * key is suited. Possible return values are
 * * APMSI_WEP64
 * * APMSI_WEP128
 * * APMSI_WEP128IV
 */
wld_securityMode_e isValidWEPKey (const char* key) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    int len = strlen(key);
    int i;

    if((len == 10) || (len == 26) || (len == 32)) {
        /* Expect a HEX string */
        for(i = 0; i < len; i++) {
            if(!(isxDigit(key[i]))) {
                return 0;
            }
        }
        return (len == 10) ? (APMSI_WEP64) : ((len == 26) ? APMSI_WEP128 : APMSI_WEP128IV);
    } else {
        /* Expect an ASCII string of fix lenght */
        if((len == 5) || (len == 13) || (len == 16)) {
            return (len == 5) ? (APMSI_WEP64) : ((len == 13) ? APMSI_WEP128 : APMSI_WEP128IV);
        }
    }
    /* All other stuff... */
    return APMSI_UNKNOWN;
}

/**
 * Function converts the binary WEP key to an readable ASCII
 * string.
 */
char* convASCII_WEPKey(const char* key, char* out, int sizeout) {
    int len, i;
    unsigned char ch;

    if(key) {
        len = strlen(key);
        if(out &&
           (( len == 5) || ( len == 13) || ( len == 16)) &&
           (sizeout > (len * 2))) {
            for(i = 0; i < len; i++) {
                ch = key[i];
                out[(i << 1) + 0] = "0123456789ABCDEF"[((ch & 0xf0) >> 4)];
                out[(i << 1) + 1] = "0123456789ABCDEF"[((ch & 0x0f) >> 0)];
            }
            out[(i << 1)] = '\0';
            return out;
        }
        // All other 10/26/32 can be used directly!
        if(((len == 10) || (len == 26) || (len == 32))) {
            return (char*) key;
        }
    }
    return NULL;
}

/**
 * Convets the readable ASCII string to a binary WEP key
 */
char* convHex_WEPKey(const char* key, char* out, int sizeout) {
    int len, i;
    unsigned char* ch;

    if(key) {
        len = strlen(key);
        if((len == 5) || (len == 13) || (len == 16)) {
            return (char*) key; // Current given key is already valid.
        }

        if(out &&
           (((len == 10) && (sizeout >= 5)) ||
            ((len == 26) && (sizeout >= 13)) ||
            ((len == 32) && (sizeout >= 16)))) {
            ch = (unsigned char*) key;
            for(i = 0; i < len; i++, ch++) {
                if(i & 1) {
                    out[i >> 1] <<= 4;
                } else {
                    out[i >> 1] = 0;
                }

                if((*ch >= '0') && (*ch <= '9')) {
                    out[i >> 1] += *ch - '0';
                } else if(( *ch >= 'a') && ( *ch <= 'f')) {
                    out[i >> 1] += 10 + *ch - 'a';
                } else if(( *ch >= 'A') && ( *ch <= 'F')) {
                    out[i >> 1] += 10 + *ch - 'A';
                }
            }
            return out;
        }
    }
    return NULL;
}

/**
 * Check for a correct PSK key.
 */
bool isValidPSKKey(const char* key) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    int len = strlen(key);
    int i;
    char* CapKey = NULL;

    if(len == 64) {           // Checked, must be 64 bytes!
        CapKey = (char*) key; // fix error: assignment of read-only location
        for(i = 0; i < len; i++) {
            if(!(isxDigit(key[i]))) {
                return 0;
            }
            CapKey[i] = toupper(key[i]);    // Convert direct to upper chars
        }
        return true;
    }
    return false;
}

/**
 * Check for \n and \r in KeyPassPhrase.
 */
bool isSanitized (const char* pname) {
    ASSERTS_NOT_NULL(pname, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Checking KeyPassPhrase validity %s", pname);
    int i = 0;
    while(pname[i]) {
        if(pname[i] < 32) {
            return false;
        }
        i++;
    }
    return true;
}

/**
 * Check for a correct AES key.
 */
bool isValidAESKey(const char* key, int maxLen) {
    ASSERTS_NOT_NULL(key, false, ME, "NULL");
    bool sanitized = isSanitized(key);
    int len = strlen(key);
    if(!sanitized) {
        return false;
    }
    if(len < 8) {
        return false;
    }
    if(len > maxLen) {
        return false;
    }
    // Ignore null character
    if(len == maxLen) {
        int i = 0;
        for(i = 0; i < len; i++) {
            if(!isxDigit(key[i])) {
                return false;
            }
        }
    }
    return true;
}


/**
 * Check for a correct SSID name.
 */
bool isValidSSID(const char* ssid) {
    // ssid may not be empty!
    ASSERTI_STR(ssid, false, ME, "empty");

    // Smaller then 33 chars?
    int i = strlen(ssid);
    return (i <= 32);
}

/**
 * @name isListOfChannels_US
 *
 * @details This function returns TRUE when US channels are covered for the selected band.
 *
 * @param possibleChannels, array of channels must terminated by a 0 channel
 * @param len, number of elements in possibleChannels
 * @param band, which band must be checked for US mode?
 *
 * @return int
 */
int isListOfChannels_US(unsigned char* possibleChannels, int len, int band) {
    int i, US, EU;

    US = 0;
    EU = 0;

    if(!(possibleChannels && len)) {
        return -1;
    }

    switch(band) {
    case SWL_FREQ_BAND_2_4GHZ:
        for(i = 0; i < len && possibleChannels[i]; i++) {
            US += (possibleChannels[i] <= 11) ? 1 : 0;
            EU += (possibleChannels[i] > 11 && possibleChannels[i] <= 14) ? 1 : 0;
        }
        // For US must contain a valid 2.4 Channel but not one from EU!
        return (US && !EU);

    case SWL_FREQ_BAND_5GHZ:
        for(i = 0; i < len && possibleChannels[i]; i++) {
            EU += ( possibleChannels[i] <= 36) ? 1 : 0;
            US += ( possibleChannels[i] >= 144) ? 1 : 0;
        }
        // For US has more 5GHz channel then EU!
        return (US && EU);
    case SWL_FREQ_BAND_6GHZ:
        break;
    default:
        break;
    }
    return -1; // We don't know
}

/* The following function have been borrowed and adapted from hostapd*/
int get_random(unsigned char* buf, size_t len) {
    FILE* f;
    size_t rc;

    f = fopen("/dev/urandom", "rb");
    if(f == NULL) {
        SAH_TRACEZ_ERROR(ME, "Cannot open /dev/urandom.");
        return -1;
    }

    rc = fread(buf, 1, len, f);
    fclose(f);

    return rc != len ? -1 : 0;
}

/**
 * @name get_randomstr
 *
 * @details Generates a random NULL terminated string in the buffer!
 *
 * @param buf Destination pointer of string, must at least cover len+1 size! (NULL char)
 * @param len Length of generated key!
 *
 * @return int returns length of string.
 */
int get_randomstr(unsigned char* buf, size_t len) {
    const char RND_ASCII[64] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    unsigned int i = 0;

    get_random(buf, len);
    for(i = 0; i < len; i++) {
        buf[i] = RND_ASCII[ (buf[i] & 0x3f) ];
    }
    buf[i] = '\0';

    return i;
}

/**
 * @name get_randomhexstr
 *
 * @details Generates a random hexadecimal NULL terminated string in the buffer!
 *
 * @param buf Destination pointer of string, must at least cover len+1 size! (NULL char)
 * @param len Length of generated key!
 *
 * @return int returns length of string.
 */
int get_randomhexstr(unsigned char* buf, size_t len) {
    const char RND_HEX[16] = "0123456789ABCDEF";
    unsigned int i = 0;

    get_random(buf, len);
    for(i = 0; i < len; i++) {
        buf[i] = RND_HEX[ (buf[i] & 0xf) ];
    }
    buf[i] = '\0';

    return i;
}

unsigned long wpsInitPin() {
    unsigned long pin = 0;

    if(0 != get_random((unsigned char*) &pin, sizeof(unsigned long))) {
        SAH_TRACEZ_ERROR(ME, "Can't get a random number from urandom, using rand and srand instead");
        srand(time(0));     /* get random numbers */
        pin = rand();
    }
#if WPS_PIN_LEN == 8
    pin %= 100000000;
#else
    pin %= 10000;
#endif
    return pin;
}

#if WPS_PIN_LEN == 8 //WPS8PIN
/* pin_gen -- generate an 8 character PIN number.
 */
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]) {
    unsigned long pin;
    unsigned char checksum;
    unsigned long acc = 0;
    unsigned long tmp;

    SAH_TRACEZ_IN(ME);

    pin = wpsInitPin();
    SAH_TRACEZ_INFO(ME, "pin from wpsInitPin: %lu", pin);

    /* use random numbers between [10000000, 99999990)
     * so that first digit is never a zero, which could be confusing.
     */
    pin = 1000000 + pin % 9000000;
    tmp = pin * 10;

    acc += 3 * ((tmp / 10000000) % 10);
    acc += 1 * ((tmp / 1000000) % 10);
    acc += 3 * ((tmp / 100000) % 10);
    acc += 1 * ((tmp / 10000) % 10);
    acc += 3 * ((tmp / 1000) % 10);
    acc += 1 * ((tmp / 100) % 10);
    acc += 3 * ((tmp / 10) % 10);

    checksum = (unsigned char) (10 - (acc % 10)) % 10;
    SAH_TRACEZ_INFO(ME, "checksum=: %d", checksum);
    snprintf(pwd, WPS_PIN_LEN + 1, "%lu", pin * 10 + checksum);
    SAH_TRACEZ_INFO(ME, "pin after checksum: %lu, %s", pin * 10 + checksum, pwd);
    SAH_TRACEZ_OUT(ME);
}
#else
/* WPS_PIN_LEN == 4 */
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]) {
    unsigned long pin;
    unsigned char checksum;
    unsigned long acc = 0;
    unsigned long tmp;

    SAH_TRACEZ_IN(ME);

    pin = wpsInitPin();
    SAH_TRACEZ_INFO(ME, "pin from wpsInitPin: %lu", pin);

    /* use random numbers between [1000, 9990)
     * so that first digit is never a zero, which could be confusing.
     */
    pin = 100 + pin % 900;
    tmp = pin * 10;

    acc += 3 * ((tmp / 1000) % 10);
    acc += 1 * ((tmp / 100) % 10);
    acc += 3 * ((tmp / 10) % 10);

    checksum = (unsigned char) (10 - (acc % 10)) % 10;
    snprintf(pwd, WPS_PIN_LEN + 1, "%lu", pin * 10 + checksum);
    SAH_TRACEZ_INFO(ME, "pin after checksum: %lu, %s", pin * 10 + checksum, pwd);
    SAH_TRACEZ_OUT(ME);
}
#endif

int wpsPinValid(unsigned long PIN) {
    unsigned long int accum = 0;

    // 4-Digit PIN [0000..9999]
    if(PIN <= 9999) {
        return TRUE;
    }
    // 8-Digit PIN
    accum += 3 * ((PIN / 10000000) % 10);
    accum += 1 * ((PIN / 1000000) % 10);
    accum += 3 * ((PIN / 100000) % 10);
    accum += 1 * ((PIN / 10000) % 10);
    accum += 3 * ((PIN / 1000) % 10);
    accum += 1 * ((PIN / 100) % 10);
    accum += 3 * ((PIN / 10) % 10);
    accum += 1 * ((PIN / 1) % 10);

    return (0 == (accum % 10));
}

/*
 * @brief checks if provided pin string has a valid numerical value
 * Cf. usp tr181 v2.15.0:
 * Device PIN used for PIN based pairing between WPS peers.
 * This PIN is either a four digit number or an eight digit number.
 * @param pinStr PIN string
 * @return true when PIN string matches validity criteria (i.e number with 4 or 8 digits)
 *         false otherwise
 */
bool wldu_checkWpsPinStr(const char* pinStr) {
    ASSERT_STR(pinStr, false, ME, "pin empty");
    uint32_t pinNum = 0;
    swl_rc_ne ret = wldu_convStrToNum(pinStr, &pinNum, sizeof(pinNum), 0, false);
    ASSERTI_EQUALS(ret, SWL_RC_OK, false, ME, "%s is not a number", pinStr);
    if((swl_str_len(pinStr) != 4) && (swl_str_len(pinStr) != 8)) {
        SAH_TRACEZ_ERROR(ME, "%s has not required pin length (4 or 8 digits)", pinStr);
        return false;
    }
    ASSERT_TRUE(wpsPinValid(pinNum), false, ME, "%s has invalid pin value", pinStr);
    return true;
}

/**
 * Get the uint8 list from a  comma separated string list.
 * @arg srcStrList:  comma separated list of channels
 * returns number of elements obtained from the list
 */
int32_t wldu_convStrToUInt8Arr(uint8_t* tgtList, uint tgtListSize, char* srcStrList) {
    ASSERT_NOT_NULL(srcStrList, -1, ME, "NULL");
    ASSERT_NOT_NULL(tgtList, -1, ME, "NULL");
    ASSERT_TRUE(tgtListSize > 0, -1, ME, "null ChanListSize");
    char tmp[32] = {0};
    unsigned int i = 0;
    int len = 0;

    do {
        if(i >= tgtListSize) {
            SAH_TRACEZ_ERROR(ME, "Source Str List is too large %d/%d", i, tgtListSize);
            return -1;
        }
        len = strcspn(srcStrList, ",");
        snprintf(tmp, sizeof(tmp), "%.*s", (int) len, srcStrList);
        tgtList[i] = atoi(tmp);
        i++;
        srcStrList += len;
    } while(*srcStrList++);

    return i;
}

int32_t wldu_convStr2Mac(unsigned char* mac, uint32_t sizeofmac, const char* str, uint32_t sizeofstr) {
    ASSERT_NOT_NULL(mac, 0, ME, "NULL");
    ASSERT_NOT_NULL(str, 0, ME, "NULL");
    ASSERT_TRUE(sizeofstr > 0, 0, ME, "null sizeofstr");
    unsigned int MAC[8];
    unsigned char CMAC[8];

    SAH_TRACEZ_INFO(ME, "%p %d - %p %d", mac, sizeofmac, str, sizeofstr);

    if(sizeofstr > 16) { /* 12:45:78:ab:de:01 == 17 !*/
        SAH_TRACEZ_INFO(ME, "%s", str);
        if(sscanf(str, "%x:%x:%x:%x:%x:%x",
                  &MAC[0], &MAC[1], &MAC[2],
                  &MAC[3], &MAC[4], &MAC[5]) == 6) {
            CMAC[0] = MAC[0];
            CMAC[1] = MAC[1];
            CMAC[2] = MAC[2];
            CMAC[3] = MAC[3];
            CMAC[4] = MAC[4];
            CMAC[5] = MAC[5];
        } else {
            // No 'str' data was given
            return 0;
        }
    } else {
        if(sizeofstr >= 6) {
            CMAC[0] = str[0];
            CMAC[1] = str[1];
            CMAC[2] = str[2];
            CMAC[3] = str[3];
            CMAC[4] = str[4];
            CMAC[5] = str[5];
        } else {
            // No 'str' data was given
            return 0;
        }
    }
    SAH_TRACEZ_INFO(ME, "%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x",
                    CMAC[0], CMAC[1], CMAC[2], CMAC[3], CMAC[4], CMAC[5]);
    if(sizeofmac >= 6) {
        memcpy(mac, CMAC, 6);
        return 1;
    }

    return 0;
}

/* This function is deprecated. Please use wldu_convStr2Mac now */
int convStr2Mac(unsigned char* mac, unsigned int sizeofmac, const unsigned char* str, unsigned int sizeofstr) {
    return wldu_convStr2Mac(mac, sizeofmac, (char*) str, sizeofstr);
}

bool wldu_convMac2Str(unsigned char* mac, uint32_t sizeofmac, char* str, uint32_t sizeofstr) {
    return swl_hex_fromBytesSep(str, sizeofstr, mac, sizeofmac, false, ':', NULL);
}

/* This function is deprecated. Please use wldu_convMac2Str now */
int convMac2Str(unsigned char* mac, unsigned int sizeofmac, unsigned char* str, unsigned int sizeofstr) {
    return wldu_convMac2Str(mac, sizeofmac, (char*) str, sizeofstr);
}

/** @deprecated Use `swl_hex.h` */
int convHex2Str(const unsigned char* hex, size_t maxhexlen, unsigned char* str, size_t maxstrlen, int upper) {
    unsigned int i, ch;
    char* hexChar = (upper) ? "0123456789ABCDEF" : "0123456789abcdef";

    if(maxstrlen - 1 < maxhexlen * 2) {
        SAH_TRACEZ_ERROR(ME, "Not enough str storage for convert req octets (Hex %d, Str %d, need %d)",
                         (int) maxhexlen,
                         (int) maxstrlen,
                         (int) (maxhexlen * 2) + 1);
        return 0;
    }

    for(i = 0; i < maxhexlen; i++) {
        ch = hex[i];
        str[2 * i + 0] = hexChar[ch / 16];
        str[2 * i + 1] = hexChar[ch % 16];
    }

    str[i * 2] = '\0';

    return (2 * i + 1);
}

static int s_convStrToInt8(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > CHAR_MAX), WLD_ERROR, ME, "num str(%s): CHAR range overflow", numSrcStr);
    ASSERT_FALSE((res < CHAR_MIN), WLD_ERROR, ME, "num str(%s): CHAR range underflow", numSrcStr);
    *((int8_t*) numDstBuf) = (int8_t) res;
    return WLD_OK;
}

static int s_convStrToUInt8(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > UCHAR_MAX), WLD_ERROR, ME, "num str(%s): UCHAR range overflow", numSrcStr);
    *((uint8_t*) numDstBuf) = (uint8_t) res;
    return WLD_OK;
}

static int s_convStrToInt16(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > SHRT_MAX), WLD_ERROR, ME, "num str(%s): SHORT range overflow", numSrcStr);
    ASSERT_FALSE((res < SHRT_MIN), WLD_ERROR, ME, "num str(%s): SHORT range underflow", numSrcStr);
    *((int16_t*) numDstBuf) = (int16_t) res;
    return WLD_OK;
}

static int s_convStrToUInt16(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res > USHRT_MAX), WLD_ERROR, ME, "num str(%s): USHORT range overflow", numSrcStr);
    *((uint16_t*) numDstBuf) = (uint16_t) res;
    return WLD_OK;
}

static int s_convStrToInt32(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long int res = strtol(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == LONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LONG range overflow", numSrcStr);
    ASSERT_FALSE((res == LONG_MIN) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LONG range underflow", numSrcStr);
    *((int32_t*) numDstBuf) = (int32_t) res;
    return WLD_OK;
}

static int s_convStrToUInt32(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long int res = strtoul(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == ULONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): ULONG range overflow", numSrcStr);
    *((uint32_t*) numDstBuf) = (uint32_t) res;
    return WLD_OK;
}

static int s_convStrToInt64(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    long long int res = strtoll(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == LLONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LLONG range overflow", numSrcStr);
    ASSERT_FALSE((res == LLONG_MIN) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): LLONG range underflow", numSrcStr);
    *((int64_t*) numDstBuf) = (int64_t) res;
    return WLD_OK;
}

static int s_convStrToUInt64(const char* numSrcStr, void* numDstBuf, uint8_t base) {
    char* endptr = NULL;
    errno = 0;
    unsigned long long int res = (uint64_t) strtoull(numSrcStr, &endptr, base);
    ASSERT_FALSE((endptr && endptr[0]), WLD_ERROR, ME, "num str(%s) (Base:%d) is invalid", numSrcStr, base);
    ASSERT_FALSE((res == ULLONG_MAX) && (errno == ERANGE), WLD_ERROR, ME, "num str(%s): ULLONG range overflow", numSrcStr);
    *((uint64_t*) numDstBuf) = (uint64_t) res;
    return WLD_OK;
}

typedef int (* func_convStrToNum_t)(const char* numSrcStr, void* numDstBuf, uint8_t base);
typedef struct {
    uint8_t byteSize;
    bool isSigned;
    func_convStrToNum_t conv;
} convStrToNum_info_t;
static const convStrToNum_info_t convStrToNumInfo[] = {
    {sizeof(int8_t), true, s_convStrToInt8, },
    {sizeof(uint8_t), false, s_convStrToUInt8, },
    {sizeof(int16_t), true, s_convStrToInt16, },
    {sizeof(uint16_t), false, s_convStrToUInt16, },
    {sizeof(int32_t), true, s_convStrToInt32, },
    {sizeof(uint32_t), false, s_convStrToUInt32, },
    {sizeof(int64_t), true, s_convStrToInt64, },
    {sizeof(uint64_t), false, s_convStrToUInt64, },
};

int wldu_convStrToNum(const char* numSrcStr, void* numDstBuf, uint8_t numDstByteSize, uint8_t base, bool isSigned) {
    ASSERT_NOT_NULL(numSrcStr, WLD_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(numDstBuf, WLD_ERROR, ME, "NULL");
    uint32_t i;
    for(i = 0; i < WLD_ARRAY_SIZE(convStrToNumInfo); i++) {
        if((convStrToNumInfo[i].byteSize == numDstByteSize) && (convStrToNumInfo[i].isSigned == isSigned)) {
            return convStrToNumInfo[i].conv(numSrcStr, numDstBuf, base);
        }
    }
    SAH_TRACEZ_ERROR(ME, "unsupp num byte size (%d)", numDstByteSize);
    return WLD_ERROR;
}

static int s_parseFullHexStrToNum(const char* src, void* numDstBuf, uint8_t numDstNBytes) {
    ASSERT_NOT_NULL(src, WLD_ERROR, ME, "NULL");
    uint8_t base = 16;
    if(strncasecmp(src, "0x", 2) == 0) {
        src += 2;
    }
    uint8_t hexlen = numDstNBytes * 2;
    ASSERTS_FALSE((strlen(src) < hexlen), WLD_OK, ME, "not a full size hex num str (%d < %d digits)", strlen(src), hexlen);
    char buf[hexlen + 1];
    wldu_copyStr(buf, src, sizeof(buf)); \
    return wldu_convStrToNum(buf, numDstBuf, numDstNBytes, base, false);
}

/** @deprecated Use `swl_hex.h` */
uint8_t wldu_parsehexToUint8(const char* src) {
    ASSERT_NOT_NULL(src, 0, ME, "NULL");
    uint8_t res = 0;
    s_parseFullHexStrToNum(src, &res, sizeof(res));
    return res;
}

/** @deprecated Use `swl_hex.h` */
uint16_t wldu_parseHexToUint16(const char* src) {
    ASSERT_NOT_NULL(src, 0, ME, "NULL");
    uint16_t res = 0;
    s_parseFullHexStrToNum(src, &res, sizeof(res));
    return res;
}

/** @deprecated Use `swl_hex.h` */
uint32_t wldu_parseHexToUint32(const char* src) {
    ASSERT_NOT_NULL(src, 0, ME, "NULL");
    uint32_t res = 0;
    s_parseFullHexStrToNum(src, &res, sizeof(res));
    return res;
}

uint64_t wldu_parseHexToUint64(const char* src) {
    ASSERT_NOT_NULL(src, 0, ME, "NULL");
    uint64_t res = 0;
    s_parseFullHexStrToNum(src, &res, sizeof(res));
    return res;
}

/** @deprecated Use `swl_hex.h` instead. */
int convStr2Hex(const char* str, size_t maxStrlen, char* hex, size_t maxHexLen) {
    size_t indexSrc = 0;
    size_t indexTgt = 0;
    while(indexSrc < (maxStrlen - 1) && indexTgt < maxHexLen) {
        hex[indexTgt] = (char) wldu_parsehexToUint8(&str[indexSrc]);
        indexSrc += 2;
        indexTgt += 1;
    }
    return indexTgt;
}

/**
 * This function converts a mac address to UPPER case ascii
 */
int mac2str(char* str, const unsigned char* mac, size_t maxstrlen) {
    int len;
    len = snprintf((char*) str, maxstrlen, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if(len < 0) {
        return 0;
    } else if(len <= (signed) maxstrlen) {
        return 1;
    } else {
        return 0;
    }
}

static bool ssid_is_printable(const uint8_t* ssid, uint8_t len) {
    for(uint8_t i = 0; i < len; i++) {
        if(!isprint(ssid[i])) {
            return false;
        }
    }

    return true;
}

/* Convert an SSID (up to 32 bytes, some of which might be 0, not guaranteed to
 * be 0 terminated). into a printable string we can present to the user.
 *
 * If (as is almost always the case) the SSID consists exclusively of printable
 * ASCII characters we copy those. If there are unprintable characters we
 * convert the SSID to a hex string.
 */
char* wld_ssid_to_string(const uint8_t* ssid, uint8_t len) {
    if(WLD_BUG_ON(!ssid)) {
        return NULL;
    }

    /* If the SSID is ASCII printable we just copy that data, if not convert it
     * to a HEX string */
    if(ssid_is_printable(ssid, len)) {
        char* str = calloc(1, len + 1);
        memcpy(str, ssid, len);
        return str;
    }

    char* str = calloc(1, (len * 2) + 1);
    ASSERTS_NOT_NULL(str, "", ME, "NULL");
    for(uint8_t i = 0; i < len; i++) {
        sprintf(str + (i * 2), "%02x", ssid[i]);
    }

    return str;
}

/**
 * @brief Convert the numeric SSID to a readable string
 *
 * This is a direct access function : nothing is allocated
 *
 * @param ssid numeric representation of the ssid
 * @param ssid_len length of the ssid
 * @param str string formatted output of the ssid
 * @return 0 on success; -1 on failure
 */
int convSsid2Str(const uint8_t* ssid, uint8_t ssid_len, char* str) {
    ASSERT_NOT_NULL(ssid, -1, ME, "NULL");

    /* If the SSID is ASCII printable we just copy that data, if not convert it
     * to a HEX string */
    if(ssid_is_printable(ssid, ssid_len)) {
        memcpy(str, ssid, ssid_len);
        return 0;
    }

    for(uint8_t i = 0; i < ssid_len; i++) {
        sprintf(str + (i * 2), "%02x", ssid[i]);
    }

    return 0;
}


/**
 * This function converts the Quality value to a Signal Strength value
 * @date (30/06/2016)
 *
 * @param quality the quality value represented in %
 * @return int the RSSI value represented in dBm
 */
int convQuality2Signal(int quality) {
    int dBm = 0;
    if(quality <= 0) {
        dBm = -100;
    } else if(quality >= 100) {
        dBm = -50;
    } else {
        dBm = (quality / 2) - 100;
    }
    return dBm;
}

/**
 * This function converts the Signal Strength value to a Quality value
 * @date (25/06/2016)
 *
 * @param dBm the RSSI value represented in dBm
 * @return int the quality value represented in %
 */
int convSignal2Quality(int dBm) {
    int quality;
    if(dBm <= -100) {
        quality = 0;
    } else if(dBm >= -50) {
        quality = 100;
    } else {
        quality = 2 * (dBm + 100);
    }
    return quality;
}

/**
 * This function replies on a delayed function callback.
 */
void fsm_delay_reply(uint64_t call_id, amxd_status_t state, int* errval) {
    SAH_TRACEZ_IN(ME);
    amxc_var_t returnValue;
    amxc_var_init(&returnValue);
    amxc_var_set_int32_t(&returnValue, (errval) ? *errval : 0);
    amxd_function_deferred_done(call_id, state, NULL, &returnValue);
    amxc_var_clean(&returnValue);
    SAH_TRACEZ_OUT(ME);
}

int getCountryParam(const char* instr, int CC, int* idx) {
    int i;

    // We're only intrested in the idx value!
    for(i = 0; !instr && CC && Rad_CountryCode[i].ShortCountryName; i++) {
        if(CC == Rad_CountryCode[i].CountryCode) {
            break;
        }
    }
    // In case we've the short name of the country code...
    for(i = 0; instr && !CC && Rad_CountryCode[i].ShortCountryName; i++) {
        if(!strcmp(Rad_CountryCode[i].ShortCountryName, instr)) {
            break;
        }
    }

    if(Rad_CountryCode[i].ShortCountryName) {
        if(idx) {
            *idx = i;
        }
        if(instr) {
            return Rad_CountryCode[i].CountryCode;
        } else {
            return CC;
        }
    }
    // We're not supporting this country...
    return -1;
}

/**
 * The logic is a bit contra... but we must be able to use it
 * for all supported vendor wifi cards (ATH, BCM,...). The
 * common part for all is the Rad_CountryCode table index value!
 */
char* getFullCountryName(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].FullCountryName;
    }
    return NULL;
}

char* getShortCountryName(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].ShortCountryName;
    }
    return NULL;
}

int getCountryCode(int idx) {
    if((idx >= 0) &&
       ( idx < ((int) (sizeof(Rad_CountryCode) / sizeof(T_COUNTRYCODE))))) {
        return Rad_CountryCode[idx].CountryCode;
    }
    return 0;
}

/**
 * @deprecated use #swl_str_copy
 *
 * @brief A safe version to copy a string to a destination buffer
 *
 * @deprecated Use `swl_string.h` instead.
 *
 * The downside of strncpy is that is will not terminate the string,
 * when the size of the src is larger then destination, we end up with a buffer that is not null terminated,
 * i.e not a valid string
 * This version will do strncpy and write a terminating NULL character at the end
 *
 * @param dest destination string buffer
 * @param src source string
 * @param destsize size of the destination buffer
 * @return char* destination buffer
 */
char* wldu_copyStr(char* dest, const char* src, size_t destsize) {
    swl_str_copy(dest, destsize, src);
    return dest;
}

/**
 * @brief A safe version to concatenate two strings
 *
 * @deprecated Use `swl_string.h` instead.
 *
 * The downside of strncat is that is will not terminate the string,
 * when the size of the src is larger then destination, we end up with a buffer that is not null terminated,
 * i.e not a valid string
 * This version will append the source string to its destination,
 * by calling wldu_copyStr at the end of the destination string
 *
 * @param dest destination string buffer
 * @param src source string to append
 * @param destsize size of the destination buffer
 * @return char* destination string buffer
 */
char* wldu_catStr(char* dest, const char* src, size_t destsize) {
    if(!dest || !destsize) {
        return dest;
    }

    dest[destsize - 1] = '\0';
    int curDestLen = strlen(dest);

    wldu_copyStr(&dest[curDestLen], src, (destsize - curDestLen));
    return dest;
}

/**
 * @brief A safe version to concatenate formatted strings.
 *
 * @deprecated Use `swl_string.h` instead.
 *
 * @param dest destination buffer
 * @param destsize size of destination buffer
 * @param format output string format
 * @return char* destination string buffer
 */
char* wldu_catFormat(char* dest, size_t destsize, const char* format, ...) {
    va_list args;
    int ret = -1;
    ASSERT_NOT_NULL(dest, dest, ME, "NULL");
    ASSERT_NOT_NULL(format, dest, ME, "NULL");
    ASSERT_TRUE(destsize > 0, dest, ME, "null destsize");
    uint32_t curDestLen = strlen(dest);
    ASSERT_TRUE(destsize > curDestLen, dest, ME, "dest already full or not initialized");

    va_start(args, format);
    ret = vsnprintf(&dest[curDestLen], (destsize - curDestLen), format, args);
    va_end(args);
    ASSERTI_TRUE(ret > 0, dest, ME, "nothing written");
    ASSERTI_FALSE(ret >= (int) (destsize - curDestLen), dest, ME, "output was truncated");
    return dest;
}

/**
 * @brief Count the number of times a given character appears in string.
 *
 * @deprecated Use `swl_string.h` instead.
 *
 * @param src source string
 * @param tgt the character to count
 * @return uint32_t number of times tgt appears in src
 */
uint32_t wldu_countChar(const char* src, char tgt) {
    ASSERT_NOT_NULL(src, 0, ME, "NULL");

    uint32_t count = 0;
    uint32_t i = 0;

    while(src[i] != '\0') {
        if(src[i] == tgt) {
            count++;
        }
        i++;
    }

    return count;
}

/** @deprecated Use `swl_string.h` instead */
bool wldu_strStartsWith(const char* msg, const char* prefix) {
    if((msg == NULL) || (prefix == NULL)) {
        return (msg == NULL) && (prefix == NULL);
    }
    return strncmp(msg, prefix, strlen(prefix)) == 0;
}

/** @deprecated Use `swl_uuid_fromMacAddress` instead */
/* The following functions have been borrowed and adapted from hostapd*/
/* Offset added for making a different UUID on Radio base */
int makeUUID_fromMACaddress(uint8_t uuid[UUID_LEN], char MACaddr[18], int offset) { //ex:E8:F1:B0:CE:07:D4
    int i, j;

    //copy the 12 chars of the mac address, in loop, until the UUID is filled.
    for(i = 0, j = 0; i < UUID_LEN; i++) {
        if((MACaddr[j] == ':') || (MACaddr[j] == '-')) {
            j++;  //skip semicolons and dashes.
        }
        uuid[i] = (MACaddr[j] + offset);

        if(++j > 16) {
            j = 0;
        }
    }

    /* See BUG53938: At least on Celeno Windows7 is unhappy with version
     * 4/variant 8 UUIDs.  It does accept version f/variant f. */
    uuid[6] &= 0x0f; uuid[6] |= (0xf << 4);
    uuid[8] &= 0x3f; uuid[8] |= 0xf0;
    SAH_TRACEZ_INFO(ME, "uuid:%s, MACaddr:%s", uuid, MACaddr);
    return 0;
}

int makeUUID_fromrandom(uint8_t uuid[UUID_LEN]) {
    if(!get_random(uuid, UUID_LEN)) {
        return -1;
    }

    /* Replace certain bits as specified in rfc4122 or X.667 */
    uuid[6] &= 0x0f; uuid[6] |= (4 << 4);   /* version 4 == random gen */
    uuid[8] &= 0x3f; uuid[8] |= 0x80;
    return 0;
}

/** @deprecated Use `swl_uuid_binToStr` instead */
int uuid_bin2str(const uint8_t* bin, char* str, size_t max_len) {
    int len;
    len = snprintf(str, max_len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
                   "%02x%02x-%02x%02x%02x%02x%02x%02x",
                   bin[0], bin[1], bin[2], bin[3],
                   bin[4], bin[5], bin[6], bin[7],
                   bin[8], bin[9], bin[10], bin[11],
                   bin[12], bin[13], bin[14], bin[15]);
    SAH_TRACEZ_INFO(ME, "uuid:%s, len:%d, max_len:%d", str, len, (int) max_len);
    if((len < 0) || ((size_t) len >= max_len)) {
        return -1;
    }
    SAH_TRACEZ_INFO(ME, "uuid:%s", str);
    return 0;
}

/**
 * @details stripOutToken
 *
 * @date (6/28/2012)
 *
 * @param pD Data buffer
 * @param pT Token that must be cut out...
 *
 * return pD pointer!
 */
char* stripOutToken(char* pD, const char* pT) {
    ASSERTS_NOT_NULL(pD, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pT, NULL, ME, "NULL");

    char* pR = pD;
    char* pC = pD;

    while(pR && *pR) {
        if((*pR == *pT) && (pR == strstr(pR, pT))) {
            /* We've a hit */
            pR += strlen(pT);
            continue;
        }
        *pC = *pR;
        pC++;
        pR++;
    }
    *pC = '\0';

    return pD;
}

/**
 *
 * @details stripString
 *
 * @param pTL Buffer for String Token List
 * @param nrTL Max nr of Token List
 * @param pD Data buffer
 * @param pT Token that is used as deliminator
 *
 * @return char**
 */
char** stripString(char** pTL, int nrTL, char* pD, const char* pT) {
    int i;
    char* pCP[2];

    for(i = nrTL - 1; i >= 0; pTL[i] = NULL, i--) {
    }

    i = 0;
    pCP[0] = pD;

    while(pCP[0] && i < nrTL) {
        pTL[i++] = pCP[0];
        pCP[1] = strstr(pCP[0], pT);
        if(pCP[1]) {
            *pCP[1] = '\0';
            pCP[0] = pCP[1] + 1;
        } else {
            return pTL;
        }
    }
    return NULL;
}

bool bitmask_to_string(amxc_string_t* output, const char** strings, const char separator, const uint32_t bitmask) {
    int i, bit;
    size_t currentLength = 0;
    size_t newLength = 0;

    amxc_string_reset(output);

    if(!output || !strings) {
        SAH_TRACEZ_ERROR(ME, "argument is NULL");
        return false;
    }

    for(i = 0; strings[i]; i++) {
        bit = 1 << i;
        if((bitmask & bit) && (strings[i][0])) {
            int ret = 0;
            if(amxc_string_is_empty(output) || (separator == '\0')) {
                ret = amxc_string_append(output, strings[i], strlen(strings[i]));
            } else {
                ret = amxc_string_appendf(output, "%c%s", separator, strings[i]);
            }
            if((ret != 0) || ((newLength = amxc_string_text_length(output)) == currentLength)) {
                SAH_TRACEZ_ERROR(ME, "length %d stays for '%s' + '%s'", (int) currentLength, output->buffer, strings[i]);
                return false;
            }
            currentLength = newLength;
        }
    }
    return true;
}

/* build a bitmask from a list of strings
 * for each string found, the corresponding bitmask will be or'ed into the result
 * if a separator is given, the elements will be required to be properly separated.
 * if no masks are given, a one will be used, shifted left by the index the found string has in the list.
 *
 * @precondition: no element in `strings` is a substring of another element.
 *
 * @deprecated, Use `swl_conv_charToMaskSep` instead
 * Error: return false when a string appears more than one time in the input.
 */
bool string_to_bitmask(uint32_t* output, const char* input, const char** strings, const uint32_t* masks, const char separator) {
    int i;
    uint32_t r = 0;
    const char* pos;
    if(!output || !input || !strings) {
        return false;
    }
    for(i = 0; strings[i] && (!masks || masks[i]); i++) {
        pos = strstr(input, strings[i]);
        if(!pos) {
            continue;
        }

        if(masks) {
            r |= masks[i];
        } else {
            r |= 1 << i;
        }
        if((pos != input) && (separator != '\0') && (*(pos - 1) != separator)) {
            return false;
        }
    }
    *output = r;
    return true;
}

/**
 * @brief Convert string notation to hex int
 *
 * @param pbDest the hex int array pointer
 * @param destSize number of hexadicimal values within the int array
 * @param pbSrc the string to convert
 * @param srcSize number of characters within the string
 */
void convStrToHex(uint8_t* pbDest, int destSize, const char* pbSrc, int srcSize) {
    ASSERT_NOT_NULL(pbDest, , ME, "NULL");
    ASSERT_NOT_NULL(pbSrc, , ME, "NULL");
    int hexSize = srcSize / 2;
    ASSERT_TRUE(hexSize <= destSize, , ME, "string to big for small hex array");

    int i;
    for(i = 0; i < hexSize; i++) {
        char h1 = pbSrc[2 * i];
        char h2 = pbSrc[2 * i + 1];

        uint8_t s1 = toupper(h1) - 0x30;
        if(s1 > 9) {
            s1 -= 7;
        }

        uint8_t s2 = toupper(h2) - 0x30;
        if(s2 > 9) {
            s2 -= 7;
        }

        pbDest[i] = s1 * 16 + s2;
    }
}

/**
 * @brief get_pattern_string
 *
 * convert a given hexdigit string based magic pattern to a hex based pattern
 *
 * @param arg hexdigit string based magic pattern
 * @param pattern hex based pattern
 * @return pattern string lenght
 */
int get_pattern_string(const char* arg, uint8_t* pattern) {
    int loop = 0;
    int num = 0;
    int pattern_len = strnlen(arg, MAX_USER_DEFINED_MAGIC << 1);

    while(loop < pattern_len) {
        if(isxdigit(arg[loop]) && isxdigit(arg[loop + 1])) {
            convStrToHex(&pattern[num], 1, &arg[loop], 2);
            num++;
            loop += 2;
        } else {
            loop++;
        }
    }
    return num;
}


/**
 * get the monotonic elapsed time.
 */
time_t wld_util_time_monotonic_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

/**
 * Convert a monotonic time in seconds to a struct tm timeval.
 * Note that if the time is zero, zero time is returned.
 */
void wld_util_time_monotonic_to_tm(const time_t seconds, struct tm* tm_val) {
    if(seconds == 0) {
        memset(tm_val, 0, sizeof(struct tm));
        return;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    time_t curMonotonic = wld_util_time_monotonic_sec();
    time_t diff = curMonotonic - seconds;
    ts.tv_sec -= diff;

    gmtime_r(&(ts.tv_sec), tm_val);
}

/**
 * Get the current time as a struct tm timeval
 */
void wld_util_time_curr_tm(struct tm* tm_val) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&(ts.tv_sec), tm_val);
}

void wld_util_time_addMonotonicTimeToMap(amxc_var_t* map, const char* name, time_t time) {
    struct tm timeval;
    wld_util_time_monotonic_to_tm(time, &timeval);
    amxc_ts_t tsp;
    amxc_ts_to_tm_local(&tsp, &timeval);
    amxc_var_add_key(amxc_ts_t, map, name, &tsp);
}

void wld_util_time_objectSetMonotonicTime(amxd_object_t* object _UNUSED, const char* name, time_t time) {
    amxc_ts_t ts;
    struct tm timeval;
    wld_util_time_monotonic_to_tm(time, &timeval);
    amxd_object_set_value(amxc_ts_t, object, name, &ts);
}

int32_t wld_util_performFactorStep(int32_t accum, int32_t newVal, int32_t factor) {
    if(accum == 0) {
        //if currently accumulator is empty, just initialize with newVal
        return newVal * 1000;
    }

    int64_t accumulator = accum;
    accumulator = ((1000 - factor) * accumulator) / 1000 + factor * newVal;
    return (int32_t) accumulator;
}

int wld_writeCapsToString(char* buffer, uint32_t size, const char* const* strArray, uint32_t caps, uint32_t maxCaps) {
    ASSERT_NOT_NULL(buffer, -1, ME, "NULL");
    ASSERT_TRUE(size > 0, -1, ME, "null size");
    uint32_t i = 0;
    uint32_t mask = 0;
    buffer[0] = 0;
    for(i = 0; i < maxCaps; i++) {
        mask = (1 << i);
        if((mask & caps) != 0) {
            wldu_catFormat(buffer, size, "%s%s", (strlen(buffer) ? "," : ""), strArray[i]);
            if(strlen(buffer) >= (size - 1)) {
                return -1;
            }
        }
    }
    return 0;
}


/**
 * @brief wld_bitmaskToCSValues
 *
 * Convert string table with bitmask to a comma seperated string list
 *
 * @param string [OUT] comma seperated string list
 * @param stringsize [IN] buffer size of string
 * @param bitmask [IN] bits pattern that represent the supported string values
 * @param strings [IN] string table
 */
void wld_bitmaskToCSValues(char* string, size_t stringsize, const int bitmask, const char* strings[]) {
    int idx, bitchk;
    string[0] = '\0';
    for(idx = 0; strings[idx]; idx++) {
        bitchk = 1 << idx;
        if(bitmask & bitchk) {
            if(idx && string[0]) {
                wldu_catStr(string, ",", stringsize);
            }
            wldu_catStr(string, strings[idx], stringsize);
        }
    }
}

int wld_util_getBanList(T_AccessPoint* pAP, wld_macfilterList_t* macList) {

    int nr_entries = 0;
    int i;
    if(pAP->MF_Mode == APMFM_WHITELIST) {
        macList->status = MF_ALLOW;
        for(i = 0; i < pAP->MF_EntryCount; i++) {
            if(!pAP->MF_TempBlacklistEnable
               || (!wldu_is_mac_in_list(pAP->MF_Entry[i], pAP->MF_Temp_Entry, pAP->MF_TempEntryCount)
                   && !wldu_is_mac_in_list(pAP->MF_Entry[i], pAP->PF_Temp_Entry, pAP->PF_TempEntryCount))) {
                // add entry either if temporary blacklisting is disabled, or entry not in temp blaclist
                memcpy(macList->macAddressList[nr_entries], pAP->MF_Entry[i], ETHER_ADDR_LEN);
                nr_entries++;
            }
        }
    } else if((pAP->MF_Mode == APMFM_BLACKLIST)
              || (pAP->MF_TempBlacklistEnable && ((pAP->MF_TempEntryCount > 0) || (pAP->PF_TempEntryCount > 0)))) {
        macList->status = MF_DENY;
        if(pAP->MF_Mode == APMFM_BLACKLIST) {
            for(i = 0; i < pAP->MF_EntryCount; i++) {
                memcpy(macList->macAddressList[nr_entries], pAP->MF_Entry[i], ETHER_ADDR_LEN);
                nr_entries++;
            }
        }
        if(pAP->MF_TempBlacklistEnable) {
            for(i = 0; i < pAP->MF_TempEntryCount; i++) {
                memcpy(macList->macAddressList[nr_entries], pAP->MF_Temp_Entry[i], ETHER_ADDR_LEN);
                nr_entries++;
            }
            for(i = 0; i < pAP->PF_TempEntryCount; i++) {
                memcpy(macList->macAddressList[nr_entries], pAP->PF_Temp_Entry[i], ETHER_ADDR_LEN);
                nr_entries++;
            }
        }
    } else {
        macList->status = MF_DISABLED;
    }
    SAH_TRACEZ_INFO(ME, "Ban mode %u, has %u entries", macList->status, nr_entries);
    for(i = 0; i < (int) nr_entries; i++) {
        SAH_TRACEZ_INFO(ME, "Entry %u "MAC_PRINT_FMT, i, MAC_PRINT_ARG((unsigned char*) &macList->macAddressList[i]));
    }
    macList->nrStaInList = nr_entries;
    return WLD_OK;
}

bool wld_util_areAllVapsDisabled(T_Radio* pRad) {
    T_AccessPoint* pAP;
    amxc_llist_for_each(it, &pRad->llAP) {
        pAP = (T_AccessPoint*) amxc_llist_it_get_data(it, T_AccessPoint, it);
        if(pAP && pAP->enable) {
            return false;
        }
    }
    return true;
}

bool wld_util_areAllEndpointsDisabled(T_Radio* pRad) {
    T_EndPoint* pEP;
    amxc_llist_for_each(it, &pRad->llEndPoints) {
        pEP = (T_EndPoint*) amxc_llist_it_get_data(it, T_EndPoint, it);
        if(pEP && pEP->enable) {
            return false;
        }
    }
    return true;
}

bool wld_util_isPowerOf(int32_t value, int32_t power) {
    ASSERT_NOT_EQUALS(value, 0, false, ME, "ZERO");
    ASSERT_NOT_EQUALS(power, 0, false, ME, "ZERO");
    while(value != 1) {
        if((value % power) != 0) {
            return false;
        }
        value = value / power;
    }

    return true;
}

bool wld_util_isPowerOf2(int32_t value) {
    ASSERT_TRUE(value > 0, false, ME, "INVALID");
    int bit = value;
    bit &= -bit;
    return (value == (bit));

}

int wld_util_getLinuxIntfState(int socket, char* ifname) {
    return wld_linuxIfUtils_getState(socket, ifname);
}

int wld_util_setLinuxIntfState(int socket, char* ifname, int state) {
    return wld_linuxIfUtils_setState(socket, ifname, state);
}

wld_tuple_t* wldu_getTupleById(wld_tuplelist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tupleId_t* wldu_getTupleIdById(wld_tupleIdlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tupleVoid_t* wldu_getTupleVoidById(wld_tupleVoidlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        if(list->tuples[i].id == id) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

wld_tuple_t* wldu_getTupleByStr(wld_tuplelist_t* list, char* str) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    ASSERT_NOT_NULL(str, NULL, ME, "NULL");
    uint32_t i;
    for(i = 0; i < list->size; i++) {
        ASSERT_NOT_NULL(list->tuples[i].str, NULL, ME, "NULL");
        if(strcmp(list->tuples[i].str, str) == 0) {
            return &list->tuples[i];
        }
    }
    return NULL;
}

const char* wldu_tuple_id2str(wld_tuplelist_t* list, int64_t id, const char* defStr) {
    ASSERT_NOT_NULL(list, defStr, ME, "NULL");
    wld_tuple_t* pTuple = wldu_getTupleById(list, id);
    ASSERTS_NOT_NULL(pTuple, defStr, ME, "NULL");
    return pTuple->str;
}

const void* wldu_tuple_id2void(wld_tupleVoidlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, NULL, ME, "NULL");
    wld_tupleVoid_t* pTuple = wldu_getTupleVoidById(list, id);
    ASSERTS_NOT_NULL(pTuple, NULL, ME, "NULL");
    return pTuple->ptr;
}

int64_t wldu_tuple_str2id(wld_tuplelist_t* list, char* str, int64_t defId) {
    ASSERT_NOT_NULL(list, defId, ME, "NULL");
    wld_tuple_t* pTuple = wldu_getTupleByStr(list, str);
    ASSERTS_NOT_NULL(pTuple, defId, ME, "NULL");
    return pTuple->id;
}

int64_t wldu_tuple_id2index(wld_tupleIdlist_t* list, int64_t id) {
    ASSERT_NOT_NULL(list, 0, ME, "NULL");
    wld_tupleId_t* pTuple = wldu_getTupleIdById(list, id);
    ASSERTS_NOT_NULL(pTuple, 0, ME, "NULL");
    return pTuple->index;
}

/*
 * converts a string to a mask based on tuple set, where tuples contain masks.
 * return false if not all the tuple set is matched.
 */
bool wldu_tuple_convStrToMaskByMask(wld_tuplelist_t* pList, const amxc_string_t* pNames, uint64_t* pBitMask) {
    bool allMatched = true;
    uint64_t bitMask = 0;
    ASSERT_NOT_NULL(pNames, false, ME, "NULL");
    ASSERT_NOT_NULL(pList, false, ME, "NULL");
    if(pBitMask) {
        *pBitMask = 0;
    }
    ASSERTS_FALSE(amxc_string_is_empty(pNames), true, ME, "empty names selection");
    ASSERTS_NOT_EQUALS(pList->size, 0, false, ME, "empty list");
    wld_tuple_t* pTuple;
    char selection[amxc_string_text_length(pNames) + 1];
    wldu_copyStr(selection, pNames->buffer, sizeof(selection));
    char* p = selection;
    char* tk = NULL;
    while((tk = strsep(&p, ",")) != NULL) {
        if(*tk == 0) {
            continue;
        }
        pTuple = wldu_getTupleByStr(pList, tk);
        if(pTuple == NULL) {
            SAH_TRACEZ_NOTICE(ME, "unknow name (%s)", tk);
            allMatched = false;
            continue;
        }
        bitMask |= pTuple->id;
    }
    if(pBitMask) {
        *pBitMask = bitMask;
    }
    return allMatched;
}

/*
 * converts a bitmask to a string based on tuple set, where tuples contain masks.
 * return false if not all the tuple set is matched.
 */
bool wldu_tuple_convMaskToStrByMask(wld_tuplelist_t* pList, uint64_t bitMask, amxc_string_t* pNames) {
    bool allMatched = true;
    ASSERT_NOT_NULL(pList, false, ME, "NULL");
    ASSERT_NOT_NULL(pNames, false, ME, "NULL");
    uint32_t i;
    int64_t id;
    uint64_t flag;
    wld_tuple_t* pTuple;
    amxc_string_clean(pNames);
    ASSERTS_NOT_EQUALS(bitMask, 0, true, ME, "empty bitmask selection");
    ASSERTS_NOT_EQUALS(pList->size, 0, false, ME, "empty list");
    for(i = 0; i < WLD_BIT_SIZE(bitMask); i++) {
        flag = (1ULL << i);
        id = (bitMask & flag);
        if(id == 0) {
            continue;
        }
        pTuple = wldu_getTupleById(pList, id);
        if(pTuple == NULL) {
            SAH_TRACEZ_NOTICE(ME, "unknown bitmask flag at shifted pos %d", i);
            allMatched = false;
            continue;
        }
        amxc_string_appendf(pNames, "%s%s", amxc_string_is_empty(pNames) ? "" : ",", pTuple->str);
    }
    return allMatched;
}

char* wldu_getLocalFile(char* buffer, size_t bufSize, char* path, char* format, ...) {
    va_list args;
    char* basePath = getenv("WLD_DBG_PATH");

    char fileNameBuffer[128];
    va_start(args, format);
    int ret = vsnprintf(fileNameBuffer, sizeof(fileNameBuffer), format, args);
    va_end(args);
    ASSERT_TRUE(ret >= 0, NULL, ME, "Invalid format / args %s", format);

    if(basePath && basePath[0]) {
        snprintf(buffer, bufSize, "%s/%s", basePath, fileNameBuffer);
        if(access(buffer, F_OK) != -1) {
            return buffer;
        }
    }
    snprintf(buffer, bufSize, "%s/%s", path, fileNameBuffer);
    return buffer;
}

swl_bandwidth_e wld_util_getMaxBwCap(wld_assocDev_capabilities_t* caps) {
    if(caps->vhtCapabilities & M_SWL_STACAP_VHT_SGI160) {
        return SWL_BW_160MHZ;
    }
    if(caps->vhtCapabilities & M_SWL_STACAP_VHT_SGI80) {
        return SWL_BW_80MHZ;
    }
    if(caps->htCapabilities & M_SWL_STACAP_HT_SGI40) {
        return SWL_BW_40MHZ;
    }
    if(caps->htCapabilities & M_SWL_STACAP_HT_SGI20) {
        return SWL_BW_20MHZ;
    }
    return SWL_BW_AUTO;
}
/**
 * Create a new MD5 hash string based on ssid and key string.
 */
void wldu_convCreds2MD5(const char* ssid, const char* key, char* md5, int md5_size) {
    char newKeyPassPhrase_md5[PSK_KEY_SIZE_LEN - 1] = {'\0'};
    PKCS5_PBKDF2_HMAC_SHA1(key, strlen(key),
                           (unsigned char*) ssid, strlen(ssid), 4096,
                           sizeof(newKeyPassPhrase_md5), (unsigned char*) newKeyPassPhrase_md5);
    convHex2Str((unsigned char*) newKeyPassPhrase_md5, sizeof(newKeyPassPhrase_md5), (unsigned char*) md5, md5_size, FALSE);
}

/**
 * Compare current key with new key.
 * If one key is a hash, a new hash is created and compared.
 * return false if different, true if equals.
 */
bool wldu_key_matches(const char* ssid, const char* oldKeyPassPhrase, const char* keyPassPhrase) {
    ASSERTS_NOT_NULL(ssid, false, ME, "NULL");
    ASSERTS_NOT_NULL(oldKeyPassPhrase, false, ME, "NULL");
    ASSERTS_NOT_NULL(keyPassPhrase, false, ME, "NULL");
    if(!strcmp(oldKeyPassPhrase, keyPassPhrase)) {
        SAH_TRACEZ_INFO(ME, "Keys are identical");
        return true;
    }
    int newKeyLen = strlen(keyPassPhrase) + 1;
    int currentKeyLen = strlen(oldKeyPassPhrase) + 1;
    char newKeyPassPhrase_md5_str[PSK_KEY_SIZE_LEN * 2 - 1] = {'\0'};
    if((currentKeyLen == PSK_KEY_SIZE_LEN) && (newKeyLen != PSK_KEY_SIZE_LEN)) {
        //oldKeyPassPhrase is a hash, need to compare this to the new hash
        wldu_convCreds2MD5(ssid, keyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN * 2 - 1);
        return (strncmp(oldKeyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN - 1) == 0);
    } else if((currentKeyLen != PSK_KEY_SIZE_LEN) && (newKeyLen == PSK_KEY_SIZE_LEN)) {
        //oldKeyPassPhrase is not a hash and keyPassPhrase is a hash
        wldu_convCreds2MD5(ssid, oldKeyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN * 2 - 1);
        return (strncmp(keyPassPhrase, newKeyPassPhrase_md5_str, PSK_KEY_SIZE_LEN - 1) == 0);
    } else {
        SAH_TRACEZ_INFO(ME, "Keys are different");
        return false;
    }
    SAH_TRACEZ_INFO(ME, "Keys are identical");
    return true;
}

/**
 * get the MFPMode
 */
wld_mfpConfig_e wld_util_getTargetMfpMode(wld_securityMode_e securityMode, wld_mfpConfig_e mfpConfig) {
    if((securityMode == APMSI_WPA3_P)
       && (mfpConfig < WLD_MFP_REQUIRED)) {
        return WLD_MFP_REQUIRED;
    }
    if((securityMode == APMSI_WPA2_WPA3_P)
       && (mfpConfig < WLD_MFP_OPTIONAL)) {
        return WLD_MFP_OPTIONAL;
    }
    return mfpConfig;
}

amxd_status_t wld_util_stats2Obj(amxd_object_t* obj, T_Stats* stats) {
    ASSERT_NOT_NULL(obj, false, ME, "NULL");
    ASSERT_NOT_NULL(stats, false, ME, "NULL");

    amxd_object_set_uint64_t(obj, "BytesSent", stats->BytesSent);
    amxd_object_set_uint64_t(obj, "BytesReceived", stats->BytesReceived);
    amxd_object_set_uint64_t(obj, "PacketsSent", stats->PacketsSent);
    amxd_object_set_uint64_t(obj, "PacketsReceived", stats->PacketsReceived);
    amxd_object_set_uint32_t(obj, "ErrorsSent", stats->ErrorsSent);
    amxd_object_set_uint32_t(obj, "RetransCount", stats->RetransCount);
    amxd_object_set_uint32_t(obj, "ErrorsReceived", stats->ErrorsReceived);
    amxd_object_set_uint32_t(obj, "DiscardPacketsSent", stats->DiscardPacketsSent);
    amxd_object_set_uint32_t(obj, "DiscardPacketsReceived", stats->DiscardPacketsReceived);
    amxd_object_set_uint32_t(obj, "UnicastPacketsSent", stats->UnicastPacketsSent);
    amxd_object_set_uint32_t(obj, "UnicastPacketsReceived", stats->UnicastPacketsReceived);
    amxd_object_set_uint32_t(obj, "MulticastPacketsSent", stats->MulticastPacketsSent);
    amxd_object_set_uint32_t(obj, "MulticastPacketsReceived", stats->MulticastPacketsReceived);
    amxd_object_set_uint32_t(obj, "BroadcastPacketsSent", stats->BroadcastPacketsSent);
    amxd_object_set_uint32_t(obj, "BroadcastPacketsReceived", stats->BroadcastPacketsReceived);
    amxd_object_set_uint32_t(obj, "UnknownProtoPacketsReceived", stats->UnknownProtoPacketsReceived);
    amxd_object_set_uint32_t(obj, "FailedRetransCount", stats->FailedRetransCount);
    amxd_object_set_uint32_t(obj, "RetryCount", stats->RetryCount);
    amxd_object_set_uint32_t(obj, "MultipleRetryCount", stats->MultipleRetryCount);
    amxd_object_set_uint32_t(obj, "Temperature", stats->TemperatureDegreesCelsius);
    amxd_object_set_int32_t(obj, "Noise", stats->noise);

    return amxd_status_ok;
}

amxd_status_t wld_util_stats2Var(amxc_var_t* map, T_Stats* stats) {
    amxc_var_add_key(uint64_t, map, "BytesSent", stats->BytesSent);
    amxc_var_add_key(uint64_t, map, "BytesReceived", stats->BytesReceived);
    amxc_var_add_key(uint64_t, map, "PacketsSent", stats->PacketsSent);
    amxc_var_add_key(uint64_t, map, "PacketsReceived", stats->PacketsReceived);
    amxc_var_add_key(uint32_t, map, "ErrorsSent", stats->ErrorsSent);
    amxc_var_add_key(uint32_t, map, "ErrorsReceived", stats->ErrorsReceived);
    amxc_var_add_key(uint32_t, map, "RetransCount", stats->RetransCount);
    amxc_var_add_key(uint32_t, map, "DiscardPacketsSent", stats->DiscardPacketsSent);
    amxc_var_add_key(uint32_t, map, "DiscardPacketsReceived", stats->DiscardPacketsReceived);
    amxc_var_add_key(uint32_t, map, "UnicastPacketsSent", stats->UnicastPacketsSent);
    amxc_var_add_key(uint32_t, map, "UnicastPacketsReceived", stats->UnicastPacketsReceived);
    amxc_var_add_key(uint32_t, map, "MulticastPacketsSent", stats->MulticastPacketsSent);
    amxc_var_add_key(uint32_t, map, "MulticastPacketsReceived", stats->MulticastPacketsReceived);
    amxc_var_add_key(uint32_t, map, "BroadcastPacketsSent", stats->BroadcastPacketsSent);
    amxc_var_add_key(uint32_t, map, "BroadcastPacketsReceived", stats->BroadcastPacketsReceived);
    amxc_var_add_key(uint32_t, map, "UnknownProtoPacketsReceived", stats->UnknownProtoPacketsReceived);
    amxc_var_add_key(uint32_t, map, "FailedRetransCount", stats->FailedRetransCount);
    amxc_var_add_key(uint32_t, map, "RetryCount", stats->RetryCount);
    amxc_var_add_key(uint32_t, map, "MultipleRetryCount", stats->MultipleRetryCount);
    amxc_var_add_key(uint32_t, map, "Temperature", stats->TemperatureDegreesCelsius);
    amxc_var_add_key(int64_t, map, "Noise", stats->noise);

    return amxd_status_ok;
}

void wld_util_writeWmmStats(amxd_object_t* parentObj, const char* objectName, unsigned long* stats) {
    amxd_object_t* object = amxd_object_get(parentObj, objectName);
    ASSERT_NOT_NULL(object, , ME, "No object named <%s>", objectName);
    amxd_object_set_uint64_t(object, "AC_BE", stats[WLD_AC_BE]);
    amxd_object_set_uint64_t(object, "AC_BK", stats[WLD_AC_BK]);
    amxd_object_set_uint64_t(object, "AC_VO", stats[WLD_AC_VO]);
    amxd_object_set_uint64_t(object, "AC_VI", stats[WLD_AC_VI]);
}

void wld_util_addWmmStats(amxd_object_t* parentObj, amxc_var_t* map _UNUSED, const char* name) {
    amxd_object_t* object = amxd_object_get(parentObj, name);
    ASSERT_NOT_NULL(object, , ME, "No object named <%s>", name);
    amxc_var_t my_map;
    amxc_var_init(&my_map);
    //Add data
    amxc_var_clean(&my_map);
}

void wld_util_updateWmmStats(amxd_object_t* parentObj, const char* objectName, unsigned long* stats) {
    amxd_object_t* object = amxd_object_get(parentObj, objectName);

    WLD_SET_VAR_UINT64(object, "AC_BE", stats[WLD_AC_BE]);
    WLD_SET_VAR_UINT64(object, "AC_BK", stats[WLD_AC_BK]);
    WLD_SET_VAR_UINT64(object, "AC_VO", stats[WLD_AC_VO]);
    WLD_SET_VAR_UINT64(object, "AC_VI", stats[WLD_AC_VI]);
}
