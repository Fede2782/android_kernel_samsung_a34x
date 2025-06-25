// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*
 * Copyright (C) 2019 Maxim Integrated Products, Inc.
 * Author : Maxim Integrated <opensource@maximintegrated.com>
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regmap.h>

#include "charger_class.h"
#include "mtk_charger.h"

#define mtk_2p_charger_DRV_VERSION			  "1.0.0_G"

enum {
	mtk_2p_charger_MASTER = 0,
	mtk_2p_charger_SLAVE,
};

static const char * const mtk_2p_charger_irq_name[] = {
	[mtk_2p_charger_MASTER] = "mtk_2p_charger_master_irq",
	[mtk_2p_charger_SLAVE] = "mtk_2p_charger_slave_irq",
};

static int mtk_2p_charger_role_data[] = {
	[mtk_2p_charger_MASTER] = mtk_2p_charger_MASTER,
	[mtk_2p_charger_SLAVE] = mtk_2p_charger_SLAVE,
};

#define IBAT_CHG_LIM_BASE			   50
#define IBAT_CHG_LIM_LSB				50

#define ITRICHG_BASE					12500//uA
#define ITRICHG_LSB					 12500//uA

#define IPRECHG_BASE					50
#define IPRECHG_LSB					 50

#define VFC_CHG_BASE					2800
#define VFC_CHG_LSB					 50

#define BAT_OVP_BASE					4000
#define BAT_OVP_LSB					 50

#define IBAT_ADC_LSV					3125 // uA

enum {
	ADC_IBAT,
	ADC_VBAT,
	ADC_VCHG,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

static const int mtk_2p_charger_adc_m[] = {
	3125, 125, 125, 3125, 5
};

static const int mtk_2p_charger_adc_l[] = {
	1000, 100, 100, 10000, 10
};


enum mtk_2p_charger_fields {
	DEVICE_REV, DEVICE_ID,
	IBAT_CHG_LIM,
	POW_LIM_DIS, VBALANCE_DIS, POW_LIM,
	ILIM_DIS, IBAT_CHG_LIM_DIS, BAT_DET_DIS, VDIFF_CHECK_DIS, LS_OFF, SHIP_EN,
	REG_RST, EN_LOWPOWER, VDIFF_OPEN_TH, AUTO_BSM_DIS, AUTO_BSM_TH, SHIP_WT,
	ITRICHG, VPRE_CHG,
	IPRECHG, VFC_CHG,
	CHG_OVP_DIS, CHG_OVP,
	BAT_OVP_DIS, BAT_OVP,
	CHG_OCP_DIS, CHG_OCP,
	DSG_OCP_DIS, DSG_OCP,
	TDIE_FLT_DIS, TDIE_FLT,
	TDIE_ALRM_DIS, TDIE_ALRM,
	CHG_OVP_DEG, BAT_OVP_DEG, CHG_OCP_DEG, DSG_OCP_DEG,
	AUTO_BSM_DEG,
	WORK_MODE, BAT_ABSENT_STAT, VDUFF_STAT,
	ADC_EN, ADC_RATE, ADC_FREEZE,
	F_MAX_FIELDS,
};

struct mtk_2p_charger_cfg_e {
	int bat_chg_lim_disable;
	int bat_chg_lim;
	int pow_lim_disable;
	int ilim_disable;
	int load_switch_disable;
	int lp_mode_enable;
	int auto_bsm_disable;
	int itrichg;
	int iprechg;
	int vfc_chg;
	int chg_ovp_disable;
	int chg_ovp;
	int bat_ovp_disable;
	int bat_ovp;
	int chg_ocp_disable;
	int chg_ocp;
	int dsg_ocp_disable;
	int dsg_ocp;
	int tdie_flt_disable;
	int tdie_alm_disable;
	int tdie_alm;
};

static struct mtk_2p_charger_cfg_e default_cfg = {
	.bat_chg_lim_disable = 0,
	.bat_chg_lim = 39,
	.pow_lim_disable = 0,
	.ilim_disable = 0,
	.load_switch_disable = 0,
	.lp_mode_enable = 0,
	.auto_bsm_disable = 1,
	.itrichg = 3,
	.iprechg = 2,
	.vfc_chg = 2,
	.chg_ovp_disable = 0,
	.chg_ovp = 0,
	.bat_ovp_disable = 0,
	.bat_ovp = 10,
	.chg_ocp_disable = 0,
	.chg_ocp = 2,
	.dsg_ocp_disable = 0,
	.dsg_ocp = 2,
	.tdie_flt_disable = 0,
	.tdie_alm_disable = 0,
	.tdie_alm = 9,
};

struct mtk_2p_charger_chip {
	const char *cs_name;
	struct charger_device *chg_dev;
	struct charger_properties chg_prop;
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];
	struct mtk_2p_charger_cfg_e *cfg;

