// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpufreq_v2_legacy.c
 * @brief   GPU-DVFS Driver Common Wrapper
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include <gpufreq_v2_legacy.h>
#include <gpufreq_ipi.h>
#include <gpufreq_mssv.h>
#include <gpufreq_debug_legacy.h>
#include <mtk_gpu_utility.h>

#if IS_ENABLED(CONFIG_MTK_PBM)
#include <mtk_pbm_gpu_cb.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif
#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
#include <linux/soc/mediatek/devapc_public.h>
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static int gpufreq_wrapper_pdrv_probe(struct platform_device *pdev);
static void gpufreq_init_external_callback(void);
static int gpufreq_ipi_to_gpueb(struct gpufreq_ipi_data data);
static int gpufreq_validate_target(unsigned int *target);
static void gpufreq_dump_infra_status_no_lock(char *log_buf, int *log_len, int log_size);
static void gpufreq_dump_dvfs_status(char *log_buf, int *log_len, int log_size);
static void gpufreq_dump_power_tracker_status(void);
static void gpufreq_abort(void);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_wrapper_of_match[] = {
	{ .compatible = "mediatek,gpufreq_wrapper" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_wrapper_pdrv = {
	.probe = gpufreq_wrapper_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq_wrapper",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_wrapper_of_match,
	},
};

static int g_ipi_channel;
static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_bringup;
static unsigned int g_ipi_magic;
static struct gpufreq_shared_status *g_shared_status;
static phys_addr_t g_shared_mem_pa;
static unsigned int g_shared_mem_size;
static struct gpufreq_platform_fp *gpufreq_fp;
static struct gpuppm_platform_fp *gpuppm_fp;
static struct gpufreq_ipi_data g_recv_msg;
static unsigned long g_ipi_irq_flags;
static unsigned long g_pwr_irq_flags;
static raw_spinlock_t gpufreq_ipi_lock;
static DEFINE_MUTEX(gpufreq_power_lock);

/**
 * ===============================================
 * External Function Callback
 * ===============================================
 */
unsigned long (*ged_get_last_commit_idx_fp)(void);
EXPORT_SYMBOL(ged_get_last_commit_idx_fp);
unsigned long (*ged_get_last_commit_top_idx_fp)(void);
EXPORT_SYMBOL(ged_get_last_commit_top_idx_fp);
unsigned long (*ged_get_last_commit_stack_idx_fp)(void);
EXPORT_SYMBOL(ged_get_last_commit_stack_idx_fp);
#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
static bool gpufreq_devapc_vio_callback(void);
struct devapc_power_callbacks devapc_cb_gpu = {
	.type = DEVAPC_TYPE_GPU,
	.query_power = gpufreq_devapc_vio_callback,
};
struct devapc_power_callbacks devapc_cb_gpu1 = {
	.type = DEVAPC_TYPE_GPU1,
	.query_power = gpufreq_devapc_vio_callback,
};
#endif /* CONFIG_DEVAPC_ARCH_MULTI */

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/***********************************************************************************
 * Function Name      : gpufreq_bringup
 * Inputs             : -
 * Outputs            : -
 * Returns            : status - Current status of bring-up
 * Description        : Check GPUFREQ bring-up status
 *                      If it's bring-up status, GPUFREQ is out-of-function
 ***********************************************************************************/
unsigned int gpufreq_bringup(void)
{
	return g_gpufreq_bringup;
}
EXPORT_SYMBOL(gpufreq_bringup);

/***********************************************************************************
 * Function Name      : gpufreq_power_ctrl_enable
 * Inputs             : -
 * Outputs            : -
 * Returns            : status - Current status of power control
 * Description        : Check whether power control of GPU HW is enable
 *                      If it's disabled, GPU HW is always power-on
 ***********************************************************************************/
unsigned int gpufreq_power_ctrl_enable(void)
{
	unsigned int power_control = false;

	/* implement on EB */
	if (g_shared_status)
		power_control = g_shared_status->power_control;
	/* implement on AP */
	else {
		if (gpufreq_fp && gpufreq_fp->power_ctrl_enable)
			power_control = gpufreq_fp->power_ctrl_enable();
		else
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

	return power_control;
}
EXPORT_SYMBOL(gpufreq_power_ctrl_enable);

/***********************************************************************************
 * Function Name      : gpufreq_active_sleep_ctrl_enable
 * Inputs             : -
 * Outputs            : -
 * Returns            : status - Current status of active-idle control
 * Description        : Check whether active-idle control of GPU HW is enable
 *                      If it's disabled, GPU HW is always active
 ***********************************************************************************/
unsigned int gpufreq_active_sleep_ctrl_enable(void)
{
	unsigned int active_sleep_control = false;

	if (g_shared_status)
		active_sleep_control = g_shared_status->active_sleep_control;
	else
		GPUFREQ_LOGE("null gpufreq shared memory (ENOENT)");

	return active_sleep_control;
}
EXPORT_SYMBOL(gpufreq_active_sleep_ctrl_enable);

/***********************************************************************************
 * Function Name      : gpufreq_get_power_state
 * Inputs             : -
 * Outputs            : -
 * Returns            : state - Current status of GPU power
 * Description        : Get current power state
 ***********************************************************************************/
unsigned int gpufreq_get_power_state(void)
{
	enum gpufreq_power_state power_state = GPU_PWR_OFF;
	int power_count = 0, active_count = 0;

	/* implement on EB */
	if (g_shared_status) {
		power_count = g_shared_status->power_count;
		power_state = power_count > 0 ? GPU_PWR_ON : GPU_PWR_OFF;
	/* implement on AP */
	} else {
		if (gpufreq_fp && gpufreq_fp->get_power_state)
			power_state = gpufreq_fp->get_power_state();
		else
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

	return power_state;
}
EXPORT_SYMBOL(gpufreq_get_power_state);

/***********************************************************************************
 * Function Name      : gpufreq_get_dvfs_state
 * Inputs             : -
 * Outputs            : -
 * Returns            : state - Current status of GPU DVFS
 * Description        : Get current DVFS state
 *                      If it isn't DVFS_FREE, then DVFS is fixed in some state
 ***********************************************************************************/
unsigned int gpufreq_get_dvfs_state(void)
{
	enum gpufreq_dvfs_state dvfs_state = DVFS_DISABLE;

	/* implement on EB */
	if (g_shared_status)
		dvfs_state = g_shared_status->dvfs_state;
	/* implement on AP */
	else {
		if (gpufreq_fp && gpufreq_fp->get_dvfs_state)
			dvfs_state = gpufreq_fp->get_dvfs_state();
		else
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

	return dvfs_state;
}
EXPORT_SYMBOL(gpufreq_get_dvfs_state);

/***********************************************************************************
 * Function Name      : gpufreq_get_shader_present
 * Inputs             : -
 * Outputs            : -
 * Returns            : shader_present - Current shader cores
 * Description        : Get GPU shader cores
 *                      This is for Mali GPU DDK to control power domain of shader cores
 ***********************************************************************************/
unsigned int gpufreq_get_shader_present(void)
{
	unsigned int shader_present = 0;

	/* implement on EB */
	if (g_shared_status)
		shader_present = g_shared_status->shader_present;
	/* implement on AP */
	else {
		if (gpufreq_fp && gpufreq_fp->get_shader_present)
			shader_present = gpufreq_fp->get_shader_present();
		else
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

	return shader_present;
}
EXPORT_SYMBOL(gpufreq_get_shader_present);

/***********************************************************************************
 * Function Name      : gpufreq_get_segment_id
 * Inputs             : -
 * Outputs            : -
 * Returns            : segment_id - Segment of GPU
 * Description        : Get GPU segment ID
 ***********************************************************************************/
unsigned int gpufreq_get_segment_id(void)
{
	unsigned int segment_id = 0;

	if (g_shared_status)
		segment_id = g_shared_status->segment_id;
	else
		GPUFREQ_LOGE("null gpufreq shared memory (ENOENT)");

	return segment_id;
}
EXPORT_SYMBOL(gpufreq_get_segment_id);

/***********************************************************************************
 * Function Name      : gpufreq_dump_infra_status
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : Dump GPU related infra status
 ***********************************************************************************/
void gpufreq_dump_infra_status(void)
{
	mutex_lock(&gpufreq_power_lock);

	gpufreq_dump_infra_status_no_lock(NULL, NULL, 0);

	mutex_unlock(&gpufreq_power_lock);
}
EXPORT_SYMBOL(gpufreq_dump_infra_status);

/***********************************************************************************
 * Function Name      : gpufreq_dump_infra_status_logbuffer
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : Dump GPU related infra status and write to log buffer
 ***********************************************************************************/
void gpufreq_dump_infra_status_logbuffer(char *log_buf, int *log_len, int log_size)
{
	mutex_lock(&gpufreq_power_lock);

	gpufreq_dump_infra_status_no_lock(log_buf, log_len, log_size);

	mutex_unlock(&gpufreq_power_lock);
}
EXPORT_SYMBOL(gpufreq_dump_infra_status_logbuffer);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_freq
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : freq   - Current freq of given target
 * Description        : Query current frequency of the target
 ***********************************************************************************/
unsigned int gpufreq_get_cur_freq(enum gpufreq_target target)
{
	unsigned int freq = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		if (target == TARGET_STACK && g_shared_status)
			freq = g_shared_status->cur_fstack;
		else if (target == TARGET_GPU && g_shared_status)
			freq = g_shared_status->cur_fgpu;
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_fstack)
		freq = gpufreq_fp->get_cur_fstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_fgpu)
		freq = gpufreq_fp->get_cur_fgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	GPUFREQ_LOGD("target: %s, current freq: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		freq);

	return freq;
}
EXPORT_SYMBOL(gpufreq_get_cur_freq);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_out_freq
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : freq   - Current out freq of given target
 * Description        : Query current out frequency of the target
 ***********************************************************************************/
unsigned int gpufreq_get_cur_out_freq(enum gpufreq_target target)
{
	return gpufreq_get_cur_freq(target);
}
EXPORT_SYMBOL(gpufreq_get_cur_out_freq);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_volt
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : volt   - Current volt of given target
 * Description        : Query current voltage of the target
 ***********************************************************************************/
unsigned int gpufreq_get_cur_volt(enum gpufreq_target target)
{
	unsigned int volt = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		if (target == TARGET_STACK && g_shared_status)
			volt = g_shared_status->cur_vstack;
		else if (target == TARGET_GPU && g_shared_status)
			volt = g_shared_status->cur_vgpu;
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_vstack)
		volt = gpufreq_fp->get_cur_vstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_vgpu)
		volt = gpufreq_fp->get_cur_vgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	GPUFREQ_LOGD("target: %s, current volt: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		volt);

	return volt;
}
EXPORT_SYMBOL(gpufreq_get_cur_volt);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_vsram
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : vsram  - Current vsram volt of given target
 * Description        : Query current vsram voltage of the target
 ***********************************************************************************/
unsigned int gpufreq_get_cur_vsram(enum gpufreq_target target)
{
	unsigned int vsram = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		if (target == TARGET_STACK && g_shared_status)
			vsram = g_shared_status->cur_vsram_stack;
		else if (target == TARGET_GPU && g_shared_status)
			vsram = g_shared_status->cur_vsram_gpu;
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_vsram_stack)
		vsram = gpufreq_fp->get_cur_vsram_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_vsram_gpu)
		vsram = gpufreq_fp->get_cur_vsram_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	GPUFREQ_LOGD("target: %s, current vsram: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		vsram);

	return vsram;
}
EXPORT_SYMBOL(gpufreq_get_cur_vsram);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_power
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : power  - Current power of given target
 * Description        : Query current power of the target
 ***********************************************************************************/
unsigned int gpufreq_get_cur_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		if (target == TARGET_STACK && g_shared_status)
			power = g_shared_status->cur_power_stack;
		else if (target == TARGET_GPU && g_shared_status)
			power = g_shared_status->cur_power_gpu;
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_pstack)
		power = gpufreq_fp->get_cur_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_pgpu)
		power = gpufreq_fp->get_cur_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	GPUFREQ_LOGD("target: %s, current power: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		power);
	return power;
}
EXPORT_SYMBOL(gpufreq_get_cur_power);

