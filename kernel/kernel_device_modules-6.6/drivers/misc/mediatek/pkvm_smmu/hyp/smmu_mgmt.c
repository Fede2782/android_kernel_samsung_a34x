// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "mpool.h"
#include "smmu_mgmt.h"
#include "smmu.h"
#include "arch_spinlock.h"
#include "arm_smmuv3.h"
#include "hyp_spinlock.h"
#include "list.h"
#include <linux/arm-smccc.h>
#include "malloc.h"
#define CARVEDOUT_OUT_OF_DRAM_RANGE 0
#define TFA_SMMU_INFO_DUMP 0

struct smmu_vm_locked {
	struct smmu_vm *vm;
};

struct smmu_free_page {
	struct list_head node;
	void *page_ptr;
};

struct smmu_hyp_vms smmu_vm_data;
struct mpool smmu_mpool;

/* Global STE */
paddr_t global_ste_pa;
void *global_ste;
size_t global_ste_sz;
/* CMDQ */
static void *smmu_cmdqs[CMDQ_NUM];

uint64_t _s2_ps_bits = OAS_36BITS;
uint64_t _pa_bits = 36;
uint64_t _ias = 36;

bool smmu_skip_flush;
// extern const struct pkvm_module_ops *pkvm_smmu_ops;
static struct spinlock_t free_page_lock;
static struct list_head pending_free_list;

static inline __must_check bool nvhe_check_data_corruption(bool v)
{
	return v;
}

#define NVHE_CHECK_DATA_CORRUPTION(condition)                                  \
	nvhe_check_data_corruption(({                                          \
		bool corruption = unlikely(condition);                         \
		if (corruption) {                                              \
			if (IS_ENABLED(CONFIG_BUG_ON_DATA_CORRUPTION)) {       \
				WARN_ON(1);                                     \
			} else                                                 \
				WARN_ON(1);                                    \
		}                                                              \
		corruption;                                                    \
	}))

/* The predicates checked here are taken from lib/list_debug.c. */

__list_valid_slowpath bool __list_add_valid_or_report(struct list_head *new,
						      struct list_head *prev,
						      struct list_head *next)
{
	if (NVHE_CHECK_DATA_CORRUPTION(next->prev != prev) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev->next != next) ||
	    NVHE_CHECK_DATA_CORRUPTION(new == prev || new == next))
		return false;

	return true;
}

__list_valid_slowpath bool
__list_del_entry_valid_or_report(struct list_head *entry)
{
	struct list_head *prev, *next;

	prev = entry->prev;
	next = entry->next;

	if (NVHE_CHECK_DATA_CORRUPTION(next == LIST_POISON1) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev == LIST_POISON2) ||
	    NVHE_CHECK_DATA_CORRUPTION(prev->next != entry) ||
	    NVHE_CHECK_DATA_CORRUPTION(next->prev != entry))
		return false;

	return true;
}

#define MPOOL_ALLOC_CONTIG(sz)                                                 \
	mpool_alloc_contiguous(&smmu_mpool, PAGES(sz), 1)

#define SID_255 (255UL)

/* default map 2MB or 4KB granule for debug or test */
static void mm_vm_identity_map_gran(struct mm_ptable_b *t, paddr_t begin,
				    paddr_t end, uint32_t mode,
				    struct mpool *ppool, paddr_t *ipa,
				    uint32_t ipa_gran)
{
	uint64_t base;
	uint64_t remain_size, entry_size;

	base = begin;
	entry_size = ipa_gran;

	if (begin >= end)
		return;
	remain_size = end - base;
	do {
		mm_vm_identity_map(t, base, base + entry_size, mode,
				   &smmu_mpool, NULL);
		remain_size -= entry_size;
		base += entry_size;
	} while (remain_size > 0);
}

struct smmu_hyp_vms *get_vms_data(void)
{
	return &smmu_vm_data;
}