	int disable_cs;
	int irq_gpio;
	int irq;
	int role;
};

//REGISTER
static const struct reg_field mtk_2p_charger_reg_fields[] = {
	/*reg00*/
	[DEVICE_REV] = REG_FIELD(0x00, 4, 7),
	[DEVICE_ID] = REG_FIELD(0x00, 0, 3),
	/*reg01*/
	[IBAT_CHG_LIM] = REG_FIELD(0x01, 0, 7),
	/*reg02*/
	[POW_LIM_DIS] = REG_FIELD(0x02, 7, 7),
	[VBALANCE_DIS] = REG_FIELD(0x02, 5, 5),
	[POW_LIM] = REG_FIELD(0x02, 0, 3),
	/*reg03*/
	[ILIM_DIS] = REG_FIELD(0x03, 7, 7),
	[IBAT_CHG_LIM_DIS] = REG_FIELD(0x03, 6, 6),
	[BAT_DET_DIS] = REG_FIELD(0x03, 5, 5),
	[VDIFF_CHECK_DIS] = REG_FIELD(0x03, 4, 4),
	[LS_OFF] = REG_FIELD(0x03, 3, 3),
	[SHIP_EN] = REG_FIELD(0x03, 0, 2),
	/*reg04*/
	[REG_RST] = REG_FIELD(0x04, 7, 7),
	[EN_LOWPOWER] = REG_FIELD(0x04, 6, 6),
	[VDIFF_OPEN_TH] = REG_FIELD(0x04, 4, 5),
	[AUTO_BSM_DIS] = REG_FIELD(0x04, 3, 3),
	[AUTO_BSM_TH] = REG_FIELD(0x04, 2, 2),
	[SHIP_WT] = REG_FIELD(0x04, 0, 0),
	/*reg05*/
	[ITRICHG] = REG_FIELD(0x05, 5, 7),
	[VPRE_CHG] = REG_FIELD(0x05, 0, 2),
	/*reg06*/
	[IPRECHG] = REG_FIELD(0x06, 4, 7),
	[VFC_CHG] = REG_FIELD(0x06, 0, 3),
	/*reg07*/
	[CHG_OVP_DIS] = REG_FIELD(0x07, 7, 7),
	[CHG_OVP] = REG_FIELD(0x07, 6, 6),
	/*reg08*/
	[BAT_OVP_DIS] = REG_FIELD(0x08, 7, 7),
	[BAT_OVP] = REG_FIELD(0x08, 2, 6),
	/*reg09*/
	[CHG_OCP_DIS] = REG_FIELD(0x09, 7, 7),
	[CHG_OCP] = REG_FIELD(0x09, 4, 6),
	/*reg0A*/
	[DSG_OCP_DIS] = REG_FIELD(0x0A, 7, 7),
	[DSG_OCP] = REG_FIELD(0x0A, 4, 6),
	/*reg0B*/
	[TDIE_FLT_DIS] = REG_FIELD(0x0B, 7, 7),
	[TDIE_FLT] = REG_FIELD(0x0B, 0, 3),
	/*reg0C*/
	[TDIE_ALRM_DIS] = REG_FIELD(0x0C, 7, 7),
	[TDIE_ALRM] = REG_FIELD(0x0C, 0, 3),
	/*reg0D*/
	[CHG_OVP_DEG] = REG_FIELD(0x0D, 6, 7),
	[BAT_OVP_DEG] = REG_FIELD(0x0D, 4, 5),
	[CHG_OCP_DEG] = REG_FIELD(0x0D, 2, 3),
	[DSG_OCP_DEG] = REG_FIELD(0x0D, 0, 1),
	/*reg0E*/
	[AUTO_BSM_DEG] = REG_FIELD(0x0E, 6, 7),
	/*reg0F*/
	[WORK_MODE] = REG_FIELD(0x0F, 5, 7),
	/*reg15*/
	[ADC_EN] = REG_FIELD(0x15, 7, 7),
	[ADC_RATE] = REG_FIELD(0x15, 6, 6),
	[ADC_FREEZE] = REG_FIELD(0x15, 5, 5),
};

