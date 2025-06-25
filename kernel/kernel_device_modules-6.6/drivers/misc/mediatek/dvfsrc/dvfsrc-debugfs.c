// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/seq_file.h>

static struct dentry *mtk_dvfsrc_dir;
static struct mtk_dvfsrc_dbgfs *dvfsrc_golbal;

struct dvfsrc_dbgfs_data {
	const int *regs;
};

struct mtk_dvfsrc_dbgfs {
	struct device *dev;
	void __iomem *dvfsrc_base;
	const struct dvfsrc_dbgfs_data *dvd;
};

enum dvfsrc_base_id {
	DVFSRC_FAKE_TEMP,
};

static u32 dvfsrc_read(u32 offset)
{
	return readl(dvfsrc_golbal->dvfsrc_base + dvfsrc_golbal->dvd->regs[offset]);
}

void dvfsrc_write(u32 offset, u32 val)
{
	writel(val, dvfsrc_golbal->dvfsrc_base + dvfsrc_golbal->dvd->regs[offset]);
}

#define dvfsrc_rmw(dvfs, offset, val, mask, shift) \
	dvfsrc_write(dvfs, offset, \
		(dvfsrc_read(dvfs, offset) & ~(mask << shift)) | (val << shift))

/*
 * Fake temperature
 */
static int dvfsrc_fake_temp_show(struct seq_file *m, void *unused)
{
	seq_printf(m, "dvfsrc_fake_temp = 0x%x\n", dvfsrc_read(DVFSRC_FAKE_TEMP));

	return 0;
}

static int dvfsrc_fake_temp_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfsrc_fake_temp_show, NULL);
}

static ssize_t dvfsrc_fake_temp_store(struct file *file, const char __user *user_buf, size_t cnt, loff_t *ppos)
{
	int ret,val = 0;
	char *buf;

	cnt = min_t(size_t, cnt, 255);

	buf = kzalloc(cnt + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, cnt)) {
		ret = -EFAULT;
		goto err;
	}
	buf[cnt] = '\0';

	ret = kstrtoint(buf, 10, &val);
	if (ret != 0) {
		ret = -EINVAL;
		goto err;
	}

	dvfsrc_write(DVFSRC_FAKE_TEMP, val);

	ret = cnt;

err:
	kfree(buf);

	return ret;
}

static const struct file_operations dvfsrc_fake_temp_ops = {
	.read = seq_read,
	.open = dvfsrc_fake_temp_open,
	.write = dvfsrc_fake_temp_store,
};

static int mtk_dvfsrc_debugfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dvfsrc_dbgfs *dvfsrc;
	struct platform_device *parent_dev;
	struct resource *res;
	int ret = 0;

	dvfsrc = devm_kzalloc(dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dev = &pdev->dev;

	parent_dev = to_platform_device(dev->parent);
	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "dvfsrc debug resource not found\n");
		return -ENODEV;
	}

	dvfsrc->dvfsrc_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(dvfsrc->dvfsrc_base))
		dvfsrc->dvfsrc_base = NULL;

	platform_set_drvdata(pdev, dvfsrc);

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);

	dvfsrc_golbal = dvfsrc;

	mtk_dvfsrc_dir = debugfs_create_dir("mtk_dvfsrc_debugfs", NULL);
	if (!mtk_dvfsrc_dir) {
		dev_info(dev, "fail to mkdir /sys/kernel/debug/mtk_dvfsrc_debugfs\n");
		return -ENOMEM;
	}

	if (!debugfs_create_file("dvfsrc_fake_temp_ops",
			0644, mtk_dvfsrc_dir, NULL, &dvfsrc_fake_temp_ops)) {
		dev_info(dev, "Unable to create dvfsrc_fake_temp_ops\n");
		debugfs_remove_recursive(mtk_dvfsrc_dir);
		return -ENOMEM;
	}

	return ret;
}


static int mtk_dvfsrc_debugfs_remove(struct platform_device *pdev)
{
	 debugfs_remove_recursive(mtk_dvfsrc_dir);
	return 0;
}

static const int mt6991_regs[] = {
	[DVFSRC_FAKE_TEMP]  = 0x314,
};

static const int mt6899_regs[] = {
	[DVFSRC_FAKE_TEMP]  = 0x28C,
};

static const struct dvfsrc_dbgfs_data mt6991_dvfsrc_debugfs_data = {
	.regs = mt6991_regs,
};

static const struct dvfsrc_dbgfs_data mt6899_dvfsrc_debugfs_data = {
	.regs = mt6899_regs,
};

static const struct of_device_id of_mtk_dvfsrc_dbgfs_match_tbl[] = {
	{
		.compatible = "mediatek,mt6991-dvfsrc-debugfs",
		.data = &mt6991_dvfsrc_debugfs_data,
	},
	{
		.compatible = "mediatek,mt6899-dvfsrc-debugfs",
		.data = &mt6899_dvfsrc_debugfs_data,
	},
	{}
};

static struct platform_driver dvfsrc_debugfs_drv = {
	.probe = mtk_dvfsrc_debugfs_probe,
	.remove	= mtk_dvfsrc_debugfs_remove,
	.driver = {
		.name = "mtk-dvfsrc-debugfs",
		.of_match_table = of_mtk_dvfsrc_dbgfs_match_tbl,
	},
};

int __init mtk_dvfsrc_debugfs_init(void)
{
	s32 status;

	status = platform_driver_register(&dvfsrc_debugfs_drv);
	if (status) {
		pr_notice("Failed to register dvfsrc debugfs driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

void __exit mtk_dvfsrc_debugfs_exit(void)
{
	platform_driver_unregister(&dvfsrc_debugfs_drv);
}
