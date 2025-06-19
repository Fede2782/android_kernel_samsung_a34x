// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_trace.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_c3d.h"
#include "mtk_disp_tdshp.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"
#include "mtk_disp_dither.h"
#include "mtk_disp_gamma.h"
#include "mtk_disp_vidle.h"

#define REQUEST_MAX_COUNT 20
#define CHECK_TRIGGER_DELAY 1
#define TUNING_COMPS_MAX_COUNT 15

#if defined(DISP_COLOR_ON)
#define COLOR_MODE			(1)
#elif defined(MDP_COLOR_ON)
#define COLOR_MODE			(2)
#elif defined(DISP_MDP_COLOR_ON)
#define COLOR_MODE			(3)
#else
#define COLOR_MODE			(0)	/*color feature off */
#endif
#define DISP_REG_PQ_PATH_SEL 0x34

struct pq_module_match {
	enum mtk_pq_module_type pq_type;
	enum mtk_ddp_comp_type type;
};

static struct pq_module_match pq_module_matches[MTK_DISP_PQ_TYPE_MAX] = {
	{MTK_DISP_PQ_COLOR, MTK_DISP_COLOR}, // 0
	{MTK_DISP_PQ_DITHER, MTK_DISP_DITHER},
	{MTK_DISP_PQ_CCORR, MTK_DISP_CCORR},
	{MTK_DISP_PQ_AAL, MTK_DISP_AAL},
	{MTK_DISP_PQ_GAMMA, MTK_DISP_GAMMA},

	{MTK_DISP_PQ_CHIST, MTK_DISP_CHIST}, // 5
	{MTK_DISP_PQ_C3D, MTK_DISP_C3D},
	{MTK_DISP_PQ_TDSHP, MTK_DISP_TDSHP},
	{MTK_DISP_PQ_DMR, MTK_DISP_ODDMR},
	{MTK_DISP_PQ_DBI, MTK_DISP_ODDMR},
};

static const char *const mtk_tuning_mdp_comps_name[TUNING_COMPS_MAX_COUNT] = {
	"mediatek,mdp_rsz0",              // 0
	"mediatek,mdp_rsz1",
	"mediatek,mdp_rdma0",
	"mediatek,mdp-tuning-mdp_hdr0",
	"mediatek,mml-tuning-mml_hdr0",
	"mediatek,mdp-tuning-mdp_color0", // 5
	"mediatek,mml-tuning-mml_color0",
	"mediatek,mdp-tuning-mdp_aal0",
	"mediatek,mml-tuning-mml_aal0",
	"mediatek,mdp-tuning-mdp_tdshp0",
	"mediatek,mml-tuning-mml_tdshp0", // 10
	"mediatek,disp_oddmr0",
	"mediatek,disp1_oddmr0",
};

static int disp_pq_proxy_virtual_get_persist_property(struct drm_crtc *crtc, void *data);
static int disp_pq_proxy_virtual_check_trigger(struct drm_crtc *crtc, void *data);

static bool mtk_drm_get_resource_from_dts(struct resource *res, const char *node_name)
{
	int rc = 0;
	struct device_node *node = NULL;

	if (!node_name)
		return false;

	node = of_find_compatible_node(NULL, NULL, node_name);
	rc = of_address_to_resource(node, 0, res);

	// check if fail to get reg.
	if (rc) {
		DDPINFO("Fail to get %s\n", node_name);
		return false;
	}

	DDPDBG("%s REG: 0x%llx ~ 0x%llx\n", node_name, res->start, res->end);
	return true;
}

void disp_pq_path_sel_set(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *handle)
{
#ifndef DRM_BYPASS_PQ
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	unsigned int pq_path_sel = priv->pq_path_sel;
	unsigned int value = 1, addr = 0;
	void __iomem *config_regs = mtk_crtc->config_regs;
	resource_size_t config_regs_pa = mtk_crtc->config_regs_pa;
	void __iomem *side_config_regs = mtk_crtc->side_config_regs;
	resource_size_t side_config_regs_pa = mtk_crtc->side_config_regs_pa;

	switch (priv->data->mmsys_id) {
	case MMSYS_MT6985:
	case MMSYS_MT6989:
	case MMSYS_MT6991:
	case MMSYS_MT6897:
	case MMSYS_MT6899:
		addr = DISP_REG_PQ_PATH_SEL;
		value = pq_path_sel > 0 ? pq_path_sel : 1;
		value = value | value << 8;
		break;
	default:
		break;
	}
	if (!addr)
		return;
	if (handle)
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base, config_regs_pa + DISP_REG_PQ_PATH_SEL, value, ~0);
	else
		writel(value, config_regs + DISP_REG_PQ_PATH_SEL);

	if (mtk_crtc->is_dual_pipe && handle)
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base, side_config_regs_pa + DISP_REG_PQ_PATH_SEL, value, ~0);
	else if (mtk_crtc->is_dual_pipe)
		writel(value, side_config_regs + DISP_REG_PQ_PATH_SEL);
