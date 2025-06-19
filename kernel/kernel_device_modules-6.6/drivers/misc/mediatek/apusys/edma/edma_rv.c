// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/printk.h>
#include <linux/dev_printk.h>

#define inf_printf(fmt, args...)	pr_info("[edma][inf] " fmt, ##args)
#define dbg_printf(fmt, args...)	pr_info("[edma][dbg][%s] " fmt, __func__, ##args)
#define err_printf(fmt, args...)	pr_info("[edma][err][%s] " fmt, __func__, ##args)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#define edma_exception(key, format, args...)			\
do {								\
	pr_info(format, ##args);				\
	aee_kernel_exception("EDMA",				\
		"\nCRDISPATCH_KEY:" key "\n" format, ##args);	\
} while (0)
#else
#define edma_exception(key, format, args...)			\
	pr_info(format, ##args)
#endif

enum {
	/* tx */
	EDMA_TX_IPI_NONE = 0,
	EDMA_TX_IPI_LOGLV,
	/* rx */
	EDMA_RX_IPI_NONE = 0,
	EDMA_RX_IPI_UP_MSG,
};

struct apusys_core_info;

/**
 * type: command type
 * data: command data
 */
struct edma_ipi_data {
	u32 type;
	u32 data;
};

struct edma_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct mutex edma_ipi_mtx;
static struct edma_rpmsg_device edma_tx_rpm_dev;
static struct edma_rpmsg_device edma_rx_rpm_dev;

static struct kobject *edma_kobj;
static u32 loglv;


static void val_update(int type, u32 val)
{
	switch (type) {
	case EDMA_TX_IPI_LOGLV:
		loglv = val;
		break;
	default:
		/* do nothing */
		break;
	}
}

static int edma_ipi_send(int type, u32 val)
{
	static struct edma_ipi_data ipi_data;
	int ret;

	if (!edma_tx_rpm_dev.ept)
		return 0;

	mutex_lock(&edma_ipi_mtx);

	/* init */
	ipi_data.type = type;
	ipi_data.data = val;

	/* power on */
	ret = rpmsg_sendto(edma_tx_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		err_printf("rpmsg_sendto(power on) fail: %d\n", ret);
		goto exit;
	}

	ret = rpmsg_send(edma_tx_rpm_dev.ept, &ipi_data, sizeof(ipi_data));
	if (ret) {
		int res;

		err_printf("rpmsg_send fail: %d\n", ret);
		/* power off */
		res = rpmsg_sendto(edma_tx_rpm_dev.ept, NULL, 0, 1);
		if (res && res != -EOPNOTSUPP)
			err_printf("rpmsg_sendto(power off) fail: %d\n", res);
		goto exit;
	}

	ret = wait_for_completion_timeout(&edma_tx_rpm_dev.ack,
				msecs_to_jiffies(100));
	if (ret == 0) {
		dbg_printf("wait for completion timeout\n");
		ret = -EBUSY;
	} else {
		val_update(type, val);
		ret = 0;
	}
exit:
	mutex_unlock(&edma_ipi_mtx);

	return ret;
}

static int edma_tx_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	int ret;

	/* power off */
	ret = rpmsg_sendto(edma_tx_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		err_printf("rpmsg_sendto(power off) fail: %d\n", ret);

	complete(&edma_tx_rpm_dev.ack);

	return 0;
}

static int edma_tx_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n", __func__,
		rpdev->id.name, rpdev->src);

	edma_tx_rpm_dev.ept = rpdev->ept;
	edma_tx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void edma_tx_rpmsg_remove(struct rpmsg_device *rpdev)
{
}

static const struct of_device_id edma_tx_rpmsg_of_match[] = {
	{.compatible = "mediatek,apu-edma-rpmsg"},
	{}
};

static struct rpmsg_driver edma_tx_rpmsg_drv = {
	.drv = {
		.name = "apu-edma-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = edma_tx_rpmsg_of_match,
	},
	.probe = edma_tx_rpmsg_probe,
	.callback = edma_tx_rpmsg_cb,
	.remove = edma_tx_rpmsg_remove,
};

static void edma_ipi_up_msg(u32 data)
{
	edma_exception("APUSYS_EDMA", "EDMA exec fail: %d\n", data);
}

static int edma_rx_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct edma_ipi_data *d = (struct edma_ipi_data *)data;
	static u32 val;

	/* init */
	val = d->data;

	switch (d->type) {
	case EDMA_RX_IPI_UP_MSG:
		edma_ipi_up_msg(d->data);
		break;
	default:
		err_printf("unexptected type: %d\n", d->type);
		break;
	}

	rpmsg_send(edma_rx_rpm_dev.ept, &val, sizeof(val));

	return 0;
}

static int edma_rx_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n", __func__,
		rpdev->id.name, rpdev->src);

	edma_rx_rpm_dev.ept = rpdev->ept;
	edma_rx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void edma_rx_rpmsg_remove(struct rpmsg_device *rpdev)
{
}

