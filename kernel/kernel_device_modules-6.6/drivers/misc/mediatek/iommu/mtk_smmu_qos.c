// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 * This driver adds support for polling smmu pmu/lmu/mpam counters.
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/delay.h>

#define CREATE_TRACE_POINTS
#include "mtk_smmu_qos_events.h"

#include "arm-smmu-v3.h"
#include "mtk-smmu-v3.h"
#include "mtk-iommu-util.h"

#define US_TO_NS(usec)			((usec) * 1000)
#define SMMU_PMU_MAX_COUNTERS		64
#define SMMU_PMU_CFG_STR_SIZE		1024
#define SMMU_MPAM_MAX_COUNTERS		4
#define SMMU_MPAM_RIS_MAX		2
#define SMMU_MPAM_PARTID_MAX		64
#define SMMU_MPAM_STRBUF_SIZE		50
#define SMMU_QOS_TRACE_BUF_MAX		1024
#define POLLING_INTERVAL_DEFAULT_US	1000

struct smmu_pmu_data {
	u32 smmu_id;
	u32 txu_id;
	u32 pmu_id;
	u32 event_list[SMMU_PMU_MAX_COUNTERS];
	u32 event_len;
	u32 sid;
	u32 span;
	bool active;
	struct perf_event_attr ev_attr[SMMU_PMU_MAX_COUNTERS];
	struct perf_event *ev[SMMU_PMU_MAX_COUNTERS];
	u64 pmu_perfCurr[SMMU_PMU_MAX_COUNTERS];
	u64 pmu_perfPrev[SMMU_PMU_MAX_COUNTERS];
	int pmu_perfCntFirst[SMMU_PMU_MAX_COUNTERS];
};

struct smmu_mpam_data {
	u32 smmu_id;
	u32 txu_id;
	u32 mpam_id;
	u32 event_list[SMMU_MPAM_MAX_COUNTERS];
	u32 event_len;
	u32 partid[SMMU_MPAM_MAX_COUNTERS];
	u32 ris[SMMU_MPAM_MAX_COUNTERS];
	u32 pmg[SMMU_MPAM_MAX_COUNTERS];
	bool enable_pmg[SMMU_MPAM_MAX_COUNTERS];
	bool active;
	struct perf_event_attr ev_attr[SMMU_MPAM_MAX_COUNTERS];
	struct perf_event *ev[SMMU_MPAM_MAX_COUNTERS];
	u64 pmu_perfCurr[SMMU_MPAM_MAX_COUNTERS];
	u64 pmu_perfPrev[SMMU_MPAM_MAX_COUNTERS];
	int pmu_perfCntFirst[SMMU_MPAM_MAX_COUNTERS];
};

struct smmu_lmu_data {
	u32 smmu_id;
	u32 txu_id;
	u32 mon_id;
	u32 mon_mask;
	bool active;
	void __iomem *wp_base;
};

static u32 ftrace_ena;
static u32 ftrace_interval;
static u32 ftrace_polling;
static struct mutex pmu_list_lock;
static struct smmu_pmu_data smmu_pmu_list[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX + 1] = {0};
static struct smmu_lmu_data smmu_lmu_list[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX + 1] = {0};
static struct smmu_mpam_data smmu_mpam_list[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX + 1] = {0};
static bool power_state[SMMU_TYPE_NUM];
static bool need_poll[SMMU_TYPE_NUM];
static char trace_buf[SMMU_QOS_TRACE_BUF_MAX];

static struct hrtimer hr_timer;
static ktime_t ktime;

static void reset_pmu_data_list(u32 smmu_id);
static void reset_lmu_data_list(u32 smmu_id);
static void reset_mpam_data_list(u32 smmu_id);
static void reset_all_data_list(void);
static void dump_pmu_data_list(u32 smmu_id);
static void dump_lmu_data_list(u32 smmu_id);
static void dump_mpam_data_list(u32 smmu_id);
static int parse_pmu_entry_list(char *inputstr, int **outputArr,
				int num_group, int *group_len);
static int parse_event_list(char *input, int *pmu_list, int **event_arr,
			    int *group_lens, int num_group);
static int update_pmu_id_list(int **pmu_arr, int num_group, int *groupLengths);
static void update_pmu_event_list(int *pmu_list, int num_pmu,
				  int **event_arr, int *group_len);
static int update_pmu_param_list(int **param_arr, int num_group,
				 int *group_len);
static int smmu_qos_pmu_set(const char *val, const struct kernel_param *kp);
static int smmu_qos_pmu_get(char *buffer, const struct kernel_param *kp);
static int smmu_qos_event_set(const char *val, const struct kernel_param *kp);
static int smmu_qos_lmu_set(const char *val, const struct kernel_param *kp);
static int update_lmu_param_list(int **param_arr, int num_group,
				 int *group_len);
static int update_mpam_id_list(int **mpam_arr, int num_group, int *group_len);
static int update_mpam_param_list(int **mpam_arr, int num_group,
				  int *group_len);
static enum hrtimer_restart smmu_trace_hrtimer_cb(struct hrtimer *timer);
static void smmu_lmu_polling(u32 smmu_type);
static void smmu_pmu_polling(u32 smmu_type);
static void smmu_mpam_polling(u32 smmu_type);
static void smmu_qos_print_trace(void (*trace)(struct va_format *),
				 const char *fmt, ...);

static inline unsigned int smmu_read_reg(void __iomem *base,
					 unsigned int offset)
{
	return readl_relaxed(base + offset);
}

static inline void smmu_write_field(void __iomem *base,
				    unsigned int reg,
				    unsigned int mask,
				    unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(base + reg);
	regval = (regval & (~mask))|val;
	writel_relaxed(regval, base + reg);
}

static inline void smmu_write_reg(void __iomem *base,
				  unsigned int offset,
				  unsigned int val)
{
	writel_relaxed(val, base + offset);
}

static const char *get_pmu_event_name(u32 event_id, bool is_tcu)
{
	if (is_tcu) {
		switch (event_id) {
		case 0:
			return "cycles";
		case 1:
			return "transaction";
		case 2:
			return "tlb_miss";
		case 3:
			return "trans_walk_new_cc";
		case 4:
			return "trans_table_walk_access";
		case 5:
			return "config_struct_access";
		case 6:
			return "pcie_ats_trans_rq";
		case 7:
			return "pcie_ats_trans_passed";
		case 0x80:
			return "s1_l0_wc_read";
		case 0x81:
			return "s1_l0_wc_miss";
		case 0x82:
			return "s1_l1_wc_read";
		case 0x83:
			return "s1_l1_wc_miss";
		case 0x84:
			return "s1_l2_wc_read";
		case 0x85:
			return "s1_l2_wc_miss";
		case 0x86:
			return "s1_l3_wc_read";
		case 0x87:
			return "s1_l3_wc_miss";
		case 0x88:
			return "s2_l0_wc_read";
		case 0x89:
			return "s2_l0_wc_miss";
		case 0x8a:
			return "s2_l1_wc_read";
		case 0x8b:
			return "s2_l1_wc_miss";
		case 0x8c:
			return "s2_l2_wc_read";
		case 0x8d:
			return "s2_l2_wc_miss";
		case 0x8e:
			return "s2_l3_wc_read";
		case 0x8f:
			return "s2_l3_wc_miss";
		case 0x90:
			return "wc_read";
		case 0x91:
			return "bufferd_translation";
		case 0x92:
			return "cc_lookup";
		case 0x93:
			return "cc_read";
		case 0x94:
			return "cc_miss";
		case 0xa0:
			return "speculative_trans";
		default:
			return "";
		}
	} else {
		switch (event_id) {
		case 0:
			return "cycles";
		case 1:
			return "transaction";
		case 2:
			return "tlb_miss";
		case 3:
			return "config_cache_miss";
		case 4:
			return "trans_table_walk_access";
		case 5:
			return "config_struct_access";
		case 6:
			return "pcie_ats_trans_rq";
		case 7:
			return "pcie_ats_trans_passed";
		case 0x80:
			return "main_tlb_lookup";
		case 0x81:
			return "main_tlb_miss";
		case 0x82:
			return "main_tlb_read";
		case 0x83:
			return "micro_tlb_lookup";
		case 0x84:
			return "micro_tlb_miss";
		case 0x85:
			return "slots_full";
		case 0x86:
			return "out_of_trans_tokens";
		case 0x87:
			return "write_data_buffer_full";
		case 0x8b:
			return "dcmo_downgrade";
		case 0x8c:
			return "stash_fail";
		case 0xd0:
			return "lti_port_slots_full_0";
		case 0xd1:
			return "lti_port_slots_full_1";
		case 0xd2:
			return "lti_port_slots_full_2";
		case 0xd3:
			return "lti_port_slots_full_3";
		case 0xd4:
			return "lti_port_slots_full_4";
		case 0xd5:
			return "lti_port_slots_full_5";
		case 0xd6:
			return "lti_port_slots_full_6";
		case 0xd7:
			return "lti_port_slots_full_7";
		case 0xe0:
			return "lti_port_out_of_trans_token_0";
		case 0xe1:
			return "lti_port_out_of_trans_token_1";
		case 0xe2:
			return "lti_port_out_of_trans_token_2";
		case 0xe3:
			return "lti_port_out_of_trans_token_3";
		case 0xe4:
			return "lti_port_out_of_trans_token_4";
		case 0xe5:
			return "lti_port_out_of_trans_token_5";
		case 0xe6:
			return "lti_port_out_of_trans_token_6";
		case 0xe7:
			return "lti_port_out_of_trans_token_7";
		default:
			return "";
		}
	}
}

