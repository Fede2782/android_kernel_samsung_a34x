// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <asm/kvm_emulate.h>
#include <asm/kvm_host.h>
#include "mkp_handler.h"
#include "handle.h"

#ifdef memset
#undef memset
#endif

#ifdef memcpy
#undef memcpy
#endif

/* Default fix range */
u64 FIX_END = ((uint64_t)0xfffffffefea00000);
u64 FIX_START = ((uint64_t)0xfffffffefe438000);

/* A64 TOP-LEVEL */
#define A64_TL_LOAD_STORE_OP0		(0b0100 << 25)
#define A64_TL_LOAD_STORE_MASK		(0b0101 << 25)

/* A64 LOAD/STORE */
#define A64_LOAD_STORE_OP0_xxx00_0011	(0b0011 << 28)
#define A64_LOAD_STORE_OP0_xxoo_MASK	(0b0011 << 28)

/* LOAD/STORE immediate post/pre-index (imm9) */
#define A64_LOAD_STORE_OP3_21_0		(0b0 << 21)
#define A64_LOAD_STORE_OP3_21_1		(0b1 << 21)
#define A64_LOAD_STORE_OP3_21_MASK	(0b1 << 21)
#define A64_LOAD_STORE_OP4_10_1		(0b1 << 10)
#define A64_LOAD_STORE_OP4_10_MASK	(0b1 << 10)

/* LOAD/STORE register offset */
#define A64_LOAD_STORE_OP4_10		(0b10 << 10)
#define A64_LOAD_STORE_OP4_MASK		(0b11 << 10)

/* LOAD/STORE unsigned immediate (imm12) */
#define A64_LOAD_STORE_OP2_24_1		(0b1 << 24)
#define A64_LOAD_STORE_OP2_24_MASK	(0b1 << 24)

/* It's a STORE inst */
#define A64_LOAD_STORE_OPC_00		(0b00 << 22)
#define A64_LOAD_STORE_OPC_MASK		(0b11 << 22)

/* size */
#define A64_LOAD_STORE_SIZE_MASK	(0b11)
#define A64_LOAD_STORE_SIZE_SHIFT	(30)

/* 0:post-index, 1:pre-index */
#define A64_LOAD_STORE_INDEX_11_0	(0b0 << 11)
#define A64_LOAD_STORE_INDEX_11_MASK	(0b1 << 11)

/* STORE encoding */
#define A64_LOAD_STORE_XT_MASK		(0b11111)
#define A64_LOAD_STORE_XN_MASK		(0b11111)
#define A64_LOAD_STORE_XM_MASK		(0b11111)
#define A64_LOAD_STORE_XN_SHIFT		(5)
#define A64_LOAD_STORE_XM_SHIFT		(16)
#define A64_LOAD_STORE_IMM_MASK		(0b111111111)
#define A64_LOAD_STORE_IMM_SHIFT	(12)
#define A64_LOAD_STORE_IMM12_MASK	(0b111111111111)
#define A64_LOAD_STORE_IMM12_SHIFT	(10)

static u64 gva_to_par_ipa(u64 va)
{
#define PAR_PA47_MASK	((1UL << 48) - 1)
#define PAR_PA12	(12)

	u64 par, tmp;

	tmp = read_sysreg(PAR_EL1);

	asm volatile (
	"at s1e1r, %0\n"
	:
	: "r" (va)
	: "memory");

	isb();

	par = read_sysreg(PAR_EL1);
	write_sysreg(tmp, PAR_EL1);

	// trace_hyp_printk("[MKP] gva_to_par_ipa:%d - PAR_EL1:0x%llx", __LINE__, par);

	return (par & PAR_PA47_MASK) >> PAR_PA12 << PAR_PA12;
}

