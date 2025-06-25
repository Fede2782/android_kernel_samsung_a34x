/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_SMI_DEBUG_H
#define __MTK_SMI_DEBUG_H

#define MAX_MON_REQ	(4)

enum smi_mon_id {
	SMI_BW_MET,
	SMI_BW_BUS,
	SMI_BW_IMGSYS,
};

enum smi_isp_id {
	ISP_TRAW = BIT(0),
	ISP_DIP = BIT(1),
};

struct smi_disp_ops {
	int (*disp_get)(void);
	int (*disp_put)(void);
};

struct smi_hw_sema_ops {
	int (*hw_sema_ctrl)(u32 master_id, bool is_get);
};

enum smi_hw_sema_master {
	SMI_DBG_MASTER_AP = 0,
	SMI_DBG_MASTER_VCP,
};

enum smi_pwr_ctrl_action {
	ACTION_GET_IF_IN_USE,
	ACTION_PUT_IF_IN_USE,
	ACTION_FORCE_ALL_ON,
	ACTION_FORCE_ALL_PUT,
};

struct smi_user_pwr_ctrl_data {
	u32 *id_list;
	u32 id_nr;
	void __iomem *pwr_sta_rg;
	bool skip_smi_chk;
};

#define SMI_CALLER_MAX_LEN 64
struct smi_user_pwr_ctrl {
	const char *name;
	u32 smi_user_id;
	void *data;
	int (*smi_user_get_if_in_use)(void *v);
	int (*smi_user_get)(void *v);
	int (*smi_user_put)(void *v);
	char caller[SMI_CALLER_MAX_LEN];
	struct list_head list;
};

enum SMI_DBG_VER {
	SMI_DBG_VER_1 = 1,
	SMI_DBG_VER_2,
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)

int mtk_smi_set_disp_ops(const struct smi_disp_ops *ops);
int mtk_smi_set_hw_sema_ops(const struct smi_hw_sema_ops *ops);
int mtk_smi_dbg_register_notifier(struct notifier_block *nb);
int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb);
int mtk_smi_dbg_register_force_on_notifier(struct notifier_block *nb);
int mtk_smi_dbg_unregister_force_on_notifier(struct notifier_block *nb);
int smi_larb_force_all_on(char *buf, const struct kernel_param *kp);
int smi_larb_force_all_put(char *buf, const struct kernel_param *kp);
s32 smi_monitor_start(struct device *dev, u32 common_id, u32 commonlarb_id[MAX_MON_REQ],
			u32 flag[MAX_MON_REQ], enum smi_mon_id mon_id);
s32 smi_monitor_stop(struct device *dev, u32 common_id,
			u32 *bw, enum smi_mon_id mon_id);

int mtk_smi_dbg_register_pwr_ctrl_cb(struct smi_user_pwr_ctrl *cb);
int mtk_smi_dbg_unregister_pwr_ctrl_cb(struct smi_user_pwr_ctrl *cb);
#else

static inline int mtk_smi_set_disp_ops(const struct smi_disp_ops *ops)
{
	return 0;
}

static inline int mtk_smi_set_hw_sema_ops(const struct smi_hw_sema_ops *ops)
{
	return 0;
}

static inline int mtk_smi_dbg_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_register_force_on_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_unregister_force_on_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int smi_larb_force_all_on(char *buf, const struct kernel_param *kp);
{
	return 0;
}

static inline int smi_larb_force_all_put(char *buf, const struct kernel_param *kp);
{
	return 0;
}

static inline s32 smi_monitor_start(struct device *dev, u32 common_id,
		u32 commonlarb_id[MAX_MON_REQ], u32 flag[MAX_MON_REQ], enum smi_mon_id mon_id)
{
	return 0;
}

static inline s32 smi_monitor_stop(struct device *dev, u32 common_id,
				u32 *bw, enum smi_mon_id mon_id)
{
	return 0;
}

static inline int mtk_smi_dbg_register_pwr_ctrl_cb(struct smi_user_pwr_ctrl *cb)
{
	return 0;
}

static inline int mtk_smi_dbg_unregister_pwr_ctrl_cb(struct smi_user_pwr_ctrl *cb)
{
	return 0;
}
#endif /* CONFIG_DEVICE_MODULES_MTK_SMI */

#endif /* __MTK_SMI_DEBUG_H */

