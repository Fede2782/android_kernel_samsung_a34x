/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_POWER_H
#define MBRAINK_POWER_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

int mbraink_power_init(void);
int mbraink_power_deinit(void);

void mbraink_power_get_voting_info(struct mbraink_voting_struct_data *mbraink_vcorefs_src);

int mbraink_get_power_info(char *buffer, unsigned int size, int datatype);
int mbraink_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo);

void mbraink_get_power_wakeup_info(struct mbraink_power_wakeup_data *wakeup_info_buffer);

int mbraink_power_get_spm_info(struct mbraink_power_spm_raw *power_spm_buffer);
int mbraink_power_get_spm_l1_info(long long *spm_l1_array, int spm_l1_size);
int mbraink_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info);

int mbraink_power_get_scp_info(struct mbraink_power_scp_info *scp_info);

int mbraink_power_get_modem_info(struct mbraink_modem_raw *modem_buffer);

int mbraink_power_get_spmi_info(struct mbraink_spmi_struct_data *mbraink_spmi_data);

int mbraink_power_get_uvlo_info(struct mbraink_uvlo_struct_data *mbraink_uvlo_data);

int mbraink_power_get_pmic_voltage_info(struct mbraink_pmic_voltage_info *pmicVoltageInfo);

void mbraink_power_suspend_prepare(void);

void mbraink_power_post_suspend(void);

int mbraink_power_get_mmdvfs_info(struct mbraink_mmdvfs_info *mmdvfsInfo);

int mbraink_power_get_power_throttle_hw_info(struct mbraink_power_throttle_hw_data *power_throttle_hw_data);

int mbraink_power_get_lpmstate_info(struct mbraink_lpm_state_data *lpmStateInfo);

#endif /*end of MBRAINK_POWER_H*/
