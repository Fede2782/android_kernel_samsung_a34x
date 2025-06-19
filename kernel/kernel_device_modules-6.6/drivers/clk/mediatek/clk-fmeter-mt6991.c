// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6991-fmeter.h"

#define FM_TIMEOUT			30
#define SUBSYS_PLL_NUM			4
#define VLP_FM_WAIT_TIME		40	/* ~= 38.64ns * 1023 */
#define FM_RST_BITS			(1 << 15)
#define FM_SCALE			(512)

/* cksys fm setting */
#define FM_PLL_TST_CK			0
#define FM_PLL_CKDIV_CK			1
#define FM_POSTDIV_SHIFT		(24)
#define FM_POSTDIV_MASK			GENMASK(26, 24)
#define FM_TST_CLK_MASK			GENMASK(31, 2)
#define FM_CKDIV_EN			(1 << 17)
#define FM_CKDIV_SHIFT			(9)
#define FM_CKDIV_MASK			GENMASK(12, 9)

/* subsys fm setting */
#define SUBSYS_CKDIV_EN			(1 << 16)
#define SUBSYS_TST_EN			(1 << 12)
#define SUBSYS_TST_CK_SEL			(1 << 4)
#define SUBSYS_CKDIV_SHIFT			(7)
#define SUBSYS_CKDIV_MASK			GENMASK(10, 7)

#define SUBSYS_NULL_OFS			0
#define PROT_IDLE_EN			(0xFFFFFFFF)

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static DEFINE_SPINLOCK(subsys_meter_lock);
#define subsys_fmeter_lock(flags)   spin_lock_irqsave(&subsys_meter_lock, flags)
#define subsys_fmeter_unlock(flags) spin_unlock_irqrestore(&subsys_meter_lock, flags)

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

/* check from topckgen&vlpcksys CODA */
#define CLK26CALI_0					(0x1C0)
#define CLK26CALI_1					(0x1C4)
#define CLK_MISC_CFG_0					(0x1D4)
#define CLK_DBG_CFG					(0x200)
#define CLK_PROT_IDLE_REG_0				(0x238)
#define CKSYS2_CLK26CALI_0				(0x0F0)
#define CKSYS2_CLK26CALI_1				(0x0F4)
#define CKSYS2_CLK_MISC_CFG_0				(0x104)
#define CKSYS2_CLK_DBG_CFG				(0x124)
#define CKSYS2_CLK_PROT_IDLE_REG_0			(0x13C)
#define VLP_FQMTR_CON0					(0x230)
#define VLP_FQMTR_CON1					(0x234)
#define VLP_CLK_PROT_IDLE_REG_0			    (0x304)

/* MFGPLL_PLL_CTRL Register */
#define MFGPLL_CON0					(0x0008)
#define MFGPLL_CON1					(0x000C)
#define MFGPLL_CON5					(0x001C)
#define MFGPLL_FQMTR_CON0				(0x0040)
#define MFGPLL_FQMTR_CON1				(0x0044)

/* MFGPLL_SC0_PLL_CTRL Register */
#define MFGPLL_SC0_CON0					(0x0008)
#define MFGPLL_SC0_CON1					(0x000C)
#define MFGPLL_SC0_CON5					(0x001C)
#define MFGPLL_SC0_FQMTR_CON0				(0x0040)
#define MFGPLL_SC0_FQMTR_CON1				(0x0044)

/* MFGPLL_SC1_PLL_CTRL Register */
#define MFGPLL_SC1_CON0					(0x0008)
#define MFGPLL_SC1_CON1					(0x000C)
#define MFGPLL_SC1_CON5					(0x001C)
#define MFGPLL_SC1_FQMTR_CON0				(0x0040)
#define MFGPLL_SC1_FQMTR_CON1				(0x0044)

/* ARMPLL_LL_PLL_CTRL Register */
#define ARMPLL_LL_CON0					(0x0008)
#define ARMPLL_LL_CON1					(0x000C)
#define ARMPLL_LL_FQMTR_CON0				(0x0040)
#define ARMPLL_LL_FQMTR_CON1				(0x0044)

/* ARMPLL_BL_PLL_CTRL Register */
#define ARMPLL_BL_CON0					(0x0008)
#define ARMPLL_BL_CON1					(0x000C)
#define ARMPLL_BL_FQMTR_CON0				(0x0040)
#define ARMPLL_BL_FQMTR_CON1				(0x0044)

/* ARMPLL_B_PLL_CTRL Register */
#define ARMPLL_B_CON0					(0x0008)
#define ARMPLL_B_CON1					(0x000C)
#define ARMPLL_B_FQMTR_CON0				(0x0040)
#define ARMPLL_B_FQMTR_CON1				(0x0044)

/* CCIPLL_PLL_CTRL Register */
#define CCIPLL_CON0					(0x0008)
#define CCIPLL_CON1					(0x000C)
#define CCIPLL_FQMTR_CON0				(0x0040)
#define CCIPLL_FQMTR_CON1				(0x0044)

/* PTPPLL_PLL_CTRL Register */
#define PTPPLL_CON0					(0x0008)
#define PTPPLL_CON1					(0x000C)
#define PTPPLL_FQMTR_CON0				(0x0040)
#define PTPPLL_FQMTR_CON1				(0x0044)


static void __iomem *fm_base[FM_SYS_NUM];

struct fmeter_data {
	enum fm_sys_id type;
	const char *name;
	unsigned int pll_con0;
	unsigned int pll_con1;
	unsigned int pll_con5;
	unsigned int con0;
	unsigned int con1;
};