struct smmu_vm *get_vm(uint16_t vmid)
{
	return &get_vms_data()->vms[vmid];
}

struct smmu_vm_locked smmu_vm_lock(struct smmu_vm *vm)
{
	struct smmu_vm_locked locked = {
		.vm = vm,
	};
	sl_lock(&vm->lock);

	return locked;
}

void smmu_vm_unlock(struct smmu_vm_locked *locked)
{
	sl_unlock(&locked->vm->lock);
}

uint8_t smmu_is_dma_coherent(void)
{
	return get_vms_data()->dma_coherent;
}

void smmu_set_dma_coherent(uint8_t dma_coherent)
{
	get_vms_data()->dma_coherent = dma_coherent;
}

void smmu_set_skip_flush(bool if_flush)
{
	smmu_skip_flush = if_flush;
}

uint8_t smmu_is_skip_flush(void)
{
	return smmu_skip_flush;
}

void smmu_flush_dcache(void *ptr, uint32_t size)
{
	/* skip flush dcache if all smmu dev are dma_coherent */
	if ((has_dma_coherent_in_devices() == SMMU_ID_NUM) ||
	    smmu_is_skip_flush())
		return;

	flush_dcache_range(ptr, size);
}

void smmu_map_mpool(void)
{
	mpool_init(&smmu_mpool, PAGE_SIZE);
	mpool_enable_locks();
}

void add_mem_to_mpool(void *va, size_t size)
{
	mpool_add_chunk(&smmu_mpool, va, size);
}

void smmu_alloc_global_ste(void)
{
	/* alloc globa_ste from mpool */
	global_ste = MPOOL_ALLOC_CONTIG(GLOBAL_STE_SIZE);
	global_ste_pa = mpool_va_to_pa(global_ste);
	global_ste_sz = GLOBAL_STE_SIZE;
	get_vms_data()->ste_base = global_ste;
}

void smmu_alloc_cmdq_bufs(void)
{
	paddr_t pa;

	for (int i = 0; i < CMDQ_NUM; i++) {
		smmu_cmdqs[i] = MPOOL_ALLOC_CONTIG(CMDQ_BUF_SIZE);
		pa = mpool_va_to_pa(smmu_cmdqs[i]);
		if (!IS_ALIGNED(pa, CMDQ_BUF_SIZE)) {
			pkvm_smmu_ops->puts("cmdq no align");
			return;
		}
	}
}

void *smmu_mpool_alloc(size_t size)
{
	return (void *)MPOOL_ALLOC_CONTIG(CMDQ_BUF_SIZE);
}

void *smmu_get_global_ste(void)
{
	return global_ste;
}

paddr_t smmu_get_global_ste_pa(void)
{
	return global_ste_pa;
}

void *smmu_get_cmdq_buf(unsigned int idx)
{
	return smmu_cmdqs[idx];
}

paddr_t smmu_get_cmdq_buf_pa(unsigned int idx)
{
	return mpool_va_to_pa(smmu_cmdqs[idx]);
}

struct mm_ptable_b *smmu_get_vm_ptable(uint16_t vmid)
{
	return &get_vms_data()->vms[vmid].ptable;
}

void smmu_dump_all_vm_stage2(void)
{
	uint16_t vmid;

	for (vmid = 0; vmid < VMID_MAX; vmid++) {
		struct smmu_vm *vm = &get_vms_data()->vms[vmid];

		if (vmid >= 2 && vm->dtb_traversed == 0x0)
			continue;
		smmu_dump_vm_stage2(vmid);
	}
}

void smmu_dump_vm_stage2(uint16_t vmid)
{
	struct arm_smccc_res smc_res;
	struct smmu_vm *vm = &get_vms_data()->vms[vmid];

	if (vm->ipa_granule == IPA_GRAN_4KB) {
		pkvm_smmu_ops->puts(
			"Skip page table dump, due to This vm use 4KB granule mapping");
		return;
	}
	if (TFA_SMMU_INFO_DUMP)
		arm_smccc_1_1_smc((MTK_SIP_HYP_SMMU_CONTROL |
				   MTK_SMC_GZ_PREFIX),
				  HYP_SMMU_S2_PGTABLE_DUMP, 0, vmid, 0, 0, 0, 0,
				  &smc_res);
}

