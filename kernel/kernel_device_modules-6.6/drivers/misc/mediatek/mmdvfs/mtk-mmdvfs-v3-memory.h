/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _MTK_MMDVFS_V3_MEMORY_H_
#define _MTK_MMDVFS_V3_MEMORY_H_

#if IS_ENABLED(CONFIG_MTK_MMDVFS) && IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
void *mmdvfs_get_mmup_base(phys_addr_t *pa);
void *mmdvfs_get_vcp_base(phys_addr_t *pa);
bool mmdvfs_get_mmup_sram_enable(void);
void __iomem *mmdvfs_get_mmup_sram(void);
bool mmdvfs_get_mmup_enable(void);
bool mmdvfs_is_init_done(void);
#else
static inline void *mmdvfs_get_mmup_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
static inline void *mmdvfs_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
static inline bool mmdvfs_get_mmup_sram_enable(void) { return false; }
static inline void __iomem *mmdvfs_get_mmup_sram(void) { return NULL; }
static inline bool mmdvfs_get_mmup_enable(void) { return false; }
static inline bool mmdvfs_is_init_done(void) { return false; }
#endif

#define MEM_BASE		(mmdvfs_get_mmup_enable() ? mmdvfs_get_mmup_base(NULL) : mmdvfs_get_vcp_base(NULL))
#define MEM_VCP_BASE		mmdvfs_get_vcp_base(NULL)
#define MEM_LOG_FLAG		(MEM_BASE + 0x0)
#define MEM_FREERUN		(MEM_BASE + 0x4)
#define MEM_VSRAM_VOL		(MEM_BASE + 0x8)
#define MEM_IPI_SYNC_FUNC(vcp)	((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0xC)
#define MEM_IPI_SYNC_DATA(vcp)	((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0x10)
#define MEM_VMRC_LOG_FLAG	(MEM_BASE + 0x14)

#define MEM_GENPD_ENABLE_USR(x)	(MEM_BASE + 0x18 + 0x4 * (x)) // CAM, VDE
#define MEM_AGING_CNT_USR(x)	(MEM_BASE + 0x20 + 0x4 * (x)) // CAM, IMG
#define MEM_FRESH_CNT_USR(x)	(MEM_BASE + 0x28 + 0x4 * (x)) // CAM, IMG
#define MEM_FORCE_OPP_PWR(x)	(MEM_BASE + 0x30 + 0x4 * (x)) // POWER_NUM
#define MEM_VOTE_OPP_PWR(x)	(MEM_BASE + 0x40 + 0x4 * (x)) // POWER_NUM
#define MEM_VOTE_OPP_USR(x)	(MEM_BASE + 0x50 + 0x4 * (x)) // USER_NUM(14)

#define MEM_MMDVFS_LP_MODE	(MEM_BASE + 0x88)
#define MEM_VMM_CEIL_ENABLE	(MEM_BASE + 0x8C)

#define MEM_CLKMUX_ENABLE	(MEM_BASE + 0x90)
#define MEM_CLKMUX_ENABLE_DONE	(MEM_BASE + 0x94)
#define MEM_VMM_EFUSE_LOW	(MEM_BASE + 0x98)
#define MEM_VMM_EFUSE_HIGH	(MEM_BASE + 0x9C)

#define MEM_VMM_OPP_VOLT(x)	(MEM_BASE + 0xA0 + 0x4 * (x)) // VMM_OPP_NUM(8)
#define MEM_VDISP_AVS_STEP(x)	(MEM_BASE + 0xC0 + 0x4 * (x)) // OPP_LEVEL(8)
#define MEM_PROFILE_TIMES	(MEM_BASE + 0xE0)

#define MEM_VCP_EXC_SEC		(MEM_BASE + 0xE4)
#define MEM_VCP_EXC_USEC	(MEM_BASE + 0xE8)
#define MEM_VCP_EXC_VAL		(MEM_BASE + 0xEC)

// USER_NUM(32)
#define MEM_USR_OPP_SEC(x, vcp)		((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0xF0 + 0x4 * (x))
#define MEM_USR_OPP_USEC(x, vcp)	((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0x170 + 0x4 * (x))
#define MEM_USR_OPP(x, vcp)		((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0x1F0 + 0x4 * (x))
#define MEM_USR_FREQ(x, vcp)		((vcp && MEM_VCP_BASE ? MEM_VCP_BASE : MEM_BASE) + 0x270 + 0x4 * (x))