static const char *get_mpam_event_name(u32 event_id, u32 ris, u32 partid, bool is_tcu)
{
	static char buf[SMMU_MPAM_STRBUF_SIZE];
	int ret;

	switch (event_id) {
	case 0:
		if (is_tcu)
			ret = snprintf(buf, SMMU_MPAM_STRBUF_SIZE, "%s_partid_%d",
				 ris == 0 ? "table_walk_cache" : "config_cache",
				 partid);
		else
			ret = snprintf(buf, SMMU_MPAM_STRBUF_SIZE, "main_tlb_cache_partid_%d",
				 partid);
		if (ret < 0 || ret >= sizeof(buf))
			return "";
		else
			return buf;
	default:
		return "";
	}
}

static void (*pmu_tcu_trace_func[SMMU_TYPE_NUM])(struct va_format *) = {
	trace_mm_smmu__tcu_pmu,
	trace_apu_smmu__tcu_pmu,
	trace_soc_smmu__tcu_pmu,
	trace_gpu_smmu__tcu_pmu
};

static void (*pmu_tbu_trace_func[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX])(struct va_format *) = {
	{
	trace_mm_smmu__tbu0_pmu,
	trace_mm_smmu__tbu1_pmu,
	trace_mm_smmu__tbu2_pmu,
	trace_mm_smmu__tbu3_pmu,
	},
	{
	trace_apu_smmu__tbu0_pmu,
	trace_apu_smmu__tbu1_pmu,
	trace_apu_smmu__tbu2_pmu,
	trace_apu_smmu__tbu3_pmu,
	},
	{
	trace_soc_smmu__tbu0_pmu,
	trace_soc_smmu__tbu1_pmu,
	trace_soc_smmu__tbu2_pmu,
	NULL
	},
	{
	trace_gpu_smmu__tbu0_pmu,
	trace_gpu_smmu__tbu1_pmu,
	trace_gpu_smmu__tbu2_pmu,
	trace_gpu_smmu__tbu3_pmu,
	}
};

static void (*lmu_tcu_trace_func[SMMU_TYPE_NUM])(struct va_format *) = {
	trace_mm_smmu__tcu_lmu,
	trace_apu_smmu__tcu_lmu,
	trace_soc_smmu__tcu_lmu,
	trace_gpu_smmu__tcu_lmu
};

static void (*lmu_tbu_trace_func[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX])(struct va_format *) = {
	{
	trace_mm_smmu__tbu0_lmu,
	trace_mm_smmu__tbu1_lmu,
	trace_mm_smmu__tbu2_lmu,
	trace_mm_smmu__tbu3_lmu,
	},
	{
	trace_apu_smmu__tbu0_lmu,
	trace_apu_smmu__tbu1_lmu,
	trace_apu_smmu__tbu2_lmu,
	trace_apu_smmu__tbu3_lmu,
	},
	{
	trace_soc_smmu__tbu0_lmu,
	trace_soc_smmu__tbu1_lmu,
	trace_soc_smmu__tbu2_lmu,
	NULL
	},
	{
	trace_gpu_smmu__tbu0_lmu,
	trace_gpu_smmu__tbu1_lmu,
	trace_gpu_smmu__tbu2_lmu,
	trace_gpu_smmu__tbu3_lmu,
	}
};

static void (*mpam_tcu_trace_func[SMMU_TYPE_NUM])(struct va_format *) = {
	trace_mm_smmu__tcu_mpam,
	trace_apu_smmu__tcu_mpam,
	trace_soc_smmu__tcu_mpam,
	trace_gpu_smmu__tcu_mpam
};

static void (*mpam_tbu_trace_func[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX])(struct va_format *) = {
	{
	trace_mm_smmu__tbu0_mpam,
	trace_mm_smmu__tbu1_mpam,
	trace_mm_smmu__tbu2_mpam,
	trace_mm_smmu__tbu3_mpam,
	},
	{
	trace_apu_smmu__tbu0_mpam,
	trace_apu_smmu__tbu1_mpam,
	trace_apu_smmu__tbu2_mpam,
	trace_apu_smmu__tbu3_mpam,
	},
	{
	trace_soc_smmu__tbu0_mpam,
	trace_soc_smmu__tbu1_mpam,
	trace_soc_smmu__tbu2_mpam,
	NULL
	},
	{
	trace_gpu_smmu__tbu0_mpam,
	trace_gpu_smmu__tbu1_mpam,
	trace_gpu_smmu__tbu2_mpam,
	trace_gpu_smmu__tbu3_mpam,
	}
};

static void dummy_handler(struct perf_event *event,
			  struct perf_sample_data *data,
			  struct pt_regs *regs)
{
}

static void dump_pmu_data_list(u32 smmu_id)
{
	int j, k;
	struct smmu_pmu_data *pmu_data;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		pmu_data = &smmu_pmu_list[smmu_id][j];
		if (pmu_data->pmu_id == U32_MAX)
			continue;

		pr_info("%s, smmu:%d txu:%d pmu:%d sid:%d span:%d active:%d\n",
			__func__,
			pmu_data->smmu_id, pmu_data->txu_id,
			pmu_data->pmu_id, pmu_data->sid,
			pmu_data->span, pmu_data->active);

		for (k = 0; k < pmu_data->event_len; k++)
			pr_info("%s, ev[%d]=%d\n",
				__func__, k, pmu_data->event_list[k]);
	}
}

static void dump_mpam_data_list(u32 smmu_id)
{
	struct smmu_mpam_data *mpam_data;
	int j, k;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		mpam_data = &smmu_mpam_list[smmu_id][j];
		if (mpam_data->mpam_id == U32_MAX)
			continue;

		pr_info("%s, smmu:%d txu:%d mpam:%d active:%d\n",
			__func__,
			mpam_data->smmu_id, mpam_data->txu_id,
			mpam_data->mpam_id, mpam_data->active);

		for (k = 0; k < mpam_data->event_len; k++) {
			pr_info("%s, ev[%d]=%d, ris=%d, pid=%d\n",
				__func__, k,
				mpam_data->event_list[k],
				mpam_data->ris[k],
				mpam_data->partid[k]);
		}
	}
}

static void dump_lmu_data_list(u32 smmu_id)
{
	int j;
	struct smmu_lmu_data *lmu_data;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		lmu_data = &smmu_lmu_list[smmu_id][j];
		if (!lmu_data->active)
			continue;

		pr_info("%s, smmu:%d txu:%d active:%d axid:%d mask:%d\n",
			__func__,
			lmu_data->smmu_id, lmu_data->txu_id,
			lmu_data->active, lmu_data->mon_id,
			lmu_data->mon_mask);
	}
}

static struct perf_event *create_perf_event(struct perf_event_attr *ev_attr)
{
	struct perf_event *ev;

	ev = perf_event_create_kernel_counter(ev_attr, 0, NULL,
					      dummy_handler, NULL);
	if (!ev || IS_ERR(ev)) {
		pr_info("%s config:%llu, config1:%llu, type:%d failed %ld\n",
			__func__,
			ev_attr->config, ev_attr->config1,
			ev_attr->type, PTR_ERR(ev));
		return NULL;
	}
	if (ev->state != PERF_EVENT_STATE_ACTIVE) {
		perf_event_release_kernel(ev);
		pr_info("%s config:%llu, config1:%llu, type:%d state=%d err\n",
			__func__,
			ev_attr->config, ev_attr->config1,
			ev_attr->type, ev->state);
		return NULL;
	}
	return ev;
}

static void update_pmu_perf_event_config(u32 smmu_id, u32 txu_id, u32 event_idx)
{
	struct smmu_pmu_data *pmu_data;
	struct perf_event_attr *ev_attr;
	u32 sid, span;

	pmu_data = &smmu_pmu_list[smmu_id][txu_id];

	ev_attr = &pmu_data->ev_attr[event_idx];
	/* read param and set */
	sid = pmu_data->sid;
	span = pmu_data->span;
	if (sid == U32_MAX)
		return;

	ev_attr->config1 = 0;
	ev_attr->config1 |= sid;
	ev_attr->config1 |= ((u64)span << 32);
	ev_attr->config1 |= (1ULL << 33);
}

