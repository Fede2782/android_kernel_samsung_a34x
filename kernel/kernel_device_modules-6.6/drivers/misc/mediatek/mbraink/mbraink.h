/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_H
#define MBRAINK_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <net/sock.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"
#include <mbraink_auto_ioctl_struct_def.h>

#define IOC_MAGIC	'k'

#define MAX_BUF_SZ			1024

/*Mbrain Delegate Info List*/
#define AUTO_IOCTL				'0'
#define POWER_INFO				'1'
#define VIDEO_INFO				'2'
#define POWER_SUSPEND_EN		'3'
#define PROCESS_MEMORY_INFO		'4'
#define PROCESS_STAT_INFO		'5'
#define THREAD_STAT_INFO		'6'
#define SET_MINITOR_PROCESS		'7'
#define MEMORY_DDR_INFO			'8'
#define IDLE_RATIO_INFO         '9'
#define TRACE_PROCESS_INFO      'a'
#define VCORE_INFO              'b'
#define CPUFREQ_NOTIFY_INFO		'c'
#define BATTERY_INFO			'e'
#define FEATURE_EN				'f'
#define WAKEUP_INFO				'g'
#define PMU_EN					'h'
#define PMU_INFO				'i'
#define POWER_SPM_RAW			'j'
#define MODEM_INFO				'k'
#define MEMORY_MDV_INFO			'l'
#define MONITOR_BINDER_PROCESS         'm'
#define TRACE_BINDER_INFO              'o'
#define VCORE_VOTE_INFO			'p'
#define POWER_SPM_L2_INFO		'q'
#define POWER_SCP_INFO			'r'
#define POWER_SPMI_INFO		's'
#define POWER_UVLO_INFO		't'
#define GPU_OPP_INFO			'u'
#define GPU_STATE_INFO			'v'
#define GPU_LOADING_INFO		'w'
#define PMIC_VOLTAGE_INFO		'x'
#define OPERATION_MODE_INFO		'y'
#define GNSS_LP_INFO			'z'
#define GNSS_MCU_INFO			'A'
#define MMDVFS_INFO				'B'
#define WIFI_RATE_INFO			'C'
#define WIFI_RADIO_INFO			'D'
#define WIFI_AC_INFO			'E'
#define WIFI_LP_INFO			'F'
#define POWER_THROTTLE_HW_INFO	'G'
#define LPM_STATE_INFO			'H'
#define UFS_INFO				'J'
#define WIFI_TXTIMEOUT_INFO		'K'
#define VDEC_FPS_INFO			'L'

/*Mbrain Delegate IOCTL List*/
#define AUTO_IOCTL_INFO			_IOR(IOC_MAGIC, AUTO_IOCTL, \
							struct mbraink_auto_ioctl_info*)
#define RO_POWER				_IOR(IOC_MAGIC, POWER_INFO, char*)
#define RO_VIDEO				_IOR(IOC_MAGIC, VIDEO_INFO, char*)
#define WO_SUSPEND_POWER_EN		_IOW(IOC_MAGIC, POWER_SUSPEND_EN, char*)
#define RO_PROCESS_MEMORY		_IOR(IOC_MAGIC, PROCESS_MEMORY_INFO, \
							struct mbraink_process_memory_data*)
#define RO_PROCESS_STAT			_IOR(IOC_MAGIC, PROCESS_STAT_INFO, \
							struct mbraink_process_stat_data*)
#define RO_THREAD_STAT			_IOR(IOC_MAGIC, THREAD_STAT_INFO, \
							struct mbraink_thread_stat_data*)
#define WO_MONITOR_PROCESS		_IOW(IOC_MAGIC, SET_MINITOR_PROCESS, \
							struct mbraink_monitor_processlist*)
#define RO_MEMORY_DDR_INFO		_IOR(IOC_MAGIC, MEMORY_DDR_INFO, \
							struct mbraink_memory_ddrInfo*)
#define RO_IDLE_RATIO                  _IOR(IOC_MAGIC, IDLE_RATIO_INFO, \
							struct mbraink_audio_idleRatioInfo*)
#define RO_TRACE_PROCESS            _IOR(IOC_MAGIC, TRACE_PROCESS_INFO, \
							struct mbraink_tracing_pid_data*)
#define RO_VCORE_INFO                 _IOR(IOC_MAGIC, VCORE_INFO, \
							struct mbraink_power_vcoreInfo*)
#define RO_CPUFREQ_NOTIFY		_IOR(IOC_MAGIC, CPUFREQ_NOTIFY_INFO, \
							struct mbraink_cpufreq_notify_struct_data*)
#define RO_BATTERY_INFO			_IOR(IOC_MAGIC, BATTERY_INFO, \
							struct mbraink_battery_data*)
#define WO_FEATURE_EN		_IOW(IOC_MAGIC, FEATURE_EN, \
							struct mbraink_feature_en*)
#define RO_WAKEUP_INFO			_IOR(IOC_MAGIC, WAKEUP_INFO, \
							struct mbraink_power_wakeup_data*)
#define WO_PMU_EN				_IOW(IOC_MAGIC, PMU_EN, \
							struct mbraink_pmu_en*)
#define RO_PMU_INFO				_IOR(IOC_MAGIC, PMU_INFO, \
							struct mbraink_pmu_info*)

#define RO_POWER_SPM_RAW			_IOR(IOC_MAGIC, POWER_SPM_RAW, \
								struct mbraink_power_spm_raw*)

#define RO_MODEM_INFO			_IOR(IOC_MAGIC, MODEM_INFO, \
							struct mbraink_modem_raw*)
