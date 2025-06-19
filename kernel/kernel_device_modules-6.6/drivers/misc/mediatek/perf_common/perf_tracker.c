// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/power_supply.h>

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif

#define CREATE_TRACE_POINTS
#include <perf_tracker_trace.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_sram.h>
#include <mtk_qos_share.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include <dvfsrc-exp.h>
#endif

#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
#include <mtk_cpufreq_api.h>
#endif

#include <perf_tracker.h>
#include <perf_tracker_internal.h>

#include "sugov/cpufreq.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v3/energy_model.h"
#endif
#define csram_read(offs)	\
	(IS_ERR_OR_NULL((void *)csram_base) ? 0 : __raw_readl(csram_base + (offs)))
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT) && IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static struct cpu_dsu_freq_state *freq_state;
#endif
static void fuel_gauge_handler(struct work_struct *work);
static int fuel_gauge_enable;
static int fuel_gauge_delay;
static DECLARE_DELAYED_WORK(fuel_gauge, fuel_gauge_handler);

#if IS_ENABLED(CONFIG_MTK_CHARGER)
static void charger_handler(struct work_struct *work);
static int charger_enable;
static int charger_delay;
static DECLARE_DELAYED_WORK(charger, charger_handler);
#endif


static int perf_tracker_on;
static DEFINE_MUTEX(perf_ctl_mutex);
static unsigned int check_dram_bw = 0xFFFF;

static struct mtk_btag_mictx_iostat_struct iostat;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
static struct mtk_btag_mictx_id ufs_mictx_id = {.storage = BTAG_STORAGE_UFS,
						.name = "perf_tracker"};
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
static unsigned int gpu_pmu_enable;
static unsigned int is_gpu_pmu_worked;
static unsigned int gpu_pmu_period = 8000000; //8ms
#endif
static unsigned int mcupm_freq_enable;

static int emi_last_bw_idx = 0xFFFF;

u64 get_cpu_stall(int cpu, u32 offset)
{
	u64 count = 0;
	if (perf_tracker_info_exist) {
		if (IS_ERR_OR_NULL((void *)stall_tcm_base))
			return count;
		count = __raw_readl(stall_tcm_base + offset + (cpu * 0x4));
	} else {
	#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
		return qos_sram_read(CM_STALL_RATIO_ID_0 + cpu);
	#else
		if (IS_ERR_OR_NULL((void *)csram_base))
			return count;
		count = __raw_readl(csram_base + offset + (cpu * 0x4));
	#endif
	}
	return count;
}
EXPORT_SYMBOL_GPL(get_cpu_stall);

#define OFFS_DVFS_CUR_OPP_S	0x98
#define OFFS_MCUPM_CUR_OPP_S	0x544
#define OFFS_MCUPM_CUR_FREQ_S	0x11e0		//gearless freq
//per core
#define OFFS_DVFS_CUR_OPP_S_PER_CORE	0x1380
#define OFFS_MCUPM_CUR_FREQ_S_PER_CORE	0x13A4

static unsigned int cpudvfs_get_cur_freq(int cluster_id, bool is_mcupm)
{
	u32 val = 0;
	u32 offset = 0;
	struct ppm_data *p = &cluster_ppm_info[cluster_id];

	if (IS_ERR_OR_NULL((void *)csram_base)) {
#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
		return mt_cpufreq_get_cur_freq(cluster_id);
#endif
		return 0;
	}

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	if (is_gearless_support())
		offset = OFFS_MCUPM_CUR_FREQ_S;
	else
		offset = OFFS_MCUPM_CUR_OPP_S;
#endif

	if (is_mcupm)
		val = __raw_readl(csram_base +
				(offset + (cluster_id * 0x4)));
	else
		val = __raw_readl(csram_base +
				(OFFS_DVFS_CUR_OPP_S + (cluster_id * 0x120)));

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	if (is_gearless_support())
		return val;
#endif

	if (p->init && val < p->opp_nr)
		return p->dvfs_tbl[val].frequency;

	return 0;
}

static unsigned int cpudvfs_get_cur_freq_perCore(int core_id, bool is_mcupm)
{
	u32 val = 0;
	u32 offset = 0;

	if (IS_ERR_OR_NULL((void *)csram_base))
		return 0;

	offset = OFFS_MCUPM_CUR_FREQ_S_PER_CORE;

	if (is_mcupm)
		val = __raw_readl(csram_base +
				(offset + (core_id * 0x4)));
	else
		val = __raw_readl(csram_base +
				(OFFS_DVFS_CUR_OPP_S_PER_CORE + (core_id * 0x4)));

	return val;
}

