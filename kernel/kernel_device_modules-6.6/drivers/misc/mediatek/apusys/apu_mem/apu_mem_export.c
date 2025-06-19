// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/printk.h>
#include <linux/errno.h>

#include "apu_mem_export.h"
#include "apummu_export.h"
#include "reviser_export.h"

struct apu_mem_op_set {
	struct apu_mem_export_ops *apu_mem_ops;
	bool is_OP_init;
};

static struct apu_mem_op_set apu_mem_plat_op_set;

#define APU_MEM_LOG_ERR(x, args...) \
	pr_info("[APU_MEM][error] %s " x, __func__, ##args)

int apu_mem_op_init(struct apu_mem_export_ops *apu_mem_init_ops)
{
	if (apu_mem_plat_op_set.is_OP_init) {
		APU_MEM_LOG_ERR("apu_mem op already init!!\n");
		return -EACCES;
	}

	apu_mem_plat_op_set.apu_mem_ops = apu_mem_init_ops;

	apu_mem_plat_op_set.is_OP_init = true;
	return 0;
}

/* Common API */
int apu_mem_alloc(uint32_t type, uint32_t size, uint64_t *addr, uint32_t *sid)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_alloc == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_alloc(type, size, addr, sid);
}

int apu_mem_free(uint32_t sid)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_free == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_free(sid);
}

int apu_mem_import(uint64_t session, uint32_t sid)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_import == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_import(session, sid);
}

int apu_mem_unimport(uint64_t session, uint32_t sid)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_unimport == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_unimport(session, sid);
}

int apu_mem_map(uint64_t session, uint32_t sid, uint64_t *addr)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_map == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_map(session, sid, addr);
}

int apu_mem_unmap(uint64_t session, uint32_t sid)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_unmap == NULL) {
		APU_MEM_LOG_ERR("%s is NULL!!\n", __func__);
		return -EACCES;
	}

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_unmap(session, sid);
}

/* Reviser only API */
int apu_mem_rvs_get_vlm(uint32_t request_size, bool force, unsigned long *ctx, uint32_t *tcm_size)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_vlm == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_vlm(request_size, force,
		ctx, tcm_size);
}

int apu_mem_rvs_free_vlm(uint32_t ctx)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_free_vlm == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_free_vlm(ctx);
}

int apu_mem_rvs_set_context(int type, int index, uint8_t ctx)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_set_context == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_set_context(type, index, ctx);
}

int apu_mem_rvs_get_resource_vlm(uint32_t *addr, uint32_t *size)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_resource_vlm == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_resource_vlm(addr, size);
}

int apu_mem_rvs_get_pool_size(uint32_t type, uint32_t *size)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_pool_size == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_rvs_get_pool_size(type, size);
}

/* APUMMU only API */
int apu_mem_map_iova(uint32_t type, uint64_t session, uint64_t device_va,
			uint32_t buf_size, uint64_t *eva)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_map_iova == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_map_iova(type, session,
		device_va, buf_size, eva);
}

int apu_mem_iova_decode(uint64_t eva, uint64_t *iova)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_iova_decode == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_iova_decode(eva, iova);
}

int apu_mem_unmap_iova(uint64_t session, uint64_t device_va, uint32_t buf_size)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_unmap_iova == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_unmap_iova(session, device_va, buf_size);
}

int apu_mem_table_get(uint64_t session, void **tbl_kva, uint32_t *size)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_table_get == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_table_get(session, tbl_kva, size);
}

int apu_mem_table_free(uint64_t session)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_table_free == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_table_free(session);
}

int apu_mem_DRAM_FB_alloc(uint64_t session, uint32_t vlm_size, uint32_t subcmd_num)
{
	if (apu_mem_plat_op_set.apu_mem_ops->apu_mem_DRAM_FB_alloc == NULL)
		return -EOPNOTSUPP;

	return apu_mem_plat_op_set.apu_mem_ops->apu_mem_DRAM_FB_alloc(session,
		vlm_size, subcmd_num);
}
