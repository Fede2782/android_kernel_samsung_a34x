/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DRIVER_H__
#define __MTK_MML_DRIVER_H__

#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/mailbox_controller.h>
#include <linux/of.h>
#include "mtk-mml.h"

#define MML_MAX_COMPONENTS	50
#define MML_MAX_PORT		18

struct mml_comp;
struct mml_dev;
struct mml_drm_ctx;
struct mml_drm_param;
struct mml_dle_ctx;
struct mml_dle_param;
struct mml_m2m_ctx;
struct mml_topology_cache;
struct mml_task;
struct mml_comp_config;
struct mml_topology_path;

enum mml_sys_id {
	mml_sys_tile = 0,	/* in most case, mml-tile, for mml_decouple_2, mmlsys0 */
	mml_sys_frame,		/* in most case, mml-frame, mmlsys1 */
	mml_max_sys
};

struct platform_device *mml_get_plat_device(struct platform_device *pdev);

static inline int of_mml_count_comps(const struct device_node *np)
{
	return of_property_count_u32_elems(np, "comp-ids");
}

static inline int of_mml_read_comp_id_index(const struct device_node *np,
	u32 index, u32 *id)
{
	return of_property_read_u32_index(np, "comp-ids", index, id);
}

int mml_comp_add(u32 id, struct device *dev, const struct component_ops *ops);

enum mml_sram_mode {
	mml_sram_racing,
	mml_sram_apudc,
	mml_sram_mode_total
};

/*
 *
 */
s32 mml_dev_get_couple_cnt(struct mml_dev *mml);

s32 mml_dev_couple_inc(struct mml_dev *mml, enum mml_mode mode);

s32 mml_dev_couple_dec(struct mml_dev *mml, enum mml_mode mode);

/*
 * mml_qos_init - init mmlsys mmqos and mmdvfs from dts property
 *
 * @mml: The mml driver instance
 * @pdev: mmlsys platform device
 * @sysid: one of enum mml_sys_id
 */
void mml_qos_init(struct mml_dev *mml, struct platform_device *pdev, u32 sysid);

/*
 * mml_qos_update_sys - check all mmlsys and update throughput
 *
 * @mml: The mml driver instance
 * @dpc: mml using dpc or not
 * @path: current topology path for task
 * @enable: begin task or end task
 *
 * Return: throughput upper bound from opp table
 */
u32 mml_qos_update_sys(struct mml_dev *mml, bool dpc,
	const struct mml_topology_path *path, bool enable);

/*
 * mml_qos_update_tput - scan throughputs in all path client and update the max one
 *
 * @mml:	The mml driver instance
 * @dpc:	mml using dpc or not
 * @sysid:	mmlsys which want to update throughput
 * @enable:	enable or disable dvfs
 *
 * Return: throughput upper bound from opp table
 */
u32 mml_qos_update_tput(struct mml_dev *mml, bool dpc, enum mml_sys_id sysid, bool enable);

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp);

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev);
s32 mml_comp_pw_enable(struct mml_comp *comp, const s8 mode);
s32 mml_comp_pw_disable(struct mml_comp *comp, const s8 mode);
s32 mml_comp_clk_enable(struct mml_comp *comp);
s32 mml_comp_clk_disable(struct mml_comp *comp, bool dpc);

/*
 * mml_comp_get_smmu_node - check if platform use smmu v3 and parse normal/secure smmu shared node
 *
 * @comp:	component to parse
 * @dev:	device node of parent
 *
 * Return:	shared device if found, original dev if not
 */
struct device *mml_smmu_get_shared_device(struct device *dev, const char *name);

void mml_comp_qos_calc(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput);
void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput, u32 tput_up);
void mml_comp_qos_clear(struct mml_comp *comp, struct mml_task *task, bool dpc);

s32 mml_dpc_task_cnt_get(struct mml_task *task);
void mml_dpc_task_cnt_inc(struct mml_task *task);
void mml_dpc_task_cnt_dec(struct mml_task *task);
void mml_dpc_exc_keep(struct mml_dev *mml, u32 sysid);
void mml_dpc_exc_release(struct mml_dev *mml, u32 sysid);
void mml_dpc_exc_keep_task(struct mml_task *task, const struct mml_topology_path *path);
void mml_dpc_exc_release_task(struct mml_task *task, const struct mml_topology_path *path);
void mml_dpc_dc_enable(struct mml_dev *mml, u32 sysid, bool en);
void mml_dpc_bw_update(struct mml_dev *mml, enum mml_sys_id sysid, u32 total_bw, u32 hrt_bw);

