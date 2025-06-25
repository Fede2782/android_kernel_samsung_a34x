// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ktime.h>

#include <swpm_module.h>
#include <swpm_module_psp.h>

#define SWPM_PSP_OPS (swpm_psp_m.plat_ops)

struct swpm_psp_manager {
	struct swpm_internal_ops *plat_ops;
};

static struct swpm_psp_manager swpm_psp_m;

int32_t sync_latest_data(void)
{
	int ret = SWPM_NOT_EXE;
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->cmd)
		ret = SWPM_PSP_OPS->cmd(SYNC_DATA, 0);

	return ret;
}
EXPORT_SYMBOL(sync_latest_data);

int32_t get_ddr_act_times(int32_t freq_num,
			  struct ddr_act_times *ddr_times)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->ddr_act_times_get &&
	    ddr_times != NULL)
		SWPM_PSP_OPS->ddr_act_times_get(freq_num, ddr_times);

	return 0;
}
EXPORT_SYMBOL(get_ddr_act_times);

int32_t get_ddr_sr_pd_times(struct ddr_sr_pd_times *ddr_times)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->ddr_sr_pd_times_get &&
	    ddr_times != NULL)
		SWPM_PSP_OPS->ddr_sr_pd_times_get(ddr_times);

	return 0;
}
EXPORT_SYMBOL(get_ddr_sr_pd_times);

int32_t get_ddr_data_ip_num(void)
{
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->num_get)
		return SWPM_PSP_OPS->num_get(DDR_DATA_IP);

	return 0;
}
EXPORT_SYMBOL(get_ddr_data_ip_num);

int32_t get_ddr_freq_num(void)
{
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->num_get)
		return SWPM_PSP_OPS->num_get(DDR_FREQ);

	return 0;
}
EXPORT_SYMBOL(get_ddr_freq_num);

int32_t get_ddr_freq_data_ip_stats(int32_t data_ip_num,
				   int32_t freq_num,
				   void *stats)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->ddr_freq_data_ip_stats_get &&
	    stats != NULL)
		return SWPM_PSP_OPS->ddr_freq_data_ip_stats_get(data_ip_num,
			freq_num, stats);

	return 0;
}
EXPORT_SYMBOL(get_ddr_freq_data_ip_stats);

/* deprecated, it will be removed soon */
int32_t get_vcore_ip_num(void)
{
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->num_get)
		return SWPM_PSP_OPS->num_get(CORE_IP);

	return 0;
}
EXPORT_SYMBOL(get_vcore_ip_num);

int32_t get_vcore_vol_num(void)
{
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->num_get)
		return SWPM_PSP_OPS->num_get(CORE_VOL);

	return 0;
}
EXPORT_SYMBOL(get_vcore_vol_num);

/* deprecated, it will be removed soon */
int32_t get_vcore_ip_vol_stats(int32_t ip_num,
				int32_t vol_num,
				void *stats)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->vcore_ip_vol_stats_get &&
	    stats != NULL)
		return SWPM_PSP_OPS->vcore_ip_vol_stats_get(ip_num,
			vol_num, stats);

	return 0;
}
EXPORT_SYMBOL(get_vcore_ip_vol_stats);

int32_t get_vcore_vol_duration(int32_t vol_num,
			       struct vol_duration *duration)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->vcore_vol_duration_get &&
	    duration != NULL)
		return SWPM_PSP_OPS->vcore_vol_duration_get(vol_num,
							duration);

	return 0;
}
EXPORT_SYMBOL(get_vcore_vol_duration);

int32_t get_xpu_ip_num(void)
{
	if (SWPM_PSP_OPS && SWPM_PSP_OPS->num_get)
		return SWPM_PSP_OPS->num_get(XPU_IP);

	return 0;
}
EXPORT_SYMBOL(get_xpu_ip_num);

int32_t get_xpu_ip_stats(int32_t ip_num,
			 void *stats)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->xpu_ip_stats_get &&
	    stats != NULL)
		return SWPM_PSP_OPS->xpu_ip_stats_get(ip_num,
						      stats);

	return 0;
}
EXPORT_SYMBOL(get_xpu_ip_stats);

int32_t get_res_sig_stats(struct res_sig_stats *stats)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->res_sig_stats_get &&
	    stats != NULL)
		return SWPM_PSP_OPS->res_sig_stats_get(stats);
	else
		return -1;
}
EXPORT_SYMBOL(get_res_sig_stats);

/* Get the info of a resource group
 * @*out1: main id of the resource group
 * @*out2: head id in the resource group
 * @*out3: size of the resource group
 */
int32_t get_res_group_info(uint32_t grp,
		uint32_t *out1, uint32_t *out2, uint32_t *out3)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->res_group_info_get)
		return SWPM_PSP_OPS->res_group_info_get(grp, out1, out2, out3);
	else
		return -1;
}
EXPORT_SYMBOL(get_res_group_info);

/* Get the resource group id of at most 3 target sigals
 */
int32_t get_res_group_id(uint32_t ip1, uint32_t ip2, uint32_t ip3,
		uint32_t *out1, uint32_t *out2, uint32_t *out3)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->res_group_id_get)
		return SWPM_PSP_OPS->res_group_id_get(ip1, ip2, ip3, out1, out2, out3);
	else
		return -1;
}
EXPORT_SYMBOL(get_res_group_id);

int mtk_register_swpm_ops(struct swpm_internal_ops *ops)
{
	if (!SWPM_PSP_OPS && ops)
		SWPM_PSP_OPS = ops;
	else
		return -1;

	return 0;
}
EXPORT_SYMBOL(mtk_register_swpm_ops);

int32_t get_data_record_number(uint32_t *number)
{
	if (SWPM_PSP_OPS &&
	    SWPM_PSP_OPS->data_record_number_get)
		return SWPM_PSP_OPS->data_record_number_get(number);
	else
		return -1;
}
EXPORT_SYMBOL(get_data_record_number);