static void create_pmu_perf_events(int smmu_id)
{
	struct smmu_pmu_data *pmu_data;
	struct perf_event_attr *ev_attr;
	struct perf_event *ev;
	int j, k, size;

	size = sizeof(struct perf_event_attr);
	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		pmu_data = &smmu_pmu_list[smmu_id][j];
		if (pmu_data->pmu_id == U32_MAX ||
		    pmu_data->event_len == 0 ||
		    pmu_data->event_len >= SMMU_PMU_MAX_COUNTERS)
			continue;
		for (k = 0; k < pmu_data->event_len; k++) {
			ev_attr = &pmu_data->ev_attr[k];
			ev_attr->config = pmu_data->event_list[k];
			ev_attr->config1 = 0;
			/* smmu uses global filter*/
			update_pmu_perf_event_config(smmu_id, j, k);
			ev_attr->type = pmu_data->pmu_id;
			ev_attr->size = size;
			ev_attr->sample_period = 0;
			ev_attr->pinned = 1;

			ev = create_perf_event(ev_attr);
			if (ev) {
				pmu_data->ev[k] = ev;
				perf_event_enable(ev);
				pmu_data->pmu_perfCntFirst[k] = 1;
				pmu_data->pmu_perfCurr[k] = 0;
				pmu_data->pmu_perfPrev[k] = 0;
			}
		}
	}
}

static void destroy_pmu_perf_event(int smmu_id)
{
	struct smmu_pmu_data *pmu_data;
	struct perf_event *ev;
	int j, k;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		pmu_data = &smmu_pmu_list[smmu_id][j];
		if (pmu_data->pmu_id == U32_MAX ||
		    pmu_data->event_len == 0 ||
		    pmu_data->event_len >= SMMU_PMU_MAX_COUNTERS)
			continue;
		for (k = 0; k < pmu_data->event_len; k++) {
			ev = pmu_data->ev[k];
			if (ev) {
				perf_event_disable(ev);
				perf_event_release_kernel(ev);
				pmu_data->ev[k] = NULL;
			}
		}
	}
}

static int smmu_perf_event_read_local(struct perf_event *event, u64 *value)
{
	unsigned long flags;
	int ret = 0;

	/*
	 * Disabling interrupts avoids all counter scheduling (context
	 * switches, timer based rotation and IPIs).
	 */
	local_irq_save(flags);

	event->pmu->read(event);
	*value = local64_read(&event->count);

	local_irq_restore(flags);

	return ret;
}

static void smmu_pmu_polling(u32 smmu_type)
{
	struct smmu_pmu_data *pmu_data;
	struct perf_event *ev;
	u64 value, delta, delta_trans = 0, delta_mrate;
	int offset, ret;
	u32 len, j, k;

	for (j = 0; j < SMMU_TBU_CNT(smmu_type) + 1; j++) {
		memset(trace_buf, 0, SMMU_QOS_TRACE_BUF_MAX);
		offset = 0;
		len = sizeof(trace_buf);
		pmu_data = &smmu_pmu_list[smmu_type][j];
		if (!pmu_data->active)
			continue;
		for (k = 0; k < pmu_data->event_len; k++) {
			ev = pmu_data->ev[k];
			if (!ev || ev->state != PERF_EVENT_STATE_ACTIVE)
				continue;
			ret = smmu_perf_event_read_local(ev, &value);
			if (ret < 0) {
				pr_info("%s, pmu_id:%d, ev:%d, read err=%d\n",
					__func__, pmu_data->pmu_id,
					pmu_data->event_list[k], ret);
				continue;
			}
			pmu_data->pmu_perfCurr[k] = value;
			delta = (pmu_data->pmu_perfCurr[k] -
				 pmu_data->pmu_perfPrev[k]);
			if (j > 0 && pmu_data->event_list[k] == 1)
				delta_trans = delta;
			pmu_data->pmu_perfPrev[k] = pmu_data->pmu_perfCurr[k];
			if (pmu_data->pmu_perfCntFirst[k] == 1) {
				/* omit the first counter */
				pmu_data->pmu_perfCntFirst[k] = 0;
				continue;
			}
			if (k > 0) {
				ret = snprintf(trace_buf + offset,
					       len - offset,
					       ", ");
				if (ret >= (len - offset) || ret < 0) {
					pr_info("%s k=%d, snprintf err:%d",
						__func__, k, ret);
					return;
				}
				offset += ret;
				if (offset >= len) {
					pr_info("%s overflow, pmu:%d, k:%d\n",
						__func__, pmu_data->pmu_id, k);
					return;
				}
			}
			ret = snprintf(trace_buf + offset,
				       len - offset,
				       "%s=%lld",
				       get_pmu_event_name(pmu_data->event_list[k],
							  j > 0 ? false : true),
				       delta);
			if (ret >= (len - offset) || ret < 0) {
				pr_info("%s, snprintf err:%d", __func__, ret);
				return;
			}
			offset += ret;

			if (j > 0 && pmu_data->event_list[k] == 2) {
				if (delta_trans > 0 && delta > 0) {
					delta_mrate = delta * 100;
					do_div(delta_mrate, delta_trans);
				} else {
					delta_mrate = 0;
				}
				ret = snprintf(trace_buf + offset,
					       len - offset,
					       ", miss_rate_pct=%lld",
					       delta_mrate);
				if (ret >= (len - offset) || ret < 0) {
					pr_info("%s, snprintf err:%d", __func__, ret);
					return;
				}
				offset += ret;
			}
		}
		if (offset == 0)
			continue;
		if (j == 0)
			smmu_qos_print_trace(pmu_tcu_trace_func[smmu_type],
					     trace_buf);
		else
			smmu_qos_print_trace(pmu_tbu_trace_func[smmu_type][j - 1],
					     trace_buf);
	}
}

static void smmu_lmu_apply_filter(u32 smmu_type)
{
	struct smmu_lmu_data *lmu_data;
	void __iomem *wp_base;
	u32 axid, mask;
	int i;

	lmu_data = &smmu_lmu_list[smmu_type][0];
	wp_base = lmu_data->wp_base;
	axid = lmu_data->mon_id;
	mask = lmu_data->mon_mask;
	smmu_write_field(wp_base,
			 SMMUWP_TCU_CTL8,
			 TCU_MON_ID,
			 FIELD_PREP(TCU_MON_ID, axid));
	smmu_write_field(wp_base,
			 SMMUWP_TCU_CTL8,
			 TCU_MON_ID_MASK,
			 FIELD_PREP(TCU_MON_ID_MASK, mask));
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		lmu_data = &smmu_lmu_list[smmu_type][i + 1];
		axid = lmu_data->mon_id;
		mask = lmu_data->mon_mask;

		smmu_write_field(wp_base, SMMUWP_TBUx_CTL4(i),
				 CTL4_RLAT_MON_ID,
				 FIELD_PREP(CTL4_RLAT_MON_ID,
					    axid));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL6(i),
				 CTL6_RLAT_MON_ID_MASK,
				 FIELD_PREP(CTL6_RLAT_MON_ID_MASK,
					    mask));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL5(i),
				 CTL5_WLAT_MON_ID,
				 FIELD_PREP(CTL5_WLAT_MON_ID,
					    axid));
		smmu_write_field(wp_base, SMMUWP_TBUx_CTL7(i),
				 CTL7_WLAT_MON_ID_MASK,
				 FIELD_PREP(CTL7_WLAT_MON_ID_MASK,
					    mask));
	}
}

static inline void smmu_lmu_counter_enable(u32 smmu_type)
{
	struct smmu_lmu_data *lmu_data;
	void __iomem *wp_base;

	lmu_data = &smmu_lmu_list[smmu_type][0];
	wp_base = lmu_data->wp_base;

	smmu_write_field(wp_base, SMMUWP_LMU_CTL0,
			 CTL0_LAT_MON_START,
			 FIELD_PREP(CTL0_LAT_MON_START, 1));
}

static inline void smmu_lmu_counter_disable(u32 smmu_type)
{
	struct smmu_lmu_data *lmu_data;
	void __iomem *wp_base;

	lmu_data = &smmu_lmu_list[smmu_type][0];
	wp_base = lmu_data->wp_base;
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0,
			 CTL0_LAT_MON_START,
			 FIELD_PREP(CTL0_LAT_MON_START, 0));
}

