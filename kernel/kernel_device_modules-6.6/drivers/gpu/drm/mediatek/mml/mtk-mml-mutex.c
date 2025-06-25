// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-dpc.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-mmp.h"

#define DISP_MUTEX_CFG	0x008

#define MUTEX_MAX_MOD_REGS	((MML_MAX_COMPONENTS + 31) >> 5)

/* MUTEX register offset */
#define MUTEX_EN(id)		(0x20 + (id) * 0x20)
#define MUTEX_SHADOW_CTRL(id)	(0x24 + (id) * 0x20)
#define MUTEX_MOD(id, offset)	((offset) + (id) * 0x20)
#define MUTEX_SOF(id, offset)	((offset) + (id) * 0x20)

int mml_mutex_dl_sof;
module_param(mml_mutex_dl_sof, int, 0644);

#define mutex_dl_perf_en	(mml_mutex_dl_sof & BIT(1))
#define mutex_dl_perf_log	(mml_mutex_dl_sof & BIT(2))

struct mutex_data {
	/* Count of display mutex HWs */
	u32 mutex_cnt;
	/* Offsets and count of MUTEX_MOD registers per mutex */
	u32 mod_offsets[MUTEX_MAX_MOD_REGS];
	u32 sof_offset;
	u32 mod_cnt;
	bool sofgrp_assign;
	u8 gpr[MML_PIPE_CNT];

	u32 (*get_mutex_sof)(struct mml_mutex_ctl *ctl);
};

struct mutex_module {
	u32 mutex_id;
	u32 index;
	u32 field;
	bool select:1;
	bool trigger:1;
};

struct mml_mutex {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct mutex_data *data;
	bool ddp_bound;
	atomic_t connect[MML_PIPE_CNT];
	enum mml_mode connected_mode;

	u16 event_pipe0_mml;
	u16 event_pipe1_mml;
	u16 event_stream_sof;
	u16 event_prete;

	struct mutex_module modules[MML_MAX_COMPONENTS];
};

static inline struct mml_mutex *comp_to_mutex(struct mml_comp *comp)
{
	return container_of(comp, struct mml_mutex, comp);
}

static s32 mutex_enable(struct mml_mutex *mutex, struct cmdq_pkt *pkt,
			const struct mml_topology_path *path, u32 mutex_sof,
			enum mml_mode mode, bool mod_en, bool sof_en)
{
	const phys_addr_t base_pa = mutex->comp.base_pa;
	const u32 sof_off = mutex->data->sof_offset;
	s32 mutex_id = -1;
	u32 mutex_mod[MUTEX_MAX_MOD_REGS] = {0};
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mutex_module *mod = &mutex->modules[path->nodes[i].id];
		if (mutex->comp.sysid != path->nodes[i].comp->sysid)
			continue;

		if (mod->select)
			mutex_id = mod->mutex_id;
		if (mod->trigger)
			mutex_mod[mod->index] |= 1 << mod->field;
	}

	if (mutex->data->sofgrp_assign) {
		/* TODO: get sof group0 shift from dts */
		mutex_mod[1] |= 1 << (24 + path->mux_group);
	} else {
		/* use mutex stream to trigger related sof group */
		mutex_id = path->mux_group;
	}

	if (mutex_id < 0)
		return -EINVAL;

	/* Do config mutex mod only in dc mode.
	 * For direct link mode, config from mml side (which mutex_sof is empty),
	 * since mml flow has correct topology.
	 */
	if (mod_en) {
		mml_mmp(mutex_mod, MMPROFILE_FLAG_PULSE, mode, mutex->data->mod_cnt);

		for (i = 0; i < mutex->data->mod_cnt; i++) {
			u32 offset = mutex->data->mod_offsets[i];

			cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_MOD(mutex_id, offset),
				       mutex_mod[i], U32_MAX);

			mml_mmp(mutex_mod, MMPROFILE_FLAG_PULSE, mutex_id, mutex_mod[i]);
		}
	}

	/* Enable mutex for dc mode, which trigger directly.
	 * For DL mode enable mutex from disp addon
	 * (which mutex_sof contains disp signal bit) and wait disp signal.
	 */
	if (sof_en) {
		cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_SOF(mutex_id, sof_off),
			mutex_sof, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_EN(mutex_id), 0x1, U32_MAX);
	}

	mml_mmp(mutex_en, MMPROFILE_FLAG_PULSE, mode, mutex_sof);

	return 0;
}

