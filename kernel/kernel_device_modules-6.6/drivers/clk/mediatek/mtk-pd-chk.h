/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_PD_CHK_H
#define __MTK_PD_CHK_H

#include <linux/pm_domain.h>
#include "clkchk.h"
#include "clk-mtk.h"

#define PD_PWR_ON	1
#define PD_PWR_OFF	0
#define SWCG(_name) {						\
		.name = _name,					\
	}

/*
 *	Before MTCMOS off procedure, perform the Subsys CGs sanity check.
 */
struct pd_check_swcg {
	struct clk *c;
	const char *name;
};

struct pd_sta {
	int pd_id;
	u32 base;
	u32 ofs;
	u32 msk;
};

struct pdchk_ops {
	struct pd_check_swcg *(*get_subsys_cg)(unsigned int id);
	void (*dump_subsys_reg)(unsigned int pd_id);
	bool (*is_in_pd_list)(unsigned int id);
	void (*debug_dump)(unsigned int pd_id, unsigned int pwr_sta);
	void (*log_dump)(unsigned int pd_id, unsigned int pwr_sta);
	void (*external_dump)(void);
	struct pd_msk *(*get_pd_pwr_msk)(int pd_id);
	u32 (*get_pd_pwr_status)(int pd_id);
	int (*get_pd_pwr_idx)(int pd_id);
	int *(*get_off_mtcmos_id)(void);
	int *(*get_notice_mtcmos_id)(void);
	bool (*is_mtcmos_chk_bug_on)(void);
	int *(*get_suspend_allow_id)(void);
	void (*trace_power_event)(unsigned int pd_id, unsigned int pwr_sta);
	void (*dump_power_event)(void);
	void (*check_hwv_irq_sta)(void);
	void (*check_mm_hwv_irq_sta)(void);
	bool (*is_suspend_retry_stop)(bool reset_cnt);
	const char *(*get_pd_name)(int idx);
	bool (*get_mtcmos_sw_state)(struct generic_pm_domain *pd);
	void (*verify_debug_flow)(struct clk_event_data *clkd);
};

void pdchk_common_init(const struct pdchk_ops *ops);
void pdchk_hwv_irq_init(struct platform_device *pdev);
int set_pdchk_notify(void);

extern const struct dev_pm_ops pdchk_dev_pm_ops;
extern struct clk *clk_chk_lookup(const char *name);
extern int pwr_hw_is_on(enum PWR_STA_TYPE type, s32 mask);
extern void pdchk_debug_dump(void);
extern struct generic_pm_domain **pdchk_get_all_genpd(void);
extern const char *pdchk_get_pd_name(int idx);
#endif /* __MTK_PD_CHK_H */
