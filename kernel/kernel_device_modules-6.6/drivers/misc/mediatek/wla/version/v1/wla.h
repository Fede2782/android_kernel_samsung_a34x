/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __WLA_H__
#define __WLA_H__

#include <linux/bitfield.h>
#include <linux/types.h>
#include "memsys_wlapm_reg.h"

#define WLA_SUCCESS			(0)
#define WLA_FAIL			(-1)

#define reg_write_field(addr, val, msk)		writel((readl(addr) & (~(msk))) | (FIELD_PREP(msk, val)),addr)
#define reg_write_raw(addr, val, msk)		writel((readl(addr) & (~(msk))) | ((val) & (msk)),addr)
#define reg_read_field(addr, msk)			FIELD_GET(msk, readl(addr))
#define reg_read_raw(addr, msk)				(readl(addr) & (msk))

#define wla_write_field(offset, val, msk) \
{ \
	reg_write_field(wla_north_base+offset, val, msk); \
	reg_write_field(wla_south_base+offset, val, msk); \
}
#define wla_write_raw(offset, val, msk) \
{ \
	reg_write_raw(wla_north_base+offset, val, msk); \
	reg_write_raw(wla_south_base+offset, val, msk); \
}

#define wla_north_read_field(offset, msk)	(reg_read_field(wla_north_base+offset, msk))
#define wla_south_read_field(offset, msk)	(reg_read_field(wla_south_base+offset, msk))
#define wla_north_read_raw(offset, msk)		(reg_read_raw(wla_north_base+offset, msk))
#define wla_south_read_raw(offset, msk)		(reg_read_raw(wla_south_base+offset, msk))

#define WLA_MON_CH_BUF		(16)
#define WLA_MON_STATUS_BUF	(6)
struct wla_mon_ch_setting {
	unsigned int sig_sel;
	unsigned int bit_sel;
	unsigned int trig_type;
};

struct wla_mon_status {
	unsigned int mux;
};

struct wla_monitor {
	struct wla_mon_ch_setting ch[WLA_MON_CH_BUF];
	/* status[0]: reserved
	 * status[1]: status1, status[2]: status2, status[3]: status3
	 */
	struct wla_mon_status status[WLA_MON_STATUS_BUF];
	unsigned int ch_num;
	unsigned int ch_hw_max;
	unsigned int sta_num;
	unsigned int sta_hw_max;
};

/* For E1/E2/... chip discrimination */
struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

extern void __iomem *wla_north_base;
extern void __iomem *wla_south_base;

void wla_set_enable(unsigned int enable);
unsigned int wla_get_enable(void);
unsigned int wla_get_group_num(void);
void wla_set_group_mst(unsigned int group, uint64_t mst_sel);
int wla_get_group_mst(unsigned int group, uint64_t *mst);
void wla_set_group_debounce(unsigned int group, unsigned int debounce);
int wla_get_group_debounce(unsigned int group, unsigned int *debounce);
void wla_set_group_strategy(unsigned int group, unsigned int strategy);
int wla_get_group_strategy(unsigned int group, unsigned int *strategy);
void wla_set_group_ignore_urg(unsigned int group, unsigned int ignore_urgent);
int wla_get_group_ignore_urg(unsigned int group, unsigned int *ignore_urgent);
void wla_set_dbg_latch_sel(unsigned int lat, unsigned int sel);
int wla_get_dbg_latch_sel(unsigned int lat, unsigned int *sel);
unsigned int wla_get_dbg_latch_hw_max(void);
void wla_set_dbg_latch_sta_mux(unsigned int stat_n, unsigned int mux);
int wla_get_dbg_latch_sta_mux(unsigned int stat_n, unsigned int *mux);
unsigned int wla_get_dbg_latch_sta_hw_max(void);

void wla_mon_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set);
int wla_mon_get_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set);
void wla_mon_sta_sel(unsigned int stat_n, struct wla_mon_status *sta);
int wla_mon_get_sta_sel(unsigned int stat_n, struct wla_mon_status *sta);
int wla_mon_start(unsigned int win_len_sec);
void wla_mon_disable(void);
unsigned int wla_mon_get_ch_hw_max(void);
unsigned int wla_mon_get_sta_hw_max(void);
int wla_mon_get_ch_cnt(unsigned int ch, uint32_t *north_cnt,
							uint32_t *south_cnt);
int wla_mon_init(struct wla_monitor *mon);

#endif	// __WLA_H__