static struct fmeter_data subsys_fm[] = {
	[FM_VLP_CKSYS] = {FM_VLP_CKSYS, "fm_vlp_cksys",
		SUBSYS_NULL_OFS, SUBSYS_NULL_OFS, SUBSYS_NULL_OFS, VLP_FQMTR_CON0, VLP_FQMTR_CON1},
	[FM_MFGPLL] = {FM_MFGPLL, "fm_mfgpll",
		MFGPLL_CON0, MFGPLL_CON1, MFGPLL_CON5, MFGPLL_FQMTR_CON0, MFGPLL_FQMTR_CON1},
	[FM_MFGPLL_SC0] = {FM_MFGPLL_SC0, "fm_mfgpll_sc0",
		MFGPLL_SC0_CON0, MFGPLL_SC0_CON1, MFGPLL_SC0_CON5, MFGPLL_SC0_FQMTR_CON0, MFGPLL_SC0_FQMTR_CON1},
	[FM_MFGPLL_SC1] = {FM_MFGPLL_SC1, "fm_mfgpll_sc1",
		MFGPLL_SC1_CON0, MFGPLL_SC1_CON1, MFGPLL_SC1_CON5, MFGPLL_SC1_FQMTR_CON0, MFGPLL_SC1_FQMTR_CON1},
	[FM_ARMPLL_LL] = {FM_ARMPLL_LL, "fm_armpll_ll",
		ARMPLL_LL_CON0, ARMPLL_LL_CON1, SUBSYS_NULL_OFS, ARMPLL_LL_FQMTR_CON0, ARMPLL_LL_FQMTR_CON1},
	[FM_ARMPLL_BL] = {FM_ARMPLL_BL, "fm_armpll_bl",
		ARMPLL_BL_CON0, ARMPLL_BL_CON1, SUBSYS_NULL_OFS, ARMPLL_BL_FQMTR_CON0, ARMPLL_BL_FQMTR_CON1},
	[FM_ARMPLL_B] = {FM_ARMPLL_B, "fm_armpll_b",
		ARMPLL_B_CON0, ARMPLL_B_CON1, SUBSYS_NULL_OFS, ARMPLL_B_FQMTR_CON0, ARMPLL_B_FQMTR_CON1},
	[FM_CCIPLL] = {FM_CCIPLL, "fm_ccipll",
		CCIPLL_CON0, CCIPLL_CON1, SUBSYS_NULL_OFS, CCIPLL_FQMTR_CON0, CCIPLL_FQMTR_CON1},
	[FM_PTPPLL] = {FM_PTPPLL, "fm_ptppll",
		PTPPLL_CON0, PTPPLL_CON1, SUBSYS_NULL_OFS, PTPPLL_FQMTR_CON0, PTPPLL_FQMTR_CON1},
};

const char *comp_list[] = {
	[FM_CKSYS] = "mediatek,mt6991-cksys",
	[FM_CKSYS_GP2] = "mediatek,mt6991-cksys_gp2",
	[FM_APMIXEDSYS] = "mediatek,mt6991-apmixedsys",
	[FM_APMIXEDSYS_GP2] = "mediatek,mt6991-apmixedsys_gp2",
	[FM_VLP_CKSYS] = "mediatek,mt6991-vlp_cksys",
	[FM_MFGPLL] = "mediatek,mt6991-mfgpll_pll_ctrl",
	[FM_MFGPLL_SC0] = "mediatek,mt6991-mfgpll_sc0_pll_ctrl",
	[FM_MFGPLL_SC1] = "mediatek,mt6991-mfgpll_sc1_pll_ctrl",
	[FM_ARMPLL_LL] = "mediatek,mt6991-armpll_ll_pll_ctrl",
	[FM_ARMPLL_BL] = "mediatek,mt6991-armpll_bl_pll_ctrl",
	[FM_ARMPLL_B] = "mediatek,mt6991-armpll_b_pll_ctrl",
	[FM_CCIPLL] = "mediatek,mt6991-ccipll_pll_ctrl",
	[FM_PTPPLL] = "mediatek,mt6991-ptppll_pll_ctrl",
};

/*
 * clk fmeter
 */

#define FPLL(_type, _id, _con0_ofs, _grp, _ckdiv_en) {.type = _type, \
		.id = _id, .con0_ofs = _con0_ofs, .grp = _grp, .ckdiv_en = _ckdiv_en}

static struct fmeter_pll fplls[] = {
	FPLL(ABIST, FM_MDPLL_FS26M_CK, 0, 0, 0),
	FPLL(ABIST, FM_RTC32K_I, 0, 0, 0),
	FPLL(ABIST, FM_MAINPLL_CKDIV_CK, 0x024C, 3, 0),
	FPLL(ABIST, FM_UNIVPLL_CKDIV_CK, 0x0260, 3, 0),
	FPLL(ABIST, FM_ADSPPLL_CKDIV_CK, 0x0288, 3, 0),
	FPLL(ABIST, FM_EMIPLL2_CKDIV_CK, 0x02B0, 3, 0),
	FPLL(ABIST, FM_EMIPLL_CKDIV_CK, 0x029C, 3, 0),
	FPLL(ABIST, FM_MSDCPLL_CKDIV_CK, 0x0278, 3, 0),
	FPLL(ABIST_CK2, FM_MAINPLL2_CKDIV_CK, 0x024C, 3, 0),
	FPLL(ABIST_CK2, FM_UNIV2_192M_CK, 0x0264, 3, 0),
	FPLL(ABIST_CK2, FM_MMPLL2_CKDIV_CK, 0x0278, 3, 0),
	FPLL(ABIST_CK2, FM_IMGPLL_CKDIV_CK, 0x028C, 3, 0),
	FPLL(ABIST_CK2, FM_TVDPLL1_CKDIV_CK, 0x029C, 3, 0),
	FPLL(ABIST_CK2, FM_TVDPLL2_CKDIV_CK, 0x02B4, 3, 0),
	FPLL(ABIST_CK2, FM_TVDPLL3_CKDIV_CK, 0x02C4, 3, 0),
	{},
};

