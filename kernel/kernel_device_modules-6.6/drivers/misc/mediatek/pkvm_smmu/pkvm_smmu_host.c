// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_host.h>
#include <crypto/sha2.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "../include/pkvm_mgmt/pkvm_mgmt.h"
#include "pkvm_smmu_host.h"

int kvm_nvhe_sym(smmu_hyp_init)(const struct pkvm_module_ops *ops);

static unsigned long pkvm_module_token;
#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x)) *)(pkvm_el2_mod_va(&kvm_nvhe_sym(x), pkvm_module_token)))
int kvm_nvhe_sym(dummy_iommu_hyp_init)(const struct pkvm_module_ops *ops);
// struct kvm_iommu_ops smmu_ops
static int add_smmu_device_handler;
/* Map memory into protect VM, also unmap memory from normal VM */
int smmu_s2_protect_mapping;
int smmu_s2_protect_region_mapping;
/* Map memory into normal VM, also change memory attribute into read-only for protect VM */
int smmu_s2_protect_unmapping;
int smmu_s2_protect_region_unmapping;
/* Host share SMMU structure memory to HYP */
int smmu_share;
/* Host debug SMMU */
int smmu_host_debug;
/* SMMU VM info probe */
static int smmu_vm_info_probe_handler;
/* SMMU init function */
static int mtk_iommu_init_handler;
/* SMMU finalise */
static int smmu_finalise_handler;
/* SMMU page table merge */
static int smmu_page_table_merge;

unsigned int smmu_device_num;

int get_hvc_smmu_s2_protect_mapping(void)
{
	return smmu_s2_protect_mapping;
}
EXPORT_SYMBOL(get_hvc_smmu_s2_protect_mapping);

int get_hvc_smmu_s2_protect_unmapping(void)
{
	return smmu_s2_protect_unmapping;
}
EXPORT_SYMBOL(get_hvc_smmu_s2_protect_unmapping);

int get_hvc_smmu_share(void)
{
	return smmu_share;
}
EXPORT_SYMBOL(get_hvc_smmu_share);

int get_hvc_smmu_host_debug(void)
{
	return smmu_host_debug;
}
EXPORT_SYMBOL(get_hvc_smmu_host_debug);
/* Check substrings of compatible info match parameter str or not */
enum smmu_id { SMMU_MM = 0, SMMU_APU, SMMU_SOC, SMMU_GPU, SMMU_ID_NUM };
static bool fdt_node_check_compatible_contains(struct device_node *node,
					       const char *str)
{
	int str_len = 0;
	int compatible_num = 0;
	const char *compatible_str[20] = { NULL };

	if (!node || !str)
		return false;

	str_len = strlen(str);
	compatible_num = of_property_read_string_array(node, "compatible",
			compatible_str, 20);

	if (compatible_num < 0) {
		pr_info("%s: Can't find compatible info in this node\n",
				__func__);
		return false;
	}

	for (int compatible_idx = 0; compatible_idx < compatible_num;
			compatible_idx++) {
		const char *prop_str = NULL;
		int prop_len = 0;

		prop_str = compatible_str[compatible_idx];
		if (!prop_str)
			continue;

		prop_len = strlen(prop_str);
		if (prop_len < str_len)
			continue;

		for (int i = 0; i <= (prop_len - str_len); i++) {
			if (prop_str[i] == str[0]) {
				for (int j = 1; (i + j) < prop_len; j++) {
					if (prop_str[i + j] != str[j])
						break;
					if (j == (str_len - 1))
						return true;
				}
			}
		}
	}
	return false;
}
/* useing compatible content to define the dts node is belong with which
 * subsys smmu
 */
