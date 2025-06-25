// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/random.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clkchk-mt6991.h"
#include "clk-fmeter.h"

#define THREAD_LEN		(16)
#define THREAD_NUM		(6)
#define PD_NUM			(6)

static int clkdbg_thread_cnt;

const char * const *get_mt6991_all_clk_names(void)
{
	static const char * const clks[] = {
		/* cksys */
		"ck_axi_sel",
		"ck_mem_sub_sel",
		"ck_io_noc_sel",
		"ck_p_axi_sel",
		"ck_pextp0_axi_sel",
		"ck_pextp1_usb_axi_sel",
		"ck_p_fmem_sub_sel",
		"ck_pexpt0_mem_sub_sel",
		"ck_pextp1_usb_mem_sub_sel",
		"ck_p_noc_sel",
		"ck_emi_n_sel",
		"ck_emi_s_sel",
		"ck_ap2conn_host_sel",
		"ck_atb_sel",
		"ck_cirq_sel",
		"ck_pbus_156m_sel",
		"ck_efuse_sel",
		"ck_mcl3gic_sel",
		"ck_mcinfra_sel",
		"ck_dsp_sel",
		"ck_mfg_ref_sel",
		"ck_mfg_eb_sel",
		"ck_uart_sel",
		"ck_spi0_b_sel",
		"ck_spi1_b_sel",
		"ck_spi2_b_sel",
		"ck_spi3_b_sel",
		"ck_spi4_b_sel",
		"ck_spi5_b_sel",
		"ck_spi6_b_sel",
		"ck_spi7_b_sel",
		"ck_msdc30_1_sel",
		"ck_msdc30_2_sel",
		"ck_disp_pwm_sel",
		"ck_usb_1p_sel",
		"ck_usb_xhci_1p_sel",
		"ck_usb_fmcnt_p1_sel",
		"ck_i2c_p_sel",
		"ck_i2c_east_sel",
		"ck_i2c_west_sel",
		"ck_i2c_north_sel",
		"ck_aes_ufsfde_sel",
		"ck_sel",
		"ck_aud_1_sel",
		"ck_aud_2_sel",
		"ck_adsp_sel",
		"ck_adsp_uarthub_b_sel",
		"ck_dpmaif_main_sel",
		"ck_pwm_sel",
		"ck_mcupm_sel",
		"ck_ipseast_sel",
		"ck_tl_sel",
		"ck_tl_p1_sel",
		"ck_tl_p2_sel",
		"ck_md_emi_sel",
		"ck_sdf_sel",
		"ck_uarthub_b_sel",
		"ck_dpsw_cmp_26m_sel",
		"ck_smapck_sel",
		"ck_ssr_pka_sel",
		"ck_ssr_dma_sel",
		"ck_ssr_kdf_sel",
		"ck_ssr_rng_sel",
		"ck_spu0_sel",
		"ck_spu1_sel",
		"ck_dxcc_sel",
		"ck_apll_i2sin0_m_sel",
		"ck_apll_i2sin1_m_sel",
		"ck_apll_i2sin2_m_sel",
		"ck_apll_i2sin3_m_sel",
		"ck_apll_i2sin4_m_sel",
		"ck_apll_i2sin6_m_sel",
		"ck_apll_i2sout0_m_sel",
		"ck_apll_i2sout1_m_sel",
		"ck_apll_i2sout2_m_sel",
		"ck_apll_i2sout3_m_sel",
		"ck_apll_i2sout4_m_sel",
		"ck_apll_i2sout6_m_sel",
		"ck_apll_fmi2s_m_sel",
		"ck_apll_tdmout_m_sel",

		/* cksys */
		"ck_apll12_div_i2sin0",
		"ck_apll12_div_i2sin1",
		"ck_apll12_div_i2sin2",
		"ck_apll12_div_i2sin3",
		"ck_apll12_div_i2sin4",
		"ck_apll12_div_i2sin6",
		"ck_apll12_div_i2sout0",
		"ck_apll12_div_i2sout1",
		"ck_apll12_div_i2sout2",
		"ck_apll12_div_i2sout3",
		"ck_apll12_div_i2sout4",
		"ck_apll12_div_i2sout6",
		"ck_apll12_div_fmi2s",
		"ck_apll12_div_tdmout_m",
		"ck_apll12_div_tdmout_b",

		/* apmixedsys */
		"mainpll",
		"univpll",
		"msdcpll",
		"adsppll",
		"emipll",
		"emipll2",

		/* cksys_gp2 */
		"ck2_seninf0_sel",
		"ck2_seninf1_sel",
		"ck2_seninf2_sel",
		"ck2_seninf3_sel",
		"ck2_seninf4_sel",
		"ck2_seninf5_sel",
		"ck2_img1_sel",
		"ck2_ipe_sel",
		"ck2_cam_sel",
		"ck2_camtm_sel",
		"ck2_dpe_sel",
		"ck2_vdec_sel",
		"ck2_ccusys_sel",
		"ck2_ccutm_sel",
		"ck2_venc_sel",
		"ck2_dp1_sel",
		"ck2_dp0_sel",
		"ck2_disp_sel",
		"ck2_mdp_sel",
		"ck2_mminfra_sel",
		"ck2_mminfra_snoc_sel",
		"ck2_mmup_sel",
		"ck2_mminfra_ao_sel",

		/* apmixedsys_gp2 */
		"mainpll2",
		"univpll2",
		"mmpll2",
		"imgpll",
		"tvdpll1",
		"tvdpll2",
		"tvdpll3",

		/* imp_iic_wrap_e */
		"impe_i2c5",

		/* imp_iic_wrap_w */
		"impw_i2c0",
		"impw_i2c3",
		"impw_i2c6",
		"impw_i2c10",

		/* imp_iic_wrap_n */
		"impn_i2c1",
		"impn_i2c2",
		"impn_i2c4",
		"impn_i2c7",
		"impn_i2c8",
		"impn_i2c9",

		/* apifrbus_ao_mem_reg */
		"ifr_mem_dpmaif_main",
		"ifr_mem_dpmaif_26m",

		/* imp_iic_wrap_c */
		"impc_i2c11",
		"impc_i2c12",
		"impc_i2c13",
		"impc_i2c14",

		/* pericfg_ao */
		"perao_uart0_bclk",
		"perao_uart1_bclk",
		"perao_uart2_bclk",
		"perao_uart3_bclk",
		"perao_uart4_bclk",
		"perao_uart5_bclk",
		"perao_pwm_x16w",
		"perao_pwm_x16w_bclk",
		"perao_pwm_pwm_bclk0",
		"perao_pwm_pwm_bclk1",
		"perao_pwm_pwm_bclk2",
		"perao_pwm_pwm_bclk3",
		"perao_spi0_bclk",
		"perao_spi1_bclk",
		"perao_spi2_bclk",
		"perao_spi3_bclk",
		"perao_spi4_bclk",
		"perao_spi5_bclk",
		"perao_spi6_bclk",
		"perao_spi7_bclk",
		"perao_ap_dma_x32w_bclk",
		"perao_msdc1_msdc_src",
		"perao_msdc1",
		"perao_msdc1_axi",
		"perao_msdc1_h_wrap",
		"perao_msdc2_msdc_src",
		"perao_msdc2",
		"perao_msdc2_axi",
		"perao_msdc2_h_wrap",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",
		"ufsao_unipro_sap",
		"ufsao_phy_sap",
		"ufsao_ufshci_ufs",
		"ufsao_ufshci_aes",

		/* pextp0cfg_ao */
		"pext_pm0_tl",
		"pext_pm0_ref",
		"pext_pp0_mcu_bus",
		"pext_pp0_pextp_ref",
		"pext_pm0_axi_250",
		"pext_pm0_ahb_apb",
		"pext_pm0_pl_p",
		"pext_pextp_vlp_ao_p0_lp",

		/* pextp1cfg_ao */
		"pext1_pm1_tl",
		"pext1_pm1_ref",
		"pext1_pm2_tl",
		"pext1_pm2_ref",
		"pext1_pp1_mcu_bus",
		"pext1_pp1_pextp_ref",
		"pext1_pp2_mcu_bus",
		"pext1_pp2_pextp_ref",
		"pext1_pm1_axi_250",
		"pext1_pm1_ahb_apb",
		"pext1_pm1_pl_p",
		"pext1_pm2_axi_250",
		"pext1_pm2_ahb_apb",
		"pext1_pm2_pl_p",
		"pext1_pextp_vlp_ao_p1_lp",
		"pext1_pextp_vlp_ao_p2_lp",

		/* vlp_cksys */
		"vlp-apll1",
		"vlp-apll2",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_32ksel",
		"vlp_apxgpt_26m_b_sel",
		"vlp_dpsw_sel",
		"vlp_dpsw_central_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_srck_sel",
		"vlp_camtg0_sel",
		"vlp_camtg1_sel",
		"vlp_camtg2_sel",
		"vlp_camtg3_sel",
		"vlp_camtg4_sel",
		"vlp_camtg5_sel",
		"vlp_camtg6_sel",
		"vlp_camtg7_sel",
		"vlp_sspm_26m_sel",
		"vlp_ulposc_sspm_sel",
		"vlp_vlp_pbus_26m_sel",
		"vlp_debug_err_flag_sel",
		"vlp_dpmsrdma_sel",
		"vlp_vlp_pbus_156m_sel",
		"vlp_spm_sel",
		"vlp_mminfra_vlp_sel",
		"vlp_usb_sel",
		"vlp_usb_xhci_sel",
		"vlp_noc_vlp_sel",
		"vlp_audio_h_sel",
		"vlp_aud_engen1_sel",
		"vlp_aud_engen2_sel",
		"vlp_aud_intbus_sel",
		"vlp_spvlp_26m_sel",
		"vlp_spu0_vlp_sel",
		"vlp_spu1_vlp_sel",

		/* scp_i3c */
		"scp_i3c_i2c1",

		/* afe */
		"afe_pcm1",
		"afe_pcm0",
		"afe_cm2",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_fm_i2s",
		"afe_mtkaifv4",
		"afe_ul2_aht",
		"afe_ul2_adc_hires",
		"afe_ul2_tml",
		"afe_ul2_adc",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in6",
		"afe_etdm_in5",
		"afe_etdm_in4",
		"afe_etdm_in3",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out5",
		"afe_etdm_out4",
		"afe_etdm_out3",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
		"afe_tdm_out",
		"afe_general15_asrc",
		"afe_general14_asrc",
		"afe_general13_asrc",
		"afe_general12_asrc",
		"afe_general11_asrc",
		"afe_general10_asrc",
		"afe_general9_asrc",
		"afe_general8_asrc",
		"afe_general7_asrc",
		"afe_general6_asrc",
		"afe_general5_asrc",
		"afe_general4_asrc",
		"afe_general3_asrc",
		"afe_general2_asrc",
		"afe_general1_asrc",
		"afe_general0_asrc",
		"afe_connsys_i2s_asrc",
		"afe_audio_hopping_ck",
		"afe_audio_f26m_ck",
		"afe_apll1_ck",
		"afe_apll2_ck",
		"afe_h208m_ck",
		"afe_apll_tuner2",
		"afe_apll_tuner1",

		/* dispsys_config */
		"mm_config",
		"mm_disp_mutex0",
		"mm_disp_aal0",
		"mm_disp_aal1",
		"mm_disp_c3d0",
		"mm_disp_c3d1",
		"mm_disp_c3d2",
		"mm_disp_c3d3",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_ccorr2",
		"mm_disp_ccorr3",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_dither0",
		"mm_disp_dither1",
		"mm_disp_dli_async0",
		"mm_disp_dli_async1",
		"mm_disp_dli_async2",
		"mm_disp_dli_async3",
		"mm_disp_dli_async4",
		"mm_disp_dli_async5",
		"mm_disp_dli_async6",
		"mm_disp_dli_async7",
		"mm_disp_dli_async8",
		"mm_disp_dli_async9",
		"mm_disp_dli_async10",
		"mm_disp_dli_async11",
		"mm_disp_dli_async12",
		"mm_disp_dli_async13",
		"mm_disp_dli_async14",
		"mm_disp_dli_async15",
		"mm_disp_dlo_async0",
		"mm_disp_dlo_async1",
		"mm_disp_dlo_async2",
		"mm_disp_dlo_async3",
		"mm_disp_dlo_async4",
		"mm_disp_dlo_async5",
		"mm_disp_dlo_async6",
		"mm_disp_dlo_async7",
		"mm_disp_dlo_async8",
		"mm_disp_gamma0",
		"mm_disp_gamma1",
		"mm_mdp_aal0",
		"mm_mdp_aal1",
		"mm_mdp_rdma0",
		"mm_disp_postmask0",
		"mm_disp_postmask1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_disp_spr0",
		"mm_disp_tdshp0",
		"mm_disp_tdshp1",
		"mm_disp_wdma0",
		"mm_disp_y2r0",
		"mm_ssc",
		"mm_disp_fake_eng0",

		/* dispsys1_config */
		"mm1_dispsys1_config",
		"mm1_dispsys1_s_config",
		"mm1_disp_mutex0",
		"mm1_disp_dli_async20",
		"mm1_disp_dli_async21",
		"mm1_disp_dli_async22",
		"mm1_disp_dli_async23",
		"mm1_disp_dli_async24",
		"mm1_disp_dli_async25",
		"mm1_disp_dli_async26",
		"mm1_disp_dli_async27",
		"mm1_disp_dli_async28",
		"mm1_disp_relay0",
		"mm1_disp_relay1",
		"mm1_disp_relay2",
		"mm1_disp_relay3",
		"mm1_DP_CLK",
		"mm1_disp_dp_intf1",
		"mm1_disp_dsc_wrap0",
		"mm1_disp_dsc_wrap1",
		"mm1_disp_dsc_wrap2",
		"mm1_disp_dsc_wrap3",
		"mm1_CLK0",
		"mm1_CLK1",
		"mm1_CLK2",
		"mm1_disp_dvo0",
		"mm1_disp_gdma0",
		"mm1_disp_merge0",
		"mm1_disp_merge1",
		"mm1_disp_merge2",
		"mm1_disp_oddmr0",
		"mm1_disp_postalign0",
		"mm1_disp_dither2",
		"mm1_disp_r2y0",
		"mm1_disp_splitter0",
		"mm1_disp_splitter1",
		"mm1_disp_splitter2",
		"mm1_disp_splitter3",
		"mm1_disp_vdcm0",
		"mm1_disp_wdma1",
		"mm1_disp_wdma2",
		"mm1_disp_wdma3",
		"mm1_disp_wdma4",
		"mm1_mdp_rdma1",
		"mm1_smi_larb0",
		"mm1_mod1",
		"mm1_mod2",
		"mm1_mod3",
		"mm1_mod4",
		"mm1_mod5",
		"mm1_cg0",
		"mm1_cg1",
		"mm1_cg2",
		"mm1_cg3",
		"mm1_cg4",
		"mm1_cg5",
		"mm1_cg6",
		"mm1_cg7",
		"mm1_f26m_ck",

		/* ovlsys_config */
		"ovlsys_config",
		"ovl_fake_eng0",
		"ovl_fake_eng1",
		"ovl_mutex0",
		"ovl_exdma0",
		"ovl_exdma1",
		"ovl_exdma2",
		"ovl_exdma3",
		"ovl_exdma4",
		"ovl_exdma5",
		"ovl_exdma6",
		"ovl_exdma7",
		"ovl_exdma8",
		"ovl_exdma9",
		"ovl_blender0",
		"ovl_blender1",
		"ovl_blender2",
		"ovl_blender3",
		"ovl_blender4",
		"ovl_blender5",
		"ovl_blender6",
		"ovl_blender7",
		"ovl_blender8",
		"ovl_blender9",
		"ovl_outproc0",
		"ovl_outproc1",
		"ovl_outproc2",
		"ovl_outproc3",
		"ovl_outproc4",
		"ovl_outproc5",
		"ovl_mdp_rsz0",
		"ovl_mdp_rsz1",
		"ovl_disp_wdma0",
		"ovl_disp_wdma1",
		"ovl_ufbc_wdma0",
		"ovl_mdp_rdma0",
		"ovl_mdp_rdma1",
		"ovl_bwm0",
		"ovl_dli0",
		"ovl_dli1",
		"ovl_dli2",
		"ovl_dli3",
		"ovl_dli4",
		"ovl_dli5",
		"ovl_dli6",
		"ovl_dli7",
		"ovl_dli8",
		"ovl_dlo0",
		"ovl_dlo1",
		"ovl_dlo2",
		"ovl_dlo3",
		"ovl_dlo4",
		"ovl_dlo5",
		"ovl_dlo6",
		"ovl_dlo7",
		"ovl_dlo8",
		"ovl_dlo9",
		"ovl_dlo10",
		"ovl_dlo11",
		"ovl_dlo12",
		"ovlsys_relay0",
		"ovl_inlinerot0",
		"ovl_smi",

		/* ovlsys1_config */
		"ovl1_ovlsys_config",
		"ovl1_ovl_fake_eng0",
		"ovl1_ovl_fake_eng1",
		"ovl1_ovl_mutex0",
		"ovl1_ovl_exdma0",
		"ovl1_ovl_exdma1",
		"ovl1_ovl_exdma2",
		"ovl1_ovl_exdma3",
		"ovl1_ovl_exdma4",
		"ovl1_ovl_exdma5",
		"ovl1_ovl_exdma6",
		"ovl1_ovl_exdma7",
		"ovl1_ovl_exdma8",
		"ovl1_ovl_exdma9",
		"ovl1_ovl_blender0",
		"ovl1_ovl_blender1",
		"ovl1_ovl_blender2",
		"ovl1_ovl_blender3",
		"ovl1_ovl_blender4",
		"ovl1_ovl_blender5",
		"ovl1_ovl_blender6",
		"ovl1_ovl_blender7",
		"ovl1_ovl_blender8",
		"ovl1_ovl_blender9",
		"ovl1_ovl_outproc0",
		"ovl1_ovl_outproc1",
		"ovl1_ovl_outproc2",
		"ovl1_ovl_outproc3",
		"ovl1_ovl_outproc4",
		"ovl1_ovl_outproc5",
		"ovl1_ovl_mdp_rsz0",
		"ovl1_ovl_mdp_rsz1",
		"ovl1_ovl_disp_wdma0",
		"ovl1_ovl_disp_wdma1",
		"ovl1_ovl_ufbc_wdma0",
		"ovl1_ovl_mdp_rdma0",
		"ovl1_ovl_mdp_rdma1",
		"ovl1_ovl_bwm0",
		"ovl1_dli0",
		"ovl1_dli1",
		"ovl1_dli2",
		"ovl1_dli3",
		"ovl1_dli4",
		"ovl1_dli5",
		"ovl1_dli6",
		"ovl1_dli7",
		"ovl1_dli8",
		"ovl1_dlo0",
		"ovl1_dlo1",
		"ovl1_dlo2",
		"ovl1_dlo3",
		"ovl1_dlo4",
		"ovl1_dlo5",
		"ovl1_dlo6",
		"ovl1_dlo7",
		"ovl1_dlo8",
		"ovl1_dlo9",
		"ovl1_dlo10",
		"ovl1_dlo11",
		"ovl1_dlo12",
		"ovl1_ovlsys_relay0",
		"ovl1_ovl_inlinerot0",
		"ovl1_smi",

		/* imgsys_main */
		"img_fdvt",
		"img_me",
		"img_mmg",
		"img_larb12",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_adl_larb",
		"img_adlrd",
		"img_adlwr0",
		"img_avs",
		"img_ips",
		"img_adlwr1",
		"img_rootcq",
		"img_bls",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common2",
		"img_sub_common3",
		"img_sub_common4",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_wpe2",
		"img_gals_trx_ipe0",
		"img_gals_trx_ipe1",
		"img26",
		"img_bwr",
		"img_gals",

		/* dip_top_dip1 */
		"dip_dip1_dip_top",
		"dip_dip1_dip_gals0",
		"dip_dip1_dip_gals1",
		"dip_dip1_dip_gals2",
		"dip_dip1_dip_gals3",
		"dip_dip1_larb10",
		"dip_dip1_larb15",
		"dip_dip1_larb38",
		"dip_dip1_larb39",

		/* dip_nr1_dip1 */
		"dip_nr1_dip1_larb",
		"dip_nr1_dip1_dip_nr1",

		/* dip_nr2_dip1 */
		"dip_nr2_dip1_dip_nr",
		"dip_nr2_dip1_larb15",
		"dip_nr2_dip1_larb39",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",
		"wpe1_dip1_gals0",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",
		"wpe2_dip1_gals0",

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",
		"wpe3_dip1_gals0",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",
		"traw_dip1_gals",

		/* traw_cap_dip1 */
		"traw__dip1_cap",

		/* img_vcore_d1a */
		"img_vcore_gals_disp",
		"img_vcore_main",
		"img_vcore_sub0",
		"img_vcore_sub1",
		"img_vcore_img_26m",

		/* vdec_soc_gcon_base */
		"vde1_larb1_cken",
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",
		"vde1_aptv_en",
		"vde1_aptv_topen",
		"vde1_vdec_soc_ips_en",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_larb",
		"ven1_venc",
		"ven1_jpgenc",
		"ven1_jpgdec",
		"ven1_jpgdec_c1",
		"ven1_gals",
		"ven1_venc_adab_ctrl",
		"ven1_venc_xpc_ctrl",
		"ven1_gals_sram",
		"ven1_res_flat",

		/* venc_gcon_core1 */
		"ven2_larb",
		"ven2_venc",
		"ven2_jpgenc",
		"ven2_jpgdec",
		"ven2_gals",
		"ven2_venc_xpc_ctrl",
		"ven2_gals_sram",
		"ven2_res_flat",

		/* venc_gcon_core2 */
		"ven_c2_larb",
		"ven_c2_venc",
		"ven_c2_gals",
		"ven_c2_venc_xpc_ctrl",
		"ven_c2_gals_sram",
		"ven_c2_res_flat",

		/* cam_main_r1a */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_larb27",
		"cam_m_larb29",
		"cam_m_cam",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_subc",
		"cam_m_cam_mraw",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv",
		"cam_m_adlrd",
		"cam_m_adlwr",
		"cam_m_uisp",
		"cam_m_fake_eng",
		"cam_m_cam2mm0_GCON_0",
		"cam_m_cam2mm1_GCON_0",
		"cam_m_cam2sys_GCON_0",
		"cam_m_cam2mm2_GCON_0",
		"cam_m_ips",
		"cam_m_cam_dpe",
		"cam_m_cam_asg",
		"cam_m_camsv_a_con_1",
		"cam_m_camsv_b_con_1",
		"cam_m_camsv_c_con_1",
		"cam_m_camsv_d_con_1",
		"cam_m_camsv_e_con_1",
		"cam_m_camsv_con_1",
		"cam_m_cam_qof_con_1",
		"cam_m_cam_bls_full_con_1",
		"cam_m_cam_bls_part_con_1",
		"cam_m_cam_bwr_con_1",
		"cam_m_cam_rtcq_con_1",
		"cam_m_cam2mm0_CG",
		"cam_m_cam2mm1_CG",
		"cam_m_cam2sys_CG",
		"cam_m_cam2mm2_CG",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_gals",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_mraw3",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* camsys_ipe */
		"camsys_ipe_larb19",
		"camsys_ipe_dpe",
		"camsys_ipe_fus",
		"camsys_ipe_dhze",
		"camsys_ipe_gals",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		"cam_ra_raw2mm_gals",
		"cam_ra_yuv2raw2mm",

		/* camsys_rmsa */
		"camsys_rmsa_larbx",
		"camsys_rmsa_cam",
		"camsys_rmsa_camtg",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		"cam_rb_raw2mm_gals",
		"cam_rb_yuv2raw2mm",

		/* camsys_rmsb */
		"camsys_rmsb_larbx",
		"camsys_rmsb_cam",
		"camsys_rmsb_camtg",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",
		"cam_rc_raw2mm_gals",
		"cam_rc_yuv2raw2mm",

		/* camsys_rmsc */
		"camsys_rmsc_larbx",
		"camsys_rmsc_cam",
		"camsys_rmsc_camtg",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",

		/* ccu_main */
		"cclarb30_con",
		"ccu2infra_GCON",
		"ccusys_ccu0_con",
		"ccusys_ccu1_con",
		"ccu2mm0_GCON",

		/* cam_vcore_r1a */
		"vcore",
		"_26m",
		"2mm0_subcommon_dis",
		"CAM_V_MM0_SUBCOMM",

		/* mdpsys_config */
		"mdp_mdp_mutex0",
		"mdp_smi0",
		"mdp_apb_bus",
		"mdp_mdp_rdma0",
		"mdp_mdp_rdma1",
		"mdp_mdp_rdma2",
		"mdp_mdp_birsz0",
		"mdp_mdp_hdr0",
		"mdp_mdp_aal0",
		"mdp_mdp_rsz0",
		"mdp_mdp_rsz2",
		"mdp_mdp_tdshp0",
		"mdp_mdp_color0",
		"mdp_mdp_wrot0",
		"mdp_mdp_wrot1",
		"mdp_mdp_wrot2",
		"mdp_mdp_fake_eng0",
		"mdp_apb_db",
		"mdp_mdp_dli_async0",
		"mdp_mdp_dli_async1",
		"mdp_mdp_dlo_async0",
		"mdp_mdp_dlo_async1",
		"mdp_mdp_dli_async2",
		"mdp_mdp_dlo_async2",
		"mdp_mdp_dlo_async3",
		"mdp_img_dl_async0",
		"mdp_mdp_rrot0",
		"mdp_mdp_merge0",
		"mdp_mdp_c3d0",
		"mdp_mdp_fg0",
		"mdp_mdp_cla2",
		"mdp_mdp_dlo_async4",
		"mdp_vpp_rsz0",
		"mdp_vpp_rsz1",
		"mdp_mdp_dlo_async5",
		"mdp_img0",
		"mdp_f26m",
		"mdp_img_dl_relay0",
		"mdp_img_dl_relay1",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_smi0",
		"mdp1_apb_bus",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_rdma1",
		"mdp1_mdp_rdma2",
		"mdp1_mdp_birsz0",
		"mdp1_mdp_hdr0",
		"mdp1_mdp_aal0",
		"mdp1_mdp_rsz0",
		"mdp1_mdp_rsz2",
		"mdp1_mdp_tdshp0",
		"mdp1_mdp_color0",
		"mdp1_mdp_wrot0",
		"mdp1_mdp_wrot1",
		"mdp1_mdp_wrot2",
		"mdp1_mdp_fake_eng0",
		"mdp1_apb_db",
		"mdp1_mdp_dli_async0",
		"mdp1_mdp_dli_async1",
		"mdp1_mdp_dlo_async0",
		"mdp1_mdp_dlo_async1",
		"mdp1_mdp_dli_async2",
		"mdp1_mdp_dlo_async2",
		"mdp1_mdp_dlo_async3",
		"mdp1_img_dl_async0",
		"mdp1_mdp_rrot0",
		"mdp1_mdp_merge0",
		"mdp1_mdp_c3d0",
		"mdp1_mdp_fg0",
		"mdp1_mdp_cla2",
		"mdp1_mdp_dlo_async4",
		"mdp1_vpp_rsz0",
		"mdp1_vpp_rsz1",
		"mdp1_mdp_dlo_async5",
		"mdp1_img0",
		"mdp1_f26m",
		"mdp1_img_dl_relay0",
		"mdp1_img_dl_relay1",

		/* disp_vdisp_ao_config */
		"mm_v_disp_vdisp_ao_config",
		"mm_v_disp_dpc",
		"mm_v_smi_sub_somm0",

		/* mfgpll_pll_ctrl */
		"mfgpll",

		/* mfgpll_sc0_pll_ctrl */
		"mfgpll-sc0",

		/* mfgpll_sc1_pll_ctrl */
		"mfgpll-sc1",

		/* ccipll_pll_ctrl */
		"ccipll",

		/* armpll_ll_pll_ctrl */
		"armpll-ll",

		/* armpll_bl_pll_ctrl */
		"armpll-bl",

		/* armpll_b_pll_ctrl */
		"armpll-b",

		/* ptppll_pll_ctrl */
		"ptppll",
		NULL

	};

	return clks;
}

