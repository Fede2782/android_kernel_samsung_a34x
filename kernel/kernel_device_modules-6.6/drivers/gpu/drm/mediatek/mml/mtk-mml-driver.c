// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/math64.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/minmax.h>
#include <linux/dma-mapping.h>
#include <mtk-smmu-v3.h>

#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/mmqos.h>
#include <soc/mediatek/smi.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <slbc_ops.h>

#include "mtk-mml-dpc.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-m2m-adaptor.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"
#include "mtk-mml-pq-core.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-color.h"

#define MML_WAKE_SAFE_CNT 64

#define CMDQ_GET_ADDR_LOW(addr)		((u16)(addr & GENMASK(15, 0)) | BIT(1))

struct mml_record {
	u32 jobid;

	u64 src_iova;
	u64 dest_iova;

	u32 src_size;
	u32 dest_size;

	u32 src_plane_offset;
	u32 dest_plane_offset;

	u64 src_iova_map_time;
	u64 dest_iova_map_time;
	u64 src_iova_unmap_time;
	u64 dest_iova_unmap_time;

	u64 config_pipe_time[MML_PIPE_CNT];
	u64 bw_time[MML_PIPE_CNT];
	u64 freq_time[MML_PIPE_CNT];
	u64 wait_fence_time[MML_PIPE_CNT];
	u64 flush_time[MML_PIPE_CNT];

	u32 cfg_jobid;
	u32 task;
	u8 state;
	u8 err;
	u8 ref;

	u32 src_crc[MML_PIPE_CNT];
	u32 dest_crc[MML_PIPE_CNT];
};

/* 512 records
 * note that (MML_RECORD_NUM - 1) will use as mask during track,
 * so change this variable by 1 << N
 */
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
#define MML_RECORD_NUM		(1 << 10)
#else
#define MML_RECORD_NUM		(1 << 8)
#endif
#define MML_RECORD_NUM_MASK	(MML_RECORD_NUM - 1)

#define MML_CRC_CNT	1024

int mml_pipe0_dest_crc;
module_param(mml_pipe0_dest_crc, int, 0644);
int mml_pipe1_dest_crc;
module_param(mml_pipe1_dest_crc, int, 0644);

/* crc compare, support only dest crc */
int mml_crc_cmp;
module_param(mml_crc_cmp, int, 0644);
int mml_crc_cmp_p0;
module_param(mml_crc_cmp_p0, int, 0644);
int mml_crc_cmp_p1;
module_param(mml_crc_cmp_p1, int, 0644);
int mml_crc_err;
module_param(mml_crc_err, int, 0644);
int mml_crc_test;
module_param(mml_crc_test, int, 0644);

/* mml_freq, always update */
int mml_freq_for_tppa;
module_param(mml_freq_for_tppa, int, 0644);

int mml_opp_rsv = 4;
module_param(mml_opp_rsv, int, 0644);

struct mml_dpc {
	atomic_t task_cnt;
	atomic_t exc_pw_cnt[mml_max_sys];
	atomic_t dc_force_cnt[mml_max_sys];
	struct mutex dpc_mutex[mml_max_sys];
};

struct mml_sys_state {
	atomic_t dl_ref;
	atomic_t racing_ref;
	u8 sys_id;
};

struct mml_dev {
	struct platform_device *pdev;
	struct mml_comp *comps[MML_MAX_COMPONENTS];
	struct mml_sys_state sys_state[mml_max_sys];
	struct mml_sys_qos qos[mml_max_sys];
	u16 port_srt_bw[mml_max_sys][MML_MAX_PORT];
	u16 port_hrt_bw[mml_max_sys][MML_MAX_PORT];
	u32 vcp_ref;
	struct mutex sys_state_mutex;
	struct mml_sys *sys;
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clts[MML_MAX_CMDQ_CLTS];
	u8 cmdq_clt_cnt;

	u32 sw_ver;
	atomic_t drm_cnt;
	struct mml_drm_ctx *drm_ctx;
	atomic_t dle_cnt;
	struct mml_dle_ctx *dle_ctx;
	struct mml_topology_cache *topology;
	struct mutex ctx_mutex;
	struct mutex clock_mutex;

	bool dl_en;
	bool racing_en;
	bool dpc_disable;
	bool v4l2_en;
	bool smmu_en;

	/* sram operation */
	struct slbc_data sram_data[mml_sram_mode_total];
	s32 sram_cnt[mml_sram_mode_total];
	struct mutex sram_mutex;
	/* The height of racing mode for each output tile in pixel. */
	u8 racing_height;
	/* inline rotate sync event */
	u16 event_mml_ready;
	u16 event_disp_ready;
	u16 event_mml_stop;
	u16 event_mml_target;

	/* wack lock to prevent system off */
	struct wakeup_source *wake_lock;
	s32 wake_ref;
	struct mutex wake_ref_mutex;

	/* mml record to tracking task */
	struct dentry *record_entry;
	struct mml_record records[MML_RECORD_NUM];
	struct mutex record_mutex;
	u16 record_idx;

	struct mml_dpc dpc;
	void (*kick_idle_cb)(void *disp_crtc);
	void *disp_crtc;

	bool tablet_ext;

	struct device *mmu_dev; /* for dmabuf to iova */
	struct device *mmu_dev_sec; /* for secure dmabuf to secure iova */
	struct mml_v4l2_dev *v4l2_dev;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	struct mutex frm_dump_mutex;
	struct mml_frm_dump_data frm_dumps[mml_max_sys][mml_frm_dump_count];
	/* bits enable using enum mml_frm_dump_buf */
	u16 frm_dump_buf[mml_max_sys];
	enum mml_sys_id frm_dump_opt_sysid;
	enum mml_frm_dump_buf frm_dump_opt_bufid;
#endif
	s32 gce_thread_cnt;
};

int mml_comp_add(u32 id, struct device *dev, const struct component_ops *ops)
{
	int ret = component_add(dev, ops);

	if (ret)
		mml_err("failed to add comp %u", id);
	else
		mml_msg("component add id %u", id);

	return ret;
}

struct platform_device *mml_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *mml_node;
	struct platform_device *mml_pdev;

	mml_node = of_parse_phandle(dev->of_node, "mediatek,mml", 0);
	if (!mml_node) {
		mml_err("%s cannot get mml node", __func__);
		dev_err(dev, "cannot get mml node\n");
		return NULL;
	}

	mml_pdev = of_find_device_by_node(mml_node);
	of_node_put(mml_node);
	if (WARN_ON(!mml_pdev)) {
		mml_err("%s cannot get mml node", __func__);
		dev_err(dev, "mml pdev failed\n");
		return NULL;
	}

	return mml_pdev;
}
EXPORT_SYMBOL_GPL(mml_get_plat_device);

static enum mml_sys_id mml_mode_to_sysid(enum mml_mode mode)
{
	/* In current implementation, only decouple2 uses mml-tile, which is mml_sys_tile.
	 * Other mml modes use mml_sys_frame.
	 *
	 * This rule maybe change if mml-tile support direct link or racing.
	 */

	return mode == MML_MODE_MML_DECOUPLE2 ? mml_sys_tile : mml_sys_frame;
}

s32 mml_dev_get_couple_cnt(struct mml_dev *mml)
{
	s32 cnt;

	/* In current implementation, only mml_sys_frame support direct link or racing mode.
	 * Thus only check sys primary.
	 */

	mutex_lock(&mml->sys_state_mutex);

	cnt = atomic_read(&mml->sys_state[mml_sys_frame].dl_ref);
	if (cnt) {
		mml_mmp(couple, MMPROFILE_FLAG_PULSE,
			(MML_MODE_DIRECT_LINK << 16) | mml_sys_frame, cnt);
		goto done;
	}

	cnt = atomic_read(&mml->sys_state[mml_sys_frame].racing_ref);
	mml_mmp(couple, MMPROFILE_FLAG_PULSE,
			(MML_MODE_RACING << 16) | mml_sys_frame, cnt);
done:
	mutex_unlock(&mml->sys_state_mutex);

	return cnt;
}

s32 mml_dev_couple_inc(struct mml_dev *mml, enum mml_mode mode)
{
	s32 cnt = -1;
	enum mml_sys_id sys_id = mml_mode_to_sysid(mode);

	mutex_lock(&mml->sys_state_mutex);

	if (mode == MML_MODE_DIRECT_LINK)
		cnt = atomic_inc_return(&mml->sys_state[sys_id].dl_ref);
	else if (mode == MML_MODE_RACING)
		cnt = atomic_inc_return(&mml->sys_state[sys_id].racing_ref);

	mml_mmp(couple, MMPROFILE_FLAG_PULSE, (mode << 16) | sys_id, cnt);
	mutex_unlock(&mml->sys_state_mutex);

	return cnt;
}

#if IS_ENABLED(CONFIG_VHOST_CMDQ)
void cmdq_set_client(struct mml_dev *mml)
{
	int cnt;

	if (!mml->gce_thread_cnt) {
		mml_err("%s gce_thread_cnt is %u!", __func__,mml->gce_thread_cnt);
		return;
	}

	for (cnt = 0; cnt < mml->gce_thread_cnt; cnt++)
		vhost_cmdq_set_client((void *)(mml->cmdq_clts[cnt]), 0);
}
#endif

s32 mml_dev_couple_dec(struct mml_dev *mml, enum mml_mode mode)
{
	s32 cnt = 0;
	enum mml_sys_id sys_id = mml_mode_to_sysid(mode);

	mutex_lock(&mml->sys_state_mutex);

	if (mode == MML_MODE_DIRECT_LINK)
		cnt = atomic_dec_return(&mml->sys_state[sys_id].dl_ref);
	else if (mode == MML_MODE_RACING)
		cnt = atomic_dec_return(&mml->sys_state[sys_id].racing_ref);

	if (!cnt)
		mml_msg("%s sys id %u", __func__, mml->sys_state[sys_id].sys_id);

	mml_mmp(couple, MMPROFILE_FLAG_PULSE, (mode << 16) | sys_id, cnt);
	mutex_unlock(&mml->sys_state_mutex);

	return cnt;
}