static void smmu_lmu_reset_event_filter(u32 smmu_type)
{
	struct smmu_lmu_data *lmu_data;
	void __iomem *wp_base;
	int i;

	lmu_data = &smmu_lmu_list[smmu_type][0];
	wp_base = lmu_data->wp_base;

	smmu_write_field(wp_base, SMMUWP_GLB_CTL4, CTL4_LAT_SPEC,
			 FIELD_PREP(CTL4_LAT_SPEC, 0));

	/* disable tcu monitor and clear axid/mask filter */
	smmu_write_reg(wp_base, SMMUWP_TCU_CTL8, 0);

	/* disable all tbu monitor and clear axid/mask filter */
	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL4(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL5(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL6(i), 0);
		smmu_write_reg(wp_base, SMMUWP_TBUx_CTL7(i), 0);
	}
}

static void start_smmu_lmu(int smmu_id)
{
	smmu_lmu_apply_filter(smmu_id);
	smmu_lmu_counter_enable(smmu_id);
}

static void stop_smmu_lmu(int smmu_id)
{
	smmu_lmu_counter_disable(smmu_id);
	smmu_lmu_reset_event_filter(smmu_id);
}

static void smmu_lmu_polling(u32 smmu_type)
{
	u32 tcu_lat_max, tcu_pend_max, tcu_lat_avg, tcu_trans_tot, tcu_lat_tot;
	u32 tcu_oos_trans_tot, tbu_rlat_max, tbu_wlat_max, tbu_id_rlat_max;
	u32 tbu_id_wlat_max, tbu_rpend_max, tbu_wpend_max, tbu_rlat_tot;
	u32 tbu_wlat_tot, tbu_rtrans_tot, tbu_wtrans_tot, tbu_roos_trans_tot;
	u32 tbu_woos_trans_tot, tbu_avg_rlat, tbu_avg_wlat, tbu_r_buf_fullness;
	u32 tbu_w_buf_fullness, regval, i;
	u32 tbu_awostd_s, tbu_awostd_m, tbu_arostd_s, tbu_arostd_m;
	u32 tbu_wostd_s, tbu_wostd_m;
	struct smmu_lmu_data *lmu_data;
	void __iomem *wp_base;
	int written, ret;

	lmu_data = &smmu_lmu_list[smmu_type][0];
	wp_base = lmu_data->wp_base;

	if (!lmu_data->active)
		return;

	/* reset counter, restart recording */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 1);

	/* tcu max read latency and max latency pended by emi */
	regval = smmu_read_reg(wp_base, SMMUWP_TCU_MON1);
	tcu_lat_max = FIELD_GET(TCU_LAT_MAX, regval);
	tcu_pend_max = FIELD_GET(TCU_PEND_MAX, regval);

	/* tcu sum of latency of total read cmds */
	tcu_lat_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON2);

	/* tcu total read cmd count */
	tcu_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON3);

	/* tcu totoal cmd count whose latency exceed latency spec */
	tcu_oos_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON4);

	/* tcu average read cmd latency */
	tcu_lat_avg = tcu_trans_tot > 0 ?
		      (tcu_lat_tot / tcu_trans_tot) : 0;

	memset(trace_buf, 0, SMMU_QOS_TRACE_BUF_MAX);
	ret = snprintf(trace_buf, sizeof(trace_buf),
		       "lat_max=%u, pend_max=%u, trans_tot=%u, lat_avg=%u, oos_trans_tot=%u",
		       tcu_lat_max, tcu_pend_max,
		       tcu_trans_tot, tcu_lat_avg,
		       tcu_oos_trans_tot);
	if (ret >= 0 || ret < sizeof(trace_buf))
		smmu_qos_print_trace(lmu_tcu_trace_func[smmu_type], trace_buf);

	for (i = 0; i < SMMU_TBU_CNT(smmu_type); i++) {
		lmu_data = &smmu_lmu_list[smmu_type][i + 1];
		/* max latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON1(i));
		tbu_rlat_max = FIELD_GET(MON1_RLAT_MAX, regval);
		tbu_wlat_max = FIELD_GET(MON1_WLAT_MAX, regval);

		/* axid of max read latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON2(i));
		tbu_id_rlat_max = FIELD_GET(MON2_ID_RLAT_MAX, regval);

		/* axid of max write latency */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON3(i));
		tbu_id_wlat_max = FIELD_GET(MON3_ID_WLAT_MAX, regval);

		/* read or write max pended latency by emi */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON4(i));
		tbu_rpend_max = FIELD_GET(MON4_RPEND_MAX, regval);
		tbu_wpend_max = FIELD_GET(MON4_WPEND_MAX, regval);

		/* sum of latency of read total cmds */
		tbu_rlat_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON5(i));

		/* sum of latency of write total cmds */
		tbu_wlat_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON6(i));

		/* total read cmds count */
		tbu_rtrans_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON7(i));

		/* total write cmds count */
		tbu_wtrans_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON8(i));

		/* totoal read cmds whose latecny exceed latency spec */
		tbu_roos_trans_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON9(i));

		/* totoal write cmds whose latecny exceed latency spec */
		tbu_woos_trans_tot = smmu_read_reg(wp_base, SMMUWP_TBUx_MON10(i));

		/* average read cmd latency */
		tbu_avg_rlat = tbu_rtrans_tot > 0 ?
			       (tbu_rlat_tot / tbu_rtrans_tot) : 0;

		/* average write cmd latency */
		tbu_avg_wlat = tbu_wtrans_tot > 0 ?
			       (tbu_wlat_tot / tbu_wtrans_tot) : 0;

		/* read and write buf fullness */
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG1(i));
		tbu_awostd_s = FIELD_GET(DBG1_AWOSTD_S, regval);
		tbu_awostd_m = FIELD_GET(DBG1_AWOSTD_M, regval);
		tbu_w_buf_fullness = tbu_awostd_s - tbu_awostd_m;
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG2(i));
		tbu_arostd_s = FIELD_GET(DBG2_AROSTD_S, regval);
		tbu_arostd_m = FIELD_GET(DBG2_AROSTD_M, regval);
		tbu_r_buf_fullness = tbu_arostd_s - tbu_arostd_m;

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG3(i));
		tbu_wostd_s = FIELD_GET(DBG3_WOSTD_S, regval);
		tbu_wostd_m = FIELD_GET(DBG3_WOSTD_M, regval);

		written = 0;
		memset(trace_buf, 0, SMMU_QOS_TRACE_BUF_MAX);
		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "r_lat_max=%u, id_rlat_max=%u, r_trans_tot=%u, ",
			       tbu_rlat_max, tbu_id_rlat_max, tbu_rtrans_tot);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;

		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "r_lat_avg=%u, w_lat_max=%u, id_wlat_max=%u, ",
			       tbu_avg_rlat, tbu_wlat_max, tbu_id_wlat_max);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;

		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "w_trans_tot=%u, w_lat_avg=%u, r_pend_max=%u, ",
			       tbu_wtrans_tot, tbu_avg_wlat, tbu_rpend_max);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;

		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "w_pend_max=%u, r_buf_fullness=%u, w_buf_fullness=%d, ",
			       tbu_wpend_max, tbu_r_buf_fullness, tbu_w_buf_fullness);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;
		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "roos_trans_tot=%u, woos_trans_tot=%u, ",
			       tbu_roos_trans_tot, tbu_woos_trans_tot);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;
		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "awostd_s=%u, awostd_m=%u, arostd_s=%u, arostd_m=%u, ",
			       tbu_awostd_s, tbu_awostd_m, tbu_arostd_s, tbu_arostd_m);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;
		ret = snprintf(trace_buf + written, sizeof(trace_buf) - written,
			       "wostd_s=%u, wostd_m=%u", tbu_wostd_s, tbu_wostd_m);
		if (ret < 0 || ret >= sizeof(trace_buf) - written) {
			WARN_ON_ONCE(1);
			continue;
		}
		written += ret;

		smmu_qos_print_trace(lmu_tbu_trace_func[smmu_type][i],
				     trace_buf);
	}
}

static void update_mpam_perf_event_config(u32 smmu_id, u32 txu_id,
					  u32 event_idx)
{
	struct smmu_mpam_data *mpam_data;
	struct perf_event_attr *ev_attr;
	u32 ris, partid, pmg;

	mpam_data = &smmu_mpam_list[smmu_id][txu_id];

	ev_attr = &mpam_data->ev_attr[event_idx];
	/* read param and set */
	ris = mpam_data->ris[event_idx];
	partid = mpam_data->partid[event_idx];
	pmg = mpam_data->pmg[event_idx];

	ev_attr->config1 = partid & 0xFFFF;
	ev_attr->config1 |= ((u64)(pmg & 0xFFFF) << 16);
	ev_attr->config1 |= ((u64)(ris & 0xFFFF) << 32);
	ev_attr->config1 |= (1ULL << 48);
}

