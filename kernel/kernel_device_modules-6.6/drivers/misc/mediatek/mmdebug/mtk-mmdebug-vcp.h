/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __MMDEBUG_VCP_H
#define __MMDEBUG_VCP_H

#define MMDEBUG_DBG(fmt, args...) \
	pr_notice("[mmdebug][dbg]%s: "fmt"\n", __func__, ##args)
#define MMDEBUG_ERR(fmt, args...) \
	pr_notice("[mmdebug][err]%s: "fmt"\n", __func__, ##args)

/* vcp/.../mmdebug_public.h */
enum MMDEBUG_FUNC {
	MMDEBUG_FUNC_SMI_DUMP,
	MMDEBUG_FUNC_KERNEL_WARN,
	MMDEBUG_FUNC_NUM
};

/* vcp/.../mmdebug_public.h */
const char *kernel_warn_type_str[] = {
	"MMDEBUG-DPSW_CHECK_VMM_OFF",
	"MMDEBUG-DPSW_CHECK_VLOGIC",
	"MMDEBUG-DPSW_TIMEOUT",
	"MMDEBUG-HFLV_CHECK_CLOCK",
	"MMDEBUG-HFLV_CHECK_SINGLE_CLOCK",
	"MMDEBUG-HFLV_CHECK_VOLTAGE",
	"MMDEBUG-HFLV_CHECK_VOLTAGE_BIN",
	"MMDEBUG-POLL_RC_TIMEOUT",
	"MMDEBUG-RPOFILE_TIMEOUT",
};

/* vcp/.../mmdebug_private.h */
struct mmdebug_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t ack;
	uint32_t base;
};

#if IS_ENABLED(CONFIG_MTK_MMDEBUG)
bool mmdebug_is_init_done(void);
int mtk_mmdebug_status_dump_register_notifier(struct notifier_block *nb);
#else
static inline bool mmdebug_is_init_done(void) { return 0; }
static inline int mtk_mmdebug_status_dump_register_notifier(struct notifier_block *nb) { return 0; }
#endif /* CONFIG_MTK_MMDEBUG */

#endif /* __MMDEBUG_VCP_H */