void mml_pw_set_kick_cb(struct mml_dev *mml, void (*kick_idle_cb)(void *disp_param), void *param);
void mml_pw_kick_idle(struct mml_dev *mml);

/*
 * mml_sram_get - get sram addr from slbc and power on
 *
 * @mml:	The mml driver instance
 *
 * Return:	The address of sram
 */
void __iomem *mml_sram_get(struct mml_dev *mml, enum mml_sram_mode mode);

/*
 * mml_sram_put - power off sram and release slbc object
 *
 * @mml:	The mml driver instance
 */
void mml_sram_put(struct mml_dev *mml, enum mml_sram_mode mode);

/*
 * mml_sram_get_racing_height - get racing height in mml cache
 *
 * @mml:	The mml driver instance
 *
 * Return:	The racing height in pixel.
 */
u8 mml_sram_get_racing_height(struct mml_dev *mml);

/*
 * mml_get_mmu_dev - get mmlsys mmu device
 *
 * @mml:	The mml driver instance
 *
 * Return:	The smmu or dmabuf device instance
 */
struct device *mml_get_mmu_dev(struct mml_dev *mml, bool secure);

/*
 * mml_dl_enable - enable direct link mode or not
 *
 * @mml:	The mml driver instance
 *
 * Return:	True for enable, false for disable.
 */
bool mml_dl_enable(struct mml_dev *mml);

/*
 * mml_dpc_disable - dpc disable or not
 *
 * @mml:	The mml driver instance
 *
 * Return:	True for enable, false for disable.
 */
bool mml_dpc_disable(struct mml_dev *mml);

/*
 * mml_racing_enable - enable racing mode (inline rotate) or not
 *
 * @mml:	The mml driver instance
 *
 * Return:	True for enable, false for disable.
 */
bool mml_racing_enable(struct mml_dev *mml);

/*
 * mml_tablet_ext - current is tablet project or not
 *
 * @mml:	The mml driver instance
 *
 * Return:	True for tablet project, false for not tablet project.
 */
bool mml_tablet_ext(struct mml_dev *mml);

/*
 * mml_ir_get_mml_ready_event - get inline rot sync event mml_ready
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of mml_ready
 */
u16 mml_ir_get_mml_ready_event(struct mml_dev *mml);

/*
 * mml_ir_get_disp_ready_event - get inline rot sync event disp_ready
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of disp_ready
 */
u16 mml_ir_get_disp_ready_event(struct mml_dev *mml);

/*
 * mml_ir_get_mml_stop_event - get inline rot sync event mml_stop
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of disp_ready
 */
u16 mml_ir_get_mml_stop_event(struct mml_dev *mml);

/*
 * mml_ir_get_target_event - get target line event
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of target line
 */
u16 mml_ir_get_target_event(struct mml_dev *mml);

/*
 * mml_ir_get_cmdq_clt - get cmdq clt
 *
 * @mml:	The mml driver instance
 * @id:     The clt id of mml dev
 *
 * Return:	The one of clts in mml dev
 */
struct cmdq_client *mml_get_cmdq_clt(struct mml_dev *mml, u32 id);

/*
 * mml_dump_thread - dump cmdq threads hold by mml
 *
 * @mml:	The mml driver instance
 */
void mml_dump_thread(struct mml_dev *mml);

/*
 * mml_clock_lock - Lock clock mutex before clock counting or call clock api
 *
 * @mml: The mml driver instance
 */
void mml_clock_lock(struct mml_dev *mml);

/*
 * mml_clock_unlock - Unlock clock mutex before clock counting or call clock api
 *
 * @mml: The mml driver instance
 */
void mml_clock_unlock(struct mml_dev *mml);

/*
 * mml_lock_wake_lock - wake lock to prevent system off
 *
 * @mml: The mml driver instance
 */
void mml_lock_wake_lock(struct mml_dev *mml, bool lock);

/*
 * mml_record_track - record mml task behavior for debug print
 *
 * @mml: The mml driver instance
 * @task: The mml task to record
 */
void mml_record_track(struct mml_dev *mml, struct mml_task *task);

/*
 * mml_record_dump - dump records to log
 *
 * @mml: The mml driver instance
 */
void mml_record_dump(struct mml_dev *mml);

