// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_gpu.h"

static struct mbraink_gpu_ops _mbraink_gpu_ops;

int mbraink_gpu_init(void)
{
	_mbraink_gpu_ops.setFeatureEnable = NULL;
	_mbraink_gpu_ops.getTimeoutCounterReport = NULL;
	_mbraink_gpu_ops.getOppInfo = NULL;
	_mbraink_gpu_ops.getStateInfo = NULL;
	_mbraink_gpu_ops.getLoadingInfo = NULL;
	_mbraink_gpu_ops.setOpMode = NULL;
	return 0;
}


int mbraink_gpu_deinit(void)
{
	_mbraink_gpu_ops.setFeatureEnable = NULL;
	_mbraink_gpu_ops.getTimeoutCounterReport = NULL;
	_mbraink_gpu_ops.getOppInfo = NULL;
	_mbraink_gpu_ops.getStateInfo = NULL;
	_mbraink_gpu_ops.getLoadingInfo = NULL;
	_mbraink_gpu_ops.setOpMode = NULL;
	return 0;
}

int register_mbraink_gpu_ops(struct mbraink_gpu_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);
	_mbraink_gpu_ops.setFeatureEnable = ops->setFeatureEnable;
	_mbraink_gpu_ops.getTimeoutCounterReport = ops->getTimeoutCounterReport;
	_mbraink_gpu_ops.getOppInfo = ops->getOppInfo;
	_mbraink_gpu_ops.getStateInfo = ops->getStateInfo;
	_mbraink_gpu_ops.getLoadingInfo = ops->getLoadingInfo;
	_mbraink_gpu_ops.setOpMode = ops->setOpMode;
	return 0;
}
EXPORT_SYMBOL(register_mbraink_gpu_ops);

int unregister_mbraink_gpu_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_gpu_ops.setFeatureEnable = NULL;
	_mbraink_gpu_ops.getTimeoutCounterReport = NULL;
	_mbraink_gpu_ops.getOppInfo = NULL;
	_mbraink_gpu_ops.getStateInfo = NULL;
	_mbraink_gpu_ops.getLoadingInfo = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_gpu_ops);


int mbraink_gpu_featureEnable(bool bEnable)
{
	int ret = 0;

	if (_mbraink_gpu_ops.setFeatureEnable)
		ret = _mbraink_gpu_ops.setFeatureEnable(bEnable);
	else
		pr_info("%s: Do not support ioctl set gpu feature enable query.\n", __func__);

	return ret;
}

ssize_t getTimeoutCouterReport(char *pBuf)
{
	ssize_t retSize = 0;

	if (pBuf == NULL) {
		pr_info("%s: gpu buffer info is null.\n", __func__);
		return retSize;
	}

	if (_mbraink_gpu_ops.getTimeoutCounterReport)
		retSize = _mbraink_gpu_ops.getTimeoutCounterReport(pBuf);
	else
		pr_info("%s: Do not support counter report info query.\n", __func__);

	return retSize;
}

int mbraink_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo)
{
	int ret = 0;

	if (gOppInfo == NULL) {
		pr_info("%s: gpu opp info is null.\n", __func__);
		return ret;
	}

	if (_mbraink_gpu_ops.getOppInfo)
		ret = _mbraink_gpu_ops.getOppInfo(gOppInfo);
	else
		pr_info("%s: Do not support ioctl get gpu opp info query.\n", __func__);

	return ret;
}

int mbraink_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo)
{
	int ret = 0;

	if (gStateInfo == NULL) {
		pr_info("%s: gpu state info is null.\n", __func__);
		return ret;
	}

	if (_mbraink_gpu_ops.getStateInfo)
		ret = _mbraink_gpu_ops.getStateInfo(gStateInfo);
	else
		pr_info("%s: Do not support ioctl get gpu state info query.\n", __func__);

	return ret;
}

int mbraink_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo)
{
	int ret = 0;

	if (gLoadingInfo == NULL) {
		pr_info("%s: gpu loading info is null.\n", __func__);
		return ret;
	}

	if (_mbraink_gpu_ops.getLoadingInfo)
		ret = _mbraink_gpu_ops.getLoadingInfo(gLoadingInfo);
	else
		pr_info("%s: Do not support ioctl get gpu loading info query.\n", __func__);

	return ret;
}

void mbraink_gpu_setOpMode(int OpMode)
{
	if (_mbraink_gpu_ops.setOpMode)
		_mbraink_gpu_ops.setOpMode(OpMode);
	else
		pr_info("%s: Do not support ioctl set operation mode.\n", __func__);
}