#endif
}

int disp_pq_proxy_virtual_sw_read(struct drm_crtc *crtc, void *data)
{
	struct DISP_READ_REG *rParams = data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int ret = 0;
	unsigned int reg_id = rParams->reg;
	struct resource res;
	int index = drm_crtc_index(crtc);

	switch (reg_id) {
	case SWREG_COLOR_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_COLOR].pa_base;
		break;
	case SWREG_GAMMA_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_GAMMA].pa_base;
		break;
	case SWREG_AAL_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_AAL].pa_base;
		break;
	case SWREG_CCORR_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_CCORR_LINEAR].pa_base;
		break;
	case SWREG_DISP_TDSHP_BASE_ADDRESS:
		ret = pq_data->tuning_pa_table[TUNING_DISP_TDSHP].pa_base;
		break;
	case SWREG_MML_HDR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_hdr0"))
			ret = res.start;
		break;
	case SWREG_MML_AAL_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_aal0"))
			ret = res.start;
		break;
	case SWREG_MML_TDSHP_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_tdshp0"))
			ret = res.start;
		break;
	case SWREG_MML_COLOR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mml-tuning-mml_color0"))
			ret = res.start;
		break;
	case SWREG_TDSHP_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_tdshp0"))
			ret = res.start;
		break;
	case SWREG_MDP_COLOR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_color0"))
			ret = res.start;
		break;
	case SWREG_COLOR_MODE:
		ret = COLOR_MODE;
		break;
	case SWREG_RSZ_BASE_ADDRESS:
#if defined(SUPPORT_ULTRA_RESOLUTION)
		ret = MDP_RSZ0_PA_BASE;
#endif
		break;
	case SWREG_MDP_RDMA_BASE_ADDRESS:
		if (!mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_hdr0") &&
			mtk_drm_get_resource_from_dts(&res, "mediatek,mdp_rdma0"))
			ret = res.start;
		break;
	case SWREG_MDP_AAL_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_aal0"))
			ret = res.start;
		break;
	case SWREG_MDP_HDR_BASE_ADDRESS:
		if (mtk_drm_get_resource_from_dts(&res, "mediatek,mdp-tuning-mdp_hdr0"))
			ret = res.start;
		break;
	case SWREG_TDSHP_TUNING_MODE:
		ret = pq_data->tdshp_flag;
		break;
	case SWREG_MIRAVISION_VERSION:
		ret = MIRAVISION_VERSION;
		break;
	case SWREG_SW_VERSION_VIDEO_DC:
		ret = SW_VERSION_VIDEO_DC;
		break;
	case SWREG_SW_VERSION_AAL:
		ret = SW_VERSION_AAL;
		break;
	default:
		DDPINFO("%s, ret = 0x%08x. unknown reg_id: 0x%08x",
				__func__, ret, reg_id);
	}

	rParams->val = ret;
	DDPDBG("%s, crtc_idx:%d, read sw reg 0x%x = 0x%x",
			__func__, index, rParams->reg, rParams->val);
	return ret;
}

int disp_pq_proxy_virtual_sw_write(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct DISP_WRITE_REG *wParams = data;
	unsigned int reg_id = wParams->reg;
	unsigned int value = wParams->val;

	if (reg_id == SWREG_TDSHP_TUNING_MODE)
		pq_data->tdshp_flag = value;
	return 0;
}

static int disp_pq_get_table_index(struct drm_crtc *crtc, unsigned int pa)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int pa_base = pa & 0xFFFFF000;
	int i;

	for (i = 0; i < TUNING_REG_MAX; i++) {
		if (pq_data->tuning_pa_table[i].pa_base == pa_base)
			return i;
	}
	return -1;
}

static bool disp_pq_tuning_pa_valid(struct drm_crtc *crtc, unsigned int pa)
{
	int i;
	struct resource res;

	if (!pa) {
		DDPPR_ERR("addr is NULL\n");
		return false;
	}
	if ((pa & 0x3) != 0) {
		DDPPR_ERR("addr is not 4-byte aligned!\n");
		return false;
	}
	if (disp_pq_get_table_index(crtc, pa) >= 0)
		return true;

	for (i = 0; i < TUNING_COMPS_MAX_COUNT; i ++) {
		if (mtk_drm_get_resource_from_dts(&res, mtk_tuning_mdp_comps_name[i])) {
			if (pa >= res.start && pa <= res.end)
				return true;
		}
	}
	return false;
}