static s32 mutex_disable(struct mml_mutex *mutex, struct cmdq_pkt *pkt,
			 const struct mml_topology_path *path)
{
	const phys_addr_t base_pa = mutex->comp.base_pa;
	s32 mutex_id = -1;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mutex_module *mod = &mutex->modules[path->nodes[i].id];

		if (mod->select)
			mutex_id = mod->mutex_id;
	}

	if (mutex_id < 0)
		return -EINVAL;

	cmdq_pkt_write(pkt, NULL, base_pa + MUTEX_EN(mutex_id), 0x0, U32_MAX);

	mml_mmp(mutex_dis, MMPROFILE_FLAG_PULSE, mutex_id, path->mux_group);

	return 0;
}

static s32 mutex_trigger(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct mml_mutex *mutex = comp_to_mutex(comp);
	const struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	bool sof_en = true;
	s32 ret;

	/* DL mode and dpc off case, mutex sof control by dsi src */
	if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		if (mml_mutex_dl_sof) {
			/* mutex en in disp pkt */
			sof_en = false;
		} else if (comp == path->mutex) {
			/* wait pre-te in first mutex trigger */
			cmdq_pkt_clear_event(pkt, mutex->event_prete);
			cmdq_pkt_wait_no_clear(pkt, mutex->event_prete);
		}

		if (mutex_dl_perf_en && ccfg->pipe == 0 && comp == path->mutex)
			cmdq_pkt_backup_stamp(pkt, &task->perf_prete);
	}

	/* DL mode config sof only, other modes enable to trigger directly */
	ret = mutex_enable(mutex, pkt, path, 0x0, cfg->info.mode, true, sof_en);

	/* asume path->mmlsys2, which is mmlsys0 always put after mmlsys1,
	 * do disp/mml event sync after both mutex called mutex_enable
	 */
	if (cfg->info.mode == MML_MODE_DIRECT_LINK &&
		(!path->mmlsys2 || comp->sysid == path->mmlsys2->sysid)) {
		if (ccfg->pipe == 0) {
			if (cfg->dual) {
				cmdq_pkt_set_event(pkt, mutex->event_pipe0_mml);
				cmdq_pkt_wfe(pkt, mutex->event_pipe1_mml);
			}

			cmdq_pkt_set_event(pkt, mml_ir_get_mml_ready_event(cfg->mml));

			if (cfg->dpc && (mml_dl_dpc & MML_DPC_MUTEX_VOTE)) {
#ifndef MML_FPGA
				mml_dpc_power_release_gce(comp->sysid, pkt);
				cmdq_pkt_wfe(pkt, mml_ir_get_disp_ready_event(cfg->mml));
				mml_dpc_power_keep_gce(comp->sysid, pkt,
					mutex->data->gpr[ccfg->pipe],
					&task->dpc_reuse_mutex);

#endif
			} else {
				cmdq_pkt_wfe(pkt, mml_ir_get_disp_ready_event(cfg->mml));
			}

			/* make sure mml wait disp frame done in current te */
			cmdq_pkt_clear_event(pkt, cfg->info.disp_done_event);

			/* Note insert disp ready stamp after disp ready event in last mutex,
			 * but update and retrieve data in first mutex.
			 */
			if (mutex_dl_perf_en)
				cmdq_pkt_backup_stamp(pkt, &task->perf_dispready);
		} else {
			cmdq_pkt_set_event(pkt, mutex->event_pipe1_mml);
			cmdq_pkt_wfe(pkt, mutex->event_pipe0_mml);
		}
	}

	return ret;
}

