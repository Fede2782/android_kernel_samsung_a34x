/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
 #ifndef _FLT_UTILITY_API_H
#define _FLT_UTILITY_API_H

#define __FLT_TP(pfx, type)	pfx ## type
#define _FLT_TP(pfx, type)	__FLT_TP(pfx, type)
#define FLT_TP(pfx)		_FLT_TP(pfx, FLT_MODE_SEL)
#define FLT_FN(name)		FLT_TP(flt_ ## name ## _mode)

extern const struct flt_class *flt_class_mode;

#endif /* _FLT_UTILITY_API_H */
