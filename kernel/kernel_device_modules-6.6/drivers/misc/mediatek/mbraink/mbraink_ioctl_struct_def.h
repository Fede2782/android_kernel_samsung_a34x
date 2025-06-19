/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_IOCTL_STRUCT_H
#define MBRAINK_IOCTL_STRUCT_H

#include <linux/kallsyms.h>

#define MAX_STRUCT_SZ				64
#define MAX_MEM_LIST_SZ				32
#define MAX_MONITOR_PROCESSNAME_SZ		64
#define MAX_MONITOR_PROCESS_NUM			16
#define MAX_DDR_FREQ_NUM			12
#define MAX_DDR_IP_NUM				8
#define MAX_TRACE_PID_NUM			16
#define MAX_VCORE_NUM				8
#define MAX_VCORE_IP_NUM			16
#define MAX_IP_NAME_LENGTH			(16)
#define MAX_NOTIFY_CPUFREQ_NUM			8
#define MAX_FREQ_SZ				64
#define MAX_WAKEUP_SOURCE_NUM			12
#define MAX_NAME_SZ						64
#define MAX_MDV_SZ						6
#define MAX_PMIC_SPMI_SZ		64
#define MAX_PMIC_UVLO_SZ		8
#define MAX_GPU_OPP_INFO_SZ			64
#define MAX_GNSS_DATA_SZ			7
#define MAX_WIFI_RADIO_SZ			3
#define MAX_WIFI_RATE_SZ			32
#define MAX_WIFI_LP_SZ				5
#define MAX_LPM_STATE_NUM				16
#define MAX_UFS_INFO_NUM				64
#define MAX_WIFI_TXTIMEOUT_SZ			32

#define NETLINK_EVENT_Q2QTIMEOUT		"NLEvent_Q2QTimeout"
#define NETLINK_EVENT_UDMFETCH			"M&"
#define NETLINK_EVENT_SYSRESUME		"NLEvent_SysResume"
#define NETLINK_EVENT_SYSBINDER		"NLEvent_SysBinder"
#define NETLINK_EVENT_SYSNOTIFIER_PS	"NLEvent_SysNotifierPS"
#define NETLINK_EVENT_PERFTIMEOUT		"NLEvent_PERFTO"
#define NETLINK_EVENT_PERFLOWOUT		"NLEvent_PERFLO"
#define NETLINK_EVENT_GPUFENCETIMOEUT "NLEvent_GFTO"
#define NETLINK_EVENT_GPURESETDONE "NLEvent_GReset"
#define NETLINK_EVENT_SYSPROCESS	"NLEvent_SysProcess"
#define NETLINK_EVENT_LOW_BATTERY_VOLTAGE_THROTTLE		"NLEvent_LBVThro"
#define NETLINK_EVENT_BATTERY_OVER_CURRENT_THROTTLE		"NLEvent_BOCThro"
#define NETLINK_EVENT_PPB_NOTIFY "NLEvent_PPBNotify"
#define NETLINK_EVENT_UFS_NOTIFY "NLEvent_UFSNotify"
#define NETLINK_EVENT_USB_ENUM "NLEvent_USBEnum"
#define NETLINK_EVENT_IMGSYS_NOTIFY "NLEvent_IMGSYSNotify"

#define NETLINK_EVENT_MESSAGE_SIZE		1024

#define MBRAINK_LANDING_FEATURE_CHECK 1

#define MBRAINK_PMU_INST_SPEC_EN	(1<<0UL)

#define MBRAINK_FEATURE_GPU_EN		(1<<0UL)
#define MBRAINK_FEATURE_AUDIO_EN	(1<<1UL)

#define MAX_POWER_SPM_TBL_SEC_SZ (928)
#define MAX_POWER_MMDVFS_SEC_SZ (264)

#define SPM_L2_MAX_RES_NUM (116)
#define SPM_L2_SZ (1888)

#define SPM_L1_DATA_NUM (14)
#define SPM_L1_SZ (112)
#define SPM_L2_RES_SIZE (16)
#define SPM_L2_LS_SZ (32)

#define SCP_SZ (200)

#define MD_HD_SZ 8
#define MD_MDHD_SZ 8
#define MD_BLK_SZ 300
#define MD_SECBLK_NUM 6
#define MD_SEC_SZ (MD_SECBLK_NUM*MD_BLK_SZ)

#define MAX_GPU_FENCE_RECORD_DATA 8

enum mbraink_op_mode {
	mbraink_op_mode_normal = 0,
	mbraink_op_mode_sbe = 1,
	mbraink_op_mode_game = 2,
	mbraink_op_mode_camera = 3,
	mbraink_op_mode_max = 4
};

