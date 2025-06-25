// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include "wla.h"

#define WLA__INIT_SETTING_VER		"init-setting-ver"
#define WLA__ENABLE					"wla-enable"
#define WLA__GROUP_HW_MAX			"group-hw-max"
#define WLA__GROUP_NODE				"group-ctrl"
#define WLA__GROUP_MASTER_SEL		"master-sel"
#define WLA__GROUP_DEBOUNCE			"debounce"
#define WLA__GROUP_STRATEGY			"strategy"
#define WLA__GROUP_IGNORE_URGENT	"ignore-urgent"
#define WLA__MON_CH_NODE			"monitor-channel"
#define WLA__MON_HW_CH_MAX			"monitor-hw-ch-max"
#define WLA__MON_CH_SIG_SEL			"sig-sel"
#define WLA__MON_CH_BIT_SEL			"bit-sel"
#define WLA__MON_CH_TRIG_TYPE		"trig-type"
#define WLA__MON_STA_NODE			"monitor-status"
#define WLA__MON_HW_STA_MAX			"monitor-hw-sta-max"
#define WLA__STA_MUX				"mux"
#define WLA__DBG_LATCH_HW_MAX		"dbg-latch-hw-max"
#define WLA__DBG_LATCH_NODE			"dbg-latch"
#define WLA__DBG_LATCH_SEL			"sel"
#define WLA__DBG_LATCH_STA_NODE		"dbg-latch-status"
#define WLA__DBG_LATCH_STA_HW_MAX	"dbg-latch-sta-hw-max"

#define WLA_GROUP_BUF	(10)
#define WLA_DBG_LATCH_BUF	(10)
#define WLA_DBG_LATCH_STA_BUF	(6)

void __iomem *wla_north_base;
void __iomem *wla_south_base;

struct wla_group_info {
	uint64_t master_sel;
	unsigned int debounce;
	unsigned int strategy;
	unsigned int ignore_urg;
};

struct wla_group {
	struct wla_group_info info[WLA_GROUP_BUF];
	unsigned int num;
	unsigned int hw_max;
};

struct wla_dbg_latch {
	unsigned int lat_sel[WLA_DBG_LATCH_BUF];
	unsigned int lat_num;
	unsigned int lat_hw_max;
	unsigned int sta_mux[WLA_DBG_LATCH_STA_BUF];
	unsigned int sta_num;
	unsigned int sta_hw_max;
};

static struct wla_group wla_grp;
static struct wla_monitor wla_mon;
static struct wla_dbg_latch wla_lat;
static unsigned int bypass_mode_only;

void wla_set_enable(unsigned int enable)
{
	/* set grp en */
	wla_write_field(WLAPM_RGU_CTRL0, ((1U << wla_grp.num) - 1),
						WLAPM_RGU_GRP_EN);
	if (enable && !bypass_mode_only) {
		/* dis bypass mode */
		wla_write_field(WLAPM_CLK_CTRL0, 0x0, BIT(0));
		wla_write_field(WLAPM_DDREN_CTRL0, 0,
							WLAPM_DDREN_BYPASS_FSM_CTRL);
	} else {
		/* bypass mode */
		wla_write_field(WLAPM_CLK_CTRL0, 0x1, BIT(0));
		wla_write_field(WLAPM_DDREN_CTRL0, 1,
							WLAPM_DDREN_BYPASS_FSM_CTRL);
	}
}
EXPORT_SYMBOL(wla_set_enable);

unsigned int wla_get_enable(void)
{
	unsigned int grp_en, bypass_mode, ret = 0;

	grp_en = wla_north_read_field(WLAPM_RGU_CTRL0, WLAPM_RGU_GRP_EN);
	bypass_mode = wla_north_read_field(WLAPM_DDREN_CTRL0,
											WLAPM_DDREN_BYPASS_FSM_CTRL);
	if (grp_en == ((1U << wla_grp.num) - 1))
		ret |= (1U << 0);
	if (bypass_mode == 0x0)
		ret |= (1U << 1);

	grp_en = wla_south_read_field(WLAPM_RGU_CTRL0, WLAPM_RGU_GRP_EN);
	bypass_mode = wla_south_read_field(WLAPM_DDREN_CTRL0,
											WLAPM_DDREN_BYPASS_FSM_CTRL);
	if (grp_en == ((1U << wla_grp.num) - 1))
		ret |= (1U << 2);
	if (bypass_mode == 0x0)
		ret |= (1U << 3);

	return ret;
}
EXPORT_SYMBOL(wla_get_enable);

