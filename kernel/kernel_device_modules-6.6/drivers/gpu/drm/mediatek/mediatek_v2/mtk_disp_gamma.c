// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_disp_gamma.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_pq_helper.h"

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y, m, n) do { } while (0)
#endif

#define DISP_GAMMA_EN		0x0000
#define DISP_GAMMA_SHADOW_SRAM	0x0014
#define DISP_GAMMA_CFG		0x0020
#define DISP_GAMMA_SIZE		0x0030
#define DISP_GAMMA_PURE_COLOR	0x0038
#define DISP_GAMMA_BANK		0x0100
#define DISP_GAMMA_LUT		0x0700
#define DISP_GAMMA_LUT_0	0x0700
#define DISP_GAMMA_LUT_1	0x0B00
#define DISP_GAMMA_BLOCK_0_R_GAIN 0x0054
#define DISP_GAMMA_BLOCK_0_G_GAIN 0x0058
#define DISP_GAMMA_BLOCK_0_B_GAIN 0x005C

#define    GAMMA_EN           BIT(0)
#define    STALL_CG_ON        BIT(8)
#define    GAMMA_MUT_EN       BIT(3)
#define    GAMMA_LUT_TYPE     BIT(2)
#define    GAMMA_LUT_EN       BIT(1)
#define    GAMMA_RELAY_MODE    BIT(0)

#define DISP_GAMMA_BLOCK_SIZE 256
#define DISP_GAMMA_GAIN_SIZE 3
#define GAIN_RANGE_UNIT 100

#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

enum DISP_GAMMA_USER_CMD {
	SET_GAMMA_GAIN = 0,
};

enum GAMMA_MODE {
	HW_8BIT = 0,
	HW_12BIT_MODE_IN_8BIT,
	HW_12BIT_MODE_IN_10BIT,
};

enum GAMMA_CMDQ_TYPE {
	GAMMA_USERSPACE = 0,
	GAMMA_RESUME,
};

struct gamma_color_protect_mode {
	unsigned int red_support;
	unsigned int green_support;
	unsigned int blue_support;
	unsigned int black_support;
	unsigned int white_support;
};

static int disp_gamma_create_gce_pkt(struct mtk_ddp_comp *comp, struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (!mtk_crtc || !pkt) {
		DDPPR_ERR("%s, invalid crtc or pkt\n", __func__);
		return -1;
	}

	if (*pkt)
		return 0;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_PQ]);
	else
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

static bool disp_gamma_clock_is_on(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	struct mtk_disp_gamma *comp_gamma_data = NULL;

	if (!comp->mtk_crtc->is_dual_pipe &&
			atomic_read(&gamma_data->gamma_is_clock_on) == 1)
		return true;

	if (comp->mtk_crtc->is_dual_pipe) {
		comp_gamma_data = comp_to_gamma(gamma_data->companion);
		if (atomic_read(&gamma_data->gamma_is_clock_on) == 1 &&
			comp_gamma_data && atomic_read(&comp_gamma_data->gamma_is_clock_on) == 1)
			return true;
	}

	return false;
}

static void disp_gamma_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_gamma *data = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = data->primary_data;
	struct mtk_ddp_comp *comp_gamma1 = data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	mutex_lock(&primary_data->data_lock);
	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_GAMMA_CFG, GAMMA_RELAY_MODE, GAMMA_RELAY_MODE);
			if (comp->mtk_crtc->is_dual_pipe && comp_gamma1)
				cmdq_pkt_write(handle, comp_gamma1->cmdq_base,
					comp_gamma1->regs_pa + DISP_GAMMA_CFG,
					GAMMA_RELAY_MODE, GAMMA_RELAY_MODE);
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_GAMMA_CFG, 0x0, GAMMA_RELAY_MODE);
				if (comp->mtk_crtc->is_dual_pipe && comp_gamma1)
					cmdq_pkt_write(handle, comp_gamma1->cmdq_base,
						comp_gamma1->regs_pa + DISP_GAMMA_CFG, 0x0, GAMMA_RELAY_MODE);
			}
		}
	}
	mutex_unlock(&primary_data->data_lock);
}

