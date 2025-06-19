// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/spinlock.h>

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
#include <swpm_module_psp.h>
#endif
#include "mbraink_sys_res.h"
#include "mbraink_sys_res_plat.h"
#include "mbraink_sys_res_mbrain_dbg.h"

static struct mbraink_sys_res_record sys_res_record[MBRAINK_SYS_RES_SCENE_NUM];
static spinlock_t sys_res_lock;
static int common_log_enable;
static unsigned int sys_res_last_buffer_index;
static unsigned int sys_res_temp_buffer_index;
static unsigned int sys_res_last_suspend_diff_buffer_index;
static unsigned int sys_res_last_diff_buffer_index;

struct mbraink_sys_res_group_info sys_res_group_info[NR_SPM_GRP] = {
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
	{0, 0,  0, 20},
};

static struct mbriank_sys_res_mapping sys_res_mapping[] = {
	{0, "md"},
	{0, "conn"},
	{0, "scp"},
	{0, "adsp"},
	{0, "pcie"},
	{0, "uarthub"},
};

static void get_sys_res_group_info(struct mbraink_sys_res_group_info *grouplist_info)
{
#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

	int ret = 0;
	struct mbraink_sys_res_group_info *group_info = NULL;

	unsigned int out1, out2, out3;

	group_info = &grouplist_info[DDREN_REQ];
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_DDREN, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[APSRC_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_APSRC, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[EMI_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_EMI, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[MAINPLL_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_MAINPLL, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[INFRA_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_INFRA,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[F26M_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_26M,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[PMIC_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_PMIC,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[VCORE_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_VCORE,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[RC_REQ]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_RC_REQ, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[PLL_EN]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_PLL_EN, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[PWR_OFF]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_PWR_OFF, &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[PWR_ACT]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_PWR_ACT,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	group_info = &(grouplist_info[SYS_STA]);
	ret = get_res_group_info(SWPM_PSP_MAIN_RES_SYS_STA,  &(group_info->sys_index),
		&(group_info->sig_table_index), &(group_info->group_num));
	if (ret)
		pr_info("get_res_group_info fail (%d)\n", ret);

	out1 = 0;
	out2 = 0;
	out3 = 0;
	ret = get_res_group_id(
			SWPM_PSP_SEL_SIG_VCORE_MD,
			SWPM_PSP_SEL_SIG_VCORE_CONN,
			SWPM_PSP_SEL_SIG_VCORE_SCP,
			&out1, &out2, &out3);
	sys_res_mapping[0].id = out1;
	sys_res_mapping[1].id = out2;
	sys_res_mapping[2].id = out3;

	if (ret)
		pr_info("get_res_group_id fail (%d)\n", ret);

	out1 = 0;
	out2 = 0;
	out3 = 0;
	ret = get_res_group_id(
			SWPM_PSP_SEL_SIG_VCORE_ADSP,
			SWPM_PSP_SEL_SIG_VCORE_PCIE0,
			SWPM_PSP_SEL_SIG_VCORE_PCIE1,
			&out1, &out2, &out3);
	sys_res_mapping[3].id = out1;
	sys_res_mapping[4].id = out2;

	if (ret)
		pr_info("get_res_group_id fail (%d)\n", ret);

	ret = get_res_group_id(
			SWPM_PSP_SEL_SIG_VCORE_MMPROC,
			SWPM_PSP_SEL_SIG_VCORE_UARTHUB,
			SWPM_PSP_SEL_SIG_UNUSE,
			&out1, &out2, &out3);
	sys_res_mapping[5].id = out2;
	if (ret)
		pr_info("get_res_group_id fail (%d)\n", ret);

#else
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
	return;
#endif
}

static int mbraink_sys_res_alloc(struct mbraink_sys_res_record *record)
{
#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

	struct res_sig_stats *spm_res_sig_stats_ptr;

	if (!record)
		return -1;

	spm_res_sig_stats_ptr =
	kcalloc(1, sizeof(struct res_sig_stats), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr)
		goto RES_SIG_ALLOC_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);

	spm_res_sig_stats_ptr->res_sig_tbl =
	kcalloc(spm_res_sig_stats_ptr->res_sig_num,
			sizeof(struct res_sig), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr->res_sig_tbl)
		goto RES_SIG_ALLOC_TABLE_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = spm_res_sig_stats_ptr;

	return 0;

RES_SIG_ALLOC_TABLE_ERROR:
	kfree(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = NULL;
RES_SIG_ALLOC_ERROR:
	return -1;
#else
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
	return -1;
#endif

}

static void mbraink_sys_res_free(struct mbraink_sys_res_record *record)
{
	if (record && record->spm_res_sig_stats_ptr) {
		kfree(record->spm_res_sig_stats_ptr->res_sig_tbl);
		kfree(record->spm_res_sig_stats_ptr);
		record->spm_res_sig_stats_ptr = NULL;
	}
}


static int __sync_lastest_mbraink_sys_res_record(struct mbraink_sys_res_record *record)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
	if (!record ||
	    !record->spm_res_sig_stats_ptr)
		return ret;

	ret = sync_latest_data();
	get_res_sig_stats(record->spm_res_sig_stats_ptr);
#endif
	return ret;
}
static void __mbraink_sys_res_record_diff(struct mbraink_sys_res_record *result,
				   struct mbraink_sys_res_record *prev,
				   struct mbraink_sys_res_record *cur)
{
	int i;

	if (!result || !prev || !cur)
		return;

	if (result->spm_res_sig_stats_ptr == NULL ||
		result->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return;

	if (prev->spm_res_sig_stats_ptr == NULL ||
		prev->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return;

	if (cur->spm_res_sig_stats_ptr == NULL ||
		cur->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return;

	result->spm_res_sig_stats_ptr->suspend_time =
		prev->spm_res_sig_stats_ptr->suspend_time -
		cur->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time =
		prev->spm_res_sig_stats_ptr->duration_time -
		cur->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time =
		prev->spm_res_sig_stats_ptr->res_sig_tbl[i].time -
		cur->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}

}

static void __mbraink_sys_res_record_add(struct mbraink_sys_res_record *result,
				   struct mbraink_sys_res_record *delta)
{
	int i;

	if (!result || !delta)
		return;

	if (result->spm_res_sig_stats_ptr == NULL ||
		result->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return;

	if (delta->spm_res_sig_stats_ptr == NULL ||
		delta->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return;

	result->spm_res_sig_stats_ptr->suspend_time +=
		delta->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time +=
		delta->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time +=
		delta->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}
}

static int update_mbraink_sys_res_record(void)
{
	unsigned int temp;
	unsigned long flag;
	int ret;

	spin_lock_irqsave(&sys_res_lock, flag);

	if (sys_res_temp_buffer_index >= MBRAINK_SYS_RES_SCENE_NUM ||
		sys_res_last_diff_buffer_index >= MBRAINK_SYS_RES_SCENE_NUM ||
		sys_res_last_buffer_index >= MBRAINK_SYS_RES_SCENE_NUM) {
		spin_unlock_irqrestore(&sys_res_lock, flag);
		return -1;
	}

	ret = __sync_lastest_mbraink_sys_res_record(&sys_res_record[sys_res_temp_buffer_index]);

	__mbraink_sys_res_record_diff(&sys_res_record[sys_res_last_diff_buffer_index],
			&sys_res_record[sys_res_temp_buffer_index],
			&sys_res_record[sys_res_last_buffer_index]);

	if (sys_res_record[sys_res_last_diff_buffer_index].spm_res_sig_stats_ptr != NULL &&
		sys_res_record[sys_res_last_diff_buffer_index].spm_res_sig_stats_ptr->suspend_time
		> 0) {

		__mbraink_sys_res_record_add(&sys_res_record[MBRAINK_SYS_RES_SCENE_SUSPEND],
				      &sys_res_record[sys_res_last_diff_buffer_index]);

		temp = sys_res_last_diff_buffer_index;
		sys_res_last_diff_buffer_index = sys_res_last_suspend_diff_buffer_index;
		sys_res_last_suspend_diff_buffer_index = temp;

	} else {

		__mbraink_sys_res_record_add(&sys_res_record[MBRAINK_SYS_RES_SCENE_COMMON],
				      &sys_res_record[sys_res_last_diff_buffer_index]);
	}

	temp = sys_res_temp_buffer_index;
	sys_res_temp_buffer_index = sys_res_last_buffer_index;
	sys_res_last_buffer_index = temp;
	spin_unlock_irqrestore(&sys_res_lock, flag);

	return ret;
}

static struct mbraink_sys_res_record *get_mbraink_sys_res_record(unsigned int scene)
{
	if (scene >= MBRAINK_SYS_RES_GET_SCENE_NUM)
		return NULL;

	switch (scene) {
	case MBRAINK_SYS_RES_COMMON:
		return &sys_res_record[MBRAINK_SYS_RES_SCENE_COMMON];
	case MBRAINK_SYS_RES_SUSPEND:
		return &sys_res_record[MBRAINK_SYS_RES_SCENE_SUSPEND];
	case MBRAINK_SYS_RES_LAST_SUSPEND:
		if (sys_res_last_suspend_diff_buffer_index >= MBRAINK_SYS_RES_SCENE_NUM)
			return NULL;
		return &sys_res_record[sys_res_last_suspend_diff_buffer_index];
	case MBRAINK_SYS_RES_LAST:
		if (sys_res_last_buffer_index >= MBRAINK_SYS_RES_SCENE_NUM)
			return NULL;
		return &sys_res_record[sys_res_last_buffer_index];
	}
	return &sys_res_record[scene];
}

static uint64_t mbraink_sys_res_get_detail(struct mbraink_sys_res_record *record,
	int op, unsigned int val)
{
	uint64_t ret = 0;
	uint64_t total_time = 0, sig_time = 0;

	if (!record)
		return 0;

	if (record->spm_res_sig_stats_ptr == NULL)
		return 0;

	switch (op) {
	case MBRAINK_SYS_RES_DURATION:
		ret = record->spm_res_sig_stats_ptr->duration_time;
		break;
	case MBRAINK_SYS_RES_SUSPEND_TIME:
		ret = record->spm_res_sig_stats_ptr->suspend_time;
		break;
	case MBRAINK_SYS_RES_SIG_TIME:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		break;
	case MBRAINK_SYS_RES_SIG_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].sig_id;
		break;
	case MBRAINK_SYS_RES_SIG_GROUP_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].grp_id;
		break;
	case MBRAINK_SYS_RES_SIG_OVERALL_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->duration_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case MBRAINK_SYS_RES_SIG_SUSPEND_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->suspend_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case MBRAINK_SYS_RES_SIG_ADDR:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		if (record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return 0;
		ret = (uint64_t)(&record->spm_res_sig_stats_ptr->res_sig_tbl[val]);
		break;
	default:
		break;
	};
	return ret;
}

static unsigned int  mbraink_sys_res_get_threshold(void)
{
	return sys_res_group_info[0].threshold;
}

static void mbraink_sys_res_set_threshold(unsigned int val)
{
	int i;

	if (val > 100)
		val = 100;

	for (i = 0; i < NR_SPM_GRP; i++)
		sys_res_group_info[i].threshold = val;
}

static void mbraink_sys_res_enable_common_log(int en)
{
	common_log_enable = (en) ? 1 : 0;
}

static int mbraink_sys_res_get_log_enable(void)
{
	return common_log_enable;
}

static void mbraink_sys_res_log(unsigned int scene)
{
	#define LOG_BUF_OUT_SZ		(768)

	unsigned long flag;
	struct mbraink_sys_res_record *sys_res_record;
	uint64_t time, sys_index, sig_tbl_index;
	uint64_t threshold, ratio;
	int time_type, ratio_type;
	char scene_name[15];
	char sys_res_log_buf[LOG_BUF_OUT_SZ] = { 0 };
	int i, j = 0, sys_res_log_size = 0, sys_res_update = 0;

	if (scene == MBRAINK_SYS_RES_LAST &&
	    !common_log_enable)
		return;

	sys_res_update = update_mbraink_sys_res_record();

	if (sys_res_update) {
		pr_info("[Mbraink][SPM] SWPM data is invalid\n");
		return;
	}

	spin_lock_irqsave(&sys_res_lock, flag);

	sys_res_record = get_mbraink_sys_res_record(scene);
	if (scene == MBRAINK_SYS_RES_LAST_SUSPEND) {
		time_type = MBRAINK_SYS_RES_SUSPEND_TIME;
		strscpy(scene_name, "suspend", 10);
		ratio_type = MBRAINK_SYS_RES_SIG_SUSPEND_RATIO;
	} else if (scene == MBRAINK_SYS_RES_LAST) {
		time_type = MBRAINK_SYS_RES_DURATION;
		strscpy(scene_name, "common", 10);
		ratio_type = MBRAINK_SYS_RES_SIG_OVERALL_RATIO;
	} else {
		spin_unlock_irqrestore(&sys_res_lock, flag);
		return;
	}

	time = mbraink_sys_res_get_detail(sys_res_record, time_type, 0);
	sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"[Mbraink][SPM] %s %llu ms; ", scene_name, time);

	for (i = 0; i < MBRAINK_SYS_RES_SYS_RESOURCE_NUM; i++) {
		sys_index = sys_res_group_info[i].sys_index;
		sig_tbl_index = sys_res_group_info[i].sig_table_index;
		threshold = sys_res_group_info[i].threshold;
		ratio = mbraink_sys_res_get_detail(sys_res_record,
					       ratio_type,
					       sys_index);

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"group %d ratio %llu", i, ratio);

		if (ratio < threshold) {
			sys_res_log_size += scnprintf(
					sys_res_log_buf + sys_res_log_size,
					LOG_BUF_OUT_SZ - sys_res_log_size,
					"; ");
			continue;
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				" (> %llu) [", threshold);

		for (j = 0; j < sys_res_group_info[i].group_num; j++) {
			ratio = mbraink_sys_res_get_detail(sys_res_record,
						       ratio_type,
						       j + sig_tbl_index);

			if (ratio < threshold)
				continue;

			if (sys_res_log_size > LOG_BUF_OUT_SZ - 45) {
				pr_info("[Mbraink][SPM] %s", sys_res_log_buf);
				sys_res_log_size = 0;
			}

			sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"0x%llx(%llu%%),",
				mbraink_sys_res_get_detail(sys_res_record,
					MBRAINK_SYS_RES_SIG_ID, j + sig_tbl_index),
				ratio);
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"]; ");
	}
	pr_info("[Mbraink][SPM] %s", sys_res_log_buf);
	spin_unlock_irqrestore(&sys_res_lock, flag);
}

static int mbraink_sys_res_get_id_name(struct mbriank_sys_res_mapping **map, unsigned int *size)
{
	unsigned int res_mapping_len;

	if (!map || !size)
		return -1;

	res_mapping_len = sizeof(sys_res_mapping) / sizeof(struct mbriank_sys_res_mapping);

	*size = res_mapping_len;
	*map = (struct mbriank_sys_res_mapping *)&sys_res_mapping;

	return 0;
}

static struct mbraink_sys_res_ops sys_res_ops = {
	.get = get_mbraink_sys_res_record,
	.update = update_mbraink_sys_res_record,
	.get_detail = mbraink_sys_res_get_detail,
	.get_threshold = mbraink_sys_res_get_threshold,
	.set_threshold = mbraink_sys_res_set_threshold,
	.enable_common_log = mbraink_sys_res_enable_common_log,
	.get_log_enable = mbraink_sys_res_get_log_enable,
	.log = mbraink_sys_res_log,
	.lock = &sys_res_lock,
	.get_id_name = mbraink_sys_res_get_id_name,
};

int mbraink_sys_res_plat_init(void)
{
	int ret, i, j;

	get_sys_res_group_info(sys_res_group_info);

	for (i = 0; i < MBRAINK_SYS_RES_SCENE_NUM; i++) {
		ret = mbraink_sys_res_alloc(&sys_res_record[i]);
		if (ret) {
			for (j = i - 1; j >= 0; j--)
				mbraink_sys_res_free(&sys_res_record[i]);
			pr_info("[Mbraink][SPM] sys_res alloc fail\n");
			return ret;
		}
	}

	sys_res_last_buffer_index = MBRAINK_SYS_RES_SCENE_LAST_SYNC;
	sys_res_temp_buffer_index = MBRAINK_SYS_RES_SCENE_TEMP;

	sys_res_last_suspend_diff_buffer_index = MBRAINK_SYS_RES_SCENE_LAST_SUSPEND_DIFF;
	sys_res_last_diff_buffer_index = MBRAINK_SYS_RES_SCENE_LAST_DIFF;

	spin_lock_init(&sys_res_lock);

	ret = register_mbraink_sys_res_ops(&sys_res_ops);
	if (ret) {
		pr_debug("[Mbraink][SPM] Failed to register mbraink sys_res operations.\n");
		return ret;
	}

	return 0;
}


void mbraink_sys_res_plat_deinit(void)
{
	int i;

	unregister_mbraink_sys_res_ops();

	for (i = 0; i < MBRAINK_SYS_RES_SCENE_NUM; i++)
		mbraink_sys_res_free(&sys_res_record[i]);
}