/***********************************************************************************
 * Function Name      : gpufreq_get_max_power
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : power  - Max power of given target in working table
 * Description        : Query maximum power of the target
 ***********************************************************************************/
unsigned int gpufreq_get_max_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_shared_status) {
		if (target == TARGET_STACK)
			power = g_shared_status->max_power_stack;
		else if (target == TARGET_GPU)
			power = g_shared_status->max_power_gpu;
		goto done;
	}
	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_max_pstack)
		power = gpufreq_fp->get_max_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_max_pgpu)
		power = gpufreq_fp->get_max_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");


done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_max_power);

/***********************************************************************************
 * Function Name      : gpufreq_get_min_power
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : power  - Min power of given target in working table
 * Description        : Query minimum power of the target
 ***********************************************************************************/
unsigned int gpufreq_get_min_power(enum gpufreq_target target)
{
	unsigned int power = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_shared_status) {
		if (target == TARGET_STACK)
			power = g_shared_status->min_power_stack;
		else if (target == TARGET_GPU)
			power = g_shared_status->min_power_gpu;
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_min_pstack)
		power = gpufreq_fp->get_min_pstack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_min_pgpu)
		power = gpufreq_fp->get_min_pgpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_min_power);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_oppidx
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : oppidx - Current working OPP index of given target
 * Description        : Query current working OPP index of the target
 ***********************************************************************************/
int gpufreq_get_cur_oppidx(enum gpufreq_target target)
{
	int oppidx = -1;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_shared_status) {
		if (target == TARGET_STACK)
			oppidx = g_shared_status->cur_oppidx_stack;
		else if (target == TARGET_GPU)
			oppidx = g_shared_status->cur_oppidx_gpu;
		goto done;
	}
	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_cur_idx_stack)
		oppidx = gpufreq_fp->get_cur_idx_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_cur_idx_gpu)
		oppidx = gpufreq_fp->get_cur_idx_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_cur_oppidx);