static s32 mutex_wait_sof(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_mutex *mutex = comp_to_mutex(comp);
	const struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		if (mutex->event_stream_sof) {
			cmdq_pkt_wfe(pkt, mutex->event_stream_sof + path->mux_group);
			if (mutex_dl_perf_en && comp == path->mutex && ccfg->pipe == 0)
				cmdq_pkt_backup_stamp(pkt, &task->perf_sof);
		}

		mutex_disable(mutex, pkt, path);
	}

	return 0;
}

static s32 mutex_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];

	if (mutex_dl_perf_en && comp == path->mutex && ccfg->pipe == 0 &&
		task->perf_prete.inst_offset) {
		cmdq_pkt_backup_update(task->pkts[ccfg->pipe], &task->perf_prete);
		cmdq_pkt_backup_update(task->pkts[ccfg->pipe], &task->perf_dispready);
		cmdq_pkt_backup_update(task->pkts[ccfg->pipe], &task->perf_sof);
	}

	return 0;
}

static const struct mml_comp_config_ops mutex_config_ops = {
	.mutex = mutex_trigger,
	.wait_sof = mutex_wait_sof,
	.reframe = mutex_reconfig_frame,
};

static void mutex_taskdone(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];

	if (mutex_dl_perf_en && comp == path->mutex && ccfg->pipe == 0 &&
		task->perf_prete.inst_offset) {
		/* Time line of dl mode
		 * perf[0]   prete    disp_ready  sof      perf[1]
		 *   |  prete  |   ready   |  sof  |  framedone |
		 *   |---------|-----------|-------|------------|
		 */

		const u32 pipe = ccfg->pipe;
		u32 prete = cmdq_pkt_backup_get(task->pkts[pipe], &task->perf_prete);
		u32 dispready = cmdq_pkt_backup_get(task->pkts[pipe], &task->perf_dispready);
		u32 sof = cmdq_pkt_backup_get(task->pkts[pipe], &task->perf_sof);
		u32 *perf = cmdq_pkt_get_perf_ret(task->pkts[pipe]);
		u32 cost_ready = CMDQ_TICK_DIFF(prete, dispready);
		u32 cost_sof = CMDQ_TICK_DIFF(dispready, sof);
		u32 cost_prete, cost_done;

		CMDQ_TICK_TO_US(cost_ready);
		CMDQ_TICK_TO_US(cost_sof);

		if (perf) {
			cost_prete = CMDQ_TICK_DIFF(perf[0], prete);
			cost_done = CMDQ_TICK_DIFF(sof, perf[1]);
			CMDQ_TICK_TO_US(cost_prete);
			CMDQ_TICK_TO_US(cost_done);
		} else {
			cost_prete = 0;
			cost_done = 0;
		}

		if (mutex_dl_perf_log)
			mml_log("task cost prete %uus ready %uus sof %uus done %uus",
				cost_prete, cost_ready, cost_sof, cost_done);
		else
			mml_msg("task cost prete %uus ready %uus sof %uus done %uus",
				cost_prete, cost_ready, cost_sof, cost_done);
	}
}

static const struct mml_comp_hw_ops mutex_hw_ops = {
	.clk_enable = mml_comp_clk_enable,
	.clk_disable = mml_comp_clk_disable,
	.task_done = mutex_taskdone,
};