// TODO: MAYBE_UNUSED
static void dump_info(int line, struct user_pt_regs *regs, u32 *ptr)
{
/*
 *	trace_hyp_printk("******************************************");
 *	trace_hyp_printk("**************************** dump_info ***");
 *	trace_hyp_printk("%d:*fault(0x%lx)", line, *(uint32_t *)ptr);
 *	trace_hyp_printk(" X0:%16llx  X1:%16llx", regs->regs[0], regs->regs[1]);
 *	trace_hyp_printk(" X2:%16llx  X3:%16llx", regs->regs[2], regs->regs[3]);
 *	trace_hyp_printk(" X4:%16llx  X5:%16llx", regs->regs[4], regs->regs[5]);
 *	trace_hyp_printk(" X6:%16llx  X7:%16llx", regs->regs[6], regs->regs[7]);
 *	trace_hyp_printk(" X8:%16llx  X9:%16llx", regs->regs[8], regs->regs[9]);
 *	trace_hyp_printk("X10:%16llx X11:%16llx", regs->regs[10], regs->regs[11]);
 *	trace_hyp_printk("X12:%16llx X13:%16llx", regs->regs[12], regs->regs[13]);
 *	trace_hyp_printk("X14:%16llx X15:%16llx", regs->regs[14], regs->regs[15]);
 *	trace_hyp_printk("X16:%16llx X17:%16llx", regs->regs[16], regs->regs[17]);
 *	trace_hyp_printk("X18:%16llx X19:%16llx", regs->regs[18], regs->regs[19]);
 *	trace_hyp_printk("X20:%16llx X21:%16llx", regs->regs[20], regs->regs[21]);
 *	trace_hyp_printk("X22:%16llx X23:%16llx", regs->regs[22], regs->regs[23]);
 *	trace_hyp_printk("X24:%16llx X25:%16llx", regs->regs[24], regs->regs[25]);
 *	trace_hyp_printk("X26:%16llx X27:%16llx", regs->regs[26], regs->regs[27]);
 *	trace_hyp_printk("X28:%16llx X29:%16llx", regs->regs[28], regs->regs[29]);
 *	trace_hyp_printk("X30:%16llx  PC:%16llx  CPSR:%16llx",
 *		regs->regs[30], regs->pc, read_sysreg_el2(SYS_SPSR));
 *		trace_hyp_printk("******************************************");
 */
}

/* Return true if it's a post-index */
static bool decode_STR_Xt_Xn_imm9(u32 inst, u32 *t, u32 *n, int *imm)
{
	*t = inst & A64_LOAD_STORE_XT_MASK;
	*n = (inst >> A64_LOAD_STORE_XN_SHIFT) & A64_LOAD_STORE_XN_MASK;
	*imm = (inst >> A64_LOAD_STORE_IMM_SHIFT) & A64_LOAD_STORE_IMM_MASK;

	// sign extension
	if (*imm >= 256)
		*imm = *imm - 512;

	// 0:post-index, 1:pre-index
	if ((inst & A64_LOAD_STORE_INDEX_11_MASK) == A64_LOAD_STORE_INDEX_11_0)
		return true;

	return false;
}

static void decode_STR_Xt_Xn_imm12(u32 inst, u32 *t, u32 *n, int *imm)
{
	*t = inst & A64_LOAD_STORE_XT_MASK;
	*n = (inst >> A64_LOAD_STORE_XN_SHIFT) & A64_LOAD_STORE_XN_MASK;
	*imm = (inst >> A64_LOAD_STORE_IMM12_SHIFT) & A64_LOAD_STORE_IMM12_MASK;
}

static void decode_STR_Xt_Xn_Xm(u32 inst, u32 *t, u32 *n, u32 *m)
{
	*t = inst & A64_LOAD_STORE_XT_MASK;
	*n = (inst >> A64_LOAD_STORE_XN_SHIFT) & A64_LOAD_STORE_XN_MASK;
	*m = (inst >> A64_LOAD_STORE_XM_SHIFT) & A64_LOAD_STORE_XM_MASK;
}

