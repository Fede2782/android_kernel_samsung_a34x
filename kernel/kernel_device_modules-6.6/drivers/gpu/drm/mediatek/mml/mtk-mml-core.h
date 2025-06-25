/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */


#ifndef __MTK_MML_CORE_H__
#define __MTK_MML_CORE_H__

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/dma-fence.h>
#include <linux/file.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/mailbox_client.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <mtk-interconnect.h>
#include <cmdq-util.h>

#include "mtk-mml.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-drm.h"
#include "mtk-mml-pq.h"

extern int mtk_mml_msg;
extern int mml_cmdq_err;
extern int mml_qos;
extern int mml_qos_log;
extern int mml_stash_bw;
extern int mml_dpc_log;
extern int mml_rrot_msg;
extern int mml_dl_dpc;
extern int mml_stash;
extern int rdma_stash_leading;

/* see mml_qos in mtk-mml-core.c */
#define MML_QOS_EN_MASK			0x1
#define MML_QOS_FORCE_CLOCK_MASK	(BIT(1))
#define MML_QOS_FORCE_CLOCK_SH		2
#define MML_QOS_FORCE_BW_MASK		(BIT(17))
#define MML_QOS_FORCE_BW_SH		18
#define MML_QOS_FORCE_MASK		0x3fff

/* get force throughput (clock) or bandwidth helper */
#define mml_qos_force_clk	((mml_qos >> MML_QOS_FORCE_CLOCK_SH) & MML_QOS_FORCE_MASK)
#define mml_qos_force_bw	((mml_qos >> MML_QOS_FORCE_BW_SH) & MML_QOS_FORCE_MASK)

/* 513 to ensure port has good ostd
 * 5329 is the worst bw calculated by 1440x3200 RGBA in+out w/ 1.2 overhead
 */
#define MML_QOS_MIN_BW		513
#define MML_QOS_MAX_BW		5329
#define MML_QOS_MIN_STASH_BW	17

/* MML couple mode HRT mode, HRT bandwidth to MMQoS and DPC
 * MML_HRT_ENABLE:	default, config srt and hrt to mmqos and dpc
 * MML_HRT_OSTD_MAX:	report hrt to dpc, report srt and MAX OSTD hrt to smi.
 * MML_HRT_OSTD_ONLY:	max ostd only mode, report 0 srt and max hrt to mmqos, but no bw to dpc
 *			this prevent mminfra or dram opp raise
 * MML_HRT_LIMIT:	hrt lower than mtk_mml_hrt_bound, follow MML_HRT_OSTD_ONLY,
 *			config srt and hrt instead.
 * MML_HRT_MMQOS:	set srt and hrt to mmqos, but skip dpc
 *
 * note: this symbol export to mtk-mml-mt6xxx to do platform config.
 */
enum mml_hrt_mode {
	MML_HRT_ENABLE = 0,
	MML_HRT_OSTD_MAX,
	MML_HRT_OSTD_ONLY,
	MML_HRT_LIMIT,
	MML_HRT_MMQOS,
};
extern int mtk_mml_hrt_mode;
extern int mml_hrt_bound;

/* define in mtk-mml-wrot.c */
extern int mml_wrot_bkgd_en;
extern int mml_rrot_debug;

enum mml_log_buf_setting {
	mml_logbuf_krn = 0x1,	/* print mml_log to kernel log by pr_notice */
	mml_logbuf_log = 0x2,	/* save log to buffer and output by mml-record */
	mml_logbuf_msg = 0x4,	/* save mml_msg to buffer and output by mml-record */
};
extern int mml_log_rec;
#define MML_LOG_SIZE	(1 << 20)

void mml_save_log_record(const char *fmt, ...);
void mml_print_log_record(struct seq_file *seq);

#define _mml_save_log(fmt, args...) do { \
	struct timespec64 _curr_time; \
	ktime_get_boottime_ts64(&_curr_time); \
	mml_save_log_record("[%5lld.%06llu]" fmt, \
		_curr_time.tv_sec, div_u64(_curr_time.tv_nsec, 1000), ##args); \
} while (0)

