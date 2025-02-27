/*
 *  Copyright (c) 2023 Rockchip Corporation
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

#include "rk_aiq_algo_camgroup_types.h"
#include "newStruct/cnr/include/cnr_algo_api.h"

#define DEFAULT_RECALCULATE_DELTA_ISO (0.01)

typedef struct {
    cnr_api_attrib_t* cnr_attrib;
    int iso;
    bool isReCal_;
} CnrGroupContext_t;

static XCamReturn groupCnrCreateCtx(RkAiqAlgoContext **context, const AlgoCtxInstanceCfg* cfg)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_ANR("%s enter \n", __FUNCTION__ );
    CnrGroupContext_t *ctx = (CnrGroupContext_t *)malloc(sizeof(CnrGroupContext_t));

    AlgoCtxInstanceCfgCamGroup *cfgInt = (AlgoCtxInstanceCfgCamGroup*)cfg;
    void *pCalibdbV2 = (void *)cfgInt->s_calibv2;
    cnr_api_attrib_t *attr = (cnr_api_attrib_t*)(CALIBDBV2_GET_MODULE_PTR(pCalibdbV2, cnr));

    *context = (RkAiqAlgoContext*)ctx;
    LOGD_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;

}

static XCamReturn groupCnrDestroyCtx(RkAiqAlgoContext *context)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_ANR("%s enter \n", __FUNCTION__ );

    CnrGroupContext_t* pCnrGroupCtx = (CnrGroupContext_t*)context;
    free(pCnrGroupCtx);

    LOGD_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupCnrPrepare(RkAiqAlgoCom* params)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_ANR("%s enter \n", __FUNCTION__ );

    CnrGroupContext_t *pCnrGroupCtx = (CnrGroupContext_t *)params->ctx;
    void *pCalibdbV2 = (void*)(params->u.prepare.calibv2 );
    cnr_api_attrib_t *attr = (cnr_api_attrib_t*)(CALIBDBV2_GET_MODULE_PTR(pCalibdbV2, cnr));

	if(!!(params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB )){
        // just update calib ptr
        if (params->u.prepare.conf_type & RK_AIQ_ALGO_CONFTYPE_UPDATECALIB_PTR) {
            pCnrGroupCtx->cnr_attrib = attr;
            return XCAM_RETURN_NO_ERROR;
        }
    }

    pCnrGroupCtx->cnr_attrib = attr;
    pCnrGroupCtx->isReCal_ = true;

    LOGD_ANR("%s exit ret:%d\n", __FUNCTION__, ret);
    return ret;
}

static XCamReturn groupCnrProcessing(const RkAiqAlgoCom* inparams, RkAiqAlgoResCom* outparams)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_ANR("%s enter", __FUNCTION__ );

    CnrGroupContext_t* pCnrGroupCtx = (CnrGroupContext_t *)inparams->ctx;
    RkAiqAlgoCamGroupProcIn* procParaGroup = (RkAiqAlgoCamGroupProcIn*)inparams;
    RkAiqAlgoCamGroupProcOut* procResParaGroup = (RkAiqAlgoCamGroupProcOut*)outparams;

    //group empty
    if(procParaGroup == nullptr || procParaGroup->camgroupParmasArray == nullptr) {
        LOGE_ANR("procParaGroup or camgroupParmasArray is null");
        return(XCAM_RETURN_ERROR_FAILED);
    }

    // skip group algo if in MANUAL mode
    if (pCnrGroupCtx->cnr_attrib->opMode != RK_AIQ_OP_MODE_AUTO) {
        return XCAM_RETURN_NO_ERROR;
    }

    int iso = pCnrGroupCtx->iso;
    float blc_ob_predgain = procParaGroup->stAblcV32_proc_res.isp_ob_predgain;
    rk_aiq_singlecam_3a_result_t* scam_3a_res = procParaGroup->camgroupParmasArray[0];
    if(scam_3a_res->aec._bEffAecExpValid) {
        RKAiqAecExpInfo_t* pCurExp = &scam_3a_res->aec._effAecExpInfo;
        if((rk_aiq_working_mode_t)procParaGroup->working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            iso =  blc_ob_predgain * scam_3a_res->hdrIso;
        } else {
            iso = scam_3a_res->hdrIso;
        }
    }

    if (procParaGroup->attribUpdated) {
        LOGI("%s attribUpdated", __func__);
        pCnrGroupCtx->isReCal_ = true;
    }

    int delta_iso = abs(iso - pCnrGroupCtx->iso);
    if (delta_iso > DEFAULT_RECALCULATE_DELTA_ISO)
        pCnrGroupCtx->isReCal_ = true;

    rk_aiq_isp_cnr_params_t *cnr_param = procResParaGroup->camgroupParmasArray[0]->cnr;

    if (pCnrGroupCtx->isReCal_) {
        CnrSelectParam(&pCnrGroupCtx->cnr_attrib->stAuto, &cnr_param->result, iso);

        cnr_param->is_update = true;
        cnr_param->en = pCnrGroupCtx->cnr_attrib->en;
        cnr_param->bypass = pCnrGroupCtx->cnr_attrib->bypass;
        LOGD_ANR("group delta_iso:%d, iso %d, cnr en:%d, bypass:%d", delta_iso, iso, cnr_param->en, cnr_param->bypass);

        // copy cam0's result to other cams
        for (int i = 1; i < procResParaGroup->arraySize; i++) {
            procResParaGroup->camgroupParmasArray[i]->cnr->result = cnr_param->result;
            procResParaGroup->camgroupParmasArray[i]->cnr->is_update = true;
            procResParaGroup->camgroupParmasArray[i]->cnr->en = cnr_param->en;
            procResParaGroup->camgroupParmasArray[i]->cnr->bypass = cnr_param->bypass;
        }
    } else {
        for (int i = 0; i < procResParaGroup->arraySize; i++) {
            procResParaGroup->camgroupParmasArray[i]->cnr->is_update = false;
        }
    }

    pCnrGroupCtx->iso = iso;
    pCnrGroupCtx->isReCal_ = false;

    LOGD_ANR("%s exit\n", __FUNCTION__);
    return ret;
}

#define RKISP_ALGO_CAMGROUP_CNR_VERSION     "v0.0.1"
#define RKISP_ALGO_CAMGROUP_CNR_VENDOR      "Rockchip"
#define RKISP_ALGO_CAMGROUP_CNR_DESCRIPTION "Rockchip Cnr camgroup algo for ISP2.0"

RkAiqAlgoDescription g_RkIspAlgoDescCamgroupCnr = {
    .common = {
        .version = RKISP_ALGO_CAMGROUP_CNR_VERSION,
        .vendor  = RKISP_ALGO_CAMGROUP_CNR_VENDOR,
        .description = RKISP_ALGO_CAMGROUP_CNR_DESCRIPTION,
        .type    = RK_AIQ_ALGO_TYPE_ACNR,
        .id      = 0,
        .create_context  = groupCnrCreateCtx,
        .destroy_context = groupCnrDestroyCtx,
    },
    .prepare = groupCnrPrepare,
    .pre_process = NULL,
    .processing = groupCnrProcessing,
    .post_process = NULL,
};
