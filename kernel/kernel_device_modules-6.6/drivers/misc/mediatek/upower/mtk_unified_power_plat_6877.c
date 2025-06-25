// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

/* local include */
#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
#include "inc/mtk_cpufreq_api.h"
#endif
#include "mtk_unified_power.h"
#include "mtk_unified_power_data.h"
#if IS_ENABLED(CONFIG_MTK_EEM_READY)
#include "mtk_eem.h"
#endif
#include "mtk_static_power.h"

#ifndef EARLY_PORTING_SPOWER
#include "mtk_common_static_power.h"
#endif

#undef  BIT
#define BIT(bit)	(1U << (bit))

#define M6833T(range)	(1 ? range)
#define L6833T(range)	(0 ? range)
/**
 * Genearte a mask wher M6833T to L6833T are all 0b1
 * @r:	Range in the form of M6833T:L6833T
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - M6833T(r))) & ~((1U << L6833T(r)) - 1))

/**
 * Set value at M6833T:L6833T. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of M6833T:L6833T
 */
/* BITS(M6833T:L6833T, value) => Set value at M6833T:L6833T  */
#define BITS(r, val)	((val << L6833T(r)) & BITMASK(r))

#define GET_BITS_VAL(_bits_, _val_)   \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))
/* #if (NR_UPOWER_TBL_LIST <= 1) */
struct upower_tbl final_upower_tbl[NR_UPOWER_BANK] = {};
/* #endif */

int degree_set[NR_UPOWER_DEGREE] = {
		UPOWER_DEGREE_0,
		UPOWER_DEGREE_1,
		UPOWER_DEGREE_2,
		UPOWER_DEGREE_3,
		UPOWER_DEGREE_4,
		UPOWER_DEGREE_5,
};

/* collect all the raw tables */
#define INIT_UPOWER_TBL_INFOS(name, tbl) {__stringify(name), &tbl}
struct upower_tbl_info
	upower_tbl_infos_list[NR_UPOWER_TBL_LIST][NR_UPOWER_BANK] = {
	/* FY */
	[0] = {
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,
				upower_tbl_l_FY),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L,
				upower_tbl_b_FY),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,
				upower_tbl_cluster_l_FY),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L,
				upower_tbl_cluster_b_FY),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI,
				upower_tbl_cci_FY),
	},

	[1] = {
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,
				upower_tbl_l_B24G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L,
				upower_tbl_b_B24G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,
				upower_tbl_cluster_l_B24G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L,
				upower_tbl_cluster_b_B24G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI,
				upower_tbl_cci_B24G),
	},
	[2] = {
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,
				upower_tbl_l_B25G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L,
				upower_tbl_b_B25G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,
				upower_tbl_cluster_l_B25G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L,
				upower_tbl_cluster_b_B25G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI,
				upower_tbl_cci_B25G),
	},
	[3] = {
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,
				upower_tbl_l_B27G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L,
				upower_tbl_b_B27G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,
				upower_tbl_cluster_l_B27G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L,
				upower_tbl_cluster_b_B27G),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI,
				upower_tbl_cci_B27G),
	},

};
/* Upower will know how to apply voltage that comes from EEM */
unsigned char upower_recognize_by_eem[NR_UPOWER_BANK] = {
	UPOWER_BANK_LL, /* LL EEM apply voltage to LL upower bank */
	UPOWER_BANK_L, /* L EEM apply voltage to L upower bank */
	UPOWER_BANK_LL, /* LL EEM apply voltage to CLS_LL upower bank */
	UPOWER_BANK_L, /* L EEM apply voltage to CLS_L upower bank */
	UPOWER_BANK_CCI, /* CCI EEM apply voltage to CCI upower bank */
};

/* Used for rcu lock, points to all the raw tables list*/
struct upower_tbl_info *p_upower_tbl_infos = &upower_tbl_infos_list[0][0];

