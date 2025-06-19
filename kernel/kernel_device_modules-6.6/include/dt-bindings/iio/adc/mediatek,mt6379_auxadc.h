/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * This header provides macros for MT6379 AUXADC device bindings.
 *
 * Copyright (c) 2023 Mediatek Inc.
 * Author: ChiaEn Wu <chiaen_wu@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6379_AUXADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6379_AUXADC_H

#include "mediatek,mt637x_auxadc_common.h"

#define MT6379_AUXADC_BATSNS		0
#define MT6379_AUXADC_BATON		1
#define MT6379_AUXADC_IMP		2
#define MT6379_AUXADC_IMIX_R		3
#define MT6379_AUXADC_VREF		4
#define MT6379_AUXADC_BATSNS_DBG	5
#define MT6379_AUXADC_MAX_CHANNEL	6

#endif
