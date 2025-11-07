// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    ghpm.c
 * @brief   GHPM API implementation
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "ghpm_wrapper.h"
#include "ghpm.h"
#include "gpueb_debug.h"
#include "gpueb_common.h"
#include "gpueb_timesync.h"


static bool __is_gpueb_exist(void);
static int __ghpm_pdrv_probe(struct platform_device *pdev);
static int __ghpm_init(void);
static int __mfg0_on_if_not_duplicate(void);
static int __ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state);
static int __wait_gpueb(enum gpueb_low_power_event event);
static void __dump_ghpm_info(void);
static void __dump_mfg_pwr_sta(void);
#if GHPM_TIMESTAMP_MONITOR_EN
static void __ghpm_timestamp_monitor(enum ghpm_timestamp_monitor_point point);
#else
#define __ghpm_timestamp_monitor(point) do {} while (0)
#endif

static const struct of_device_id g_ghpm_of_match[] = {
	{ .compatible = "mediatek,ghpm" },
	{ /* sentinel */ }
};
static struct platform_driver g_ghpm_pdrv = {
	.probe = __ghpm_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "ghpm",
		.owner = THIS_MODULE,
		.of_match_table = g_ghpm_of_match,
	},
};

static int g_ipi_channel;
static int g_gpueb_slot_size;
static atomic_t g_power_count;  /* power count for ghpm control mfg0 on/off */
static atomic_t g_progress_status;
static void __iomem *g_gpueb_lp_state_gpr;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_mfg_vcore_ao_config_base;
static void __iomem *g_spm_mfg0_pwr_con;
static void __iomem *g_clk_cfg_6;
static void __iomem *g_clk_cfg_6_set;
static void __iomem *g_clk_cfg_6_clr;
static struct gpueb_slp_ipi_data msgbuf;
static unsigned long g_pwr_irq_flags;
static raw_spinlock_t ghpm_lock;
static bool first_on_after_bootup;
static enum mfg_mt6991_e2_con g_mfg_mt6991_e2_con;
#if GHPM_TIMESTAMP_MONITOR_EN
static unsigned long long g_ghpm_ts64[GHPM_TS_MONITOR_NUM];
#endif
static int g_gpueb_resume_fail_recovery_times;

unsigned int g_ghpm_ready;
EXPORT_SYMBOL(g_ghpm_ready);

static struct ghpm_platform_fp platform_ghpm_fp = {
	.ghpm_ctrl = __ghpm_ctrl,
	.wait_gpueb = __wait_gpueb,
	.dump_ghpm_info = __dump_ghpm_info,
};

#if GHPM_TIMESTAMP_MONITOR_EN
static void __ghpm_timestamp_monitor(enum ghpm_timestamp_monitor_point point)
{
	if (point < GHPM_TS_MONITOR_NUM)
		g_ghpm_ts64[point] = sched_clock();
}
#endif

static void __dump_mfg_pwr_sta(void)
{
	gpueb_log_e(GHPM_TAG, "0:0x%08x/0x%0x8x, 1:0x%08x, 2:0x%08x, 37:0x%08x",
		readl(MFG_RPC_MFG0_PWR_CON), readl(g_spm_mfg0_pwr_con),
		readl(MFG_RPC_MFG1_PWR_CON), readl(MFG_RPC_MFG2_PWR_CON),
		readl(MFG_RPC_MFG37_PWR_CON));
}

static void __ghpm_assert(void)
{
	__dump_ghpm_info();
	__dump_mfg_pwr_sta();
	gpueb_dump_status(NULL, NULL, 0);
	BUG_ON(1);
}

static void __ghpm_enable(void)
{
	if (g_mfg_mt6991_e2_con == MFG_MT6991_A0)
		writel(readl(MFG_GHPM_CFG0_CON) | GHPM_EN, MFG_GHPM_CFG0_CON);
	else
		writel(readl(MFG_RPCTOP_DUMMY_REG_2) | GHPM_EN_FOR_MT6991_B0,
			MFG_RPCTOP_DUMMY_REG_2);
}

