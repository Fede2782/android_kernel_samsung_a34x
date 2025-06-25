// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 * Authors:
 *	Po-Wen Kao <powen.kao@mediatek.com>
 */

#include <asm-generic/errno-base.h>
#include <linux/blkdev.h>
#include <linux/container_of.h>
#include <linux/dev_printk.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/stddef.h>
#include <linux/workqueue.h>

#include "ufs-mediatek-mbrain.h"
#include "ufs-mediatek.h"
#include "ufs/ufshcd.h"


static int ufs_mb_notify(struct ufs_hba *hba, struct ufs_mbrain_event *event)
{
	struct ufs_mtk_host *host;

	host = ufshcd_get_variant(hba);
	if (host && host->mb_notify)
		return host->mb_notify(event);

	return -EBADF;
}

int ufs_mb_get_info(struct platform_device *pdev, struct ufs_mbrain_dev_info *mb_dev_info)
{
	struct ufs_hba *hba;

	hba = dev_get_drvdata(&pdev->dev);
	if (!hba || !hba->ufs_device_wlun)
		return -EBADF;
	mb_dev_info->model = hba->ufs_device_wlun->model;
	mb_dev_info->rev = hba->ufs_device_wlun->rev;
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mb_get_info);

int ufs_mb_register(struct platform_device *pdev,  ufs_mb_event_notify notify)
{
	struct ufs_mtk_host *host;
	struct ufs_hba *hba;

	hba = dev_get_drvdata(&pdev->dev);
	host = ufshcd_get_variant(hba);

	if (host->mb_notify) {
		dev_info(hba->dev, "%s: already registered\n", __func__);
		return -EINVAL;
	}

	host->mb_notify = notify;
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mb_register);

int ufs_mb_unregister(struct platform_device *pdev)
{
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;

	hba = dev_get_drvdata(&pdev->dev);
	host = ufshcd_get_variant(hba);
	host->mb_notify = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mb_unregister);

static void ufs_mb_work(struct work_struct *work)
{
	struct ufs_hba *hba;
	struct ufs_mtk_host *host;
	struct ufs_mbrain_entry *entry = container_of(work, struct ufs_mbrain_entry, mb_work);
	struct ufs_mbrain_event mb_event;
	int ret;

	host = entry->host;
	hba = host->hba;

	mb_event.ver = UFS_MB_VER1;
	mb_event.data = &entry->data;

	/* Synchronous notification */
	ret = ufs_mb_notify(hba, &mb_event);
	if (ret)
		dev_info(hba->dev, "failed to notify mbrain(%d)", ret);

	dev_info(hba->dev, "UIC error(%d)=0x%x @ %llu",
		 entry->data.event,  entry->data.val, entry->data.mb_ts);
	dev_info(hba->dev, "Gear[%d, %d]",
		 entry->data.gear_rx, entry->data.gear_tx);
	entry->busy = false;
}

int ufs_mb_queue_error(struct ufs_hba *hba, struct ufs_mbrain_entry *entry)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	bool success = false;

	dev_info(hba->dev, "mb queue error");

	success = queue_work(host->mb_workq, &entry->mb_work);
	if (!success) {
		/* too many UIC error */
		dev_info(hba->dev, "work already in queue");
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mb_queue_error);

int ufs_mb_init(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mbrain_entry *entry;
	int i, j;

	for (i=0; i < ARRAY_SIZE(host->mb_entries); i++) {
		for (j=0; j < UFS_EVENT_HIST_LENGTH; j++) {
			entry = &host->mb_entries[i][j];
			INIT_WORK(&entry->mb_work, ufs_mb_work);
			entry->host = host;
		}
	}

	host->mb_workq = create_singlethread_workqueue("ufs_mtk_mb_wq");
	return 0;
}
EXPORT_SYMBOL_GPL(ufs_mb_init);