#define FMCLK3(_t, _i, _n, _o, _g, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .grp = _g, .ck_div = _c}
#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_sel", 0x0270, 31, 1),
	FMCLK2(CKGEN, FM_MEM_SUB_CK, "fm_mem_sub_sel", 0x0270, 30, 1),
	FMCLK2(CKGEN, FM_IO_NOC_CK, "fm_io_noc_sel", 0x0270, 29, 1),
	FMCLK2(CKGEN, FM_P_AXI_CK, "fm_peri_axi_sel", 0x0270, 28, 1),
	FMCLK2(CKGEN, FM_PEXTP0_AXI_CK, "fm_ufs_pextp0_axi_sel", 0x0270, 27, 1),
	FMCLK2(CKGEN, FM_PEXTP1_USB_AXI_CK, "fm_pextp1_usb_axi_sel", 0x0270, 26, 1),
	FMCLK2(CKGEN, FM_P_FMEM_SUB_CK, "fm_peri_fmem_sub_sel", 0x0270, 25, 1),
	FMCLK2(CKGEN, FM_PEXPT0_MEM_SUB_CK, "fm_ufs_pexpt0_mem_sub_sel", 0x0270, 24, 1),
	FMCLK2(CKGEN, FM_PEXTP1_USB_MEM_SUB_CK, "fm_pextp1_usb_mem_sub_sel", 0x0270, 23, 1),
	FMCLK2(CKGEN, FM_P_NOC_CK, "fm_peri_noc_sel", 0x0270, 22, 1),
	FMCLK2(CKGEN, FM_EMI_N_CK, "fm_emi_n_sel", 0x0270, 21, 1),
	FMCLK2(CKGEN, FM_EMI_S_CK, "fm_emi_s_sel", 0x0270, 20, 1),
	FMCLK2(CKGEN, FM_EMI_SLICE_N_CK, "fm_emi_slice_n_sel", 0x0270, 19, 1),
	FMCLK2(CKGEN, FM_EMI_SLICE_S_CK, "fm_emi_slice_s_sel", 0x0270, 18, 1),
	FMCLK2(CKGEN, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_sel", 0x0270, 17, 1),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_sel", 0x0270, 16, 1),
	FMCLK2(CKGEN, FM_CIRQ_CK, "fm_cirq_sel", 0x0270, 15, 1),
	FMCLK2(CKGEN, FM_PBUS_156M_CK, "fm_pbus_156m_sel", 0x0270, 14, 1),
	FMCLK2(CKGEN, FM_NOC_LOW_CK, "fm_noc_low_sel", 0x0270, 13, 1),
	FMCLK2(CKGEN, FM_NOC_MID_CK, "fm_noc_mid_sel", 0x0270, 12, 1),
	FMCLK2(CKGEN, FM_EFUSE_CK, "fm_efuse_sel", 0x0270, 11, 1),
	FMCLK2(CKGEN, FM_MCL3GIC_CK, "fm_mcu_l3gic_sel", 0x0270, 10, 1),
	FMCLK2(CKGEN, FM_MCINFRA_CK, "fm_mcu_infra_sel", 0x0270, 9, 1),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_sel", 0x0270, 8, 1),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_sel", 0x0270, 7, 1),
	FMCLK2(CKGEN, FM_MFGSC_REF_CK, "fm_mfgsc_ref_sel", 0x0270, 6, 1),
	FMCLK2(CKGEN, FM_MFG_EB_CK, "fm_mfg_eb_sel", 0x0270, 5, 1),
	FMCLK2(CKGEN, FM_UART_CK, "fm_uart_sel", 0x0270, 4, 1),
	FMCLK2(CKGEN, FM_SPI0_B_CK, "fm_spi0_bclk_sel", 0x0270, 3, 1),
	FMCLK2(CKGEN, FM_SPI1_B_CK, "fm_spi1_bclk_sel", 0x0270, 2, 1),
	FMCLK2(CKGEN, FM_SPI2_B_CK, "fm_spi2_bclk_sel", 0x0270, 1, 1),
	FMCLK2(CKGEN, FM_SPI3_B_CK, "fm_spi3_bclk_sel", 0x0270, 0, 1),
	FMCLK2(CKGEN, FM_SPI4_B_CK, "fm_spi4_bclk_sel", 0x0274, 31, 1),
	FMCLK2(CKGEN, FM_SPI5_B_CK, "fm_spi5_bclk_sel", 0x0274, 30, 1),
	FMCLK2(CKGEN, FM_SPI6_B_CK, "fm_spi6_bclk_sel", 0x0274, 29, 1),
	FMCLK2(CKGEN, FM_SPI7_B_CK, "fm_spi7_bclk_sel", 0x0274, 28, 1),
	FMCLK2(CKGEN, FM_MSDC_MACRO_1P_CK, "fm_msdc_macro_1p_sel", 0x0274, 27, 1),
	FMCLK2(CKGEN, FM_MSDC_MACRO_2P_CK, "fm_msdc_macro_2p_sel", 0x0274, 26, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_sel", 0x0274, 25, 1),
	FMCLK2(CKGEN, FM_MSDC30_2_CK, "fm_msdc30_2_sel", 0x0274, 24, 1),
	FMCLK2(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_sel", 0x0274, 23, 1),
	FMCLK2(CKGEN, FM_USB_1P_CK, "fm_usb_top_1p_sel", 0x0274, 22, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_1P_CK, "fm_ssusb_xhci_1p_sel", 0x0274, 21, 1),
	FMCLK2(CKGEN, FM_USB_FMCNT_P1_CK, "fm_ssusb_fmcnt_p1_sel", 0x0274, 20, 1),
	FMCLK2(CKGEN, FM_I2C_P_CK, "fm_i2c_peri_sel", 0x0274, 19, 1),
	FMCLK2(CKGEN, FM_I2C_EAST_CK, "fm_i2c_east_sel", 0x0274, 18, 1),
	FMCLK2(CKGEN, FM_I2C_WEST_CK, "fm_i2c_west_sel", 0x0274, 17, 1),
	FMCLK2(CKGEN, FM_I2C_NORTH_CK, "fm_i2c_north_sel", 0x0274, 16, 1),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_sel", 0x0274, 15, 1),
	FMCLK2(CKGEN, FM_CK, "fm_ufs_sel", 0x0274, 14, 1),
	FMCLK2(CKGEN, FM_MBIST_CK, "fm_ufs_mbist_sel", 0x0274, 13, 1),
	FMCLK2(CKGEN, FM_PEXTP_MBIST_CK, "fm_pextp_mbist_sel", 0x0274, 12, 1),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_sel", 0x0274, 11, 1),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_sel", 0x0274, 10, 1),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_sel", 0x0274, 9, 1),
	FMCLK2(CKGEN, FM_ADSP_UARTHUB_B_CK, "fm_adsp_uarthub_bclk_sel", 0x0274, 8, 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_sel", 0x0274, 7, 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_sel", 0x0274, 6, 1),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_sel", 0x0274, 5, 1),
	FMCLK2(CKGEN, FM_SFLASH_CK, "fm_sflash_sel", 0x0274, 4, 1),
	FMCLK2(CKGEN, FM_IPSEAST_CK, "fm_ipseast_sel", 0x0274, 3, 1),
	FMCLK2(CKGEN, FM_IPSWEST_CK, "fm_ipswest_sel", 0x0274, 2, 1),
	FMCLK2(CKGEN, FM_TL_CK, "fm_tl_sel", 0x0274, 1, 1),
	FMCLK2(CKGEN, FM_TL_P1_CK, "fm_tl_p1_sel", 0x0274, 0, 1),
	FMCLK2(CKGEN, FM_TL_P2_CK, "fm_tl_p2_sel", 0x0278, 31, 1),
	FMCLK2(CKGEN, FM_EMI_INTERFACE_546_CK, "fm_emi_interface_546_sel", 0x0278, 30, 1),
	FMCLK2(CKGEN, FM_SDF_CK, "fm_sdf_sel", 0x0278, 29, 1),
	FMCLK2(CKGEN, FM_UARTHUB_B_CK, "fm_uarthub_bclk_sel", 0x0278, 28, 1),
	FMCLK2(CKGEN, FM_DPSW_CMP_26M_CK, "fm_dpsw_cmp_26m_sel", 0x0278, 27, 1),
	FMCLK2(CKGEN, FM_SMAPCK_CK, "fm_smapck_sel", 0x0278, 26, 1),
	FMCLK2(CKGEN, FM_SSR_PKA_CK, "fm_ssr_pka_sel", 0x0278, 25, 1),
	FMCLK2(CKGEN, FM_SSR_DMA_CK, "fm_ssr_dma_sel", 0x0278, 24, 1),
	FMCLK2(CKGEN, FM_SSR_KDF_CK, "fm_ssr_kdf_sel", 0x0278, 23, 1),
	FMCLK2(CKGEN, FM_SSR_RNG_CK, "fm_ssr_rng_sel", 0x0278, 22, 1),
	FMCLK2(CKGEN, FM_SPU0_CK, "fm_spu0_sel", 0x0278, 21, 1),
	FMCLK2(CKGEN, FM_SPU1_CK, "fm_spu1_sel", 0x0278, 20, 1),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_sel", 0x0278, 19, 1),
	FMCLK2(CKGEN, FM_SPU0_BOOT_CK, "fm_spu0_boot_sel", 0x0278, 18, 1),
	FMCLK2(CKGEN, FM_SPU1_BOOT_CK, "fm_spu1_boot_sel", 0x0278, 17, 1),
	FMCLK2(CKGEN, FM_SGMII0_REF_325M_CK, "fm_sgmii0_ref_325m_sel", 0x0278, 16, 1),
	FMCLK2(CKGEN, FM_SGMII0_CK, "fm_sgmii0_reg_sel", 0x0278, 15, 1),
	FMCLK2(CKGEN, FM_SGMII1_REF_325M_CK, "fm_sgmii1_ref_325m_sel", 0x0278, 14, 1),
	FMCLK2(CKGEN, FM_SGMII1_CK, "fm_sgmii1_reg_sel", 0x0278, 13, 1),
	FMCLK2(CKGEN, FM_GMAC_312P5M_CK, "fm_gmac_312p5m_sel", 0x0278, 12, 1),
	FMCLK2(CKGEN, FM_GMAC_125M_CK, "fm_gmac_125m_sel", 0x0278, 11, 1),
	FMCLK2(CKGEN, FM_GMAC_RMII_CK, "fm_gmac_rmii_sel", 0x0278, 10, 1),
	FMCLK2(CKGEN, FM_GMAC_62P4M_PTP_CK, "fm_gmac_62p4m_ptp_sel", 0x0278, 9, 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_MDPLL_FS26M_CK, "fm_mdpll_fs26m_ck", 1),
	FMCLK(ABIST, FM_RTC32K_I, "fm_rtc32k_i", 1),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0254, 3, 13),
	FMCLK3(ABIST, FM_UNIVPLL_CKDIV_CK, "fm_univpll_ckdiv_ck", 0x0268, 3, 13),
	FMCLK3(ABIST, FM_ADSPPLL_CKDIV_CK, "fm_adsppll_ckdiv_ck", 0x0290, 3, 13),
	FMCLK3(ABIST, FM_EMIPLL2_CKDIV_CK, "fm_emipll2_ckdiv_ck", 0x02B8, 3, 13),
	FMCLK3(ABIST, FM_EMIPLL_CKDIV_CK, "fm_emipll_ckdiv_ck", 0x02A4, 3, 13),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x027C, 3, 13),
	/* CKGEN_CK2 Part */
	FMCLK2(CKGEN_CK2, FM_SENINF0_CK, "fm_seninf0_sel", 0x0174, 31, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF1_CK, "fm_seninf1_sel", 0x0174, 30, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF2_CK, "fm_seninf2_sel", 0x0174, 29, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF3_CK, "fm_seninf3_sel", 0x0174, 28, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF4_CK, "fm_seninf4_sel", 0x0174, 27, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF5_CK, "fm_seninf5_sel", 0x0174, 26, 1),
	FMCLK2(CKGEN_CK2, FM_IMG1_CK, "fm_img1_sel", 0x0174, 25, 1),
	FMCLK2(CKGEN_CK2, FM_IPE_CK, "fm_ipe_sel", 0x0174, 24, 1),
	FMCLK2(CKGEN_CK2, FM_CAM_CK, "fm_cam_sel", 0x0174, 23, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTM_CK, "fm_camtm_sel", 0x0174, 22, 1),
	FMCLK2(CKGEN_CK2, FM_DPE_CK, "fm_dpe_sel", 0x0174, 21, 1),
	FMCLK2(CKGEN_CK2, FM_VDEC_CK, "fm_vdec_sel", 0x0174, 20, 1),
	FMCLK2(CKGEN_CK2, FM_CCUSYS_CK, "fm_ccusys_sel", 0x0174, 19, 1),
	FMCLK2(CKGEN_CK2, FM_CCUTM_CK, "fm_ccutm_sel", 0x0174, 18, 1),
	FMCLK2(CKGEN_CK2, FM_VENC_CK, "fm_venc_sel", 0x0174, 17, 1),
	FMCLK2(CKGEN_CK2, FM_DVO_CK, "fm_dvo_sel", 0x0174, 16, 1),
	FMCLK2(CKGEN_CK2, FM_DVO_FAVT_CK, "fm_dvo_favt_sel", 0x0174, 15, 1),
	FMCLK2(CKGEN_CK2, FM_DP1_CK, "fm_dp1_sel", 0x0174, 14, 1),
	FMCLK2(CKGEN_CK2, FM_DP0_CK, "fm_dp0_sel", 0x0174, 13, 1),
	FMCLK2(CKGEN_CK2, FM_DISP_CK, "fm_disp_sel", 0x0174, 12, 1),
	FMCLK2(CKGEN_CK2, FM_MDP_CK, "fm_mdp_sel", 0x0174, 11, 1),
	FMCLK2(CKGEN_CK2, FM_MMINFRA_CK, "fm_mminfra_sel", 0x0174, 10, 1),
	FMCLK2(CKGEN_CK2, FM_MMINFRA_SNOC_CK, "fm_mminfra_snoc_sel", 0x0174, 9, 1),
	FMCLK2(CKGEN_CK2, FM_MMUP_CK, "fm_mmup_sel", 0x0174, 8, 1),
	FMCLK2(CKGEN_CK2, FM_DUMMY1_CK, "fm_dummy1_sel", 0x0174, 7, 1),
	FMCLK2(CKGEN_CK2, FM_DUMMY2_CK, "fm_dummy2_sel", 0x0174, 6, 1),
	FMCLK2(CKGEN_CK2, FM_MMINFRA_AO_CK, "fm_mminfra_ao_sel", 0x0174, 5, 1),
	/* ABIST_CK2 Part */
	FMCLK3(ABIST_CK2, FM_MAINPLL2_CKDIV_CK, "fm_mainpll2_ckdiv_ck", 0x0254, 3, 13),
	FMCLK3(ABIST_CK2, FM_UNIV2_192M_CK, "fm_univ2_192m_ck", 0x0268, 3, 13),
	FMCLK3(ABIST_CK2, FM_MMPLL2_CKDIV_CK, "fm_mmpll2_ckdiv_ck", 0x027C, 3, 13),
	FMCLK3(ABIST_CK2, FM_IMGPLL_CKDIV_CK, "fm_imgpll_ckdiv_ck", 0x0290, 3, 13),
	FMCLK3(ABIST_CK2, FM_TVDPLL1_CKDIV_CK, "fm_tvdpll1_ckdiv_ck", 0x02A4, 3, 13),
	FMCLK3(ABIST_CK2, FM_TVDPLL2_CKDIV_CK, "fm_tvdpll2_ckdiv_ck", 0x02B8, 3, 13),
	FMCLK3(ABIST_CK2, FM_TVDPLL3_CKDIV_CK, "fm_tvdpll3_ckdiv_ck", 0x02CC, 3, 13),
	/* VLPCK Part */
	FMCLK2(VLPCK, FM_SCP_CK, "fm_vlp_scp_sel", 0x039C, 31, 1),
	FMCLK2(VLPCK, FM_SCP_SPI_CK, "fm_vlp_scp_spi_sel", 0x039C, 30, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_CK, "fm_vlp_scp_iic_sel", 0x039C, 29, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_HS_CK, "fm_vlp_scp_iic_high_spd_sel", 0x039C, 28, 1),
	FMCLK2(VLPCK, FM_PWRAP_ULPOSC_CK, "fm_vlp_pwrap_ulposc_sel", 0x039C, 27, 1),
	FMCLK2(VLPCK, FM_SPMI_32KCK, "fm_vlp_spmi_m_tia_32k_sel", 0x039C, 26, 1),
	FMCLK2(VLPCK, FM_APXGPT_26M_B_CK, "fm_vlp_apxgpt_26m_bclk_sel", 0x039C, 25, 1),
	FMCLK2(VLPCK, FM_DPSW_CK, "fm_vlp_dpsw_sel", 0x039C, 24, 1),
	FMCLK2(VLPCK, FM_DPSW_CENTRAL_CK, "fm_vlp_dpsw_central_sel", 0x039C, 23, 1),
	FMCLK2(VLPCK, FM_SPMI_M_CK, "fm_vlp_spmi_m_mst_sel", 0x039C, 22, 1),
	FMCLK2(VLPCK, FM_DVFSRC_CK, "fm_vlp_dvfsrc_sel", 0x039C, 21, 1),
	FMCLK2(VLPCK, FM_PWM_VLP_CK, "fm_vlp_pwm_vlp_sel", 0x039C, 20, 1),
	FMCLK2(VLPCK, FM_AXI_VLP_CK, "fm_vlp_axi_vlp_sel", 0x039C, 19, 1),
	FMCLK2(VLPCK, FM_SYSTIMER_26M_CK, "fm_vlp_systimer_26m_sel", 0x039C, 18, 1),
	FMCLK2(VLPCK, FM_SSPM_CK, "fm_vlp_sspm_sel", 0x039C, 17, 1),
	FMCLK2(VLPCK, FM_SRCK_CK, "fm_vlp_srck_sel", 0x039C, 16, 1),
	FMCLK2(VLPCK, FM_CAMTG0_CK, "fm_vlp_camtg0_sel", 0x039C, 15, 1),
	FMCLK2(VLPCK, FM_CAMTG1_CK, "fm_vlp_camtg1_sel", 0x039C, 14, 1),
	FMCLK2(VLPCK, FM_CAMTG2_CK, "fm_vlp_camtg2_sel", 0x039C, 13, 1),
	FMCLK2(VLPCK, FM_CAMTG3_CK, "fm_vlp_camtg3_sel", 0x039C, 12, 1),
	FMCLK2(VLPCK, FM_CAMTG4_CK, "fm_vlp_camtg4_sel", 0x039C, 11, 1),
	FMCLK2(VLPCK, FM_CAMTG5_CK, "fm_vlp_camtg5_sel", 0x039C, 10, 1),
	FMCLK2(VLPCK, FM_CAMTG6_CK, "fm_vlp_camtg6_sel", 0x039C, 9, 1),
	FMCLK2(VLPCK, FM_CAMTG7_CK, "fm_vlp_camtg7_sel", 0x039C, 8, 1),
	FMCLK2(VLPCK, FM_IPS_CK, "fm_vlp_ips_sel", 0x039C, 7, 1),
	FMCLK2(VLPCK, FM_SSPM_26M_CK, "fm_vlp_sspm_26m_sel", 0x039C, 6, 1),
	FMCLK2(VLPCK, FM_ULPOSC_SSPM_CK, "fm_vlp_ulposc_sspm_sel", 0x039C, 5, 1),
	FMCLK2(VLPCK, FM_VLP_PBUS_26M_CK, "fm_vlp_vlp_pbus_26m_sel", 0x039C, 4, 1),
	FMCLK2(VLPCK, FM_DEBUG_ERR_FLAG_VLP_26M_CK, "fm_vlp_debug_err_flag_vlp_26m_sel", 0x039C, 3, 1),
	FMCLK2(VLPCK, FM_DPMSRDMA_CK, "fm_vlp_dpmsrdma_sel", 0x039C, 2, 1),
	FMCLK2(VLPCK, FM_VLP_PBUS_156M_CK, "fm_vlp_vlp_pbus_156m_sel", 0x039C, 1, 1),
	FMCLK2(VLPCK, FM_SPM_CK, "fm_vlp_spm_sel", 0x039C, 0, 1),
	FMCLK2(VLPCK, FM_MMINFRA_VLP_CK, "fm_vlp_mminfra_vlp_sel", 0x03A0, 31, 1),
	FMCLK2(VLPCK, FM_USB_CK, "fm_vlp_usb_top_sel", 0x03A0, 30, 1),
	FMCLK2(VLPCK, FM_USB_XHCI_CK, "fm_vlp_ssusb_xhci_sel", 0x03A0, 29, 1),
	FMCLK2(VLPCK, FM_NOC_VLP_CK, "fm_vlp_noc_vlp_sel", 0x03A0, 28, 1),
	FMCLK2(VLPCK, FM_AUDIO_H_CK, "fm_vlp_audio_h_sel", 0x03A0, 27, 1),
	FMCLK2(VLPCK, FM_AUD_ENGEN1_CK, "fm_vlp_aud_engen1_sel", 0x03A0, 26, 1),
	FMCLK2(VLPCK, FM_AUD_ENGEN2_CK, "fm_vlp_aud_engen2_sel", 0x03A0, 25, 1),
	FMCLK2(VLPCK, FM_AUD_INTBUS_CK, "fm_vlp_aud_intbus_sel", 0x03A0, 24, 1),
	FMCLK2(VLPCK, FM_SPVLP_26M_CK, "fm_vlp_spu_vlp_26m_sel", 0x03A0, 23, 1),
	FMCLK2(VLPCK, FM_SPU0_VLP_CK, "fm_vlp_spu0_vlp_sel", 0x03A0, 22, 1),
	FMCLK2(VLPCK, FM_SPU1_VLP_CK, "fm_vlp_spu1_vlp_sel", 0x03A0, 21, 1),
	FMCLK2(VLPCK, FM_VLP_DUMMY1_CK, "fm_vlp_vlp_dummy1_sel", 0x03A0, 20, 1),
	FMCLK2(VLPCK, FM_VLP_DUMMY2_CK, "fm_vlp_vlp_dummy2_sel", 0x03A0, 19, 1),
	FMCLK(VLPCK, FM_OSC2_SYNC_CK, "fm_osc2_sync_ck", 1),
	FMCLK(VLPCK, FM_OSC3_SYNC_CK, "fm_osc3_sync_ck", 1),
	FMCLK(VLPCK, FM_ABIST_FQMTR_BUS_1, "fm_abist_fqmtr_bus_1", 1),
	FMCLK(VLPCK, FM_VLP_APLL1_ORI, "fm_vlp_apll1_ori", 1),
	FMCLK(VLPCK, FM_VLP_APLL2_ORI, "fm_vlp_apll2_ori", 1),
	FMCLK(VLPCK, FM_VLP_APLL1_CK, "fm_vlp_apll1_ck", 1),
	FMCLK(VLPCK, FM_VLP_APLL2_CK, "fm_vlp_apll2_ck", 1),
	FMCLK3(VLPCK, FM_VLP_APLL1_CKDIV_CK, "fm_vlp_apll1_ckdiv_ck", 0x0278, 3, 13),
	FMCLK3(VLPCK, FM_VLP_APLL2_CKDIV_CK, "fm_vlp_apll2_ckdiv_ck", 0x0290, 3, 13),
	FMCLK(VLPCK, FM_ABIST_FQMTR_0, "fm_abist_fqmtr_0", 1),
	FMCLK(VLPCK, FM_ULPOSC2_CK, "fm_ulposc2_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC3_CK, "fm_ulposc3_ck", 1),
	/* SUBSYS Part */
	FMCLK(SUBSYS, FM_MFGPLL, "fm_mfgpll", 1),
	FMCLK(SUBSYS, FM_MFGPLL_SC0, "fm_mfgpll_sc0", 1),
	FMCLK(SUBSYS, FM_MFGPLL_SC1, "fm_mfgpll_sc1", 1),
	FMCLK(SUBSYS, FM_ARMPLL_LL, "fm_armpll_ll", 1),
	FMCLK(SUBSYS, FM_ARMPLL_BL, "fm_armpll_bl", 1),
	FMCLK(SUBSYS, FM_ARMPLL_B, "fm_armpll_b", 1),
	FMCLK(SUBSYS, FM_CCIPLL, "fm_ccipll", 1),
	FMCLK(SUBSYS, FM_PTPPLL, "fm_ptppll", 1),
	{},
};