unsigned int wla_get_group_num(void)
{
	return wla_grp.num;
}
EXPORT_SYMBOL(wla_get_group_num);

void wla_set_group_mst(unsigned int group, uint64_t mst_sel)
{
	if (group < wla_grp.hw_max) {
		wla_write_field(WLAPM_RGU_GRP0 + group * 8,
							(uint32_t)(mst_sel & 0xffffffff), GENMASK(31, 0));
		wla_write_field(WLAPM_RGU_GRP1 + group * 8,
							(uint32_t)(mst_sel >> 32), GENMASK(31, 0));
	}
}
EXPORT_SYMBOL(wla_set_group_mst);

int wla_get_group_mst(unsigned int group, uint64_t *mst)
{
	uint64_t value_n, value_s;

	if (group >= wla_grp.hw_max || mst == NULL)
		return WLA_FAIL;
	/* get grp mst */
	value_n = wla_north_read_field(WLAPM_RGU_GRP1 + group * 8, GENMASK(31, 0));
	value_n = (value_n << 32) |
					wla_north_read_field(WLAPM_RGU_GRP0 + group * 8,
											GENMASK(31, 0));
	value_s = wla_south_read_field(WLAPM_RGU_GRP1 + group * 8, GENMASK(31, 0));
	value_s = (value_s << 32) |
					wla_south_read_field(WLAPM_RGU_GRP0 + group * 8,
											GENMASK(31, 0));
	if (value_n != value_s)
		return WLA_FAIL;

	*mst = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_mst);

void wla_set_group_debounce(unsigned int group, unsigned int debounce)
{
	if (group < wla_grp.hw_max) {
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, debounce,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);
	}
}
EXPORT_SYMBOL(wla_set_group_debounce);

int wla_get_group_debounce(unsigned int group, unsigned int *debounce)
{
	uint64_t value_n, value_s;

	if (group >= wla_grp.hw_max || debounce == NULL)
		return WLA_FAIL;
	/* get grp mst */
	value_n =
		wla_north_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
								WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);
	value_s =
		wla_south_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
								WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);

	if (value_n != value_s)
		return WLA_FAIL;

	*debounce = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_debounce);

void wla_set_group_strategy(unsigned int group, unsigned int strategy)
{
	if (group < wla_grp.hw_max) {
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, strategy,
							WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, 0x1,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_ENABLE);
	}
}
EXPORT_SYMBOL(wla_set_group_strategy);

int wla_get_group_strategy(unsigned int group, unsigned int *strategy)
{
	uint64_t value_n, value_s;

	if (group >= wla_grp.hw_max || strategy == NULL)
		return WLA_FAIL;
	/* get grp mst */
	value_n = wla_north_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
	value_s = wla_south_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);

	if (value_n != value_s)
		return WLA_FAIL;

	*strategy = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_strategy);

void wla_set_group_ignore_urg(unsigned int group, unsigned int ignore_urg)
{
	if (group < wla_grp.hw_max) {
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), ignore_urg,
							WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
	}
}
EXPORT_SYMBOL(wla_set_group_ignore_urg);

int wla_get_group_ignore_urg(unsigned int group, unsigned int *ignore_urg)
{
	uint64_t value_n, value_s;

	if (group >= wla_grp.hw_max || ignore_urg == NULL)
		return WLA_FAIL;
	/* get grp ignore_urg */
	value_n = wla_north_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
	value_s = wla_south_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);

	if (value_n != value_s)
		return WLA_FAIL;

	*ignore_urg = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_ignore_urg);

void wla_set_dbg_latch_sel(unsigned int lat, unsigned int sel)
{
	if (lat < wla_lat.lat_hw_max)
		wla_write_raw(WLAPM_DBG_CTRL0, sel << (lat * 3), 0x7U << (lat * 3));
}
EXPORT_SYMBOL(wla_set_dbg_latch_sel);