s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp);
s32 mml_register_comp(struct device *master, struct mml_comp *comp);
void mml_unregister_comp(struct device *master, struct mml_comp *comp);

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_param *disp,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_drm_param *disp));
void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx));
struct mml_dle_ctx *mml_dev_get_dle_ctx(struct mml_dev *mml,
	struct mml_dle_param *dl,
	void (*ctx_setup)(struct mml_dle_ctx *ctx, struct mml_dle_param *dl));
void mml_dev_put_dle_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_dle_ctx *ctx));
struct mml_m2m_ctx *mml_dev_create_m2m_ctx(struct mml_dev *mml,
	struct mml_m2m_ctx *(*ctx_create)(struct mml_dev *mml));

struct mml_v4l2_dev *mml_get_v4l2_dev(struct mml_dev *mml);

/*
 * mml_topology_get_cache - Get topology cache struct store in mml.
 *
 * @mml: The mml driver instance.
 *
 * Return: Topology cache struct for this mml.
 */
struct mml_topology_cache *mml_topology_get_cache(struct mml_dev *mml);

/*
 * mml_dev_get_comp_by_id - Get component instance by component id, which
 * represent specific hardware engine in MML.
 *
 * @mml:	The mml driver instance.
 * @id:		Component ID, one of id in dt-bindings in specific IP.
 *
 * Return: pointer of component instance
 */
struct mml_comp *mml_dev_get_comp_by_id(struct mml_dev *mml, u32 id);

/*
 * mml_get_sys - get mmlsys instance for mml
 */
void *mml_get_sys(struct mml_dev *mml);

/*
 * mml_get_node_base_pa - Help parse node pa addr
 *
 * @pdev:	Platform device.
 * @name:	Node property name.
 * @idx:	Index in property.
 * @base:	[out]Node base addr va
 *
 * Return: node base addr pa
 */
phys_addr_t mml_get_node_base_pa(struct platform_device *pdev, const char *name,
	u32 idx, void __iomem **base);

/*
 * mml for swpm
 */
void mml_update_freq_status(u32 freq);
void mml_update_pq_status(const void *pq);
void mml_update_status_to_tppa(const struct mml_frame_info *info);
struct mml_swpm_func *mml_get_swpm_func(void);

struct mml_swpm_func {
	bool set_func; /* Function set by swpm or not */
	void (*update_wrot)(u32 wrot0_sts, u32 wrot1_sts, u32 wrot2_sts, u32 wrot3_sts);
	void (*update_cg)(u32 cg_con0, u32 cg_con1);
	void (*update_freq)(u32 freq);
	void (*update_pq)(u32 pq);
};

enum mml_mon_mods {
	mml_mon_mmlsys = 0,
	mml_mon_wrot = 1, /* wrot 0~3 occupy index 1~4 */
	mml_mon_total = 5
};

/*
 * mml_backup_crc - Backup crc register to dma buffer for later record or debug.
 *
 * Note if buffer not exist in mml_dev, create it by call cmdq alloc api with pipe 0 pkt client.
 *
 * @task:	Current mml task.
 * @ccfg:	Current config cache.
 * @crc_reg:	Register address to backup.
 * @crc_idx_out:	Output an index of crc buffer.
 *
 * Return: Instruction offset of giving task pkt[pipe]. 0 if fail.
 */
u32 mml_backup_crc(struct mml_task *task, struct mml_comp_config *ccfg, phys_addr_t crc_reg,
	u32 *crc_idx_out);

/*
 * mml_backup_crc_update - Update backup instruction destination buffer pa.
 *
 * @task:	Current mml task.
 * @ccfg:	Current config cache.
 * @crc_inst_offset:	Offset to the crc backup instruction
 * @crc_idx_out:	Output an new index of crc buffer.
 */
void mml_backup_crc_update(struct mml_task *task, struct mml_comp_config *ccfg,
	u32 crc_inst_offset, u32 *crc_idx_out);

/*
 * mml_backup_crc_get - Get CRC value by index.
 *
 * Note if buffer not exist in mml_dev, create it by call cmdq alloc api with pipe 0 pkt client.
 *
 * @task:	Current mml task.
 * @ccfg:	Current config cache.
 * @crc_idx:	Index of crc buffer.
 *
 * Return: crc value
 */
u32 mml_backup_crc_get(struct mml_task *task, struct mml_comp_config *ccfg, u32 crc_idx);

