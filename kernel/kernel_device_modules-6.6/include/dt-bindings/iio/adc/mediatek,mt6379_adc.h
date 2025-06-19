/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * This header provides macros for MT6379 ADC device bindings.
 *
 * Copyright (c) 2023 Mediatek Inc.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6379_ADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6379_ADC_H

/* ADC channel idx. */
#define MT6379_ADC_VBATMON	0
#define MT6379_ADC_CHGVIN	1
#define MT6379_ADC_USBDP	2
#define MT6379_ADC_VSYS		3
#define MT6379_ADC_VBAT		4
#define MT6379_ADC_IBUS		5
#define MT6379_ADC_IBAT		6
#define MT6379_ADC_USBDM	7
#define MT6379_ADC_TEMPJC	8
#define MT6379_ADC_VREFTS	9
#define MT6379_ADC_TS		10
#define MT6379_ADC_PDVBUS	11
#define MT6379_ADC_CC1		12
#define MT6379_ADC_CC2		13
#define MT6379_ADC_SBU1		14
#define MT6379_ADC_SBU2		15
#define MT6379_ADC_WLSVIN_DIV10	16
#define MT6379_ADC_WLSIIN	17
#define MT6379_ADC_VBATMON2	18
#define MT6379_ADC_ZCV		19
#define MT6379_ADC_MAX_CHANNEL	20

#endif

