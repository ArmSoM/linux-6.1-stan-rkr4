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

#include "sample_comm.h"


// #define USE_GAMMA_NEWSTRUCT
#ifdef USE_GAMMA_NEWSTRUCT
#include "uAPI2/rk_aiq_user_api2_gamma.h"
#include "isp/rk_aiq_isp_gamma21.h"
#else
#include "uAPI2/rk_aiq_user_api2_agamma.h"
#endif
#include "uAPI2/rk_aiq_user_api2_helper.h"
#include <string>

#define LIMIT_VALUE(value,max_value,min_value)      (value > max_value? max_value : value < min_value ? min_value : value)


static void sample_agamma_usage()
{
    printf("Usage : \n");
    printf("\t 0) AGAMMA:         test rk_aiq_user_api2_agamma_v10_SetAttrib stManual Sync.\n");
    printf("\t 1) AGAMMA:         test rk_aiq_user_api2_agamma_v10_SetAttrib stManual Async.\n");
    printf("\t 2) AGAMMA:         test rk_aiq_user_api2_agamma_v10_GetAttrib.\n");
    printf("\t 3) AGAMMA:         test rk_aiq_uapi2_setGammaCoef.\n");
    printf("\t 4) AGAMMA:         test rk_aiq_user_api2_agamma_v11_SetAttrib stManual Sync.\n");
    printf("\t 5) AGAMMA:         test rk_aiq_user_api2_agamma_v11_SetAttrib stManual Async.\n");
    printf("\t 6) AGAMMA:         test rk_aiq_user_api2_agamma_v11_GetAttrib.\n");
    printf("\t 7) AGAMMA:         test rk_aiq_user_api2_agamma_SetAttrib stManual Sync.\n");
    printf("\t 8) AGAMMA:         test rk_aiq_user_api2_agamma_SetAttrib stManual Async.\n");
    printf("\t 9) AGAMMA:         test rk_aiq_user_api2_agamma_SetAttrib stFast Sync.\n");
    printf("\t a) AGAMMA:         test rk_aiq_user_api2_agamma_SetAttrib stFast Async.\n");
    printf("\t b) AGAMMA:         test rk_aiq_user_api2_agamma_GetAttrib.\n");
    printf("\t c) AGAMMA:         test rk_aiq_user_api2_gamma_GetAttrib.\n");
    printf("\t d) AGAMMA:         test rk_aiq_user_api2_gamma_SetAttrib manual.\n");
    printf("\t e) AGAMMA:         test rk_aiq_user_api2_gamma_SetAttrib auto.\n");
    printf("\t f) AGAMMA:         test rk_aiq_user_api2_gamma_QueryStatus.\n");

    printf("\n");
    printf("\t please press the key: ");

    return;
}

void sample_print_agamma_info(const void *arg)
{
    printf ("enter AGAMMA modult test!\n");
}

static void sample_agamma_test(const rk_aiq_sys_ctx_t* ctx) {

    printf("sample_agamma_test\n");
    rk_aiq_gamma_v11_attr_t attr_v11;
    memset(&attr_v11, 0x0, sizeof(rk_aiq_gamma_v11_attr_t));
    rk_aiq_user_api2_agamma_v11_GetAttrib(ctx, &attr_v11);

    // attr_v11.mode = RK_AIQ_GAMMA_MODE_MANUAL;
    // attr_v11.stManual.Gamma_out_offset +=20;
    // for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
    //     attr_v11.stManual.Gamma_curve[i] += 1;
    // }

    attr_v11.stAuto.GammaTuningPara.Gamma_out_offset +=20;
    for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
        attr_v11.stAuto.GammaTuningPara.Gamma_curve[i] += 1;
    }

    rk_aiq_user_api2_agamma_v11_SetAttrib(ctx, &attr_v11);
}

