// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#include "mt6991-afe-common.h"
#include <linux/pm_runtime.h>

#include "mtk-sram-manager.h"
#include "../common/mtk-afe-fe-dai.h"
#include "../../codecs/mt6681-private.h"

/* don't use this directly if not necessary */
static struct mtk_base_afe *local_afe;
static struct audio_swpm_data local_audio_data;

int mt6991_set_local_afe(struct mtk_base_afe *afe)
{
	local_afe = afe;
	return 0;
}

unsigned int mt6991_swpm_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 32000:
		return AUDIO_RATE_32K;
	case 48000:
		return AUDIO_RATE_48K;
	case 96000:
		return AUDIO_RATE_96K;
	case 192000:
		return AUDIO_RATE_192K;
	case 384000:
		return AUDIO_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, AUDIO_RATE_48K);
		return AUDIO_RATE_48K;
	}
}

unsigned int mt6991_general_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_IPM2P0_RATE_8K;
	case 11025:
		return MTK_AFE_IPM2P0_RATE_11K;
	case 12000:
		return MTK_AFE_IPM2P0_RATE_12K;
	case 16000:
		return MTK_AFE_IPM2P0_RATE_16K;
	case 22050:
		return MTK_AFE_IPM2P0_RATE_22K;
	case 24000:
		return MTK_AFE_IPM2P0_RATE_24K;
	case 32000:
		return MTK_AFE_IPM2P0_RATE_32K;
	case 44100:
		return MTK_AFE_IPM2P0_RATE_44K;
	case 48000:
		return MTK_AFE_IPM2P0_RATE_48K;
	case 88200:
		return MTK_AFE_IPM2P0_RATE_88K;
	case 96000:
		return MTK_AFE_IPM2P0_RATE_96K;
	case 176400:
		return MTK_AFE_IPM2P0_RATE_176K;
	case 192000:
		return MTK_AFE_IPM2P0_RATE_192K;
	/* not support 260K */
	case 352800:
		return MTK_AFE_IPM2P0_RATE_352K;
	case 384000:
		return MTK_AFE_IPM2P0_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_IPM2P0_RATE_48K);
		return MTK_AFE_IPM2P0_RATE_48K;
	}
}

unsigned int mt6991_general_rate_transform_inverse(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case MTK_AFE_IPM2P0_RATE_8K:
		return 8000;
	case MTK_AFE_IPM2P0_RATE_11K:
		return 11025;
	case MTK_AFE_IPM2P0_RATE_12K:
		return 12000;
	case MTK_AFE_IPM2P0_RATE_16K:
		return 16000;
	case MTK_AFE_IPM2P0_RATE_22K:
		return 22050;
	case MTK_AFE_IPM2P0_RATE_24K:
		return 24000;
	case MTK_AFE_IPM2P0_RATE_32K:
		return 32000;
	case MTK_AFE_IPM2P0_RATE_44K:
		return 44100;
	case MTK_AFE_IPM2P0_RATE_48K:
		return 48000;
	case MTK_AFE_IPM2P0_RATE_88K:
		return 88200;
	case MTK_AFE_IPM2P0_RATE_96K:
		return 96000;
	case MTK_AFE_IPM2P0_RATE_176K:
		return 176400;
	case MTK_AFE_IPM2P0_RATE_192K:
		return 192000;
	/* not support 260K */
	case MTK_AFE_IPM2P0_RATE_352K:
		return 352800;
	case MTK_AFE_IPM2P0_RATE_384K:
		return 384000;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, 48000);
		return 48000;
	}
}

static unsigned int pcm_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_PCM_RATE_8K;
	case 16000:
		return MTK_AFE_PCM_RATE_16K;
	case 32000:
		return MTK_AFE_PCM_RATE_32K;
	case 48000:
		return MTK_AFE_PCM_RATE_48K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_PCM_RATE_32K);
		return MTK_AFE_PCM_RATE_32K;
	}
}

unsigned int mt6991_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6991_DAI_PCM_0:
	case MT6991_DAI_PCM_1:
		return pcm_rate_transform(dev, rate);
	default:
		return mt6991_general_rate_transform(dev, rate);
	}
}