#define _mml_log(fmt, args...) do { \
	if (mml_log_rec & mml_logbuf_krn) \
		pr_notice("[mml]" fmt "\n", ##args); \
	if (mml_log_rec & mml_logbuf_log) \
		_mml_save_log(fmt "\n", ##args); \
} while (0)

#define mml_msg(fmt, args...) \
do { \
	if (mml_log_rec & mml_logbuf_msg) \
		_mml_save_log(fmt "\n", ##args); \
	if (mtk_mml_msg && (mml_log_rec & mml_logbuf_krn)) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_log(fmt, args...) \
do { \
	_mml_log(fmt, ##args); \
	if (mml_cmdq_err) \
		cmdq_util_error_save("[mml]"fmt"\n", ##args); \
} while (0)

#define mml_err(fmt, args...) \
do { \
	_mml_log("[err]" fmt, ##args); \
	if (mml_cmdq_err) \
		cmdq_util_error_save("[mml]"fmt"\n", ##args); \
} while (0)

#define mml_msg_qos(fmt, args...) \
do { \
	if (mml_qos_log) \
		_mml_log("[qos]" fmt, ##args); \
	else \
		mml_msg("[qos]" fmt, ##args); \
} while (0)

#define mml_msg_dpc(fmt, args...) \
do { \
	if (mml_dpc_log) \
		_mml_log("[dpc]" fmt, ##args); \
	else \
		mml_msg("[dpc]" fmt, ##args); \
} while (0)

#define DB_OPT_MML	(DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | \
	DB_OPT_MMPROFILE_BUFFER | DB_OPT_FTRACE | DB_OPT_DUMP_DISPLAY)

#ifdef MML_FPGA
#define _aee_api(...)
#define _fatal_api(...)
#else
#define _aee_api(opt, tag, fmt, args...) \
	(aee_kernel_warning_api(__FILE__, __LINE__, opt, tag, fmt, ##args))

#define _fatal_api(opt, tag, fmt, args...) \
	(aee_kernel_fatal_api(__FILE__, __LINE__, opt, tag, fmt, ##args))

#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define mml_aee(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("%s %d len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		cmdq_aee(fmt, ##args); \
		cmdq_util_error_save("[mml][aee] "fmt"\n", ##args); \
		_aee_api(DB_OPT_MML, tag, fmt, ##args); \
	} while (0)

#define mml_fatal(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("%s %d len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		_mml_log("[err][fatal]" fmt, ##args); \
		_fatal_api(DB_OPT_MML, tag, fmt, ##args); \
	} while (0)


#else
#define mml_aee(key, fmt, args...) \
	do { \
		char tag[LINK_MAX]; \
		int len = snprintf(tag, LINK_MAX, "CRDISPATCH_KEY:%s", key); \
		if (len >= LINK_MAX) \
			pr_debug("%s %d len:%d over max:%d\n", \
				__func__, __LINE__, len, LINK_MAX); \
		cmdq_aee(fmt" (aee not ready)", ##args); \
		cmdq_util_error_save("[mml][aee] "fmt"\n", ##args); \
	} while (0)

#define mml_fatal(args...) mml_aee(##args)

#endif

/* mml ftrace */
extern int mml_trace;

#define MML_TTAG_OVERDUE	"mml_endtime_overdue"
#define MML_TID_IRQ		0	/* trace on <idle>-0 process */

#define mml_trace_begin_tid(tid, fmt, args...) \
	mml_tracing_mark_write("B|%d|" fmt "\n", tid, ##args)

#define mml_trace_begin(fmt, args...) \
	mml_trace_begin_tid(current->tgid, fmt, ##args)

#define mml_trace_end() \
	mml_tracing_mark_write("E|%d\n", current->tgid)

#define mml_trace_c(tag, c) \
	mml_tracing_mark_write("C|%d|%s|%d\n", current->tgid, tag, c)

#define mml_trace_tag_start(tag) mml_trace_c(tag, 1)

#define mml_trace_tag_end(tag) mml_trace_c(tag, 0)

#define mml_trace_ex_begin(fmt, args...) do { \
	if (mml_trace) \
		mml_trace_begin(fmt, ##args); \
} while (0)

#define mml_trace_ex_end() do { \
	if (mml_trace) \
		mml_trace_end(); \
} while (0)

/* mml pq control */
extern int mml_pq_disable;

/* mml slt */
extern int mml_slt;

/* racing mode ut and debug */
extern int mml_racing_ut;
extern int mml_racing_eoc;
extern int mml_rdma_urgent;
extern int mml_rdma_crc;
extern int mml_rdma_dbg;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
extern u32 *rdma_crc_va[MML_PIPE_CNT];
extern dma_addr_t rdma_crc_pa[MML_PIPE_CNT];
#endif

#define MML_MAX_PATH_NODES	27 /* must align MAX_TILE_FUNC_NO in tile_driver.h */
#define MML_MAX_PATH_CACHES	32 /* must >= PATH_MML_MAX in all mtk-mml-mtxxxx.c */
#define MML_MAX_AID_COMPS	10
#define MML_MAX_CMDQ_CLTS	4
#define MML_MAX_OPPS		6
#define MML_MAX_TPUT		800
#define MML_CMDQ_NEXT_SPR	(CMDQ_GPR_CNT_ID + CMDQ_GPR_R09)
#define MML_CMDQ_NEXT_SPR2	(CMDQ_GPR_CNT_ID + CMDQ_GPR_R11)
#define MML_CMDQ_ROUND_SPR	CMDQ_THR_SPR_IDX3
#define MML_ROUND_SPR_INIT	0x8000
#define MML_NEXTSPR_NEXT	BIT(0)
#define MML_NEXTSPR_DUAL	BIT(1)

/* mode decision */
#define mml_isdc(mode)		(mode == MML_MODE_MML_DECOUPLE || mode == MML_MODE_MML_DECOUPLE2)
#define mml_iscouple(mode)	(mode == MML_MODE_DIRECT_LINK || mode == MML_MODE_RACING)
#define mml_stash_en(mode)	((mml_isdc(mode) && (mml_stash & 0x1)) || \
				(mml_iscouple(mode) && (mml_stash & 0x2)))

struct mml_topology_cache;
struct mml_ctx;
struct mml_frame_config;
struct mml_task;
struct mml_frame_tile;

struct mml_task_ops {
	void (*queue)(struct mml_task *task, u32 pipe);
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
	/* optional: adaptor may use frame_done to handle error */
	void (*frame_err)(struct mml_task *task);
	s32 (*dup_task)(struct mml_task *task, u32 pipe);
	struct mml_tile_cache *(*get_tile_cache)(struct mml_task *task, u32 pipe);
	void (*kt_setsched)(struct mml_ctx *ctx);
	void (*ddren)(struct mml_task *task, struct cmdq_pkt *pkt, bool enable);
	void (*dispen)(struct mml_task *task, bool enable);
};

struct mml_config_ops {
	void (*get)(struct mml_frame_config *cfg);
	void (*put)(struct mml_frame_config *cfg);
	void (*free)(struct mml_frame_config *cfg);
};

struct mml_cap {
	enum mml_mode target;
	enum mml_mode running;
};

struct mml_path_node {
	u8 id;
	struct mml_path_node *prev[MML_MAX_INPUTS];
	struct mml_path_node *next[MML_MAX_OUTPUTS];
	struct mml_comp *comp;
	u8 out_idx;

	/* index from engine array to tile_engines
	 * in mml_topology_path structure.
	 */
	u8 tile_eng_idx;
};

struct mml_frame_info_cache {
	u32 pixels;
	u32 duration;
	u32 remain;
	u8 dl_opp;
	u8 dc_opp;
};

struct mml_topology_info {
	enum mml_mode mode;
	u8 dst_cnt;
	struct mml_pq_config pq;
};

struct mml_topology_path {
	u8 path_id;
	struct mml_topology_info info;
	bool alpharot;

	/* Nodes of this path, each node may link to others as a tree.
	 * Some of nodes like mmlsys, mutex, may not link to others.
	 */
	struct mml_path_node nodes[MML_MAX_PATH_NODES];
	u8 node_cnt;

	/* sw workaround, cmdq use pipe to decide which hardware to be config */
	u8 hw_pipe;

	/* special ptr for mmlsys and mutex */
	struct mml_comp *mmlsys;
	u8 mmlsys_idx;
	struct mml_comp *mutex;
	u8 mutex_idx;

	/* another mmlsys, which dli into current mmlsys */
	struct mml_comp *mmlsys2;
	u8 mmlsys2_idx;
	struct mml_comp *mutex2;
	u8 mutex2_idx;

	/* Index of engine array,
	 * which represent only engines in data path.
	 * These engines join tile driver calculate.
	 */
	u8 tile_engines[MML_MAX_PATH_NODES];
	u8 tile_engine_cnt;

	/* Describe which engine is region pq in */
	u8 pq_rdma_id;
	u8 tdshp_id;

	/* Describe which engine is out0 and which is out1 */
	u32 out_engine_ids[MML_MAX_OUTPUTS];

	union {
		/* compatible single sys */
		struct {
			u8 aid_engine_ids[MML_MAX_AID_COMPS];
			u8 aid_eng_cnt;
		};

		struct {
			u8 ids[MML_MAX_AID_COMPS];
			u8 cnt;
		} aid_engine_sys[mml_max_sys];
	};

	/* cmdq client to compose command */
	u8 clt_id;
	struct cmdq_client *clt;

	/* Path configurations */
	u8 mux_group;
	union {
		/* multi-sys define */
		u64 reset_bits_sys[mml_max_sys];
		struct {
			u32 reset0;
			u32 reset1;
		} reset_sys[mml_max_sys];

		/* backward compatible only 1 mmlsys case */
		union {
			u64 reset_bits;
			struct {
				u32 reset0;
				u32 reset1;
			};
		};
	};

	/* engine flag for this path */
	u64 engine_flags;

	/* The flag to show which mmlsys enable in this path.
	 * Indexed by enum mml_sys_id, could be mml_sys_frame or mml_sys_tile
	 */
	bool sys_en[mml_max_sys];
};

struct mml_topology_ops {
	enum mml_mode (*query_mode)(struct mml_dev *mml,
				    struct mml_frame_info *info,
				    u32 *reason);
	enum mml_mode (*query_mode2)(struct mml_dev *mml,
				     struct mml_frame_info *info,
				     u32 *reason,
				     u32 panel_width,
				     u32 panel_height,
				     struct mml_frame_info_cache *info_cache);
	s32 (*init_cache)(struct mml_dev *mml,
			  struct mml_topology_cache *cache,
			  struct cmdq_client **clts,
			  u32 clt_cnt);
	s32 (*select)(struct mml_topology_cache *cache,
		      struct mml_frame_config *cfg);
	struct cmdq_client *(*get_racing_clt)(struct mml_topology_cache *cache,
					      u32 pipe);
	const struct mml_topology_path *(*get_dl_path)(struct mml_topology_cache *cache,
						       struct mml_frame_info *info,
						       u32 pipe,
						       struct mml_frame_size *panel);
	enum mml_mode (*support_couple)(void);
	bool (*support_dc2)(void);
	enum mml_hw_caps (*support_hw_caps)(void);
};

struct mml_path_client {
	/* running tasks on same cients from all configs */
	struct list_head tasks;
	/* current throughput */
	u32 throughput;
	u32 sys_en_ref[mml_max_sys];
};

struct mml_sys_qos {
	struct regulator *reg;
	struct clk *dvfs_clk;
	u32 opp_cnt;
	u32 opp_speeds[MML_MAX_OPPS];
	int opp_volts[MML_MAX_OPPS];
	u64 freq_max;
	u32 current_volt;
	u8 current_level;
	struct mutex qos_mutex;
};

struct mml_topology_cache {
	const struct mml_topology_ops *op;
	struct mml_topology_path paths[MML_MAX_PATH_CACHES];
	struct mml_path_client path_clts[MML_MAX_CMDQ_CLTS];
	u32 dpc_qos_ref;
	struct mutex qos_mutex;	/* lock to qos operation */
	struct mml_sys_qos *qos;
};

struct mml_comp_config {
	const struct mml_path_node *node;
	u8 pipe;
	u8 tile_eng_idx;

	/* The component private data. Components can store list of labels or
	 * more info for specific component data in this ptr.
	 */
	void *data;
};

#define dvfs_cache_sz(c, w, h, b, l) do { \
	c->max_frame_size.width = max(c->max_frame_size.width, (w)); \
	c->max_frame_size.height = max(c->max_frame_size.height, (h)); \
	c->total_line_bubble += (b); \
	c->total_latency += (l); \
	c->max_tput_pixel = (c->max_frame_size.width + c->total_line_bubble) * \
		(c->max_frame_size.height + c->total_latency); \
} while (0)

#define dvfs_cache_log(cache, comp, name) \
	mml_msg("[dvfs]tput cache %5s %2u bubble %u latency %u pixel %ux%u data %u", \
		name, comp->id, cache->total_line_bubble, cache->total_latency, \
		cache->max_frame_size.width, cache->max_frame_size.height, \
		cache->total_datasize)

struct mml_pipe_cache {
	/* command reuse */
	u32 label_cnt;

	/* dvfs qos part */
	u32 total_datasize;
	u32 max_tput_pixel;

	u32 total_line_bubble;
	u32 total_latency;
	struct mml_frame_size max_frame_size;

	/* Set in core and comp prepare. Used in tile prepare and make command */
	struct mml_comp_config cfg[MML_MAX_PATH_NODES];
};

struct mml_frame_config {
	struct list_head entry;
	struct mml_frame_info info;
	enum mml_sys_id sysid;		/* main mmlsys used for this config */

	/* frame input image size after rrot binning and rotate */
	struct mml_frame_size frame_in;
	/* frame input crop size after rrot binning and rotate by output */
	struct mml_crop frame_in_crop[MML_MAX_OUTPUTS];
	/* rrot output tile size to merge by pipe */
	struct mml_frame_size rrot_out[MML_PIPE_CNT];
	/* hdr, aal, and c3d input size from rdma or rsz */
	struct mml_frame_size frame_in_hdr;
	/* frame input pixel size (tile full size) after rrot binning and rotate,
	 * and crop offset, round up, and alignment.
	 */
	struct mml_frame_size frame_tile_sz;
	/* binning level config by: 2'd0: 1; 2'd1: 2; 2'd2: 4; 2'd3: 8 */
	u8 bin_x;
	u8 bin_y;
	/* frame output rotate/flip on wrot */
	u8 out_rotate[MML_MAX_OUTPUTS];
	bool out_flip[MML_MAX_OUTPUTS];
	/* frame output image size before wrot rotate */
	struct mml_frame_size frame_out[MML_MAX_OUTPUTS];
	/* direct-link input roi offset and output rect */
	struct mml_rect dl_in[MML_PIPE_CNT];
	struct mml_rect dl_out[MML_PIPE_CNT];

	struct list_head tasks;
	struct list_head await_tasks;
	struct list_head done_tasks;
	u32 run_task_cnt;
	u32 await_task_cnt;
	u8 done_task_cnt;
	/* mutex to join operations of task pipes, like buffer flush */
	struct mutex pipe_mutex;
	struct kref ref;

	/* see more detail in frame_calc_layer_hrt */
	u16 layer_w;
	u16 layer_h;
	u16 panel_w;
	u16 panel_h;

	/* dvfs and qos */
	u32 duration;
	u32 fps;

	/* display parameter */
	bool disp_dual;
	bool disp_vdo;

	/* platform driver */
	struct mml_ctx *ctx;
	struct mml_dev *mml;

	/* adaptor */
	u32 job_id;
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;
	const struct mml_config_ops *cfg_ops;

	/* workqueue for handling slow part of task done */
	struct workqueue_struct *wq_done;

	/* kthread worker for task done, assign from ctx */
	struct kthread_worker *ctx_kt_done;

	/* use on context wq_destroy */
	struct work_struct work_destroy;

	/* cache for pipe and component private data for this config */
	struct mml_pipe_cache cache[MML_PIPE_CNT];

	/* topology */
	const struct mml_topology_path *path[MML_PIPE_CNT];
	bool dual:1;
	bool alpharot:1;
	bool alpharsz:1;
	bool rgbrot:1;
	bool shadow:1;
	bool framemode:1;
	bool nocmd:1;
	bool err:1;
	bool dpc:1;
	bool rrot_dual:1;
	bool merge2p:1;
	bool rsz_front:1;

	/* tile */
	struct mml_frame_tile *frame_tile[MML_PIPE_CNT];
	u32 hist_div[MML_MAX_PATH_NODES];
	struct mutex hist_div_mutex;
	struct timespec64 dvfs_boost_time;
};

struct mml_dma_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	u64 iova;
	void *va;
};

struct mml_file_buf {
	/* dma buf heap */
	union {
		struct mml_dma_buf dma[MML_MAX_PLANES];
		u64 apu_handle;
	};
	u32 size[MML_MAX_PLANES];
	u8 cnt;
	struct dma_fence *fence;
	u64 map_time;
	u64 unmap_time;

	/* buffer usage */
	bool flush:1;
	bool invalid:1;
};

struct mml_task_buffer {
	struct mml_file_buf src;
	struct mml_file_buf seg_map;
	struct mml_file_buf dest[MML_MAX_OUTPUTS];
	u8 dest_cnt;
	bool flushed;
};

enum mml_task_state {
	MML_TASK_INITIAL,
	MML_TASK_DUPLICATE,
	MML_TASK_REUSE,
	MML_TASK_RUNNING,
	MML_TASK_IDLE
};

struct mml_reuse_offset {
	u16 label_idx;
	u16 offset;
	u16 cnt;
};

struct mml_reuse_array {
	struct mml_reuse_offset *offs;
	u16 idx;
	u16 offs_size;
};

/* same as CMDQ_NUM_CMD(CMDQ_CMD_BUFFER_SIZE) */
#define MML_REUSE_OFFSET_MAX	480

struct mml_task_reuse {
	struct cmdq_reuse *labels;
	u16 *label_mods;
	u16 label_idx;
	bool *label_check;
};

/* pipe info for mml_task */
struct mml_task_pipe {
	struct mml_task *task;	/* back to task */
	struct list_head entry_clt;
	u32 throughput;
	u32 bandwidth;
	struct completion ready;	/* ready for submit */

	struct {
		bool clk;
	} en;
};

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
enum mml_dump_buf_t {
	MMLDUMPT_SRC,
	MMLDUMPT_DEST,
	MMLDUMPT_CNT
};
#endif

enum mml_adaptor_type {
	MML_ADAPTOR_DRM,
	MML_ADAPTOR_DLE,
	MML_ADAPTOR_M2M,
};

struct mml_task {
	struct list_head entry;
	struct mml_job job;
	struct mml_frame_config *config;
	struct mml_task_buffer buf;
	struct mml_pq_param pq_param[MML_MAX_OUTPUTS];
	struct timespec64 submit_time;
	struct timespec64 end_time;
	struct dma_fence *fence;
	enum mml_task_state state;
	enum mml_adaptor_type adaptor_type;
	struct kref ref;
	struct mml_task_pipe pipe[MML_PIPE_CNT];
	struct cmdq_backup backup_crc_rdma[MML_PIPE_CNT]; /* rdma or rrot0 and rrot0_2nd */
	struct cmdq_backup backup_crc_wdma[MML_PIPE_CNT];
	u32 dpc_srt_bw[mml_max_sys];
	u32 dpc_hrt_bw[mml_max_sys];
	u32 dpc_srt_write_bw[mml_max_sys];
	u32 dpc_hrt_write_bw[mml_max_sys];

	struct cmdq_backup perf_prete;
	struct cmdq_backup perf_dispready;
	struct cmdq_backup perf_sof;

	/* mml context */
	struct mml_ctx *ctx;
	void *cb_param;

	/* command */
	struct cmdq_pkt *pkts[MML_PIPE_CNT];

	/* make command cache labels for reuse command */
	struct mml_task_reuse reuse[MML_PIPE_CNT];
	struct cmdq_poll_reuse dpc_reuse_sys;
	struct cmdq_poll_reuse dpc_reuse_mutex;

	/* config and done on thread */
	struct kthread_work work_config[MML_PIPE_CNT];
	struct work_struct work_done;
	struct kthread_work kt_work_done;
	atomic_t pipe_done;

	/* mml pq task */
	struct mml_pq_task *pq_task;

	bool done;
	bool err;
	bool dump_full;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	/* frame dump */
	bool dump_queued[MMLDUMPT_CNT];
#endif

	u64 config_pipe_time[MML_PIPE_CNT];
	u64 bw_time[MML_PIPE_CNT];
	u64 freq_time[MML_PIPE_CNT];
	u64 wait_fence_time[MML_PIPE_CNT];
	u64 flush_time[MML_PIPE_CNT];
	u32 src_crc[MML_PIPE_CNT];
	u32 dest_crc[MML_PIPE_CNT];
};

struct tile_func_block;
union mml_tile_data;

struct mml_tile_region {
	u16 xs;
	u16 xe;
	u16 ys;
	u16 ye;
};

struct mml_tile_offset {
	u32 x;
	u32 y;
	u32 x_sub;
	u32 y_sub;
};

struct mml_tile_engine {
	/* component id for dump */
	u8 comp_id;

	/* tile input/output region */
	struct mml_tile_region in;
	struct mml_tile_region out;

	struct mml_tile_offset luma;
	struct mml_tile_offset chroma;
};

struct mml_comp_tile_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg,
		       struct tile_func_block *func,
		       union mml_tile_data *data);
	s32 (*region_pq_bw)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg,
		    struct mml_tile_engine *ref_tile,
		    struct mml_tile_engine *tile);
};

struct mml_comp_config_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg);
	s32 (*buf_map)(struct mml_comp *comp, struct mml_task *task,
		       const struct mml_path_node *node);
	void (*buf_unmap)(struct mml_comp *comp, struct mml_task *task,
			  const struct mml_path_node *node);
	s32 (*buf_prepare)(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg);
	void (*buf_unprepare)(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg);
	u32 (*get_label_count)(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg);
	/* op to make command in frame change case */
	s32 (*init)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg);
	s32 (*frame)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg);
	s32 (*tile)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg, u32 idx);
	s32 (*mutex)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg);
	s32 (*wait_sof)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg, u32 idx);
	s32 (*wait)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg, u32 idx);
	void (*reset)(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg);
	s32 (*post)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg);
	s32 (*done)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg);
	/* op to make command in reuse case */
	s32 (*reframe)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg);
	s32 (*repost)(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg);
};

struct mml_comp_hw_ops {
	void (*init_frame_done_event)(struct mml_comp *comp, u32 event);
	s32 (*pw_enable)(struct mml_comp *comp, const s8 mode);
	s32 (*pw_disable)(struct mml_comp *comp, const s8 mode);
	s32 (*mminfra_pw_enable)(struct mml_comp *comp);
	s32 (*mminfra_pw_disable)(struct mml_comp *comp);
	s32 (*clk_enable)(struct mml_comp *comp);
	s32 (*clk_disable)(struct mml_comp *comp, bool dpc);
	u32 (*qos_datasize_get)(struct mml_task *task,
				struct mml_comp_config *ccfg);
	u32 (*qos_stash_bw_get)(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg,
				u32 *srt_bw, u32 *hrt_bw);
	u32 (*qos_format_get)(struct mml_task *task,
			      struct mml_comp_config *ccfg);
	void (*qos_set)(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg, u32 throughput, u32 tput_up);
	void (*qos_clear)(struct mml_comp *comp, struct mml_task *task, bool dpc);
	void (*task_done)(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg);
};

struct mml_comp_debug_ops {
	void (*dump)(struct mml_comp *comp);
	void (*reset)(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe);
};

struct mml_comp {
	u32 id;
	u32 sub_idx;
	enum mml_sys_id sysid;
	void __iomem *base;
	phys_addr_t base_pa;
	struct clk *clks[2];
	struct device *larb_dev;
	phys_addr_t larb_base;
	u32 larb_port;
	s32 pw_cnt;
	s32 mminfra_pw_cnt;
	s32 clk_cnt;
	u32 srt_bw;
	u32 hrt_bw;
	u32 stash_srt_bw;
	u32 stash_hrt_bw;
	struct icc_path *icc_path;
	struct icc_path *icc_dpc_path;
	struct icc_path *icc_stash_path;
	struct icc_path *icc_dpc_stash_path;
	const struct mml_comp_tile_ops *tile_ops;
	const struct mml_comp_config_ops *config_ops;
	const struct mml_comp_hw_ops *hw_ops;
	const struct mml_comp_debug_ops *debug_ops;
	const char *name;
	bool bound;
};

struct mml_tile_config {
	/* index of the tile */
	u16 tile_no;

	/* current horizontal tile number, from left to right */
	u16 h_tile_no;
	/* current vertical tile number, from top to bottom */
	u16 v_tile_no;

	/* align with tile_engine_cnt */
	u8 engine_cnt;
	struct mml_tile_engine tile_engines[MML_MAX_PATH_NODES];

	/* assign by wrot, end of current tile line */
	bool eol;
};

/* array size must align MAX_TILE_FUNC_NO in tile_driver.h */
struct mml_tile_cache {
	void *func_list[MML_MAX_PATH_NODES];
	struct mml_tile_config *tiles;
	bool ready;
};

struct mml_frame_tile {
	/* total tile number */
	u16 tile_cnt;
	/* total horizontal tile number */
	u16 h_tile_cnt;
	/* total vertical tile number */
	u16 v_tile_cnt;
	/* source crop with tile overhead */
	struct mml_rect src_crop;
	struct mml_tile_config *tiles;
};

struct mml_frm_dump_data {
	const char *prefix;
	char name[128];
	void *frame;
	u32 bufsize;
	u32 size;
	u8 dump_option;
};

/* config_get_tile - helper inline func which uses tile index to get
 * mml_tile_engine instance inside config.
 *
 * @cfg:	The mml_frame_config contains tile.
 * @ccfg:	The mml_comp_config of which tile engine.
 * @idx:	Tile index to mml_frame_tile->tiles array.
 *
 * Return:	mml_tile_engine struct pointer to related tile and engine.
 */
static inline struct mml_tile_engine *config_get_tile(
	struct mml_frame_config *cfg, struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_tile_engine *engines =
		cfg->frame_tile[ccfg->pipe]->tiles[idx].tile_engines;

	return &engines[ccfg->tile_eng_idx];
}

/* config_get_next_node_tile - helper inline func which uses tile index to get
 * next node mml_tile_engine instance inside config.
 *
 * @cfg:	The mml_frame_config contains tile.
 * @ccfg:	The mml_comp_config of which tile engine.
 * @idx:	Tile index to mml_frame_tile->tiles array.
 *
 * Return:	mml_tile_engine struct pointer to related tile and engine.
 */
static inline struct mml_tile_engine *config_get_next_node_tile(
	struct mml_frame_config *cfg, struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_tile_engine *engines =
		cfg->frame_tile[ccfg->pipe]->tiles[idx].tile_engines;

	return &engines[ccfg->node->next[0]->tile_eng_idx];
}

/*
 * mml_topology_register_ip - Register topology operation to node list in core
 * by giving a name. Core will later call init with IP name to match one of
 * operation node in list.
 *
 * @ip:	name of IP, like mt6983
 * @op: operations of specific IP
 *
 * Return:
 */
int mml_topology_register_ip(const char *ip, const struct mml_topology_ops *op);

/*
 * mml_topology_unregister_ip - Unregister topology operation in node list in
 * core by giving a name. Note all config related to this topology MUST
 * shutdown before unregister.
 *
 * @ip:	name of IP, like mt6983
 */
void mml_topology_unregister_ip(const char *ip);

/*
 * mml_topology_get_mode_caps - Query mml supported modes. The mode bits refer to enum mml_mode
 *
 * Return:	Bits to represent enabled mode in current platform.
 */
u32 mml_topology_get_mode_caps(void);

/*
 * mml_topology_get_hw_caps - Query mml hardware capability.
 *
 * Return:	Hardware caps in bits. See enum mml_hw_caps.
 */
u32 mml_topology_get_hw_caps(void);

/*
 * mml_topology_create - Create cache structure and set one of platform
 * topology to giving platform device.
 *
 * @mml:	mml driver private data
 * @pdev:	platform device of mml
 * @clts:	The cmdq clitns array. Client instance will assign to path.
 * @clts_cnt:	Count of cmdq client.
 *
 * Return: Topology cache struct which alloc by giving pdev.
 */
struct mml_topology_cache *mml_topology_create(struct mml_dev *mml,
					       struct platform_device *pdev,
					       struct cmdq_client **clts,
					       u32 clts_cnt);

/*
 * mml_core_time_dur_us - duration between 2 time spec
 *
 * @lhs:	end time
 * @rhs:	start time
 *
 * Return: Topology cache struct which alloc by giving pdev.
 */
u64 mml_core_time_dur_us(const struct timespec64 *lhs, const struct timespec64 *rhs);

/*
 * mml_core_dump_buf - dump mml frame data into cache buffer
 *
 * @task:	task to dump
 * @data:	frame data to buffer
 * @buf:	the mml file buf struct to copy data
 * @frm:	frame data related to in/out 0/1 buffer
 */
void mml_core_dump_buf(struct mml_task *task, const struct mml_frame_data *data,
	struct mml_file_buf *buf, struct mml_frm_dump_data *frm);

/*
 * mml_core_get_dump_inst - return debug dump buffer with current buf size
 *
 * @size:	buffer size in bytes
 * @raw:	buffer for readable instruction dump
 * @size_raw:	raw buffer size
 *
 * Return:	The inst buffer in string.
 */
char *mml_core_get_dump_inst(u32 *size, void **raw, u32 *size_raw);

/**
 * mml_core_create_task -
 *
 * Return:
 */
struct mml_task *mml_core_create_task(u32 jobid);

/**
 * mml_core_destroy_task -
 * @task:
 *
 */
void mml_core_destroy_task(struct mml_task *task);

/*
 * mml_core_init_config - initialize data use in core.
 *
 * @cfg: The frame config to be init.
 */
void mml_core_init_config(struct mml_frame_config *cfg);

/*
 * mml_core_deinit_config - destroy meta or content store in frame config
 * which allocated in core.
 *
 * @cfg: The frame config to be deinit.
 */
void mml_core_deinit_config(struct mml_frame_config *cfg);

/**
 * mml_core_submit_task - submit and config the task. If the queue operation is
 * presented, queue the task in config work thread; otherwise, config the task
 * in current thread.
 *
 * @cfg:	the frame config to handle task
 * @task:	task to execute
 */
void mml_core_submit_task(struct mml_frame_config *cfg, struct mml_task *task);

/**
 * mml_core_stop_racing - set next spr to 1 to stop current racing task
 *
 * @cfg:	the frame config to stop
 * @force:	true to use cmdq stop gce hardware thread, false to set next_spr
 *		to next only.
 */
void mml_core_stop_racing(struct mml_frame_config *cfg, bool force);


void mml_add_reuse_label(u32 comp_id, struct mml_task_reuse *reuse, u16 *label_idx, u32 value);

/* mml_assign - assign to reg_idx with value. Cache the label of this
 * instruction to mml_pipe_cache and record its entry into label_array.
 *
 * @comp_id	component id for check
 * @pkt:	cmdq task
 * @reg_idx:	common purpose register index
 * @value:	value to write
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @cache:	task cache from mml config
 * @label_idx:	ptr to label entry point to write instruction
 *
 * return:	0 if success, error no if fail
 */
s32 mml_assign(u32 comp_id, struct cmdq_pkt *pkt, u16 reg_idx, u32 value,
	       struct mml_task_reuse *reuse,
	       struct mml_pipe_cache *cache,
	       u16 *label_idx);

/* mml_write - write to addr with value and mask. Cache the label of this
 * instruction to mml_pipe_cache and record its entry into label_array.
 *
 * @comp_id	component id for check
 * @pkt:	cmdq task
 * @addr:	register addr or dma addr
 * @value:	value to write
 * @mask:	mask to value
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @cache:	task cache from mml config
 * @label_idx:	ptr to label entry point to write instruction
 *
 * return:	0 if success, error no if fail
 */
s32 mml_write(u32 comp_id, struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	      struct mml_task_reuse *reuse,
	      struct mml_pipe_cache *cache,
	      u16 *label_idx);

/* mml_update - update new value to cache, which entry index from label.
 *
 * @comp_id	component id for check
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @label_idx:	label entry point to instruction want to update
 * @value:	value to be update
 */
void mml_update(u32 comp_id, struct mml_task_reuse *reuse, u16 label_idx, u32 value);

/* mml_reuse_touch - mark check without change value
 *
 * @comp_id	component id for check
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @label_idx:	label entry point to instruction want to update
 */
void mml_reuse_touch(u32 comp_id, struct mml_task_reuse *reuse, u16 label_idx);

s32 mml_write_array(u32 comp_id, struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	struct mml_task_reuse *reuse, struct mml_pipe_cache *cache,
	struct mml_reuse_array *reuses);

void mml_update_array(u32 comp_id, struct mml_task_reuse *reuse,
	struct mml_reuse_array *reuses, u32 reuse_idx, u32 off_idx, u32 value);

int mml_tracing_mark_write(char *fmt, ...);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)