uint64_t hypmmu_lookup_entry(uint64_t ipa, int *valid, uint16_t vmid)
{
	struct smmu_vm *vm;
	uint64_t pte;

	/* according vmid to get pte info */
	vm = get_vm(vmid);
	pte = stage2_pte(&vm->ptable, ipa);
	*valid = pte & PTE_VALID_BIT;

	return pte;
}

void smmu_dtb_vm_identity_map(struct mm_ptable_b *t, struct smmu_vm *vm)
{
	uint64_t base, end, size, dram_size, pa_max;
	uint32_t mode, rwx;
	uint32_t debug_ipa_granule;
	int i;

	rwx = 0x7;
	dram_size = DEFAULT_IDENTITY_IPA_SIZE;
	pa_max = SZ_1GB + dram_size;
	for (i = 0; i < SMMU_MAP_UNMAP_REGION_NUM; i++) {
		base = vm->identity_map[i].base;
		size = vm->identity_map[i].size;
		mode = vm->identity_map_mode;
		debug_ipa_granule = vm->ipa_granule;
		if (size == 0)
			break;
		end = base + size;

		/* The first map pte type is default identity_map_mode */
		if (i == 0) {
			if (end > pa_max)
				end = pa_max;

			if (debug_ipa_granule == IPA_GRAN_2MB) {
				mm_vm_identity_map_gran(t, base, end, mode,
							&smmu_mpool, NULL,
							SZ_2MB);
			} else if (debug_ipa_granule == IPA_GRAN_4KB) {
				mm_vm_identity_map_gran(t, base, end, mode,
							&smmu_mpool, NULL,
							SZ_4KB);
			} else
				mm_vm_identity_map(t, base, end, mode,
						   &smmu_mpool, NULL);
		} else {
			mode = rwx;
			mm_vm_identity_map(t, base, end, mode, &smmu_mpool,
					   NULL);
		}
	}
}

void smmu_dtb_vm_identity_unmap(struct mm_ptable_b *t, struct smmu_vm *vm)
{
	uint64_t begin, end, base, size;

	for (int i = 0; i < SMMU_MAP_UNMAP_REGION_NUM; i++) {
		base = vm->identity_unmap[i].base;
		size = vm->identity_unmap[i].size;

		if (base == 0x0 || size == 0)
			break;
		begin = base;
		end = base + size;
		mm_vm_unmap(t, begin, end, &smmu_mpool);
	}
}

void smmu_vm_map(uint16_t vmid, paddr_t pa, uint32_t size, uint8_t mode)
{
	struct mm_ptable_b *t;
	struct smmu_vm_locked locked;
	uint8_t map_mode;

	map_mode = mode | MM_FLAG_COMMIT;
	locked = smmu_vm_lock(get_vm(vmid));
	t = &locked.vm->ptable;
	mm_vm_identity_map(t, pa, pa + size, map_mode, &smmu_mpool, NULL);
	smmu_vm_unlock(&locked);
}

void smmu_vm_unmap(uint16_t vmid, paddr_t pa, uint32_t size)
{
	struct mm_ptable_b *t;
	struct smmu_vm_locked locked;

	locked = smmu_vm_lock(get_vm(vmid));
	t = &locked.vm->ptable;
	mm_vm_unmap(t, pa, pa + size, &smmu_mpool);
	smmu_vm_unlock(&locked);
}

