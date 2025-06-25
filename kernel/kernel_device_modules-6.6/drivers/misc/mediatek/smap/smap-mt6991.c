// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

enum SMAP_MODE {
	MODE_NORMAL,
	MODE_TEST1,
	MODE_TEST2,
};

struct mtk_smap {
	struct device *dev;
	void __iomem *regs;
	unsigned int mode;
	struct workqueue_struct *smap_wq;
	struct delayed_work defer_work;
	unsigned int delay_ms;
	bool def_disable;
};

static inline u32 smap_read(struct mtk_smap *dvfs, u32 offset)
{
	return readl(dvfs->regs + offset);
}

static inline void smap_write(struct mtk_smap *dvfs, u32 offset, u32 val)
{
	writel(val, dvfs->regs + offset);
}

#define smap_rmw(dvfs, offset, val, mask, shift) \
	smap_write(dvfs, offset, \
		(smap_read(dvfs, offset) & ~(mask << shift)) | (val << shift))

static void smap_init(struct mtk_smap *smap)
{
	smap_write(smap, 0x4CC, 0x3);
	smap_write(smap, 0x064, 0x67);
	smap_write(smap, 0x5A0, 0x67);
	if ((smap->mode == MODE_TEST1) || (smap->mode == MODE_TEST2))
		smap_write(smap, 0x08C, 0x28);
	else
		smap_write(smap, 0x08C, 0x72);

	smap_write(smap, 0x638, 0xC8);
	smap_write(smap, 0x448, 0xFFFF);
	smap_write(smap, 0x44C, 0xFFFF);
	smap_write(smap, 0x5A4, 0xFFFF);
	smap_write(smap, 0x5A8, 0xFFFF);
	smap_write(smap, 0x458, 0x1FFFF);
	smap_write(smap, 0x45C, 0x1FFFF);
	smap_write(smap, 0x5AC, 0x1FFFF);
	smap_write(smap, 0x5B0, 0x1FFFF);
	smap_write(smap, 0x4B4, 0x1FFFF);
	smap_write(smap, 0x4B8, 0x1FFFF);
	smap_write(smap, 0x600, 0x1FFFF);
	smap_write(smap, 0x604, 0x1FFFF);
	if ((smap->mode == MODE_TEST1) || (smap->mode == MODE_TEST2)){
		smap_write(smap, 0x464, 0x6);
		smap_write(smap, 0x5B4, 0x6);
	} else {
		smap_write(smap, 0x464, 0x2);
		smap_write(smap, 0x5B4, 0x2);
	}
	smap_write(smap, 0x5C4, 0x3);
	smap_write(smap, 0x5C8, 0x3);
	smap_write(smap, 0x5CC, 0x3);
	smap_write(smap, 0x5D0, 0x3);
	smap_write(smap, 0x5D4, 0x3);
	smap_write(smap, 0x5D8, 0x3);
	smap_write(smap, 0x5E8, 0x3);
	smap_write(smap, 0x5EC, 0x3);
	smap_write(smap, 0x5F0, 0x3);
	smap_write(smap, 0x5F4, 0x3);
	smap_write(smap, 0x5F8, 0x0);
	smap_write(smap, 0x5FC, 0x3);
	smap_write(smap, 0x468, 0xFADFFE33);
	smap_write(smap, 0x64C, 0x33333333);
	smap_write(smap, 0x650, 0x3);
	smap_write(smap, 0x658, 0x55555555);
	smap_write(smap, 0x65C, 0x5);
	smap_write(smap, 0x504, 0x88);
	smap_write(smap, 0x508, 0x88);
	smap_write(smap, 0x50C, 0x88);
	smap_write(smap, 0x510, 0x88);
	smap_write(smap, 0x514, 0x88);
	smap_write(smap, 0x69C, 0x88);
	smap_write(smap, 0x700, 0x88);
	smap_write(smap, 0x704, 0x88);
	smap_write(smap, 0x708, 0x88);
	smap_write(smap, 0x70C, 0x88);
	smap_write(smap, 0x748, 0x88);
	smap_write(smap, 0x74C, 0x88);
	smap_write(smap, 0x750, 0x88);
	smap_write(smap, 0x754, 0x88);
	smap_write(smap, 0x758, 0x88);
	smap_write(smap, 0x580, 0x1);
	smap_write(smap, 0x584, 0x1);
	smap_write(smap, 0x588, 0x1);
	smap_write(smap, 0x58C, 0x1);
	smap_write(smap, 0x63C, 0xFF | (0x1DD<<8));
	smap_write(smap, 0x644, 0x3B | (0x3C<<8));
	smap_write(smap, 0x6A4, 0x15 | (0x17<<8) | (0x1C<<16));
	smap_write(smap, 0x6A8, 0x0);
	smap_write(smap, 0x6AC, 0x23<<8);
	smap_write(smap, 0x51C, 0x0);
	smap_write(smap, 0x524, 0x1);
	if ((smap->mode == MODE_TEST1) || (smap->mode == MODE_TEST2))
		smap_write(smap, 0x528, 0x1);
	else
		smap_write(smap, 0x528, 0x0);

	if ((smap->mode == MODE_TEST1) || (smap->mode == MODE_TEST2))
		smap_write(smap, 0x47C, 0x0);
	else
		smap_write(smap, 0x47C, 0x40);

	smap_write(smap, 0x774,  0x2);

	if (!smap->def_disable)
		smap_write(smap, 0x4D0,  0x1);
}

