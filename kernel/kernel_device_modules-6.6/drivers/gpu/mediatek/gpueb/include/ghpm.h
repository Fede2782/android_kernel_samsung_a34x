/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GHPM_H__
#define __GHPM_H__

#include <linux/platform_device.h>

#define GHPM_TIMESTAMP_MONITOR_EN          (1)
#define GPUEB_WAIT_OFF_FAIL_WRITE_DUMMY    (1)
#define GHPM_TIMEOUT_ERR_KE                (0)
#define GPUEB_WAIT_OFF_FAIL_FLAG           (0xBADDEAD)

#define GHPM_IPI_TIMEOUT                   (5000)
#define GPUEB_WAIT_CHECK_TIME_1            (1000)
#define GPUEB_WAIT_CHECK_TIME_2            (5000)
#define GPUEB_WAIT_TIMEOUT                 (10000)

/* RPC */
#define MFG_RPC_SLV_CTRL_UPDATE            (g_mfg_rpc_base + 0x0068)          /* 0x4B800068 */
#define MFG_RPC_SLV_SLP_PROT_RDY_STA       (g_mfg_rpc_base + 0x0078)          /* 0x4B800078 */
#define MFG_RPC_MFG1_PWR_CON               (g_mfg_rpc_base + 0x0500)          /* 0x4B800500 */
#define MFG_RPC_MFG0_PWR_CON               (g_mfg_rpc_base + 0x0504)          /* 0x4B800504 */
#define MFG_RPC_MFG2_PWR_CON               (g_mfg_rpc_base + 0x0508)          /* 0x4B800508 */
#define MFG_RPC_MFG37_PWR_CON              (g_mfg_rpc_base + 0x0594)          /* 0x4B800594 */
#define MFG_RPCTOP_DUMMY_REG_0             (g_mfg_rpc_base + 0x0650)          /* 0x4B800650 */
#define MFG_RPCTOP_DUMMY_REG_2             (g_mfg_rpc_base + 0x0658)          /* 0x4B800658 */
#define GHPM_EN_FOR_MT6991_B0              (BIT(0))
#define MFG_GHPM_CFG0_CON                  (g_mfg_rpc_base + 0x0800)          /* 0x4B800800 */
#define APB_WDT_RST_EN                     (BIT(24))
#define POLL_WDT_RST_EN                    (BIT(23))
#define HRE_BAK_ACK_WDT_RST_EN             (BIT(22))
#define HRE_RES_ACK_WDT_RST_EN             (BIT(21))
#define SRC_ACK_WDT_RST_EN                 (BIT(20))
#define SW_OFF_SEQ_TRI                     (BIT(4))
#define SW_OFF_SEQ_TRI_SEL                 (BIT(3))
#define ON_SEQ_TRI                         (BIT(2))
#define GHPM_EN                            (BIT(0))
#define MFG_GHPM_RO0_CON                   (g_mfg_rpc_base + 0x09A4)          /* 0x4B8009A4 */
#define TIMEOUT_ERR_RECORD                 (BIT(18))
#define GHPM_PWR_STATE                     (BIT(16))
#define GHPM_STATE                         (GENMASK(7,0))
#define MFG_GHPM_RO1_CON                   (g_mfg_rpc_base + 0x09A8)          /* 0x4B8009A8 */
#define APB_TIMEOUT_RECORD                 (BIT(31))
#define HRE_BAK_ACK_SKIP_RECORD            (BIT(30))
#define HRE_RES_ACK_SKIP_RECORD            (BIT(29))
#define SRC_ACK_TIMEOUT_RECORD             (BIT(28))
#define MAINPLL_ACK_SKIP_RECORD            (BIT(27))
#define STATE_RECORD                       (GENMASK(7,0))
#define MFG_GHPM_RO2_CON                   (g_mfg_rpc_base + 0x09AC)          /* 0x4B8009AC */