int get_smmu_index_from_fdt(struct device_node *node)
{
	if (fdt_node_check_compatible_contains(node, "mm-smmu"))
		return SMMU_MM;

	if (fdt_node_check_compatible_contains(node, "apu-smmu"))
		return SMMU_APU;

	if (fdt_node_check_compatible_contains(node, "soc-smmu"))
		return SMMU_SOC;

	if (fdt_node_check_compatible_contains(node, "gpu-smmu"))
		return SMMU_GPU;

	pr_info("%s: There isn't subsys info in the dts Node\n", __func__);
	return -1;
}
#define NR_RTS_STR_ARRAY (50)
#define NR_SID (255)
#define IOMMU_DRIVER_MEM_PFN_MAX (100U)
#define IPA_GRAN_1GB (0U)
#define IPA_GRAN_2MB (1U)
#define IPA_GRAN_4KB (2U)

struct fmpt {
	u64 *smpt;
	u64 mem_order;
};

struct mpt {
	/* Memory used by IOMMU driver */
	struct fmpt fmpt[IOMMU_DRIVER_MEM_PFN_MAX];
	u32 mem_block_num;
};

enum setup_vm_ops {
	DEFAULT_VMID = 0,
	VM_NO_MAP_MBLOCK,
	S2_BYPASS_SID,
	VMID,
	SID,
	IDENTITY_MAP_MODE,
	IDENTITY_MAP,
	IDENTITY_UNMAP,
	IDENTITY_MAP_MBLOCK,
	IDENTITY_UNMAP_MBLOCK,
	IDENTITY_MAP_MBLOCK_EXCLUSIVE,
	IPA_GRANULE
};
void all_vm_no_map_mblock(struct device_node *node)
{
	struct device_node *mblock_node = NULL;
	const char *rts_string[NR_RTS_STR_ARRAY] = { NULL };
	struct reserved_mem *rmem = NULL;
	int nr_str = 0;

	nr_str = of_property_read_string_array(
		node, "vm-no-map-mblock", &rts_string[0], NR_RTS_STR_ARRAY);

	if (nr_str < 0)
		pr_info("%s: vm-no-map-mblock is none\n", __func__);
	else {
		for (int i = 0; i < nr_str; i++) {
			mblock_node = of_find_compatible_node(NULL, NULL,
							      rts_string[i]);
			if (!mblock_node)
				pr_info("%s :can't find %s reserve memory\n",
				       rts_string[i], __func__);
			else {
				rmem = of_reserved_mem_lookup(mblock_node);
				pkvm_el2_mod_call(smmu_vm_info_probe_handler,
						  VM_NO_MAP_MBLOCK, rmem->base,
						  rmem->size);
			}
		}
	}
}

void s2_bypass_sid(struct device_node *node)
{
	unsigned int property_array[NR_SID] = { 0U };
	int ret = 0;
	int array_size = 0;

	array_size = of_property_count_u32_elems(node, "s2-bypass-sid");

	if (array_size < 0)
		pr_info("%s: s2-bypass-sid property is none\n", __func__);
	else {
		ret = of_property_read_u32_array(node, "s2-bypass-sid",
						 property_array, array_size);
		if (ret < 0)
			pr_info("%s: s2-bypass-sid is none\n", __func__);
		else {
			for (int i = 0; i < array_size; i++) {
				pkvm_el2_mod_call(smmu_vm_info_probe_handler,
						  S2_BYPASS_SID,
						  property_array[i]);
			}
		}
	}
}

void get_dram_range(phys_addr_t *phy_dram_start, phys_addr_t *phy_dram_size)
{
	struct device_node *memory = NULL;
	const __be32 *memcell_buf = NULL;
	int len = 0, ret = 0;
	int array_size = 0;
	unsigned long long property_array_64[4] = { 0ULL };

	for_each_node_by_type (memory, "memory") {
		memcell_buf = of_get_property(memory, "reg", &len);

		if (!memcell_buf || len <= 0)
			continue;
		array_size = of_property_count_u64_elems(memory, "reg");
		ret = of_property_read_u64_array(memory, "reg",
						 property_array_64, array_size);

		if (ret < 0)
			continue;
		*phy_dram_start = property_array_64[0];
		*phy_dram_size = property_array_64[1];
	}
}