void smmu_vm_defragment(uint16_t vmid)
{
	struct mm_ptable_b *t;
	struct smmu_vm_locked locked;

	/* make sure that defragment procedure does not face race condition */
	locked = smmu_vm_lock(get_vm(vmid));
	t = &get_vms_data()->vms[vmid].ptable;
	mm_vm_defrag(t, &smmu_mpool);
	smmu_vm_unlock(&locked);
    /* Dump stage-2 page table to check defragment result */
	// smmu_dump_vm_stage2(vmid);
}

static void setup_dts_map_unmap(void)
{
	uint16_t vmid;
	struct mm_ptable_b *t;
	/* Init stage-2 pgd page table */
	for (vmid = 0; vmid < VMID_MAX; vmid++) {
		struct smmu_vm *vm = &get_vms_data()->vms[vmid];
		/*
		 * VMID 0 and 1 are dedicated to linux-vm-0 and protected-vm-1
		 * need to force init ptable, for the VMID >= 2 we will skip
		 * if the dtb has not traversed.
		 */
		if (vmid >= 2 && vm->dtb_traversed == 0x0)
			continue;
		t = smmu_get_vm_ptable(vmid);
		mm_vm_init(t, vmid, &smmu_mpool);

		/* Depends on DTB map/unmap regions */
		smmu_dtb_vm_identity_map(t, vm);
		smmu_dtb_vm_identity_unmap(t, vm);
	}
}

/*
 * Exclusive map :
 * map exclusive access to one VM, the other must be unampped.
 */
static void setup_dts_exclusive_map(void)
{
	int i;
	struct idt_exclusive_region *e;
	uint16_t vmid;
	struct smmu_vm *vm;
	struct mm_ptable_b *t;
	uint64_t begin, end;

	for (i = 0; i < get_vms_data()->idt_exclusive_map_idx; i++) {
		e = &get_vms_data()->idt_exclusive_map[i];

		for (vmid = 0; vmid < VMID_MAX; vmid++) {
			vm = get_vm(vmid);
			t = &vm->ptable;

			if (vmid >= 2 && vm->dtb_traversed == 0x0)
				continue;

			begin = e->base;
			end = begin + e->size;

			if (e->vmid == vmid) {
				/* Map matched VM */
				mm_vm_identity_map(t, begin, end, e->mode,
						   &smmu_mpool, NULL);
			} else {
				/* Unmap others VM */
				mm_vm_unmap(t, begin, end, &smmu_mpool);
			}
		}
	}
}

static void setup_dts_vm_no_map(void)
{
	uint16_t vmid;
	struct smmu_hyp_vms *vms;
	struct smmu_vm *vm;
	uint64_t begin, end;
	/* SMMU: VM no-map */
	for (vmid = 0; vmid < VMID_MAX; vmid++) {
		vms = get_vms_data();
		vm = &vms->vms[vmid];

		if (vmid >= 2 && vm->dtb_traversed == 0x0)
			continue;

		for (int i = 0; i < vms->idt_no_map_idx; i++) {
			begin = vms->idt_no_map[i].base;
			end = begin + vms->idt_no_map[i].size;
			mm_vm_unmap(&vm->ptable, begin, end, &smmu_mpool);
		}
	}
}

static void provide_vm_ttbr_to_tfa(void)
{
	struct arm_smccc_res smc_res;
	paddr_t normal_vm_ttbr_pa, protect_vm_ttbr_pa;

	normal_vm_ttbr_pa = get_vm(0)->ptable.root;
	protect_vm_ttbr_pa = get_vm(1)->ptable.root;
	if (TFA_SMMU_INFO_DUMP)
		arm_smccc_1_1_smc((MTK_SIP_HYP_SMMU_CONTROL |
				   MTK_SMC_GZ_PREFIX),
				  HYP_SMMU_S2_TTBR_INFO, 0, normal_vm_ttbr_pa,
				  protect_vm_ttbr_pa, 0, 0, 0, &smc_res);
}

void smmu_vms_identity_map(void)
{
	setup_dts_map_unmap();
	setup_dts_exclusive_map();
	setup_dts_vm_no_map();
	/* provide ttbr info to tfa */
	provide_vm_ttbr_to_tfa();
}

