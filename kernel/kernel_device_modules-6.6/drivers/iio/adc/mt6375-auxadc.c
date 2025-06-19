// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiaEn Wu <chiaen_wu@richtek.com>
 */

#include <dt-bindings/iio/adc/mediatek,mt6375_auxadc.h>
#include <dt-bindings/iio/adc/mediatek,mt6379_auxadc.h>
#include <linux/alarmtimer.h>
#include <linux/bitfield.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <linux/iio/iio.h>
#include <linux/iio/iio-opaque.h>

#define FGADC_R_CON0			0x2E5
#define SYSTEM_INFO_CON2_H		0x2FE
#define HK_TOP_RST_CON0			0x30F
#define HK_TOP_INT_CON0			0x310
#define HK_TOP_INT_CON0_SET		0x311
#define HK_TOP_INT_CON0_CLR		0x312
#define HK_TOP_INT_CON1_SET		0x314
#define HK_TOP_INT_CON1_CLR		0x315
#define HK_TOP_INT_MASK_CON0		0x316
#define HK_TOP_INT_MASK_CON0_SET	0x317
#define HK_TOP_INT_MASK_CON0_CLR	0x318
#define HK_TOP_INT_MASK_CON1		0x319
#define HK_TOP_INT_STATUS0		0x31C
#define HK_TOP_INT_RAW_STATUS1		0x31F
#define HK_TOP_WKEY			0x328
#define AUXADC_OUT_CH3			0x408
#define AUXADC_OUT_CH11			0x40A
#define AUXADC_OUT_CH0			0x410
#define AUXADC_OUT_IMP_AVG		0x41C
#define AUXADC_RQST0			0x438
#define AUXADC_IMP0			0x4A8
#define AUXADC_IMP1			0x4A9
#define RG_AUXADC_LBAT0			0x4AD
#define RG_AUXADC_LBAT2_0		0x4B9
#define RG_AUXADC_NAG_0			0x4D2

/* MT6375 */
#define MT6375_RG_BM_BASE		0x200
#define MT6375_RG_HK1_BASE		0x300
#define MT6375_RG_HK2_BASE		0x400

/* MT6379 */
#define MT6379_RG_BAT1_BM_BASE		0x700
#define MT6379_RG_BAT1_HK1_BASE		0x800
#define MT6379_RG_BAT1_HK2_BASE		0x900
#define MT6379_RG_BAT2_BM_BASE		0xA00
#define MT6379_RG_BAT2_HK1_BASE		0xB00
#define MT6379_RG_BAT2_HK2_BASE		0xC00

#define MT6379_RG_CORE_CTRL0		0x001
#define MT6379_RG_GM30_EVT		0x068
#define MT6379_RG_SPMI_TXDRV2		0x02B

#define MT6379_MASK_CELL_COUNT		BIT(7)
#define MT6379_MASK_RCS_INT_DONE	BIT(0)
#define MT6379_MASK_HK1_EVT		BIT(4)
#define MT6379_MASK_HK2_EVT		BIT(2)
#define MT6379_MASK_GM_LDO_EVT		BIT(0)

#define RG_BASE_CMP(rg1, rg2)		(((rg1) >> 8) == ((rg2) >> 8))
#define XLATE_NEW_ADDR(addr, new_base)	(((addr) & 0x00ff) + (new_base))

#define VBAT0_FLAG			BIT(0)
#define RG_RESET_MASK			BIT(1)
#define VREF_ENMASK			BIT(4)
#define BATON_ENMASK			BIT(3)
#define BATSNS_ENMASK			BIT(0)
#define ADC_OUT_RDY			BIT(7)
#define AUXADC_IMP_ENMASK		BIT(0)
#define AUXADC_IMP_PRDSEL_MASK		GENMASK(1, 0)
#define AUXADC_IMP_CNTSEL_MASK		GENMASK(3, 2)
#define AUXADC_IMP_CNTSEL_SHFT		2
#define INT_RAW_AUXADC_IMP		BIT(0)
#define NUM_IRQ_REG			2

#define AUXADC_LBAT_EN_MASK		BIT(0)
#define AUXADC_LBAT2_EN_MASK		BIT(0)
#define AUXADC_NAG_IRQ_EN_MASK		BIT(5)
#define AUXADC_NAG_EN_MASK		BIT(0)

struct dev_constraint {
	unsigned int rg_bm_base;
	unsigned int rg_hk1_base;
	unsigned int rg_hk2_base;
	unsigned int gm30_evt_mask;
	const unsigned int bat_idx;
	int (*pre_irq_handler)(void *data);
	int (*post_irq_handler)(void *data);
	const char *pmic_compatible;
};

struct mt6375_priv {
	struct device *dev;
	struct regmap *regmap;
	struct irq_domain *domain;
	struct irq_chip irq_chip;
	struct mutex adc_lock;
	struct mutex irq_lock;
	struct regulator *isink_load;
	struct alarm vbat0_alarm;
	struct work_struct vbat0_work;
	struct wakeup_source *vbat0_ws;
	struct power_supply *gauge_psy;
	struct power_supply *battery_psy;
	const struct dev_constraint *dinfo;
	atomic_t vbat0_flag;
	bool is_imix_r_unused;
	int imix_r;
	int irq;
	int pre_uisoc;
	u8 unmask_buf[NUM_IRQ_REG];

#ifdef CONFIG_LOCKDEP
	struct lock_class_key info_exist_key;
#endif
};

#define VBAT0_POLL_TIME_SEC	5
#define ALARM_COUNT_MAX		12
static const int vbat_event[] = { RG_INT_STATUS_BAT_H, RG_INT_STATUS_BAT_L,
				  RG_INT_STATUS_BAT2_H, RG_INT_STATUS_BAT2_L,
				  RG_INT_STATUS_NAG_C_DLTV };