void vm_vmid_set(struct device_node *node, unsigned int *vmid)
{
	int ret;
	unsigned int property_value;

	ret = of_property_read_u32(node, "vmid", &property_value);
	if (ret)
		pr_info("%s: vmid is none\n", __func__);
	else {
		*vmid = property_value;
		pkvm_el2_mod_call(smmu_vm_info_probe_handler, VMID, *vmid);
	}
}

void vm_sid_set(struct device_node *node, unsigned int vmid)
{
	int array_size = 0;
	int ret = 0;
	unsigned int property_array[NR_SID] = { 0U };

	array_size = of_property_count_u32_elems(node, "sid");

	if (array_size < 0)
		pr_info("%s: sid property is none\n", __func__);
	else {
		ret = of_property_read_u32_array(node, "sid", property_array,
						 array_size);
		if (ret < 0)
			pr_info("%s: sid is none\n", __func__);
		else {
			for (int i = 0; i < array_size; i++) {
				pkvm_el2_mod_call(smmu_vm_info_probe_handler,
						  SID, vmid, property_array[i]);
			}
		}
	}
}

void vm_idmap_mode_set(struct device_node *node, unsigned int vmid)
{
	int ret;
	unsigned int property_value;

	ret = of_property_read_u32(node, "identity-map-mode", &property_value);

	if (ret)
		pr_info("%s: identity-map-mode is none\n", __func__);
	else
		pkvm_el2_mod_call(smmu_vm_info_probe_handler, IDENTITY_MAP_MODE,
				  vmid, property_value);
}

void vm_idmap_set(struct device_node *node, unsigned int vmid,
		  phys_addr_t phy_dram_start, phys_addr_t phy_dram_size)
{
	phys_addr_t base, size;
	unsigned int property_size, property_set, i, offset;

	if (of_get_property(node, "identity-map", &property_size)) {
		property_set = property_size / (sizeof(unsigned int) * 4);
		for (i = 0; i < property_set; i++) {
			offset = i * 2;
			if (of_property_read_u64_index(node, "identity-map",
						       offset, &base))
				break;

			if (of_property_read_u64_index(node, "identity-map",
						       offset + 1, &size))
				break;

			if (base < phy_dram_start) {
				pr_info("%s: memory range is incorrect : region start = %llx < phy_dram_start(%llx)\n",
				       __func__, base, phy_dram_start);
				base = phy_dram_start;
			}

			if (size > phy_dram_size) {
				pr_info("%s: memory range is incorrect : region size = %llx < phy_dram_size(%llx)\n",
				       __func__, size, phy_dram_size);
				size = phy_dram_size;
			}
			pkvm_el2_mod_call(smmu_vm_info_probe_handler,
					  IDENTITY_MAP, vmid, base, size);
		}
	}
}

void vm_idunmap_set(struct device_node *node, unsigned int vmid,
		    phys_addr_t phy_dram_start, phys_addr_t phy_dram_size)
{
	phys_addr_t base, size;
	unsigned int property_size, property_set, i, offset;

	if (of_get_property(node, "identity-unmap", &property_size)) {
		property_set = property_size / (sizeof(unsigned int) * 4);
		for (i = 0; i < property_set; i++) {
			offset = i * 4;
			if (of_property_read_u64_index(node, "identity-unmap",
						       offset, &base))
				break;
			if (of_property_read_u64_index(node, "identity-unmap",
						       offset + 1, &size))
				break;

			if (base < phy_dram_start) {
				pr_info("%s: memory range is incorrect : region start = %llx < phy_dram_start(%llx)\n",
				       __func__, base, phy_dram_start);
				base = phy_dram_start;
			}

			if (size > phy_dram_size) {
				pr_info("%s:memory range is incorrect : region size = %llx < phy_dram_size(%llx)\n",
				       __func__, size, phy_dram_size);
				size = phy_dram_size;
			}
			pkvm_el2_mod_call(smmu_vm_info_probe_handler,
					  IDENTITY_UNMAP, vmid, base, size);
		}
	}
}