/***********************************************************************************
 * Function Name      : gpufreq_get_opp_num
 * Inputs             : target  - Target of GPU DVFS (GPU, STACK, DEFAULT)
 * Outputs            : -
 * Returns            : opp_num - # of OPP index in working table of given target
 * Description        : Query number of OPP index in working table of the target
 ***********************************************************************************/
int gpufreq_get_opp_num(enum gpufreq_target target)
{
	int opp_num = -1;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_shared_status) {
		if (target == TARGET_STACK)
			opp_num = g_shared_status->opp_num_stack;
		else if (target == TARGET_GPU)
			opp_num = g_shared_status->opp_num_gpu;
		goto done;
	}
	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_opp_num_stack)
		opp_num = gpufreq_fp->get_opp_num_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_opp_num_gpu)
		opp_num = gpufreq_fp->get_opp_num_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return opp_num;
}
EXPORT_SYMBOL(gpufreq_get_opp_num);

/***********************************************************************************
 * Function Name      : gpufreq_get_freq_by_idx
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      oppidx - Worknig OPP index of the target
 * Outputs            : -
 * Returns            : freq   - Freq of given target at given working OPP index
 * Description        : Query freq of the target by working OPP index
 ***********************************************************************************/
unsigned int gpufreq_get_freq_by_idx(enum gpufreq_target target, int oppidx)
{
	struct gpufreq_ipi_data send_msg = {};
	unsigned int freq = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_GET_FREQ_BY_IDX;
		send_msg.target = target;
		send_msg.u.oppidx = oppidx;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			freq = g_recv_msg.u.freq;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_fstack_by_idx)
		freq = gpufreq_fp->get_fstack_by_idx(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_fgpu_by_idx)
		freq = gpufreq_fp->get_fgpu_by_idx(oppidx);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return freq;
}
EXPORT_SYMBOL(gpufreq_get_freq_by_idx);

/***********************************************************************************
 * Function Name      : gpufreq_get_power_by_idx
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      oppidx - Worknig OPP index of the target
 * Outputs            : -
 * Returns            : power  - Power of given target at given working OPP index
 * Description        : Query volt of the target by working OPP index
 ***********************************************************************************/
unsigned int gpufreq_get_power_by_idx(enum gpufreq_target target, int oppidx)
{
	struct gpufreq_ipi_data send_msg = {};
	unsigned int power = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_GET_POWER_BY_IDX;
		send_msg.target = target;
		send_msg.u.oppidx = oppidx;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			power = g_recv_msg.u.power;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_pstack_by_idx)
		power = gpufreq_fp->get_pstack_by_idx(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_pgpu_by_idx)
		power = gpufreq_fp->get_pgpu_by_idx(oppidx);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return power;
}
EXPORT_SYMBOL(gpufreq_get_power_by_idx);

/***********************************************************************************
 * Function Name      : gpufreq_get_oppidx_by_freq
 * Inputs             : target - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      freq   - Freq of the target
 * Outputs            : -
 * Returns            : oppidx - Working OPP index of given target that has given freq
 * Description        : Query working OPP index of the target that has the frequency
 ***********************************************************************************/
int gpufreq_get_oppidx_by_freq(enum gpufreq_target target, unsigned int freq)
{
	struct gpufreq_ipi_data send_msg = {};
	int oppidx = -1;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_GET_OPPIDX_BY_FREQ;
		send_msg.target = target;
		send_msg.u.freq = freq;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			oppidx = g_recv_msg.u.oppidx;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_idx_by_fstack)
		oppidx = gpufreq_fp->get_idx_by_fstack(freq);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_idx_by_fgpu)
		oppidx = gpufreq_fp->get_idx_by_fgpu(freq);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return oppidx;
}
EXPORT_SYMBOL(gpufreq_get_oppidx_by_freq);

/***********************************************************************************
 * Function Name      : gpufreq_get_leakage_power
 * Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      volt      - Voltage of the target
 * Outputs            : -
 * Returns            : p_leakage - Leakage power of given target computed by given volt
 * Description        : Compute leakage power of the target by the voltage
 ***********************************************************************************/
unsigned int gpufreq_get_leakage_power(enum gpufreq_target target, unsigned int volt)
{
	struct gpufreq_ipi_data send_msg = {};
	unsigned int p_leakage = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_GET_LEAKAGE_POWER;
		send_msg.target = target;
		send_msg.u.volt = volt;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			p_leakage = g_recv_msg.u.power;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_lkg_pstack)
		p_leakage = gpufreq_fp->get_lkg_pstack(volt, 30);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_lkg_pgpu)
		p_leakage = gpufreq_fp->get_lkg_pgpu(volt, 30);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return p_leakage;
}
EXPORT_SYMBOL(gpufreq_get_leakage_power);

/***********************************************************************************
 * Function Name      : gpufreq_get_dynamic_power
 * Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      freq      - Frequency of the target
 *                      volt      - Voltage of the target
 * Outputs            : -
 * Returns            : p_dynamic - Dynamic power of given target computed by given freq
 *                                  and volt
 * Description        : Compute dynamic power of the target by the frequency and voltage
 ***********************************************************************************/
unsigned int gpufreq_get_dynamic_power(enum gpufreq_target target,
	unsigned int freq, unsigned int volt)
{
	unsigned int p_dynamic = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_dyn_pstack)
		p_dynamic = gpufreq_fp->get_dyn_pstack(freq, volt);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_dyn_pgpu)
		p_dynamic = gpufreq_fp->get_dyn_pgpu(freq, volt);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return p_dynamic;
}
EXPORT_SYMBOL(gpufreq_get_dynamic_power);

/***********************************************************************************
 * Function Name      : gpufreq_power_control
 * Inputs             : power          - Target power state
 * Outputs            : -
 * Returns            : power_count    - Success
 *                      GPUFREQ_EINVAL - Failure
 *                      GPUFREQ_ENOENT - Null implementation
 * Description        : Control power state of whole MFG system
 ***********************************************************************************/
int gpufreq_power_control(enum gpufreq_power_state power)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (!gpufreq_power_ctrl_enable()) {
		GPUFREQ_LOGD("power control is disabled");
		ret = GPUFREQ_SUCCESS;
		goto done;
	}

	mutex_lock(&gpufreq_power_lock);

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_POWER_CONTROL;
		send_msg.u.power_state = power;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done_unlock;
	}

	/* implement on AP */
	if (gpufreq_fp && gpufreq_fp->power_control)
		ret = gpufreq_fp->power_control(power);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

done_unlock:
	if (unlikely(ret < 0))
		GPUFREQ_LOGE("fail to control power state: %s (%d)",
			power ? "GPU_PWR_ON" : "GPU_PWR_OFF", ret);

	mutex_unlock(&gpufreq_power_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_power_control);

