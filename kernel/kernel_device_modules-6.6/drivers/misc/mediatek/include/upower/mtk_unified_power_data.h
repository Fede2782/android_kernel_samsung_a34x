/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
#ifndef MTK_UNIFIED_POWER_DATA_H
#define MTK_UNIFIED_POWER_DATA_H

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_unified_power_data_6765.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6833)
#include "mtk_unified_power_data_6833.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6739)
#include "mtk_unified_power_data_6739.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include "mtk_unified_power_data_6761.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6877)
#include "mtk_unified_power_data_6877.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
#include "mtk_unified_power_data_6893.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6781)
#include "mtk_unified_power_data_6781.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6853)
#include "mtk_unified_power_data_6853.h"
#else
#include "mtk_unified_power_data_plat.h"
#endif

#endif /* UNIFIED_POWER_DATA_H */
