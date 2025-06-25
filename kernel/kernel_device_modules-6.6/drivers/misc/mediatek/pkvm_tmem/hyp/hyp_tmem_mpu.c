// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "hyp_tmem_mpu.h"

#include "hyp_pmm.h"

#ifdef memset
#undef memset
#endif

extern const struct pkvm_module_ops *tmem_ops;
#define CALL_FROM_OPS(fn, ...) tmem_ops->fn(__VA_ARGS__)

void *memset(void *dst, int c, size_t count)
{
	return CALL_FROM_OPS(memset, dst, c, count);
}

/* record for MPU addr/size */
struct mpu_record {
	uint64_t addr;
	uint64_t size;
};

static struct mpu_record pkvm_mpu_rec[MPU_REQ_ORIGIN_EL2_ZONE_MAX];

static inline bool is_zone_valid(uint32_t mpu_zone)
{
	return (mpu_zone < MPU_REQ_ORIGIN_EL2_ZONE_MAX);
}

static inline bool is_range_aligned(uint64_t addr, uint64_t size)
{
	return (((addr | size) & PLATFORM_MPU_ALIGN_MASK) == 0);
}

static uint32_t get_zone_info(uint32_t mpu_zone, uint32_t op)
{
	uint32_t zone_info;

	zone_info = (op & 0x0000FFFF);
	zone_info |= ((mpu_zone & 0x0000FFFF) << 16);

	return zone_info;
}

uint64_t platform_mpu_get_alignment(void)
{
	return (0x1 << PLATFORM_MPU_ALIGN_BITS);
}

static uint32_t sip_smc_mpu_request(uint32_t zone,
		uint64_t pa, uint64_t sz, bool is_set)
{
	uint32_t enc_pa = (uint32_t)(pa >> PLATFORM_MPU_ALIGN_BITS);
	uint32_t zone_info = get_zone_info(zone, (is_set) ? 1 : 0);
	struct arm_smccc_res smc_res;

	arm_smccc_1_1_smc(MTK_SIP_HYP_MPU_PERM_SET_AARCH64, enc_pa, sz, zone_info,
				0, 0, 0, 0, &smc_res);

	return smc_res.a0;
}

static uint64_t platform_mpu_op(uint32_t mpu_zone, uint64_t addr, uint64_t size,
				bool is_set, const struct pkvm_module_ops *tmem_ops)
{
	uint32_t rc;

	if (!is_zone_valid(mpu_zone)) {
		tmem_ops->puts("invalid zone id\n");
		return MPU_ERROR_INVALID_ZONE;
	}

	if (!is_range_aligned(addr, size)) {
		tmem_ops->puts("unaligned range addr\n");
		return MPU_ERROR_ALIGNMENT;
	}

	rc = sip_smc_mpu_request(mpu_zone, addr, size, is_set);
	if (rc) {
		tmem_ops->puts("failed to sip_smc_mpu_request\n");
		return MPU_ERROR_PERM_SET_SMC_FAIL;
	}

	return MPU_SUCCESS;
}

uint64_t platform_mpu_set(uint32_t mpu_zone, uint64_t addr, uint64_t size,
				const struct pkvm_module_ops *tmem_ops)
{
	return platform_mpu_op(mpu_zone, addr, size, true, tmem_ops);
}

uint64_t platform_mpu_clr(uint32_t mpu_zone, uint64_t addr, uint64_t size,
				const struct pkvm_module_ops *tmem_ops)
{
	return platform_mpu_op(mpu_zone, addr, size, false, tmem_ops);
}

static int PKVM_MPU_ShareMemProtRequest(enum MPU_REQ_ORIGIN_ZONE_ID zone_id,
					uint64_t addr, uint64_t size, bool is_enable,
					const char *dbg_tag, const struct pkvm_module_ops *tmem_ops)
{
	uint64_t rc;
	struct mpu_record *rec = &pkvm_mpu_rec[zone_id];

	if (is_enable) {
		/*
		 * EL1S2 unmap
		 */
		tmem_ops->host_stage2_mod_prot(addr >> ONE_PAGE_OFFSET, 0, size/ONE_PAGE_SIZE, 0);

		tmem_ops->puts("pkvm_tmem: platform_mpu_set\n");
		rc = platform_mpu_set(zone_id, addr, size, tmem_ops);
		if (rc) {
			tmem_ops->puts("failed to Enable MPU protection\n");
			return TZ_RESULT_ERROR_GENERIC;
		}
	} else {
		tmem_ops->puts("pkvm_tmem: platform_mpu_clr\n");
		rc = platform_mpu_clr(zone_id, rec->addr, rec->size, tmem_ops);
		if (rc) {
			tmem_ops->puts("failed to Disable MPU protection\n");
			return TZ_RESULT_ERROR_GENERIC;
		}

		/*
		 * EL1S2 map
		 */
		tmem_ops->host_stage2_mod_prot(rec->addr >> ONE_PAGE_OFFSET, 7, rec->size/ONE_PAGE_SIZE, 0);
	}

	if (is_enable) {
		rec->addr = addr;
		rec->size = size;
	}

	return TZ_RESULT_SUCCESS;
}

int enable_region_protection(uint32_t region_id, uint64_t addr, uint64_t size,
				const struct pkvm_module_ops *tmem_ops)
{
	int ret;

	switch (region_id) {
	case MPU_REQ_ORIGIN_EL2_ZONE_PROT:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_PROT,
				addr, size, true, "PKVM-PROT", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_SVP:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_SVP,
				addr, size, true, "PKVM-SVP", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_WFD:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_WFD,
				addr, size, true, "PKVM-WFD", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_TUI:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_TUI,
				addr, size, true, "PKVM-TUI", tmem_ops);
		break;
	default:
		tmem_ops->puts("wrong region_id. cannot enable MPU prot\n");
		return TZ_RESULT_ERROR_GENERIC;
	}

	return ret;
}

int disable_region_protection(uint32_t region_id,
				const struct pkvm_module_ops *tmem_ops)
{
	int ret;

	switch (region_id) {
	case MPU_REQ_ORIGIN_EL2_ZONE_PROT:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_PROT,
				0, 0, false, "PKVM-PROT", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_SVP:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_SVP,
				0, 0, false, "PKVM-SVP", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_WFD:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_WFD,
				0, 0, false, "PKVM-WFD", tmem_ops);
		break;
	case MPU_REQ_ORIGIN_EL2_ZONE_TUI:
		ret = PKVM_MPU_ShareMemProtRequest(MPU_REQ_ORIGIN_EL2_ZONE_TUI,
				0, 0, false, "PKVM-TUI", tmem_ops);
		break;
	default:
		tmem_ops->puts("wrong region_id. cannot enable MPU prot.\n");
		return TZ_RESULT_ERROR_GENERIC;
	}

	return ret;
}
