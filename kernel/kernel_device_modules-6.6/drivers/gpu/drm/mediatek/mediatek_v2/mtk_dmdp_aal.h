/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DMDP_AAL_H__
#define __MTK_DMDP_AAL_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

void disp_mdp_aal_regdump(struct mtk_ddp_comp *comp);
void disp_mdp_aal_bypass(struct mtk_ddp_comp *comp, int bypass, int caller, struct cmdq_pkt *handle);
void disp_mdp_aal_bypass_flag(struct mtk_ddp_comp *comp, int bypass);
void disp_mdp_aal_init_data_update(struct mtk_ddp_comp *comp, const struct DISP_AAL_INITREG *init_regs);
void disp_mdp_aal_set_valid(struct mtk_ddp_comp *comp, bool valid);
// for displayPQ update to swpm tppa
unsigned int disp_mdp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif
