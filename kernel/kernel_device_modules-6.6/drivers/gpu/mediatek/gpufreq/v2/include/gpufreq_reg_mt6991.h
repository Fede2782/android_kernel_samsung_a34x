/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6991_H__
#define __GPUFREQ_REG_MT6991_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                              (0x48000000)
#define MALI_GPU_ID                            (g_mali_base + 0x000)               /* 0x48000000 */
#define MALI_GPU_IRQ_STATUS                    (g_mali_base + 0x02C)               /* 0x4800002C */
#define MALI_SHADER_READY_LO                   (g_mali_base + 0x140)               /* 0x48000140 */
#define MALI_TILER_READY_LO                    (g_mali_base + 0x150)               /* 0x48000150 */
#define MALI_L2_READY_LO                       (g_mali_base + 0x160)               /* 0x48000160 */

#define MFG_TOP_CFG_BASE                       (0x48500000)
#define MFG_TOP_CG_CON                         (g_mfg_top_base + 0x0000)           /* 0x48500000 */
#define MFG_SRAM_FUL_SEL_ULV                   (g_mfg_top_base + 0x0080)           /* 0x48500080 */
#define MFG_SRAM_FUL_SEL_ULV_TOP               (g_mfg_top_base + 0x0084)           /* 0x48500084 */
#define MFG_QCHANNEL_CON                       (g_mfg_top_base + 0x00B4)           /* 0x485000B4 */
#define MFG_DEBUG_SEL                          (g_mfg_top_base + 0x0170)           /* 0x48500170 */
#define MFG_DEBUG_TOP                          (g_mfg_top_base + 0x0178)           /* 0x48500178 */
#define MFG_BRISKET_ST0_AO_CFG_0               (g_mfg_top_base + 0x0370)           /* 0x48500370 */
#define MFG_BRISKET_ST1_AO_CFG_0               (g_mfg_top_base + 0x0374)           /* 0x48500374 */
#define MFG_BRISKET_ST2_AO_CFG_0               (g_mfg_top_base + 0x0378)           /* 0x48500378 */
#define MFG_BRISKET_ST5_AO_CFG_0               (g_mfg_top_base + 0x03A4)           /* 0x485003A4 */
#define MFG_BRISKET_ST6_AO_CFG_0               (g_mfg_top_base + 0x03A8)           /* 0x485003A8 */
#define MFG_BRISKET_ST7_AO_CFG_0               (g_mfg_top_base + 0x03AC)           /* 0x485003AC */
#define MFG_EARLY_DCM_CON                      (g_mfg_top_base + 0x0B24)           /* 0x48500B24 */
#define MFG_DEFAULT_DELSEL_00                  (g_mfg_top_base + 0x0C80)           /* 0x48500C80 */
#define MFG_POWER_TRACKER_SETTING              (g_mfg_top_base + 0x0FE0)           /* 0x48500FE0 */
#define MFG_POWER_TRACKER_PDC_STATUS0          (g_mfg_top_base + 0x0FE4)           /* 0x48500FE4 */
#define MFG_POWER_TRACKER_PDC_STATUS1          (g_mfg_top_base + 0x0FE8)           /* 0x48500FE8 */
#define MFG_POWER_TRACKER_PDC_STATUS2          (g_mfg_top_base + 0x0FEC)           /* 0x48500FEC */
#define MFG_POWER_TRACKER_PDC_STATUS3          (g_mfg_top_base + 0x0FF0)           /* 0x48500FF0 */
#define MFG_POWER_TRACKER_PDC_STATUS4          (g_mfg_top_base + 0x0FF4)           /* 0x48500FF4 */
#define MFG_POWER_TRACKER_PDC_STATUS5          (g_mfg_top_base + 0x0FF8)           /* 0x48500FF8 */
#define MFG_DREQ_TOP_DBG_CON_0                 (g_mfg_top_base + 0x1350)           /* 0x48501350 */
#define MFG_DREQ_TOP_DBG_CON_1                 (g_mfg_top_base + 0x1354)           /* 0x48501354 */