const struct fmeter_clk *mt6991_get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_pdn(void __iomem *base,
		unsigned int type, unsigned int ID)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID)
			break;
	}

	if (i >= ARRAY_SIZE(fclks) - 1)
		return 1;

	if (!fclks[i].ofs)
		return 0;

	if (type == SUBSYS) {
		if ((clk_readl(base + fclks[i].ofs) & fclks[i].pdn)
				!= fclks[i].pdn) {
			return 1;
		}
	} else if ((clk_readl(base + fclks[i].ofs)
			& BIT(fclks[i].pdn)) != BIT(fclks[i].pdn))
		return 1;

	return 0;
}

static unsigned int get_post_div(unsigned int type, unsigned int ID)
{
	unsigned int post_div = 1;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return post_div;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return post_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			if (type == ABIST)
				post_div =  clk_readl(fm_base[FM_APMIXEDSYS] + fclks[i].ofs);
			else if (type == ABIST_CK2)
				post_div =  clk_readl(fm_base[FM_APMIXEDSYS_GP2] + fclks[i].ofs);
			post_div = 1 << ((post_div & FM_POSTDIV_MASK) >> FM_POSTDIV_SHIFT);
			break;
		}
	}

	if (i == (ARRAY_SIZE(fclks) - 1))
		return post_div;

	return post_div;
}