void mml_qos_init(struct mml_dev *mml, struct platform_device *pdev, u32 sysid)
{
	struct device *dev = &pdev->dev;
	struct mml_sys_qos *sysqos = &mml->qos[sysid];
	struct dev_pm_opp *opp;
	int num;
	unsigned long freq = 0;
	u32 i;

	mml_msg("%s sysid %u", __func__, sysid);
	mutex_init(&sysqos->qos_mutex);

	/* Create opp table from dts */
	dev_pm_opp_of_add_table(dev);

	/* Get regulator instance by name. */
	sysqos->reg = devm_regulator_get_optional(dev, "mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(sysqos->reg)) {
		sysqos->reg = NULL;
		sysqos->dvfs_clk = devm_clk_get(dev, "mmdvfs_clk");
		if (IS_ERR_OR_NULL(sysqos->dvfs_clk)) {
			mml_err("%s get mmdvfs clk failed %d",
				__func__, (int)PTR_ERR(sysqos->dvfs_clk));
			sysqos->dvfs_clk = NULL;
			return;
		}
		mml_log("%s support mmdvfs clk", __func__);
	} else {
		mml_log("%s support mmdvfs regulator", __func__);
	}

	num = dev_pm_opp_get_opp_count(dev); /* number of available opp */
	if (num <= 0) {
		mml_err("%s no available opp table %d", __func__, num);
		return;
	}

	sysqos->opp_cnt = (u32)num;
	if (sysqos->opp_cnt > ARRAY_SIZE(sysqos->opp_speeds)) {
		mml_err("%s opp num more than table size %u %u",
			__func__, sysqos->opp_cnt, (u32)ARRAY_SIZE(sysqos->opp_speeds));
		sysqos->opp_cnt = ARRAY_SIZE(sysqos->opp_speeds);
	}

	i = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {

		if (i >= sysqos->opp_cnt)
			break;

		/* available freq from table, store in MHz */
		sysqos->opp_speeds[i] = (u32)div_u64(freq, 1000000) - mml_opp_rsv;
		sysqos->opp_volts[i] = dev_pm_opp_get_voltage(opp);
		sysqos->freq_max = sysqos->opp_speeds[i];
		mml_log("mml%u opp %u: %uMHz\t%d",
			sysid, i, sysqos->opp_speeds[i], sysqos->opp_volts[i]);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

static void mml_dvfs_vcp_enable(struct mml_dev *mml, enum mml_sys_id sysid)
{
	mml->vcp_ref++;
	if (mml->vcp_ref == 1) {
		mml_mmp(mmdvfs, MMPROFILE_FLAG_START, sysid, 0);
		mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MML);
	}
}

static void mml_dvfs_vcp_disable(struct mml_dev *mml, enum mml_sys_id sysid)
{
	mml->vcp_ref--;
	if (mml->vcp_ref == 0) {
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MML);
		mml_mmp(mmdvfs, MMPROFILE_FLAG_END, sysid, 0);
	}
}

u32 mml_qos_update_tput(struct mml_dev *mml, bool dpc, enum mml_sys_id sysid, bool enable)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	struct mml_sys_qos *sysqos;
	u32 tput = 0, i;
	int volt;

	if (unlikely(!tp))
		return 0;

	sysqos = &tp->qos[sysid];
	if (!sysqos->reg && !sysqos->dvfs_clk)
		return 0;

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++) {
		if (!tp->path_clts[i].sys_en_ref[sysid])
			continue;
		/* select max one across clients */
		tput = max(tput, tp->path_clts[i].throughput);
	}

	for (i = 0; i < sysqos->opp_cnt; i++) {
		if (tput < sysqos->opp_speeds[i])
			break;
	}
	i = min(i, sysqos->opp_cnt - 1);
	volt = sysqos->opp_volts[i];

	if (mml_freq_for_tppa)
		mml_update_freq_status(sysqos->opp_speeds[i]);

	if (!dpc && sysqos->dvfs_clk && enable)
		mml_dvfs_vcp_enable(mml, sysid);

	if (sysqos->current_volt == volt)	/* skip for better performance */
		goto done;
	sysqos->current_level = i;

	mml_msg_qos("%s sys %u dvfs update %u to %u(%u)by tput %u",
		__func__, sysid, tp->qos[sysid].current_volt, volt, sysqos->opp_speeds[i], tput);
	tp->qos[sysid].current_volt = volt;
	mml_trace_begin("mml_volt_%u", volt);

#ifndef MML_FPGA
	if (dpc)
		goto no_dvfs;

	if (sysqos->reg) {
		int ret = regulator_set_voltage(sysqos->reg, volt, INT_MAX);

		if (ret)
			mml_err("%s sys %u fail to set volt %d",
				__func__, sysid, volt);
		else
			mml_msg("%s sys %u volt %d (%u) tput %u",
				__func__, sysid, volt, i, tput);
	} else if (sysqos->dvfs_clk) {
		/* set dvfs clock rate by unit Hz */
		int ret = clk_set_rate(sysqos->dvfs_clk, sysqos->opp_speeds[i] * 1000000);

		if (ret)
			mml_err("%s sys %u %s fail to set rate %uMHz error %d cnt %u",
				__func__, sysid, enable ? "on" : "off", sysqos->opp_speeds[i],
				ret, mml->vcp_ref);
		else
			mml_msg("%s sys %u %s rate %uMHz (%u) tput %u cnt %u",
				__func__, sysid, enable ? "on" : "off", sysqos->opp_speeds[i],
				i, tput, mml->vcp_ref);
	}

no_dvfs:
#endif
	mml_trace_end();

	mml_update_freq_status(sysqos->opp_speeds[i]);

done:
	if (!dpc && sysqos->dvfs_clk) {
		if (!enable)
			mml_dvfs_vcp_disable(mml, sysid);
		mml_msg("%s vcp ref sys %u ref %u %s",
			__func__, sysid, mml->vcp_ref, enable ? "on" : "off");
	}
	return volt;
}

u32 mml_qos_update_sys(struct mml_dev *mml, bool dpc,
	const struct mml_topology_path *path, bool enable)
{
	u32 sysid, tput = 0;
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);

	if (unlikely(!tp))
		return 0;

	/* for all mmlsys, update the count to current path client reference count,
	 * so that mml_qos_update_tput api could update throughput to running client.
	 */
	for (sysid = 0; sysid < mml_max_sys; sysid++) {
		if (!path->sys_en[sysid])
			continue;
		if (enable)
			tp->path_clts[path->clt_id].sys_en_ref[sysid]++;
		else
			tp->path_clts[path->clt_id].sys_en_ref[sysid]--;
	}

	/* update throughput to the mmlsys(s) which used in this path */
	for (sysid = 0; sysid < mml_max_sys; sysid++) {
		if (!path->sys_en[sysid])
			continue;
		tput = max(tput, mml_qos_update_tput(mml, dpc, sysid, enable));
	}

	return tput;
}

static void create_dev_topology_locked(struct mml_dev *mml)
{
	/* make sure topology ready before client can use mml */
	if (!mml->topology) {
		mml->topology = mml_topology_create(mml, mml->pdev,
			mml->cmdq_clts, mml->cmdq_clt_cnt);
		if (!IS_ERR_OR_NULL(mml->topology))
			mml->topology->qos = mml->qos;
	}
	if (IS_ERR(mml->topology))
		mml_err("topology create fail %ld", PTR_ERR(mml->topology));
}

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_param *disp,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_drm_param *disp))
{
	struct mml_drm_ctx *ctx;

	mutex_lock(&mml->ctx_mutex);

	create_dev_topology_locked(mml);
	if (IS_ERR(mml->topology)) {
		ctx = ERR_CAST(mml->topology);
		goto exit;
	}

	if (atomic_inc_return(&mml->drm_cnt) == 1)
		mml->drm_ctx = ctx_create(mml, disp);
	ctx = mml->drm_ctx;

exit:
	mutex_unlock(&mml->ctx_mutex);
	return ctx;
}

void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx))
{
	struct mml_drm_ctx *ctx;
	int cnt;

	mutex_lock(&mml->ctx_mutex);
	ctx = mml->drm_ctx;
	cnt = atomic_dec_if_positive(&mml->drm_cnt);
	if (cnt == 0)
		mml->drm_ctx = NULL;
	mutex_unlock(&mml->ctx_mutex);
	if (cnt == 0)
		ctx_release(ctx);

	WARN_ON(cnt < 0);
}

struct mml_dle_ctx *mml_dev_get_dle_ctx(struct mml_dev *mml,
	struct mml_dle_param *dl,
	void (*ctx_setup)(struct mml_dle_ctx *ctx, struct mml_dle_param *dl))
{
	struct mml_dle_ctx *ctx;

	mutex_lock(&mml->ctx_mutex);

	create_dev_topology_locked(mml);
	if (IS_ERR(mml->topology)) {
		ctx = ERR_CAST(mml->topology);
		goto exit;
	}

	if (!mml->dle_ctx) {
		mml->dle_ctx = mml_dle_ctx_create(mml);
		if (IS_ERR(mml->dle_ctx)) {
			ctx = mml->dle_ctx;
			mml->dle_ctx = NULL;
			goto exit;
		}
	}
	ctx = mml->dle_ctx;

	if (atomic_inc_return(&mml->dle_cnt) == 1)
		ctx_setup(ctx, dl);

exit:
	mutex_unlock(&mml->ctx_mutex);
	return ctx;
}

