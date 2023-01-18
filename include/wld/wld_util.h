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
#ifndef __WLD_UTIL_H__
#define __WLD_UTIL_H__

#include "wld.h"




#ifdef __cplusplus
extern "C" {
#endif

// Fix compiler warnings
#define _UNUSED_(x) (void) x
#define _UNUSED __attribute__((unused))

// Generic utilities
/* Taken from NeMo, used in attribute flags */
#define ARG_BYNAME request_function_args_by_name
#define ARG_MANDATORY   (0x80000000)

#define TRACEZ_IO_IN "ioIn"
#define TRACEZ_IO_OUT "ioOut"
#define TRACEZ_IO_PROBE "ioProbe" //Special trace zone for probe updates.

/* For our helper function */
#define TPH_STR       (0x01)
#define TPH_INT8      (0x02)
#define TPH_INT16     (0x03)
#define TPH_INT32     (0x04)
#define TPH_UINT8     (0x05)
#define TPH_UINT16    (0x06)
#define TPH_UINT32    (0x07)
#define TPH_BOOL      (0x08)
#define TPH_DOUBLE    (0x09)
#define TPH_BSTR      (0x0A)
#define TPH_SECSTR    (0x0B)
#define TPH_WPS_CM    (0x0C)
#define TPH_SEL_SSID  (0x0D)
#define TPH_PSTR      (0x0E)

#define MAX_USER_DEFINED_MAGIC  256

#define WLD_S_BUF 32
#define WLD_M_BUF 128
#define WLD_L_BUF 1024
#define WLD_XL_BUF 8192

#define WLD_SEC_PER_H 3600
#define WLD_SEC_PER_DAY 86400

typedef struct {
    char buf[WLD_S_BUF];
} wld_sbuf_t;

typedef struct {
    char buf[WLD_M_BUF];
} wld_mbuf_t;

typedef struct {
    char buf[WLD_L_BUF];
} wld_lbuf_t;

#define isxDigit(c)  ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

#define WLD_MIN(a, b) \
    ({ __typeof__ (a) _a = (a); \
         __typeof__ (b) _b = (b); \
         _a < _b ? _a : _b; })
#define WLD_MAX(a, b) \
    ({ __typeof__ (a) _a = (a); \
         __typeof__ (b) _b = (b); \
         _a > _b ? _a : _b; })

#define WLD_ARRAY_SIZE(X) (((X) == NULL) ? 0 : (sizeof((X)) / sizeof((X)[0])))
#define WLD_BYTE_SIZE(X) (sizeof(typeof(X)))
#define WLD_HEXSTR_SIZE(X) (WLD_BYTE_SIZE(X) * 2)
#define WLD_BIT_SIZE(X) (WLD_BYTE_SIZE(X) * 8)

bool __WLD_BUG_ON(bool bug, const char* condition, const char* function,
                  const char* file, int line);

#define WLD_BUG_ON(x) \
    __WLD_BUG_ON(!!(x), #x, __FUNCTION__, __FILE__, __LINE__)

typedef int32_t lbool_t;
#define LTRUE __LINE__
#define LFALSE -__LINE__
#define LBOOL(x) ((x) ? LTRUE : LFALSE)

void wldu_llist_freeDataInternal(amxc_llist_t* list, size_t offset);
void wldu_llist_freeDataFunInternal(amxc_llist_t* list, size_t offset, void (* destroy)(void* val));
void wldu_llist_mapInternal(amxc_llist_t* list, size_t offset, void (* map)(void* val, void* data), void* data);


#define llist_freeData(list, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it))
#define llist_destroyData(list, destroyFun, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it), destroyFun)
#define llist_map(list, mapFun, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it), mapFun, data)

/* Helper macro's for MAC debugging */
#define MAC_PRINT_FMT   "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_PRINT_ARG(mac) (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]

/* MAC address define */
#define WLD_EMPTY_MAC_ADDRESS "00:00:00:00:00:00"
#define WLD_BROADCAST_MAC_ADDRES "FF:FF:FF:FF:FF:FF"

static const uint8_t wld_ether_bcast[ETHER_ADDR_LEN] = {255, 255, 255, 255, 255, 255};
static const uint8_t wld_ether_null[ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

#define WLD_SET_VAR_INT32(object, objectName, val) { \
        amxd_param_t* param = amxd_object_get_param_def(object, objectName); \
        amxc_var_set(int32_t, &param->value, val);}

#define WLD_SET_VAR_UINT32(object, objectName, val) { \
        amxd_param_t* param = amxd_object_get_param_def(object, objectName); \
        amxc_var_set(uint32_t, &param->value, val);}

#define WLD_SET_VAR_UINT64(object, objectName, val) { \
        amxd_param_t* param = amxd_object_get_param_def(object, objectName); \
        amxc_var_set(uint64_t, &param->value, val);}

