/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_REG_H
#define __VCP_REG_H

#define R_GIPC_IN_SET                   (vcpreg.cfgreg_ap + 0x0028)
#define R_GIPC_IN_CLR                   (vcpreg.cfgreg_ap + 0x002C)
	#define B_GIPC0_SETCLR_0            (1 << 0)
	#define B_GIPC0_SETCLR_1            (1 << 1)
	#define B_GIPC0_SETCLR_2            (1 << 2)
	#define B_GIPC0_SETCLR_3            (1 << 3)
	#define B_GIPC1_SETCLR_0            (1 << 4)
	#define GIPC_VCP_HART1_HALT         B_GIPC1_SETCLR_0
	#define B_GIPC1_SETCLR_1            (1 << 5)
	#define GIPC_VCP_HART1_SWI          B_GIPC1_SETCLR_1
	#define B_GIPC1_SETCLR_2            (1 << 6)
	#define GIPC_VCP_HART1_SHUT         B_GIPC1_SETCLR_2
	#define B_GIPC1_SETCLR_3            (1 << 7)
	#define GIPC_VCP_HART1_DUMP         B_GIPC1_SETCLR_3
	#define B_GIPC2_SETCLR_0            (1 << 8)
	#define GIPC_MMUP_HALT              B_GIPC2_SETCLR_0
	#define B_GIPC2_SETCLR_1            (1 << 9)
	#define GIPC_MMUP_SWI               B_GIPC2_SETCLR_1
	#define B_GIPC2_SETCLR_2            (1 << 10)
	#define GIPC_MMUP_SHUT              B_GIPC2_SETCLR_2
	#define B_GIPC2_SETCLR_3            (1 << 11)
	#define GIPC_MMUP_DUMP              B_GIPC2_SETCLR_3
	#define B_GIPC3_SETCLR_0            (1 << 12)
	#define GIPC_VCP_HART0_HALT         B_GIPC3_SETCLR_0
	#define B_GIPC3_SETCLR_1            (1 << 13)
	#define GIPC_VCP_HART0_SWI          B_GIPC3_SETCLR_1
	#define B_GIPC3_SETCLR_2            (1 << 14)
	#define GIPC_VCP_HART0_SHUT         B_GIPC3_SETCLR_2
	#define B_GIPC3_SETCLR_3            (1 << 15)
	#define GIPC_VCP_HART0_DUMP         B_GIPC3_SETCLR_3
	#define B_GIPC4_SETCLR_0            (1 << 16)
	#define B_GIPC4_SETCLR_1            (1 << 17)
	#define B_GIPC4_SETCLR_2            (1 << 18)
	#define B_GIPC4_SETCLR_3            (1 << 19)
#define VCP_SEMAPHORE                   (vcpreg.cfgreg_ap + 0x0040)
#define R_CORE0_WDT_IRQ                 (vcpreg.cfgreg_ap + 0x0050)
#define R_CORE1_WDT_IRQ                 (vcpreg.cfgreg_ap + 0x0054)
	#define B_WDT_IRQ                   (1 << 0)
#define AP_R_GPR0                       (vcpreg.cfgreg_ap + 0x0060)
#define AP_R_GPR1                       (vcpreg.cfgreg_ap + 0x0064)
#define AP_R_GPR2                       (vcpreg.cfgreg_ap + 0x0068)
	#define B_CORE0_SUSPEND             (1 << 0)
	#define B_CORE0_RESUME              (1 << 1)
#define AP_R_GPR3                       (vcpreg.cfgreg_ap + 0x006C)
	#define B_CORE1_SUSPEND             (1 << 0)
	#define B_CORE1_RESUME              (1 << 1)

#define VCP_SYS_CTRL                    (vcpreg.cfg)
#define VCP_SLP_PROT_EN                 (vcpreg.cfg + 0x0008)
#define VCP_TO_INFRA_TX                 (vcpreg.cfg + 0x0108)
#define VCP_GPR_DEBUG_HINT              (vcpreg.cfg + 0x0140)
	#define B_SERR                      (1 << 0)
#define VCP_BUS_DEBUG_OUT               (vcpreg.cfg + 0x0150)
#define VCP_DDREN_NEW_CTRL              (vcpreg.cfg + 0x01B0)
#define VCP_DDREN_NEW_CTRL2             (vcpreg.cfg + 0x01B8)
#define VCP_APSRC_CTRL2                 (vcpreg.cfg + 0x01BC)

#define R_CORE0_SW_RSTN_CLR             (vcpreg.cfg_core0 + 0x0000)
#define R_CORE0_SW_RSTN_SET             (vcpreg.cfg_core0 + 0x0004)
#define R_CORE0_DBG_CTRL                (vcpreg.cfg_core0 + 0x0010)
	#define M_CORE_TBUF_DBG_SEL         (7 << 4)
	#define S_CORE_TBUF_DBG_SEL         (4)
	#define M_CORE_TBUF_DBG_SEL_RV55    (0xfff0ff0f)
	#define S_CORE_TBUF_S               (4)
	#define S_CORE_TBUF1_S              (16)
