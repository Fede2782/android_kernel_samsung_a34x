/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */
#ifndef _MTD_VERITY_H
#define _MTD_VERITY_H

extern int mtd_verity(int mtd_num);
extern int bdev_verity(dev_t rootfs_dev);
extern int get_rootfs_mtd_num(void);
extern int early_lookup_bdev(const char *name, dev_t *devt);

#endif
