/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MVPU20_IPI_H__
#define __MVPU20_IPI_H__

#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#define mvpu_aee_exception(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_exception("MVPU", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mvpu_aee_exception(key, format, args...)
#endif

int mvpu20_ipi_init(void);
void mvpu20_ipi_deinit(void);
int mvpu20_ipi_send(uint32_t type, uint32_t dir, uint64_t *val);

#endif /* __MVPU20_IPI_H__ */

