// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>

#include "mvpu_driver.h"
#include "mvpu20_request.h"
#include "mvpu_plat.h"
#include "mvpu20_ipi.h"

/*
 * type0: Distinguish pwr_time, timeout, klog, or CX
 * type1: prepare to C1~C16
 * dir  : Distinguish read or write
 * data : store data
 */

struct mvpu_ipi_data {
	u32 type0;
	u16 dir;
	u64 data;
};
static struct mvpu_ipi_data ipi_tx_recv_buf;
static struct mvpu_ipi_data ipi_rx_send_buf;
static struct mutex mvpu_ipi_mtx;

struct mvpu_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct mvpu_rpmsg_device mvpu_tx_rpm_dev;
static struct mvpu_rpmsg_device mvpu_rx_rpm_dev;

int mvpu20_ipi_send(uint32_t type, uint32_t dir, uint64_t *val)
{
	struct mvpu_ipi_data ipi_data;
	int ret = 0, rpms_ret = 0;

	if (!mvpu_tx_rpm_dev.ept)
		return -1;

	ipi_data.type0 = type;
	ipi_data.dir = dir;
	ipi_data.data = *val;

	mutex_lock(&mvpu_ipi_mtx);

	/* power on */
	rpms_ret = rpmsg_sendto(mvpu_tx_rpm_dev.ept, NULL, 1, 0);
	if (rpms_ret && rpms_ret != -EOPNOTSUPP) {
		pr_info("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, rpms_ret);
		ret = -1;
		goto out;
	}

	rpms_ret = rpmsg_send(mvpu_tx_rpm_dev.ept, &ipi_data, sizeof(ipi_data));
	if (rpms_ret) {
		pr_info("%s: rpmsg_send fail(%d)\n", __func__, rpms_ret);
		ret = rpms_ret;

		rpms_ret = rpmsg_sendto(mvpu_tx_rpm_dev.ept, NULL, 0, 1);
		if (rpms_ret && rpms_ret != -EOPNOTSUPP)
			pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, rpms_ret);

		goto out;
	}

	ret = wait_for_completion_interruptible_timeout(
			&mvpu_tx_rpm_dev.ack, msecs_to_jiffies(1000));

	*val = ipi_tx_recv_buf.data;

	if (ret == 0) {
		pr_info("%s: timeout\n", __func__);
		ret = -1;
	} else {
		ret = 0;
	}

out:
	mutex_unlock(&mvpu_ipi_mtx);
	return ret;
}

static int mvpu20_rpmsg_tx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{

	struct mvpu_ipi_data *d = (struct mvpu_ipi_data *)data;
	int ret;

	ipi_tx_recv_buf.type0= d->type0;
	ipi_tx_recv_buf.dir= d->dir;
	ipi_tx_recv_buf.data= d->data;

	/* wait_for_completion_interruptible_timeout */
	complete(&mvpu_tx_rpm_dev.ack);

	/* power off */
	ret = rpmsg_sendto(mvpu_tx_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		pr_info("%s: rpmsg_sendto(power off) fail: %d\n", __func__, ret);

	return 0;
}

static int mvpu20_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct mvpu_ipi_data *d = (struct mvpu_ipi_data *)data;

	if (d->type0 == MVPU_IPI_MICROP_MSG) {

		ipi_rx_send_buf.type0  = d->type0;
		ipi_rx_send_buf.dir    = d->dir;
		ipi_rx_send_buf.data   = d->data;

		rpmsg_send(mvpu_rx_rpm_dev.ept, &ipi_rx_send_buf, sizeof(ipi_rx_send_buf));

		mvpu_aee_exception("MVPU", "MVPU aee");
	} else {
		pr_info("Receive command ack -> use the wrong channel!?\n");
	}

	return 0;
}

static int mvpu_rpmsg_tx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
		__func__, rpdev->id.name, rpdev->src);

	mvpu_tx_rpm_dev.ept = rpdev->ept;
	mvpu_tx_rpm_dev.rpdev = rpdev;

	return 0;
}

static int mvpu_rpmsg_rx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n",
		 __func__, rpdev->id.name, rpdev->src);

	mvpu_rx_rpm_dev.ept = rpdev->ept;
	mvpu_rx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void mvpu_rpmsg_remove(struct rpmsg_device *rpdev)
{
}


static const struct of_device_id mvpu_tx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mvpu-tx-rpmsg"},
	{},
};

static const struct of_device_id mvpu_rx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mvpu-rx-rpmsg"},
	{},
};

static struct rpmsg_driver mvpu20_rpmsg_tx_drv = {
	.drv = {
		.name = "mvpu-tx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mvpu_tx_rpmsg_of_match,
	},
	.probe = mvpu_rpmsg_tx_probe,
	.callback = mvpu20_rpmsg_tx_cb,
	.remove = mvpu_rpmsg_remove,
};

static struct rpmsg_driver mvpu20_rpmsg_rx_drv = {
	.drv = {
		.name = "mvpu-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mvpu_rx_rpmsg_of_match,
	},
	.probe = mvpu_rpmsg_rx_probe,
	.callback = mvpu20_rpmsg_rx_cb,
	.remove = mvpu_rpmsg_remove,
};

int mvpu20_ipi_init(void)
{
	int ret;

	pr_info("%s +\n", __func__);

	init_completion(&mvpu_rx_rpm_dev.ack);
	init_completion(&mvpu_tx_rpm_dev.ack);
	mutex_init(&mvpu_ipi_mtx);

	ret = register_rpmsg_driver(&mvpu20_rpmsg_rx_drv);
	if (ret)
		pr_info("failed to register mvpu rx rpmsg\n");

	ret = register_rpmsg_driver(&mvpu20_rpmsg_tx_drv);
	if (ret)
		pr_info("failed to register mvpu tx rpmsg\n");

	return 0;
}

void mvpu20_ipi_deinit(void)
{
	unregister_rpmsg_driver(&mvpu20_rpmsg_tx_drv);
	unregister_rpmsg_driver(&mvpu20_rpmsg_rx_drv);
	mutex_destroy(&mvpu_ipi_mtx);
}