unsigned int base_offset_read(void __iomem *base, unsigned int offs)
{
	if (IS_ERR_OR_NULL((void *)base))
		return 0;
	return __raw_readl(base + (offs));
}

int perf_tracker_enable(int on)
{
	mutex_lock(&perf_ctl_mutex);
	perf_tracker_on = on;
	mutex_unlock(&perf_ctl_mutex);

	return (perf_tracker_on == on) ? 0 : -1;
}
EXPORT_SYMBOL_GPL(perf_tracker_enable);

static inline void format_sbin_data(char *buf, u32 size, u32 *sbin_data, u32 lens)
{
	char *ptr = buf;
	char *buffer_end = buf + size;
	int i;

	ptr += snprintf(ptr, buffer_end - ptr, "ARRAY[");
	for (i = 0; i < lens; i++) {
		ptr += snprintf(ptr, buffer_end - ptr, "%02x,%02x,%02x,%02x,",
				(*(sbin_data+i)) & 0xff, (*(sbin_data+i) >> 8) & 0xff,
				(*(sbin_data+i) >> 16) & 0xff, (*(sbin_data+i) >> 24) & 0xff);
	}
	ptr -= 1;
	ptr += snprintf(ptr, buffer_end - ptr, "]");
}

enum {
	SBIN_EMI_BW_RECORD		= 1U << 0,
	SBIN_U_RECORD			= 1U << 1,
	SBIN_MCUPM_RECORD		= 1U << 2,
	SBIN_PMU_RECORD			= 1U << 3,
	SBIN_U_VOTING_RECORD		= 1U << 4,
	SBIN_U_A_E_RECORD		= 1U << 5,
	SBIN_DRAM_BW_RECORD		= 1U << 6,
	SBIN_S_RECORD			= 1U << 7,
};

#define K(x) ((x) << (PAGE_SHIFT - 10))
#define max_cpus 8
#define emi_bw_hist_nums 8
#define dram_bw_hist_nums 4
#define emi_bw_record_nums 32
#define dram_bw_record_nums 16
#define u_record_nums 2
#define mcupm_record_nums 9
#define u_voting_record_nums 3
#define u_a_e_record_nums 5
#define sched_Lcpu_freq_nums 4
#define sched_freq_nums 6
#define dram_rate_nums 1
#define vcore_uv_nums 1
#define cpu_mcupm_freq_nums 6
#define PRINT_BUFFER_SIZE ((emi_bw_record_nums+u_record_nums+mcupm_record_nums \
		+u_voting_record_nums+u_a_e_record_nums+dram_bw_record_nums \
		+sched_freq_nums+dram_rate_nums+vcore_uv_nums+cpu_mcupm_freq_nums) \
		 * 12 + 8)

void perf_tracker(u64 wallclock,
		  bool hit_long_check)
{
	long mm_available = 0, mm_free = 0;
	u32 dram_rate = 0;
	struct mtk_btag_mictx_iostat_struct *iostat_ptr = &iostat;
	int emi_bw_idx = 0xFFFF, dram_bw_idx = 0xFFFF;
	bool bw_idx_checked = 0;
	u32 bw_record = 0;
	u32 sbin_data[emi_bw_record_nums+u_record_nums+mcupm_record_nums
		+u_voting_record_nums+u_a_e_record_nums
		+dram_bw_record_nums
		+sched_freq_nums+dram_rate_nums+vcore_uv_nums+cpu_mcupm_freq_nums] = {0};
	int sbin_lens = 0;
#if IS_ENABLED(CONFIG_ARM64)
	char sbin_data_print[PRINT_BUFFER_SIZE] = {0};
#else
	char *sbin_data_print = NULL;
#endif
	u32 sbin_data_ctl = 0;
	u32 u_v = 0, u_f = 0;
	u32 u_aff = 0;
	u32 u_bmoni = 0;
	u32 u_uff = 0, u_ucf = 0;
	u32 u_ecf = 0;
	u32 vcore_uv = 0;
	int i;
	int stall[max_cpus] = {0};
	unsigned int sched_freq[6] = {0};
	unsigned int cpu_mcupm_freq[6] = {0};
	int cid;

	if (!perf_tracker_on)
		return;

#if !IS_ENABLED(CONFIG_ARM64)
	sbin_data_print = kmalloc(PRINT_BUFFER_SIZE, GFP_KERNEL);
	if (!sbin_data_print)
		return;
	memset(sbin_data_print, 0, PRINT_BUFFER_SIZE);
#endif

	/* dram freq */
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	dram_rate = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_KHZ);
	dram_rate = dram_rate / 1000;
	/* vcore  */
	vcore_uv = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_UV);