void mml_dev_put_dle_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_dle_ctx *ctx))
{
	struct mml_dle_ctx *ctx;
	int cnt;

	mutex_lock(&mml->ctx_mutex);
	ctx = mml->dle_ctx;
	cnt = atomic_dec_if_positive(&mml->dle_cnt);
	if (cnt == 0)
		mml->dle_ctx = NULL;
	mutex_unlock(&mml->ctx_mutex);
	if (cnt == 0)
		ctx_release(ctx);

	WARN_ON(cnt < 0);
}

struct mml_m2m_ctx *mml_dev_create_m2m_ctx(struct mml_dev *mml,
	struct mml_m2m_ctx *(*ctx_create)(struct mml_dev *mml))
{
	struct mml_m2m_ctx *ctx;

	mutex_lock(&mml->ctx_mutex);

#if IS_ENABLED(CONFIG_VHOST_CMDQ)
	cmdq_set_client(mml);
#endif

	create_dev_topology_locked(mml);
	if (IS_ERR(mml->topology)) {
		ctx = ERR_CAST(mml->topology);
		goto exit;
	}

	ctx = ctx_create(mml);

exit:
	mutex_unlock(&mml->ctx_mutex);
	return ctx;
}

struct mml_v4l2_dev *mml_get_v4l2_dev(struct mml_dev *mml)
{
	return mml->v4l2_dev;
}

struct mml_topology_cache *mml_topology_get_cache(struct mml_dev *mml)
{
	return IS_ERR(mml->topology) ? NULL : mml->topology;
}
EXPORT_SYMBOL_GPL(mml_topology_get_cache);

struct mml_comp *mml_dev_get_comp_by_id(struct mml_dev *mml, u32 id)
{
	return mml->comps[id];
}
EXPORT_SYMBOL_GPL(mml_dev_get_comp_by_id);

void *mml_get_sys(struct mml_dev *mml)
{
	return mml->sys;
}

phys_addr_t mml_get_node_base_pa(struct platform_device *pdev, const char *name,
	u32 idx, void __iomem **base)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct resource res;
	phys_addr_t base_pa = 0;

	node = of_parse_phandle(dev->of_node, name, idx);
	if (!node)
		goto done;

	if (of_address_to_resource(node, 0, &res))
		goto done;

	base_pa = res.start;
	*base = of_iomap(node, 0);
	mml_log("%s%u %pa %p", name, idx, &base_pa, *base);

done:
	if (node)
		of_node_put(node);
	return base_pa;
}

static int master_bind(struct device *dev)
{
	return component_bind_all(dev, NULL);
}

static void master_unbind(struct device *dev)
{
	component_unbind_all(dev, NULL);
}

static const struct component_master_ops mml_master_ops = {
	.bind = master_bind,
	.unbind = master_unbind,
};

static int comp_compare(struct device *dev, int subcomponent, void *data)
{
	u32 comp_id;
	u32 match_id = (u32)(uintptr_t)data;

	dev_dbg(dev, "%s %d -- match_id:%d\n", __func__,
		subcomponent, match_id);
	if (!of_mml_read_comp_id_index(dev->of_node, subcomponent, &comp_id)) {
		dev_dbg(dev, "%s -- comp_id:%d\n", __func__, comp_id);
		return match_id == comp_id;
	}
	return 0;
}

static inline int of_mml_read_comp_count(const struct device_node *np,
	u32 *count)
{
	return of_property_read_u32(np, "comp-count", count);
}

static int comp_master_init(struct device *dev, struct mml_dev *mml)
{
	struct component_match *match = NULL;
	u32 comp_count = 0;
	ulong i;
	int ret;

	if (of_mml_read_comp_count(dev->of_node, &comp_count)) {
		dev_err(dev, "no comp-count in dts node\n");
		return -EINVAL;
	}
	dev_notice(dev, "%s -- comp-count:%d\n", __func__, comp_count);
	/* engine id 0 leaves empty, so begin with 1 */
	for (i = 1; i < comp_count; i++)
		component_match_add_typed(dev, &match, comp_compare, (void *)i);

	ret = component_master_add_with_match(dev, &mml_master_ops, match);
	if (ret)
		dev_err(dev, "failed to add match: %d\n", ret);

	return ret;
}

static void comp_master_deinit(struct device *dev)
{
	component_master_del(dev, &mml_master_ops);
}

static inline int of_mml_read_comp_name_index(const struct device_node *np,
	int index, const char **name)
{
	return of_property_read_string_index(np, "comp-names", index, name);
}

static const char *comp_clock_names = "comp-clock-names";

static s32 comp_init(struct platform_device *pdev, struct mml_comp *comp,
	const char *clkpropname)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	struct property *prop;
	const char *clkname;
	int i, ret;

	mml_msg("%s comp id %u %s (%s)", __func__, comp->id, comp->name, clkpropname);

	comp->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (!res) {
		dev_err(dev, "failed to get resource\n");
		return -EINVAL;
	}
	comp->base_pa = res->start;

	/* default set to mml-frame, maybe change in tp_init_cache */
	comp->sysid = mml_sys_frame;

	/* ignore clks if clkpropname is null as subcomponent */
	if (!clkpropname)
		return 0;

	ret = 0;
	if (of_property_count_strings(node, clkpropname) > 0) {
		/* get named clks as component or subcomponent */
		i = 0;
		of_property_for_each_string(node, clkpropname, prop, clkname) {
			if (i >= ARRAY_SIZE(comp->clks)) {
				dev_err(dev, "out of clk array size %d in %s\n",
					i, node->full_name);
				ret = -E2BIG;
				break;
			}
			comp->clks[i] = of_clk_get_by_name(node, clkname);
			if (IS_ERR(comp->clks[i])) {
				dev_err(dev, "failed to get clk %s in %s err %ld\n",
					clkname, node->full_name,
					PTR_ERR(comp->clks[i]));
			} else {
				i++;
			}
		}
		if (i < ARRAY_SIZE(comp->clks))
			comp->clks[i] = ERR_PTR(-ENOENT);
	} else if (!comp->sub_idx) {
		/* get all clks as component */
		for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
			comp->clks[i] = of_clk_get(node, i);
			if (IS_ERR(comp->clks[i]))
				break;
		}
		if (!i)
			dev_info(dev, "no clks in node %s\n", node->full_name);
	} else {
		/* no named clks as subcomponent */
		dev_info(dev, "no %s property in node %s\n",
			 clkpropname, node->full_name);
	}

#ifdef MML_FPGA
	if (ret < 0) {
		mml_log("%s result %d and continue for fpga", __func__, ret);
		return 0;
	}
#endif
	return ret;
}

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	int ret;

	ret = of_mml_read_comp_id_index(node, 0, &comp_id);
	if (ret) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	of_mml_read_comp_name_index(node, 0, &comp->name);
	return comp_init(comp_pdev, comp, comp_clock_names);
}

s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	const char *name_ptr = NULL;
	char name[32] = "";
	int ret;

	ret = of_mml_read_comp_id_index(node, subcomponent, &comp_id);
	if (ret) {
		dev_err(dev, "no comp-ids in subcomponent %d %s: %d\n",
			subcomponent, node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	comp->sub_idx = subcomponent;
	if (!of_mml_read_comp_name_index(node, subcomponent, &comp->name)) {
		ret = snprintf(name, sizeof(name), "%s-clock-names", comp->name);
		if (ret >= sizeof(name)) {
			dev_err(dev, "len:%d over name size:%lu",
				ret, (unsigned long)sizeof(name));
			name[sizeof(name) - 1] = '\0';
		}
		name_ptr = name;
	} else if (!comp->sub_idx) {
		name_ptr = comp_clock_names;
	}
	return comp_init(comp_pdev, comp, name_ptr);
}

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev)
{
	struct platform_device *larb_pdev;
	struct of_phandle_args larb_args;
	struct resource res;

	/* parse larb node and port from dts */
	if (of_parse_phandle_with_fixed_args(dev->of_node, "mediatek,larb",
		1, 0, &larb_args)) {
		mml_err("%s fail to parse mediatek,larb comp %u %s",
			__func__, comp->id,
			comp->name ? comp->name : "");
		return -ENOENT;
	}
	comp->larb_port = larb_args.args[0];
	if (!of_address_to_resource(larb_args.np, 0, &res))
		comp->larb_base = res.start;

	larb_pdev = of_find_device_by_node(larb_args.np);
	of_node_put(larb_args.np);
	if (WARN_ON(!larb_pdev)) {
		mml_log("%s no larb and defer", __func__);
		return -EPROBE_DEFER;
	}
	/* larb dev for smi api */
	comp->larb_dev = &larb_pdev->dev;

	/* also do mmqos and mmdvfs since dma component do init here */
#ifndef MML_FPGA
	comp->icc_path = of_mtk_icc_get(dev, "mml_dma");
	if (IS_ERR_OR_NULL(comp->icc_path))
		comp->icc_path = NULL;
	else
		mml_log("%s %s supports qos",
			__func__, comp->name ? comp->name : "");

	comp->icc_dpc_path = of_mtk_icc_get(dev, "mml_dma_dpc");
	if (IS_ERR_OR_NULL(comp->icc_dpc_path))
		comp->icc_dpc_path = NULL;
	else
		mml_log("%s %s supports qos dpc",
			__func__, comp->name ? comp->name : "");

	comp->icc_stash_path = of_mtk_icc_get(dev, "mml_dma_stash");
	if (IS_ERR_OR_NULL(comp->icc_stash_path))
		comp->icc_stash_path = NULL;
	else
		mml_log("%s %s supports qos stash",
			__func__, comp->name ? comp->name : "");

	comp->icc_dpc_stash_path = of_mtk_icc_get(dev, "mml_dma_dpc_stash");
	if (IS_ERR_OR_NULL(comp->icc_dpc_stash_path))
		comp->icc_dpc_stash_path = NULL;
	else
		mml_log("%s %s supports qos dpc stash",
			__func__, comp->name ? comp->name : "");
#endif

	return 0;
}