void vm_idmap_mblock_set(struct device_node *node, unsigned int vmid)
{
	unsigned int i;
	struct device_node *mblock_node;
	const char *rts_string[NR_RTS_STR_ARRAY] = {};
	struct reserved_mem *rmem = NULL;
	int nr_str = 0;

	nr_str = of_property_read_string_array(
		node, "identity-map-mblock", &rts_string[0], NR_RTS_STR_ARRAY);

	if (nr_str < 0)
		pr_info("%s: identity-map-mblock is none\n", __func__);
	else {
		for (i = 0; i < nr_str; i++) {
			mblock_node = of_find_compatible_node(NULL, NULL,
							      rts_string[i]);
			if (!mblock_node)
				pr_info("%s: can't find %s reserve memory\n",
				       __func__, rts_string[i]);
			else {
				rmem = of_reserved_mem_lookup(mblock_node);
				if (rmem)
					pkvm_el2_mod_call(
						smmu_vm_info_probe_handler,
						IDENTITY_MAP_MBLOCK, vmid,
						rmem->base, rmem->size);
			}
		}
	}
}

unsigned int get_pgt_granule(struct device_node *node)
{
	unsigned int ipa_granule;
	int ret;

	ret = of_property_read_u32(node, "ipa-granule", &ipa_granule);

	if (ret) {
		ipa_granule = IPA_GRAN_1GB;
		pr_info("%s: ipa-granule is default 1GB\n", __func__);
	} else {
		if (ipa_granule == IPA_GRAN_2MB)
			pr_info("%s: ipa-granule is 2MB\n", __func__);
		else if (ipa_granule == IPA_GRAN_4KB)
			pr_info("%s: ipa-granule is 4KB\n", __func__);
		else
			pr_info("%s: ipa-granule is 1GB\n", __func__);
	}
	return ipa_granule;
}

unsigned int smmu_vm_info_probe(void)
{
	struct device_node *node;
	phys_addr_t phy_dram_start, phy_dram_size;
	unsigned int vm_num, vmid, ipa_granule;

	/* VM common info */
	node = of_find_compatible_node(NULL, NULL, "mtk,smmu-hyp-vms");

	if (!node) {
		pr_info("%s: search smmu-hyp-vms failed\n", __func__);
		return -EINVAL;
	}

	all_vm_no_map_mblock(node);
	s2_bypass_sid(node);
	get_dram_range(&phy_dram_start, &phy_dram_size);
	ipa_granule = get_pgt_granule(node);

	/* set each vm specific info */
	vm_num = 0;
	for_each_compatible_node (node, NULL, "mtk,smmu-vm") {
		if (!node) {
			pr_info("%s: can't find smmu-vm node\n", __func__);
			return -EINVAL;
		}
		/* Probe vm info and provide those info to HYP */
		vm_vmid_set(node, &vmid);
		vm_sid_set(node, vmid);
		vm_idmap_mode_set(node, vmid);
		vm_idmap_set(node, vmid, phy_dram_start, phy_dram_size);
		vm_idunmap_set(node, vmid, phy_dram_start, phy_dram_size);
		vm_idmap_mblock_set(node, vmid);
		/* set vm ipa page table mapping granule */
		pkvm_el2_mod_call(smmu_vm_info_probe_handler, IPA_GRANULE, vmid,
				  ipa_granule);
		vm_num++;
	}

	return vm_num;
}