static void create_mpam_perf_events(int smmu_id)
{
	struct smmu_mpam_data *mpam_data;
	struct perf_event_attr *ev_attr;
	struct perf_event *ev;
	int j, k, size;

	size = sizeof(struct perf_event_attr);
	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		mpam_data = &smmu_mpam_list[smmu_id][j];
		if (mpam_data->mpam_id == U32_MAX ||
		    mpam_data->event_len == 0 ||
		    mpam_data->event_len >= SMMU_MPAM_MAX_COUNTERS)
			continue;
		for (k = 0; k < mpam_data->event_len; k++) {
			ev_attr = &mpam_data->ev_attr[k];
			ev_attr->config = mpam_data->event_list[k];
			ev_attr->config1 = 0;
			/* smmu uses global filter*/
			update_mpam_perf_event_config(smmu_id, j, k);
			ev_attr->type = mpam_data->mpam_id;
			ev_attr->size = size;
			ev_attr->sample_period = 0;
			ev_attr->pinned = 1;

			ev = create_perf_event(ev_attr);
			if (ev) {
				mpam_data->ev[k] = ev;
				perf_event_enable(ev);
				mpam_data->pmu_perfCntFirst[k] = 1;
				mpam_data->pmu_perfCurr[k] = 0;
				mpam_data->pmu_perfPrev[k] = 0;
			}
		}
	}
}

static void destroy_mpam_perf_event(int smmu_id)
{
	struct smmu_mpam_data *mpam_data;
	struct perf_event *ev;
	int j, k;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		mpam_data = &smmu_mpam_list[smmu_id][j];
		if (mpam_data->mpam_id == U32_MAX ||
		    mpam_data->event_len == 0 ||
		    mpam_data->event_len >= SMMU_MPAM_MAX_COUNTERS)
			continue;
		for (k = 0; k < mpam_data->event_len; k++) {
			ev = mpam_data->ev[k];
			if (ev) {
				perf_event_disable(ev);
				perf_event_release_kernel(ev);
				mpam_data->ev[k] = NULL;
			}
		}
	}
}

static void smmu_mpam_polling(u32 smmu_type)
{
	struct smmu_mpam_data *mpam_data;
	struct perf_event *ev;
	int ret, offset;
	u64 delta, value;
	u32 j, k;

	for (j = 0; j < SMMU_TBU_CNT(smmu_type) + 1; j++) {
		memset(trace_buf, 0, SMMU_QOS_TRACE_BUF_MAX);
		offset = 0;
		mpam_data = &smmu_mpam_list[smmu_type][j];
		if (!mpam_data->active)
			continue;
		for (k = 0; k < mpam_data->event_len; k++) {
			ev = mpam_data->ev[k];
			if (!ev || ev->state != PERF_EVENT_STATE_ACTIVE)
				continue;
			ret = smmu_perf_event_read_local(ev, &value);
			if (ret < 0) {
				pr_info("%s, id:%d evt:%d read fail, ret=%d\n",
					__func__, mpam_data->mpam_id,
					mpam_data->event_list[k], ret);
				continue;
			}
			mpam_data->pmu_perfCurr[k] = value;
			delta = (mpam_data->pmu_perfCurr[k] -
				 mpam_data->pmu_perfPrev[k]);
			mpam_data->pmu_perfPrev[k] = mpam_data->pmu_perfCurr[k];
			if (mpam_data->pmu_perfCntFirst[k] == 1) {
				/* omit the first counter */
				mpam_data->pmu_perfCntFirst[k] = 0;
				continue;
			}
			if (k > 0) {
				ret = snprintf(trace_buf + offset,
					       sizeof(trace_buf) - offset,
					       ", ");
				if (ret >= (sizeof(trace_buf) - offset) ||
				    ret < 0) {
					pr_info("%s k=%d, snprintf err:%d",
						__func__, k, ret);
					return;
				}
				offset += ret;
				if (offset >= sizeof(trace_buf)) {
					pr_info("%s overflow, id:%d, k:%d\n",
						__func__,
						mpam_data->mpam_id, k);
					return;
				}
			}
			ret += snprintf(trace_buf + offset,
					sizeof(trace_buf) - offset,
					"%s=%lld",
					get_mpam_event_name(mpam_data->event_list[k],
							    mpam_data->ris[k],
							    mpam_data->partid[k],
							    j > 0 ? false : true),
					delta);
			if (ret >= (sizeof(trace_buf) - offset) || ret < 0) {
				pr_info("%s k=0, snprintf err:%d",
					__func__, ret);
				return;
			}
			offset += ret;
		}
		if (offset == 0)
			continue;
		if (j == 0)
			smmu_qos_print_trace(mpam_tcu_trace_func[smmu_type],
					     trace_buf);
		else
			smmu_qos_print_trace(mpam_tbu_trace_func[smmu_type][j - 1],
					     trace_buf);
	}
}

static int smmu_qos_debug_set_ftrace(const char *val,
				     const struct kernel_param *kp)
{
	u32 polling_interval;
	u32 ena = 0;
	int ret, i, j;

	ret = kstrtou32(val, 0, &ena);
	if (ret)
		return -EINVAL;

	mutex_lock(&pmu_list_lock);
	if (ftrace_ena == ena) {
		mutex_unlock(&pmu_list_lock);
		return 0;
	}

	polling_interval = ftrace_interval;
	if (polling_interval == 0)
		polling_interval = POLLING_INTERVAL_DEFAULT_US;

	if (ena) {
		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			need_poll[i] = false;
			for (j = 0; j < SMMU_TBU_CNT(i) + 1; j++) {
				if (smmu_pmu_list[i][j].active ||
				    smmu_lmu_list[i][j].active ||
				    smmu_mpam_list[i][j].active) {
					need_poll[i] = true;
					break;
				}
			}
		}

		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			if (!need_poll[i])
				continue;

			ret = mtk_smmu_rpm_get(i);
			if (ret) {
				pr_info("%s, smmu:%d get power fail\n",
					__func__, i);
				power_state[i] = false;
				continue;
			} else if (i != SOC_SMMU) {
				pr_info("%s, smmu:%d get power ok\n",
					__func__, i);
			}
			power_state[i] = true;
			create_pmu_perf_events(i);
			start_smmu_lmu(i);
			create_mpam_perf_events(i);
			ftrace_polling = true;
		}
		if (ftrace_polling) {
			ktime = ktime_set(0, US_TO_NS(polling_interval));
			hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
			ftrace_ena = true;
		}
	} else {
		if (ftrace_polling) {
			hrtimer_cancel(&hr_timer);
			ftrace_polling = false;
		}

		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			need_poll[i] = false;

			if (!power_state[i])
				continue;

			destroy_pmu_perf_event(i);
			stop_smmu_lmu(i);
			destroy_mpam_perf_event(i);

			mtk_smmu_rpm_put(i);
			pr_info("%s, smmu:%d put power ok\n",
				__func__, i);
			power_state[i] = false;
		}
		ftrace_ena = false;
		reset_all_data_list();
	}
	mutex_unlock(&pmu_list_lock);
	return 0;
}

static const struct kernel_param_ops smmu_qos_debug_set_ftrace_ops = {
	.set = smmu_qos_debug_set_ftrace,
	.get = param_get_uint,
};
module_param_cb(ftrace_ena, &smmu_qos_debug_set_ftrace_ops, &ftrace_ena, 0644);
MODULE_PARM_DESC(ftrace_ena, "smmu ftrace log");

static int smmu_qos_set_interval(const char *val,
				 const struct kernel_param *kp)
{
	u32 polling_interval = 0;
	int ret;

	ret = kstrtou32(val, 0, &polling_interval);
	if (ret || polling_interval == 0)
		polling_interval = POLLING_INTERVAL_DEFAULT_US;

	ftrace_interval = polling_interval;
	return ret;
}

static const struct kernel_param_ops smmu_qos_set_interval_ops = {
	.set = smmu_qos_set_interval,
	.get = param_get_uint,
};
module_param_cb(ftrace_interval, &smmu_qos_set_interval_ops,
		&ftrace_interval, 0644);
MODULE_PARM_DESC(ftrace_interval, "smmu polling interval in us");

static const struct kernel_param_ops smmu_qos_set_pmu_ops = {
	.set = smmu_qos_pmu_set,
	.get = smmu_qos_pmu_get,
};

static const struct kernel_param_ops smmu_qos_set_pmu_event_ops = {
	.set = smmu_qos_event_set,
	.get = smmu_qos_pmu_get,
};