s32 mml_comp_pw_enable(struct mml_comp *comp, const s8 mode)
{
	int ret = 0;

	comp->pw_cnt++;
	if (comp->pw_cnt > 1)
		return 0;
	if (comp->pw_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->pw_cnt);
		return -EINVAL;
	}

	if (!comp->larb_dev) {
		mml_err("%s no larb for comp %u", __func__, comp->id);
		return 0;
	}

#ifndef MML_FPGA
	mml_msg_dpc("%s comp %u pm_runtime_resume_and_get", __func__, comp->id);
	mml_mmp(pw_get, MMPROFILE_FLAG_PULSE, comp->id, 0);
	ret = pm_runtime_resume_and_get(comp->larb_dev);
	if (ret)
		mml_err("%s enable fail ret:%d", __func__, ret);
#endif

	return ret;
}

s32 mml_comp_pw_disable(struct mml_comp *comp, const s8 mode)
{
	comp->pw_cnt--;
	if (comp->pw_cnt > 0)
		return 0;
	if (comp->pw_cnt < 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->pw_cnt);
		return -EINVAL;
	}

	if (!comp->larb_dev) {
		mml_err("%s no larb for comp %u", __func__, comp->id);
		return 0;
	}

#ifndef MML_FPGA
	mml_msg_dpc("%s comp %u pm_runtime_put_sync", __func__, comp->id);
	mml_mmp(pw_put, MMPROFILE_FLAG_PULSE, comp->id, 0);
	pm_runtime_put_sync(comp->larb_dev);
#endif

	return 0;
}

s32 mml_comp_clk_enable(struct mml_comp *comp)
{
	u32 i;
	int ret;

	comp->clk_cnt++;
	if (comp->clk_cnt > 1)
		return 0;
	if (comp->clk_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->clk_cnt);
		return -EINVAL;
	}

	mml_mmp(clk_enable, MMPROFILE_FLAG_START, comp->id, 0);
	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR_OR_NULL(comp->clks[i]))
			break;
		ret = clk_prepare_enable(comp->clks[i]);
		if (ret)
			mml_err("%s clk_prepare_enable fail %d", __func__, ret);
	}
	mml_mmp(clk_enable, MMPROFILE_FLAG_END, comp->id, 0);

	return 0;
}

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

s32 mml_comp_clk_disable(struct mml_comp *comp, bool dpc)
{
	u32 i;

	comp->clk_cnt--;
	if (comp->clk_cnt > 0)
		return 0;
	if (comp->clk_cnt < 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->clk_cnt);
		return -EINVAL;
	}

	mml_mmp(clk_disable, MMPROFILE_FLAG_START, comp->id, 0);
	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR_OR_NULL(comp->clks[i]))
			break;
		clk_disable_unprepare(comp->clks[i]);
	}
	mml_mmp(clk_disable, MMPROFILE_FLAG_END, comp->id, 0);

	return 0;
}

struct device *mml_smmu_get_shared_device(struct device *dev, const char *name)
{
	struct device_node *node;
	struct platform_device *shared_pdev;
	struct device *shared_dev = dev;

	node = of_parse_phandle(dev->of_node, name, 0);
	if (node) {
		shared_pdev = of_find_device_by_node(node);
		if (shared_pdev)
			shared_dev = &shared_pdev->dev;
	}

	return shared_dev;
}

s32 mml_dpc_task_cnt_get(struct mml_task *task)
{
	struct mml_dev *mml = task->config->mml;

	return atomic_read(&mml->dpc.task_cnt);
}

void mml_dpc_task_cnt_inc(struct mml_task *task)
{
	struct mml_dev *mml = task->config->mml;
	s32 cur_task_cnt = atomic_inc_return(&mml->dpc.task_cnt);

	if (cur_task_cnt == 1) {
		const struct mml_topology_path *path = task->config->path[0];

		mml_msg_dpc("%s scenario in, dpc start", __func__);
		mml_clock_lock(mml);
		call_hw_op(path->mmlsys, mminfra_pw_enable);
		mml_dpc_exc_keep(mml, path->mmlsys->sysid);
		call_hw_op(path->mmlsys, pw_enable, task->config->info.mode);
		if (path->mmlsys2)
			call_hw_op(path->mmlsys2, pw_enable, task->config->info.mode);
		mml_mmp(dpc, MMPROFILE_FLAG_START, 1, 0);
		mml_dpc_exc_release(mml, path->mmlsys->sysid);
		call_hw_op(path->mmlsys, mminfra_pw_disable);
		mml_clock_unlock(mml);
	}
}

void mml_dpc_task_cnt_dec(struct mml_task *task)
{
	struct mml_dev *mml = task->config->mml;
	s32 cur_task_cnt = atomic_dec_return(&mml->dpc.task_cnt);

	if (cur_task_cnt < 0) {
		cur_task_cnt = 0;
		atomic_set(&mml->dpc.task_cnt, 0);
		mml_err("%s task_cnt < 0", __func__);
	}

	if (cur_task_cnt == 0) {
		const struct mml_topology_path *path = task->config->path[0];

		mml_msg_dpc("%s scenario out, dpc end", __func__);
		mml_clock_lock(mml);
		call_hw_op(path->mmlsys, mminfra_pw_enable);
		mml_dpc_exc_keep(mml, path->mmlsys->sysid);
		mml_mmp(dpc, MMPROFILE_FLAG_END, 0, 0);
		if (path->mmlsys2)
			call_hw_op(path->mmlsys2, pw_disable,
				task->config->info.mode);
		call_hw_op(path->mmlsys, pw_disable, task->config->info.mode);
		mml_dpc_exc_release(mml, path->mmlsys->sysid);
		call_hw_op(path->mmlsys, mminfra_pw_disable);
		mml_clock_unlock(mml);
	}
}

void mml_dpc_exc_keep(struct mml_dev *mml, u32 sysid)
{
	s32 cur_exc_pw_cnt;

	mutex_lock(&mml->dpc.dpc_mutex[sysid]);

	cur_exc_pw_cnt = atomic_inc_return(&mml->dpc.exc_pw_cnt[sysid]);
	mml_mmp(dpc_exception_flow, MMPROFILE_FLAG_PULSE, 0x10000 | sysid, cur_exc_pw_cnt);

	if (cur_exc_pw_cnt > 1)
		goto done;
	if (cur_exc_pw_cnt <= 0) {
		mml_err("%s cnt %d", __func__, cur_exc_pw_cnt);
		goto done;
	}

	mml_mmp(dpc_exception_flow, MMPROFILE_FLAG_START, 1, 0);
	mml_dpc_power_keep(sysid);

done:
	mutex_unlock(&mml->dpc.dpc_mutex[sysid]);
}

void mml_dpc_exc_release(struct mml_dev *mml, u32 sysid)
{
	s32 cur_exc_pw_cnt;

	mutex_lock(&mml->dpc.dpc_mutex[sysid]);

	cur_exc_pw_cnt = atomic_dec_return(&mml->dpc.exc_pw_cnt[sysid]);
	mml_mmp(dpc_exception_flow, MMPROFILE_FLAG_PULSE, sysid, cur_exc_pw_cnt);

	if (cur_exc_pw_cnt > 0)
		goto done;
	if (cur_exc_pw_cnt < 0) {
		mml_err("%s cnt %d", __func__, cur_exc_pw_cnt);
		goto done;
	}

	mml_mmp(dpc_exception_flow, MMPROFILE_FLAG_END, 0, 0);
	mml_dpc_power_release(sysid);

done:
	mutex_unlock(&mml->dpc.dpc_mutex[sysid]);
}

void mml_dpc_exc_keep_task(struct mml_task *task, const struct mml_topology_path *path)
{
	struct mml_frame_config *cfg = task->config;

	mml_dpc_exc_keep(cfg->mml, path->mmlsys->sysid);
	if (path->mmlsys2)
		mml_dpc_exc_keep(cfg->mml, path->mmlsys2->sysid);
}

void mml_dpc_exc_release_task(struct mml_task *task, const struct mml_topology_path *path)
{
	struct mml_frame_config *cfg = task->config;

	if (path->mmlsys2)
		mml_dpc_exc_release(cfg->mml, path->mmlsys2->sysid);
	mml_dpc_exc_release(cfg->mml, path->mmlsys->sysid);
}

void mml_dpc_dc_enable(struct mml_dev *mml, u32 sysid, bool dcen)
{
	s32 cur_dc_force_cnt;

	if (dcen) {
		cur_dc_force_cnt = atomic_inc_return(&mml->dpc.dc_force_cnt[sysid]);

		if (cur_dc_force_cnt > 1)
			return;
		if (cur_dc_force_cnt <= 0) {
			mml_err("%s  cnt %d", __func__, cur_dc_force_cnt);
			return;
		}

		mml_mmp(dpc_dc, MMPROFILE_FLAG_START, sysid, 0);
	} else {
		cur_dc_force_cnt = atomic_dec_return(&mml->dpc.dc_force_cnt[sysid]);

		if (cur_dc_force_cnt > 0)
			return;
		if (cur_dc_force_cnt < 0) {
			mml_err("%s  cnt %d", __func__, cur_dc_force_cnt);
			return;
		}

		mml_mmp(dpc_dc, MMPROFILE_FLAG_END, sysid, 0);
	}

	mml_msg_dpc("%s group en sys %u group %s",
		__func__, sysid, dcen ? "false" : "true");
	mml_dpc_group_enable(sysid, !dcen);
}

void mml_pw_set_kick_cb(struct mml_dev *mml,
	void (*kick_idle_cb)(void *disp_crtc), void *disp_crtc)
{
	mml->kick_idle_cb = kick_idle_cb;
	mml->disp_crtc = disp_crtc;
}

void mml_pw_kick_idle(struct mml_dev *mml)
{
	if (mml->kick_idle_cb)
		mml->kick_idle_cb(mml->disp_crtc);
}