static void __ghpm_disable(void)
{
	if (g_mfg_mt6991_e2_con == MFG_MT6991_A0)
		writel(readl(MFG_GHPM_CFG0_CON) & ~GHPM_EN, MFG_GHPM_CFG0_CON);
	else
		writel(readl(MFG_RPCTOP_DUMMY_REG_2) & ~GHPM_EN_FOR_MT6991_B0,
			MFG_RPCTOP_DUMMY_REG_2);
}

static int __trigger_ghpm_on(void)
{
	int i;

	ghpm_profile(PROF_GHPM_CTRL_ON, PROF_GHPM_OP_START);

	/* Turn on CLK_MFG_EB */
	writel(PDN_MFG_EB_BIT, CLK_CKFG_6_CLR);

	/* trigger ghpm on -> reset gpueb -> warm boot -> gpueb resume */
	gpueb_log_d(GHPM_TAG, "ghpm on");

	/* Polling GHPM IDLE state MFG_GHPM_RO0_CON [7:0] = 8'b0*/
	i = 0;
	while (((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0)) {
		udelay(1);
		if (++i > GPUEB_WAIT_TIMEOUT) {
			gpueb_log_e(GHPM_TAG, "GHPM ON, check ghpm_state(0x%x)=idle failed",
				readl(MFG_GHPM_RO0_CON));
			return GHPM_STATE_ERR;
		}
	}

	/* Polling GHPM_PWR_STATE MFG_GHPM_RO0_CON [16] = 1'b0*/
	i = 0;
	while (((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) == GHPM_PWR_STATE)) {
		udelay(1);
		if (++i > GPUEB_WAIT_TIMEOUT) {
			gpueb_log_e(GHPM_TAG, "MFG0 off not by GHPM last time");
			return GHPM_PWR_STATE_ERR;
		}
	}

	__ghpm_timestamp_monitor(TRIGGER_GHPM_ON);
	/* Trigger GHPM on sequence */
	__ghpm_enable();
	writel(readl(MFG_GHPM_CFG0_CON) & ~ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
	writel(readl(MFG_GHPM_CFG0_CON) | ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
	__ghpm_disable();

	atomic_set(&g_progress_status, POWER_ON_IN_PROGRESS);
	ghpm_profile(PROF_GHPM_CTRL_ON, PROF_GHPM_OP_END);
	gpueb_log_d(GHPM_TAG, "ghpm trigger on done");

	return GHPM_SUCCESS;
}

static void __ghpm_off_on_recovery(void)
{
	int i = 0;

	gpueb_log_i(GHPM_TAG, "Try to recover GPUEB through power off then power on MFG0 again");

	if (g_mfg_mt6991_e2_con == MFG_MT6991_A0)
		writel(readl(MFG_RPC_AO_CLK_CFG) | CG_FAXI_CK_SOC_IN_FREE_RUN, MFG_RPC_AO_CLK_CFG);

	/* Select trigger GHPM off sequence method: RPC */
	writel(readl(MFG_GHPM_CFG0_CON) | SW_OFF_SEQ_TRI_SEL, MFG_GHPM_CFG0_CON);

	/* Trigger GHPM off sequence by RPC */
	__ghpm_enable();
	writel(readl(MFG_GHPM_CFG0_CON) & ~SW_OFF_SEQ_TRI, MFG_GHPM_CFG0_CON);
	writel(readl(MFG_GHPM_CFG0_CON) | SW_OFF_SEQ_TRI, MFG_GHPM_CFG0_CON);
	__ghpm_disable();

	/* Select trigger GHPM off sequence method back to GPUEB dedicate line to GHPM */
	writel(readl(MFG_GHPM_CFG0_CON) & ~SW_OFF_SEQ_TRI_SEL, MFG_GHPM_CFG0_CON);

	/* Wait GHPM off MFG0 finished */
	i = 0;
	while (((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0) ||
			((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) == GHPM_PWR_STATE) ||
			(mfg0_pwr_sta() != MFG0_PWR_OFF)) {
		udelay(1);
		if (++i > GPUEB_WAIT_TIMEOUT) {
			gpueb_log_e(GHPM_TAG, "Wait MFG0 off failed when try to recovery");
			__ghpm_assert();
		}
	}

	/* Trigger GHPM on sequence */
	__ghpm_enable();
	writel(readl(MFG_GHPM_CFG0_CON) & ~ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
	writel(readl(MFG_GHPM_CFG0_CON) | ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
	__ghpm_disable();

	/* Wait GHPM on MFG0 finished */
	i = 0;
	while (((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0) ||
			((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) != GHPM_PWR_STATE) ||
			(mfg0_pwr_sta() != MFG0_PWR_ON)) {
		udelay(1);
		if (++i > GPUEB_WAIT_TIMEOUT) {
			gpueb_log_e(GHPM_TAG, "Wait MFG0 off then on failed when try to recovery");
			__ghpm_assert();
		}
	}
}

static int __mfg0_on_if_not_duplicate(void)
{
	int ret;
	int i;

	gpueb_log_d(GHPM_TAG, "mfg0_pwr_con(0x%x), pwr cnt=%d, progress=%d",
		get_mfg0_pwr_con(), atomic_read(&g_power_count), atomic_read(&g_progress_status));

	if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
		gpueb_log_d(GHPM_TAG, "power off in progress by others");
		/* another thread already notify gpueb to trigger ghpm off */
		i = 0;
		while (((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0) ||
			((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) == GHPM_PWR_STATE) ||
			(mfg0_pwr_sta() != MFG0_PWR_OFF)) {
			udelay(1);
			if (++i > GPUEB_WAIT_TIMEOUT) {
				gpueb_log_e(GHPM_TAG, "wait ghpm off mfg0 finish timeout");
				ret = GHPM_DUPLICATE_ON_ERR;
			}
		}
		gpueb_log_d(GHPM_TAG, "mfg0_pwr_con(0x%x), pwr cnt=%d, progress=%d",
			get_mfg0_pwr_con(), atomic_read(&g_power_count),
			atomic_read(&g_progress_status));

		ret = __trigger_ghpm_on();
	} else {
		gpueb_log_e(GHPM_TAG, "power/power_cnt not aligned");
		ret = GHPM_DUPLICATE_ON_ERR;
	}

	return ret;
}

static int __ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state)
{
	int ret = GHPM_ERR;
	struct gpueb_slp_ipi_data data;

	raw_spin_lock_irqsave(&ghpm_lock, g_pwr_irq_flags);

	gpueb_log_d(GHPM_TAG, "ENTRY, power=%d, g_progress_status=%d, g_power_count=%d",
		power, atomic_read(&g_progress_status), atomic_read(&g_power_count));

	if (power == GHPM_ON) {
		atomic_inc(&g_power_count);
		if (atomic_read(&g_power_count) == 1) {
			if (mfg0_pwr_sta() == MFG0_PWR_ON) {
				if (first_on_after_bootup == false) {
					/*
					 * MFG0 shutdown on from TFA after bootup, ghpm don't need
					 * to power on mfg0 again when ghpm on first time coming
					 */
					gpueb_log_d(GHPM_TAG, "ghpm on first been called");
					first_on_after_bootup = true;
					ret = GHPM_SUCCESS;
				} else {
					/* power count = 1 but mfg0 already on */
					ret = __mfg0_on_if_not_duplicate();
				}
			} else {
				/* trigger ghpm on -> reset gpueb -> warm boot -> gpueb resume */
				ret = __trigger_ghpm_on();
			}
		} else {
			gpueb_log_d(GHPM_TAG, "no need to ghpm ctrl on");
			ret = GHPM_SUCCESS;
		}
	} else if (power == GHPM_OFF) {
		atomic_dec(&g_power_count);
		if (atomic_read(&g_power_count) == 0) {
			gpueb_log_d(GHPM_TAG, "gpueb off and ghpm trigger off");
			ghpm_profile(PROF_GHPM_CTRL_OFF, PROF_GHPM_OP_START);

			/* IPI to gpueb for suspend flow and then trigger ghpm off */
			data.event = SUSPEND_POWER_OFF;
			data.off_state = off_state;
			data.magic = GPUEB_SLEEP_IPI_MAGIC_NUMBER;

			__ghpm_timestamp_monitor(IPI_SUSPEND_GPUEB);

			ret = mtk_ipi_send(
				get_gpueb_ipidev(),
				g_ipi_channel,
				IPI_SEND_POLLING,
				(void *)&data,
				(sizeof(data) / g_gpueb_slot_size),
				GHPM_IPI_TIMEOUT);

			if (unlikely(ret != IPI_ACTION_DONE)) {
				gpueb_log_e(GHPM_TAG, "OFF IPI failed, ret=%d", ret);
				ret = GHPM_OFF_EB_IPI_ERR;
				goto done_unlock;
			}
			atomic_set(&g_progress_status, POWER_OFF_IN_PROGRESS);
			ghpm_profile(PROF_GHPM_CTRL_OFF, PROF_GHPM_OP_END);
			gpueb_log_d(GHPM_TAG, "suspend gpueb ipi done");
		} else {
			gpueb_log_d(GHPM_TAG, "no need to ghpm ctrl off");
		}
		ret = GHPM_SUCCESS;
	} else {
		gpueb_log_e(GHPM_TAG, "Invalid power=%d", power);
		ret = GHPM_INPUT_ERR;
	}

done_unlock:
	gpueb_log_d(GHPM_TAG, "EXIT, power=%d, g_progress_status=%d, g_power_count=%d",
		power, atomic_read(&g_progress_status), atomic_read(&g_power_count));

	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);

	if (ret != GHPM_SUCCESS)
		__ghpm_assert();

	return ret;
}

static int __wait_gpueb(enum gpueb_low_power_event event)
{
	unsigned int i;
	int recovery_flag = 0; /* Only 1 chance of recovery everytime when GPUEB resume failed */
	unsigned int val = 0;

	raw_spin_lock_irqsave(&ghpm_lock, g_pwr_irq_flags);
	gpueb_log_d(GHPM_TAG, "Entry, event=%d, g_progress_status=%d, g_power_count=%d",
		event, atomic_read(&g_progress_status), atomic_read(&g_power_count));

	if (event == SUSPEND_POWER_ON) {
		if (atomic_read(&g_progress_status) == POWER_ON_IN_PROGRESS) {
			ghpm_profile(PROF_GHPM_WAIT_ON, PROF_GHPM_OP_START);
			i = 0;
			__ghpm_timestamp_monitor(POLLING_GHPM_ON_START);
			val = readl(MFG_GHPM_RO0_CON);
			while (((val & GHPM_STATE) != 0x0) || ((val & GHPM_PWR_STATE) != GHPM_PWR_STATE) ||
				(mfg0_pwr_sta() != MFG0_PWR_ON)) {
#if GHPM_TIMEOUT_ERR_KE
				if ((readl(MFG_GHPM_RO0_CON) & TIMEOUT_ERR_RECORD) == TIMEOUT_ERR_RECORD) {
					__ghpm_timestamp_monitor(POLLING_GHPM_ON_TIMEOUT_ERR);
					gpueb_log_e(GHPM_TAG, "GHPM ON, timeout error record assert");
					goto wait_err;
				}
#endif
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					__ghpm_timestamp_monitor(POLLING_GHPM_ON_TIMEOUT);
					gpueb_log_e(GHPM_TAG, "Wait MFG0 on failed");
					goto wait_err;
				}
				if (i == GPUEB_WAIT_CHECK_TIME_1 || i == GPUEB_WAIT_CHECK_TIME_2) {
					gpueb_log_e(GHPM_TAG, "GHPM ON, i=%d polling timeout dump", i);
					__dump_ghpm_info();
					__dump_mfg_pwr_sta();
				}
				val = readl(MFG_GHPM_RO0_CON);
			}
#if GHPM_TIMEOUT_ERR_KE
			if ((readl(MFG_GHPM_RO0_CON) & TIMEOUT_ERR_RECORD) == TIMEOUT_ERR_RECORD) {
				__ghpm_timestamp_monitor(POLLING_GHPM_ON_TIMEOUT_ERR);
				gpueb_log_e(GHPM_TAG, "GHPM ON, timeout error record assert");
				goto wait_err;
			}
#endif
			/* Polling gpr after mfg0 on to check if gpueb resumed or not */
			i = 0;
			__ghpm_timestamp_monitor(POLLING_GPUEB_RESUME_START);
			while ((readl(g_gpueb_lp_state_gpr) != GPUEB_ON_RESUME)) {
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					__ghpm_timestamp_monitor(POLLING_GPUEB_RESUME_TIMEOUT);
					gpueb_log_e(GHPM_TAG, "g_gpueb_lp_state_gpr: 0x%08x",
						readl(g_gpueb_lp_state_gpr));
					goto wait_err;
				}

				/* In case GHPM on done but GPUEB resume failed (e.g. EB reset not working) */
				if (recovery_flag == 0 && i == GPUEB_WAIT_CHECK_TIME_1) {
					gpueb_log_e(GHPM_TAG, "GHPM ON but GPUEB resume failed (i=%d)", i);
					__dump_ghpm_info();
					__dump_mfg_pwr_sta();
					gpueb_dump_status(NULL, NULL, 0);
					__ghpm_timestamp_monitor(GHPM_OFF_ON_RECOVERY);
					__ghpm_off_on_recovery();

					recovery_flag = 1;
					g_gpueb_resume_fail_recovery_times++;

					/* Reset polling timeout counter */
					i = 0;
				}
			}
			gpueb_timesync_update();
			__ghpm_timestamp_monitor(GPUEB_ON_DONE);
			atomic_set(&g_progress_status, NOT_IN_PROGRESS);
			ghpm_profile(PROF_GHPM_WAIT_ON, PROF_GHPM_OP_END);
			gpueb_log_d(GHPM_TAG, "GPUEB resume done, i=%u", i);
		} else if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
			gpueb_log_e(GHPM_TAG, "wrong call order by user, no on before off");
			goto wait_err;
		} else if (atomic_read(&g_progress_status) == NOT_IN_PROGRESS) {
			gpueb_log_d(GHPM_TAG, "no need to wait");
		} else {
			gpueb_log_e(GHPM_TAG, "g_progress_status=%d unexpected!",
				atomic_read(&g_progress_status));
			goto wait_err;
		}
	} else if (event == SUSPEND_POWER_OFF) {
		if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
			ghpm_profile(PROF_GHPM_WAIT_OFF, PROF_GHPM_OP_START);
			__ghpm_timestamp_monitor(POLLING_GPUEB_OFF_START);
			i = 0;
			while ((mfg0_pwr_sta() != MFG0_PWR_OFF) ||
				((readl(MFG_GHPM_RO0_CON) & (GHPM_STATE | GHPM_PWR_STATE)) != 0x0)) {
#if GHPM_TIMEOUT_ERR_KE
				if ((readl(MFG_GHPM_RO0_CON) & TIMEOUT_ERR_RECORD) == TIMEOUT_ERR_RECORD) {
					__ghpm_timestamp_monitor(POLLING_GHPM_OFF_TIMEOUT_ERR);
					gpueb_log_e(GHPM_TAG, "GHPM OFF, timeout error record assert");
					goto wait_err;
				}
#endif
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					__ghpm_timestamp_monitor(POLLING_GPUEB_OFF_TIMEOUT);
					gpueb_log_e(GHPM_TAG, "Wait MFG0 off failed");
					goto wait_err;
				}
				if (i == GPUEB_WAIT_CHECK_TIME_1 || i == GPUEB_WAIT_CHECK_TIME_2) {
					gpueb_log_e(GHPM_TAG, "GHPM OFF, i=%d polling timeout dump", i);
					__dump_ghpm_info();
					__dump_mfg_pwr_sta();
				}
			}
			if ((readl(MFG_GHPM_RO0_CON) & TIMEOUT_ERR_RECORD) == TIMEOUT_ERR_RECORD) {
				__ghpm_timestamp_monitor(POLLING_GHPM_OFF_TIMEOUT_ERR);
#if GHPM_TIMEOUT_ERR_KE
				gpueb_log_e(GHPM_TAG, "GHPM OFF, timeout error record assert");
				goto wait_err;
#endif
			}
			atomic_set(&g_progress_status, NOT_IN_PROGRESS);

			/* Turn off CLK_MFG_EB */
			writel(PDN_MFG_EB_BIT, CLK_CKFG_6_SET);

			__ghpm_timestamp_monitor(GPUEB_OFF_DONE);
			ghpm_profile(PROF_GHPM_WAIT_OFF, PROF_GHPM_OP_END);
			gpueb_log_d(GHPM_TAG, "GPUEB suspend done, i=%u", i);
		} else if (atomic_read(&g_progress_status) == POWER_ON_IN_PROGRESS) {
			gpueb_log_d(GHPM_TAG, "on after off at once, no need to wait");
		} else if (atomic_read(&g_progress_status) == NOT_IN_PROGRESS) {
			gpueb_log_d(GHPM_TAG, "no need to wait");
		} else {
			gpueb_log_e(GHPM_TAG, "g_progress_status=%d unexpected!",
				atomic_read(&g_progress_status));
			goto wait_err;
		}
	}

	gpueb_log_d(GHPM_TAG, "Exit, event=%d, g_progress_status=%d, g_power_count=%d",
		event, atomic_read(&g_progress_status), atomic_read(&g_power_count));
	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);

	return WAIT_DONE;

