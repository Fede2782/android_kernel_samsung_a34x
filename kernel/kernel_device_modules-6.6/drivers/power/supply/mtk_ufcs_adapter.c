// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>

#include "adapter_class.h"
#include <ufcs_class.h>

struct mtk_ufcs_adapter_info {
	struct device *dev;
	struct ufcs_port *port;
	struct adapter_device *adapter;
	struct notifier_block ufcs_nb;
	atomic_t ufcs_type;
};

static int put_ufcs_dpm_reaction(struct ufcs_port *port, enum ufcs_dpm_request req,
				 union ufcs_dpm_input *in, union ufcs_dpm_output *out)
{
	int retry_cnt = 0, ret;

	do {
		ret = ufcs_port_dpm_reaction(port, req, in, out);
		if (ret != -EBUSY)
			break;

		msleep(200);
	} while (retry_cnt++ < 5);

	return ret;
}

static int ufcs_get_property(struct adapter_device *adap,  enum adapter_property prop)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);

	switch (prop) {
	case CAP_TYPE:
		return atomic_read(&info->ufcs_type);
	default:
		return -EINVAL;
	}

	return 0;
}

static int ufcs_adapter_get_status(struct adapter_device *adap, struct adapter_status *sta)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);
	struct device *dev = info->dev;
	union ufcs_dpm_output output;
	int ret;

	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_SRC_INFO, NULL, &output);
	if (ret) {
		dev_info(dev, "%s: Failed to get src info(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}

	sta->temperature = output.device_temp;

	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_ERROR_INFO, NULL, &output);
	if (ret) {
		dev_info(dev, "%s: Failed to get error info(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}

	sta->ocp = output.output_ocp;
	sta->otp = output.usb_conn_otp; //connector
	sta->ovp = output.output_ovp;

	dev_info(dev, "%s: temp(dev:%d,con:%d), ocp:%d, otp:%d, ovp:%d\n", __func__,
		 sta->temperature, output.conn_temp, sta->ocp, sta->otp, sta->ovp);

	return MTK_ADAPTER_OK;
}

static int ufcs_adapter_set_cap(struct adapter_device *adap, enum adapter_cap_type type,
				int mV, int mA)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);
	struct device *dev = info->dev;
	union ufcs_dpm_input input;
	union ufcs_dpm_output output;
	int ret;

	input.req_millivolt = mV;
	input.req_milliamp = mA;
	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_POWER_REQUEST, &input, &output);
	if (ret) {
		dev_info(dev, "%s: Failed to send power request(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}

	dev_info(dev, "%s: power request %d mV %d mA, ready:%d",  __func__, mV, mA,
		 output.power_request_ready);

	return MTK_ADAPTER_OK;
}

static int ufcs_adapter_get_cap(struct adapter_device *adap, enum adapter_cap_type type,
		      struct adapter_power_cap *cap)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);
	struct device *dev = info->dev;
	union ufcs_dpm_output output;
	int i, ret;

	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_SRC_CAP, NULL, &output);
	if (ret) {
		dev_info(dev, "%s: Failed to get src cap(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}

	cap->nr = output.src_cap_cnt;
	for (i = 0; i < cap->nr; i++) {
		cap->min_mv[i] = output.src_cap[i].min_mV;
		cap->max_mv[i] = output.src_cap[i].max_mV;
		cap->ma[i] = output.src_cap[i].max_mA;
		cap->type[i] = MTK_UFCS;
		dev_info(dev, "%s: cap_idx[%d], %d ~ %d mV, %d mA\n", __func__,
			 i, cap->min_mv[i], cap->max_mv[i], cap->ma[i]);
	}

	return MTK_ADAPTER_OK;
}

static int ufcs_adapter_get_output(struct adapter_device *adap, int *mV, int *mA)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);
	struct device *dev = info->dev;
	union ufcs_dpm_output output;
	int ret;

	if (!mV || !mA)
		return -EINVAL;

	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_SRC_INFO, NULL, &output);
	if (ret) {
		dev_info(dev, "%s: Failed to get src info(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}

	*mV = output.output_millivolt;
	*mA = output.output_milliamp;

	dev_info(dev, "%s: %d mV %d mA\n", __func__, *mV, *mA);
	return MTK_ADAPTER_OK;
}

static int ufcs_adapter_authentication(struct adapter_device *dev,
			     struct adapter_auth_data *data)
{
	/* TODO: */
	return MTK_ADAPTER_NOT_SUPPORT;
}

static int ufcs_adapter_exit_mode(struct adapter_device *adap)
{
	struct mtk_ufcs_adapter_info *info = adapter_dev_get_drvdata(adap);
	struct device *dev = info->dev;
	int ret;

	ret = put_ufcs_dpm_reaction(info->port, UFCS_DPM_EXIT_UFCS_MODE, NULL, NULL);
	if (ret) {
		dev_info(dev, "%s: Failed to exit mode(%d)\n", __func__, ret);
		return ret == -ETIMEDOUT ? MTK_ADAPTER_TIMEOUT : MTK_ADAPTER_ERROR;
	}
	return MTK_ADAPTER_OK;
}

static struct adapter_ops adapter_ops = {
	.get_property = ufcs_get_property,
	.get_status = ufcs_adapter_get_status,
	.set_cap = ufcs_adapter_set_cap,
	.get_cap = ufcs_adapter_get_cap,
	.get_output = ufcs_adapter_get_output,
	.authentication = ufcs_adapter_authentication,
	.exit_mode = ufcs_adapter_exit_mode,
};

static int ufcs_notifier_call(struct notifier_block *nb, unsigned long event,
			      void *data)
{
	struct mtk_ufcs_adapter_info *info =
		container_of(nb, struct mtk_ufcs_adapter_info, ufcs_nb);
	struct adapter_device *adapter = info->adapter;
	int ufcs_type, evt;

	dev_info(info->dev, "%s: evt:%lu (2:PASS, 1:FAIL, 0: DETACH/PORT RESET)\n",
		 __func__, event);

	switch (event) {
	case UFCS_NOTIFY_ATTACH_PASS:
		evt = TA_ATTACH;
		ufcs_type = MTK_UFCS;
		break;
	case UFCS_NOTIFY_ATTACH_FAIL:
		evt = TA_DETECT_FAIL;
		ufcs_type = MTK_CAP_TYPE_UNKNOWN;
		break;
	case UFCS_NOTIFY_ATTACH_NONE:
		evt = TA_DETACH;
		ufcs_type = MTK_CAP_TYPE_UNKNOWN;
		break;
	default:
		return NOTIFY_DONE;
	}

	atomic_set(&info->ufcs_type, ufcs_type);

	/* Notify the adapter user */
	srcu_notifier_call_chain(&adapter->evt_nh, evt, NULL);

	return NOTIFY_DONE;
}

static int mtk_ufcs_adapter_probe(struct platform_device *pdev)
{
	struct mtk_ufcs_adapter_info *info;
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(&pdev->dev, "%s, probe++\n", __func__);
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	info->port = ufcs_port_get_by_name("port.0");
	if (!info->port) {
		dev_notice(dev, "%s: Failed to get ufcs port: port.0\n",
			   __func__);
		return -ENODEV;
	}

	info->ufcs_nb.notifier_call = ufcs_notifier_call;
	ret = register_ufcs_dev_notifier(info->port, &info->ufcs_nb);
	if (ret) {
		dev_notice(dev, "%s: failed to register ufcs notify(%d)\n",
			   __func__, ret);
		return ret;
	}

	info->adapter = adapter_device_register("ufcs_adapter", dev, info,
						&adapter_ops, NULL);
	if (IS_ERR(info->adapter)) {
		ret = PTR_ERR(info->adapter);
		dev_notice(dev, "%s get ufcs_adapter fail(%d)\n", __func__,
			   ret);
		return ret;
	}

	adapter_dev_set_drvdata(info->adapter, info);

	dev_info(dev, "%s: successfully\n", __func__);
	return 0;
}

static const struct of_device_id mtk_ufcs_adapter_of_match[] = {
	{.compatible = "mediatek,ufcs-adapter" },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ufcs_adapter_of_match);

static struct platform_driver mtk_ufcs_adapter_driver = {
	.probe = mtk_ufcs_adapter_probe,
	.driver = {
		   .name = "ufcs-adapter",
		   .of_match_table = mtk_ufcs_adapter_of_match,
	},
};
module_platform_driver(mtk_ufcs_adapter_driver);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MTK UFCS Adapter Driver");
MODULE_LICENSE("GPL");