static const struct {
	u16 addr;
	u8 mask;
} vbat_event_regs[] = {
	{ RG_AUXADC_LBAT0, AUXADC_LBAT_EN_MASK },
	{ RG_AUXADC_LBAT0, AUXADC_LBAT_EN_MASK },
	{ RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK },
	{ RG_AUXADC_LBAT2_0, AUXADC_LBAT2_EN_MASK },
	{ RG_AUXADC_NAG_0, AUXADC_NAG_IRQ_EN_MASK | AUXADC_NAG_EN_MASK },
};

#define AUXADC_CHAN(_idx, _resolution, _type, _info) {		\
	.type = _type,						\
	.channel = MT6375_AUXADC_##_idx,			\
	.scan_index = MT6375_AUXADC_##_idx,			\
	.datasheet_name = #_idx,				\
	.scan_type =  {						\
		.sign = 'u',					\
		.realbits = _resolution,			\
		.storagebits = 16,				\
		.endianness = IIO_CPU,				\
	},							\
	.indexed = 1,						\
	.info_mask_separate = _info				\
}

#define AUXADC_CHAN_PROCESSED(_idx, _resolution, _type)		\
	AUXADC_CHAN(_idx, _resolution, _type, BIT(IIO_CHAN_INFO_PROCESSED))

#define AUXADC_CHAN_RAW(_idx, _resolution, _type)		\
	AUXADC_CHAN(_idx, _resolution, _type, BIT(IIO_CHAN_INFO_RAW))

#define AUXADC_CHAN_RAW_SCALE(_idx, _resolution, _type)		\
	AUXADC_CHAN(_idx, _resolution, _type,			\
		    BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE))

static const struct iio_chan_spec auxadc_channels[] = {
	AUXADC_CHAN_RAW_SCALE(BATSNS, 15, IIO_VOLTAGE),
	AUXADC_CHAN_RAW_SCALE(BATON, 12, IIO_VOLTAGE),
	AUXADC_CHAN_PROCESSED(IMP, 15, IIO_VOLTAGE),
	AUXADC_CHAN_RAW(IMIX_R, 16, IIO_RESISTANCE),
	AUXADC_CHAN_RAW_SCALE(VREF, 12, IIO_VOLTAGE),
	AUXADC_CHAN_RAW_SCALE(BATSNS_DBG, 15, IIO_VOLTAGE)
};

static unsigned int rg_xlate(struct mt6375_priv *priv, unsigned int addr)
{
	const struct dev_constraint *dinfo = priv->dinfo;

	/* If there is no dinfo, it means this chip is MT6375 */
	if (!dinfo)
		return addr;

	/* For BM base */
	if (RG_BASE_CMP(addr, MT6375_RG_BM_BASE))
		return XLATE_NEW_ADDR(addr, dinfo->rg_bm_base);

	/* For HK1 base */
	if (RG_BASE_CMP(addr, MT6375_RG_HK1_BASE))
		return XLATE_NEW_ADDR(addr, dinfo->rg_hk1_base);

	/* For HK2 base */
	if (RG_BASE_CMP(addr, MT6375_RG_HK2_BASE))
		return XLATE_NEW_ADDR(addr, dinfo->rg_hk2_base);

	dev_notice(priv->dev, "%s: error reg address! error addr:0x%x\n",
		   __func__, addr);
	return addr;
}

static int auxadc_get_chg_vbat(struct mt6375_priv *priv, int *chg_vbat)
{
	static struct iio_channel *chg_vbat_chan;
	int ret = 0, vbat;

	if (IS_ERR_OR_NULL(chg_vbat_chan))
		chg_vbat_chan = devm_iio_channel_get(priv->dev, "chg_vbat");

	if (IS_ERR(chg_vbat_chan))
		return PTR_ERR(chg_vbat_chan);

	ret = iio_read_channel_processed(chg_vbat_chan, &vbat);
	if (ret < 0)
		return ret;

	*chg_vbat = vbat / 1000;
	return ret;
}

static int auxadc_read_channel(struct mt6375_priv *priv, int chan, int dbits, int *val)
{
	unsigned int enable, out_reg, rdy_val;
	u16 raw_val;
	int ret, chg_vbat = 0;

	if (chan == MT6375_AUXADC_VREF) {
		enable = VREF_ENMASK;
		out_reg = AUXADC_OUT_CH11;
	} else if (chan == MT6375_AUXADC_BATON) {
		enable = BATON_ENMASK;
		out_reg = AUXADC_OUT_CH3;
	} else if (chan == MT6375_AUXADC_BATSNS) {
		if (atomic_read(&priv->vbat0_flag)) {
			ret = auxadc_get_chg_vbat(priv, &chg_vbat);
			dev_info(priv->dev, "%s: use chg_vbat:%d(%d)\n", __func__, chg_vbat, ret);
			if (ret >= 0)
				*val = chg_vbat;

			return ret ? ret : IIO_VAL_INT;
		}

		enable = BATSNS_ENMASK;
		out_reg = AUXADC_OUT_CH0;
	} else {
		enable = BATSNS_ENMASK;
		out_reg = AUXADC_OUT_CH0;
	}

	ret = regmap_write(priv->regmap, rg_xlate(priv, AUXADC_RQST0), enable);
	if (ret)
		return ret;

	usleep_range(1000, 1200);

	ret = regmap_read_poll_timeout(priv->regmap, rg_xlate(priv, out_reg + 1),
				       rdy_val, rdy_val & ADC_OUT_RDY, 500, 11 * 1000);
	if (ret == -ETIMEDOUT)
		dev_err(priv->dev, "(%d) channel timeout\n", chan);

	ret = regmap_raw_read(priv->regmap, rg_xlate(priv, out_reg),
			      &raw_val, sizeof(raw_val));
	if (ret)
		return ret;

	*val = raw_val & (BIT(dbits) - 1);

	return IIO_VAL_INT;
}