static const struct regmap_config mtk_2p_charger_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int mtk_2p_charger_field_write(struct mtk_2p_charger_chip *sc,
				enum mtk_2p_charger_fields field_id, int val)
{
	int ret;

	ret = regmap_field_write(sc->rmap_fields[field_id], val);
	if (ret < 0)
		dev_info(sc->dev, "mtk_2p_charger write field %d fail: %d\n", field_id, ret);

	return ret;
}

static int mtk_2p_charger_read_block(struct mtk_2p_charger_chip *sc,
				int reg, uint8_t *val, int len)
{
	int ret;

	ret = regmap_bulk_read(sc->regmap, reg, val, len);
	if (ret < 0)
		dev_info(sc->dev, "mtk_2p_charger read %02x block failed %d\n", reg, ret);

	return ret;
}

/*******************************************************/
static int mtk_2p_charger_reg_reset(struct mtk_2p_charger_chip *sc)
{
	return mtk_2p_charger_field_write(sc, REG_RST, 1);
}

/* constant current */
static int mtk_2p_charger_set_ibat_limit(struct mtk_2p_charger_chip *sc, int curr)
{
	dev_info(sc->dev, "%s : %dmA\n", __func__, curr);

	return mtk_2p_charger_field_write(sc, IBAT_CHG_LIM,
			(curr - IBAT_CHG_LIM_BASE) / IBAT_CHG_LIM_LSB);
}

static int mtk_2p_charger_set_lowpower_mode(struct mtk_2p_charger_chip *sc, bool en)
{
	return mtk_2p_charger_field_write(sc, EN_LOWPOWER, !!en);
}

static int mtk_2p_charger_get_adc(struct mtk_2p_charger_chip *sc,
			int channel, int *result)
{
	int reg = 0x17 + channel * 2;
	u8 val[2] = {0};
	int ret;

	ret = mtk_2p_charger_read_block(sc, reg, val, 2);
	if (ret) {
		dev_info(sc->dev, "%s: failed: %d, %d, %d\n", __func__, reg, val[0], val[1]);
		return ret;
	}

	*result = (val[1] | (val[0] << 8)) *
				mtk_2p_charger_adc_m[channel] / mtk_2p_charger_adc_l[channel];

	dev_info(sc->dev, "%s: %d, %d\n", __func__, channel, *result);
	return ret;
}

static int mtk_2p_charger_dump_init_registers(struct mtk_2p_charger_chip *sc)
{
	int ret = 0;
	int i = 0;
	int val = 0;

	for (i = 0; i <= 0x20; i++) {
		ret = regmap_read(sc->regmap, i, &val);
		dev_info(sc->dev, "%s reg[0x%02x] = 0x%02x\n",
				__func__, i, val);
	}
	return ret;
}

#define DUMP_REG_BUF_SIZE 1024
static int mtk_2p_charger_dump_registers(struct charger_device *chg_dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;
	int i = 0;
	int val = 0;
	char buf[DUMP_REG_BUF_SIZE] = "\0";

	for (i = 0; i < 0x16; i++) {
		ret = regmap_read(sc->regmap, i, &val);
		scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "reg[0x%02x] = 0x%02x", i, val);
	}
	scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "reg[0x%02x] = 0x%02x", i, val);
	dev_info(sc->dev, "%s: %s\n", __func__, buf);
	return ret;
}

static irqreturn_t mtk_2p_charger_irq_handler(int irq, void *data)
{
	struct mtk_2p_charger_chip *sc = data;

	dev_info(sc->dev, "%s: irq: %d\n", __func__, irq);

	return IRQ_HANDLED;
}