#define MEM_MUX_OPP_SEC(x)	(MEM_BASE + 0x2F0 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_OPP_USEC(x)	(MEM_BASE + 0x330 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_OPP(x)		(MEM_BASE + 0x370 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_MIN(x)		(MEM_BASE + 0x3B0 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_FORCE_CLK(x)	(MEM_BASE + 0x3F0 + 0x4 * (x)) // MUX_NUM(16)

#define MEM_PWR_OPP_SEC(x)	(MEM_BASE + 0x430 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_OPP_USEC(x)	(MEM_BASE + 0x440 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_OPP(x)		(MEM_BASE + 0x450 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_CUR_GEAR(x)	(MEM_BASE + 0x460 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_FORCE_VOL(x)	(MEM_BASE + 0x470 + 0x4 * (x)) // POWER_NUM(4)

#define MEM_VMM_BUCK_ON		(MEM_BASE + 0x480)

#define MEM_PWR_TOTAL_TIME(x, y) \
	(MEM_BASE + 0x484 + 0x40 * (x) + 0x8 * (y)) // POWER_NUM(4), OPP(8) (u64)

#define MEM_MUX_CB_MUX_OPP	(MEM_BASE + 0x584)
#define MEM_MUX_CB_SEC		(MEM_BASE + 0x588)
#define MEM_MUX_CB_USEC		(MEM_BASE + 0x58C)
#define MEM_MUX_CB_END_SEC	(MEM_BASE + 0x590)
#define MEM_MUX_CB_END_USEC	(MEM_BASE + 0x594)

#define MEM_CEIL_LEVEL(x)		(MEM_BASE + 0x598 + 0x4 * (x)) // POWER_NUM(3)
/* next start: 0x5A4 */

#define MEM_REC_CNT_MAX		16

// POWER_NUM(3), CAM_ALONE
// x: POWER_NUM, y: CNT
#define MEM_REC_PWR_ALN_OBJ		3
#define MEM_REC_PWR_ALN_CNT(x)		(MEM_BASE + 0x7DC + 0xC4 * (x))
#define MEM_REC_PWR_ALN_SEC(x, y)	(MEM_BASE + 0x7E0 + 0xC4 * (x) + MEM_REC_PWR_ALN_OBJ * 0x4 * (y))
#define MEM_REC_PWR_ALN_NSEC(x, y)	(MEM_BASE + 0x7E4 + 0xC4 * (x) + MEM_REC_PWR_ALN_OBJ * 0x4 * (y))
#define MEM_REC_PWR_ALN_OPP(x, y)	(MEM_BASE + 0x7E8 + 0xC4 * (x) + MEM_REC_PWR_ALN_OBJ * 0x4 * (y))

#define MEM_REC_VMM_CEIL_OBJ		3
#define MEM_REC_VMM_CEIL_CNT		(MEM_BASE + 0xAEC)
#define MEM_REC_VMM_CEIL_SEC(x)		(MEM_BASE + 0xAF0 + MEM_REC_VMM_CEIL_OBJ * 0x4 * (x))
#define MEM_REC_VMM_CEIL_USEC(x)	(MEM_BASE + 0xAF4 + MEM_REC_VMM_CEIL_OBJ * 0x4 * (x))
#define MEM_REC_VMM_CEIL_VAL(x)		(MEM_BASE + 0xAF8 + MEM_REC_VMM_CEIL_OBJ * 0x4 * (x))

#define MEM_REC_MUX_OBJ		3
#define MEM_REC_MUX_CNT		(MEM_BASE + 0xBB0)
#define MEM_REC_MUX_SEC(x)	(MEM_BASE + 0xBB4 + MEM_REC_MUX_OBJ * 0x4 * (x))
#define MEM_REC_MUX_USEC(x)	(MEM_BASE + 0xBB8 + MEM_REC_MUX_OBJ * 0x4 * (x))
/* mux_id/opp/min/level */
#define MEM_REC_MUX_VAL(x)	(MEM_BASE + 0xBBC + MEM_REC_MUX_OBJ * 0x4 * (x))