static int check_gauge_psy(struct mt6375_priv *priv)
{
	struct power_supply *psy = priv->gauge_psy;

	if (psy)
		return 0;

	psy = power_supply_get_by_name("mtk-gauge");
	if (!psy) {
		psy = power_supply_get_by_phandle(priv->dev->of_node, "gauge");
		if (IS_ERR_OR_NULL(psy))
			return -ENODEV;
	}

	priv->gauge_psy = psy;
	return 0;
}

static int gauge_get_imp_ibat(struct mt6375_priv *priv)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	ret = check_gauge_psy(priv);
	if (ret) {
		dev_err(priv->dev, "%s, No gauge device!\n", __func__);
		return 0;
	}

	psy = priv->gauge_psy;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (ret)
		return ret;

	return prop.intval;
}

static int auxadc_read_imp(struct mt6375_priv *priv, int *vbat, int *ibat)
{
	unsigned int wait_time_in_ms, regval;
	const unsigned int prd_sel[] = { 6, 8, 10, 12 };
	const unsigned int cnt_sel[] = { 1, 2, 4, 8 };
	u16 raw_val;
	int ret;
	int dbits = auxadc_channels[MT6375_AUXADC_IMP].scan_type.realbits;

	if (atomic_read(&priv->vbat0_flag)) {
		dev_info(priv->dev, "%s: vbat cell abnormal, return -EIO\n", __func__);
		return -EIO;
	}

	ret = regmap_write(priv->regmap, rg_xlate(priv, AUXADC_IMP0), 0);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, rg_xlate(priv, HK_TOP_INT_CON1_CLR),
			   INT_RAW_AUXADC_IMP);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, rg_xlate(priv, HK_TOP_INT_CON1_SET),
			   INT_RAW_AUXADC_IMP);
	if (ret)
		return ret;

	ret = regmap_read(priv->regmap, rg_xlate(priv, AUXADC_IMP1), &regval);
	if (ret)
		return ret;

	wait_time_in_ms = prd_sel[regval & AUXADC_IMP_PRDSEL_MASK];
	wait_time_in_ms *= cnt_sel[(regval & AUXADC_IMP_CNTSEL_MASK) >> AUXADC_IMP_CNTSEL_SHFT];

	ret = regmap_write(priv->regmap, rg_xlate(priv, AUXADC_IMP0), AUXADC_IMP_ENMASK);
	if (ret)
		return ret;

	msleep(wait_time_in_ms);

	ret = regmap_read_poll_timeout(priv->regmap, rg_xlate(priv, HK_TOP_INT_RAW_STATUS1),
				       regval, (regval & INT_RAW_AUXADC_IMP), 100, 1000);
	if (ret == -ETIMEDOUT)
		dev_err(priv->dev, "IMP channel timeout\n");

	ret = regmap_raw_read(priv->regmap, rg_xlate(priv, AUXADC_OUT_IMP_AVG),
			      &raw_val, sizeof(raw_val));
	if (ret)
		return ret;

	raw_val &= (BIT(dbits) - 1);
	*vbat = div_s64((s64)raw_val * 7360, BIT(dbits));

	ret = regmap_write(priv->regmap, rg_xlate(priv, AUXADC_IMP0), 0);
	if (ret)
		return ret;

	*ibat = gauge_get_imp_ibat(priv);

	return IIO_VAL_INT;
}

static int auxadc_read_scale(struct mt6375_priv *priv, int chan, int dbits, int *val1, int *val2)
{
	switch (chan) {
	case MT6375_AUXADC_BATSNS:
		if (atomic_read(&priv->vbat0_flag)) {
			*val1 = 1;
			*val2 = 1;
		} else {
			*val1 = 7360;
			*val2 = BIT(dbits);
		}
		return IIO_VAL_FRACTIONAL;
	case MT6375_AUXADC_BATSNS_DBG:
		*val1 = 7360;
		*val2 = BIT(dbits);
		return IIO_VAL_FRACTIONAL;
	case MT6375_AUXADC_BATON:
	case MT6375_AUXADC_VREF:
		*val1 = 2760;
		*val2 = BIT(dbits);
		return IIO_VAL_FRACTIONAL;
	default:
		return -EINVAL;
	}
}

static int auxadc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val1,
			   int *val2, long mask)
{
	struct mt6375_priv *priv = iio_priv(indio_dev);
	int dbits = chan->scan_type.realbits;
	int ch_idx = chan->channel;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		switch (ch_idx) {
		case MT6375_AUXADC_IMP:
			mutex_lock(&priv->adc_lock);
			pm_stay_awake(priv->dev);
			ret = auxadc_read_imp(priv, val1, val2);
			pm_relax(priv->dev);
			mutex_unlock(&priv->adc_lock);
			return ret;
		}

		return -EINVAL;
	case IIO_CHAN_INFO_RAW:
		switch (ch_idx) {
		case MT6375_AUXADC_BATSNS:
		case MT6375_AUXADC_BATON:
		case MT6375_AUXADC_VREF:
		case MT6375_AUXADC_BATSNS_DBG:
			mutex_lock(&priv->adc_lock);
			pm_stay_awake(priv->dev);
			ret = auxadc_read_channel(priv, ch_idx, dbits, val1);
			pm_relax(priv->dev);
			mutex_unlock(&priv->adc_lock);
			return ret;
		case MT6375_AUXADC_IMIX_R:
			*val1 = priv->is_imix_r_unused ? 90 : priv->imix_r;
			return IIO_VAL_INT;
		}

		return -EINVAL;
	case IIO_CHAN_INFO_SCALE:
		return auxadc_read_scale(priv, ch_idx, dbits, val1, val2);
	default:
		return -EINVAL;
	}
}