static ssize_t mtk_2p_charger_registers_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(dev);
	u8 addr;
	int val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "mtk_2p_charger");
	if (idx < 0)
		return 0;

	for (addr = 0x0; addr <= 0x20; addr++) {
		ret = regmap_read(sc->regmap, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t mtk_2p_charger_registers_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x20)
		regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR_RW(mtk_2p_charger_registers);

static void mtk_2p_charger_create_device_node(struct device *dev)
{
	int ret = 0;
	if (!dev)
		return;
	ret = device_create_file(dev, &dev_attr_mtk_2p_charger_registers);
	if (ret) {
		dev_info(dev, "Failed to get 2p: %d\n", ret);
		return;
	}
}

static int mtk_2p_charger_parse_dt(struct mtk_2p_charger_chip *sc, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	int ret;
	struct {
		char *name;
		int *conv_data;
	} props[] = {
		{"sc,mtk-2p-charger,bat-chg-lim-disable", &(sc->cfg->bat_chg_lim_disable)},
		{"sc,mtk-2p-charger,bat-chg-lim", &(sc->cfg->bat_chg_lim)},
		{"sc,mtk-2p-charger,pow-lim-disable", &(sc->cfg->pow_lim_disable)},
		{"sc,mtk-2p-charger,ilim-disable", &(sc->cfg->ilim_disable)},
		{"sc,mtk-2p-charger,auto-bsm-mode-disable", &(sc->cfg->auto_bsm_disable)},
		{"sc,mtk-2p-charger,load-switch-disable", &(sc->cfg->load_switch_disable)},
		{"sc,mtk-2p-charger,low-power-mode-enable", &(sc->cfg->lp_mode_enable)},
		{"sc,mtk-2p-charger,itrichg", &(sc->cfg->itrichg)},
		{"sc,mtk-2p-charger,iprechg", &(sc->cfg->iprechg)},
		{"sc,mtk-2p-charger,vfc-chg", &(sc->cfg->vfc_chg)},
		{"sc,mtk-2p-charger,chg-ovp-disable", &(sc->cfg->chg_ovp_disable)},
		{"sc,mtk-2p-charger,chg-ovp", &(sc->cfg->chg_ovp)},
		{"sc,mtk-2p-charger,bat-ovp-disable", &(sc->cfg->bat_ovp_disable)},
		{"sc,mtk-2p-charger,bat-ovp", &(sc->cfg->bat_ovp)},
		{"sc,mtk-2p-charger,chg-ocp-disable", &(sc->cfg->chg_ocp_disable)},
		{"sc,mtk-2p-charger,chg-ocp", &(sc->cfg->chg_ocp)},
		{"sc,mtk-2p-charger,dsg-ocp-disable", &(sc->cfg->dsg_ocp_disable)},
		{"sc,mtk-2p-charger,dsg-ocp", &(sc->cfg->dsg_ocp)},
		{"sc,mtk-2p-charger,tdie-flt-disable", &(sc->cfg->tdie_flt_disable)},
		{"sc,mtk-2p-charger,tdie-alm-disable", &(sc->cfg->tdie_alm_disable)},
		{"sc,mtk-2p-charger,tdie-alm", &(sc->cfg->tdie_alm)},
	};

	ret = of_get_named_gpio(np, "mtk-2p-charger,intr-gpio", 0);
	if (ret < 0) {
		dev_info(sc->dev, "no intr-gpio info\n");
		return ret;
	}
	sc->irq_gpio = ret;

	/* initialize data for optional properties */
	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = of_property_read_u32(np, props[i].name,
						props[i].conv_data);
		if (ret < 0) {
			dev_info(sc->dev, "can not read %s\n", props[i].name);
			continue;
		}
	}

	return ret;
}