#define MEM_REC_VMM_DBG_OBJ	5
#define MEM_REC_VMM_DBG_CNT	(MEM_BASE + 0xC74)
#define MEM_REC_VMM_SEC(x)	(MEM_BASE + 0xC78 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_NSEC(x)	(MEM_BASE + 0xC7C + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_VOLT(x)	(MEM_BASE + 0xC80 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_TEMP(x)	(MEM_BASE + 0xC84 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_AVS(x)	(MEM_BASE + 0xC88 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))

#define MEM_REC_PWR_OBJ		4
#define MEM_REC_PWR_CNT		(MEM_BASE + 0xDB8)
#define MEM_REC_PWR_SEC(x)	(MEM_BASE + 0xDBC + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_NSEC(x)	(MEM_BASE + 0xDC0 + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_ID(x)	(MEM_BASE + 0xDC4 + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_OPP(x)	(MEM_BASE + 0xDC8 + MEM_REC_PWR_OBJ * 0x4 * (x))

#define MEM_REC_USR_OBJ		5
#define MEM_REC_USR_CNT		(MEM_BASE + 0xEBC)
#define MEM_REC_USR_SEC(x)	(MEM_BASE + 0xEC0 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_NSEC(x)	(MEM_BASE + 0xEC4 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_PWR(x)	(MEM_BASE + 0xEC8 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_ID(x)	(MEM_BASE + 0xECC + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_OPP(x)	(MEM_BASE + 0xED0 + MEM_REC_USR_OBJ * 0x4 * (x))

#define MEM_SRAM_OFFSET		(MEM_BASE + 0xFFC)
#define SRAM_BASE		mmdvfs_get_mmup_sram()

#define SRAM_OBJ_CNT		(3)
#define SRAM_PWR_CNT		(4)
#define SRAM_REC_CNT		(8)
#define SRAM_MUX_CNT		(16)

enum {
	SRAM_USR_SMI, /* VMMRC */
	SRAM_USR_DPC, /* VDISPRC */
	SRAM_USR_VDE,
	SRAM_USR_VEN,
	SRAM_USR_VCP,
	SRAM_USR_CAMSV,
	SRAM_USR_NUM = 2
};

/* USR : SRAM_OBJ_CNT * SRAM_USR_NUM * SRAM_REC_CNT = 48  */
#define SRAM_REC_CNT_USR(x)	(SRAM_BASE + 4 * (0 + (x))) // SRAM_USR_NUM(2)
#define SRAM_USR_SEC(x, y)	(SRAM_BASE + 4 * (2 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_USR_NUM) + 0))
#define SRAM_USR_USEC(x, y)	(SRAM_BASE + 4 * (2 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_USR_NUM) + 1))
#define SRAM_USR_VAL(x, y)	(SRAM_BASE + 4 * (2 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_USR_NUM) + 2))

/* set_mux : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_REC_CNT_MUX	(SRAM_BASE + 4 * (50))
#define SRAM_MUX_SEC(y)		(SRAM_BASE + 4 * (51 + SRAM_OBJ_CNT * (y) + 0))
#define SRAM_MUX_USEC(y)	(SRAM_BASE + 4 * (51 + SRAM_OBJ_CNT * (y) + 1))
#define SRAM_MUX_VAL(y)		(SRAM_BASE + 4 * (51 + SRAM_OBJ_CNT * (y) + 2))

/* PWR : SRAM_OBJ_CNT * SRAM_PWR_CNT * SRAM_REC_CNT = 96 */
#define SRAM_REC_CNT_PWR(x)	(SRAM_BASE + 4 * (75 + (x)))
#define SRAM_PWR_SEC(x, y)	(SRAM_BASE + 4 * (79 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 0))
#define SRAM_PWR_USEC(x, y)	(SRAM_BASE + 4 * (79 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 1))
#define SRAM_PWR_VAL(x, y)	(SRAM_BASE + 4 * (79 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 2))

