// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/trace_events.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6991-clk.h>

/* bringup config */

#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define CKSYS2_CLK_CFG_UPDATE			0x0004
#define VLP_CLK_CFG_UPDATE			0x0004
#define VLP_CLK_CFG_UPDATE1			0x0008
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_18				0x0130
#define CLK_CFG_18_SET				0x0134
#define CLK_CFG_18_CLR				0x0138
#define CLK_CFG_19				0x0140
#define CLK_CFG_19_SET				0x0144
#define CLK_CFG_19_CLR				0x0148
#define CLK_AUDDIV_0				0x020C
#define CLK_FENC_STATUS_MON_0			0x0270
#define CLK_FENC_STATUS_MON_1			0x0274
#define CLK_FENC_STATUS_MON_2			0x0278
#define CKSYS2_CLK_CFG_0			0x0010
#define CKSYS2_CLK_CFG_0_SET			0x0014
#define CKSYS2_CLK_CFG_0_CLR			0x0018
#define CKSYS2_CLK_CFG_1			0x0020
#define CKSYS2_CLK_CFG_1_SET			0x0024
#define CKSYS2_CLK_CFG_1_CLR			0x0028
#define CKSYS2_CLK_CFG_2			0x0030
#define CKSYS2_CLK_CFG_2_SET			0x0034
#define CKSYS2_CLK_CFG_2_CLR			0x0038
#define CKSYS2_CLK_CFG_3			0x0040
#define CKSYS2_CLK_CFG_3_SET			0x0044
#define CKSYS2_CLK_CFG_3_CLR			0x0048
#define CKSYS2_CLK_CFG_4			0x0050
#define CKSYS2_CLK_CFG_4_SET			0x0054
#define CKSYS2_CLK_CFG_4_CLR			0x0058
#define CKSYS2_CLK_CFG_5			0x0060
#define CKSYS2_CLK_CFG_5_SET			0x0064
#define CKSYS2_CLK_CFG_5_CLR			0x0068
#define CKSYS2_CLK_CFG_6			0x0070
#define CKSYS2_CLK_CFG_6_SET			0x0074
#define CKSYS2_CLK_CFG_6_CLR			0x0078
#define CKSYS2_CLK_FENC_STATUS_MON_0		0x0174
#define VLP_CLK_CFG_0				0x0010
#define VLP_CLK_CFG_0_SET			0x0014
#define VLP_CLK_CFG_0_CLR			0x0018
#define VLP_CLK_CFG_1				0x0020
#define VLP_CLK_CFG_1_SET			0x0024
#define VLP_CLK_CFG_1_CLR			0x0028
#define VLP_CLK_CFG_2				0x0030
#define VLP_CLK_CFG_2_SET			0x0034
#define VLP_CLK_CFG_2_CLR			0x0038
#define VLP_CLK_CFG_3				0x0040
#define VLP_CLK_CFG_3_SET			0x0044
#define VLP_CLK_CFG_3_CLR			0x0048
#define VLP_CLK_CFG_4				0x0050
#define VLP_CLK_CFG_4_SET			0x0054
#define VLP_CLK_CFG_4_CLR			0x0058
#define VLP_CLK_CFG_5				0x0060
#define VLP_CLK_CFG_5_SET			0x0064
#define VLP_CLK_CFG_5_CLR			0x0068
#define VLP_CLK_CFG_6				0x0070
#define VLP_CLK_CFG_6_SET			0x0074
#define VLP_CLK_CFG_6_CLR			0x0078
#define VLP_CLK_CFG_7				0x0080
#define VLP_CLK_CFG_7_SET			0x0084
#define VLP_CLK_CFG_7_CLR			0x0088
#define VLP_CLK_CFG_8				0x0090
#define VLP_CLK_CFG_8_SET			0x0094
#define VLP_CLK_CFG_8_CLR			0x0098
#define VLP_CLK_CFG_9				0x00A0
#define VLP_CLK_CFG_9_SET			0x00A4
#define VLP_CLK_CFG_9_CLR			0x00A8
#define VLP_CLK_CFG_10				0x00B0
#define VLP_CLK_CFG_10_SET			0x00B4
#define VLP_CLK_CFG_10_CLR			0x00B8
#define VLP_OCIC_FENC_STATUS_MON_0		0x039C
#define VLP_OCIC_FENC_STATUS_MON_1		0x03A0


/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_MEM_SUB_SHIFT			1
#define TOP_MUX_IO_NOC_SHIFT			2
#define TOP_MUX_PERI_AXI_SHIFT			3
#define TOP_MUX_UFS_PEXTP0_AXI_SHIFT		4
#define TOP_MUX_PEXTP1_USB_AXI_SHIFT		5
#define TOP_MUX_PERI_FMEM_SUB_SHIFT		6
#define TOP_MUX_UFS_PEXPT0_MEM_SUB_SHIFT	7
#define TOP_MUX_PEXTP1_USB_MEM_SUB_SHIFT	8
#define TOP_MUX_PERI_NOC_SHIFT			9
#define TOP_MUX_EMI_N_SHIFT			10
#define TOP_MUX_EMI_S_SHIFT			11
#define TOP_MUX_AP2CONN_HOST_SHIFT		14
#define TOP_MUX_ATB_SHIFT			15
#define TOP_MUX_CIRQ_SHIFT			16
#define TOP_MUX_PBUS_156M_SHIFT			17
#define TOP_MUX_EFUSE_SHIFT			20
#define TOP_MUX_MCU_L3GIC_SHIFT			21
#define TOP_MUX_MCU_INFRA_SHIFT			22
#define TOP_MUX_DSP_SHIFT			23
#define TOP_MUX_MFG_REF_SHIFT			24
#define TOP_MUX_MFG_EB_SHIFT			26
#define TOP_MUX_UART_SHIFT			27
#define TOP_MUX_SPI0_BCLK_SHIFT			28
#define TOP_MUX_SPI1_BCLK_SHIFT			29
#define TOP_MUX_SPI2_BCLK_SHIFT			30
#define TOP_MUX_SPI3_BCLK_SHIFT			0
#define TOP_MUX_SPI4_BCLK_SHIFT			1
#define TOP_MUX_SPI5_BCLK_SHIFT			2
#define TOP_MUX_SPI6_BCLK_SHIFT			3
#define TOP_MUX_SPI7_BCLK_SHIFT			4
#define TOP_MUX_MSDC30_1_SHIFT			7
#define TOP_MUX_MSDC30_2_SHIFT			8
#define TOP_MUX_DISP_PWM_SHIFT			9
#define TOP_MUX_USB_TOP_1P_SHIFT		10
#define TOP_MUX_SSUSB_XHCI_1P_SHIFT		11
#define TOP_MUX_SSUSB_FMCNT_P1_SHIFT		12
#define TOP_MUX_I2C_PERI_SHIFT			13
#define TOP_MUX_I2C_EAST_SHIFT			14
#define TOP_MUX_I2C_WEST_SHIFT			15
#define TOP_MUX_I2C_NORTH_SHIFT			16
#define TOP_MUX_AES_UFSFDE_SHIFT		17
#define TOP_MUX_UFS_SHIFT			18
#define TOP_MUX_AUD_1_SHIFT			21
#define TOP_MUX_AUD_2_SHIFT			22
#define TOP_MUX_ADSP_SHIFT			23
#define TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT		24
#define TOP_MUX_DPMAIF_MAIN_SHIFT		25
#define TOP_MUX_PWM_SHIFT			26
#define TOP_MUX_MCUPM_SHIFT			27
#define TOP_MUX_IPSEAST_SHIFT			29
#define TOP_MUX_TL_SHIFT			0
#define TOP_MUX_TL_P1_SHIFT			1
#define TOP_MUX_TL_P2_SHIFT			2
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		3
#define TOP_MUX_SDF_SHIFT			4
#define TOP_MUX_UARTHUB_BCLK_SHIFT		5
#define TOP_MUX_DPSW_CMP_26M_SHIFT		6
#define TOP_MUX_SMAPCK_SHIFT			7
#define TOP_MUX_SSR_PKA_SHIFT			8
#define TOP_MUX_SSR_DMA_SHIFT			9
#define TOP_MUX_SSR_KDF_SHIFT			10
#define TOP_MUX_SSR_RNG_SHIFT			11
#define TOP_MUX_SPU0_SHIFT			12
#define TOP_MUX_SPU1_SHIFT			13
#define TOP_MUX_DXCC_SHIFT			14
#define TOP_MUX_SENINF0_SHIFT			0
#define TOP_MUX_SENINF1_SHIFT			1
#define TOP_MUX_SENINF2_SHIFT			2
#define TOP_MUX_SENINF3_SHIFT			3
#define TOP_MUX_SENINF4_SHIFT			4
#define TOP_MUX_SENINF5_SHIFT			5
#define TOP_MUX_IMG1_SHIFT			6
#define TOP_MUX_IPE_SHIFT			7
#define TOP_MUX_CAM_SHIFT			8
#define TOP_MUX_CAMTM_SHIFT			9
#define TOP_MUX_DPE_SHIFT			10
#define TOP_MUX_VDEC_SHIFT			11
#define TOP_MUX_CCUSYS_SHIFT			12
#define TOP_MUX_CCUTM_SHIFT			13
#define TOP_MUX_VENC_SHIFT			14
#define TOP_MUX_DP1_SHIFT			17
#define TOP_MUX_DP0_SHIFT			18
#define TOP_MUX_DISP_SHIFT			19
#define TOP_MUX_MDP_SHIFT			20
#define TOP_MUX_MMINFRA_SHIFT			21
#define TOP_MUX_MMINFRA_SNOC_SHIFT		22
#define TOP_MUX_MMUP_SHIFT			23
#define TOP_MUX_MMINFRA_AO_SHIFT		26
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_SCP_SPI_SHIFT			1
#define TOP_MUX_SCP_IIC_SHIFT			2
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		3
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		4
#define TOP_MUX_SPMI_M_TIA_32K_SHIFT		5
#define TOP_MUX_APXGPT_26M_BCLK_SHIFT		6
#define TOP_MUX_DPSW_SHIFT			7
#define TOP_MUX_DPSW_CENTRAL_SHIFT		8
#define TOP_MUX_SPMI_M_MST_SHIFT		9
#define TOP_MUX_DVFSRC_SHIFT			10
#define TOP_MUX_PWM_VLP_SHIFT			11
#define TOP_MUX_AXI_VLP_SHIFT			12
#define TOP_MUX_SYSTIMER_26M_SHIFT		13
#define TOP_MUX_SSPM_SHIFT			14
#define TOP_MUX_SRCK_SHIFT			15
#define TOP_MUX_CAMTG0_SHIFT			16
#define TOP_MUX_CAMTG1_SHIFT			17
#define TOP_MUX_CAMTG2_SHIFT			18
#define TOP_MUX_CAMTG3_SHIFT			19
#define TOP_MUX_CAMTG4_SHIFT			20
#define TOP_MUX_CAMTG5_SHIFT			21
#define TOP_MUX_CAMTG6_SHIFT			22
#define TOP_MUX_CAMTG7_SHIFT			23
#define TOP_MUX_SSPM_26M_SHIFT			25
#define TOP_MUX_ULPOSC_SSPM_SHIFT		26
#define TOP_MUX_VLP_PBUS_26M_SHIFT		27
#define TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT	28
#define TOP_MUX_DPMSRDMA_SHIFT			29
#define TOP_MUX_VLP_PBUS_156M_SHIFT		30
#define TOP_MUX_SPM_SHIFT			0
#define TOP_MUX_MMINFRA_VLP_SHIFT		1
#define TOP_MUX_USB_TOP_SHIFT			2
#define TOP_MUX_SSUSB_XHCI_SHIFT		3
#define TOP_MUX_NOC_VLP_SHIFT			4
#define TOP_MUX_AUDIO_H_SHIFT			5
#define TOP_MUX_AUD_ENGEN1_SHIFT		6
#define TOP_MUX_AUD_ENGEN2_SHIFT		7
#define TOP_MUX_AUD_INTBUS_SHIFT		8
#define TOP_MUX_SPU_VLP_26M_SHIFT		9
#define TOP_MUX_SPU0_VLP_SHIFT			10
#define TOP_MUX_SPU1_VLP_SHIFT			11

/* TOPCK CKSTA REG */
#define CKSTA_REG				0x01C8
#define CKSTA_REG1				0x01CC
#define CKSTA_REG2				0x01D0
#define CKSYS2_CKSTA_REG1			0x00FC
#define CKSYS2_CKSTA_REG2			0x0100
#define CKSYS2_CKSTA_REG			0x00F8
#define VLP_CKSTA_REG0			0x0250
#define VLP_CKSTA_REG1			0x0254

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0214
#define CLK_AUDDIV_3				0x0220
#define CLK_AUDDIV_4				0x0224
#define CLK_AUDDIV_5				0x0228

/* APMIXED PLL REG */
#define MAINPLL_CON0				0x250
#define MAINPLL_CON1				0x254
#define MAINPLL_CON2				0x258
#define MAINPLL_CON3				0x25C
#define UNIVPLL_CON0				0x264
#define UNIVPLL_CON1				0x268
#define UNIVPLL_CON2				0x26C
#define UNIVPLL_CON3				0x270
#define MSDCPLL_CON0				0x278
#define MSDCPLL_CON1				0x27C
#define MSDCPLL_CON2				0x280
#define MSDCPLL_CON3				0x284
#define ADSPPLL_CON0				0x28C
#define ADSPPLL_CON1				0x290
#define ADSPPLL_CON2				0x294
#define ADSPPLL_CON3				0x298
#define EMIPLL_CON0				0x2A0
#define EMIPLL_CON1				0x2A4
#define EMIPLL_CON2				0x2A8
#define EMIPLL_CON3				0x2AC
#define EMIPLL2_CON0				0x2B4
#define EMIPLL2_CON1				0x2B8
#define EMIPLL2_CON2				0x2BC
#define EMIPLL2_CON3				0x2C0
#define MAINPLL2_CON0				0x250
#define MAINPLL2_CON1				0x254
#define MAINPLL2_CON2				0x258
#define MAINPLL2_CON3				0x25C
#define UNIVPLL2_CON0				0x264
#define UNIVPLL2_CON1				0x268
#define UNIVPLL2_CON2				0x26C
#define UNIVPLL2_CON3				0x270
#define MMPLL2_CON0				0x278
#define MMPLL2_CON1				0x27C
#define MMPLL2_CON2				0x280
#define MMPLL2_CON3				0x284
#define IMGPLL_CON0				0x28C
#define IMGPLL_CON1				0x290
#define IMGPLL_CON2				0x294
#define IMGPLL_CON3				0x298
#define TVDPLL1_CON0				0x2A0
#define TVDPLL1_CON1				0x2A4
#define TVDPLL1_CON2				0x2A8
#define TVDPLL1_CON3				0x2AC
#define TVDPLL2_CON0				0x2B4
#define TVDPLL2_CON1				0x2B8
#define TVDPLL2_CON2				0x2BC
#define TVDPLL2_CON3				0x2C0
#define TVDPLL3_CON0				0x2C8
#define TVDPLL3_CON1				0x2CC
#define TVDPLL3_CON2				0x2D0
#define TVDPLL3_CON3				0x2D4
#define VLP_AP_PLL_CON3				0x264
#define VLP_APLL1_TUNER_CON0			0x2A4
#define VLP_APLL2_TUNER_CON0			0x2A8
#define VLP_APLL1_CON0				0x274
#define VLP_APLL1_CON1				0x278
#define VLP_APLL1_CON2				0x27C
#define VLP_APLL1_CON3				0x280
#define VLP_APLL2_CON0				0x28C
#define VLP_APLL2_CON1				0x290
#define VLP_APLL2_CON2				0x294
#define VLP_APLL2_CON3				0x298
#define MFGPLL_CON0				0x008
#define MFGPLL_CON1				0x00C
#define MFGPLL_CON2				0x010
#define MFGPLL_CON3				0x014
#define MFGPLL_SC0_CON0				0x008
#define MFGPLL_SC0_CON1				0x00C
#define MFGPLL_SC0_CON2				0x010
#define MFGPLL_SC0_CON3				0x014
#define MFGPLL_SC1_CON0				0x008
#define MFGPLL_SC1_CON1				0x00C
#define MFGPLL_SC1_CON2				0x010
#define MFGPLL_SC1_CON3				0x014
#define CCIPLL_CON0				0x008
#define CCIPLL_CON1				0x00C
#define CCIPLL_CON2				0x010
#define CCIPLL_CON3				0x014
#define ARMPLL_LL_CON0				0x008
#define ARMPLL_LL_CON1				0x00C
#define ARMPLL_LL_CON2				0x010
#define ARMPLL_LL_CON3				0x014
#define ARMPLL_BL_CON0				0x008
#define ARMPLL_BL_CON1				0x00C
#define ARMPLL_BL_CON2				0x010
#define ARMPLL_BL_CON3				0x014
#define ARMPLL_B_CON0				0x008
#define ARMPLL_B_CON1				0x00C
#define ARMPLL_B_CON2				0x010
#define ARMPLL_B_CON3				0x014
#define PTPPLL_CON0				0x008
#define PTPPLL_CON1				0x00C
#define PTPPLL_CON2				0x010
#define PTPPLL_CON3				0x014

