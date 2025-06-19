// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6991-afe-gpio.c  --  Mediatek 6991 afe gpio ctrl
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>

#include "mt6991-afe-common.h"
#include "mt6991-afe-gpio.h"

struct pinctrl *aud_pinctrl;
struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

static struct audio_gpio_attr aud_gpios[MT6991_AFE_GPIO_GPIO_NUM] = {
	[MT6991_AFE_GPIO_DAT_MISO0_OFF] = {"aud-dat-miso0-off", false, NULL},
	[MT6991_AFE_GPIO_DAT_MISO0_ON] = {"aud-dat-miso0-on", false, NULL},
	[MT6991_AFE_GPIO_DAT_MISO1_OFF] = {"aud-dat-miso1-off", false, NULL},
	[MT6991_AFE_GPIO_DAT_MISO1_ON] = {"aud-dat-miso1-on", false, NULL},
	[MT6991_AFE_GPIO_DAT_MOSI_OFF] = {"aud-dat-mosi-off", false, NULL},
	[MT6991_AFE_GPIO_DAT_MOSI_ON] = {"aud-dat-mosi-on", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT0_OFF] = {"aud-gpio-i2sout0-off", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT0_ON] = {"aud-gpio-i2sout0-on", false, NULL},
	[MT6991_AFE_GPIO_I2SIN0_OFF] = {"aud-gpio-i2sin0-off", false, NULL},
	[MT6991_AFE_GPIO_I2SIN0_ON] = {"aud-gpio-i2sin0-on", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT4_OFF] = {"aud-gpio-i2sout4-off", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT4_ON] = {"aud-gpio-i2sout4-on", false, NULL},
	[MT6991_AFE_GPIO_I2SIN4_OFF] = {"aud-gpio-i2sin4-off", false, NULL},
	[MT6991_AFE_GPIO_I2SIN4_ON] = {"aud-gpio-i2sin4-on", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT5_OFF] = {"aud-gpio-i2sout5-off", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT5_ON] = {"aud-gpio-i2sout5-on", false, NULL},
	[MT6991_AFE_GPIO_I2SIN5_OFF] = {"aud-gpio-i2sin5-off", false, NULL},
	[MT6991_AFE_GPIO_I2SIN5_ON] = {"aud-gpio-i2sin5-on", false, NULL},
	[MT6991_AFE_GPIO_I2SIN6_OFF] = {"aud-gpio-i2sin6-off", false, NULL},
	[MT6991_AFE_GPIO_I2SIN6_ON] = {"aud-gpio-i2sin6-on", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT6_OFF] = {"aud-gpio-i2sout6-off", false, NULL},
	[MT6991_AFE_GPIO_I2SOUT6_ON] = {"aud-gpio-i2sout6-on", false, NULL},
#ifndef SKIP_SB_VOW
	[MT6991_AFE_GPIO_VOW_SCP_DMIC_DAT_OFF] = {"vow-scp-dmic-dat-off", false, NULL},
	[MT6991_AFE_GPIO_VOW_SCP_DMIC_DAT_ON] = {"vow-scp-dmic-dat-on", false, NULL},
	[MT6991_AFE_GPIO_VOW_SCP_DMIC_CLK_OFF] = {"vow-scp-dmic-clk-off", false, NULL},
	[MT6991_AFE_GPIO_VOW_SCP_DMIC_CLK_ON] = {"vow-scp-dmic-clk-on", false, NULL},
#endif
	[MT6991_AFE_GPIO_AP_DMIC_OFF] = {"aud-gpio-ap-dmic-off", false, NULL},
	[MT6991_AFE_GPIO_AP_DMIC_ON] = {"aud-gpio-ap-dmic-on", false, NULL},
	// [MT6991_AFE_GPIO_AP_DMIC1_OFF] = {"aud-gpio-ap-dmic1-off", false, NULL},
	// [MT6991_AFE_GPIO_AP_DMIC1_ON] = {"aud-gpio-ap-dmic1-on", false, NULL},
	[MT6991_AFE_GPIO_DAT_MOSI_CH34_OFF] = {"aud-dat-mosi-ch34-off", false, NULL},
	[MT6991_AFE_GPIO_DAT_MOSI_CH34_ON] = {"aud-dat-mosi-ch34-on", false, NULL},
	[MT6991_AFE_GPIO_DAT_MISO_ONLY_OFF] = {"aud-dat-miso-only-off", false, NULL},
	[MT6991_AFE_GPIO_DAT_MISO_ONLY_ON] = {"aud-dat-miso-only-on", false, NULL},
	[MT6991_GPIO_EXT_HP_AMP_OFF] = {"aud-gpio-ext-hp-amp-off", false, NULL},
	[MT6991_GPIO_EXT_HP_AMP_ON] = {"aud-gpio-ext-hp-amp-on", false, NULL},
};

