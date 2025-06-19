// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6877-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6877.h"
#include "clk-fmeter.h"
#include "clk-mt6877-fmeter.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0

static unsigned int suspend_cnt;

/*
 * The clk names in Mediatek CCF.
 */
/* audio */
struct pd_check_swcg audio_swcgs[] = {
	SWCG("aud_afe"),
	SWCG("aud_22m"),
	SWCG("aud_24m"),
	SWCG("aud_apll2_tuner"),
	SWCG("aud_apll_tuner"),
	SWCG("aud_tdm_ck"),
	SWCG("aud_adc"),
	SWCG("aud_dac"),
	SWCG("aud_dac_predis"),
	SWCG("aud_tml"),
	SWCG("aud_nle"),
	SWCG("aud_connsys_i2s_asrc"),
	SWCG("aud_general1_asrc"),
	SWCG("aud_general2_asrc"),
	SWCG("aud_dac_hires"),
	SWCG("aud_adc_hires"),
	SWCG("aud_adc_hires_tml"),
	SWCG("aud_adda6_adc"),
	SWCG("aud_adda6_adc_hires"),
	SWCG("aud_3rd_dac"),
	SWCG("aud_3rd_dac_predis"),
	SWCG("aud_3rd_dac_tml"),
	SWCG("aud_3rd_dac_hires"),
	SWCG(NULL),
};

/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv0"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
	SWCG("cam_m_ccu_gals"),
	SWCG("cam_m_cam2mm_gals"),
	SWCG("cam_m_camsv4"),
	SWCG("cam_m_pda"),
	SWCG(NULL),
};

/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};

/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};

/* imgsys1 */
struct pd_check_swcg imgsys1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};

/* imgsys2 */
struct pd_check_swcg imgsys2_swcgs[] = {
	SWCG("imgsys2_larb9"),
	SWCG("imgsys2_larb10"),
	SWCG("imgsys2_mfb"),
	SWCG("imgsys2_wpe"),
	SWCG("imgsys2_mss"),
	SWCG("imgsys2_gals"),
	SWCG(NULL),
};

/* ipesys */
struct pd_check_swcg ipesys_swcgs[] = {
	SWCG("ipe_larb19"),
	SWCG("ipe_larb20"),
	SWCG("ipe_smi_subcom"),
	SWCG("ipe_fd"),
	SWCG("ipe_fe"),
	SWCG("ipe_rsc"),
	SWCG("ipe_dpe"),
	SWCG("ipe_gals"),
	SWCG(NULL),
};

/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_rdma0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_img_dl_async0"),
	SWCG("mdp_img_dl_async1"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_smi0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_mutex0"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_color0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_img_dl_rel0_as0"),
	SWCG("mdp_img_dl_rel1_as1"),
	SWCG(NULL),
};

