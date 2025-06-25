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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include "linux/atomic/atomic-instrumented.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>
#include <linux/arm-smccc.h>
#include <uapi/asm-generic/errno-base.h>

#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "gpueb_common.h"
#include "gpueb_debug.h"
#include "ghpm_wrapper.h"
#include "ghpm_swwa.h"
#include "ghpm_debug.h"
#include "gpueb_timesync.h"


static bool __is_gpueb_exist(void);
static int __ghpm_swwa_pdrv_probe(struct platform_device *pdev);
static int __ghpm_swwa_init(void);
static int __mfg0_on_if_not_duplicate(void);
static int __ghpm_swwa_ctrl(enum ghpm_state power, enum mfg0_off_state off_state);
static int __wait_gpueb(enum gpueb_low_power_event event);
static void __dump_ghpm_info(void);
static void __dump_mfg_pwr_sta(void);

static const struct of_device_id g_ghpm_swwa_of_match[] = {
	{ .compatible = "mediatek,ghpm" },
	{ /* sentinel */ }
};
static struct platform_driver g_ghpm_swwa_pdrv = {
	.probe = __ghpm_swwa_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "ghpm_swwa",
		.owner = THIS_MODULE,
		.of_match_table = g_ghpm_swwa_of_match,
	},
};

static int g_ipi_channel;
static int g_gpueb_slot_size;
static atomic_t g_power_count;  /* power count for ghpm control mfg0 on/off */
static atomic_t g_progress_status;
static void __iomem *g_gpueb_lp_state_gpr;
static void __iomem *g_mfg1_pwr_con;
static void __iomem *g_mfg2_pwr_con;
static void __iomem *g_mfg37_pwr_con;
static struct gpueb_slp_ipi_data msgbuf;
static unsigned long g_pwr_irq_flags;
static raw_spinlock_t ghpm_lock;
static bool first_on_after_bootup;

unsigned int g_ghpm_ready;
EXPORT_SYMBOL(g_ghpm_ready);

static struct ghpm_platform_fp platform_ghpmswwa_fp = {
	.ghpm_ctrl = __ghpm_swwa_ctrl,
	.wait_gpueb = __wait_gpueb,
	.dump_ghpm_info = __dump_ghpm_info,
};

static void __dump_mfg_pwr_sta(void)
{
	gpueb_log_e(GHPM_SWWA_TAG, "0:0x%08x, 1:0x%08x, 2:0x%08x, 37:0x%08x",
		get_mfg0_pwr_con(), readl(g_mfg1_pwr_con),
		readl(g_mfg2_pwr_con), readl(g_mfg37_pwr_con));
}

static int __mfg0_on_if_not_duplicate(void)
{
	int ret;
	int i;
	struct arm_smccc_res res;

	gpueb_log_d(GHPM_SWWA_TAG, "mfg0_pwr_con(0x%x), pwr cnt=%d, progress=%d",
		get_mfg0_pwr_con(), atomic_read(&g_power_count), atomic_read(&g_progress_status));

	if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
		gpueb_log_d(GHPM_SWWA_TAG, "power off in progress by others");
		/* another thread is already powering off */
		i = 0;
		while (mfg0_pwr_sta() != MFG0_PWR_OFF) {
			udelay(1);
			if (++i > GPUEB_WAIT_TIMEOUT) {
				gpueb_log_e(GHPM_SWWA_TAG, "wait mfg0 off finish timeout");
				ret = GHPM_DUPLICATE_ON_ERR;
			}
		}
		gpueb_log_d(GHPM_SWWA_TAG, "mfg0_pwr_con(0x%x), pwr cnt=%d, progress=%d",
			get_mfg0_pwr_con(), atomic_read(&g_power_count),
			atomic_read(&g_progress_status));

		/* smc to power on mfg0 -> reset eb -> warm boot -> eb resume */
		gpueb_log_d(GHPM_SWWA_TAG, "ghpm swwa on");
		arm_smccc_smc(
			MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
			GHPM_SWWA_SMP_OP_MFG0_ON,      /* a1 */
			0, 0, 0, 0, 0, 0, &res);
		atomic_set(&g_progress_status, POWER_ON_IN_PROGRESS);
		gpueb_log_d(GHPM_SWWA_TAG, "ghpm swwa on done");
		atomic_set(&g_progress_status, POWER_ON_IN_PROGRESS);
		ret = GHPM_SUCCESS;
	} else {
		gpueb_log_e(GHPM_SWWA_TAG, "power/power_cnt not aligned");
		ret = GHPM_DUPLICATE_ON_ERR;
	}

	return ret;
}

