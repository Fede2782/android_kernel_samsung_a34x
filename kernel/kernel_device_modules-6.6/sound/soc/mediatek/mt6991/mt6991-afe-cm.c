// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "mtk-afe-fe-dai.h"
#include "mtk-base-afe.h"

#include "mt6991-afe-cm.h"
#include "mt6991-afe-common.h"

static unsigned int cm0_rate;
static unsigned int cm1_rate;
static unsigned int cm2_rate;
static unsigned int cm0_mux_switch;
static unsigned int cm1_mux_switch;
static unsigned int cm2_mux_switch;

struct mtk_base_cm_data {
	int reg;
	int on_mask;
	int on_shift;
	int on_bypass_mask;
	int on_bypass_shift;
	int on_bypass_mask_shift;
	int rate_mask;
	int rate_shift;
	int update_mask;
	int update_shift;
	int swap_mask;
	int swap_shift;
	int ch_mask;
	int ch_shift;
	int mux_mask;
	int mux_shift;
};

const struct mtk_base_cm_data cm_data[CM_NUM] = {
	[CM0] = {
		.reg = AFE_CM0_CON0,
		.on_mask = AFE_CM0_ON_MASK,
		.on_shift = AFE_CM0_ON_SFT,
		.on_bypass_mask = AFE_CM0_BYPASS_MODE_MASK,
		.on_bypass_shift = AFE_CM0_BYPASS_MODE_SFT,
		.on_bypass_mask_shift = AFE_CM0_BYPASS_MODE_MASK_SFT,
		.rate_mask = AFE_CM0_1X_EN_SEL_FS_MASK,
		.rate_shift = AFE_CM0_1X_EN_SEL_FS_SFT,
		.update_mask = AFE_CM0_UPDATE_CNT_MASK,
		.update_shift = AFE_CM0_UPDATE_CNT_SFT,
		.swap_mask = AFE_CM0_BYTE_SWAP_MASK,
		.swap_shift = AFE_CM0_BYTE_SWAP_SFT,
		.ch_mask = AFE_CM0_CH_NUM_MASK,
		.ch_shift = AFE_CM0_CH_NUM_SFT,
		.mux_mask = AFE_CM0_OUTPUT_MUX_MASK,
		.mux_shift = AFE_CM0_OUTPUT_MUX_SFT,
	},
	[CM1] = {
		.reg = AFE_CM1_CON0,
		.on_mask = AFE_CM1_ON_MASK,
		.on_shift = AFE_CM1_ON_SFT,
		.on_bypass_mask = AFE_CM1_BYPASS_MODE_MASK,
		.on_bypass_shift = AFE_CM1_BYPASS_MODE_SFT,
		.on_bypass_mask_shift = AFE_CM1_BYPASS_MODE_MASK_SFT,
		.rate_mask = AFE_CM1_1X_EN_SEL_FS_MASK,
		.rate_shift = AFE_CM1_1X_EN_SEL_FS_SFT,
		.update_mask = AFE_CM1_UPDATE_CNT_MASK,
		.update_shift = AFE_CM1_UPDATE_CNT_SFT,
		.swap_mask = AFE_CM1_BYTE_SWAP_MASK,
		.swap_shift = AFE_CM1_BYTE_SWAP_SFT,
		.ch_mask = AFE_CM1_CH_NUM_MASK,
		.ch_shift = AFE_CM1_CH_NUM_SFT,
		.mux_mask = AFE_CM1_OUTPUT_MUX_MASK,
		.mux_shift = AFE_CM1_OUTPUT_MUX_SFT,
	},
	[CM2] = {
		.reg = AFE_CM2_CON0,
		.on_mask = AFE_CM2_ON_MASK,
		.on_shift = AFE_CM2_ON_SFT,
		.on_bypass_mask = AFE_CM2_BYPASS_MODE_MASK,
		.on_bypass_shift = AFE_CM2_BYPASS_MODE_SFT,
		.on_bypass_mask_shift = AFE_CM2_BYPASS_MODE_MASK_SFT,
		.rate_mask = AFE_CM2_1X_EN_SEL_FS_MASK,
		.rate_shift = AFE_CM2_1X_EN_SEL_FS_SFT,
		.update_mask = AFE_CM2_UPDATE_CNT_MASK,
		.update_shift = AFE_CM2_UPDATE_CNT_SFT,
		.swap_mask = AFE_CM2_BYTE_SWAP_MASK,
		.swap_shift = AFE_CM2_BYTE_SWAP_SFT,
		.ch_mask = AFE_CM2_CH_NUM_MASK,
		.ch_shift = AFE_CM2_CH_NUM_SFT,
		.mux_mask = AFE_CM2_OUTPUT_MUX_MASK,
		.mux_shift = AFE_CM2_OUTPUT_MUX_SFT,
	},
};

void mt6991_set_cm_rate(int id, unsigned int rate)
{
	if (id == CM0)
		cm0_rate = rate;
	else if (id == CM1)
		cm1_rate = rate;
	else if (id == CM2)
		cm2_rate = rate;
}
EXPORT_SYMBOL_GPL(mt6991_set_cm_rate);

