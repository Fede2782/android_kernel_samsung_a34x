// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chia-Mao Hung <chia-mao.hung@mediatek.com>
 */
#include <linux/module.h>
#include "vcp_status.h"
#include "vcp.h"

struct vcp_status_fp *vcp_fp;

static int __init mtk_vcp_status_init(void)
{
	return 0;
}

void vcp_set_fp(struct vcp_status_fp *fp)
{
	if (!fp)
		return;
	vcp_fp = fp;
}
EXPORT_SYMBOL_GPL(vcp_set_fp);

int mmup_enable_count_ex(void)
{
	if (!vcp_fp || !vcp_fp->mmup_enable_count)
		return 0;

	return vcp_fp->mmup_enable_count();
}
EXPORT_SYMBOL_GPL(mmup_enable_count_ex);

bool is_mmup_enable_ex(void)
{
	if (!vcp_fp || !vcp_fp->is_mmup_enable)
		return false;

	return vcp_fp->is_mmup_enable();
}
EXPORT_SYMBOL_GPL(is_mmup_enable_ex);

struct mtk_ipi_device *vcp_get_ipidev(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->get_ipidev)
		return NULL;
	return vcp_fp->get_ipidev(id);
}
EXPORT_SYMBOL_GPL(vcp_get_ipidev);

phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_phys)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_phys(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_phys_ex);

phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_virt)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_virt(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_virt_ex);

phys_addr_t vcp_get_reserve_mem_size_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_size)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_size(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_size_ex);

void __iomem *vcp_get_sram_virt_ex(void)
{
	if (!vcp_fp || !vcp_fp->vcp_get_sram_virt)
		return NULL;
	return vcp_fp->vcp_get_sram_virt();
}
EXPORT_SYMBOL_GPL(vcp_get_sram_virt_ex);

int vcp_register_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_register_feature)
		return -1;
	return vcp_fp->vcp_register_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_register_feature_ex);

int vcp_deregister_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_deregister_feature)
		return -1;
	return vcp_fp->vcp_deregister_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_deregister_feature_ex);

unsigned int is_vcp_ready_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->is_vcp_ready)
		return 0;
	return vcp_fp->is_vcp_ready(id);
}
EXPORT_SYMBOL_GPL(is_vcp_ready_ex);

unsigned int is_vcp_suspending_ex(void)
{
	if (!vcp_fp || !vcp_fp->is_vcp_suspending)
		return 0;
	return vcp_fp->is_vcp_suspending();
}
EXPORT_SYMBOL_GPL(is_vcp_suspending_ex);

unsigned int is_vcp_ao_ex(void)
{
	if (!vcp_fp || !vcp_fp->is_vcp_ao)
		return 0;
	return vcp_fp->is_vcp_ao();
}
EXPORT_SYMBOL_GPL(is_vcp_ao_ex);

void vcp_A_register_notify_ex(enum feature_id id, struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_register_notify)
		return;
	vcp_fp->vcp_A_register_notify(id, nb);
}
EXPORT_SYMBOL_GPL(vcp_A_register_notify_ex);

void vcp_A_unregister_notify_ex(enum feature_id id, struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_unregister_notify)
		return;
	vcp_fp->vcp_A_unregister_notify(id, nb);
}
EXPORT_SYMBOL_GPL(vcp_A_unregister_notify_ex);

unsigned int vcp_cmd_ex(enum feature_id id, enum vcp_cmd_id cmd_id, char *user)
{
	if (!vcp_fp || !vcp_fp->vcp_cmd)
		return 0;
	return vcp_fp->vcp_cmd(id, cmd_id, user);
}
EXPORT_SYMBOL_GPL(vcp_cmd_ex);

int vcp_register_mminfra_cb_ex(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func)
{
	if(!vcp_fp || !vcp_fp->vcp_register_mminfra_cb)
		return -1;

	vcp_fp->vcp_register_mminfra_cb(fpt_on, fpt_off, mminfra_dump_func);
	return 0;
}
EXPORT_SYMBOL_GPL(vcp_register_mminfra_cb_ex);

struct device *vcp_get_io_device_ex(enum VCP_IOMMU_DEV io_num)
{
	if (!vcp_fp || !vcp_fp->vcp_get_io_device)
		return NULL;

	return vcp_fp->vcp_get_io_device(io_num);
}
EXPORT_SYMBOL_GPL(vcp_get_io_device_ex);

static void __exit mtk_vcp_status_exit(void)
{
}
module_init(mtk_vcp_status_init);
module_exit(mtk_vcp_status_exit);
MODULE_LICENSE("GPL");