int mt6991_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data)
{
	struct mt6991_afe_private *afe_priv = afe->platform_priv;
	void *temp_data;

	temp_data = devm_kzalloc(afe->dev,
				 priv_size,
				 GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	if (priv_data)
		memcpy(temp_data, priv_data, priv_size);

	if (id < 0 || id >= MT6991_DAI_NUM) {
		dev_info(afe->dev, "%s(), invalid DAI id %d\n", __func__, id);
		return -EINVAL;
	}

	afe_priv->dai_priv[id] = temp_data;

	return 0;
}

/* DC compensation */
int mt6991_enable_dc_compensation(bool enable)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6991_enable_dc_compensation);

int mt6991_set_lch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6991_set_lch_dc_compensation);

int mt6991_set_rch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mt6991_set_rch_dc_compensation);

int mt6991_adda_dl_gain_control(bool mute)
{
	unsigned int dl_gain_ctl;

	if (!local_afe)
		return -EPERM;

	if (mute)
		dl_gain_ctl = MTK_AFE_ADDA_DL_GAIN_MUTE;
	else
		dl_gain_ctl = 0xf74ff74f;

	dev_info(local_afe->dev, "%s(), adda_dl_gain %x\n",
		 __func__, dl_gain_ctl);

	return 0;
}
EXPORT_SYMBOL(mt6991_adda_dl_gain_control);

int mt6991_get_output_device(struct mtk_base_afe *afe)
{
	unsigned int value = 0;
	struct mt6991_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component = afe_priv->codec_component;

	value = mt6681_get_working_device(codec_component, true);
	if (value == HP_OUTPUT_DEVICE)
		return AUDIO_OUTPUT_HEADPHONE;
	else if (value == RCV_OUTPUT_DEVICE)
		return AUDIO_OUTPUT_RECEIVER;

	regmap_read(afe->regmap, ETDM_OUT4_CON0, &value);
	value = (value & OUT_REG_ETDM_OUT_EN_MASK_SFT) >>
		OUT_REG_ETDM_OUT_EN_SFT;
	if (value) {
		regmap_read(afe->regmap, ETDM_IN4_CON0, &value);
		value = (value & REG_ETDM_IN_EN_MASK_SFT) >> REG_ETDM_IN_EN_SFT;
		if (value)
			return AUDIO_OUTPUT_SPEAKER;
		else
			return AUDIO_OUTPUT_RECEIVER;
	}

	return AUDIO_OUTPUT_INVALID;
}

int mt6991_get_input_device(struct mtk_base_afe *afe)
{
	unsigned int value = 0, adc_num = 0;
	struct mt6991_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component = afe_priv->codec_component;

	value = mt6681_get_working_device(codec_component, false);
	adc_num = mt6681_get_adc_num(codec_component);

	if (value == HEADSET_MIC_INPUT_DEVICE)
		return AUDIO_INPUT_HEADSET_MIC + (adc_num << AUDIO_INPUT_ADC_SHIFT);
	else if (value == DUAL_MIC_INPUT_DEVICE)
		return AUDIO_INPUT_BUILTIN_MIC_DUAL + (adc_num << AUDIO_INPUT_ADC_SHIFT);
	else if (value == THREE_MIC_INPUT_DEVICE)
		return AUDIO_INPUT_BUILTIN_MIC_THREE + (adc_num << AUDIO_INPUT_ADC_SHIFT);

	return AUDIO_INPUT_INVALID;
}

void mt6991_aud_swpm_power_off(void)
{
	memset(&local_audio_data, 0, sizeof(local_audio_data));
	local_audio_data.freq_clock = 26;
}

