/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef UFS_MEDIATEK_MBRAIN_H
#define UFS_MEDIATEK_MBRAIN_H

#include <linux/types.h>
#include <linux/platform_device.h>

#include "ufs/ufshcd.h"

enum ufs_mb_version {
	UFS_MB_VER1 = 1,
};

#define UFS_MB_PAYLOAD_LEN 64
#define SCSI_MODEL_LEN 16
#define SCSI_REV_LEN 4

struct ufs_mbrain_dev_info {
	/* len = SCSI_MODEL_LEN */
	const char *model;

	/* len = SCSI_REV_LEN */
	const char *rev;
};

struct ufs_mbrain_data {
	/* UFS_MB_VER1 */
	enum ufs_event_type event;
	uint64_t mb_ts;
	/* event specific value (reg / hba->errors / tag..) */
	uint32_t val;
	enum ufs_hs_gear_tag gear_rx;
	enum ufs_hs_gear_tag gear_tx;
};

struct ufs_mbrain_entry {
	struct ufs_mtk_host *host;
	struct work_struct mb_work;
	bool busy;

	/* event data */
	struct ufs_mbrain_data data;
};

/* Interface to pass data to mbrain */
struct ufs_mbrain_event {
	/* Update version whenever `struct ufs_mbrain_data` fields changes */
	enum ufs_mb_version ver;
	struct ufs_mbrain_data *data;
};

typedef int (*ufs_mb_event_notify)(struct ufs_mbrain_event *event);

/* APIs called from mbrain */
int ufs_mb_register(struct platform_device *pdev, ufs_mb_event_notify notify);
int ufs_mb_unregister(struct platform_device *pdev);
int ufs_mb_get_info(struct platform_device *pdev, struct ufs_mbrain_dev_info *mb_dev_info);


/* Call to queue error */
int ufs_mb_init(struct ufs_hba *hba);
int ufs_mb_queue_error(struct ufs_hba *hba, struct ufs_mbrain_entry *entry);

#endif /* UFS_MEDIATEK_MBRAIN_H */