static int mtk_2p_charger_init_device(struct mtk_2p_charger_chip *sc)
{
	int ret = 0;
	int i;
	struct {
		enum mtk_2p_charger_fields field_id;
		int conv_data;
	} props[] = {
		{IBAT_CHG_LIM_DIS, sc->cfg->bat_chg_lim_disable},
		{IBAT_CHG_LIM, sc->cfg->bat_chg_lim},
		{POW_LIM_DIS, sc->cfg->pow_lim_disable},
		{ILIM_DIS, sc->cfg->ilim_disable},
		{LS_OFF, sc->cfg->load_switch_disable},
		{EN_LOWPOWER, sc->cfg->lp_mode_enable},
		{AUTO_BSM_DIS, sc->cfg->auto_bsm_disable},
		{ITRICHG, sc->cfg->itrichg},
		{IPRECHG, sc->cfg->iprechg},
		{VFC_CHG, sc->cfg->vfc_chg},
		{CHG_OVP_DIS, sc->cfg->chg_ovp_disable},
		{CHG_OVP, sc->cfg->chg_ovp},
		{BAT_OVP_DIS, sc->cfg->bat_ovp_disable},
		{BAT_OVP, sc->cfg->bat_ovp},
		{CHG_OCP_DIS, sc->cfg->chg_ocp_disable},
		{CHG_OCP, sc->cfg->chg_ocp},
		{DSG_OCP_DIS, sc->cfg->dsg_ocp_disable},
		{DSG_OCP, sc->cfg->dsg_ocp},
		{TDIE_FLT_DIS, sc->cfg->tdie_flt_disable},
		{TDIE_ALRM_DIS, sc->cfg->tdie_alm_disable},
		{TDIE_ALRM, sc->cfg->tdie_alm},
	};

	ret = mtk_2p_charger_reg_reset(sc);
	if (ret < 0)
		dev_info(sc->dev, "%s Failed to reset registers(%d)\n", __func__, ret);


	for (i = 0; i < ARRAY_SIZE(props); i++)
		ret = mtk_2p_charger_field_write(sc, props[i].field_id, props[i].conv_data);

	return mtk_2p_charger_dump_init_registers(sc);
}

static int mtk_2p_charger_register_interrupt(struct mtk_2p_charger_chip *sc)
{
	int ret = 0;

	ret = devm_gpio_request(sc->dev, sc->irq_gpio, "mtk_2p_charger,intr-gpio");
	if (ret < 0) {
		dev_info(sc->dev, "failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(sc->irq_gpio);
	if (ret < 0) {
		dev_info(sc->dev, "failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	sc->irq = gpio_to_irq(sc->irq_gpio);
	if (ret < 0) {
		dev_info(sc->dev, "failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	if (sc->role < 0)
		return -1;
	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					mtk_2p_charger_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					mtk_2p_charger_irq_name[sc->role], sc);
	if (ret < 0) {
		dev_info(sc->dev, "request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(sc->irq);

	return 0;
}

/* mediatek implement */
static int mtk_2p_charger_set_cc(struct charger_device *chg_dev,
								u32 cc)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	ret = mtk_2p_charger_set_ibat_limit(sc, cc);
	dev_info(&(chg_dev->dev), "%s: %d, ret=%d\n", __func__, cc, ret);

	return ret;
}

static int mtk_2p_charger_set_cv(struct charger_device *chg_dev,
								u32 cv)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;

	dev_info(&(chg_dev->dev), "%s: not support on mtk_2p_charger\n", __func__);
	return ret;
}

static int mtk_2p_charger_get_vbus(struct charger_device *chg_dev,
								u32 *vbus)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;
	ret = mtk_2p_charger_get_adc(sc, ADC_VCHG, vbus);
	if (ret < 0)
		dev_info(&(chg_dev->dev), "%s: Failed to get vbus_adc\n", __func__);
	dev_info(&(chg_dev->dev), "%s: %d\n", __func__, *vbus);
	return ret;
}

static int mtk_2p_charger_get_vbat(struct charger_device *chg_dev,
								u32 *vbat)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;
	ret = mtk_2p_charger_get_adc(sc, ADC_VBAT, vbat);
	if (ret < 0)
		dev_info(&(chg_dev->dev), "%s: Failed to get vbat_adc\n", __func__);
	dev_info(&(chg_dev->dev), "%s: %d\n", __func__, *vbat);
	return ret;
}

static int mtk_2p_charger_get_ibat(struct charger_device *chg_dev,
								int *ibat)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;
	ret = mtk_2p_charger_get_adc(sc, ADC_IBAT, ibat);
	if (ret < 0)
		dev_info(&(chg_dev->dev), "%s: Failed to get ibat_adc\n", __func__);
	dev_info(&(chg_dev->dev), "%s: %d\n", __func__, *ibat);
	return ret;
}

static int mtk_2p_charger_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;

	dev_info(&(chg_dev->dev), "not support on mtk_2p_charger\n");
	return ret;
}

static int mtk_2p_charger_cs_enable_lowpower(struct charger_device *chg_dev, bool enable)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));

	if (sc->disable_cs) {
		dev_info(&(chg_dev->dev), "%s dismiss\n", __func__);
		return -1;
	}
	mtk_2p_charger_set_lowpower_mode(sc, enable);

	dev_info(&(chg_dev->dev), "%s: %d\n", __func__, enable);
	return 0;
}