static char g_pmu_id_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string pmu_id_list_kps = {
	.string = g_pmu_id_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(pmu_id_list, &smmu_qos_set_pmu_ops, &pmu_id_list_kps, 0644);
MODULE_PARM_DESC(pmu_id_list, "smmu pmu id list");

static char g_pmu_event_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string pmu_event_list_kps = {
	.string = g_pmu_event_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(pmu_event_list, &smmu_qos_set_pmu_event_ops, &pmu_event_list_kps, 0644);
MODULE_PARM_DESC(pmu_event_list, "smmu pmu event list");

static char g_pmu_param_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string pmu_param_list_kps = {
	.string = g_pmu_param_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(pmu_param_list, &smmu_qos_set_pmu_ops, &pmu_param_list_kps, 0644);
MODULE_PARM_DESC(pmu_param_list, "smmu pmu param list");

static char g_mpam_id_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string mpam_id_list_kps = {
	.string = g_mpam_id_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(mpam_id_list, &smmu_qos_set_pmu_ops, &mpam_id_list_kps, 0644);
MODULE_PARM_DESC(mpam_id_list, "smmu mpam id list");

static char g_mpam_param_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string mpam_param_list_kps = {
	.string = g_mpam_param_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(mpam_param_list, &smmu_qos_set_pmu_ops, &mpam_param_list_kps, 0644);
MODULE_PARM_DESC(mpam_param_list, "smmu mpam param list");

static const struct kernel_param_ops smmu_qos_set_lmu_ops = {
	.set = smmu_qos_lmu_set,
	.get = smmu_qos_pmu_get,
};

static char g_lmu_id_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string lmu_id_list_kps = {
	.string = g_lmu_id_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(lmu_id_list, &smmu_qos_set_lmu_ops, &lmu_id_list_kps, 0644);
MODULE_PARM_DESC(lmu_id_list, "smmu lmu id list");

static const struct kernel_param_ops smmu_qos_set_lmu_param_ops = {
	.set = smmu_qos_pmu_set,
	.get = smmu_qos_pmu_get,
};

static char g_lmu_param_list[SMMU_PMU_CFG_STR_SIZE];
static struct kparam_string lmu_param_list_kps = {
	.string = g_lmu_param_list,
	.maxlen = SMMU_PMU_CFG_STR_SIZE,
};
module_param_cb(lmu_param_list, &smmu_qos_set_lmu_param_ops, &lmu_param_list_kps, 0644);
MODULE_PARM_DESC(lmu_param_list, "smmu lmu param list");


static int smmu_qos_pmu_set(const char *val, const struct kernel_param *kp)
{
	int i, ret = 0, num_group = 0;
	int *group_len = NULL;
	int **out_arr = NULL;
	char *group = NULL, *val_local = NULL, *val_cpy = NULL, *val_tmp = NULL;

	if (strlen(val) == 0) {
		pr_info("%s, input len 0, return\n", __func__);
		return -1;
	}

	ret = param_set_copystring(val, kp);
	if (ret) {
		pr_info("%s, copy string err:%d\n", __func__, ret);
		goto out_free;
	}

	val_local = kstrndup(val, strlen(val), GFP_KERNEL);
	if (!val_local) {
		ret = -ENOMEM;
		goto out_free;
	}
	val_cpy = strstrip(val_local);
	val_tmp = kstrndup(val_cpy, strlen(val_cpy), GFP_KERNEL);
	if (!val_tmp) {
		ret = -ENOMEM;
		goto out_free;
	}
	while ((group = strsep(&val_tmp, "-")))
		num_group++;

	group_len = kmalloc_array(num_group, sizeof(int), GFP_KERNEL);
	if (!group_len) {
		ret = -ENOMEM;
		goto out_free;
	}
	out_arr = kmalloc_array(num_group, sizeof(int *), GFP_KERNEL);
	if (!out_arr) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = parse_pmu_entry_list(val_cpy, out_arr, num_group, group_len);
	if (ret) {
		pr_info("%s, parse_pmu_entry_list err:%d\n", __func__, ret);
		goto out_free;
	}

	mutex_lock(&pmu_list_lock);
	if (ftrace_ena) {
		pr_info("%s, not allowed to set param during polling\n",
			__func__);
		mutex_unlock(&pmu_list_lock);
		return -EINVAL;
	}
	if (kp->arg == &pmu_id_list_kps)
		ret = update_pmu_id_list(out_arr, num_group, group_len);
	else if (kp->arg == &pmu_param_list_kps)
		ret = update_pmu_param_list(out_arr, num_group, group_len);
	else if (kp->arg == &lmu_param_list_kps)
		ret = update_lmu_param_list(out_arr, num_group, group_len);
	else if (kp->arg == &mpam_id_list_kps)
		ret = update_mpam_id_list(out_arr, num_group, group_len);
	else if (kp->arg == &mpam_param_list_kps)
		ret = update_mpam_param_list(out_arr, num_group, group_len);

	mutex_unlock(&pmu_list_lock);

out_free:
	kfree(group_len);
	if (out_arr) {
		for (i = 0; i < num_group; i++)
			kfree(out_arr[i]);

		kfree(out_arr);
	}
	kfree(val_local);
	kfree(val_tmp);

	return ret;
}

static int smmu_qos_pmu_get(char *buffer, const struct kernel_param *kp)
{
	const char *str;
	int i;

	if (kp->arg == &pmu_id_list_kps) {
		for (i = 0; i < SMMU_TYPE_NUM; i++)
			dump_pmu_data_list(i);
	} else if (kp->arg == &lmu_id_list_kps) {
		for (i = 0; i < SMMU_TYPE_NUM; i++)
			dump_lmu_data_list(i);
	} else if (kp->arg == &mpam_id_list_kps) {
		for (i = 0; i < SMMU_TYPE_NUM; i++)
			dump_mpam_data_list(i);
	}

	str = kp->str->string;
	return sysfs_emit(buffer, "%s\n", str);
}

static int smmu_qos_event_set(const char *val, const struct kernel_param *kp)
{
	char *val_local = NULL, *val_cpy = NULL, *val_tmp = NULL;
	int *pmu_list = NULL, *group_len = NULL;
	int i, ret, num_group = 0;
	int **event_arr = NULL;
	char *group = NULL;

	if (strlen(val) == 0) {
		pr_info("%s, input len 0, return\n", __func__);
		return -1;
	}

	ret = param_set_copystring(val, kp);
	if (ret) {
		pr_info("%s, copy string err:%d\n", __func__, ret);
		goto out_free;
	}

	val_local = kstrndup(val, strlen(val), GFP_KERNEL);
	if (!val_local) {
		ret = -ENOMEM;
		goto out_free;
	}

	val_cpy = strstrip(val_local);
	val_tmp = kstrndup(val_cpy, strlen(val_cpy), GFP_KERNEL);
	if (!val_tmp) {
		ret = -ENOMEM;
		goto out_free;
	}
	while ((group = strsep(&val_tmp, "-")))
		num_group++;

	pmu_list = kmalloc_array(num_group, sizeof(int), GFP_KERNEL);
	if (!pmu_list) {
		ret = -ENOMEM;
		goto out_free;
	}
	event_arr = kmalloc_array(num_group, sizeof(int *), GFP_KERNEL);
	if (!event_arr) {
		ret = -ENOMEM;
		goto out_free;
	}
	group_len = kmalloc_array(num_group, sizeof(int), GFP_KERNEL);
	if (!group_len) {
		ret = -ENOMEM;
		goto out_free;
	}

	ret = parse_event_list(val_cpy, pmu_list, event_arr,
			       group_len, num_group);
	if (ret)
		goto out_free;

	mutex_lock(&pmu_list_lock);
	if (ftrace_ena) {
		pr_info("%s, not allowed to set param during polling\n",
			__func__);
		mutex_unlock(&pmu_list_lock);
		return -EINVAL;
	}
	update_pmu_event_list(pmu_list, num_group, event_arr, group_len);
	mutex_unlock(&pmu_list_lock);

out_free:
	kfree(val_local);
	kfree(val_tmp);
	kfree(pmu_list);
	if (event_arr) {
		for (i = 0; i < num_group; i++)
			kfree(event_arr[i]);

		kfree(event_arr);
	}
	kfree(group_len);

	return ret;
}

static void reset_pmu_data_list(u32 smmu_id)
{
	struct smmu_pmu_data *pmu_data;
	int j;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		pmu_data = &smmu_pmu_list[smmu_id][j];
		memset(pmu_data->event_list, 0, sizeof(pmu_data->event_list));
		memset(pmu_data->ev_attr, 0, sizeof(pmu_data->ev_attr));
		memset(pmu_data->ev, 0, sizeof(pmu_data->ev));
		memset(pmu_data->pmu_perfCurr, 0,
		       sizeof(pmu_data->pmu_perfCurr));
		memset(pmu_data->pmu_perfPrev, 0,
		       sizeof(pmu_data->pmu_perfPrev));
		memset(pmu_data->pmu_perfCntFirst, 0,
		       sizeof(pmu_data->pmu_perfCntFirst));
		pmu_data->smmu_id = smmu_id;
		pmu_data->txu_id = j;
		pmu_data->pmu_id = U32_MAX;
		pmu_data->sid = U32_MAX;
		pmu_data->span = 0;
		pmu_data->active = false;
		pmu_data->event_len = 0;
	}
}

static void reset_lmu_data_list(u32 smmu_id)
{
	struct smmu_lmu_data *lmu_data;
	int j;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		lmu_data = &smmu_lmu_list[smmu_id][j];
		lmu_data->smmu_id = smmu_id;
		lmu_data->txu_id = j;
		lmu_data->mon_id = 0;
		lmu_data->mon_mask = 0;
		lmu_data->active = false;
	}
}

static void reset_mpam_data_list(u32 smmu_id)
{
	struct smmu_mpam_data *mpam_data;
	int j;

	for (j = 0; j < SMMU_TBU_CNT(smmu_id) + 1; j++) {
		mpam_data = &smmu_mpam_list[smmu_id][j];
		memset(mpam_data->event_list, 0,
		       sizeof(mpam_data->event_list));
		memset(mpam_data->ris, 0, sizeof(mpam_data->ris));
		memset(mpam_data->partid, 0, sizeof(mpam_data->partid));
		memset(mpam_data->ev_attr, 0, sizeof(mpam_data->ev_attr));
		memset(mpam_data->ev, 0, sizeof(mpam_data->ev));
		memset(mpam_data->pmu_perfCurr, 0,
		       sizeof(mpam_data->pmu_perfCurr));
		memset(mpam_data->pmu_perfPrev, 0,
		       sizeof(mpam_data->pmu_perfPrev));
		memset(mpam_data->pmu_perfCntFirst, 0,
		       sizeof(mpam_data->pmu_perfCntFirst));
		mpam_data->smmu_id = smmu_id;
		mpam_data->txu_id = j;
		mpam_data->mpam_id = U32_MAX;
		mpam_data->active = false;
		mpam_data->event_len = 0;
	}
}

static void reset_all_data_list(void)
{
	int i;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		reset_pmu_data_list(i);
		reset_lmu_data_list(i);
		reset_mpam_data_list(i);
	}
}

static int parse_pmu_entry_list(char *input, int **out_arr,
				int num_group, int *group_len)
{
	char *token, *group, *tmpGroup, *id_group_dup;
	int group_idx = 0, length = 0, idx, ret;

	group = input;
	while ((token = strsep(&group, "-")) || group_idx < num_group) {
		if (token)
			tmpGroup = token;
		else
			tmpGroup = input;

		length = 0;
		id_group_dup = kstrdup(token, GFP_KERNEL);
		if (!id_group_dup)
			return -ENOMEM;
		while (strsep(&id_group_dup, ":"))
			length++;
		kfree(id_group_dup);
		group_len[group_idx] = length;

		out_arr[group_idx] = kmalloc_array(length, sizeof(int), GFP_KERNEL);
		if (!out_arr[group_idx])
			return -ENOMEM;

		idx = 0;
		while ((token = strsep(&tmpGroup, ":")) && idx < length) {
			ret = kstrtoint(token, 10, &out_arr[group_idx][idx]);
			if (ret) {
				pr_info("%s, parse failed:%d\n", __func__, ret);
				return ret;
			}
			idx++;
		}

		group_idx++;
	}

	return 0;
}

static int parse_event_list(char *input, int *pmu_list, int **event_arr,
			    int *group_lens, int num_group)
{
	char *token, *group, *tmpGroup, *group_dup;
	int group_idx = 0, length = 0, idx, err;

	group = input;
	while ((token = strsep(&group, "-")) || group_idx < num_group) {
		if (!token)
			pr_info("%s, err, token:%s, group:%s, idx:%d, num:%d",
				__func__, token, group, group_idx, num_group);

		tmpGroup = token;

		length = 0;
		group_dup = kstrdup(token, GFP_KERNEL);
		if (!group_dup)
			return -ENOMEM;

		err =  kstrtoint(strsep(&group_dup, ":"), 10, &pmu_list[group_idx]);
		if (err)
			return -EINVAL;

		while (strsep(&group_dup, ","))
			length++;
		kfree(group_dup);
		group_lens[group_idx] = length;
		event_arr[group_idx] = kmalloc_array(length, sizeof(int),
						     GFP_KERNEL);
		if (!event_arr[group_idx])
			return -ENOMEM;

		idx = 0;
		strsep(&tmpGroup, ":");
		while ((token = strsep(&tmpGroup, ",")) && idx < length) {
			err =  kstrtoint(token, 10, &event_arr[group_idx][idx]);
			if (err)
				return -EINVAL;

			idx++;
		}
		group_idx++;
	}
	return 0;
}

static int update_pmu_id_list(int **pmu_arr, int num_group, int *group_len)
{
	int i, smmu_id, txu_id, pmu_id;
	struct smmu_pmu_data *pmu_data;

	for (i = 0; i < num_group; i++) {
		if (group_len[i] != 3) {
			pr_info("%s, entry %d sz not 3, break\n", __func__, i);
			return -1;
		}
		smmu_id = pmu_arr[i][0];
		txu_id = pmu_arr[i][1];
		pmu_id = pmu_arr[i][2];
		if (smmu_id < 0 || smmu_id >= SMMU_TYPE_NUM) {
			pr_info("%s, smmu_id at entry[%d][0]:%d err\n",
				__func__, i, smmu_id);
			return -1;
		}
		if (txu_id < 0 || txu_id >= (SMMU_TBU_CNT(smmu_id) + 1)) {
			pr_info("%s, txu_id at entry[%d][1]:%d err\n",
				__func__, i, txu_id);
			return -1;
		}
		if (pmu_id < 0) {
			pr_info("%s, pmu_id at entry[%d][2]:%d err\n",
				__func__, i, pmu_id);
			return -1;
		}
		pmu_data = &smmu_pmu_list[smmu_id][txu_id];
		pmu_data->smmu_id = smmu_id;
		pmu_data->txu_id = txu_id;
		pmu_data->pmu_id = pmu_id;
	}
	return 0;
}

static void update_pmu_data_event(u32 pmu_id, u32 *event_list, u32 len)
{
	struct smmu_pmu_data *pmu_data = NULL;
	int i, j, k;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		for (j = 0; j < SMMU_TBU_CNT(i) + 1; j++) {
			if (pmu_id == smmu_pmu_list[i][j].pmu_id) {
				pmu_data = &smmu_pmu_list[i][j];
				break;
			}
		}
		if (pmu_data)
			break;
	}
	if (!pmu_data) {
		pr_info("%s, invalid pmu_d:%d\n", __func__, pmu_id);
		return;
	}

	pmu_data->active = true;
	pmu_data->event_len = 0;
	for (k = 0; k < len; k++) {
		pmu_data->event_list[k] = event_list[k];
		pmu_data->event_len++;
	}
}

static void update_pmu_event_list(int *pmu_list, int num_pmu,
				  int **event_arr, int *group_len)
{
	int i;

	for (i = 0; i < num_pmu; i++)
		update_pmu_data_event(pmu_list[i], event_arr[i], group_len[i]);
}

static void update_pmu_param_data(u32 pmu_id, u32 sid, u32 span)
{
	struct smmu_pmu_data *pmu_data;
	int i, j;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		for (j = 0; j < SMMU_TBU_CNT(i) + 1; j++) {
			pmu_data = &smmu_pmu_list[i][j];
			if (pmu_data->pmu_id == pmu_id) {
				pmu_data->sid = sid;
				pmu_data->span = (span == 1 ? 1 : 0);
				return;
			}
		}
	}
}

static int update_pmu_param_list(int **param_arr, int num_group,
				 int *group_len)
{
	int i, sid, span, pmu_id;

	for (i = 0; i < num_group; i++) {
		if (group_len[i] != 3) {
			pr_info("%s, entry %d sz!=3, break\n", __func__, i);
			return -1;
		}
		pmu_id = param_arr[i][0];
		sid = param_arr[i][1];
		span = param_arr[i][2];
		if (pmu_id < 0 || sid < 0 || span < 0) {
			pr_info("%s, input err at entry[%d]\n", __func__, i);
			return -1;
		}
		update_pmu_param_data(pmu_id, sid, span);
	}
	return 0;
}

static void update_lmu_id_list(int *lmu_arr, int length)
{
	struct smmu_lmu_data *lmu_data;
	int idx, i;

	for (idx = 0; idx < length; idx++) {
		for (i = 0; i < SMMU_TBU_CNT(idx) + 1; i++) {
			lmu_data = &smmu_lmu_list[lmu_arr[idx]][i];
			lmu_data->active = true;
		}
	}
}

static int smmu_qos_lmu_set(const char *val, const struct kernel_param *kp)
{
	char *val_local = NULL, *id_group_dup = NULL;
	char *token = NULL, *tokenTmp = NULL;
	int length = 0, idx = 0, ret = 0;
	int *lmu_arr = NULL;

	if (strlen(val) == 0) {
		pr_info("%s, input len 0, return\n", __func__);
		return -1;
	}

	ret = param_set_copystring(val, kp);
	if (ret) {
		pr_info("%s, copy string err:%d\n", __func__, ret);
		goto out_free;
	}

	val_local = kstrndup(val, strlen(val), GFP_KERNEL);
	if (!val_local) {
		ret = -ENOMEM;
		goto out_free;
	}

	token = val_local;
	id_group_dup = kstrndup(token, strlen(token), GFP_KERNEL);
	if (!id_group_dup) {
		ret = -ENOMEM;
		goto out_free;
	}
	while (strsep(&id_group_dup, ","))
		length++;

	lmu_arr = kmalloc_array(length, sizeof(int), GFP_KERNEL);
	if (!lmu_arr) {
		ret = -ENOMEM;
		goto out_free;
	}

	idx = 0;
	while ((tokenTmp = strsep(&token, ",")) && idx < length) {
		ret = kstrtoint(tokenTmp, 10, &lmu_arr[idx]);
		if (ret) {
			pr_info("%s, parse failed:%d\n", __func__, ret);
			goto out_free;
		}
		idx++;
	}

	mutex_lock(&pmu_list_lock);
	if (ftrace_ena) {
		pr_info("%s, not allowed to set param during polling",
			__func__);
		mutex_unlock(&pmu_list_lock);
		ret = -EINVAL;
		goto out_free;
	}
	update_lmu_id_list(lmu_arr, idx);
	mutex_unlock(&pmu_list_lock);

out_free:
	kfree(val_local);
	kfree(id_group_dup);
	kfree(lmu_arr);

	return ret;
}

static int update_lmu_param_list(int **param_arr, int num_group,
				 int *group_len)
{
	u32 i, lmu_id, txu_id, mon_id, mon_mask;
	struct smmu_lmu_data *lmu_data;

	for (i = 0; i < num_group; i++) {
		if (group_len[i] != 4) {
			pr_info("%s, entry %d sz!=4, break\n", __func__, i);
			return -1;
		}
		lmu_id = param_arr[i][0];
		txu_id = param_arr[i][1];
		mon_id = param_arr[i][2];
		mon_mask = param_arr[i][3];
		if (lmu_id >= SMMU_TYPE_NUM || txu_id >= SMMU_TBU_CNT(i) + 1) {
			pr_info("%s, input err at entry[%d]\n", __func__, i);
			return -1;
		}
		lmu_data = &smmu_lmu_list[lmu_id][txu_id];
		lmu_data->mon_id = mon_id;
		lmu_data->mon_mask = mon_mask;
	}
	return 0;
}

static int update_mpam_id_list(int **mpam_arr, int num_group, int *group_len)
{
	int i, smmu_id, txu_id, mpam_id;
	struct smmu_mpam_data *mpam_data;

	for (i = 0; i < num_group; i++) {
		if (group_len[i] != 3) {
			pr_info("%s, entry %d sz not 3, break\n", __func__, i);
			return -1;
		}
		smmu_id = mpam_arr[i][0];
		txu_id = mpam_arr[i][1];
		mpam_id = mpam_arr[i][2];
		if (smmu_id < 0 || smmu_id >= SMMU_TYPE_NUM) {
			pr_info("%s, smmu_id at entry[%d][0]:%d err\n",
				__func__, i, smmu_id);
			return -1;
		}
		if (txu_id < 0 || txu_id >= (SMMU_TBU_CNT(smmu_id) + 1)) {
			pr_info("%s, txu_id at entry[%d][1]:%d err\n",
				__func__, i, txu_id);
			return -1;
		}
		if (mpam_id < 0) {
			pr_info("%s, mpam_id at entry[%d][2]:%d err\n",
				__func__, i, mpam_id);
			return -1;
		}
		mpam_data = &smmu_mpam_list[smmu_id][txu_id];
		mpam_data->smmu_id = smmu_id;
		mpam_data->txu_id = txu_id;
		mpam_data->mpam_id = mpam_id;
	}
	return 0;
}

static int update_mpam_param_data(u32 mpam_id, u32 event_id,
				  u32 ris, u32 partid, u32 enable_pmg, u32 pmg)
{
	struct smmu_mpam_data *mpam_data = NULL;
	int i, j, ev_idx;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		for (j = 0; j < SMMU_TBU_CNT(i) + 1; j++) {
			if (smmu_mpam_list[i][j].mpam_id == mpam_id) {
				mpam_data = &smmu_mpam_list[i][j];
				break;
			}
		}
		if (mpam_data)
			break;
	}

	if (!mpam_data) {
		pr_info("%s, invalid mpam_id:%d\n", __func__, mpam_id);
		return -EINVAL;
	}

	if (mpam_data->event_len >= SMMU_MPAM_MAX_COUNTERS) {
		pr_info("%s, event len:%d exceed %d, break\n",
			__func__,
			mpam_data->event_len,
			SMMU_MPAM_MAX_COUNTERS);
		return -EINVAL;
	}

	ev_idx = mpam_data->event_len;
	mpam_data->event_len++;
	mpam_data->event_list[ev_idx] = 0;
	mpam_data->ris[ev_idx] = ris;
	mpam_data->partid[ev_idx] = partid;
	mpam_data->enable_pmg[ev_idx] = enable_pmg ? true : false;
	mpam_data->pmg[ev_idx] = pmg;
	mpam_data->active = true;
	return 0;
}

