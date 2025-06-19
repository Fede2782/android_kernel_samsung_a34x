/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __POLICY_H
#define __POLICY_H

#define FUNCTION_BITS	(8)
#define RESERVE_BITS	(3)
#define POLICY_BITS	(5)

#define FUNCTION_SHIFT	(0)
#define RESERVE_SHIFT	(FUNCTION_SHIFT + FUNCTION_BITS)
#define POLICY_SHIFT	(RESERVE_SHIFT + RESERVE_BITS)

#define FUNCTION_MAXNR	(1UL << (FUNCTION_BITS))
#define POLICY_MAXNR	(1UL << (POLICY_BITS))
#define FUNCTION_MASK	(FUNCTION_MAXNR - 1)
#define POLICY_MASK	(POLICY_MAXNR - 1)
#define FUNCTION_ID(x)	((x >> FUNCTION_SHIFT) & FUNCTION_MASK)
#define POLICY_ID(x)	((x >> POLICY_SHIFT) & POLICY_MASK)

/* MKP Policy ID */
enum mkp_policy_id {
	/* MKP default policies (0 ~ 15) */
	MKP_POLICY_MKP = 0,		/* Policy ID for MKP itself */
	MKP_POLICY_DRV,			/* Policy ID for kernel drivers */
	MKP_POLICY_SELINUX_STATE,	/* Policy ID for selinux_state */
	MKP_POLICY_SELINUX_AVC,		/* Policy ID for selinux_avc */
	MKP_POLICY_TASK_CRED,		/* Policy ID for task credential */
	MKP_POLICY_KERNEL_CODE,		/* Policy ID for kernel text */
	MKP_POLICY_KERNEL_RODATA,	/* Policy ID for kernel rodata */
	MKP_POLICY_KERNEL_PAGES,	/* Policy ID for other mapped kernel pages */
	MKP_POLICY_PGTABLE,		/* Policy ID for page table */
	MKP_POLICY_S1_MMU_CTRL,		/* Policy ID for stage-1 MMU control */
	MKP_POLICY_FILTER_SMC_HVC,	/* Policy ID for HVC/SMC call filtering */
	MKP_POLICY_DEFAULT_END,
	MKP_POLICY_DEFAULT_MAX = 15,

	/* Policies for vendors start from here (16 ~ 31) */
	MKP_POLICY_VENDOR_START = 16,
	MKP_POLICY_NR = POLICY_MAXNR,
};

/* Characteristic for Policy */
enum mkp_policy_char {
	NO_MAP_TO_DEVICE	= 0x00000001,
	NO_UPGRADE_TO_WRITE	= 0x00000002,
	NO_UPGRADE_TO_EXEC	= 0x00000004,
	HANDLE_PERMANENT	= 0x00000100,
	SB_ENTRY_DISORDERED	= 0x00010000,
	SB_ENTRY_ORDERED	= 0x00020000,
	ACTION_NOTIFICATION	= 0x00100000,
	ACTION_WARNING		= 0x00200000,
	ACTION_PANIC		= 0x00400000,
	ACTION_BITS		= 0x00700000,
	CHAR_POLICY_INVALID	= 0x40000000,
	CHAR_POLICY_AVAILABLE	= 0x80000000,
	POLICY_CHAR_MASK	= 0xffffffff,
};

/* Functions */
extern u32 get_policy_characteristic(enum mkp_policy_id policy);
extern bool policy_is_valid(enum mkp_policy_id policy);
extern void initialize_policy_table(void);
extern bool policy_has_no_hw_access(enum mkp_policy_id policy);
extern void stop_setup_policy(void);

/* For sharebuf */
extern bool check_sb_entry_ordered(enum mkp_policy_id policy);
extern bool check_sb_entry_disordered(enum mkp_policy_id policy);

/* Do actions when MKP service detects threats */
extern void do_policy_action(enum mkp_policy_id policy, const char *func, int ln);

#endif