#define MFG_SMMU_BASE                          (0x48600000)
#define MFG_SMMU_CR0                           (g_mfg_smmu_base + 0x0020)          /* 0x48600020 */
#define MFG_SMMU_GBPA                          (g_mfg_smmu_base + 0x0044)          /* 0x48600044 */

#define MFG_VGPU_BUS_TRACKER_BASE              (0x48800000)
#define MFG_VGPU_BUS_DBG_CON_0                 (g_mfg_vgpu_bus_trk_base + 0x000)   /* 0x48800000 */
#define MFG_VGPU_BUS_TIMEOUT_INFO              (g_mfg_vgpu_bus_trk_base + 0x028)   /* 0x48800028 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L   (g_mfg_vgpu_bus_trk_base + 0x040)   /* 0x48800040 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H   (g_mfg_vgpu_bus_trk_base + 0x044)   /* 0x48800044 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_L          (g_mfg_vgpu_bus_trk_base + 0x048)   /* 0x48800048 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_H          (g_mfg_vgpu_bus_trk_base + 0x04C)   /* 0x4880004C */
#define MFG_VGPU_BUS_AR_SLVERR_ADDR_L          (g_mfg_vgpu_bus_trk_base + 0x080)   /* 0x48800080 */
#define MFG_VGPU_BUS_AR_SLVERR_ADDR_H          (g_mfg_vgpu_bus_trk_base + 0x084)   /* 0x48800084 */
#define MFG_VGPU_BUS_AR_SLVERR_ID              (g_mfg_vgpu_bus_trk_base + 0x088)   /* 0x48800088 */
#define MFG_VGPU_BUS_AR_SLVERR_LOG             (g_mfg_vgpu_bus_trk_base + 0x08C)   /* 0x4880008C */
#define MFG_VGPU_BUS_AW_SLVERR_ADDR_L          (g_mfg_vgpu_bus_trk_base + 0x090)   /* 0x48800090 */
#define MFG_VGPU_BUS_AW_SLVERR_ADDR_H          (g_mfg_vgpu_bus_trk_base + 0x094)   /* 0x48800094 */
#define MFG_VGPU_BUS_AW_SLVERR_ID              (g_mfg_vgpu_bus_trk_base + 0x098)   /* 0x48800098 */
#define MFG_VGPU_BUS_AW_SLVERR_LOG             (g_mfg_vgpu_bus_trk_base + 0x09C)   /* 0x4880009C */
#define MFG_VGPU_BUS_AR_TRACKER_LOG            (g_mfg_vgpu_bus_trk_base + 0x200)   /* 0x48800200 */
#define MFG_VGPU_BUS_AR_TRACKER_ID             (g_mfg_vgpu_bus_trk_base + 0x300)   /* 0x48800300 */
#define MFG_VGPU_BUS_AR_TRACKER_L              (g_mfg_vgpu_bus_trk_base + 0x400)   /* 0x48800400 */
#define MFG_VGPU_BUS_AW_TRACKER_LOG            (g_mfg_vgpu_bus_trk_base + 0x800)   /* 0x48800800 */
#define MFG_VGPU_BUS_AW_TRACKER_ID             (g_mfg_vgpu_bus_trk_base + 0x900)   /* 0x48800900 */
#define MFG_VGPU_BUS_AW_TRACKER_L              (g_mfg_vgpu_bus_trk_base + 0xA00)   /* 0x48800A00 */