static int disp_gamma_write_sram(struct mtk_ddp_comp *comp,
	int lock, struct DISP_GAMMA_12BIT_LUT_T *gamma_12b_lut)
{
	int i, j, block_num, offset = 0;
	int ret = 0;
	unsigned int write_value = 0;
	unsigned int config_value = 0;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;
	struct cmdq_pkt *handle = NULL;
	struct cmdq_reuse *reuse_lut;

	if (!gamma_12b_lut) {
		DDPPR_ERR("%s: gamma_12b_lut null\n", __func__);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&gamma->primary_data->data_lock);

	if (gamma_12b_lut->hw_id == DISP_GAMMA_TOTAL) {
		DDPPR_ERR("%s: table not initialized\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (gamma->primary_data->data_mode == HW_12BIT_MODE_IN_10BIT) {
		block_num = DISP_GAMMA_12BIT_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else if (gamma->primary_data->data_mode == HW_12BIT_MODE_IN_8BIT) {
		block_num = DISP_GAMMA_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else {
		DDPPR_ERR("%s: g_gamma_data_mode is error\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	reuse_lut = gamma->reuse_gamma_lut;
	handle = primary_data->sram_pkt;

	if (handle == NULL) {
		ret = -EFAULT;
		DDPPR_ERR("%s: handle null\n", __func__);
		goto gamma_write_lut_unlock;
	}
	handle->no_pool = true;

	if (gamma->primary_data->table_out_sel == 1)
		gamma->primary_data->table_config_sel = 0;
	else
		gamma->primary_data->table_config_sel = 1;

	if ((int)(gamma_12b_lut->lut_0[0] & 0x3FF) -
		(int)(gamma_12b_lut->lut_0[510] & 0x3FF) > 0) {
		config_value = 0x4;
		DDPINFO("decreasing LUT\n");
	} else {
		config_value = 0;
		DDPINFO("Incremental LUT\n");
	}

	write_value = (gamma->primary_data->table_config_sel << 1) |
			gamma->primary_data->table_out_sel;

	if (!gamma->pkt_reused) {
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + DISP_GAMMA_SHADOW_SRAM,
				write_value, 0x3, &reuse_lut[offset++]);
		for (i = 0; i < block_num; i++) {
			write_value = i | (gamma->primary_data->data_mode - 1) << 2;
			cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + DISP_GAMMA_BANK,
					write_value, 0x7, &reuse_lut[offset++]);
			for (j = 0; j < DISP_GAMMA_BLOCK_SIZE; j++) {
				write_value = gamma_12b_lut->lut_0[i * DISP_GAMMA_BLOCK_SIZE + j];
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + DISP_GAMMA_LUT_0 + j * 4,
					write_value, ~0, &reuse_lut[offset++]);
				write_value = gamma_12b_lut->lut_1[i * DISP_GAMMA_BLOCK_SIZE + j];
				cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + DISP_GAMMA_LUT_1 + j * 4,
					write_value, ~0, &reuse_lut[offset++]);
			}
		}
		cmdq_pkt_write_value_addr_reuse(handle, comp->regs_pa + DISP_GAMMA_CFG,
				config_value, GAMMA_LUT_TYPE, &reuse_lut[offset++]);
		gamma->pkt_reused = true;
	} else {
		reuse_lut[offset].val = write_value;
		cmdq_pkt_reuse_value(handle, &reuse_lut[offset++]);
		for (i = 0; i < block_num; i++) {
			reuse_lut[offset].val = i | (gamma->primary_data->data_mode - 1) << 2;
			cmdq_pkt_reuse_value(handle, &reuse_lut[offset++]);
			for (j = 0; j < DISP_GAMMA_BLOCK_SIZE; j++) {
				reuse_lut[offset].val = gamma_12b_lut->lut_0[i * DISP_GAMMA_BLOCK_SIZE + j];
				cmdq_pkt_reuse_value(handle, &reuse_lut[offset++]);

				reuse_lut[offset].val = gamma_12b_lut->lut_1[i * DISP_GAMMA_BLOCK_SIZE + j];
				cmdq_pkt_reuse_value(handle, &reuse_lut[offset++]);
			}
		}
		reuse_lut[offset].val = config_value;
		cmdq_pkt_reuse_value(handle, &reuse_lut[offset++]);
	}

gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->data_lock);

	return ret;
}