#endif

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	/* emi history */
	emi_bw_idx = qos_rec_get_hist_idx();
	dram_bw_idx = emi_bw_idx;
	if (emi_bw_idx != 0xFFFF && emi_bw_idx != emi_last_bw_idx) {
		emi_last_bw_idx = emi_bw_idx;
		bw_idx_checked = 1;
		for (bw_record = 0; bw_record < emi_bw_record_nums; bw_record += 8) {
			/* occupied bw history */
			sbin_data[bw_record]   = qos_rec_get_hist_bw(emi_bw_idx, 0);
			sbin_data[bw_record+1] = qos_rec_get_hist_bw(emi_bw_idx, 1);
			sbin_data[bw_record+2] = qos_rec_get_hist_bw(emi_bw_idx, 2);
			sbin_data[bw_record+3] = qos_rec_get_hist_bw(emi_bw_idx, 3);
			/* data bw history */
			sbin_data[bw_record+4] = qos_rec_get_hist_data_bw(emi_bw_idx, 0);
			sbin_data[bw_record+5] = qos_rec_get_hist_data_bw(emi_bw_idx, 1);
			sbin_data[bw_record+6] = qos_rec_get_hist_data_bw(emi_bw_idx, 2);
			sbin_data[bw_record+7] = qos_rec_get_hist_data_bw(emi_bw_idx, 3);

			emi_bw_idx -= 1;
			if (emi_bw_idx < 0)
				emi_bw_idx = emi_bw_idx + emi_bw_hist_nums;
		}
	}
#endif
	sbin_lens += emi_bw_record_nums;
	sbin_data_ctl |= SBIN_EMI_BW_RECORD;
	/* u */
	if (cluster_nr == 2) {
		u_v = csram_read(U_VOLT_2_CLUSTER);
		u_f = csram_read(U_FREQ_2_CLUSTER);
	} else if (cluster_nr == 3) {
		u_v = csram_read(U_VOLT_3_CLUSTER);
		u_f = csram_read(U_FREQ_3_CLUSTER);
	}
	sbin_data[sbin_lens] = u_v;
	sbin_data[sbin_lens+1] = u_f;
	sbin_lens += u_record_nums;
	sbin_data_ctl |= SBIN_U_RECORD;

	/* mcupm freq */
	if (mcupm_freq_enable) {
		for (i = 0; i < mcupm_record_nums; i++)
			sbin_data[sbin_lens+i] = csram_read(
				MCUPM_OFFSET_BASE+i*4);

		sbin_lens += mcupm_record_nums;
		sbin_data_ctl |= SBIN_MCUPM_RECORD;
	}

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT) && IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	if (is_wl_support()) {
		/* get U freq. voting */
		freq_state = get_dsu_freq_state();
		for (i = 0; i < freq_state->pd_count; i++)
			sbin_data[sbin_lens+i] = (u32)freq_state->dsu_freq_vote[i];
		sbin_lens += u_voting_record_nums;
		sbin_data_ctl |= SBIN_U_VOTING_RECORD;
	}