int disp_pq_proxy_virtual_hw_read(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret = 0;
	struct DISP_READ_REG *rParams = data;
	void __iomem *va = 0;
	unsigned int pa;

	pa = (unsigned int)rParams->reg;

	if (!disp_pq_tuning_pa_valid(crtc, pa)) {
		DDPPR_ERR("reg read, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	va = ioremap(pa, sizeof(*va));

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
	rParams->val = readl(va) & rParams->mask;
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
	DDPINFO("%s, read pa:0x%x(va:0x%lx) = 0x%x (0x%x)\n",
		__func__, pa, (long)va, rParams->val, rParams->mask);

	iounmap(va);
	return ret;
}

static void frame_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = cb_data->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	bool need_wait_done = (bool)cb_data->misc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int index = drm_crtc_index(crtc);

	if (need_wait_done) {
		atomic_set(&pq_data->cfg_done, 1);
		wake_up_interruptible(&pq_data->cfg_done_wq);
		CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)cb_data->cmdq_handle, 0xF000000F);
		DDPINFO("%s cfg_done wakeup\n", __func__);
	}
	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

int disp_pq_proxy_virtual_hw_write(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct DISP_WRITE_REG *wParams = data;
	unsigned int pa = (unsigned int)wParams->reg;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_cmdq_cb_data *cb_data = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;

	if (!disp_pq_tuning_pa_valid(crtc, pa)) {
		DDPPR_ERR("reg write, addr invalid, pa:0x%x\n", pa);
		return -EFAULT;
	}

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (!cmdq_handle) {
		DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
		return -EFAULT;
	}
	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						pa, wParams->val, wParams->mask);

	if (mtk_crtc->is_dual_pipe) {
		unsigned int companion_pa = 0;
		int offset = pa & 0xfff;
		int index_table = disp_pq_get_table_index(crtc, pa);

		if (index_table >= 0) {
			companion_pa = pq_data->tuning_pa_table[index_table].companion_pa_base;
			if (companion_pa)
				companion_pa += offset;
		}
		if (companion_pa) {
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						companion_pa, wParams->val, wParams->mask);
			if (index_table == TUNING_DISP_COLOR &&
					offset == disp_color_get_reg_offset("disp_color_pos_main")) {
				struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
						mtk_crtc, MTK_DISP_COLOR, 0);

				if (comp)
					disp_color_write_pos_main_for_dual_pipe(comp,
						cmdq_handle, wParams, pa, companion_pa);
			}
		}
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		cmdq_pkt_destroy(cmdq_handle);
		return -EFAULT;
	}

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
	if (cmdq_pkt_flush_threaded(cmdq_handle, frame_cmdq_cb, cb_data) < 0) {
		DDPPR_ERR("failed to flush %s\n", __func__);
		kfree(cb_data);
	}
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

	return 0;
}

static int disp_pq_proxy_virtual_wait_crtc_ready(struct drm_crtc *crtc, void *data)
{
	int *ready = (int *)data;
	struct pq_common_data *pq_data = to_mtk_crtc(crtc)->pq_data;

	if (atomic_read(&pq_data->pipe_info_filled) != 1) {
		*ready = 0;
		return -1;
	}
	*ready = 1;
	return 0;
}

static int mtk_drm_ioctl_get_pixel_type_by_fence(struct drm_crtc *crtc, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct pixel_type_map *pixel_types = &mtk_crtc->pq_data->pixel_types;
	struct mtk_pixel_type_fence *param = data;
	unsigned int fence_idx = param->fence_idx;
	int i, ret= -1;

	if (!pq_data)
		return -1;

	for (i = 0; i < SPR_TYPE_FENCE_MAX; i++)
		if (fence_idx == pixel_types->map[i].fence_idx) {
			param->type = pixel_types->map[i].type;
			param->secure = pixel_types->map[i].secure;
			break;
		}
	if (i != SPR_TYPE_FENCE_MAX)
		ret = 0;
	DDPDBG("%s: fence %u type %u", __func__, param->fence_idx, param->type);
	return ret;
}

int disp_pq_proxy_virtual_type_impl(struct drm_crtc *crtc, struct drm_device *dev,
		unsigned int cmd, char *kdata, struct drm_file *file_priv)
{
	struct mtk_ddp_comp *comp;
	int ret = -1;

	switch (cmd) {
	case PQ_VIRTUAL_SET_PROPERTY:
		ret = disp_pq_proxy_virtual_get_persist_property(crtc, kdata);
		break;
	case PQ_VIRTUAL_GET_MASTER_INFO:
		ret = mtk_drm_get_master_info_ioctl(dev, kdata, file_priv);
		break;
	case PQ_VIRTUAL_GET_IRQ:
		comp = mtk_ddp_comp_sel_in_cur_crtc_path(
				to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);
		ret = disp_ccorr_act_get_irq(comp, kdata);
		break;
	case PQ_COLOR_WRITE_REG:
		ret = disp_pq_proxy_virtual_hw_write(crtc, kdata);
		break;
	case PQ_COLOR_WRITE_SW_REG:
		ret = disp_pq_proxy_virtual_sw_write(crtc, kdata);
		break;
	case PQ_COLOR_READ_REG:
		ret = disp_pq_proxy_virtual_hw_read(crtc, kdata);
		break;
	case PQ_COLOR_READ_SW_REG:
		ret = disp_pq_proxy_virtual_sw_read(crtc, kdata);
		break;
	case PQ_VIRTUAL_CHECK_TRIGGER:
		ret = disp_pq_proxy_virtual_check_trigger(crtc, kdata);
		break;
	case PQ_VIRTUAL_RELAY_ENGINES:
		ret = disp_pq_proxy_virtual_relay_engines(crtc, kdata);
		break;
	case PQ_VIRTUAL_WAIT_CRTC_READY:
		ret = disp_pq_proxy_virtual_wait_crtc_ready(crtc, kdata);
		break;
	case PQ_VIRTUAL_GET_PIXEL_TYPE_BY_FENCE:
		ret = mtk_drm_ioctl_get_pixel_type_by_fence(crtc, kdata);
		break;
	default:
		DDPPR_ERR("%s, unknown cmd:%d\n", __func__, cmd);
	}
	return ret;
}