static int mtk_2p_charger_chg_is_charge_done(struct charger_device *chg_dev
									, bool *done)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;

	dev_info(&(chg_dev->dev), "not support on mtk_2p_charger\n");
	return ret;
}

static int mtk_2p_charger_check_cs_temp(struct charger_device *chg_dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;

	dev_info(&(chg_dev->dev), "not support on mtk_2p_charger\n");
	return ret;
}

static int mtk_2p_charger_dump_init_setting(struct charger_device *chg_dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (sc->disable_cs)
		return -1;

	dev_info(&(chg_dev->dev), "no necessary on mtk_2p_charger\n");
	return ret;
}

static int mtk_2p_charger_init_setting(struct charger_device *chg_dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(&(chg_dev->dev));
	int ret = 0;

	if (!sc) {
		dev_info(&(chg_dev->dev), "mtk_2p_charger init failed\n");
		return -1;
	}

	if (sc->disable_cs)
		return -1;



	/* disable auto BSM mode : for WA */
	mtk_2p_charger_field_write(sc, AUTO_BSM_DIS, 1);
	if (ret < 0)
		dev_info(&(chg_dev->dev), "mtk_2p_charger: write auto_bsm_dis failed\n");

	/* enable mtk_2p_charger ADC */
	mtk_2p_charger_field_write(sc, ADC_EN, 1);
	if (ret < 0)
		dev_info(&(chg_dev->dev), "mtk_2p_charger: write auto_bsm_dis failed\n");

	return ret;
}

static const struct of_device_id mtk_2p_charger_charger_match_table[] = {
	{   .compatible = "mediatek,mtk_2p_charger_master",
		.data = &mtk_2p_charger_role_data[mtk_2p_charger_MASTER], },
	{   .compatible = "mediatek,mtk_2p_charger_slave",
		.data = &mtk_2p_charger_role_data[mtk_2p_charger_SLAVE], },
	{ },
};

static const struct charger_ops mtk_2p_charger_chg_ops = {
	.get_vbus_adc = mtk_2p_charger_get_vbus,
	.get_vbat_adc = mtk_2p_charger_get_vbat,
	.get_cs_current = mtk_2p_charger_get_ibat,
	.set_constant_voltage = mtk_2p_charger_set_cv,
	.set_charging_current = mtk_2p_charger_set_cc,
	.dump_registers = mtk_2p_charger_dump_registers,
	.dump_init_setting = mtk_2p_charger_dump_init_setting,
	.cs_init_setting = mtk_2p_charger_init_setting,
	.is_charging_done = mtk_2p_charger_chg_is_charge_done,
	.check_cs_temp = mtk_2p_charger_check_cs_temp,
	.cs_enable_lowpower = mtk_2p_charger_cs_enable_lowpower,
	.event = mtk_2p_charger_do_event,
};

static int mtk_2p_charger_register_chgdev(struct mtk_2p_charger_chip *sc)
{
	sc->chg_prop.alias_name = sc->cs_name;
	sc->chg_dev = charger_device_register(sc->cs_name, sc->dev,
									sc, &mtk_2p_charger_chg_ops,
									&sc->chg_prop);
	return sc->chg_dev ? 0 : -EINVAL;
}