/* HW Voter REG */
#define HWV_CG_0_SET				0x0000
#define HWV_CG_0_CLR				0x0004
#define HWV_CG_0_DONE				0x2C00
#define HWV_CG_1_SET				0x0008
#define HWV_CG_1_CLR				0x000C
#define HWV_CG_1_DONE				0x2C04
#define HWV_CG_2_SET				0x0010
#define HWV_CG_2_CLR				0x0014
#define HWV_CG_2_DONE				0x2C08
#define HWV_CG_3_SET				0x0018
#define HWV_CG_3_CLR				0x001C
#define HWV_CG_3_DONE				0x2C0C
#define HWV_CG_4_SET				0x0020
#define HWV_CG_4_CLR				0x0024
#define HWV_CG_4_DONE				0x2C10
#define HWV_CG_5_SET				0x0028
#define HWV_CG_5_CLR				0x002C
#define HWV_CG_5_DONE				0x2C14
#define HWV_CG_6_SET				0x0030
#define HWV_CG_6_CLR				0x0034
#define HWV_CG_6_DONE				0x2C18
#define HWV_CG_7_SET				0x0038
#define HWV_CG_7_CLR				0x003C
#define HWV_CG_7_DONE				0x2C1C
#define HWV_CG_8_SET				0x0040
#define HWV_CG_8_CLR				0x0044
#define HWV_CG_8_DONE				0x2C20
#define HWV_CG_9_SET				0x0048
#define HWV_CG_9_CLR				0x004C
#define HWV_CG_9_DONE				0x2C24
#define HWV_CG_10_SET				0x0050
#define HWV_CG_10_CLR				0x0054
#define HWV_CG_10_DONE				0x2C28
#define HWV_CG_30_SET				0x0058
#define HWV_CG_30_CLR				0x005C
#define HWV_CG_30_DONE				0x2C2C
#define MM_HW_CCF_HW_CCF_2_SET			0x0000
#define MM_HW_CCF_HW_CCF_2_CLR			0x0004
#define MM_HW_CCF_HW_CCF_2_DONE			0x2C00
#define MM_HW_CCF_HW_CCF_3_SET			0x0008
#define MM_HW_CCF_HW_CCF_3_CLR			0x000C
#define MM_HW_CCF_HW_CCF_3_DONE			0x2C04
#define MM_HW_CCF_HW_CCF_6_SET			0x0010
#define MM_HW_CCF_HW_CCF_6_CLR			0x0014
#define MM_HW_CCF_HW_CCF_6_DONE			0x2C08
#define MM_HW_CCF_HW_CCF_7_SET			0x0018
#define MM_HW_CCF_HW_CCF_7_CLR			0x001C
#define MM_HW_CCF_HW_CCF_7_DONE			0x2C0C
#define MM_HW_CCF_HW_CCF_8_SET			0x0020
#define MM_HW_CCF_HW_CCF_8_CLR			0x0024
#define MM_HW_CCF_HW_CCF_8_DONE			0x2C10
#define MM_HW_CCF_HW_CCF_9_SET			0x0028
#define MM_HW_CCF_HW_CCF_9_CLR			0x002C
#define MM_HW_CCF_HW_CCF_9_DONE			0x2C14
#define MM_HW_CCF_HW_CCF_10_SET			0x0030
#define MM_HW_CCF_HW_CCF_10_CLR			0x0034
#define MM_HW_CCF_HW_CCF_10_DONE		0x2C18
#define MM_HW_CCF_HW_CCF_11_SET			0x0038
#define MM_HW_CCF_HW_CCF_11_CLR			0x003C
#define MM_HW_CCF_HW_CCF_11_DONE		0x2C1C
#define MM_HW_CCF_HW_CCF_12_SET			0x0040
#define MM_HW_CCF_HW_CCF_12_CLR			0x0044
#define MM_HW_CCF_HW_CCF_12_DONE		0x2C20
#define MM_HW_CCF_HW_CCF_13_SET			0x0048
#define MM_HW_CCF_HW_CCF_13_CLR			0x004C
#define MM_HW_CCF_HW_CCF_13_DONE		0x2C24
#define MM_HW_CCF_HW_CCF_15_SET			0x0050
#define MM_HW_CCF_HW_CCF_15_CLR			0x0054
#define MM_HW_CCF_HW_CCF_15_DONE		0x2C28
#define MM_HW_CCF_HW_CCF_16_SET			0x0058
#define MM_HW_CCF_HW_CCF_16_CLR			0x005C
#define MM_HW_CCF_HW_CCF_16_DONE		0x2C2C
#define MM_HW_CCF_HW_CCF_17_SET			0x0060
#define MM_HW_CCF_HW_CCF_17_CLR			0x0064
#define MM_HW_CCF_HW_CCF_17_DONE		0x2C30
#define MM_HW_CCF_HW_CCF_18_SET			0x0068
#define MM_HW_CCF_HW_CCF_18_CLR			0x006C
#define MM_HW_CCF_HW_CCF_18_DONE		0x2C34
#define MM_HW_CCF_HW_CCF_5_SET			0x0070
#define MM_HW_CCF_HW_CCF_5_CLR			0x0074
#define MM_HW_CCF_HW_CCF_5_DONE			0x2C38
#define MM_HW_CCF_HW_CCF_21_SET			0x0078
#define MM_HW_CCF_HW_CCF_21_CLR			0x007C
#define MM_HW_CCF_HW_CCF_21_DONE		0x2C3C
#define MM_HW_CCF_HW_CCF_22_SET			0x0080
#define MM_HW_CCF_HW_CCF_22_CLR			0x0084
#define MM_HW_CCF_HW_CCF_22_DONE		0x2C40
#define MM_HW_CCF_HW_CCF_23_SET			0x0088
#define MM_HW_CCF_HW_CCF_23_CLR			0x008C
#define MM_HW_CCF_HW_CCF_23_DONE		0x2C44
#define MM_HW_CCF_HW_CCF_24_SET			0x0090
#define MM_HW_CCF_HW_CCF_24_CLR			0x0094
#define MM_HW_CCF_HW_CCF_24_DONE		0x2C48
#define MM_HW_CCF_HW_CCF_25_SET			0x0098
#define MM_HW_CCF_HW_CCF_25_CLR			0x009C
#define MM_HW_CCF_HW_CCF_25_DONE		0x2C4C
#define MM_HW_CCF_HW_CCF_26_SET			0x00A0
#define MM_HW_CCF_HW_CCF_26_CLR			0x00A4
#define MM_HW_CCF_HW_CCF_26_DONE		0x2C50
#define MM_HW_CCF_HW_CCF_30_SET			0x00F0
#define MM_HW_CCF_HW_CCF_30_CLR			0x00F4
#define MM_HW_CCF_HW_CCF_30_DONE		0x2C78
#define MM_HW_CCF_HW_CCF_31_SET			0x00F8
#define MM_HW_CCF_HW_CCF_31_CLR			0x00FC
#define MM_HW_CCF_HW_CCF_31_DONE		0x2C7C
#define MM_HW_CCF_HW_CCF_32_SET			0x0100
#define MM_HW_CCF_HW_CCF_32_CLR			0x0104
#define MM_HW_CCF_HW_CCF_32_DONE		0x2C80
#define MM_HW_CCF_HW_CCF_33_SET			0x0108
#define MM_HW_CCF_HW_CCF_33_CLR			0x010C
#define MM_HW_CCF_HW_CCF_33_DONE		0x2C84
#define MM_HW_CCF_HW_CCF_34_SET			0x0110
#define MM_HW_CCF_HW_CCF_34_CLR			0x0114
#define MM_HW_CCF_HW_CCF_34_DONE		0x2C88
#define MM_HW_CCF_HW_CCF_35_SET			0x0118
#define MM_HW_CCF_HW_CCF_35_CLR			0x011C
#define MM_HW_CCF_HW_CCF_35_DONE		0x2C8C
#define MM_HW_CCF_HW_CCF_36_SET			0x0120
#define MM_HW_CCF_HW_CCF_36_CLR			0x0124
#define MM_HW_CCF_HW_CCF_36_DONE		0x2C90
#define MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0	0x0240

#define SW_VERSION_A0		0x0000
#define SW_VERSION_B0		0x0001

static DEFINE_SPINLOCK(mt6991_clk_lock);

static const struct mtk_fixed_factor ck_divs[] = {
	FACTOR(CLK_CK_MAINPLL_D3, "ck_mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_CK_MAINPLL_D4, "ck_mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_CK_MAINPLL_D4_D2, "ck_mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_CK_MAINPLL_D4_D4, "ck_mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_CK_MAINPLL_D4_D8, "ck_mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_CK_MAINPLL_D5, "ck_mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_CK_MAINPLL_D5_D2, "ck_mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_CK_MAINPLL_D5_D4, "ck_mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_CK_MAINPLL_D5_D8, "ck_mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_CK_MAINPLL_D6, "ck_mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_CK_MAINPLL_D6_D2, "ck_mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_CK_MAINPLL_D7, "ck_mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_CK_MAINPLL_D7_D2, "ck_mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_CK_MAINPLL_D7_D4, "ck_mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_CK_MAINPLL_D7_D8, "ck_mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_CK_MAINPLL_D9, "ck_mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_CK_UNIVPLL_D4, "ck_univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_CK_UNIVPLL_D4_D2, "ck_univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_CK_UNIVPLL_D4_D4, "ck_univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_CK_UNIVPLL_D4_D8, "ck_univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_CK_UNIVPLL_D5, "ck_univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_CK_UNIVPLL_D5_D2, "ck_univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_CK_UNIVPLL_D5_D4, "ck_univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_CK_UNIVPLL_D6, "ck_univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_CK_UNIVPLL_D6_D2, "ck_univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_CK_UNIVPLL_D6_D4, "ck_univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_CK_UNIVPLL_D6_D8, "ck_univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_CK_UNIVPLL_D6_D16, "ck_univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_CK_UNIVPLL_192M, "ck_univpll_192m",
			"univpll", 1, 13),
	FACTOR(CLK_CK_UNIVPLL_192M_D4, "ck_univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_CK_UNIVPLL_192M_D8, "ck_univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_CK_UNIVPLL_192M_D16, "ck_univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_CK_UNIVPLL_192M_D32, "ck_univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_CK_UNIVPLL_192M_D10, "ck_univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_CK_APLL1, "ck_apll1_ck",
			"vlp-apll1", 1, 1),
	FACTOR(CLK_CK_APLL1_D4, "ck_apll1_d4",
			"vlp-apll1", 1, 4),
	FACTOR(CLK_CK_APLL1_D8, "ck_apll1_d8",
			"vlp-apll1", 1, 8),
	FACTOR(CLK_CK_APLL2, "ck_apll2_ck",
			"vlp-apll2", 1, 1),
	FACTOR(CLK_CK_APLL2_D4, "ck_apll2_d4",
			"vlp-apll2", 1, 4),
	FACTOR(CLK_CK_APLL2_D8, "ck_apll2_d8",
			"vlp-apll2", 1, 8),
	FACTOR(CLK_CK_ADSPPLL, "ck_adsppll_ck",
			"adsppll", 1, 1),
	FACTOR(CLK_CK_EMIPLL1, "ck_emipll1_ck",
			"emipll", 1, 1),
	FACTOR(CLK_CK_TVDPLL1_D2, "ck_tvdpll1_d2",
			"tvdpll1", 1, 2),
	FACTOR(CLK_CK_MSDCPLL_D2, "ck_msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_CK_CLKRTC, "ck_clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_CK_TCK_26M_MX9, "ck_tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_CK_F26M, "ck_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_CK_F26M_CK_D2, "ck_f26m_d2",
			"clk13m", 1, 1),
	FACTOR(CLK_CK_OSC, "ck_osc",
			"ulposc", 1, 1),
	FACTOR(CLK_CK_OSC_D2, "ck_osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_CK_OSC_D3, "ck_osc_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_CK_OSC_D4, "ck_osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_CK_OSC_D5, "ck_osc_d5",
			"ulposc", 1, 5),
	FACTOR(CLK_CK_OSC_D7, "ck_osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_CK_OSC_D8, "ck_osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_CK_OSC_D10, "ck_osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_CK_OSC_D14, "ck_osc_d14",
			"ulposc", 1, 14),
	FACTOR(CLK_CK_OSC_D20, "ck_osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_CK_OSC_D32, "ck_osc_d32",
			"ulposc", 1, 32),
	FACTOR(CLK_CK_OSC_D40, "ck_osc_d40",
			"ulposc", 1, 40),
	FACTOR(CLK_CK_OSC3, "ck_osc3",
			"ulposc3", 1, 1),
	FACTOR(CLK_CK_P_AXI, "ck_p_axi_ck",
			"ck_p_axi_sel", 1, 1),
	FACTOR(CLK_CK_PEXTP0_AXI, "ck_pextp0_axi_ck",
			"ck_pextp0_axi_sel", 1, 1),
	FACTOR(CLK_CK_PEXTP1_USB_AXI, "ck_pextp1_usb_axi_ck",
			"ck_pextp1_usb_axi_sel", 1, 1),
	FACTOR(CLK_CK_PEXPT0_MEM_SUB, "ck_pexpt0_mem_sub_ck",
			"ck_pexpt0_mem_sub_sel", 1, 1),
	FACTOR(CLK_CK_PEXTP1_USB_MEM_SUB, "ck_pextp1_usb_mem_sub_ck",
			"ck_pextp1_usb_mem_sub_sel", 1, 1),
	FACTOR(CLK_CK_UART, "ck_uart_ck",
			"ck_uart_sel", 1, 1),
	FACTOR(CLK_CK_SPI0_BCLK, "ck_spi0_b_ck",
			"ck_spi0_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI1_BCLK, "ck_spi1_b_ck",
			"ck_spi1_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI2_BCLK, "ck_spi2_b_ck",
			"ck_spi2_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI3_BCLK, "ck_spi3_b_ck",
			"ck_spi3_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI4_BCLK, "ck_spi4_b_ck",
			"ck_spi4_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI5_BCLK, "ck_spi5_b_ck",
			"ck_spi5_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI6_BCLK, "ck_spi6_b_ck",
			"ck_spi6_b_sel", 1, 1),
	FACTOR(CLK_CK_SPI7_BCLK, "ck_spi7_b_ck",
			"ck_spi7_b_sel", 1, 1),
	FACTOR(CLK_CK_MSDC30_1, "ck_msdc30_1_ck",
			"ck_msdc30_1_sel", 1, 1),
	FACTOR(CLK_CK_MSDC30_2, "ck_msdc30_2_ck",
			"ck_msdc30_2_sel", 1, 1),
	FACTOR(CLK_CK_I2C_PERI, "ck_i2c_p_ck",
			"ck_i2c_p_sel", 1, 1),
	FACTOR(CLK_CK_I2C_EAST, "ck_i2c_east_ck",
			"ck_i2c_east_sel", 1, 1),
	FACTOR(CLK_CK_I2C_WEST, "ck_i2c_west_ck",
			"ck_i2c_west_sel", 1, 1),
	FACTOR(CLK_CK_I2C_NORTH, "ck_i2c_north_ck",
			"ck_i2c_north_sel", 1, 1),
	FACTOR(CLK_CK_AES_UFSFDE, "ck_aes_ufsfde_ck",
			"ck_aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_CK_UFS, "ck_ck",
			"ck_sel", 1, 1),
	FACTOR(CLK_CK_AUD_1, "ck_aud_1_ck",
			"ck_aud_1_sel", 1, 1),
	FACTOR(CLK_CK_AUD_2, "ck_aud_2_ck",
			"ck_aud_2_sel", 1, 1),
	FACTOR(CLK_CK_DPMAIF_MAIN, "ck_dpmaif_main_ck",
			"ck_dpmaif_main_sel", 1, 1),
	FACTOR(CLK_CK_PWM, "ck_pwm_ck",
			"ck_pwm_sel", 1, 1),
	FACTOR(CLK_CK_TL, "ck_tl_ck",
			"ck_tl_sel", 1, 1),
	FACTOR(CLK_CK_TL_P1, "ck_tl_p1_ck",
			"ck_tl_p1_sel", 1, 1),
	FACTOR(CLK_CK_TL_P2, "ck_tl_p2_ck",
			"ck_tl_p2_sel", 1, 1),
	FACTOR(CLK_CK_SSR_RNG, "ck_ssr_rng_ck",
			"ck_ssr_rng_sel", 1, 1),
};