bool disp_pq_is_cmd_need_pm(enum mtk_pq_frame_cfg_cmd cmd)
{
	bool ret = true;

	switch (cmd) {
	case PQ_AAL_GET_HIST:
	case PQ_AAL_GET_SIZE:
	case PQ_CCORR_GET_IRQ:
	case PQ_C3D_GET_IRQ:
	case PQ_TDSHP_GET_SIZE:
	case PQ_VIRTUAL_GET_IRQ:
	case PQ_DBI_LOAD_PARAM:
	case PQ_DBI_LOAD_TB:
	case PQ_DBI_REMAP_CHG:
	case PQ_DBI_GET_HW_ID:
	case PQ_DBI_GET_WIDTH:
	case PQ_DBI_GET_HEIGHT:
	case PQ_DBI_GET_DBV:
	case PQ_DBI_GET_FPS:
	case PQ_DBI_GET_SCP:
	case PQ_VIRTUAL_GET_PIXEL_TYPE_BY_FENCE:
	case PQ_DBI_GET_SCP_LIFECYCLE:
		ret = false;
		break;
	default:
		break;
	}
	return ret;
}

int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_pq_proxy_ctl *params = data;
	struct mtk_ddp_comp *comp;
	unsigned int pq_type;
	unsigned int cmd;
	unsigned int i, j;
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned long long time;
	int ret = -1;
	int pm_ret = 0;

	if (!params || !params->size || !params->data) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}

	crtc = drm_crtc_find(dev, file_priv, params->crtc_id);
	if (!crtc) {
		DDPPR_ERR("%s, invalid crtc id:%d!\n", __func__, params->crtc_id);
		return -1;
	}

	pq_type = params->cmd >> 16;
	cmd = params->cmd & 0xffff;
	if (atomic_read(&to_mtk_crtc(crtc)->pq_data->pipe_info_filled) != 1 &&
			cmd != PQ_VIRTUAL_WAIT_CRTC_READY &&
			cmd != PQ_VIRTUAL_GET_MASTER_INFO) {
		DDPPR_ERR("%s, crtc %d not ready! cmd:%d\n",
				__func__, params->crtc_id, cmd);
		return -1;
	}

	time = sched_clock();
	if (params->size <= sizeof(stack_kdata))
		kdata = stack_kdata;
	else
		kdata = kmalloc(params->size, GFP_KERNEL);

	if (!kdata) {
		DDPPR_ERR("%s:%d, kdata alloc failed pq_type:%d, cmd:%d\n", __func__,
				__LINE__, pq_type, cmd);
		return -1;
	}

	if (copy_from_user(kdata, (void __user *)params->data, params->size) != 0)
		goto err;
	if (disp_pq_is_cmd_need_pm(cmd))
		pm_ret = mtk_vidle_pq_power_get(__func__);

	if (pq_type == MTK_DISP_VIRTUAL_TYPE) {
		ret = disp_pq_proxy_virtual_type_impl(crtc, dev, cmd, kdata, file_priv);
	} else {
		for_each_comp_in_cur_crtc_path(comp, to_mtk_crtc(crtc), i, j) {
			if (pq_module_matches[pq_type].type == mtk_ddp_comp_get_type(comp->id)) {
				ret = mtk_ddp_comp_pq_ioctl_transact(comp, cmd, kdata,
									params->size);
				if (ret < 0)
					DDPPR_ERR("%s:%d, ioctl transact failed, comp:%d,%d\n",
						__func__, __LINE__, comp->id, cmd);
			}
		}
	}
	if (disp_pq_is_cmd_need_pm(cmd)) {
		if (!pm_ret)
			mtk_vidle_pq_power_put(__func__);
	}
	if (cmd > PQ_GET_CMD_START) {
		if (copy_to_user((void __user *)params->data, kdata,  params->size) != 0)
			goto err;
	}
	if (cmd != PQ_AAL_GET_HIST && cmd != PQ_AAL_EVENTCTL && cmd !=  PQ_CCORR_GET_IRQ &&
			cmd != PQ_VIRTUAL_WAIT_CRTC_READY && cmd != PQ_VIRTUAL_GET_MASTER_INFO &&
			cmd != PQ_CHIST_GET && cmd != PQ_VIRTUAL_GET_IRQ)
		DDPINFO("%s, crtc index:%d, pq_type:%d, cmd:%d, use %llu us\n", __func__,
				drm_crtc_index(crtc), pq_type, cmd, (sched_clock() - time) / 1000);
	else
		DDPDBG("%s, crtc index:%d, pq_type:%d, cmd:%d, use %llu us\n", __func__,
				drm_crtc_index(crtc), pq_type, cmd, (sched_clock() - time) / 1000);
