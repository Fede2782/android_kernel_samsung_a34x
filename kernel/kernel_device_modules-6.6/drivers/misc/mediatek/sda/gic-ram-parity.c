// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <mt-plat/aee.h>
#include <linux/regmap.h>

/* GICT related reg offset for error record 1 */
#define GICT_ERR1STATUS	(0x50)
#define GICT_ERR1MISC0	(0x60)

/* GICT related reg offset for error record 2 */
#define GICT_ERR2STATUS	(0x90)
#define GICT_ERR2MISC0	(0xA0)

#define GICD_CFGID	(0xF000)
#define SPIS_SHIFT_BIT	(15)
#define SPIS_MASK	(0x3F << SPIS_SHIFT_BIT)

#define UE_BIT	(1ULL << 29)

enum error_type_index {
	CE = 0,
	UE,
};

struct reg_offset {
	unsigned int misc0_offset;
	unsigned int status_offset;
};

struct err_record {
	u32 irq;
	char *error_type;
	u64 misc0;
	u64 status;
	struct reg_offset reg_offset;
};

struct power_domain {
	struct regmap *map;
	unsigned int reg;
	/*unit: uV*/
	unsigned int vol;
};

struct gic_ram_parity {
	struct work_struct work;

	/* setting from device tree */
	unsigned int nr_irq;
	void __iomem *gicd_base;
	void __iomem *gict_base;

	/* recorded parity errors */
	atomic_t nr_err;
	struct err_record *record;
	struct power_domain power_domain[2];
};
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define ECC_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)
#else
#define ECC_LOG(fmt, ...)
#endif

static struct gic_ram_parity gic_ram_parity;

static u64 id_mask;

static ssize_t gic_ram_status_show(struct device_driver *driver, char *buf)
{
	unsigned int nr_err;

	nr_err = atomic_read(&gic_ram_parity.nr_err);

	if (nr_err)
		return scnprintf(buf, PAGE_SIZE, "True, %u times\n",
				nr_err);
	else
		return scnprintf(buf, PAGE_SIZE, "False\n");
}

static DRIVER_ATTR_RO(gic_ram_status);

static void gic_ram_parity_irq_work(struct work_struct *w)
{
	static char *buf;
	int n;

	if (!buf) {
		buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!buf)
			goto call_aee;
	}

	n = 0;

	if (!(gic_ram_parity.record[UE].status & UE_BIT)) {
		n += scnprintf(buf + n, PAGE_SIZE - n,
				"%s type ECC error has occurred before PWR DWN!\n",
				gic_ram_parity.record[UE].error_type);
		if (n > PAGE_SIZE)
			goto call_aee;

		n += scnprintf(buf + n, PAGE_SIZE - n,
				"Please check tf-a log for datail\n");
		if (n > PAGE_SIZE)
			goto call_aee;
	}

	n += scnprintf(buf + n, PAGE_SIZE - n, "gic ram parity error,");
	if (n > PAGE_SIZE)
		goto call_aee;

	n += scnprintf(buf + n, PAGE_SIZE - n, "%s:%s, %s:0x%016llx, %s:0x%016llx, %s:%llu, %s:%d, %s:%d",
			"error type", gic_ram_parity.record[UE].error_type,
			"status", gic_ram_parity.record[UE].status,
			"misc0", gic_ram_parity.record[UE].misc0,
			"INTID", ((gic_ram_parity.record[UE].misc0) & id_mask) + 32,
			"vcore sram voltage", gic_ram_parity.power_domain[0].vol,
			"vcore voltage", gic_ram_parity.power_domain[1].vol);

	if (n > PAGE_SIZE)
		goto call_aee;

call_aee:
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	/* Note: the length of message printed by aee_kernel_exception is limited by
	 * KERNEL_REPORT_LENGTH (should be 344), if oversize, it will be cut off.
	 */
	aee_kernel_exception("gic ram parity", "%s\n%s", buf, "CRDISPATCH_KEY:GIC RAM Parity Issue");
#endif
	pr_debug("CRDISPATCH_KEY:GIC RAM Parity Issue");
	if (!buf)
		kfree(buf);
}