/* mml_calc_bw - calculate bandwidth by giving pixel and current throughput
 *
 * @data:	data size in bytes, size to each dma port
 * @pixel:	pixel count, one of max pixel count of all dma ports
 * @throughput:	throughput (frequency) in Mhz, to handling frame in time
 *
 * Note: throughput calculate by following formula:
 *		max_pixel / (end_time - current_time)
 *	which represents necessary cycle (or frequency in MHz) to process
 *	pixels in this time slot (before end time from current time).
 */
static u32 mml_calc_bw(struct mml_dev *mml, u64 data, u32 pixel, u64 throughput)
{
	/* ocucpied bw efficiency is 1.33 while accessing DRAM
	 * also 1.3 overhead to secure ostd
	 */

	if (mml->smmu_en)
		data = (u64)div_u64(data * throughput * 13, 10);
	else
		data = (u64)div_u64(data * 4 * throughput * 13, 3 * 10);
	if (!pixel)
		pixel = 1;

	return max_t(u32, MML_QOS_MIN_BW, min_t(u32, div_u64(data, pixel), MML_QOS_MAX_BW));
}

static u32 mml_calc_bw_couple(struct mml_dev *mml, u32 datasize)
{
	/* hrt bw: width * height * bpp * fps * 1.25 * 1.75 = HRT MB/s
	 *
	 * width * height * bpp = datasize in bytes
	 * the 1.25 (v-blanking) separate to * 10 / 8
	 * the 1.75 (occupy bandwidth for IR) separate to * 7 / 4
	 *
	 * so div_u64((u64)(datasize * 120 * 10 * 7) >> 3, 4 * 1000000)
	 */
	if (mml->smmu_en)
		return (u32)div_u64((u64)datasize * 3, 20000);
	else
		return (u32)div_u64((u64)datasize * 21, 80000);
}

void mml_comp_qos_calc(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[0];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	u32 datasize, srt_bw, hrt_bw, stash_srt_bw, stash_hrt_bw;
	bool hrt;

	datasize = comp->hw_ops->qos_datasize_get(task, ccfg);
	if (!datasize) {
		hrt = false;
		srt_bw = 0;
		hrt_bw = 0;
	} else if (cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK) {
		hrt = true;
		if (likely(mtk_mml_hrt_mode == MML_HRT_ENABLE) ||
			mtk_mml_hrt_mode == MML_HRT_OSTD_MAX ||
			mtk_mml_hrt_mode == MML_HRT_LIMIT ||
			mtk_mml_hrt_mode == MML_HRT_MMQOS) {
			srt_bw = mml_calc_bw_couple(cfg->mml, datasize);
			hrt_bw = (u32)((u64)datasize * 1000 / cfg->info.act_time);

			if (cfg->panel_w > dest->data.width)
				hrt_bw = (u32)((u64)hrt_bw * cfg->panel_w / dest->data.width);

			if (mtk_mml_hrt_mode == MML_HRT_LIMIT && hrt_bw < mml_hrt_bound) {
				srt_bw = 0;
				hrt_bw = 0;
			}
		} else {	/* MML_HRT_OSTD_ONLY */
			srt_bw = 0;
			hrt_bw = 0;
		}

		if ((unlikely(mml_qos & MML_QOS_FORCE_BW_MASK))) {
			srt_bw = mml_qos_force_bw;
			hrt_bw = srt_bw;
		}
	} else {
		hrt = false;
		srt_bw = mml_calc_bw(cfg->mml, datasize, cache->max_tput_pixel, throughput);
		if ((unlikely(mml_qos & MML_QOS_FORCE_BW_MASK)))
			srt_bw = mml_qos_force_bw;
		hrt_bw = 0;
	}

	stash_srt_bw = srt_bw;
	stash_hrt_bw = hrt_bw;
	comp->hw_ops->qos_stash_bw_get(comp, task, ccfg, &stash_srt_bw, &stash_hrt_bw);

	if (mml_stash_bw) {
		stash_srt_bw = mml_stash_bw & 0xfffe;
		stash_hrt_bw = (mml_stash_bw >> 16) & 0xfffe;
	}

	/* store for debug log */
	task->pipe[ccfg->pipe].bandwidth = max(srt_bw, task->pipe[ccfg->pipe].bandwidth);

	comp->srt_bw = max_t(u32, comp->srt_bw, srt_bw);
	comp->hrt_bw = max_t(u32, comp->hrt_bw, hrt_bw);
	comp->stash_srt_bw = max_t(u32, comp->stash_srt_bw, stash_srt_bw);
	comp->stash_hrt_bw = max_t(u32, comp->stash_hrt_bw, stash_hrt_bw);

	mml_msg("%s comp %u bw %u %u stash %u %u tput %u%s",
		__func__, comp->id, srt_bw, hrt_bw, stash_srt_bw, stash_hrt_bw, throughput,
		hrt ? " hrt" : "");
}

void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput, u32 tput_up)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_dev *mml = cfg->mml;
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	const u32 srt_bw = comp->srt_bw, hrt_bw = comp->hrt_bw;
	const u32 stash_srt_bw = comp->stash_srt_bw, stash_hrt_bw = comp->stash_hrt_bw;
	bool hrt = cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK;
	bool updated = false;

	/* store for debug log */
	task->pipe[ccfg->pipe].bandwidth = max(comp->srt_bw, task->pipe[ccfg->pipe].bandwidth);
	if (srt_bw == mml->port_srt_bw[comp->sysid][comp->larb_port] &&
		hrt_bw == mml->port_hrt_bw[comp->sysid][comp->larb_port])
		goto skip_update;

	mml_trace_begin("mml_comp%u_bw_%u_%u", comp->id, srt_bw, hrt_bw);
#ifndef MML_FPGA
	if (cfg->dpc) {
		u32 srt_icc, hrt_icc;

		if (mtk_mml_hrt_mode == MML_HRT_OSTD_MAX) {
			srt_icc = MBps_to_icc(srt_bw);
			hrt_icc = hrt_bw <= mml_hrt_bound ?
				MTK_MMQOS_MAX_BW : MBps_to_icc(hrt_bw);
		} else if (mtk_mml_hrt_mode == MML_HRT_OSTD_ONLY) {
			srt_icc = 0;
			hrt_icc = MTK_MMQOS_MAX_BW;
		} else if (mtk_mml_hrt_mode == MML_HRT_LIMIT) {
			if (hrt_bw < mml_hrt_bound) {
				srt_icc = 0;
				hrt_icc = MTK_MMQOS_MAX_BW;
			} else {
				srt_icc = MBps_to_icc(srt_bw);
				hrt_icc = MBps_to_icc(hrt_bw);
			}
		} else {
			/* MML_HRT_ENABLE, MML_HRT_MMQOS */
			srt_icc = MBps_to_icc(srt_bw);
			hrt_icc = MBps_to_icc(hrt_bw);
		}

		mtk_icc_set_bw(comp->icc_dpc_path, srt_icc, hrt_icc);
		if (comp->icc_stash_path)
			mtk_icc_set_bw(comp->icc_dpc_stash_path,
				MBps_to_icc(stash_srt_bw), MBps_to_icc(stash_hrt_bw));
	} else {
		mtk_icc_set_bw(comp->icc_path,
			MBps_to_icc(srt_bw), MBps_to_icc(hrt_bw));
		if (comp->icc_stash_path)
			mtk_icc_set_bw(comp->icc_stash_path,
				MBps_to_icc(stash_srt_bw), MBps_to_icc(stash_hrt_bw));
	}
#endif
	mml_trace_end();
	updated = true;
	mml->port_srt_bw[comp->sysid][comp->larb_port] = srt_bw;
	mml->port_hrt_bw[comp->sysid][comp->larb_port] = hrt_bw;

skip_update:
	if (cfg->dpc) {
		task->dpc_srt_bw[comp->sysid] += srt_bw;
		task->dpc_hrt_bw[comp->sysid] += hrt_bw;
		task->dpc_srt_write_bw[comp->sysid] += stash_srt_bw;
		task->dpc_hrt_write_bw[comp->sysid] += stash_hrt_bw;
	}

	mml_mmp(bandwidth, MMPROFILE_FLAG_PULSE, comp->id, (comp->srt_bw << 16) | comp->hrt_bw);

	mml_msg_qos("%s comp %u %s bw %u %u stash %u %u by throughput %u pixel %u%s%s dpc %u hrtmode %d",
		__func__, comp->id, comp->name, srt_bw, hrt_bw, stash_srt_bw, stash_hrt_bw,
		throughput, cache->max_tput_pixel,
		hrt ? " hrt" : "", updated ? " update" : "",
		task->config->dpc, mtk_mml_hrt_mode);
}

void mml_comp_qos_clear(struct mml_comp *comp, struct mml_task *task, bool dpc)
{
	struct mml_dev *mml = task->config->mml;

#ifndef MML_FPGA
	if (dpc) {
		mtk_icc_set_bw(comp->icc_dpc_path, 0, 0);
		if (comp->icc_dpc_stash_path)
			mtk_icc_set_bw(comp->icc_dpc_stash_path, 0, 0);
	} else {
		mtk_icc_set_bw(comp->icc_path, 0, 0);
		if (comp->icc_stash_path)
			mtk_icc_set_bw(comp->icc_stash_path, 0, 0);
	}
#endif
	comp->srt_bw = 0;
	comp->hrt_bw = 0;
	comp->stash_srt_bw = 0;
	comp->stash_hrt_bw = 0;
	mml->port_srt_bw[comp->sysid][comp->larb_port] = 0;
	mml->port_hrt_bw[comp->sysid][comp->larb_port] = 0;

	mml_mmp(bandwidth, MMPROFILE_FLAG_PULSE, comp->id, 0);

	mml_msg_qos("%s comp %u %s qos bw clear%s",
		__func__, comp->id, comp->name, dpc ? " dpc" : "");
}