static const struct mtk_fixed_factor vlp_ck_divs[] = {
	FACTOR(CLK_VLP_CK_OSC3, "vlp_osc3",
			"ulposc3", 1, 1),
	FACTOR(CLK_VLP_CK_CLKSQ, "vlp_clksq_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_VLP_CK_AUDIO_H, "vlp_audio_h_ck",
			"vlp_audio_h_sel", 1, 1),
	FACTOR(CLK_VLP_CK_AUD_ENGEN1, "vlp_aud_engen1_ck",
			"vlp_aud_engen1_sel", 1, 1),
	FACTOR(CLK_VLP_CK_AUD_ENGEN2, "vlp_aud_engen2_ck",
			"vlp_aud_engen2_sel", 1, 1),
	FACTOR(CLK_VLP_CK_INFRA_26M, "vlp_infra_26m_ck",
			"ck_tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_AUD_CLKSQ, "vlp_aud_clksq_ck",
			"vlp_clksq_ck", 1, 1),
};

static const struct mtk_fixed_factor ck2_divs[] = {
	FACTOR(CLK_CK2_MAINPLL2_D2, "ck2_mainpll2_d2",
			"mainpll2", 1, 2),
	FACTOR(CLK_CK2_MAINPLL2_D3, "ck2_mainpll2_d3",
			"mainpll2", 1, 3),
	FACTOR(CLK_CK2_MAINPLL2_D4, "ck2_mainpll2_d4",
			"mainpll2", 1, 4),
	FACTOR(CLK_CK2_MAINPLL2_D4_D2, "ck2_mainpll2_d4_d2",
			"mainpll2", 1, 8),
	FACTOR(CLK_CK2_MAINPLL2_D4_D4, "ck2_mainpll2_d4_d4",
			"mainpll2", 1, 16),
	FACTOR(CLK_CK2_MAINPLL2_D5, "ck2_mainpll2_d5",
			"mainpll2", 1, 5),
	FACTOR(CLK_CK2_MAINPLL2_D5_D2, "ck2_mainpll2_d5_d2",
			"mainpll2", 1, 10),
	FACTOR(CLK_CK2_MAINPLL2_D6, "ck2_mainpll2_d6",
			"mainpll2", 1, 6),
	FACTOR(CLK_CK2_MAINPLL2_D6_D2, "ck2_mainpll2_d6_d2",
			"mainpll2", 1, 12),
	FACTOR(CLK_CK2_MAINPLL2_D7, "ck2_mainpll2_d7",
			"mainpll2", 1, 7),
	FACTOR(CLK_CK2_MAINPLL2_D7_D2, "ck2_mainpll2_d7_d2",
			"mainpll2", 1, 14),
	FACTOR(CLK_CK2_MAINPLL2_D9, "ck2_mainpll2_d9",
			"mainpll2", 1, 9),
	FACTOR(CLK_CK2_UNIVPLL2_D3, "ck2_univpll2_d3",
			"univpll2", 1, 3),
	FACTOR(CLK_CK2_UNIVPLL2_D4, "ck2_univpll2_d4",
			"univpll2", 1, 4),
	FACTOR(CLK_CK2_UNIVPLL2_D4_D2, "ck2_univpll2_d4_d2",
			"univpll2", 1, 8),
	FACTOR(CLK_CK2_UNIVPLL2_D5, "ck2_univpll2_d5",
			"univpll2", 1, 5),
	FACTOR(CLK_CK2_UNIVPLL2_D5_D2, "ck2_univpll2_d5_d2",
			"univpll2", 1, 10),
	FACTOR(CLK_CK2_UNIVPLL2_D6, "ck2_univpll2_d6",
			"univpll2", 1, 6),
	FACTOR(CLK_CK2_UNIVPLL2_D6_D2, "ck2_univpll2_d6_d2",
			"univpll2", 1, 12),
	FACTOR(CLK_CK2_UNIVPLL2_D6_D4, "ck2_univpll2_d6_d4",
			"univpll2", 1, 24),
	FACTOR(CLK_CK2_UNIVPLL2_D7, "ck2_univpll2_d7",
			"univpll2", 1, 7),
	FACTOR(CLK_CK2_IMGPLL_D2, "ck2_imgpll_d2",
			"imgpll", 1, 2),
	FACTOR(CLK_CK2_IMGPLL_D4, "ck2_imgpll_d4",
			"imgpll", 1, 4),
	FACTOR(CLK_CK2_IMGPLL_D5, "ck2_imgpll_d5",
			"imgpll", 1, 5),
	FACTOR(CLK_CK2_IMGPLL_D5_D2, "ck2_imgpll_d5_d2",
			"imgpll", 1, 10),
	FACTOR(CLK_CK2_MMPLL2_D3, "ck2_mmpll2_d3",
			"mmpll2", 1, 3),
	FACTOR(CLK_CK2_MMPLL2_D4, "ck2_mmpll2_d4",
			"mmpll2", 1, 4),
	FACTOR(CLK_CK2_MMPLL2_D4_D2, "ck2_mmpll2_d4_d2",
			"mmpll2", 1, 8),
	FACTOR(CLK_CK2_MMPLL2_D5, "ck2_mmpll2_d5",
			"mmpll2", 1, 5),
	FACTOR(CLK_CK2_MMPLL2_D5_D2, "ck2_mmpll2_d5_d2",
			"mmpll2", 1, 10),
	FACTOR(CLK_CK2_MMPLL2_D6, "ck2_mmpll2_d6",
			"mmpll2", 1, 6),
	FACTOR(CLK_CK2_MMPLL2_D6_D2, "ck2_mmpll2_d6_d2",
			"mmpll2", 1, 12),
	FACTOR(CLK_CK2_MMPLL2_D7, "ck2_mmpll2_d7",
			"mmpll2", 1, 7),
	FACTOR(CLK_CK2_MMPLL2_D9, "ck2_mmpll2_d9",
			"mmpll2", 1, 9),
	FACTOR(CLK_CK2_TVDPLL1_D4, "ck2_tvdpll1_d4",
			"tvdpll1", 1, 4),
	FACTOR(CLK_CK2_TVDPLL1_D8, "ck2_tvdpll1_d8",
			"tvdpll1", 1, 8),
	FACTOR(CLK_CK2_TVDPLL1_D16, "ck2_tvdpll1_d16",
			"tvdpll1", 1, 16),
	FACTOR(CLK_CK2_TVDPLL2_D2, "ck2_tvdpll2_d2",
			"tvdpll2", 1, 2),
	FACTOR(CLK_CK2_TVDPLL2_D4, "ck2_tvdpll2_d4",
			"tvdpll2", 1, 4),
	FACTOR(CLK_CK2_TVDPLL2_D8, "ck2_tvdpll2_d8",
			"tvdpll2", 1, 8),
	FACTOR(CLK_CK2_TVDPLL2_D16, "ck2_tvdpll2_d16",
			"tvdpll2", 92, 1473),
	FACTOR(CLK_CK2_CCUSYS, "ck2_ccusys_ck",
			"ck2_ccusys_sel", 1, 1),
	FACTOR(CLK_CK2_VENC, "ck2_venc_ck",
			"ck2_venc_sel", 1, 1),
	FACTOR(CLK_CK2_MMINFRA, "ck2_mminfra_ck",
			"ck2_mminfra_sel", 1, 1),
	FACTOR(CLK_CK2_IMG1, "ck2_img1_ck",
			"ck2_img1_sel", 1, 1),
	FACTOR(CLK_CK2_IPE, "ck2_ipe_ck",
			"ck2_ipe_sel", 1, 1),
	FACTOR(CLK_CK2_CAM, "ck2_cam_ck",
			"ck2_cam_sel", 1, 1),
	FACTOR(CLK_CK2_CAMTM, "ck2_camtm_ck",
			"ck2_camtm_sel", 1, 1),
	FACTOR(CLK_CK2_DPE, "ck2_dpe_ck",
			"ck2_dpe_sel", 1, 1),
	FACTOR(CLK_CK2_VDEC, "ck2_vdec_ck",
			"ck2_vdec_sel", 1, 1),
	FACTOR(CLK_CK2_DP1, "ck2_dp1_ck",
			"ck2_dp1_sel", 1, 1),
	FACTOR(CLK_CK2_DP0, "ck2_dp0_ck",
			"ck2_dp0_sel", 1, 1),
	FACTOR(CLK_CK2_DISP, "ck2_disp_ck",
			"ck2_disp_sel", 1, 1),
	FACTOR(CLK_CK2_MDP, "ck2_mdp_ck",
			"ck2_mdp_sel", 1, 1),
	FACTOR(CLK_CK2_AVS_IMG, "ck2_avs_img_ck",
			"ck_tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_CK2_AVS_VDEC, "ck2_avs_vdec_ck",
			"ck_tck_26m_mx9_ck", 1, 1),
};

static const char * const ck_axi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_osc_d8",
	"ck_osc_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d7_d2"
};

static const char * const ck_mem_sub_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_osc_d4",
	"ck_univpll_d4_d4",
	"ck_osc_d3",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5",
	"ck_mainpll_d4",
	"ck_mainpll_d3"
};

static const char * const ck_io_noc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_osc_d8",
	"ck_osc_d4",
	"ck_mainpll_d6_d2",
	"ck_mainpll_d9"
};

static const char * const ck_p_axi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d8",
	"ck_mainpll_d5_d8",
	"ck_osc_d8",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d7_d2"
};

static const char * const ck_pextp0_axi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d8",
	"ck_mainpll_d5_d8",
	"ck_osc_d8",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d7_d2"
};

static const char * const ck_pextp1_usb_axi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d8",
	"ck_mainpll_d5_d8",
	"ck_osc_d8",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d7_d2"
};

static const char * const ck_p_fmem_sub_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_osc_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5",
	"ck_mainpll_d4"
};

static const char * const ck_pexpt0_mem_sub_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_osc_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5",
	"ck_mainpll_d4"
};

static const char * const ck_pextp1_usb_mem_sub_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_osc_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5",
	"ck_mainpll_d4"
};

static const char * const ck_p_noc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_osc_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5",
	"ck_mainpll_d4",
	"ck_mainpll_d3"
};

static const char * const ck_emi_n_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_emipll1_ck"
};

static const char * const ck_emi_n_b0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d6_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d5",
	"ck_emipll1_ck"
};

static const char * const ck_emi_s_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d5_d8",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_emipll1_ck"
};

static const char * const ck_emi_s_b0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d6_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d5",
	"ck_emipll1_ck"
};

static const char * const ck_ap2conn_host_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4"
};

static const char * const ck_atb_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6"
};

static const char * const ck_cirq_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d7_d4"
};

static const char * const ck_pbus_156m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d2",
	"ck_osc_d2",
	"ck_mainpll_d7"
};

static const char * const ck_efuse_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const ck_mcl3gic_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d8",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d7_d2"
};

static const char * const ck_mcinfra_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d9",
	"ck_mainpll_d6"
};

static const char * const ck_dsp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck_univpll_d6_d2",
	"ck_osc_d2",
	"ck_univpll_d5",
	"ck_osc"
};

static const char * const ck_mfg_ref_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d2"
};

static const char * const ck_mfg_eb_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d6_d2",
	"ck_mainpll_d5_d2"
};

static const char * const ck_uart_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d8",
	"ck_univpll_d6_d4",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi0_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi1_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi2_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi3_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi4_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi5_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi6_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_spi7_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d4_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_192m",
	"ck_univpll_d6_d2"
};

static const char * const ck_msdc30_1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_d6_d2",
	"ck_msdcpll_d2"
};

static const char * const ck_msdc30_2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_mainpll_d6_d2",
	"ck_univpll_d6_d2",
	"ck_msdcpll_d2"
};

static const char * const ck_disp_pwm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d32",
	"ck_osc_d8",
	"ck_univpll_d6_d4",
	"ck_univpll_d5_d4",
	"ck_osc_d4",
	"ck_mainpll_d4_d4"
};

static const char * const ck_usb_1p_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d5_d4"
};

static const char * const ck_usb_xhci_1p_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d5_d4"
};

static const char * const ck_usb_fmcnt_p1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d4"
};

static const char * const ck_i2c_p_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d5_d2"
};

static const char * const ck_i2c_east_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d5_d2"
};

static const char * const ck_i2c_west_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d5_d2"
};

static const char * const ck_i2c_north_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8",
	"ck_univpll_d5_d4",
	"ck_mainpll_d4_d4",
	"ck_univpll_d5_d2"
};

static const char * const ck_aes_ufsfde_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_univpll_d6_d2",
	"ck_mainpll_d4_d2",
	"ck_univpll_d6",
	"ck_mainpll_d4"
};

static const char * const ck_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_univpll_d6_d2",
	"ck_mainpll_d4_d2",
	"ck_univpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5"
};

static const char * const ck_aud_1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_apll1_ck"
};

static const char * const ck_aud_2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_apll2_ck"
};

static const char * const ck_adsp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_adsppll_ck"
};

static const char * const ck_adsp_uarthub_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d6_d2"
};

static const char * const ck_dpmaif_main_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d4_d4",
	"ck_univpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_univpll_d4_d2",
	"ck_mainpll_d6",
	"ck_univpll_d6",
	"ck_mainpll_d5",
	"ck_univpll_d5"
};

static const char * const ck_pwm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4",
	"ck_univpll_d4_d8"
};

static const char * const ck_mcupm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d6_d2",
	"ck_univpll_d6_d2",
	"ck_mainpll_d5_d2"
};

static const char * const ck_ipseast_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d6",
	"ck_mainpll_d5",
	"ck_mainpll_d4",
	"ck_mainpll_d3"
};

static const char * const ck_tl_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d5_d2"
};

static const char * const ck_tl_p1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d5_d2"
};

static const char * const ck_tl_p2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d5_d2"
};

static const char * const ck_md_emi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4"
};

static const char * const ck_sdf_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck_mainpll_d4",
	"ck_univpll_d4"
};

static const char * const ck_uarthub_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_d6_d4",
	"ck_univpll_d6_d2"
};