static int __ghpm_swwa_ctrl(enum ghpm_state power, enum mfg0_off_state off_state)
{
	int ret = GHPM_SWWA_ERR;
	struct gpueb_slp_ipi_data data;
	struct arm_smccc_res res;
	int i;

	raw_spin_lock_irqsave(&ghpm_lock, g_pwr_irq_flags);

	gpueb_log_d(GHPM_SWWA_TAG, "ENTRY, power=%d, g_progress_status=%d, g_power_count=%d",
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
					gpueb_log_d(GHPM_SWWA_TAG, "ghpm on first been called");
					first_on_after_bootup = true;
					ret = GHPM_SUCCESS;
				} else {
					/* power count = 1 but mfg0 already on */
					ret = __mfg0_on_if_not_duplicate();
				}
			} else {
				/* smc to power on mfg0 -> reset eb -> warm boot -> eb resume */
				gpueb_log_d(GHPM_SWWA_TAG, "ghpm swwa on");
				arm_smccc_smc(
					MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
					GHPM_SWWA_SMP_OP_MFG0_ON,      /* a1 */
					0, 0, 0, 0, 0, 0, &res);
				atomic_set(&g_progress_status, POWER_ON_IN_PROGRESS);
				gpueb_log_d(GHPM_SWWA_TAG, "ghpm swwa on done");
				ret = GHPM_SUCCESS;
			}
		} else {
			gpueb_log_d(GHPM_SWWA_TAG, "no need to power on mfg0 again.");
			ret = GHPM_SUCCESS;
		}
	} else if (power == GHPM_OFF) {
		atomic_dec(&g_power_count);
		if (atomic_read(&g_power_count) == 0) {
			gpueb_log_d(GHPM_SWWA_TAG, "suspend gpueb");

			/* IPI to gpueb for suspend flow and then trigger ghpm off */
			data.event = SUSPEND_POWER_OFF;
			data.off_state = off_state;
			data.magic = GPUEB_SLEEP_IPI_MAGIC_NUMBER;
			ret = mtk_ipi_send(
				get_gpueb_ipidev(),
				g_ipi_channel,
				IPI_SEND_POLLING,
				(void *)&data,
				(sizeof(data) / g_gpueb_slot_size),
				GHPM_IPI_TIMEOUT);

			if (unlikely(ret != IPI_ACTION_DONE)) {
				gpueb_log_e(GHPM_SWWA_TAG, "OFF IPI failed, ret=%d", ret);
				gpueb_dump_status(NULL, NULL, 0);
				ret = GHPM_OFF_EB_IPI_ERR;
				goto done_unlock;
			}

			i = 0;
			while ((readl(g_gpueb_lp_state_gpr) != GPUEB_ON_SUSPEND) ||
					!is_gpueb_wfi() ) {
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					gpueb_log_e(GHPM_SWWA_TAG, "Wait EB OFF failed");
					gpueb_log_e(GHPM_SWWA_TAG, "g_gpueb_lp_state_gpr: 0x%08x",
						readl(g_gpueb_lp_state_gpr));
					gpueb_log_e(GHPM_SWWA_TAG, "is_gpueb_wfi: 0x%s",
						is_gpueb_wfi()? "true": "false");
					gpueb_dump_status(NULL, NULL, 0);
					goto done_unlock;
				}
			}
			gpueb_log_d(GHPM_SWWA_TAG, "suspend gpueb done");

			/* smc to power off mfg0 */
			gpueb_log_d(GHPM_SWWA_TAG, "mfg0 off smc");
			arm_smccc_smc(
				MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
				GHPM_SWWA_SMP_OP_MFG0_OFF,     /* a1 */
				0, 0, 0, 0, 0, 0, &res);
			atomic_set(&g_progress_status, POWER_OFF_IN_PROGRESS);
			gpueb_log_d(GHPM_SWWA_TAG, "mfg0 off smc done");
		} else {
			gpueb_log_d(GHPM_SWWA_TAG, "no need to ghpm ctrl off");
		}
		ret = GHPM_SUCCESS;
	} else {
		gpueb_log_e(GHPM_SWWA_TAG, "Invalid power=%d", power);
		ret = GHPM_INPUT_ERR;
	}

