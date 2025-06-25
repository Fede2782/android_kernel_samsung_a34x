/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * This header provides AUXADC IRQ Status for MT6375/MT6379 AUXADC
 *
 * Copyright (c) 2023 Mediatek Inc.
 * Author: ChiaEn Wu <chiaen_wu@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT637X_AUXADC_COMMON_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT637X_AUXADC_COMMON_H

#define RG_INT_STATUS_BAT_H		0
#define RG_INT_STATUS_BAT_L		1
#define RG_INT_STATUS_BAT2_H		2
#define RG_INT_STATUS_BAT2_L		3
#define RG_INT_STATUS_BAT_TEMP_H	4
#define RG_INT_STATUS_BAT_TEMP_L	5
#define RG_INT_STATUS_BAT_TEMP_PP_H	6
#define RG_INT_STATUS_BAT_TEMP_PP_L	7
#define RG_INT_STATUS_AUXADC_IMP	8
#define RG_INT_STATUS_NAG_C_DLTV	9

#endif