static void mutex_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_mutex *mutex = comp_to_mutex(comp);
	u8 i, j;
	u32 value;

	mml_err("mutex component %u dump:", comp->id);

	value = readl(base + DISP_MUTEX_CFG);
	mml_err("DISP_MUTEX_CFG %#010x", value);

	for (i = 0; i < mutex->data->mutex_cnt; i++) {
		u32 en, sof;
		u32 shadow = readl(base + MUTEX_SHADOW_CTRL(i));
		u32 mod[MUTEX_MAX_MOD_REGS] = {0};
		bool used = false;

		shadow |= 0x4;
		writel(shadow, base + MUTEX_SHADOW_CTRL(i));
		shadow = readl(base + MUTEX_SHADOW_CTRL(i));
		en = readl(base + MUTEX_EN(i));
		sof = readl(base + MUTEX_SOF(i, mutex->data->sof_offset));

		for (j = 0; j < mutex->data->mod_cnt; j++) {
			u32 offset = mutex->data->mod_offsets[j];

			mod[j] = readl(base + MUTEX_MOD(i, offset));
			if (mod[j])
				used = true;
		}
		if (i == 1 || used || en || sof) {
			mml_err("MDP_MUTEX%d_EN %#010x MDP_MUTEX%d_CTL %#010x shadow %#x",
				i, en, i, sof, shadow);
			for (j = 0; j < mutex->data->mod_cnt; j++)
				mml_err("MDP_MUTEX%d_MOD%d %#010x",
					i, j, mod[j]);
		}
	}
}

static const struct mml_comp_debug_ops mutex_debug_ops = {
	.dump = &mutex_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &mutex->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &mutex->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			mutex->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_mutex *mutex = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &mutex->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &mutex->ddp_comp);
		mutex->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static inline struct mml_mutex *ddp_comp_to_mutex(struct mtk_ddp_comp *ddp_comp)
{
	return container_of(ddp_comp, struct mml_mutex, ddp_comp);
}

static u32 get_mutex_sof(struct mml_mutex_ctl *ctl)
{
	u32 sof = 0;

	switch (ctl->sof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3;
		break;
	}

	switch (ctl->eof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1 << 6;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2 << 6;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3 << 6;
		break;
	}
	if (!sof)
		mml_err("no sof/eof source %u/%u but not cmd mode",
			ctl->sof_src, ctl->eof_src);
	return sof;
}

static u32 get_mutex_sof_mt6991(struct mml_mutex_ctl *ctl)
{
	u32 sof = 0;

	switch (ctl->sof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3;
		break;
	}

	switch (ctl->eof_src) {
	case DDP_COMPONENT_DSI0:
		sof |= 1 << 7;
		break;
	case DDP_COMPONENT_DSI1:
		sof |= 2 << 7;
		break;
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
		sof |= 3 << 7;
		break;
	}
	if (!sof)
		mml_err("no sof/eof source %u/%u but not cmd mode",
			ctl->sof_src, ctl->eof_src);
	return sof;
}


static void mutex_addon_config_dl(struct mtk_ddp_comp *ddp_comp,
				  enum mtk_ddp_comp_id prev,
				  enum mtk_ddp_comp_id next,
				  union mtk_addon_config *addon_config,
				  struct cmdq_pkt *pkt)
{
	struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	const struct mml_topology_path *path;
	u8 pipe = cfg->pipe;
	u32 sof;

	if (!cfg->ctx) {
		mml_err("%s cannot configure %d without ctx", __func__, cfg->config_type.type);
		return;
	}

	if (cfg->config_type.type == ADDON_CONNECT) {
		if (!cfg->dual && cfg->submit.info.dl_pos == MML_DL_POS_RIGHT)
			pipe = 1;

		path = mml_drm_query_dl_path(cfg->ctx, &cfg->submit, pipe);
		if (!path) {
			mml_err("%s mml_drm_query_dl_path fail", __func__);
			return;
		}

		/* this path does not use current mml mutex */
		if (path->mutex != &mutex->comp && path->mutex2 != &mutex->comp) {
			mml_err("%s no mutex in path %u", __func__, path->path_id);
			return;
		}

		sof = mutex->data->get_mutex_sof(&cfg->mutex);
		mutex_enable(mutex, pkt, path, sof, MML_MODE_DIRECT_LINK, false, true);
		mutex->connected_mode = MML_MODE_DIRECT_LINK;
	}
}

