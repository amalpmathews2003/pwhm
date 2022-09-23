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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <cmocka.h>

#include <debug/sahtrace.h>

#include "wld_util.h"

static void test_convIntArrToString(void** state _UNUSED) {
    int test1[] = {0, 2, 4, 6};
    char buffer[30];
    assert_true(convIntArrToString(buffer, sizeof(buffer), test1, WLD_ARRAY_SIZE(test1)));
    assert_string_equal(buffer, "0,2,4,6");

    int test2[] = {-10, 5, -8, 3, -6, 1};
    assert_true(convIntArrToString(buffer, sizeof(buffer), test2, WLD_ARRAY_SIZE(test2)));
    assert_string_equal(buffer, "-10,5,-8,3,-6,1");

    char buffer2[6];
    assert_false(convIntArrToString(buffer2, sizeof(buffer2), test2, WLD_ARRAY_SIZE(test2)));
    assert_string_equal(buffer2, "-10,5");


    assert_true(convIntArrToString(buffer, sizeof(buffer), test1, 0));
    assert_string_equal(buffer, "");

    assert_false(convIntArrToString(buffer, 0, test1, WLD_ARRAY_SIZE(test1)));
    assert_string_equal(buffer, "");

    assert_false(convIntArrToString(NULL, sizeof(buffer), test1, WLD_ARRAY_SIZE(test1)));
    assert_string_equal(buffer, "");

    assert_false(convIntArrToString(buffer, sizeof(buffer), NULL, WLD_ARRAY_SIZE(test1)));
    assert_string_equal(buffer, "");
}


static void test_convStrToIntArray(void** state _UNUSED) {
    int buffer1[30];
    int buffer2[5];
    int buffer3[30];
    int test1[] = {0, 2, 4, 6};
    int test2[] = {-10, 5, -8, 3, -6, 1};
    int outSize;
    assert_true(convStrToIntArray(buffer1, WLD_ARRAY_SIZE(buffer1), "0,2,4,6", &outSize));
    assert_memory_equal(buffer1, test1, WLD_ARRAY_SIZE(test1));
    assert_int_equal(outSize, 4);

    assert_true(convStrToIntArray(buffer1, WLD_ARRAY_SIZE(buffer1), "-10,5,-8,3,-6,1", &outSize));
    assert_memory_equal(buffer1, test2, WLD_ARRAY_SIZE(test2));
    assert_int_equal(outSize, 6);

    assert_false(convStrToIntArray(buffer2, WLD_ARRAY_SIZE(buffer2), "-10,5,-8,3,-6,1", &outSize));
    assert_memory_equal(buffer1, test2, WLD_ARRAY_SIZE(buffer2));
    assert_int_equal(outSize, 5);

    assert_true(convStrToIntArray(buffer1, WLD_ARRAY_SIZE(buffer1), "0,2,4,6", NULL));
    assert_memory_equal(buffer1, test1, WLD_ARRAY_SIZE(test1));

    assert_true(convStrToIntArray(buffer1, WLD_ARRAY_SIZE(buffer1), "-10,5,-8,3,-6,1", NULL));
    assert_memory_equal(buffer1, test2, WLD_ARRAY_SIZE(test2));

    assert_false(convStrToIntArray(buffer2, WLD_ARRAY_SIZE(buffer2), "-10,5,-8,3,-6,1", NULL));
    assert_memory_equal(buffer1, test2, WLD_ARRAY_SIZE(buffer2));

    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer3, 0, sizeof(buffer3));

    outSize = 999;
    assert_false(convStrToIntArray(buffer1, 0, "-10,5,-8,3,-6,1", &outSize));
    assert_memory_equal(buffer1, buffer3, sizeof(buffer1));
    assert_int_equal(outSize, 0);

    assert_false(convStrToIntArray(buffer1, 2, "-10,5,-8,3,-6,1", &outSize));
    buffer3[0] = -10;
    buffer3[1] = 5;
    assert_memory_equal(buffer1, buffer3, sizeof(buffer1));
    assert_int_equal(outSize, 2);

    outSize = 999;
    assert_false(convStrToIntArray(NULL, WLD_ARRAY_SIZE(buffer1), "-10,5,-8,3,-6,1", &outSize));
    assert_int_equal(outSize, 0);

    outSize = 999;
    assert_false(convStrToIntArray(buffer1, WLD_ARRAY_SIZE(buffer1), NULL, &outSize));
    assert_int_equal(outSize, 0);
}


