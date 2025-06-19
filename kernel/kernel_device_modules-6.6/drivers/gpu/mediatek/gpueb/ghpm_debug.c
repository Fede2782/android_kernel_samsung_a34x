// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpueb_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "gpueb_helper.h"
#include "ghpm_debug.h"
#include "gpueb_ipi.h"
#include "ghpm_wrapper.h"
#include "gpueb_common.h"

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */

static unsigned int g_mfg0_ao_state;
int g_ghpm_profile_enable;

#if GHPM_TEST
static void gpuhre_read_backup(void);
static void gpuhre_write_random(void);
static void gpuhre_check_restore(void);

static void gpuhre_read_backup(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_READ_BACKUP,     /* a1 */
		0, 0, 0, 0, 0, 0, &res);

	gpueb_log_i(GHPM_TAG, "done");
}

static void gpuhre_write_random(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_WRITE_RANDOM,    /* a1 */
		0, 0, 0, 0, 0, 0, &res);
	gpueb_log_i(GHPM_TAG, "done");
}

static void gpuhre_check_restore(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUHRE_SMP_OP_CHECK_RESTORE,   /* a1 */
		0, 0, 0, 0, 0, 0, &res);
	gpueb_log_i(GHPM_TAG, "done");
}
#endif /* GHPM_TEST */

#if defined(CONFIG_PROC_FS)
#if GHPM_TEST
static int ghpm_ctrl_test_proc_show(struct seq_file *m, void *v)
{
	int ret = 0;

	seq_puts(m, "GHPM TEST\n");

	ret = ghpm_ctrl(GHPM_OFF, MFG1_OFF);
	if (ret)
		gpueb_log_e(GHPM_TAG, "ghpm_ctrl off failed, ret=%d", ret);
	else
		gpueb_log_i(GHPM_TAG, "ghpm_ctrl off done");

	ret = wait_gpueb(SUSPEND_POWER_OFF);
	if (ret)
		gpueb_log_e(GHPM_TAG, "wait_gpueb suspend failed, ret=%d", ret);
	else
		gpueb_log_i(GHPM_TAG, "wait_gpueb suspend done");

	ret = ghpm_ctrl(GHPM_ON, MFG1_OFF);
	if (ret)
		gpueb_log_e(GHPM_TAG, "ghpm_ctrl on failed, ret=%d", ret);
	else
		gpueb_log_i(GHPM_TAG, "ghpm_ctrl on done");

	ret = wait_gpueb(SUSPEND_POWER_ON);
	if (ret)
		gpueb_log_e(GHPM_TAG, "wait_gpueb resume failed, ret=%d", ret);
	else
		gpueb_log_i(GHPM_TAG, "wait_gpueb resume done");

	return 0;
}

static ssize_t ghpm_ctrl_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;
	int state, vcore_off_allow;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &state, &vcore_off_allow) == 2) {
		ret = ghpm_ctrl(state, vcore_off_allow);
		if (ret)
			gpueb_log_e(GHPM_TAG, "ghpm_ctrl failed, ret=%d", ret);
		else
			gpueb_log_i(GHPM_TAG, "ghpm_ctrl done");

		ret = wait_gpueb(state);
		if (ret)
			gpueb_log_e(GHPM_TAG, "wait_gpueb failed, ret=%d", ret);
		else
			gpueb_log_i(GHPM_TAG, "wait_gpueb done");
	}

done:
	return (ret < 0) ? ret : count;
}

static int gpuhre_read_backup_proc_show(struct seq_file *m, void *v)
{
	gpueb_log_i(GHPM_TAG, "read reg");
	gpuhre_read_backup();
	return 0;
}

static int gpuhre_write_random_proc_show(struct seq_file *m, void *v)
{
	gpueb_log_i(GHPM_TAG, "write random value");
	gpuhre_write_random();
	return 0;
}

static int gpuhre_check_restore_proc_show(struct seq_file *m, void *v)
{
	gpueb_log_i(GHPM_TAG, "check restore");
	gpuhre_check_restore();
	return 0;
}
#endif /* GHPM_TEST */

static int mfg0_ao_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "@%s: g_mfg0_ao_state=%d\n", __func__, g_mfg0_ao_state);

	return 0;
}