static void sample_gamma_tuningtool_test(const rk_aiq_sys_ctx_t* ctx)
{
    char *ret_str = NULL;

    printf(">>> start tuning tool test: op attrib get ...\n");

    std::string json_gamma_status_str = " \n\
        [{ \n\
            \"op\":\"get\", \n\
            \"path\": \"/uapi/0/gamma_uapi/info\", \n\
            \"value\": \n\
            { \"opMode\": \"RK_AIQ_OP_MODE_MANUAL\", \"en\": 0,\"bypass\": 3} \n\
        }]";

    rkaiq_uapi_unified_ctl(const_cast<rk_aiq_sys_ctx_t*>(ctx),
                           const_cast<char*>(json_gamma_status_str.c_str()), &ret_str, RKAIQUAPI_OPMODE_GET);

    if (ret_str) {
        printf("gamma status json str: %s\n", ret_str);
    }

    printf("  start tuning tool test: op attrib set ...\n");
    std::string json_gamma_str = " \n\
        [{ \n\
            \"op\":\"replace\", \n\
            \"path\": \"/uapi/0/gamma_uapi/attr\", \n\
            \"value\": \n\
            { \"opMode\": \"RK_AIQ_OP_MODE_MANUAL\", \"en\": 1,\"bypass\": 1} \n\
        }]";
    printf("gamma json_cmd_str: %s\n", json_gamma_str.c_str());
    ret_str = NULL;
    rkaiq_uapi_unified_ctl(const_cast<rk_aiq_sys_ctx_t*>(ctx),
                           const_cast<char*>(json_gamma_str.c_str()), &ret_str, RKAIQUAPI_OPMODE_SET);

    // wait more than 2 frames
    usleep(90 * 1000);

#ifdef USE_GAMMA_NEWSTRUCT
    gamma_status_t status;
    memset(&status, 0, sizeof(gamma_status_t));

    rk_aiq_user_api2_gamma_QueryStatus(ctx, &status);

    if (status.opMode != RK_AIQ_OP_MODE_MANUAL || status.en != 1 || status.bypass != 1) {
        printf("gamma op set_attrib failed !\n");
        printf("gamma status: opmode:%d(EXP:%d), en:%d(EXP:%d), bypass:%d(EXP:%d)\n",
               status.opMode, RK_AIQ_OP_MODE_MANUAL, status.en, 1, status.bypass, 1);
    } else {
        printf("gamma op set_attrib success !\n");
    }
#endif

    printf(">>> tuning tool test done \n");
}

