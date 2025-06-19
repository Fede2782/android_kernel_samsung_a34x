/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MMINFRA_DEBUG_H
#define __MTK_MMINFRA_DEBUG_H

#if IS_ENABLED(CONFIG_MTK_MMINFRA)

#define CCF_TIMEOUT0			(0x154C)
#define CCF_TIMEOUT1			(0x1550)
#define CCF_CTRL			(0x150C)
#define TIMEOUT0_EN			(1U << 0)
#define TIMEOUT1_EN			(1U << 0)
#define DIS_INT_FOR_AP			(1U << 31)
#define HWCCF_TIMEOUT_160NS		(0x10003)

#define MMINFRA_DONE			(1U << 0)
#define VCP_READY			(1U << 1)
#define MTK_POLL_HWV_VOTE_US		(2)
#define MTK_POLL_HWV_VOTE_CNT		(2500)
#define MTK_POLL_DONE_DELAY_US		(1)
#define MTK_POLL_DONE_TIMEOUT		(3000)

enum mm_power_ver {
	mm_pwr_v1 = 1, /* 1st version */
	mm_pwr_v2 = 2, /* mt6989 */
	mm_pwr_v3 = 3, /* mt6991 */
};

enum mm_power {
	MM_0 = 0,
	MM_1,
	MM_AO,
	MM_PWR_NR,
};

enum mmpc_sta {
	MM_DDRSRC = 0,
	MM_EMI,
	MM_BUSPLL,
	MM_INFRA,
	MM_CK26M,
	MM_PMIC,
	MM_VCORE,
	MMPC_NR,
};


struct mminfra_hw_voter {
	void __iomem	*vlp_base;
	void __iomem	*hw_ccf_base;
	u32		vlp_base_pa;
	u32		set_ofs;
	u32		clr_ofs;
	u32		en_ofs;
	u32		en_shift;
	u32		done_bits_ofs;
	bool		get_if_in_use_ena;
};

struct mtk_mminfra_pwr_ctrl {
	spinlock_t		lock;
	struct mminfra_hw_voter	hw_voter;
	atomic_t		ref_cnt;
};

int mtk_mminfra_dbg_hang_detect(const char *user, bool skip_pm_runtime);

void mtk_mminfra_off_gipc(void);

#else

static inline int mtk_mminfra_dbg_hang_detect(const char *user, bool skip_pm_runtime)
{
	return 0;
}

static inline void mtk_mminfra_off_gipc(void) { }

#endif /* CONFIG_MTK_MMINFRA */

#endif /* __MTK_MMINFRA_DEBUG_H */