err:
	if (kdata != stack_kdata) {
		kfree(kdata);
	}
	return ret;
}

bool is_pq_frame_cfg_need_wait_done(struct mtk_drm_pq_param *requests, unsigned int cmds_len)
{
	bool ret = false;
	int index;

	for (index = 0; index < cmds_len; index++) {
		unsigned int cmd = requests[index].cmd & 0xffff;

		if (cmd == PQ_C3D_SET_LUT || cmd == PQ_GAMMA_SET_12BIT_GAMMALUT) {
			ret = true;
			break;
		}
	}
	return ret;
}

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_pq_config_ctl *params = data;
	int ret;

	if (data == NULL) {
		DDPPR_ERR("%s, null data!\n", __func__);
		return -1;
	}

	crtc = drm_crtc_find(dev, file_priv, params->crtc_id);
	if (!crtc) {
		DDPPR_ERR("%s, invalid crtc id:%d!\n", __func__, params->crtc_id);
		return -1;
	}
	if (atomic_read(&to_mtk_crtc(crtc)->pq_data->pipe_info_filled) != 1) {
		DDPPR_ERR("%s, crtc %d not ready!\n", __func__, params->crtc_id);
		return -1;
	}

	ret = disp_pq_helper_frame_config(crtc, NULL, data, true);
	return ret;
}