#define WLD_SET_VAR_BOOL(object, objectName, val) { \
        amxd_param_t* param = amxd_object_get_param_def(object, objectName); \
        amxc_var_set(bool, &param->value, val);}

#define WLD_SET_VAR_CSTRING(object, objectName, val) { \
        amxd_param_t* param = amxd_object_get_param_def(object, objectName); \
        amxc_var_set(cstring_t, &param->value, val);}

bool findStrInArray(const char* str, const char** strarray, int* index);
int findStrInArrayN(const char* str, const char** strarray, int size, int default_index);
bool convIntArrToString(char* str, int str_size, const int* int_list, int list_size);
bool convStrArrToStr(char* str, int str_size, const char** strList, int list_size);
bool convStrToIntArray(int* int_list, size_t list_size, const char* str, int* outSize);
bool wldu_is_mac_in_list(unsigned char macAddr[ETHER_ADDR_LEN], unsigned char macList[][ETHER_ADDR_LEN], int listSize);
bool set_OBJ_ParameterHelper(int type, amxd_object_t* obj, char* rpath, const void* pdata);
bool get_OBJ_ParameterHelper(int type, amxd_object_t* obj, const char* crpath, void* pdata);
unsigned long conv_ModeBitStr(char** enumStrList, char* str);
long conv_ModeIndexStr(const char** enumStrList, const char* str);
uint32_t conv_strToMaskSep(const char* str, const char** enumStrList, uint32_t maxVal, char separator);
uint32_t conv_strToMask(const char* str, const char** enumStrList, uint32_t maxVal);
bool conv_maskToStrSep(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize, char separator);
bool conv_maskToStr(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize);
uint32_t conv_strToEnum(const char** enumStrList, const char* str, uint32_t maxVal, uint32_t defaultVal);
int conv_ModeIndexStrEx(const char** enumStrList, const char* str, int minIdx, int maxIdx);
char* itoa(int value, char* result, int base);

int wldu_convStrToUInt8Arr(uint8_t* tgtList, uint tgtListSize, char* srcStrList);
int getLowestBitLongArray(unsigned long* pval, int elem);
int areBitsSetLongArray(unsigned long* pval, int elem);
int clearAllBitsLongArray(unsigned long* pval, int elem);
int isBitSetLongArray(unsigned long* pval, int elem, int bitNr);
int setBitLongArray(unsigned long* pval, int elem, int bitNr);
int clearBitLongArray(unsigned long* pval, int elem, int bitNr);
int markAllBitsLongArray(unsigned long* pval, int elem, int bitNr);
int isBitOnlySetLongArray(unsigned long* pval, int elem, int bitNr);
int areBitsOnlySetLongArray(unsigned long* pval, int elem, int* bitNrArray, int nrBitElem);

void moveFSM_VAPBit2ACFSM(T_AccessPoint* pAP);
void moveFSM_RADBit2ACFSM(T_Radio* pR);
void moveFSM2ACFSM(T_FSM* fsm);


void longArrayCopy(unsigned long* to, unsigned long* from, int len);
void longArrayBitOr(unsigned long* to, unsigned long* from, int len);
void longArrayClean(unsigned long* array, int len);

char* convASCII_WEPKey(const char* Key, char* out, int sizeout);
char* convHex_WEPKey(const char* key, char* out, int sizeout);

bool isModeWEP(wld_securityMode_e mode);
bool isModeWPAPersonal(wld_securityMode_e mode);
bool isModeWPA3Personal(wld_securityMode_e mode);

wld_securityMode_e isValidWEPKey (const char* key);
bool isValidPSKKey(const char* key);
bool isValidAESKey(const char* key, int maxLen);
bool isValidSSID(const char* ssid);
bool isSanitized (const char* pname);

int isListOfChannels_US(unsigned char* possibleChannels, int len, int band);

#define WPS_PIN_LEN 8
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]);
int wpsPinValid(unsigned long PIN);
bool wldu_checkWpsPinStr(const char* pinStr);