struct mbraink_process_stat_struct {
	unsigned short pid;
	unsigned short uid;
	int priority;
	u64 process_jiffies;
};

struct mbraink_process_stat_data {
	unsigned short pid;
	unsigned short pid_count;
	unsigned int current_cnt;
	struct mbraink_process_stat_struct drv_data[MAX_STRUCT_SZ];
};

struct mbraink_process_memory_struct {
	unsigned short pid;
	u64 rss;
	u64 rswap;
	u64 rpage;
};

struct mbraink_process_memory_data {
	unsigned short pid;
	unsigned short pid_count;
	unsigned int current_cnt;
	struct mbraink_process_memory_struct drv_data[MAX_MEM_LIST_SZ];
};

struct mbraink_thread_stat_struct {
	unsigned short pid;
	unsigned short tid;
	unsigned short uid;
	int priority;
	u64 thread_jiffies;
};

struct mbraink_thread_stat_data {
	unsigned short pid_idx;
	unsigned short tid;
	unsigned short tid_count;
	unsigned int current_cnt;
	struct mbraink_thread_stat_struct drv_data[MAX_STRUCT_SZ];
};

struct mbraink_monitor_processlist {
	unsigned short monitor_process_count;
	char process_name[MAX_MONITOR_PROCESS_NUM][MAX_MONITOR_PROCESSNAME_SZ];
};

struct mbraink_memory_ddrActiveInfo {
	int32_t freqInMhz;
	int64_t totalActiveTimeInMs;
	uint64_t totalIPActiveTimeInMs[MAX_DDR_IP_NUM];
};

struct mbraink_memory_ddrInfo {
	struct mbraink_memory_ddrActiveInfo ddrActiveInfo[MAX_DDR_FREQ_NUM];
	int64_t srTimeInMs;
	int64_t pdTimeInMs;
	int32_t totalDdrFreqNum;
	int32_t totalDdrIpNum;
};

struct mbraink_audio_idleRatioInfo {
	int64_t timestamp;
	int64_t s0_time;
	int64_t s1_time;
	int64_t mcusys_active_time;
	int64_t mcusys_pd_time;
	int64_t cluster_active_time;
	int64_t cluster_idle_time;
	int64_t cluster_pd_time;
	int64_t adsp_active_time;
	int64_t adsp_wfi_time;
	int64_t adsp_pd_time;
	int64_t audio_hw_time;
};

struct mbraink_tracing_pid {
	unsigned short pid;
	unsigned short tgid;
	unsigned short uid;
	int priority;
	char name[TASK_COMM_LEN];
	long long start;
	long long end;
	u64 jiffies;
};

struct mbraink_tracing_pid_data {
	unsigned short tracing_idx;
	unsigned short tracing_count;
	struct mbraink_tracing_pid drv_data[MAX_TRACE_PID_NUM];
};

struct mbraink_power_vcoreDurationInfo {
	int32_t vol;
	int64_t duration;
};

struct mbraink_power_vcoreIpDurationInfo {
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};

struct mbraink_power_vcoreIpStats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct mbraink_power_vcoreIpDurationInfo times;
};

struct mbraink_power_vcoreInfo {
	struct mbraink_power_vcoreDurationInfo vcoreDurationInfo[MAX_VCORE_NUM];
	struct mbraink_power_vcoreIpStats vcoreIpDurationInfo[MAX_VCORE_IP_NUM];
	int32_t totalVCNum;
	int32_t totalVCIpNum;
};

struct mbraink_cpufreq_notify_struct {
	long long timestamp;
	int cid;
	unsigned short qos_type;
	unsigned int freq_limit;
	char caller[MAX_FREQ_SZ];
};

struct mbraink_cpufreq_notify_struct_data {
	unsigned short notify_cluster_idx;
	unsigned short notify_idx;
	unsigned short notify_count;
	struct mbraink_cpufreq_notify_struct drv_data[MAX_NOTIFY_CPUFREQ_NUM];
};

struct mbraink_battery_data {
	long long timestamp;
	int quse;
	int qmaxt;
	int precise_soc;
	int precise_uisoc;
	int quse2;
	int qmaxt2;
	int precise_soc2;
	int precise_uisoc2;
};

struct mbraink_feature_en {
	unsigned int feature_en;
};

struct mbraink_power_wakeup_struct {
	char name[MAX_NAME_SZ];
	unsigned long  active_count;
	unsigned long event_count;
	unsigned long wakeup_count;
	unsigned long expire_count;
	s64 active_time;
	s64 total_time;
	s64 max_time;
	s64 last_time;
	s64 prevent_sleep_time;
};