/***********************************************************************************
 * Function Name      : gpufreq_active_sleep_control
 * Inputs             : power          - Target power state
 * Outputs            : -
 * Returns            : active_count   - Success
 *                      GPUFREQ_EINVAL - Failure
 *                      GPUFREQ_ENOENT - Null implementation
 * Description        : Control runtime active-idle state of GPU
 ***********************************************************************************/
int gpufreq_active_sleep_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}
EXPORT_SYMBOL(gpufreq_active_sleep_control);

/***********************************************************************************
 * Function Name      : gpufreq_commit
 * Inputs             : target          - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      oppidx          - Working OPP index of the target
 * Outputs            : -
 * Returns            : GPUFREQ_SUCCESS - Success
 *                      GPUFREQ_EINVAL  - Failure
 *                      GPUFREQ_ENOENT  - Null implementation
 * Description        : Commit DVFS request to the target with working OPP index
 *                      It will be constrained by the limit recorded in GPU PPM
 ***********************************************************************************/
int gpufreq_commit(enum gpufreq_target target, int oppidx)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, oppidx=%d", target, oppidx);

	ret = gpufreq_validate_target(&target);
	if (ret)
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_COMMIT;
		send_msg.target = target;
		send_msg.u.oppidx = oppidx;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (gpuppm_fp && gpuppm_fp->limited_commit)
		ret = gpuppm_fp->limited_commit(target, oppidx);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	}

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit %s OPP index: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU", oppidx, ret);

	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_commit);

/***********************************************************************************
 * Function Name      : gpufreq_dual_commit
 * Inputs             : gpu_oppidx      - Working OPP index of GPU
 *                      stack_oppidx    - Working OPP index of STACK
 * Outputs            : -
 * Returns            : GPUFREQ_SUCCESS - Success
 *                      GPUFREQ_EINVAL  - Failure
 *                      GPUFREQ_ENOENT  - Null implementation
 * Description        : Commit DVFS request to both GPU & STACK with working OPP index
 *                      It will be constrained by the limit recorded in GPU PPM
 ***********************************************************************************/
int gpufreq_dual_commit(int gpu_oppidx, int stack_oppidx)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}
EXPORT_SYMBOL(gpufreq_dual_commit);

/***********************************************************************************
 * Function Name      : gpufreq_set_limit
 * Inputs             : target          - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      limiter         - Pre-defined user that set limit to GPU DVFS
 *                      ceiling_info    - Upper limit info (oppidx, freq, volt, power, ...)
 *                      floor_info      - Lower limit info (oppidx, freq, volt, power, ...)
 * Outputs            : -
 * Returns            : GPUFREQ_SUCCESS - Success
 *                      GPUFREQ_EINVAL  - Failure
 *                      GPUFREQ_ENOENT  - Null implementation
 * Description        : Set ceiling and floor limit to GPU DVFS by specified limiter
 *                      It will immediately trigger DVFS if current OPP violates limit
 ***********************************************************************************/
int gpufreq_set_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int ceiling_info, int floor_info)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, limiter=%d, ceiling_info=%d, floor_info=%d",
		target, limiter, ceiling_info, floor_info);

	ret = gpufreq_validate_target(&target);
	if (ret)
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_SET_LIMIT;
		send_msg.target = target;
		send_msg.u.set_limit.limiter = limiter;
		send_msg.u.set_limit.ceiling_info = ceiling_info;
		send_msg.u.set_limit.floor_info = floor_info;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (gpuppm_fp && gpuppm_fp->set_limit)
		ret = gpuppm_fp->set_limit(target, limiter, ceiling_info, floor_info, true);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	}

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set %s limiter: %d, ceiling_info: %d, floor_info: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			limiter, ceiling_info, floor_info, ret);

	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpufreq_set_limit);

/***********************************************************************************
 * Function Name      : gpufreq_set_limit_by_power
 * Inputs             : target          - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      limiter         - Pre-defined user that set limit to GPU DVFS
 *                      ceiling_info    - Upper limit power info
 *                      floor_info      - Lower limit power info
 * Outputs            : -
 * Returns            : GPUFREQ_SUCCESS - Success
 *                      GPUFREQ_EINVAL  - Failure
 *                      GPUFREQ_ENOENT  - Null implementation
 * Description        : Set ceiling and floor limit to GPU DVFS by specified limiter
 *                      It will immediately trigger DVFS if current OPP violates limit
 ***********************************************************************************/
int gpufreq_set_limit_by_power(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int ceiling_info, int floor_info)
{
	int ceiling_idx = -1, floor_idx = -1;
	int ceiling_freq = GPUPPM_KEEP_IDX, floor_freq = GPUPPM_KEEP_IDX;
	int ret = GPUFREQ_SUCCESS;

	//update the power table with current temp.
	if (gpufreq_fp && gpufreq_fp->update_power_table)
		gpufreq_fp->update_power_table();

	//use the power info to get freq
	if (ceiling_info > 0) {
		if (gpufreq_fp && gpufreq_fp->get_idx_by_pgpu)
			ceiling_idx = gpufreq_fp->get_idx_by_pgpu((unsigned int)ceiling_info);

		if (gpufreq_fp && gpufreq_fp->get_fgpu_by_idx)
			ceiling_freq =  gpufreq_fp->get_fgpu_by_idx(ceiling_idx);

	} else {
		//if info is gpuppm_reserved_idx, just pass to set_limit.
		ceiling_freq = ceiling_info;
	}
	//use the power info to get freq
	if (floor_info > 0) {
		if (gpufreq_fp && gpufreq_fp->get_idx_by_pgpu)
			floor_idx = gpufreq_fp->get_idx_by_pgpu((unsigned int)floor_info);

		if (gpufreq_fp && gpufreq_fp->get_fgpu_by_idx)
			floor_freq =  gpufreq_fp->get_fgpu_by_idx(floor_idx);

	} else {
		//if info is gpuppm_reserved_idx, just pass to set_limit.
		floor_freq = floor_info;
	}
	//pass the freq info to set_limit.
	ret = gpufreq_set_limit(target, limiter, ceiling_freq, floor_freq);
	return ret;
}
EXPORT_SYMBOL(gpufreq_set_limit_by_power);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_limit_idx
 * Inputs             : target    - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      limit     - Type of limit (GPUPPM_CEILING, GPUPPM_FLOOR)
 * Outputs            : -
 * Returns            : limit_idx - Working OPP index of ceiling or floor
 * Description        : Query current working OPP index of ceiling or floor limit
 ***********************************************************************************/
int gpufreq_get_cur_limit_idx(enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	int limit_idx = -1;

	if (gpufreq_validate_target(&target))
		goto done;

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	/* implement on EB */
	if (g_shared_status) {
		if (limit == GPUPPM_CEILING)
			limit_idx = g_shared_status->cur_ceiling;
		else if (limit == GPUPPM_FLOOR)
			limit_idx = g_shared_status->cur_floor;
		goto done;
	}

	/* implement on AP */
	if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_ceiling)
	//todo limit_idx = gpuppm_fp->get_ceiling(target);
		limit_idx = gpuppm_fp->get_ceiling();
	else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_floor)
	//todo limit_idx = gpuppm_fp->get_floor(target);
		limit_idx = gpuppm_fp->get_floor();
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