#endif
	if (perf_tracker_info_exist) {
		/* get U A, E */
		u_aff = base_offset_read(u_tcm_base, U_AFFO);
		u_bmoni = base_offset_read(u_tcm_base, U_BMONIO);
		u_uff = csram_read(U_UFFO);
		u_ucf = csram_read(U_UCFO);
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		u_ecf = get_dsu_ceiling_freq();
#endif
		sbin_data[sbin_lens] = u_aff;
		sbin_data[sbin_lens+1] = u_bmoni;
		sbin_data[sbin_lens+2] = u_uff;
		sbin_data[sbin_lens+3] = u_ucf;
		sbin_data[sbin_lens+4] = u_ecf;
		sbin_lens += u_a_e_record_nums;
		sbin_data_ctl |= SBIN_U_A_E_RECORD;
	}

	if (check_dram_bw == 0) {
		if (dram_bw_idx != 0xFFFF && bw_idx_checked) {
			/* dram bw history */
			for (bw_record = 0; bw_record < dram_bw_record_nums; bw_record += 4) {
				/* occupied bw history */
				sbin_data[sbin_lens+bw_record]   = qos_rec_get_dramc_hist_bw(dram_bw_idx, 0);
				sbin_data[sbin_lens+bw_record+1] = qos_rec_get_dramc_hist_bw(dram_bw_idx, 1);
				sbin_data[sbin_lens+bw_record+2] = qos_rec_get_dramc_hist_bw(dram_bw_idx, 2);
				sbin_data[sbin_lens+bw_record+3] = qos_rec_get_dramc_hist_bw(dram_bw_idx, 3);

				dram_bw_idx -= 1;
				if (dram_bw_idx < 0)
					dram_bw_idx = dram_bw_idx + dram_bw_hist_nums;
			}
		}
		sbin_lens += dram_bw_record_nums;
		sbin_data_ctl |= SBIN_DRAM_BW_RECORD;
	}

	if (!is_percore) { // per cluster
		sched_freq[0] = cpudvfs_get_cur_freq(0, false);
		sched_freq[4] = cpudvfs_get_cur_freq(1, false);
		sched_freq[5] = cpudvfs_get_cur_freq(2, false);
		cpu_mcupm_freq[0] = cpudvfs_get_cur_freq(0, true);
		cpu_mcupm_freq[4] = cpudvfs_get_cur_freq(1, true);
		cpu_mcupm_freq[5] = cpudvfs_get_cur_freq(2, true);
	} else {
		for (cid = 0; cid < sched_Lcpu_freq_nums; cid++) {
			sched_freq[cid] = cpudvfs_get_cur_freq_perCore(cid, false);
			cpu_mcupm_freq[cid] = cpudvfs_get_cur_freq_perCore(cid, true);
		}
		sched_freq[4] = cpudvfs_get_cur_freq_perCore(4, false);
		sched_freq[5] = cpudvfs_get_cur_freq_perCore(7, false);
		cpu_mcupm_freq[4] = cpudvfs_get_cur_freq_perCore(4, true);
		cpu_mcupm_freq[5] = cpudvfs_get_cur_freq_perCore(7, true);
	}

	for (cid = 0; cid < sched_freq_nums; cid++)
		sbin_data[sbin_lens+cid]  = sched_freq[cid];
	sbin_lens += sched_freq_nums;
	sbin_data[sbin_lens]  = dram_rate;
	sbin_data[sbin_lens+1]  = vcore_uv;
	sbin_lens += dram_rate_nums + vcore_uv_nums;
	for (cid = 0; cid < cpu_mcupm_freq_nums; cid++)
		sbin_data[sbin_lens+cid]  = cpu_mcupm_freq[cid];
	sbin_lens += cpu_mcupm_freq_nums;
	sbin_data_ctl |= SBIN_S_RECORD;

#if IS_ENABLED(CONFIG_ARM64)
	format_sbin_data(sbin_data_print, sizeof(sbin_data_print), sbin_data, sbin_lens);
#else
	format_sbin_data(sbin_data_print, PRINT_BUFFER_SIZE, sbin_data, sbin_lens);
#endif
	trace_perf_index_sbin(sbin_data_print, sbin_lens, sbin_data_ctl);

	/* trace for short msg */
	trace_perf_index_s(
			sched_freq[0], sched_freq[4], sched_freq[5],
			dram_rate,
			vcore_uv, cpu_mcupm_freq[0], cpu_mcupm_freq[4], cpu_mcupm_freq[5]);

	if (!hit_long_check) {
		#if !IS_ENABLED(CONFIG_ARM64)
			kfree(sbin_data_print);
		#endif
		return;
	}

	/* free mem */
	mm_free = global_zone_page_state(NR_FREE_PAGES);
	mm_available = si_mem_available();

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	/* If getting I/O stat fail, fallback to zero value. */
	if (mtk_btag_mictx_get_data(ufs_mictx_id, iostat_ptr))
		memset(iostat_ptr, 0,
			sizeof(struct mtk_btag_mictx_iostat_struct));
#endif

	/* cpu stall ratio */
	for (i = 0; i < nr_cpu_ids || i < max_cpus; i++)
		stall[i] = get_cpu_stall(i,CPU_STALL_RATIO_OFFSET);

	/* trace for long msg */
	trace_perf_index_l(
			K(mm_free),
			K(mm_available),
			iostat_ptr,
			stall
			);