#define MFG_GPUEB_BUS_TRACKER_BASE             (0x4B190000)
#define MFG_GPUEB_BUS_DBG_CON_0                (g_mfg_eb_bus_trk_base + 0x10000)   /* 0x4B1A0000 */
#define MFG_GPUEB_BUS_TIMEOUT_INFO             (g_mfg_eb_bus_trk_base + 0x10028)   /* 0x4B1A0028 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_eb_bus_trk_base + 0x10040)   /* 0x4B1A0040 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_eb_bus_trk_base + 0x10044)   /* 0x4B1A0044 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_L         (g_mfg_eb_bus_trk_base + 0x10048)   /* 0x4B1A0048 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_H         (g_mfg_eb_bus_trk_base + 0x1004C)   /* 0x4B1A004C */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x10080)   /* 0x4B1A0080 */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x10084)   /* 0x4B1A0084 */
#define MFG_GPUEB_BUS_AR_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x10088)   /* 0x4B1A0088 */
#define MFG_GPUEB_BUS_AR_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x1008C)   /* 0x4B1A008C */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x10090)   /* 0x4B1A0090 */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x10094)   /* 0x4B1A0094 */
#define MFG_GPUEB_BUS_AW_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x10098)   /* 0x4B1A0098 */
#define MFG_GPUEB_BUS_AW_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x1009C)   /* 0x4B1A009C */
#define MFG_GPUEB_BUS_AR_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x10200)   /* 0x4B1A0200 */
#define MFG_GPUEB_BUS_AR_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x10300)   /* 0x4B1A0300 */
#define MFG_GPUEB_BUS_AR_TRACKER_L             (g_mfg_eb_bus_trk_base + 0x10400)   /* 0x4B1A0400 */
#define MFG_GPUEB_BUS_AW_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x10800)   /* 0x4B1A0800 */
#define MFG_GPUEB_BUS_AW_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x10900)   /* 0x4B1A0900 */
#define MFG_GPUEB_BUS_AW_TRACKER_L             (g_mfg_eb_bus_trk_base + 0x10A00)   /* 0x4B1A0A00 */