XCamReturn sample_agamma_module(const void* arg)
{
    int key = -1;
    CLEAR();

#ifdef USE_GAMMA_NEWSTRUCT
    gamma_api_attrib_t attr21;
    gamma_status_t status;
#endif
    rk_aiq_gamma_v10_attr_t attr_v10;
    rk_aiq_gamma_v11_attr_t attr_v11;
    rk_aiq_gamma_attrib_V2_t attr_v2;
    float X_v10[CALIBDB_AGAMMA_KNOTS_NUM_V10] = {
        0.0f,    1.0f,    2.0f,    3.0f,    4.0f,    5.0f,    6.0f,    7.0f,    8.0f,
        10.0f,   12.0f,   14.0f,   16.0f,   20.0f,   24.0f,   28.0f,   32.0f,   40.0f,
        48.0f,   56.0f,   64.0f,   80.0f,   96.0f,   112.0f,  128.0f,  160.0f,  192.0f,
        224.0f,  256.0f,  320.0f,  384.0f,  448.0f,  512.0f,  640.0f,  768.0f,  896.0f,
        1024.0f, 1280.0f, 1536.0f, 1792.0f, 2048.0f, 2560.0f, 3072.0f, 3584.0f, 4095.0f};
    float Y_v10                               = 0.0f;
    float X_v11[CALIBDB_AGAMMA_KNOTS_NUM_V11] = {
        0.0f,    1.0f,    2.0f,    3.0f,    4.0f,    5.0f,    6.0f,    7.0f,    8.0f,    10.0f,
        12.0f,   14.0f,   16.0f,   20.0f,   24.0f,   28.0f,   32.0f,   40.0f,   48.0f,   56.0f,
        64.0f,   80.0f,   96.0f,   112.0f,  128.0f,  160.0f,  192.0f,  224.0f,  256.0f,  320.0f,
        384.0f,  448.0f,  512.0f,  640.0f,  768.0f,  896.0f,  1024.0f, 1280.0f, 1536.0f, 1792.0f,
        2048.0f, 2304.0f, 2560.0f, 2816.0f, 3072.0f, 3328.0f, 3584.0f, 3840.0f, 4095.0f};
    float Y_v11 = 0.0f;

    const demo_context_t *demo_ctx = (demo_context_t *)arg;
    const rk_aiq_sys_ctx_t* ctx ;
    if (demo_ctx->camGroup) {
        ctx = (rk_aiq_sys_ctx_t*)(demo_ctx->camgroup_ctx);
    } else {
        ctx = (rk_aiq_sys_ctx_t*)(demo_ctx->aiq_ctx);
    }

    do {
        sample_agamma_usage ();

        key = getchar ();
        while (key == '\n' || key == '\r')
            key = getchar();
        printf ("\n");


        switch (key)
        {
        case '0': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v10_SetAttrib stManual Sync\n\n");
            attr_v10.sync.sync_mode            = RK_AIQ_UAPI_MODE_DEFAULT;
            attr_v10.sync.done                 = false;
            attr_v10.mode                      = RK_AIQ_GAMMA_MODE_MANUAL;
            attr_v10.stManual.Gamma_en         = true;
            attr_v10.stManual.Gamma_out_segnum = GAMMATYPE_LOG;
            attr_v10.stManual.Gamma_out_offset = 0;
            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V10; i++) {
                Y_v10 = 4095.0f * pow(X_v10[i] / 4095.0f, 1.0f / 1.0f + 0.0f);
                Y_v10 = LIMIT_VALUE(Y_v10, 4095.0f, 0.0f);
                attr_v10.stManual.Gamma_curve[i] = (int)(Y_v10 + 0.5f);
            }
            rk_aiq_user_api2_agamma_v10_SetAttrib(ctx, &attr_v10);
            break;
        }
        case '1': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v10_SetAttrib stManual Async\n\n");
            attr_v10.sync.sync_mode            = RK_AIQ_UAPI_MODE_ASYNC;
            attr_v10.sync.done                 = false;
            attr_v10.mode                      = RK_AIQ_GAMMA_MODE_MANUAL;
            attr_v10.stManual.Gamma_en         = true;
            attr_v10.stManual.Gamma_out_segnum = GAMMATYPE_LOG;
            attr_v10.stManual.Gamma_out_offset = 1024;

            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V10; i++) {
                Y_v10 = 4095.0f * pow(X_v10[i] / 4095.0f, 1.0f / 2.0f + 0.0f);
                Y_v10 = LIMIT_VALUE(Y_v10, 4095.0f, 0.0f);
                attr_v10.stManual.Gamma_curve[i] = (int)(Y_v10 + 0.5f);
            }
            rk_aiq_user_api2_agamma_v10_SetAttrib(ctx, &attr_v10);
            break;
        }
        case '2': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v10_GetAttrib\n\n");
            rk_aiq_user_api2_agamma_v10_GetAttrib(ctx, &attr_v10);
            printf("\t sync = %d, done = %d\n", attr_v10.sync.sync_mode, attr_v10.sync.done);
            printf("\t attr_v11.mode:%d stManual.Gamma_en:%d stManual.Gamma_out_offset:%d\n\n",
                   attr_v10.mode, attr_v10.stManual.Gamma_en, attr_v10.stManual.Gamma_out_offset);
            printf("\t stManual.Gamma_curve:%d %d %d %d %d %d\n\n",
                   attr_v10.stManual.Gamma_curve[0], attr_v10.stManual.Gamma_curve[1],
                   attr_v10.stManual.Gamma_curve[2], attr_v10.stManual.Gamma_curve[3],
                   attr_v10.stManual.Gamma_curve[4], attr_v10.stManual.Gamma_curve[5]);
            break;
        }
        case '3': {
            printf("\t AGAMMA test rk_aiq_uapi2_setGammaCoef\n\n");
            rk_aiq_uapi2_setGammaCoef(ctx, 5.0f, 0.0f);
            break;
        }
        case '4': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v11_SetAttrib stManual Sync\n\n");
            attr_v11.sync.sync_mode            = RK_AIQ_UAPI_MODE_DEFAULT;
            attr_v11.sync.done                 = false;
            attr_v11.mode                      = RK_AIQ_GAMMA_MODE_MANUAL;
            attr_v11.stManual.Gamma_en         = true;
            attr_v11.stManual.Gamma_out_offset = 0;
            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
                Y_v11 = 4095.0f * pow(X_v11[i] / 4095.0f, 1.0f / 1.0f + 0.0f);
                Y_v11 = LIMIT_VALUE(Y_v11, 4095.0f, 0.0f);
                attr_v11.stManual.Gamma_curve[i] = (int)(Y_v11 + 0.5f);
            }
            rk_aiq_user_api2_agamma_v11_SetAttrib(ctx, &attr_v11);
            break;
        }
        case '5': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v11_SetAttrib stManual Async\n\n");
            attr_v11.sync.sync_mode            = RK_AIQ_UAPI_MODE_ASYNC;
            attr_v11.sync.done                 = false;
            attr_v11.mode                      = RK_AIQ_GAMMA_MODE_MANUAL;
            attr_v11.stManual.Gamma_en         = true;
            attr_v11.stManual.Gamma_out_offset = 1024;
            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
                Y_v11 = 4095.0f * pow(X_v11[i] / 4095.0f, 1.0f / 2.2f + 0.0f);
                Y_v11 = LIMIT_VALUE(Y_v11, 4095.0f, 0.0f);
                attr_v11.stManual.Gamma_curve[i] = (int)(Y_v11 + 0.5f);
            }
            rk_aiq_user_api2_agamma_v11_SetAttrib(ctx, &attr_v11);
            break;
        }
        case '6': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_v11_GetAttrib\n\n");
            rk_aiq_user_api2_agamma_v11_GetAttrib(ctx, &attr_v11);
            printf("\t sync = %d, done = %d\n", attr_v11.sync.sync_mode, attr_v11.sync.done);
            printf("\t attr_v11.mode:%d stManual.Gamma_en:%d stManual.Gamma_out_offset:%d\n\n",
                   attr_v11.mode, attr_v11.stManual.Gamma_en, attr_v11.stManual.Gamma_out_offset);
            printf("\t stManual.Gamma_curve:%d %d %d %d %d %d\n\n",
                   attr_v11.stManual.Gamma_curve[0], attr_v11.stManual.Gamma_curve[1],
                   attr_v11.stManual.Gamma_curve[2], attr_v11.stManual.Gamma_curve[3],
                   attr_v11.stManual.Gamma_curve[4], attr_v11.stManual.Gamma_curve[5]);
            break;
        }
        case '7': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_SetAttrib stManual Sync\n\n");
            attr_v2.sync.sync_mode                    = RK_AIQ_UAPI_MODE_DEFAULT;
            attr_v2.sync.done                         = false;
            attr_v2.atrrV30.mode                      = GAMMA_MODE_MANUAL;
            attr_v2.atrrV30.stManual.Gamma_en         = true;
            attr_v2.atrrV30.stManual.Gamma_out_offset = 0;
            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
                Y_v11 = 4095.0f * pow(X_v11[i] / 4095.0f, 1.0f / 1.0f + 0.0f);
                Y_v11 = LIMIT_VALUE(Y_v11, 4095.0f, 0.0f);
                attr_v2.atrrV30.stManual.Gamma_curve[i] = (int)(Y_v11 + 0.5f);
            }
            rk_aiq_user_api2_agamma_SetAttrib(ctx, attr_v2);
            break;
        }
        case '8': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_SetAttrib stManual Async\n\n");
            attr_v2.sync.sync_mode                    = RK_AIQ_UAPI_MODE_ASYNC;
            attr_v2.sync.done                         = false;
            attr_v2.atrrV30.mode                      = GAMMA_MODE_MANUAL;
            attr_v2.atrrV30.stManual.Gamma_en         = true;
            attr_v2.atrrV30.stManual.Gamma_out_offset = 1024;
            for (int i = 0; i < CALIBDB_AGAMMA_KNOTS_NUM_V11; i++) {
                Y_v11 = 4095.0f * pow(X_v11[i] / 4095.0f, 1.0f / 2.2f + 0.0f);
                Y_v11 = LIMIT_VALUE(Y_v11, 4095.0f, 0.0f);
                attr_v2.atrrV30.stManual.Gamma_curve[i] = (int)(Y_v11 + 0.5f);
            }
            rk_aiq_user_api2_agamma_SetAttrib(ctx, attr_v2);
            break;
        }
        case '9': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_SetAttrib stFast Sync\n\n");
            attr_v2.sync.sync_mode             = RK_AIQ_UAPI_MODE_DEFAULT;
            attr_v2.sync.done                  = false;
            attr_v2.atrrV30.mode               = GAMMA_MODE_FAST;
            attr_v2.atrrV30.stFast.GammaCoef   = 1.0f;
            attr_v2.atrrV30.stFast.SlopeAtZero = 0.0f;
            rk_aiq_user_api2_agamma_SetAttrib(ctx, attr_v2);
            break;
        }
        case 'a': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_SetAttrib stFast Async\n\n");
            attr_v2.sync.sync_mode             = RK_AIQ_UAPI_MODE_ASYNC;
            attr_v2.sync.done                  = false;
            attr_v2.atrrV30.mode               = GAMMA_MODE_FAST;
            attr_v2.atrrV30.stFast.GammaCoef   = 2.2f;
            attr_v2.atrrV30.stFast.SlopeAtZero = 0.0f;
            rk_aiq_user_api2_agamma_SetAttrib(ctx, attr_v2);
            break;
        }
        case 'b': {
            printf("\t AGAMMA test rk_aiq_user_api2_agamma_GetAttrib\n\n");
            rk_aiq_user_api2_agamma_GetAttrib(ctx, &attr_v2);
            printf("\t sync = %d, done = %d\n", attr_v2.sync.sync_mode, attr_v2.sync.done);
            printf("\t attr_v11.mode:%d stManual.Gamma_en:%d stManual.Gamma_out_offset:%d\n\n",
                   attr_v2.atrrV30.mode, attr_v2.atrrV30.stManual.Gamma_en,
                   attr_v2.atrrV30.stManual.Gamma_out_offset);
            printf("\t stManual.Gamma_curve:%d %d %d %d %d %d\n\n",
                   attr_v2.atrrV30.stManual.Gamma_curve[0], attr_v2.atrrV30.stManual.Gamma_curve[1],
                   attr_v2.atrrV30.stManual.Gamma_curve[2], attr_v2.atrrV30.stManual.Gamma_curve[3],
                   attr_v2.atrrV30.stManual.Gamma_curve[4],
                   attr_v2.atrrV30.stManual.Gamma_curve[5]);
            break;
        }