static bool disp_gamma_flush_sram(struct mtk_ddp_comp *comp, int cmd_type)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma_data->primary_data;
	struct cmdq_pkt *cmdq_handle = primary_data->sram_pkt;
	struct cmdq_client *client = NULL;

	if (!cmdq_handle) {
		DDPMSG("%s: cmdq handle is null.\n", __func__);
		return false;
	}

	if (disp_gamma_clock_is_on(comp) == false)
		return false;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
	else
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	cmdq_mbox_enable(client->chan);
	CRTC_MMP_MARK(0, gamma_ioctl, comp->id, (unsigned long)cmdq_handle);

	switch (cmd_type) {
	case GAMMA_USERSPACE:
		primary_data->table_out_sel = primary_data->table_config_sel;
		cmdq_pkt_refinalize(cmdq_handle);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_mbox_disable(client->chan);
		break;

	case GAMMA_RESUME:
		cmdq_pkt_refinalize(cmdq_handle);
		cmdq_pkt_flush(cmdq_handle);
		cmdq_mbox_disable(client->chan);
		break;
	default:
		DDPPR_ERR("%s, invalid cmd_type:%d\n", __func__, cmd_type);
	}

	return true;
}

static void disp_gamma_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			GAMMA_LUT_EN, GAMMA_LUT_EN);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_SHADOW_SRAM,
			primary_data->table_config_sel << 1 | primary_data->table_out_sel, ~0);
}

static int disp_gamma_cfg_set_12bit_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct DISP_GAMMA_12BIT_LUT_T *config = data;
	struct mtk_ddp_comp *companion = gamma->companion;
	int ret = -1;

	if (!data || !mtk_crtc) {
		DDPPR_ERR("%s, invalid data or crtc!\n", __func__);
		return ret;
	}
	// 1. kick idle
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled) {
		DDPMSG("%s:%d, slepted\n", __func__, __LINE__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	CRTC_MMP_EVENT_START(0, gamma_ioctl, 0, 0);
	// 2. lock for protect crtc & power
	mutex_lock(&primary_data->clk_lock);
	if (!disp_gamma_clock_is_on(comp) || !mtk_crtc->enabled) {
		mutex_unlock(&primary_data->clk_lock);
		DDPMSG("%s, skip write sram, power is off!\n", __func__);
		CRTC_MMP_EVENT_END(0, gamma_ioctl, 0, 2);
		return 0;
	}
	memcpy(&primary_data->gamma_12b_lut, (struct DISP_GAMMA_12BIT_LUT_T *)data,
			sizeof(struct DISP_GAMMA_12BIT_LUT_T));

	if (disp_gamma_write_sram(comp, 0, config) < 0) {
		mutex_unlock(&primary_data->clk_lock);
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if ((comp->mtk_crtc != NULL) && comp->mtk_crtc->is_dual_pipe) {
		if (disp_gamma_write_sram(gamma->companion, 0, config) < 0) {
			mutex_unlock(&primary_data->clk_lock);
			DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
			return -EFAULT;
		}
	}

	ret = disp_gamma_flush_sram(comp, GAMMA_USERSPACE);
	disp_gamma_flip_sram(comp, handle);
	if (comp->mtk_crtc->is_dual_pipe && companion)
		disp_gamma_flip_sram(companion, handle);

	if (!atomic_read(&primary_data->gamma_sram_hw_init)) {
		atomic_set(&primary_data->gamma_sram_hw_init, 1);
		disp_gamma_bypass(comp, 0, PQ_FEATURE_DEFAULT, handle);
		DDPINFO("%s, set gamma unrelay\n", __func__);
	}

	CRTC_MMP_EVENT_END(0, gamma_ioctl, 0, 1);
	mutex_unlock(&primary_data->clk_lock);

	return ret;
}

static int disp_gamma_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock, struct DISP_GAMMA_LUT_T *user_gamma_lut)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;
	int i, ret = 0;

	DDPINFO("%s: Set module(%d) lut\n", __func__, comp->id);

	if (lock)
		mutex_lock(&gamma->primary_data->data_lock);
	if (user_gamma_lut == NULL) {
		DDPINFO("%s: gamma_lut null\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}
	if (user_gamma_lut->hw_id == DISP_GAMMA_TOTAL) {
		DDPINFO("%s: table not initialized\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}
	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
			user_gamma_lut->lut[i], ~0);

		if ((i & 0x3f) == 0) {
			DDPINFO("[0x%08lx](%d) = 0x%x\n",
				(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
				i, user_gamma_lut->lut[i]);
		}
	}
	i--;
	DDPINFO("[0x%08lx](%d) = 0x%x\n",
		(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
		i, user_gamma_lut->lut[i]);

	if ((int)(user_gamma_lut->lut[0] & 0x3FF) -
		(int)(user_gamma_lut->lut[510] & 0x3FF) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, GAMMA_LUT_TYPE, GAMMA_LUT_TYPE);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0, GAMMA_LUT_TYPE);
		DDPINFO("Incremental LUT\n");
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			GAMMA_LUT_EN, GAMMA_LUT_EN);
gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->data_lock);

	return ret;

}


static int disp_gamma_cfg_set_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	struct DISP_GAMMA_LUT_T *config = data;
	struct mtk_ddp_comp *companion = gamma_data->companion;

	gamma_data->primary_data->gamma_lut_cur = *((struct DISP_GAMMA_LUT_T *)data);

	mutex_lock(&gamma_data->primary_data->data_lock);
	if (disp_gamma_set_lut(comp, handle, 0, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		mutex_unlock(&gamma_data->primary_data->data_lock);
		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe && gamma_data->companion) {
		if (disp_gamma_set_lut(gamma_data->companion, handle, 0, config) < 0) {
			DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
			mutex_unlock(&gamma_data->primary_data->data_lock);
			return -EFAULT;
		}
	}

	if (!atomic_read(&gamma_data->primary_data->gamma_sram_hw_init)) {
		atomic_set(&gamma_data->primary_data->gamma_sram_hw_init, 1);
		gamma_data->primary_data->relay_state &= ~(0x1 << PQ_FEATURE_DEFAULT);
		if (gamma_data->primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_GAMMA_CFG, 0x0, 0x1);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_GAMMA_CFG, 0x0, 0x1);
			DDPINFO("%s, set gamma unrelay\n", __func__);
		}
	}
	mutex_unlock(&gamma_data->primary_data->data_lock);

	return 0;
}