static irqreturn_t gic_ram_parity_isr(int irq, void *dev_id)
{
	struct reg_offset *offset;
	void __iomem *base;
	u64 misc0, status;
	unsigned int i, val = 0;

	atomic_inc(&gic_ram_parity.nr_err);

	for (i = 0, offset = NULL; i < gic_ram_parity.nr_irq; i++) {
		if (gic_ram_parity.record[i].irq == irq) {
			offset = &(gic_ram_parity.record[i].reg_offset);
			pr_info("parity isr for %d\n", i);
			break;
		}
	}

	if (offset == NULL) {
		pr_info("no matched irq %d\n", irq);
		return IRQ_HANDLED;
	}

	base = gic_ram_parity.gict_base;

	status = readl(base + offset->status_offset);
	misc0 = readl(base + offset->misc0_offset);

	/* clear error status, write 1 to clear */
	writel(status, base + offset->status_offset);

	/* save error recorded data into global variable for workqueue to dump DB */
	gic_ram_parity.record[i].status = status;
	gic_ram_parity.record[i].misc0 = misc0;

	/* read voltage */
	regmap_read(gic_ram_parity.power_domain[0].map,
			gic_ram_parity.power_domain[0].reg,
			&val);
	gic_ram_parity.power_domain[0].vol = val*6250;

	regmap_read(gic_ram_parity.power_domain[1].map,
			gic_ram_parity.power_domain[1].reg,
			&val);
	gic_ram_parity.power_domain[1].vol = val*5000;

	if (gic_ram_parity.record[UE].irq == irq)
		schedule_work(&gic_ram_parity.work);

	if (!(gic_ram_parity.record[i].status & UE_BIT)) {
		pr_info("%s type ECC error has occurred before PWR DWN!\n",
				gic_ram_parity.record[i].error_type);
		pr_info("Please check tf-a log for datail\n");
		return IRQ_HANDLED;
	}

	ECC_LOG("GIC RAM ECC error, %s %s, %s: 0x%016llx, %s: 0x%016llx, %s: %llu\n",
			"error type", gic_ram_parity.record[i].error_type,
			"misc0", misc0, "status", status, "INTID",
			(misc0 & id_mask) + 32);
	ECC_LOG("%s: %d(uV)\n", "vcore_sram voltage", gic_ram_parity.power_domain[0].vol);
	ECC_LOG("%s: %d(uV)\n", "vcore voltage", gic_ram_parity.power_domain[1].vol);

	/* disable irq to avoid burst kernel log and DB */
	disable_irq_nosync(irq);

	return IRQ_HANDLED;
}

static int __count_gic_ram_parity_irq(struct device_node *dev)
{
	struct of_phandle_args oirq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &oirq) == 0)
		nr++;

	return nr;
}

static unsigned int get_misc0_data_id_mask(void)
{
	u64 num_spi, val;
	void __iomem *base;

	base = gic_ram_parity.gicd_base;

	num_spi = ((readl(base + GICD_CFGID) & SPIS_MASK) >> SPIS_SHIFT_BIT) * 32;

	val = 1;

	while (val < num_spi) {
		val <<= 1;

		if (val == num_spi)
			return (val - 1);
	}

	return (val - 1);
}