unsigned long long mpool_mem_calculate(unsigned int vm_num)
{
	unsigned long long num_4k_pages = 0;
	phys_addr_t phy_dram_start = 0, phy_dram_size = 0;

	get_dram_range(&phy_dram_start, &phy_dram_size);
	/* MTK smmu support 4K page table granule */
	if ((phy_dram_start != 0) && (phy_dram_size != 0)) {
		/* level 1 page table */
		num_4k_pages = 1;
		/* level 2 page table */
		num_4k_pages += phy_dram_size / SZ_1G;
		/* level 3 page table */
		num_4k_pages += phy_dram_size / SZ_2M;
	} else {
		pr_info("%s: phy_dram_start or phy_dram_size is zero\n",
		       __func__);
		return 0;
	}
	num_4k_pages *= vm_num;
	/* MTK smmu subsys real cmdq and global ste memory */
	num_4k_pages += (SZ_2M / SZ_4K);
	num_4k_pages += (SZ_64K / SZ_4K);
	return num_4k_pages;
}

bool device_reg_probe(struct device_node *node, unsigned int *reg_start, unsigned int *reg_size)
{
	int array_size = 0;
	unsigned int reg_range = 0U, reg[8] = { 0U };
	bool ret = false;

	array_size = of_property_count_u32_elems(node, "reg");
	if (array_size < 0)
		return ret;

	if (of_property_read_u32_array(node, "reg", reg, array_size))
		pr_info("%s: read reg value fail\n", __func__);
	else {
		/* Add up device's reg size */
		for (int j = 3; j < array_size; j = j + 4)
			reg_range += reg[j];
		*reg_start = reg[1];
		*reg_size = reg_range;
		ret = true;
	}
	return ret;
}

bool is_device_coherent(struct device_node *node)
{
	if (of_property_read_bool(node, "dma-coherent"))
		return true;
	return false;
}

struct device_node *get_dev_node_by_alias(struct device_node *alias_node, unsigned int i)
{
	char name[64] = "";
	const char *path = NULL;
	struct device_node *node = NULL;

	if (sprintf(name, "mtksmmu%d", i) < 0)
		return node;

	if(!of_property_read_string(alias_node, name, &path))
		node = of_find_node_by_path(path);
	else
		pr_info("%s: search device node by aliases name :%s fail\n",  __func__, name);
	return node;
}

void smmu_device_probe(void)
{
	struct device_node *node = NULL, *alias_node = NULL;
	unsigned int reg_start = 0U, reg_size = 0U;
	bool dma_coherent = false;
	int smmu_dev_id = 0;

	alias_node = of_find_node_by_path("/aliases");

	if (!alias_node) {
		pr_info("%s: search alias fail\n", __func__);
		return;
	}

	for (unsigned int i = 0; i < SMMU_ID_NUM; i++) {
		node = get_dev_node_by_alias(alias_node, i);
		if (!node) {
			pr_info("%s: search device node by path fail\n", __func__);
			continue;
		}

		if (device_reg_probe(node, &reg_start, &reg_size)) {
			smmu_dev_id = get_smmu_index_from_fdt(node);
			if (smmu_dev_id >= 0) {
				dma_coherent = is_device_coherent(node);
				pkvm_el2_mod_call(add_smmu_device_handler,
						reg_start, reg_size, dma_coherent, smmu_dev_id);
			}
		}
	}
}

void smmu_setup_hvc(void)
{
	add_smmu_device_handler = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(add_smmu_device), pkvm_module_token);

	smmu_vm_info_probe_handler = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(setup_vm), pkvm_module_token);

	mtk_iommu_init_handler = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_iommu_init), pkvm_module_token);

	smmu_finalise_handler = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(smmu_finalise), pkvm_module_token);

}