void mt6991_set_cm_mux(int id, unsigned int mux)
{
	if (id == CM0)
		cm0_mux_switch = mux;
	else if (id == CM1)
		cm1_mux_switch = mux;
	else if (id == CM2)
		cm2_mux_switch = mux;
	else
		return;

	pr_info("%s(), set CM%d mux = %d\n", __func__, id, mux);
}
EXPORT_SYMBOL_GPL(mt6991_set_cm_mux);

int mt6991_get_cm_mux(int id)
{
	int value = 0;

	switch (id) {
	case CM0:
		value = cm0_mux_switch;
		break;
	case CM1:
		value = cm1_mux_switch;
		break;
	case CM2:
		value = cm2_mux_switch;
		break;
	default:
		pr_info("%s(), CM id %d not exist!!\n", __func__, id);
		return 0;
	}

	pr_info("%s(), CM%d value %d\n", __func__, id, value);
	return value;
}
EXPORT_SYMBOL_GPL(mt6991_get_cm_mux);

static int mt6991_convert_cm_ch(unsigned int ch)
{
	return ch - 1;
}

static unsigned int calculate_cm_update(int rate, int ch)
{
	int update_val;

	pr_info("%s(), rate %d, channel %d\n",
		 __func__, rate, ch);

	update_val = (((26000000 / rate) - 10) / (ch / 2)) - 1;

	return (unsigned int)update_val;
}

int mt6991_set_cm(struct mtk_base_afe *afe, int id,
	       unsigned int update, bool swap, unsigned int ch)
{
	unsigned int rate = 0;
	unsigned int mux = 0;
	struct mtk_base_cm_data cm;
	unsigned int samplerate = 0;
	unsigned int update_val = 0;

	pr_info("%s()-0, CM%d, rate %d, update %d, swap %d, ch %d\n",
		__func__, id, rate, update, swap, ch);

	switch (id) {
	case CM0:
		cm = cm_data[id];
		rate = cm0_rate;
		mux = cm0_mux_switch;
		break;
	case CM1:
		cm = cm_data[id];
		rate = cm1_rate;
		mux = cm1_mux_switch;
		break;
	case CM2:
		cm = cm_data[id];
		rate = cm2_rate;
		mux = cm2_mux_switch;
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}
	/* use real samplerate to count */
	samplerate = mt6991_general_rate_transform_inverse(afe->dev, rate);
	update_val = (update == 0x1)? calculate_cm_update(samplerate, (int)ch) : 0x64;

	/* update cnt */
	mtk_regmap_update_bits(afe->regmap, cm.reg,
			       cm.update_mask, update_val, cm.update_shift);

	/* rate */
	mtk_regmap_update_bits(afe->regmap, cm.reg,
			       cm.rate_mask, rate, cm.rate_shift);

	/* ch num */
	ch = mt6991_convert_cm_ch(ch);
	mtk_regmap_update_bits(afe->regmap, cm.reg,
			       cm.ch_mask, ch, cm.ch_shift);

	/* swap */
	mtk_regmap_update_bits(afe->regmap, cm.reg,
			       cm.swap_mask, swap, cm.swap_shift);

	/* mux */
	mtk_regmap_update_bits(afe->regmap, cm.reg,
			       cm.mux_mask, mux, cm.mux_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6991_set_cm);

int mt6991_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en)
{
	struct mtk_base_cm_data cm;

	switch (id) {
	case CM0:
	case CM1:
	case CM2:
		cm = cm_data[id];
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}

	mtk_regmap_update_bits(afe->regmap, cm.reg, cm.on_bypass_mask,
			       en, cm.on_bypass_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6991_enable_cm_bypass);

int mt6991_enable_cm(struct mtk_base_afe *afe, int id, bool en)
{
	struct mtk_base_cm_data cm;

	pr_info("%s, CM%d, en %d\n", __func__, id, en);

	switch (id) {
	case CM0:
	case CM1:
	case CM2:
		cm = cm_data[id];
		break;
	default:
		dev_info(afe->dev, "%s(), CM%d not found\n",
			 __func__, id);
		return 0;
	}
	mtk_regmap_update_bits(afe->regmap, cm.reg, cm.on_mask,
			       en, cm.on_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mt6991_enable_cm);

int mt6991_is_need_enable_cm(struct mtk_base_afe *afe, int id)
{
	unsigned int value = 0;
	struct mtk_base_cm_data cm;

	switch (id) {
	case CM0:
	case CM1:
	case CM2:
		cm = cm_data[id];
		break;
	default:
		pr_info("%s(), CM%d not found\n", __func__, id);
		return 0;
	}

	regmap_read(afe->regmap, cm.reg, &value);
	value &= cm.on_bypass_mask_shift;
	value >>= cm.on_bypass_shift;

	pr_info("%s(), CM%d value %d\n", __func__, id, value);
	if (value != 0x1)
		return true;
	return 0;
}
EXPORT_SYMBOL_GPL(mt6991_is_need_enable_cm);

MODULE_DESCRIPTION("Mediatek afe cm");
MODULE_AUTHOR("yiwen chiou<yiwen.chiou@mediatek.com>");
MODULE_LICENSE("GPL");