/* debug parameter to enable dumpsrv */
extern int mml_dump_srv;
extern int mml_dump_srv_opt;

/* control dumpsrv function work or stop */
enum dump_srv_ctrl {
	DUMPCTRL_DISABLE = 0,
	DUMPCTRL_PAUSE,
	DUMPCTRL_ENABLE,
};

/* bits of mml_dump_srv_opt */
enum dump_srv_option {
	DUMPOPT_SRC = 0x1,
	DUMPOPT_DEST = 0x2,
	DUMPOPT_ALL = 0x3,
	DUMPOPT_SRC_ASYNC = 0x4,
	DUMPOPT_DEST_ASYNC = 0x8,
	DUMPOPT_ONCE = 0x10,
};

/* mml_dump_buf - wakeup dump service to dump buffer into file
 *
 * @task:	mml task to dump
 * @data:	frame data related to buffer, src or one of dest
 * @width:	buffer width
 * @height:	buffer height
 * @prefix:	string insert into file name
 * @fmt:	formating string
 * @buf:	mml buffer to dump
 * @buf_type:	buffer type, source or destination
 * @async:	set true to force config thread wait dump complete before next task
 */
void mml_dump_buf(struct mml_task *task, struct mml_frame_data *data,
	u32 width, u32 height, const char *prefix,
	char *fmt, struct mml_file_buf *buf, enum mml_dump_buf_t buf_type, bool async);

/* mml_dump_wait - wait the dump finish of last call to mml_dump_buf
 *
 * @task:	mml task to dump
 * @buf_type:	buffer type, source or destination
 */
void mml_dump_wait(struct mml_task *task, enum mml_dump_buf_t buf_type);

#endif	/* CONFIG_MTK_MML_DEBUG */

#endif	/* __MTK_MML_CORE_H__ */
