// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/module.h>

#include <wla_dbg_sysfs.h>
#include "wla.h"

#define WLA_GROUP_OP(op, _priv) ({\
	op.fs_read = wla_group_read;\
	op.fs_write = wla_group_write;\
	op.priv = _priv; })

#define WLA_GROUP_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_GROUP_OP(_n.op, &_n); })

#define WLA_MON_OP(op, _priv) ({\
	op.fs_read = wla_mon_read;\
	op.fs_write = wla_mon_write;\
	op.priv = _priv; })

#define WLA_MON_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_MON_OP(_n.op, &_n); })

#define WLA_DBG_LATCH_OP(op, _priv) ({\
	op.fs_read = wla_dbg_latch_read;\
	op.fs_write = wla_dbg_latch_write;\
	op.priv = _priv; })

#define WLA_DBG_LATCH_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_DBG_LATCH_OP(_n.op, &_n); })

enum {
	TYPE_MASTER_SEL,
	TYPE_DEBOUNCE,
	TYPE_STRATEGY,
	TYPE_IGNORE_GROUP,

	NF_TYPE_GROUP_MAX
};

enum {
	TYPE_START,
	TYPE_CH_CNT,
	TYPE_MON_SEL,
	TYPE_STATUS_SEL,

	NF_TYPE_MON_MAX
};

enum {
	TYPE_LAT_SEL,
	TYPE_LAT_STA_MUX,

	NF_TYPE_DBG_LATCH_MAX
};

static struct wla_sysfs_handle wla_entry_dbg_group;
static struct wla_sysfs_handle wla_entry_dbg_monitor;
static struct wla_sysfs_handle wla_dbg_latch;

static struct WLA_DBG_NODE master_sel;
static struct WLA_DBG_NODE debounce;
static struct WLA_DBG_NODE strategy;
static struct WLA_DBG_NODE ignore_urgent;

static struct WLA_DBG_NODE start;
static struct WLA_DBG_NODE ch_cnt;
static struct WLA_DBG_NODE mon_sel;
static struct WLA_DBG_NODE status_sel;

static struct WLA_DBG_NODE lat_sel;
static struct WLA_DBG_NODE lat_sta_mux;