static ssize_t mfg0_ao_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sysfs_streq(buf, "enable") && g_mfg0_ao_state == MFG0_AO_DISABLE) {
		ret = gpueb_ctrl(GHPM_ON, MFG1_OFF, SUSPEND_POWER_ON);
		if (ret) {
			gpueb_log_e(GHPM_TAG, "fail to power on GPUEB (%d)", ret);
			goto done;
		}
		g_mfg0_ao_state = MFG0_AO_ENABLE;
	} else if (sysfs_streq(buf, "disable") && g_mfg0_ao_state == MFG0_AO_ENABLE) {
		ret = gpueb_ctrl(GHPM_OFF, MFG1_OFF, SUSPEND_POWER_OFF);
		if (ret) {
			gpueb_log_e(GHPM_TAG, "fail to power off GPUEB (%d)", ret);
			goto done;
		}
		g_mfg0_ao_state = MFG0_AO_DISABLE;
	} else {
		gpueb_log_e(GHPM_TAG, "useless: buf=%s, g_mfg0_ao_state=%d", buf, g_mfg0_ao_state);
	}

done:
	return (ret < 0) ? ret : count;
}

static int ghpm_profile_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "@%s: g_ghpm_profile_enable=%d\n", __func__, g_ghpm_profile_enable);
	seq_puts(m,"[Profiling] [Count###] [Latest] [Average] [Maximum] [Minimum]\n");
	for (i = 0; i < PROF_GHPM_TYPE_NUM; i++) {
		seq_printf(m, "[%-9s] %10lld %6lldus %7lldus %7lldus %7lldus\n",
			GHPM_PROFILE_TYPE_STRING(i),
			g_ghpm_profile[i][PROF_GHPM_IDX_COUNT],
			g_ghpm_profile[i][PROF_GHPM_IDX_LAST],
			g_ghpm_profile[i][PROF_GHPM_IDX_AVG],
			g_ghpm_profile[i][PROF_GHPM_IDX_MAX],
			g_ghpm_profile[i][PROF_GHPM_IDX_MIN]);
	}

	return 0;
}

static ssize_t ghpm_profile_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sysfs_streq(buf, "start") && g_ghpm_profile_enable == 0) {
		g_ghpm_profile_enable = 1;
	} else if (sysfs_streq(buf, "end") && g_ghpm_profile_enable == 1) {
		g_ghpm_profile_enable = 0;
	} else {
		gpueb_log_e(GHPM_TAG, "useless: buf=%s, g_ghpm_profile_enable=%d",
			buf, g_ghpm_profile_enable);
	}

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS : initialization */
#if GHPM_TEST
PROC_FOPS_RW(ghpm_ctrl_test);
PROC_FOPS_RO(gpuhre_read_backup);
PROC_FOPS_RO(gpuhre_write_random);
PROC_FOPS_RO(gpuhre_check_restore);
#endif
PROC_FOPS_RW(mfg0_ao);
PROC_FOPS_RW(ghpm_profile);

static int ghpm_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
#if GHPM_TEST
		PROC_ENTRY(ghpm_ctrl_test),
		PROC_ENTRY(gpuhre_read_backup),
		PROC_ENTRY(gpuhre_write_random),
		PROC_ENTRY(gpuhre_check_restore),
#endif
		PROC_ENTRY(mfg0_ao),
		PROC_ENTRY(ghpm_profile)
	};

	dir = proc_mkdir("ghpm", NULL);
	if (!dir) {
		gpueb_log_e(GHPM_TAG, "fail to create /proc/ghpm (ENOMEM)");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			gpueb_log_i(GHPM_TAG, "fail to create /proc/ghpm/%s", default_entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

void ghpm_debug_init(struct platform_device *pdev)
{
#if defined(CONFIG_PROC_FS)
	int ret = 0;

	gpueb_log_i(GHPM_TAG, "ghpm_create_procfs");
	ret = ghpm_create_procfs();
	if (ret)
		gpueb_log_e(GHPM_TAG, "fail to create procfs (%d)", ret);
	g_mfg0_ao_state = MFG0_AO_DISABLE;
#else
	gpueb_log_i(GHPM_TAG, "CONFIG_PROC_FS is disabled");
	return;
#endif
}