static void mutex_addon_config_addon(struct mtk_ddp_comp *ddp_comp,
				     enum mtk_ddp_comp_id prev,
				     enum mtk_ddp_comp_id next,
				     union mtk_addon_config *addon_config,
				     struct cmdq_pkt *pkt)
{
	struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	const struct mml_topology_path *path;
	u32 mutex_sof;

	if (!cfg->task) {
		mml_err("%s cannot configure %d without ctx", __func__, cfg->config_type.type);
		return;
	}

	path = cfg->task->config->path[cfg->pipe];

	if (cfg->config_type.type == ADDON_DISCONNECT) {
		if (atomic_cmpxchg_acquire(&mutex->connect[cfg->pipe], 1, 0))
			mutex_disable(mutex, pkt, path);
		else
			mml_err("%s disconnect without connect pipe %u", __func__, cfg->pipe);
	} else {
		atomic_set(&mutex->connect[cfg->pipe], 1);
		mutex_sof = cfg->mutex.is_cmd_mode ? 0 : mutex->data->get_mutex_sof(&cfg->mutex);
		mutex_enable(mutex, pkt, path, mutex_sof, MML_MODE_DDP_ADDON, true, true);
		mutex->connected_mode = MML_MODE_DDP_ADDON;
	}
}

static void mutex_addon_config(struct mtk_ddp_comp *ddp_comp,
			       enum mtk_ddp_comp_id prev,
			       enum mtk_ddp_comp_id next,
			       union mtk_addon_config *addon_config,
			       struct cmdq_pkt *pkt)
{
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	enum mml_mode mode = cfg->submit.info.mode;

	if (mode == MML_MODE_UNKNOWN) {
		struct mml_mutex *mutex = ddp_comp_to_mutex(ddp_comp);

		if (mutex->connected_mode == MML_MODE_UNKNOWN ||
			!atomic_read(&mutex->connect[cfg->pipe])) {
			/* no current connected mode, stop */
			return;
		}

		mode = mutex->connected_mode;
	}

	if (mode == MML_MODE_DIRECT_LINK) {
		/* in dpc enable case, mutex trigger by mml pkt directly */
		if (mml_mutex_dl_sof)
			mutex_addon_config_dl(ddp_comp, prev, next, addon_config, pkt);
	} else if (mode == MML_MODE_DDP_ADDON)
		mutex_addon_config_addon(ddp_comp, prev, next, addon_config, pkt);
	else
		mml_err("%s not support mode %d(%d)", __func__, mode, cfg->submit.info.mode);
}

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
	.addon_config = mutex_addon_config,
};