static const char * const mt6375_auxadc_chan_labels[MT6375_AUXADC_MAX_CHANNEL] = {
	/* The label index of MT6375 is tha same as that of MT6379. */
	[MT6375_AUXADC_BATSNS]		= "batsns",
	[MT6375_AUXADC_BATON]		= "baton",
	[MT6375_AUXADC_IMP]		= "imp",
	[MT6375_AUXADC_IMIX_R]		= "imix_r",
	[MT6375_AUXADC_VREF]		= "vref",
	[MT6375_AUXADC_BATSNS_DBG]	= "batsns_dbg",
};

static int auxadc_chan_labels(struct iio_dev *iiodev,
			      struct iio_chan_spec const *chan, char *label)
{
	struct mt6375_priv *priv = iio_priv(iiodev);

	if (chan->channel == MT6375_AUXADC_IMIX_R && priv->is_imix_r_unused)
		return sysfs_emit(label, "%s\n", "imix_r (phase out!!)");

	return sysfs_emit(label, "%s\n", chan->channel >= 0 ?
			  mt6375_auxadc_chan_labels[chan->channel] : "INVALID");
}

static const struct iio_info auxadc_iio_info = {
	.read_raw = auxadc_read_raw,
	.read_label = auxadc_chan_labels,
};

static void auxadc_irq_lock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	mutex_lock(&priv->irq_lock);
}

static void auxadc_irq_sync_unlock(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);
	unsigned int idx = data->hwirq / 8, bits = BIT(data->hwirq % 8), reg;
	int ret;

	if (idx >= NUM_IRQ_REG)
		goto irq_sync_unlock_out;

	if (priv->unmask_buf[idx] & bits)
		reg = HK_TOP_INT_CON0_SET + idx * 3;
	else
		reg = HK_TOP_INT_CON0_CLR + idx * 3;

	ret = regmap_write(priv->regmap, rg_xlate(priv, reg), bits);
	if (ret)
		dev_err(priv->dev, "Failed to set/clr irq con %d\n", (int)data->hwirq);

irq_sync_unlock_out:
	mutex_unlock(&priv->irq_lock);
}

static void auxadc_irq_disable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] &= ~BIT(data->hwirq % 8);
}

static void auxadc_irq_enable(struct irq_data *data)
{
	struct mt6375_priv *priv = irq_data_get_irq_chip_data(data);

	priv->unmask_buf[data->hwirq / 8] |= BIT(data->hwirq % 8);
}