#define MFG_RPC_BASE                           (0x4B800000)
#define MFG_RPC_SLV_WAY_EN_SET                 (g_mfg_rpc_base + 0x0060)           /* 0x4B800060 */
#define MFG_RPC_SLV_WAY_EN_CLR                 (g_mfg_rpc_base + 0x0064)           /* 0x4B800064 */
#define MFG_RPC_SLV_CTRL_UPDATE                (g_mfg_rpc_base + 0x0068)           /* 0x4B800068 */
#define MFG_RPC_SLV_SLP_PROT_EN_SET            (g_mfg_rpc_base + 0x0070)           /* 0x4B800070 */
#define MFG_RPC_SLV_SLP_PROT_EN_CLR            (g_mfg_rpc_base + 0x0074)           /* 0x4B800074 */
#define MFG_RPC_SLV_SLP_PROT_RDY_STA           (g_mfg_rpc_base + 0x0078)           /* 0x4B800078 */
#define MFG_RPC_SLP_PROT_EN0_SET               (g_mfg_rpc_base + 0x0080)           /* 0x4B800080 */
#define MFG_RPC_SLP_PROT_EN0_CLR               (g_mfg_rpc_base + 0x0084)           /* 0x4B800084 */
#define MFG_RPC_SLP_PROT_RDY_STA0              (g_mfg_rpc_base + 0x0090)           /* 0x4B800090 */
#define MFG_RPC_MFG0_PWR_CON                   (g_mfg_rpc_base + 0x0504)           /* 0x4B800504 */
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x0500)           /* 0x4B800500 */
#define MFG_RPC_MFG37_PWR_CON                  (g_mfg_rpc_base + 0x0594)           /* 0x4B800594 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x0508)           /* 0x4B800508 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x050C)           /* 0x4B80050C */
#define MFG_RPC_MFG4_PWR_CON                   (g_mfg_rpc_base + 0x0510)           /* 0x4B800510 */
#define MFG_RPC_MFG5_PWR_CON                   (g_mfg_rpc_base + 0x0514)           /* 0x4B800514 */
#define MFG_RPC_MFG7_PWR_CON                   (g_mfg_rpc_base + 0x051C)           /* 0x4B80051C */
#define MFG_RPC_MFG8_PWR_CON                   (g_mfg_rpc_base + 0x0520)           /* 0x4B800520 */
#define MFG_RPC_MFG23_PWR_CON                  (g_mfg_rpc_base + 0x055C)           /* 0x4B80055C */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x0524)           /* 0x4B800524 */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x0528)           /* 0x4B800528 */
#define MFG_RPC_MFG11_PWR_CON                  (g_mfg_rpc_base + 0x052C)           /* 0x4B80052C */
#define MFG_RPC_MFG12_PWR_CON                  (g_mfg_rpc_base + 0x0530)           /* 0x4B800530 */
#define MFG_RPC_MFG13_PWR_CON                  (g_mfg_rpc_base + 0x0534)           /* 0x4B800534 */
#define MFG_RPC_MFG14_PWR_CON                  (g_mfg_rpc_base + 0x0538)           /* 0x4B800538 */
#define MFG_RPC_MFG15_PWR_CON                  (g_mfg_rpc_base + 0x053C)           /* 0x4B80053C */
#define MFG_RPC_MFG16_PWR_CON                  (g_mfg_rpc_base + 0x0540)           /* 0x4B800540 */
#define MFG_RPC_MFG17_PWR_CON                  (g_mfg_rpc_base + 0x0544)           /* 0x4B800544 */
#define MFG_RPC_MFG18_PWR_CON                  (g_mfg_rpc_base + 0x0548)           /* 0x4B800548 */
#define MFG_RPC_MFG19_PWR_CON                  (g_mfg_rpc_base + 0x054C)           /* 0x4B80054C */
#define MFG_RPC_MFG20_PWR_CON                  (g_mfg_rpc_base + 0x0550)           /* 0x4B800550 */
#define MFG_RPC_MFG25_PWR_CON                  (g_mfg_rpc_base + 0x0564)           /* 0x4B800564 */
#define MFG_RPC_MFG26_PWR_CON                  (g_mfg_rpc_base + 0x0568)           /* 0x4B800568 */
#define MFG_RPC_MFG27_PWR_CON                  (g_mfg_rpc_base + 0x056C)           /* 0x4B80056C */
#define MFG_RPC_MFG28_PWR_CON                  (g_mfg_rpc_base + 0x0570)           /* 0x4B800570 */
#define MFG_RPC_MFG29_PWR_CON                  (g_mfg_rpc_base + 0x0574)           /* 0x4B800574 */
#define MFG_RPC_MFG30_PWR_CON                  (g_mfg_rpc_base + 0x0578)           /* 0x4B800578 */
#define MFG_RPC_MFG31_PWR_CON                  (g_mfg_rpc_base + 0x057C)           /* 0x4B80057C */
#define MFG_RPC_MFG32_PWR_CON                  (g_mfg_rpc_base + 0x0580)           /* 0x4B800580 */
#define MFG_RPC_MFG33_PWR_CON                  (g_mfg_rpc_base + 0x0584)           /* 0x4B800584 */
#define MFG_RPC_MFG34_PWR_CON                  (g_mfg_rpc_base + 0x0588)           /* 0x4B800588 */
#define MFG_RPC_MFG35_PWR_CON                  (g_mfg_rpc_base + 0x058C)           /* 0x4B80058C */
#define MFG_RPC_MFG36_PWR_CON                  (g_mfg_rpc_base + 0x0590)           /* 0x4B800590 */
#define MFG_RPC_PWR_CON_STATUS                 (g_mfg_rpc_base + 0x0600)           /* 0x4B800600 */
#define MFG_RPC_PWR_CON_2ND_STATUS             (g_mfg_rpc_base + 0x0604)           /* 0x4B800604 */
#define MFG_RPC_PWR_CON_STATUS_1               (g_mfg_rpc_base + 0x0608)           /* 0x4B800608 */
#define MFG_RPC_PWR_CON_2ND_STATUS_1           (g_mfg_rpc_base + 0x060C)           /* 0x4B80060C */
#define MFG_RPC_BRISKET_TOP_AO_CFG_0           (g_mfg_rpc_base + 0x0620)           /* 0x4B800620 */

#define MFG_PLL_BASE                           (0x4B810000)
#define MFG_PLL_CON0                           (g_mfg_pll_base + 0x008)            /* 0x4B810008 */
#define MFG_PLL_CON1                           (g_mfg_pll_base + 0x00C)            /* 0x4B81000C */
#define MFG_PLL_CON5                           (g_mfg_pll_base + 0x01C)            /* 0x4B81001C */
#define MFG_PLL_FQMTR_CON0                     (g_mfg_pll_base + 0x040)            /* 0x4B810040 */
#define MFG_PLL_FQMTR_CON1                     (g_mfg_pll_base + 0x044)            /* 0x4B810044 */