int wla_get_dbg_latch_sel(unsigned int lat, unsigned int *sel)
{
	unsigned int value_n, value_s;

	if (lat >= wla_lat.lat_hw_max || sel == NULL)
		return WLA_FAIL;
	/* get dbg latch sel */
	value_n = wla_north_read_raw(WLAPM_DBG_CTRL0, 0x7U << (lat * 3));
	value_s = wla_south_read_raw(WLAPM_DBG_CTRL0, 0x7U << (lat * 3));

	if (value_n != value_s)
		return WLA_FAIL;

	value_n = value_n >> (lat * 3);
	*sel = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_dbg_latch_sel);

unsigned int wla_get_dbg_latch_hw_max(void)
{
	return wla_lat.lat_hw_max;
}
EXPORT_SYMBOL(wla_get_dbg_latch_hw_max);

void wla_set_dbg_latch_sta_mux(unsigned int stat_n, unsigned int mux)
{
	if (stat_n <= wla_lat.sta_hw_max && stat_n >= 1) {
		wla_write_raw(WLAPM_DDREN_DBG, mux << ((stat_n - 1) * 4),
						0xFU << ((stat_n - 1) * 4));
	}
}
EXPORT_SYMBOL(wla_set_dbg_latch_sta_mux);

int wla_get_dbg_latch_sta_mux(unsigned int stat_n, unsigned int *mux)
{
	unsigned int value_n, value_s;

	if (stat_n < 1 || stat_n > wla_lat.lat_hw_max)
		return WLA_FAIL;
	/* get dbg latch sta mux */
	value_n = wla_north_read_raw(WLAPM_DDREN_DBG, 0xFU << ((stat_n - 1) * 4));
	value_s = wla_south_read_raw(WLAPM_DDREN_DBG, 0xFU << ((stat_n - 1) * 4));

	if (value_n != value_s)
		return WLA_FAIL;

	value_n = value_n >> ((stat_n - 1) * 4);
	*mux = value_n;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_dbg_latch_sta_mux);

unsigned int wla_get_dbg_latch_sta_hw_max(void)
{
	return wla_lat.sta_hw_max;
}
EXPORT_SYMBOL(wla_get_dbg_latch_sta_hw_max);

static int wla_dbg_latch_init(struct wla_dbg_latch *lat)
{
	unsigned int n;

	if (lat == NULL)
		return WLA_FAIL;

	for (n = 0; n < lat->lat_num; n++)
		wla_set_dbg_latch_sel(n, lat->lat_sel[n]);

	for (n = 1; n <= lat->sta_num; n++)
		wla_set_dbg_latch_sta_mux(n, lat->sta_mux[n]);

	return WLA_SUCCESS;
}

static int get_chipid(void)
{
	struct device_node *node;
	struct tag_chipid *chip_id;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (node) {
		chip_id = (struct tag_chipid *)of_get_property(node,
								"atag,chipid",
								&len);
		if (!chip_id) {
			pr_info("could not found atag,chipid in chosen\n");
			return -ENODEV;
		}
	} else {
		pr_info("chosen node not found in device tree\n");
		return -ENODEV;
	}

	pr_info("[mtk-wla] current sw version: %u\n", chip_id->sw_ver);

	return chip_id->sw_ver;
}

static void __wla_bypass_mode_init(void)
{
	wla_write_field(WLAPM_CLK_CTRL0, 0x1, BIT(0));
	wla_write_field(WLAPM_CLK_CTRL1, 0x1, WLAPM_CKCTRL_DBG_CK_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FROM_LEGACY_MODE);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FORCE_ON);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_HWS1_FSM_IDLE_DBG_MON_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER_EN);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_DDREN_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_HWS1_FSM_COUNTER);
}