static int auxadc_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct mt6375_priv *priv = h->host_data;

	irq_set_chip_data(virq, priv);
	irq_set_chip(virq, &priv->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, priv->irq);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops auxadc_domain_ops = {
	.map = auxadc_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static int auxadc_vbat_is_valid(struct mt6375_priv *priv, bool *valid)
{
	static struct iio_channel *auxadc_vbat_chan;
	int ret = 0, chg_vbat = 0, auxadc_vbat = 0;

	if (IS_ERR_OR_NULL(auxadc_vbat_chan))
		auxadc_vbat_chan = devm_iio_channel_get(priv->dev, "auxadc_vbat");

	if (IS_ERR(auxadc_vbat_chan))
		return PTR_ERR(auxadc_vbat_chan);

	ret = auxadc_get_chg_vbat(priv, &chg_vbat);
	dev_info(priv->dev, "%s: chg_vbat = %d(%d)\n", __func__, chg_vbat, ret);

	ret |= iio_read_channel_processed(auxadc_vbat_chan, &auxadc_vbat);
	dev_info(priv->dev, "%s: auxadc_vbat = %d(%d)\n", __func__, auxadc_vbat, ret);

	if (!ret && abs(chg_vbat - auxadc_vbat) > 1000) {
		dev_info(priv->dev, "%s: unexpected vbat cell!!\n", __func__);
		*valid = false;
	} else
		*valid = true;

	return ret;
}

static int auxadc_handle_vbat0(struct mt6375_priv *priv, bool is_vbat0)
{
	struct power_supply *chg_psy;
	union power_supply_propval val;
	int i, ret;

	/* set/clr vbat0 bits */
	ret = regmap_update_bits(priv->regmap, rg_xlate(priv, SYSTEM_INFO_CON2_H),
				 VBAT0_FLAG, is_vbat0 ? 0xFF : 0);
	if (ret < 0) {
		dev_notice(priv->dev, "%s: failed to clear vbat0 flag\n", __func__);
		return ret;
	}

	/* notify gauge & charger */
	chg_psy = devm_power_supply_get_by_phandle(priv->dev, "charger");
	if (IS_ERR_OR_NULL(chg_psy))
		return PTR_ERR(chg_psy);

	val.intval = is_vbat0 ? true : false;
	ret = power_supply_set_property(chg_psy, POWER_SUPPLY_PROP_ENERGY_EMPTY, &val);
	power_supply_changed(chg_psy);

	/* mask/unmask irq & disable function */
	for (i = 0; i < ARRAY_SIZE(vbat_event); i++) {
		if (is_vbat0) {
			ret = regmap_update_bits(priv->regmap,
						 rg_xlate(priv, vbat_event_regs[i].addr),
						 vbat_event_regs[i].mask, 0);
			disable_irq_nosync(irq_find_mapping(priv->domain, vbat_event[i]));
		} else {
			ret = regmap_update_bits(priv->regmap,
						 rg_xlate(priv, vbat_event_regs[i].addr),
						 vbat_event_regs[i].mask, 0xFF);
			enable_irq(irq_find_mapping(priv->domain, vbat_event[i]));
		}
	}

	atomic_set(&priv->vbat0_flag, is_vbat0);
	return ret;
}

static void auxadc_vbat0_poll_work(struct work_struct *work)
{
	struct mt6375_priv *priv = container_of(work, struct mt6375_priv, vbat0_work);
	bool valid = false;
	int ret;
#ifdef CONFIG_MTK_GAUGE_VBAT0_DEBUG
	ktime_t add;
#endif

	__pm_stay_awake(priv->vbat0_ws);
	ret = auxadc_vbat_is_valid(priv, &valid);
	if (ret < 0 || !valid) {
		dev_info(priv->dev, "%s: restart timer\n", __func__);
#ifdef CONFIG_MTK_GAUGE_VBAT0_DEBUG
		add = ktime_set(VBAT0_POLL_TIME_SEC, 0);
		alarm_forward_now(&priv->vbat0_alarm, add);
		alarm_restart(&priv->vbat0_alarm);
#endif
		__pm_relax(priv->vbat0_ws);
		return;
	}

	dev_info(priv->dev, "%s: vbat recover\n", __func__);
	if (auxadc_handle_vbat0(priv, false))
		dev_notice(priv->dev, "%s: failed to handle vbat0\n", __func__);

	__pm_relax(priv->vbat0_ws);
}

static enum alarmtimer_restart vbat0_alarm_poll_func(struct alarm *alarm, ktime_t now)
{
	struct mt6375_priv *priv = container_of(alarm, struct mt6375_priv, vbat0_alarm);

	schedule_work(&priv->vbat0_work);
	return ALARMTIMER_NORESTART;
}

static int auxadc_check_vbat_event(struct mt6375_priv *priv, u8 *status_buf)
{
	int i, ret = 0, idx_i, idx_j;
	bool valid = false;
	ktime_t now, add;

	if (atomic_read(&priv->vbat0_flag))
		return ret;

	for (i = 0; i < ARRAY_SIZE(vbat_event); i++) {
		idx_i = vbat_event[i] / 8;
		idx_j = vbat_event[i] % 8;
		if (status_buf[idx_i] & BIT(idx_j))
			break;
	}

	if (i == ARRAY_SIZE(vbat_event)) {
		dev_info(priv->dev, "%s: without related event\n", __func__);
		return ret;
	}

	ret = auxadc_vbat_is_valid(priv, &valid);
	if (ret < 0 || valid)
		return ret;

	ret = auxadc_handle_vbat0(priv, true);
	if (ret < 0) {
		dev_notice(priv->dev, "%s: failed to handle vbat0\n", __func__);
		return ret;
	}

	/* start alarm */
	now = ktime_get_boottime();
	add = ktime_set(VBAT0_POLL_TIME_SEC, 0);
	alarm_start(&priv->vbat0_alarm, ktime_add(now, add));

	return ret;
}

static int mt6379_pre_irq_handler(void *data)
{
	struct mt6375_priv *priv = data;
	unsigned int reg_val;
	int ret;

	ret = regmap_read(priv->regmap, MT6379_RG_GM30_EVT, &reg_val);
	if (ret) {
		dev_notice(priv->dev, "%s: Failed to read GM30_EVT\n", __func__);
		return ret;
	}

	/*
	 * If there are some auxadc events, it will return non-zero.
	 * Otherwise it will return 0.
	 */
	return (reg_val & priv->dinfo->gm30_evt_mask);
}

static int mt6379_post_irq_handler(void *data)
{
	struct mt6375_priv *priv = data;

	/* MT6379 do retrigger */
	return regmap_write(priv->regmap, MT6379_RG_SPMI_TXDRV2, MT6379_MASK_RCS_INT_DONE);
}

static irqreturn_t auxadc_irq_thread(int irq, void *data)
{
	static const u8 mask[NUM_IRQ_REG] = { 0x3F, 0x02 };
	static const u8 no_status[NUM_IRQ_REG];
	u8 status_buf[NUM_IRQ_REG], status;
	struct mt6375_priv *priv = data;
	struct device *dev = priv->dev;
	bool handled = false;
	int i, j, ret;

	if (priv->dinfo && priv->dinfo->pre_irq_handler) {
		ret = priv->dinfo->pre_irq_handler(priv);
		if (ret == 0)
			return IRQ_HANDLED;
	}

	ret = regmap_raw_read(priv->regmap, rg_xlate(priv, HK_TOP_INT_STATUS0),
			      status_buf, sizeof(status_buf));
	if (ret) {
		dev_err(dev, "Error reading INT status\n");
		return IRQ_HANDLED;
	}

	if (!memcmp(status_buf, no_status, NUM_IRQ_REG))
		return IRQ_HANDLED;

	ret = auxadc_check_vbat_event(priv, status_buf);
	if (ret < 0)
		dev_info(dev, "check vbat event failed\n");

	/* mask all irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   rg_xlate(priv, HK_TOP_INT_MASK_CON0_SET + i * 3), mask[i]);
		if (ret)
			dev_err(dev, "Failed to mask irq[%d]\n", i);
	}

	for (i = 0; i < NUM_IRQ_REG; i++) {
		status = status_buf[i] & priv->unmask_buf[i];
		if (!status)
			continue;

		for (j = 0; j < 8; j++) {
			if (!(status & BIT(j)))
				continue;

			dev_err(priv->dev, "%s, handle auxadc irq (reg: 0x%X, bit: %d)\n",
				__func__, rg_xlate(priv, HK_TOP_INT_STATUS0 + i), j);
			handle_nested_irq(irq_find_mapping(priv->domain, i * 8 + j));
			handled = true;
		}
	}

	/* after process, unmask all irqs */
	for (i = 0; i < NUM_IRQ_REG; i++) {
		ret = regmap_write(priv->regmap,
				   rg_xlate(priv, HK_TOP_INT_MASK_CON0_CLR + i * 3), mask[i]);
		if (ret)
			dev_err(dev, "Failed to unmask irq[%d]\n", i);
	}

	ret = regmap_raw_write(priv->regmap, rg_xlate(priv, HK_TOP_INT_STATUS0),
			       status_buf, sizeof(status_buf));
	if (ret)
		dev_err(dev, "Error clear INT status\n");

	if (handled && priv->dinfo && priv->dinfo->post_irq_handler) {
		ret = priv->dinfo->post_irq_handler(priv);
		if (ret)
			dev_notice(dev, "Failed to do post irq handler\n");
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int auxadc_add_irq_chip(struct mt6375_priv *priv)
{
	u32 val, state_reg, clear_reg, mask_reg;
	int i, ret;


	for (i = 0; i < NUM_IRQ_REG; i++) {
		state_reg = rg_xlate(priv, HK_TOP_INT_CON0 + i * 3);
		ret = regmap_read(priv->regmap, state_reg, &val);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to read irq con%d (0x%03X)\n",
				 __func__, i, state_reg);
			return ret;
		}

		dev_info(priv->dev, "%s, [before clr] irq con%d (0x%03X) val: 0x%02X\n",
			 __func__, i, state_reg, val);

		clear_reg = rg_xlate(priv, HK_TOP_INT_CON0_CLR + i * 3);
		ret = regmap_write(priv->regmap, clear_reg, 0xFF);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to clear irq con%d (0x%03X)\n",
				 __func__, i, clear_reg);
			return ret;
		}

		mask_reg = rg_xlate(priv, HK_TOP_INT_MASK_CON0 + i * 3);
		ret = regmap_read(priv->regmap, mask_reg, &val);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to read irq con%d mask (0x%03X)\n",
				 __func__, i, mask_reg);
			return ret;
		}

		dev_info(priv->dev, "%s, irq con%d mask (0x%03X) val: 0x%x\n",
			 __func__, i, mask_reg, val);

		ret = regmap_write(priv->regmap, mask_reg, 0);
		if (ret) {
			dev_info(priv->dev, "%s, Failed to init irq con%d mask (0x%03X)\n",
				 __func__, i, mask_reg);
			return ret;
		}
	}

	/* Default mask AUXADC_IMP */
	ret = regmap_update_bits(priv->regmap, rg_xlate(priv, HK_TOP_INT_MASK_CON1),
				 INT_RAW_AUXADC_IMP, INT_RAW_AUXADC_IMP);
	if (ret) {
		dev_err(priv->dev, "Failed to defaut unmask AUXADC_IMP\n");
		return ret;
	}

	priv->irq_chip.name = dev_name(priv->dev);
	priv->irq_chip.irq_bus_lock = auxadc_irq_lock;
	priv->irq_chip.irq_bus_sync_unlock = auxadc_irq_sync_unlock;
	priv->irq_chip.irq_disable = auxadc_irq_disable;
	priv->irq_chip.irq_enable = auxadc_irq_enable;

	priv->domain = irq_domain_add_linear(priv->dev->of_node, NUM_IRQ_REG * 8,
					     &auxadc_domain_ops, priv);
	if (!priv->domain) {
		dev_err(priv->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(priv->irq, NULL, auxadc_irq_thread, IRQF_SHARED | IRQF_ONESHOT,
				   dev_name(priv->dev), priv);
	if (ret) {
		dev_err(priv->dev, "Failed to request IRQ %d for %s: %d\n", priv->irq,
			dev_name(priv->dev), ret);
		goto err_irq;
	}

	enable_irq_wake(priv->irq);
	return 0;

err_irq:
	irq_domain_remove(priv->domain);
	return ret;
}

static void auxadc_del_irq_chip(void *data)
{
	struct mt6375_priv *priv = data;
	unsigned int virq;
	int hwirq;

	free_irq(priv->irq, priv);

	for (hwirq = 0; hwirq < NUM_IRQ_REG * 8; hwirq++) {
		virq = irq_find_mapping(priv->domain, hwirq);
		if (virq)
			irq_dispose_mapping(virq);
	}

	irq_domain_remove(priv->domain);
}

static int auxadc_reset(struct mt6375_priv *priv)
{
	u8 reset_key[2] = { 0x63, 0x63 };
	int ret;

	ret = regmap_raw_write(priv->regmap, rg_xlate(priv, HK_TOP_WKEY),
			       reset_key, sizeof(reset_key));
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, rg_xlate(priv, HK_TOP_RST_CON0), RG_RESET_MASK);
	if (ret)
		return ret;

	ret = regmap_write(priv->regmap, rg_xlate(priv, HK_TOP_RST_CON0), 0);
	if (ret)
		return ret;

	reset_key[0] = reset_key[1] = 0;
	return regmap_raw_write(priv->regmap, rg_xlate(priv, HK_TOP_WKEY),
				reset_key, sizeof(reset_key));
}