#define R_CORE0_CLK_SYS_REQ             (vcpreg.cfg_core0 + 0x0020)
#define R_CORE0_BUS_REQ                 (vcpreg.cfg_core0 + 0x0024)
#define R_CORE0_APSRC_REQ               (vcpreg.cfg_core0 + 0x0028)
#define R_CORE0_DDREN_REQ               (vcpreg.cfg_core0 + 0x002C)
#define R_CORE0_WDT_CFG                 (vcpreg.cfg_core0 + 0x0034)
	#define V_INSTANT_WDT               0x80000000

#define R_CORE1_SW_RSTN_CLR             (vcpreg.cfg_core1 + 0x0000)
#define R_CORE1_SW_RSTN_SET             (vcpreg.cfg_core1 + 0x0004)
#define R_CORE1_DBG_CTRL                (vcpreg.cfg_core1 + 0x0010)
#define R_CORE1_CLK_SYS_REQ             (vcpreg.cfg_core1 + 0x0020)
#define R_CORE1_BUS_REQ                 (vcpreg.cfg_core1 + 0x0024)
#define R_CORE1_APSRC_REQ               (vcpreg.cfg_core1 + 0x0028)
#define R_CORE1_DDREN_REQ               (vcpreg.cfg_core1 + 0x002C)
#define R_CORE1_WDT_CFG                 (vcpreg.cfg_core1 + 0x0034)

#define R_CORE0_STATUS                  (vcpreg.cfg_core0 + 0x0070)
	#define B_CORE_GATED                (1 << 0)
	#define B_HART0_HALT                (1 << 1)
	#define B_HART1_HALT                (1 << 2)
	#define B_CORE_AXIS_BUSY            (1 << 4)
#define R_CORE0_MON_PC                  (vcpreg.cfg_core0 + 0x0080)
#define R_CORE0_MON_LR                  (vcpreg.cfg_core0 + 0x0084)
#define R_CORE0_MON_SP                  (vcpreg.cfg_core0 + 0x0088)
#define R_CORE0_TBUF_WPTR               (vcpreg.cfg_core0 + 0x008c)
#define R_CORE0_MON_PC_LATCH            (vcpreg.cfg_core0 + 0x00d0)
#define R_CORE0_MON_LR_LATCH            (vcpreg.cfg_core0 + 0x00d4)
#define R_CORE0_MON_SP_LATCH            (vcpreg.cfg_core0 + 0x00d8)
#define R_CORE0_T1_MON_PC               (vcpreg.cfg_core0 + 0x0160)
#define R_CORE0_T1_MON_LR               (vcpreg.cfg_core0 + 0x0164)
#define R_CORE0_T1_MON_SP               (vcpreg.cfg_core0 + 0x0168)
#define R_CORE0_T1_MON_PC_LATCH         (vcpreg.cfg_core0 + 0x0170)
#define R_CORE0_T1_MON_LR_LATCH         (vcpreg.cfg_core0 + 0x0174)
#define R_CORE0_T1_MON_SP_LATCH         (vcpreg.cfg_core0 + 0x0178)

#define R_CORE1_STATUS                  (vcpreg.cfg_core1 + 0x0070)
#define R_CORE1_MON_PC                  (vcpreg.cfg_core1 + 0x0080)
#define R_CORE1_MON_LR                  (vcpreg.cfg_core1 + 0x0084)
#define R_CORE1_MON_SP                  (vcpreg.cfg_core1 + 0x0088)
#define R_CORE1_TBUF_WPTR               (vcpreg.cfg_core1 + 0x008c)
#define R_CORE1_MON_PC_LATCH            (vcpreg.cfg_core1 + 0x00d0)
#define R_CORE1_MON_LR_LATCH            (vcpreg.cfg_core1 + 0x00d4)
#define R_CORE1_MON_SP_LATCH            (vcpreg.cfg_core1 + 0x00d8)
#define R_CORE1_T1_MON_PC               (vcpreg.cfg_core1 + 0x0160)
#define R_CORE1_T1_MON_LR               (vcpreg.cfg_core1 + 0x0164)
#define R_CORE1_T1_MON_SP               (vcpreg.cfg_core1 + 0x0168)
#define R_CORE1_T1_MON_PC_LATCH         (vcpreg.cfg_core1 + 0x0170)
#define R_CORE1_T1_MON_LR_LATCH         (vcpreg.cfg_core1 + 0x0174)
#define R_CORE1_T1_MON_SP_LATCH         (vcpreg.cfg_core1 + 0x0178)