#define MFG_PLL_SC0_BASE                       (0x4B810400)
#define MFG_PLL_SC0_CON0                       (g_mfg_pll_sc0_base + 0x008)        /* 0x4B810408 */
#define MFG_PLL_SC0_CON1                       (g_mfg_pll_sc0_base + 0x00C)        /* 0x4B81040C */
#define MFG_PLL_SC0_CON5                       (g_mfg_pll_sc0_base + 0x01C)        /* 0x4B81041C */
#define MFG_PLL_SC0_FQMTR_CON0                 (g_mfg_pll_sc0_base + 0x040)        /* 0x4B810410 */
#define MFG_PLL_SC0_FQMTR_CON1                 (g_mfg_pll_sc0_base + 0x044)        /* 0x4B810414 */

#define MFG_PLL_SC1_BASE                       (0x4B810800)
#define MFG_PLL_SC1_CON0                       (g_mfg_pll_sc1_base + 0x008)        /* 0x4B810808 */
#define MFG_PLL_SC1_CON1                       (g_mfg_pll_sc1_base + 0x00C)        /* 0x4B81080C */
#define MFG_PLL_SC1_CON5                       (g_mfg_pll_sc1_base + 0x01C)        /* 0x4B81081C */
#define MFG_PLL_SC1_FQMTR_CON0                 (g_mfg_pll_sc1_base + 0x040)        /* 0x4B810810 */
#define MFG_PLL_SC1_FQMTR_CON1                 (g_mfg_pll_sc1_base + 0x044)        /* 0x4B810814 */

#define MFG_HBVC_BASE                          (0x4B840000)
#define MFG_HBVC_FLL0_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x600)           /* 0x4B840600 */
#define MFG_HBVC_FLL0_DBG_FRONTEND1            (g_mfg_hbvc_base + 0x604)           /* 0x4B840604 */
#define MFG_HBVC_FLL1_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x608)           /* 0x4B840608 */
#define MFG_HBVC_FLL1_DBG_FRONTEND1            (g_mfg_hbvc_base + 0x60C)           /* 0x4B84060C */
#define MFG_HBVC_GRP0_DBG_BACKEND0             (g_mfg_hbvc_base + 0x680)           /* 0x4B840680 */
#define MFG_HBVC_GRP0_DBG_BACKEND1             (g_mfg_hbvc_base + 0x684)           /* 0x4B840684 */
#define MFG_HBVC_GRP1_DBG_BACKEND0             (g_mfg_hbvc_base + 0x688)           /* 0x4B840680 */
#define MFG_HBVC_GRP1_DBG_BACKEND1             (g_mfg_hbvc_base + 0x68C)           /* 0x4B84068C */

#define MFG_VCORE_AO_CFG_BASE                  (0x4B860000)
#define MFG_RPC_AO_CLK_CFG                     (g_mfg_vcore_ao_cfg_base + 0x0078)  /* 0x4B860078 */
#define MFGSYS_PROTECT_EN_SET_0                (g_mfg_vcore_ao_cfg_base + 0x0080)  /* 0x4B860080 */
#define MFGSYS_PROTECT_EN_CLR_0                (g_mfg_vcore_ao_cfg_base + 0x0084)  /* 0x4B860084 */
#define MFGSYS_PROTECT_EN_STA_0                (g_mfg_vcore_ao_cfg_base + 0x0088)  /* 0x4B860088 */
#define MFG_VCORE_AO_CK_FAST_REF_SEL           (g_mfg_vcore_ao_cfg_base + 0x000C)  /* 0x4B86000C */
#define MFG_VCORE_AO_RPC_DREQ_CONFIG           (g_mfg_vcore_ao_cfg_base + 0x00B4)  /* 0x4B8600B4 */
#define MFG_VCORE_AO_MT6991_ID_CON             (g_mfg_vcore_ao_cfg_base + 0x0128)  /* 0x4B860128 */

