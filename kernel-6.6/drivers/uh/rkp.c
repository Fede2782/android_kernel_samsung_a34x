// SPDX-License-Identifier: GPL-2.0

#include <linux/rkp.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

static u64 robuffer_base __rkp_ro;
static u64 robuffer_size __rkp_ro;
#ifdef CONFIG_UH_PKVM
bool rkp_started;
u64 early_module_core_text[RKP_EARLY_MODULE] = {0,};
u64 early_module_core_size[RKP_EARLY_MODULE] = {0,};
#else
bool rkp_started __rkp_ro;
#endif

/* init/main.c */
#ifdef CONFIG_UH_PKVM
void rkp_init(void)
#else
void __init rkp_init(void)
#endif
{
	struct rkp_init init_data;

	memset((void *)&init_data, 0, sizeof(struct rkp_init));
	/* initialized rkp_init struct */
	init_data.magic = RKP_INIT_MAGIC;
	init_data._text = (u64)_stext;
	init_data._etext = (u64)_etext;
	init_data._srodata = (u64)__start_rodata;
	init_data._erodata = (u64)__end_rodata;
	init_data.init_mm_pgd = (u64)__pa(swapper_pg_dir);
	init_data.id_map_pgd = (u64)__pa(idmap_pg_dir);
	init_data.zero_pg_addr = (u64)__pa(empty_zero_page);
#ifdef CONFIG_UNMAP_KERNEL_AT_EL0
	init_data.tramp_pgd = (u64)__pa(tramp_pg_dir);
	init_data.tramp_valias = (u64)TRAMP_VALIAS;
#endif
	uh_call(UH_APP_RKP, RKP_START, (u64)&init_data, 0, 0, 0);
	rkp_started = true;
}

/* init/main.c */
#ifdef CONFIG_UH_PKVM
void rkp_early_module_support(void)
{
	struct module_info early_module_info[RKP_EARLY_MODULE];

	for (int i = 0; i < RKP_EARLY_MODULE; i++) {
		early_module_info[i].base_va = 0;
		early_module_info[i].vm_size = 0;
		early_module_info[i].core_base_va = early_module_core_text[i];
		early_module_info[i].core_text_size = early_module_core_size[i];
		early_module_info[i].core_ro_size = 0;
		early_module_info[i].init_base_va = 0;
		early_module_info[i].init_text_size = 0;
		uh_call(UH_APP_RKP, RKP_MODULE_LOAD, RKP_MODULE_PXN_CLEAR, (u64)&early_module_info[i], 0, 0);
		pr_info("early_module_info[%d] va = %llx", i, (u64)&early_module_info[i]);
	}
}
#endif

void rkp_deferred_init(void)
{
	uh_call(UH_APP_RKP, RKP_DEFERRED_START, 0, 0, 0, 0);
#ifdef CONFIG_UH_PKVM
	rkp_early_module_support();
#endif
}

/* RO BUFFER */
void rkp_robuffer_init(void)
{
	uh_call(UH_APP_RKP, RKP_GET_RO_INFO, (u64)&robuffer_base, (u64)&robuffer_size, 0, 0);
	pr_info("robuffer_base: %llx, robuffser_size: %llx\n", robuffer_base, robuffer_size);
}

inline phys_addr_t rkp_ro_alloc_phys(int shift)
{
	phys_addr_t alloc_addr = 0;

#ifdef CONFIG_UH_PKVM
	if (rkp_started)
#endif
		uh_call(UH_APP_RKP, RKP_ROBUFFER_ALLOC, (u64)&alloc_addr, 1, 0, 0);

	return alloc_addr;
}

inline phys_addr_t rkp_ro_alloc_phys_for_text(void)
{
	phys_addr_t alloc_addr = 0;

#ifdef CONFIG_UH_PKVM
	if (rkp_started)
#endif
		uh_call(UH_APP_RKP, RKP_ROBUFFER_ALLOC, (u64)&alloc_addr, 1, 1, 0);

	return alloc_addr;
}

inline void *rkp_ro_alloc(void)
{
	void *addr = NULL;

#ifdef CONFIG_UH_PKVM
	if (!rkp_started)
		return NULL;
#endif

	uh_call(UH_APP_RKP, RKP_ROBUFFER_ALLOC, (u64)&addr, 1, 0, 0);
	if (!addr)
		return 0;

	return (void *)__phys_to_virt(addr);
}

inline void rkp_ro_free(void *addr)
{
#ifdef CONFIG_UH_PKVM
	if (rkp_started)
#endif
		uh_call(UH_APP_RKP, RKP_ROBUFFER_FREE, (u64)addr, 0, 0, 0);
}

inline bool is_rkp_ro_buffer(u64 addr)
{
	u64 va = (u64)phys_to_virt(robuffer_base);

	if ((va <= addr) && (addr < va + robuffer_size))
		return true;
	else
		return false;
}