#define R_CORE0_TBUF_DATA31_0           (vcpreg.cfg_core0 + 0x00e0)
#define R_CORE0_TBUF_DATA63_32          (vcpreg.cfg_core0 + 0x00e4)
#define R_CORE0_TBUF_DATA95_64          (vcpreg.cfg_core0 + 0x00e8)
#define R_CORE0_TBUF_DATA127_96         (vcpreg.cfg_core0 + 0x00ec)
#define R_CORE0_TBUF1_DATA31_0          (vcpreg.cfg_core0 + 0x00f0)
#define R_CORE0_TBUF1_DATA63_32         (vcpreg.cfg_core0 + 0x00f4)
#define R_CORE0_TBUF1_DATA95_64         (vcpreg.cfg_core0 + 0x00f8)
#define R_CORE0_TBUF1_DATA127_96        (vcpreg.cfg_core0 + 0x00fc)

#define R_CORE1_TBUF_DATA31_0           (vcpreg.cfg_core1 + 0x00e0)
#define R_CORE1_TBUF_DATA63_32          (vcpreg.cfg_core1 + 0x00e4)
#define R_CORE1_TBUF_DATA95_64          (vcpreg.cfg_core1 + 0x00e8)
#define R_CORE1_TBUF_DATA127_96         (vcpreg.cfg_core1 + 0x00ec)
#define R_CORE1_TBUF1_DATA31_0          (vcpreg.cfg_core1 + 0x00f0)
#define R_CORE1_TBUF1_DATA63_32         (vcpreg.cfg_core1 + 0x00f4)
#define R_CORE1_TBUF1_DATA95_64         (vcpreg.cfg_core1 + 0x00f8)
#define R_CORE1_TBUF1_DATA127_96        (vcpreg.cfg_core1 + 0x00fc)

#define R_CORE0_GENERAL_REG0            (vcpreg.cfg_core0 + 0x0040)
#define R_CORE0_GENERAL_REG1            (vcpreg.cfg_core0 + 0x0044)
	#define DRAM_RESV_ADDR_REG_VCP        R_CORE0_GENERAL_REG1
#define R_CORE0_GENERAL_REG2            (vcpreg.cfg_core0 + 0x0048)
	#define DRAM_RESV_SIZE_REG_VCP        R_CORE0_GENERAL_REG2
#define R_CORE0_GENERAL_REG3            (vcpreg.cfg_core0 + 0x004C)
	#define DRAM_RESV_LOGGER_REG_VCP    R_CORE0_GENERAL_REG3
#define R_CORE0_GENERAL_REG4            (vcpreg.cfg_core0 + 0x0050)
#define R_CORE0_GENERAL_REG5            (vcpreg.cfg_core0 + 0x0054)
	#define VCP_GPR_C0_H0_REBOOT        R_CORE0_GENERAL_REG5
	#define CORE_RDY_TO_REBOOT          0x34
	#define CORE_REBOOT_OK              0x1
#define R_CORE0_GENERAL_REG6            (vcpreg.cfg_core0 + 0x0058)
	#define VCP_GPR_C0_H1_REBOOT        R_CORE0_GENERAL_REG6
#define R_CORE0_GENERAL_REG7            (vcpreg.cfg_core0 + 0x005C)
#define R_CORE1_GENERAL_REG0            (vcpreg.cfg_core1 + 0x0040)
#define R_CORE1_GENERAL_REG1            (vcpreg.cfg_core1 + 0x0044)
	#define DRAM_RESV_ADDR_REG_MMUP        R_CORE1_GENERAL_REG1
#define R_CORE1_GENERAL_REG2            (vcpreg.cfg_core1 + 0x0048)
	#define DRAM_RESV_SIZE_REG_MMUP        R_CORE1_GENERAL_REG2
#define R_CORE1_GENERAL_REG3            (vcpreg.cfg_core1 + 0x004C)
	#define DRAM_RESV_LOGGER_REG_MMUP   R_CORE1_GENERAL_REG3
#define R_CORE1_GENERAL_REG4            (vcpreg.cfg_core1 + 0x0050)
#define R_CORE1_GENERAL_REG5            (vcpreg.cfg_core1 + 0x0054)
	#define VCP_GPR_C1_H0_REBOOT        R_CORE1_GENERAL_REG5
#define R_CORE1_GENERAL_REG6            (vcpreg.cfg_core1 + 0x0058)
	#define VCP_GPR_C1_H1_REBOOT        R_CORE1_GENERAL_REG6
#define R_CORE1_GENERAL_REG7            (vcpreg.cfg_core1 + 0x005C)