static const char * const ck_dpsw_cmp_26m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const ck_smapck_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8"
};

static const char * const ck_ssr_pka_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const char * const ck_ssr_dma_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const char * const ck_ssr_kdf_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7"
};

static const char * const ck_ssr_rng_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2"
};

static const char * const ck_spu0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const char * const ck_spu1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const char * const ck_dxcc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d4_d8",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2"
};

static const char * const ck_apll_i2sin0_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sin1_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sin2_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sin3_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sin4_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sin6_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout0_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout1_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout2_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout3_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout4_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_i2sout6_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_fmi2s_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const char * const ck_apll_tdmout_m_parents[] = {
	"ck_aud_1_sel",
	"ck_aud_2_sel"
};

static const struct mtk_mux ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AXI_SEL/* dts */, "ck_axi_sel",
		ck_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MEM_SUB_SEL/* dts */, "ck_mem_sub_sel",
		ck_mem_sub_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_IO_NOC_SEL/* dts */, "ck_io_noc_sel",
		ck_io_noc_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_AXI_SEL/* dts */, "ck_p_axi_sel",
		ck_p_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP0_AXI_SEL/* dts */, "ck_pextp0_axi_sel",
		ck_pextp0_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXTP0_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_AXI_SEL/* dts */, "ck_pextp1_usb_axi_sel",
		ck_pextp1_usb_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_FMEM_SUB_SEL/* dts */, "ck_p_fmem_sub_sel",
		ck_p_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXPT0_MEM_SUB_SEL/* dts */, "ck_pexpt0_mem_sub_sel",
		ck_pexpt0_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXPT0_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_MEM_SUB_SEL/* dts */, "ck_pextp1_usb_mem_sub_sel",
		ck_pextp1_usb_mem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_NOC_SEL/* dts */, "ck_p_noc_sel",
		ck_p_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_N_SEL/* dts */, "ck_emi_n_sel",
		ck_emi_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_S_SEL/* dts */, "ck_emi_s_sel",
		ck_emi_s_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_AP2CONN_HOST_SEL/* dts */, "ck_ap2conn_host_sel",
		ck_ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ATB_SEL/* dts */, "ck_atb_sel",
		ck_atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_CIRQ_SEL/* dts */, "ck_cirq_sel",
		ck_cirq_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PBUS_156M_SEL/* dts */, "ck_pbus_156m_sel",
		ck_pbus_156m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PBUS_156M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_EFUSE_SEL/* dts */, "ck_efuse_sel",
		ck_efuse_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCL3GIC_SEL/* dts */, "ck_mcl3gic_sel",
		ck_mcl3gic_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCINFRA_SEL/* dts */, "ck_mcinfra_sel",
		ck_mcinfra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_DSP_SEL/* dts */, "ck_dsp_sel",
		ck_dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_REF_SEL/* dts */, "ck_mfg_ref_sel",
		ck_mfg_ref_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_EB_SEL/* dts */, "ck_mfg_eb_sel",
		ck_mfg_eb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_UART_SEL/* dts */, "ck_uart_sel",
		ck_uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI0_BCLK_SEL/* dts */, "ck_spi0_b_sel",
		ck_spi0_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI1_BCLK_SEL/* dts */, "ck_spi1_b_sel",
		ck_spi1_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI2_BCLK_SEL/* dts */, "ck_spi2_b_sel",
		ck_spi2_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI3_BCLK_SEL/* dts */, "ck_spi3_b_sel",
		ck_spi3_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI4_BCLK_SEL/* dts */, "ck_spi4_b_sel",
		ck_spi4_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI5_BCLK_SEL/* dts */, "ck_spi5_b_sel",
		ck_spi5_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI6_BCLK_SEL/* dts */, "ck_spi6_b_sel",
		ck_spi6_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI7_BCLK_SEL/* dts */, "ck_spi7_b_sel",
		ck_spi7_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MSDC30_1_SEL/* dts */, "ck_msdc30_1_sel",
		ck_msdc30_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MSDC30_2_SEL/* dts */, "ck_msdc30_2_sel",
		ck_msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 23/* cksta shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DISP_PWM_SEL/* dts */, "ck_disp_pwm_sel",
		ck_disp_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_TOP_1P_SEL/* dts */, "ck_usb_1p_sel",
		ck_usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_XHCI_1P_SEL/* dts */, "ck_usb_xhci_1p_sel",
		ck_usb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_FMCNT_P1_SEL/* dts */, "ck_usb_fmcnt_p1_sel",
		ck_usb_fmcnt_p1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_FMCNT_P1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_P_SEL/* dts */, "ck_i2c_p_sel",
		ck_i2c_p_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_PERI_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_EAST_SEL/* dts */, "ck_i2c_east_sel",
		ck_i2c_east_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_EAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_WEST_SEL/* dts */, "ck_i2c_west_sel",
		ck_i2c_west_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_WEST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_NORTH_SEL/* dts */, "ck_i2c_north_sel",
		ck_i2c_north_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_NORTH_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AES_UFSFDE_SEL/* dts */, "ck_aes_ufsfde_sel",
		ck_aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SEL/* dts */, "ck_sel",
		ck_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AUD_1_SEL/* dts */, "ck_aud_1_sel",
		ck_aud_1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_AUD_2_SEL/* dts */, "ck_aud_2_sel",
		ck_aud_2_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_SEL/* dts */, "ck_adsp_sel",
		ck_adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_UARTHUB_BCLK_SEL/* dts */, "ck_adsp_uarthub_b_sel",
		ck_adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPMAIF_MAIN_SEL/* dts */, "ck_dpmaif_main_sel",
		ck_dpmaif_main_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PWM_SEL/* dts */, "ck_pwm_sel",
		ck_pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCUPM_SEL/* dts */, "ck_mcupm_sel",
		ck_mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_IPSEAST_SEL/* dts */, "ck_ipseast_sel",
		ck_ipseast_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSEAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_SEL/* dts */, "ck_tl_sel",
		ck_tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_P1_SEL/* dts */, "ck_tl_p1_sel",
		ck_tl_p1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_P1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_P2_SEL/* dts */, "ck_tl_p2_sel",
		ck_tl_p2_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_P2_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_INTERFACE_546_SEL/* dts */, "ck_md_emi_sel",
		ck_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SDF_SEL/* dts */, "ck_sdf_sel",
		ck_sdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_UARTHUB_BCLK_SEL/* dts */, "ck_uarthub_b_sel",
		ck_uarthub_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPSW_CMP_26M_SEL/* dts */, "ck_dpsw_cmp_26m_sel",
		ck_dpsw_cmp_26m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SMAPCK_SEL/* dts */, "ck_smapck_sel",
		ck_smapck_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_PKA_SEL/* dts */, "ck_ssr_pka_sel",
		ck_ssr_pka_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_DMA_SEL/* dts */, "ck_ssr_dma_sel",
		ck_ssr_dma_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_KDF_SEL/* dts */, "ck_ssr_kdf_sel",
		ck_ssr_kdf_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_RNG_SEL/* dts */, "ck_ssr_rng_sel",
		ck_ssr_rng_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU0_SEL/* dts */, "ck_spu0_sel",
		ck_spu0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU0_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU1_SEL/* dts */, "ck_spu1_sel",
		ck_spu1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DXCC_SEL/* dts */, "ck_dxcc_sel",
		ck_dxcc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AXI_SEL/* dts */, "ck_axi_sel",
		ck_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MEM_SUB_SEL/* dts */, "ck_mem_sub_sel",
		ck_mem_sub_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_IO_NOC_SEL/* dts */, "ck_io_noc_sel",
		ck_io_noc_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_AXI_SEL/* dts */, "ck_p_axi_sel",
		ck_p_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP0_AXI_SEL/* dts */, "ck_pextp0_axi_sel",
		ck_pextp0_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXTP0_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_AXI_SEL/* dts */, "ck_pextp1_usb_axi_sel",
		ck_pextp1_usb_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_FMEM_SUB_SEL/* dts */, "ck_p_fmem_sub_sel",
		ck_p_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXPT0_MEM_SUB_SEL/* dts */, "ck_pexpt0_mem_sub_sel",
		ck_pexpt0_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXPT0_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_MEM_SUB_SEL/* dts */, "ck_pextp1_usb_mem_sub_sel",
		ck_pextp1_usb_mem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_NOC_SEL/* dts */, "ck_p_noc_sel",
		ck_p_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_N_SEL/* dts */, "ck_emi_n_sel",
		ck_emi_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_S_SEL/* dts */, "ck_emi_s_sel",
		ck_emi_s_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AP2CONN_HOST_SEL/* dts */, "ck_ap2conn_host_sel",
		ck_ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ATB_SEL/* dts */, "ck_atb_sel",
		ck_atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_CIRQ_SEL/* dts */, "ck_cirq_sel",
		ck_cirq_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PBUS_156M_SEL/* dts */, "ck_pbus_156m_sel",
		ck_pbus_156m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PBUS_156M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_EFUSE_SEL/* dts */, "ck_efuse_sel",
		ck_efuse_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCL3GIC_SEL/* dts */, "ck_mcl3gic_sel",
		ck_mcl3gic_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCINFRA_SEL/* dts */, "ck_mcinfra_sel",
		ck_mcinfra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_DSP_SEL/* dts */, "ck_dsp_sel",
		ck_dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MFG_REF_SEL/* dts */, "ck_mfg_ref_sel",
		ck_mfg_ref_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		7/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_EB_SEL/* dts */, "ck_mfg_eb_sel",
		ck_mfg_eb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_MULT_HWV_FENC(CLK_CK_UART_SEL/* dts */, "ck_uart_sel",
		ck_uart_parents/* parent */, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_3_DONE,
		HWV_CG_3_SET, HWV_CG_3_CLR, /* hwv */
		24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		4/* fenc shift */),
	/* CLK_CFG_7 */
	MUX_MULT_HWV_FENC(CLK_CK_SPI0_BCLK_SEL/* dts */, "ck_spi0_b_sel",
		ck_spi0_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		3/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI1_BCLK_SEL/* dts */, "ck_spi1_b_sel",
		ck_spi1_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		2/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI2_BCLK_SEL/* dts */, "ck_spi2_b_sel",
		ck_spi2_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		1/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		1/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI3_BCLK_SEL/* dts */, "ck_spi3_b_sel",
		ck_spi3_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		0/* fenc shift */),
	/* CLK_CFG_8 */
	MUX_MULT_HWV_FENC(CLK_CK_SPI4_BCLK_SEL/* dts */, "ck_spi4_b_sel",
		ck_spi4_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		31/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI5_BCLK_SEL/* dts */, "ck_spi5_b_sel",
		ck_spi5_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		30/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI6_BCLK_SEL/* dts */, "ck_spi6_b_sel",
		ck_spi6_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		28/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		29/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI7_BCLK_SEL/* dts */, "ck_spi7_b_sel",
		ck_spi7_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		28/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MSDC30_1_SEL/* dts */, "ck_msdc30_1_sel",
		ck_msdc30_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MSDC30_2_SEL/* dts */, "ck_msdc30_2_sel",
		ck_msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_10 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_DISP_PWM_SEL/* dts */, "ck_disp_pwm_sel",
		ck_disp_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		23/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_TOP_1P_SEL/* dts */, "ck_usb_1p_sel",
		ck_usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		22/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_XHCI_1P_SEL/* dts */, "ck_usb_xhci_1p_sel",
		ck_usb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		21/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_FMCNT_P1_SEL/* dts */, "ck_usb_fmcnt_p1_sel",
		ck_usb_fmcnt_p1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_FMCNT_P1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		20/* fenc shift */),
	/* CLK_CFG_11 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_P_SEL/* dts */, "ck_i2c_p_sel",
		ck_i2c_p_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_PERI_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		19/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_EAST_SEL/* dts */, "ck_i2c_east_sel",
		ck_i2c_east_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_EAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		18/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_WEST_SEL/* dts */, "ck_i2c_west_sel",
		ck_i2c_west_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_WEST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		17/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_I2C_NORTH_SEL/* dts */, "ck_i2c_north_sel",
		ck_i2c_north_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_6_DONE,
		HWV_CG_6_SET, HWV_CG_6_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_NORTH_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		16/* fenc shift */),
	/* CLK_CFG_12 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AES_UFSFDE_SEL/* dts */, "ck_aes_ufsfde_sel",
		ck_aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		15/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK_FLAGS2(CLK_CK_SEL/* dts */, "ck_sel",
		ck_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		14/* fenc shift */, MUX_ROUND_CLOSEST),
	/* CLK_CFG_13 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AUD_1_SEL/* dts */, "ck_aud_1_sel",
		ck_aud_1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		10/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		11/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AUD_2_SEL/* dts */, "ck_aud_2_sel",
		ck_aud_2_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		9/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_ADSP_SEL/* dts */, "ck_adsp_sel",
		ck_adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		8/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		9/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_UARTHUB_BCLK_SEL/* dts */, "ck_adsp_uarthub_b_sel",
		ck_adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_DPMAIF_MAIN_SEL/* dts */, "ck_dpmaif_main_sel",
		ck_dpmaif_main_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		6/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		7/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_PWM_SEL/* dts */, "ck_pwm_sel",
		ck_pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		6/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCUPM_SEL/* dts */, "ck_mcupm_sel",
		ck_mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_IPSEAST_SEL/* dts */, "ck_ipseast_sel",
		ck_ipseast_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSEAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		3/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_SEL/* dts */, "ck_tl_sel",
		ck_tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		1/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_P1_SEL/* dts */, "ck_tl_p1_sel",
		ck_tl_p1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_P1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		0/* fenc shift */),
	/* CLK_CFG_16 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_P2_SEL/* dts */, "ck_tl_p2_sel",
		ck_tl_p2_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_P2_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		31/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_INTERFACE_546_SEL/* dts */, "ck_md_emi_sel",
		ck_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SDF_SEL/* dts */, "ck_sdf_sel",
		ck_sdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_MULT_HWV_FENC(CLK_CK_UARTHUB_BCLK_SEL/* dts */, "ck_uarthub_b_sel",
		ck_uarthub_b_parents/* parent */, CLK_CFG_16,
		CLK_CFG_16_SET, CLK_CFG_16_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_7_DONE,
		HWV_CG_7_SET, HWV_CG_7_CLR, /* hwv */
		24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		28/* fenc shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPSW_CMP_26M_SEL/* dts */, "ck_dpsw_cmp_26m_sel",
		ck_dpsw_cmp_26m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SMAPCK_SEL/* dts */, "ck_smapck_sel",
		ck_smapck_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_PKA_SEL/* dts */, "ck_ssr_pka_sel",
		ck_ssr_pka_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_DMA_SEL/* dts */, "ck_ssr_dma_sel",
		ck_ssr_dma_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_KDF_SEL/* dts */, "ck_ssr_kdf_sel",
		ck_ssr_kdf_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_RNG_SEL/* dts */, "ck_ssr_rng_sel",
		ck_ssr_rng_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU0_SEL/* dts */, "ck_spu0_sel",
		ck_spu0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU0_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU1_SEL/* dts */, "ck_spu1_sel",
		ck_spu1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DXCC_SEL/* dts */, "ck_dxcc_sel",
		ck_dxcc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */),
#endif
};