paddr_t mpool_va_to_pa(void *va)
{
	return pkvm_smmu_ops->hyp_pa(va);
}

void *mpool_pa_to_va(paddr_t pa)
{
	return pkvm_smmu_ops->hyp_va(pa);
}

void add_to_exclusive_map_region(uint16_t vmid, uint64_t base, uint64_t size,
				 uint32_t mode)
{
	struct smmu_hyp_vms *vms = get_vms_data();
	struct idt_exclusive_region *e;
	int idx = vms->idt_exclusive_map_idx;

	e = &vms->idt_exclusive_map[idx];
	e->vmid = vmid;
	e->base = base;
	e->size = size;
	e->mode = mode;
	vms->idt_exclusive_map_idx++;
}
/* Check memory address belongs to the VM */
bool address_vm_range_check(struct smmu_vm *vm, uint64_t base, uint64_t end)
{
	int idx;

	if (!vm)
		return false;

	for (idx = 0; idx < vm->identity_map_idx; idx++) {
		if (base < vm->identity_map[idx].base)
			continue;
		else if (end > (vm->identity_map[idx].base + vm->identity_map[idx].size))
			continue;
		else
			return true;
	}
	return false;
}

void add_to_map_region(struct smmu_vm *vm, uint64_t base, uint64_t size)
{
	int idx;

	idx = vm->identity_map_idx;
	vm->identity_map[idx].base = base;
	vm->identity_map[idx].size = size;
	vm->identity_map_idx++;
}

void add_to_unmap_region(struct smmu_vm *vm, uint64_t base, uint64_t size)
{
	int idx;

	idx = vm->identity_unmap_idx;
	vm->identity_unmap[idx].base = base;
	vm->identity_unmap[idx].size = size;
	vm->identity_unmap_idx++;
}

void add_to_nomap_region(uint64_t base, uint64_t size)
{
	struct smmu_hyp_vms *vms;
	int idx;

	vms = get_vms_data();
	idx = vms->idt_no_map_idx;
	vms->idt_no_map[idx].base = base;
	vms->idt_no_map[idx].size = size;
	vms->idt_no_map_idx++;
}

static uint64_t *get_step(uint16_t sid)
{
	return get_vms_data()->ste_base + (64 * sid);
}

void init_ste_valid(uint16_t sid, uint16_t valid)
{
	uint64_t *step, v;

	step = get_step(sid);
	v = (valid) ? STE_VALID : 0;
	step[0] = v;
}

void init_all_sids_to_default_vmid(uint16_t vmid)
{
	uint64_t *step;

	for (int i = 0; i < SID_CNT; i++) {
		step = get_step(i);
		step[2] = vmid;
	}
}

void init_vms(struct smmu_hyp_vms *vms)
{
	uint64_t *step;
	uint16_t sid;
	uint16_t vmid;
	int i;
	/* Assign sids for each VM in the DTS */
	for (vmid = 0; vmid < VMID_MAX; vmid++) {
		for (i = 0; i < vms->vms[vmid].sid_num; i++) {
			sid = vms->vms[vmid].sids[i];
			step = get_step(sid);
			step[2] = vmid;
		}
	}
}

void init_default_s1_bypass_s2_bypass(uint16_t sid)
{
	uint64_t *step;

	step = get_step(sid);
	/* ste.config = 0b100 (s1:bypass s2:bypass) */
	step[0] &= ~((unsigned long)STE_CFG_MASK << STE_CFG_SHIFT);
	step[0] |= COMPOSE(STE_CFG_BYPASS, STE_CFG_SHIFT, STE_CFG_MASK);
}

void init_default_s1_bypass_s2_trans(void)
{
	uint64_t *step;
	int i;

	for (i = 0; i < SID_CNT; i++) {
		step = get_step(i);
		/* ste.config = 0b110 (s1:bypass s2:translate) */
		step[0] |= COMPOSE(STE_CFG_STG2, STE_CFG_SHIFT, STE_CFG_MASK);
	}
}

