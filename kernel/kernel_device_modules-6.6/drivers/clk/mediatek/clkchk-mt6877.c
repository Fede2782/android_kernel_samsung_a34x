// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6877-power.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6877.h"
// #include "clk-fmeter.h"
// #include "clk-mt6877-fmeter.h"

#define TAG			"[clkchk] "

#define	BUG_ON_CHK_ENABLE	0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		0

static unsigned int suspend_cnt;

/*
 * clkchk vf table
 */

#if CHECK_VCORE_FREQ
struct mtk_vf {
	const char *name;
	int freq_table[5];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4},	\
	}

/*
 * Opp0 : 0p75v
 * Opp1 : 0p725v
 * Opp2 : 0p65v
 * Opp3 : 0p60v
 * Opp4 : 0p55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 624000, 624000, 416000, 364000, 312000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp0_sel", 546000, 546000, 416000, 312000, 208000),
	MTK_VF_TABLE("mdp0_sel", 594000, 594000, 436800, 343750, 275000),
	MTK_VF_TABLE("img1_sel", 624000, 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("ipe_sel", 546000, 546000, 416000, 312000, 275000),
	MTK_VF_TABLE("dpe_sel", 546000, 546000, 458333, 364000, 249600),
	MTK_VF_TABLE("cam_sel", 624000, 624000, 546000, 392857, 297000),
	MTK_VF_TABLE("ccu_sel", 499200, 499200, 392857, 364000, 275000),
	MTK_VF_TABLE("dsp_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("dsp1_sel", 624000, 624000, 624000, 624000, 624000),
	MTK_VF_TABLE("dsp2_sel", 624000, 624000, 624000, 624000, 624000),
	MTK_VF_TABLE("dsp4_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("dsp7_sel", 687500, 687500, 687500, 687500, 687500),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 312000, 312000, 273000, 242667, 218400),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 750000, 750000, 750000, 750000, 750000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 624000, 458333, 343750, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("spmi_m_mst_sel", 39000, 39000, 39000, 39000, 39000),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("mem_sub_sel", 436800, 436800, 364000, 273000, 182000),
	MTK_VF_TABLE("aes_msdcfde_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_mbist_sel", 297000, 297000, 297000, 297000, 297000),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("msdc_new_rx_sel", 384000, 384000, 384000, 384000, 384000),
	{},
};
#endif

static const char *get_vf_name(int id)
{
#if CHECK_VCORE_FREQ
	if (id < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return NULL;
	}

	return vf_table[id].name;
#else
	return NULL;
#endif
}

static int get_vf_opp(int id, int opp)
{
#if CHECK_VCORE_FREQ
	if (id < 0 || opp < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return 0;
	}

	if (id >= ARRAY_SIZE(vf_table) || opp >= 5) {
		pr_err("[%s] invalid id:%d or opp:%d\n", __func__, id, opp);
		return 0;
	}

	return vf_table[id].freq_table[opp];
#else
	return 0;
#endif
}

static u32 get_vf_num(void)
{
#if CHECK_VCORE_FREQ
	return ARRAY_SIZE(vf_table) - 1;
#else
	return 0;
#endif
}

static int get_vcore_opp(void)
{
	int opp;
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	opp = get_sw_req_vcore_opp();

#if defined(CONFIG_MTK_DVFSRC_MT6877_PRETEST)
	if (opp >= 1)
		opp = opp - 1;
#endif

#else
	opp =  VCORE_NULL;
#endif
	return opp;
}

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[infracfg_ao_bus] = REGBASE_V(0x10001000, infracfg_ao_bus, PD_NULL, CLK_NULL),
	[spm] = REGBASE_V(0x10006000, spm, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[scp_par] = REGBASE_V(0x10720000, scp_par, PD_NULL, CLK_NULL),
	[audsys] = REGBASE_V(0x11210000, audsys, MT6877_CHK_PD_AUDIO, CLK_NULL),
	[msdc0] = REGBASE_V(0x11230000, msdc0, PD_NULL, CLK_NULL),
	[impc] = REGBASE_V(0x11282000, impc, PD_NULL,"fi2c_pseudo_ck"),
	[impe] = REGBASE_V(0x11cb1000, impe, PD_NULL,"fi2c_pseudo_ck"),
	[imps] = REGBASE_V(0x11d04000, imps, PD_NULL,"fi2c_pseudo_ck"),
	[impws] = REGBASE_V(0x11d23000, impws, PD_NULL,"fi2c_pseudo_ck"),
	[impw] = REGBASE_V(0x11e01000, impw, PD_NULL,"fi2c_pseudo_ck"),
	[impn] = REGBASE_V(0x11f01000, impn, PD_NULL,"fi2c_pseudo_ck"),
	// [mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, "PG_MFG5", CLK_NULL),
	// [mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, "PG_MFG5", CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6877_CHK_PD_DISP, CLK_NULL),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6877_CHK_PD_ISP0, CLK_NULL),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, MT6877_CHK_PD_ISP1, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6877_CHK_PD_VDEC, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6877_CHK_PD_VENC, CLK_NULL),
	// [apu_conn2] = REGBASE_V(0x19020000, apu_conn2, "PG_APU", CLK_NULL),
	// [apu_conn1] = REGBASE_V(0x19024000, apu_conn1, "PG_APU", CLK_NULL),
	// [apuv] = REGBASE_V(0x19029000, apuv, "PG_APU", CLK_NULL),
	// [apu0] = REGBASE_V(0x19030000, apu0, "PG_APU", CLK_NULL),
	// [apu1] = REGBASE_V(0x19031000, apu1, "PG_APU", CLK_NULL),
	// [apum0] = REGBASE_V(0x19034000, apum0, "PG_APU", CLK_NULL),
	[apu_ao] = REGBASE_V(0x190f3000, apu_ao, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6877_CHK_PD_CAM, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6877_CHK_PD_CAM_RAWA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6877_CHK_PD_CAM_RAWB, CLK_NULL),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6877_CHK_PD_IPE, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6877_CHK_PD_DISP, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top,  0x0010, CLK_CFG_0),
	REGNAME(top,  0x0020, CLK_CFG_1),
	REGNAME(top,  0x0030, CLK_CFG_2),
	REGNAME(top,  0x0040, CLK_CFG_3),
	REGNAME(top,  0x0050, CLK_CFG_4),
	REGNAME(top,  0x0060, CLK_CFG_5),
	REGNAME(top,  0x0070, CLK_CFG_6),
	REGNAME(top,  0x0080, CLK_CFG_7),
	REGNAME(top,  0x0090, CLK_CFG_8),
	REGNAME(top,  0x00A0, CLK_CFG_9),
	REGNAME(top,  0x00B0, CLK_CFG_10),
	REGNAME(top,  0x00C0, CLK_CFG_11),
	REGNAME(top,  0x00D0, CLK_CFG_12),
	REGNAME(top,  0x00E0, CLK_CFG_13),
	REGNAME(top,  0x00F0, CLK_CFG_14),
	REGNAME(top,  0x0100, CLK_CFG_15),
	REGNAME(top,  0x0110, CLK_CFG_16),
	REGNAME(top,  0x0120, CLK_CFG_20),
	REGNAME(top,  0x0180, CLK_CFG_17),
	REGNAME(top,  0x0320, CLK_AUDDIV_0),
	REGNAME(top,  0x0328, CLK_AUDDIV_2),
	REGNAME(top,  0x0334, CLK_AUDDIV_3),
	REGNAME(top,  0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao,  0x90, MODULE_SW_CG_0),
	REGNAME(ifrao,  0x94, MODULE_SW_CG_1),
	REGNAME(ifrao,  0xac, MODULE_SW_CG_2),
	REGNAME(ifrao,  0xc8, MODULE_SW_CG_3),
	REGNAME(ifrao,  0xe8, MODULE_SW_CG_4),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg_ao_bus,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_ao_bus,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_ao_bus,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_ao_bus,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_ao_bus,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_ao_bus,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_ao_bus,  0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg_ao_bus,  0x0B8C, INFRA_TOPAXI_PROTECTEN_VDNR_STA0),
	REGNAME(infracfg_ao_bus,  0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg_ao_bus,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_ao_bus,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_ao_bus,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_ao_bus,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_ao_bus,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_ao_bus,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	/* SPM register */
	REGNAME(spm,  0xE80, MFG0_PWR_CON),
	REGNAME(spm,  0xEF8, XPU_PWR_STATUS),
	REGNAME(spm,  0xEFC, XPU_PWR_STATUS_2ND),
	REGNAME(spm,  0xE84, MFG1_PWR_CON),
	REGNAME(spm,  0xE88, MFG2_PWR_CON),
	REGNAME(spm,  0xE8C, MFG3_PWR_CON),
	REGNAME(spm,  0xE90, MFG4_PWR_CON),
	REGNAME(spm,  0xE94, MFG5_PWR_CON),
	REGNAME(spm,  0xE00, MD1_PWR_CON),
	REGNAME(spm,  0xEF0, PWR_STATUS),
	REGNAME(spm,  0xEF4, PWR_STATUS_2ND),
	REGNAME(spm,  0xEE8, MD_BUCK_ISO_CON),
	REGNAME(spm,  0xE04, CONN_PWR_CON),
	REGNAME(spm,  0xE24, ISP0_PWR_CON),
	REGNAME(spm,  0xE28, ISP1_PWR_CON),
	REGNAME(spm,  0xE2C, IPE_PWR_CON),
	REGNAME(spm,  0xE30, VDE0_PWR_CON),
	REGNAME(spm,  0xE38, VEN_PWR_CON),
	REGNAME(spm,  0xE48, DIS0_PWR_CON),
	REGNAME(spm,  0xE54, AUDIO_PWR_CON),
	REGNAME(spm,  0xE58, ADSP_PWR_CON),
	REGNAME(spm,  0xEEC, SOC_BUCK_ISO_CON),
	REGNAME(spm,  0xE5C, CAM_PWR_CON),
	REGNAME(spm,  0xE60, CAM_RAWA_PWR_CON),
	REGNAME(spm,  0xE64, CAM_RAWB_PWR_CON),
	REGNAME(spm,  0xE78, CSI_PWR_CON),
	REGNAME(spm,  0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(spm,  0x178, OTHER_PWR_STATUS),
	/* APMIXEDSYS register */
	REGNAME(apmixed,  0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed,  0x20c, ARMPLL_LL_CON1),
	REGNAME(apmixed,  0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed,  0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed,  0x218, ARMPLL_BL_CON0),
	REGNAME(apmixed,  0x21c, ARMPLL_BL_CON1),
	REGNAME(apmixed,  0x220, ARMPLL_BL_CON2),
	REGNAME(apmixed,  0x224, ARMPLL_BL_CON3),
	REGNAME(apmixed,  0x238, CCIPLL_CON0),
	REGNAME(apmixed,  0x23c, CCIPLL_CON1),
	REGNAME(apmixed,  0x240, CCIPLL_CON2),
	REGNAME(apmixed,  0x244, CCIPLL_CON3),
	REGNAME(apmixed,  0x350, MAINPLL_CON0),
	REGNAME(apmixed,  0x354, MAINPLL_CON1),
	REGNAME(apmixed,  0x358, MAINPLL_CON2),
	REGNAME(apmixed,  0x35c, MAINPLL_CON3),
	REGNAME(apmixed,  0x308, UNIVPLL_CON0),
	REGNAME(apmixed,  0x30c, UNIVPLL_CON1),
	REGNAME(apmixed,  0x310, UNIVPLL_CON2),
	REGNAME(apmixed,  0x314, UNIVPLL_CON3),
	REGNAME(apmixed,  0x360, MSDCPLL_CON0),
	REGNAME(apmixed,  0x364, MSDCPLL_CON1),
	REGNAME(apmixed,  0x368, MSDCPLL_CON2),
	REGNAME(apmixed,  0x36c, MSDCPLL_CON3),
	REGNAME(apmixed,  0x3a0, MMPLL_CON0),
	REGNAME(apmixed,  0x3a4, MMPLL_CON1),
	REGNAME(apmixed,  0x3a8, MMPLL_CON2),
	REGNAME(apmixed,  0x3ac, MMPLL_CON3),
	REGNAME(apmixed,  0x380, ADSPPLL_CON0),
	REGNAME(apmixed,  0x384, ADSPPLL_CON1),
	REGNAME(apmixed,  0x388, ADSPPLL_CON2),
	REGNAME(apmixed,  0x38c, ADSPPLL_CON3),
	REGNAME(apmixed,  0x248, TVDPLL_CON0),
	REGNAME(apmixed,  0x24c, TVDPLL_CON1),
	REGNAME(apmixed,  0x250, TVDPLL_CON2),
	REGNAME(apmixed,  0x254, TVDPLL_CON3),
	REGNAME(apmixed,  0x328, APLL1_CON0),
	REGNAME(apmixed,  0x32c, APLL1_CON1),
	REGNAME(apmixed,  0x330, APLL1_CON2),
	REGNAME(apmixed,  0x334, APLL1_CON3),
	REGNAME(apmixed,  0x338, APLL1_CON4),
	REGNAME(apmixed,  0x33c, APLL2_CON0),
	REGNAME(apmixed,  0x340, APLL2_CON1),
	REGNAME(apmixed,  0x344, APLL2_CON2),
	REGNAME(apmixed,  0x348, APLL2_CON3),
	REGNAME(apmixed,  0x34c, APLL2_CON4),
	REGNAME(apmixed,  0x390, MPLL_CON0),
	REGNAME(apmixed,  0x394, MPLL_CON1),
	REGNAME(apmixed,  0x398, MPLL_CON2),
	REGNAME(apmixed,  0x39c, MPLL_CON3),
	REGNAME(apmixed,  0x318, USBPLL_CON0),
	REGNAME(apmixed,  0x31c, USBPLL_CON1),
	REGNAME(apmixed,  0x320, USBPLL_CON2),
	REGNAME(apmixed,  0x324, USBPLL_CON3),
	/* SCP_PAR_TOP register */
	REGNAME(scp_par,  0x180, AUDIODSP_CK_CG),
	/* AUDIO register */
	REGNAME(audsys,  0x0, AUDIO_TOP_0),
	REGNAME(audsys,  0x4, AUDIO_TOP_1),
	/* MSDC0 register */
	REGNAME(msdc0,  0x68, MSDC_NEW_RX_CFG),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc,  0xe00, AP_CLOCK_CG_CEN),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe,  0xe00, AP_CLOCK_CG_EST),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps,  0xe00, AP_CLOCK_CG_SOU),
	/* IMP_IIC_WRAP_WS register */
	REGNAME(impws,  0xe00, AP_CLOCK_CG_WEST_SOU),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw,  0xe00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_N register */
	REGNAME(impn,  0xe00, AP_CLOCK_CG_NOR),
	/* GPU_PLL_CTRL register */
	REGNAME(mfg_ao,  0x8, MFGPLL1_CON0),
	REGNAME(mfg_ao,  0xc, MFGPLL1_CON1),
	REGNAME(mfg_ao,  0x10, MFGPLL1_CON2),
	REGNAME(mfg_ao,  0x14, MFGPLL1_CON3),
	REGNAME(mfg_ao,  0x38, MFGPLL4_CON0),
	REGNAME(mfg_ao,  0x3c, MFGPLL4_CON1),
	REGNAME(mfg_ao,  0x40, MFGPLL4_CON2),
	REGNAME(mfg_ao,  0x44, MFGPLL4_CON3),
	/* MFGCFG register */
	REGNAME(mfgcfg,  0x0, MFG_CG),
	/* MMSYS_CONFIG register */
	REGNAME(mm,  0x100, MMSYS_CG_0),
	REGNAME(mm,  0x1a0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1,  0x0, IMG_CG),
	/* IMGSYS2 register */
	REGNAME(imgsys2,  0x0, IMG_CG),
	/* VDEC_GCON register */
	REGNAME(vde2,  0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1,  0x0, VENCSYS_CG),
	/* APU_CONN2 register */
	REGNAME(apu_conn2,  0x0, APU_CONN_CG),
	/* APU_CONN1 register */
	REGNAME(apu_conn1,  0x0, APU_CONN1_CG),
	/* APUSYS_VCORE register */
	REGNAME(apuv,  0x0, APUSYS_VCORE_CG),
	/* APU0 register */
	REGNAME(apu0,  0x100, CORE_CG),
	/* APU1 register */
	REGNAME(apu1,  0x100, CORE_CG),
	/* APU_MDLA0 register */
	REGNAME(apum0,  0x0, MDLA_CG),
	/* APU_PLL_CTRL register */
	REGNAME(apu_ao,  0x8, APUPLL_CON0),
	REGNAME(apu_ao,  0xc, APUPLL_CON1),
	REGNAME(apu_ao,  0x10, APUPLL_CON2),
	REGNAME(apu_ao,  0x14, APUPLL_CON3),
	REGNAME(apu_ao,  0x18, NPUPLL_CON0),
	REGNAME(apu_ao,  0x1c, NPUPLL_CON1),
	REGNAME(apu_ao,  0x20, NPUPLL_CON2),
	REGNAME(apu_ao,  0x24, NPUPLL_CON3),
	REGNAME(apu_ao,  0x28, APUPLL1_CON0),
	REGNAME(apu_ao,  0x2c, APUPLL1_CON1),
	REGNAME(apu_ao,  0x30, APUPLL1_CON2),
	REGNAME(apu_ao,  0x34, APUPLL1_CON3),
	REGNAME(apu_ao,  0x38, APUPLL2_CON0),
	REGNAME(apu_ao,  0x3c, APUPLL2_CON1),
	REGNAME(apu_ao,  0x40, APUPLL2_CON2),
	REGNAME(apu_ao,  0x44, APUPLL2_CON3),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb,  0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe,  0x0, IMG_CG),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp,  0x100, MDPSYS_CG_0),
	REGNAME(mdp,  0x120, MDPSYS_CG_2),
	{},
};