/* set_parent : SRAM_OBJ_CNT * SRAM_PWR_CNT * SRAM_REC_CNT = 96 */
#define SRAM_REC_CNT_CLK(x)	(SRAM_BASE + 4 * (175 + (x)))
#define SRAM_CLK_SEC(x, y)	(SRAM_BASE + 4 * (179 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 0))
#define SRAM_CLK_USEC(x, y)	(SRAM_BASE + 4 * (179 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 1))
#define SRAM_CLK_VAL(x, y)	(SRAM_BASE + 4 * (179 + SRAM_OBJ_CNT * ((x) + (y) * SRAM_PWR_CNT) + 2))

/* set_rate : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_REC_CNT_RATE	(SRAM_BASE + 4 * (275))
#define SRAM_RATE_SEC(y)	(SRAM_BASE + 4 * (276 + SRAM_OBJ_CNT * (y) + 0))
#define SRAM_RATE_USEC(y)	(SRAM_BASE + 4 * (276 + SRAM_OBJ_CNT * (y) + 1))
#define SRAM_RATE_VAL(y)	(SRAM_BASE + 4 * (276 + SRAM_OBJ_CNT * (y) + 2))

/* CEIL : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_REC_CNT_CEIL	(SRAM_BASE + 4 * (300))
#define SRAM_CEIL_SEC(y)	(SRAM_BASE + 4 * (301 + SRAM_OBJ_CNT * (y) + 0))
#define SRAM_CEIL_USEC(y)	(SRAM_BASE + 4 * (301 + SRAM_OBJ_CNT * (y) + 1))
#define SRAM_CEIL_VAL(y)	(SRAM_BASE + 4 * (301 + SRAM_OBJ_CNT * (y) + 2))

/* VMM : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_REC_CNT_VMM	(SRAM_BASE + 4 * (325))
#define SRAM_VMM_SEC(y)		(SRAM_BASE + 4 * (326 + SRAM_OBJ_CNT * (y) + 0))
#define SRAM_VMM_USEC(y)	(SRAM_BASE + 4 * (326 + SRAM_OBJ_CNT * (y) + 1))
#define SRAM_VMM_VAL(y)		(SRAM_BASE + 4 * (326 + SRAM_OBJ_CNT * (y) + 2))

/* VMM : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_VMM_HW_VAL(y)	(SRAM_BASE + 4 * (350 + SRAM_REC_CNT * 0 + (y)))
#define SRAM_VMM_VOLT(y)	(SRAM_BASE + 4 * (350 + SRAM_REC_CNT * 1 + (y)))

#define SRAM_VMM_EFUSE_HIGH	(SRAM_BASE + 4 * (372))
#define SRAM_VMM_EFUSE_LOW	(SRAM_BASE + 4 * (373))


/* VDISP : SRAM_OBJ_CNT * SRAM_REC_CNT = 24 */
#define SRAM_REC_CNT_VDISP	(SRAM_BASE + 4 * (374))
#define SRAM_VDISP_SEC(y)	(SRAM_BASE + 4 * (375 + SRAM_OBJ_CNT * (y) + 0))
#define SRAM_VDISP_USEC(y)	(SRAM_BASE + 4 * (375 + SRAM_OBJ_CNT * (y) + 1))
#define SRAM_VDISP_VAL(y)	(SRAM_BASE + 4 * (375 + SRAM_OBJ_CNT * (y) + 2))

#define SRAM_MUX_MIN(x)		(SRAM_BASE + 4 * (399 + (x))) // SRAM_MUX_CNT
#define SRAM_PWR_GEAR(x)	(SRAM_BASE + 4 * (415 + (x))) // SRAM_PWR_CNT
#define SRAM_VMM_CEIL		(SRAM_BASE + 4 * (418))

// next : 419

/* mbrain : u64(2) * SRAM_PWR_CNT * OPP_NUM(8) = 64 */
#define SRAM_PWR_TOTAL(x, y)	(SRAM_BASE + 4 * (570 + 2 * ((y) + (x) * 8)))

/* mux_cb : mt6991 only */
#define SRAM_MUX_CB_SEC		(SRAM_BASE + 4 * (635))
#define SRAM_MUX_CB_USEC	(SRAM_BASE + 4 * (636))
#define SRAM_MUX_CB_END_SEC	(SRAM_BASE + 4 * (637))
#define SRAM_MUX_CB_END_USEC	(SRAM_BASE + 4 * (638))
#define SRAM_MUX_CB_MUX_OPP	(SRAM_BASE + 4 * (639))
#endif