int disp_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock)
{
	struct mtk_drm_pq_config_ctl *params = data;
	unsigned int cmds_len = params->len;
	struct mtk_cmdq_cb_data *cb_data = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct cmdq_pkt *pq_cmdq_handle;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_pq_param requests[REQUEST_MAX_COUNT];
	unsigned int check_trigger = params->check_trigger;
	unsigned int i, j, k;
	int index = drm_crtc_index(crtc);
	bool is_atomic_commit = cmdq_handle;
	int pm_ret = 0;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	bool need_wait_done = false;

	DDPDBG("%s:%d ++, crtc index:%d\n", __func__, __LINE__, index);
	mtk_drm_trace_begin("disp_pq_helper_frame_config");

	if (!cmds_len || cmds_len > REQUEST_MAX_COUNT || params->data == NULL) {
		DDPPR_ERR("%s:%d, invalid requests for pq config\n",
			__func__, __LINE__);
		mtk_drm_trace_end();

		return -1;
	}

	if (copy_from_user(&requests, params->data, sizeof(struct mtk_drm_pq_param) * cmds_len)) {
		mtk_drm_trace_end();

		return -1;
	}

	if (is_atomic_commit)
		pq_cmdq_handle = cmdq_handle;
	else {
		pq_cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (!pq_cmdq_handle) {
			DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
			return -1;
		}

		if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
			mtk_crtc_wait_frame_done(mtk_crtc, pq_cmdq_handle,
				DDP_SECOND_PATH, 0);
		else
			mtk_crtc_wait_frame_done(mtk_crtc, pq_cmdq_handle, DDP_FIRST_PATH, 0);
	}

	/* Record Vblank start timestamp */
	mtk_vblank_config_rec_start(mtk_crtc, pq_cmdq_handle, PQ_HELPER_CONFIG);

	if (is_pq_frame_cfg_need_wait_done(requests, cmds_len) && pq_data) {
		int ret;

		need_wait_done = true;
		CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)pq_cmdq_handle, atomic_read(&pq_data->cfg_done));
		DDPINFO("%s wait cfg_done %d!\n", __func__, atomic_read(&pq_data->cfg_done));
		ret = wait_event_interruptible_timeout(pq_data->cfg_done_wq,
				atomic_cmpxchg(&pq_data->cfg_done, 1, 0) == 1, msecs_to_jiffies(1000));
		if (ret == -ERESTARTSYS)
			DDPMSG("%s cfg_done wakeup by signal\n", __func__);
		else
			DDPINFO("%s cfg_done wakeup %d %d\n", __func__, ret, atomic_read(&pq_data->cfg_done));
		CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)pq_cmdq_handle,
				0xF0000000 | atomic_read(&pq_data->cfg_done));
		atomic_set(&pq_data->cfg_done, 0);
	}

	/* call comp frame config */
	pm_ret = mtk_vidle_pq_power_get(__func__);

	for (k = 0; k < cmds_len; k++) {
		unsigned int pq_type = requests[k].cmd >> 16;
		unsigned int cmd = requests[k].cmd & 0xffff;

		if (pq_type >= MTK_DISP_PQ_TYPE_MAX || !requests[k].size)
			continue;

		if (cmd != PQ_AAL_SET_PARAM && cmd != PQ_COLOR_DRECOLOR_SET_PARAM &&
				cmd != PQ_CCORR_SET_CCORR && cmd != PQ_COLOR_SET_COLOR_REG)
			DDPINFO("%s, pq_type:%d, cmd:%d\n", __func__, pq_type, cmd);
		else
			DDPDBG("%s, pq_type:%d, cmd:%d\n", __func__, pq_type, cmd);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
			if (pq_module_matches[pq_type].type == mtk_ddp_comp_get_type(comp->id)) {
				char stack_kdata[128];
				char *kdata = NULL;

				if (requests[k].size <= sizeof(stack_kdata))
					kdata = stack_kdata;
				else
					kdata = kmalloc(requests[k].size, GFP_KERNEL);

				if (!kdata) {
					DDPPR_ERR("%s:%d, kdata alloc failed comp:%d,%d\n",
							__func__, __LINE__, comp->id, cmd);
					continue;
				}
				if (copy_from_user(kdata, (void __user *)requests[k].data,
						requests[k].size) == 0) {
					mtk_drm_trace_begin("frame_config(compId: %d)", comp->id);

					if (mtk_ddp_comp_pq_frame_config(comp, pq_cmdq_handle,
							cmd, kdata, requests[k].size) < 0)
						DDPPR_ERR("%s:%d, config failed, comp:%d,%d\n",
							__func__, __LINE__, comp->id, cmd);

					mtk_drm_trace_end();
				}

				if (kdata != stack_kdata)
					kfree(kdata);
			}
		}
	}
	if (!pm_ret)
		mtk_vidle_pq_power_put(__func__);

	/* atomic commit will flush in crtc */
	if (!is_atomic_commit) {
		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data) {
			DDPPR_ERR("cb data creation failed\n");
			cmdq_pkt_destroy(pq_cmdq_handle);
			if (need_wait_done) {
				need_wait_done = false;
				atomic_set(&pq_data->cfg_done, 1);
				wake_up_interruptible(&pq_data->cfg_done_wq);
			}
			CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)pq_cmdq_handle, 0xE0000001);
			return -1;
		}

		cb_data->crtc = crtc;
		cb_data->cmdq_handle = pq_cmdq_handle;
		cb_data->misc = need_wait_done;

		if (user_lock)
			DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

		mtk_drm_trace_begin("mtk_drm_idlemgr_kick");
		mtk_drm_idlemgr_kick(__func__, crtc, !user_lock);
		mtk_drm_trace_end();

		if (!(mtk_crtc->enabled)) {
			DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
			cmdq_pkt_destroy(pq_cmdq_handle);
			kfree(cb_data);
			if (user_lock)
				DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
			if (need_wait_done) {
				need_wait_done = false;
				atomic_set(&pq_data->cfg_done, 1);
				wake_up_interruptible(&pq_data->cfg_done_wq);
			}
			CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)pq_cmdq_handle, 0xE0000002);
			return -1;
		}

		/* Record Vblank end timestamp and calculate duration */
		mtk_vblank_config_rec_end_cal(mtk_crtc, pq_cmdq_handle, PQ_HELPER_CONFIG);

		mtk_drm_trace_begin("flush+check_trigger");
		if (cmdq_pkt_flush_threaded(pq_cmdq_handle, frame_cmdq_cb, cb_data) < 0) {
			DDPPR_ERR("failed to flush %s\n", __func__);
			if (need_wait_done) {
				need_wait_done = false;
				atomic_set(&pq_data->cfg_done, 1);
				wake_up_interruptible(&pq_data->cfg_done_wq);
			}
			kfree(cb_data);
			CRTC_MMP_MARK(index, pq_frame_config, (unsigned long)pq_cmdq_handle, 0xE0000003);
		} else if (check_trigger)
			mtk_crtc_check_trigger(mtk_crtc, check_trigger == CHECK_TRIGGER_DELAY
						|| mtk_crtc->msync2.msync_frame_status, !user_lock);
		DDPDBG("%s msync_frame_status:%d\n", __func__, mtk_crtc->msync2.msync_frame_status);
		mtk_drm_trace_end();

		if (user_lock)
			DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

	}
	DDPDBG("%s:%d --\n", __func__, __LINE__);
	mtk_drm_trace_end();

	return 0;
}