static int disp_gamma_write_gain_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock, struct mtk_disp_gamma_sb_param *user_gamma_gain)
{
	int i;
	int ret = 0;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_sb_param *sb_param = &gamma->primary_data->sb_param;
	unsigned int gamma_gain_range = gamma->data->gamma_gain_range;
	unsigned int sb_gain_range = sb_param->gain_range;
	unsigned int hw_gain[3], ratio = GAIN_RANGE_UNIT;

	if (user_gamma_gain == NULL)
		goto unlock;

	if (lock)
		mutex_lock(&gamma->primary_data->data_lock);
	if(comp == NULL || handle == NULL)
		goto unlock;

	if (sb_gain_range == 0 || gamma_gain_range == 0) {
		DDPINFO("%s %d/%d hw/sw gain range invalid!!\n", __func__, gamma_gain_range, sb_gain_range);
	} else if (sb_gain_range != gamma_gain_range) {
		ratio = gamma_gain_range * GAIN_RANGE_UNIT / sb_gain_range;
		if (ratio != GAIN_RANGE_UNIT)
			DDPINFO("%s %d/%d hw/sw gain range different!!\n", __func__, gamma_gain_range, sb_gain_range);
	}

	for (i = 0; i < DISP_GAMMA_GAIN_SIZE; i++) {
		hw_gain[i] = sb_param->gain[i] * ratio / GAIN_RANGE_UNIT;
		if (hw_gain[i] > gamma_gain_range)
			hw_gain[i] = gamma_gain_range;
	}

	/* close gamma gain mul if all channels don't need gain */
	if (((hw_gain[0] == gamma_gain_range) &&
	     (hw_gain[1] == gamma_gain_range) &&
	     (hw_gain[2] == gamma_gain_range)) ||
	    ((hw_gain[0] == 0) &&
	     (hw_gain[1] == 0) &&
	     (hw_gain[2] == 0))) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0, GAMMA_MUT_EN);
		DDPINFO("all gain == %d\n", hw_gain[0]);
		goto unlock;
	}

	for (i = 0; i < DISP_GAMMA_GAIN_SIZE; i++) {
		/* using rang - 1 to approximate range */
		if (hw_gain[i] == gamma_gain_range)
			hw_gain[i] = gamma_gain_range - 1;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_BLOCK_0_R_GAIN + i * 4,
			hw_gain[i], ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, GAMMA_MUT_EN, GAMMA_MUT_EN);

unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->data_lock);
	return ret;
}

// for HWC LayerBrightness, backlight & gamma gain update by atomic
int disp_gamma_set_gain(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int gain[3], unsigned int gain_range)
{
	int ret = 0;
	bool support_gamma_gain;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	support_gamma_gain = gamma->data->support_gamma_gain;
	if (!support_gamma_gain) {
		DDPPR_ERR("%s gamma gain not support!\n",__func__);
		return -EFAULT;
	}

	if (gamma->primary_data->sb_param.gain[gain_r] != gain[gain_r] ||
		gamma->primary_data->sb_param.gain[gain_g] != gain[gain_g] ||
		gamma->primary_data->sb_param.gain[gain_b] != gain[gain_b]) {

		mutex_lock(&gamma->primary_data->data_lock);
		gamma->primary_data->sb_param.gain[gain_r] = gain[gain_r];
		gamma->primary_data->sb_param.gain[gain_g] = gain[gain_g];
		gamma->primary_data->sb_param.gain[gain_b] = gain[gain_b];
		gamma->primary_data->sb_param.gain_range = gain_range;

		if (!gamma->primary_data->hwc_ctl_silky_brightness_support)
			gamma->primary_data->hwc_ctl_silky_brightness_support = true;

		if (disp_gamma_write_gain_reg(comp, handle, 0, &gamma->primary_data->sb_param) < 0) {
			mutex_unlock(&gamma->primary_data->data_lock);
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			if (disp_gamma_write_gain_reg(gamma->companion, handle, 0,
				&gamma->primary_data->sb_param) < 0) {
				mutex_unlock(&gamma->primary_data->data_lock);
				return -EFAULT;
			}
		}
		mutex_unlock(&gamma->primary_data->data_lock);
		CRTC_MMP_MARK(0, gamma_backlight, gamma->primary_data->sb_param.gain[gain_r], (unsigned long)handle);
		DDPINFO("%s : gain(r: %d, g: %d, b: %d), range: %d, handle: %p\n", __func__,
			gamma->primary_data->sb_param.gain[gain_r],
			gamma->primary_data->sb_param.gain[gain_g],
			gamma->primary_data->sb_param.gain[gain_b],
			gamma->primary_data->sb_param.gain_range, handle);
	}

	return ret;
}

static void disp_gamma_init_primary_data(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *data = comp_to_gamma(comp);
	struct mtk_disp_gamma *companion_data = comp_to_gamma(data->companion);
	struct mtk_disp_gamma_primary *primary_data = data->primary_data;

	if (data->is_right_pipe) {
		kfree(data->primary_data);
		data->primary_data = companion_data->primary_data;
		return;
	}

	// init primary data
	mutex_init(&primary_data->clk_lock);
	mutex_init(&primary_data->data_lock);

	primary_data->gamma_12b_lut.hw_id = DISP_GAMMA_TOTAL;
	primary_data->gamma_lut_cur.hw_id = DISP_GAMMA_TOTAL;

	disp_gamma_create_gce_pkt(comp, &primary_data->sram_pkt);
	atomic_set(&primary_data->gamma_sram_hw_init, 0);
	primary_data->relay_state = 0x1 << PQ_FEATURE_DEFAULT;
}

static void disp_gamma_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!gamma->is_right_pipe) {
			gamma->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead += gamma->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width += gamma->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			gamma->tile_overhead.width = cfg->tile_overhead.left_in_width;
		} else {
			gamma->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				gamma->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				gamma->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			gamma->tile_overhead.width = cfg->tile_overhead.right_in_width;
		}
	}

}