#ifdef USE_GAMMA_NEWSTRUCT
        case 'c': {
            printf("\t GAMMA test rk_aiq_user_api2_gamma_GetAttrib\n\n");
            rk_aiq_user_api2_gamma_GetAttrib(ctx, &attr21);

            printf("\t attr21.opMode:%d attr21.en:%d attr21.stAuto.sta.Gamma_out_offset:%d\n\n",
                   attr21.opMode, attr21.en,
                   attr21.stAuto.sta.Gamma_out_offset);
            printf("\t attr21.stAuto.sta.EnableDot49:%d attr21.stAuto.sta.equ_segm:%d\n\n",
                   attr21.stAuto.sta.EnableDot49, attr21.stAuto.sta.equ_segm);
            printf("\t stAuto.Gamma_curve:%d %d %d %d \n\n",
                   attr21.stAuto.sta.Gamma_curve[0], attr21.stAuto.sta.Gamma_curve[1],
                   attr21.stAuto.sta.Gamma_curve[2], attr21.stAuto.sta.Gamma_curve[3]);

            printf("\t attr21.opMode:%d attr21.en:%d attr21.stMan.sta.Gamma_out_offset:%d\n\n",
                   attr21.opMode, attr21.en,
                   attr21.stMan.sta.Gamma_out_offset);
            printf("\t attr21.stMan.sta.EnableDot49:%d attr21.stMan.sta.equ_segm:%d\n\n",
                   attr21.stMan.sta.EnableDot49, attr21.stMan.sta.equ_segm);
            printf("\t stManual.Gamma_curve:%d %d %d %d \n\n",
                   attr21.stMan.sta.Gamma_curve[0], attr21.stMan.sta.Gamma_curve[1],
                   attr21.stMan.sta.Gamma_curve[2], attr21.stMan.sta.Gamma_curve[3]);
            break;
        }
        case 'd': {
            printf("\t GAMMA test rk_aiq_user_api2_gamma_SetAttrib manual\n\n");
            attr21.opMode = RK_AIQ_OP_MODE_MANUAL;
            attr21.stMan.sta.Gamma_out_offset += 20;
            for (int i = 0; i < 49; i++) attr21.stMan.sta.Gamma_curve[i]+=1;
            rk_aiq_user_api2_gamma_SetAttrib(ctx, &attr21);
            break;
        }
        case 'e': {
            printf("\t GAMMA test rk_aiq_user_api2_gamma_SetAttrib auto\n\n");
            attr21.opMode = RK_AIQ_OP_MODE_AUTO;
            attr21.stAuto.sta.Gamma_out_offset += 20;
            for (int i = 0; i < 49; i++) attr21.stAuto.sta.Gamma_curve[i]+=1;
            rk_aiq_user_api2_gamma_SetAttrib(ctx, &attr21);
            break;
        }
        case 'f': {
            printf("\t GAMMA test rk_aiq_user_api2_gamma_QueryStatus\n\n");
            rk_aiq_user_api2_gamma_QueryStatus(ctx, &status);
            printf("\t status.opMode:%d status.en:%d status.stMan.sta.Gamma_out_offset:%d\n\n",
                   status.opMode, status.en,
                   status.stMan.sta.Gamma_out_offset);
            printf("\t stManual.Gamma_curve:%d %d %d %d\n\n",
                   status.stMan.sta.Gamma_curve[0], status.stMan.sta.Gamma_curve[1],
                   status.stMan.sta.Gamma_curve[2], status.stMan.sta.Gamma_curve[3]);
            break;
        }
        case 'g': {
            sample_gamma_tuningtool_test(ctx);
            break;
        }
#endif
        case 'h': {
            sample_agamma_test(ctx);
            break;
        }
        default:
            break;
        }
    } while (key != 'q' && key != 'Q');

    return XCAM_RETURN_NO_ERROR;
}
