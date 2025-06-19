// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include "wla.h"

static struct wla_monitor *wla_mon;

static void __set_sig_sel(unsigned int sel_n, uint32_t val, uint32_t mask)
{
	switch (sel_n) {
	case 0:
		wla_write_raw(WLAPM_MON1, val, mask);
		break;
	case 1:
		wla_write_raw(WLAPM_MON1, val << 16, mask << 16);
		break;
	case 2:
		wla_write_raw(WLAPM_MON2, val << 16, mask << 16);
		break;
	}
}

static void __set_bit_sel(unsigned int sel_n, uint32_t val, uint32_t mask)
{
	switch (sel_n) {
	case 0:
		wla_write_raw(WLAPM_MON3, val, mask);
		break;
	case 1:
		wla_write_raw(WLAPM_MON3, val << 16, mask << 16);
		break;
	case 2:
		wla_write_raw(WLAPM_MON4, val, mask);
		break;
	case 3:
		wla_write_raw(WLAPM_MON4, val << 16, mask << 16);
		break;
	case 4:
		wla_write_raw(WLAPM_MON5, val << 16, mask << 16);
		break;
	}
}

static int __get_sig_sel(unsigned int sel_n, uint32_t *val, uint32_t mask)
{
	uint32_t val_n, val_s;
	int ret = WLA_SUCCESS;

	switch (sel_n) {
	case 0:
		val_n = wla_north_read_raw(WLAPM_MON1, mask);
		val_s = wla_south_read_raw(WLAPM_MON1, mask);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n;
		break;
	case 1:
		val_n = wla_north_read_raw(WLAPM_MON1, mask << 16);
		val_s = wla_south_read_raw(WLAPM_MON1, mask << 16);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n >> 16;
		break;
	case 2:
		val_n = wla_north_read_raw(WLAPM_MON2, mask << 16);
		val_s = wla_south_read_raw(WLAPM_MON2, mask << 16);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n >> 16;
		break;
	}
	return ret;
}

static int __get_bit_sel(unsigned int sel_n, uint32_t *val, uint32_t mask)
{
	uint32_t val_n, val_s;
	int ret = WLA_SUCCESS;

	switch (sel_n) {
	case 0:
		val_n = wla_north_read_raw(WLAPM_MON3, mask);
		val_s = wla_south_read_raw(WLAPM_MON3, mask);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n;
		break;
	case 1:
		val_n = wla_north_read_raw(WLAPM_MON3, mask << 16);
		val_s = wla_south_read_raw(WLAPM_MON3, mask << 16);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n >> 16;
		break;
	case 2:
		val_n = wla_north_read_raw(WLAPM_MON4, mask);
		val_s = wla_south_read_raw(WLAPM_MON4, mask);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n;
		break;
	case 3:
		val_n = wla_north_read_raw(WLAPM_MON4, mask << 16);
		val_s = wla_south_read_raw(WLAPM_MON4, mask << 16);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n >> 16;
		break;
	case 4:
		val_n = wla_north_read_raw(WLAPM_MON5, mask << 16);
		val_s = wla_south_read_raw(WLAPM_MON5, mask << 16);
		if (val_n != val_s)
			ret |= WLA_FAIL;
		else
			*val = val_n >> 16;
		break;
	}
	return ret;
}

void wla_mon_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set)
{
	uint32_t sel_value, mask;
	unsigned int sel_ch_start, sel_reg_n, shift;

	if (ch >= wla_mon->ch_hw_max || ch_set == NULL)
		return;

	sel_value = ch_set->sig_sel & 0x7;
	sel_ch_start = (ch * 3) % 16;
	sel_reg_n = (ch * 3) / 16;

	if (sel_ch_start > 13 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		__set_sig_sel(sel_reg_n, sel_value << sel_ch_start,
						mask << sel_ch_start);
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x7;
		__set_sig_sel(sel_reg_n, sel_value >> shift, mask >> shift);
	} else {
		__set_sig_sel(sel_reg_n, sel_value << sel_ch_start,
						0x7U << sel_ch_start);
	}

	sel_value = ch_set->bit_sel & 0x1F;
	sel_ch_start = (ch * 5) % 16;
	sel_reg_n = (ch * 5) / 16;

	if (sel_ch_start > 11 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		__set_bit_sel(sel_reg_n, sel_value << sel_ch_start,
						mask << sel_ch_start);
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x1f;
		__set_bit_sel(sel_reg_n, sel_value >> shift, mask >> shift);
	} else {
		__set_bit_sel(sel_reg_n, sel_value << sel_ch_start,
						0x1fU << sel_ch_start);
	}

	/* 0x0:rising, 0x1:falling, 0x2:high, 0x3:low */
	wla_write_raw(WLAPM_MON6, ch_set->trig_type << (ch * 2), 0x3U << (ch * 2));
}
EXPORT_SYMBOL(wla_mon_ch_sel);