wait_err:
#if GPUEB_WAIT_OFF_FAIL_WRITE_DUMMY
	if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS && mfg0_pwr_sta() == MFG0_PWR_OFF) {
		writel(GPUEB_WAIT_OFF_FAIL_FLAG, MFG_RPCTOP_DUMMY_REG_0);
		gpueb_log_e(GHPM_TAG, "Set MFG_RPCTOP_DUMMY_REG_0=0x%x", readl(MFG_RPCTOP_DUMMY_REG_0));
	}
#endif
	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);
	gpueb_log_e(GHPM_TAG, "Wait GPUEB timeout, event=%d", event);
	__ghpm_assert();
	return WAIT_TIMEOUT;
}

static void __dump_ghpm_info(void)
{
#if GHPM_TIMESTAMP_MONITOR_EN
	int i;
#endif
	gpueb_log_e(GHPM_TAG, "MFG_GHPM_RO0_CON=0x%x", readl(MFG_GHPM_RO0_CON));
	gpueb_log_e(GHPM_TAG, "MFG_GHPM_RO1_CON=0x%x", readl(MFG_GHPM_RO1_CON));
	gpueb_log_e(GHPM_TAG, "MFG_GHPM_RO2_CON=0x%x", readl(MFG_GHPM_RO2_CON));
	gpueb_log_e(GHPM_TAG, "MFG_RPC_MFG0_PWR_CON=0x%x", readl(MFG_RPC_MFG0_PWR_CON));
	gpueb_log_e(GHPM_TAG, "MFG_RPC_DUMMY_REG=0x%x", readl(MFG_RPC_DUMMY_REG));
	gpueb_log_e(GHPM_TAG, "MFG_RPC_DUMMY_REG_1=0x%x", readl(MFG_RPC_DUMMY_REG_1));
	gpueb_log_e(GHPM_TAG, "MFGSYS_PROTECT_EN_SET_0=0x%x", readl(MFGSYS_PROTECT_EN_SET_0));
	gpueb_log_e(GHPM_TAG, "MFGSYS_PROTECT_EN_STA_0=0x%x", readl(MFGSYS_PROTECT_EN_STA_0));
	gpueb_log_e(GHPM_TAG, "MFG_SODI_EMI=0x%x", readl(MFG_SODI_EMI));
	gpueb_log_e(GHPM_TAG, "g_progress_status=%d, g_power_count=%d, g_gpueb_resume_fail_recovery_times=%d",
		atomic_read(&g_progress_status), atomic_read(&g_power_count), g_gpueb_resume_fail_recovery_times);

#if GHPM_TIMESTAMP_MONITOR_EN
	for (i = 0; i < GHPM_TS_MONITOR_NUM; i++) {
		gpueb_log_e(GHPM_TAG, "[%s]: %lld",
			GHPM_TS_MON_STRING(i), g_ghpm_ts64[i]);
	}
#endif
}