#define MFG_VCORE_DEVAPC_BASE                  (0x4B8B0000)
#define MFG_VCORE_DEVAPC_D0_VIO_MASK_0         (g_mfg_vcore_devapc_base + 0x000)   /* 0x4B8B0000 */
#define MFG_VCORE_DEVAPC_D0_VIO_STA_0          (g_mfg_vcore_devapc_base + 0x400)   /* 0x4B8B0400 */
#define MFG_VCORE_DEVAPC_VIO_SHFT_STA_0        (g_mfg_vcore_devapc_base + 0xF20)   /* 0x4B8B0F20 */

#define MFG_VCORE_BUS_TRACKER_BASE             (0x4B900000)
#define MFG_VCORE_BUS_DBG_CON_0                (g_mfg_vcore_bus_trk_base + 0x000)  /* 0x4B900000 */
#define MFG_VCORE_BUS_TIMEOUT_INFO             (g_mfg_vcore_bus_trk_base + 0x028)  /* 0x4B900028 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_vcore_bus_trk_base + 0x040)  /* 0x4B900040 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_vcore_bus_trk_base + 0x044)  /* 0x4B900044 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_L         (g_mfg_vcore_bus_trk_base + 0x048)  /* 0x4B900048 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_H         (g_mfg_vcore_bus_trk_base + 0x04C)  /* 0x4B90004C */
#define MFG_VCORE_BUS_AR_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x080)  /* 0x4B900080 */
#define MFG_VCORE_BUS_AR_SLVERR_ADDR_H         (g_mfg_vcore_bus_trk_base + 0x084)  /* 0x4B900084 */
#define MFG_VCORE_BUS_AR_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x088)  /* 0x4B900088 */
#define MFG_VCORE_BUS_AR_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x08C)  /* 0x4B90008C */
#define MFG_VCORE_BUS_AW_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x090)  /* 0x4B900090 */
#define MFG_VCORE_BUS_AW_SLVERR_ADDR_H         (g_mfg_vcore_bus_trk_base + 0x094)  /* 0x4B900094 */
#define MFG_VCORE_BUS_AW_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x098)  /* 0x4B900098 */
#define MFG_VCORE_BUS_AW_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x09C)  /* 0x4B90009C */
#define MFG_VCORE_BUS_AR_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x200)  /* 0x4B900200 */
#define MFG_VCORE_BUS_AR_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x300)  /* 0x4B900300 */
#define MFG_VCORE_BUS_AR_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0x400)  /* 0x4B900400 */
#define MFG_VCORE_BUS_AW_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x800)  /* 0x4B900800 */
#define MFG_VCORE_BUS_AW_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x900)  /* 0x4B900900 */
#define MFG_VCORE_BUS_AW_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0xA00)  /* 0x4B900A00 */

#define CKSYS_BASE                             (0x10000000)
#define CKSYS_CLK_CFG_6                        (g_cksys_base + 0x070)              /* 0x10000070 */

#define NTH_EMICFG_AO_MEM_BASE                 (0x10404000)
#define NTH_AO_SLEEP_PROT_START                (g_nth_emicfg_ao_mem_base + 0x000)  /* 0x10404000 */
#define NTH_AO_GLITCH_PROT_START               (g_nth_emicfg_ao_mem_base + 0x084)  /* 0x10404084 */
#define NTH_AO_M6M7_IDLE_BIT_EN_1              (g_nth_emicfg_ao_mem_base + 0x228)  /* 0x10404228 */
#define NTH_AO_M6M7_IDLE_BIT_EN_0              (g_nth_emicfg_ao_mem_base + 0x22C)  /* 0x1040422C */

#define NTH_EMI_AO_DEBUG_CTRL_BASE             (0x10416000)
#define NTH_EMI_AO_BUS_U_DEBUG_CTRL0           (g_nth_emi_ao_debug_ctrl + 0x000)   /* 0x10416000 */

#define STH_EMICFG_AO_MEM_BASE                 (0x10504000)
#define STH_AO_SLEEP_PROT_START                (g_sth_emicfg_ao_mem_base + 0x000)  /* 0x10504000 */
#define STH_AO_GLITCH_PROT_START               (g_sth_emicfg_ao_mem_base + 0x084)  /* 0x10504084 */
#define STH_AO_M6M7_IDLE_BIT_EN_1              (g_sth_emicfg_ao_mem_base + 0x228)  /* 0x10504228 */
#define STH_AO_M6M7_IDLE_BIT_EN_0              (g_sth_emicfg_ao_mem_base + 0x22C)  /* 0x1050422C */