static const struct mml_comp_hw_ops mml_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

void __iomem *mml_sram_get(struct mml_dev *mml, enum mml_sram_mode mode)
{
#ifndef MML_FPGA
	int ret;
	void __iomem *sram = NULL;

	mutex_lock(&mml->sram_mutex);

	if (!mml->sram_cnt[mode]) {
		ret = slbc_request(&mml->sram_data[mode]);
		if (ret < 0) {
			mml_err("%s request slbc fail %d", __func__, ret);
			goto done;
		}

		ret = slbc_power_on(&mml->sram_data[mode]);
		if (ret < 0) {
			mml_err("%s slbc power on fail %d", __func__, ret);
			goto done;
		}

		mml_msg("mml sram %#lx mode %d", (unsigned long)mml->sram_data[mode].paddr, mode);
	}

	mml->sram_cnt[mode]++;
	sram = mml->sram_data[mode].paddr;

done:
	mutex_unlock(&mml->sram_mutex);
	return sram;
#else
	return NULL;
#endif
}

void mml_sram_put(struct mml_dev *mml, enum mml_sram_mode mode)
{
#ifndef MML_FPGA
	mutex_lock(&mml->sram_mutex);

	mml->sram_cnt[mode]--;
	if (!mml->sram_cnt[mode]) {
		slbc_power_off(&mml->sram_data[mode]);
		slbc_release(&mml->sram_data[mode]);
		goto done;
	} else if (mml->sram_cnt[mode] < 0) {
		mml_err("%s sram slbc count wrong %d mode %d", __func__, mml->sram_cnt[mode], mode);
		goto done;
	}

done:
	mutex_unlock(&mml->sram_mutex);
#endif
}

struct device *mml_get_mmu_dev(struct mml_dev *mml, bool secure)
{
	return secure ? mml->mmu_dev_sec : mml->mmu_dev;
}

bool mml_dl_enable(struct mml_dev *mml)
{
	return mml->dl_en;
}
EXPORT_SYMBOL_GPL(mml_dl_enable);

bool mml_dpc_disable(struct mml_dev *mml)
{
	return mml->dpc_disable;
}
EXPORT_SYMBOL_GPL(mml_dpc_disable);

bool mml_racing_enable(struct mml_dev *mml)
{
	return mml->racing_en;
}
EXPORT_SYMBOL_GPL(mml_racing_enable);

bool mml_tablet_ext(struct mml_dev *mml)
{
	return mml->tablet_ext;
}
EXPORT_SYMBOL_GPL(mml_tablet_ext);

u8 mml_sram_get_racing_height(struct mml_dev *mml)
{
	return mml->racing_height;
}

u16 mml_ir_get_mml_ready_event(struct mml_dev *mml)
{
	return mml->event_mml_ready;
}

u16 mml_ir_get_disp_ready_event(struct mml_dev *mml)
{
	return mml->event_disp_ready;
}

u16 mml_ir_get_mml_stop_event(struct mml_dev *mml)
{
	return mml->event_mml_stop;
}

u16 mml_ir_get_target_event(struct mml_dev *mml)
{
	return mml->event_mml_target;
}

struct cmdq_client *mml_get_cmdq_clt(struct mml_dev *mml, u32 id)
{
	return mml->cmdq_clts[id];
}

void mml_dump_thread(struct mml_dev *mml)
{
	u32 i;

	for (i = 0; i < MML_MAX_CMDQ_CLTS; i++)
		if (mml->cmdq_clts[i])
			cmdq_thread_dump(mml->cmdq_clts[i]->chan, NULL, NULL, NULL);
}

void mml_clock_lock(struct mml_dev *mml)
{
	mutex_lock(&mml->clock_mutex);
}

void mml_clock_unlock(struct mml_dev *mml)
{
	mutex_unlock(&mml->clock_mutex);
}

void mml_lock_wake_lock(struct mml_dev *mml, bool lock)
{
	mutex_lock(&mml->wake_ref_mutex);
	if (lock) {
		mml->wake_ref++;
		if (mml->wake_ref == 1)
			__pm_stay_awake(mml->wake_lock);
		mml_mmp(wake_lock, MMPROFILE_FLAG_PULSE, mml->wake_ref, 0);
	} else {
		mml->wake_ref--;
		if (mml->wake_ref == 0)
			__pm_relax(mml->wake_lock);

		if (mml->wake_ref < 0)
			mml_err("%s wake_ref < 0", __func__);
		mml_mmp(wake_unlock, MMPROFILE_FLAG_PULSE, mml->wake_ref, 0);
	}
	mutex_unlock(&mml->wake_ref_mutex);

	if (mml->wake_ref > MML_WAKE_SAFE_CNT) {
		static bool aeeonce;

		mml_err("too many wake lock:%d", mml->wake_ref);

		if (!aeeonce) {
			aeeonce = true;
			mml_fatal("mml", "too many wake lock:%d", mml->wake_ref);
		}

	}
}

s32 mml_register_comp(struct device *master, struct mml_comp *comp)
{
	struct mml_dev *mml = dev_get_drvdata(master);

	if (mml->comps[comp->id]) {
		dev_err(master, "duplicated component id:%d\n", comp->id);
		return -EINVAL;
	}
	mml->comps[comp->id] = comp;
	comp->bound = true;

	if (!comp->hw_ops)
		comp->hw_ops = &mml_hw_ops;

	return 0;
}

void mml_unregister_comp(struct device *master, struct mml_comp *comp)
{
	struct mml_dev *mml = dev_get_drvdata(master);

	if (mml->comps[comp->id] == comp) {
		mml->comps[comp->id] = NULL;
		comp->bound = false;
	}
}

static void mml_record_crc_check(struct mml_task *task)
{
	if (mml_crc_cmp_p0 != task->dest_crc[0]) {
		mml_crc_err++;
		mml_err("CRC check job %u pipe 0 crc fail %#010x != %#010x error count %d",
			task->job.jobid, task->dest_crc[0], mml_crc_cmp_p0, mml_crc_err);
	}

	if (task->pkts[1] && mml_crc_cmp_p1 != task->dest_crc[1]) {
		mml_crc_err++;
		mml_err("CRC check job %u pipe 1 crc fail %#010x != %#010x error count %d",
			task->job.jobid, task->dest_crc[1], mml_crc_cmp_p1, mml_crc_err);
	}
}

void mml_record_track(struct mml_dev *mml, struct mml_task *task)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_task_buffer *buf = &task->buf;
	struct mml_record *record;
	u32 i;

	mutex_lock(&mml->record_mutex);

	record = &mml->records[mml->record_idx];

	record->jobid = task->job.jobid;
	record->src_iova = buf->src.dma[0].iova;
	record->dest_iova = buf->dest[0].dma[0].iova;
	record->src_size = buf->src.size[0];
	record->dest_size = buf->dest[0].size[0];
	record->src_plane_offset = cfg->info.src.plane_offset[0];
	record->dest_plane_offset = cfg->info.dest[0].data.plane_offset[0];
	record->src_iova_map_time = buf->src.map_time;
	record->dest_iova_map_time = buf->dest[0].map_time;
	record->src_iova_unmap_time = buf->src.unmap_time;
	record->dest_iova_unmap_time = buf->dest[0].unmap_time;
	record->cfg_jobid = cfg->job_id;
	record->task = (u32)(unsigned long)task;
	record->state = task->state;
	record->err = task->err;
	record->ref = kref_read(&task->ref);

	for (i = 0; i < MML_PIPE_CNT; i++) {
		record->config_pipe_time[i] = task->config_pipe_time[i];
		record->bw_time[i] = task->bw_time[i];
		record->freq_time[i] = task->freq_time[i];
		record->wait_fence_time[i] = task->wait_fence_time[i];
		record->flush_time[i] = task->flush_time[i];
		record->src_crc[i] = task->src_crc[i];
		record->dest_crc[i] = task->dest_crc[i];
	}

	if (mml_crc_cmp)
		mml_record_crc_check(task);

	mml_pipe0_dest_crc = task->dest_crc[0];
	if (MML_PIPE_CNT > 1)
		mml_pipe1_dest_crc = task->dest_crc[1];

	if (unlikely(mml_crc_test)) {
		mml_log("%s mml_crc_test %d job %u", __func__, mml_crc_test, task->job.jobid);
		record->src_crc[0] = mml_crc_test;
		record->dest_crc[0] = task->job.jobid;
		mml_crc_test = 0;
	}

	mml->record_idx = (mml->record_idx + 1) & MML_RECORD_NUM_MASK;

	mutex_unlock(&mml->record_mutex);
}

#define REC_TITLE "Index,Job ID," \
	"src map time,src unmap time,src,src size,plane 0," \
	"dest map time,dest unmap time,dest,dest size,plane 0," \
	"config job,task inst," \
	"PIPE0 stamp config,bw,freq,fence,flush,PIPE1 stamp config,bw,freq,fence,flush," \
	"state,ref,error," \
	"src_crc_pipe0,dest_crc_pipe0,src_crc_pipe1,dest_crc_pipe1"