/* mmsys_config */
struct pd_check_swcg mmsys_config_swcgs[] = {
	SWCG("mm_disp_mutex0"),
	SWCG("mm_apb_bus"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_smi_infra"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_spr0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_smi_common"),
	SWCG("mm_disp_cm0"),
	SWCG("mm_dsi0"),
	SWCG("mm_smi_gals"),
	SWCG("mm_disp_dsc_wrap"),
	SWCG("mm_smi_iommu"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ufbc_wdma0"),
	SWCG("mm_dsi0_dsi_domain"),
	SWCG("mm_disp_26m_ck"),
	SWCG(NULL),
};
/* vdec_gcon */
struct pd_check_swcg vdec_gcon_swcgs[] = {
	SWCG("vde2_vdec_cken"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke5_gals"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	int pd_parent;			/* power domain parent id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6877_CHK_PD_AUDIO, PD_NULL, audio_swcgs, audsys},
	{MT6877_CHK_PD_CAM, MT6877_CHK_PD_DISP, camsys_main_swcgs, cam_m},
	{MT6877_CHK_PD_CAM_RAWA, MT6877_CHK_PD_CAM, camsys_rawa_swcgs, cam_ra},
	{MT6877_CHK_PD_CAM_RAWB, MT6877_CHK_PD_CAM, camsys_rawb_swcgs, cam_rb},
	{MT6877_CHK_PD_ISP0, MT6877_CHK_PD_DISP, imgsys1_swcgs, imgsys1},
	{MT6877_CHK_PD_ISP1, MT6877_CHK_PD_DISP, imgsys2_swcgs, imgsys2},
	{MT6877_CHK_PD_IPE, MT6877_CHK_PD_DISP, ipesys_swcgs, ipe},
	{MT6877_CHK_PD_DISP, PD_NULL, mdpsys_config_swcgs, mdp},
	{MT6877_CHK_PD_DISP, PD_NULL, mmsys_config_swcgs, mm},
	{MT6877_CHK_PD_VDEC, MT6877_CHK_PD_DISP, vdec_gcon_swcgs, vde2},
	{MT6877_CHK_PD_VENC, MT6877_CHK_PD_DISP, venc_gcon_swcgs, ven1},
	{},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6877_CHK_PD_NUM)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6877_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6877(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6877_CHK_PD_CONN,
	MT6877_CHK_PD_ISP0,
	MT6877_CHK_PD_ISP1,
	MT6877_CHK_PD_IPE,
	MT6877_CHK_PD_VDEC,
	MT6877_CHK_PD_VENC,
	MT6877_CHK_PD_DISP,
	MT6877_CHK_PD_AUDIO,
	MT6877_CHK_PD_APU,
	MT6877_CHK_PD_CAM,
	MT6877_CHK_PD_CAM_RAWA,
	MT6877_CHK_PD_CAM_RAWB,
	MT6877_CHK_PD_CSI,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6877_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i;

	print_subsys_reg_mt6877(top);
	print_subsys_reg_mt6877(infracfg_ao_bus);
	print_subsys_reg_mt6877(ifrao);
	print_subsys_reg_mt6877(spm);
	print_subsys_reg_mt6877(apmixed);
	if (id >= MT6877_CHK_PD_NUM)
		return;
	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6877(mtk_subsys_check[i].chk_id);
		}
	}
	BUG_ON(1);
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6877_CHK_PD_CONN, spm, 0x0EF0, BIT(1)},
	{MT6877_CHK_PD_ISP0, spm, 0x0EF0, BIT(9)},
	{MT6877_CHK_PD_ISP1, spm, 0x0EF0, BIT(10)},
	{MT6877_CHK_PD_IPE, spm, 0x0EF0, BIT(11)},
	{MT6877_CHK_PD_VDEC, spm, 0x0EF0, BIT(12)},
	{MT6877_CHK_PD_VENC, spm, 0x0EF0, BIT(14)},
	{MT6877_CHK_PD_DISP, spm, 0x0EF0, BIT(18)},
	{MT6877_CHK_PD_AUDIO, spm, 0x0EF0, BIT(21)},
	{MT6877_CHK_PD_APU, spm, 0x0178, BIT(5)},
	{MT6877_CHK_PD_CAM, spm, 0x0EF0, BIT(23)},
	{MT6877_CHK_PD_CAM_RAWA, spm, 0x0EF0, BIT(24)},
	{MT6877_CHK_PD_CAM_RAWB, spm, 0x0EF0, BIT(25)},
	{MT6877_CHK_PD_CSI, spm, 0x0EF0, BIT(30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6877_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6877_CHK_PD_ISP0,
	MT6877_CHK_PD_ISP1,
	MT6877_CHK_PD_IPE,
	MT6877_CHK_PD_VDEC,
	MT6877_CHK_PD_VENC,
	MT6877_CHK_PD_DISP,
	MT6877_CHK_PD_APU,
	MT6877_CHK_PD_CAM,
	MT6877_CHK_PD_CAM_RAWA,
	MT6877_CHK_PD_CAM_RAWB,
	MT6877_CHK_PD_CSI,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6877_CHK_PD_CONN,
	MT6877_CHK_PD_AUDIO,
	PD_NULL,
};

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static bool pdchk_is_suspend_retry_stop(bool reset_cnt)
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

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6877_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.get_pd_pwr_status = get_pd_pwr_status,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.is_suspend_retry_stop = pdchk_is_suspend_retry_stop,
};

static int pd_chk_mt6877_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;
	pdchk_common_init(&pdchk_mt6877_ops);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6877[] = {
	{
		.compatible = "mediatek,mt6877-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6877_drv = {
	.probe = pd_chk_mt6877_probe,
	.driver = {
		.name = "pd-chk-mt6877",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6877,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6877_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6877_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
