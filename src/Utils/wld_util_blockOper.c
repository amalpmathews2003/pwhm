/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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

#include <stdlib.h>
#include <string.h>
#include "wld_common.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "Utils/wld_util_blockOper.h"

#define ME "blkOp"

static bool s_isValidBlockerName(const char* name) {
    return !swl_str_isEmpty(name) && swl_str_find(name, ",") < 0;
}

swl_rc_ne wld_util_blockOper_addBlocker(wld_util_blockOper_t* ctx, const char* name) {
    ASSERTS_NOT_NULL(ctx, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(s_isValidBlockerName(name), SWL_RC_INVALID_PARAM, ME, "Invalid blocker name");
    if(!swl_strlst_contains(ctx->blockedBy, ",", name)) {
        swl_strlst_catFormat(ctx->blockedBy, sizeof(ctx->blockedBy), ",", "%s", name);
        ctx->blockerCount++;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_util_blockOper_blockRemove(wld_util_blockOper_t* ctx, const char* name) {
    ASSERTS_NOT_NULL(ctx, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(s_isValidBlockerName(name), SWL_RC_INVALID_PARAM, ME, "Invalid blocker name");
    char original[MAX_BLOCKERS_STR_LEN];
    swl_str_ncopy(original, sizeof(original), ctx->blockedBy, sizeof(ctx->blockedBy));
    char tempBuf[MAX_BLOCKERS_STR_LEN] = { 0 };
    const char* sep = ",";
    char* token = strtok(original, sep);
    bool removed = false;
    while(token != NULL) {
        if(swl_str_matches(token, name)) {
            removed = true;
        } else {
            swl_strlst_cat(tempBuf, sizeof(tempBuf), sep, token);
        }
        token = strtok(NULL, sep);
    }
    if(removed) {
        swl_str_copy(ctx->blockedBy, sizeof(ctx->blockedBy), tempBuf);
        ctx->blockerCount--;
    }
    return (removed ? SWL_RC_OK : SWL_RC_ERROR);
}

bool wld_util_blockOper_hasBlocker(wld_util_blockOper_t* ctx, const char* name) {
    ASSERTS_NOT_NULL(ctx, false, ME, "NULL");
    ASSERTS_FALSE(swl_str_isEmpty(name), false, ME, "Empty name");
    return swl_strlst_contains(ctx->blockedBy, ",", name);
}

bool wld_util_blockOper_hasAnyBlocker(wld_util_blockOper_t* ctx) {
    ASSERTS_NOT_NULL(ctx, false, ME, "NULL");
    return ctx->blockerCount > 0;
}

const char* wld_util_blockOper_getBlockers(wld_util_blockOper_t* ctx) {
    ASSERTS_NOT_NULL(ctx, NULL, ME, "NULL");
    return ctx->blockedBy;
}