int wldu_convStrToNum(const char* numSrcStr, void* numDstBuf, uint8_t numDstByteSize, uint8_t base, bool isSigned);
uint64_t wldu_parseHexToUint64(const char* src);
uint32_t wldu_parseHexToUint32(const char* src);
uint16_t wldu_parseHexToUint16(const char* src);
uint8_t wldu_parsehexToUint8(const char* src);
int convStr2Mac(unsigned char* mac, unsigned int sizeofmac, const unsigned char* str, unsigned int sizeofstr);
int convMac2Str(unsigned char* mac, unsigned int sizeofmac, unsigned char* str, unsigned int sizeofstr);
int32_t wldu_convStr2Mac(unsigned char* mac, uint32_t sizeofmac, const char* str, uint32_t sizeofstr);
bool wldu_convMac2Str(unsigned char* mac, uint32_t sizeofmac, char* str, uint32_t sizeofstr);
int convHex2Str(const unsigned char* hex, size_t maxhexlen, unsigned char* str, size_t maxstrlen, int upper);
int convStr2Hex(const char* str, size_t maxStrlen, char* hex, size_t maxHexLen);
int mac2str(char* str, const unsigned char* mac, size_t maxstrlen);
int convQuality2Signal(int quality);
int convSignal2Quality(int dBm);

/* SSID manipulation */
char* wld_ssid_to_string(const uint8_t* ssid, uint8_t len);
int convSsid2Str(const uint8_t* ssid, uint8_t ssidLen, char* str, size_t maxStrSize);

/* MacFiltering APIs */
#define MAX_ADDR_IN_FILTER_LIST 64
typedef enum {
    MF_DISABLED,
    MF_ALLOW,
    MF_DENY
} wld_macfilterStatus_e;

typedef struct {
    wld_macfilterStatus_e status;
    unsigned char macAddressList[MAX_ADDR_IN_FILTER_LIST][ETHER_ADDR_LEN];
    int nrStaInList;
} wld_macfilterList_t;

int wld_util_getBanList(T_AccessPoint* pAP, wld_macfilterList_t* macList);

void fsm_delay_reply(uint64_t call_id, amxd_status_t state, int* errval);
int getCountryParam(const char* instr, int CC, int* idx);
char* getFullCountryName(int idx);
char* getShortCountryName(int idx);
int getCountryCode(int idx);

char* wldu_copyStr(char* dest, const char* src, size_t destsize);
char* wldu_catStr(char* dest, const char* src, size_t destsize);
char* wldu_catFormat(char* dest, size_t destsize, const char* format, ...);
uint32_t wldu_countChar(const char* src, char tgt);
bool wldu_strStartsWith(const char* msg, const char* prefix);

int debugIsVapPointer(void* p);
int debugIsEpPointer(void* p);
int debugIsRadPointer(void* p);
int debugIsSsidPointer(void* p);

/** Deprecated. Use `SWL_UUID_BIN_SIZE` from `swl_uuid.h`. */
#define UUID_LEN 16
int get_random(unsigned char* buf, size_t len);
int get_randomstr(unsigned char* buf, size_t len);
int get_randomhexstr(unsigned char* buf, size_t len);
int makeUUID_fromrandom(uint8_t*);
int makeUUID_fromMACaddress(uint8_t*, char*, int);
int uuid_bin2str(const uint8_t*, char*, size_t);

char* stripOutToken(char* pD, const char* pT);
char** stripString(char** pTL, int nrTL, char* pD, const char* pT);

bool bitmask_to_string(amxc_string_t* output, const char** strings, const char separator, const uint32_t bitmask);
bool string_to_bitmask(uint32_t* output, const char* input, const char** strings, const uint32_t* masks, const char separator);
void convStrToHex(uint8_t* pbDest, int destSize, const char* pbSrc, int srcSize);
int get_pattern_string(const char* arg, uint8_t* pattern);

/* libc5 combatability just declare them */
char* if_indextoname(unsigned int __ifindex, char* __ifname);
unsigned int if_nametoindex(const char* __ifname);

/**
 * get the monotonic elapsed time.
 */
time_t wld_util_time_monotonic_sec();
void wld_util_time_curr_tm(struct tm* tm_val);
void wld_util_time_monotonic_to_tm(const time_t seconds, struct tm* tm_val);
void wld_util_time_addMonotonicTimeToMap(amxc_var_t* map, const char* name, time_t time);
void wld_util_time_objectSetMonotonicTime(amxd_object_t* object, const char* name, time_t time);

//how to go from accumulator to actual value. add + or - 499 to ensure proper rounding
#define WLD_ACC_TO_VAL(val) (val >= 0 ? ((val + 500) / 1000) : ((val - 500) / 1000))
int32_t wld_util_performFactorStep(int32_t accum, int32_t newVal, int32_t factor);