struct mbraink_power_wakeup_data {
	uint8_t is_has_data;
	unsigned short next_pos;
	struct mbraink_power_wakeup_struct drv_data[MAX_WAKEUP_SOURCE_NUM];
};

struct mbraink_pmu_en {
	unsigned int pmu_en;
};

struct mbraink_pmu_info {
	unsigned long cpu0_pmu_data_inst_spec;
	unsigned long cpu1_pmu_data_inst_spec;
	unsigned long cpu2_pmu_data_inst_spec;
	unsigned long cpu3_pmu_data_inst_spec;
	unsigned long cpu4_pmu_data_inst_spec;
	unsigned long cpu5_pmu_data_inst_spec;
	unsigned long cpu6_pmu_data_inst_spec;
	unsigned long cpu7_pmu_data_inst_spec;
};

struct mbraink_power_spm_raw {
	uint8_t type;
	unsigned short pos;
	unsigned short size;
	unsigned char spm_data[MAX_POWER_SPM_TBL_SEC_SZ];
};

struct mbraink_power_spm_l2_info {
	unsigned int value[4];
	unsigned char spm_data[SPM_L2_SZ];
};

struct mbraink_power_scp_info {
	unsigned char scp_data[SCP_SZ];
};

struct mbraink_modem_raw {
	uint8_t type;
	uint8_t is_has_data;
	unsigned short count;
	unsigned char data1[MD_HD_SZ];
	unsigned char data2[MD_MDHD_SZ];
	unsigned char data3[MD_SEC_SZ];
};

struct mbraink_memory_mdvInfo {
	uint8_t type;
	uint8_t mid;
	uint8_t ver;
	uint16_t pos;
	uint32_t size;
	uint32_t raw[MAX_MDV_SZ];
};

struct mbraink_binder_trace {
	unsigned short from_pid;
	unsigned short from_tid;
	unsigned short to_pid;
	unsigned int count;
};

struct mbraink_binder_trace_data {
	unsigned short tracing_idx;
	unsigned short tracing_count;
	struct mbraink_binder_trace drv_data[MAX_TRACE_PID_NUM];
};

struct mbraink_voting_struct_data {
	int voting_num;
	unsigned int mbraink_voting_data[MAX_STRUCT_SZ];
};

struct mbraink_spmi_struct_data {
	unsigned int spmi_count;
	unsigned int spmi[MAX_PMIC_SPMI_SZ];
};

struct mbraink_uvlo_err_data {
	int ot;
	int uv;
	int oc;
};

struct mbraink_uvlo_struct_data {
	unsigned int uvlo_count;
	struct mbraink_uvlo_err_data uvlo_err_data[MAX_PMIC_UVLO_SZ];
};

struct mbraink_gpu_opp_raw {
	uint32_t data1;
	uint64_t data2;
	uint64_t data3;
};

struct mbraink_gpu_opp_info {
	struct mbraink_gpu_opp_raw raw[MAX_GPU_OPP_INFO_SZ];
	uint64_t data1;
};

struct mbraink_gpu_state_info {
	uint64_t data1;
	uint64_t data2;
	uint64_t data3;
	uint64_t data4;
};

struct mbraink_gpu_loading_info {
	uint64_t data1;
	uint64_t data2;
};

struct mbraink_pmic_voltage_info {
	uint64_t vcore;
	uint64_t vsram_core;
};

struct mbraink_operation_mode_info {
	enum mbraink_op_mode opMode;
};

struct mbraink_gnss2mbr_lp_struct {
	u64 dump_ts;
	u32 dump_index;
	u32 gnss_mcu_sid;
	u8 gnss_mcu_is_on;

	/* N/A if gnss_mcu_is_on = false */
	u8 gnss_pwr_is_hi;
	u8 gnss_pwr_wrn;

	/* history statistic */
	u32 gnss_pwr_wrn_cnt;
};

struct mbraink_gnss2mbr_lp_data {
	u16 count;
	struct mbraink_gnss2mbr_lp_struct lp_data[MAX_GNSS_DATA_SZ];
};

struct mbraink_gnss2mbr_mcu_struct {
	u32 gnss_mcu_sid; /* last finished one */
	u32 clock_cfg_val;

	u64 open_ts;
	u32 open_duration;

	u8 has_exception;
	u8 force_close;

	u64 close_ts;
	u32 close_duration;

	/* history statistic */
	u32 open_duration_max;
	u32 close_duration_max;
	u32 exception_cnt;
	u32 force_close_cnt;
};