static const struct mtk_mux ck_muxes_b0[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AXI_SEL/* dts */, "ck_axi_sel",
		ck_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MEM_SUB_SEL/* dts */, "ck_mem_sub_sel",
		ck_mem_sub_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_IO_NOC_SEL/* dts */, "ck_io_noc_sel",
		ck_io_noc_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_AXI_SEL/* dts */, "ck_p_axi_sel",
		ck_p_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP0_AXI_SEL/* dts */, "ck_pextp0_axi_sel",
		ck_pextp0_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXTP0_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_AXI_SEL/* dts */, "ck_pextp1_usb_axi_sel",
		ck_pextp1_usb_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_FMEM_SUB_SEL/* dts */, "ck_p_fmem_sub_sel",
		ck_p_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXPT0_MEM_SUB_SEL/* dts */, "ck_pexpt0_mem_sub_sel",
		ck_pexpt0_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXPT0_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_MEM_SUB_SEL/* dts */, "ck_pextp1_usb_mem_sub_sel",
		ck_pextp1_usb_mem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_NOC_SEL/* dts */, "ck_p_noc_sel",
		ck_p_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_N_SEL/* dts */, "ck_emi_n_sel",
		ck_emi_n_b0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_S_SEL/* dts */, "ck_emi_s_sel",
		ck_emi_s_b0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_AP2CONN_HOST_SEL/* dts */, "ck_ap2conn_host_sel",
		ck_ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ATB_SEL/* dts */, "ck_atb_sel",
		ck_atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_CIRQ_SEL/* dts */, "ck_cirq_sel",
		ck_cirq_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PBUS_156M_SEL/* dts */, "ck_pbus_156m_sel",
		ck_pbus_156m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PBUS_156M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_EFUSE_SEL/* dts */, "ck_efuse_sel",
		ck_efuse_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCL3GIC_SEL/* dts */, "ck_mcl3gic_sel",
		ck_mcl3gic_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCINFRA_SEL/* dts */, "ck_mcinfra_sel",
		ck_mcinfra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_DSP_SEL/* dts */, "ck_dsp_sel",
		ck_dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_REF_SEL/* dts */, "ck_mfg_ref_sel",
		ck_mfg_ref_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_EB_SEL/* dts */, "ck_mfg_eb_sel",
		ck_mfg_eb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_UART_SEL/* dts */, "ck_uart_sel",
		ck_uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI0_BCLK_SEL/* dts */, "ck_spi0_b_sel",
		ck_spi0_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI1_BCLK_SEL/* dts */, "ck_spi1_b_sel",
		ck_spi1_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI2_BCLK_SEL/* dts */, "ck_spi2_b_sel",
		ck_spi2_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI3_BCLK_SEL/* dts */, "ck_spi3_b_sel",
		ck_spi3_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI4_BCLK_SEL/* dts */, "ck_spi4_b_sel",
		ck_spi4_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI5_BCLK_SEL/* dts */, "ck_spi5_b_sel",
		ck_spi5_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI6_BCLK_SEL/* dts */, "ck_spi6_b_sel",
		ck_spi6_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPI7_BCLK_SEL/* dts */, "ck_spi7_b_sel",
		ck_spi7_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MSDC30_1_SEL/* dts */, "ck_msdc30_1_sel",
		ck_msdc30_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MSDC30_2_SEL/* dts */, "ck_msdc30_2_sel",
		ck_msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 23/* cksta shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DISP_PWM_SEL/* dts */, "ck_disp_pwm_sel",
		ck_disp_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_TOP_1P_SEL/* dts */, "ck_usb_1p_sel",
		ck_usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_XHCI_1P_SEL/* dts */, "ck_usb_xhci_1p_sel",
		ck_usb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_USB_FMCNT_P1_SEL/* dts */, "ck_usb_fmcnt_p1_sel",
		ck_usb_fmcnt_p1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_FMCNT_P1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_P_SEL/* dts */, "ck_i2c_p_sel",
		ck_i2c_p_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_PERI_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_EAST_SEL/* dts */, "ck_i2c_east_sel",
		ck_i2c_east_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_EAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_WEST_SEL/* dts */, "ck_i2c_west_sel",
		ck_i2c_west_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_WEST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_I2C_NORTH_SEL/* dts */, "ck_i2c_north_sel",
		ck_i2c_north_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_NORTH_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AES_UFSFDE_SEL/* dts */, "ck_aes_ufsfde_sel",
		ck_aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SEL/* dts */, "ck_sel",
		ck_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AUD_1_SEL/* dts */, "ck_aud_1_sel",
		ck_aud_1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_AUD_2_SEL/* dts */, "ck_aud_2_sel",
		ck_aud_2_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_SEL/* dts */, "ck_adsp_sel",
		ck_adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_UARTHUB_BCLK_SEL/* dts */, "ck_adsp_uarthub_b_sel",
		ck_adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPMAIF_MAIN_SEL/* dts */, "ck_dpmaif_main_sel",
		ck_dpmaif_main_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PWM_SEL/* dts */, "ck_pwm_sel",
		ck_pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCUPM_SEL/* dts */, "ck_mcupm_sel",
		ck_mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_IPSEAST_SEL/* dts */, "ck_ipseast_sel",
		ck_ipseast_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSEAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_SEL/* dts */, "ck_tl_sel",
		ck_tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_P1_SEL/* dts */, "ck_tl_p1_sel",
		ck_tl_p1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_P1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_TL_P2_SEL/* dts */, "ck_tl_p2_sel",
		ck_tl_p2_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_P2_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_INTERFACE_546_SEL/* dts */, "ck_md_emi_sel",
		ck_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SDF_SEL/* dts */, "ck_sdf_sel",
		ck_sdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_UARTHUB_BCLK_SEL/* dts */, "ck_uarthub_b_sel",
		ck_uarthub_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPSW_CMP_26M_SEL/* dts */, "ck_dpsw_cmp_26m_sel",
		ck_dpsw_cmp_26m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SMAPCK_SEL/* dts */, "ck_smapck_sel",
		ck_smapck_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_PKA_SEL/* dts */, "ck_ssr_pka_sel",
		ck_ssr_pka_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_DMA_SEL/* dts */, "ck_ssr_dma_sel",
		ck_ssr_dma_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_KDF_SEL/* dts */, "ck_ssr_kdf_sel",
		ck_ssr_kdf_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_RNG_SEL/* dts */, "ck_ssr_rng_sel",
		ck_ssr_rng_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU0_SEL/* dts */, "ck_spu0_sel",
		ck_spu0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU0_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU1_SEL/* dts */, "ck_spu1_sel",
		ck_spu1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DXCC_SEL/* dts */, "ck_dxcc_sel",
		ck_dxcc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AXI_SEL/* dts */, "ck_axi_sel",
		ck_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MEM_SUB_SEL/* dts */, "ck_mem_sub_sel",
		ck_mem_sub_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_IO_NOC_SEL/* dts */, "ck_io_noc_sel",
		ck_io_noc_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IO_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_AXI_SEL/* dts */, "ck_p_axi_sel",
		ck_p_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP0_AXI_SEL/* dts */, "ck_pextp0_axi_sel",
		ck_pextp0_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXTP0_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_AXI_SEL/* dts */, "ck_pextp1_usb_axi_sel",
		ck_pextp1_usb_axi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_FMEM_SUB_SEL/* dts */, "ck_p_fmem_sub_sel",
		ck_p_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXPT0_MEM_SUB_SEL/* dts */, "ck_pexpt0_mem_sub_sel",
		ck_pexpt0_mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_PEXPT0_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_PEXTP1_USB_MEM_SUB_SEL/* dts */, "ck_pextp1_usb_mem_sub_sel",
		ck_pextp1_usb_mem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP1_USB_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_P_NOC_SEL/* dts */, "ck_p_noc_sel",
		ck_p_noc_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_NOC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_N_SEL/* dts */, "ck_emi_n_sel",
		ck_emi_n_b0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_S_SEL/* dts */, "ck_emi_s_sel",
		ck_emi_s_b0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_AP2CONN_HOST_SEL/* dts */, "ck_ap2conn_host_sel",
		ck_ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ATB_SEL/* dts */, "ck_atb_sel",
		ck_atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_CIRQ_SEL/* dts */, "ck_cirq_sel",
		ck_cirq_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_PBUS_156M_SEL/* dts */, "ck_pbus_156m_sel",
		ck_pbus_156m_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PBUS_156M_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_EFUSE_SEL/* dts */, "ck_efuse_sel",
		ck_efuse_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCL3GIC_SEL/* dts */, "ck_mcl3gic_sel",
		ck_mcl3gic_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCINFRA_SEL/* dts */, "ck_mcinfra_sel",
		ck_mcinfra_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_DSP_SEL/* dts */, "ck_dsp_sel",
		ck_dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MFG_REF_SEL/* dts */, "ck_mfg_ref_sel",
		ck_mfg_ref_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		7/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MFG_EB_SEL/* dts */, "ck_mfg_eb_sel",
		ck_mfg_eb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_MULT_HWV_FENC(CLK_CK_UART_SEL/* dts */, "ck_uart_sel",
		ck_uart_parents/* parent */, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_3_DONE,
		HWV_CG_3_SET, HWV_CG_3_CLR, /* hwv */
		24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		4/* fenc shift */),
	/* CLK_CFG_7 */
	MUX_MULT_HWV_FENC(CLK_CK_SPI0_BCLK_SEL/* dts */, "ck_spi0_b_sel",
		ck_spi0_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		3/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		3/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI1_BCLK_SEL/* dts */, "ck_spi1_b_sel",
		ck_spi1_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		2/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI2_BCLK_SEL/* dts */, "ck_spi2_b_sel",
		ck_spi2_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		1/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		1/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI3_BCLK_SEL/* dts */, "ck_spi3_b_sel",
		ck_spi3_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_4_DONE,
		HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_0/* fenc ofs */,
		0/* fenc shift */),
	/* CLK_CFG_8 */
	MUX_MULT_HWV_FENC(CLK_CK_SPI4_BCLK_SEL/* dts */, "ck_spi4_b_sel",
		ck_spi4_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		31/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI5_BCLK_SEL/* dts */, "ck_spi5_b_sel",
		ck_spi5_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		30/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI6_BCLK_SEL/* dts */, "ck_spi6_b_sel",
		ck_spi6_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		28/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		29/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_SPI7_BCLK_SEL/* dts */, "ck_spi7_b_sel",
		ck_spi7_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_5_DONE,
		HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		27/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		28/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MSDC30_1_SEL/* dts */, "ck_msdc30_1_sel",
		ck_msdc30_1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		24/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_MSDC30_2_SEL/* dts */, "ck_msdc30_2_sel",
		ck_msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		23/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		24/* fenc shift */),
	/* CLK_CFG_10 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_DISP_PWM_SEL/* dts */, "ck_disp_pwm_sel",
		ck_disp_pwm_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		23/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_TOP_1P_SEL/* dts */, "ck_usb_1p_sel",
		ck_usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		21/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		22/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_XHCI_1P_SEL/* dts */, "ck_usb_xhci_1p_sel",
		ck_usb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		20/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		21/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_USB_FMCNT_P1_SEL/* dts */, "ck_usb_fmcnt_p1_sel",
		ck_usb_fmcnt_p1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_FMCNT_P1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		19/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		20/* fenc shift */),
	/* CLK_CFG_11 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_P_SEL/* dts */, "ck_i2c_p_sel",
		ck_i2c_p_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_PERI_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		18/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		19/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_EAST_SEL/* dts */, "ck_i2c_east_sel",
		ck_i2c_east_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_EAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		17/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		18/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_I2C_WEST_SEL/* dts */, "ck_i2c_west_sel",
		ck_i2c_west_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_WEST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		16/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		17/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK_I2C_NORTH_SEL/* dts */, "ck_i2c_north_sel",
		ck_i2c_north_parents/* parent */, CLK_CFG_11,
		CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_6_DONE,
		HWV_CG_6_SET, HWV_CG_6_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_NORTH_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		15/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		16/* fenc shift */),
	/* CLK_CFG_12 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AES_UFSFDE_SEL/* dts */, "ck_aes_ufsfde_sel",
		ck_aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		14/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		15/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK_FLAGS2(CLK_CK_SEL/* dts */, "ck_sel",
		ck_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		13/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		14/* fenc shift */, MUX_ROUND_CLOSEST),
	/* CLK_CFG_13 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AUD_1_SEL/* dts */, "ck_aud_1_sel",
		ck_aud_1_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		10/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		11/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_AUD_2_SEL/* dts */, "ck_aud_2_sel",
		ck_aud_2_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		9/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		10/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_ADSP_SEL/* dts */, "ck_adsp_sel",
		ck_adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		8/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		9/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_ADSP_UARTHUB_BCLK_SEL/* dts */, "ck_adsp_uarthub_b_sel",
		ck_adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_DPMAIF_MAIN_SEL/* dts */, "ck_dpmaif_main_sel",
		ck_dpmaif_main_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		6/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		7/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_PWM_SEL/* dts */, "ck_pwm_sel",
		ck_pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		5/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		6/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_MCUPM_SEL/* dts */, "ck_mcupm_sel",
		ck_mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_IPSEAST_SEL/* dts */, "ck_ipseast_sel",
		ck_ipseast_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSEAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		2/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		3/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_SEL/* dts */, "ck_tl_sel",
		ck_tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		31/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		1/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_P1_SEL/* dts */, "ck_tl_p1_sel",
		ck_tl_p1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_P1_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		30/* cksta shift */, CLK_FENC_STATUS_MON_1/* fenc ofs */,
		0/* fenc shift */),
	/* CLK_CFG_16 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_CK_TL_P2_SEL/* dts */, "ck_tl_p2_sel",
		ck_tl_p2_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_P2_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		29/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		31/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_EMI_INTERFACE_546_SEL/* dts */, "ck_md_emi_sel",
		ck_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SDF_SEL/* dts */, "ck_sdf_sel",
		ck_sdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_MULT_HWV_FENC(CLK_CK_UARTHUB_BCLK_SEL/* dts */, "ck_uarthub_b_sel",
		ck_uarthub_b_parents/* parent */, CLK_CFG_16,
		CLK_CFG_16_SET, CLK_CFG_16_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_7_DONE,
		HWV_CG_7_SET, HWV_CG_7_CLR, /* hwv */
		24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		26/* cksta shift */, CLK_FENC_STATUS_MON_2/* fenc ofs */,
		28/* fenc shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DPSW_CMP_26M_SEL/* dts */, "ck_dpsw_cmp_26m_sel",
		ck_dpsw_cmp_26m_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SMAPCK_SEL/* dts */, "ck_smapck_sel",
		ck_smapck_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SMAPCK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_PKA_SEL/* dts */, "ck_ssr_pka_sel",
		ck_ssr_pka_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_DMA_SEL/* dts */, "ck_ssr_dma_sel",
		ck_ssr_dma_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 22/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_KDF_SEL/* dts */, "ck_ssr_kdf_sel",
		ck_ssr_kdf_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SSR_RNG_SEL/* dts */, "ck_ssr_rng_sel",
		ck_ssr_rng_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU0_SEL/* dts */, "ck_spu0_sel",
		ck_spu0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU0_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK_SPU1_SEL/* dts */, "ck_spu1_sel",
		ck_spu1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPU1_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 18/* cksta shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD_CHK(CLK_CK_DXCC_SEL/* dts */, "ck_dxcc_sel",
		ck_dxcc_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 17/* cksta shift */),
#endif
};

static const char * const vlp_scp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d6",
	"ck_mainpll_d4",
	"ck_mainpll_d3",
	"ck_apll1_ck"
};

static const char * const vlp_scp_spi_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d5_d2"
};

static const char * const vlp_scp_iic_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d7_d2"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d5_d4",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d7"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_osc_d14",
	"ck_osc_d10"
};

static const char * const vlp_spmi_32ksel_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_clkrtc",
	"ck_osc_d20",
	"ck_osc_d14",
	"ck_osc_d10"
};

static const char * const vlp_apxgpt_26m_b_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_dpsw_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d7",
	"ck_mainpll_d7_d4"
};

static const char * const vlp_dpsw_central_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d7",
	"ck_mainpll_d7_d4"
};

static const char * const vlp_spmi_m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_osc_d14",
	"ck_osc_d10"
};

static const char * const vlp_dvfsrc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_clkrtc",
	"ck_osc_d20",
	"ck_osc_d8",
	"ck_mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d7_d4",
	"ck_osc_d4",
	"ck_mainpll_d7_d2"
};

static const char * const vlp_systimer_26m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_sspm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d5_d2",
	"ck_osc_d2",
	"ck_mainpll_d6"
};