struct upower_doe upower_bank_name = {
	.dtsn = {
		"UPOWER_BANK_LL",
		"UPOWER_BANK_L",
		"UPOWER_BANK_CLS_LL",
		"UPOWER_BANK_CLS_L",
		"UPOWER_BANK_CCI"
	},
};

#ifndef EARLY_PORTING_SPOWER
int upower_bank_to_spower_bank(int upower_bank)
{
	int ret;

	switch (upower_bank) {
	case UPOWER_BANK_LL:
		ret = MTK_SPOWER_CPULL;
		break;
	case UPOWER_BANK_L:
		ret = MTK_SPOWER_CPUL;
		break;
	case UPOWER_BANK_CCI:
		ret = MTK_SPOWER_CCI;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
#endif

static unsigned int _mt_cpufreq_get_cpu_level_upower(void)
{

	unsigned int lv = 0;
	int val = 0;

	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
	struct device_node *dev_node;

	dev_node = of_find_node_by_name(NULL, "mt_cpufreq");
	if (!dev_node)
		pr_info("get mt_cpufreq node fail\n");

	efuse_cell = of_nvmem_cell_get(dev_node, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		pr_info("@%s: cannot get efuse_segment_cell\n", __func__);
		return PTR_ERR(efuse_cell);
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		pr_info("@%s: cannot get efuse_buf\n", __func__);
		return PTR_ERR(efuse_buf);
	}

	val = (*efuse_buf & 0xF); /* segment code */
	kfree(efuse_buf);

	switch (val) {
	case 4:
		lv = 3;
		break;
	case 3:
		lv = 0;
		break;
	case 2:
		lv = 2;
		break;
	default:
		lv = 1;
	}

	upower_info("CPU_LEVEL_%d, efuse_val = 0x%x\n", lv, val);
	return lv;
}

static void upower_scale_l_cap(void)
{
	unsigned int ratio;
	unsigned int temp;
	unsigned int max_cap = 1024;
	int i, j;
	struct upower_tbl *tbl;

	/* get L opp0's cap and calculate scaling ratio */
	/* ratio = round_up(1024 * 1000 / opp0 cap) */
	/* new cap = orig cap * ratio / 1000 */
	tbl = upower_tbl_infos[UPOWER_BANK_L].p_upower_tbl;
	temp = tbl->row[UPOWER_OPP_NUM - 1].cap;
	ratio = ((max_cap * 1000) + (temp - 1)) / temp;
	upower_error("scale ratio = %d, orig cap = %d\n", ratio, temp);

	/* if L opp0's cap is 1024, no need to scale cap anymore */
	if (temp == 1024)
		return;

	/* scaling L and cluster L cap value */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		if ((i == UPOWER_BANK_L) || (i == UPOWER_BANK_CLS_L)) {
			tbl = upower_tbl_infos[i].p_upower_tbl;
			for (j = 0; j < UPOWER_OPP_NUM; j++) {
				temp = tbl->row[j].cap;
				tbl->row[j].cap = temp * ratio / 1000;
			}
		}
	}

	/* check opp0's cap after scaling */
	tbl = upower_tbl_infos[UPOWER_BANK_L].p_upower_tbl;
	temp = tbl->row[UPOWER_OPP_NUM - 1].cap;
	if (temp != 1024) {
		upower_error("Notice: new cap is not 1024 after scaling(%d)\n",
				ratio);
		tbl->row[UPOWER_OPP_NUM - 1].cap = 1024;
	}
}

/****************************************************
 * According to chip version get the raw upower tbl *
 * and let upower_tbl_infos points to it.           *
 * Choose a non used upower tbl location and let    *
 * upower_tbl_ref points to it to store target      *
 * power tbl.                                       *
 ***************************************************/

int cpu_cluster_mapping(unsigned int cpu)
{
	enum upower_bank bank = UPOWER_BANK_LL;

	if (cpu < 6) /* cpu 0-3 */
		bank = UPOWER_BANK_LL;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_LL + 1;

	return bank;
}


void get_original_table(void)
{
	unsigned short idx = _mt_cpufreq_get_cpu_level_upower();
	int i, j;

	if (idx >= NR_UPOWER_TBL_LIST) {
		upower_info("%s():%d | idx=%d | upower table index out of range\n", __func__, __LINE__, idx);
		idx = 0; /* default use MT6771T_6785 */
	}


	/* get location of reference table */
	upower_tbl_infos = &upower_tbl_infos_list[idx][0];

	/* get location of target table */
/* #if (NR_UPOWER_TBL_LIST <= 1) */
	upower_tbl_ref = &final_upower_tbl[0];
/* #else
 *	upower_tbl_ref = upower_tbl_infos_list[(idx+1) %
 *	NR_UPOWER_TBL_LIST][0].p_upower_tbl;
 * #endif
 */

	upower_debug("idx %d dest:%p, src:%p\n",
			(idx+1)%NR_UPOWER_TBL_LIST,
			upower_tbl_ref, upower_tbl_infos);
	/* p_upower_tbl_infos = upower_tbl_infos; */

	/*
	 *  Clear volt fields before eem run.                                  *
	 *  If eem is enabled, it will apply volt into it. If eem is disabled, *
	 *  the values of volt are 0 , and upower will apply orig volt into it *
	 */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].volt = 0;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++)
		upower_debug("bank[%d] dest:%p dyn_pwr:%u, volt[0]%u\n",
				i, &upower_tbl_ref[i],
				upower_tbl_ref[i].row[0].dyn_pwr,
				upower_tbl_ref[i].row[0].volt);

	/* Not support L+ now, scale L and cluster L cap to 1024 */
	upower_scale_l_cap();
}