static int mml_record_print(struct seq_file *seq, void *data)
{
	struct mml_dev *mml = (struct mml_dev *)seq->private;
	struct mml_record *record;
	u32 i, idx;

	/* Protect only index part, since it is ok to print race data,
	 * but not good to hurt performance of mml_record_track.
	 */
	mutex_lock(&mml->record_mutex);
	idx = mml->record_idx;
	mutex_unlock(&mml->record_mutex);

	seq_puts(seq, REC_TITLE ",\n");
	for (i = 0; i < ARRAY_SIZE(mml->records); i++) {
		record = &mml->records[idx];
		seq_printf(seq,
			/* idx to task */
			"%u,%u,%llu,%llu,%#llx,%u,%u,%llu,%llu,%#llx,%u,%u,%u,%#x,"
			/* config_pipe_time to flush_time */
			"%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
			/* state to dest crc */
			"%u,%u,%s,%#010x,%#010x,%#010x,%#010x\n",
			idx,
			record->jobid,
			record->src_iova_map_time,
			record->src_iova_unmap_time,
			record->src_iova,
			record->src_size,
			record->src_plane_offset,
			record->dest_iova_map_time,
			record->dest_iova_unmap_time,
			record->dest_iova,
			record->dest_size,
			record->dest_plane_offset,
			record->cfg_jobid,
			record->task,
			record->config_pipe_time[0],
			record->bw_time[0],
			record->freq_time[0],
			record->wait_fence_time[0],
			record->flush_time[0],
			record->config_pipe_time[1],
			record->bw_time[1],
			record->freq_time[1],
			record->wait_fence_time[1],
			record->flush_time[1],
			record->state,
			record->ref,
			record->err ? "error" : "",
			record->src_crc[0],
			record->dest_crc[0],
			record->src_crc[1],
			record->dest_crc[1]);
		idx = (idx + 1) & MML_RECORD_NUM_MASK;

		/* do not occupy log space */
		if (seq->size <= MML_LOG_SIZE)
			break;
	}

	mml_print_log_record(seq);

	return 0;
}

void mml_record_dump(struct mml_dev *mml)
{
	struct mml_record *record;
	u32 i, idx;
	/* dump 10 records only */
	const u32 dump_count = 10;

	/* Protect only index part, since it is ok to print race data,
	 * but not good to hurt performance of mml_record_track.
	 */
	mutex_lock(&mml->record_mutex);
	idx = (mml->record_idx + MML_RECORD_NUM - dump_count) & MML_RECORD_NUM_MASK;
	mutex_unlock(&mml->record_mutex);

	mml_err(REC_TITLE);
	for (i = 0; i < dump_count; i++) {
		record = &mml->records[idx];
		mml_err("%u,%u,%llu,%llu,%#llx,%u,%u,%llu,%llu,%#llx,%u,%u,%#010x,%#010x,%#010x,%#010x",
			idx,
			record->jobid,
			record->src_iova_map_time,
			record->src_iova_unmap_time,
			record->src_iova,
			record->src_size,
			record->src_plane_offset,
			record->dest_iova_map_time,
			record->dest_iova_unmap_time,
			record->dest_iova,
			record->dest_size,
			record->dest_plane_offset,
			record->src_crc[0],
			record->dest_crc[0],
			record->src_crc[1],
			record->dest_crc[1]);
		idx = (idx + 1) & MML_RECORD_NUM_MASK;
	}
}

static int mml_record_open(struct inode *inode, struct file *file)
{
	/* 128KB for records and MML_LOG_SIZE for log */
	return single_open_size(file, mml_record_print, inode->i_private,
		0x20000 + MML_LOG_SIZE);
}

static const struct file_operations mml_record_fops = {
	.owner = THIS_MODULE,
	.open = mml_record_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mml_record_init(struct mml_dev *mml)
{
	struct dentry *dir;
	bool exists = false;

	mutex_init(&mml->record_mutex);

	dir = debugfs_lookup("mml", NULL);
	if (!dir) {
		dir = debugfs_create_dir("mml", NULL);
		if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
			mml_err("debugfs_create_dir mml failed:%ld", PTR_ERR(dir));
			return;
		}
	} else
		exists = true;

	mml->record_entry = debugfs_create_file(
		"mml-record", 0444, dir, mml, &mml_record_fops);
	if (IS_ERR(mml->record_entry)) {
		mml_err("debugfs_create_file mml-record failed:%ld",
			PTR_ERR(mml->record_entry));
		mml->record_entry = NULL;
	}

	if (exists)
		dput(dir);

	mml_log("%s done with size %zu", __func__, sizeof(mml->records));
}

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
void mml_dump_reset(struct mml_dev *mml, enum mml_sys_id sysid)
{
	mutex_lock(&mml->frm_dump_mutex);
	mml->frm_dump_buf[sysid] = 0;
	mutex_unlock(&mml->frm_dump_mutex);
}

void mml_dump_enable(struct mml_dev *mml, enum mml_sys_id sysid,
	enum mml_frm_dump_buf bufid, bool enable, bool always)
{
	u32 mask = BIT(bufid);

	if (always)
		mask = mask << mml_frm_dump_src0_always;

	mutex_lock(&mml->frm_dump_mutex);

	if (enable)
		mml->frm_dump_buf[sysid] = mml->frm_dump_buf[sysid] | mask;
	else
		mml->frm_dump_buf[sysid] = mml->frm_dump_buf[sysid] & ~mask;

	mutex_unlock(&mml->frm_dump_mutex);
}

void mml_dump_set_option(struct mml_dev *mml, enum mml_sys_id sysid, enum mml_frm_dump_buf bufid,
	enum mml_frm_dump_opt opt)
{
	mutex_lock(&mml->frm_dump_mutex);
	mml->frm_dumps[sysid][bufid].dump_option = opt;
	mml->frm_dump_opt_sysid = sysid;
	mml->frm_dump_opt_bufid = bufid;
	mutex_unlock(&mml->frm_dump_mutex);
}

struct mml_frm_dump_data *mml_dump_read_data_lock(struct mml_dev *mml)
{
	mutex_lock(&mml->frm_dump_mutex);
	return &mml->frm_dumps[mml->frm_dump_opt_sysid][mml->frm_dump_opt_bufid];
}

void mml_dump_read_data_unlock(struct mml_dev *mml)
{
	mutex_unlock(&mml->frm_dump_mutex);
}

void mml_dump_input(struct mml_dev *mml, enum mml_sys_id sysid, struct mml_task *task, bool force)
{
	const struct mml_frame_config *cfg = task->config;
	struct mml_frm_dump_data *frm_data;
	u16 mask_once = 0;
	u64 cost = sched_clock();

	mutex_lock(&mml->frm_dump_mutex);

	if ((mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_src0)) ||
		(mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_src0_always)) ||
		force) {
		mml_buf_invalid(&task->buf.src);
		frm_data = &mml->frm_dumps[sysid][mml_frm_dump_src0];
		mml_core_dump_buf(task, &cfg->info.src, &task->buf.src, frm_data);
		mask_once |= BIT(mml_frm_dump_src0);
	}

	if (cfg->info.seg_map.format &&
		((mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_src1)) ||
		(mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_src1_always)) ||
		force)) {
		mml_buf_invalid(&task->buf.seg_map);
		frm_data = &mml->frm_dumps[sysid][mml_frm_dump_src1];
		mml_core_dump_buf(task, &cfg->info.seg_map, &task->buf.seg_map, frm_data);
		mask_once |= BIT(mml_frm_dump_src1);
	}

	if (mask_once)
		mml->frm_dump_buf[sysid] &= ~mask_once;
	mutex_unlock(&mml->frm_dump_mutex);
	if (mask_once) {
		cost = sched_clock() - cost;
		mml_log("[dump]input frame %#x cost %lluus", mask_once, (u64)div_u64(cost, 1000));
	}
}

void mml_dump_output(struct mml_dev *mml, enum mml_sys_id sysid, struct mml_task *task)
{
	const struct mml_frame_config *cfg = task->config;
	struct mml_frm_dump_data *frm_data;
	u16 mask_once = 0;
	u64 cost = sched_clock();

	mutex_lock(&mml->frm_dump_mutex);

	if ((mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_dest0)) ||
		(mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_dest0_always))) {
		frm_data = &mml->frm_dumps[sysid][mml_frm_dump_dest0];
		mml_core_dump_buf(task, &cfg->info.dest[0].data, &task->buf.dest[0], frm_data);
		mask_once |= BIT(mml_frm_dump_dest0);
	}

	if (cfg->info.dest_cnt == 2 &&
		((mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_dest1)) ||
		(mml->frm_dump_buf[sysid] & BIT(mml_frm_dump_dest1_always)))) {
		frm_data = &mml->frm_dumps[sysid][mml_frm_dump_dest1];
		mml_core_dump_buf(task, &cfg->info.dest[1].data, &task->buf.dest[1], frm_data);
		mask_once |= BIT(mml_frm_dump_dest1);
	}

	if (mask_once)
		mml->frm_dump_buf[sysid] &= ~mask_once;
	mutex_unlock(&mml->frm_dump_mutex);
	if (mask_once) {
		cost = sched_clock() - cost;
		mml_log("[dump]output frame %#x cost %lluus", mask_once, (u64)div_u64(cost, 1000));
	}
}
#endif

static int sys_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mml_sys *sys;

	if (unlikely(!mml)) {
		dev_err(dev, "mml_dev is NULL\n");
		return -EFAULT;
	}

	sys = mml->sys;
	if (unlikely(!sys)) {
		dev_err(dev, "mml->sys is NULL\n");
		return -EFAULT;
	}

	return mml_sys_bind(dev, master, sys, data);
}

static void sys_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mml_sys *sys = mml->sys;

	mml_sys_unbind(dev, master, sys, data);
}

