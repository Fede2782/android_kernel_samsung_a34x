/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include "mpool.h"
#include "arch_spinlock.h"
/* The maximum vmid */
#define STATIC_VM_NUM (16)
#define VMID_MAX (STATIC_VM_NUM)
#define SID_CNT (256)
#define SZ_32GB (32UL << 30)
#define MTK_VALID_PA_BITS (35UL)
#define SHIFT_1KB (10U)
#define SHIFT_1MB (20U)
#define SHIFT_2MB (21U)
#define SHIFT_1GB (30U)
#define SZ_4KB (4UL << SHIFT_1KB)
#define SZ_2MB (1UL << SHIFT_2MB)
#define SZ_1GB (1UL << SHIFT_1GB)
#define MAX_IPA_SPACE (1UL << MTK_VALID_PA_BITS)
#define DEFAULT_IDENTITY_IPA_BASE (0x040000000UL)
#define DEFAULT_IDENTITY_IPA_SIZE (0x7C0000000UL)
#define DEFAULT_IDENTITY_IPA_END                                               \
	(DEFAULT_IDENTITY_IPA_BASE + DEFAULT_IDENTITY_IPA_SIZE)
#define SMMU_MAP_UNMAP_REGION_NUM (64)
#define STE_BITS (512UL)
#define GLOBAL_STE_SIZE ((SID_CNT * STE_BITS) >> 3)
#define PAGES(x) (x >> 12UL)
#define CMDQ_BUF_SIZE (1UL << 19)
#define CMDQ_NUM (4)
#define PTE_VALID_BIT (1UL)
typedef unsigned long paddr_t;
typedef uintptr_t vaddr_t;
/* Porting workaround (MUST be same as hf mm.h)*/
/* WARN: mm_ptable_b must be exactly the same as mm_ptable */
struct mm_ptable_b {
	/*
	 * VMID/ASID associated with a page table. ASID 0 is reserved for use by
	 * the hypervisor.
	 */
	uint16_t id;
	/* Address of the root of the page table. */
	paddr_t root;
};
/* The following are arch-independent page mapping modes. */
#define MM_MODE_R UINT32_C(0x0001) /* read */
#define MM_MODE_W UINT32_C(0x0002) /* write */
#define MM_MODE_X UINT32_C(0x0004) /* execute */
#define MM_MODE_D UINT32_C(0x0008) /* device */
#define MM_FLAG_COMMIT 0x01
#define MM_FLAG_UNMAP 0x02
#define MM_FLAG_STAGE1 0x04
#define IPA_GRAN_1GB (0U)
#define IPA_GRAN_2MB (1U)
#define IPA_GRAN_4KB (2U)
#define STR_IPA_GRAN(gran)                                                     \
	((gran == IPA_GRAN_2MB) ? "2MB" :                                      \
	 (gran == IPA_GRAN_4KB) ? "4KB" :                                      \
				  "default")
/*
 * These structure is for smmu_mgmt.c usage
 * because some structure belong to hfanium just like spinlock.
 * it will cause some redefine problem if hfanium structure was included
 */
#define UINT32_C(c) (c##U)
#define INVALID_SID_BIT (1UL << 63)
#define INVALID_VMID_BIT (1UL << 62)
#define INVALID_IPA_BIT (1UL << 61)
#define INVALID_STE_ROW_BIT (1UL << 60)
#define INVALID_ACTION_ID_BIT (1UL << 59)
#define INVALID_SMMU_TYPE_BIT (1UL << 58)

struct ipa_range {
	uint64_t base;
	uint64_t size;
};

/* Identity exclusive region */
struct idt_exclusive_region {
	uint16_t vmid;
	uint32_t mode;
	uint64_t base;
	uint64_t size;
};

struct smmu_vm {
	struct mm_ptable_b ptable;
	struct spinlock_t lock;
	uint16_t vmid;
	uint16_t sid_num;
	uint16_t sids[SID_CNT];
	paddr_t s2_pgd;
	uint8_t dtb_traversed;
	uint8_t identity_map_mode;
	uint8_t identity_map_idx;
	struct ipa_range identity_map[SMMU_MAP_UNMAP_REGION_NUM];
	uint8_t identity_unmap_idx;
	struct ipa_range identity_unmap[SMMU_MAP_UNMAP_REGION_NUM];
	uint8_t ipa_granule;
	struct ipa_range vm_ipa_range;
};