static unsigned int get_clk_div(unsigned int type, unsigned int ID)
{
	unsigned int clk_div = 1;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return clk_div;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return clk_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			if (type == ABIST)
				clk_div =  clk_readl(fm_base[FM_APMIXEDSYS] + fclks[i].ofs - 0x4);
			else if (type == ABIST_CK2)
				clk_div =  clk_readl(fm_base[FM_APMIXEDSYS_GP2] + fclks[i].ofs - 0x4);
			clk_div = (clk_div & FM_CKDIV_MASK) >> FM_CKDIV_SHIFT;
			break;
		}
	}

	if (i == (ARRAY_SIZE(fclks) - 1))
		return clk_div;

	return clk_div;
}

static void set_clk_div_en(unsigned int type, unsigned int ID, bool onoff)
{
	void __iomem *pll_con0 = NULL;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return;

	for (i = 0; i < ARRAY_SIZE(fplls) - 1; i++) {
		if (fplls[i].type == type && fplls[i].id == ID
				&& fplls[i].grp != 0) {
			if (type == ABIST)
				pll_con0 =  fm_base[FM_APMIXEDSYS] + fplls[i].con0_ofs;
			else if(type == ABIST_CK2)
				pll_con0 =  fm_base[FM_APMIXEDSYS_GP2] + fplls[i].con0_ofs;
			break;
		}
	}

	if ((i == (ARRAY_SIZE(fplls) - 1)) || pll_con0 == NULL)
		return;

	if (onoff) {
		// check ckdiv_en
		if (clk_readl(pll_con0) & FM_CKDIV_EN)
			fplls[i].ckdiv_en = 1;
		// pll con0[17] = 1
		// select pll_ckdiv, enable pll_ckdiv, enable test clk
		clk_writel(pll_con0, (clk_readl(pll_con0) | FM_CKDIV_EN));
	} else {
		if (!fplls[i].ckdiv_en)
			clk_writel(pll_con0, (clk_readl(pll_con0) & ~(FM_CKDIV_EN)));
	}
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq(unsigned int ID, int type)
{
	void __iomem *dbg_addr = fm_base[FM_CKSYS] + CLK_DBG_CFG;
	void __iomem *misc_addr = fm_base[FM_CKSYS] + CLK_MISC_CFG_0;
	void __iomem *cali0_addr = fm_base[FM_CKSYS] + CLK26CALI_0;
	void __iomem *cali1_addr = fm_base[FM_CKSYS] + CLK26CALI_1;
	void __iomem *prot_idle_addr = fm_base[FM_CKSYS] + CLK_PROT_IDLE_REG_0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;
	unsigned int clk_div = 1, post_div = 1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	clk_writel(prot_idle_addr, ~PROT_IDLE_EN);

	set_clk_div_en(type, ID, true);

	if (type == CKGEN && check_pdn(fm_base[FM_CKSYS], CKGEN, ID)) {
		//pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		fmeter_unlock(flags);
		return -1;
	}

	while (clk_readl(cali0_addr) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) & ~(FM_RST_BITS)));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) | FM_RST_BITS));

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFF80FFFC) | (ID << 16));
	} else {
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		fmeter_unlock(flags);
		return 0;
	}

	clk_misc_cfg_0 = clk_readl(misc_addr);
	clk_writel(misc_addr, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk_writel(cali0_addr, 0x9000);
	clk_writel(cali0_addr, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(cali0_addr) & 0x10);

	temp = clk_readl(cali1_addr) & 0xFFFF;

	if (type == ABIST)
		post_div = get_post_div(type, ID);

	clk_div = get_clk_div(type, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	set_clk_div_en(type, ID, false);

	clk_writel(dbg_addr, clk_dbg_cfg);
	clk_writel(misc_addr, clk_misc_cfg_0);

	clk_writel(cali0_addr, FM_RST_BITS);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT) {
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		return 0;
	}

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	clk_writel(prot_idle_addr, PROT_IDLE_EN);

	return (output * 4);
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq_ck2(unsigned int ID, int type)
{
	void __iomem *dbg_addr = fm_base[FM_CKSYS_GP2] + CKSYS2_CLK_DBG_CFG;
	void __iomem *misc_addr = fm_base[FM_CKSYS_GP2] + CKSYS2_CLK_MISC_CFG_0;
	void __iomem *cali0_addr = fm_base[FM_CKSYS_GP2] + CKSYS2_CLK26CALI_0;
	void __iomem *cali1_addr = fm_base[FM_CKSYS_GP2] + CKSYS2_CLK26CALI_1;
	void __iomem *prot_idle_addr = fm_base[FM_CKSYS_GP2] + CKSYS2_CLK_PROT_IDLE_REG_0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;
	unsigned int clk_div = 1, post_div = 1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	set_clk_div_en(type, ID, true);

	clk_writel(prot_idle_addr, ~PROT_IDLE_EN);

	if (type == CKGEN_CK2 && check_pdn(fm_base[FM_CKSYS_GP2], CKGEN_CK2, ID)) {
		//pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		fmeter_unlock(flags);
		return -1;
	}

	while (clk_readl(cali0_addr) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) & ~(FM_RST_BITS)));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) | FM_RST_BITS));

	if (type == CKGEN_CK2) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST_CK2) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFC0FFFC) | (ID << 16));
	} else {
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		fmeter_unlock(flags);
		return 0;
	}

	clk_misc_cfg_0 = clk_readl(misc_addr);
	clk_writel(misc_addr, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk_writel(cali0_addr, 0x9000);
	clk_writel(cali0_addr, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(cali0_addr) & 0x10);

	temp = clk_readl(cali1_addr) & 0xFFFF;

	if (type == ABIST_CK2)
		post_div = get_post_div(type, ID);

	clk_div = get_clk_div(type, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	set_clk_div_en(type, ID, false);

	if (i > FM_TIMEOUT) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	clk_writel(dbg_addr, clk_dbg_cfg);
	clk_writel(misc_addr, clk_misc_cfg_0);

	clk_writel(cali0_addr, FM_RST_BITS);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT) {
		clk_writel(prot_idle_addr, PROT_IDLE_EN);
		return 0;
	}

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	clk_writel(prot_idle_addr, PROT_IDLE_EN);

	return (output * 4);
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq2(unsigned int  type, unsigned int id)
{
	void __iomem *pll_con0 = fm_base[type] + subsys_fm[type].pll_con0;
	void __iomem *pll_con1 = fm_base[type] + subsys_fm[type].pll_con1;
	void __iomem *pll_con5 = fm_base[type] + subsys_fm[type].pll_con5;
	void __iomem *con0 = fm_base[type] + subsys_fm[type].con0;
	void __iomem *con1 = fm_base[type] + subsys_fm[type].con1;
	void __iomem *prot_idle_addr = fm_base[type] + VLP_CLK_PROT_IDLE_REG_0;
	unsigned int temp, clk_div = 1, post_div = 1, ckdiv_en = -1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	if (type == FM_VLP_CKSYS && check_pdn(fm_base[FM_VLP_CKSYS], VLPCK, id)) {
		//pr_notice("ID-%d: MUX PDN, return 0.\n", id);
		fmeter_unlock(flags);
		return -1;
	}

	if (subsys_fm[type].pll_con0) {
		if (type != FM_VLP_CKSYS) {
			// check ckdiv_en
			if (clk_readl(pll_con0) & SUBSYS_CKDIV_EN)
				ckdiv_en = 1;
			// pll con0[16] = 1, pll con0[12] = 1
			// select pll_ckdiv, enable pll_ckdiv, enable test clk
			clk_writel(pll_con0, (clk_readl(pll_con0) | (SUBSYS_CKDIV_EN | SUBSYS_TST_EN)));
		}
	}

	if (type == FM_MFGPLL || type == FM_MFGPLL_SC0 || type == FM_MFGPLL_SC1)
		clk_writel(pll_con5, (clk_readl(pll_con5) | SUBSYS_TST_CK_SEL));

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	clk_writel(con0, clk_readl(con0) & ~(FM_RST_BITS));
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	clk_writel(con0, clk_readl(con0) | FM_RST_BITS);

	/* sel fqmtr_cksel */
	if (type == FM_VLP_CKSYS) {
		clk_writel(prot_idle_addr, ~PROT_IDLE_EN);
		clk_writel(con0, (clk_readl(con0) & 0xFFE0FFFF) | (id << 16));
	} else
		clk_writel(con0, (clk_readl(con0) & 0x00FFFFF8) | (id << 0));
	/* set ckgen_load_cnt to 1024 */
	clk_writel(con1, (clk_readl(con1) & 0xFC00FFFF) | (0x1FF << 16));

	/* sel fqmtr_cksel and set ckgen_k1 to 0(DIV4) */
	clk_writel(con0, (clk_readl(con0) & 0x00FFFFFF) | (3 << 24));

	/* fqmtr_en set to 1, fqmtr_exc set to 0, fqmtr_start set to 0 */
	clk_writel(con0, (clk_readl(con0) & 0xFFFF8007) | 0x1000);
	/*fqmtr_start set to 1 */
	clk_writel(con0, clk_readl(con0) | 0x10);
	// fmeter con0[1:0] = 0
	// choose test clk
	clk_writel(con0, (clk_readl(con0) & FM_TST_CLK_MASK));

	/* wait frequency meter finish */
	if (type == FM_VLP_CKSYS) {
		udelay(VLP_FM_WAIT_TIME);
	} else {
		while (clk_readl(con0) & 0x10) {
			udelay(10);
			i++;
			if (i > FM_TIMEOUT) {
				pr_notice("[%d]con0: 0x%x, con1: 0x%x\n",
					id, clk_readl(con0), clk_readl(con1));
				break;
			}
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / FM_SCALE; // Khz

	if (subsys_fm[type].pll_con0) {
		if (type != FM_VLP_CKSYS) {
			clk_div = (clk_readl(pll_con0) & SUBSYS_CKDIV_MASK) >> SUBSYS_CKDIV_SHIFT;
			if (ckdiv_en)
				clk_writel(pll_con0, (clk_readl(pll_con0) & ~(SUBSYS_TST_EN)));
			else
				clk_writel(pll_con0, (clk_readl(pll_con0)
					& ~(SUBSYS_CKDIV_EN | SUBSYS_TST_EN)));
		}
	}

	if (clk_div == 0)
		clk_div = 1;

	if (subsys_fm[type].pll_con1 != 0)
		if (type != FM_VLP_CKSYS)
			post_div = 1 << ((clk_readl(pll_con1) & FM_POSTDIV_MASK) >> FM_POSTDIV_SHIFT);

	if (type == FM_MFGPLL || type == FM_MFGPLL_SC0 || type == FM_MFGPLL_SC1)
		clk_writel(pll_con5, (clk_readl(pll_con5) & ~(SUBSYS_TST_CK_SEL)));

	if (type == FM_VLP_CKSYS)
		clk_writel(prot_idle_addr, PROT_IDLE_EN);

	clk_writel(con0, FM_RST_BITS);

	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;

	return (output * 4 * clk_div) / post_div;
}

static unsigned int mt6991_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6991_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6991_get_ckgen_ck2_freq(unsigned int ID)
{
	return __mt_get_freq_ck2(ID, CKGEN_CK2);
}

static unsigned int mt6991_get_abist_ck2_freq(unsigned int ID)
{
	return __mt_get_freq_ck2(ID, ABIST_CK2);
}

static unsigned int mt6991_get_vlpck_freq(unsigned int ID)
{
	return __mt_get_freq2(FM_VLP_CKSYS, ID);
}

static unsigned int mt6991_get_subsys_freq(unsigned int ID)
{
	int output = 0;
	unsigned long flags;

	subsys_fmeter_lock(flags);

	pr_notice("subsys ID: %d\n", ID);
	if (ID >= FM_SYS_NUM)
		return 0;

	output = __mt_get_freq2(ID, FM_PLL_TST_CK);

	subsys_fmeter_unlock(flags);

	return output;
}

static unsigned int mt6991_get_fmeter_freq(unsigned int id,
		enum FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6991_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6991_get_abist_freq(id);
	else if (type == CKGEN_CK2)
		return mt6991_get_ckgen_ck2_freq(id);
	else if (type == ABIST_CK2)
		return mt6991_get_abist_ck2_freq(id);
	else if (type == SUBSYS)
		return mt6991_get_subsys_freq(id);
	else if (type == VLPCK)
		return mt6991_get_vlpck_freq(id);

	return FT_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6991_get_fmeter_clks,
	.get_fmeter_freq = mt6991_get_fmeter_freq,
};

static int clk_fmeter_mt6991_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < FM_SYS_NUM; i++) {
		fm_base[i] = get_base_from_comp(comp_list[i]);
		if (IS_ERR(fm_base[i]))
			goto ERR;

	}

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s(%s) can't find base\n", __func__, comp_list[i]);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6991_drv = {
	.probe = clk_fmeter_mt6991_probe,
	.driver = {
		.name = "clk-fmeter-mt6991",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6991", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6991_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6991_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