/* intc reg*/
#define R_CORE0_IRQ_STA0                (vcpreg.cfg_intc_core0 + 0x0000)
#define R_CORE0_IRQ_STA1                (vcpreg.cfg_intc_core0 + 0x0004)
#define R_CORE0_IRQ_STA2                (vcpreg.cfg_intc_core0 + 0x0008)
#define R_CORE0_IRQ_STA3                (vcpreg.cfg_intc_core0 + 0x000C)
#define R_CORE0_IRQ_EN0                 (vcpreg.cfg_intc_core0 + 0x0020)
#define R_CORE0_IRQ_EN1                 (vcpreg.cfg_intc_core0 + 0x0024)
#define R_CORE0_IRQ_EN2                 (vcpreg.cfg_intc_core0 + 0x0028)
#define R_CORE0_IRQ_EN3                 (vcpreg.cfg_intc_core0 + 0x002C)
#define R_CORE0_IRQ_SLP0                (vcpreg.cfg_intc_core0 + 0x0040)
#define R_CORE0_IRQ_SLP1                (vcpreg.cfg_intc_core0 + 0x0044)
#define R_CORE0_IRQ_SLP2                (vcpreg.cfg_intc_core0 + 0x0048)
#define R_CORE0_IRQ_SLP3                (vcpreg.cfg_intc_core0 + 0x004C)

#define R_CORE1_IRQ_STA0                (vcpreg.cfg_intc_core1 + 0x0000)
#define R_CORE1_IRQ_STA1                (vcpreg.cfg_intc_core1 + 0x0004)
#define R_CORE1_IRQ_STA2                (vcpreg.cfg_intc_core1 + 0x0008)
#define R_CORE1_IRQ_STA3                (vcpreg.cfg_intc_core1 + 0x000C)
#define R_CORE1_IRQ_EN0                 (vcpreg.cfg_intc_core1 + 0x0020)
#define R_CORE1_IRQ_EN1                 (vcpreg.cfg_intc_core1 + 0x0024)
#define R_CORE1_IRQ_EN2                 (vcpreg.cfg_intc_core1 + 0x0028)
#define R_CORE1_IRQ_EN3                 (vcpreg.cfg_intc_core1 + 0x002C)
#define R_CORE1_IRQ_SLP0                (vcpreg.cfg_intc_core1 + 0x0040)
#define R_CORE1_IRQ_SLP1                (vcpreg.cfg_intc_core1 + 0x0044)
#define R_CORE1_IRQ_SLP2                (vcpreg.cfg_intc_core1 + 0x0048)
#define R_CORE1_IRQ_SLP3                (vcpreg.cfg_intc_core1 + 0x004C)

/* pwr ctl */
#define VCP_R_SLP_EN                    (vcpreg.cfg_pwr)
#define VCP_POWER_STATUS                (vcpreg.cfg_pwr + 0x068)

/* bus debug reg */
#define VCP_BUS_DBG_CON                 (vcpreg.bus_debug)
#define VCP_BUS_DBG_RESULT0             (vcpreg.bus_debug + 0x408)

/* bus tracker */
#define VCP_BUS_TRACKER_CON             (vcpreg.bus_tracker)
#define VCP_BUS_TRACKER_AR_TRACK_L      (vcpreg.bus_tracker + 0x0400)
#define VCP_BUS_TRACKER_AR_TRACK_H      (vcpreg.bus_tracker + 0x0600)
#define VCP_BUS_TRACKER_AW_TRACK_L      (vcpreg.bus_tracker + 0x0A00)
#define VCP_BUS_TRACKER_AW_TRACK_H      (vcpreg.bus_tracker + 0x0C00)

/* bus protect */
#define VCP_BUS_PROT                    (vcpreg.bus_prot)

/* sec GPR */
#define R_GPR0_CFGREG_SEC               (vcpreg.cfg_sec_gpr)
#define R_GPR1_CFGREG_SEC               (vcpreg.cfg_sec_gpr + 0x0004)
#define R_GPR2_CFGREG_SEC               (vcpreg.cfg_sec_gpr + 0x0008)
	#define MMUP_AP_SUSPEND             (1U << 0)
	#define MMUP_WARM_BOOT              (1U << 1)
#define R_GPR3_CFGREG_SEC               (vcpreg.cfg_sec_gpr + 0x000C)
	#define VCP_AP_SUSPEND              (1U << 0)
	#define VCP_WARM_BOOT               (1U << 1)

/* vcp rdy */
#define VLP_AO_RSVD7                    (vcpreg.vcp_vlp_ao_rsvd7)
	#define READY_BIT                   (1U << 1)

/* vcp pwr */
#define VCP_PWR_ACK                     (vcpreg.vcp_pwr_ack)

/* vcp fenc */
#define VCP_PDN                         (vcpreg.vcp_clk_sys + 0x0060)
#define VCP_FENC                        (vcpreg.vcp_clk_sys + 0x0174)

#endif