done:
	return limit_idx;
}
EXPORT_SYMBOL(gpufreq_get_cur_limit_idx);

/***********************************************************************************
 * Function Name      : gpufreq_get_cur_limiter
 * Inputs             : target  - Target of GPU DVFS (GPU, STACK, DEFAULT)
 *                      limit   - Type of limit (GPUPPM_CEILING, GPUPPM_FLOOR)
 * Outputs            : -
 * Returns            : limiter - Pre-defined user that set limit to GPU DVFS
 * Description        : Query which user decide current ceiling and floor OPP index
 ***********************************************************************************/
unsigned int gpufreq_get_cur_limiter(enum gpufreq_target target, enum gpuppm_limit_type limit)
{
	struct gpufreq_shared_status *shared_status = NULL;
	unsigned int limiter = 0;

	if (gpufreq_validate_target(&target))
		goto done;

	if (limit >= GPUPPM_INVALID || limit < 0) {
		GPUFREQ_LOGE("invalid limit target: %d (EINVAL)", limit);
		goto done;
	}

	/* implement on EB */
	//todo : need refactor later
#if GPUFREQ_TODO
	if (g_gpueb_support) {
		shared_status = (struct gpufreq_shared_status *)(uintptr_t)g_status_shared_mem_va;

		if (target == TARGET_STACK && shared_status) {
			if (limit == GPUPPM_CEILING)
				limiter = shared_status->cur_c_limiter_stack;
			else if (limit == GPUPPM_FLOOR)
				limiter = shared_status->cur_f_limiter_stack;
		} else if (target == TARGET_GPU && shared_status) {
			if (limit == GPUPPM_CEILING)
				limiter = shared_status->cur_c_limiter_gpu;
			else if (limit == GPUPPM_FLOOR)
				limiter = shared_status->cur_f_limiter_gpu;
		}
		goto done;
	}
#endif

	/* implement on AP */
	if (limit == GPUPPM_CEILING && gpuppm_fp && gpuppm_fp->get_c_limiter)
	//todo limiter = gpuppm_fp->get_c_limiter(target);
		limiter = gpuppm_fp->get_c_limiter();
	else if (limit == GPUPPM_FLOOR && gpuppm_fp && gpuppm_fp->get_f_limiter)
	//todo limiter = gpuppm_fp->get_f_limiter(target);
		limiter = gpuppm_fp->get_f_limiter();
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

done:
	GPUFREQ_LOGD("target: %s, current %s limiter: %d",
		target == TARGET_STACK ? "STACK" : "GPU",
		limit == GPUPPM_CEILING ? "ceiling" : "floor",
		limiter);
	return limiter;
}
EXPORT_SYMBOL(gpufreq_get_cur_limiter);

/***********************************************************************************
 * Function Name      : gpufreq_get_core_mask_table
 * Inputs             : -
 * Outputs            : -
 * Returns            : core_mask_table  - Pointer to array of core mask structure
 * Description        : Get core mask table
 *                      This is for DCS to scale the number of shader cores
 ***********************************************************************************/
struct gpufreq_core_mask_info *gpufreq_get_core_mask_table(void)
{
	struct gpufreq_core_mask_info *core_mask_table = NULL;

	/* implement only on AP */
	if (gpufreq_fp && gpufreq_fp->get_core_mask_table)
		core_mask_table = gpufreq_fp->get_core_mask_table();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	return core_mask_table;
}
EXPORT_SYMBOL(gpufreq_get_core_mask_table);

/***********************************************************************************
 * Function Name      : gpufreq_get_core_num
 * Inputs             : -
 * Outputs            : -
 * Returns            : core_num  - # of GPU Shader cores
 * Description        : Get number of GPU shader cores
 *                      This is for DCS to scale the number of shader cores
 ***********************************************************************************/
unsigned int gpufreq_get_core_num(void)
{
	unsigned int core_num = 0;

	/* implement only on AP */
	if (gpufreq_fp && gpufreq_fp->get_core_num)
		core_num = gpufreq_fp->get_core_num();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

	return core_num;
}
EXPORT_SYMBOL(gpufreq_get_core_num);

/***********************************************************************************
 * Function Name      : gpufreq_pdca_config
 * Inputs             : power          - Target power state
 * Outputs            : -
 * Returns            : -
 * Description        : Manually control PDCA setting
 ***********************************************************************************/
void gpufreq_pdca_config(enum gpufreq_power_state power)
{

}
EXPORT_SYMBOL(gpufreq_pdca_config);

/***********************************************************************************
 * Function Name      : gpufreq_fake_mtcmos_control
 * Inputs             : power          - Target power state
 * Outputs            : -
 * Returns            : -
 * Description        : Fake PWR_CON value of SPM MFG register
 ***********************************************************************************/
void gpufreq_fake_mtcmos_control(enum gpufreq_power_state power)
{
	if (power == GPU_PWR_ON)
		gpufreq_set_mfgsys_config(CONFIG_FAKE_MTCMOS_CTRL, FEAT_ENABLE);
	else if (power == GPU_PWR_OFF)
		gpufreq_set_mfgsys_config(CONFIG_FAKE_MTCMOS_CTRL, FEAT_DISABLE);
}
EXPORT_SYMBOL(gpufreq_fake_mtcmos_control);

/***********************************************************************************
 * Function Name      : gpufreq_update_debug_opp_info
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_update_debug_opp_info(void)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_get_working_table
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
const struct gpufreq_opp_info *gpufreq_get_working_table(enum gpufreq_target target)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
#if GPUFREQ_TODO
	if (g_gpueb_support) {
		send_msg.cmd_id = CMD_GET_WORKING_TABLE;
		send_msg.target = target;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		if (!ret)
			opp_table = (struct gpufreq_opp_info *)(uintptr_t)g_debug_shared_mem_va;
		goto done;
	}
#endif

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_working_table_stack)
		opp_table = gpufreq_fp->get_working_table_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_working_table_gpu)
		opp_table = gpufreq_fp->get_working_table_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return opp_table;
}
EXPORT_SYMBOL(gpufreq_get_working_table);

/***********************************************************************************
 * Function Name      : gpufreq_get_signed_table
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
const struct gpufreq_opp_info *gpufreq_get_signed_table(enum gpufreq_target target)
{
	struct gpufreq_ipi_data send_msg = {};
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;

	if (gpufreq_validate_target(&target))
		goto done;

	/* implement on EB */
#if GPUFREQ_TODO
	if (g_gpueb_support) {
		send_msg.cmd_id = CMD_GET_SIGNED_TABLE;
		send_msg.target = target;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		if (!ret)
			opp_table = (struct gpufreq_opp_info *)(uintptr_t)g_debug_shared_mem_va;
		goto done;
	}
#endif

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_signed_table_stack)
		opp_table = gpufreq_fp->get_signed_table_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_signed_table_gpu)
		opp_table = gpufreq_fp->get_signed_table_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return opp_table;
}

