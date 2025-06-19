// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6397/core.h>/* PMIC MFD core header */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/suspend.h>

#define PMIC_RGS_CHRDET_ADDR		(0xa88)
#define PMIC_RGS_CHRDET_MASK		(0x1)
#define PMIC_RGS_CHRDET_SHIFT	(5)

#define R_CHARGER_1	330
#define R_CHARGER_2	39

struct mtk_chr_det {
	struct mt6397_chip *chip;
	struct regmap *regmap;
	struct platform_device *pdev;

	struct power_supply *bc12_psy;
	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	//BSP:modify LGQNHBJE-406 by jie.hu 20220526 start
	struct power_supply *chg_type_psy;
	//BSP:modify LGQNHBJE-406 by jie.hu 20220526 end

	struct iio_channel *chan_vbus;
	//BSP:modify LGQNHBJE-406 by jie.hu 20220526 start
	struct notifier_block type_nb;
	struct work_struct chr_work;
	struct work_struct rechr_work;
	int det_cnt;
	//BSP:modify LGQNHBJE-406 by jie.hu 20220526 end

	enum power_supply_usb_type type;

	/* suspend notify */
	struct notifier_block pm_nb;
	bool is_suspend;

	u32 bootmode;
	u32 boottype;
	bool vcdt_int_active;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static enum power_supply_property chr_type_properties[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

void bc11_set_register_value(struct regmap *map,
	unsigned int addr,
	unsigned int mask,
	unsigned int shift,
	unsigned int val)
{
	regmap_update_bits(map,
		addr,
		mask << shift,
		val << shift);
}

unsigned int get_register_value(struct regmap *map,
	unsigned int addr,
	unsigned int mask,
	unsigned int shift)
{
	unsigned int value = 0;

	regmap_read(map, addr, &value);
	value =
		(value &
		(mask << shift))
		>> shift;
	return value;
}

static int get_vbus_voltage(struct mtk_chr_det *info,
	int *val)
{
	int ret;

	if (!IS_ERR(info->chan_vbus)) {
		ret = iio_read_channel_processed(info->chan_vbus, val);
		if (ret < 0)
			pr_notice("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		//pr_notice("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	*val = (((R_CHARGER_1 +
			R_CHARGER_2) * 100 * *val) /
			R_CHARGER_2) / 100;

	return ret;
}


void do_charger_detect(struct mtk_chr_det *info, bool en)
{
	union power_supply_propval prop_online;
	int ret = 0;

	prop_online.intval = en;

	ret = power_supply_set_property(info->bc12_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop_online);
	if (ret)
		pr_info("%s: Failed to set online property\n", __func__);
}

static int mtk_chr_det_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct mtk_chr_det *info;

	info = (struct mtk_chr_det *)container_of(notifier,
		struct mtk_chr_det, pm_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		info->is_suspend = true;
		pr_info("%s: enter PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		info->is_suspend = false;
		pr_info("%s: enter PM_POST_SUSPEND\n", __func__);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

//BSP:modify LGQNHBJE-406 by jie.hu 20220526 start
static void get_charger_type(struct mtk_chr_det *info, bool en)
{
	union power_supply_propval prop_type, prop_usb_type;
	int ret = 0;

	ret = power_supply_get_property(info->bc12_psy,
			POWER_SUPPLY_PROP_TYPE, &prop_type);
	ret = power_supply_get_property(info->bc12_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop_usb_type);
	pr_notice("type:%d usb_type:%d ret:%d\n", prop_type.intval, prop_usb_type.intval, ret);
	if ((prop_type.intval == POWER_SUPPLY_TYPE_USB) &&
			(prop_usb_type.intval == POWER_SUPPLY_USB_TYPE_DCP) && info->det_cnt < 10) {
		pr_notice("%s: det_cnt:%d\n", __func__, info->det_cnt);
		info->det_cnt++;
		do_charger_detect(info, en);
	}
}
//BSP:modify LGQNHBJE-406 by jie.hu 20220526 end
#if IS_ENABLED(CONFIG_TRAN_FOLD_DISPLAY)
extern int mtkfb_set_backlight_level(unsigned int level);
#endif
static void do_charger_detection_work(struct work_struct *dwork)
{
//BSP:modify LGQNHBJE-406 by jie.hu 20220526 start
//	struct work_struct *dwork = to_delayed_work(data);
	struct mtk_chr_det *info = (struct mtk_chr_det *)container_of(
					dwork, struct mtk_chr_det, chr_work);
//BSP:modify LGQNHBJE-406 by jie.hu 20220526 end
	unsigned int chrdet = 0;

	chrdet = get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
	if (chrdet) {
		do_charger_detect(info, chrdet);
	} else {
		/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
		/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
		if (info->bootmode == 8 || info->bootmode == 9) {
			pr_info("%s: Unplug Charger/USB\n", __func__);

			while (1) {
				if (info->is_suspend == false) {
					pr_info("%s, not in suspend, shutdown\n", __func__);
					#if IS_ENABLED(CONFIG_TRAN_FOLD_DISPLAY)
					mtkfb_set_backlight_level(0);
					#endif
					kernel_power_off();
				} else {
					pr_info("%s, suspend, cannot shutdown\n", __func__);
					msleep(20);
				}
			}
		}
	}
}

//BSP:modify LGQNHBJE-406 by jie.hu 20220819 start
static void redetect_charger_detection_work(struct work_struct *dwork)
{
	struct mtk_chr_det *info = (struct mtk_chr_det *)container_of(
					dwork, struct mtk_chr_det, rechr_work);
	unsigned int chrdet = 0;

	chrdet = get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);
	if (!chrdet) {
		info->det_cnt = 0;
		return;
	}
	get_charger_type(info, chrdet);
}
//BSP:modify LGQNHBJE-406 by jie.hu 20220819 end

irqreturn_t chrdet_int_handler(int irq, void *data)
{
	struct mtk_chr_det *info = data;
	unsigned int chrdet = 0;

	chrdet = get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);
	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
	do_charger_detect(info, chrdet);

	//BSP:modify USICPRO-22079 by lin.xiang 20230412 start
	if (info->bootmode == 8 || info->bootmode == 9)
		schedule_work(&info->chr_work);
	//BSP:modify USICPRO-22079 by lin.xiang 20230412 end

	return IRQ_HANDLED;
}

static int psy_chr_type_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_chr_det *info;
	int vbus = 0;

	pr_notice("%s: prop:%d\n", __func__, psp);
	info = (struct mtk_chr_det *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		get_vbus_voltage(info, &vbus);
		val->intval = vbus;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int check_boot_mode(struct mtk_chr_det *info, struct device *dev)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_notice("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_notice("%s: failed to get atag,boot\n", __func__);
		else {
			pr_notice("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}
	return 0;
}

//BSP:modify LGQNHBJE-406 by jie.hu 20220819 start
static int chg_type_try_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;
	static struct power_supply *chg_type_psy;
	struct mtk_chr_det *mci = (struct mtk_chr_det *)container_of(nb,
						    struct mtk_chr_det, type_nb);

	if (event != PSY_EVENT_PROP_CHANGED){
		pr_info("%s no CHANGED\n",__func__);
		return NOTIFY_OK;
	}

	chg_type_psy = mci->bc12_psy;

	if (IS_ERR_OR_NULL(chg_type_psy)) {
		pr_info("%s retry to get chg_psy\n", __func__);
		chg_type_psy = power_supply_get_by_name("charger");
		mci->chg_type_psy = chg_type_psy;
	}

	if (IS_ERR_OR_NULL(chg_type_psy)) {
		return NOTIFY_OK;
	}

	if (psy == chg_type_psy) {
		pr_info("%s: rechg_work\n", __func__);
		schedule_work(&mci->rechr_work);
	}

	return NOTIFY_OK;
}
//BSP:modify LGQNHBJE-406 by jie.hu 20220819 end

static int mtk_chr_det_probe(struct platform_device *pdev)
{
	struct mtk_chr_det *info;
	struct iio_channel *chan_vbus;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_notice("%s: starts\n", __func__);

	chan_vbus = devm_iio_channel_get(
		&pdev->dev, "pmic_vbus");
	if (IS_ERR(chan_vbus)) {
		pr_notice("mt6357 charger type requests probe deferral \n");
		return -EPROBE_DEFER;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = (struct mt6397_chip *)dev_get_drvdata(pdev->dev.parent);
	info->regmap = info->chip->regmap;

	dev_set_drvdata(&pdev->dev, info);
	info->pdev = pdev;

	check_boot_mode(info, &pdev->dev);

	info->bc12_psy = devm_power_supply_get_by_phandle(&pdev->dev, "bc12");
	if (IS_ERR_OR_NULL(info->bc12_psy)) {
		pr_notice("Failed to get bc12 psy\n");
		info->bc12_psy = power_supply_get_by_name("charger");
		if(IS_ERR_OR_NULL(info->bc12_psy))
		{
			pr_notice("Failed to get charger psy by name!\n");
		}
		//return PTR_ERR(info->bc12_psy);
	}

	info->psy_desc.name = "mtk_charger_type";
	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = chr_type_properties;
	info->psy_desc.num_properties = ARRAY_SIZE(chr_type_properties);
	info->psy_desc.get_property = psy_chr_type_get_property;
	info->psy_cfg.drv_data = info;
	info->psy_cfg.of_node = np;

	info->psy = power_supply_register(&pdev->dev, &info->psy_desc, &info->psy_cfg);

	info->chan_vbus = devm_iio_channel_get(
		&pdev->dev, "pmic_vbus");
	if (IS_ERR(info->chan_vbus)) {
		//pr_notice("chan_vbus auxadc get fail, ret=%d\n",
		//	PTR_ERR(info->chan_vbus));
		pr_notice("chan_vbus auxadc get fail\n");
	}

	info->pm_nb.notifier_call = mtk_chr_det_pm_event;
	//BSP:add for pm notify register by lei.shi5 20221206 start
	register_pm_notifier(&info->pm_nb);
	//BSP:add for pm notify register by lei.shi5 20221206 end

	if (of_property_read_bool(np, "vcdt_int_active"))
		info->vcdt_int_active = true;

	pr_notice("%s: vcdt_int_active:%d\n", __func__, info->vcdt_int_active);

	//BSP:modify LGQNHBJE-406 by jie.hu 20220819 start
	INIT_WORK(&info->rechr_work, redetect_charger_detection_work);
	//BSP:modify LGQNHBJE-406 by jie.hu 20220819 end
	if (info->vcdt_int_active) {
		//BSP:modify LGQNHBJE-406 by jie.hu 20220526 start
		info->type_nb.notifier_call = chg_type_try_psy_notifier_cb;
		power_supply_reg_notifier(&info->type_nb);
		INIT_WORK(&info->chr_work, do_charger_detection_work);
		schedule_work(&info->chr_work);
		//BSP:modify LGQNHBJE-406 by jie.hu 20220526 end
		ret = devm_request_threaded_irq(&pdev->dev,
			platform_get_irq_byname(pdev, "CHRDET"), NULL,
			chrdet_int_handler, IRQF_TRIGGER_HIGH, "CHRDET", info);
		if (ret < 0)
			pr_notice("%s request chrdet irq fail\n", __func__);
	}


	pr_notice("%s: done\n", __func__);

	return 0;
}

static const struct of_device_id mtk_chr_det_of_match[] = {
	{.compatible = "mediatek,mtk-chr-det",},
	{},
};

static int mtk_chr_det_remove(struct platform_device *pdev)
{
	struct mtk_chr_det *info = platform_get_drvdata(pdev);

	if (info)
		devm_kfree(&pdev->dev, info);
	return 0;
}

MODULE_DEVICE_TABLE(of, mtk_chr_det_of_match);

static struct platform_driver mtk_chr_det_driver = {
	.probe = mtk_chr_det_probe,
	.remove = mtk_chr_det_remove,
	.driver = {
		.name = "mtk-charger-detection",
		.of_match_table = mtk_chr_det_of_match,
		},
};

static int __init mtk_chr_det_init(void)
{
	return platform_driver_register(&mtk_chr_det_driver);
}
module_init(mtk_chr_det_init);

static void __exit mtk_chr_det_exit(void)
{
	platform_driver_unregister(&mtk_chr_det_driver);
}
module_exit(mtk_chr_det_exit);

MODULE_AUTHOR("gerard.huangn <gerard.huang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Type Detection Driver");
MODULE_LICENSE("GPL");