static void smap_update_result(struct mtk_smap *smap)
{
	unsigned int val, val_2;
	u64 time;

	val = smap_read(smap, 0x478);
	if (val & 0x4) {
		time = local_clock();
		val_2 = smap_read(smap, 0x560);
		smap_write(smap, 0x560, val_2 + 1);
		val_2 = smap_read(smap, 0x564);
		smap_write(smap, 0x564, val_2 + 1);
		smap_write(smap, 0x56C, (unsigned int)(time & 0xFFFFFFFF));
		smap_write(smap, 0x568, (unsigned int)((time >> 32) & 0xFFFFFFFF));
		smap_write(smap, 0x4D4, 1);
		smap_write(smap, 0x4D4, 0);
	}
}

static void smap_periodic_work_handler(struct work_struct *work)
{
	struct mtk_smap *smap =
		container_of(work, struct mtk_smap, defer_work.work);

	smap_update_result(smap);

	queue_delayed_work(smap->smap_wq, &smap->defer_work,
		msecs_to_jiffies(smap->delay_ms));
}

static ssize_t smap_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_smap *smap = dev_get_drvdata(dev);
	int len = 0;

#define smap_print(...) \
	snprintf(buf + len, (4096 - len) > 0 ? (4096 - len) : 0, __VA_ARGS__)
	len += smap_print("TOTAL_CNT = %d\n", smap_read(smap, 0x560));
	len += smap_print("CNT = %d\n", smap_read(smap, 0x564));
	len += smap_print("SYSTIME_H = %x\n", smap_read(smap, 0x568));
	len += smap_print("SYSTIME_L = %x\n", smap_read(smap, 0x56C));
	len += smap_print("RSRV_4 = %d\n", smap_read(smap, 0x570));
	len += smap_print("RSRV_5 = %d\n", smap_read(smap, 0x574));
	len += smap_print("RSRV_6 = %d\n", smap_read(smap, 0x578));
	len += smap_print("RSRV_7 = %d\n", smap_read(smap, 0x57C));

	return (len > 4096) ? 4096 : len;
}
static DEVICE_ATTR_RO(smap_status);


static ssize_t smap_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	u32 val_1 = 0;
	int ret;

	struct mtk_smap *smap = dev_get_drvdata(dev);

	ret = sscanf(buf, "%19s %d", cmd, &val_1);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "clear")) {
		smap_write(smap, 0x564, 0);
		smap_write(smap, 0x568, 0);
		smap_write(smap, 0x56C, 0);
	} else if (!strcmp(cmd, "enable")) {
		if (val_1)
			smap_write(smap, 0x4D0, 0x1);
		else
			smap_write(smap, 0x4D0, 0x0);
	}else if (!strcmp(cmd, "mon_en")) {
		if (smap->smap_wq) {
			cancel_delayed_work_sync(&smap->defer_work);
			if (val_1) {
				queue_delayed_work(smap->smap_wq, &smap->defer_work,
					msecs_to_jiffies(smap->delay_ms));
			}
		}
	}
out:
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_WO(smap_dbg);

static struct attribute *smap_sysfs_attrs[] = {
	&dev_attr_smap_status.attr,
	&dev_attr_smap_dbg.attr,
	NULL,
};

static struct attribute_group smap_sysfs_attr_group = {
	.attrs = smap_sysfs_attrs,
};

int smap_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &smap_sysfs_attr_group);
	if (ret)
		return ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj,
		"smap");

	return ret;
}

static int smap_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct mtk_smap *smap;
	struct device_node *np = pdev->dev.of_node;
	u32 smap_mode = 0;
	u32 interval = 0;

	smap = devm_kzalloc(dev, sizeof(struct mtk_smap), GFP_KERNEL);
	if (!smap)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	smap->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(smap->regs))
		return PTR_ERR(smap->regs);

	smap->dev = dev;
	of_property_read_u32(np, "smap-mode", &smap_mode);
	smap->mode = smap_mode;
	smap->def_disable= of_property_read_bool(np, "smap-disable");
	smap_init(smap);

	platform_set_drvdata(pdev, smap);
	of_property_read_u32(np, "smap-mon-interval", &interval);
	if (interval < 500)
		smap->delay_ms = 1000 * 3;
	else
		smap->delay_ms = interval;

	smap->smap_wq = create_singlethread_workqueue("smap_wq");
	INIT_DELAYED_WORK(&smap->defer_work, smap_periodic_work_handler);
	if (of_property_read_bool(np, "smap-mon-enable")) {
		if (smap->smap_wq) {
			queue_delayed_work(smap->smap_wq, &smap->defer_work,
				msecs_to_jiffies(smap->delay_ms));
		}
	}
	smap_register_sysfs(dev);

	return 0;
}

static int smap_remove(struct platform_device *pdev)
{
	struct mtk_smap *smap = platform_get_drvdata(pdev);

	if (smap->smap_wq) {
		cancel_delayed_work_sync(&smap->defer_work);
		destroy_workqueue(smap->smap_wq);
	}

	return 0;
}

static const struct of_device_id smap_of_match[] = {
	{ .compatible = "mediatek,mt6991-smap", },
	{}
};

static struct platform_driver smap_pdrv = {
	.probe = smap_probe,
	.remove = smap_remove,
	.driver = {
		.name = "smap",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(smap_of_match),
	},
};

static int __init smap_module_init(void)
{
	return platform_driver_register(&smap_pdrv);
}
module_init(smap_module_init);

static void __exit smap_module_exit(void)
{
	platform_driver_unregister(&smap_pdrv);
}
module_exit(smap_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK SMAP driver");