done_unlock:
	gpueb_log_d(GHPM_SWWA_TAG, "EXIT, power=%d, g_progress_status=%d, g_power_count=%d",
		power, atomic_read(&g_progress_status), atomic_read(&g_power_count));

	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);

	if (ret != GHPM_SUCCESS) {
		__dump_ghpm_info();
		__dump_mfg_pwr_sta();
		gpueb_dump_status(NULL, NULL, 0);
		BUG_ON(1);
	}

	return ret;
}

static int __wait_gpueb(enum gpueb_low_power_event event)
{
	unsigned int i;

	raw_spin_lock_irqsave(&ghpm_lock, g_pwr_irq_flags);
	gpueb_log_d(GHPM_SWWA_TAG, "Entry, event=%d, g_progress_status=%d, g_power_count=%d",
		event, atomic_read(&g_progress_status), atomic_read(&g_power_count));

	if (event == SUSPEND_POWER_ON) {
		if (atomic_read(&g_progress_status) == POWER_ON_IN_PROGRESS) {
			i = 0;
			while ((readl(g_gpueb_lp_state_gpr) != GPUEB_ON_RESUME)) {
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					gpueb_log_e(GHPM_SWWA_TAG, "g_gpueb_lp_state_gpr: 0x%08x",
						readl(g_gpueb_lp_state_gpr));
					goto wait_err;
				}
			}
			gpueb_timesync_update();
			atomic_set(&g_progress_status, NOT_IN_PROGRESS);
			gpueb_log_d(GHPM_SWWA_TAG, "GPUEB resume done, i=%u", i);
		} else if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
			gpueb_log_e(GHPM_SWWA_TAG, "wrong call order by user, no on before off");
			goto wait_err;
		} else if (atomic_read(&g_progress_status) == NOT_IN_PROGRESS) {
			gpueb_log_d(GHPM_SWWA_TAG, "no need to wait");
		} else {
			gpueb_log_e(GHPM_SWWA_TAG, "g_progress_status=%d unexpected!",
				atomic_read(&g_progress_status));
			goto wait_err;
		}
	} else if (event == SUSPEND_POWER_OFF) {
		if (atomic_read(&g_progress_status) == POWER_OFF_IN_PROGRESS) {
			i = 0;
			while (mfg0_pwr_sta() == MFG0_PWR_ON) {
				udelay(1);
				if (++i > GPUEB_WAIT_TIMEOUT) {
					gpueb_log_e(GHPM_SWWA_TAG, "MFG0_PWR_CON: 0x%08x",
						get_mfg0_pwr_con());
					goto wait_err;
				}
			}
			atomic_set(&g_progress_status, NOT_IN_PROGRESS);
			gpueb_log_d(GHPM_SWWA_TAG, "GPUEB suspend done, i=%u", i);
		} else if (atomic_read(&g_progress_status) == POWER_ON_IN_PROGRESS) {
			gpueb_log_d(GHPM_SWWA_TAG, "on after off at once, no need to wait");
		} else if (atomic_read(&g_progress_status) == NOT_IN_PROGRESS) {
			gpueb_log_d(GHPM_SWWA_TAG, "no need to wait");
		} else {
			gpueb_log_e(GHPM_SWWA_TAG, "g_progress_status=%d unexpected!",
				atomic_read(&g_progress_status));
			goto wait_err;
		}
	}

	gpueb_log_d(GHPM_SWWA_TAG, "Exit, event=%d, g_progress_status=%d, g_power_count=%d",
		event, atomic_read(&g_progress_status), atomic_read(&g_power_count));
	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);

	return WAIT_DONE;

wait_err:
	atomic_set(&g_progress_status, NOT_IN_PROGRESS);
	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);
	gpueb_log_e(GHPM_SWWA_TAG, "Wait GPUEB timeout, event=%d", event);
	__dump_ghpm_info();
	__dump_mfg_pwr_sta();
	gpueb_dump_status(NULL, NULL, 0);
	BUG_ON(1);
	return WAIT_TIMEOUT;
}

static void __dump_ghpm_info(void)
{
	gpueb_log_d(GHPM_SWWA_TAG, "g_progress_status=%d, g_power_count=%d",
		atomic_read(&g_progress_status), atomic_read(&g_power_count));
	gpueb_log_d(GHPM_SWWA_TAG, "No need to dump ghpm hw info");
}