static void apply_to_memory(void *dst, const void *src, u32 xt, u32 size)
{
#define ZERO_REG	(0x1F)
	if (xt == ZERO_REG) {
		switch (size) {
		case 8:
			asm volatile (
		"	str xzr, [%0]\n"
			: "+r" (dst)
			:
			: "memory");
			break;
		case 4:
			asm volatile (
		"	str wzr, [%0]\n"
			: "+r" (dst)
			:
			: "memory");
			break;
		case 2:
			asm volatile (
		"	strh wzr, [%0]\n"
			: "+r" (dst)
			:
			: "memory");
			break;
		case 1:
			asm volatile (
		"	strb wzr, [%0]\n"
			: "+r" (dst)
			:
			: "memory");
			break;
		default:
			module_ops->memset(dst, 0, size);
			break;

		}
	} else {
		switch (size) {
		case 8:
			asm volatile(
		"	ldr x6, [%1]\n"
		"	dmb ishld\n"
		"	str x6, [%0]\n"
			: "+r" (dst)
			: "r" (src)
			: "x6", "cc", "memory");
			break;
		case 4:
			asm volatile(
		"	ldr w6, [%1]\n"
		"	dmb ishld\n"
		"	str w6, [%0]\n"
			: "+r" (dst)
			: "r" (src)
			: "x6", "cc", "memory");
			break;
		case 2:
			asm volatile(
		"	ldrh w6, [%1]\n"
		"	dmb ishld\n"
		"	strh w6, [%0]\n"
			: "+r" (dst)
			: "r" (src)
			: "x6", "cc", "memory");
			break;
		case 1:
			asm volatile(
		"	ldrb w6, [%1]\n"
		"	dmb ishld\n"
		"	strb w6, [%0]\n"
			: "+r" (dst)
			: "r" (src)
			: "x6", "cc", "memory");
			break;
		default:
			module_ops->memcpy(dst, src, size);
			break;
		}
	}
#undef ZERO_REG
}

static int fix_store(struct user_pt_regs *regs,
		u32 inst,
		u64 el2_gpa_va,
		u32 size)
{
	u8 *ptr = NULL;
	void *fixmap_ptr = NULL;
	u32 xt, xn, xm;
	int imm;
	int scale;

	/* Is it a load/store inst */
	if ((inst & A64_TL_LOAD_STORE_MASK) != A64_TL_LOAD_STORE_OP0)
		goto no_err;

	/* op0[31:28] == xx11 */
	if ((inst & A64_LOAD_STORE_OP0_xxoo_MASK) != A64_LOAD_STORE_OP0_xxx00_0011)
		goto no_err;

	/* size should be the same as (1 << scale) */
	scale = (inst >> A64_LOAD_STORE_SIZE_SHIFT) & A64_LOAD_STORE_SIZE_MASK;
	if (size != (u32)(1 << scale))
		goto no_err;

	/* Check whether el2_gpa_va is size aligned to avoid OOB */
	if (el2_gpa_va & (u64)(size - 1))
		goto no_err;

	/* Set ptr to destination */
	ptr = (u8 *)el2_gpa_va;

	/*
	 * op2[24] == 1 (unsigned immediate)
	 * STR Xt, [Xn, #pimm]
	 */
	if ((inst & A64_LOAD_STORE_OP2_24_MASK) == A64_LOAD_STORE_OP2_24_1) {
		decode_STR_Xt_Xn_imm12(inst, &xt, &xn, &imm);
		imm <<= scale;
		// trace_hyp_printk("[MKP] Xt(%u), Xn(%u), Imm(%d)", xt, xn, imm);
		fixmap_ptr = module_ops->fixmap_map((u64)ptr);
		dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
		apply_to_memory(fixmap_ptr, &(regs->regs[xt]), xt, size);
		dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
		module_ops->fixmap_unmap();
	} else {
		/* op3[21] == 0 */
		if ((inst & A64_LOAD_STORE_OP3_21_MASK) == A64_LOAD_STORE_OP3_21_0) {
			/*
			 * op4[10] == 1 (post-indexed or pre-indexed)
			 * STR Xt, [Xn], #simm or STR Xt, [Xn, #simm]!
			 */
			if ((inst & A64_LOAD_STORE_OP4_10_MASK) == A64_LOAD_STORE_OP4_10_1) {
				// TODO: MAYBE_UNUSED
				// bool is_post = decode_STR_Xt_Xn_imm9(inst, &xt, &xn, &imm);
				decode_STR_Xt_Xn_imm9(inst, &xt, &xn, &imm);  // tmp

				// trace_hyp_printk("[MKP] Xt(%u), Xn(%u), Imm(%d)", xt, xn, imm);
				/*
				if (is_post)
					trace_hyp_printk("[MKP] post-index");
				else
					trace_hyp_printk("[MKP] pre-index");
				*/

				fixmap_ptr = module_ops->fixmap_map((u64)ptr);
				dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
				apply_to_memory(fixmap_ptr, &(regs->regs[xt]), xt, size);
				/* Update register here no matter whether it is post- or pre-indexed */
				regs->regs[xn] += imm;
				dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
				module_ops->fixmap_unmap();
			} else {
				/*
				 * op4[10] == 0 unscaled, unprevileged
				 * STR Xt, [Xn] or STR Xt, [Xn, #simm]
				 */
				// TODO: MAYBE_UNUSED
				// bool ignored = decode_STR_Xt_Xn_imm9(inst, &xt, &xn, &imm);
				decode_STR_Xt_Xn_imm9(inst, &xt, &xn, &imm);

				// trace_hyp_printk("[MKP] Xt(%u), Xn(%u), Imm(%d)", xt, xn, imm);
				// trace_hyp_printk("[MKP] ignored: %d", (int)ignored);
				fixmap_ptr = module_ops->fixmap_map((u64)ptr);
				dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
				apply_to_memory(fixmap_ptr, &(regs->regs[xt]), xt, size);
				dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
				module_ops->fixmap_unmap();
			}
		}
		/* op3[21] == 1 */
		else {
			/*
			 * op4[11:10] == 10 register offset (#amount, TBC)
			 * STR Xt, [Xn, Xm]
			 */
			if ((inst & A64_LOAD_STORE_OP4_MASK) == A64_LOAD_STORE_OP4_10) {
				decode_STR_Xt_Xn_Xm(inst, &xt, &xn, &xm);
				// trace_hyp_printk("[MKP] Xt(%u), Xn(%u), Xm(%u)", xt, xn, xm);
				fixmap_ptr = module_ops->fixmap_map((u64)ptr);
				dump_info(__LINE__, regs, (u32 *)fixmap_ptr);
				apply_to_memory(fixmap_ptr, &(regs->regs[xt]), xt, size);
				module_ops->fixmap_unmap();
			} else {
				goto no_err;
			}
		}
	}

	/* handling is done */
	return 0;
no_err:
	return -1;
}

