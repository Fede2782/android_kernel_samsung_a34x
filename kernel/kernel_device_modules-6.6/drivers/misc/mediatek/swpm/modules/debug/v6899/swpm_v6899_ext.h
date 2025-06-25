/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_SWPM_SP_PLATFORM_H__
#define __MTK_SWPM_SP_PLATFORM_H__

/* numbers of power state (active, idle, off) */
enum pmsr_power_state {
	PMSR_ACTIVE,
	PMSR_IDLE,
	PMSR_OFF,

	NR_POWER_STATE,
};
/* #define NR_POWER_STATE (3) */

enum xpu_ip_state {
	XPU_IP_DISP0,
	XPU_IP_DISP1,
	XPU_IP_VENC0,
	XPU_IP_VENC1,
	XPU_IP_SCP,
	XPU_IP_ADSP,
	XPU_IP_MCU,

	NR_XPU_IP,
};

/* ddr byte count ip (total read, total write, cpu, mm, gpu, others) */
enum ddr_bc_ip {
	DDR_BC_TOTAL_R,
	DDR_BC_TOTAL_W,
	DDR_BC_TOTAL_CPU,
	DDR_BC_TOTAL_GPU,
	DDR_BC_TOTAL_MM,
	DDR_BC_TOTAL_OTHERS,

	NR_DDR_BC_IP,
};
/* #define NR_DDR_BC_IP (6) */

enum spm_sig_group {
	DDREN_REQ,
	APSRC_REQ,
	EMI_REQ,
	MAINPLL_REQ,
	INFRA_REQ,
	F26M_REQ,
	PMIC_REQ,
	VCORE_REQ,
	RC_REQ,
	PLL_EN,
	PWR_OFF,
	PWR_ACT,
	SYS_STA,

	NR_SPM_GRP,
	UNSUPPORT_REQ,
};

/* mem extension ip word count (1 word -> 8 bytes @ 64bits) */
struct mem_ip_bc_addr {
	unsigned int word_cnt_L_addr;
	unsigned int word_cnt_H_addr;
};

struct xpu_ip_pwr_sta {
	unsigned int state[NR_POWER_STATE];
};

struct xpu_index_ext {
	/* xpu ip power state distribution */
	struct xpu_ip_pwr_sta pwr_state[NR_XPU_IP];
};

struct suspend_time {
	/* total suspended time H/L*/
	unsigned int time_L;
	unsigned int time_H;
};

struct duration_time {
	/* total duration time H/L*/
	unsigned int time_L;
	unsigned int time_H;
};

struct share_spm_sig {
	unsigned int spm_sig_addr;
	unsigned int spm_sig_num[NR_SPM_GRP];
	unsigned int win_len;
};

struct share_index_ext {
	struct xpu_index_ext xpu_idx_ext;
	struct suspend_time suspend;
	struct duration_time duration;

	/* last core volt index */
	unsigned int last_volt_idx;

	/* last ddr freq index */
	unsigned int last_freq_idx;

	/* for determine if the data has been updated */
	unsigned int update_count;
};

struct share_ctrl_ext {
	unsigned int read_lock;
	unsigned int write_lock;
	unsigned int clear_flag;
};

struct share_subsys_data {
	unsigned int ddr_freq_num;
	unsigned int ddr_opp_freq_addr;
	unsigned int mem_acc_time_addr;
	unsigned int acc_sr_time_addr;
	unsigned int acc_pd_time_addr;
	struct mem_ip_bc_addr word_cnt_addr[NR_DDR_BC_IP];
	unsigned int emi_bw_read;
	unsigned int emi_bw_write;
	unsigned int core_volt_num;
	unsigned int core_opp_info_addr;
	unsigned int core_acc_time_addr;
};

extern spinlock_t swpm_sub_data_spinlock;
extern struct share_subsys_data sub_idx_snap;
extern int32_t swpm_dbg_en(uint32_t num1, uint32_t num2,
		uint32_t *out1, uint32_t *out2);
extern void swpm_v6899_ext_init(void);
extern void swpm_v6899_ext_exit(void);

#endif /* __MTK_SWPM_SP_PLATFORM_H__ */
