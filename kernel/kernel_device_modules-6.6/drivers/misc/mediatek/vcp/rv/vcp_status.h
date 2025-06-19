/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef VCP_STATUS_H
#define VCP_STATUS_H

#include "vcp.h"

typedef int (*mminfra_pwr_ptr)(void);
typedef void (*mminfra_dump_ptr)(void);
typedef phys_addr_t (*vcp_get_reserve_mem_phys_fp)(enum vcp_reserve_mem_id_t id);
typedef phys_addr_t (*vcp_get_reserve_mem_virt_fp)(enum vcp_reserve_mem_id_t id);
typedef phys_addr_t (*vcp_get_reserve_mem_size_fp)(enum vcp_reserve_mem_id_t id);
typedef void __iomem *(*vcp_get_sram_virt_fp)(void);
typedef int (*vcp_register_feature_fp)(enum feature_id id);
typedef int (*vcp_deregister_feature_fp)(enum feature_id id);
typedef unsigned int (*is_vcp_ready_fp)(enum feature_id id);
typedef void (*vcp_A_register_notify_fp)(enum feature_id id, struct notifier_block *nb);
typedef void (*vcp_A_unregister_notify_fp)(enum feature_id id, struct notifier_block *nb);
typedef unsigned int (*vcp_cmd_fp)(enum feature_id id, enum vcp_cmd_id cmd_id, char *user);
typedef int (*mmup_enable_count_fp)(void);
typedef bool (*is_mmup_enable_fp)(void);
typedef unsigned int (*is_vcp_suspending_fp)(void);
typedef unsigned int (*is_vcp_ao_fp)(void);
typedef struct mtk_ipi_device *(*get_ipidev_fp)(enum feature_id id);
typedef struct device *(*vcp_get_io_device_fp)(enum VCP_IOMMU_DEV io_num);
typedef int (*vcp_register_mminfra_cb_fp)(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func);

struct vcp_status_fp {
	vcp_get_reserve_mem_phys_fp vcp_get_reserve_mem_phys;
	vcp_get_reserve_mem_virt_fp vcp_get_reserve_mem_virt;
	vcp_get_reserve_mem_size_fp vcp_get_reserve_mem_size;
	vcp_get_sram_virt_fp        vcp_get_sram_virt;
	vcp_register_feature_fp     vcp_register_feature;
	vcp_deregister_feature_fp   vcp_deregister_feature;
	is_vcp_ready_fp             is_vcp_ready;
	vcp_A_register_notify_fp    vcp_A_register_notify;
	vcp_A_unregister_notify_fp  vcp_A_unregister_notify;
	vcp_cmd_fp                  vcp_cmd;
	mmup_enable_count_fp        mmup_enable_count;
	is_mmup_enable_fp           is_mmup_enable;
	is_vcp_suspending_fp        is_vcp_suspending;
	is_vcp_ao_fp                is_vcp_ao;
	get_ipidev_fp               get_ipidev;
	vcp_get_io_device_fp        vcp_get_io_device;
	vcp_register_mminfra_cb_fp  vcp_register_mminfra_cb;
};


void vcp_set_fp(struct vcp_status_fp *fp);
struct mtk_ipi_device *vcp_get_ipidev(enum feature_id id);
phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id);
phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id);
phys_addr_t vcp_get_reserve_mem_size_ex(enum vcp_reserve_mem_id_t id);
void __iomem *vcp_get_sram_virt_ex(void);
int vcp_register_feature_ex(enum feature_id id);
int vcp_deregister_feature_ex(enum feature_id id);
unsigned int is_vcp_ready_ex(enum feature_id id);
void vcp_A_register_notify_ex(enum feature_id id, struct notifier_block *nb);
void vcp_A_unregister_notify_ex(enum feature_id id, struct notifier_block *nb);
unsigned int vcp_cmd_ex(enum feature_id id, enum vcp_cmd_id cmd_id, char *user);
int mmup_enable_count_ex(void);
bool is_mmup_enable_ex(void);
unsigned int is_vcp_suspending_ex(void);
unsigned int is_vcp_ao_ex(void);
int vcp_register_mminfra_cb_ex(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func);
struct device *vcp_get_io_device_ex(enum VCP_IOMMU_DEV io_num);

#endif