void init_default_s1_s2_bypass(void)
{
	uint64_t *step;
	int i;

	for (i = 0; i < SID_CNT; i++) {
		step = get_step(i);
		/* ste.config = 0b110 (s1:bypass s2:translate) */
		step[0] |= COMPOSE(STE_CFG_BYPASS, STE_CFG_SHIFT, STE_CFG_MASK);
	}
}

void dump_ste_sid(uint32_t sid)
{
	uint64_t *step;

	step = get_step(sid);
	pkvm_smmu_ops->puts("dump_ste_sid");
	pkvm_smmu_ops->putx64(sid);
	pkvm_smmu_ops->putx64(step[0]);
	pkvm_smmu_ops->putx64(step[1]);
	pkvm_smmu_ops->putx64(step[2]);
	pkvm_smmu_ops->putx64(step[3]);
	pkvm_smmu_ops->putx64(step[4]);
	pkvm_smmu_ops->putx64(step[5]);
	pkvm_smmu_ops->putx64(step[6]);
	pkvm_smmu_ops->putx64(step[7]);
}

void dump_ste(void)
{
	struct arm_smccc_res smc_res;

	if (TFA_SMMU_INFO_DUMP)
		arm_smccc_1_1_smc(
			(MTK_SIP_HYP_SMMU_CONTROL | MTK_SMC_GZ_PREFIX),
			HYP_SMMU_GLOBAL_STE_DUMP, 0, 0, 0, 0, 0, 0, &smc_res);
}

void smmu_configure_streams(void)
{
	struct smmu_hyp_vms *hyp;
	uint16_t vmid, sid;
	struct smmu_vm *vm;
	uint64_t *step;
	int i;

	hyp = get_vms_data();
	/* Assign streams to default vmid, except SID_255 */
	for (sid = 1; sid < SID_CNT - 1; sid++) {
		vm = &hyp->vms[hyp->default_vmid];
		vmid = hyp->default_vmid;
		step = get_step(sid);
		smmuv3_config_ste_stg2(_s2_ps_bits, _pa_bits, _ias, vmid,
				       vm->ptable, step);
	}

	/* Overwrite - Assign streams depends on DTS settings */
	for (vmid = 0; vmid < VMID_MAX; vmid++) {
		vm = &get_vms_data()->vms[vmid];
		for (i = 0; i < vm->sid_num; i++) {
			sid = vm->sids[i];
			step = get_step(sid);
			smmuv3_config_ste_stg2(_s2_ps_bits, _pa_bits, _ias,
					       vmid, vm->ptable, step);
		}
	}

	/* Flush dache of the global_ste */
	smmu_flush_dcache(global_ste, global_ste_sz);
}

void smmu_init_streams(void)
{
	struct smmu_hyp_vms *vms;

	vms = get_vms_data();
	/* Init default vmid */
	init_all_sids_to_default_vmid(vms->default_vmid);

	/* Default SID range 1 to 255 are valid */
	for (int sid = 1; sid < SID_CNT; sid++)
		init_ste_valid(sid, 1);
	/* Force SID 0 to be invalidate */
	init_ste_valid(0, 0);

	/* Default s1:bypass s2:translate */
	// init_default_s1_s2_bypass();
	init_default_s1_bypass_s2_trans();
}

static void setup_s2_bypass_streams(void)
{
	int sid;
	/* SID 255 dedicated for s1:bypass s2:bypass */
	init_default_s1_bypass_s2_bypass(SID_255);

	for (sid = 0; sid < SID_CNT; sid++)
		if (get_vms_data()->s2_bypass_sid[sid])
			init_default_s1_bypass_s2_bypass(sid);
}