static const char * const vlp_srck_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_camtg0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc3",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc3",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg3_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg4_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg5_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg6_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_camtg7_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_univpll_192m_d32",
	"ck_univpll_192m_d16",
	"ck_f26m_d2",
	"ck_osc_d40",
	"ck_osc_d32",
	"ck_univpll_192m_d10",
	"ck_univpll_192m_d8",
	"ck_univpll_d6_d16",
	"ck_osc_d20",
	"ck2_tvdpll1_d16",
	"ck_univpll_d6_d8"
};

static const char * const vlp_sspm_26m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_ulposc_sspm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d2",
	"ck_mainpll_d4_d2"
};

static const char * const vlp_vlp_pbus_26m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_debug_err_flag_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_dpmsrdma_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d2"
};

static const char * const vlp_vlp_pbus_156m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d2",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d7"
};

static const char * const vlp_spm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d7_d4"
};

static const char * const vlp_mminfra_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d3"
};

static const char * const vlp_usb_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d9"
};

static const char * const vlp_usb_xhci_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d9"
};

static const char * const vlp_noc_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d9"
};

static const char * const vlp_audio_h_parents[] = {
	"ck_tck_26m_mx9_ck",
	"vlp_clksq_ck",
	"ck_apll1_ck",
	"ck_apll2_ck"
};

static const char * const vlp_aud_engen1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"vlp_clksq_ck",
	"ck_apll1_d8",
	"ck_apll1_d4"
};

static const char * const vlp_aud_engen2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"vlp_clksq_ck",
	"ck_apll2_d8",
	"ck_apll2_d4"
};

static const char * const vlp_aud_intbus_parents[] = {
	"ck_tck_26m_mx9_ck",
	"vlp_clksq_ck",
	"ck_mainpll_d7_d4",
	"ck_mainpll_d4_d4"
};

static const char * const vlp_spvlp_26m_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20"
};

static const char * const vlp_spu0_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const char * const vlp_spu1_vlp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d20",
	"ck_mainpll_d4_d4",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck_mainpll_d5"
};

static const struct mtk_mux vlp_ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 28/* cksta shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPMI_M_TIA_32K_SEL/* dts */, "vlp_spmi_32ksel",
		vlp_spmi_32ksel_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_TIA_32K_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_APXGPT_26M_BCLK_SEL/* dts */, "vlp_apxgpt_26m_b_sel",
		vlp_apxgpt_26m_b_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_BCLK_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 24/* cksta shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPSW_CENTRAL_SEL/* dts */, "vlp_dpsw_central_sel",
		vlp_dpsw_central_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_CENTRAL_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 20/* cksta shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 16/* cksta shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 12/* cksta shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG4_SEL/* dts */, "vlp_camtg4_sel",
		vlp_camtg4_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG5_SEL/* dts */, "vlp_camtg5_sel",
		vlp_camtg5_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG6_SEL/* dts */, "vlp_camtg6_sel",
		vlp_camtg6_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG6_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_CAMTG7_SEL/* dts */, "vlp_camtg7_sel",
		vlp_camtg7_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG7_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_VLP_PBUS_26M_SEL/* dts */, "vlp_vlp_pbus_26m_sel",
		vlp_vlp_pbus_26m_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 4/* cksta shift */),
	/* VLP_CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DEBUG_ERR_FLAG_SEL/* dts */, "vlp_debug_err_flag_sel",
		vlp_debug_err_flag_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPMSRDMA_SEL/* dts */, "vlp_dpmsrdma_sel",
		vlp_dpmsrdma_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMSRDMA_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_VLP_PBUS_156M_SEL/* dts */, "vlp_vlp_pbus_156m_sel",
		vlp_vlp_pbus_156m_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_156M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPM_SEL/* dts */, "vlp_spm_sel",
		vlp_spm_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 0/* cksta shift */),
	/* VLP_CLK_CFG_8 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_MMINFRA_VLP_SEL/* dts */, "vlp_mminfra_vlp_sel",
		vlp_mminfra_vlp_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MMINFRA_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 12/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_USB_TOP_SEL/* dts */, "vlp_usb_sel",
		vlp_usb_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_USB_XHCI_SEL/* dts */, "vlp_usb_xhci_sel",
		vlp_usb_xhci_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_NOC_VLP_SEL/* dts */, "vlp_noc_vlp_sel",
		vlp_noc_vlp_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NOC_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 9/* cksta shift */),
	/* VLP_CLK_CFG_9 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AUDIO_H_SEL/* dts */, "vlp_audio_h_sel",
		vlp_audio_h_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_ENGEN1_SEL/* dts */, "vlp_aud_engen1_sel",
		vlp_aud_engen1_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_ENGEN2_SEL/* dts */, "vlp_aud_engen2_sel",
		vlp_aud_engen2_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_INTBUS_SEL/* dts */, "vlp_aud_intbus_sel",
		vlp_aud_intbus_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 5/* cksta shift */),
	/* VLP_CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPVLP_26M_SEL/* dts */, "vlp_spvlp_26m_sel",
		vlp_spvlp_26m_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPU0_VLP_SEL/* dts */, "vlp_spu0_vlp_sel",
		vlp_spu0_vlp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU0_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPU1_VLP_SEL/* dts */, "vlp_spu1_vlp_sel",
		vlp_spu1_vlp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU1_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 2/* cksta shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		31/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		31/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 28/* cksta shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPMI_M_TIA_32K_SEL/* dts */, "vlp_spmi_32ksel",
		vlp_spmi_32ksel_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_TIA_32K_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_APXGPT_26M_BCLK_SEL/* dts */, "vlp_apxgpt_26m_b_sel",
		vlp_apxgpt_26m_b_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_BCLK_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 24/* cksta shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPSW_CENTRAL_SEL/* dts */, "vlp_dpsw_central_sel",
		vlp_dpsw_central_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_CENTRAL_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 21/* cksta shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_PWM_VLP_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		20/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		20/* fenc shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 16/* cksta shift */),
	/* VLP_CLK_CFG_4 */
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4,
		VLP_CLK_CFG_4_SET, VLP_CLK_CFG_4_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_9_DONE,
		HWV_CG_9_SET, HWV_CG_9_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		15/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		15/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG1_SEL/* dts */, "vlp_camtg1_sel",
		vlp_camtg1_parents/* parent */, VLP_CLK_CFG_4,
		VLP_CLK_CFG_4_SET, VLP_CLK_CFG_4_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_9_DONE,
		HWV_CG_9_SET, HWV_CG_9_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		14/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		14/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG2_SEL/* dts */, "vlp_camtg2_sel",
		vlp_camtg2_parents/* parent */, VLP_CLK_CFG_4,
		VLP_CLK_CFG_4_SET, VLP_CLK_CFG_4_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_9_DONE,
		HWV_CG_9_SET, HWV_CG_9_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		13/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG3_SEL/* dts */, "vlp_camtg3_sel",
		vlp_camtg3_parents/* parent */, VLP_CLK_CFG_4,
		VLP_CLK_CFG_4_SET, VLP_CLK_CFG_4_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_9_DONE,
		HWV_CG_9_SET, HWV_CG_9_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		12/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* VLP_CLK_CFG_5 */
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG4_SEL/* dts */, "vlp_camtg4_sel",
		vlp_camtg4_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_10_DONE,
		HWV_CG_10_SET, HWV_CG_10_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		11/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG5_SEL/* dts */, "vlp_camtg5_sel",
		vlp_camtg5_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_10_DONE,
		HWV_CG_10_SET, HWV_CG_10_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		10/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG6_SEL/* dts */, "vlp_camtg6_sel",
		vlp_camtg6_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_10_DONE,
		HWV_CG_10_SET, HWV_CG_10_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		9/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_VLP_CK_CAMTG7_SEL/* dts */, "vlp_camtg7_sel",
		vlp_camtg7_parents/* parent */, VLP_CLK_CFG_5,
		VLP_CLK_CFG_5_SET, VLP_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_10_DONE,
		HWV_CG_10_SET, HWV_CG_10_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG7_SHIFT/* upd shift */, VLP_CKSTA_REG0/* cksta ofs */,
		8/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
	/* VLP_CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_VLP_PBUS_26M_SEL/* dts */, "vlp_vlp_pbus_26m_sel",
		vlp_vlp_pbus_26m_parents/* parent */, VLP_CLK_CFG_6, VLP_CLK_CFG_6_SET,
		VLP_CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 4/* cksta shift */),
	/* VLP_CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DEBUG_ERR_FLAG_SEL/* dts */, "vlp_debug_err_flag_sel",
		vlp_debug_err_flag_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DEBUG_ERR_FLAG_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_DPMSRDMA_SEL/* dts */, "vlp_dpmsrdma_sel",
		vlp_dpmsrdma_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMSRDMA_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_VLP_PBUS_156M_SEL/* dts */, "vlp_vlp_pbus_156m_sel",
		vlp_vlp_pbus_156m_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_PBUS_156M_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPM_SEL/* dts */, "vlp_spm_sel",
		vlp_spm_parents/* parent */, VLP_CLK_CFG_7, VLP_CLK_CFG_7_SET,
		VLP_CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPM_SHIFT/* upd shift */,
		VLP_CKSTA_REG0/* cksta ofs */, 0/* cksta shift */),
	/* VLP_CLK_CFG_8 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_MMINFRA_VLP_SEL/* dts */, "vlp_mminfra_vlp_sel",
		vlp_mminfra_vlp_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MMINFRA_VLP_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		12/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		31/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_USB_TOP_SEL/* dts */, "vlp_usb_sel",
		vlp_usb_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		11/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		30/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_USB_XHCI_SEL/* dts */, "vlp_usb_xhci_sel",
		vlp_usb_xhci_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		10/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		29/* fenc shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_NOC_VLP_SEL/* dts */, "vlp_noc_vlp_sel",
		vlp_noc_vlp_parents/* parent */, VLP_CLK_CFG_8, VLP_CLK_CFG_8_SET,
		VLP_CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NOC_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 9/* cksta shift */),
	/* VLP_CLK_CFG_9 */
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_AUDIO_H_SEL/* dts */, "vlp_audio_h_sel",
		vlp_audio_h_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		8/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		27/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_ENGEN1_SEL/* dts */, "vlp_aud_engen1_sel",
		vlp_aud_engen1_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		7/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		26/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_ENGEN2_SEL/* dts */, "vlp_aud_engen2_sel",
		vlp_aud_engen2_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		6/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		25/* fenc shift */),
	MUX_GATE_FENC_CLR_SET_UPD_CHK(CLK_VLP_CK_AUD_INTBUS_SEL/* dts */, "vlp_aud_intbus_sel",
		vlp_aud_intbus_parents/* parent */, VLP_CLK_CFG_9, VLP_CLK_CFG_9_SET,
		VLP_CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, VLP_CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */, VLP_CKSTA_REG1/* cksta ofs */,
		5/* cksta shift */, VLP_OCIC_FENC_STATUS_MON_1/* fenc ofs */,
		24/* fenc shift */),
	/* VLP_CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPVLP_26M_SEL/* dts */, "vlp_spvlp_26m_sel",
		vlp_spvlp_26m_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU_VLP_26M_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPU0_VLP_SEL/* dts */, "vlp_spu0_vlp_sel",
		vlp_spu0_vlp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU0_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_VLP_CK_SPU1_VLP_SEL/* dts */, "vlp_spu1_vlp_sel",
		vlp_spu1_vlp_parents/* parent */, VLP_CLK_CFG_10, VLP_CLK_CFG_10_SET,
		VLP_CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPU1_VLP_SHIFT/* upd shift */,
		VLP_CKSTA_REG1/* cksta ofs */, 2/* cksta shift */),
#endif
};

static const char * const ck2_seninf0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_seninf1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_seninf2_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_seninf3_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_seninf4_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_seninf5_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d8",
	"ck_osc_d5",
	"ck_osc_d4",
	"ck2_univpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck_osc_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_univpll2_d5"
};

static const char * const ck2_img1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck2_mmpll2_d6_d2",
	"ck_osc_d2",
	"ck2_imgpll_d5_d2",
	"ck2_mmpll2_d5_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mmpll2_d5",
	"ck2_univpll2_d4",
	"ck2_imgpll_d4"
};

static const char * const ck2_ipe_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck_osc_d2",
	"ck2_univpll2_d6",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_imgpll_d5",
	"ck_mainpll_d4",
	"ck2_mmpll2_d5",
	"ck2_imgpll_d4"
};

static const char * const ck2_cam_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d10",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck_osc_d2",
	"ck2_mmpll2_d5_d2",
	"ck2_univpll2_d4_d2",
	"ck2_univpll2_d7",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mmpll2_d5",
	"ck2_univpll2_d4",
	"ck2_imgpll_d4",
	"ck2_mmpll2_d4"
};

static const char * const ck2_camtm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_univpll2_d6_d4",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck2_univpll2_d6_d2"
};

static const char * const ck2_dpe_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_mmpll2_d5_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mmpll2_d5",
	"ck2_imgpll_d4",
	"ck2_mmpll2_d4"
};

static const char * const ck2_vdec_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck2_mainpll2_d4_d4",
	"ck2_mainpll2_d7_d2",
	"ck2_mainpll2_d6_d2",
	"ck2_mainpll2_d5_d2",
	"ck2_mainpll2_d9",
	"ck2_mainpll2_d4_d2",
	"ck2_mainpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_univpll2_d6",
	"ck2_mainpll2_d5",
	"ck2_mainpll2_d4",
	"ck2_imgpll_d2"
};

static const char * const ck2_ccusys_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck_osc_d2",
	"ck2_mmpll2_d5_d2",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d7",
	"ck2_univpll2_d6",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mainpll2_d4",
	"ck2_mainpll2_d3",
	"ck2_univpll2_d3"
};

static const char * const ck2_ccutm_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_univpll2_d6_d4",
	"ck_osc_d4",
	"ck_osc_d3",
	"ck2_univpll2_d6_d2"
};

static const char * const ck2_venc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_mainpll2_d5_d2",
	"ck2_univpll2_d5_d2",
	"ck2_mainpll2_d4_d2",
	"ck2_mmpll2_d9",
	"ck2_univpll2_d4_d2",
	"ck2_mmpll2_d4_d2",
	"ck2_mainpll2_d6",
	"ck2_univpll2_d6",
	"ck2_mainpll2_d5",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_univpll2_d3"
};

static const char * const ck2_dp1_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_tvdpll2_d16",
	"ck2_tvdpll2_d8",
	"ck2_tvdpll2_d4",
	"ck2_tvdpll2_d2"
};

static const char * const ck2_dp0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_tvdpll1_d16",
	"ck2_tvdpll1_d8",
	"ck2_tvdpll1_d4",
	"ck_tvdpll1_d2"
};

static const char * const ck2_disp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck2_mainpll2_d5",
	"ck2_mmpll2_d6",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3"
};

static const char * const ck2_disp_b0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d4_d2",
	"ck_mainpll_d6",
	"ck2_mmpll2_d4",
	"ck2_mmpll2_d6",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3"
};

static const char * const ck2_mdp_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck2_mainpll2_d5_d2",
	"ck2_mmpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck2_mainpll2_d4_d2",
	"ck2_mainpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mainpll2_d5",
	"ck2_mmpll2_d6",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3"
};

static const char * const ck2_mdp_b0_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_mainpll_d5_d2",
	"ck2_mmpll2_d4",
	"ck2_mmpll2_d6_d2",
	"ck2_mainpll2_d9",
	"ck2_mainpll2_d4_d2",
	"ck2_mainpll2_d7",
	"ck2_mainpll2_d6",
	"ck2_mainpll2_d5",
	"ck2_mmpll2_d6",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3"
};