struct smmu_hyp_vms {
	uint16_t default_vmid;
	uint64_t use_slb;
	uint64_t slb_pa;
	uint64_t slb_size;
	uint16_t sid0_invalidate;
	uint8_t dma_coherent;
	void *ste_base;
	struct smmu_vm vms[VMID_MAX];
	uint8_t idt_exclusive_map_idx;
	struct idt_exclusive_region idt_exclusive_map[SMMU_MAP_UNMAP_REGION_NUM];
	uint8_t idt_no_map_idx;
	struct ipa_range idt_no_map[SMMU_MAP_UNMAP_REGION_NUM];
	uint8_t s2_bypass_sid[SID_CNT];
};

/*
 * public for hyp_pmm.c and others
 * these api are safe to public, due to there is no any
 * hafnium structure.
 */
void smmu_assign_mpool(paddr_t pa, size_t size);
void smmu_alloc_global_ste(void);
void smmu_alloc_cmdq_bufs(void);
void smmu_vms_identity_map(void);

void *smmu_get_cmdq_buf(unsigned int idx);
paddr_t smmu_get_cmdq_buf_pa(unsigned int idx);
void *smmu_get_global_ste(void);
paddr_t smmu_get_global_ste_pa(void);

paddr_t mpool_va_to_pa(void *va);
void *mpool_pa_to_va(paddr_t pa);
int smmuv3_config_ste_stg2(uint64_t s2_ps_bits, uint64_t pa_bits, uint64_t ias,
			   uint16_t vmid, struct mm_ptable_b ptable,
			   uint64_t *ste_data);

void smmu_vm_map(uint16_t vmid, paddr_t pa, uint32_t size, uint8_t mode);
void smmu_vm_unmap(uint16_t vmid, paddr_t pa, uint32_t size);
void smmu_dump_vm_stage2(uint16_t vmid);
void smmu_vm_defragment(uint16_t vmid);
void smmu_flush_dcache(void *ptr, uint32_t size);
void dump_ste_sid(uint32_t sid);
void dump_ste(void);
void smmu_dump_all_vm_stage2(void);
void smmu_dconfig_setup(uint32_t config_param);
unsigned long smmu_s2_table_content_info(uint64_t ipa, uint32_t sid);
void smmu_lazy_free(void);
// void smmu_memory_setting(paddr_t base, void* va, size_t size);
/*
 * @brief: query smmu entry by an input ipa
 * @param ipa: query ipa
 * @param valid: report if ipa is valid? 1: valid; 0: invalid
 * @param vmid: determine querying ipa from which vm
 * @return value:  64-bit descriptor; 0: if the ipa is not mapped
 */
uint64_t hypmmu_lookup_entry(uint64_t ipa, int *valid, uint16_t vmid);
/* assign smmu structure mem */
void *smmu_alloc_structure_mem(size_t size);
void *smmu_mpool_alloc(size_t size);
void smmu_map_mpool(void);
void add_mem_to_mpool(void *va, size_t size);
void smmu_mem_init(uint level);
struct smmu_hyp_vms *get_vms_data(void);
struct smmu_vm *get_vm(uint16_t vmid);
void add_to_nomap_region(uint64_t base, uint64_t size);
void add_to_map_region(struct smmu_vm *vm, uint64_t base, uint64_t size);
void add_to_unmap_region(struct smmu_vm *vm, uint64_t base, uint64_t size);
void add_to_exclusive_map_region(uint16_t vmid, uint64_t base, uint64_t size,
				 uint32_t mode);
/* Porting workaround */
bool mm_ptable_init(struct mm_ptable_b *t, uint16_t id, int flags,
		    struct mpool *ppool);

bool mm_vm_init(struct mm_ptable_b *t, uint16_t id, struct mpool *ppool);
bool mm_vm_identity_prepare(struct mm_ptable_b *t, paddr_t begin, paddr_t end,
			    uint32_t mode, struct mpool *ppool);
bool mm_vm_identity_map(struct mm_ptable_b *t, paddr_t begin, paddr_t end,
			uint32_t mode, struct mpool *ppool, paddr_t *ipa);
void mm_vm_identity_commit(struct mm_ptable_b *t, paddr_t begin, paddr_t end,
			   uint32_t mode, struct mpool *ppool, paddr_t *ipa);
bool mm_vm_unmap(struct mm_ptable_b *t, paddr_t begin, paddr_t end,
		 struct mpool *ppool);
void mm_ptable_dump_s2(struct mm_ptable_b *t);
uint64_t stage2_pte(struct mm_ptable_b *t, uint64_t ipa);
void mm_vm_defrag(struct mm_ptable_b *t, struct mpool *ppool);
unsigned long ptable_s2_ipa_info_get(struct mm_ptable_b *t, uint64_t ipa);
int platform_mblock_query(const char *name, uint64_t *base, uint64_t *size);
bool address_vm_range_check(struct smmu_vm *vm, uint64_t base, uint64_t size);