#define STH_EMI_AO_DEBUG_CTRL_BASE             (0x10516000)
#define STH_EMI_AO_BUS_U_DEBUG_CTRL0           (g_sth_emi_ao_debug_ctrl + 0x000)   /* 0x10516000 */

#define SEMI_MI32_SMI_BASE                     (0x10621000)
#define SEMI_MI32_SMI_DEBUG_S0                 (g_semi_mi32_smi + 0x400)           /* 0x10621400 */
#define SEMI_MI32_SMI_DEBUG_S1                 (g_semi_mi32_smi + 0x404)           /* 0x10621404 */
#define SEMI_MI32_SMI_DEBUG_S2                 (g_semi_mi32_smi + 0x408)           /* 0x10621408 */
#define SEMI_MI32_SMI_DEBUG_M0                 (g_semi_mi32_smi + 0x430)           /* 0x10621430 */
#define SEMI_MI32_SMI_DEBUG_MISC               (g_semi_mi32_smi + 0x440)           /* 0x10621440 */

#define NEMI_MI32_SMI_BASE                     (0x10622000)
#define NEMI_MI32_SMI_DEBUG_S0                 (g_nemi_mi32_smi + 0x400)           /* 0x10622400 */
#define NEMI_MI32_SMI_DEBUG_S1                 (g_nemi_mi32_smi + 0x404)           /* 0x10622404 */
#define NEMI_MI32_SMI_DEBUG_S2                 (g_nemi_mi32_smi + 0x408)           /* 0x10622408 */
#define NEMI_MI32_SMI_DEBUG_M0                 (g_nemi_mi32_smi + 0x430)           /* 0x10622430 */
#define NEMI_MI32_SMI_DEBUG_MISC               (g_nemi_mi32_smi + 0x440)           /* 0x10622440 */

#define SEMI_MI33_SMI_BASE                     (0x10623000)
#define SEMI_MI33_SMI_DEBUG_S0                 (g_semi_mi33_smi + 0x400)           /* 0x10623400 */
#define SEMI_MI33_SMI_DEBUG_S1                 (g_semi_mi33_smi + 0x404)           /* 0x10623404 */
#define SEMI_MI33_SMI_DEBUG_S2                 (g_semi_mi33_smi + 0x408)           /* 0x10623408 */
#define SEMI_MI33_SMI_DEBUG_M0                 (g_semi_mi33_smi + 0x430)           /* 0x10623430 */
#define SEMI_MI33_SMI_DEBUG_MISC               (g_semi_mi33_smi + 0x440)           /* 0x10623440 */

#define NEMI_MI33_SMI_BASE                     (0x10624000)
#define NEMI_MI33_SMI_DEBUG_S0                 (g_nemi_mi33_smi + 0x400)           /* 0x10624400 */
#define NEMI_MI33_SMI_DEBUG_S1                 (g_nemi_mi33_smi + 0x404)           /* 0x10624404 */
#define NEMI_MI33_SMI_DEBUG_S2                 (g_nemi_mi33_smi + 0x408)           /* 0x10624408 */
#define NEMI_MI33_SMI_DEBUG_M0                 (g_nemi_mi33_smi + 0x430)           /* 0x10624430 */
#define NEMI_MI33_SMI_DEBUG_MISC               (g_nemi_mi33_smi + 0x440)           /* 0x10624440 */

#define INFRA_AO_DEBUG_CTRL_BASE               (0x10644000)
#define INFRA_AO_BUS0_U_DEBUG_CTRL0            (g_infra_ao_debug_ctrl + 0x000)     /* 0x10644000 */