/* This function can only be used on the old platforms which use "imix-r" function */
static int auxadc_get_uisoc(struct mt6375_priv *priv)
{
	struct power_supply *psy = priv->battery_psy;
	union power_supply_propval prop;
	int ret;

	if (!psy) {
		psy = power_supply_get_by_name("battery");
		if (!psy)
			return -ENODEV;

		priv->battery_psy = psy;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret || prop.intval < 0)
		return -EINVAL;

	return prop.intval;
}

/* This function can only be used on the old platforms which use "imix-r" function */
static int auxadc_get_rac(struct mt6375_priv *priv)
{
	int vbat_1 = 0, vbat_2 = 0;
	int ibat_1 = 0, ibat_2 = 0;
	int rac = 0, ret = 0;
	int retry_count = 0;

	/* to make sure dummy load has been disabled */
	if (regulator_is_enabled(priv->isink_load))
		regulator_disable(priv->isink_load);

	do {
		mutex_lock(&priv->adc_lock);
		ret = auxadc_read_imp(priv, &vbat_1, &ibat_1);
		mutex_unlock(&priv->adc_lock);
		if (ret < 0)
			return ret;

		/* enable_dummy_load */
		ret = regulator_enable(priv->isink_load);
		if (ret)
			return ret;

		mdelay(50);

		mutex_lock(&priv->adc_lock);
		ret = auxadc_read_imp(priv, &vbat_2, &ibat_2);
		mutex_unlock(&priv->adc_lock);
		if (ret < 0)
			return ret;

		/* disable_dummy_load */
		ret = regulator_disable(priv->isink_load);
		if (ret)
			return ret;

		mdelay(50);

		/* translate to 0.1mV */
		vbat_1 *= 10;
		vbat_2 *= 10;

		/* Cal.Rac: 70mA <= c_diff <= 120mA, 4mV <= v_diff <= 200mV */
		if ((ibat_2 - ibat_1) >= 700 && (ibat_2 - ibat_1) <= 1200 &&
		    (vbat_1 - vbat_2) >= 40 && (vbat_1 - vbat_2) <= 2000) {
			/*m-ohm */
			rac = ((vbat_1 - vbat_2) * 1000) / (ibat_2 - ibat_1);
			if (rac < 0)
				ret = (rac - (rac * 2)) * 1;
			else
				ret = rac * 1;
			if (ret < 50)
				ret = -1;
		} else
			ret = -1;

		dev_info(priv->dev, "v1=%d,v2=%d,c1=%d,c2=%d,v_diff=%d,c_diff=%d,rac=%d,ret=%d,retry=%d\n",
			 vbat_1, vbat_2, ibat_1, ibat_2, (vbat_1 - vbat_2), (ibat_2 - ibat_1),
			 rac, ret, retry_count);

		if (++retry_count >= 3)
			break;
	} while (ret == -1);

	return ret;
}