static const struct regname *get_all_reg_names_mt6877(void)
{
	return rn;
}

void print_subsys_reg_mt6877(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	if (id >= chk_sys_num || id < 0) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL(print_subsys_reg_mt6877);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6877(spm);
	print_subsys_reg_mt6877(top);
	print_subsys_reg_mt6877(ifrao);
	print_subsys_reg_mt6877(infracfg_ao_bus);
	print_subsys_reg_mt6877(apmixed);
}


static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};
#endif

u32 get_mt6877_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6877_reg_value);

static void  init_regbase(void)
{
	size_t i;



	for (i = 0; i < ARRAY_SIZE(rb); i++) {
		if (!rb[i].phys)
			continue;

		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
	}
}

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	"usbpll",
	"mfg_ao_mfgpll1",
	"mfg_ao_mfgpll4",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const *get_off_pll_names(void)
{
	return off_pll_names;
}

static const char * const *get_notice_pll_names(void)
{
	return notice_pll_names;
}

/*
 * clkdbg pwr_status
 */

static u32 pwr_ofs[STA_NUM] = {
	[PWR_STA] = 0x0EF0,
	[PWR_STA2] = 0x0EF4,
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (pwr_ofs[i]) {
			pwr_addr[i] = rb[spm].virt + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}

/*
 * clkchk pwr_msk  need to review
 */
static struct pvd_msk pvd_pwr_mask[] = {
	{"topckgen_clk", PWR_STA, 0x00000000},
	{"infracfg_ao_clk", PWR_STA, 0x00000000},
	{"apmixedsys_clk", PWR_STA, 0x00000000},
	{"mdpsys_config_clk", PWR_STA, BIT(18)},
	{"ipesys_clk", PWR_STA, BIT(11)},
	{"vdec_gcon_clk", PWR_STA, BIT(12)},
	{"venc_gcon_clk", PWR_STA, BIT(14)},
	{"imgsys1_clk", PWR_STA, BIT(9)},
	{"imgsys2_clk", PWR_STA, BIT(10)},
	{"audio_clk", PWR_STA, BIT(21)},
	{"scp_par_top_clk", PWR_STA, BIT(22)},
	{"camsys_main_clk", PWR_STA, BIT(23)},
	{"camsys_rawa_clk", PWR_STA, BIT(24)},
	{"camsys_rawb_clk", PWR_STA, BIT(25)},
	{"imp_iic_wrap_c_clk", PWR_STA, 0x00000000},
	{"imp_iic_wrap_e_clk", PWR_STA, 0x00000000},
	{"imp_iic_wrap_s_clk", PWR_STA, 0x00000000},
	{"imp_iic_wrap_ws_clk", PWR_STA, 0x00000000},
	{"imp_iic_wrap_w_clk", PWR_STA, 0x00000000},
	{"imp_iic_wrap_n_clk", PWR_STA, 0x00000000},
	{"mfgcfg_clk", PWR_STA, 0x00000000},
	{"mmsys_config_clk", PWR_STA, BIT(18)},
	{},
};

static struct pvd_msk *get_pvd_pwr_mask(void)
{
	return pvd_pwr_mask;
}

static bool is_pll_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static bool is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static struct clkchk_ops clkchk_mt6877_ops = {
	.get_all_regnames = get_all_reg_names_mt6877,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	.devapc_dump = devapc_dump,
#endif
	.is_suspend_retry_stop = is_suspend_retry_stop,
};


static int clk_chk_mt6877_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6877_ops);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#if CHECK_VCORE_FREQ
	mtk_clk_check_muxes();
#endif

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6877[] = {
	{
		.compatible = "mediatek,mt6877-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6877_drv = {
	.probe = clk_chk_mt6877_probe,
	.driver = {
		.name = "clk-chk-mt6877",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6877,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6877_init(void)
{
	return platform_driver_register(&clk_chk_mt6877_drv);
}

static void __exit clkchk_mt6877_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6877_drv);
}

late_initcall(clkchk_mt6877_init);
module_exit(clkchk_mt6877_exit);
MODULE_LICENSE("GPL");