static void disp_gamma_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	gamma->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		gamma->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	gamma->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_gamma_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	unsigned int overhead_v;
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = gamma->tile_overhead.width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (gamma->set_partial_update != 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_SIZE,
			(width << 16) | cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : gamma->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_SIZE,
			(width << 16) | (gamma->roi_height + overhead_v * 2), ~0);
	}
	if (gamma->primary_data->data_mode == HW_12BIT_MODE_IN_8BIT ||
		gamma->primary_data->data_mode == HW_12BIT_MODE_IN_10BIT) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_BANK,
			(gamma->primary_data->data_mode - 1) << 2, 0x4);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_PURE_COLOR,
			gamma->primary_data->color_protect.gamma_color_protect_support |
			gamma->primary_data->color_protect.gamma_color_protect_lsb, ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, 0, STALL_CG_ON);

	mutex_lock(&primary_data->data_lock);
	if (atomic_read(&primary_data->gamma_sram_hw_init) != 0) {
		if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS] ||
			gamma->primary_data->hwc_ctl_silky_brightness_support)
			disp_gamma_write_gain_reg(comp, handle, 0, &gamma->primary_data->sb_param);

		if (gamma->primary_data->data_mode != HW_12BIT_MODE_IN_8BIT &&
			gamma->primary_data->data_mode != HW_12BIT_MODE_IN_10BIT) {
			disp_gamma_set_lut(comp, handle, 0, &gamma->primary_data->gamma_lut_cur);
		} else {
			if (primary_data->need_refinalize) {
				disp_gamma_flush_sram(comp, GAMMA_RESUME);
				primary_data->need_refinalize = false;
			}
			disp_gamma_flip_sram(comp, handle);
		}
	}

	if (primary_data->relay_state != 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, GAMMA_RELAY_MODE, GAMMA_RELAY_MODE);

	mutex_unlock(&primary_data->data_lock);
}

static void disp_gamma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, GAMMA_EN, ~0);
}

static void disp_gamma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, 0x0, ~0);
}

static void disp_gamma_set(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state, struct cmdq_pkt *handle)
{
	unsigned int i;
	struct drm_color_lut *lut;
	u32 word = 0;
	u32 word_first = 0;
	u32 word_last = 0;

	DDPINFO("%s\n", __func__);

	if (state->gamma_lut) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_GAMMA_CFG,
				GAMMA_LUT_EN, GAMMA_LUT_EN);
		lut = (struct drm_color_lut *)state->gamma_lut->data;
		for (i = 0; i < MTK_LUT_SIZE; i++) {
			word = GAMMA_ENTRY(lut[i].red >> 6,
				lut[i].green >> 6, lut[i].blue >> 6);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa
				+ (DISP_GAMMA_LUT + i * 4),
				word, ~0);

			// first & last word for
			//	decreasing/incremental LUT
			if (i == 0)
				word_first = word;
			else if (i == MTK_LUT_SIZE - 1)
				word_last = word;
		}
	}
	if ((word_first - word_last) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			GAMMA_LUT_TYPE, GAMMA_LUT_TYPE);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			0, GAMMA_LUT_TYPE);
		DDPINFO("Incremental LUT\n");
	}
}

static int disp_gamma_disable_mul_en(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	struct mtk_disp_gamma *gamma_priv = comp_to_gamma(comp);
	struct mtk_ddp_comp *companion = gamma_priv->companion;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, 0, GAMMA_MUT_EN);

	if (comp->mtk_crtc->is_dual_pipe && companion)
		cmdq_pkt_write(handle, companion->cmdq_base,
			companion->regs_pa + DISP_GAMMA_CFG, 0, GAMMA_MUT_EN);

	return 0;
}

static int disp_gamma_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void disp_gamma_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&gamma->gamma_is_clock_on, 1);
}

static void disp_gamma_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = gamma->primary_data;

	DDPINFO("%s: compid: %d\n", __func__, comp->id);
	mutex_lock(&primary_data->clk_lock);
	atomic_set(&gamma->gamma_is_clock_on, 0);
	primary_data->need_refinalize = true;
	mtk_ddp_comp_clk_unprepare(comp);
	mutex_unlock(&primary_data->clk_lock);
}

