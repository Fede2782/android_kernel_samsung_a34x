// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_HELPER_H__
#define __GPUEB_HELPER_H__

#ifndef GENMASK
#define GENMASK(h, l)       (((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#ifndef BIT
#define BIT(n)              (1U << (n))
#endif

#define GPUEB_TAG           "[GPU/EB]"
#define GHPM_TAG            "[GPU/GHPM]"
#define GHPM_SWWA_TAG       "[GPU/GHPM_SWWA]"

#define GPUEB_DEBUG_ENABLE  (0)

#define gpueb_log_e(tag, fmt, args...) \
	pr_err(tag"[ERROR][%s:%d]: "fmt"\n", __func__, __LINE__, ##args)
#define gpueb_log_i(tag, fmt, args...) \
	pr_info(tag"[INFO][%s:%d]: "fmt"\n", __func__, __LINE__, ##args)
#if GPUEB_DEBUG_ENABLE
#define gpueb_log_d(tag, fmt, args...) \
	pr_info(tag"[DEBUG][%s:%d]: "fmt"\n", __func__, __LINE__, ##args)
#else
#define gpueb_log_d(tag, fmt, args...)
#endif

#define gpueb_pr_logbuf(tag, buf, len, size, fmt, args...) \
	{ \
		pr_info(tag"@%s: "fmt"\n", __func__, ##args); \
		if (buf && len) \
			*len += snprintf(buf + *len, size - *len, fmt"\n", ##args); \
	}

#endif /* __GPUEB_HELPER_H__ */