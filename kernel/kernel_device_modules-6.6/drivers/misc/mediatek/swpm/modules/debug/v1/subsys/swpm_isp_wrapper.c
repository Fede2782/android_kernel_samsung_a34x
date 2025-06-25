// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/types.h>
#include "swpm_isp_wrapper.h"

static struct isp_swpm_cb_func cb_func;

#if IS_ENABLED(CONFIG_MTK_SWPM_ISP)
void set_p1_idx(struct ISP_P1 idx)
{
	if (get_cb_func()->is_registered)
		get_cb_func()->p1_cb(&idx);
}
EXPORT_SYMBOL(set_p1_idx);

void set_p2_idx(struct ISP_P2 idx)
{
	if (get_cb_func()->is_registered)
		get_cb_func()->p2_cb(&idx);
}
EXPORT_SYMBOL(set_p2_idx);

void set_csi_idx(struct CSI idx)
{
	if (get_cb_func()->is_registered)
		get_cb_func()->csi_cb(&idx);
}
EXPORT_SYMBOL(set_csi_idx);

void isp_swpm_register(struct isp_swpm_cb_func *func)
{
	cb_func.p1_cb = func->p1_cb;
	cb_func.p2_cb = func->p2_cb;
	cb_func.csi_cb = func->csi_cb;
	cb_func.is_registered = true;
}
EXPORT_SYMBOL(isp_swpm_register);

void isp_swpm_unregister(void)
{
	cb_func.is_registered = false;
	cb_func.p1_cb = NULL;
	cb_func.p2_cb = NULL;
	cb_func.csi_cb = NULL;
}
EXPORT_SYMBOL(isp_swpm_unregister);
#endif

struct isp_swpm_cb_func *get_cb_func(void)
{
	return &cb_func;
}
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWPM isp wrapper");
MODULE_AUTHOR("MediaTek Inc.");