static DEFINE_MUTEX(gpio_request_mutex);

int mt6991_afe_gpio_init(struct mtk_base_afe *afe)
{
	int ret;
	int i = 0;

	aud_pinctrl = devm_pinctrl_get(afe->dev);
	if (IS_ERR(aud_pinctrl)) {
		ret = PTR_ERR(aud_pinctrl);
		dev_info(afe->dev, "%s(), ret %d, cannot get aud_pinctrl!\n",
			__func__, ret);
		return -ENODEV;
	}

	for (i = 0; i < MT6991_AFE_GPIO_GPIO_NUM; i++) {
		if (aud_gpios[i].name == NULL) {
			dev_info(afe->dev, "%s(), gpio id %d not define!!!\n",
			 __func__, i);
#ifndef SKIP_SB_GPIO
			AUDIO_AEE("gpio not define");
#endif
		}
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(aud_pinctrl,
					aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			dev_info(afe->dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
				__func__, aud_gpios[i].name, ret);
		} else
			aud_gpios[i].gpio_prepare = true;
	}

	/* gpio status init */
	mt6991_afe_gpio_request(afe, false, MT6991_DAI_ADDA, 0);
	mt6991_afe_gpio_request(afe, false, MT6991_DAI_ADDA, 1);

	return 0;
}

static int mt6991_afe_gpio_select(struct mtk_base_afe *afe,
				  enum mt6991_afe_gpio type)
{
	int ret = 0;

	if (type >= MT6991_AFE_GPIO_GPIO_NUM) {
		dev_info(afe->dev, "%s(), error, invalid gpio type %d\n",
			__func__, type);
		return -EINVAL;
	}

	if (!aud_gpios[type].gpio_prepare) {
		dev_info(afe->dev, "%s(), error, gpio type %d not prepared\n",
			 __func__, type);
		return -EIO;
	}

	ret = pinctrl_select_state(aud_pinctrl,
				   aud_gpios[type].gpioctrl);
	if (ret)
		dev_info(afe->dev, "%s(), error, can not set gpio type %d\n",
			__func__, type);

	return ret;
}

static int mt6991_afe_gpio_adda_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6991_afe_gpio_select(afe,
					      MT6991_AFE_GPIO_DAT_MOSI_ON);
	else
		return mt6991_afe_gpio_select(afe,
					      MT6991_AFE_GPIO_DAT_MOSI_OFF);
}

static int mt6991_afe_gpio_adda_ul(struct mtk_base_afe *afe, bool enable)
{
	struct mt6991_afe_private *afe_priv = afe->platform_priv;

	if (enable) {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_ON);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO0_ON);
		}
	} else {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_OFF);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO0_OFF);
		}
	}
}

static int mt6991_afe_gpio_adda_ch34_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6991_afe_gpio_select(afe,
					      MT6991_AFE_GPIO_DAT_MOSI_CH34_ON);
	else
		return mt6991_afe_gpio_select(afe,
					      MT6991_AFE_GPIO_DAT_MOSI_CH34_OFF);
}

static int mt6991_afe_gpio_adda_ch34_ul(struct mtk_base_afe *afe, bool enable)
{
	struct mt6991_afe_private *afe_priv = afe->platform_priv;

	if (enable) {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_ON);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO0_ON);
		}
	} else {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_OFF);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO0_OFF);
		}
	}
}

static int mt6991_afe_gpio_adda_ch56_ul(struct mtk_base_afe *afe, bool enable)
{
	struct mt6991_afe_private *afe_priv = afe->platform_priv;

	if (enable) {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO0_ON);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO1_ON);
		}
	} else {
		if (afe_priv->audio_r_miso1_enable == 1) {
			return mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO0_ON);
		} else {
			return mt6991_afe_gpio_select(afe,
						      MT6991_AFE_GPIO_DAT_MISO1_OFF);
		}
	}
}