int disp_gamma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_disp_gamma *data = comp_to_gamma(comp);
	struct mtk_disp_gamma_primary *primary_data = data->primary_data;

	switch (cmd) {
	case FORCE_TRIG_CTL:
	{
		uint32_t force_delay_trigger;

		force_delay_trigger = *(uint32_t *)params;
		atomic_set(&primary_data->force_delay_check_trig, force_delay_trigger);
	}
		break;
	case PQ_FILL_COMP_PIPE_INFO:
	{
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_gamma *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_gamma(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_gamma_init_primary_data(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			disp_gamma_init_primary_data(data->companion);
	}
		break;
	case NOTIFY_CONNECTOR_SWITCH:
	{
		DDPMSG("%s, set sram_hw_init 0\n", __func__);
		atomic_set(&primary_data->gamma_sram_hw_init, 0);
		primary_data->relay_state |= (0x1 << PQ_FEATURE_DEFAULT);
	}
		break;
	default:
		break;
	}
	return 0;
}

void disp_gamma_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	disp_gamma_config(comp, cfg, handle);
}

static int disp_gamma_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	/* will only call left path */
	switch (cmd) {
	case PQ_GAMMA_SET_GAMMALUT:
		ret = disp_gamma_cfg_set_gammalut(comp, handle, data);
		break;
	case PQ_GAMMA_SET_12BIT_GAMMALUT:
		ret = disp_gamma_cfg_set_12bit_gammalut(comp, handle, data, data_size);
		break;
	case PQ_GAMMA_DISABLE_MUL_EN:
		ret = disp_gamma_disable_mul_en(comp, handle, data);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_gamma_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	/* will only call left path */
	switch (cmd) {
	default:
		break;
	}
	return ret;
}

static int disp_gamma_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	gamma->set_partial_update = enable;
	gamma->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : gamma->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (gamma->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_GAMMA_SIZE,
				   gamma->roi_height + overhead_v * 2, 0xffff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_GAMMA_SIZE,
				   full_height, 0xffff);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_gamma_funcs = {
	.gamma_set = disp_gamma_set,
	.config = disp_gamma_config,
	.first_cfg = disp_gamma_first_cfg,
	.start = disp_gamma_start,
	.stop = disp_gamma_stop,
	.bypass = disp_gamma_bypass,
	.user_cmd = disp_gamma_user_cmd,
	.io_cmd = disp_gamma_io_cmd,
	.prepare = disp_gamma_prepare,
	.unprepare = disp_gamma_unprepare,
	.config_overhead = disp_gamma_config_overhead,
	.config_overhead_v = disp_gamma_config_overhead_v,
	.pq_frame_config = disp_gamma_frame_config,
	.pq_ioctl_transact = disp_gamma_ioctl_transact,
	.partial_update = disp_gamma_set_partial_update,
};

static int disp_gamma_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void disp_gamma_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_gamma_component_ops = {
	.bind = disp_gamma_bind, .unbind = disp_gamma_unbind,
};

void disp_gamma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x24, 0x28);
	mtk_cust_dump_reg(baddr, 0x54, 0x58, 0x5c, 0x50);
	mtk_cust_dump_reg(baddr, 0x14, 0x20, 0x700, 0xb00);
}

void disp_gamma_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp),
			gamma_data->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0xff0; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && gamma_data->companion) {
		baddr = gamma_data->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(gamma_data->companion),
				&gamma_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(gamma_data->companion));
		for (k = 0; k <= 0xff0; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(gamma_data->companion));
	}
}

static void disp_gamma_parse_dts(const struct device_node *np,
	struct mtk_ddp_comp *comp)
{
	struct gamma_color_protect_mode color_protect_mode;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if (of_property_read_u32(np, "gamma-data-mode",
		&gamma->primary_data->data_mode)) {
		DDPPR_ERR("comp_id: %d, gamma_data_mode = %d\n",
			comp->id, gamma->primary_data->data_mode);
		gamma->primary_data->data_mode = HW_8BIT;
	}

	if (of_property_read_u32(np, "color-protect-lsb",
		&gamma->primary_data->color_protect.gamma_color_protect_lsb)) {
		DDPPR_ERR("comp_id: %d, color_protect_lsb = %d\n",
			comp->id, gamma->primary_data->color_protect.gamma_color_protect_lsb);
		gamma->primary_data->color_protect.gamma_color_protect_lsb = 0;
	}