static void disp_pq_helper_fill_tuning_table(struct mtk_ddp_comp *comp,
	int comp_type, int path_order, resource_size_t pa, resource_size_t companion_pa)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int table_index = TUNING_REG_MAX;

	switch (comp_type) {
	case MTK_DISP_COLOR:
		table_index = TUNING_DISP_COLOR;
		break;
	case MTK_DISP_CCORR:
	{
		struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);

		if (ccorr_data->is_linear)
			table_index = TUNING_DISP_CCORR_LINEAR;
	}
		break;
	case MTK_DISP_AAL:
		table_index = TUNING_DISP_AAL;
		break;
	case MTK_DISP_GAMMA:
		table_index = TUNING_DISP_GAMMA;
		break;
	case MTK_DISP_TDSHP:
		table_index = TUNING_DISP_TDSHP;
		break;
	case MTK_DMDP_AAL:
		table_index = TUNING_DISP_MDP_AAL;
		break;
	case MTK_DISP_ODDMR:
		table_index = TUNING_DISP_ODDMR_TOP;
		break;
	case MTK_DISP_DITHER:
	case MTK_DISP_C3D:
	case MTK_DISP_CHIST:
		break;
	default:
		DDPPR_ERR("%s, unknown comp_type:%d\n", __func__, comp_type);
	}

	if (table_index < TUNING_REG_MAX) {
		pq_data->tuning_pa_table[table_index].type = comp_type;
		pq_data->tuning_pa_table[table_index].pa_base = pa;
		pq_data->tuning_pa_table[table_index].companion_pa_base = companion_pa;
	}
}

int disp_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion)
{
	int _path_order, ret;
	bool _is_right_pipe;
	struct mtk_ddp_comp *_companion = NULL;
	int comp_type;

	ret = mtk_ddp_comp_locate_in_cur_crtc_path(comp->mtk_crtc, comp->id,
					&_is_right_pipe, &_path_order);
	if (ret < 0)
		return ret;
	if (is_right_pipe)
		*is_right_pipe = _is_right_pipe;
	if (path_order)
		*path_order = _path_order;
	DDPINFO("%s %s order %d pipe %d\n", __func__,
					mtk_dump_comp_str(comp), _path_order, _is_right_pipe);
	comp_type = mtk_ddp_comp_get_type(comp->id);
	if (comp_type < 0) {
		DDPPR_ERR("%s comp id %d is invalid\n", __func__, comp->id);
		return comp_type;
	}
	if (comp->mtk_crtc->is_dual_pipe && companion) {
		if (!_is_right_pipe)
			_companion = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
						comp_type, _path_order);
		else
			_companion = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
						comp_type, _path_order);
		if (!_companion)
			ret = -1;
		if (_companion) {
			*companion = _companion;
			DDPMSG("%s companion %s\n", __func__, mtk_dump_comp_str(_companion));
		}
	}

	if (!_is_right_pipe) {
		resource_size_t companion_regs_pa = 0;

		if (_companion)
			companion_regs_pa = _companion->regs_pa;
		disp_pq_helper_fill_tuning_table(comp, comp_type, _path_order,
					comp->regs_pa, companion_regs_pa);
	}
	return ret;
}

static int disp_pq_proxy_virtual_get_persist_property(struct drm_crtc *crtc, void *data)
{
	int i;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int pq_persist_property[32];

	memset(pq_persist_property, 0, sizeof(pq_persist_property));
	memcpy(pq_persist_property, (unsigned int *)data, sizeof(pq_persist_property));

	for (i = 0; i < DISP_PQ_PROPERTY_MAX; i++) {
		pq_data->old_persist_property[i] = pq_data->new_persist_property[i];
		pq_data->new_persist_property[i] = pq_persist_property[i];
	}

	DDPFUNC("+");

	if (pq_data->old_persist_property[DISP_PQ_DITHER_COLOR_DETECT] !=
		pq_data->new_persist_property[DISP_PQ_DITHER_COLOR_DETECT])
		disp_dither_set_color_detect(crtc,
			pq_data->new_persist_property[DISP_PQ_DITHER_COLOR_DETECT]);

	DDPFUNC("-");

	return 0;
}

struct drm_crtc *disp_pq_get_crtc_from_connector(int connector_id, struct drm_device *drm_dev)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int cur_connector_id = 0;

	if (!drm_dev) {
		DDPPR_ERR("%s: failed to get drm_dev!\n", __func__);
		return NULL;
	}
	drm_for_each_crtc(crtc, drm_dev) {
		output_comp = mtk_ddp_comp_request_output(to_mtk_crtc(crtc));
		if (output_comp == NULL)
			continue;
		mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &cur_connector_id);
		if (cur_connector_id == connector_id)
			return crtc;
	}
	return NULL;
}