struct mbraink_gnss2mbr_mcu_data {
	u16 count;
	struct mbraink_gnss2mbr_mcu_struct mcu_data[MAX_GNSS_DATA_SZ];
};

struct mbraink_mmdvfs_info {
	unsigned short size;
	unsigned char mmdvfs_data[MAX_POWER_MMDVFS_SEC_SZ];
};

struct mbraink_wifi2mbr_lls_rate_struct {
	u64 timestamp;
	unsigned int rate_idx; /* rate idx, ref: LLS_RATE_XXX_MASK */
	unsigned int bitrate;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int mpdu_lost;
	unsigned int retries;
};

struct mbraink_wifi2mbr_lls_rate_data {
	u16 count;
	u32 idx;
	struct mbraink_wifi2mbr_lls_rate_struct rate_data[MAX_WIFI_RATE_SZ];
};

struct mbraink_wifi2mbr_lls_radio_struct {
	u64 timestamp;
	int radio;
	unsigned int on_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int on_time_scan;
	unsigned int on_time_roam_scan;
	unsigned int on_time_pno_scan;
};

struct mbraink_wifi2mbr_lls_radio_data {
	u16 count;
	u32 idx;
	struct mbraink_wifi2mbr_lls_radio_struct radio_data[MAX_WIFI_RADIO_SZ];
};

enum mbraink_enum_mbr_wifi_ac {
	MBRAINK_MBR_WIFI_AC_VO,
	MBRAINK_MBR_WIFI_AC_VI,
	MBRAINK_MBR_WIFI_AC_BE,
	MBRAINK_MBR_WIFI_AC_BK,
	MBRAINK_MBR_WIFI_AC_MAX,
};

struct mbraink_wifi2mbr_lls_ac_struct {
	u64 timestamp;
	enum mbraink_enum_mbr_wifi_ac ac;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int tx_mcast;
	unsigned int tx_ampdu;
	unsigned int mpdu_lost;
	unsigned int retries;
	unsigned int contention_time_min;
	unsigned int contention_time_max;
	unsigned int contention_time_avg;
	unsigned int contention_num_samples;
};

struct mbraink_wifi2mbr_lls_ac_data {
	u16 count;
	u32 idx;
	struct mbraink_wifi2mbr_lls_ac_struct ac_data[MBRAINK_MBR_WIFI_AC_MAX];
};

struct mbraink_wifi2mbr_lp_ratio_struct {
	u64 timestamp;
	int radio;
	unsigned int total_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int rx_listen_time;
	unsigned int sleep_time;
};

struct mbraink_wifi2mbr_lp_ratio_data {
	u16 count;
	u32 idx;
	struct mbraink_wifi2mbr_lp_ratio_struct lp_data[MAX_WIFI_LP_SZ];
};

struct mbraink_power_throttle_hw_data {
	unsigned long kernel_time;
	unsigned long duration;
	int soc;
	int temp;
	int soc_rdc;
	int soc_rac;
	int hpt_bat_budget;
	int hpt_cg_budget;
	int ppb_cg_budget;
	int hpt_cpub_thr_cnt;
	int hpt_cpub_thr_time;
	int hpt_cpum_thr_cnt;
	int hpt_cpum_thr_time;
	int hpt_gpu_thr_cnt;
	int hpt_gpu_thr_time;
	int hpt_cpub_sf;
	int hpt_cpum_sf;
	int hpt_gpu_sf;
	int ppb_combo;
	int ppb_c_combo0;
	int ppb_g_combo0;
	int ppb_g_flavor;
};

struct mbraink_lpm_state_info {
	u64 count;
	u64 duration;
};

struct mbraink_lpm_state_data {
	u16 state_num;
	struct mbraink_lpm_state_info lpm_state_info[MAX_LPM_STATE_NUM];
};

struct mbraink_ufs_info {
	unsigned char model[MAX_UFS_INFO_NUM];
	unsigned char rev[MAX_UFS_INFO_NUM];
};

struct mbraink_wifi2mbr_txtimeout_struct {
	u64 timestamp;
	unsigned int token_id;
	unsigned int wlan_index;
	unsigned int bss_index;
	unsigned int timeout_duration;
	unsigned int operation_mode;
	unsigned int idle_slot_diff_cnt;
};

struct mbraink_wifi2mbr_txtimeout_data {
	u16 count;
	u32 idx;
	struct mbraink_wifi2mbr_txtimeout_struct txtimeout_data[MAX_WIFI_TXTIMEOUT_SZ];
};

struct mbraink_vdec_fps {
	unsigned short pid;
	int vdec_fps;
};
#endif
