/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __ADSP_DBG_H__
#define __ADSP_DBG_H__

#include <linux/platform_device.h>
#include "adsp_platform_driver.h"

struct seq_file;

#define CMDFN(_cmd, _fn) {\
		.cmd = _cmd,\
		.fn = _fn,\
	}

struct cmd_fn {
	const char *cmd;
	void (*fn)(struct seq_file *s, void *);
};

void adsp_dbg_init(struct adspsys_priv *mt_adspsys);
#endif /* __ADSP_DBG_H__ */