static bool __is_gpueb_exist(void)
{
	struct device_node *of_gpueb = NULL;

	of_gpueb = of_find_compatible_node(NULL, NULL, "mediatek,gpueb");
	if (!of_gpueb) {
		gpueb_log_e(GHPM_TAG, "fail to find gpueb of_node");
		return false;
	}

	return true;
}

static enum mfg_mt6991_e2_con __mfg_mt6991_e2_con(void __iomem *reg)
{
	int val;

	val = readl(reg);
	if (val == MFG_MT6991_E1_ID)
		return MFG_MT6991_A0;
	else if (val == MFG_MT6991_E2_ID)
		return MFG_MT6991_B0;

	gpueb_log_e(GHPM_TAG, "Unknown MT6991 E2 ID CON value: 32'd%d\n, treat as E1", val);
	return MFG_MT6991_A0;
}

static int __ghpm_pdrv_probe(struct platform_device *pdev)
{
	struct device *ghpm_dev = &pdev->dev;
	struct resource *res = NULL;
	void __iomem *mfg_mt6991_e2_id_con;
	int ret = -ENOENT;

	gpueb_log_i(GHPM_TAG, "start to probe ghpm driver");

	if (!__is_gpueb_exist()) {
		gpueb_log_i(GHPM_TAG, "no gpueb node, skip probe");
		goto done;
	}

	if (!g_ghpm_support) {
		gpueb_log_i(GHPM_TAG, "ghpm not support, skip probe");
		ret = 0;
		goto done;
	}

	if (unlikely(!ghpm_dev)) {
		gpueb_log_e(GHPM_TAG, "fail to find ghpm device");
		goto done;
	}

	/* get mfg_rpc base address */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap MFG_RPC: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get mfg_vcore_ao_config base address */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_vcore_ao_config");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource MFG_VCORE_AO_CONFIG");
		goto done;
	}
	g_mfg_vcore_ao_config_base = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_vcore_ao_config_base)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap MFG_VCORE_AO_CONFIG: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get g_spm_mfg0_pwr_con */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_mfg0_pwr_con");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource SPM_MFG0_PWR_CON");
		goto done;
	}
	g_spm_mfg0_pwr_con = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_spm_mfg0_pwr_con)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap SPM_MFG0_PWR_CON: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get g_clk_cfg_6 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk_cfg_6");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource CLK_CFG_6");
		goto done;
	}
	g_clk_cfg_6 = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_clk_cfg_6)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap CLK_CFG_6: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get g_clk_cfg_6_set */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk_cfg_6_set");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource CLK_CFG_6_SET");
		goto done;
	}
	g_clk_cfg_6_set = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_clk_cfg_6_set)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap CLK_CFG_6_SET: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get g_clk_cfg_6_clr */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk_cfg_6_clr");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource CLK_CFG_6_CLR");
		goto done;
	}
	g_clk_cfg_6_clr = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_clk_cfg_6_clr)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap CLK_CFG_6_CLR: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}

	/* get mfg_mt6991_e2_id_con base address */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_mt6991_e2_id_con");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_TAG, "fail to get resource mfg_mt6991_e2_id_con");
		goto done;
	}
	mfg_mt6991_e2_id_con = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!mfg_mt6991_e2_id_con)) {
		gpueb_log_e(GHPM_TAG, "fail to ioremap mfg_mt6991_e2_id_con: 0x%llx",
			(unsigned long long) res->start);
		goto done;
	}
	g_mfg_mt6991_e2_con = __mfg_mt6991_e2_con(mfg_mt6991_e2_id_con);

	/* get GPUEB_LP_STATE_GPR address */
	g_gpueb_lp_state_gpr = gpueb_get_gpr_addr(GPUEB_SRAM_GPR10);
	if (unlikely(!g_gpueb_lp_state_gpr)) {
		gpueb_log_e(GPUEB_TAG, "fail to get GPUEB_LP_STATE_GPR (%d)", GPUEB_SRAM_GPR10);
		goto done;
	}

	g_ipi_channel = gpueb_get_send_PIN_ID_by_name("IPI_ID_SLEEP");
	if (unlikely(g_ipi_channel < 0)) {
		gpueb_log_e(GHPM_TAG, "fail to get IPI_ID_SLEEP id");
		goto done;
	}

	ret = mtk_ipi_register(get_gpueb_ipidev(), g_ipi_channel, NULL, NULL, &msgbuf);
	if (ret != IPI_ACTION_DONE) {
		gpueb_log_e(GHPM_TAG, "ipi register fail: id=%d, ret=%d", g_ipi_channel, ret);
		goto done;
	}

	g_gpueb_slot_size = get_gpueb_slot_size();
	if (unlikely(!g_gpueb_slot_size)) {
		gpueb_log_e(GPUEB_TAG, "fail to get gpueb slot size");
		goto done;
	}

	atomic_set(&g_power_count, 0);
	atomic_set(&g_progress_status, NOT_IN_PROGRESS);
	raw_spin_lock_init(&ghpm_lock);
	first_on_after_bootup = false;

	ghpm_register_ghpm_fp(&platform_ghpm_fp);

	ret = 0;
	gpueb_log_i(GHPM_TAG, "ghpm driver probe done");

done:
	g_ghpm_ready = 1;
	return ret;
}

/* API: register ghpm platform driver */
static int __init __ghpm_init(void)
{
	int ret = 0;

	gpueb_log_i(GHPM_TAG, "start to init ghpm platform driver");

	/* register ghpm platform driver */
	ret = platform_driver_register(&g_ghpm_pdrv);
	if (ret) {
		gpueb_log_e(GHPM_TAG, "fail to register ghpm platform driver: %d", ret);
		goto done;
	}

	gpueb_log_i(GHPM_TAG, "ghpm platform driver init done");

done:
	return ret;
}

/* API: unregister ghpm driver */
static void __exit __ghpm_exit(void)
{
	platform_driver_unregister(&g_ghpm_pdrv);
}

module_init(__ghpm_init);
module_exit(__ghpm_exit);

MODULE_DEVICE_TABLE(of, g_ghpm_of_match);
MODULE_DESCRIPTION("MediaTek GHPM platform driver");
MODULE_LICENSE("GPL");