	if (of_property_read_u32(np, "color-protect-red",
		&color_protect_mode.red_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_red = %d\n",
			comp->id, color_protect_mode.red_support);
		color_protect_mode.red_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-green",
		&color_protect_mode.green_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_green = %d\n",
			comp->id, color_protect_mode.green_support);
		color_protect_mode.green_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-blue",
		&color_protect_mode.blue_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_blue = %d\n",
			comp->id, color_protect_mode.blue_support);
		color_protect_mode.blue_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-black",
		&color_protect_mode.black_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_black = %d\n",
			comp->id, color_protect_mode.black_support);
		color_protect_mode.black_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-white",
		&color_protect_mode.white_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_white = %d\n",
			comp->id, color_protect_mode.white_support);
		color_protect_mode.white_support = 0;
	}

	gamma->primary_data->color_protect.gamma_color_protect_support =
		color_protect_mode.red_support << 4 |
		color_protect_mode.green_support << 5 |
		color_protect_mode.blue_support << 6 |
		color_protect_mode.black_support << 7 |
		color_protect_mode.white_support << 8;
}

static int disp_gamma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_gamma *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_GAMMA);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	disp_gamma_parse_dts(dev->of_node, &priv->ddp_comp);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_gamma_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_gamma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int disp_gamma_remove(struct platform_device *pdev)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_gamma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

struct mtk_disp_gamma_data legacy_driver_data = {
	.support_gamma_gain = false,
	.gamma_gain_range = 8192,
};

struct mtk_disp_gamma_data mt6985_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 8192,
};

struct mtk_disp_gamma_data mt6897_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 8192,
};

struct mtk_disp_gamma_data mt6899_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 16384,
};

struct mtk_disp_gamma_data mt6989_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 16384,
};

struct mtk_disp_gamma_data mt6878_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 16384,
};

struct mtk_disp_gamma_data mt6991_driver_data = {
	.support_gamma_gain = true,
	.gamma_gain_range = 16384,
};

static const struct of_device_id mtk_disp_gamma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6761-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6765-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6768-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6885-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6877-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6781-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6853-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6833-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6983-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6895-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6879-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6985-disp-gamma",
	  .data = &mt6985_driver_data,},
	{ .compatible = "mediatek,mt6886-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6835-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6897-disp-gamma",
	  .data = &mt6897_driver_data,},
	{ .compatible = "mediatek,mt6989-disp-gamma",
	  .data = &mt6989_driver_data,},
	{ .compatible = "mediatek,mt6878-disp-gamma",
	  .data = &mt6878_driver_data,},
	{ .compatible = "mediatek,mt6991-disp-gamma",
	  .data = &mt6991_driver_data,},
	{ .compatible = "mediatek,mt6899-disp-gamma",
	  .data = &mt6899_driver_data,},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_gamma_driver_dt_match);

struct platform_driver mtk_disp_gamma_driver = {
	.probe = disp_gamma_probe,
	.remove = disp_gamma_remove,
	.driver = {

			.name = "mediatek-disp-gamma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_gamma_driver_dt_match,
		},
};

void disp_gamma_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_GAMMA, 0);
	struct mtk_disp_gamma *gamma;
	struct DISP_GAMMA_12BIT_LUT_T *gamma_12b_lut;
	int i;

	DDPINFO("[GAMMA debug]: %s\n", opt);
	if (strncmp(opt, "dumpsram", 8) == 0) {
		if (!comp) {
			DDPPR_ERR("[GAMMA debug] null pointer!\n");
			return;
		}
		gamma = comp_to_gamma(comp);
		gamma_12b_lut = &gamma->primary_data->gamma_12b_lut;
		for (i = 0; i < 50; i += 4) {
			DDPMSG("[debug] gamma_lut0 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					i, gamma_12b_lut->lut_0[i], gamma_12b_lut->lut_0[i + 1],
					gamma_12b_lut->lut_0[i + 2], gamma_12b_lut->lut_0[i + 3]);
		}
		for (i = 0; i < 50; i += 4) {
			DDPMSG("[debug] gamma_lut1 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					i, gamma_12b_lut->lut_1[i], gamma_12b_lut->lut_1[i + 1],
					gamma_12b_lut->lut_1[i + 2], gamma_12b_lut->lut_1[i + 3]);
		}
	}
}

unsigned int disp_gamma_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_gamma *gamma_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_GAMMA, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	gamma_data = comp_to_gamma(comp);

	return gamma_data->primary_data->relay_state != 0 ? 1 : 0;
}
