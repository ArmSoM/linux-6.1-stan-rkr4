/*
 *  Copyright (c) 2019 Rockchip Corporation
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

#ifndef _RK_AIQ_USER_API2_ACCM_H_
#define _RK_AIQ_USER_API2_ACCM_H_
#include "accm/rk_aiq_uapi_accm_int.h"

#ifndef RK_AIQ_SYS_CTX_T
#define RK_AIQ_SYS_CTX_T
typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;
#endif

RKAIQ_BEGIN_DECLARE

XCamReturn rk_aiq_user_api2_accm_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           const rk_aiq_ccm_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           rk_aiq_ccm_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_SetIqParam(const rk_aiq_sys_ctx_t* sys_ctx,
                                           const rk_aiq_ccm_calib_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_GetIqParam(const rk_aiq_sys_ctx_t* sys_ctx,
                                           rk_aiq_ccm_calib_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_v2_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           const rk_aiq_ccm_v2_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_v2_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           rk_aiq_ccm_v2_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_v2_SetIqParam(const rk_aiq_sys_ctx_t* sys_ctx,
                                           const rk_aiq_ccm_v2_calib_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_v2_GetIqParam(const rk_aiq_sys_ctx_t* sys_ctx,
                                           rk_aiq_ccm_v2_calib_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_QueryCcmInfo(const rk_aiq_sys_ctx_t* sys_ctx,
                                              rk_aiq_ccm_querry_info_t* ccm_querry_info);
XCamReturn rk_aiq_user_api2_GetAcolorSwInfo(const rk_aiq_sys_ctx_t* sys_ctx,
                                              rk_aiq_color_info_t* aColor_sw_info);
XCamReturn rk_aiq_user_api2_accm_SetAcolorSwInfo(const rk_aiq_sys_ctx_t* sys_ctx,
                                              rk_aiq_color_info_t aColor_sw_info);
XCamReturn rk_aiq_user_api2_accm_v3_SetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           const rk_aiq_ccm_v3_attrib_t* attr);
XCamReturn rk_aiq_user_api2_accm_v3_GetAttrib(const rk_aiq_sys_ctx_t* sys_ctx,
                                           rk_aiq_ccm_v3_attrib_t* attr);

RKAIQ_END_DECLARE

#endif
