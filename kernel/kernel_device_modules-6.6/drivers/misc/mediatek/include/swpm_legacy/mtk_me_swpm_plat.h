/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_ME_SWPM_PLAT_H__
#define __MTK_ME_SWPM_PLAT_H__

#if IS_ENABLED(CONFIG_MTK_SWPM_MT6877)
#include "subsys/mtk_me_swpm_mt6877.h"
#else
/* Use a default header for other projects */
/* Todo: Should refine in the future */
#include "subsys/mtk_me_swpm_default.h"
#endif

#endif