static bool __is_gpueb_exist(void)
{
	struct device_node *of_gpueb = NULL;

	of_gpueb = of_find_compatible_node(NULL, NULL, "mediatek,gpueb");
	if (!of_gpueb) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to find gpueb of_node");
		return false;
	}

	return true;
}

static int __ghpm_swwa_pdrv_probe(struct platform_device *pdev)
{
	struct device *ghpm_dev = &pdev->dev;
	struct resource *res = NULL;
	int ret = -ENOENT;

	gpueb_log_i(GHPM_SWWA_TAG, "start to probe ghpm swwa driver");

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
		gpueb_log_e(GHPM_SWWA_TAG, "fail to find ghpm device");
		goto done;
	}

	/* get mfg1_pwr_con */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg1_pwr_con");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to get resource mfg1_pwr_con");
		goto done;
	}
	g_mfg1_pwr_con = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg1_pwr_con)) {
		gpueb_log_i(GHPM_SWWA_TAG, "fail to ioremap mfg1_pwr_con: 0x%llx",
			(u64) res->start);
		goto done;
	}

	/* get mfg2_pwr_con */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg2_pwr_con");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to get resource mfg2_pwr_con");
		goto done;
	}
	g_mfg2_pwr_con = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg2_pwr_con)) {
		gpueb_log_i(GHPM_SWWA_TAG, "fail to ioremap mfg2_pwr_con: 0x%llx",
			(u64) res->start);
		goto done;
	}

	/* get mfg37_pwr_con */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg37_pwr_con");
	if (unlikely(!res)) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to get resource mfg37_pwr_con");
		goto done;
	}
	g_mfg37_pwr_con = devm_ioremap(ghpm_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg37_pwr_con)) {
		gpueb_log_i(GHPM_SWWA_TAG, "fail to ioremap mfg37_pwr_con: 0x%llx",
			(u64) res->start);
		goto done;
	}

	/* get GPUEB_LP_STATE_GPR address */
	g_gpueb_lp_state_gpr = gpueb_get_gpr_addr(GPUEB_SRAM_GPR10);
	if (unlikely(!g_gpueb_lp_state_gpr)) {
		gpueb_log_e(GPUEB_TAG, "fail to get GPUEB_LP_STATE_GPR (%d)", GPUEB_SRAM_GPR10);
		goto done;
	}

	g_ipi_channel = gpueb_get_send_PIN_ID_by_name("IPI_ID_SLEEP");
	if (unlikely(g_ipi_channel < 0)) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to get IPI_ID_SLEEP id");
		goto done;
	}

	ret = mtk_ipi_register(get_gpueb_ipidev(), g_ipi_channel, NULL, NULL, &msgbuf);
	if (ret != IPI_ACTION_DONE) {
		gpueb_log_e(GHPM_SWWA_TAG, "ipi register fail: id=%d, ret=%d", g_ipi_channel, ret);
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

	ghpm_register_ghpm_fp(&platform_ghpmswwa_fp);

	ret = 0;
	gpueb_log_i(GHPM_SWWA_TAG, "ghpm swwa driver probe done");
done:
	g_ghpm_ready = 1;
	return ret;
}

/* API: register ghpm swwa platform driver */
static int __init __ghpm_swwa_init(void)
{
	int ret = 0;

	gpueb_log_i(GHPM_SWWA_TAG, "start to init ghpm_swwa platform driver");

	/* register ghpm platform driver */
	ret = platform_driver_register(&g_ghpm_swwa_pdrv);
	if (ret) {
		gpueb_log_e(GHPM_SWWA_TAG, "fail to register ghpm_swwa platform driver: %d", ret);
		goto done;
	}

	gpueb_log_i(GHPM_SWWA_TAG, "ghpm swwa platform driver init done");

done:
	return ret;
}

/* API: unregister ghpm driver */
static void __exit __ghpm_swwa_exit(void)
{
	platform_driver_unregister(&g_ghpm_swwa_pdrv);
}

module_init(__ghpm_swwa_init);
module_exit(__ghpm_swwa_exit);

MODULE_DEVICE_TABLE(of, g_ghpm_swwa_of_match);
MODULE_DESCRIPTION("MediaTek GHPM SWWA platform driver");
MODULE_LICENSE("GPL");