#define EMI_INFRA_AO_MEM_REG_BASE              (0x10646000)
#define EMI_IFR_AO_SLEEP_PROT_START            (g_emi_infra_ao_mem_base + 0x000)   /* 0x10646000 */
#define EMI_IFR_AO_GLITCH_PROT_START           (g_emi_infra_ao_mem_base + 0x084)   /* 0x10646084 */
#define EMI_IFR_AO_M6M7_IDLE_BIT_EN_0          (g_emi_infra_ao_mem_base + 0x228)   /* 0x10646228 */
#define EMI_IFR_AO_M6M7_IDLE_BIT_EN_1          (g_emi_infra_ao_mem_base + 0x22C)   /* 0x1064622C */
#define EMI_IFR_AO_SLPPROT_EN_SET              (g_emi_infra_ao_mem_base + 0xB08)   /* 0x10646B08 */

#define EMI_INFRACFG_REG_BASE                  (0x10648000)
#define EMI_IFR_MFG_ACP_DVM_GALS_MST_DBG       (g_emi_infra_cfg_base + 0x804)      /* 0x10648804 */
#define EMI_IFR_MFG_ACP_GALS_MST_DBG           (g_emi_infra_cfg_base + 0x814)      /* 0x10648814 */
#define EMI_IFR_M7_STH_GALS_SLV_DBG            (g_emi_infra_cfg_base + 0x824)      /* 0x10648824 */
#define EMI_IFR_M7_NTH_GALS_SLV_DBG            (g_emi_infra_cfg_base + 0x828)      /* 0x10648828 */
#define EMI_IFR_M6_STH_GALS_SLV_DBG            (g_emi_infra_cfg_base + 0x82C)      /* 0x1064882C */
#define EMI_IFR_M6_NTH_GALS_SLV_DBG            (g_emi_infra_cfg_base + 0x830)      /* 0x10648830 */
#define EMI_IFR_APU_M1_NOC_GALS_SLV_DBG        (g_emi_infra_cfg_base + 0x834)      /* 0x10648834 */
#define EMI_IFR_APU_M0_NOC_GALS_SLV_DBG        (g_emi_infra_cfg_base + 0x838)      /* 0x10648838 */
#define EMI_IFR_STH_MFG_EMI1_GALS_SLV_DBG      (g_emi_infra_cfg_base + 0x83C)      /* 0x1064883C */
#define EMI_IFR_NTH_MFG_EMI1_GALS_SLV_DBG      (g_emi_infra_cfg_base + 0x840)      /* 0x10648840 */
#define EMI_IFR_STH_MFG_EMI0_GALS_SLV_DBG      (g_emi_infra_cfg_base + 0x844)      /* 0x10648844 */
#define EMI_IFR_NTH_MFG_EMI0_GALS_SLV_DBG      (g_emi_infra_cfg_base + 0x848)      /* 0x10648848 */

#define SPM_BASE                               (0x1C004000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x0410)                  /* 0x1C004410 */
#define SPM_SRC_REQ                            (g_sleep + 0x0818)                  /* 0x1C004818 */
#define SPM_MFG0_PWR_CON                       (g_sleep + 0x0EA8)                  /* 0x1C004EA8 */
#define SPM_SOC_BUCK_ISO_CON                   (g_sleep + 0x0F64)                  /* 0x1C004F64 */
#define SPM_SOC_BUCK_ISO_CON_SET               (g_sleep + 0x0F68)                  /* 0x1C004F68 */
#define SPM_SOC_BUCK_ISO_CON_CLR               (g_sleep + 0x0F6C)                  /* 0x1C004F6C */
#define SPM_BUS_PROTECT_MSB_CON_SET            (g_sleep + 0x90E8)                  /* 0x1C00D0E8 */
#define SPM_BUS_PROTECT_MSB_CON_CLR            (g_sleep + 0x90EC)                  /* 0x1C00D0EC */
#define SPM_BUS_PROTECT_CG_MSB_CON_SET         (g_sleep + 0x9200)                  /* 0x1C00D200 */
#define SPM_BUS_PROTECT_CG_MSB_CON_CLR         (g_sleep + 0x9204)                  /* 0x1C00D204 */
#define SPM_BUS_PROTECT_RDY_MSB                (g_sleep + 0x920C)                  /* 0x1C00D20C */

#endif /* __GPUFREQ_REG_MT6991_H__ */