static int update_mpam_param_list(int **mpam_arr, int num_group,
				  int *group_len)
{
	int i, mpam_id, event_id, ris, partid, enable_pmg, pmg;

	for (i = 0; i < num_group; i++) {
		if (group_len[i] != 6) {
			pr_info("%s, entry %d sz not 6, break\n", __func__, i);
			continue;
		}
		mpam_id = mpam_arr[i][0];
		event_id = mpam_arr[i][1];
		ris = mpam_arr[i][2];
		partid = mpam_arr[i][3];
		enable_pmg = mpam_arr[i][4];
		pmg = mpam_arr[i][5];
		if (mpam_id < 0 || event_id < 0 || ris < 0 || partid < 0 ||
		    enable_pmg < 0 || pmg < 0) {
			pr_info("%s, invalid entry[%d]\n", __func__, i);
			continue;
		}

		update_mpam_param_data(mpam_id, event_id, ris,
				       partid, enable_pmg, pmg);
	}
	return 0;
}

static enum hrtimer_restart smmu_trace_hrtimer_cb(struct hrtimer *timer)
{
	int i;

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		if (!power_state[i])
			continue;

		smmu_pmu_polling(i);
		smmu_lmu_polling(i);
		smmu_mpam_polling(i);
	}

	hrtimer_forward_now(timer, ktime);
	return HRTIMER_RESTART;
}