/*
 * clkdbg test tasks
 */
static struct task_struct *clkdbg_test_thread[THREAD_NUM];

static void stop_clkdbg_test_task(void)
{
	int i;

	for (i = 0; i < clkdbg_thread_cnt; i++) {
		if (clkdbg_test_thread[clkdbg_thread_cnt]) {
			kthread_stop(clkdbg_test_thread[clkdbg_thread_cnt]);
			clkdbg_test_thread[clkdbg_thread_cnt] = NULL;
		}
	}
}

static int clkdbg_thread_fn(void *data)
{
	struct device *dev[PD_NUM] = {NULL};
	const char *dev_name[PD_NUM] = {
		"dsi_phy2",
		"dsi_phy1",
		"dsi_phy0",
		"csi_ls_rx",
		"csi_bs_rx",
		"mm_infra1",
	};
	int i;
	int ret = 0;
	unsigned int thread_cnt = 0;

	for (i = 0; i < PD_NUM; i++)
		dev[i] = clkdbg_dev_from_name(dev_name[i]);
	while (!kthread_should_stop()) {
		unsigned int pd_idx = 0;

		if ((thread_cnt % 10000) == 0)
			pr_info("clkdbg_thread is running...(%d)\n", thread_cnt);

		pd_idx = get_random_u32() % PD_NUM;
		if (dev[pd_idx] != NULL) {
			ret = pm_runtime_get_sync(dev[pd_idx]);
			if (ret < 0 && ret != -EAGAIN) {
				pr_notice("%s fail to power on(%d)\n", dev_name[pd_idx], ret);
				goto ERR;
			}
			ret = pm_runtime_put_sync(dev[pd_idx]);
			if (ret < 0 && ret != -EAGAIN) {
				pr_notice("%s fail to power off(%d)\n", dev_name[pd_idx], ret);
				goto ERR;
			}
		}

		thread_cnt++;
	}
ERR:
	stop_clkdbg_test_task();
	return 0;
}

