/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6877_H
#define __DRV_CLKCHK_MT6877_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	infracfg_ao_bus = 2,
	spm = 3,
	apmixed = 4,
	scp_par = 5,
	audsys = 6,
	msdc0 = 7,
	impc = 8,
	impe = 9,
	imps = 10,
	impws = 11,
	impw = 12,
	impn = 13,
	mfg_ao = 14,
	mfgcfg = 15,
	mm = 16,
	imgsys1 = 17,
	imgsys2 = 18,
	vde2 = 19,
	ven1 = 20,
	apu_conn2 = 21,
	apu_conn1 = 22,
	apuv = 23,
	apu0 = 24,
	apu1 = 25,
	apum0 = 26,
	apu_ao = 27,
	cam_m = 28,
	cam_ra = 29,
	cam_rb = 30,
	ipe = 31,
	mdp = 32,
	chk_sys_num = 33,
};

enum chk_pd_id {
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
	MT6877_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6877(enum chk_sys_id id);
extern u32 get_mt6877_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6877_H */