#define WO_MONITOR_BINDER_PROCESS	_IOW(IOC_MAGIC,	MONITOR_BINDER_PROCESS,	\
						struct mbraink_monitor_processlist*)
#define RO_TRACE_BINDER			_IOR(IOC_MAGIC, TRACE_BINDER_INFO,	\
						struct mbraink_binder_trace_data*)

#define RO_MEMORY_MDV_INFO			_IOR(IOC_MAGIC, MEMORY_MDV_INFO, \
							struct mbraink_memory_mdvInfo*)

#define RO_VCORE_VOTE			_IOR(IOC_MAGIC, VCORE_VOTE_INFO, \
						struct mbraink_voting_struct_data*)

#define RO_POWER_SPM_L2_INFO	_IOR(IOC_MAGIC, POWER_SPM_L2_INFO, \
						struct mbraink_power_spm_l2_info*)


#define RO_POWER_SCP_INFO	_IOR(IOC_MAGIC, POWER_SCP_INFO, \
						struct mbraink_power_scp_raw*)
#define RO_POWER_SPMI_INFO	_IOR(IOC_MAGIC, POWER_SPMI_INFO, \
						struct mbraink_spmi_struct_data*)
#define RO_POWER_UVLO_INFO	_IOR(IOC_MAGIC, POWER_UVLO_INFO, \
							struct mbraink_uvlo_struct_data*)

#define RO_GPU_OPP_INFO	_IOR(IOC_MAGIC, GPU_OPP_INFO, \
						struct mbraink_gpu_opp_info*)

#define RO_GPU_STATE_INFO	_IOR(IOC_MAGIC, GPU_STATE_INFO, \
						struct mbraink_gpu_state_info*)

#define RO_GPU_LOADING_INFO	_IOR(IOC_MAGIC, GPU_LOADING_INFO, \
						struct mbraink_gpu_loading_info*)

#define RO_PMIC_VOLTAGE_INFO	_IOR(IOC_MAGIC, PMIC_VOLTAGE_INFO, \
						struct mbraink_pmic_voltage_info*)

#define WO_OPERATION_MODE_INFO	_IOW(IOC_MAGIC, OPERATION_MODE_INFO, \
						struct mbraink_operation_mode_info*)
#define RO_GNSS_LP		_IOR(IOC_MAGIC, GNSS_LP_INFO, \
					struct mbraink_gnss2mbr_lp_data*)
#define RO_GNSS_MCU		_IOR(IOC_MAGIC, GNSS_MCU_INFO, \
					struct mbraink_gnss2mbr_mcu_data*)
#define RO_MMDVFS_INFO	_IOR(IOC_MAGIC, MMDVFS_INFO, \
						struct mbraink_mmdvfs_info*)

#define RO_WIFI_RATE_INFO	_IOR(IOC_MAGIC, WIFI_RATE_INFO, \
					struct mbraink_wifi2mbr_lls_rate_data*)
#define RO_WIFI_RADIO_INFO	_IOR(IOC_MAGIC, WIFI_RADIO_INFO, \
					struct mbraink_wifi2mbr_lls_radio_data*)
#define RO_WIFI_AC_INFO		_IOR(IOC_MAGIC, WIFI_AC_INFO, \
					struct mbraink_wifi2mbr_lls_ac_data*)
#define RO_WIFI_LP_INFO		_IOR(IOC_MAGIC, WIFI_LP_INFO, \
					struct mbraink_wifi2mbr_lp_ratio_data*)
#define RO_POWER_THROTTLE_HW_INFO	_IOR(IOC_MAGIC, POWER_THROTTLE_HW_INFO, \
						struct mbraink_power_throttle_hw_data*)
#define RO_LPM_STATE_INFO		_IOR(IOC_MAGIC, LPM_STATE_INFO, \
						struct mbraink_lpm_state_data*)

#define RO_UFS_INFO	_IOR(IOC_MAGIC, UFS_INFO, \
								struct mbraink_ufs_info*)
#define RO_WIFI_TXTIMEOUT_INFO	_IOR(IOC_MAGIC, WIFI_TXTIMEOUT_INFO, \
					struct mbraink_wifi2mbr_txtimeout_data*)

#define RO_VDEC_FPS		_IOR(IOC_MAGIC, VDEC_FPS_INFO, \
					struct mbraink_vdec_fps*)

#define SUSPEND_DATA	0
#define RESUME_DATA		1
#define CURRENT_DATA	2

#ifndef GENL_ID_GENERATE
#define GENL_ID_GENERATE    0
#endif

enum {
	MBRAINK_A_UNSPEC,
	MBRAINK_A_MSG,
	__MBRAINK_A_MAX,
};
#define MBRAINK_A_MAX (__MBRAINK_A_MAX - 1)

enum {
	MBRAINK_C_UNSPEC,
	MBRAINK_C_PID_CTRL,
	__MBRAINK_C_MAX,
};
#define MBRAINK_C_MAX (__MBRAINK_C_MAX - 1)

struct mbraink_data {
#define CHRDEV_NAME     "mbraink_chrdev"
	struct cdev mbraink_cdev;
	char suspend_power_info_en[2];
	long long last_suspend_timestamp;
	long long last_resume_timestamp;
	long long last_suspend_ktime;
	struct mbraink_battery_data suspend_battery_buffer;
	int client_pid;
	unsigned int feature_en;
	unsigned int pmu_en;
};

int mbraink_netlink_send_msg(const char *msg);

#endif /*end of MBRAINK_H*/