/***********************************************************************************
 * Function Name      : gpufreq_set_stress_test
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_set_stress_test(unsigned int mode)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

#if GPUFREQ_TODO
	/* implement on EB */
	if (g_gpueb_support) {
		send_msg.cmd_id = CMD_SET_STRESS_TEST;
		send_msg.u.mode = mode;

		ret = gpufreq_ipi_to_gpueb(send_msg);
	/* implement on AP */

	} else {
#endif
		if (gpufreq_fp && gpufreq_fp->set_stress_test)
			gpufreq_fp->set_stress_test(mode);
		else {
			ret = GPUFREQ_ENOENT;
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		}
#if GPUFREQ_TODO
	}
#endif

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_get_limit_table
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
const struct gpuppm_limit_info *gpufreq_get_limit_table(enum gpufreq_target target)
{
	struct gpufreq_ipi_data send_msg = {};
	const struct gpuppm_limit_info *limit_table = NULL;
	int ret = GPUFREQ_SUCCESS;

	if (gpufreq_validate_target(&target))
		goto done;

#if GPUFREQ_TODO
	/* implement on EB */
	if (g_gpueb_support) {
		send_msg.cmd_id = CMD_GET_LIMIT_TABLE;
		send_msg.target = target;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		if (!ret)
			limit_table = (struct gpuppm_limit_info *)(uintptr_t)g_debug_shared_mem_va;
		goto done;
	}
#endif
	/* implement on AP */
	if (gpuppm_fp && gpuppm_fp->get_limit_table)
		limit_table = gpuppm_fp->get_limit_table(target);
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

done:
	return limit_table;
}

/***********************************************************************************
 * Function Name      : gpufreq_switch_limit
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_switch_limit(enum gpufreq_target target,
	enum gpuppm_limiter limiter, int c_enable, int f_enable)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	ret = gpufreq_validate_target(&target);
	if (ret)
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_SWITCH_LIMIT;
		send_msg.target = target;
		send_msg.u.set_limit.limiter = limiter;
		send_msg.u.set_limit.ceiling_info = c_enable;
		send_msg.u.set_limit.floor_info = f_enable;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (gpuppm_fp && gpuppm_fp->switch_limit)
		ret = gpuppm_fp->switch_limit(target, limiter, c_enable, f_enable, true);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");
	}

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to switch %s limiter: %d, c_enable: %d, f_enable: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			limiter, c_enable, f_enable, ret);

	GPUFREQ_TRACE_END();

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_fix_target_oppidx
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_fix_target_oppidx(enum gpufreq_target target, int oppidx)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, oppidx=%d", target, oppidx);

	ret = gpufreq_validate_target(&target);
	if (ret)
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_FIX_TARGET_OPPIDX;
		send_msg.target = target;
		send_msg.u.oppidx = oppidx;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->fix_target_oppidx_stack)
		ret = gpufreq_fp->fix_target_oppidx_stack(oppidx);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->fix_target_oppidx_gpu)
		ret = gpufreq_fp->fix_target_oppidx_gpu(oppidx);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to fix %s OPP index: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU", oppidx, ret);

	GPUFREQ_TRACE_END();

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_fix_dual_target_oppidx
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_fix_dual_target_oppidx(int gpu_oppidx, int stack_oppidx)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_fix_custom_freq_volt
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_fix_custom_freq_volt(enum gpufreq_target target,
	unsigned int freq, unsigned int volt)
{
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("target=%d, freq=%d, volt=%d", target, freq, volt);

	ret = gpufreq_validate_target(&target);
	if (ret)
		goto done;

	/* implement on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_FIX_CUSTOM_FREQ_VOLT;
		send_msg.target = target;
		send_msg.u.custom.freq = freq;
		send_msg.u.custom.volt = volt;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
		goto done;
	}

	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->fix_custom_freq_volt_stack)
		ret = gpufreq_fp->fix_custom_freq_volt_stack(freq, volt);
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->fix_custom_freq_volt_gpu)
		ret = gpufreq_fp->fix_custom_freq_volt_gpu(freq, volt);
	else {
		ret = GPUFREQ_ENOENT;
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
	}

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to fix %s F(%d)/V(%d) (%d)",
			target == TARGET_STACK ? "STACK" : "GPU", freq, volt, ret);

	GPUFREQ_TRACE_END();

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_fix_dual_custom_freq_volt
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_fix_dual_custom_freq_volt(unsigned int fgpu, unsigned int vgpu,
	unsigned int fstack, unsigned int vstack)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_set_mfgsys_config
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
int gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}
EXPORT_SYMBOL(gpufreq_set_mfgsys_config);

/***********************************************************************************
 * Function Name      : gpufreq_mssv_commit
 * Description        : Only for GPUFREQ MSSV test purpose
 ***********************************************************************************/
int gpufreq_mssv_commit(unsigned int target, unsigned int val)
{
#if GPUFREQ_MSSV_TEST_MODE
	struct gpufreq_ipi_data send_msg = {};
	int ret = GPUFREQ_SUCCESS;

	/* implement only on EB */
	if (g_gpueb_support) {
		raw_spin_lock_irqsave(&gpufreq_ipi_lock, g_ipi_irq_flags);
		send_msg.cmd_id = CMD_MSSV_COMMIT;
		send_msg.u.mssv.target = target;
		send_msg.u.mssv.val = val;

		if (!gpufreq_ipi_to_gpueb(send_msg))
			ret = g_recv_msg.u.return_value;
		else
			ret = GPUFREQ_EINVAL;
		raw_spin_unlock_irqrestore(&gpufreq_ipi_lock, g_ipi_irq_flags);
	/* implement on AP */
	} else {
		if (gpufreq_fp && gpufreq_fp->mssv_commit)
			ret = gpufreq_fp->mssv_commit(target, val);
		else {
			ret = GPUFREQ_ENOENT;
			GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		}
	}

	return ret;
#else
	GPUFREQ_UNREFERENCED(target);
	GPUFREQ_UNREFERENCED(val);

	return GPUFREQ_EINVAL;
#endif /* GPUFREQ_MSSV_TEST_MODE */
}

/***********************************************************************************
 * Function Name      : gpufreq_ipi_to_gpueb
 * Description        : Only for GPUFREQ internal IPI purpose
 ***********************************************************************************/
