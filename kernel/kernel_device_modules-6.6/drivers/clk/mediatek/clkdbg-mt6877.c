// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"


const char * const *get_mt6877_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"spm_sel",
		"scp_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"mdp0_sel",
		"img1_sel",
		"ipe_sel",
		"dpe_sel",
		"cam_sel",
		"ccu_sel",
		"dsp_sel",
		"dsp1_sel",
		"dsp2_sel",
		"dsp4_sel",
		"dsp7_sel",
		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"uart_sel",
		"spi_sel",
		"msdc5hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"pwrap_ulposc_sel",
		"atb_sel",
		"sspm_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"dxcc_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",
		"dpmaif_main_sel",
		"venc_sel",
		"vdec_sel",
		"camtm_sel",
		"pwm_sel",
		"audio_h_sel",
		"mcupm_sel",
		"spmi_m_mst_sel",
		"dvfsrc_sel",
		"mem_sub_sel",
		"aes_msdcfde_sel",
		"ufs_mbist_sel",
		"mfg_internal2_sel",
		"mfg_internal1_sel",
		"ap2conn_host_sel",
		"msdc_new_rx_sel",
		"apll_i2s0_mck_sel",
		"apll_i2s1_mck_sel",
		"apll_i2s2_mck_sel",
		"apll_i2s3_mck_sel",
		"apll_i2s4_mck_sel",
		"apll_i2s5_mck_sel",
		"apll_i2s6_mck_sel",
		"apll_i2s7_mck_sel",
		"apll_i2s8_mck_sel",
		"apll_i2s9_mck_sel",

		/* topckgen */
		"apll12_div0",
		"apll12_div1",
		"apll12_div2",
		"apll12_div3",
		"apll12_div4",
		"apll12_divb",
		"apll12_div5",
		"apll12_div6",
		"apll12_div7",
		"apll12_div8",
		"apll12_div9",

		/* infracfg_ao */
		"ifrao_pmic_tmr",
		"ifrao_pmic_ap",
		"ifrao_pmic_md",
		"ifrao_pmic_conn",
		"ifrao_apxgpt",
		"ifrao_gce",
		"ifrao_gce2",
		"ifrao_therm",
		"ifrao_i2c_pseudo",
		"ifrao_pwm_hclk",
		"ifrao_pwm1",
		"ifrao_pwm2",
		"ifrao_pwm3",
		"ifrao_pwm4",
		"ifrao_pwm",
		"ifrao_uart0",
		"ifrao_uart1",
		"ifrao_uart2",
		"ifrao_uart3",
		"ifrao_gce_26m",
		"ifrao_btif",
		"ifrao_spi0",
		"ifrao_msdc0",
		"ifrao_msdc1",
		"ifrao_msdc0_clk",
		"ifrao_auxadc",
		"ifrao_cpum",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_auxadc_md",
		"ifrao_msdc1_clk",
		"ifrao_msdc0_aes_clk",
		"ifrao_ccif_ap",
		"ifrao_audio",
		"ifrao_ccif_md",
		"ifrao_ssusb",
		"ifrao_disp_pwm",
		"ifrao_cldmabclk",
		"ifrao_audio26m",
		"ifrao_spi1",
		"ifrao_spi2",
		"ifrao_spi3",
		"ifrao_unipro_sysclk",
		"ifrao_ufs_bclk",
		"ifrao_apdma",
		"ifrao_spi4",
		"ifrao_spi5",
		"ifrao_cq_dma",
		"ifrao_ufs",
		"ifrao_aes_ufsfde",
		"ifrao_ssusb_xhci",
		"ifrao_ap_msdc0",
		"ifrao_md_msdc0",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_fbist2fpc",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_md",
		"ifrao_spi6_ck",
		"ifrao_spi7_ck",
		"ifrao_aes_0p_ck",

		/* apmixedsys */
		"armpll_ll",
		"armpll_bl",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"usbpll",

		/* scp_par_top */
		"scp_par_audiodsp",

		/* audio */
		"aud_afe",
		"aud_22m",
		"aud_24m",
		"aud_apll2_tuner",
		"aud_apll_tuner",
		"aud_tdm_ck",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_nle",
		"aud_connsys_i2s_asrc",
		"aud_general1_asrc",
		"aud_general2_asrc",
		"aud_dac_hires",
		"aud_adc_hires",
		"aud_adc_hires_tml",
		"aud_adda6_adc",
		"aud_adda6_adc_hires",
		"aud_3rd_dac",
		"aud_3rd_dac_predis",
		"aud_3rd_dac_tml",
		"aud_3rd_dac_hires",

		/* msdc0 */
		"msdc0_msdc_rx",

		/* imp_iic_wrap_c */
		"impc_ap_clock_i2c10",
		"impc_ap_clock_i2c11",

		/* imp_iic_wrap_e */
		"impe_ap_clock_i2c3",

		/* imp_iic_wrap_s */
		"imps_ap_clock_i2c5",
		"imps_ap_clock_i2c7",
		"imps_ap_clock_i2c8",
		"imps_ap_clock_i2c9",

		/* imp_iic_wrap_ws */
		"impws_ap_clock_i2c1",
		"impws_ap_clock_i2c2",
		"impws_ap_clock_i2c4",

		/* imp_iic_wrap_w */
		"impw_ap_clock_i2c0",

		/* imp_iic_wrap_n */
		"impn_ap_clock_i2c6",

		/* gpu_pll_ctrl */
		"mfg_ao_mfgpll1",
		"mfg_ao_mfgpll4",

		/* mfgcfg */
		"mfgcfg_bg3d",

		/* mmsys_config */
		"mm_disp_mutex0",
		"mm_apb_bus",
		"mm_disp_ovl0",
		"mm_disp_rdma0",
		"mm_disp_ovl0_2l",
		"mm_disp_wdma0",
		"mm_disp_ccorr1",
		"mm_disp_rsz0",
		"mm_disp_aal0",
		"mm_disp_ccorr0",
		"mm_disp_color0",
		"mm_smi_infra",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_spr0",
		"mm_disp_dither0",
		"mm_smi_common",
		"mm_disp_cm0",
		"mm_dsi0",
		"mm_smi_gals",
		"mm_disp_dsc_wrap",
		"mm_smi_iommu",
		"mm_disp_ovl1_2l",
		"mm_disp_ufbc_wdma0",
		"mm_dsi0_dsi_domain",
		"mm_disp_26m_ck",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_dip",
		"imgsys1_gals",

		/* imgsys2 */
		"imgsys2_larb9",
		"imgsys2_larb10",
		"imgsys2_mfb",
		"imgsys2_wpe",
		"imgsys2_mss",
		"imgsys2_gals",

		/* vdec_gcon */
		"vde2_vdec_cken",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke5_gals",

		/* apu_conn2 */
		"apu_conn2_ahb",
		"apu_conn2_axi",
		"apu_conn2_isp",
		"apu_conn2_cam_adl",
		"apu_conn2_img_adl",
		"apu_conn2_emi_26m",
		"apu_conn2_vpu_udi",
		"apu_conn2_edma_0",
		"apu_conn2_edma_1",
		"apu_conn2_edmal_0",
		"apu_conn2_edmal_1",
		"apu_conn2_mnoc",
		"apu_conn2_tcm",
		"apu_conn2_md32",
		"apu_conn2_iommu_0",
		"apu_conn2_iommu_1",
		"apu_conn2_md32_32k",
		"apu_conn2_cpe",

		/* apu_conn1 */
		"apu_conn1_axi",
		"apu_conn1_edma_0",
		"apu_conn1_edma_1",
		"apu_conn1_iommu_0",
		"apu_conn1_iommu_1",

		/* apusys_vcore */
		"apuv_ahb",
		"apuv_axi",
		"apuv_adl",
		"apuv_qos",

		/* apu0 */
		"apu0_apu",
		"apu0_axi_m",
		"apu0_jtag",

		/* apu1 */
		"apu1_apu",
		"apu1_axi_m",
		"apu1_jtag",

		/* apu_mdla0 */
		"apum0_mdla_cg0",
		"apum0_mdla_cg1",
		"apum0_mdla_cg2",
		"apum0_mdla_cg3",
		"apum0_mdla_cg4",
		"apum0_mdla_cg5",
		"apum0_mdla_cg6",
		"apum0_mdla_cg7",
		"apum0_mdla_cg8",
		"apum0_mdla_cg9",
		"apum0_mdla_cg10",
		"apum0_mdla_cg11",
		"apum0_mdla_cg12",
		"apum0_apb",
		"apum0_axi_m",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv0",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
		"cam_m_ccu0",
		"cam_m_ccu1",
		"cam_m_mraw0",
		"cam_m_ccu_gals",
		"cam_m_cam2mm_gals",
		"cam_m_camsv4",
		"cam_m_pda",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* ipesys */
		"ipe_larb19",
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_dpe",
		"ipe_gals",

		/* mdpsys_config */
		"mdp_rdma0",
		"mdp_tdshp0",
		"mdp_img_dl_async0",
		"mdp_img_dl_async1",
		"mdp_rdma1",
		"mdp_tdshp1",
		"mdp_smi0",
		"mdp_apb_bus",
		"mdp_wrot0",
		"mdp_rsz0",
		"mdp_hdr0",
		"mdp_mutex0",
		"mdp_wrot1",
		"mdp_color0",
		"mdp_aal0",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_img_dl_rel0_as0",
		"mdp_img_dl_rel1_as1",

		/* SCPSYS */
		"PG_MFG0",
		"PG_MFG1",
		"PG_MFG2",
		"PG_MFG3",
		"PG_MFG4",
		"PG_MFG5",
		"PG_MD",
	};

	return clks;
}

/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6877_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6877_all_clk_names,
};

static int clk_dbg_mt6877_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6877_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6877_drv = {
	.probe = clk_dbg_mt6877_probe,
	.driver = {
		.name = "clk-dbg-mt6877",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6877_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6877_drv, "clk-dbg-mt6877");
}

static void __exit clkdbg_mt6877_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6877_drv);
}

subsys_initcall(clkdbg_mt6877_init);
module_exit(clkdbg_mt6877_exit);
MODULE_LICENSE("GPL");