static struct mml_mutex *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_mutex *priv;
	struct device_node *node = dev->of_node;
	struct property *prop;
	const char *name;
	u32 mod[3], comp_id, mutex_id;
	s32 id_count, i, ret;
	bool add_ddp = true;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	of_property_for_each_string(node, "mutex-comps", prop, name) {
		ret = of_property_read_u32_array(node, name, mod, 3);
		if (ret) {
			dev_err(dev, "no property %s in dts node %s: %d\n",
				name, dev->of_node->full_name, ret);
			return ret;
		}
		if (mod[0] >= MML_MAX_COMPONENTS) {
			dev_err(dev, "%s id %u is larger than max:%d\n",
				name, mod[0], MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mod[1] >= priv->data->mod_cnt) {
			dev_err(dev,
				"%s mod index %u is larger than count:%d\n",
				name, mod[1], priv->data->mod_cnt);
			return -EINVAL;
		}
		if (mod[2] >= 32) {
			dev_err(dev,
				"%s mod field %u is larger than bits:32\n",
				name, mod[2]);
			return -EINVAL;
		}
		priv->modules[mod[0]].index = mod[1];
		priv->modules[mod[0]].field = mod[2];
		priv->modules[mod[0]].trigger = true;
	}

	id_count = of_property_count_u32_elems(node, "mutex-ids");
	for (i = 0; i + 1 < id_count; i += 2) {
		of_property_read_u32_index(node, "mutex-ids", i, &comp_id);
		of_property_read_u32_index(node, "mutex-ids", i + 1, &mutex_id);
		if (comp_id >= MML_MAX_COMPONENTS) {
			dev_err(dev, "component id %u is larger than max:%d\n",
				comp_id, MML_MAX_COMPONENTS);
			return -EINVAL;
		}
		if (mutex_id >= priv->data->mutex_cnt) {
			dev_err(dev, "mutex id %u is larger than count:%d\n",
				mutex_id, priv->data->mutex_cnt);
			return -EINVAL;
		}
		priv->modules[comp_id].mutex_id = mutex_id;
		priv->modules[comp_id].select = true;
	}

	priv->comp.config_ops = &mutex_config_ops;
	priv->comp.hw_ops = &mutex_hw_ops;
	priv->comp.debug_ops = &mutex_debug_ops;

	if (!of_property_read_u16(dev->of_node, "event-pipe0-mml", &priv->event_pipe0_mml))
		mml_log("dl event event_pipe0_mml %u", priv->event_pipe0_mml);
	if (!of_property_read_u16(dev->of_node, "event-pipe1-mml", &priv->event_pipe1_mml))
		mml_log("dl event event_pipe1_mml %u", priv->event_pipe1_mml);

	if (!of_property_read_u16(dev->of_node, "sof-event", &priv->event_stream_sof))
		mml_log("stream0 sof event %u", priv->event_stream_sof);
	if (!of_property_read_u16(dev->of_node, "event-prete", &priv->event_prete))
		mml_log("dpc pre-te event %u", priv->event_prete);

	ret = mml_ddp_comp_init(dev, &priv->ddp_comp, &priv->comp,
				&ddp_comp_funcs);
	if (ret) {
		mml_log("failed to init ddp component: %d", ret);
		add_ddp = false;
	}

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = mml_comp_add(priv->comp.id, dev, &mml_comp_ops);
	if (add_ddp)
		ret = mml_comp_add(priv->comp.id, dev, &mml_comp_ops);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

static const struct mutex_data mt6983_mutex_data = {
	.mutex_cnt = 16,
	.mod_offsets = {0x30, 0x34},
	.sof_offset = 0x2c,
	.mod_cnt = 2,
	.sofgrp_assign = true,
	.get_mutex_sof = get_mutex_sof,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
};

static const struct mutex_data mt6991_mutex_data = {
	.mutex_cnt = 16,
	.mod_offsets = {0x34, 0x38},
	.sof_offset = 0x30,
	.mod_cnt = 2,
	.get_mutex_sof = get_mutex_sof_mt6991,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
};

static const struct mutex_data mt6991_mmlt_mutex_data = {
	.mutex_cnt = 16,
	.mod_offsets = {0x34, 0x38},
	.sof_offset = 0x30,
	.mod_cnt = 2,
	.get_mutex_sof = get_mutex_sof_mt6991,
	.gpr = {CMDQ_GPR_R12, CMDQ_GPR_R14},
};

const struct of_device_id mml_mutex_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6879-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6895-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6985-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6897-mml_mutex",
		.data = &mt6983_mutex_data
	},
	{
		.compatible = "mediatek,mt6899-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6989-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6878-mml_mutex",
		.data = &mt6983_mutex_data,
	},
	{
		.compatible = "mediatek,mt6991-mml_mutex",
		.data = &mt6991_mutex_data,
	},
	{
		.compatible = "mediatek,mt6991-mmlt_mutex",
		.data = &mt6991_mmlt_mutex_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_mutex_driver_dt_match);

struct platform_driver mml_mutex_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-mutex",
		.owner = THIS_MODULE,
		.of_match_table = mml_mutex_driver_dt_match,
	},
};

//module_platform_driver(mml_mutex_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%pa name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, &comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%pa pw: %d clk: %d\n",
				comp->larb_port, &comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(mutex_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(mutex_debug, "mml mutex debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML Mutex driver");
MODULE_LICENSE("GPL v2");
