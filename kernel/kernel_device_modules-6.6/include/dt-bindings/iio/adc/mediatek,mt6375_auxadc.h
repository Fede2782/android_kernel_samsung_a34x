/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header provides macros for MT6375 AUXADC device bindings.
 *
 * Copyright (c) 2021 Mediatek Inc.
 * Author: ShuFan Lee <shufan_lee@richtek.com>
 *
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_AUXADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6375_AUXADC_H

#include "mediatek,mt637x_auxadc_common.h"

/* ADC channel idx. */
#define MT6375_AUXADC_BATSNS		0
#define MT6375_AUXADC_BATON		1
#define MT6375_AUXADC_IMP		2
#define MT6375_AUXADC_IMIX_R		3
#define MT6375_AUXADC_VREF		4
#define MT6375_AUXADC_BATSNS_DBG	5
#define MT6375_AUXADC_MAX_CHANNEL	6

#endif
