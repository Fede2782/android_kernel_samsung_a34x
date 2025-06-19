// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "ksym.h"
#include <mrdump_helper.h>

#include <debug_kinfo.h>
#include <asm/memory.h>

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

static unsigned long mkp_stext, mkp_etext, mkp_init_begin;

static bool init_debug_kinfo(void)
{
	if (mkp_stext != 0 && mkp_etext != 0 && mkp_init_begin != 0)
		return true;

	mkp_stext = aee_get_stext();
	mkp_etext = aee_get_etext();
	mkp_init_begin = aee_get_init_begin();

	if (mkp_stext != 0 && mkp_etext != 0 && mkp_init_begin != 0) {
		pr_info("mkp: %s success\n", __func__);
		return true;
	} else {
		pr_info("mkp: %s failed\n", __func__);
		return false;
	}
}

void mkp_get_krn_info(void **p_stext, void **p_etext,
	void **p__init_begin)
{
	bool done = init_debug_kinfo();

	if (!done)
		return;

	*p_stext = (void *)mkp_stext;
	*p_etext = (void *)mkp_etext;
	*p__init_begin = (void *)mkp_init_begin;

	MKP_DEBUG("_stext: %p, _etext: %p\n", *p_stext, *p_etext);
	MKP_DEBUG(" __init_begin: %p\n", *p__init_begin);
}

void mkp_get_krn_code(void **p_stext, void **p_etext)
{
	if (*p_stext && *p_etext)
		return;

	bool done = init_debug_kinfo();

	if (!done)
		return;

	*p_stext = (void *)mkp_stext;
	*p_etext = (void *)mkp_etext;

	if (!(*p_etext)) {
		MKP_ERR("%s: _stext not found\n", __func__);
		return;
	}
	if (!(*p_etext)) {
		MKP_ERR("%s: _etext not found\n", __func__);
		return;
	}
	MKP_DEBUG("_stext: %p, _etext: %p\n", *p_stext, *p_etext);
	return;
}

void mkp_get_krn_rodata(void **p_etext, void **p__init_begin)
{
	if (*p_etext && *p__init_begin)
		return;

	bool done = init_debug_kinfo();

	if (!done)
		return;

	*p_etext = (void *)mkp_etext;
	*p__init_begin = (void *)mkp_init_begin;

	if (!(*p_etext)) {
		MKP_ERR("%s: _etext not found\n", __func__);
		return;
	}
	if (!(*p__init_begin)) {
		MKP_ERR("%s: __init_begin not found\n", __func__);
		return;
	}
	MKP_DEBUG("_etext: %p, __init_begin: %p\n", *p_etext, *p__init_begin);
	return;
}