void mt6991_aud_update_power_scenario(void)
{
	struct mt6991_afe_private *afe_priv;
	struct mtk_afe_adda_priv *adda_priv;
	struct snd_soc_component *codec_component;
	struct audio_swpm_data audio_data = {0};
	unsigned int value = 0, reg_val = 0;
	int dl_ch = 0, ul_ch = 0;
	int ul_hifi = 0, dl_hifi = 0;

	if (!local_afe) {
		pr_info("%s(), local_afe is NULL ptr\n", __func__);
		local_audio_data = audio_data;
		return;
	}
	afe_priv = local_afe->platform_priv;
	if (!afe_priv) {
		pr_info("%s(), afe_priv is NULL ptr\n", __func__);
		goto exit;
	}

	adda_priv = afe_priv->dai_priv[MT6991_DAI_ADDA];
	codec_component = afe_priv->codec_component;

	if (!codec_component) {
		dev_info(local_afe->dev, "%s(), codec_component is NULL ptr\n", __func__);
		goto exit;
	}

	audio_data.freq_clock = 26;
	regmap_read(local_afe->regmap, AUDIO_TOP_CON4, &reg_val);
	value = (reg_val & CG_AUDIO_F26M_CK_MASK_SFT) >> CG_AUDIO_F26M_CK_SFT;
	if (value || (reg_val == 0x78fd5265)) {
		dev_info(local_afe->dev, "%s(), afe agent is not enable\n", __func__);
		audio_data.afe_on = AUDIO_AFE_OFF;
		audio_data.D0_ratio = 0;
		goto exit;
	}
	audio_data.afe_on = AUDIO_AFE_ON;
	audio_data.D0_ratio = 100;

	// get input/output device
	audio_data.output_device = mt6991_get_output_device(local_afe);
	audio_data.input_device = mt6991_get_input_device(local_afe);
	if (audio_data.input_device != AUDIO_INPUT_INVALID &&
	    audio_data.output_device != AUDIO_OUTPUT_INVALID) {
		audio_data.user_case = AUDIO_USER_TALKING;
		// TODO hardcode for VOIP
		ul_ch = 3;
		dl_ch = 2;
		ul_hifi = mt6681_get_adda_hifi_mode(codec_component, false)
			  ? AUDIO_ADDA_UL_HIFI : AUDIO_ADDA_UL_LP;
		if (audio_data.output_device == AUDIO_OUTPUT_HEADPHONE)
			dl_hifi = mt6681_get_adda_hifi_mode(codec_component, true)
				  ? AUDIO_ADDA_DL_HIFI : AUDIO_ADDA_DL_LP;
		else
			audio_data.output_device = AUDIO_OUTPUT_RECEIVER;
		audio_data.sample_rate = AUDIO_RATE_48K;

	} else if (audio_data.input_device != AUDIO_INPUT_INVALID) {
		audio_data.user_case = AUDIO_USER_RECORD;
		ul_ch = mtk_get_channel_value();
		ul_hifi = mt6681_get_adda_hifi_mode(codec_component, false)
			  ? AUDIO_ADDA_UL_HIFI : AUDIO_ADDA_UL_LP;
		value = adda_priv->ul_rate;
		audio_data.sample_rate = mt6991_swpm_rate_transform(local_afe->dev, value);
	} else if (audio_data.output_device != AUDIO_OUTPUT_INVALID) {
		audio_data.user_case = AUDIO_USER_PLAYBACK;
		dl_ch = mtk_get_channel_value();
		if (audio_data.output_device == AUDIO_OUTPUT_HEADPHONE) {
			dl_hifi = mt6681_get_adda_hifi_mode(codec_component, true)
				  ? AUDIO_ADDA_DL_HIFI : AUDIO_ADDA_DL_LP;
			value = adda_priv->dl_rate;
			audio_data.sample_rate = mt6991_swpm_rate_transform(local_afe->dev, value);
		} else {
			audio_data.sample_rate = AUDIO_RATE_48K;
		}

	} else {
		audio_data.user_case = AUDIO_USER_INVALID;
		audio_data.adda_mode = AUDIO_ADDA_INVALID;
		audio_data.sample_rate = AUDIO_RATE_INVALID;
		audio_data.channel_num = AUDIO_CHANNEL_INVALID;
		goto exit;
	}
	ul_hifi = ul_hifi << AUDIO_ADDA_SHIFT;
	ul_ch = ul_ch << AUDIO_CHANNEL_SHIFT;
	audio_data.adda_mode = ul_hifi + dl_hifi;
	audio_data.channel_num = ul_ch + dl_ch;

exit:
	local_audio_data = audio_data;
	pr_debug("%s(), updates data ON %u, user %u, out %u, in %u, adda %u, rate %u, ch %u, freq %u, D0 %u\n",
		__func__, local_audio_data.afe_on,
		local_audio_data.user_case, local_audio_data.output_device,
		local_audio_data.input_device, local_audio_data.adda_mode,
		local_audio_data.sample_rate, local_audio_data.channel_num,
		local_audio_data.freq_clock, local_audio_data.D0_ratio);
}

bool check_swpm_data_valid(struct audio_swpm_data data)
{
	// the table is defined by DE in TPPA
	struct audio_swpm_data valid_table[] = {
		{0, 3, 1, 18, 16, 2, 17, 0, 0},
		{0, 2, 0, 67, 16, 2, 64, 0, 0},
		{0, 2, 0, 35, 16, 2, 32, 0, 0},
		{0, 2, 0, 35, 32, 2, 32, 0, 0},
		{0, 1, 2, 0, 0, 2, 4, 0, 0},
		{0, 1, 3, 0, 1, 4, 2, 0, 0},
		{0, 1, 3, 0, 1, 2, 2, 0, 0},
		{0, 1, 3, 0, 2, 2, 2, 0, 0},
		{0, 3, 1, 52, 32, 2, 50, 0, 0}
	};
	int i = 0;

	for (i = 0; i < sizeof(valid_table) / sizeof(struct audio_swpm_data); ++i) {
		if (valid_table[i].user_case == data.user_case &&
		    valid_table[i].output_device == data.output_device &&
		    valid_table[i].input_device == data.input_device &&
		    valid_table[i].adda_mode == data.adda_mode &&
		    valid_table[i].sample_rate == data.sample_rate &&
		    valid_table[i].channel_num == data.channel_num) {
			return true;
		}
	}
	pr_debug("%s(), wrong ON %u, user %u, out %u, in %u, adda %u, rate %u, ch %u, freq %u, D0 %u\n",
		__func__, data.afe_on,
		data.user_case, data.output_device,
		data.input_device, data.adda_mode,
		data.sample_rate, data.channel_num,
		data.freq_clock, data.D0_ratio);

	return false;
}

void *mt6991_aud_get_power_scenario(void)
{
	struct audio_swpm_data default_state;

	// default value for playback
	if (local_audio_data.user_case != AUDIO_USER_TALKING) {
		default_state.adda_mode = 0;
		default_state.afe_on = AUDIO_AFE_ON;
		default_state.user_case = AUDIO_USER_PLAYBACK;
		default_state.output_device = AUDIO_OUTPUT_SPEAKER;
		default_state.input_device = AUDIO_INPUT_INVALID;
		default_state.adda_mode = AUDIO_ADDA_INVALID;
		default_state.sample_rate = AUDIO_RATE_48K;
		default_state.channel_num = AUDIO_CHANNEL_DL_4;
		default_state.D0_ratio = 100;
		default_state.freq_clock = 26;
	} else {
		default_state.adda_mode = 0;
		default_state.afe_on = AUDIO_AFE_ON;
		default_state.user_case = AUDIO_USER_TALKING;
		default_state.output_device = AUDIO_OUTPUT_RECEIVER;
		default_state.input_device = AUDIO_INPUT_BUILTIN_MIC_THREE + (AUDIO_INPUT_ADC_3 << 4);
		default_state.adda_mode = AUDIO_ADDA_UL_HIFI << 4;
		default_state.sample_rate = AUDIO_RATE_48K;
		default_state.channel_num = AUDIO_CHANNEL_DL_2 + (AUDIO_CHANNEL_UL_3 << 4);
		default_state.D0_ratio = 100;
		default_state.freq_clock = 26;
	}

	if (local_audio_data.afe_on != AUDIO_AFE_OFF && check_swpm_data_valid(local_audio_data) == false)
		local_audio_data = default_state;

	return (void *)&local_audio_data;
}
EXPORT_SYMBOL(mt6991_aud_get_power_scenario);
