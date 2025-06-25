/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef MTK_I3C_MASTER_API_H
#define MTK_I3C_MASTER_API_H

int mtk_i3c_master_entasx(struct i3c_master_controller *master,
		u8 addr, u8 entas_mode);
int mtk_i3c_master_rstdaa(struct i3c_master_controller *master,
			u8 addr);
int mtk_i3c_master_setmwl(struct i3c_master_controller *master,
			struct i3c_device_info *info, u8 addr, struct i3c_ccc_mwl *mwl);
int mtk_i3c_master_setmrl(struct i3c_master_controller *master,
			struct i3c_device_info *info, u8 addr, struct i3c_ccc_mrl *mrl);
int mtk_i3c_master_enttm(struct i3c_master_controller *master,
			struct i3c_ccc_enttm *enttm);
int mtk_i3c_master_setdasa(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 static_addr, u8 dyn_addr);
int mtk_i3c_master_setnewda(struct i3c_master_controller *master,
		struct i3c_device_info *info, u8 oldaddr, u8 newaddr);
int mtk_i3c_master_getmwl(struct i3c_master_controller *master,
				    struct i3c_device_info *info);
int mtk_i3c_master_getmrl(struct i3c_master_controller *master,
				    struct i3c_device_info *info);
int mtk_i3c_master_getpid(struct i3c_master_controller *master,
				    struct i3c_device_info *info);
int mtk_i3c_master_getbcr(struct i3c_master_controller *master,
				    struct i3c_device_info *info);
int mtk_i3c_master_getdcr(struct i3c_master_controller *master,
				    struct i3c_device_info *info);
int mtk_i3c_master_getmxds(struct i3c_master_controller *master,
				     struct i3c_device_info *info);
int mtk_i3c_master_gethdrcap(struct i3c_master_controller *master,
				       struct i3c_device_info *info);
int mtk_i3c_master_change_i3c_speed(struct i3c_master_controller *master,
		unsigned int scl_rate_i3c);
struct list_head *get_mtk_i3c_list(void);

#endif /* MTK_I3C_MASTER_API_H */