/* MFG_VCORE_AO_CONFIG */
#define MFG_RPC_DUMMY_REG                  (g_mfg_vcore_ao_config_base + 0x18) /* 0x4B860018 */
#define MFG_RPC_DUMMY_REG_1                (g_mfg_vcore_ao_config_base + 0x1C) /* 0x4B86001C */
#define MFG_RPC_AO_CLK_CFG                 (g_mfg_vcore_ao_config_base + 0x78) /* 0x4B860078 */
#define CG_FAXI_CK_SOC_IN_FREE_RUN         (BIT(0))
#define MFGSYS_PROTECT_EN_SET_0            (g_mfg_vcore_ao_config_base + 0x80) /* 0x4B860080 */
#define MFGSYS_PROTECT_EN_STA_0            (g_mfg_vcore_ao_config_base + 0x88) /* 0x4B860088 */
#define MFG_SODI_EMI                       (g_mfg_vcore_ao_config_base + 0xCC) /* 0x4B8600CC */

#define CLK_CKFG_6                         (g_clk_cfg_6)
#define CLK_CKFG_6_SET                     (g_clk_cfg_6_set)
#define CLK_CKFG_6_CLR                     (g_clk_cfg_6_clr)
#define PDN_MFG_EB_BIT                     (BIT(23))

/* MT6991 E2_ID_CON CODA meaning: E1->A0, E2->B0 */
#define MFG_MT6991_E1_ID                   (0)
#define MFG_MT6991_E2_ID                   (0x101)

#define GHPM_TS_MON_STRING(type) \
	( \
		type == TRIGGER_GHPM_ON ? __stringify(TRIGGER_GHPM_ON) : \
		type == POLLING_GHPM_ON_START ? __stringify(POLLING_GHPM_ON_START) : \
		type == POLLING_GHPM_ON_TIMEOUT ? __stringify(POLLING_GHPM_ON_TIMEOUT) : \
		type == POLLING_GHPM_ON_TIMEOUT_ERR ? __stringify(POLLING_GHPM_ON_TIMEOUT_ERR) : \
		type == POLLING_GPUEB_RESUME_START ? __stringify(POLLING_GPUEB_RESUME_START) : \
		type == POLLING_GPUEB_RESUME_TIMEOUT ? __stringify(POLLING_GPUEB_RESUME_TIMEOUT) : \
		type == GHPM_OFF_ON_RECOVERY ? __stringify(GHPM_OFF_ON_RECOVERY) : \
		type == GPUEB_ON_DONE ? __stringify(GPUEB_ON_DONE) : \
		type == IPI_SUSPEND_GPUEB ? __stringify(IPI_SUSPEND_GPUEB) : \
		type == POLLING_GPUEB_OFF_START ? __stringify(POLLING_GPUEB_OFF_START) : \
		type == POLLING_GHPM_OFF_TIMEOUT_ERR ? __stringify(POLLING_GHPM_OFF_TIMEOUT_ERR) : \
		type == POLLING_GPUEB_OFF_TIMEOUT ? __stringify(POLLING_GPUEB_OFF_TIMEOUT) : \
		type == GPUEB_OFF_DONE ? __stringify(GPUEB_OFF_DONE) : \
		"UNKNOWN" \
	)

enum mfg_mt6991_e2_con {
	MFG_MT6991_A0,
	MFG_MT6991_B0
};

#if GHPM_TIMESTAMP_MONITOR_EN
enum ghpm_timestamp_monitor_point {
	TRIGGER_GHPM_ON,
	POLLING_GHPM_ON_START,
	POLLING_GHPM_ON_TIMEOUT,
	POLLING_GHPM_ON_TIMEOUT_ERR,
	POLLING_GPUEB_RESUME_START,
	POLLING_GPUEB_RESUME_TIMEOUT,
	GHPM_OFF_ON_RECOVERY,
	GPUEB_ON_DONE,
	IPI_SUSPEND_GPUEB,
	POLLING_GPUEB_OFF_START,
	POLLING_GHPM_OFF_TIMEOUT_ERR,
	POLLING_GPUEB_OFF_TIMEOUT,
	GPUEB_OFF_DONE,
	GHPM_TS_MONITOR_NUM
};
#endif

#endif /* __GHPM_H__ */