#if IS_ENABLED(CONFIG_VHOST_CMDQ)
void cmdq_set_client(struct mml_dev *mml);
#endif

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
enum mml_frm_dump_buf {
	/* dump once */
	mml_frm_dump_src0,
	mml_frm_dump_src1,
	mml_frm_dump_dest0,
	mml_frm_dump_dest1,
	mml_frm_dump_count,

	/* dump always */
	mml_frm_dump_src0_always,
	mml_frm_dump_src1_always,
	mml_frm_dump_dest0_always,
	mml_frm_dump_dest1_always,
};

enum mml_frm_dump_opt {
	mml_frm_dump_frame,
	mml_frm_dump_name,
	mml_frm_dump_total
};

/*
 * mml_dump_enable - Enable or disable dump feature.
 *
 * @mml:	The mml_dev instance
 * @sysid:	mmlsys id
 */
void mml_dump_reset(struct mml_dev *mml, enum mml_sys_id sysid);

/*
 * mml_dump_enable - Enable or disable dump feature.
 *
 * @mml:	The mml_dev instance
 * @sysid:	mmlsys id
 * @bufid:	The buffer id to input/output 0/1
 * @enable:	Enable or disable the dump
 * @always:	Always dump or dump once.
 */
void mml_dump_enable(struct mml_dev *mml, enum mml_sys_id sysid,
	enum mml_frm_dump_buf bufid, bool enable, bool always);

/*
 * mml_dump_set_option - Setup the sysid, bufid and optino into option cache. Client may
 *	read/pull data which index by these options.
 *
 * @mml:	The mml_dev instance
 * @sysid:	mmlsys id
 * @bufid:	The buffer id to input/output 0/1
 * @opt:	The option want to read.
 */
void mml_dump_set_option(struct mml_dev *mml, enum mml_sys_id sysid, enum mml_frm_dump_buf bufid,
	enum mml_frm_dump_opt opt);

/*
 * mml_dump_read_data_lock - Get frame data by current dump option frm_dump_opt_sysid and
 *	frm_dump_opt_bufid which set by mml_dump_set_option.
 *
 * Note this api also lock dump mutex, thus caller must call mml_dump_read_data_unlock to unlock
 * after done with return frame data.
 *
 * @mml:	The mml_dev instance
 *
 * Return:	The frame dump data struct.
 */
struct mml_frm_dump_data *mml_dump_read_data_lock(struct mml_dev *mml);

/*
 * mml_dump_read_data_unlock - Unlock dump mutex, which lock in mml_dump_read_data_lock.
 *
 * @mml:	The mml_dev instance
 */
void mml_dump_read_data_unlock(struct mml_dev *mml);

/*
 * mml_dump_input - Dump input frame into mml dev frame cache.
 *
 * @mml:	The mml_dev instance
 * @sysid:	Current task mmlsys id
 * @task:	Current task to dump
 * @force:	Force dump input, use for timeout dump.
 */
void mml_dump_input(struct mml_dev *mml, enum mml_sys_id sysid, struct mml_task *task, bool force);

/*
 * mml_dump_output - Dump output frame into mml dev frame cache.
 *
 * @mml:	The mml_dev instance
 * @sysid:	Current task mmlsys id
 * @task:	Current task to dump
 */
void mml_dump_output(struct mml_dev *mml, enum mml_sys_id sysid, struct mml_task *task);
#endif

/*
 * mml_get_chip_swver - Get sw ver from chip id
 *
 * @mml:	The mml_dev instance
 *
 * Return:	sw ver
 */
u32 mml_get_chip_swver(struct mml_dev *mml);

extern struct platform_driver mml_sys_driver;
extern struct platform_driver mml_aal_driver;
extern struct platform_driver mml_color_driver;
extern struct platform_driver mml_fg_driver;
extern struct platform_driver mml_hdr_driver;
extern struct platform_driver mml_mutex_driver;
extern struct platform_driver mml_rdma_driver;
extern struct platform_driver mml_rsz_driver;
extern struct platform_driver mml_pq_rdma_driver;
extern struct platform_driver mml_pq_birsz_driver;
extern struct platform_driver mml_tcc_driver;
extern struct platform_driver mml_tdshp_driver;
extern struct platform_driver mml_wrot_driver;
extern struct platform_driver mml_rrot_driver;
extern struct platform_driver mml_merge_driver;
extern struct platform_driver mml_c3d_driver;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
extern struct platform_driver mtk_mml_test_drv;
#endif

#endif	/* __MTK_MML_DRIVER_H__ */