int mt6991_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink)
{
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
	struct mt6991_afe_private *afe_priv = afe->platform_priv;
	struct mtk_clk_ao_attr *dai_attr;
	bool clk_ao;

	if (dai >= MT6991_DAI_I2S_IN0 && dai <= MT6991_DAI_I2S_OUT6) {
		dai_attr = &(afe_priv->clk_ao_data[dai]);
		clk_ao = dai_attr->apll_ao || dai_attr->mclk_ao ||
				dai_attr->bclk_ao || dai_attr->lrck_ao;
	}
#endif

	mutex_lock(&gpio_request_mutex);
	switch (dai) {
	case MT6991_DAI_ADDA:
		if (uplink)
			mt6991_afe_gpio_adda_ul(afe, enable);
		else
			mt6991_afe_gpio_adda_dl(afe, enable);
		break;
	case MT6991_DAI_ADDA_CH34:
		if (uplink)
			mt6991_afe_gpio_adda_ch34_ul(afe, enable);
		else
			mt6991_afe_gpio_adda_ch34_dl(afe, enable);
		break;
	case MT6991_DAI_ADDA_CH56:
		if (uplink)
			mt6991_afe_gpio_adda_ch56_ul(afe, enable);
		break;
	case MT6991_DAI_I2S_IN0:
	case MT6991_DAI_I2S_OUT0:
		if (enable) {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN0_ON);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT0_ON);
		} else {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN0_OFF);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT0_OFF);
		}
		break;
	case MT6991_DAI_I2S_IN1:
	case MT6991_DAI_I2S_OUT1:
		break;
	case MT6991_DAI_I2S_IN4:
	case MT6991_DAI_I2S_OUT4:
		if (enable) {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN4_ON);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT4_ON);
		} else {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN4_OFF);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT4_OFF);
		}
		break;
	case MT6991_DAI_I2S_IN5:
		if (enable)
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN5_ON);
		else
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN5_OFF);
		break;
	case MT6991_DAI_I2S_OUT5:
		if (enable)
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT5_ON);
		else {
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO)
			if (!clk_ao)
#endif
				mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT5_OFF);
		}
		break;
	case MT6991_DAI_I2S_IN6:
	case MT6991_DAI_I2S_OUT6:
		if (enable) {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN6_ON);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT6_ON);
		} else {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SIN6_OFF);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_I2SOUT6_OFF);
		}
		break;
	case MT6991_DAI_VOW:
		break;
	case MT6991_DAI_VOW_SCP_DMIC:
#ifndef SKIP_SB_VOW
		if (enable) {
			mt6991_afe_gpio_select(afe,
					       MT6991_AFE_GPIO_VOW_SCP_DMIC_CLK_ON);
			mt6991_afe_gpio_select(afe,
					       MT6991_AFE_GPIO_VOW_SCP_DMIC_DAT_ON);
		} else {
			mt6991_afe_gpio_select(afe,
					       MT6991_AFE_GPIO_VOW_SCP_DMIC_CLK_OFF);
			mt6991_afe_gpio_select(afe,
					       MT6991_AFE_GPIO_VOW_SCP_DMIC_DAT_OFF);
		}
#endif
		break;
	case MT6991_DAI_MTKAIF:
		if (enable) {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_ON);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO0_ON);
		} else {
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO1_OFF);
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO0_OFF);
		}
		break;
	case MT6991_DAI_MISO_ONLY:
		if (enable)
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO_ONLY_ON);
		else
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_DAT_MISO_ONLY_OFF);
		break;
	case MT6991_DAI_AP_DMIC:
		if (enable)
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_AP_DMIC_ON);
		else
			mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_AP_DMIC_OFF);
		break;
	case MT6991_DAI_AP_DMIC_CH34:
		dev_info(afe->dev, "%s(), DMIC1 is not enable, need GPIO number\n", __func__);
		// if (enable)
		//	mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_AP_DMIC1_ON);
		// else
		//	mt6991_afe_gpio_select(afe, MT6991_AFE_GPIO_AP_DMIC1_OFF);
		break;
	case MT6991_GPIO_EXT_HP_AMP:
		if (enable)
			mt6991_afe_gpio_select(afe, MT6991_GPIO_EXT_HP_AMP_ON);
		else
			mt6991_afe_gpio_select(afe, MT6991_GPIO_EXT_HP_AMP_OFF);
		break;
	default:
		mutex_unlock(&gpio_request_mutex);
		dev_info(afe->dev, "%s(), invalid dai %d\n", __func__, dai);
		return -EINVAL;
	}
	mutex_unlock(&gpio_request_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mt6991_afe_gpio_request);

bool mt6991_afe_gpio_is_prepared(enum mt6991_afe_gpio type)
{
	return aud_gpios[type].gpio_prepare;
}
EXPORT_SYMBOL(mt6991_afe_gpio_is_prepared);