#ifdef SUPPORT_UPOWER_DCONFIG
void upower_by_doe(void)
{
	struct upower_doe *d = &upower_bank_name;
	struct device_node *node = NULL;
	struct upower_tbl *tbl;
	int ret, i, j, k;

	node = of_find_compatible_node(NULL, NULL, UPOWER_DT_NODE);
	if (!node) {
		upower_debug(" %s Can't find state node\n", __func__);
		return;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		ret = of_property_read_u32_array(node, d->dtsn[i],
			d->dts_opp_tbl[i], ARRAY_SIZE(d->dts_opp_tbl[i]));
		if (ret) {
			upower_debug("Can't find %s node\n", d->dtsn[i]);
		} else {
			tbl = upower_tbl_infos[i].p_upower_tbl;
			for (j = 0; j < UPOWER_OPP_NUM; j++) {
				tbl->row[j].cap = d->dts_opp_tbl[i][
					j*(NR_UPOWER_DEGREE + 3) + 0];
				tbl->row[j].volt = d->dts_opp_tbl[i][
					j*(NR_UPOWER_DEGREE + 3) + 1];
				tbl->row[j].dyn_pwr = d->dts_opp_tbl[i][
					j*(NR_UPOWER_DEGREE + 3) + 2];
				for (k = 0; k < NR_UPOWER_DEGREE; k++)
					tbl->row[j].lkg_pwr[k] = d->dts_opp_tbl[
						i][j * (NR_UPOWER_DEGREE + 3)
						+k+3];
				upower_debug("@@@ %d .cap[%d] = %u\n", i, j,
					     tbl->row[j].cap);
			}
			for (j = 0; j < NR_UPOWER_DEGREE; j++) {
				for (k = 0; k < NR_UPOWER_CSTATES; k++) {
					tbl->idle_states[j][k].power =
						d->dts_opp_tbl[i][UPOWER_OPP_NUM
						* (NR_UPOWER_DEGREE + 3) +
						j*NR_UPOWER_CSTATES+k];
					upower_debug(
	"@@@ %d .idle_states[%d][%d] = %u\n", i, j, k,
	tbl->idle_states[j][k].power);

				}
			}
		}
	}
}
#endif

MODULE_DESCRIPTION("MediaTek Unified Power Driver v0.0");
MODULE_LICENSE("GPL");