static void test_ssid_to_string_ascii(void** state _UNUSED) {
    uint8_t ssid[] = { 'A', 'B', ' ', 'C' };
    char* result = wld_ssid_to_string(ssid, 4);
    assert_string_equal(result, "AB C");
    free(result);
}

static void test_ssid_to_string_hex(void** state _UNUSED) {
    uint8_t ssid[] = { 'A', 3, ' ', 'C' };
    char* result = wld_ssid_to_string(ssid, 4);
    assert_string_equal(result, "41032043");
    free(result);
}

static void test_ssid_to_string_zero_byte(void** state _UNUSED) {
    uint8_t ssid[] = { 0, 'A', 'B', 'C' };
    char* result = wld_ssid_to_string(ssid, 4);
    assert_string_equal(result, "00414243");
    free(result);
}

static void test_wldu_convStrToNum(void** state _UNUSED) {
    int8_t ch = 0;
    int16_t shrt = 0;
    int32_t lng = 0;
    int64_t llng = 0;
    int ret;
    char buf[256] = {0};

    snprintf(buf, sizeof(buf), "%hhx", CHAR_MAX);

    //parse of hex string: KO: using base 0 without 0x prefix
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 0, true);
    assert_int_equal(ret, WLD_ERROR);
    //parse of hex string: OK: using base 16
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 16, true);
    assert_int_equal(ret, WLD_OK);
    assert_int_equal(ch, CHAR_MAX);

    snprintf(buf, sizeof(buf), "%hhd", CHAR_MIN);
    //parse negative char value: OK: using base 0
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 0, true);
    assert_int_equal(ret, WLD_OK);
    assert_int_equal(ch, CHAR_MIN);

    snprintf(buf, sizeof(buf), "0x%hhx", UCHAR_MAX);
    //parse unsigned char max value into a signed char variable: KO: range overflow
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 0, true);
    assert_int_equal(ret, WLD_ERROR);
    //parse unsigned char max value into an unsigned char variable: OK
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 0, false);
    assert_int_equal(ret, WLD_OK);
    assert_int_equal((uint8_t) ch, UCHAR_MAX);

    snprintf(buf, sizeof(buf), "%hd", SHRT_MIN);

    //parse short min value into an unsigned char variable: KO: range overflow
    ret = wldu_convStrToNum(buf, &ch, sizeof(ch), 0, true);
    assert_int_equal(ret, WLD_ERROR);
    //parse short min value into a signed short variable: OK
    ret = wldu_convStrToNum(buf, &shrt, sizeof(shrt), 0, true);
    assert_int_equal(ret, WLD_OK);
    assert_int_equal(shrt, SHRT_MIN);

    snprintf(buf, sizeof(buf), "%hx", USHRT_MAX);

    //parse unsigned short max value into a signed short variable: KO: range overflow
    ret = wldu_convStrToNum(buf, &shrt, sizeof(shrt), 16, true);
    assert_int_equal(ret, WLD_ERROR);
    //parse unsigned short max value into a unsigned short variable: OK
    ret = wldu_convStrToNum(buf, &shrt, sizeof(shrt), 16, false);
    assert_int_equal(ret, WLD_OK);
    assert_int_equal((uint16_t) shrt, USHRT_MAX);

    snprintf(buf, sizeof(buf), "%lx", LONG_MAX);
    //parse long max value into a long variable: OK
    //NB: LONG_MAX and LONG_MIN values depend on target word size
    ret = wldu_convStrToNum(buf, &lng, sizeof(lng), 16, true);
    assert_int_equal(ret, WLD_OK);
    assert_true(lng == (int32_t) LONG_MAX);

    snprintf(buf, sizeof(buf), "%llx", LLONG_MAX);

    //parse long long max value into a long long variable: OK
    ret = wldu_convStrToNum(buf, &llng, sizeof(llng), 16, true);
    assert_int_equal(ret, WLD_OK);
    assert_true(llng == LLONG_MAX);

    snprintf(buf, sizeof(buf), "%llx", ULLONG_MAX);
    //parse unsigned long long max value into a signed long long variable: KO: range overflow
    ret = wldu_convStrToNum(buf, &llng, sizeof(llng), 16, true);
    assert_int_equal(ret, WLD_ERROR);
    //parse unsigned long long max value into unsigned long long variable: OK
    ret = wldu_convStrToNum(buf, &llng, sizeof(llng), 16, false);
    assert_int_equal(ret, WLD_OK);
    assert_true((uint64_t) llng == ULLONG_MAX);
}