static int gic_ram_parity_probe(struct platform_device *pdev)
{
	int ret;
	size_t size;
	int i, irq;
	struct device_node *node;
	struct platform_device *pmic_pdev = NULL;
	struct platform_device *vcore_pdev = NULL;
	struct of_phandle_args args;

	dev_info(&pdev->dev, "driver probed\n");

	/* initialize struct gic_ram_parity  */
	INIT_WORK(&gic_ram_parity.work, gic_ram_parity_irq_work);

	gic_ram_parity.nr_irq = __count_gic_ram_parity_irq(pdev->dev.of_node);

	/* get gicd and gicr base from "arm,gic-v3" dts node */
	node = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	gic_ram_parity.gicd_base = of_iomap(node, 0);
	if (!gic_ram_parity.gicd_base)
		return -ENOMEM;
	gic_ram_parity.gict_base = gic_ram_parity.gicd_base + 0x20000;

	atomic_set(&gic_ram_parity.nr_err, 0);

	size = sizeof(struct err_record) * gic_ram_parity.nr_irq;
	gic_ram_parity.record = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!gic_ram_parity.record)
		return -ENOMEM;

	/* initialize struct err_record */
	gic_ram_parity.record[CE].error_type = "CE";
	gic_ram_parity.record[UE].error_type = "UE";

	/* setup GICT* offset */
	gic_ram_parity.record[CE].reg_offset.misc0_offset = GICT_ERR1MISC0;
	gic_ram_parity.record[CE].reg_offset.status_offset = GICT_ERR1STATUS;
	gic_ram_parity.record[UE].reg_offset.misc0_offset = GICT_ERR2MISC0;
	gic_ram_parity.record[UE].reg_offset.status_offset = GICT_ERR2STATUS;

	id_mask = get_misc0_data_id_mask();

	/* request irq for handling CE & UE type error */
	for (i = 0; i < gic_ram_parity.nr_irq; i++) {
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		if (irq == 0) {
			dev_err(&pdev->dev,
					"failed to irq_of_parse_and_map %d\n", 0);
			return -ENXIO;
		}
		gic_ram_parity.record[i].irq = irq;

		ret = devm_request_irq(&pdev->dev, irq, gic_ram_parity_isr,
				IRQF_TRIGGER_NONE | IRQF_ONESHOT,
				"gic_ram_parity", NULL);
		if (ret) {
			dev_err(&pdev->dev,
					"failed to request irq for irq %d\n", irq);
			return -ENXIO;
		}
	}

	/* init struct power_domain[0] for get vcore_sram volt later */
	node = of_find_node_by_name(NULL, "pmic");
	if (!node) {
		dev_err(&pdev->dev, "pmic node not found\n");
		return -ENODEV;
	}

	pmic_pdev = of_find_device_by_node(node->child);
	if (!pmic_pdev) {
		dev_err(&pdev->dev, "pmic child device not found\n");
		return -ENODEV;
	}

	gic_ram_parity.power_domain[0].map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	if (!gic_ram_parity.power_domain[0].map)
		return -ENODEV;

	ret = of_property_read_u32(pdev->dev.of_node,
			"pmic-reg", &gic_ram_parity.power_domain[0].reg);
	if (ret) {
		dev_err(&pdev->dev, "no pmic-reg");
		return -ENXIO;
	}

	/* init struct power_domain[1] for get vcore volt later */
	ret = of_parse_phandle_with_fixed_args(pdev->dev.of_node, "vcore-volt", 1, 0, &args);
	if (ret)
		return ret;

	gic_ram_parity.power_domain[1].reg = args.args[0];

	vcore_pdev = of_find_device_by_node(args.np->child);
	if (!vcore_pdev) {
		dev_err(&pdev->dev, "no vcore_pdev\n");
		return -ENXIO;
	}

	gic_ram_parity.power_domain[1].map = dev_get_regmap(vcore_pdev->dev.parent, NULL);
	if (!gic_ram_parity.power_domain[1].map)
		return -ENODEV;

	return ret;
}

static int gic_ram_parity_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	flush_work(&gic_ram_parity.work);

	return 0;
}

static const struct of_device_id gic_ram_parity_of_ids[] = {
	{ .compatible = "mediatek,gic-ram-parity", },
	{}
};

static struct platform_driver gic_ram_parity_drv = {
	.driver = {
		.name = "gic_ram_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = gic_ram_parity_of_ids,
	},
	.probe = gic_ram_parity_probe,
	.remove = gic_ram_parity_remove,
};

static int __init gic_ram_parity_init(void)
{
	int ret;

	ret = platform_driver_register(&gic_ram_parity_drv);
	if (ret)
		return ret;

	ret = driver_create_file(&gic_ram_parity_drv.driver,
				 &driver_attr_gic_ram_status);
	if (ret)
		return ret;

	return 0;
}

static __exit void gic_ram_parity_exit(void)
{
	driver_remove_file(&gic_ram_parity_drv.driver,
			 &driver_attr_gic_ram_status);

	platform_driver_unregister(&gic_ram_parity_drv);
}

module_init(gic_ram_parity_init);
module_exit(gic_ram_parity_exit);

MODULE_DESCRIPTION("MediaTek GIC RAM Parity Driver");
MODULE_LICENSE("GPL");