int wld_writeCapsToString(char* buffer, uint32_t size, const char* const* strArray, uint32_t caps, uint32_t maxCaps);
void wld_bitmaskToCSValues(char* string, size_t stringsize, const int bitmask, const char* strings[]);
bool wld_util_areAllVapsDisabled(T_Radio* pRad);
bool wld_util_areAllEndpointsDisabled(T_Radio* pRad);
bool wld_util_isPowerOf(int32_t value, int32_t power);
bool wld_util_isPowerOf2(int32_t value);
int wld_util_getLinuxIntfState(int socket, char* ifname);
int wld_util_setLinuxIntfState(int socket, char* ifname, int state);

typedef struct {
    int64_t id;
    int64_t index;
} wld_tupleId_t;

typedef struct {
    wld_tupleId_t* tuples;
    uint32_t size;
} wld_tupleIdlist_t;

typedef struct {
    int64_t id;
    char* str;
} wld_tuple_t;

typedef struct {
    wld_tuple_t* tuples;
    uint32_t size;
} wld_tuplelist_t;

typedef struct {
    int64_t id;
    void* ptr;
} wld_tupleVoid_t;

typedef struct {
    wld_tupleVoid_t* tuples;
    uint32_t size;
} wld_tupleVoidlist_t;

wld_tupleVoid_t* wldu_getTupleVoidById(wld_tupleVoidlist_t* list, int64_t id);
wld_tupleId_t* wldu_getTupleIdById(wld_tupleIdlist_t* list, int64_t id);
wld_tuple_t* wldu_getTupleById(wld_tuplelist_t* list, int64_t id);
wld_tuple_t* wldu_getTupleByStr(wld_tuplelist_t* list, char* str);
const char* wldu_tuple_id2str(wld_tuplelist_t* list, int64_t id, const char* defStr);
const void* wldu_tuple_id2void(wld_tupleVoidlist_t* list, int64_t id);
int64_t wldu_tuple_str2id(wld_tuplelist_t* list, char* str, int64_t defId);
int64_t wldu_tuple_id2index(wld_tupleIdlist_t* list, int64_t id);
bool wldu_tuple_convStrToMaskByMask(wld_tuplelist_t* pList, const amxc_string_t* pNames, uint64_t* pBitMask);
bool wldu_tuple_convMaskToStrByMask(wld_tuplelist_t* pList, uint64_t bitMask, amxc_string_t* pNames);
char* wldu_getLocalFile(char* buffer, size_t bufSize, char* path, char* format, ...);

swl_bandwidth_e wld_util_getMaxBwCap(wld_assocDev_capabilities_t* caps);
bool wldu_key_matches(const char* ssid, const char* oldKeyPassPhrase, const char* keyPassPhrase);
#define WLD_TUPLE(ID, STR) {ID, STR}
#define WLD_TUPLE_GEN(ID) {ID,#ID}
#define WLD_TUPLE_DEF_STR ""

void wldu_convCreds2MD5(const char* ssid, const char* key, char* md5, int md5_size);

wld_mfpConfig_e wld_util_getTargetMfpMode(wld_securityMode_e securityMode, wld_mfpConfig_e mfpConfig);
void wld_util_updateStatusChangeInfo(wld_status_changeInfo_t* info, wld_status_e status);

/**
 * @brief extract object (or instance) name from the object path
 *
 * @param objName (in/out): output buffer where name is saved
 * @param objNameSize (in): max name size
 * @param obj (in): object context
 *
 */
bool wld_util_getObjName(char* objName, size_t objNameSize, amxd_object_t* obj);

/**
 * @brief wld_util_updateStats convert the internal stats context to object map,
 * this object is the actual datamodel representation of the internal context stats
 *
 * @param obj the dict output
 * @param stats stats the internal stats (input)
 * @return amxd_status_t return code
 */
amxd_status_t wld_util_stats2Obj(amxd_object_t* obj, T_Stats* stats);

/**
 * @brief convert the internal stats context to variant map,
 * then the variant map can be the return of any amxd callback
 *
 * @param map the dict output
 * @param stats the internal stats (input)
 * @return amxd_status_t return code
 */
amxd_status_t wld_util_stats2Var(amxc_var_t* map, T_Stats* stats);

void wld_util_writeWmmStats(amxd_object_t* parentObj, const char* objectName, unsigned long* stats);
void wld_util_addWmmStats(amxd_object_t* parentObj, amxc_var_t* map, const char* name);

void wld_util_updateWmmStats(amxd_object_t* parentObj, const char* objectName, unsigned long* stats);

#ifdef __cplusplus
}/* extern "C" */
#endif

#endif