static void test_wldu_parseHexToUint64(void** state _UNUSED) {
    assert_true(wldu_parseHexToUint64("000000000000a1b2") == 41394LL);
    assert_true(wldu_parseHexToUint64("0000000000001234") == 4660);
    assert_true(wldu_parseHexToUint64("000000000000000f") == 15);
    assert_true(wldu_parseHexToUint64("00000000000000f0") == 240);
    assert_true(wldu_parseHexToUint64("0000000000000f00") == 3840);
    assert_true(wldu_parseHexToUint64("000000000000f000") == 61440);
    assert_true(wldu_parseHexToUint64("00000000ffffffff") == 4294967295);
    assert_true(wldu_parseHexToUint64("0000000200000000") == 1LL << 33);
    assert_true(wldu_parseHexToUint64("ffffffffffffffff") == (uint64_t) -1);
    assert_true(wldu_parseHexToUint64("fffffffffffffff") == 0);
    assert_true(wldu_parseHexToUint64("f") == 0);
    assert_true(wldu_parseHexToUint64("") == 0);
}

static void test_wldu_parseHexToUint32(void** state _UNUSED) {
    assert_int_equal(wldu_parseHexToUint32("0000a1b2"), 41394);
    assert_int_equal(wldu_parseHexToUint32("00001234"), 4660);
    assert_int_equal(wldu_parseHexToUint32("0000000f"), 15);
    assert_int_equal(wldu_parseHexToUint32("000000f0"), 240);
    assert_int_equal(wldu_parseHexToUint32("00000f00"), 3840);
    assert_int_equal(wldu_parseHexToUint32("0000f000"), 61440);
    assert_int_equal(wldu_parseHexToUint32("0000ffff"), 65535);
    assert_int_equal(wldu_parseHexToUint32("ffffffff"), 4294967295);
    assert_int_equal(wldu_parseHexToUint32("fffffff"), 0);
    assert_int_equal(wldu_parseHexToUint32("ffffff"), 0);
    assert_int_equal(wldu_parseHexToUint32("fffff"), 0);
    assert_int_equal(wldu_parseHexToUint32("ffff"), 0);
    assert_int_equal(wldu_parseHexToUint32("fff"), 0);
    assert_int_equal(wldu_parseHexToUint32("ff"), 0);
    assert_int_equal(wldu_parseHexToUint32("f"), 0);
    assert_int_equal(wldu_parseHexToUint32(""), 0);
}

static void test_wldu_parseHexToUint16(void** state _UNUSED) {
    assert_int_equal(wldu_parseHexToUint16("a1b2"), 41394);
    assert_int_equal(wldu_parseHexToUint16("1234"), 4660);
    assert_int_equal(wldu_parseHexToUint16("000f"), 15);
    assert_int_equal(wldu_parseHexToUint16("00f0"), 240);
    assert_int_equal(wldu_parseHexToUint16("0f00"), 3840);
    assert_int_equal(wldu_parseHexToUint16("f000"), 61440);
    assert_int_equal(wldu_parseHexToUint16("ffffffff"), 65535);
    assert_int_equal(wldu_parseHexToUint16("ffff"), 65535);
    assert_int_equal(wldu_parseHexToUint16("fff"), 0);
    assert_int_equal(wldu_parseHexToUint16("ff"), 0);
    assert_int_equal(wldu_parseHexToUint16("f"), 0);
    assert_int_equal(wldu_parseHexToUint16(""), 0);
}