/* Currently, it can handle write abort only */
// TODO: MAYBE_UNUSED macro
static u32 handle_low_el_dabt(struct user_pt_regs *regs, u64 fault_va, u64 pc_ipa,
		u64 el2_gpa_va, u32 size)
{
	u64 pc_el2_va = pc_ipa;
	// u64 pc;
	void *fixmap_ptr;
	u32 inst = 0;
	int ret = 0;

	// pc = read_sysreg_el2(SYS_ELR);

	if (pc_el2_va == 0) {
		module_ops->puts("handle_low_el_dabt: invalid pc_el2_va!");
		goto no_err;
	}

	fixmap_ptr = module_ops->fixmap_map((u64)pc_el2_va);

	if (!fixmap_ptr) {
		module_ops->puts("handle_low_el_dabt: fixmap failed");
		return 1;
	}

	inst = *(u32 *)fixmap_ptr;
	module_ops->fixmap_unmap();

	// trace_hyp_printk("[MKP] handle_low_el_dabt:%d pc(0x%llx) pc_el2_va(0x%llx)", __LINE__, pc, pc_el2_va);
	// trace_hyp_printk("[MKP] handle_low_el_dabt: INST(%x) fault_va(0x%llx) el2_gpa_va(0x%llx)",
	//	inst, fault_va, el2_gpa_va);

	ret = fix_store(regs, inst, el2_gpa_va, size);
	if (ret == 0)
		return 0;	/* For pKVM perm fault handler, return 0 will not trigger BUG_ON */

	module_ops->puts("handle_low_el_dabt: failed to fix_store");

no_err:
	// trace_hyp_printk("[MKP] handle_low_el_dabt:%d - INST(%x)", __LINE__, inst);
	return 1;		/* For pKVM perm fault handler, return 1 will trigger BUG_ON */
}