static ssize_t wla_mon_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int idx, num;
	int ret;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_CH_CNT: {
		uint32_t nth_cnt, sth_cnt;
		uint32_t req_low, hws1_req, slc_ddren_low;

		num = wla_mon_get_ch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret = wla_mon_get_ch_cnt(idx, &nth_cnt, &sth_cnt);
			if (!ret)
				wla_dbg_log("ch_%d nth_cnt = %d, sth_cnt = %d\n",
								idx, nth_cnt, sth_cnt);
			else
				wla_dbg_log("read fail\n");
		}
		ret = wla_mon_get_ch_cnt(0, &req_low, NULL);
		ret |= wla_mon_get_ch_cnt(1, &hws1_req, NULL);
		ret |= wla_mon_get_ch_cnt(3, &slc_ddren_low, NULL);
		if (!ret) {
			wla_dbg_log("normal s1 counter = %d\n", req_low);
			wla_dbg_log("extra s1 counter = %d\n",
							(hws1_req > req_low) ? hws1_req - req_low : 0);
			wla_dbg_log("slc_ddren_req low cnt = %d\n", slc_ddren_low);
		} else
			wla_dbg_log("read fail\n");
		wla_mon_disable();
		break;
	}
	case TYPE_MON_SEL: {
		struct wla_mon_ch_setting ch_set = {0};

		num = wla_mon_get_ch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret =  wla_mon_get_ch_sel(idx, &ch_set);
			if (!ret)
				wla_dbg_log(
					"ch_%d sig_sel = %d, bit_sel = %d, trig_type = %d\n",
					idx, ch_set.sig_sel, ch_set.bit_sel, ch_set.trig_type);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_STATUS_SEL: {
		struct wla_mon_status sta;

		num = wla_mon_get_sta_hw_max();
		for (idx = 1; idx <= num; idx++) {
			ret =  wla_mon_get_sta_sel(idx, &sta);
			if (!ret)
				wla_dbg_log("sta_%d mux = 0x%x\n", idx, sta.mux);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_mon_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int para1;
	int ret;

	if (!FromUserBuf || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_START:
		/* para1: win_len_sec */
		if (kstrtouint(FromUserBuf, 10, &para1))
			return -EINVAL;
		ret = wla_mon_start(para1);
		if (ret)
			pr_info("input must <= 150s\n");
		break;
	case TYPE_MON_SEL: {
		/* para1: channel */
		struct wla_mon_ch_setting ch_set;

		if (sscanf(FromUserBuf, "%d %d %d %x",
						&para1, &ch_set.sig_sel,
						&ch_set.bit_sel, &ch_set.trig_type) != 4)
			return -EINVAL;
		wla_mon_ch_sel(para1, &ch_set);
		break;
	}
	case TYPE_STATUS_SEL: {
		/* para1: status_n */
		struct wla_mon_status sta;

		if (sscanf(FromUserBuf, "%d %x", &para1, &sta.mux) != 2)
			return -EINVAL;
		wla_mon_sta_sel(para1, &sta);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_group_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int grp, grp_num, data;
	uint64_t master;
	int ret;

	if (!p || !node)
		return -EINVAL;

	grp_num = wla_get_group_num();
	switch (node->type) {
	case TYPE_MASTER_SEL:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_mst(grp, &master);
			if (!ret)
				wla_dbg_log("group_mst_%d = 0x%llx\n", grp, master);
			else
				wla_dbg_log("read group_%d fail\n", grp);
		}
		break;

	case TYPE_DEBOUNCE:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_debounce(grp, &data);
			if (!ret)
				wla_dbg_log("group_debounce_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	case TYPE_STRATEGY:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_strategy(grp, &data);
			if (!ret)
				wla_dbg_log("group_strategy_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	case TYPE_IGNORE_GROUP:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_ignore_urg(grp, &data);
			if (!ret)
				wla_dbg_log("group_ignore_urg_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_group_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int grp;
	uint64_t para;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%d %llx", &grp, &para) != 2)
		return -EINVAL;

	switch (node->type) {
	case TYPE_MASTER_SEL:
		wla_set_group_mst(grp, para);
		break;

	case TYPE_DEBOUNCE:
		wla_set_group_debounce(grp, (uint32_t)para);
		break;

	case TYPE_STRATEGY:
		wla_set_group_strategy(grp, (uint32_t)para);
		break;

	case TYPE_IGNORE_GROUP:
		wla_set_group_ignore_urg(grp, (uint32_t)para);
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_dbg_latch_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int idx, num;
	int ret;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_LAT_SEL: {
		unsigned int sel;

		num = wla_get_dbg_latch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret = wla_get_dbg_latch_sel(idx, &sel);
			if (!ret)
				wla_dbg_log("latch_%d_sel = 0x%x\n", idx, sel);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_LAT_STA_MUX: {
		unsigned int mux;

		num = wla_get_dbg_latch_sta_hw_max();
		for (idx = 1; idx <= num; idx++) {
			ret =  wla_get_dbg_latch_sta_mux(idx, &mux);
			if (!ret)
				wla_dbg_log("sta_%d mux = 0x%x\n", idx, mux);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_dbg_latch_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int para1;

	if (!FromUserBuf || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_LAT_SEL: {
		/* para1: latch num */
		unsigned int sel;

		if (sscanf(FromUserBuf, "%d %x", &para1, &sel) != 2)
			return -EINVAL;
		wla_set_dbg_latch_sel(para1, sel);
		break;
	}
	case TYPE_LAT_STA_MUX: {
		/* para1: status_n */
		unsigned int mux;

		if (sscanf(FromUserBuf, "%d %x", &para1, &mux) != 2)
			return -EINVAL;
		wla_set_dbg_latch_sta_mux(para1, mux);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_stdby_en_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int enable;

	if (!FromUserBuf)
		return -EINVAL;
	/* adb cmd example: */
	/* "echo 1 > /proc/wla/wla_dbg/enable" */

	/* para1: enable */
	if (!kstrtouint(FromUserBuf, 10, &enable)) {
		wla_set_enable(enable);
		return sz;
	}
	return -EINVAL;
}

static ssize_t wla_stdby_en_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int enable;

	if (!p)
		return -EINVAL;

	enable = wla_get_enable();
	if (enable == 0xf)
		wla_dbg_log("WLA standby mode: Enable\n");
	else
		wla_dbg_log("WLA standby mode: Disable = 0x%x\n", enable);

	return p - ToUserBuf;
}

static const struct wla_sysfs_op wla_stdby_en_fops = {
	.fs_read = wla_stdby_en_read,
	.fs_write = wla_stdby_en_write,
};

static int __init wla_fs_init(void)
{
	wla_dbg_sysfs_root_entry_create();
	wla_dbg_sysfs_entry_node_add("standby_en", WLA_DBG_SYS_FS_MODE
			, &wla_stdby_en_fops, NULL);

	wla_dbg_sysfs_sub_entry_add("group", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_entry_dbg_group);

	WLA_GROUP_NODE_INIT(master_sel, "master_sel",
				    TYPE_MASTER_SEL);
	wla_dbg_sysfs_sub_entry_node_add(master_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&master_sel.op,
					&wla_entry_dbg_group,
					&master_sel.handle);

	WLA_GROUP_NODE_INIT(debounce, "debounce",
				    TYPE_DEBOUNCE);
	wla_dbg_sysfs_sub_entry_node_add(debounce.name,
					WLA_DBG_SYS_FS_MODE,
					&debounce.op,
					&wla_entry_dbg_group,
					&debounce.handle);

	WLA_GROUP_NODE_INIT(strategy, "strategy",
				    TYPE_STRATEGY);
	wla_dbg_sysfs_sub_entry_node_add(strategy.name,
					WLA_DBG_SYS_FS_MODE,
					&strategy.op,
					&wla_entry_dbg_group,
					&strategy.handle);

	WLA_GROUP_NODE_INIT(ignore_urgent, "ignore_urgent",
				    TYPE_IGNORE_GROUP);
	wla_dbg_sysfs_sub_entry_node_add(ignore_urgent.name,
					WLA_DBG_SYS_FS_MODE,
					&ignore_urgent.op,
					&wla_entry_dbg_group,
					&ignore_urgent.handle);

	wla_dbg_sysfs_sub_entry_add("monitor", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_entry_dbg_monitor);

	WLA_MON_NODE_INIT(start, "start",
				    TYPE_START);
	wla_dbg_sysfs_sub_entry_node_add(start.name,
					WLA_DBG_SYS_FS_MODE,
					&start.op,
					&wla_entry_dbg_monitor,
					&start.handle);

	WLA_MON_NODE_INIT(ch_cnt, "ch_cnt",
				    TYPE_CH_CNT);
	wla_dbg_sysfs_sub_entry_node_add(ch_cnt.name,
					WLA_DBG_SYS_FS_MODE,
					&ch_cnt.op,
					&wla_entry_dbg_monitor,
					&ch_cnt.handle);

	WLA_MON_NODE_INIT(mon_sel, "mon_sel",
				    TYPE_MON_SEL);
	wla_dbg_sysfs_sub_entry_node_add(mon_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&mon_sel.op,
					&wla_entry_dbg_monitor,
					&mon_sel.handle);

	WLA_MON_NODE_INIT(status_sel, "status_sel",
				    TYPE_STATUS_SEL);
	wla_dbg_sysfs_sub_entry_node_add(status_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&status_sel.op,
					&wla_entry_dbg_monitor,
					&status_sel.handle);

	wla_dbg_sysfs_sub_entry_add("dbg_latch", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_dbg_latch);

	WLA_DBG_LATCH_NODE_INIT(lat_sel, "lat_sel",
				    TYPE_LAT_SEL);
	wla_dbg_sysfs_sub_entry_node_add(lat_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&lat_sel.op,
					&wla_dbg_latch,
					&lat_sel.handle);

	WLA_DBG_LATCH_NODE_INIT(lat_sta_mux, "lat_sta_mux",
				    TYPE_LAT_STA_MUX);
	wla_dbg_sysfs_sub_entry_node_add(lat_sta_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&lat_sta_mux.op,
					&wla_dbg_latch,
					&lat_sta_mux.handle);
	return 0;
}

static void __exit wla_fs_exit(void)
{
}

module_init(wla_fs_init);
module_exit(wla_fs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK wla debug module - v1");
MODULE_AUTHOR("MediaTek Inc.");
