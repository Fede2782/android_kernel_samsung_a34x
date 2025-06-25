/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SWPM Module with Power Service Pack
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_MODULE_PSP_H__
#define __SWPM_MODULE_PSP_H__

#include <linux/types.h>

#define MAX_IP_NAME_LENGTH (16)


enum SWPM_PSP_MAIN_RES {
	SWPM_PSP_MAIN_RES_DDREN,
	SWPM_PSP_MAIN_RES_APSRC,
	SWPM_PSP_MAIN_RES_EMI,
	SWPM_PSP_MAIN_RES_MAINPLL,
	SWPM_PSP_MAIN_RES_INFRA,
	SWPM_PSP_MAIN_RES_26M,
	SWPM_PSP_MAIN_RES_PMIC,
	SWPM_PSP_MAIN_RES_VCORE,
	SWPM_PSP_MAIN_RES_RC_REQ,
	SWPM_PSP_MAIN_RES_PLL_EN,
	SWPM_PSP_MAIN_RES_PWR_OFF,
	SWPM_PSP_MAIN_RES_PWR_ACT,
	SWPM_PSP_MAIN_RES_SYS_STA,
	SWPM_MAIN_RES_NUM,
};


enum SWPM_PSP_SEL_SIG {
	SWPM_PSP_SEL_SIG_VCORE_MD,
	SWPM_PSP_SEL_SIG_VCORE_CONN,
	SWPM_PSP_SEL_SIG_VCORE_SCP,
	SWPM_PSP_SEL_SIG_VCORE_ADSP,
	SWPM_PSP_SEL_SIG_VCORE_PCIE0,
	SWPM_PSP_SEL_SIG_VCORE_PCIE1,
	SWPM_PSP_SEL_SIG_VCORE_MMPROC,
	SWPM_PSP_SEL_SIG_VCORE_UARTHUB,
	SWPM_PSP_SEL_SIG_NUM,
	SWPM_PSP_SEL_SIG_UNUSE,
};

/* swpm power service pack interface types */
enum swpm_num_type {
	DDR_DATA_IP,
	DDR_FREQ,
	CORE_IP,
	CORE_VOL,
	XPU_IP,
};

enum swpm_psp_return_type {
	SWPM_PSP_SUCCESS = 0,
	SWPM_NOT_EXE = 1,
	SWPM_FLAG_ERR = 2,
	SWPM_LOCK_ERR = 3,
};

/* swpm power service pack structure */
struct ip_vol_times {
	int32_t vol;
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};

struct ip_times {
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};
struct ip_stats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct ip_vol_times *vol_times;
	struct ip_times *times;
};
struct vol_duration {
	int32_t vol;
	int64_t duration;
};
struct ddr_act_times {
	int32_t freq;
	int64_t active_time;
};
struct ddr_sr_pd_times {
	int64_t sr_time;
	int64_t pd_time;
};
struct ddr_bc_stats {
	int32_t freq;
	uint64_t value;
};
struct ddr_ip_bc_stats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct ddr_bc_stats *bc_stats;
};

struct res_sig {
	uint64_t time;
	uint32_t sig_id;
	uint32_t grp_id;
};
struct res_sig_stats {
	struct res_sig *res_sig_tbl;
	uint32_t res_sig_num;
	uint64_t duration_time;
	uint64_t suspend_time;
};

/* swpm power service pack internal ops structure */
struct swpm_internal_ops {
	int32_t (*const cmd)(unsigned int type,
			  unsigned int val);
	int32_t (*const ddr_act_times_get)
		(int32_t freq_num,
		 struct ddr_act_times *ddr_times);
	int32_t (*const ddr_sr_pd_times_get)
		(struct ddr_sr_pd_times *ddr_times);
	int32_t (*const ddr_freq_data_ip_stats_get)
		(int32_t data_ip_num,
		 int32_t freq_num,
		 void *stats);
	int32_t (*const vcore_ip_vol_stats_get)
		(int32_t ip_num,
		 int32_t vol_num,
		 void *stats);
	int32_t (*const vcore_vol_duration_get)
		(int32_t vol_num,
		 struct vol_duration *duration);
	int32_t (*const xpu_ip_stats_get)
		(int32_t ip_num,
		 void *stats);
	int32_t (*const res_sig_stats_get)
		(struct res_sig_stats *stats);
	int32_t (*const num_get)
		(enum swpm_num_type type);
	int32_t (*const res_group_info_get)
		(uint32_t grp, uint32_t *out1,
		 uint32_t *out2, uint32_t *out3);
	int32_t (*const res_group_id_get)
		(uint32_t ip1, uint32_t ip2, uint32_t ip3,
		 uint32_t *out1, uint32_t *out2, uint32_t *out3);
	int32_t (*const data_record_number_get)
		(uint32_t *number);
};

extern int32_t sync_latest_data(void);
extern int32_t get_ddr_act_times(int32_t freq_num,
				 struct ddr_act_times *ddr_times);
extern int32_t get_ddr_sr_pd_times(struct ddr_sr_pd_times *ddr_times);
extern int32_t get_ddr_data_ip_num(void);
extern int32_t get_ddr_freq_num(void);
extern int32_t get_ddr_freq_data_ip_stats(int32_t data_ip_num,
					  int32_t freq_num,
					  void *stats);
extern int32_t get_vcore_ip_num(void);
extern int32_t get_vcore_vol_num(void);
extern int32_t get_vcore_ip_vol_stats(int32_t ip_num,
				       int32_t vol_num,
				       void *stats);
extern int32_t get_vcore_vol_duration(int32_t vol_num,
				      struct vol_duration *duration);
extern int32_t get_xpu_ip_num(void);
extern int32_t get_xpu_ip_stats(int32_t ip_num, void *stats);
extern int32_t get_res_sig_stats(struct res_sig_stats *stats);
extern int32_t get_res_group_info(uint32_t grp,
		uint32_t *out1, uint32_t *out2, uint32_t *out3);
extern int32_t get_res_group_id(uint32_t ip1, uint32_t ip2, uint32_t ip3,
		uint32_t *out1, uint32_t *out2, uint32_t *out3);
extern int mtk_register_swpm_ops(struct swpm_internal_ops *ops);
extern int32_t get_data_record_number(uint32_t *number);

#endif /* __SWPM_MODULE_PSP_H__ */