#if !IS_ENABLED(CONFIG_ARM64)
	kfree(sbin_data_print);
#endif
}

/*
 * make perf tracker on
 * /sys/devices/system/cpu/perf/enable
 * 1: on
 * 0: off
 */
static ssize_t show_perf_enable(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "enable = %d\n",
			perf_tracker_on);
	return len;
}

static ssize_t store_perf_enable(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t count)
{
	int val = 0;

	mutex_lock(&perf_ctl_mutex);

	if (sscanf(buf, "%iu", &val) != 0) {
		val = (val > 0) ? 1 : 0;

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
		// GPU PMU Recording
		if (val == 1 && gpu_pmu_enable && !is_gpu_pmu_worked) {
			mtk_ltr_gpu_pmu_start(gpu_pmu_period);
			is_gpu_pmu_worked = 1;
		} else if (val == 0 && is_gpu_pmu_worked) {
			mtk_ltr_gpu_pmu_stop();
			is_gpu_pmu_worked = 0;
		}
#endif
		/* do something after on/off perf_tracker */
		if (val && !perf_tracker_on) {

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
			mtk_btag_mictx_enable(&ufs_mictx_id, val);
#endif
			insert_freq_qos_hook();
			check_dram_bw = qos_rec_check_sram_ext();
			if (perf_timer_enable)
				timer_on();
			else
				passtiveTick_on();
		} else if (!val && perf_tracker_on) {
			remove_freq_qos_hook();
			if (perf_timer_enable)
				timer_off();
			else
				passtiveTick_off();

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
			mtk_btag_mictx_enable(&ufs_mictx_id, val);
#endif
		}
		perf_tracker_on = val;
	}
	mutex_unlock(&perf_ctl_mutex);

	return count;
}



static int enable_timer(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	mutex_lock(&perf_ctl_mutex);

	retval = kstrtouint(buf, 0, &val);
	if (retval)
		return -EINVAL;

	val = (val > 0) ? 1 : 0;

	if (val == 1) {
		if (!perf_timer_enable) {
			perf_timer_enable = 1;
			if (perf_tracker_on) {
				timer_on();
				passtiveTick_off();
			}
		}
	} else {
		if (perf_timer_enable) {
			perf_timer_enable = 0;
			if (perf_tracker_on) {
				timer_off();
				passtiveTick_on();
			}
		}
	}

	mutex_unlock(&perf_ctl_mutex);
	return retval;
}

static const struct kernel_param_ops enable_timer_ops = {
	.set = enable_timer,
	.get = param_get_int,
};

module_param_cb(enable_timer, &enable_timer_ops, &perf_timer_enable, 0664);
MODULE_PARM_DESC(enable_timer, "Enable or disable enable_timer");

static void fuel_gauge_handler(struct work_struct *work)
{
	int curr, volt, cap;
	struct power_supply *psy;
	union power_supply_propval val;

	if (!fuel_gauge_enable)
		return;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy = power_supply_get_by_name("mtk-fg-battery");
#else
	psy = power_supply_get_by_name("battery");
#endif
	if (psy == NULL)
		return;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	volt = val.intval;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	cap = val.intval;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	curr = val.intval;

	curr = curr/1000;
	volt = volt/1000;
	trace_fuel_gauge(curr, volt, cap);
	queue_delayed_work(system_power_efficient_wq,
			&fuel_gauge, msecs_to_jiffies(fuel_gauge_delay));
}

static ssize_t show_fuel_gauge_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "fuel_gauge_enable = %u\n",
			fuel_gauge_enable);
	return len;
}

static ssize_t store_fuel_gauge_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		fuel_gauge_enable = (tmp > 0) ? 1 : 0;

	if (fuel_gauge_enable) {
		/* default delay 8ms */
		fuel_gauge_delay = (fuel_gauge_delay > 0) ?
				fuel_gauge_delay : 8;

		/* start fuel gauge tracking */
		queue_delayed_work(system_power_efficient_wq,
				&fuel_gauge,
				msecs_to_jiffies(fuel_gauge_delay));
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_fuel_gauge_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "fuel_gauge_period = %u(ms)\n",
				fuel_gauge_delay);
	return len;
}

