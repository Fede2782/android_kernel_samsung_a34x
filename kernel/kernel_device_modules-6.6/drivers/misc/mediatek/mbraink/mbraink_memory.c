// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "mbraink_memory.h"
#include <mbraink_modules_ops_def.h>

static struct mbraink_memory_ops _mbraink_memory_ops;

int mbraink_memory_init(void)
{
	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;
	return 0;
}

int mbraink_memory_deinit(void)
{
	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;
	return 0;
}

int register_mbraink_memory_ops(struct mbraink_memory_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_memory_ops.getDdrInfo = ops->getDdrInfo;
	_mbraink_memory_ops.getMdvInfo = ops->getMdvInfo;
	_mbraink_memory_ops.get_ufs_info = ops->get_ufs_info;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_memory_ops);

int unregister_mbraink_memory_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;

	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_memory_ops);

int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	int ret = 0;

	if (pMemoryDdrInfo == NULL) {
		pr_info("%s: Ddr Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getDdrInfo)
		ret = _mbraink_memory_ops.getDdrInfo(pMemoryDdrInfo);
	else {
		pr_info("%s: Do not support ioctl getDdrInfo query.\n", __func__);
		pMemoryDdrInfo->totalDdrFreqNum = 0;
	}

	return ret;
}

int mbraink_memory_getMdvInfo(struct mbraink_memory_mdvInfo  *pMemoryMdv)
{
	int ret = 0;

	if (pMemoryMdv == NULL) {
		pr_info("%s: Mdv Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getMdvInfo)
		ret = _mbraink_memory_ops.getMdvInfo(pMemoryMdv);
	else
		pr_info("%s: Do not support ioctl getMdv query.\n", __func__);

	return ret;
}

int mbraink_get_ufs_info(struct mbraink_ufs_info *ufs_info)
{
	int ret = 0;

	if (_mbraink_memory_ops.get_ufs_info)
		ret = _mbraink_memory_ops.get_ufs_info(ufs_info);
	else
		pr_info("%s: Do not support ioctl get_ufs_info.\n", __func__);

	return ret;
}