static int disp_pq_proxy_virtual_check_trigger(struct drm_crtc *crtc, void *data)
{

	int ret = 0;
	bool delay_trigger;
	struct mtk_drm_crtc *mtk_crtc;

	if ((!crtc) || (!data)) {
		DDPMSG("%s: failed!\n", __func__);
		return -EFAULT;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc ) {
		DDPMSG("%s invalid mtk_crtc\n", __func__);
		return -EFAULT;
	}

	delay_trigger = *(bool *)data;
	DDPINFO("%s msync_frame_status:%d, delay_trigger:%d\n",
		__func__, mtk_crtc->msync2.msync_frame_status, delay_trigger);
	mtk_crtc_check_trigger(mtk_crtc, delay_trigger || mtk_crtc->msync2.msync_frame_status, true);

	return ret;
}

static bool disp_pq_is_relay_engines(struct mtk_ddp_comp *comp, uint32_t engine)
{
	bool ret = false;

	if (((engine & 0x1) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_COLOR))
		|| ((engine & 0x2) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CCORR))
		|| ((engine & 0x4) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_C3D))
		|| ((engine & 0x8) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_TDSHP))
		|| ((engine & 0x10) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_AAL))
		|| ((engine & 0x20) && (mtk_ddp_comp_get_type(comp->id) == MTK_DMDP_AAL))
		|| ((engine & 0x40) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_GAMMA))
		|| ((engine & 0x80) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DITHER))
		|| ((engine & 0x100) && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CHIST)))
		ret = true;

	return ret;
}

static void disp_pq_relay_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(cb_data->crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	atomic_set(&pq_data->pq_hw_relay_cfg_done, 1);
	wake_up_interruptible(&pq_data->pq_hw_relay_cb_wq);

	DDPMSG("%s: config hw done\n", __func__);

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

int disp_pq_proxy_virtual_relay_engines(struct drm_crtc *crtc, void *data)
{
	int relay = 0;
	bool wait_config_done = false;
	uint32_t relay_engines = 0x0;
	uint32_t caller;

	struct mtk_pq_relay_enable *relayCtlSet = data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_cmdq_cb_data *cb_data = NULL;
	struct cmdq_pkt *cmdq_handle = NULL;
	struct mtk_ddp_comp *comp = NULL;
	int index = drm_crtc_index(crtc);
	int ret = 0;
	int i, j;

	relay = relayCtlSet->enable ? 1 : 0;
	wait_config_done = relayCtlSet->wait_hw_config_done;
	relay_engines = relayCtlSet->relay_engines;
	caller = (uint32_t)relayCtlSet->caller;

	DDPMSG("%s: crtc_index: %d, relay: %d, wait: %d, engine: 0x%x\n",
		__func__, index, relay, wait_config_done, relay_engines);

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (!cmdq_handle) {
		DDPPR_ERR("%s:%d NULL cmdq handle\n", __func__, __LINE__);
		return -EFAULT;
	}
	if (mtk_crtc_with_sub_path(crtc, mtk_crtc->ddp_mode))
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_SECOND_PATH, 0);
	else
		mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (comp && comp->funcs && comp->funcs->bypass
			&& disp_pq_is_relay_engines(comp, relay_engines))
			mtk_ddp_comp_bypass(comp, relay, caller, cmdq_handle);
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			if (comp && comp->funcs && comp->funcs->bypass
				&& disp_pq_is_relay_engines(comp, relay_engines))
				mtk_ddp_comp_bypass(comp, relay, caller, cmdq_handle);
		}
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		cmdq_pkt_destroy(cmdq_handle);
		return -1;
	}

	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	if (!(mtk_crtc->enabled)) {
		DDPINFO("%s:%d, slepted\n", __func__, __LINE__);
		cmdq_pkt_destroy(cmdq_handle);
		kfree(cb_data);
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);
		return 1;
	}

	if (cmdq_pkt_flush_threaded(cmdq_handle, disp_pq_relay_cmdq_cb, cb_data) < 0) {
		DDPPR_ERR("failed to flush %s\n", __func__);
		kfree(cb_data);
	} else
		mtk_crtc_check_trigger(mtk_crtc, true, false);

	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, mtk_crtc->enabled);

	while (wait_config_done) {
		if (atomic_read(&pq_data->pq_hw_relay_cfg_done) == 0) {
			DDPDBG("%s: wait_event_interruptible ++\n", __func__);
			ret = wait_event_interruptible_timeout(pq_data->pq_hw_relay_cb_wq,
				(atomic_cmpxchg(&pq_data->pq_hw_relay_cfg_done, 1, 0) == 1),
				msecs_to_jiffies(1000));
			if (ret == -ERESTARTSYS) {
				DDPMSG("%s: interrupted unexpected by signal\n", __func__);
				continue;
			}

			if (ret == 0)
				DDPDBG("%s: wait_event_interruptible by timeout --\n", __func__);
		} else
			DDPDBG("%s(%d)\n", __func__, atomic_read(&pq_data->pq_hw_relay_cfg_done));

		atomic_set(&pq_data->pq_hw_relay_cfg_done, 0);
		wait_config_done = false;
	}

	return 0;
}