static const struct of_device_id edma_rx_rpmsg_of_match[] = {
	{.compatible = "mediatek,edma-rx-rpmsg"},
	{}
};

static struct rpmsg_driver edma_rx_rpmsg_drv = {
	.drv = {
		.name = "edma-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = edma_rx_rpmsg_of_match,
	},
	.probe = edma_rx_rpmsg_probe,
	.callback = edma_rx_rpmsg_cb,
	.remove = edma_rx_rpmsg_remove,
};

static int edma_ipi_init(void)
{
	int ret;

	init_completion(&edma_tx_rpm_dev.ack);
	init_completion(&edma_rx_rpm_dev.ack);
	mutex_init(&edma_ipi_mtx);

	ret = register_rpmsg_driver(&edma_tx_rpmsg_drv);
	if (ret) {
		err_printf("register_rpmsg_driver tx fail: %d\n", ret);
		goto exit;
	}

	ret = register_rpmsg_driver(&edma_rx_rpmsg_drv);
	if (ret) {
		err_printf("register_rpmsg_driver rx fail: %d\n", ret);
		goto untx;
	}

	return 0;

untx:
	unregister_rpmsg_driver(&edma_tx_rpmsg_drv);
exit:
	return ret;
}

static void edma_ipi_deinit(void)
{
	unregister_rpmsg_driver(&edma_rx_rpmsg_drv);
	unregister_rpmsg_driver(&edma_tx_rpmsg_drv);
	mutex_destroy(&edma_ipi_mtx);
}

static ssize_t loglv_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", loglv);
}

static ssize_t loglv_store(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	edma_ipi_send(EDMA_TX_IPI_LOGLV, val);

	return size;
}

static struct kobj_attribute edma_loglv_attr =
	__ATTR(edma_rv_log_lv, 0640, loglv_show, loglv_store);

static int edma_sysfs_init(void)
{
	int ret;

	edma_kobj = kobject_create_and_add("edma_rv", kernel_kobj);
	if (!edma_kobj) {
		err_printf("kobject_create_and_add fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = sysfs_create_file(edma_kobj, &edma_loglv_attr.attr);
	if (ret) {
		err_printf("sysfs_create_file fail: %d\n", ret);
		goto kput;
	}

	return 0;

kput:
	kobject_put(edma_kobj);
exit:
	return ret;
}

static void edma_sysfs_exit(void)
{
	sysfs_remove_file(edma_kobj, &edma_loglv_attr.attr);
	kobject_put(edma_kobj);
}

int edma_rv_setup(struct apusys_core_info *info)
{
	int ret;

	ret = edma_ipi_init();
	if (ret) {
		err_printf("edma_ipi_init fail: %d\n", ret);
		goto exit;
	}

	ret = edma_sysfs_init();
	if (ret) {
		err_printf("edma_sysfs_init fail: %d\n", ret);
		goto ipid;
	}

	return 0;

ipid:
	edma_ipi_deinit();
exit:
	return ret;
}

void edma_rv_shutdown(void)
{
	edma_sysfs_exit();
	edma_ipi_deinit();
}