static void __wla_standby_power_init(void)
{
	unsigned int group;

	wla_write_field(WLAPM_CLK_CTRL1, 0x1, WLAPM_CKCTRL_DBG_CK_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FROM_LEGACY_MODE);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FORCE_ON);


	wla_write_field(WLAPM_DDREN_CTRL0, 0x1,
						WLAPM_DDREN_DDREN_FSM_IGNORE_ACK_WINDOW_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_HWS1_FSM_IDLE_DBG_MON_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER_EN);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x12,
						WLAPM_DDREN_FSM_IGNORE_ACK_WINDOW_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_FSM_BYPASS_SLC_REQ);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_DDREN_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_HWS1_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x3,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_CHN_EMI_IDLE);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x3,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_SLC_IDLE);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x0,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_SLC_DDREN_REQ);


	wla_write_field(WLAPM_DDREN_CTRL2, 0x3,
						WLAPM_HWS1_HIGH_MONITOR_ACCESS_DRAM_BUSY_EN);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x100,
						WLAPM_HWS1_HIGH_MONITOR_ACCESS_DRAM_BUSY_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_CHN_EMI_IDLE_POR_ADJ);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_CHN_EMI_IDLE_BYPASS);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_SLC_IDLE_PRO_ADJ);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_SLC_IDLE_EN_BYPASS);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_HWS1_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_DDREN_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x1a, WLAPM_HWS1_FSM_DBG_IDLE_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_SLC_READ_MISS_DDREN_BYPASS);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0, WLAPM_SLC_READ_MISS_DDREN_POR_ADJ);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0,
						WLAPM_SLC_WRITE_THROUGH_DDREN_BYPASS);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x0,
						WLAPM_SLC_WRITE_THROUGH_DDREN_POR_ADJ);

	for (group = 0; group < wla_grp.hw_max; group++) {
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0xd,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_ENABLE);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_WAKEUP_MODE_SEL);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_ACK_MODE_SEL);
	}
}

static int wla_init(const char *init_ver, struct wla_group *group)
{
	unsigned int grp;
	unsigned int enable = 0;

	if (!strcmp(init_ver, "standby_power_ver_a")) {
		__wla_standby_power_init();
		enable = 1;
	} else if (!strcmp(init_ver, "bypass_mode")) {
		__wla_bypass_mode_init();
		bypass_mode_only = 1;
		enable = 0;
	} else if (!strcmp(init_ver, "depend_on_chipid_ver_a")) {
		if (get_chipid()) {
			__wla_standby_power_init();
			enable = 1;
		} else {
			__wla_bypass_mode_init();
			bypass_mode_only = 1;
			enable = 0;
		}
	} else
		return WLA_FAIL;

	for (grp = 0; grp < group->num; grp++) {
		/* set group master */
		wla_set_group_mst(grp, group->info[grp].master_sel);
		/* standby strategy */
		/* 0x7=idle, 0x1=s1(wake up by slc_ddren_req),
		 * 0x2=s1(wake up by slc_idle), 0x3=s1(wake up by emi_chnc_idle)
		 */
		wla_set_group_strategy(grp, group->info[grp].strategy);
		/* group debounce */
		wla_set_group_debounce(grp, group->info[grp].debounce);
		/* group ignore urgent */
		wla_set_group_ignore_urg(grp, group->info[grp].ignore_urg);
	}

	wla_set_enable(enable);

	return WLA_SUCCESS;
}