static void provide_global_ste_to_tfa(void)
{
	struct arm_smccc_res smc_res;

	if (TFA_SMMU_INFO_DUMP)
		arm_smccc_1_1_smc((MTK_SIP_HYP_SMMU_CONTROL |
				   MTK_SMC_GZ_PREFIX),
				  HYP_SMMU_GLOBAL_STE_BASE_INFO, 0,
				  smmu_get_global_ste_pa(), GLOBAL_STE_SIZE, 0,
				  0, 0, &smc_res);
}

void smmu_mem_init(uint level)
{
	/* Init lazzy page free list */
	INIT_LIST_HEAD(&pending_free_list);
	/* Allocate cmdq buffer first due to cmdq is 1MB alignment */
	smmu_alloc_cmdq_bufs();
	/* Allocate ste buffer */
	smmu_alloc_global_ste();
	/* VM identity map for each static VM */
	smmu_set_skip_flush(true);
	smmu_vms_identity_map();
	smmu_set_skip_flush(false);

	/* Init and configure all streams for each VM */
	smmu_init_streams();
	smmu_configure_streams();
	setup_s2_bypass_streams();
	/* provide global ste address info to tfa to dump */
	provide_global_ste_to_tfa();
	dump_ste();
}

/*
 * according to ipa, return page table contributes.
 * because ipa may mapped into multiple VM, therefore, using sid to decide which
 * vm's info is right.
 */
unsigned long smmu_s2_table_content_info(uint64_t ipa, uint32_t sid)
{
	uint64_t *step = NULL;
	uint16_t vmid = 0;
	struct smmu_vm *vm = NULL;
	struct smmu_vm_locked locked;
	uint64_t ipa_upper_bit = 0ULL, ipa_lower_bit = 0ULL, combine_ipa = 0ULL,
		 ret_value = 0ULL;

	if (sid >= SID_CNT) {
		pkvm_smmu_ops->puts("[ERROR]Invalid sid");
		return INVALID_SID_BIT;
	}
	step = get_step(sid);
	/* get sid's vmid */
	vmid = EXTRACT(step[2], STE_VMID_SHIFT, STE_VMID_MASK);

	if (vmid >= VMID_MAX) {
		pkvm_smmu_ops->puts("[ERROR]Invalid vmid");
		return INVALID_VMID_BIT;
	}
	vm = get_vm(vmid);
	/*
	 * ipa has been transformed by guest driver, therefore need the tranform ipa
	 * back
	 */
	ipa_upper_bit = (ipa & 0xfff) << 32;
	ipa_lower_bit = (ipa & (~0xfff));
	combine_ipa = (ipa_upper_bit | ipa_lower_bit);

	if (combine_ipa >= DEFAULT_IDENTITY_IPA_END) {
		pkvm_smmu_ops->puts("[ERROR]Invalid ipa");
		return INVALID_IPA_BIT;
	}
	locked = smmu_vm_lock(get_vm(vmid));
	ret_value = stage2_pte(&vm->ptable, combine_ipa);
	smmu_vm_unlock(&locked);
	return ret_value;
}

void smmu_free_to_list(void *ptr)
{
	struct smmu_free_page *p;

	p = malloc(sizeof(struct smmu_free_page));

	if (!p) {
		pkvm_smmu_ops->puts(
			"smmu_free_to_list : malloc node fail, directly free page");
		mpool_free(&smmu_mpool, ptr);
	} else {
		p->page_ptr = ptr;
		sl_lock(&free_page_lock);
		list_add(&p->node, &pending_free_list);
		sl_unlock(&free_page_lock);
	}
}

void smmu_lazy_free(void)
{
	struct smmu_free_page *node;
	struct list_head *itr;

	sl_lock(&free_page_lock);
	list_for_each (itr, &pending_free_list) {
		node = list_entry(itr, struct smmu_free_page, node);
		mpool_free(&smmu_mpool, node->page_ptr);
		free(node);
	}
	INIT_LIST_HEAD(&pending_free_list);
	sl_unlock(&free_page_lock);
}