static const char * const ck2_mminfra_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d5_d2",
	"ck_mainpll_d9",
	"ck2_mmpll2_d6_d2",
	"ck2_mainpll2_d4_d2",
	"ck_mainpll_d6",
	"ck2_univpll2_d6",
	"ck2_mainpll2_d5",
	"ck2_mmpll2_d6",
	"ck2_univpll2_d5",
	"ck2_mainpll2_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3",
	"ck2_univpll2_d3"
};

static const char * const ck2_mminfra_snoc_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d7_d2",
	"ck_mainpll_d9",
	"ck_mainpll_d7",
	"ck_mainpll_d6",
	"ck2_mmpll2_d4_d2",
	"ck_mainpll_d5",
	"ck_mainpll_d4",
	"ck2_univpll2_d4",
	"ck2_mmpll2_d4",
	"ck2_mainpll2_d3",
	"ck2_univpll2_d3",
	"ck2_mmpll2_d3",
	"ck2_mainpll2_d2"
};

static const char * const ck2_mmup_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck2_mainpll2_d6",
	"ck2_mainpll2_d5",
	"ck_osc_d2",
	"ck_osc",
	"ck_mainpll_d4",
	"ck2_univpll2_d4",
	"ck2_mainpll2_d3"
};

static const char * const ck2_mminfra_ao_parents[] = {
	"ck_tck_26m_mx9_ck",
	"ck_osc_d4",
	"ck_mainpll_d3"
};

static const struct mtk_mux ck2_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CKSYS2_CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF0_SEL/* dts */, "ck2_seninf0_sel",
		ck2_seninf0_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF1_SEL/* dts */, "ck2_seninf1_sel",
		ck2_seninf1_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF2_SEL/* dts */, "ck2_seninf2_sel",
		ck2_seninf2_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF3_SEL/* dts */, "ck2_seninf3_sel",
		ck2_seninf3_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF4_SEL/* dts */, "ck2_seninf4_sel",
		ck2_seninf4_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF4_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF5_SEL/* dts */, "ck2_seninf5_sel",
		ck2_seninf5_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF5_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_IMG1_SEL/* dts */, "ck2_img1_sel",
		ck2_img1_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_IPE_SEL/* dts */, "ck2_ipe_sel",
		ck2_ipe_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CAM_SEL/* dts */, "ck2_cam_sel",
		ck2_cam_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CAMTM_SEL/* dts */, "ck2_camtm_sel",
		ck2_camtm_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DPE_SEL/* dts */, "ck2_dpe_sel",
		ck2_dpe_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_VDEC_SEL/* dts */, "ck2_vdec_sel",
		ck2_vdec_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CCUSYS_SEL/* dts */, "ck2_ccusys_sel",
		ck2_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CCUTM_SEL/* dts */, "ck2_ccutm_sel",
		ck2_ccutm_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_VENC_SEL/* dts */, "ck2_venc_sel",
		ck2_venc_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DP1_SEL/* dts */, "ck2_dp1_sel",
		ck2_dp1_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DP0_SEL/* dts */, "ck2_dp0_sel",
		ck2_dp0_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DISP_SEL/* dts */, "ck2_disp_sel",
		ck2_disp_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 12/* cksta shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MDP_SEL/* dts */, "ck2_mdp_sel",
		ck2_mdp_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MDP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_SEL/* dts */, "ck2_mminfra_sel",
		ck2_mminfra_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_SNOC_SEL/* dts */, "ck2_mminfra_snoc_sel",
		ck2_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMUP_SEL/* dts */, "ck2_mmup_sel",
		ck2_mmup_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_AO_SEL/* dts */, "ck2_mminfra_ao_sel",
		ck2_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
#else
	/* CKSYS2_CLK_CFG_0 */
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF0_SEL/* dts */, "ck2_seninf0_sel",
		ck2_seninf0_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		31/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		31/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF1_SEL/* dts */, "ck2_seninf1_sel",
		ck2_seninf1_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		30/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		30/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF2_SEL/* dts */, "ck2_seninf2_sel",
		ck2_seninf2_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		29/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		29/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF3_SEL/* dts */, "ck2_seninf3_sel",
		ck2_seninf3_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		28/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		28/* fenc shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF4_SEL/* dts */, "ck2_seninf4_sel",
		ck2_seninf4_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF4_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		27/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF5_SEL/* dts */, "ck2_seninf5_sel",
		ck2_seninf5_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF5_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		26/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_IMG1_SEL/* dts */, "ck2_img1_sel",
		ck2_img1_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 6, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		25/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_IPE_SEL/* dts */, "ck2_ipe_sel",
		ck2_ipe_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 7, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		24/* fenc shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_CAM_SEL/* dts */, "ck2_cam_sel",
		ck2_cam_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 8, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		23/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_CAMTM_SEL/* dts */, "ck2_camtm_sel",
		ck2_camtm_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		22/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		22/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_DPE_SEL/* dts */, "ck2_dpe_sel",
		ck2_dpe_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 10, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		21/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_VDEC_SEL/* dts */, "ck2_vdec_sel",
		ck2_vdec_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 11, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		20/* fenc shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_MULT_HWV_FENC(CLK_CK2_CCUSYS_SEL/* dts */, "ck2_ccusys_sel",
		ck2_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		19/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_CCUTM_SEL/* dts */, "ck2_ccutm_sel",
		ck2_ccutm_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		18/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_VENC_SEL/* dts */, "ck2_venc_sel",
		ck2_venc_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 14, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		17/* fenc shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_MULT_HWV_FENC(CLK_CK2_DP1_SEL/* dts */, "ck2_dp1_sel",
		ck2_dp1_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		14/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_DP0_SEL/* dts */, "ck2_dp0_sel",
		ck2_dp0_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_DISP_SEL/* dts */, "ck2_disp_sel",
		ck2_disp_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 19, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MDP_SEL/* dts */, "ck2_mdp_sel",
		ck2_mdp_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 20, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MMINFRA_SEL/* dts */, "ck2_mminfra_sel",
		ck2_mminfra_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 21, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MMINFRA_SNOC_SEL/* dts */, "ck2_mminfra_snoc_sel",
		ck2_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 22, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_MMUP_SEL/* dts */, "ck2_mmup_sel",
		ck2_mmup_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_30_DONE,
		HWV_CG_30_SET, HWV_CG_30_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_MMINFRA_AO_SEL/* dts */, "ck2_mminfra_ao_sel",
		ck2_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_36_DONE,
		MM_HW_CCF_HW_CCF_36_SET, MM_HW_CCF_HW_CCF_36_CLR, /* hwv */
		16/* lsb */, 2/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		5/* fenc shift */),
#endif
};

static const struct mtk_mux ck2_muxes_b0[] = {
#if MT_CCF_MUX_DISABLE
	/* CKSYS2_CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF0_SEL/* dts */, "ck2_seninf0_sel",
		ck2_seninf0_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF1_SEL/* dts */, "ck2_seninf1_sel",
		ck2_seninf1_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF2_SEL/* dts */, "ck2_seninf2_sel",
		ck2_seninf2_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF3_SEL/* dts */, "ck2_seninf3_sel",
		ck2_seninf3_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF4_SEL/* dts */, "ck2_seninf4_sel",
		ck2_seninf4_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF4_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_SENINF5_SEL/* dts */, "ck2_seninf5_sel",
		ck2_seninf5_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF5_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_IMG1_SEL/* dts */, "ck2_img1_sel",
		ck2_img1_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_IPE_SEL/* dts */, "ck2_ipe_sel",
		ck2_ipe_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CAM_SEL/* dts */, "ck2_cam_sel",
		ck2_cam_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CAMTM_SEL/* dts */, "ck2_camtm_sel",
		ck2_camtm_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DPE_SEL/* dts */, "ck2_dpe_sel",
		ck2_dpe_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_VDEC_SEL/* dts */, "ck2_vdec_sel",
		ck2_vdec_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CCUSYS_SEL/* dts */, "ck2_ccusys_sel",
		ck2_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_CCUTM_SEL/* dts */, "ck2_ccutm_sel",
		ck2_ccutm_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_VENC_SEL/* dts */, "ck2_venc_sel",
		ck2_venc_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DP1_SEL/* dts */, "ck2_dp1_sel",
		ck2_dp1_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DP0_SEL/* dts */, "ck2_dp0_sel",
		ck2_dp0_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_DISP_SEL/* dts */, "ck2_disp_sel",
		ck2_disp_b0_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 12/* cksta shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MDP_SEL/* dts */, "ck2_mdp_sel",
		ck2_mdp_b0_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MDP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_SEL/* dts */, "ck2_mminfra_sel",
		ck2_mminfra_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_SNOC_SEL/* dts */, "ck2_mminfra_snoc_sel",
		ck2_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMUP_SEL/* dts */, "ck2_mmup_sel",
		ck2_mmup_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_CK2_MMINFRA_AO_SEL/* dts */, "ck2_mminfra_ao_sel",
		ck2_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
#else
	/* CKSYS2_CLK_CFG_0 */
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF0_SEL/* dts */, "ck2_seninf0_sel",
		ck2_seninf0_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		31/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		31/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF1_SEL/* dts */, "ck2_seninf1_sel",
		ck2_seninf1_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		30/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		30/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF2_SEL/* dts */, "ck2_seninf2_sel",
		ck2_seninf2_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		29/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		29/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF3_SEL/* dts */, "ck2_seninf3_sel",
		ck2_seninf3_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		28/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		28/* fenc shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF4_SEL/* dts */, "ck2_seninf4_sel",
		ck2_seninf4_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF4_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		27/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_SENINF5_SEL/* dts */, "ck2_seninf5_sel",
		ck2_seninf5_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF5_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		26/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_IMG1_SEL/* dts */, "ck2_img1_sel",
		ck2_img1_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 6, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		25/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_IPE_SEL/* dts */, "ck2_ipe_sel",
		ck2_ipe_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 7, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		24/* fenc shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_CAM_SEL/* dts */, "ck2_cam_sel",
		ck2_cam_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 8, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		23/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_CAMTM_SEL/* dts */, "ck2_camtm_sel",
		ck2_camtm_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		22/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		22/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_DPE_SEL/* dts */, "ck2_dpe_sel",
		ck2_dpe_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 10, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		21/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_VDEC_SEL/* dts */, "ck2_vdec_sel",
		ck2_vdec_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 11, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		20/* fenc shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_MULT_HWV_FENC(CLK_CK2_CCUSYS_SEL/* dts */, "ck2_ccusys_sel",
		ck2_ccusys_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		19/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_CCUTM_SEL/* dts */, "ck2_ccutm_sel",
		ck2_ccutm_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		18/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_VENC_SEL/* dts */, "ck2_venc_sel",
		ck2_venc_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 14, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		17/* fenc shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_MULT_HWV_FENC(CLK_CK2_DP1_SEL/* dts */, "ck2_dp1_sel",
		ck2_dp1_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		14/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_DP0_SEL/* dts */, "ck2_dp0_sel",
		ck2_dp0_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		13/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_DISP_SEL/* dts */, "ck2_disp_sel",
		ck2_disp_b0_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_34_DONE,
		MM_HW_CCF_HW_CCF_34_SET, MM_HW_CCF_HW_CCF_34_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 19, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		12/* fenc shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MDP_SEL/* dts */, "ck2_mdp_sel",
		ck2_mdp_b0_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 20, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		11/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MMINFRA_SEL/* dts */, "ck2_mminfra_sel",
		ck2_mminfra_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 21, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		10/* fenc shift */),
	MUX_MULT_DFS_HWV_FENC(CLK_CK2_MMINFRA_SNOC_SEL/* dts */, "ck2_mminfra_snoc_sel",
		ck2_mminfra_snoc_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_35_DONE,
		MM_HW_CCF_HW_CCF_35_SET, MM_HW_CCF_HW_CCF_35_CLR,
		MM_HW_CCF_HW_CCF_MUX_UPDATE_31_0, 22, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SNOC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		9/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_MMUP_SEL/* dts */, "ck2_mmup_sel",
		ck2_mmup_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_30_DONE,
		HWV_CG_30_SET, HWV_CG_30_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		8/* fenc shift */),
	MUX_MULT_HWV_FENC(CLK_CK2_MMINFRA_AO_SEL/* dts */, "ck2_mminfra_ao_sel",
		ck2_mminfra_ao_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_36_DONE,
		MM_HW_CCF_HW_CCF_36_SET, MM_HW_CCF_HW_CCF_36_CLR, /* hwv */
		16/* lsb */, 2/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_AO_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, CKSYS2_CLK_FENC_STATUS_MON_0/* fenc ofs */,
		5/* fenc shift */),
#endif
};

static const struct mtk_composite ck_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_CK_APLL_I2SIN0_MCK_SEL/* dts */, "ck_apll_i2sin0_m_sel",
		ck_apll_i2sin0_m_parents/* parent */, 0x020C/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SIN1_MCK_SEL/* dts */, "ck_apll_i2sin1_m_sel",
		ck_apll_i2sin1_m_parents/* parent */, 0x020C/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SIN2_MCK_SEL/* dts */, "ck_apll_i2sin2_m_sel",
		ck_apll_i2sin2_m_parents/* parent */, 0x020C/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SIN3_MCK_SEL/* dts */, "ck_apll_i2sin3_m_sel",
		ck_apll_i2sin3_m_parents/* parent */, 0x020C/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SIN4_MCK_SEL/* dts */, "ck_apll_i2sin4_m_sel",
		ck_apll_i2sin4_m_parents/* parent */, 0x020C/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SIN6_MCK_SEL/* dts */, "ck_apll_i2sin6_m_sel",
		ck_apll_i2sin6_m_parents/* parent */, 0x020C/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT0_MCK_SEL/* dts */, "ck_apll_i2sout0_m_sel",
		ck_apll_i2sout0_m_parents/* parent */, 0x020C/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT1_MCK_SEL/* dts */, "ck_apll_i2sout1_m_sel",
		ck_apll_i2sout1_m_parents/* parent */, 0x020C/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT2_MCK_SEL/* dts */, "ck_apll_i2sout2_m_sel",
		ck_apll_i2sout2_m_parents/* parent */, 0x020C/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT3_MCK_SEL/* dts */, "ck_apll_i2sout3_m_sel",
		ck_apll_i2sout3_m_parents/* parent */, 0x020C/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT4_MCK_SEL/* dts */, "ck_apll_i2sout4_m_sel",
		ck_apll_i2sout4_m_parents/* parent */, 0x020C/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_I2SOUT6_MCK_SEL/* dts */, "ck_apll_i2sout6_m_sel",
		ck_apll_i2sout6_m_parents/* parent */, 0x020C/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_FMI2S_MCK_SEL/* dts */, "ck_apll_fmi2s_m_sel",
		ck_apll_fmi2s_m_parents/* parent */, 0x020C/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_CK_APLL_TDMOUT_MCK_SEL/* dts */, "ck_apll_tdmout_m_sel",
		ck_apll_tdmout_m_parents/* parent */, 0x020C/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN0/* dts */, "ck_apll12_div_i2sin0"/* ccf */,
		"ck_apll_i2sin0_m_sel"/* parent */, 0x020C/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN1/* dts */, "ck_apll12_div_i2sin1"/* ccf */,
		"ck_apll_i2sin1_m_sel"/* parent */, 0x020C/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN2/* dts */, "ck_apll12_div_i2sin2"/* ccf */,
		"ck_apll_i2sin2_m_sel"/* parent */, 0x020C/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN3/* dts */, "ck_apll12_div_i2sin3"/* ccf */,
		"ck_apll_i2sin3_m_sel"/* parent */, 0x020C/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN4/* dts */, "ck_apll12_div_i2sin4"/* ccf */,
		"ck_apll_i2sin4_m_sel"/* parent */, 0x020C/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SIN6/* dts */, "ck_apll12_div_i2sin6"/* ccf */,
		"ck_apll_i2sin6_m_sel"/* parent */, 0x020C/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT0/* dts */, "ck_apll12_div_i2sout0"/* ccf */,
		"ck_apll_i2sout0_m_sel"/* parent */, 0x020C/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT1/* dts */, "ck_apll12_div_i2sout1"/* ccf */,
		"ck_apll_i2sout1_m_sel"/* parent */, 0x020C/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT2/* dts */, "ck_apll12_div_i2sout2"/* ccf */,
		"ck_apll_i2sout2_m_sel"/* parent */, 0x020C/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT3/* dts */, "ck_apll12_div_i2sout3"/* ccf */,
		"ck_apll_i2sout3_m_sel"/* parent */, 0x020C/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT4/* dts */, "ck_apll12_div_i2sout4"/* ccf */,
		"ck_apll_i2sout4_m_sel"/* parent */, 0x020C/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_I2SOUT6/* dts */, "ck_apll12_div_i2sout6"/* ccf */,
		"ck_apll_i2sout6_m_sel"/* parent */, 0x020C/* pdn ofs */,
		11/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_CK_APLL12_CK_DIV_FMI2S/* dts */, "ck_apll12_div_fmi2s"/* ccf */,
		"ck_apll_fmi2s_m_sel"/* parent */, 0x020C/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_TDMOUT_M/* dts */, "ck_apll12_div_tdmout_m"/* ccf */,
		"ck_apll_tdmout_m_sel"/* parent */, 0x020C/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_CK_APLL12_CK_DIV_TDMOUT_B/* dts */, "ck_apll12_div_tdmout_b"/* ccf */,
		"ck_apll_tdmout_m_sel"/* parent */, 0x020C/* pdn ofs */,
		14/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		16/* lsb */),
};