static ssize_t store_fuel_gauge_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		if (tmp > 0) /* ms */
			fuel_gauge_delay = tmp;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}


#if IS_ENABLED(CONFIG_MTK_CHARGER)
static void charger_handler(struct work_struct *work)
{
	int volt, temp;
	struct power_supply *psy;
	union power_supply_propval val;

	if (!charger_enable)
		return;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL)
		return;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
	volt = val.intval;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	temp = val.intval;
	trace_charger(temp, volt);

	queue_delayed_work(system_power_efficient_wq,
			&charger, msecs_to_jiffies(charger_delay));
}

static ssize_t show_charger_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "charger_enable = %u\n",
			charger_enable);
	return len;
}

static ssize_t store_charger_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		charger_enable = (tmp > 0) ? 1 : 0;

	if (charger_enable) {
		/* default delay 1000ms */
		charger_delay = (charger_delay > 0) ?
				charger_delay : 1000;

		queue_delayed_work(system_power_efficient_wq,
				&charger,
				msecs_to_jiffies(charger_delay));
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_charger_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0, max_len = 4096;

	len += snprintf(buf, max_len, "charger_period = %u(ms)\n",
				charger_delay);
	return len;
}

static ssize_t store_charger_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int tmp;

	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &tmp) == 0)
		if (tmp > 0) /* ms */
			charger_delay = tmp;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
static ssize_t show_gpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "gpu_pmu_enable = %u is_working = %u\n",
		gpu_pmu_enable, is_gpu_pmu_worked);
	return len;
}

static ssize_t store_gpu_pmu_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &gpu_pmu_enable) == 0)
		gpu_pmu_enable = (gpu_pmu_enable > 0) ? 1 : 0;

	if (perf_tracker_on && gpu_pmu_enable && !is_gpu_pmu_worked) {
		mtk_ltr_gpu_pmu_start(gpu_pmu_period);
		is_gpu_pmu_worked = 1;
	} else if (!gpu_pmu_enable && is_gpu_pmu_worked) {
		mtk_ltr_gpu_pmu_stop();
		is_gpu_pmu_worked = 0;
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

static ssize_t show_gpu_pmu_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "gpu_pmu_period = %u\n",
			gpu_pmu_period);
	return len;
}

static ssize_t store_gpu_pmu_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &gpu_pmu_period) == 0) {
		if (gpu_pmu_period < 1000000) // 1ms
			gpu_pmu_period = 1000000;
	}

	mutex_unlock(&perf_ctl_mutex);

	return count;
}
#endif

static ssize_t show_mcupm_freq_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf, max_len, "mcupm_freq_enable = %u\n", mcupm_freq_enable);
	return len;
}

static ssize_t store_mcupm_freq_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&perf_ctl_mutex);

	if (kstrtouint(buf, 10, &mcupm_freq_enable) == 0)
		mcupm_freq_enable = (mcupm_freq_enable > 0) ? 1 : 0;

	mutex_unlock(&perf_ctl_mutex);

	return count;
}

struct kobj_attribute perf_tracker_enable_attr =
__ATTR(enable, 0600, show_perf_enable, store_perf_enable);
struct kobj_attribute perf_fuel_gauge_enable_attr =
__ATTR(fuel_gauge_enable, 0600,	show_fuel_gauge_enable, store_fuel_gauge_enable);
struct kobj_attribute perf_fuel_gauge_period_attr =
__ATTR(fuel_gauge_period, 0600,	show_fuel_gauge_period, store_fuel_gauge_period);

#if IS_ENABLED(CONFIG_MTK_CHARGER)
struct kobj_attribute perf_charger_enable_attr =
__ATTR(charger_enable, 0600, show_charger_enable, store_charger_enable);
struct kobj_attribute perf_charger_period_attr =
__ATTR(charger_period, 0600, show_charger_period, store_charger_period);
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
struct kobj_attribute perf_gpu_pmu_enable_attr =
__ATTR(gpu_pmu_enable, 0600, show_gpu_pmu_enable, store_gpu_pmu_enable);
struct kobj_attribute perf_gpu_pmu_period_attr =
__ATTR(gpu_pmu_period, 0600, show_gpu_pmu_period, store_gpu_pmu_period);
#endif

struct kobj_attribute perf_mcupm_freq_enable_attr =
__ATTR(mcupm_freq_enable, 0600, show_mcupm_freq_enable, store_mcupm_freq_enable);