#define IMIX_R_MIN_MOHM	100
#define IMIX_R_CALI_CNT	2

/* This function can only be used on the old platforms which use "imix-r" function */
static int auxadc_cali_imix_r(struct mt6375_priv *priv)
{
	int i, ret, imix_r_avg = 0, rac_val[IMIX_R_CALI_CNT];
	int cur_uisoc = auxadc_get_uisoc(priv);

	ret = check_gauge_psy(priv);
	if (ret) {
		dev_info(priv->dev, "%s, gauge is disabled, skip calibrate imix_r\n", __func__);
		return -ENODEV;
	}

	if (cur_uisoc < 0 || cur_uisoc == priv->pre_uisoc) {
		dev_info(priv->dev, "%s, pre_SOC=%d SOC= %d, skip\n", __func__,
			 priv->pre_uisoc, cur_uisoc);
		return 0;
	}

	priv->pre_uisoc = cur_uisoc;

	for (i = 0; i < IMIX_R_CALI_CNT; i++) {
		rac_val[i] = auxadc_get_rac(priv);
		if (rac_val[i] < 0)
			return -EIO;

		imix_r_avg += rac_val[i];
	}

	imix_r_avg /= IMIX_R_CALI_CNT;
	if (imix_r_avg > IMIX_R_MIN_MOHM)
		priv->imix_r = imix_r_avg;

	dev_info(priv->dev, "[%s] %d, %d, ravg:%d\n", __func__, rac_val[0], rac_val[1], imix_r_avg);
	return 0;
}

static int mt6375_auxadc_parse_dt(struct mt6375_priv *priv)
{
	struct device_node *np, *imix_r_np;
	u32 val = 0;
	int ret = 0;

	if (priv->dinfo)
		np = of_find_compatible_node(NULL, NULL, priv->dinfo->pmic_compatible);
	else
		np = of_find_compatible_node(NULL, NULL, "mediatek,pmic-auxadc");

	if (!np)
		return -ENODEV;

	imix_r_np = of_get_child_by_name(np, "imix_r");
	if (!imix_r_np)
		imix_r_np = of_get_child_by_name(np, "imix-r");

	if (!imix_r_np) {
		dev_info(priv->dev, "%s, No imix_r/imix-r dt node, using \"no imix-r\" version\n",
			 __func__);
		priv->is_imix_r_unused = true;
		return 0;
	}

	ret = of_property_read_u32(imix_r_np, "val", &val);
	if (ret) {
		dev_info(priv->dev, "%s, No imix_r/imix-r value(%d)\n", __func__, ret);
		return ret;
	}

	priv->imix_r = val;
	dev_info(priv->dev, "%s, imix_r = %d\n", __func__, priv->imix_r);

	priv->isink_load = devm_regulator_get_exclusive(priv->dev, "isink_load");
	if (IS_ERR(priv->isink_load))
		priv->isink_load = devm_regulator_get_exclusive(priv->dev, "isink-load");

	if (IS_ERR(priv->isink_load)) {
		dev_info(priv->dev, "%s, Failed to get isink_load/isink-load regulator [%d]\n",
			 __func__, (int)PTR_ERR(priv->isink_load));
		return PTR_ERR(priv->isink_load);
	}

	return 0;
}

#ifdef CONFIG_LOCKDEP
static void mt6375_unregister_lockdep_key(void *data)
{
	struct lock_class_key *key = data;

	lockdep_unregister_key(key);
}
#endif