int wla_mon_get_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set)
{
	unsigned int sel_ch_start, sel_reg_n, shift;
	uint32_t mask, val_n, val_s;
	uint32_t val_l = 0, val_h = 0;
	int ret = WLA_SUCCESS;

	if (ch >= wla_mon->ch_hw_max || ch_set == NULL)
		return WLA_FAIL;

	sel_ch_start = (ch * 3) % 16;
	sel_reg_n = (ch * 3) / 16;

	if (sel_ch_start > 13 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		ret |= __get_sig_sel(sel_reg_n, &val_l, mask << sel_ch_start);
		val_l = val_l >> sel_ch_start;
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x7;
		ret |= __get_sig_sel(sel_reg_n, &val_h,  mask >> shift);
		ch_set->sig_sel = (val_h << shift) | val_l;
	} else {
		ret |= __get_sig_sel(sel_reg_n, &ch_set->sig_sel, 0x7U << sel_ch_start);
		ch_set->sig_sel = ch_set->sig_sel >> sel_ch_start;
	}

	sel_ch_start = (ch * 5) % 16;
	sel_reg_n = (ch * 5) / 16;

	if (sel_ch_start > 11 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		ret |= __get_bit_sel(sel_reg_n, &val_l, mask << sel_ch_start);
		val_l = val_l >> sel_ch_start;
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x1f;
		ret |= __get_bit_sel(sel_reg_n, &val_h, mask >> shift);
		ch_set->bit_sel = (val_h << shift) | val_l;
	} else {
		ret |= __get_bit_sel(sel_reg_n, &ch_set->bit_sel, 0x1fU << sel_ch_start);
		ch_set->bit_sel = ch_set->bit_sel >> sel_ch_start;
	}

	/* 0x0:rising, 0x1:falling, 0x2:high, 0x3:low */
	val_n = wla_north_read_raw(WLAPM_MON6, 0x3U << (ch * 2));
	val_s = wla_south_read_raw(WLAPM_MON6, 0x3U << (ch * 2));
	if (val_n != val_s)
		ret |= WLA_FAIL;
	else
		ch_set->trig_type = val_n >> (ch * 2);

	return ret;
}
EXPORT_SYMBOL(wla_mon_get_ch_sel);

void wla_mon_sta_sel(unsigned int stat_n, struct wla_mon_status *sta)
{
	if (stat_n < 1 || stat_n > wla_mon->sta_hw_max || sta == NULL)
		return;

	wla_write_raw(WLAPM_DDREN_DBG, (sta->mux << ((stat_n - 1) * 6)) << 12,
					(0x3fU << ((stat_n - 1) * 6)) << 12);
}
EXPORT_SYMBOL(wla_mon_sta_sel);

int wla_mon_get_sta_sel(unsigned int stat_n, struct wla_mon_status *sta)
{
	uint32_t val_n, val_s;

	if (stat_n < 1 || stat_n > wla_mon->sta_hw_max || sta == NULL)
		return WLA_FAIL;

	val_n = wla_north_read_raw(WLAPM_DDREN_DBG,
								(0x3fU << ((stat_n - 1) * 6)) << 12);
	val_s = wla_south_read_raw(WLAPM_DDREN_DBG,
								(0x3fU << ((stat_n - 1) * 6)) << 12);

	if (val_n != val_s)
		return WLA_FAIL;

	sta->mux = (val_n >> ((stat_n - 1) * 6)) >> 12;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_sta_sel);

int wla_mon_start(unsigned int win_len_sec)
{
	uint32_t cnt;

	if (win_len_sec > 150)
		return WLA_FAIL;

	cnt = win_len_sec * 26000000;
	wla_write_field(WLAPM_MON0, 0x36, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, 0, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x0, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, cnt, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x3, GENMASK(31, 28));
	wla_write_field(WLAPM_MON0, 0x0, WLAPM_PMSR_IRQ_CLEAR);
	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_start);

void wla_mon_disable(void)
{
	wla_write_field(WLAPM_MON0, 0x36, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, 0, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x0, GENMASK(31, 24));
}
EXPORT_SYMBOL(wla_mon_disable);

unsigned int wla_mon_get_ch_hw_max(void)
{
	return wla_mon->ch_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_ch_hw_max);

unsigned int wla_mon_get_sta_hw_max(void)
{
	return wla_mon->sta_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_sta_hw_max);

int wla_mon_get_ch_cnt(unsigned int ch, uint32_t *north_cnt,
							uint32_t *south_cnt)
{
	if (ch >= wla_mon->ch_hw_max)
		return WLA_FAIL;

	if (wla_north_read_field(WLAPM_MON7, WLAPM_PMSR_IRQ_B) ||
			wla_south_read_field(WLAPM_MON7, WLAPM_PMSR_IRQ_B))
		return WLA_FAIL;

	if (north_cnt)
		*north_cnt = wla_north_read_field(WLAPM_MON9 + (ch * 4), GENMASK(31, 0));

	if (south_cnt)
		*south_cnt = wla_south_read_field(WLAPM_MON9 + (ch * 4), GENMASK(31, 0));

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_ch_cnt);

int wla_mon_init(struct wla_monitor *mon)
{
	unsigned int num;

	if (mon == NULL)
		return WLA_FAIL;

	wla_mon = mon;

	for (num = 0; num < mon->ch_num; num++)
		wla_mon_ch_sel(num, &mon->ch[num]);

	for (num = 1; num <= mon->sta_num; num++)
		wla_mon_sta_sel(num, &mon->status[num]);

	return WLA_SUCCESS;
}