void smmu_qos_print_trace(void (*trace)(struct va_format *),
			  const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	trace(&vaf);
	va_end(args);
}

int mtk_smmu_qos_probe(struct platform_device *pdev)
{
	struct mtk_smmu_data *data;
	void __iomem *wp_base;
	int i, j;

	/* hrtimer settings */
	ktime = ktime_set(0, US_TO_NS(POLLING_INTERVAL_DEFAULT_US));

	/* Initialize timer structure */
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &smmu_trace_hrtimer_cb;

	mutex_init(&pmu_list_lock);
	ftrace_polling = false;
	reset_all_data_list();

	for (i = 0; i < SMMU_TYPE_NUM; i++) {
		power_state[i] = false;
		need_poll[i] = false;
		data = get_smmu_data(i);
		if (data != NULL) {
			wp_base = data->smmu.wp_base;
			for (j = 0; j < SMMU_TBU_CNT(i) + 1; j++)
				smmu_lmu_list[i][j].wp_base = wp_base;
		} else {
			pr_info("%s, get smmu[%d] data err.\n", __func__, i);
			return -EINVAL;
		}
	}
	return 0;
}

static const struct of_device_id mtk_smmuqos_dbg_of_ids[] = {
	{ .compatible = "mediatek,smmu-qos" },
	{},
};

static struct platform_driver mtk_smmu_qos_drv = {
	.probe	= mtk_smmu_qos_probe,
	.driver	= {
		.name = "mtk-smmu-qos",
		.of_match_table = of_match_ptr(mtk_smmuqos_dbg_of_ids),
	}
};

module_platform_driver(mtk_smmu_qos_drv);
MODULE_LICENSE("GPL");