static const struct dev_constraint mt6379_bat1_dinfo = {
	.bat_idx = 0,
	.pmic_compatible = "mediatek,pmic-auxadc-1",
	.rg_bm_base = MT6379_RG_BAT1_BM_BASE,
	.rg_hk1_base = MT6379_RG_BAT1_HK1_BASE,
	.rg_hk2_base = MT6379_RG_BAT1_HK2_BASE,
	.gm30_evt_mask = MT6379_MASK_HK1_EVT,
	.pre_irq_handler = mt6379_pre_irq_handler,
	.post_irq_handler = mt6379_post_irq_handler,
};

static const struct dev_constraint mt6379_bat2_dinfo = {
	.bat_idx = 1,
	.pmic_compatible = "mediatek,pmic-auxadc-2",
	.rg_bm_base = MT6379_RG_BAT2_BM_BASE,
	.rg_hk1_base = MT6379_RG_BAT2_HK1_BASE,
	.rg_hk2_base = MT6379_RG_BAT2_HK2_BASE,
	.gm30_evt_mask = MT6379_MASK_HK2_EVT,
	.pre_irq_handler = mt6379_pre_irq_handler,
	.post_irq_handler = mt6379_post_irq_handler,
};

static int mt6379_check_bat_cell_count(struct mt6375_priv *priv)
{
	unsigned int val, cell_count;
	int ret;

	/* MT6375 is not needed to check */
	if (!priv->dinfo)
		return 0;

	ret = regmap_read(priv->regmap, MT6379_RG_CORE_CTRL0, &val);
	if (ret)
		return dev_err_probe(priv->dev, ret, "Failed to read CORE_CTRL0\n");

	cell_count = FIELD_GET(MT6379_MASK_CELL_COUNT, val);
	if (priv->dinfo->bat_idx > cell_count)
		return dev_err_probe(priv->dev, -ENODEV, "%s, HW not support!\n", __func__);

	return 0;
}

static int mt6375_auxadc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct mt6375_priv *priv;
	int ret;
#ifdef CONFIG_LOCKDEP
	struct iio_dev_opaque *iio_dev_opaque;
#endif

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = &pdev->dev;
	priv->dinfo = device_get_match_data(&pdev->dev);
	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		return -ENODEV;
	}

	ret = mt6379_check_bat_cell_count(priv);
	if (ret)
		return ret;

	mutex_init(&priv->adc_lock);
	mutex_init(&priv->irq_lock);
	priv->pre_uisoc = 101;
	atomic_set(&priv->vbat0_flag, 0);
	device_init_wakeup(&pdev->dev, true);
	platform_set_drvdata(pdev, priv);
	priv->vbat0_ws = wakeup_source_register(&pdev->dev, "vbat0_ws");

#ifdef CONFIG_LOCKDEP
	lockdep_register_key(&priv->info_exist_key);
	iio_dev_opaque = to_iio_dev_opaque(indio_dev);
	lockdep_set_class(&iio_dev_opaque->info_exist_lock, &priv->info_exist_key);
	ret = devm_add_action_or_reset(&pdev->dev, mt6375_unregister_lockdep_key,
				       &priv->info_exist_key);
	if (ret)
		return ret;
#endif

	ret = mt6375_auxadc_parse_dt(priv);
	if (ret) {
		dev_notice(&pdev->dev, "Failed to parse dt\n");
		return ret;
	}

	ret = auxadc_reset(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to reset\n");
		return ret;
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0) {
		dev_err(&pdev->dev, "Failed to get gm30 irq\n");
		return priv->irq;
	}

	ret = auxadc_add_irq_chip(priv);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add irq chip\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, auxadc_del_irq_chip, priv);
	if (ret)
		return ret;

	INIT_WORK(&priv->vbat0_work, auxadc_vbat0_poll_work);
	alarm_init(&priv->vbat0_alarm, ALARM_BOOTTIME, vbat0_alarm_poll_func);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &auxadc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = auxadc_channels;
	indio_dev->num_channels = ARRAY_SIZE(auxadc_channels);

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static int mt6375_auxadc_remove(struct platform_device *pdev)
{
	struct mt6375_priv *priv = platform_get_drvdata(pdev);

	if (priv) {
		if (priv->gauge_psy)
			power_supply_put(priv->gauge_psy);

		if (!priv->is_imix_r_unused && priv->battery_psy)
			power_supply_put(priv->battery_psy);
	}

	return 0;
}

static int mt6375_auxadc_suspend_late(struct device *dev)
{
	struct mt6375_priv *priv = dev_get_drvdata(dev);
	int ret;

	/* If the function of imix-r is unused, just skip this stage  */
	if (priv->is_imix_r_unused)
		return 0;

	ret = auxadc_cali_imix_r(priv);
	if (ret)
		dev_err(dev, "calibrate imix_r ret=[%d]\n", ret);

	return 0;
}

static const struct dev_pm_ops mt6375_auxadc_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mt6375_auxadc_suspend_late, NULL)
};

static const struct of_device_id mt6375_auxadc_of_match[] = {
	{ .compatible = "mediatek,mt6375-auxadc", },
	{ .compatible = "mediatek,mt6379-auxadc-1", .data = (void *) &mt6379_bat1_dinfo },
	{ .compatible = "mediatek,mt6379-auxadc-2", .data = (void *) &mt6379_bat2_dinfo },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6375_auxadc_of_match);

static struct platform_driver mt6375_auxadc_driver = {
	.probe = mt6375_auxadc_probe,
	.remove = mt6375_auxadc_remove,
	.driver = {
		.name = "mt6375-auxadc",
		.of_match_table = mt6375_auxadc_of_match,
		.pm = &mt6375_auxadc_dev_pm_ops,
	},
};
module_platform_driver(mt6375_auxadc_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6375_MT6379 AUXADC Driver");
MODULE_LICENSE("GPL");