static int mtk_2p_charger_charger_probe(struct i2c_client *client)
{
	struct mtk_2p_charger_chip *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret = 0;
	int i;

	sc = devm_kzalloc(&client->dev, sizeof(struct mtk_2p_charger_chip), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;
	sc->client = client;
	sc->cs_name = "current_selector_master";
	sc->regmap = devm_regmap_init_i2c(client,
							&mtk_2p_charger_regmap_config);
	if (IS_ERR(sc->regmap)) {
		dev_info(sc->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mtk_2p_charger_reg_fields); i++) {
		const struct reg_field *reg_fields = mtk_2p_charger_reg_fields;

		sc->rmap_fields[i] =
			devm_regmap_field_alloc(sc->dev,
						sc->regmap,
						reg_fields[i]);
		if (IS_ERR(sc->rmap_fields[i])) {
			dev_info(sc->dev, "cannot allocate regmap field\n");
			return PTR_ERR(sc->rmap_fields[i]);
		}
	}

	i2c_set_clientdata(client, sc);
	mtk_2p_charger_create_device_node(&(client->dev));

	match = of_match_node(mtk_2p_charger_charger_match_table, node);
	if (match == NULL) {
		dev_info(sc->dev, "device tree match not found!\n");
		goto err_get_match;
	}

	sc->role = *(int *)match->data;

	sc->cfg = &default_cfg;
	ret = mtk_2p_charger_parse_dt(sc, &client->dev);
	if (ret < 0) {
		dev_info(sc->dev, "%s parse dt failed(%d)\n", __func__, ret);
		goto err_parse_dt;
	}

	ret = mtk_2p_charger_init_device(sc);
	if (ret < 0) {
		dev_info(sc->dev, "%s init device failed(%d)\n", __func__, ret);
		goto err_init_device;
	}

	ret = mtk_2p_charger_register_interrupt(sc);
	if (ret < 0) {
		dev_info(sc->dev, "%s register irq fail(%d)\n",
					__func__, ret);
		goto err_register_irq;
	}

	ret = mtk_2p_charger_register_chgdev(sc);
	if (ret < 0) {
		dev_info(sc->dev, "%s register chgdev failed(%d)\n",
					__func__, ret);
		goto err_register_chgdev;
	}

	dev_info(sc->dev, "mtk_2p_charger[%s] probe successfully!\n",
			sc->role == mtk_2p_charger_MASTER ? "master" : "slave");
	return 0;

err_register_irq:
err_init_device:
err_parse_dt:
err_get_match:
err_register_chgdev:
	dev_info(sc->dev, "mtk_2p_charger probe failed!\n");
	devm_kfree(sc->dev, sc);
	return ret;
}


static void mtk_2p_charger_charger_remove(struct i2c_client *client)
{
	struct mtk_2p_charger_chip *sc = i2c_get_clientdata(client);

	if (sc)
		devm_kfree(sc->dev, sc);
}

#ifdef CONFIG_PM_SLEEP
static int mtk_2p_charger_suspend(struct device *dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(dev);

	dev_info(sc->dev, "Suspend successfully!");
	if (device_may_wakeup(dev))
		enable_irq_wake(sc->irq);
	disable_irq(sc->irq);

	return 0;
}
static int mtk_2p_charger_resume(struct device *dev)
{
	struct mtk_2p_charger_chip *sc = dev_get_drvdata(dev);

	dev_info(sc->dev, "Resume successfully!");
	if (device_may_wakeup(dev))
		disable_irq_wake(sc->irq);
	enable_irq(sc->irq);

	return 0;
}

static const struct dev_pm_ops mtk_2p_charger_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_2p_charger_suspend, mtk_2p_charger_resume)
};
#endif

static struct i2c_driver mtk_2p_charger_charger_driver = {
	.driver	 = {
		.name   = "mtk_2p_charger",
		.owner  = THIS_MODULE,
		.of_match_table = mtk_2p_charger_charger_match_table,
#ifdef CONFIG_PM_SLEEP
		.pm = &mtk_2p_charger_pm,
#endif
	},
	.probe	  = mtk_2p_charger_charger_probe,
	.remove	 = mtk_2p_charger_charger_remove,
};

module_i2c_driver(mtk_2p_charger_charger_driver);

MODULE_DESCRIPTION("SC mtk_2p_charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");