u32 mkp_sync_handler(struct user_pt_regs *regs)
{
	u64 esr = read_sysreg_el2(SYS_ESR);
	u64 far = read_sysreg_el2(SYS_FAR);
	u64 hpfar = read_sysreg(hpfar_el2);
	u64 el2_gpa_va = (hpfar & HPFAR_MASK) << 8 | (far & FAR_MASK);
	u64 pc_el2_va;
	int line = 0;
	u32 ret = 0;

	/*
	if (el2_gpa_va == 0) {
		trace_hyp_printk("[MKP] mkp_sync_handler:%d ESR_EL2(0x%lx) EC(0x%lx)", __LINE__,
			esr, ESR_ELx_EC(esr));
		trace_hyp_printk("[MKP] mkp_sync_handler, pc(%llx) gva(%llx) el2_gpa_va(%llx)",
			regs->pc, far, el2_gpa_va);
	}
	*/

	switch(ESR_ELx_EC(esr)) {
	case ESR_ELx_EC_DABT_LOW:
		/* Fault in MKP's protections */
		if (query_policy_from_ipa(el2_gpa_va) == MKP_POLICY_NR) {
			//trace_hyp_printk("[MKP] mkp_sync_handler:%d, %llx doesn't belong to MKP's protections.",
			//	__LINE__, el2_gpa_va);
			ret = MKP_EXCEPTION_NO_ERROR;
			goto finish;
		}

		/* Cache maintenance or fault on S2 tranlation, just return MKP_EXCEPTION_NO_ERROR */
		if ((esr & ESR_ELx_S1PTW) || (esr & ESR_ELx_CM)) {
			//trace_hyp_printk("[MKP] mkp_sync_handler:%d, ESR_EL2(0x%lx) EC(0x%lx)",
			//	__LINE__, esr, ESR_ELx_EC(esr));
			ret = MKP_EXCEPTION_NO_ERROR;
			goto finish;
		}

		/* Only support inst length of 32 - 0b1: 32, 0b0: 16 */
		if (!(esr & ESR_ELx_IL)) {
			line = __LINE__;
			break;
		}

		/* Instruction Syndrome Valid */
		if (!(esr & ESR_ELx_ISV)) {
			line = __LINE__;
			break;
		}

		/* Currently, it supports write data abort only */
		if (!(esr & ESR_ELx_WNR)) {
			line = __LINE__;
			break;
		}

		/* Get pc ipa */
		pc_el2_va = gva_to_par_ipa(regs->pc) | (regs->pc & (PAGE_SIZE - 1));

		/* Is faulting va in the range of FIXADDR_xxx */
		if (far >= FIX_END || far < FIX_START)
			goto finish;

		/* Start handling lower EL data abort */
		ret = handle_low_el_dabt(regs, far, pc_el2_va, el2_gpa_va,
			(u32)(1 << ((esr & ESR_ELx_SAS) >> ESR_ELx_SAS_SHIFT)));

		/* Handling is completed */
		goto finish;

	case ESR_ELx_EC_IABT_LOW:
	default:
		//trace_hyp_printk("[MKP] mkp_sync_handler:%d: unable to handle - ESR_EL2(0x%lx) EC(0x%lx)",
		//	__LINE__, esr, ESR_ELx_EC(esr));
		break;
	}

	/* MKP service tries to report something it fails to handle by injecting a dabt to EL1 */
	module_ops->puts("mkp_sync_handler: failed to handle at line:");
	module_ops->putx64(line);
	module_ops->puts("ESR_EL2:");
	module_ops->putx64(esr);
	module_ops->puts("EC:");
	module_ops->putx64((u64)ESR_ELx_EC(esr));
	module_ops->puts("pc:");
	module_ops->putx64((u64)regs->pc);
	module_ops->puts("gva:");
	module_ops->putx64(far);
	module_ops->puts("el2_gpa_va:");
	module_ops->putx64(el2_gpa_va);
finish:
	return ret;
}

int mkp_perm_fault_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	int ret;

	ret = mkp_sync_handler(regs);
	write_sysreg_el2(read_sysreg_el2(SYS_ELR) + 4, SYS_ELR);

	return ret;
}

void mkp_illegal_abt_notifier(struct user_pt_regs *regs)
{
	mkp_sync_handler(regs);
	write_sysreg_el2(read_sysreg_el2(SYS_ELR) + 4, SYS_ELR);
}