void smmu_host_hvc(void)
{
	struct arm_smccc_res res;
	/* register page base smmu mapping function into HYP */
	smmu_s2_protect_mapping = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_smmu_secure_v2), pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC, SMC_ID_MTK_PKVM_SMMU_SEC_MAP,
			  smmu_s2_protect_mapping, 0, 0, 0, 0, &res);
	/* register page base smmu unmapping function into HYP */
	smmu_s2_protect_unmapping = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_smmu_unsecure_v2), pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_SEC_UNMAP, smmu_s2_protect_unmapping, 0, 0, 0, 0, &res);
	/* register region base smmu mapping function into HYP */
	smmu_s2_protect_region_mapping = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_smmu_secure), pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_SEC_REGION_MAP,
			  smmu_s2_protect_region_mapping, 0, 0, 0, 0, &res);
	/* register region base smmu unmapping function into HYP */
	smmu_s2_protect_region_unmapping = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_smmu_unsecure), pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_SEC_REGION_UNMAP,
			  smmu_s2_protect_region_unmapping, 0, 0, 0, 0, &res);
	/* register function of host share smmu structure to hyp into HYP */
	smmu_share = pkvm_register_el2_mod_call(kvm_nvhe_sym(mtk_smmu_share),
						pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_MEM_SHARE, smmu_share, 0, 0, 0,
			  0, &res);
	/* register host smmu debug function into HYP */
	smmu_host_debug = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(mtk_smmu_host_debug), pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_DEBUG_DUMP, smmu_host_debug, 0,
			  0, 0, 0, &res);
	/* register merge smmu s2 page table function into HYP */
	smmu_page_table_merge = pkvm_register_el2_mod_call(
		__kvm_nvhe_smmu_merge_s2_table, pkvm_module_token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_SMMU_PAGE_TABLE_MERGE,
			  smmu_page_table_merge, 0, 0, 0, 0, &res);
}

void smmu_alloc_memory(struct mpt *mpt, unsigned long long target_page_counts,
		       unsigned long long *acc_page_num)
{
	/* allocate memory from 2M contious memory to 4KB contious memory */
	for (int page_order = 9; page_order >= 0; page_order--) {
		unsigned long addr = 0UL;

		if ((target_page_counts - *acc_page_num) <
		    (1ULL << page_order)) {
			pr_info("%s: left %#llx < request page  %#llx\n",
			       __func__, (target_page_counts - *acc_page_num),
			       (1ULL << page_order));
			continue;
		}
		addr = __get_free_pages(GFP_KERNEL, page_order);

		if (addr) {
			mpt->fmpt[mpt->mem_block_num].smpt = (u64 *)addr;
			mpt->fmpt[mpt->mem_block_num].mem_order = page_order;
			*acc_page_num += (1ULL << page_order);
			mpt->mem_block_num++;
			break;
		}
		pr_info("%s: get page_order %#x fail\n", __func__, page_order);
	}
}

void free_mpool_mem(struct mpt *mpt)
{
	unsigned int i;
	unsigned long addr;

	if (mpt == NULL)
		return;

	for (i = 0; i < mpt->mem_block_num; i++) {
		addr = (u64)mpt->fmpt[mpt->mem_block_num].smpt;

		if (addr)
			free_pages(addr,
				   mpt->fmpt[mpt->mem_block_num].mem_order);
	}
}

unsigned long smmu_mpool_mem_allocate(struct mpt *mpt,
				      unsigned long long target_page_counts)
{
	unsigned long addr;
	unsigned long pfn;
	unsigned long long acc_page_num = 0;

	if (sizeof(*mpt) > PAGE_SIZE) {
		pr_info("%s: mpt size %lx bigger than PAGE_SIZE\n", __func__, sizeof(*mpt));
		pr_info("%s: struct fmpt size %lx\n", __func__, sizeof(struct fmpt));
		return 0;
	}
	addr = get_zeroed_page(GFP_KERNEL);

	if (!addr) {
		pr_info("%s: alloc page fail\n", __func__);
		return 0;
	}

	mpt = (struct mpt *)addr;
	mpt->mem_block_num = 0;

	if (target_page_counts == 0)
		return 0;

	pr_info("%s: target_page_counts %#llx\n", __func__, target_page_counts);

	while (acc_page_num < target_page_counts) {
		if (mpt->mem_block_num >= IOMMU_DRIVER_MEM_PFN_MAX) {
			pr_info("%s: mpt->mem_block_num: %#x over IOMMU_DRIVER_MEM_PFN_MAX: %#x. Target_page_counts %#llx but, only allocated %#llx pages\n",
			       __func__, mpt->mem_block_num,
			       IOMMU_DRIVER_MEM_PFN_MAX, target_page_counts,
			       acc_page_num);
			break;
		}
		smmu_alloc_memory(mpt, target_page_counts, &acc_page_num);
	}
	pfn = __pa(mpt) >> PAGE_SHIFT;
	return pfn;
}