enum subsys_id {
	CCIPLL_PLL_CTRL = 0,
	APMIXEDSYS = 1,
	VLP_CKSYS = 2,
	MFGPLL_PLL_CTRL = 3,
	APMIXEDSYS_GP2 = 4,
	PTPPLL_PLL_CTRL = 5,
	ARMPLL_BL_PLL_CTRL = 6,
	MFGPLL_SC0_PLL_CTRL = 7,
	ARMPLL_B_PLL_CTRL = 8,
	ARMPLL_LL_PLL_CTRL = 9,
	MFGPLL_SC1_PLL_CTRL = 10,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6991_PLL_FMAX		(3800UL * MHZ)
#define MT6991_PLL_FMIN		(1500UL * MHZ)
#define MT6991_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_flags, _rst_bar_mask,				\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.flags = (_flags | PLL_CFLAGS | CLK_FENC_ENABLE),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6991_PLL_FMAX,				\
		.fmin = MT6991_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6991_INTEGER_BITS,			\
	}

#define PLL_SETCLR(_id, _name, _pll_setclr, _en_setclr_bit,		\
			_rstb_setclr_bit, _flags, _pd_reg,		\
			_pd_shift, _tuner_reg, _tuner_en_reg,		\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.pll_setclr = &(_pll_setclr),				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6991_PLL_FMAX,				\
		.fmin = MT6991_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6991_INTEGER_BITS,			\
	}

#define PLL_FENC(_id, _name, _fenc_sta_ofs, _fenc_sta_bit,		\
			_flags, _pd_reg, _pd_shift,			\
			 _pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.fenc_sta_ofs = _fenc_sta_ofs,				\
		.fenc_sta_bit = _fenc_sta_bit,				\
		.flags = (_flags | PLL_CFLAGS | CLK_FENC_ENABLE),	\
		.fmax = MT6991_PLL_FMAX,				\
		.fmin = MT6991_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6991_INTEGER_BITS,			\
	}

static const struct mtk_pll_data cci_plls[] = {
	PLL(CLK_CCIPLL, "ccipll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_FENC(CLK_APMIXED_MAINPLL, "mainpll",
		0x003C/*fenc*/, 7, PLL_AO,
		MAINPLL_CON1, 24/*pd*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_UNIVPLL, "univpll",
		0x003C/*fenc*/, 6, 0,
		UNIVPLL_CON1, 24/*pd*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_MSDCPLL, "msdcpll",
		0x003C/*fenc*/, 5, 0,
		MSDCPLL_CON1, 24/*pd*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_ADSPPLL, "adsppll",
		0x003C/*fenc*/, 4, 0,
		ADSPPLL_CON1, 24/*pd*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_EMIPLL, "emipll",
		0x003C/*fenc*/, 3, PLL_AO,
		EMIPLL_CON1, 24/*pd*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED_EMIPLL2, "emipll2",
		0x003C/*fenc*/, 2, PLL_AO,
		EMIPLL2_CON1, 24/*pd*/,
		EMIPLL2_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data vlp_ck_plls[] = {
	PLL_FENC(CLK_VLP_CK_VLP_APLL1, "vlp-apll1",
		0x0358/*fenc*/, 1, 0,
		VLP_APLL1_CON1, 24/*pd*/,
		VLP_APLL1_CON2, 0, 32/*pcw*/),
	PLL_FENC(CLK_VLP_CK_VLP_APLL2, "vlp-apll2",
		0x0358/*fenc*/, 0, 0,
		VLP_APLL2_CON1, 24/*pd*/,
		VLP_APLL2_CON2, 0, 32/*pcw*/),
};

static const struct mtk_pll_data mfg_ao_plls[] = {
	PLL(CLK_MFG_AO_MFGPLL, "mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data apmixed2_plls[] = {
	PLL_FENC(CLK_APMIXED2_MAINPLL2, "mainpll2",
		0x03C/*fenc*/, 6, 0,
		MAINPLL2_CON1, 24/*pd*/,
		MAINPLL2_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_UNIVPLL2, "univpll2",
		0x03C/*fenc*/, 5, 0,
		UNIVPLL2_CON1, 24/*pd*/,
		UNIVPLL2_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_MMPLL2, "mmpll2",
		0x03C/*fenc*/, 4, 0,
		MMPLL2_CON1, 24/*pd*/,
		MMPLL2_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_IMGPLL, "imgpll",
		0x03C/*fenc*/, 3, 0,
		IMGPLL_CON1, 24/*pd*/,
		IMGPLL_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_TVDPLL1, "tvdpll1",
		0x03C/*fenc*/, 2, 0,
		TVDPLL1_CON1, 24/*pd*/,
		TVDPLL1_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_TVDPLL2, "tvdpll2",
		0x03C/*fenc*/, 1, 0,
		TVDPLL2_CON1, 24/*pd*/,
		TVDPLL2_CON1, 0, 22/*pcw*/),
	PLL_FENC(CLK_APMIXED2_TVDPLL3, "tvdpll3",
		0x03C/*fenc*/, 0, 0,
		TVDPLL3_CON1, 24/*pd*/,
		TVDPLL3_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data ptp_plls[] = {
	PLL(CLK_PTPPLL, "ptppll", PTPPLL_CON0/*base*/,
		PTPPLL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		PTPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		PTPPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_bl_plls[] = {
	PLL(CLK_CPBL_ARMPLL_BL, "armpll-bl", ARMPLL_BL_CON0/*base*/,
		ARMPLL_BL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc0_ao_plls[] = {
	PLL(CLK_MFGSC0_AO_MFGPLL_SC0, "mfgpll-sc0", MFGPLL_SC0_CON0/*base*/,
		MFGPLL_SC0_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_SC0_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_SC0_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_b_plls[] = {
	PLL(CLK_CPB_ARMPLL_B, "armpll-b", ARMPLL_B_CON0/*base*/,
		ARMPLL_B_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_B_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_B_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_ll_plls[] = {
	PLL(CLK_CPLL_ARMPLL_LL, "armpll-ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc1_ao_plls[] = {
	PLL(CLK_MFGSC1_AO_MFGPLL_SC1, "mfgpll-sc1", MFGPLL_SC1_CON0/*base*/,
		MFGPLL_SC1_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_SC1_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_SC1_CON1, 0, 22/*pcw*/),
};

static int clk_mt6991_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
static int clk_set_trace_event(struct platform_device *pdev)
{
	trace_set_clr_event(NULL, "clk_disable", 1);
	trace_set_clr_event(NULL, "clk_disable_complete", 1);
	trace_set_clr_event(NULL, "clk_enable", 1);
	trace_set_clr_event(NULL, "clk_enable_complete", 1);
	trace_set_clr_event(NULL, "clk_prepare", 1);
	trace_set_clr_event(NULL, "clk_prepare_complete", 1);
	trace_set_clr_event(NULL, "clk_set_parent", 1);
	trace_set_clr_event(NULL, "clk_set_parent_complete", 1);
	trace_set_clr_event(NULL, "clk_set_rate", 1);
	trace_set_clr_event(NULL, "clk_set_rate_complete", 1);
	trace_set_clr_event(NULL, "clk_unprepare", 1);
	trace_set_clr_event(NULL, "clk_unprepare_complete", 1);
	trace_set_clr_event(NULL, "rpm_idle", 1);
	trace_set_clr_event(NULL, "rpm_resume", 1);
	trace_set_clr_event(NULL, "rpm_return_int", 1);
	trace_set_clr_event(NULL, "rpm_suspend", 1);
	trace_set_clr_event(NULL, "rpm_usage", 1);
	pr_notice("%s init end\n",__func__);

	return 0;
}
#endif

static int clk_mt6991_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6991_apmixed2_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(APMIXEDSYS_GP2, apmixed2_plls,
			pdev, ARRAY_SIZE(apmixed2_plls));
}

static int clk_mt6991_cpu_bl_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(ARMPLL_BL_PLL_CTRL, cpu_bl_plls,
			pdev, ARRAY_SIZE(cpu_bl_plls));
}

static int clk_mt6991_cpu_b_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(ARMPLL_B_PLL_CTRL, cpu_b_plls,
			pdev, ARRAY_SIZE(cpu_b_plls));
}

static int clk_mt6991_cpu_ll_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(ARMPLL_LL_PLL_CTRL, cpu_ll_plls,
			pdev, ARRAY_SIZE(cpu_ll_plls));
}

static int clk_mt6991_cci_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(CCIPLL_PLL_CTRL, cci_plls,
			pdev, ARRAY_SIZE(cci_plls));
}

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static u32 clk_mt6991_get_chipid(void)
{
	struct device_node *node;
	struct tag_chipid *chip_id = NULL;
	int len;
	u32 chip_sw_ver = 0;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");
	if (node) {
		chip_id = (struct tag_chipid *) of_get_property(node, "atag,chipid", &len);
		if (!chip_id) {
			pr_info("%s could not found atag,chipid in chosen\n", __func__);
			BUG_ON(1);
		}
	} else {
		pr_info("%s chosen node not found in device tree\n", __func__);
	}
	if (chip_id) {
		chip_sw_ver = chip_id->sw_ver;
		pr_info("%s current sw version:0x%x\n", __func__, chip_sw_ver);
	}

	return chip_sw_ver;
}

static int clk_mt6991_ck_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_CK_NR_CLK);

	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(ck_divs, ARRAY_SIZE(ck_divs),
			clk_data);

	if (clk_mt6991_get_chipid() == SW_VERSION_A0) {
		mtk_clk_register_muxes(ck_muxes, ARRAY_SIZE(ck_muxes), node,
				&mt6991_clk_lock, clk_data);
	} else if (clk_mt6991_get_chipid() == SW_VERSION_B0) {
		mtk_clk_register_muxes(ck_muxes_b0, ARRAY_SIZE(ck_muxes_b0), node,
				&mt6991_clk_lock, clk_data);
	} else {
		pr_err("%s(): ck_muxes register failed since undefined sw_version(%x)\n",
				__func__, clk_mt6991_get_chipid());
	}

	mtk_clk_register_composites(ck_composites, ARRAY_SIZE(ck_composites),
			base, &mt6991_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6991_ck2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_CK2_NR_CLK);

	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(ck2_divs, ARRAY_SIZE(ck2_divs),
			clk_data);

	if (clk_mt6991_get_chipid() == SW_VERSION_A0) {
		mtk_clk_register_muxes(ck2_muxes, ARRAY_SIZE(ck2_muxes), node,
				&mt6991_clk_lock, clk_data);
	} else if (clk_mt6991_get_chipid() == SW_VERSION_B0) {
		mtk_clk_register_muxes(ck2_muxes_b0, ARRAY_SIZE(ck2_muxes_b0), node,
				&mt6991_clk_lock, clk_data);
	} else {
		pr_err("%s(): ck2_muxes register failed since undefined sw_version(%x)\n",
				__func__, clk_mt6991_get_chipid());
	}


	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6991_mfg_ao_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(MFGPLL_PLL_CTRL, mfg_ao_plls,
			pdev, ARRAY_SIZE(mfg_ao_plls));
}

static int clk_mt6991_mfgsc0_ao_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(MFGPLL_SC0_PLL_CTRL, mfgsc0_ao_plls,
			pdev, ARRAY_SIZE(mfgsc0_ao_plls));
}

static int clk_mt6991_mfgsc1_ao_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(MFGPLL_SC1_PLL_CTRL, mfgsc1_ao_plls,
			pdev, ARRAY_SIZE(mfgsc1_ao_plls));
}

static int clk_mt6991_ptp_probe(struct platform_device *pdev)
{
	return clk_mt6991_pll_registration(PTPPLL_PLL_CTRL, ptp_plls,
			pdev, ARRAY_SIZE(ptp_plls));
}

static int clk_mt6991_vlp_ck_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			pr_err("%s(): ioremap failed\n", __func__);
			return PTR_ERR(base);
		}
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CK_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(vlp_ck_divs, ARRAY_SIZE(vlp_ck_divs),
			clk_data);

	mtk_clk_register_muxes(vlp_ck_muxes, ARRAY_SIZE(vlp_ck_muxes), node,
			&mt6991_clk_lock, clk_data);

	mtk_clk_register_plls(node, vlp_ck_plls, ARRAY_SIZE(vlp_ck_plls),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[VLP_CKSYS] = vlp_ck_plls;
	plls_base[VLP_CKSYS] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt6991_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6991_pll_force_off);

static const struct of_device_id of_match_clk_mt6991[] = {
	{
#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
		.compatible = "clk-ftrace",
		.data = clk_set_trace_event,
	}, {
#endif
		.compatible = "mediatek,mt6991-apmixedsys",
		.data = clk_mt6991_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6991-apmixedsys_gp2",
		.data = clk_mt6991_apmixed2_probe,
	}, {
		.compatible = "mediatek,mt6991-armpll_bl_pll_ctrl",
		.data = clk_mt6991_cpu_bl_probe,
	}, {
		.compatible = "mediatek,mt6991-armpll_b_pll_ctrl",
		.data = clk_mt6991_cpu_b_probe,
	}, {
		.compatible = "mediatek,mt6991-armpll_ll_pll_ctrl",
		.data = clk_mt6991_cpu_ll_probe,
	}, {
		.compatible = "mediatek,mt6991-ccipll_pll_ctrl",
		.data = clk_mt6991_cci_probe,
	}, {
		.compatible = "mediatek,mt6991-mfgpll_pll_ctrl",
		.data = clk_mt6991_mfg_ao_probe,
	}, {
		.compatible = "mediatek,mt6991-mfgpll_sc0_pll_ctrl",
		.data = clk_mt6991_mfgsc0_ao_probe,
	}, {
		.compatible = "mediatek,mt6991-mfgpll_sc1_pll_ctrl",
		.data = clk_mt6991_mfgsc1_ao_probe,
	}, {
		.compatible = "mediatek,mt6991-ptppll_pll_ctrl",
		.data = clk_mt6991_ptp_probe,
	}, {
		.compatible = "mediatek,mt6991-cksys",
		.data = clk_mt6991_ck_probe,
	}, {
		.compatible = "mediatek,mt6991-cksys_gp2",
		.data = clk_mt6991_ck2_probe,
	}, {
		.compatible = "mediatek,mt6991-vlp_cksys",
		.data = clk_mt6991_vlp_ck_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6991_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6991_drv = {
	.probe = clk_mt6991_probe,
	.driver = {
		.name = "clk-mt6991",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6991,
	},
};

module_platform_driver(clk_mt6991_drv);
MODULE_LICENSE("GPL");