static const struct component_ops sys_comp_ops = {
	.bind	= sys_bind,
	.unbind = sys_unbind,
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

static void mml_get_chipid(struct mml_dev *mml)
{
	struct device_node *node;
	struct tag_chipid *chip_id = NULL;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");
	if (node) {
		chip_id = (struct tag_chipid *) of_get_property(node, "atag,chipid", &len);
		if (!chip_id)
			mml_log("could not found atag,chipid in chosen");
	} else {
		mml_log("chosen node not found in device tree");
	}
	if (chip_id)
		mml->sw_ver = chip_id->sw_ver;
	mml_log("current sw version:%#x\n", mml->sw_ver);
}

u32 mml_get_chip_swver(struct mml_dev *mml)
{
	return mml->sw_ver;
}
EXPORT_SYMBOL_GPL(mml_get_chip_swver);

static bool dbg_probed;
static int mml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml;
	u32 i;
	int ret, thread_cnt;

	mml_log("%s with dev %p mml size %zu", __func__, dev, sizeof(struct mml_dev));
	mml = devm_kzalloc(dev, sizeof(*mml), GFP_KERNEL);
	if (!mml)
		return -ENOMEM;
	platform_set_drvdata(pdev, mml);

	mml->pdev = pdev;
	mutex_init(&mml->sys_state_mutex);
	mutex_init(&mml->ctx_mutex);
	mutex_init(&mml->clock_mutex);
	mutex_init(&mml->wake_ref_mutex);
	mutex_init(&mml->dpc.dpc_mutex[mml_sys_tile]);
	mutex_init(&mml->dpc.dpc_mutex[mml_sys_frame]);

	for (i = 0; i < ARRAY_SIZE(mml->sys_state); i++) {
		mml->sys_state[i].sys_id = i;
		atomic_set(&mml->sys_state[i].dl_ref, 0);
		atomic_set(&mml->sys_state[i].racing_ref, 0);
	}

	/* init sram request parameters */
	mutex_init(&mml->sram_mutex);
	mml->sram_data[mml_sram_racing].uid = UID_MML;
	mml->sram_data[mml_sram_racing].type = TP_BUFFER;
	mml->sram_data[mml_sram_racing].flag = FG_POWER;
	mml->sram_data[mml_sram_apudc].uid = UID_AISR_MML;
	mml->sram_data[mml_sram_apudc].type = TP_BUFFER;
	mml->sram_data[mml_sram_apudc].flag = FG_POWER;

	mml->sys = mml_sys_create(pdev, mml, &sys_comp_ops);
	if (IS_ERR(mml->sys)) {
		ret = PTR_ERR(mml->sys);
		dev_err(dev, "failed to init mml sys: %d\n", ret);
		goto err_sys_add;
	}

	if (smmu_v3_enabled()) {
		/* shared smmu device, setup 34bit in dts */
		mml->mmu_dev = mml_smmu_get_shared_device(dev, "mtk,smmu-shared");
		mml->mmu_dev_sec = mml_smmu_get_shared_device(dev, "mtk,smmu-shared-sec");
		mml->smmu_en = true;
	} else {
		mml->mmu_dev = dev;
		mml->mmu_dev_sec = dev;
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret)
			mml_err("fail to config sys dma mask %d", ret);
	}

#if defined(MML_DL_SUPPORT)
	mml->dl_en = of_property_read_bool(dev->of_node, "dl-enable");
	if (mml->dl_en)
		mml_log("direct link mode enable");
#endif
	mml->dpc_disable = of_property_read_bool(dev->of_node, "dpc-disable");
	if (mml->dpc_disable)
		mml_log("dpc disable by project");

#if defined(MML_IR_SUPPORT)
	mml->racing_en = of_property_read_bool(dev->of_node, "racing-enable");
	if (mml->racing_en)
		mml_log("IR mode enable");
#endif

	mml->v4l2_en = of_property_read_bool(dev->of_node, "v4l2-enable");

	mml->tablet_ext = of_property_read_bool(dev->of_node, "tablet-ext");

	mml_get_chipid(mml);

	if (of_property_read_u8(dev->of_node, "racing-height", &mml->racing_height))
		mml->racing_height = 64;	/* default height 64px */

	if (!of_property_read_u16(dev->of_node, "event-ir-mml-ready", &mml->event_mml_ready))
		mml_log("racing event_mml_ready %u", mml->event_mml_ready);
	if (!of_property_read_u16(dev->of_node, "event-ir-disp-ready", &mml->event_disp_ready))
		mml_log("racing event_disp_ready %u", mml->event_disp_ready);
	if (!of_property_read_u16(dev->of_node, "event-ir-mml-stop", &mml->event_mml_stop))
		mml_log("racing event_mml_stop %u", mml->event_mml_stop);
	if (!of_property_read_u16(dev->of_node, "event-ir-eof", &mml->event_mml_target))
		mml_log("racing event_mml_target %u", mml->event_mml_target);

	thread_cnt = of_count_phandle_with_args(
		dev->of_node, "mboxes", "#mbox-cells");
	if (thread_cnt <= 0 || thread_cnt > MML_MAX_CMDQ_CLTS)
		thread_cnt = MML_MAX_CMDQ_CLTS;
	mml->gce_thread_cnt = thread_cnt;
	mml->cmdq_base = cmdq_register_device(dev);
	for (i = 0; i < thread_cnt; i++) {
		mml->cmdq_clts[i] = cmdq_mbox_create(dev, i);
		if (IS_ERR_OR_NULL(mml->cmdq_clts[i])) {
			ret = PTR_ERR(mml->cmdq_clts[i]);
			dev_err(dev, "unable to create cmdq mbox on %p:%d err %d",
				dev, i, ret);
			mml->cmdq_clts[i] = NULL;
			if (i == 0)
				goto err_mbox_create;
			else
				break;
		}
	}
	mml->cmdq_clt_cnt = i;

	dbg_probed = true;

	ret = comp_master_init(dev, mml);
	if (unlikely(ret)) {
		dev_err(dev, "failed to initialize mml component master\n");
		goto err_init_master;
	}

	mml->wake_lock = wakeup_source_register(dev, "mml_pm_lock");
	mml_record_init(mml);

	/* for inline rotate dle context, avoid blocking dle addon flow */
	if (mml->racing_en) {
		mml_log("racing mode enable");
		mml->dle_ctx = mml_dle_ctx_create(mml);
	}

	/* register v4l2 device */
	if (mml->v4l2_en) {
		mml_log("v4l2 device enable");
		mml->v4l2_dev = mml_v4l2_dev_create(dev);
	}

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	mutex_init(&mml->frm_dump_mutex);
	mml->frm_dump_opt_sysid = mml_sys_frame; /* default set to mml1 for timeout dump */
	mml->frm_dumps[mml_sys_frame][mml_frm_dump_src0].prefix = "in";
	mml->frm_dumps[mml_sys_frame][mml_frm_dump_src1].prefix = "in1";
	mml->frm_dumps[mml_sys_frame][mml_frm_dump_dest0].prefix = "out";
	mml->frm_dumps[mml_sys_frame][mml_frm_dump_dest1].prefix = "out1";
	mml->frm_dumps[mml_sys_tile][mml_frm_dump_src0].prefix = "in";
	mml->frm_dumps[mml_sys_tile][mml_frm_dump_src1].prefix = "in1";
	mml->frm_dumps[mml_sys_tile][mml_frm_dump_dest0].prefix = "out";
	mml->frm_dumps[mml_sys_tile][mml_frm_dump_dest1].prefix = "out1";
#endif

	mml_log("%s success end", __func__);
	return 0;

err_init_master:
err_mbox_create:
	mml_sys_destroy(pdev, mml->sys, &sys_comp_ops);
	for (i = 0; i < MML_MAX_CMDQ_CLTS; i++)
		if (mml->cmdq_clts[i]) {
			cmdq_mbox_destroy(mml->cmdq_clts[i]);
			mml->cmdq_clts[i] = NULL;
		}
err_sys_add:
	devm_kfree(dev, mml);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int mml_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_v4l2_dev_destroy(dev, mml->v4l2_dev);
	wakeup_source_unregister(mml->wake_lock);
	comp_master_deinit(dev);
	mml_sys_destroy(pdev, mml->sys, &sys_comp_ops);
	devm_kfree(dev, mml);

	return 0;
}

static int __maybe_unused mml_pm_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mml_pm_resume(struct device *dev)
{
	return 0;
}

static int __maybe_unused mml_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;
	return mml_pm_suspend(dev);
}

static int __maybe_unused mml_resume(struct device *dev)
{
	if (pm_runtime_active(dev))
		return 0;
	return mml_pm_resume(dev);
}

static const struct dev_pm_ops mml_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mml_suspend, mml_resume)
	SET_RUNTIME_PM_OPS(mml_pm_suspend, mml_pm_resume, NULL)
};

static struct platform_driver mtk_mml_driver = {
	.probe = mml_probe,
	.remove = mml_remove,
	.driver = {
		.name = "mediatek-mml",
		.owner = THIS_MODULE,
		.pm = &mml_pm_ops,
		.of_match_table = mtk_mml_of_ids,
	},
};

static struct platform_driver *mml_drivers[] = {
	&mtk_mml_driver,
	&mml_sys_driver,
	&mml_aal_driver,
	&mml_color_driver,
	&mml_fg_driver,
	&mml_hdr_driver,
	&mml_mutex_driver,
	&mml_rdma_driver,
	&mml_rsz_driver,
	&mml_pq_rdma_driver,
	&mml_pq_birsz_driver,
	&mml_tcc_driver,
	&mml_tdshp_driver,
	&mml_wrot_driver,
	&mml_rrot_driver,
	&mml_merge_driver,
	&mml_c3d_driver,

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	&mtk_mml_test_drv,
#endif
};

static int __init mml_driver_init(void)
{
	int ret;

	mml_msg("%s register drivers", __func__);

	ret = platform_register_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
	if (ret) {
		mml_err("failed to register mml core drivers: %d", ret);
		return ret;
	}

	mml_mmp_init();

	ret = mml_pq_core_init();
	if (ret)
		mml_err("failed to init mml pq core: %d", ret);

	/* register pm notifier */

	return ret;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	mml_pq_core_uninit();
	platform_unregister_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
}
module_exit(mml_driver_exit);

static s32 dbg_case;
static int dbg_set(const char *val, const struct kernel_param *kp)
{
	int result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump current setting");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static int dbg_get(char *buf, const struct kernel_param *kp)
{
	int length = 0;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed: %d\n", dbg_case, dbg_probed);
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(drv_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(drv_debug, "mml driver debug case");

MODULE_DESCRIPTION("MediaTek multimedia-layer driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