static void test_wldu_parseHexToUint8(void** state _UNUSED) {
    assert_int_equal(wldu_parsehexToUint8("11"), 17);
    assert_int_equal(wldu_parsehexToUint8("ffff"), 255);
    assert_int_equal(wldu_parsehexToUint8("ff"), 255);
    assert_int_equal(wldu_parsehexToUint8("0f"), 15);
    assert_int_equal(wldu_parsehexToUint8("f0"), 240);
    assert_int_equal(wldu_parsehexToUint8("f"), 0);
    assert_int_equal(wldu_parsehexToUint8(""), 0);
}

static void test_convStr2Hex(void** state _UNUSED) {
    char buffer[64];
    int size = sizeof(buffer);
    char* test1 = "000102030405060708090a0b0c0d0e0f";
    uint8_t test1Result[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    int resultSize = convStr2Hex(test1, strlen(test1), buffer, size);
    assert_int_equal(16, resultSize);
    assert_memory_equal(buffer, test1Result, 16);

    char* test2 = "102030405060708090a0b0c0d0e0f0";
    uint8_t test2Result[15] = {16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240};
    resultSize = convStr2Hex(test2, strlen(test2), buffer, size);
    assert_int_equal(15, resultSize);
    assert_memory_equal(buffer, test2Result, 15);

    char* test3 = "0112233445566778899aabbccddeeffedcba9876543210f";
    uint8_t test3Result[23] = {1, 18, 35, 52, 69, 86, 103, 120, 137, 154, 171, 188, 205, 222, 239, 254, 220, 186, 152, 118, 84, 50, 16};
    resultSize = convStr2Hex(test3, strlen(test3), buffer, size);
    assert_int_equal(23, resultSize);
    assert_memory_equal(buffer, test3Result, 23);
}

static void test_wldu_strStartsWith(void** state _UNUSED) {
    assert_true(wldu_strStartsWith("abc", "ab"));
    assert_true(wldu_strStartsWith("1234abcdef", "1234"));


    assert_false(wldu_strStartsWith("ab", "abc"));
    assert_false(wldu_strStartsWith("abcd", "dcba"));
    assert_false(wldu_strStartsWith("abcd", "bcd"));
}


static void test_isValidSSID(void** state _UNUSED) {
    assert_false(isValidSSID(NULL));
    assert_false(isValidSSID(""));

    //32 char allowed, 33 not
    assert_false(isValidSSID("123456789012345678901234567890123"));
    assert_true(isValidSSID("12345678901234567890123456789012"));

    assert_true(isValidSSID("test1234!#_"));

    assert_true(isValidSSID("abcdefghijklmnopqrstuvwxyz"));
    assert_true(isValidSSID("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    assert_true(isValidSSID("1234567890"));
    assert_true(isValidSSID("@^`,|%;.~(){}:?=-+_#!"));

    //can now have all special chars
    assert_true(isValidSSID("!test1234!#$"));
    assert_true(isValidSSID("#test1234!#$"));
    assert_true(isValidSSID(";test1234!#$"));

    assert_true(isValidSSID("test_\""));
    assert_true(isValidSSID("test_\\"));
    assert_true(isValidSSID("test_$"));
    assert_true(isValidSSID("test_["));
    assert_true(isValidSSID("test_]"));
}

static void test_wldu_convStrToUInt8Arr(void** state _UNUSED) {
    uint8_t buffer[128];
    /* 2.4Ghz channel list */
    char* test1 = "1,2,3,4,5,6,7,8,9,10,11,12,13";
    uint8_t test1Result[13] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    int resultSize = wldu_convStrToUInt8Arr(buffer, 13, test1);
    assert_int_equal(13, resultSize);
    assert_memory_equal(buffer, test1Result, 13);

    /*5Ghz channel list */
    char* test2 = "36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140";
    uint8_t test2Result[19] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
    resultSize = wldu_convStrToUInt8Arr(buffer, 19, test2);
    assert_int_equal(19, resultSize);
    assert_memory_equal(buffer, test2Result, 19);

    /*Random number check */
    char* test3 = "36,40,41,17,52,56,8,100,104";
    uint8_t test3Result[9] = {36, 40, 41, 17, 52, 56, 8, 100, 104};
    resultSize = wldu_convStrToUInt8Arr(buffer, 9, test3);
    assert_int_equal(9, resultSize);
    assert_memory_equal(buffer, test3Result, 9);

    /* All possible channel check wl0 & wl1*/
    char* test4 = "1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140";
    uint8_t test4Result[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
    resultSize = wldu_convStrToUInt8Arr(buffer, 32, test4);
    assert_int_equal(32, resultSize);
    assert_memory_equal(buffer, test4Result, 32);

    /* Out of bound check*/
    char* test5 = "1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,1,2,3,4,5,6,7,8,9,10,11,12,13";
    resultSize = wldu_convStrToUInt8Arr(buffer, 32, test5);
    assert_int_equal(-1, resultSize);

}

static const char* strList[] = {"bla", "bla2", "foo", "bar", "foobar", "barfoo"};

static void test_conv_maskToStrGo(char* input, uint32_t expectedMask, char* expectedOutput) {
    char buffer[128];
    uint32_t mask = conv_strToMask(input, strList, WLD_ARRAY_SIZE(strList));
    assert_int_equal(mask, expectedMask);
    conv_maskToStr(mask, strList, WLD_ARRAY_SIZE(strList), buffer, sizeof(buffer));
    if(expectedOutput == NULL) {
        assert_string_equal(buffer, input);
    } else {
        assert_string_equal(buffer, expectedOutput);
    }

}

static void test_conv_maskToStr(void** state _UNUSED) {
    test_conv_maskToStrGo("bla,foobar", 0x11, NULL);
    test_conv_maskToStrGo("bla", 0x1, NULL);
    test_conv_maskToStrGo("barfoo", 0x20, NULL);
    test_conv_maskToStrGo("bla,bla2,foo,bar,foobar,barfoo", 0x3f, NULL);
    test_conv_maskToStrGo("abla", 0x0, "");
    test_conv_maskToStrGo("abla,rabarblafoo,foo,foobarbar", 0x4, "foo");
}

static void test_conv_strToMaskGo(uint32_t input, char* expectedChar, uint32_t output) {
    char buffer[128];
    conv_maskToStr(input, strList, WLD_ARRAY_SIZE(strList), buffer, sizeof(buffer));
    assert_string_equal(buffer, expectedChar);
    uint32_t mask = conv_strToMask(buffer, strList, WLD_ARRAY_SIZE(strList));
    assert_int_equal(mask, output);

}

static void test_conv_strToMask(void** state _UNUSED) {
    test_conv_strToMaskGo(0x1, "bla", 0x1);
    test_conv_strToMaskGo(0x25, "bla,foo,barfoo", 0x25);
    test_conv_strToMaskGo(0x3f, "bla,bla2,foo,bar,foobar,barfoo", 0x3f);
    test_conv_strToMaskGo(0xff, "bla,bla2,foo,bar,foobar,barfoo", 0x3f);
}

static void test_isPower(void** state _UNUSED) {
    assert_true(wld_util_isPowerOf(4, 2));
    assert_true(wld_util_isPowerOf(1024, 2));
    assert_false(wld_util_isPowerOf(678, 2));
    assert_false(wld_util_isPowerOf(1025, 2));
    assert_false(wld_util_isPowerOf(1023, 2));

    assert_true(wld_util_isPowerOf(4, -2));
    assert_true(wld_util_isPowerOf(-8, -2));


    assert_true(wld_util_isPowerOf(25, 5));
    assert_true(wld_util_isPowerOf(125, 5));
    assert_true(wld_util_isPowerOf(625, 5));
    assert_false(wld_util_isPowerOf(624, 5));
    assert_false(wld_util_isPowerOf(626, 5));


    assert_true(wld_util_isPowerOf2(2));
    assert_true(wld_util_isPowerOf2(4));
    assert_true(wld_util_isPowerOf2(1024));
    assert_true(wld_util_isPowerOf2(0x10000000));
    assert_false(wld_util_isPowerOf2(0x10101011));
    assert_false(wld_util_isPowerOf2(0x11010101));
}

static void test_tuple(void** state _UNUSED) {
    enum {
        TEST_TUPLE_UNDEF = -1,
        TEST_TUPLE_1 = 0x01,
        TEST_TUPLE_2 = 0x02,
        TEST_TUPLE_ALL = 0xff,
    };
    wld_tuple_t testTuples[] = {
        WLD_TUPLE(TEST_TUPLE_1, "TUPLE_1"),
        WLD_TUPLE_GEN(TEST_TUPLE_2),
    };
    wld_tuplelist_t testTuplesList = {.tuples = testTuples, WLD_ARRAY_SIZE(testTuples)};

    assert_ptr_equal(wldu_getTupleByStr(&testTuplesList, "TUPLE_1"), &testTuples[0]);
    assert_null(wldu_getTupleByStr(&testTuplesList, "TUPLE_2"));
    assert_ptr_equal(wldu_getTupleById(&testTuplesList, TEST_TUPLE_2), &testTuples[1]);
    assert_null(wldu_getTupleById(&testTuplesList, TEST_TUPLE_ALL));
    assert_string_equal(wldu_tuple_id2str(&testTuplesList, TEST_TUPLE_2, WLD_TUPLE_DEF_STR), "TEST_TUPLE_2");
    assert_string_equal(wldu_tuple_id2str(&testTuplesList, TEST_TUPLE_ALL, WLD_TUPLE_DEF_STR), WLD_TUPLE_DEF_STR);
    assert_int_equal(wldu_tuple_str2id(&testTuplesList, "TUPLE_2", TEST_TUPLE_UNDEF), TEST_TUPLE_UNDEF);
    assert_int_equal(wldu_tuple_str2id(&testTuplesList, "TUPLE_1", TEST_TUPLE_UNDEF), TEST_TUPLE_1);
}

static void test_isValidAesKey(void** state _UNUSED) {
    // key too short
    assert_false(isValidAESKey("8dokxrp", PSK_KEY_SIZE_LEN - 1));
    // key too long, and hex
    assert_false(isValidAESKey("f2ad2a10df6e31ffbe3776388e1df3789d6e0611056d33a57a1815a9ec094d46abab", PSK_KEY_SIZE_LEN - 1));
    // key 64, not hex
    assert_false(isValidAESKey("zzad2a10df6e31ffbe3776388e1df3789d6e0611056d33a57a1815a9ec094dgg", (PSK_KEY_SIZE_LEN - 1)));



    // key 8, not hex
    assert_true(isValidAESKey("8dokxrpr", PSK_KEY_SIZE_LEN - 1));
    // key 63, not hex
    assert_true(isValidAESKey("zzad2a10df6e31ffbe3776388e1df3789d6e0611056d33a57a1815a9ec094dg", PSK_KEY_SIZE_LEN - 1));
    // key 64, hex
    assert_true(isValidAESKey("f2ad2a10df6e31ffbe3776388e1df3789d6e0611056d33a57a1815a9ec094d46", PSK_KEY_SIZE_LEN - 1));
    // weird signs
    assert_true(isValidAESKey("/*-+!@#$%^&*()_{}\"[]<>?", PSK_KEY_SIZE_LEN - 1));
}

static int setup_suite(void** state _UNUSED) {
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    return 0;
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_WARNING);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);
    sahTraceAddZone(sahTraceLevel(), "pcb");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ssid_to_string_ascii),
        cmocka_unit_test(test_ssid_to_string_hex),
        cmocka_unit_test(test_ssid_to_string_zero_byte),
        cmocka_unit_test(test_wldu_convStrToNum),
        cmocka_unit_test(test_wldu_parseHexToUint64),
        cmocka_unit_test(test_wldu_parseHexToUint32),
        cmocka_unit_test(test_wldu_parseHexToUint16),
        cmocka_unit_test(test_wldu_parseHexToUint8),
        cmocka_unit_test(test_convStr2Hex),
        cmocka_unit_test(test_wldu_convStrToUInt8Arr),
        cmocka_unit_test(test_wldu_strStartsWith),
        cmocka_unit_test(test_isValidSSID),
        cmocka_unit_test(test_conv_maskToStr),
        cmocka_unit_test(test_conv_strToMask),
        cmocka_unit_test(test_isPower),
        cmocka_unit_test(test_tuple),
        cmocka_unit_test(test_isValidAesKey),
        cmocka_unit_test(test_convIntArrToString),
        cmocka_unit_test(test_convStrToIntArray),
    };
    int rc = cmocka_run_group_tests(tests, setup_suite, teardown_suite);
    sahTraceClose();
    return rc;
}