static int wla_probe(struct platform_device *pdev)
{
	struct device_node *parent_node = pdev->dev.of_node;
	struct device_node *np;
	const char *init_setting_ver;
	struct wla_group *group = &wla_grp;
	struct wla_monitor *mon = &wla_mon;
	struct wla_dbg_latch *lat = &wla_lat;
	uint32_t wla_en = 0;
	int ret = WLA_FAIL;

	wla_north_base = of_iomap(parent_node, 0);
	wla_south_base = of_iomap(parent_node, 1);

	of_property_read_u32(parent_node, WLA__ENABLE, &wla_en);

	if (wla_en) {
		of_property_read_string(parent_node, WLA__INIT_SETTING_VER,
									&init_setting_ver);
		of_property_read_u32(parent_node, WLA__GROUP_HW_MAX, &group->hw_max);
		while ((np = of_parse_phandle(parent_node, WLA__GROUP_NODE,
										group->num))) {
			of_property_read_u64(np, WLA__GROUP_MASTER_SEL,
									&group->info[group->num].master_sel);
			of_property_read_u32(np, WLA__GROUP_DEBOUNCE,
									&group->info[group->num].debounce);
			of_property_read_u32(np, WLA__GROUP_STRATEGY,
									&group->info[group->num].strategy);
			of_property_read_u32(np, WLA__GROUP_IGNORE_URGENT,
									&group->info[group->num].ignore_urg);
			of_node_put(np);
			group->num++;
			if (group->num >= group->hw_max || group->num >= WLA_GROUP_BUF)
				break;
		}
		of_property_read_u32(parent_node, WLA__MON_HW_CH_MAX, &mon->ch_hw_max);
		of_property_read_u32(parent_node, WLA__MON_HW_STA_MAX,
								&mon->sta_hw_max);
		of_property_read_u32(parent_node, WLA__DBG_LATCH_HW_MAX,
								&lat->lat_hw_max);
		of_property_read_u32(parent_node, WLA__DBG_LATCH_STA_HW_MAX,
								&lat->sta_hw_max);
		while ((np = of_parse_phandle(parent_node, WLA__MON_CH_NODE,
										mon->ch_num))) {
			of_property_read_u32(np, WLA__MON_CH_SIG_SEL,
									&mon->ch[mon->ch_num].sig_sel);
			of_property_read_u32(np, WLA__MON_CH_BIT_SEL,
									&mon->ch[mon->ch_num].bit_sel);
			of_property_read_u32(np, WLA__MON_CH_TRIG_TYPE,
									&mon->ch[mon->ch_num].trig_type);
			of_node_put(np);
			mon->ch_num++;
			if (mon->ch_num >= mon->ch_hw_max || mon->ch_num >= WLA_MON_CH_BUF)
				break;
		}
		while ((np = of_parse_phandle(parent_node, WLA__MON_STA_NODE,
										mon->sta_num))) {
			mon->sta_num++;
			of_property_read_u32(np, WLA__STA_MUX,
									&mon->status[mon->sta_num].mux);
			of_node_put(np);
			if (mon->sta_num >= mon->sta_hw_max ||
					mon->sta_num >= (WLA_MON_STATUS_BUF - 1))
				break;
		}
		while ((np = of_parse_phandle(parent_node, WLA__DBG_LATCH_NODE,
										lat->lat_num))) {
			of_property_read_u32(np, WLA__DBG_LATCH_SEL,
									&lat->lat_sel[lat->lat_num]);
			of_node_put(np);
			lat->lat_num++;
			if (lat->lat_num >= lat->lat_hw_max ||
					lat->lat_num >= WLA_DBG_LATCH_BUF)
				break;
		}
		while ((np = of_parse_phandle(parent_node, WLA__DBG_LATCH_STA_NODE,
										lat->sta_num))) {
			lat->sta_num++;
			of_property_read_u32(np, WLA__STA_MUX,
									&lat->sta_mux[lat->sta_num]);
			of_node_put(np);
			if (lat->sta_num >= lat->sta_hw_max ||
					lat->sta_num >= (WLA_DBG_LATCH_STA_BUF - 1))
				break;
		}
		ret = wla_init(init_setting_ver, group);
		if (ret)
			pr_info("[mtk-wla] wla init failed\n");
		ret = wla_mon_init(mon);
		if (ret)
			pr_info("[mtk-wla] wla mon init failed\n");
		ret = wla_dbg_latch_init(lat);
		if (ret)
			pr_info("[mtk-wla] wla dbg latch init failed\n");
		pr_info("[mtk-wla] wla init done\n");
	} else
		pr_info("[mtk-wla] wla disable\n");

	return WLA_SUCCESS;
}

static const struct of_device_id wla_of_match[] = {
	{
		.compatible = "mediatek,wla",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, wla_of_match);

static struct platform_driver wla_driver = {
	.driver = {
		.name = "wla",
		.of_match_table = wla_of_match,
	},
	.probe	= wla_probe,
};
module_platform_driver(wla_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK work load aware - v1");
MODULE_AUTHOR("MediaTek Inc.");
