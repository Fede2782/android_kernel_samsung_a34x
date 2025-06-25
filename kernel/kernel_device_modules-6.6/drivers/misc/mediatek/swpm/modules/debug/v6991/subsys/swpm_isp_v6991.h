/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_ISP_V6991_H__
#define __SWPM_ISP_V6991_H__

#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <subsys/swpm_isp_wrapper.h>

#define MyTag "[swpm_isp]"
#define isp_log_basic(fmt, args...) pr_info(MyTag "[%s] " fmt "\n", __func__, ##args)
#define isp_log_detail(level, fmt, args...)                         \
	do {                                                            \
		if (level >= 1)                                             \
			pr_info(MyTag "[%s] " fmt "\n", __func__, ##args);  \
	} while (0)                                                     \

/* isp share memory data structure */
struct isp_swpm_data {
	struct ISP_P1 isp_p1_idx;
	struct ISP_P2 isp_p2_idx;
	struct CSI csi_idx;
};

/* swpm interface */
extern int swpm_isp_init(void);
extern void swpm_isp_exit(void);

void update_p1_idx(struct ISP_P1 *idx);
void update_p2_idx(struct ISP_P2 *idx);
void update_csi_idx(struct CSI *idx);


#endif // __SWPM_ISP_V6991_H__
