/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM smmu_qos

#if !defined(_TRACE_MTK_SMMU_QOS_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_SMMU_QOS_EVENTS_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(smmu_pmu_template,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf),
	TP_STRUCT__entry(__vstring(msg, vaf->fmt, vaf->va)),
	TP_fast_assign(
		__assign_vstr(msg, vaf->fmt, vaf->va);
	),
	TP_printk("%s", __get_str(msg))
);

#define DEFINE_PMU_EVENT(name)			\
DEFINE_EVENT(smmu_pmu_template, name,		\
	TP_PROTO(struct va_format *vaf),	\
	TP_ARGS(vaf))

DEFINE_PMU_EVENT(mm_smmu__tcu_pmu);
DEFINE_PMU_EVENT(mm_smmu__tbu0_pmu);
DEFINE_PMU_EVENT(mm_smmu__tbu1_pmu);
DEFINE_PMU_EVENT(mm_smmu__tbu2_pmu);
DEFINE_PMU_EVENT(mm_smmu__tbu3_pmu);
DEFINE_PMU_EVENT(apu_smmu__tcu_pmu);
DEFINE_PMU_EVENT(apu_smmu__tbu0_pmu);
DEFINE_PMU_EVENT(apu_smmu__tbu1_pmu);
DEFINE_PMU_EVENT(apu_smmu__tbu2_pmu);
DEFINE_PMU_EVENT(apu_smmu__tbu3_pmu);
DEFINE_PMU_EVENT(soc_smmu__tcu_pmu);
DEFINE_PMU_EVENT(soc_smmu__tbu0_pmu);
DEFINE_PMU_EVENT(soc_smmu__tbu1_pmu);
DEFINE_PMU_EVENT(soc_smmu__tbu2_pmu);
DEFINE_PMU_EVENT(gpu_smmu__tcu_pmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu0_pmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu1_pmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu2_pmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu3_pmu);

DEFINE_PMU_EVENT(mm_smmu__tcu_lmu);
DEFINE_PMU_EVENT(apu_smmu__tcu_lmu);
DEFINE_PMU_EVENT(soc_smmu__tcu_lmu);
DEFINE_PMU_EVENT(gpu_smmu__tcu_lmu);
DEFINE_PMU_EVENT(mm_smmu__tbu0_lmu);
DEFINE_PMU_EVENT(mm_smmu__tbu1_lmu);
DEFINE_PMU_EVENT(mm_smmu__tbu2_lmu);
DEFINE_PMU_EVENT(mm_smmu__tbu3_lmu);
DEFINE_PMU_EVENT(apu_smmu__tbu0_lmu);
DEFINE_PMU_EVENT(apu_smmu__tbu1_lmu);
DEFINE_PMU_EVENT(apu_smmu__tbu2_lmu);
DEFINE_PMU_EVENT(apu_smmu__tbu3_lmu);
DEFINE_PMU_EVENT(soc_smmu__tbu0_lmu);
DEFINE_PMU_EVENT(soc_smmu__tbu1_lmu);
DEFINE_PMU_EVENT(soc_smmu__tbu2_lmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu0_lmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu1_lmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu2_lmu);
DEFINE_PMU_EVENT(gpu_smmu__tbu3_lmu);

DEFINE_PMU_EVENT(mm_smmu__tcu_mpam);
DEFINE_PMU_EVENT(mm_smmu__tbu0_mpam);
DEFINE_PMU_EVENT(mm_smmu__tbu1_mpam);
DEFINE_PMU_EVENT(mm_smmu__tbu2_mpam);
DEFINE_PMU_EVENT(mm_smmu__tbu3_mpam);
DEFINE_PMU_EVENT(apu_smmu__tcu_mpam);
DEFINE_PMU_EVENT(apu_smmu__tbu0_mpam);
DEFINE_PMU_EVENT(apu_smmu__tbu1_mpam);
DEFINE_PMU_EVENT(apu_smmu__tbu2_mpam);
DEFINE_PMU_EVENT(apu_smmu__tbu3_mpam);
DEFINE_PMU_EVENT(soc_smmu__tcu_mpam);
DEFINE_PMU_EVENT(soc_smmu__tbu0_mpam);
DEFINE_PMU_EVENT(soc_smmu__tbu1_mpam);
DEFINE_PMU_EVENT(soc_smmu__tbu2_mpam);
DEFINE_PMU_EVENT(gpu_smmu__tcu_mpam);
DEFINE_PMU_EVENT(gpu_smmu__tbu0_mpam);
DEFINE_PMU_EVENT(gpu_smmu__tbu1_mpam);
DEFINE_PMU_EVENT(gpu_smmu__tbu2_mpam);
DEFINE_PMU_EVENT(gpu_smmu__tbu3_mpam);

#endif /* _TRACE_MTK_SMMU_QOS_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk_smmu_qos_events

/* This part must be outside protection */
#include <trace/define_trace.h>