static int gpufreq_ipi_to_gpueb(struct gpufreq_ipi_data data)
{
	int ret = GPUFREQ_SUCCESS;

	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_validate_target
 * Description        : Validate gpufreq target and re-assign default target
 ***********************************************************************************/
static int gpufreq_validate_target(unsigned int *target)
{
	if (*target >= TARGET_INVALID || (*target == TARGET_STACK && !g_dual_buck)) {
		GPUFREQ_LOGE("invalid OPP target: %d (EINVAL)", *target);
		return GPUFREQ_EINVAL;
	}

	if (*target == TARGET_DEFAULT) {
		if (g_dual_buck)
			*target = TARGET_STACK;
		else
			*target = TARGET_GPU;
	}

	return GPUFREQ_SUCCESS;
}

/***********************************************************************************
 * Function Name      : gpufreq_dump_infra_status_no_lock
 * Description        : Only for GPUFREQ internal use
 ***********************************************************************************/
static void gpufreq_dump_infra_status_no_lock(char *log_buf, int *log_len, int log_size)
{
	gpufreq_dump_dvfs_status(log_buf, log_len, log_size);
	gpufreq_dump_power_tracker_status();

	/* implement on AP */
	if (gpufreq_fp && gpufreq_fp->dump_infra_status)
		gpufreq_fp->dump_infra_status(log_buf, log_len, log_size);
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
}

/***********************************************************************************
 * Function Name      : gpufreq_dump_dvfs_status
 * Description        : Dump DVFS status from shared memory
 ***********************************************************************************/
static void gpufreq_dump_dvfs_status(char *log_buf, int *log_len, int log_size)
{
	int cur_oppidx_gpu = 0, cur_oppidx_stack = 0;
	int vgpu_diff = 0, vstack_diff = 0;
	unsigned int cur_vgpu = 0, cur_vstack = 0, opp_vgpu = 0, opp_vstack = 0;
	struct gpufreq_ptp3_shared_status ptp3_status = {};

	if (g_shared_status) {
		cur_oppidx_gpu = g_shared_status->cur_oppidx_gpu;
		cur_vgpu = g_shared_status->cur_vgpu;
		if (cur_oppidx_gpu >= 0 && cur_oppidx_gpu < g_shared_status->opp_num_gpu) {
			opp_vgpu = g_shared_status->working_table_gpu[cur_oppidx_gpu].volt;
			vgpu_diff = (int)cur_vgpu - (int)opp_vgpu;
		} else
			GPUFREQ_LOGE("abnormal cur_oppidx_gpu: %d", cur_oppidx_gpu);
		cur_oppidx_stack = g_shared_status->cur_oppidx_stack;
		cur_vstack = g_shared_status->cur_vstack;
		if (cur_oppidx_stack >= 0 && cur_oppidx_stack < g_shared_status->opp_num_stack) {
			opp_vstack = g_shared_status->working_table_stack[cur_oppidx_stack].volt;
			vstack_diff = (int)cur_vstack - (int)opp_vstack;
		} else
			GPUFREQ_LOGE("abnormal cur_oppidx_stack: %d", cur_oppidx_stack);

		ptp3_status = g_shared_status->ptp3_status;
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"== [GPUFREQ DVFS STATUS: %s] ==",
			(ptp3_status.dvfs_mode == HW_DUAL_LOOP_DVFS ? "HW_LOOP" :
			(ptp3_status.dvfs_mode == SW_DUAL_LOOP_DVFS ? "SW_LOOP" : "LEGACY")));
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"GPU[%d] Freq: %d, Volt: %d (%d), Vsram: %d",
			g_shared_status->cur_oppidx_gpu, g_shared_status->cur_fgpu,
			g_shared_status->cur_vgpu, vgpu_diff,
			g_shared_status->cur_vsram_gpu);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"STACK[%d] Freq: %d, Volt: %d (%d), Vsram: %d",
			g_shared_status->cur_oppidx_stack, g_shared_status->cur_fstack,
			g_shared_status->cur_vstack, vstack_diff,
			g_shared_status->cur_vsram_stack);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"Temperature: %d'C, GPUTemperComp: %d/%d, STACKTemperComp: %d/%d",
			g_shared_status->temperature,
			g_shared_status->temper_comp_norm_gpu,
			g_shared_status->temper_comp_high_gpu,
			g_shared_status->temper_comp_norm_stack,
			g_shared_status->temper_comp_high_stack);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"Ceiling/Floor: %d/%d, Limiter: %d/%d",
			g_shared_status->cur_ceiling, g_shared_status->cur_floor,
			g_shared_status->cur_c_limiter, g_shared_status->cur_f_limiter);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"PowerCount: %d, ActiveCount: %d, Buck: %d, MTCMOS: %d, CG: %d",
			g_shared_status->power_count, g_shared_status->active_count,
			g_shared_status->buck_count, g_shared_status->mtcmos_count,
			g_shared_status->cg_count);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"InFreq: %d/%d, OutFreq: %d/%d, CC:%d/%d, FC:%d/%d",
			g_shared_status->ptp3_info.infreq0,
			g_shared_status->ptp3_info.infreq1,
			g_shared_status->ptp3_info.outfreq0,
			g_shared_status->ptp3_info.outfreq1,
			g_shared_status->ptp3_info.hw_cc,
			g_shared_status->ptp3_info.sw_cc,
			g_shared_status->ptp3_info.hw_fc,
			g_shared_status->ptp3_info.sw_fc);
		GPUFREQ_LOGB(log_buf, log_len, log_size,
			"GPU_SB_Version: 0x%04x, GPU_PTP_Version: 0x%04x",
			g_shared_status->sb_version, g_shared_status->ptp_version);
	}
}

/***********************************************************************************
 * Function Name      : gpufreq_dump_power_tracker_status
 * Description        : Dump PDC power tracker status
 ***********************************************************************************/
static void gpufreq_dump_power_tracker_status(void)
{
	/* implement on AP */
	if (gpufreq_get_power_state() == GPU_PWR_ON) {
		if (g_shared_status && g_shared_status->power_tracker_mode) {
			if (gpufreq_fp && gpufreq_fp->dump_power_tracker_status)
				gpufreq_fp->dump_power_tracker_status();
			else
				GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");
		}
	}
}

/***********************************************************************************
 * Function Name      : gpufreq_abort
 * Description        : Trigger exception when fatal error and dump infra status
 ***********************************************************************************/
static void gpufreq_abort(void)
{
	gpufreq_dump_infra_status_no_lock(NULL, NULL, 0);

	BUG_ON(1);
}

#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
static bool gpufreq_devapc_vio_callback(void)
{
	gpufreq_set_mfgsys_config(CONFIG_DEVAPC_HANDLE, CONFIG_VAL_IGNORE);

	return GPU_PWR_ON;
}
#endif /* CONFIG_DEVAPC_ARCH_MULTI */

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void gpufreq_batt_oc_callback(enum BATTERY_OC_LEVEL_TAG batt_oc_level, void *data)
{
	int ret = GPUFREQ_SUCCESS;

	ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_BATT_OC, batt_oc_level, GPUPPM_KEEP_IDX);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set LIMIT_BATT_OC limit level: %d (%d)",
			batt_oc_level, ret);
}
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void gpufreq_low_batt_callback(enum LOW_BATTERY_LEVEL_TAG low_batt_level,void *data)
{
	int ret = GPUFREQ_SUCCESS;

	ret = gpufreq_set_limit(TARGET_DEFAULT, LIMIT_LOW_BATT, low_batt_level, GPUPPM_KEEP_IDX);
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to set LIMIT_LOW_BATT limit level: %d (%d)",
			low_batt_level, ret);
}
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