static int mtk_kvm_arm_smmu_v3_init(void)
{
	int ret;
	struct kvm_hyp_memcache atomic_mc = {};

	ret = topup_hyp_memcache(&atomic_mc, 100, 0);
	if (ret)
		pr_info("topup_hyp_memcache failed ret=%d\n", ret);

	ret = kvm_iommu_init_hyp(ksym_ref_addr_nvhe(smmu_ops), &atomic_mc, 0);
	if (ret)
		pr_info("kvm_iommu_init_hyp ret=%d\n", ret);


	ret = pkvm_el2_mod_call(smmu_finalise_handler);
	if (ret)
		pr_info("smmu_finalise_handler ret=%d\n", ret);

	return ret;
}

pkvm_handle_t mtk_kvm_arm_smmu_v3_id(struct device *dev)
{
	return 0;
}

struct kvm_iommu_driver kvm_smmu_v3_ops = {
	.init_driver = mtk_kvm_arm_smmu_v3_init,
	.get_iommu_id = mtk_kvm_arm_smmu_v3_id,
};

static int __init smmu_nvhe_init(void)
{
	int ret;
	unsigned int vm_num;
	unsigned long long target_page_counts;
	unsigned long pfn;
	struct mpt *mpt = NULL;

	if (!is_protected_kvm_enabled()) {
		pr_info("Skip pKVM smmu init, cause pKVM is not enabled\n");
		return 0;
	}
	ret = pkvm_load_el2_module(kvm_nvhe_sym(smmu_hyp_init),
				   &pkvm_module_token);

	if (ret) {
		pr_info("%s: smmu hyp module init skip, because deprivilege\n",
		       __func__);
		return 0;
	}

	/* pKVM sMMU HVC handler */
	pr_info("%s: smmu_hvc_setup start\n", __func__);
	smmu_setup_hvc();
	/* setup smmu vm info */
	pr_info("%s: smmu_vm_info_probe start\n", __func__);
	vm_num = smmu_vm_info_probe();
	/* alloc mpool mem */
	pr_info("%s: mpool_mem_calculate start\n", __func__);
	target_page_counts = mpool_mem_calculate(vm_num);
	/* Allocate memory from body system for smmu mpool*/
	pr_info("%s: smmu_mpool_mem_allocate start\n", __func__);
	pfn = smmu_mpool_mem_allocate(mpt, target_page_counts);
	if (pfn == 0)
		goto free_mem;
	pr_info("%s: mtk_iommu_init_handler start\n", __func__);
	ret = pkvm_el2_mod_call(mtk_iommu_init_handler, pfn);
	if (ret) {
		pr_info("%s: smmu init fail, ret = %d\n", __func__, ret);
		goto free_mem;
	}

	/* get smmu dts info */
	pr_info("%s: smmu_device_probe start\n", __func__);
	smmu_device_probe();
	/* If we reached here, all hyp smmu setting done */
	pr_info("%s: HYP SMMU init done\n", __func__);
	/* register hvc for host intact with hypervisor smmu */
	smmu_host_hvc();
	pr_info("%s: register kvm_smmu_v3_ops\n", __func__);
	ret = kvm_iommu_register_driver(&kvm_smmu_v3_ops);
	if (ret)
		pr_info("%s: register kvm_smmu_v3_ops fail\n", __func__);

	return 0;
free_mem:
	pr_info("%s: free_mpool_mem start\n", __func__);
	free_mpool_mem(mpt);
	return ret;
}
module_init(smmu_nvhe_init);
MODULE_LICENSE("GPL");
