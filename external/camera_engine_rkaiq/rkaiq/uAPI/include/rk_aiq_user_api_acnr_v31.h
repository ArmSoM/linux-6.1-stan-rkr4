/*
 *  Copyright (c) 2024 Rockchip Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RK_AIQ_USER_API_ACNRV31_H_
#define _RK_AIQ_USER_API_ACNRV31_H_

#include "acnrV31/rk_aiq_uapi_acnr_int_v31.h"

typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;

RKAIQ_BEGIN_DECLARE

XCamReturn
rk_aiq_user_api_acnrV31_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, const rk_aiq_cnr_attrib_v31_t* attr);

XCamReturn
rk_aiq_user_api_acnrV31_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_cnr_attrib_v31_t* attr);

XCamReturn
rk_aiq_user_api_acnrV31_SetStrength(const rk_aiq_sys_ctx_t* sys_ctx, const rk_aiq_cnr_strength_v31_t *pStrength);

XCamReturn
rk_aiq_user_api_acnrV31_GetStrength(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_cnr_strength_v31_t *pStrength);

XCamReturn
rk_aiq_user_api_acnrV31_GetInfo(const rk_aiq_sys_ctx_t* sys_ctx, rk_aiq_cnr_info_v31_t *pInfo);

RKAIQ_END_DECLARE

#endif