static int start_clkdbg_test_task(void)
{
	char thread_name[THREAD_LEN];
	int ret = 0;

	if (clkdbg_thread_cnt >= THREAD_NUM || clkdbg_thread_cnt < 0)
		return 0;

	if (clkdbg_test_thread[clkdbg_thread_cnt]) {
		pr_info("%s clkdbg_thread is already running\n", __func__);
		return -EBUSY;
	}

	ret = snprintf(thread_name, THREAD_LEN, "clkdbg_thread%d", clkdbg_thread_cnt);

	if (ret < 0) {
		pr_info("%s snprintf error(%d)\n", __func__, ret);
		return ret;
	}

	clkdbg_test_thread[clkdbg_thread_cnt] = kthread_run(clkdbg_thread_fn, NULL, "%s", thread_name);
	if (IS_ERR(clkdbg_test_thread[clkdbg_thread_cnt])) {
		pr_info("%s Failed to start clkdbg_thread(%d)\n", __func__, clkdbg_thread_cnt);
		return PTR_ERR(clkdbg_test_thread[clkdbg_thread_cnt]);
	}
	clkdbg_thread_cnt++;

	return 0;
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

static struct clkdbg_ops clkdbg_mt6991_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6991_all_clk_names,
	.start_task = start_clkdbg_test_task,
};

static int clk_dbg_mt6991_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6991_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6991_drv = {
	.probe = clk_dbg_mt6991_probe,
	.driver = {
		.name = "clk-dbg-mt6991",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6991_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6991_drv, "clk-dbg-mt6991");
}

static void __exit clkdbg_mt6991_exit(void)
{
	unset_clkdbg_ops();
	stop_clkdbg_test_task();
	platform_driver_unregister(&clk_dbg_mt6991_drv);
}

subsys_initcall(clkdbg_mt6991_init);
module_exit(clkdbg_mt6991_exit);
MODULE_LICENSE("GPL");
