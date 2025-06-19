/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_MODULES_OPS_STRUCT_H
#define MBRAINK_MODULES_OPS_STRUCT_H

#include <mbraink_ioctl_struct_def.h>

//Memory
struct mbraink_memory_ops {
	int (*getDdrInfo)(struct mbraink_memory_ddrInfo *pMemoryDdrInfo);
	int (*getMdvInfo)(struct mbraink_memory_mdvInfo *pMemoryMdv);
	int (*get_ufs_info)(struct mbraink_ufs_info *ufs_info);
};
int register_mbraink_memory_ops(struct mbraink_memory_ops *ops);
int unregister_mbraink_memory_ops(void);

//Audio
struct mbraink_audio_ops {
	int (*setUdmFeatureEn)(bool bEnable);
	int (*getIdleRatioInfo)(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo);
};
int register_mbraink_audio_ops(struct mbraink_audio_ops *ops);
int unregister_mbraink_audio_ops(void);

//Battery
struct mbraink_battery_ops {
	void (*getBatteryInfo)(struct mbraink_battery_data *battery_buffer,
			      long long timestamp);
};
int register_mbraink_battery_ops(struct mbraink_battery_ops *ops);
int unregister_mbraink_battery_ops(void);

//Power
struct mbraink_power_ops {
	void (*getVotingInfo)(struct mbraink_voting_struct_data *mbraink_vcorefs_src);
	int (*getPowerInfo)(char *buffer, unsigned int size, int datatype);
	int (*getVcoreInfo)(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo);
	void (*getWakeupInfo)(struct mbraink_power_wakeup_data *wakeup_info_buffer);
	int (*getSpmInfo)(struct mbraink_power_spm_raw *power_spm_buffer);
	int (*getSpmL1Info)(long long *spm_l1_array, int spm_l1_size);
	int (*getSpmL2Info)(struct mbraink_power_spm_l2_info *spm_l2_info);
	int (*getScpInfo)(struct mbraink_power_scp_info *scp_info);
	int (*getModemInfo)(struct mbraink_modem_raw *modem_buffer);
	int (*getSpmiInfo)(struct mbraink_spmi_struct_data *mbraink_spmi_data);
	int (*getUvloInfo)(struct mbraink_uvlo_struct_data *mbraink_uvlo_data);
	int (*getPmicVoltageInfo)(struct mbraink_pmic_voltage_info *pmicVoltageInfo);
	void (*suspendprepare)(void);
	void (*postsuspend)(void);
	int (*getMmdvfsInfo)(struct mbraink_mmdvfs_info *mmdvfsInfo);
	int (*getPowerThrottleHwInfo)(struct mbraink_power_throttle_hw_data *power_throttle_hw_data);
	int (*getLpmStateInfo)(struct mbraink_lpm_state_data *mbraink_lpm_state);
};
int register_mbraink_power_ops(struct mbraink_power_ops *ops);
int unregister_mbraink_power_ops(void);

//GPU
struct mbraink_gpu_ops {
	int (*setFeatureEnable)(bool bEnable);
	ssize_t (*getTimeoutCounterReport)(char *pBuf);
	int (*getOppInfo)(struct mbraink_gpu_opp_info *gOppInfo);
	int (*getStateInfo)(struct mbraink_gpu_state_info *gStateInfo);
	int (*getLoadingInfo)(struct mbraink_gpu_loading_info *gLoadingInfo);
	void (*setOpMode)(int opMode);
};
int register_mbraink_gpu_ops(struct mbraink_gpu_ops *ops);
int unregister_mbraink_gpu_ops(void);

/*GPS*/
struct mbraink_gps_ops {
	void (*get_gnss_lp_data)(struct mbraink_gnss2mbr_lp_data *gnss_lp_buffer);
	void (*get_gnss_mcu_data)(struct mbraink_gnss2mbr_mcu_data *gnss_mcu_buffer);
};
int register_mbraink_gps_ops(struct mbraink_gps_ops *ops);
int unregister_mbraink_gps_ops(void);

/*WIFI*/
struct mbraink_wifi_ops {
	void (*get_wifi_rate_data)(int current_idx,
				struct mbraink_wifi2mbr_lls_rate_data *rate_data);
	void (*get_wifi_radio_data)(struct mbraink_wifi2mbr_lls_radio_data *radio_data);
	void (*get_wifi_ac_data)(struct mbraink_wifi2mbr_lls_ac_data *ac_data);
	void (*get_wifi_lp_data)(struct mbraink_wifi2mbr_lp_ratio_data *lp_data);
	void (*get_wifi_txtimeout_data)(int current_idx,
				struct mbraink_wifi2mbr_txtimeout_data *txtimeout_data);
};
int register_mbraink_wifi_ops(struct mbraink_wifi_ops *ops);
int unregister_mbraink_wifi_ops(void);
#endif //MBRAINK_MODULES_OPS_STRUCT_H