static void gpufreq_init_external_callback(void)
{
#if IS_ENABLED(CONFIG_MTK_PBM)
	struct pbm_gpu_callback_table pbm_cb = {
		.get_max_pb = gpufreq_get_max_power,
		.get_min_pb = gpufreq_get_min_power,
		.get_cur_pb = gpufreq_get_cur_power,
		.get_cur_vol = gpufreq_get_cur_volt,
		.set_limit = gpufreq_set_limit,
	};
#endif /* CONFIG_MTK_PBM */

	/* hook GPU HAL callback */
	mtk_get_gpu_limit_index_fp = gpufreq_get_cur_limit_idx;
	mtk_get_gpu_limiter_fp = gpufreq_get_cur_limiter;
	mtk_get_gpu_cur_freq_fp = gpufreq_get_cur_freq;
	mtk_get_gpu_cur_oppidx_fp = gpufreq_get_cur_oppidx;

	/* register PBM callback */
//todo mark for not ready
#if IS_ENABLED(CONFIG_MTK_PBM) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6761_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6765_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6781_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6853_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6833_SUPPORT)
	register_pbm_gpu_notify(&pbm_cb);
#endif /* CONFIG_MTK_PBM */

	/* register power throttling callback */
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	register_low_battery_notify(&gpufreq_low_batt_callback, LOW_BATTERY_PRIO_GPU, NULL);
#endif /* CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	register_battery_oc_notify(&gpufreq_batt_oc_callback, BATTERY_OC_PRIO_GPU, NULL);
#endif /* CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */

//todo mark for not ready
#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)  && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6761_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6765_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6781_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6853_SUPPORT) && \
	!IS_ENABLED(CONFIG_MTK_GPU_MT6833_SUPPORT)
	register_devapc_power_callback(&devapc_cb_gpu);
	register_devapc_power_callback(&devapc_cb_gpu1);
#endif /* CONFIG_DEVAPC_ARCH_MULTI */
}

void gpufreq_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpufreq platform function pointer (EINVAL)");
		return;
	}

	gpufreq_fp = platform_fp;

	/* init shared status on AP side */
	if (gpufreq_fp && gpufreq_fp->set_shared_status)
		gpufreq_fp->set_shared_status(g_shared_status);
}
EXPORT_SYMBOL(gpufreq_register_gpufreq_fp);

void gpufreq_register_gpuppm_fp(struct gpuppm_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpuppm platform function pointer (EINVAL)");
		return;
	}

	gpuppm_fp = platform_fp;

	/* init gpufreq debug */
	/* all platform & gpuppm impl is ready after gpuppm func pointer registration */
	/* register external callback function at here */
	gpufreq_init_external_callback();

	/* init gpufreq debug */
	gpufreq_debug_init(g_dual_buck, g_gpueb_support);
#if GPUFREQ_TODO
	/* workaround to dump mt6983/mt6895 critical volt */
	if (gpufreq_fp && gpufreq_fp->get_critical_volt)
		gpufreq_fp->get_critical_volt(gpufreq_get_signed_table(TARGET_STACK));
#endif
}
EXPORT_SYMBOL(gpufreq_register_gpuppm_fp);

static int gpufreq_wrapper_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *of_wrapper = pdev->dev.of_node;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq wrapper driver");

	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq wrapper of_node, treat as bringup");
		g_gpufreq_bringup = true;
		goto done;
	}

	of_property_read_u32(of_wrapper, "dual-buck", &g_dual_buck);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &g_gpufreq_bringup);

	/* keep probe successful but do nothing when bringup */
	if (g_gpufreq_bringup) {
		GPUFREQ_LOGI("skip gpufreq wrapper driver probe when bringup");
		goto done;
	}

	/* init spinlock */
	raw_spin_lock_init(&gpufreq_ipi_lock);

	GPUFREQ_LOGI("gpufreq wrapper driver probe done, dual_buck: %s, gpueb_mode: %s",
		g_dual_buck ? "true" : "false",
		g_gpueb_support ? "on" : "off");

done:
	return ret;
}

static int __init gpufreq_wrapper_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq wrapper driver");

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_wrapper_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq wrapper driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq wrapper driver init done");

done:
	return ret;
}

/***********************************************************************************
 * Function Name      : gpufreq_get_debug_opp_info
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
struct gpufreq_debug_opp_info gpufreq_get_debug_opp_info(enum gpufreq_target target)
{
	struct gpufreq_shared_status *shared_status = NULL;
	struct gpufreq_ipi_data send_msg = {};
	struct gpufreq_debug_opp_info opp_info = {};

	if (gpufreq_validate_target(&target))
		goto done;

#if GPUFREQ_TODO
	/* implement on EB */
	if (g_gpueb_support) {
		shared_status = (struct gpufreq_shared_status *)(uintptr_t)g_status_shared_mem_va;
		send_msg.cmd_id = CMD_GET_DEBUG_OPP_INFO;
		send_msg.target = target;

		if (!gpufreq_ipi_to_gpueb(send_msg) && shared_status)
			opp_info = shared_status->opp_info;
		goto done;
	}
#endif
	/* implement on AP */
	if (target == TARGET_STACK && gpufreq_fp && gpufreq_fp->get_debug_opp_info_stack)
		opp_info = gpufreq_fp->get_debug_opp_info_stack();
	else if (target == TARGET_GPU && gpufreq_fp && gpufreq_fp->get_debug_opp_info_gpu)
		opp_info = gpufreq_fp->get_debug_opp_info_gpu();
	else
		GPUFREQ_LOGE("null gpufreq platform function pointer (ENOENT)");

done:
	return opp_info;
}

/***********************************************************************************
 * Function Name      : gpufreq_get_debug_limit_info
 * Description        : Only for GPUFREQ internal debug purpose
 ***********************************************************************************/
struct gpufreq_debug_limit_info gpufreq_get_debug_limit_info(enum gpufreq_target target)
{
	struct gpufreq_shared_status *shared_status = NULL;
	struct gpufreq_ipi_data send_msg = {};
	struct gpufreq_debug_limit_info limit_info = {};

	if (gpufreq_validate_target(&target))
		goto done;

#if GPUFREQ_TODO
	/* implement on EB */
	if (g_gpueb_support) {
		shared_status = (struct gpufreq_shared_status *)(uintptr_t)g_status_shared_mem_va;
		send_msg.cmd_id = CMD_GET_DEBUG_LIMIT_INFO;
		send_msg.target = target;

		if (!gpufreq_ipi_to_gpueb(send_msg) && shared_status)
			limit_info = shared_status->limit_info;
		goto done;
	}
#endif
	/* implement on AP */
	if (gpuppm_fp && gpuppm_fp->get_debug_limit_info)
		limit_info = gpuppm_fp->get_debug_limit_info(target);
	else
		GPUFREQ_LOGE("null gpuppm platform function pointer (ENOENT)");

done:
	return limit_info;
}

static void __exit gpufreq_wrapper_exit(void)
{
	platform_driver_unregister(&g_gpufreq_wrapper_pdrv);
}

module_init(gpufreq_wrapper_init);
module_exit(gpufreq_wrapper_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_wrapper_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS wrapper driver");
MODULE_LICENSE("GPL");
