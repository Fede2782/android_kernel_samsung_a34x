// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/syscalls.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <uapi/linux/sched/types.h>
#include <drm/drm_auth.h>
#define NONE DEL_NONE
#include <linux/scmi_protocol.h>
#undef NONE
#include <soc/mediatek/dramc.h>

#include "tinysys-scmi.h"
#include "drm_internal.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_session.h"
#include "mtk_fence.h"
#include "mtk_debug.h"
#include "mtk_layering_rule.h"
#include "mtk_rect.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_panel_ext.h"
#include <linux/clk.h>
#include "mtk_disp_pmqos.h"
#include "mtk_disp_recovery.h"
#include "mtk_drm_arr.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_tdshp.h"
#include "mtk_disp_color.h"
#include "mtk_disp_gamma.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_c3d.h"
#include "mtk_disp_chist.h"
#include "mtk_lease.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_drm_trace.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_vidle.h"
#include "mtk_vdisp_common.h"

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
#include <linux/interconnect.h>
#include "dvfsrc-exp.h"
#endif
#include "mtk_dsi.h"

#include "mtk_drm_mmp.h"
/* *******Panel Master******** */
#include "mtk_fbconfig_kdebug.h"
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DPTX_AUTO)
#include "mtk_drm_dp/mtk_drm_dp_api.h"
#else
#include "mtk_dp_api.h"
#endif
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
#include "mtk_drm_edp/mtk_drm_edp_api.h"
#endif
//#include "swpm_me.h"
//#include "include/pmic_api_buck.h"
#include <../drivers/gpu/drm/mediatek/mml/mtk-mml.h>

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "../mml/mtk-mml-driver.h"

#include "slbc_ops.h"
#include <linux/syscalls.h>

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
#include <linux/nvmem-consumer.h>
#endif

#include "mtk-mminfra-debug.h"
#include "mtk_disp_bdg.h"

#include "mtk_disp_vdisp_ao.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
#include "mtk_drm_auto/mtk_drm_crtc_auto.h"
#endif

#define CLKBUF_COMMON_H

#define DRIVER_NAME "mediatek"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20150513"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0
#define IDLE_FPS 10 /*when fps is less than or euqal to 10, hwc not sending hw vsync*/

void disp_dbg_deinit(void);
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *dev);

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
u32 *disp_perfs;
#endif

static atomic_t top_isr_ref; /* irq power status protection */
static atomic_t top_clk_ref; /* top clk status protection*/
static spinlock_t top_clk_lock; /* power status protection*/

struct device *g_dpc_dev; /* mminfra power control */

unsigned long long mutex_time_start;
unsigned long long mutex_time_end;
long long mutex_time_period;
const char *mutex_locker;

int aod_scp_flag;
unsigned long long mutex_nested_time_start;
unsigned long long mutex_nested_time_end;
long long mutex_nested_time_period;
const char *mutex_nested_locker;
static unsigned int g_disp_plat_dbg_addr;
static unsigned int g_disp_plat_dbg_size;
static void __iomem *g_disp_plat_dbg_buf_addr;
static struct scmi_tinysys_info_st *tinfo;
static int feature_id;

struct lcm_fps_ctx_t lcm_fps_ctx[MAX_CRTC];

static int manual_shift;
static bool no_shift;

static int mml_hw_caps;
static int mml_mode_caps;

struct mtk_se_dma_map {
	int fd;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct list_head list;
};

static struct mtk_se_dma_map dma_map_list;

#ifdef DRM_OVL_SELF_PATTERN
struct drm_crtc *test_crtc;
void *test_va;
dma_addr_t test_pa;
#endif

struct mtk_aod_scp_cb aod_scp_ipi;

struct mtk_drm_disp_sec_cb disp_sec_cb;
EXPORT_SYMBOL(disp_sec_cb);

void **mtk_drm_disp_sec_cb_init(void)
{
	DDPMSG("%s+\n", __func__);
	return (void **)&disp_sec_cb.cb;
}
EXPORT_SYMBOL(mtk_drm_disp_sec_cb_init);

struct mtk_drm_disp_mtee_cb disp_mtee_cb;
EXPORT_SYMBOL(disp_mtee_cb);

void mtk_drm_svp_init(struct drm_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		DDPMSG("%s, disp debug init with invalid dev\n", __func__);
	else
		DDPMSG("%s, disp debug init\n", __func__);
	disp_mtee_cb.dev = dev;
}

void **mtk_drm_disp_mtee_cb_init(void)
{
	DDPMSG("%s+\n", __func__);
	return (void **)&disp_mtee_cb.cb;
}
EXPORT_SYMBOL(mtk_drm_disp_mtee_cb_init);

void mtk_aod_scp_ipi_init(struct mtk_aod_scp_cb *cb)
{
	if (!aod_scp_flag) {
		aod_scp_ipi.send_ipi = NULL;
		aod_scp_ipi.module_backup = NULL;
		return;
	}

	aod_scp_ipi.send_ipi = cb->send_ipi;
	aod_scp_ipi.module_backup = cb->module_backup;
}
EXPORT_SYMBOL(mtk_aod_scp_ipi_init);

static ssize_t read_disp_plat_dbg_buf(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buff, loff_t pos, size_t count)
{
	ssize_t bytes = 0, ret;

	if (!(g_disp_plat_dbg_addr))
		return 0;

	if (!(g_disp_plat_dbg_size))
		return 0;

	ret = memory_read_from_buffer(buff, count, &pos,
		g_disp_plat_dbg_buf_addr, g_disp_plat_dbg_size);

	if (ret < 0)
		return ret;
	else
		return bytes + ret;
}

static struct bin_attribute disp_plat_dbg_buf_attr = {
	.attr = {.name = "disp_plat_dbg_buf", .mode = 0444},
	.read = read_disp_plat_dbg_buf,
};
int scmi_set(void *buffer)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct disp_plat_dbg_scmi_data *scmi_data = buffer;

	DDPMSG("%s: cmd:%d (%d,%d,%d,%d)\n", __func__, scmi_data->cmd,
			scmi_data->p1, scmi_data->p2, scmi_data->p3, scmi_data->p4);
	ret = scmi_tinysys_common_set(tinfo->ph, feature_id, scmi_data->cmd,
			scmi_data->p1, scmi_data->p2, scmi_data->p3, scmi_data->p4);
#endif
	return ret;
}
void disp_plat_dbg_init(void)
{
	int err;
	struct disp_plat_dbg_scmi_data scmi_data;

	DDPMSG("%s:addr=0x%x,size=0x%x\n", __func__, g_disp_plat_dbg_addr, g_disp_plat_dbg_size);

	if ((g_disp_plat_dbg_addr > 0) && (g_disp_plat_dbg_size > 0)) {
		if (!tinfo) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
			tinfo = get_scmi_tinysys_info();
#endif
			if ((IS_ERR_OR_NULL(tinfo)) || (IS_ERR_OR_NULL(tinfo->ph))) {
				DDPMSG("%s: tinfo or tinfo->ph is wrong!!\n", __func__);
				tinfo = NULL;
				} else {
					err = of_property_read_u32(tinfo->sdev->dev.of_node,
						"scmi-dispplatdbg", &feature_id);
					if (err) {
						DDPMSG("get scmi-dispplatdbg fail\n");
						return;
					}

					DDPMSG("%s: get scmi_smi succeed id=%d!!\n",
						__func__, feature_id);

					scmi_data.cmd = DISP_PLAT_DBG_INIT;
					scmi_data.p1 = g_disp_plat_dbg_addr;
					scmi_data.p2 = g_disp_plat_dbg_size;

					err = scmi_set(&scmi_data);
					if (err)
						DDPMSG("%s: call scmi_tinysys_common_set err=%d\n",
						__func__, err);
				}
			}
		}
	DDPMSG("%s++\n", __func__);
}

int mtk_atoi(const char *str)
{
	int len = strlen(str);
	int num = 0, i = 0;
	int sign = (str[0] == '-') ? -1 : 1;

	if (sign == -1)
		i = 1;
	else
		i = 0;
	for (; i < len; i++) {
		if (str[i] < '0' || str[i] > '9')
			break;
		num *= 10;
		num += (int)(str[i] - '0');
	}

	pr_notice("[debug] num=%d sign=%d\n",
			num, sign);

	return num * sign;
}

void disp_drm_debug(const char *opt)
{
	pr_notice("[debug] opt=%s\n", opt);
	if (strncmp(opt, "shift:", 6) == 0) {

		int len = strlen(opt);
		char buf[100];

		strcpy(buf, opt + 6);
		buf[len - 6] = '\0';

		pr_notice("[debug] buf=%s\n",
			buf);

		manual_shift = mtk_atoi(buf);

		pr_notice("[debug] manual_shift=%d\n",
			manual_shift);
	} else if (strncmp(opt, "no_shift:", 9) == 0) {
		no_shift = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[debug] no_shift=%d\n",
			no_shift);
	}
}

static inline struct mtk_atomic_state *to_mtk_state(struct drm_atomic_state *s)
{
	return container_of(s, struct mtk_atomic_state, base);
}

static void mtk_atomic_state_free(struct kref *k)
{
	struct mtk_atomic_state *mtk_state =
		container_of(k, struct mtk_atomic_state, kref);
	struct drm_atomic_state *state = &mtk_state->base;

	drm_atomic_state_clear(state);
	drm_atomic_state_default_release(state);
	kfree(mtk_state);
}

static void mtk_atomic_state_put(struct drm_atomic_state *state)
{
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);

	kref_put(&mtk_state->kref, mtk_atomic_state_free);
}

void mtk_atomic_state_put_queue(struct drm_atomic_state *state)
{
	struct drm_device *drm = state->dev;
	struct mtk_drm_private *mtk_drm = drm->dev_private;
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);
	unsigned long flags;

	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_add_tail(&mtk_state->list, &mtk_drm->unreference.list);
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);

	schedule_work(&mtk_drm->unreference.work);
}

static uint32_t mtk_atomic_crtc_mask(struct drm_device *drm,
				     struct drm_atomic_state *state)
{
	uint32_t crtc_mask = 0;
	int i;

	for (i = 0, crtc_mask = 0; i < drm->mode_config.num_crtc; i++) {
		struct __drm_crtcs_state *crtc = &(state->crtcs[i]);

		if (!crtc->ptr)
			continue;

		crtc_mask |= (1 << drm_crtc_index(crtc->ptr));
	}

	return crtc_mask;
}

static void mtk_unreference_work(struct work_struct *work)
{
	struct mtk_drm_private *mtk_drm =
		container_of(work, struct mtk_drm_private, unreference.work);
	unsigned long flags;
	struct mtk_atomic_state *state, *tmp;

	/*
	 * framebuffers cannot be unreferenced in atomic context.
	 * Therefore, only hold the spinlock when iterating unreference_list,
	 * and drop it when doing the unreference.
	 */
	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_for_each_entry_safe(state, tmp, &mtk_drm->unreference.list, list) {
		list_del(&state->list);
		spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
		drm_atomic_state_put(&state->base);
		spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	}
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
}

static void __always_unused mtk_atomic_schedule(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	private->commit.state = state;
	schedule_work(&private->commit.work);
}

static void mtk_atomic_wait_for_fences(struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i;

	for_each_old_plane_in_state(state, plane, plane_state, i)
		mtk_fb_wait(plane->state->fb);
}

#define UNIT 32768
static void mtk_atomic_rsz_calc_dual_params(
	struct drm_crtc *crtc, struct mtk_rect *src_roi,
	struct mtk_rect *dst_roi,
	struct mtk_rsz_param param[])
{
	int left = dst_roi->x;
	int right = dst_roi->x + dst_roi->width - 1;
	int tile_idx = 0;
	u32 out_tile_loss[2] = {0, 0};
	u32 in_tile_loss[2] = {out_tile_loss[0] + 4, out_tile_loss[1] + 4};
	u32 step = 0;
	s32 init_phase = 0;
	s32 offset[2] = {0};
	s32 int_offset[2] = {0};
	s32 sub_offset[2] = {0};
	u32 tile_in_len[2] = {0};
	u32 tile_out_len[2] = {0};
	u32 out_x[2] = {0};
	bool is_dual = false;
	int width = crtc->state->adjusted_mode.hdisplay;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	struct total_tile_overhead to_info;

	to_info = mtk_crtc_get_total_overhead(mtk_crtc);
	DDPINFO("%s:overhead is_support:%d, width L:%d R:%d\n", __func__,
			to_info.is_support, to_info.left_in_width, to_info.right_in_width);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && drm_crtc_index(crtc) == 0)
		width = mtk_ddp_comp_io_cmd(
				output_comp, NULL,
				DSI_GET_VIRTUAL_WIDTH, NULL);

	if (right < width / 2)
		tile_idx = 0;
	else if (left >= width / 2)
		tile_idx = 1;
	else
		is_dual = true;


	DDPINFO("%s :idx:%d,width:%d,src_w:%d,dst_w:%d,src_x:%d\n", __func__,
	       tile_idx, width, src_roi->width, dst_roi->width, src_roi->x);
	step = (UNIT * (src_roi->width - 1) + (dst_roi->width - 2)) /
			(dst_roi->width - 1); /* for ceil */

	offset[0] = (step * (dst_roi->width - 1) -
		UNIT * (src_roi->width - 1)) / 2;
	init_phase = UNIT - offset[0];
	sub_offset[0] = -offset[0];

	if (sub_offset[0] < 0) {
		int_offset[0]--;
		sub_offset[0] = UNIT + sub_offset[0];
	}
	if (sub_offset[0] >= UNIT) {
		int_offset[0]++;
		sub_offset[0] = sub_offset[0] - UNIT;
	}

	if (is_dual) {
		/*left side*/
		out_tile_loss[0] = (to_info.is_support ? to_info.left_overhead : 0);
		in_tile_loss[0] = out_tile_loss[0] + 4;

		DDPINFO("%s :out_tile_loss[0]:%d, in_tile_loss[0]:%d\n", __func__,
			out_tile_loss[0], in_tile_loss[0]);

		/*right bound - left bound + tile loss*/
		tile_in_len[0] = (((width / 2 - dst_roi->x) * src_roi->width * 10) /
			dst_roi->width + 5) / 10;
		if (tile_in_len[0] + in_tile_loss[0] >= src_roi->width)
			in_tile_loss[0] = src_roi->width - tile_in_len[0];

		tile_out_len[0] = width / 2 - dst_roi->x + out_tile_loss[0];
		if (tile_in_len[0] + in_tile_loss[0] > tile_out_len[0])
			in_tile_loss[0] = tile_out_len[0] - tile_in_len[0];
		tile_in_len[0] += in_tile_loss[0];
		out_x[0] = dst_roi->x;
	} else {
		tile_in_len[0] = src_roi->width;
		tile_out_len[0] = dst_roi->width;
		if (tile_idx == 0)
			out_x[0] = dst_roi->x;
		else
			out_x[0] = dst_roi->x - width / 2 +
			(to_info.is_support ? to_info.right_overhead : 0);
	}

	param[tile_idx].out_x = out_x[0];
	param[tile_idx].step = step;
	param[tile_idx].int_offset = (u32)(int_offset[0] & 0xffff);
	param[tile_idx].sub_offset = (u32)(sub_offset[0] & 0x1fffff);
	param[tile_idx].in_len = tile_in_len[0];
	param[tile_idx].out_len = tile_out_len[0];
	DDPINFO("%s,%d:%s:step:%u,offset:%u.%u,len:%u->%u,out_x:%u\n", __func__, __LINE__,
	       is_dual ? "dual" : "single",
	       param[tile_idx].step,
	       param[tile_idx].int_offset,
	       param[tile_idx].sub_offset,
	       param[tile_idx].in_len,
	       param[tile_idx].out_len,
	       param[tile_idx].out_x);

	if (int_offset[0] < -1)
		DDPINFO("%s,%d:pipe0_scale_err\n", __func__, __LINE__);
	if (!is_dual)
		return;

	/* right half */
	out_tile_loss[1] = (to_info.is_support ? to_info.right_overhead : 0);
	in_tile_loss[1] = out_tile_loss[1] + 4;
	DDPINFO("%s :out_tile_loss[1]:%d, in_tile_loss[1]:%d\n", __func__,
		out_tile_loss[1], in_tile_loss[1]);

	tile_out_len[1] = dst_roi->width - (tile_out_len[0] - out_tile_loss[0]) +
			out_tile_loss[1];
	tile_in_len[1] = (((tile_out_len[1] - out_tile_loss[1]) * src_roi->width *
			10) / dst_roi->width + 5) / 10;
	if (tile_in_len[1] + in_tile_loss[1] >= src_roi->width)
		in_tile_loss[1] = src_roi->width - tile_in_len[1];

	if (tile_in_len[1] + in_tile_loss[1] > tile_out_len[1])
		in_tile_loss[1] = tile_out_len[1] - tile_in_len[1];
	tile_in_len[1] += in_tile_loss[1];

	offset[1] = (-offset[0]) + ((tile_out_len[0] - out_tile_loss[0] -
			out_tile_loss[1]) * step) -
			(src_roi->width - tile_in_len[1]) * UNIT + manual_shift;
	/*
	 * offset[1] = (init_phase + dst_roi->width / 2 * step) -
	 *	(src_roi->width / 2 - in_tile_loss[1] - (offset[0] ? 1 : 0) + 1) * UNIT +
	 *	UNIT + manual_shift;
	 */
	if (no_shift)
		offset[1] = 0;
	DDPINFO("%s,in_ph:%d,man_sh:%d,off[1]:%d\n", __func__, init_phase, manual_shift, offset[1]);
	int_offset[1] = offset[1] / UNIT;
	if (offset[1] >= 0)
		sub_offset[1] = offset[1] - UNIT * int_offset[1];
	else
		sub_offset[1] = UNIT * int_offset[1] - offset[1];
	param[1].step = step;
	param[1].out_x = 0;
	param[1].int_offset = (u32)(int_offset[1] & 0xffff);
	param[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	param[1].in_len = tile_in_len[1];
	param[1].out_len = tile_out_len[1];

	DDPINFO("%s,%d :%s:step:%u,offset:%u.%u,len:%u->%u,out_x:%u\n", __func__, __LINE__,
	       is_dual ? "dual" : "single",
	       param[1].step,
	       param[1].int_offset,
	       param[1].sub_offset,
	       param[1].in_len,
	       param[1].out_len,
	       param[1].out_x);

	if (int_offset[1] < -1 || tile_out_len[1] >= dst_roi->width)
		DDPINFO("%s,%d:pipe1_scale_err\n", __func__, __LINE__);
}

static void mtk_atomic_disp_rsz_roi(struct drm_device *dev,
				    struct drm_atomic_state *old_state)
{
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_crtc_state;
	int i, j;
	struct mtk_rect dst_layer_roi = {0};
	struct mtk_rect dst_total_roi[MAX_CRTC] = {0};
	struct mtk_rect src_layer_roi = {0};
	struct mtk_rect src_total_roi[MAX_CRTC] = {0};
	bool rsz_enable[MAX_CRTC] = {false};
	struct mtk_plane_comp_state comp_state[MAX_CRTC][OVL_LAYER_NR];

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
		struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
		struct mtk_crtc_state *old_mtk_crtc_state = to_mtk_crtc_state(old_crtc_state);

		if(old_mtk_crtc_state->prop_val[CRTC_PROP_LYE_IDX] == state->prop_val[CRTC_PROP_LYE_IDX]
			&& !mtk_crtc->is_dual_pipe) {
			if ((old_mtk_crtc_state->prop_val[CRTC_PROP_LYE_IDX] == 0) &&
				((state->rsz_src_roi.width == 0) || (state->rsz_src_roi.height == 0)))
				DDPINFO("%s[%d] cala rsz roi for lye_idx is zero\n", __func__, __LINE__);
			else
				return;
		}
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, j) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		for (i = 0 ; i < mtk_crtc->layer_nr; i++) {
			struct drm_plane *plane = &mtk_crtc->planes[i].base;

			mtk_plane_get_comp_state(
				plane, &comp_state[drm_crtc_index(crtc)][i],
				crtc, 1);
		}
	}

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		struct drm_plane_state *plane_state = plane->state;
		int src_w = drm_rect_width(&plane_state->src) >> 16;
		int src_h = drm_rect_height(&plane_state->src) >> 16;
		int dst_x = plane_state->dst.x1;
		int dst_y = plane_state->dst.y1;
		int dst_w = drm_rect_width(&plane_state->dst);
		int dst_h = drm_rect_height(&plane_state->dst);
		int tmp_w = 0, tmp_h = 0;
		int idx;

		if (!plane_state->crtc)
			continue;

		if (i < OVL_LAYER_NR)
			idx = i;
		else if (i < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR))
			idx = i - OVL_LAYER_NR;
		else if (i < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR + SP_INPUT_LAYER_NR))
			idx = i - (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR);
		else
			idx = i - (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR + SP_INPUT_LAYER_NR);

		if (crtc && comp_state[drm_crtc_index(crtc)][idx].layer_caps
			& MTK_DISP_RSZ_LAYER) {
			if (dst_w != 0)
				tmp_w = ((dst_x * src_w * 10) / dst_w + 5) / 10;
			else
				tmp_w = 0;
			if (dst_h != 0)
				tmp_h = ((dst_y * src_h * 10) / dst_h + 5) / 10;
			else
				tmp_h = 0;
			mtk_rect_make(&src_layer_roi,
				tmp_w, tmp_h,
				src_w, src_h);
			mtk_rect_make(&dst_layer_roi,
				dst_x, dst_y,
				dst_w, dst_h);

			/* not enable resizer for the case that plane ROI is different from HRT
			 * (e.g. layer composed by GPU
			 */
			if (src_layer_roi.width == dst_layer_roi.width &&
				src_layer_roi.height == dst_layer_roi.height)
				continue;
			mtk_rect_join(&src_layer_roi,
				      &src_total_roi[drm_crtc_index(
					      plane_state->crtc)],
				      &src_total_roi[drm_crtc_index(
					      plane_state->crtc)]);
			mtk_rect_join(&dst_layer_roi,
				      &dst_total_roi[drm_crtc_index(
					      plane_state->crtc)],
				      &dst_total_roi[drm_crtc_index(
					      plane_state->crtc)]);
			rsz_enable[drm_crtc_index(plane_state->crtc)] = true;
		}
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		if (!rsz_enable[i] || mtk_crtc->fake_layer.fake_layer_mask) {
			struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;

			/* Non-hwc, direct access to display during shutdown charging may */
			/* result in the Hdisplay or vdisplay value being 0. */
			if ((priv->data->mmsys_id == MMSYS_MT6768 ||
				priv->data->mmsys_id == MMSYS_MT6765) &&
				(crtc->state->adjusted_mode.hdisplay == 0 ||
				crtc->state->adjusted_mode.vdisplay == 0)){
				struct drm_display_mode *timing = NULL;
				struct mtk_ddp_comp *comp;

				comp = priv->ddp_comp[DDP_COMPONENT_DSI0];
				mtk_ddp_comp_io_cmd(comp, NULL, DSI_GET_TIMING, &timing);
				crtc->state->adjusted_mode.hdisplay = timing->hdisplay;
				crtc->state->adjusted_mode.vdisplay = timing->vdisplay;
			}
			src_total_roi[i].x = 0;
			src_total_roi[i].y = 0;
			src_total_roi[i].width =
				crtc->state->adjusted_mode.hdisplay;
			src_total_roi[i].height =
					crtc->state->adjusted_mode.vdisplay;
			dst_total_roi[i].x = 0;
			dst_total_roi[i].y = 0;
			dst_total_roi[i].width =
				crtc->state->adjusted_mode.hdisplay;
			dst_total_roi[i].height =
				crtc->state->adjusted_mode.vdisplay;
		}
		state->rsz_src_roi = src_total_roi[i];
		state->rsz_dst_roi = dst_total_roi[i];
		if (mtk_crtc->is_dual_pipe && rsz_enable[i])
			mtk_atomic_rsz_calc_dual_params(crtc,
				&state->rsz_src_roi,
				&state->rsz_dst_roi,
				state->rsz_param);
		DDPINFO("[RPO] crtc[%d] (%d,%d,%d,%d)->(%d,%d,%d,%d)\n",
			drm_crtc_index(crtc), src_total_roi[i].x,
			src_total_roi[i].y, src_total_roi[i].width,
			src_total_roi[i].height, dst_total_roi[i].x,
			dst_total_roi[i].y, dst_total_roi[i].width,
			dst_total_roi[i].height);
	}
}

static void
mtk_atomic_calculate_plane_enabled_number(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	int i;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	unsigned int cnt[MAX_CRTC] = {0};

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		if (plane->state->crtc) {
			struct mtk_drm_crtc *mtk_crtc =
					to_mtk_crtc(plane->state->crtc);
			cnt[drm_crtc_index(&mtk_crtc->base)]++;
		}
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

		atomic_set(&state->plane_enabled_num, cnt[i]);
	}
}

static void mtk_atomic_check_plane_sec_state(struct drm_device *dev,
				     struct drm_atomic_state *new_state)
{
	int i;
	int sec_on[MAX_CRTC] = {0};
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	for_each_old_plane_in_state(new_state, plane, new_plane_state, i) {
		if (plane->state->crtc) {
			struct mtk_drm_crtc *mtk_crtc =
					to_mtk_crtc(plane->state->crtc);

			if (plane->state->fb
				&& plane->state->fb->format->format
					!= DRM_FORMAT_C8
				&& mtk_drm_fb_is_secure(plane->state->fb))
				sec_on[drm_crtc_index(&mtk_crtc->base)] = true;
		}
	}

	for_each_old_crtc_in_state(new_state, crtc, new_crtc_state, i) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		/* check output buffer is secure or not */
		if (mtk_crtc_check_out_sec(crtc))
			sec_on[drm_crtc_index(crtc)] = true;

		/* Leave secure sequence */
		if (mtk_crtc->sec_on && !sec_on[i])
			mtk_crtc_disable_secure_state(&mtk_crtc->base);

		/* When the engine switch to secure, we don't let system
		 * enter LP idle mode. Because it may make more secure risks
		 * with tiny power benefit.
		 */
		if (!mtk_crtc->sec_on && sec_on[i])
			mtk_drm_idlemgr_kick(__func__, crtc, false);

		mtk_crtc->sec_on = sec_on[i];
	}
}

static bool mtk_atomic_need_force_doze_switch(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_state;

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state->doze_changed ||
	    drm_atomic_crtc_needs_modeset(crtc->state))
		return false;

	DDPINFO("%s crtc%d, active:%d, doze_active:%llu\n", __func__,
		drm_crtc_index(crtc), crtc->state->active,
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
	return true;
}

static void mtk_atomic_force_doze_switch(struct drm_device *dev,
					 struct drm_atomic_state *old_state,
					 struct drm_connector *connector,
					 struct drm_crtc *crtc)
{
	const struct drm_encoder_helper_funcs *funcs;
	struct drm_encoder *encoder;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_funcs *panel_funcs;
	struct cmdq_pkt *handle;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
#endif

	/*
	 * If CRTC doze_active state change but the active state
	 * keep with the same value, the enable/disable cb of the
	 * encorder would not be executed. So we have to force
	 * run those cbs to change the doze directly.
	 */
	if (!mtk_atomic_need_force_doze_switch(crtc))
		return;

	DDPINFO("%s\n", __func__);

	encoder = connector->state->best_encoder;
	funcs = encoder->helper_private;

	panel_funcs = mtk_drm_get_lcm_ext_funcs(crtc);
	mtk_drm_idlemgr_kick(__func__, crtc, false);
	if (panel_funcs && panel_funcs->doze_get_mode_flags) {
		/* blocking flush before stop trigger loop */
		mtk_crtc_pkt_create(&handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (mtk_crtc_is_frame_trigger_mode(crtc))
			cmdq_pkt_wait_no_clear(handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		else
			cmdq_pkt_wait_no_clear(handle,
				mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
		cmdq_pkt_flush(handle);
		cmdq_pkt_destroy(handle);

#ifndef DRM_CMDQ_DISABLE
		cmdq_mbox_enable(client->chan); /* GCE clk refcnt + 1 */
		mtk_crtc_stop_trig_loop(crtc);
#endif

		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_sodi_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_event_loop(crtc);

		if (mtk_crtc_is_frame_trigger_mode(crtc)) {
			mtk_disp_mutex_disable(mtk_crtc->mutex[0]);
			mtk_disp_mutex_src_set(mtk_crtc, false);
			mtk_disp_mutex_enable(mtk_crtc->mutex[0]);
		}
	}
	/*
	 * No matter what the target crct power state it is,
	 * the encorder should be enabled for register controlling
	 * purpose.
	 */
	if (!funcs)
		return;
	if (funcs->enable)
		funcs->enable(encoder);
	else if (funcs->commit)
		funcs->commit(encoder);

	/*
	 * If the target crtc power state is POWER_OFF or
	 * DOZE_SUSPEND, call the encorder disable cb here.
	 */
	if (!crtc->state->active) {
		if (connector->state->crtc && funcs->prepare)
			funcs->prepare(encoder);
		else if (funcs->disable)
			funcs->disable(encoder);
		else if (funcs->dpms)
			funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
	}

	if (panel_funcs && panel_funcs->doze_get_mode_flags) {
		if (mtk_crtc_is_frame_trigger_mode(crtc)) {
			mtk_disp_mutex_disable(mtk_crtc->mutex[0]);
			mtk_disp_mutex_src_set(mtk_crtc, true);
		}

		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);

#ifndef DRM_CMDQ_DISABLE
		mtk_crtc_start_trig_loop(crtc);
		cmdq_mbox_disable(client->chan); /* GCE clk refcnt - 1 */
#endif

		mtk_crtc_hw_block_ready(crtc);
	}
}

static void mtk_atomic_aod_scp_ipi(struct drm_crtc *crtc, bool prepare)
{
	struct mtk_crtc_state *mtk_state;
	unsigned int ulps_wakeup_prd = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!aod_scp_flag || !aod_scp_ipi.send_ipi || !aod_scp_ipi.module_backup ||
		!crtc) {
		DDPDBG("%s directly return due to invalid parameter\n", __func__);
		return;
	}

	mtk_state = to_mtk_crtc_state(crtc->state);

	DDPDBG("%s: update AOD-SCP active=%d, doze state=%lld, prepare=%d, idle=%d\n", __func__,
			crtc->state->active,
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE],
			prepare,
			mtk_drm_is_idle(crtc));

	// switch to DDIC SPR on doze active state for AOD-SCP
	if (mtk_crtc->enabled && crtc->state->active && prepare &&
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
		if (mtk_crtc->spr_is_on == 1) {
			mtk_crtc->aod_scp_spr_switch = 1;
			mtk_drm_switch_spr(crtc, 0, 0);
		}
	}

	if (!crtc->state->active && prepare) {
		if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			ulps_wakeup_prd = mtk_drm_aod_scp_get_dsi_ulps_wakeup_prd(crtc);
			aod_scp_ipi.module_backup(crtc, ulps_wakeup_prd);
		}
		aod_scp_ipi.send_ipi(mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
	}
}

static void mtk_atomic_doze_update_dsi_state(struct drm_device *dev,
					 struct drm_crtc *crtc, bool prepare)
{
	struct mtk_crtc_state *mtk_state;
	struct mtk_drm_private *priv = dev->dev_private;

	mtk_state = to_mtk_crtc_state(crtc->state);
	DDPINFO("%s doze_changed:%d, needs_modeset:%d, doze_active:%llu\n",
		__func__, mtk_state->doze_changed,
		drm_atomic_crtc_needs_modeset(crtc->state),
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);

	mtk_atomic_aod_scp_ipi(crtc, prepare);

	if (mtk_state->doze_changed && priv->data->doze_ctrl_pmic) {
		if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] && prepare) {
			DDPDBG("enter AOD, disable PMIC LPMODE\n");
			//pmic_ldo_vio18_lp(SRCLKEN0, 0, 1, HW_LP);
			//pmic_ldo_vio18_lp(SRCLKEN2, 0, 1, HW_LP);
			/* DWFS, drivers/misc/mediatek/clkbuf/v1/src/mtk_clkbuf_ctl.c */
			//clk_buf_voter_ctrl_by_id(12, SW_BBLPM);
		} else if (!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]
				&& !prepare) {
			DDPDBG("exit AOD, enable PMIC LPMODE\n");
			//pmic_ldo_vio18_lp(SRCLKEN0, 1, 1, HW_LP);
			//pmic_ldo_vio18_lp(SRCLKEN2, 1, 1, HW_LP);
			/* DWFS, drivers/misc/mediatek/clkbuf/v1/src/mtk_clkbuf_ctl.c */
			//clk_buf_voter_ctrl_by_id(12, SW_OFF);
		}
	}
	if (!mtk_state->doze_changed ||
		!drm_atomic_crtc_needs_modeset(crtc->state))
		return;

	/* consider suspend->doze, doze_suspend->resume when doze prepare
	 * consider doze->suspend, resume->doze_supend when doze finish
	 */
	if ((crtc->state->active && prepare) ||
		(!crtc->state->active && !prepare))
		mtk_crtc_change_output_mode(crtc,
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
}
static void pq_bypass_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}
static void mtk_atomic_doze_update_pq(struct drm_crtc *crtc, unsigned int stage, bool old_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *mtk_state;
	struct mtk_ddp_comp *comp;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;
	int i, j;
	unsigned int bypass = 0;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
#endif

	DDPINFO("%s+: new crtc state = %d, old crtc state = %d, stage = %d\n", __func__,
		crtc->state->active, old_state, stage);
	mtk_state = to_mtk_crtc_state(crtc->state);

	if (!crtc->state->active) {
		if (mtk_state->doze_changed &&
			!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			DDPINFO("%s: doze switch to suspend, need enable pq first\n", __func__);
		} else {
			DDPINFO("%s: crtc is not active\n", __func__);
			return;
		}
	}

	if (mtk_state->doze_changed) {
		if (!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			if (!crtc->state->active && stage == 1)
				return;
			else if (crtc->state->active && stage == 0)
				return;
			bypass = 0;
		} else {
			if (!old_state && stage == 0)
				return;
			else if (old_state && stage == 1)
				return;
			bypass = 1;
		}
	} else {
		DDPINFO("%s: doze not change, skip update pq\n", __func__);
		return;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		return;
	}
	mtk_drm_idlemgr_kick(__func__, crtc, false);

#ifndef DRM_CMDQ_DISABLE
	if (bypass)
		cmdq_mbox_enable(client->chan); /* GCE clk refcnt + 1 */
#endif

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	if (mtk_crtc_is_frame_trigger_mode(crtc))
		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
	else
		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (comp->doze_bypass & DOZE_BYPASS_PQ)
			mtk_ddp_comp_bypass(comp, bypass, PQ_FEATURE_KRN_DOZE, cmdq_handle);
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			if(comp->doze_bypass & DOZE_BYPASS_PQ)
				mtk_ddp_comp_bypass(comp, bypass, PQ_FEATURE_KRN_DOZE, cmdq_handle);
		}
	}

	if (cmdq_pkt_flush_threaded(cmdq_handle, pq_bypass_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush user_cmd\n");

#ifndef DRM_CMDQ_DISABLE
	if (bypass) {
		cmdq_mbox_stop(client); /* Before power off need stop first */
		cmdq_mbox_disable(client->chan); /* GCE clk refcnt - 1 */
	}
#endif
}

static void mtk_atomic_doze_preparation(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;

	for_each_new_connector_in_state(old_state, connector,
		old_conn_state, i) {

		crtc = connector->state->crtc;
		if (!crtc) {
			DDPPR_ERR("%s connector has no crtc\n", __func__);
			continue;
		}

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && comp->id == DDP_COMPONENT_DSI0 && old_state->crtcs[0].old_state)
			mtk_atomic_doze_update_pq(crtc, 0,
				old_state->crtcs[0].old_state->active);

		if (comp && comp->id == DDP_COMPONENT_DSI1 && old_state->crtcs[3].old_state)
			mtk_atomic_doze_update_pq(crtc, 0,
				old_state->crtcs[3].old_state->active);

		mtk_atomic_doze_update_dsi_state(dev, crtc, 1);

		mtk_atomic_force_doze_switch(dev, old_state, connector, crtc);

	}
}

static void mtk_atomic_doze_finish(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;

	for_each_new_connector_in_state(old_state, connector,
		old_conn_state, i) {

		crtc = connector->state->crtc;
		if (!crtc) {
			DDPPR_ERR("%s connector has no crtc\n", __func__);
			continue;
		}

		mtk_atomic_doze_update_dsi_state(dev, crtc, 0);

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && comp->id == DDP_COMPONENT_DSI0 && old_state->crtcs[0].old_state)
			mtk_atomic_doze_update_pq(crtc, 1,
				old_state->crtcs[0].old_state->active);

		if (comp && comp->id == DDP_COMPONENT_DSI1 && old_state->crtcs[3].old_state)
			mtk_atomic_doze_update_pq(crtc, 1,
				old_state->crtcs[3].old_state->active);
	}
}

static bool mtk_drm_is_enable_from_lk(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = crtc ? to_mtk_crtc(crtc) : NULL;
	struct mtk_ddp_comp *comp = mtk_crtc ? mtk_ddp_comp_request_output(mtk_crtc) : NULL;
	unsigned int alias = 0;

	if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI) {
		alias = mtk_ddp_comp_get_alias(comp->id);

		if (mtk_disp_num_from_atag() & BIT(alias) ||
				(mtk_disp_num_from_atag() == 0 && drm_crtc_index(crtc) == 0))
			return true;
	}
	return false;
}

static bool mtk_atomic_skip_plane_update(struct mtk_drm_private *private,
					 struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	/* If the power state goes to or leave to doze mode, the display
	 * engine should not be self-refresh it again to avoid updating the
	 * unexpected content. The middleware is only change one CRTC
	 * power state at a time and there has no new frame updating
	 * when power state change. So here we can traverse all the
	 * CRTC state and skip the whole frame update. If the behavior
	 * above changed, the statement below should be modified.
	 */
	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *mtk_state =
			to_mtk_crtc_state(crtc->state);
		if (mtk_state->doze_changed ||
			(drm_atomic_crtc_needs_modeset(crtc->state) &&
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])) {
			DDPINFO("%s doze changed, skip self-update\n",
				__func__);
			return true;
		}
	}

	return false;
#ifdef IF_ZERO
	/* The CRTC would be enabled in LK stage and the content of
	 * corresponding display
	 *  may modified unexpected when first set crtc. If the case occur, we
	 * skip the plane
	 * update to make sure the display correctly.
	 */
	if (!state->legacy_set_config)
		return false;

	/* In general case, the set crtc stage control one crtc only. If the
	 * multiple CRTCs set
	 * at the same time, we have to make sure all the CRTC has been enabled
	 * from LK, then
	 * skip the plane update.
	 */
	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		if (!mtk_drm_is_enable_from_lk(crtc))
			return false;
	}

	return true;
#endif
}

bool mtk_drm_lcm_is_connect(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp = mtk_crtc ?
			mtk_ddp_comp_request_output(mtk_crtc) : NULL;
	struct mtk_panel_ext *panel_ext = NULL;

	if (!(comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI))
		return false;

	mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);

	if (IS_ERR_OR_NULL(panel_ext))
		return false;

	/* panel conntection state not check yet, adjust after connector enable */
	if (panel_ext->is_connected == -1)
		return true;

	return !!panel_ext->is_connected;

}

static void drm_atomic_esd_chk_first_enable(struct drm_device *dev,
				     struct drm_atomic_state *old_state)
{
	static bool is_first = true;
	int i;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;


	if (is_first) {
		for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
			struct mtk_drm_crtc *mtk_crtc = crtc ? to_mtk_crtc(crtc) : NULL;

			if (drm_crtc_index(crtc) == 0) {
				if  (mtk_drm_lcm_is_connect(mtk_crtc))
					mtk_disp_esd_check_switch(crtc, true);
				break;
			}
		}

		is_first = false;
	}
}

static void mtk_drm_enable_trig(struct drm_device *drm,
		struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector,
				new_conn_state, i) {
		if (!new_conn_state->best_encoder)
			continue;

		if (!new_conn_state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(new_conn_state->crtc->state))
			continue;

		mtk_crtc_hw_block_ready(new_conn_state->crtc);
	}
}

static struct mml_submit *mtk_alloc_mml_submit(void)
{
	struct mml_submit *temp = NULL;
	unsigned int i = 0;

	temp = kzalloc(sizeof(struct mml_submit), GFP_KERNEL);
	temp->job = kzalloc(sizeof(struct mml_job), GFP_KERNEL);
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp->pq_param[i] =	kzalloc(sizeof(struct mml_pq_param), GFP_KERNEL);

	return temp;
}

void mtk_free_mml_submit(struct mml_submit *temp)
{
	unsigned int i = 0;

	if (!temp)
		return;

	for (i = 0; i < MML_MAX_PLANES && i < temp->buffer.src.cnt; ++i) {
		if (temp->buffer.src.use_dma &&	temp->buffer.src.dmabuf[i]) {
			DDPINFO("%s dmabuf:0x%lx\n", __func__,
					(unsigned long)temp->buffer.src.dmabuf[i]);
			dma_buf_put(temp->buffer.src.dmabuf[i]);
		}
	}

	kfree(temp->job);
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		kfree(temp->pq_param[i]);
	kfree(temp);
}

int copy_mml_submit(struct mml_submit *src, struct mml_submit *dst)
{
	struct mml_job *temp_job = NULL;
	struct mml_pq_param *temp_pq_param[MML_MAX_OUTPUTS] = {NULL, NULL};
	int i = 0;

	temp_job = dst->job;
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp_pq_param[i] = dst->pq_param[i];

	memcpy(dst, src, sizeof(struct mml_submit));

	if (temp_job) {
		memmove(temp_job, dst->job, sizeof(struct mml_job));
		dst->job = temp_job;
	}

	for (i = 0; i < MML_MAX_OUTPUTS; ++i) {
		if (temp_pq_param[i]) {
			memcpy(temp_pq_param[i], dst->pq_param[i], sizeof(struct mml_pq_param));
			dst->pq_param[i] = temp_pq_param[i];
		}
	}

	return 0;
}

static int copy_mml_submit_from_user(struct mml_submit *src,
	struct mml_submit *dst)
{
	struct mml_job *temp_job = NULL;
	struct mml_pq_param *temp_pq_param[MML_MAX_OUTPUTS] = {NULL, NULL};
	int i = 0;

	temp_job = dst->job;
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp_pq_param[i] = dst->pq_param[i];

	if (copy_from_user(dst, src, sizeof(struct mml_submit))) {
		DDPINFO("%s copy_from_user all fail\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(temp_job, dst->job, sizeof(struct mml_job))) {
		DDPMSG("%s copy_from_user mml_job fail\n", __func__);
		return -EINVAL;
	}
	dst->job = temp_job;

	for (i = 0; i < MML_MAX_OUTPUTS; ++i) {
		if (copy_from_user(temp_pq_param[i],
			dst->pq_param[i], sizeof(struct mml_pq_param))) {
			DDPMSG("%s copy_from_user mml_pq_param fail\n", __func__);
			return -EINVAL;
		}
		dst->pq_param[i] = temp_pq_param[i];
	}

	return 0;
}

static void *fd_to_dma_buf(int32_t fd)
{
	struct dma_buf *dmabuf = NULL;

	if (fd <= 0)
		return NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		DDPMSG("%s fail to get dma_buf by fd %d err %ld",
			__func__, fd, PTR_ERR(dmabuf));
		return NULL;
	}
	return (void *)dmabuf;
}

static enum mml_mode _mtk_atomic_mml_plane(struct drm_device *dev,
	struct mtk_plane_state *mtk_plane_state)
{
	struct mml_submit *submit_kernel = NULL;
	struct mml_submit *submit_pq = NULL;
	struct mml_drm_ctx *mml_ctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_crtc_state *crtc_state = NULL;
	int i = 0, src_fd_0 = 0;
	int ret = 0;
	unsigned int fps = 0;
	unsigned int vtotal = 0;
	unsigned int line_time = 0;
	unsigned int tgt_comp = 0;
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_ddp_comp *output_comp = NULL;

	if (!mtk_plane_state->prop_val[PLANE_PROP_IS_MML])
		return MML_MODE_UNKNOWN;

	crtc = mtk_plane_state->crtc;
	mtk_crtc = to_mtk_crtc(crtc);
	crtc_state = to_mtk_crtc_state(crtc->state);
	fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
	vtotal = crtc->state->adjusted_mode.vtotal;
	priv = dev->dev_private;

	mml_ctx = mtk_drm_get_mml_drm_ctx(dev, crtc);
	if (unlikely(!mml_ctx))
		return MML_MODE_UNKNOWN;

	submit_pq = mtk_alloc_mml_submit();
	if (unlikely(!submit_pq)) {
		DDPMSG("%s:err_alloc_submit_pq\n", __func__);
		goto err_alloc_submit_pq;
	}

	submit_kernel = mtk_alloc_mml_submit();
	if (unlikely(!submit_kernel)) {
		DDPMSG("%s:err_alloc_submit_kernel\n", __func__);
		goto err_alloc_submit_kernel;
	}

	ret = copy_mml_submit_from_user(
	    (struct mml_submit *)(mtk_plane_state->prop_val[PLANE_PROP_MML_SUBMIT]), submit_kernel);
	if (unlikely(ret < 0)) {
		DDPMSG("%s:err_copy_submit\n", __func__);
		goto err_copy_submit;
	}

	if (submit_kernel->info.mode == MML_MODE_RACING && (!kref_read(&mtk_crtc->mml_ir_sram.ref)))
		mtk_crtc_alloc_sram(mtk_crtc, crtc_state->prop_val[CRTC_PROP_LYE_IDX]);

	for (i = 0; i < MML_MAX_PLANES; ++i) {
		submit_kernel->buffer.dest[0].fd[i] = -1;
		submit_kernel->buffer.dest[0].size[i] = 0;
	}

	src_fd_0 = submit_kernel->buffer.src.fd[0];
	if (src_fd_0 > 0) {
		submit_kernel->buffer.src.use_dma = true;
		submit_kernel->buffer.src.dmabuf[0] = fd_to_dma_buf(src_fd_0);
		if (submit_kernel->buffer.src.dmabuf[0] == NULL) {
			DDPMSG("%s:err_fd_to_dma\n", __func__);
			goto err_fd_to_dma;
		}
	} else {
		DDPMSG("%s:err_src_fd\n", __func__);
		goto err_src_fd;
	}

	if (submit_kernel->info.mode == MML_MODE_DIRECT_LINK) {
		mtk_addon_get_comp(crtc, crtc_state->lye_state.mml_dl_lye, &tgt_comp, NULL);
		comp = priv->ddp_comp[tgt_comp];
		ret = mtk_ddp_comp_io_cmd(comp, NULL, GET_OVL_SYS_NUM, NULL);
		DDPINFO("%s, %d tgt_comp:%d\n", __func__, __LINE__, tgt_comp);

		if (ret != -1)
			submit_kernel->info.ovlsys_id = ret;
		else
			DDPMSG("%s, %d GET_OVL_SYS_NUM fail\n", __func__, __LINE__);
	}

	ret = mtk_crtc->gce_obj.event[EVENT_MML_DISP_DONE_EVENT];
	if (ret)
		submit_kernel->info.disp_done_event = ret;

	mml_drm_split_info(submit_kernel, submit_pq);

	mtk_drm_idlemgr_kick(__func__, crtc, false); /* power on dsi */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && (mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI))
		mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_LINE_TIME_NS, &line_time);
	submit_kernel->info.act_time = line_time * submit_pq->info.dest[0].compose.height;

#define _ATOMIC_MML_FMT \
	"fps=%d vtotal=%d line_time=%d dst_h=%d act_time=%d dma=0x%lx src_fmt=%#010x plane=%d\n"
	DDPINFO(_ATOMIC_MML_FMT, fps, vtotal, line_time, submit_pq->info.dest[0].data.height,
		submit_kernel->info.act_time, (unsigned long)submit_kernel->buffer.src.dmabuf[0],
		submit_kernel->info.src.format,	submit_kernel->buffer.src.cnt);

	crtc_state->mml_dst_roi.x = mtk_plane_state->base.dst.x1;
	crtc_state->mml_dst_roi.y = mtk_plane_state->base.dst.y1;
	crtc_state->mml_dst_roi.width = submit_pq->info.dest[0].compose.width;
	crtc_state->mml_dst_roi.height = submit_pq->info.dest[0].compose.height;

	if (mtk_crtc->is_dual_pipe) {
		const int x = crtc_state->mml_dst_roi.x;
		const int y = crtc_state->mml_dst_roi.y;
		const int w = crtc_state->mml_dst_roi.width;
		const int h = crtc_state->mml_dst_roi.height;
		struct mtk_ddp_comp *output_comp = NULL;
		int panel_w = -1, mid_line = -1;
		unsigned int to_left = 0, to_right = 0;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp && drm_crtc_index(crtc) == 0)
			panel_w = mtk_ddp_comp_io_cmd(output_comp, NULL,
						      DSI_GET_VIRTUAL_WIDTH, NULL);
		mid_line = panel_w / 2;

		if (mtk_crtc->tile_overhead.is_support) {
			to_left = mtk_crtc->tile_overhead.left_overhead;
			to_right = mtk_crtc->tile_overhead.right_overhead;
			DDPINFO("%s: tile_overhead L:%d R:%d\n", __func__, to_left, to_right);
		}

		crtc_state->mml_dst_roi_dual[0] = crtc_state->mml_dst_roi;
		if ((x + w) > (mid_line + to_left))
			crtc_state->mml_dst_roi_dual[0].width = mid_line + to_left - x;
		else
			crtc_state->mml_dst_roi_dual[0].width += to_left;

		crtc_state->mml_dst_roi_dual[1].x = mid_line - to_right;
		crtc_state->mml_dst_roi_dual[1].y = y;
		crtc_state->mml_dst_roi_dual[1].height = h;
		crtc_state->mml_dst_roi_dual[1].width = to_right;
		if ((x + w) > mid_line)
			crtc_state->mml_dst_roi_dual[1].width += x + w - mid_line;

		memcpy(&submit_kernel->dl_out[0], &crtc_state->mml_dst_roi_dual[0],
		       sizeof(struct mml_rect));
		memcpy(&submit_kernel->dl_out[1], &crtc_state->mml_dst_roi_dual[1],
		       sizeof(struct mml_rect));
	} else {
		crtc_state->mml_dst_roi_dual[0] = crtc_state->mml_dst_roi;
		memcpy(&submit_kernel->dl_out[0], &crtc_state->mml_dst_roi_dual[0],
		       sizeof(struct mml_rect));
	}

	ret = mml_drm_submit(mml_ctx, submit_kernel, &(mtk_crtc->mml_cb));
	if (ret)
		goto err_submit;

	CRTC_MMP_MARK(0, mml_dbg, crtc_state->prop_val[CRTC_PROP_LYE_IDX], MMP_MML_SUBMIT);

	/* release previous mml_cfg */
	mtk_free_mml_submit(mtk_crtc->mml_cfg);
	mtk_free_mml_submit(mtk_crtc->mml_cfg_pq);

	mtk_crtc->mml_cfg = submit_kernel;
	mtk_crtc->mml_cfg_pq = submit_pq;

	return submit_kernel->info.mode;

err_submit:
err_copy_submit:
err_alloc_submit_kernel:
err_fd_to_dma:
err_src_fd:
	mtk_free_mml_submit(submit_kernel);
err_alloc_submit_pq:
	mtk_free_mml_submit(submit_pq);
	return MML_MODE_UNKNOWN;
}

static void mtk_atomic_mml(struct drm_device *dev,
	struct drm_atomic_state *state)
{
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_cs = NULL, *new_cs = NULL;
	struct mtk_crtc_state *mtk_crtc_state = NULL;
	struct mtk_crtc_state *old_mtk_state = NULL;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state, *old_plane_state;
	struct mtk_plane_state *mtk_plane_state;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int i = 0;
	enum mml_mode new_mode = MML_MODE_UNKNOWN;
	struct mtk_drm_private *priv = dev->dev_private;
	bool need_update_plane = false;
	int crtc_idx = 0;

	for_each_oldnew_crtc_in_state(state, crtc, old_cs, new_cs, i) {
		if (drm_crtc_index(crtc)== 0)
			break;
		else
			return;
	}

	if (new_cs && old_cs) {
		mtk_crtc_state = to_mtk_crtc_state(new_cs);
		old_mtk_state = to_mtk_crtc_state(old_cs);
	} else
		return;

	mtk_crtc = to_mtk_crtc(crtc);

	for_each_old_plane_in_state(state, plane, old_plane_state, i) {
		plane_state = plane->state;
		if (plane_state && plane_state->crtc && drm_crtc_index(plane_state->crtc) == 0) {
			need_update_plane = true;
			mtk_plane_state = to_mtk_plane_state(plane_state);
			mtk_plane_state->mml_mode = _mtk_atomic_mml_plane(dev, mtk_plane_state);
			if (mtk_plane_state->mml_mode > MML_MODE_UNKNOWN) {
				new_mode = mtk_plane_state->mml_mode;
				mtk_plane_state->mml_cfg = mtk_crtc->mml_cfg_pq;
			}
		}
	}

	/* return this function when CRTC's no plane to update and exist lye_state with mml lye config */
	/* which imply mml_dl_lye does not change in this atomic_commit */
	if (!need_update_plane && (mtk_crtc_state->lye_state.mml_ir_lye ||
			mtk_crtc_state->lye_state.mml_dl_lye)) {
		DDPINFO("no plane_update with mml lye, skip %s\n", __func__);
		return;
	}

	if (unlikely((new_mode == MML_MODE_UNKNOWN) &&
	    (mtk_crtc_state->lye_state.mml_ir_lye || mtk_crtc_state->lye_state.mml_dl_lye))) {
		mtk_crtc_state->lye_state.mml_ir_lye = 0;
		mtk_crtc_state->lye_state.mml_dl_lye = 0;
		DDPMSG("%s clear mml lye to avoid abnormal cases\n", __func__);
	}
	if (unlikely((mtk_crtc->mml_link_state == MML_IR_IDLE) &&
	    (old_mtk_state->lye_state.mml_ir_lye || old_mtk_state->lye_state.mml_dl_lye))) {
		old_mtk_state->lye_state.mml_ir_lye = 0;
		old_mtk_state->lye_state.mml_dl_lye = 0;
		DDPMSG("%s clear old mml lye to avoid abnormal idle resume\n", __func__);
	}

	mtk_crtc->is_mml = (new_mode == MML_MODE_RACING);
	mtk_crtc->is_mml_dl = (new_mode == MML_MODE_DIRECT_LINK);
	if (mtk_crtc->is_mml_dl && priv->data->ovl_exdma_rule) {
		mtk_crtc->skip_check_trigger = true;
		mtk_crtc_clr_set_dirty(mtk_crtc);
	}

	if (old_mtk_state->lye_state.mml_dl_lye && !mtk_crtc->is_mml_dl)
		mtk_crtc->mml_link_state = MML_STOP_LINKING;
	else if (old_mtk_state->lye_state.mml_ir_lye && !mtk_crtc->is_mml)
		mtk_crtc->mml_link_state = MML_STOP_LINKING;
	else if (!old_mtk_state->lye_state.mml_ir_lye && mtk_crtc->is_mml)
		mtk_crtc->mml_link_state = MML_IR_ENTERING;
	else if (mtk_crtc->is_mml && (mtk_crtc->mml_link_state == MML_IR_IDLE))
		mtk_crtc->mml_link_state = MML_IR_ENTERING;
	else if (mtk_crtc->is_mml || mtk_crtc->is_mml_dl)
		mtk_crtc->mml_link_state = MML_DIRECT_LINKING;
	else if (mtk_crtc->is_mml_dc && mtk_crtc->mml_cfg_dc == NULL &&
		mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_DECOUPLE_MODE))
		mtk_crtc->mml_link_state = MML_STOP_DC;
	else
		mtk_crtc->mml_link_state = NON_MML;

	if (mtk_crtc->mml_link_state == MML_STOP_DC)
		mtk_crtc->is_mml_dc = false;

	if (priv && priv->dpc_dev && !mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VIDLE_FULL_SCENARIO)) {
		if (mtk_crtc->mml_link_state == MML_DIRECT_LINKING &&
		    !mtk_vidle_is_ff_enabled()) {
			mtk_vidle_enable(true, priv);
			mtk_vidle_config_ff(true);
			if (mtk_vidle_is_ff_enabled())
				CRTC_MMP_MARK(crtc_idx, enter_vidle, 0xd1, new_mode);
		} else if ((mtk_crtc->mml_link_state == MML_STOP_LINKING ||
			   mtk_crtc->mml_link_state == MML_STOP_DC) &&
			   mtk_vidle_is_ff_enabled()) {
			mtk_vidle_config_ff(false);
			mtk_vidle_enable(false, priv);
			if (!mtk_vidle_is_ff_enabled())
				CRTC_MMP_MARK(crtc_idx, leave_vidle, 0xd1, new_mode);
		}
	}
}

static void mtk_set_first_config(struct drm_device *dev,
					struct drm_atomic_state *old_state)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		if (private->already_first_config == false &&
				connector->encoder && connector->encoder->crtc) {
			private->already_first_config = true;
			DDPMSG("%s, set first config true\n", __func__);
		}
	}

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		if (mtk_crtc->enabled && !new_crtc_state->active)
			mtk_crtc->enabled = false;
	}
}

static void mtk_atomic_complete(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	struct drm_device *drm = private->drm;
	struct drm_crtc *crtc0 = private->crtc[0];
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc0);
	bool cust_skip_frame = mtk_crtc->cust_skip_frame;

	mtk_atomic_wait_for_fences(state);

	/*
	 * Mediatek drm supports runtime PM, so plane registers cannot be
	 * written when their crtc is disabled.
	 *
	 * The comment for drm_atomic_helper_commit states:
	 *     For drivers supporting runtime PM the recommended sequence is
	 *
	 *     drm_atomic_helper_commit_modeset_disables(dev, state);
	 *     drm_atomic_helper_commit_modeset_enables(dev, state);
	 *     drm_atomic_helper_commit_planes(dev, state,
	 *                                     DRM_PLANE_COMMIT_ACTIVE_ONLY);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */
	/*
	 * To change the CRTC doze state, call the encorder enable/disable
	 * directly if the actvie state doesn't change.
	 */
	mtk_atomic_doze_preparation(drm, state);

	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_modeset_enables(drm, state);

	mtk_set_first_config(drm, state);

	mtk_drm_enable_trig(drm, state);

	mtk_atomic_disp_rsz_roi(drm, state);

	mtk_atomic_calculate_plane_enabled_number(drm, state);

	mtk_atomic_check_plane_sec_state(drm, state);

	mtk_atomic_mml(drm, state);

	if (!mtk_atomic_skip_plane_update(private, state) && !cust_skip_frame) {
		drm_atomic_helper_commit_planes(drm, state,
						DRM_PLANE_COMMIT_ACTIVE_ONLY);
		drm_atomic_esd_chk_first_enable(drm, state);
	} else if (cust_skip_frame) {
		pr_info("[%s] skip frame update\n", __func__);
#ifdef MTK_DRM_FENCE_SUPPORT
		release_fence_frame_skip(crtc0);
#endif
	}

	mtk_atomic_doze_finish(drm, state);

	if (!mtk_drm_helper_get_opt(private->helper_opt,
				    MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK))
		drm_atomic_helper_wait_for_vblanks(drm, state);

	drm_atomic_helper_cleanup_planes(drm, state);
	drm_atomic_state_put(state);
}

static void mtk_atomic_work(struct work_struct *work)
{
	struct mtk_drm_private *private =
		container_of(work, struct mtk_drm_private, commit.work);

	mtk_atomic_complete(private, private->commit.state);
}

static int mtk_atomic_check(struct drm_device *dev,
			    struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct mtk_crtc_state *old_state, *new_state;
	struct mtk_drm_private *priv = dev->dev_private;
	int i, ret = 0;

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		old_state = to_mtk_crtc_state(crtc->state);
		new_state = to_mtk_crtc_state(crtc_state);

		if (drm_crtc_index(crtc) == 0)
			mtk_drm_crtc_mode_check(crtc, crtc->state, crtc_state);

		if (new_state->prop_val[CRTC_PROP_LYE_IDX]) {
			struct mtk_drm_lyeblob_ids *ids, *next;
			struct list_head *lyeblob_head = NULL;
			struct drm_property_blob *blob = NULL;
			struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

			if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_SPHRT) == 0)
				lyeblob_head = &priv->lyeblob_head;
			else
				lyeblob_head = &mtk_crtc->lyeblob_head;

			mutex_lock(&priv->lyeblob_list_mutex);
			list_for_each_entry_safe(ids, next, lyeblob_head, list) {
				if (!ids->ddp_blob_id)
					continue;

				if (ids->lye_idx != new_state->prop_val[CRTC_PROP_LYE_IDX])
					continue;

				blob = drm_property_lookup_blob(crtc->dev, ids->ddp_blob_id);
				if (unlikely(!blob)) {
					DDPMSG("%s NULL blob\n", __func__);
					break;
				}
				new_state->lye_state =	*(struct mtk_lye_ddp_state *)blob->data;
				drm_property_blob_put(blob);
				break;
			}
			mutex_unlock(&priv->lyeblob_list_mutex);
		} else {
			new_state->lye_state.rpo_lye = 0;
			new_state->lye_state.mml_ir_lye = 0;
			new_state->lye_state.mml_dl_lye = 0;
		}

		if (new_state->prop_val[CRTC_PROP_BL_SYNC_GAMMA_GAIN]) {
			unsigned int gamma_gain[GAMMA_GAIN_MAX] = {0};

			ret = copy_from_user(gamma_gain,
			(unsigned int __user *)(new_state->prop_val[CRTC_PROP_BL_SYNC_GAMMA_GAIN]),
				sizeof(unsigned int) * GAMMA_GAIN_MAX);
			if (ret)
				DDPINFO("copy GAMMA_GAIN from user error, ret = %d, addr[%llx]\n",
					ret, new_state->prop_val[CRTC_PROP_BL_SYNC_GAMMA_GAIN]);

			DDPINFO("BL_SYNC_GAMMA_GAIN[%d][%d][%d], BL_SYNC_GAMMA_GAIN_RANGE[%d]\n",
				gamma_gain[GAMMA_GAIN_0], gamma_gain[GAMMA_GAIN_1],
				gamma_gain[GAMMA_GAIN_2], gamma_gain[GAMMA_GAIN_RANGE]);
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_0] = gamma_gain[GAMMA_GAIN_0];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_1] = gamma_gain[GAMMA_GAIN_1];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_2] = gamma_gain[GAMMA_GAIN_2];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_RANGE] = gamma_gain[GAMMA_GAIN_RANGE];
		} else {
			DDPINFO("BL_SYNC_GAMMA_GAIN, no update\n");
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_0] =
				old_state->bl_sync_gamma_gain[GAMMA_GAIN_0];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_1] =
				old_state->bl_sync_gamma_gain[GAMMA_GAIN_1];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_2] =
				old_state->bl_sync_gamma_gain[GAMMA_GAIN_2];
			new_state->bl_sync_gamma_gain[GAMMA_GAIN_RANGE] =
				old_state->bl_sync_gamma_gain[GAMMA_GAIN_RANGE];
		}

		if (old_state->prop_val[CRTC_PROP_DOZE_ACTIVE] ==
		    new_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
			continue;
		DDPINFO("[CRTC:%d:%s] doze active changed\n", crtc->base.id,
				crtc->name);
		new_state->doze_changed = true;
		ret = drm_atomic_add_affected_connectors(state, crtc);
		if (ret) {
			DDPINFO("DRM add conn failed! state:%p, ret:%d\n",
					state, ret);
			return ret;
		}
	}

	return ret;
}

static void mtk_atomic_check_res_scaling(struct mtk_drm_crtc *mtk_crtc,
	struct drm_display_mode *mode, int mode_idx)
{
	struct drm_display_mode *pmode = NULL;
	int i;

	if ((mtk_crtc == NULL) || (mode == NULL))
		return;

	if ((mode->hdisplay != mtk_crtc->scaling_ctx.lcm_width) ||
			(mode->vdisplay != mtk_crtc->scaling_ctx.lcm_height)) {
		mtk_crtc->scaling_ctx.scaling_en = true;
		/* adjusted_mode -> scaling_mode */
		if (mtk_crtc->scaling_ctx.cust_mode_mapping) {
			mtk_crtc->scaling_ctx.scaling_mode =
				mtk_drm_crtc_avail_disp_mode(&mtk_crtc->base,
					mtk_crtc->scaling_ctx.mode_mapping[mode_idx]);
			DDPMSG("%s mode_idx: %d->%d\n", __func__, mode_idx,
				mtk_crtc->scaling_ctx.mode_mapping[mode_idx]);
		} else if (mtk_crtc->avail_modes_num > 0) {
			for (i = 0; i < mtk_crtc->avail_modes_num; i++) {
				pmode = &mtk_crtc->avail_modes[i];
				if ((pmode->hdisplay == mtk_crtc->scaling_ctx.lcm_width)
					&& (pmode->vdisplay == mtk_crtc->scaling_ctx.lcm_height)
					&& (drm_mode_vrefresh(pmode) == drm_mode_vrefresh(mode))) {
					mtk_crtc->scaling_ctx.scaling_mode = pmode;
					break;
				}
			}
		}

		if (mtk_crtc->scaling_ctx.scaling_mode) {
			DDPINFO("%s++ scaling-up enable, resolution:(%ux%u)=>(%ux%u)\n",
				__func__, mode->hdisplay, mode->vdisplay,
				mtk_crtc->scaling_ctx.scaling_mode->hdisplay,
				mtk_crtc->scaling_ctx.scaling_mode->vdisplay);
		}
	} else {
		mtk_crtc->scaling_ctx.scaling_en = false;
		mtk_crtc->scaling_ctx.scaling_mode = mode;
		DDPINFO("%s++ scaling-up disable, resolution:(%ux%u)=>(%ux%u)\n",
			__func__, mode->hdisplay, mode->vdisplay,
			mtk_crtc->scaling_ctx.scaling_mode->hdisplay,
			mtk_crtc->scaling_ctx.scaling_mode->vdisplay);
	}
}

static void mtk_atomic_check_res_switch(struct mtk_drm_private *private,
				struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc_state *old_crtc_state;
	struct drm_display_mode *mode = NULL;
	struct drm_display_mode *old_mode = NULL;
	int j, o_i, o_j;
	struct mtk_ddp_config cfg = {0};
	struct mtk_ddp_config scaling_cfg = {0};
	struct mtk_ddp_comp *comp;
	int mode_idx;
	struct mtk_crtc_state *new_mtk_state = NULL;

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, j) {

		mtk_crtc = to_mtk_crtc(crtc);
		mode = &crtc->state->adjusted_mode;
		old_mode = &old_crtc_state->adjusted_mode;
		new_mtk_state = to_mtk_crtc_state(crtc->state);
		mode_idx = new_mtk_state->prop_val[CRTC_PROP_DISP_MODE_IDX];

		if ((drm_crtc_index(crtc) == 0)
			&& (mtk_crtc->res_switch != RES_SWITCH_NO_USE)
			&& mtk_crtc->mode_chg){

			if (mtk_crtc->res_switch == RES_SWITCH_ON_AP)
				mtk_atomic_check_res_scaling(mtk_crtc, mode, mode_idx);

			if ((mode->hdisplay == old_mode->hdisplay) &&
				(mode->vdisplay == old_mode->vdisplay))
				return;

			DDPMSG("%s++ resolution switch:%dx%d->%dx%d,fps:%d->%d\n", __func__,
				old_mode->hdisplay, old_mode->vdisplay,
				mode->hdisplay, mode->vdisplay,
				drm_mode_vrefresh(old_mode), drm_mode_vrefresh(mode));

			/* update tile overhead in advance */
			cfg.w = crtc->state->adjusted_mode.hdisplay;
			cfg.h = crtc->state->adjusted_mode.vdisplay;
			if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
				mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
				cfg.vrefresh =
					mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
			else
				cfg.vrefresh = drm_mode_vrefresh(&crtc->state->adjusted_mode);
			cfg.bpc = mtk_crtc->bpc;
			cfg.p_golden_setting_context = __get_golden_setting_context(mtk_crtc);

			cfg.rsz_src_w = cfg.w;
			cfg.rsz_src_h = cfg.h;

			if (mtk_crtc->scaling_ctx.scaling_en) {
				memcpy(&scaling_cfg, &cfg, sizeof(struct mtk_ddp_config));
				scaling_cfg.w = mtk_crtc->scaling_ctx.scaling_mode->hdisplay;
				scaling_cfg.h = mtk_crtc->scaling_ctx.scaling_mode->vdisplay;
				scaling_cfg.p_golden_setting_context =
					__get_scaling_golden_setting_context(mtk_crtc);
			}

			if (mtk_crtc->is_dual_pipe &&
				mtk_drm_helper_get_opt(private->helper_opt,
					MTK_DRM_OPT_TILE_OVERHEAD)) {
				cfg.tile_overhead.is_support = true;
				cfg.tile_overhead.left_in_width = cfg.w / 2;
				cfg.tile_overhead.right_in_width = cfg.w / 2;

				if (mtk_crtc->scaling_ctx.scaling_en) {
					scaling_cfg.tile_overhead.is_support = true;
					scaling_cfg.tile_overhead.left_in_width =
						scaling_cfg.w / 2;
					scaling_cfg.tile_overhead.right_in_width =
						scaling_cfg.w / 2;
				}

			} else {
				cfg.tile_overhead.is_support = false;
				cfg.tile_overhead.left_in_width = cfg.w;
				cfg.tile_overhead.right_in_width = cfg.w;
			}

			/*Calculate total overhead*/
			for_each_comp_in_crtc_path_reverse(comp, mtk_crtc, o_i, o_j) {

				if (mtk_crtc->scaling_ctx.scaling_en &&
						comp->in_scaling_path) {
					mtk_ddp_comp_config_overhead(comp, &scaling_cfg);
					DDPINFO("in scaling_path");
					DDPINFO("%s:comp %s width L:%d R:%d, overhead L:%d R:%d\n",
						__func__, mtk_dump_comp_str(comp),
						scaling_cfg.tile_overhead.left_in_width,
						scaling_cfg.tile_overhead.right_in_width,
						scaling_cfg.tile_overhead.left_overhead,
						scaling_cfg.tile_overhead.right_overhead);
				} else {
					mtk_ddp_comp_config_overhead(comp, &cfg);
					DDPINFO("%s:comp %s width L:%d R:%d, overhead L:%d R:%d\n",
						__func__, mtk_dump_comp_str(comp),
						cfg.tile_overhead.left_in_width,
						cfg.tile_overhead.right_in_width,
						cfg.tile_overhead.left_overhead,
						cfg.tile_overhead.right_overhead);
				}

				if (mtk_crtc->scaling_ctx.scaling_en
					&& mtk_crtc_check_is_scaling_comp(mtk_crtc, comp->id))
					cfg.tile_overhead = scaling_cfg.tile_overhead;
			}

			if (mtk_crtc->is_dual_pipe) {

				/*Calculate total overhead*/
				for_each_comp_in_dual_pipe_reverse(comp, mtk_crtc, o_i, o_j) {

					if (mtk_crtc->scaling_ctx.scaling_en &&
							comp->in_scaling_path) {
						mtk_ddp_comp_config_overhead(comp, &scaling_cfg);
						DDPINFO("%s, comp %s in scaling_path", __func__,
								mtk_dump_comp_str(comp));
						DDPINFO("width L:%d R:%d, ovhead L:%d R:%d\n",
							scaling_cfg.tile_overhead.left_in_width,
							scaling_cfg.tile_overhead.right_in_width,
							scaling_cfg.tile_overhead.left_overhead,
							scaling_cfg.tile_overhead.right_overhead);
					} else {
						mtk_ddp_comp_config_overhead(comp, &cfg);
						DDPINFO("%s:comp %s", __func__,
								mtk_dump_comp_str(comp));
						DDPINFO("width L:%d R:%d, ovhead L:%d R:%d\n",
							cfg.tile_overhead.left_in_width,
							cfg.tile_overhead.right_in_width,
							cfg.tile_overhead.left_overhead,
							cfg.tile_overhead.right_overhead);
					}

					if (mtk_crtc->scaling_ctx.scaling_en
						&& mtk_crtc_check_is_scaling_comp(mtk_crtc,
							comp->id))
						cfg.tile_overhead = scaling_cfg.tile_overhead;
				}
			}

			/*store total overhead data*/
			mtk_crtc_store_total_overhead(mtk_crtc, cfg.tile_overhead);
		}
	}
}

static int mtk_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *state, bool async)
{
	struct mtk_drm_private *private = drm->dev_private;
	uint32_t crtc_mask;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc0 = to_mtk_crtc(private->crtc[0]);
	int ret, i = 0, j = 0;
	unsigned int pf = 0;
	struct drm_crtc_state *new_crtc_state = NULL;
	struct mtk_crtc_state *mtk_crtc_state = NULL;

	DDP_PROFILE("[PROFILE] %s+\n", __func__);

	ret = drm_atomic_helper_prepare_planes(drm, state);
	if (ret)
		return ret;

	mtk_check_powermode(state, MTK_POWER_MODE_CHANGE);

	crtc_mask = mtk_atomic_crtc_mask(drm, state);

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, j) {
		/* check only crtc 0 for mml and update pf */
		if (!(crtc_mask & 0x1))
			break;

		mtk_crtc = to_mtk_crtc(crtc);
		mtk_crtc_state = to_mtk_crtc_state(new_crtc_state);
		pf = (unsigned int)mtk_crtc_state->prop_val[CRTC_PROP_PRES_FENCE_IDX];

		if (mtk_crtc_state->prop_val[CRTC_PROP_USER_SCEN] & USER_SCEN_SAME_POWER_MODE) {
			DDPMSG("skip atomic commit with same power mode\n");
			return 0;
		}

		if (mtk_crtc->is_mml) {
			/* if last frame is mml, need to wait job done before holding lock */
			ret = wait_event_interruptible(mtk_crtc->signal_mml_last_job_is_flushed_wq,
					atomic_read(&mtk_crtc->wait_mml_last_job_is_flushed));
			DDP_PROFILE("[PROFILE] pf:%u mml last job is flushed\n", pf);
		}
		break;
	}

	DDP_COMMIT_LOCK(&private->commit.lock, __func__, pf);
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
	mtk_crtc0->need_lock_tid = sys_gettid();
#else
	mtk_crtc0->need_lock_tid = current->pid;
#endif

	DRM_MMP_EVENT_START(mutex_lock, 0, 0);

	DDPINFO("%s step1,pm_status :%d\n", __func__,
		atomic_read(&private->kernel_pm.status));

	/* block atomic commit till kernel resume */
	ret = wait_event_interruptible(private->kernel_pm.wq,
			atomic_read(&private->kernel_pm.status) != KERNEL_PM_SUSPEND);
	DDPINFO("%s step2,pm_status :%d\n", __func__,
		atomic_read(&private->kernel_pm.status));
	if (unlikely(ret != 0))
		DDPMSG("%s kernel_pm wait queue woke up accidently\n", __func__);

	/* for shutdown case, crtc active state is not allowed */
	if (unlikely(atomic_read(&private->kernel_pm.status) == KERNEL_SHUTDOWN)) {
		for_each_new_crtc_in_state(state, crtc, new_crtc_state, j) {
			if (new_crtc_state->active == true) {
				DDPMSG("skip atomic commit after drm shutdown\n");
				goto cm_unlock;
			}
		}
	}

	flush_work(&private->commit.work);

	DDPINFO("%s step3,pm_status :%d\n", __func__,
		atomic_read(&private->kernel_pm.status));
	for (i = 0; i < MAX_CRTC; i++) {
		if (!(crtc_mask >> i & 0x1))
			continue;
		crtc = private->crtc[i];
		mtk_crtc = to_mtk_crtc(crtc);

		atomic_set(&(mtk_crtc->wait_mml_last_job_is_flushed), 0);

		DRM_MMP_MARK(mutex_lock, (unsigned long)&mtk_crtc->lock, i);

		DDP_MUTEX_LOCK_NESTED(&mtk_crtc->lock, i, __func__, __LINE__);
		CRTC_MMP_EVENT_START((int)drm_crtc_index(crtc), atomic_commit, 0, 0);
		drm_trace_tag_mark_bycrtc("atomic_commit", drm_crtc_index(crtc));
	}
	mutex_nested_time_start = sched_clock();
	DDPINFO("%s step4,pm_status :%d\n", __func__,
		atomic_read(&private->kernel_pm.status));
	ret = drm_atomic_helper_swap_state(state, 0);
	if (ret) {
		DDPPR_ERR("DRM swap state failed! state:%p, ret:%d\n",
				state, ret);
		goto mutex_unlock;
	}

	drm_atomic_state_get(state);
	mtk_atomic_check_res_switch(private, state);

#ifdef IF_ZERO /*TODO: use async atomic_commit would occur crtc_state and crtc race condition */
	if (async)
		mtk_atomic_schedule(private, state);
	else
		mtk_atomic_complete(private, state);
#endif
	mtk_atomic_complete(private, state);

	mutex_nested_time_end = sched_clock();
	mutex_nested_time_period =
			mutex_nested_time_end - mutex_nested_time_start;
	if (mutex_nested_time_period > 1000000000) {
		DDPPR_ERR("M_ULOCK_NESTED:%s[%d] timeout:<%lld ns>!\n",
			__func__, __LINE__, mutex_nested_time_period);
		DRM_MMP_MARK(mutex_lock, (unsigned long)mutex_time_period, 0);
		dump_stack();
	}

mutex_unlock:
	for (i = MAX_CRTC - 1; i >= 0; i--) {
		if (!(crtc_mask >> i & 0x1))
			continue;
		crtc = private->crtc[i];
		mtk_crtc = to_mtk_crtc(crtc);

		CRTC_MMP_EVENT_END((int)drm_crtc_index(crtc), atomic_commit, 0, 0);
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, i, __func__, __LINE__);
		DRM_MMP_MARK(mutex_lock, (unsigned long)&mtk_crtc->lock, i + (1 << 8));
	}


cm_unlock:
	DRM_MMP_EVENT_END(mutex_lock, 0, 0);
	mtk_crtc0->need_lock_tid = 0;
	DDP_COMMIT_UNLOCK(&private->commit.lock, __func__, pf);
	mtk_check_powermode(state, MTK_POWER_MODE_DONE);
	DDP_PROFILE("[PROFILE] %s-\n", __func__);

	return 0;
}

static struct drm_atomic_state *
mtk_drm_atomic_state_alloc(struct drm_device *dev)
{
	struct mtk_atomic_state *mtk_state;

	mtk_state = kzalloc(sizeof(*mtk_state), GFP_KERNEL);
	if (!mtk_state)
		return NULL;

	if (drm_atomic_state_init(dev, &mtk_state->base) < 0) {
		kfree(mtk_state);
		return NULL;
	}

	INIT_LIST_HEAD(&mtk_state->list);
	kref_init(&mtk_state->kref);

	return &mtk_state->base;
}

static const struct drm_mode_config_funcs mtk_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = mtk_atomic_check,
	.atomic_commit = mtk_atomic_commit,
	.atomic_state_alloc = mtk_drm_atomic_state_alloc,
	.atomic_state_free = mtk_atomic_state_put,
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0, DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_BLS,  DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_ext[] = {
	DDP_COMPONENT_RDMA1, DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,  DDP_COMPONENT_COLOR0, DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD,    DDP_COMPONENT_RDMA0,  DDP_COMPONENT_DPI0,
	DDP_COMPONENT_WDMA0, DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,  DDP_COMPONENT_COLOR1, DDP_COMPONENT_AAL1,
	DDP_COMPONENT_OD1,   DDP_COMPONENT_RDMA1,  DDP_COMPONENT_DPI1,
	DDP_COMPONENT_WDMA1, DDP_COMPONENT_PWM1,
};

static enum mtk_ddp_comp_id mt8173_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_COLOR0, DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD,   DDP_COMPONENT_RDMA0,  DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0, DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt8173_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,  DDP_COMPONENT_COLOR1, DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_RDMA1, DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,  DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,    DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_ext[] = {
// Todo: Just for create a simple external session
// and Can create crtc1 successfully use this path.
#ifdef IF_ZERO
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_WDMA0,
#else
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_RDMA0,
/*DDP_COMPONENT_DPI0,*/
#endif
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_WDMA_VIRTUAL0,
	DDP_COMPONENT_WDMA_VIRTUAL1, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA_VIRTUAL0, DDP_COMPONENT_WDMA_VIRTUAL1,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,    DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA_VIRTUAL0,
	DDP_COMPONENT_WDMA_VIRTUAL1, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6761_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6761_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6761_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6761_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	 DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6761_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6761_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6761_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6765_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6765_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6765_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6765_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	 DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6765_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6765_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6765_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_sec_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6768_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0, DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	 DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6768_addon_sec_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6768_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6768_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6877_mtk_ddp_main[] = {
#ifndef MTK_DRM_BRINGUP_STAGE
	DDP_COMPONENT_OVL0_2L,
#endif
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_SPR0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6877_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6877_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6877_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const struct mtk_addon_scenario_data mt6877_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6877_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6781_mtk_ddp_main[] = {
#ifndef MTK_DRM_BRINGUP_STAGE
	DDP_COMPONENT_OVL0_2L,
#endif
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6781_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6781_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};
#endif
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6781_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};
#endif
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6781_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif
static const struct mtk_addon_scenario_data mt6781_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6781_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,		DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA0_VIRTUAL0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,		DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_dual_main[] = {
	DDP_COMPONENT_OVL1_2L,		DDP_COMPONENT_OVL1,
	DDP_COMPONENT_OVL1_VIRTUAL0,	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA1_VIRTUAL0, DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,		DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1,	DDP_COMPONENT_DITHER1,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_WDMA0,
};


static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,		DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,	DDP_COMPONENT_WDMA0,
};


static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,		DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,		DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,		DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_RDMA4,
	DDP_COMPONENT_DP_INTF0,
// path to DSI1
//	DDP_COMPONENT_OVL1_2L,		DDP_COMPONENT_OVL1,
//	DDP_COMPONENT_OVL1_VIRTUAL0,	DDP_COMPONENT_RDMA1,
//	DDP_COMPONENT_RDMA1_VIRTUAL0, DDP_COMPONENT_DSI1
};

static const enum mtk_ddp_comp_id mt6885_dual_data_ext[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_RDMA5,
	/*DDP_COMPONENT_MERGE1,*/
	DDP_COMPONENT_DSC0,
};
static const enum mtk_ddp_comp_id mt6885_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L, /*DDP_COMPONENT_OVL1_2L,*/
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,       DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,      DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L, /*DDP_COMPONENT_OVL1_2L,*/
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,       DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_DSC0,
	DDP_COMPONENT_DLO_ASYNC1, DDP_COMPONENT_DLI_ASYNC5,
	DDP_COMPONENT_MAIN1_VIRTUAL,
	DDP_COMPONENT_DSI1,      DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6897_mtk_ovlsys_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
};

static const enum mtk_ddp_comp_id mt6897_mtk_ddp_main[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB3,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	 DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,	 DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,	 DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP0_OUT_CB3,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	//DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,	 DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6897_mtk_ovlsys_dual_main[] = {
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVL6_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
};

static const enum mtk_ddp_comp_id mt6897_mtk_ddp_dual_main[] = {
	DDP_COMPONENT_DLI_ASYNC6,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ1_OUT_CB3,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,	 DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,	 DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,	 DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,	 DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
	DDP_COMPONENT_SPR1,	 DDP_COMPONENT_ODDMR1,
	DDP_COMPONENT_DITHER3,	 DDP_COMPONENT_POSTALIGN1,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP1_OUT_CB3,
	DDP_COMPONENT_MERGE1_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC2,
	DDP_COMPONENT_DLI_ASYNC4,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,	 DDP_COMPONENT_CHIST3,
#endif
};

static const enum mtk_ddp_comp_id mt6897_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB4,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6897_mtk_ddp_dual_ext_dp[] = {
	DDP_COMPONENT_OVL6_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB2,
	DDP_COMPONENT_COMP1_OUT_CB4,
	DDP_COMPONENT_MERGE1_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC5,
	DDP_COMPONENT_MERGE0,
};

static const enum mtk_ddp_comp_id mt6897_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVL3_2L_VIRTUAL0,
	DDP_COMPONENT_OVLSYS_WDMA0,
};
static const enum mtk_ddp_comp_id mt6897_mtk_ddp_dual_third[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVL7_2L_VIRTUAL0,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB3,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,      DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,   DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP0_OUT_CB3,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_main_bringup_minor[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB3,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,      DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,   DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP0_OUT_CB3,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC1,
	DDP_COMPONENT_DLI_ASYNC11,
	DDP_COMPONENT_COMP1_OUT_CB6,
	DDP_COMPONENT_MERGE1_OUT_CB3,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

/* CRTC0 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
//	DDP_COMPONENT_OVL0_BG_CB0,
	DDP_COMPONENT_OVL1_2L,
//	DDP_COMPONENT_OVL0_BG_CB1,
	DDP_COMPONENT_OVL2_2L,
//	DDP_COMPONENT_OVL0_BLEND_CB0,
//	DDP_COMPONENT_DLO_RELAY,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC0,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#else
	DDP_COMPONENT_PQ0_OUT_CB3,
#endif
	DDP_COMPONENT_SPR0,		DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER0,	DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
	DDP_COMPONENT_DSC0,
	DDP_COMPONENT_COMP0_OUT_CB1,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ovlsys_dual_main_bringup[] = {
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVL6_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC6,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ1_OUT_CB3,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,    DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,      DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
	DDP_COMPONENT_SPR1,      DDP_COMPONENT_ODDMR1,
	DDP_COMPONENT_DITHER3,   DDP_COMPONENT_POSTALIGN1,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP1_OUT_CB3,
	DDP_COMPONENT_MERGE1_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC2,
	DDP_COMPONENT_DLI_ASYNC4,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,    DDP_COMPONENT_CHIST3,
#endif
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_main[] = {
	DDP_COMPONENT_OVL4_2L,
//	DDP_COMPONENT_OVL1_BG_CB0,
	DDP_COMPONENT_OVL5_2L,
//	DDP_COMPONENT_OVL1_BG_CB1,
	DDP_COMPONENT_OVL6_2L,
//	DDP_COMPONENT_OVL0_BLEND_CB0,
//	DDP_COMPONENT_DLO_RELAY,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
	DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_PQ1_IN_CB0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ1_OUT_CB0,
#else
	DDP_COMPONENT_PQ1_OUT_CB3,
#endif
	DDP_COMPONENT_SPR1,		DDP_COMPONENT_ODDMR1,
	DDP_COMPONENT_DITHER2,	DDP_COMPONENT_POSTALIGN1,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB0,
	DDP_COMPONENT_VDCM1,
	DDP_COMPONENT_COMP1_OUT_CB0,
	DDP_COMPONENT_MERGE1_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC2,
};

static const enum mtk_ddp_comp_id mt6985_scaling_main[] = {
	DDP_COMPONENT_RSZ0,
};

static const enum mtk_ddp_comp_id mt6985_scaling_main_dual[] = {
	DDP_COMPONENT_RSZ2,
};

static const enum mtk_ddp_comp_id mt6989_scaling_main[] = {
	DDP_COMPONENT_RSZ0,
};

/* CRTC0 */

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB4,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_ext_dp[] = {
	DDP_COMPONENT_OVL6_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB2,
	DDP_COMPONENT_COMP1_OUT_CB4,
	DDP_COMPONENT_MERGE1_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC5,
	DDP_COMPONENT_MERGE1,
};
/* CRTC1 */

/* CRTC2 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_mem_dp_w_tdshp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC13,
	DDP_COMPONENT_DLI_ASYNC3,
	DDP_COMPONENT_PQ0_IN_CB3,
	DDP_COMPONENT_PQ0_OUT_CB4,
	//DDP_COMPONENT_TDSHP1,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB4,
	DDP_COMPONENT_MERGE0_OUT_CB2,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVL5_2L_VIRTUAL0,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

/* CRTC2 */

/* CRTC3 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_secondary_dp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC11,
	DDP_COMPONENT_DLI_ASYNC7,
	DDP_COMPONENT_TDSHP3,
	DDP_COMPONENT_PQ1_OUT_CB2,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB3,
	DDP_COMPONENT_COMP1_OUT_CB5,
	DDP_COMPONENT_MERGE1_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_secondary_dp[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC0,
	DDP_COMPONENT_DLI_ASYNC10,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_discrete_chip[] = {
	DDP_COMPONENT_MDP_RDMA1,
	DDP_COMPONENT_Y2R1,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB3,
	DDP_COMPONENT_COMP1_OUT_CB5,
	DDP_COMPONENT_MERGE1_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_discrete_chip[] = {
	DDP_COMPONENT_MDP_RDMA0,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC1,
	DDP_COMPONENT_DLI_ASYNC11,
	DDP_COMPONENT_MERGE2,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ovlsys_main_full_set[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC8,
	DDP_COMPONENT_OVLSYS_DLI_ASYNC2,
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,      DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,   DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_DLO_ASYNC0,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_COMP0_IN_CB4,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6989_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_PQ0_OUT_CB5,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC11,
	DDP_COMPONENT_COMP0_IN_CB9,
	DDP_COMPONENT_COMP0_OUT_CB9,
	DDP_COMPONENT_MERGE0_OUT_CB5,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ddp_secondary[] = {
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC13,
	DDP_COMPONENT_DLI_ASYNC7,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB7,
#else
	DDP_COMPONENT_RSZ1,
	DDP_COMPONENT_TDSHP1,    DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,    DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ0_OUT_CB2,
#endif
	DDP_COMPONENT_PANEL0_COMP_OUT_CB4,
	DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC12,
	DDP_COMPONENT_COMP0_IN_CB8,
	DDP_COMPONENT_COMP0_OUT_CB8,
	DDP_COMPONENT_MERGE0_OUT_CB4,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ddp_discrete_chip[] = {
	DDP_COMPONENT_MDP_RDMA0,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_PQ0_OUT_CB7,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB4,
	DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC12,
	DDP_COMPONENT_COMP0_IN_CB8,
	DDP_COMPONENT_COMP0_OUT_CB8,
	DDP_COMPONENT_MERGE0_OUT_CB4,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6989_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVL4_2L_VIRTUAL0,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

static const enum mtk_ddp_comp_id mt6899_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
};

static const enum mtk_ddp_comp_id mt6899_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,	 DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,	 DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_DLO_ASYNC0,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_COMP0_IN_CB4,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,	 DDP_COMPONENT_CHIST1,
#endif

};
static const enum mtk_ddp_comp_id mt6899_mtk_ddp_main_bypass_pc_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#endif
	DDP_COMPONENT_DLO_ASYNC0,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_COMP0_IN_CB4,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,	 DDP_COMPONENT_CHIST1,
#endif

};

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6899_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_OUT_CB5,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC11,		// need check
	DDP_COMPONENT_COMP0_IN_CB9,
	DDP_COMPONENT_COMP0_OUT_CB9,
	DDP_COMPONENT_MERGE0_OUT_CB5,
	DDP_COMPONENT_DP_INTF0,
};

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static const enum mtk_ddp_comp_id mt6991_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL_EXDMA3,
	DDP_COMPONENT_OVL0_BLENDER1,
	DDP_COMPONENT_OVL_EXDMA4,
	DDP_COMPONENT_OVL0_BLENDER2,
	DDP_COMPONENT_OVL_EXDMA5,
	DDP_COMPONENT_OVL0_BLENDER3,
	DDP_COMPONENT_OVL_EXDMA6,
	DDP_COMPONENT_OVL0_BLENDER4,
	DDP_COMPONENT_OVL_EXDMA7,
	DDP_COMPONENT_OVL0_BLENDER5,
	DDP_COMPONENT_OVL_EXDMA2,
	DDP_COMPONENT_OVL0_EXDMA_OUT_CB3,
	DDP_COMPONENT_OVL0_BLENDER6,
	DDP_COMPONENT_OVL0_OUTPROC0,
	//DDP_COMPONENT_OVL0_OUTPROC_OUT_CB6,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_OVL_EXDMA0,
};
static const enum mtk_ddp_comp_id mt6991_mtk_ovlsys_main_bringup_dual[] = {
	DDP_COMPONENT_OVL_EXDMA3,
	DDP_COMPONENT_OVL0_BLENDER1,
	DDP_COMPONENT_OVL_EXDMA4,
	DDP_COMPONENT_OVL0_BLENDER2,
	DDP_COMPONENT_OVL_EXDMA5,
	DDP_COMPONENT_OVL0_BLENDER3,
	DDP_COMPONENT_OVL_EXDMA6,
	DDP_COMPONENT_OVL0_BLENDER4,
	DDP_COMPONENT_OVL_EXDMA7,
	DDP_COMPONENT_OVL0_BLENDER5,
	DDP_COMPONENT_OVL_EXDMA8,
	DDP_COMPONENT_OVL0_BLENDER6,
	DDP_COMPONENT_OVL_EXDMA9,
	DDP_COMPONENT_OVL0_BLENDER7,
	DDP_COMPONENT_OVL_EXDMA2,
	DDP_COMPONENT_OVL0_EXDMA_OUT_CB3,
	DDP_COMPONENT_OVL0_BLENDER8,
	DDP_COMPONENT_OVL0_OUTPROC0,
	//DDP_COMPONENT_OVL0_OUTPROC_OUT_CB6,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_OVL_EXDMA0,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB6,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC1, DDP_COMPONENT_DLI_ASYNC21,
	DDP_COMPONENT_SPLITTER0_IN_CB1,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#else
	DDP_COMPONENT_MDP_RSZ0,		DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_CCORR0,		DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_C3D0,		DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D1,		DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC0,	DDP_COMPONENT_DLI_ASYNC20,
	DDP_COMPONENT_ODDMR0,		DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#endif
	DDP_COMPONENT_COMP0_OUT_CB6,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_VDISP_AO,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,	DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER5,
	DDP_COMPONENT_OVL1_EXDMA7,
	DDP_COMPONENT_OVL1_BLENDER6,
	DDP_COMPONENT_OVL1_EXDMA8,
	DDP_COMPONENT_OVL1_BLENDER7,
	DDP_COMPONENT_OVL1_EXDMA9,
	DDP_COMPONENT_OVL1_BLENDER8,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER5,
	DDP_COMPONENT_OVL1_EXDMA7,
	DDP_COMPONENT_OVL1_BLENDER6,
	DDP_COMPONENT_OVL1_EXDMA8,
	DDP_COMPONENT_OVL1_BLENDER7,
	DDP_COMPONENT_OVL1_EXDMA9,
	DDP_COMPONENT_OVL1_BLENDER8,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC10,
	DDP_COMPONENT_DLI_ASYNC13,
	DDP_COMPONENT_PQ0_IN_CB13,
	DDP_COMPONENT_PQ0_OUT_CB7,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC22,
	DDP_COMPONENT_SPLITTER0_IN_CB2,
	DDP_COMPONENT_SPLITTER0_OUT_CB10,
	DDP_COMPONENT_COMP0_OUT_CB8,
	DDP_COMPONENT_MERGE0_OUT_CB7,
	DDP_COMPONENT_DP_INTF0,
};

/* CRTC3 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_secondary[] = {
	DDP_COMPONENT_OVL1_EXDMA8,
	DDP_COMPONENT_OVL1_BLENDER8,
	DDP_COMPONENT_OVL1_EXDMA9,
	DDP_COMPONENT_OVL1_BLENDER9,
	DDP_COMPONENT_OVL1_OUTPROC4,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC11,

	DDP_COMPONENT_DLI_ASYNC14,
	DDP_COMPONENT_PQ0_IN_CB14,
	DDP_COMPONENT_PQ0_OUT_CB11,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB7,
	DDP_COMPONENT_DLO_ASYNC6,

	DDP_COMPONENT_DLI_ASYNC26,
	DDP_COMPONENT_SPLITTER0_IN_CB6,
	DDP_COMPONENT_SPLITTER0_OUT_CB14,
	DDP_COMPONENT_COMP0_OUT_CB7,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DSI1,
};

/* CRTC3 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_discrete_chip[] = {
	DDP_COMPONENT_MDP_RDMA0,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_PQ0_IN_CB16,
	DDP_COMPONENT_PQ0_OUT_CB11,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB7,
	DDP_COMPONENT_DLO_ASYNC6,

	DDP_COMPONENT_DLI_ASYNC26,
	DDP_COMPONENT_SPLITTER0_IN_CB6,
	DDP_COMPONENT_SPLITTER0_OUT_CB14,
	DDP_COMPONENT_COMP0_OUT_CB7,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DSI1,
};
#else
static const enum mtk_ddp_comp_id mt6991_mtk_ovlsys_main_bringup[] = {
	DDP_COMPONENT_OVL_EXDMA3,
	DDP_COMPONENT_OVL0_BLENDER1,
	DDP_COMPONENT_OVL_EXDMA4,
	DDP_COMPONENT_OVL0_BLENDER2,
	DDP_COMPONENT_OVL_EXDMA5,
	DDP_COMPONENT_OVL0_BLENDER3,
	DDP_COMPONENT_OVL_EXDMA6,
	DDP_COMPONENT_OVL0_BLENDER4,
	DDP_COMPONENT_OVL_EXDMA7,
	DDP_COMPONENT_OVL0_BLENDER5,
	DDP_COMPONENT_OVL0_OUTPROC0,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_DLI_ASYNC0,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB6,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC1, DDP_COMPONENT_DLI_ASYNC21,
	DDP_COMPONENT_SPLITTER0_IN_CB1,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#else
	DDP_COMPONENT_MDP_RSZ0,		DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_CCORR0,		DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_C3D0,		DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D1,		DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
	DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC0,	DDP_COMPONENT_DLI_ASYNC20,
	DDP_COMPONENT_ODDMR0,		DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_SPLITTER0_OUT_CB9,
#endif
	DDP_COMPONENT_COMP0_OUT_CB6,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_VDISP_AO,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_CHIST0,	DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER4,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6991_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL1_EXDMA3,
	DDP_COMPONENT_OVL1_BLENDER1,
	DDP_COMPONENT_OVL1_EXDMA4,
	DDP_COMPONENT_OVL1_BLENDER2,
	DDP_COMPONENT_OVL1_EXDMA5,
	DDP_COMPONENT_OVL1_BLENDER3,
	DDP_COMPONENT_OVL1_EXDMA6,
	DDP_COMPONENT_OVL1_BLENDER4,
	DDP_COMPONENT_OVL1_OUTPROC0,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ0_IN_CB8,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB7,
#else
	DDP_COMPONENT_MDP_RSZ1,		DDP_COMPONENT_TDSHP1,
	DDP_COMPONENT_CCORR2,		DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_C3D2,		DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D3,		DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,		DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1,	DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_PQ0_OUT_CB3,
#endif
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_DLO_ASYNC2,
	DDP_COMPONENT_DLI_ASYNC22,
	DDP_COMPONENT_SPLITTER0_IN_CB2,
	DDP_COMPONENT_SPLITTER0_OUT_CB10,
	DDP_COMPONENT_COMP0_OUT_CB7,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_secondary[] = {
	DDP_COMPONENT_OVL_EXDMA9,
	DDP_COMPONENT_OVL0_BLENDER7,
	DDP_COMPONENT_OVL0_OUTPROC2,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC7,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_IN_CB2,
	DDP_COMPONENT_PQ0_OUT_CB8,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC23,
	DDP_COMPONENT_SPLITTER0_IN_CB3,
	DDP_COMPONENT_SPLITTER0_OUT_CB11,
	DDP_COMPONENT_COMP0_OUT_CB8,
	DDP_COMPONENT_MERGE0_OUT_CB2,
	DDP_COMPONENT_DISP_DVO,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_discrete_chip[] = {

};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_fifth_path[] = {
	DDP_COMPONENT_OVL1_EXDMA8,
	DDP_COMPONENT_OVL1_BLENDER6,
	DDP_COMPONENT_OVL1_OUTPROC1,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC6,
	DDP_COMPONENT_DLI_ASYNC9,
	DDP_COMPONENT_PQ0_IN_CB9,
	DDP_COMPONENT_PQ0_OUT_CB9,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB4,
	DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC24,
	DDP_COMPONENT_SPLITTER0_IN_CB4,
	DDP_COMPONENT_SPLITTER0_OUT_CB12,
#ifdef MTK_DSI1_SUPPORT_DSC1
	DDP_COMPONENT_DSC1,
#else
	DDP_COMPONENT_COMP0_OUT_CB9,
#endif
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_sixth_path[] = {
	DDP_COMPONENT_OVL1_EXDMA9,
	DDP_COMPONENT_OVL1_BLENDER7,
	DDP_COMPONENT_OVL1_OUTPROC2,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC7,
	DDP_COMPONENT_DLI_ASYNC10,
	DDP_COMPONENT_PQ0_IN_CB10,
	DDP_COMPONENT_PQ0_OUT_CB10,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB5,
	DDP_COMPONENT_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC25,
	DDP_COMPONENT_SPLITTER0_IN_CB5,
	DDP_COMPONENT_SPLITTER0_OUT_CB13,
	DDP_COMPONENT_COMP0_OUT_CB10,
	DDP_COMPONENT_MERGE0_OUT_CB4,
	DDP_COMPONENT_DSI2,
};

static const enum mtk_ddp_comp_id mt6991_mtk_ddp_seventh_path[] = {
	DDP_COMPONENT_OVL1_EXDMA7,
	DDP_COMPONENT_OVL1_BLENDER5,
	DDP_COMPONENT_OVL1_OUTPROC3,
	DDP_COMPONENT_OVLSYS1_DLO_ASYNC8,
	DDP_COMPONENT_DLI_ASYNC11,
	DDP_COMPONENT_PQ0_IN_CB11,
	DDP_COMPONENT_PQ0_OUT_CB11,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB6,
	DDP_COMPONENT_DLO_ASYNC6,
	DDP_COMPONENT_DLI_ASYNC26,
	DDP_COMPONENT_SPLITTER0_IN_CB6,
	DDP_COMPONENT_SPLITTER0_OUT_CB14,
	DDP_COMPONENT_COMP0_OUT_CB11,
	DDP_COMPONENT_MERGE0_OUT_CB5,
	DDP_COMPONENT_DP_INTF1,
};
#endif

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_dual_main[] = {
	/* Can't enable dual pipe with bypass PQ */
	DDP_COMPONENT_OVL2_2L, /*DDP_COMPONENT_OVL3_2L,*/
	DDP_COMPONENT_OVL1, DDP_COMPONENT_OVL1_VIRTUAL0,
	DDP_COMPONENT_OVL1_VIRTUAL1, DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_TDSHP1,	 DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,	 DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,	 DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,	 DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_CM1,		 DDP_COMPONENT_SPR1,
	DDP_COMPONENT_PQ1_VIRTUAL, DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC0,

	DDP_COMPONENT_PWM1, /* This PWM is for connect CHIST */
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,	 DDP_COMPONENT_CHIST3,
#endif
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_main_wb_path[] = {
	// todo ..
	//DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	//DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL2_2L_NWCG,
	DDP_COMPONENT_OVL2_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA3,
	DDP_COMPONENT_SUB1_VIRTUAL1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_ext_dsi[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_MERGE1,
	DDP_COMPONENT_MAIN1_VIRTUAL,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6983_dual_data_ext[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_MERGE1,
	/*DDP_COMPONENT_DSC1,*/
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L_NWCG, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	 DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,		 DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,	 DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,	 DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_dual_main[] = {
	/* Can't enable dual pipe with bypass PQ */
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVL1, DDP_COMPONENT_OVL1_VIRTUAL0,
	DDP_COMPONENT_OVL1_VIRTUAL1, DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_TDSHP1,	 DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,	 DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,  DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,  DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_CM1,		 DDP_COMPONENT_SPR1,
	DDP_COMPONENT_PQ1_VIRTUAL, DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC0,

	DDP_COMPONENT_PWM1, /* This PWM is for connect CHIST */
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,	 DDP_COMPONENT_CHIST3,
#endif
};

/* mt6886 is the same as mt6895 */
static const enum mtk_ddp_comp_id mt6895_mtk_ddp_main_wb_path[] = {
	// todo ..
	//DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	//DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_OVL2_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP1_PQ0_VIRTUAL,
	DDP_COMPONENT_OVL2_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA3,
	DDP_COMPONENT_SUB1_VIRTUAL1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6895_dual_data_ext[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	/*DDP_COMPONENT_MERGE1,*/
	DDP_COMPONENT_DSC1,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6886_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6886_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0_VIRTUAL,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,		 DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,	 DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,
#endif
};

static const struct mtk_addon_module_data addon_ovl_rsz_data[] = {
	{OVL_RSZ, ADDON_EMBED, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_ovl_rsz_data_1[] = {
	{OVL_RSZ_1, ADDON_EMBED, DDP_COMPONENT_OVL4_2L},
};

/* it's aim for mt6989 OVLSYS1 RSZ */
static const struct mtk_addon_module_data addon_ovl_rsz_data_2[] = {
	{OVL_RSZ_1, ADDON_EMBED, DDP_COMPONENT_OVL3_2L},
};

static const struct mtk_addon_module_data mt6983_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0},
};

static const struct mtk_addon_module_data mt6983_addon_wdma0_data_v2[] = {
	{DISP_WDMA0_v2, ADDON_AFTER, DDP_COMPONENT_OVL0_VIRTUAL0},
};

static const struct mtk_addon_module_data mt6983_addon_wdma2_data[] = {
	{DISP_WDMA2, ADDON_AFTER, DDP_COMPONENT_SPR1},
};

static const struct mtk_addon_module_data mt6983_addon_wdma2_data_v2[] = {
	{DISP_WDMA2_v2, ADDON_AFTER, DDP_COMPONENT_OVL1_VIRTUAL0},
};

static const struct mtk_addon_module_data mt6985_addon_wdma0_data[] = {
	/* real case: need wait PQ */
	/* {DISP_WDMA0_v3, ADDON_AFTER, DDP_COMPONENT_PQ0_OUT_CB0}, */
	{DISP_WDMA0_v3, ADDON_AFTER, DDP_COMPONENT_PANEL0_COMP_OUT_CB0},
};

static const struct mtk_addon_module_data mt6985_addon_wdma1_data[] = {
	{DISP_WDMA1, ADDON_AFTER, DDP_COMPONENT_MERGE1_OUT_CB0},
};

static const struct mtk_addon_module_data mt6989_addon_wdma0_data[] = {
	/* Leroy CWB */
	{DISP_WDMA0_v5, ADDON_AFTER, DDP_COMPONENT_DLI_ASYNC8},
};

static const struct mtk_addon_module_data mt6991_addon_wdma0_data[] = {
	/* Liber CWB */
	{DISP_WDMA0_v6, ADDON_AFTER, DDP_COMPONENT_SPLITTER0_OUT_CB9},
};

static const struct mtk_addon_module_data mt6991_addon_mid_wdma_data[] = {
	{DISP_WDMA_MID, ADDON_AFTER, DDP_COMPONENT_PQ0_OUT_CB0},
};

static const struct mtk_addon_module_data mt6989_addon_ovl_ufbc_wdma0_data[] = {
	/* Leroy IWB */
	{DISP_OVLSYS_UFBC_WDMA0, ADDON_AFTER, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6897_addon_wdma0_data[] = {
	{DISP_WDMA0_v4, ADDON_AFTER, DDP_COMPONENT_PANEL0_COMP_OUT_CB0},
};

static const struct mtk_addon_module_data mt6985_addon_ovlsys_wdma0_data[] = {
	{DISP_OVLSYS_WDMA0, ADDON_AFTER, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6989_addon_ovlsys_wdma0_data[] = {
	{DISP_OVLSYS_WDMA0_v2, ADDON_AFTER, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6991_addon_ovlsys_wdma0_data[] = {
	{DISP_OVLSYS_WDMA0_v3, ADDON_AFTER, DDP_COMPONENT_OVL0_OUTPROC0},
};

static const struct mtk_addon_module_data mt6985_addon_ovlsys_wdma2_data[] = {
	{DISP_OVLSYS_WDMA2, ADDON_AFTER, DDP_COMPONENT_OVL6_2L},
};

/* mt6886 is the same as mt6895 */
static const struct mtk_addon_module_data mt6895_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0},
};

static const struct mtk_addon_module_data mt6895_addon_wdma2_data[] = {
	{DISP_WDMA2, ADDON_AFTER, DDP_COMPONENT_SPR1},
};

static const struct mtk_addon_module_data mt6855_addon_rsz_data[] = {
	{DISP_RSZ, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};
static const struct mtk_addon_module_data mt6855_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0_VIRTUAL},
};

static const struct mtk_addon_module_data addon_mml_sram_only_data[] = {
	{DISP_MML_SRAM_ONLY, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6886_addon_mml_sram_only_data[] = {
	{DISP_MML_SRAM_ONLY, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};


static const struct mtk_addon_module_data mt6895_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data mt6886_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ_v3, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_rsz_data_1[] = {
	{DISP_MML_IR_PQ_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ_v2, ADDON_EMBED, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_rsz_data_1[] = {
	{DISP_MML_IR_PQ_v2_1, ADDON_EMBED, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_dl_data[] = {
	{DISP_MML_DL, ADDON_BEFORE, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_dl_data_1[] = {
	{DISP_MML_DL_1, ADDON_BEFORE, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data mt6991_addon_mml_dl_data[] = {
	{DISP_MML_DL_EXDMA, ADDON_BEFORE, DDP_COMPONENT_OVL_EXDMA0/*DDP_COMPONENT_OVL0_EXDMA_OUT_CB0*/},
};

static const struct mtk_addon_module_data addon_dsc0_data[] = {
	{DSC_0, ADDON_AFTER, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_dsc1_data[] = {
	{DSC_1, ADDON_AFTER, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_scenario_data mt6779_addon_main[ADDON_SCN_NR] = {
		[NONE] = {

				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {

				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {

				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6779_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6885_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(addon_wdma0_data),
				.module_data = addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6885_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v3),
				.module_data = addon_rsz_data_v3,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v3),
				.module_data = addon_rsz_data_v3,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(addon_wdma1_data),
				.module_data = addon_wdma1_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};


static const struct mtk_addon_scenario_data mt6885_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data),
				.module_data = mt6983_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK_OVL] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data_v2),
				.module_data = mt6983_addon_wdma0_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_rsz_data),
				.module_data = mt6983_addon_mml_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
				.module_data = addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};

static const struct mtk_addon_scenario_data mt6983_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v4),
				.module_data = addon_rsz_data_v4,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v4),
				.module_data = addon_rsz_data_v4,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma2_data),
				.module_data = mt6983_addon_wdma2_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK_OVL] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma2_data_v2),
				.module_data = mt6983_addon_wdma2_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_rsz_data_1),
				.module_data = mt6983_addon_mml_rsz_data_1,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_sram_only_data_1),
				.module_data = mt6983_addon_mml_sram_only_data_1,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};


static const struct mtk_addon_scenario_data mt6983_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6985_addon_wdma0_data),
		.module_data = mt6985_addon_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma0_data),
		.module_data = mt6985_addon_ovlsys_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data),
		.module_data = addon_ovl_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data),
		.module_data = mt6985_addon_mml_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
		.module_data = addon_mml_sram_only_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data),
		.module_data = mt6985_addon_mml_dl_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_main_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6985_addon_wdma1_data),
		.module_data = mt6985_addon_wdma1_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma2_data),
		.module_data = mt6985_addon_ovlsys_wdma2_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data_1),
		.module_data = addon_ovl_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data_1),
		.module_data = mt6985_addon_mml_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_sram_only_data_1),
		.module_data = mt6985_addon_mml_sram_only_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data_1),
		.module_data = mt6985_addon_mml_dl_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_dual_data_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6989_addon_wdma0_data),
		.module_data = mt6989_addon_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6989_addon_ovlsys_wdma0_data),
		.module_data = mt6989_addon_ovlsys_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data),
		.module_data = addon_ovl_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data),
		.module_data = mt6985_addon_mml_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
		.module_data = addon_mml_sram_only_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data),
		.module_data = mt6985_addon_mml_dl_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[DSC_COMP] = {
		.module_num = ARRAY_SIZE(addon_dsc0_data),
		.module_data = addon_dsc0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[IDLE_WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6989_addon_ovl_ufbc_wdma0_data),
		.module_data = mt6989_addon_ovl_ufbc_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};


static const struct mtk_addon_scenario_data mt6991_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_main_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data_2),
		.module_data = addon_ovl_rsz_data_2,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_secondary_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_discrete_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_module_data mt6991_addon_ovl_rsz_data[] = {
	{OVL_RSZ_2, ADDON_BEFORE, DDP_COMPONENT_OVL0_BLENDER1},
	//{OVL_RSZ_3, ADDON_BEFORE, DDP_COMPONENT_OVL1_BLENDER9},
};

static const struct mtk_addon_scenario_data mt6991_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6991_addon_mml_dl_data),
		.module_data = mt6991_addon_mml_dl_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(mt6991_addon_ovl_rsz_data),
		.module_data = mt6991_addon_ovl_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6991_addon_wdma0_data),
		.module_data = mt6991_addon_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_MID] = {
		.module_num = ARRAY_SIZE(mt6991_addon_mid_wdma_data),
		.module_data = mt6991_addon_mid_wdma_data,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6991_addon_ovlsys_wdma0_data),
		.module_data = mt6991_addon_ovlsys_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const enum mtk_ddp_comp_id mt6991_scaling_main[] = {
	DDP_COMPONENT_MDP_RSZ0,
};

static const struct mtk_addon_scenario_data mt6897_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6897_addon_wdma0_data),
		.module_data = mt6897_addon_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma0_data),
		.module_data = mt6985_addon_ovlsys_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data),
		.module_data = addon_ovl_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data),
		.module_data = mt6985_addon_mml_rsz_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
		.module_data = addon_mml_sram_only_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data),
		.module_data = mt6985_addon_mml_dl_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6897_addon_main_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6985_addon_wdma1_data),
		.module_data = mt6985_addon_wdma1_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma2_data),
		.module_data = mt6985_addon_ovlsys_wdma2_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data_1),
		.module_data = addon_ovl_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data_1),
		.module_data = mt6985_addon_mml_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_sram_only_data_1),
		.module_data = mt6985_addon_mml_sram_only_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data_1),
		.module_data = mt6985_addon_mml_dl_data_1,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6897_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6897_dual_data_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_dp_w_tdshp[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_dp_wo_tdshp[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6989_addon_dp_wo_tdshp[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_secondary_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_secondary_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_discrete_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_discrete_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_secondary_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_secondary_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_discrete_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_discrete_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6895_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma0_data),
				.module_data = mt6895_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
				.module_data = addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};


static const struct mtk_addon_scenario_data mt6895_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v6),
				.module_data = addon_rsz_data_v6,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v6),
				.module_data = addon_rsz_data_v6,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma2_data),
				.module_data = mt6895_addon_wdma2_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6895_addon_mml_sram_only_data_1),
				.module_data = mt6895_addon_mml_sram_only_data_1,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};


static const struct mtk_addon_scenario_data mt6895_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6886_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6886_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma0_data),
				.module_data = mt6895_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6886_addon_mml_sram_only_data),
				.module_data = mt6886_addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6886_addon_mml_rsz_data),
				.module_data = mt6886_addon_mml_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_ext[] = {
#ifdef CONFIG_DRM_MEDIATEK_HDMI
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_RDMA4,
	DDP_COMPONENT_DPI0,
#else
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_RDMA4,
#endif
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};

static const struct mtk_addon_scenario_data mt6873_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6873_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_SPR0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6853_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};
#endif
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif
static const struct mtk_addon_scenario_data mt6853_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6853_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6833_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};
#endif
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6833_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6833_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL,
	DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0, DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_CM0, DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,
#endif
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_ext[] = {
	//todo: ovl0_2l_nwcg -> dp
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,	DDP_COMPONENT_WDMA1,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0, DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_CM0, DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6879_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
			.module_num = 0,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
			.module_num = ARRAY_SIZE(addon_rsz_data),
			.module_data = addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[TWO_SCALING] = {
			.module_num = ARRAY_SIZE(addon_rsz_data),
			.module_data = addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
			.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data),
			.module_data = mt6983_addon_wdma0_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6879_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL,
	DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL, DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_SPR0_VIRTUAL,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6855_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
			.module_num = 0,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
			.module_num = ARRAY_SIZE(mt6855_addon_rsz_data),
			.module_data = mt6855_addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[TWO_SCALING] = {
			.module_num = ARRAY_SIZE(mt6855_addon_rsz_data),
			.module_data = mt6855_addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
			.module_num = ARRAY_SIZE(mt6855_addon_wdma0_data),
			.module_data = mt6855_addon_wdma0_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6855_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_crtc_path_data mt6779_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6779_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6779_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6779_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6779_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6779_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6779_addon_main,
};

static const struct mtk_crtc_path_data mt6779_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6779_addon_ext,
};

static const struct mtk_crtc_path_data mt6779_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_third),
	.addon_data = mt6779_addon_ext,
};

static const struct mtk_crtc_path_data mt6761_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6761_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6761_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6761_addon_main,
};

static const struct mtk_crtc_path_data mt6761_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6761_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6761_mtk_ddp_third),
	.addon_data = mt6761_addon_ext,
};

static const struct mtk_crtc_path_data mt6765_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6765_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6765_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6765_addon_main,
};

static const struct mtk_crtc_path_data mt6765_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6765_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6765_mtk_ddp_third),
	.addon_data = mt6765_addon_ext,
};

static struct mtk_crtc_path_data mt6768_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6768_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6768_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6768_addon_main,
};

static const struct mtk_crtc_path_data mt6768_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6768_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6768_mtk_ddp_third),
	.addon_data = mt6768_addon_ext,
};

static const enum mtk_ddp_comp_id mt6768_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_RDMA0,
};

static const struct mtk_crtc_path_data mt6768_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6768_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6768_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6768_addon_ext,
};

static const enum mtk_ddp_comp_id mt6761_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_RDMA0,
};

static const struct mtk_crtc_path_data mt6761_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6761_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6761_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6761_addon_ext,
};

static const enum mtk_ddp_comp_id mt6877_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_RDMA0,
};

static const struct mtk_crtc_path_data mt6877_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6877_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6877_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6877_addon_ext,
};

static const struct mtk_crtc_path_data mt6885_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6885_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6885_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6885_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6885_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6885_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6885_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6885_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6885_addon_main,
	.addon_data_dual = mt6885_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6885_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6885_addon_ext,
	.dual_path[0] = mt6885_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6885_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6885_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_third),
	.addon_data = mt6885_addon_ext,
};

static const struct mtk_crtc_path_data mt6983_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6983_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6983_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = true,
	.addon_data = mt6983_addon_main,
	.addon_data_dual = mt6983_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6983_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6983_addon_ext,
	.dual_path[0] = mt6983_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6983_mtk_ext_dsi_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_ext_dsi,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_ext_dsi),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6983_addon_ext,
	.dual_path[0] = mt6983_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6983_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_third),
	.addon_data = mt6983_addon_ext,
};

static const struct mtk_crtc_path_data mt6985_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6985_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.ovl_path[DDP_MINOR][0] = mt6985_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6985_mtk_ovlsys_main_bringup),
	.path[DDP_MINOR][0] = mt6985_mtk_ddp_main_bringup_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_main_bringup_minor),
	.path_req_hrt[DDP_MINOR][0] = true,
	.dual_ovl_path[0] = mt6985_mtk_ovlsys_dual_main_bringup,
	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6985_mtk_ovlsys_dual_main_bringup),
	.dual_path[0] = mt6985_mtk_ddp_dual_main_bringup,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6985_addon_main,
	.addon_data_dual = mt6985_addon_main_dual,
	.scaling_data = mt6985_scaling_main,
	.scaling_data_dual = mt6985_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6985_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6985_mtk_ddp_dual_ext_dp,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_ext_dp),
	.addon_data = mt6985_addon_ext,
};

static const struct mtk_crtc_path_data mt6985_mtk_dp_w_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_mem_dp_w_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_mem_dp_w_tdshp),
	.addon_data = mt6983_addon_dp_w_tdshp,
};

static const struct mtk_crtc_path_data mt6985_mtk_dp_wo_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_mem_dp_wo_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_mem_dp_wo_tdshp),
	.addon_data = mt6983_addon_dp_wo_tdshp,
};

static const struct mtk_crtc_path_data mt6985_mtk_secondary_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_secondary_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_secondary_dp),
	.dual_path[0] = mt6985_mtk_ddp_dual_secondary_dp,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_secondary_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6985_addon_secondary_path,
//	.addon_data_dual = mt6985_addon_secondary_path_dual,
};

static const struct mtk_crtc_path_data mt6985_mtk_discrete_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_discrete_chip,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_discrete_chip),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6985_mtk_ddp_dual_discrete_chip,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_discrete_chip),
	.addon_data = mt6983_addon_discrete_path,
//	.addon_data_dual = mt6985_addon_discrete_path_dual,
	.is_discrete_path = true,
};

static const struct mtk_crtc_path_data mt6989_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6989_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.ovl_path[DDP_MINOR][0] = mt6989_mtk_ovlsys_main_bringup,
//	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_bringup),
//	.path[DDP_MINOR][0] = mt6989_mtk_ddp_main_bringup_minor,
//	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup_minor),
//	.path_req_hrt[DDP_MINOR][0] = true,
//	.dual_ovl_path[0] = mt6989_mtk_ovlsys_dual_main_bringup,
//	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6989_mtk_ovlsys_dual_main_bringup),
//	.dual_path[0] = mt6989_mtk_ddp_dual_main_bringup,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6989_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6989_mtk_main_full_set_data = {
	.ovl_path[DDP_MAJOR][0] = mt6989_mtk_ovlsys_main_full_set,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_full_set),
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6989_addon_main,
	.addon_data_dual = mt6989_addon_main_dual,
	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6989_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_ext_dp,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_ext_dp),
	.addon_data = mt6989_addon_ext,
};

static const struct mtk_crtc_path_data mt6989_mtk_dp_w_tdshp_path_data = {
//	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_mem_dp_w_tdshp,
//	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_mem_dp_w_tdshp),
//	.addon_data = mt6983_addon_dp_w_tdshp,
};

static const struct mtk_crtc_path_data mt6989_mtk_dp_wo_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_mem_dp_wo_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_mem_dp_wo_tdshp),
	.addon_data = mt6989_addon_dp_wo_tdshp,
};

static const struct mtk_crtc_path_data mt6989_mtk_secondary_path_data = {
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_secondary,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_secondary),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_secondary_dp,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_secondary_dp),
	.addon_data = mt6989_addon_secondary_path,
//	.addon_data_dual = mt6989_addon_secondary_path_dual,
};

static const struct mtk_crtc_path_data mt6989_mtk_discrete_path_data = {
	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_discrete_chip,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_discrete_chip),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_discrete_chip,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_discrete_chip),
	.addon_data = mt6989_addon_discrete_path,
//	.addon_data_dual = mt6989_addon_discrete_path_dual,
	.is_discrete_path = true,
};

static const struct mtk_crtc_path_data mt6899_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6899_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6899_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6899_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6899_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.ovl_path[DDP_MINOR][0] = mt6989_mtk_ovlsys_main_bringup,
//	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_bringup),
//	.path[DDP_MINOR][0] = mt6989_mtk_ddp_main_bringup_minor,
//	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup_minor),
//	.path_req_hrt[DDP_MINOR][0] = true,
//	.dual_ovl_path[0] = mt6989_mtk_ovlsys_dual_main_bringup,
//	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6989_mtk_ovlsys_dual_main_bringup),
//	.dual_path[0] = mt6989_mtk_ddp_dual_main_bringup,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6989_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};
static const struct mtk_crtc_path_data mt6899_mtk_main_bypass_pc_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6899_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6899_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6899_mtk_ddp_main_bypass_pc_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6899_mtk_ddp_main_bypass_pc_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.ovl_path[DDP_MINOR][0] = mt6989_mtk_ovlsys_main_bringup,
//	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_bringup),
//	.path[DDP_MINOR][0] = mt6989_mtk_ddp_main_bringup_minor,
//	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup_minor),
//	.path_req_hrt[DDP_MINOR][0] = true,
//	.dual_ovl_path[0] = mt6989_mtk_ovlsys_dual_main_bringup,
//	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6989_mtk_ovlsys_dual_main_bringup),
//	.dual_path[0] = mt6989_mtk_ddp_dual_main_bringup,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6989_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6899_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6899_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6899_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6989_addon_ext,
};

static const struct mtk_crtc_path_data mt6991_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6991_mtk_ovlsys_main_bringup,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup),
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.ovl_path[DDP_MINOR][0] = mt6991_mtk_ovlsys_main_bringup,
//	.ovl_path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup),
//	.path[DDP_MINOR][0] = mt6991_mtk_ddp_main_bringup_minor,
//	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_main_bringup_minor),
//	.path_req_hrt[DDP_MINOR][0] = true,
//	.dual_ovl_path[0] = mt6989_mtk_ovlsys_dual_main_bringup,
//	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6989_mtk_ovlsys_dual_main_bringup),
//	.dual_path[0] = mt6989_mtk_ddp_dual_main_bringup,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6991_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
	.scaling_data = mt6991_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6991_mtk_main_dual_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6991_mtk_ovlsys_main_bringup_dual,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup_dual),
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6991_addon_main,
	.scaling_data = mt6991_scaling_main,
	.is_exdma_dual_layer = true,
};

static const struct mtk_crtc_path_data mt6991_mtk_main_bypass_pc_dual_path_data  = {
	.ovl_path[DDP_MAJOR][0] = mt6991_mtk_ovlsys_main_bringup_dual,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup_dual),
	.path[DDP_MAJOR][0] = mt6991_mtk_ovlsys_main_bringup_dual,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ovlsys_main_bringup_dual),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6991_addon_main,
	.scaling_data = mt6991_scaling_main,
	.is_exdma_dual_layer = true,
};

static const struct mtk_crtc_path_data mt6991_mtk_main_full_set_data = {
//	.ovl_path[DDP_MAJOR][0] = mt6989_mtk_ovlsys_main_full_set,
//	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ovlsys_main_full_set),
//	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_main_bringup,
//	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_main_bringup),
//	.path_req_hrt[DDP_MAJOR][0] = true,
//	.addon_data = mt6989_addon_main,
//	.addon_data_dual = mt6989_addon_main_dual,
//	.scaling_data = mt6989_scaling_main,
//	.scaling_data_dual = mt6989_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6991_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6991_addon_ext,
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	.is_exdma_dual_layer = true,
#endif
};

static const struct mtk_crtc_path_data mt6991_mtk_dp_w_tdshp_path_data = {
//	.path[DDP_MAJOR][0] = mt6989_mtk_ddp_mem_dp_w_tdshp,
//	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6989_mtk_ddp_mem_dp_w_tdshp),
//	.addon_data = mt6983_addon_dp_w_tdshp,
};

static const struct mtk_crtc_path_data mt6991_mtk_dp_wo_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_mem_dp_wo_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_mem_dp_wo_tdshp),
	.addon_data = mt6989_addon_dp_wo_tdshp,
	.is_exdma_dual_layer = true,
};

static const struct mtk_crtc_path_data mt6991_mtk_secondary_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_secondary,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_secondary),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_secondary_dp,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_secondary_dp),
	.addon_data = mt6989_addon_secondary_path,
//	.addon_data_dual = mt6989_addon_secondary_path_dual,
};

static const struct mtk_crtc_path_data mt6991_mtk_discrete_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_discrete_chip,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_discrete_chip),
	.path_req_hrt[DDP_MAJOR][0] = true,
//	.dual_path[0] = mt6989_mtk_ddp_dual_discrete_chip,
//	.dual_path_len[0] = ARRAY_SIZE(mt6989_mtk_ddp_dual_discrete_chip),
	.addon_data = mt6989_addon_discrete_path,
//	.addon_data_dual = mt6989_addon_discrete_path_dual,
	.is_discrete_path = true,
};

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static const struct mtk_crtc_path_data mt6991_mtk_fifth_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_fifth_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_fifth_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt6991_mtk_sixth_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_sixth_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_sixth_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt6991_mtk_seventh_path_data = {
	.path[DDP_MAJOR][0] = mt6991_mtk_ddp_seventh_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6991_mtk_ddp_seventh_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
};
#endif

static const struct mtk_crtc_path_data mt6897_mtk_main_path_data = {
	.ovl_path[DDP_MAJOR][0] = mt6897_mtk_ovlsys_main,
	.ovl_path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6897_mtk_ovlsys_main),
	.path[DDP_MAJOR][0] = mt6897_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6897_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_ovl_path[0] = mt6897_mtk_ovlsys_dual_main,
	.dual_ovl_path_len[0] = ARRAY_SIZE(mt6897_mtk_ovlsys_dual_main),
	.dual_path[0] = mt6897_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6897_mtk_ddp_dual_main),
	.addon_data = mt6897_addon_main,
	.addon_data_dual = mt6897_addon_main_dual,
	.scaling_data = mt6985_scaling_main,
	.scaling_data_dual = mt6985_scaling_main_dual,
};

static const struct mtk_crtc_path_data mt6897_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6897_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6897_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6897_mtk_ddp_dual_ext_dp,
	.dual_path_len[0] = ARRAY_SIZE(mt6897_mtk_ddp_dual_ext_dp),
	.addon_data = mt6897_addon_ext,
};

static const struct mtk_crtc_path_data mt6897_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6897_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6897_mtk_ddp_third),
	.dual_path[0] = mt6897_mtk_ddp_dual_third,
	.dual_path_len[0] = ARRAY_SIZE(mt6897_mtk_ddp_dual_third),
	.addon_data = mt6897_addon_ext,
};

static const struct mtk_crtc_path_data mt6895_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6895_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6895_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6895_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6895_mtk_ddp_main_wb_path),
	.addon_data = mt6895_addon_main,
	.addon_data_dual = mt6895_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6895_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6895_addon_ext,
	.dual_path[0] = mt6895_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6895_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6886_mtk_ext_path_data = {
	.is_fake_path = true,
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_third),
	.addon_data = mt6886_addon_ext,
};

static const struct mtk_crtc_path_data mt6895_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_third),
	.addon_data = mt6895_addon_ext,
};

static const struct mtk_crtc_path_data mt6886_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_third),
	.addon_data = mt6886_addon_ext,
};

static const struct mtk_crtc_path_data mt6886_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6895_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6895_mtk_ddp_main_wb_path),
	.addon_data = mt6886_addon_main,
};

static const struct mtk_crtc_path_data mt2701_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt2701_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2701_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2701_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt2701_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2701_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2712_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt2712_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2712_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2712_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt2712_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2712_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt8173_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt8173_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt8173_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt8173_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt8173_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt8173_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt6873_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6873_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6873_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6873_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6873_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6873_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6873_addon_main,
};

static const struct mtk_crtc_path_data mt6873_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6873_addon_ext,
};

static const struct mtk_crtc_path_data mt6873_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_third),
	.addon_data = mt6873_addon_ext,
};

static const struct mtk_crtc_path_data mt6853_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6853_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6853_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6853_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6853_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6853_addon_main,
};

static const struct mtk_crtc_path_data mt6853_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6853_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6853_mtk_ddp_third),
	.addon_data = mt6853_addon_ext,
};

static const struct mtk_crtc_path_data mt6833_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6833_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6833_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6833_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6833_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6833_addon_main,
};

static const struct mtk_crtc_path_data mt6833_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6833_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6833_mtk_ddp_third),
	.addon_data = mt6833_addon_ext,
};

static const struct mtk_crtc_path_data mt6877_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6877_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6877_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6877_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6877_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6877_addon_main,
};

static const struct mtk_crtc_path_data mt6877_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6877_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6877_mtk_ddp_third),
	.addon_data = mt6877_addon_ext,
};

static const struct mtk_crtc_path_data mt6781_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6781_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6781_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6781_addon_main,
};

static const struct mtk_crtc_path_data mt6781_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6781_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6781_mtk_ddp_third),
	.addon_data = mt6781_addon_ext,
};

static const struct mtk_crtc_path_data mt6879_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
#ifdef IF_ZERO
	.path[DDP_MINOR][1] = mt6879_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6879_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
#endif
	.addon_data = mt6879_addon_main,
};

static const struct mtk_crtc_path_data mt6879_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6879_addon_ext,
};

static const struct mtk_crtc_path_data mt6879_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_third),
	.addon_data = mt6879_addon_ext,
};

static const struct mtk_crtc_path_data mt6855_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6855_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6855_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
#ifdef IF_ZERO
	.path[DDP_MINOR][1] = mt6855_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6855_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
#endif
	.addon_data = mt6855_addon_main,
};

static const struct mtk_crtc_path_data mt6855_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6855_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6855_mtk_ddp_third),
	.addon_data = mt6855_addon_ext,
};

const struct mtk_session_mode_tb mt6779_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6779_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 19, .share_port = false},
		{.CG_idx = 1, .CG_bit = 4, .share_port = false},
};

static const struct mtk_fake_eng_data mt6779_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6779_fake_eng_reg),
	.fake_eng_reg = mt6779_fake_eng_reg,
};

static const struct mtk_session_mode_tb mt6761_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6761_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6761_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6761_fake_eng_reg),
	.fake_eng_reg = mt6761_fake_eng_reg,
};

static const struct mtk_session_mode_tb mt6765_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6765_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6765_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6765_fake_eng_reg),
	.fake_eng_reg = mt6765_fake_eng_reg,
};

static const struct mtk_session_mode_tb mt6768_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6768_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6768_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6768_fake_eng_reg),
	.fake_eng_reg = mt6768_fake_eng_reg,
};

const struct mtk_session_mode_tb mt6885_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6983_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6985_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6989_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6991_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},

		// When multi-display (3~10 display), just use SESSION_TRIPLE_DL
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				// Although ddp_mode array size is MAX_SESSION_COUNT,
				// it will init to DDP_MAJOR by default, so we do not need to init all element
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};


const struct mtk_session_mode_tb mt6897_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
		},
		[MTK_DRM_SESSION_DOUBLE_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
		},
		[MTK_DRM_SESSION_DC_MIRROR] = {
				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
		},
		[MTK_DRM_SESSION_TRIPLE_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
		},
};

/* mt6886 is the same as mt6895 */
const struct mtk_session_mode_tb mt6895_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6873_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};
const struct mtk_session_mode_tb mt6853_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6833_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6877_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6781_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6879_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6855_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {
				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {
				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6885_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6885_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6885_fake_eng_reg),
	.fake_eng_reg = mt6885_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6983_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6983_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6983_fake_eng_reg),
	.fake_eng_reg = mt6983_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6985_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6985_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6985_fake_eng_reg),
	.fake_eng_reg = mt6985_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6895_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 3, .share_port = true},
		{.CG_idx = 0, .CG_bit = 6, .share_port = true},
};

static const struct mtk_fake_eng_reg mt6989_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6989_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6989_fake_eng_reg),
	.fake_eng_reg = mt6989_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6991_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6991_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6991_fake_eng_reg),
	.fake_eng_reg = mt6991_fake_eng_reg,
};

/* mt6886 is the same as mt6895 */
static const struct mtk_fake_eng_data mt6895_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6895_fake_eng_reg),
	.fake_eng_reg = mt6895_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6873_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 18, .share_port = true},
		{.CG_idx = 0, .CG_bit = 19, .share_port = true},
};

static const struct mtk_fake_eng_data mt6873_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6873_fake_eng_reg),
	.fake_eng_reg = mt6873_fake_eng_reg,
};
static const struct mtk_fake_eng_reg mt6853_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6853_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6853_fake_eng_reg),
	.fake_eng_reg = mt6853_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6833_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6833_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6833_fake_eng_reg),
	.fake_eng_reg = mt6833_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6877_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6877_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6877_fake_eng_reg),
	.fake_eng_reg = mt6877_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6781_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6781_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6781_fake_eng_reg),
	.fake_eng_reg = mt6781_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6879_fake_eng_reg[] = {
	{.CG_idx = 0, .CG_bit = 3, .share_port = false},
	{.CG_idx = 0, .CG_bit = 6, .share_port = false},
};

static const struct mtk_fake_eng_data mt6879_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6879_fake_eng_reg),
	.fake_eng_reg = mt6879_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6855_fake_eng_reg[] = {
	{.CG_idx = 0, .CG_bit = 3, .share_port = false},
	{.CG_idx = 0, .CG_bit = 6, .share_port = false},
};

static const struct mtk_fake_eng_data mt6855_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6855_fake_eng_reg),
	.fake_eng_reg = mt6855_fake_eng_reg,
};

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.main_path_data = &mt2701_mtk_main_path_data,
	.ext_path_data = &mt2701_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT2701,
	.shadow_register = true,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.main_path_data = &mt2712_mtk_main_path_data,
	.ext_path_data = &mt2712_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT2712,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.main_path_data = &mt8173_mtk_main_path_data,
	.ext_path_data = &mt8173_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT8173,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6779_mmsys_driver_data = {
	.main_path_data = &mt6779_mtk_main_path_data,
	.ext_path_data = &mt6779_mtk_ext_path_data,
	.third_path_data = &mt6779_mtk_third_path_data,
	.mmsys_id = MMSYS_MT6779,
	.mode_tb = mt6779_mode_tb,
	.sodi_config = mt6779_mtk_sodi_config,
	.fake_eng_data = &mt6779_fake_eng_data,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
	.use_infra_mem_res = false,
};

const struct mtk_mmsys_driver_data mt6761_mmsys_driver_data = {
	.main_path_data = &mt6761_mtk_main_path_data,
	.ext_path_data = &mt6761_mtk_ext_path_data,
	.third_path_data = &mt6761_mtk_third_path_data,
	.fake_eng_data = &mt6761_fake_eng_data,
	.mmsys_id = MMSYS_MT6761,
	.mode_tb = mt6761_mode_tb,
	.sodi_config = mt6768_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

const struct mtk_mmsys_driver_data mt6765_mmsys_driver_data = {
	.main_path_data = &mt6765_mtk_main_path_data,
	.ext_path_data = &mt6765_mtk_third_path_data,
	.third_path_data = &mt6765_mtk_third_path_data,
	.fake_eng_data = &mt6765_fake_eng_data,
	.mmsys_id = MMSYS_MT6765,
	.mode_tb = mt6765_mode_tb,
	.sodi_config = mt6768_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

const struct mtk_mmsys_driver_data mt6768_mmsys_driver_data = {
	.main_path_data = &mt6768_mtk_main_path_data,
	.ext_path_data = &mt6768_mtk_ext_path_data,
	.third_path_data = &mt6768_mtk_third_path_data,
	.fake_eng_data = &mt6768_fake_eng_data,
	.mmsys_id = MMSYS_MT6768,
	.mode_tb = mt6768_mode_tb,
	.sodi_config = mt6768_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6885_mmsys_driver_data = {
	.main_path_data = &mt6885_mtk_main_path_data,
	.ext_path_data = &mt6885_mtk_ext_path_data,
	.third_path_data = &mt6885_mtk_third_path_data,
	.fake_eng_data = &mt6885_fake_eng_data,
	.mmsys_id = MMSYS_MT6885,
	.mode_tb = mt6885_mode_tb,
	.sodi_config = mt6885_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6983_mmsys_driver_data = {
	.main_path_data = &mt6983_mtk_main_path_data,
	.ext_path_data = &mt6983_mtk_ext_path_data,
	.ext_alter_path_data = &mt6983_mtk_ext_dsi_path_data,
	.third_path_data = &mt6983_mtk_third_path_data,
	.fourth_path_data_secondary = &mt6983_mtk_ext_dsi_path_data,
	.fake_eng_data = &mt6983_fake_eng_data,
	.mmsys_id = MMSYS_MT6983,
	.mode_tb = mt6983_mode_tb,
	.sodi_config = mt6983_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6985_mmsys_driver_data = {
	.main_path_data = &mt6985_mtk_main_path_data,
	.ext_path_data = &mt6985_mtk_ext_path_data,
	.third_path_data = &mt6985_mtk_dp_w_tdshp_path_data,
	.third_path_data_wo_tdshp = &mt6985_mtk_dp_wo_tdshp_path_data,
	.fourth_path_data_secondary = &mt6985_mtk_secondary_path_data,
	.fourth_path_data_discrete = &mt6985_mtk_discrete_path_data,
	.fake_eng_data = &mt6985_fake_eng_data,
	.mmsys_id = MMSYS_MT6985,
	.mode_tb = mt6985_mode_tb,
	.sodi_config = mt6985_mtk_sodi_config,
	.sodi_apsrc_config = mt6985_mtk_sodi_apsrc_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.pf_ts_type = IRQ_CMDQ_CB,
};

static const struct mtk_mmsys_driver_data mt6989_mmsys_driver_data = {
	.main_path_data = &mt6989_mtk_main_path_data,
	.ext_alter_path_data = &mt6989_mtk_main_full_set_data,//temporary solution for OVL full set
	.ext_path_data = &mt6989_mtk_ext_path_data,
	.third_path_data = &mt6989_mtk_dp_w_tdshp_path_data,
	.third_path_data_wo_tdshp = &mt6989_mtk_dp_wo_tdshp_path_data,
	.fourth_path_data_secondary = &mt6989_mtk_secondary_path_data,
	.fourth_path_data_discrete = &mt6989_mtk_discrete_path_data,
	.fake_eng_data = &mt6989_fake_eng_data,
	.mmsys_id = MMSYS_MT6989,
	.mode_tb = mt6989_mode_tb,
	.sodi_config = mt6989_mtk_sodi_config,
	.sodi_apsrc_config = mt6989_mtk_sodi_apsrc_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.need_emi_eff = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.pf_ts_type = IRQ_CMDQ_CB,
	.respective_ostdl = true,
	.update_channel_bw_by_layer = mtk_disp_update_channel_bw_by_layer_MT6989,
	.update_channel_bw_by_larb = mtk_disp_update_channel_bw_by_larb_MT6989,
};

static const struct mtk_mmsys_driver_data mt6899_mmsys_driver_data = {
	.main_path_data = &mt6899_mtk_main_path_data,
	.main_bypass_pc_path_data = &mt6899_mtk_main_bypass_pc_path_data,
	//.ext_alter_path_data = &mt6989_mtk_main_full_set_data,//temporary solution for OVL full set
	.ext_path_data = &mt6899_mtk_ext_path_data,
	//.third_path_data = &mt6989_mtk_dp_w_tdshp_path_data,
	//.third_path_data_wo_tdshp = &mt6989_mtk_dp_wo_tdshp_path_data,
	//.fourth_path_data_secondary = &mt6989_mtk_secondary_path_data,
	//.fourth_path_data_discrete = &mt6989_mtk_discrete_path_data,
	.fake_eng_data = &mt6989_fake_eng_data,
	.mmsys_id = MMSYS_MT6899,
	.mode_tb = mt6989_mode_tb,
	.sodi_config = mt6989_mtk_sodi_config,
	.sodi_apsrc_config = mt6989_mtk_sodi_apsrc_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.need_emi_eff = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.pf_ts_type = IRQ_CMDQ_CB,
	.respective_ostdl = true,
	.real_srt_ostdl = true,
	.wb_skip_sec_buf = true,
	.update_channel_bw_by_layer = mtk_disp_update_channel_bw_by_layer_MT6899,
	.update_channel_bw_by_larb = mtk_disp_update_channel_bw_by_larb_MT6899,
};

static const struct mtk_mmsys_driver_data mt6991_mmsys_driver_data = {
	.main_path_data = &mt6991_mtk_main_path_data,
	.main_dual_path_data = &mt6991_mtk_main_dual_path_data,
	.ext_alter_path_data = &mt6991_mtk_main_full_set_data,//temporary solution for OVL full set
	.ext_path_data = &mt6991_mtk_ext_path_data,
	.third_path_data = &mt6991_mtk_dp_w_tdshp_path_data,
	.third_path_data_wo_tdshp = &mt6991_mtk_dp_wo_tdshp_path_data,
	.fourth_path_data_secondary = &mt6991_mtk_secondary_path_data,
	.fourth_path_data_discrete = &mt6991_mtk_discrete_path_data,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	.fifth_path_data = &mt6991_mtk_fifth_path_data,
	.sixth_path_data = &mt6991_mtk_sixth_path_data,
	.seventh_path_data = &mt6991_mtk_seventh_path_data,
#endif
	.fake_eng_data = &mt6991_fake_eng_data,
	.mmsys_id = MMSYS_MT6991,
	.mode_tb = mt6991_mode_tb,
	.sodi_config = mt6991_mtk_sodi_config,
	.sodi_apsrc_config = mt6991_mtk_sodi_apsrc_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.need_emi_eff = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.gce_event_config = mtk_gce_event_config_MT6991,
	.vdisp_ao_irq_config = mtk_vdisp_ao_irq_config_MT6991,
	.pf_ts_type = IRQ_CMDQ_CB,
	.respective_ostdl = true,
	.ovl_exdma_rule = true,
	.real_srt_ostdl = true,
	.skip_trans = true,
	.update_channel_hrt = mtk_disp_update_channel_hrt_MT6991,
	.get_channel_idx = mtk_disp_get_channel_idx_MT6991,
};

static const struct mtk_mmsys_driver_data mt6897_mmsys_driver_data = {
	.main_path_data = &mt6897_mtk_main_path_data,
	.ext_path_data = &mt6897_mtk_ext_path_data,
	.third_path_data = &mt6897_mtk_third_path_data,
	.mmsys_id = MMSYS_MT6897,
	.mode_tb = mt6897_mode_tb,
	.sodi_config = mt6985_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.pf_ts_type = IRQ_CMDQ_CB,
};

static const struct mtk_mmsys_driver_data mt6895_mmsys_driver_data = {
	.main_path_data = &mt6895_mtk_main_path_data,
	.ext_path_data = &mt6895_mtk_ext_path_data,
	.third_path_data = &mt6895_mtk_third_path_data,
	.fake_eng_data = &mt6895_fake_eng_data,
	.mmsys_id = MMSYS_MT6895,
	.mode_tb = mt6895_mode_tb,
	.sodi_config = mt6895_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = true,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6886_mmsys_driver_data = {
	.main_path_data = &mt6886_mtk_main_path_data,
	/*
	 * mt6886 not support dp path will use third path as ext_path_data
	 * to avoid crtc_id is 1 for third path
	 */
	.ext_path_data = &mt6886_mtk_ext_path_data,
	/* WFD path */
	.third_path_data = &mt6886_mtk_third_path_data,
	.fake_eng_data = &mt6895_fake_eng_data,
	.mmsys_id = MMSYS_MT6886,
	.mode_tb = mt6895_mode_tb,
	/* sodi is same as mt6895 */
	.sodi_config = mt6895_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = true,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6873_mmsys_driver_data = {
	.main_path_data = &mt6873_mtk_main_path_data,
	.ext_path_data = &mt6873_mtk_ext_path_data,
	.third_path_data = &mt6873_mtk_third_path_data,
	.fake_eng_data = &mt6873_fake_eng_data,
	.mmsys_id = MMSYS_MT6873,
	.mode_tb = mt6873_mode_tb,
	.sodi_config = mt6873_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6853_mmsys_driver_data = {
	.main_path_data = &mt6853_mtk_main_path_data,
	.ext_path_data = &mt6853_mtk_third_path_data,
	.third_path_data = &mt6853_mtk_third_path_data,
	.fake_eng_data = &mt6853_fake_eng_data,
	.mmsys_id = MMSYS_MT6853,
	.mode_tb = mt6853_mode_tb,
	.sodi_config = mt6853_mtk_sodi_config,
	.has_smi_limitation = true,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6833_mmsys_driver_data = {
	.main_path_data = &mt6833_mtk_main_path_data,
	.ext_path_data = &mt6833_mtk_third_path_data,
	.third_path_data = &mt6833_mtk_third_path_data,
	.fake_eng_data = &mt6833_fake_eng_data,
	.mmsys_id = MMSYS_MT6833,
	.mode_tb = mt6833_mode_tb,
	.sodi_config = mt6833_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.has_smi_limitation = true,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6877_mmsys_driver_data = {
	.main_path_data = &mt6877_mtk_main_path_data,
	.ext_path_data = &mt6877_mtk_ext_path_data,
	.third_path_data = &mt6877_mtk_third_path_data,
	.fake_eng_data = &mt6877_fake_eng_data,
	.mmsys_id = MMSYS_MT6877,
	.mode_tb = mt6877_mode_tb,
	.sodi_config = mt6877_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
	.has_smi_limitation = true,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6781_mmsys_driver_data = {
	.main_path_data = &mt6781_mtk_main_path_data,
	.ext_path_data = &mt6781_mtk_third_path_data,
	.third_path_data = &mt6781_mtk_third_path_data,
	.fake_eng_data = &mt6781_fake_eng_data,
	.mmsys_id = MMSYS_MT6781,
	.mode_tb = mt6781_mode_tb,
	.sodi_config = mt6781_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
	.use_infra_mem_res = true,
};

static const struct mtk_mmsys_driver_data mt6879_mmsys_driver_data = {
	.main_path_data = &mt6879_mtk_main_path_data,
	.ext_path_data = &mt6879_mtk_third_path_data,
	.third_path_data = &mt6879_mtk_third_path_data,
	.fake_eng_data = &mt6879_fake_eng_data,
	.mmsys_id = MMSYS_MT6879,
	.mode_tb = mt6879_mode_tb,
	.sodi_config = mt6879_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

static const struct mtk_mmsys_driver_data mt6855_mmsys_driver_data = {
	.main_path_data = &mt6855_mtk_main_path_data,
	.ext_path_data = &mt6855_mtk_third_path_data,
	.third_path_data = &mt6855_mtk_third_path_data,
	.fake_eng_data = &mt6855_fake_eng_data,
	.mmsys_id = MMSYS_MT6855,
	.mode_tb = mt6855_mode_tb,
	.sodi_config = mt6855_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.use_infra_mem_res = false,
};

#ifdef MTK_DRM_FENCE_SUPPORT
void mtk_drm_suspend_release_present_fence(struct device *dev,
					   unsigned int index)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	mtk_release_present_fence(private->session_id[index],
				  atomic_read(&private->crtc_present[index]), 0);
}

void mtk_drm_suspend_release_sf_present_fence(struct device *dev,
					      unsigned int index)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	mtk_release_sf_present_fence(private->session_id[index],
			atomic_read(&private->crtc_sf_present[index]));
}

int mtk_drm_suspend_release_fence(struct device *dev)
{
	unsigned int i = 0;
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	for (i = 0; i < MTK_TIMELINE_OUTPUT_TIMELINE_ID; i++) {
		DDPINFO("%s layerid=%d\n", __func__, i);
		mtk_release_layer_fence(private->session_id[0], i);
	}
	/* release present fence */
	mtk_drm_suspend_release_present_fence(dev, 0);
	/* TODO: sf present fence is obsolete, should remove corresponding code */
	/* mtk_drm_suspend_release_sf_present_fence(dev, 0); */

	return 0;
}
#endif

/*---------------- function for repaint start ------------------*/
void drm_trigger_repaint(enum DRM_REPAINT_TYPE type, struct drm_device *drm_dev)
{
	if (type > DRM_WAIT_FOR_REPAINT && type < DRM_REPAINT_TYPE_NUM) {
		struct mtk_drm_private *pvd = drm_dev->dev_private;
		struct repaint_job_t *repaint_job;

		/* get a repaint_job_t from pool */
		spin_lock(&pvd->repaint_data.wq.lock);
		if (!list_empty(&pvd->repaint_data.job_pool)) {
			repaint_job =
				list_first_entry(&pvd->repaint_data.job_pool,
						 struct repaint_job_t, link);
			list_del_init(&repaint_job->link);
		} else { /* create repaint_job_t if pool is empty */
			repaint_job = kzalloc(sizeof(struct repaint_job_t),
					      GFP_ATOMIC);
			if (IS_ERR_OR_NULL(repaint_job)) {
				DDPPR_ERR("allocate repaint_job_t fail\n");
				spin_unlock(&pvd->repaint_data.wq.lock);
				return;
			}
			INIT_LIST_HEAD(&repaint_job->link);
			DDPINFO("[REPAINT] allocate a new repaint_job_t\n");
		}

		/* init & insert repaint_job_t into queue */
		repaint_job->type = (unsigned int)type;
		list_add_tail(&repaint_job->link, &pvd->repaint_data.job_queue);

		DDPINFO("[REPAINT] insert repaint_job in queue, type:%u\n",
			type);
		spin_unlock(&pvd->repaint_data.wq.lock);
		wake_up_interruptible(&pvd->repaint_data.wq);
	}
}

int mtk_drm_wait_repaint_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *type = data;
	struct repaint_job_t *repaint_job;
	struct mtk_drm_private *pvd = dev->dev_private;

	/* reset status & wake-up threads which wait for repainting */
	DDPINFO("[REPAINT] HWC waits for repaint\n");

	/*  wait for repaint */
	ret = wait_event_interruptible(
		pvd->repaint_data.wq,
		!list_empty(&pvd->repaint_data.job_queue));

	if (ret >= 0) {
		/* retrieve first repaint_job_t from queue */
		spin_lock(&pvd->repaint_data.wq.lock);
		repaint_job = list_first_entry(&pvd->repaint_data.job_queue,
					       struct repaint_job_t, link);

		*type = repaint_job->type;

		/* remove from queue & add repaint_job_t in pool */
		list_del_init(&repaint_job->link);
		list_add_tail(&repaint_job->link, &pvd->repaint_data.job_pool);
		spin_unlock(&pvd->repaint_data.wq.lock);
		DDPINFO("[REPAINT] trigger repaint, type: %u\n", *type);
	} else {
		*type = DRM_WAIT_FOR_REPAINT;
		DDPINFO("[REPAINT] interrupted unexpected\n");
	}

	return 0;
}
/*---------------- function for repaint end ------------------*/

int mtk_drm_esd_recovery_check_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	unsigned int *crtc_id = data;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	int ret = 0;

	crtc = drm_crtc_find(dev, file_priv, *crtc_id);

	if (!crtc) {
		DDPPR_ERR("%s unknown CRTC ID %d\n", __func__, *crtc_id);
		return -EINVAL;
	}

	mtk_crtc = to_mtk_crtc(crtc);

	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is null\n", __func__);
		return  -EFAULT;
	}

	DDPINFO("[ESD RECOVERY] HWC waits for esd recovery\n");

	/*  wait for esd recovery */
	ret = wait_event_interruptible(
		mtk_crtc->esd_notice_wq,
		atomic_read(&mtk_crtc->esd_notice_status));

	if (ret >= 0)
		DDPINFO("[ESD RECOVERY] trigger esd notice\n");
	else
		DDPINFO("[ESD RECOVERY] interrupted unexpected\n");

	return 0;
}

int mtk_drm_pm_ctrl(struct mtk_drm_private *priv, enum disp_pm_action action)
{
	int ret = 0;

	if (!priv)
		return -1;

	switch (action) {
	case DISP_PM_ENABLE:
		if (priv->dsi_phy0_dev && (!pm_runtime_enabled(priv->dsi_phy0_dev)))
			pm_runtime_enable(priv->dsi_phy0_dev);

		if (priv->dsi_phy1_dev && (!pm_runtime_enabled(priv->dsi_phy1_dev)))
			pm_runtime_enable(priv->dsi_phy1_dev);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev && (!pm_runtime_enabled(priv->dsi_phy2_dev)))
			pm_runtime_enable(priv->dsi_phy2_dev);
#endif
		if (priv->dpc_dev && (!pm_runtime_enabled(priv->dpc_dev)))
			pm_runtime_enable(priv->dpc_dev);

		pm_runtime_enable(priv->mmsys_dev);

		if (priv->side_mmsys_dev)
			pm_runtime_enable(priv->side_mmsys_dev);
		if (priv->ovlsys_dev)
			pm_runtime_enable(priv->ovlsys_dev);
		if (priv->side_ovlsys_dev)
			pm_runtime_enable(priv->side_ovlsys_dev);
		break;
	case DISP_PM_DISABLE:
		if (priv->side_ovlsys_dev)
			pm_runtime_disable(priv->side_ovlsys_dev);
		if (priv->ovlsys_dev)
			pm_runtime_disable(priv->ovlsys_dev);
		if (priv->side_mmsys_dev)
			pm_runtime_disable(priv->side_mmsys_dev);

		pm_runtime_disable(priv->mmsys_dev);

		if (priv->dpc_dev)
			pm_runtime_disable(priv->dpc_dev);

		if (priv->dsi_phy0_dev)
			pm_runtime_disable(priv->dsi_phy0_dev);

		if (priv->dsi_phy1_dev)
			pm_runtime_disable(priv->dsi_phy1_dev);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev)
			pm_runtime_disable(priv->dsi_phy2_dev);
#endif
		break;
	case DISP_PM_GET:
		if (priv->dsi_phy0_dev) {
			ret = pm_runtime_resume_and_get(priv->dsi_phy0_dev);
			if (unlikely(ret)) {
				DDPMSG("request dsi phy0 power failed\n");
				return ret;
			}
		}

		if (priv->dsi_phy1_dev) {
			ret = pm_runtime_resume_and_get(priv->dsi_phy1_dev);
			if (unlikely(ret)) {
				DDPMSG("request dsi phy1 power failed\n");
				return ret;
			}
		}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev) {
			ret = pm_runtime_resume_and_get(priv->dsi_phy2_dev);
			if (unlikely(ret)) {
				DDPMSG("request dsi phy1 power failed\n");
				return ret;
			}
		}
#endif
		/* mminfra power request */
		if (priv->dpc_dev) {
			ret = pm_runtime_resume_and_get(priv->dpc_dev);
			if (unlikely(ret)) {
				DDPMSG("request mminfra power failed\n");
				return ret;
			}
		}

		pm_runtime_get_sync(priv->mmsys_dev);

		if (priv->side_mmsys_dev)
			pm_runtime_get_sync(priv->side_mmsys_dev);
		if (priv->ovlsys_dev)
			pm_runtime_get_sync(priv->ovlsys_dev);
		if (priv->side_ovlsys_dev)
			pm_runtime_get_sync(priv->side_ovlsys_dev);
		break;
	case DISP_PM_PUT:
		if (priv->side_ovlsys_dev)
			pm_runtime_put_sync(priv->side_ovlsys_dev);
		if (priv->ovlsys_dev)
			pm_runtime_put_sync(priv->ovlsys_dev);
		if (priv->side_mmsys_dev)
			pm_runtime_put_sync(priv->side_mmsys_dev);

		pm_runtime_put_sync(priv->mmsys_dev);

		if (priv->dpc_dev)
			pm_runtime_put_sync(priv->dpc_dev);

		if (priv->dsi_phy0_dev)
			pm_runtime_put_sync(priv->dsi_phy0_dev);

		if (priv->dsi_phy1_dev)
			pm_runtime_put_sync(priv->dsi_phy1_dev);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev)
			pm_runtime_put_sync(priv->dsi_phy2_dev);
#endif
		break;
	case DISP_PM_PUT_SYNC:
		if (priv->side_ovlsys_dev)
			pm_runtime_put_sync(priv->side_ovlsys_dev);
		if (priv->ovlsys_dev)
			pm_runtime_put_sync(priv->ovlsys_dev);
		if (priv->side_mmsys_dev)
			pm_runtime_put_sync(priv->side_mmsys_dev);

		pm_runtime_put_sync(priv->mmsys_dev);

		if (priv->dpc_dev) {
			if (vdisp_func.poll_power_cnt)
				vdisp_func.poll_power_cnt(0);
			pm_runtime_put_sync(priv->dpc_dev);
		}

		if (priv->dsi_phy0_dev)
			pm_runtime_put_sync(priv->dsi_phy0_dev);

		if (priv->dsi_phy1_dev)
			pm_runtime_put_sync(priv->dsi_phy1_dev);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev)
			pm_runtime_put_sync(priv->dsi_phy2_dev);
#endif
		break;
	case DISP_PM_CHECK:
		if (priv->dsi_phy0_dev && pm_runtime_get_if_in_use(priv->dsi_phy0_dev) <= 0) {
			DDPMSG("%s, dsi phy0 unused,%d", __func__,
				atomic_read(&priv->dsi_phy0_dev->power.usage_count));
			ret = -1;
			goto err_dsi_phy0;
		}
		if (priv->dsi_phy1_dev && pm_runtime_get_if_in_use(priv->dsi_phy1_dev) <= 0) {
			DDPMSG("%s, dsi phy1 unused,%d", __func__,
				atomic_read(&priv->dsi_phy1_dev->power.usage_count));
			ret = -2;
			goto err_dsi_phy1;
		}
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (priv->dsi_phy2_dev && pm_runtime_get_if_in_use(priv->dsi_phy2_dev) <= 0) {
			DDPMSG("%s, dsi phy2 unused,%d", __func__,
				atomic_read(&priv->dsi_phy2_dev->power.usage_count));
			ret = -3;
			goto err_dsi_phy2;
		}
#endif
		if (priv->dpc_dev && pm_runtime_get_if_in_use(priv->dpc_dev) <= 0) {
			DDPMSG("%s, dpc mminfra unused,%d", __func__,
				atomic_read(&priv->dpc_dev->power.usage_count));
			ret = -4;
			goto err_dpc_dev;
		}
		if (priv->mmsys_dev && pm_runtime_get_if_in_use(priv->mmsys_dev) <= 0) {
			DDPMSG("%s, mmsys unused,%d", __func__,
				atomic_read(&priv->mmsys_dev->power.usage_count));
			ret = -5;
			goto err_mmsys;
		}
		if (priv->side_mmsys_dev && pm_runtime_get_if_in_use(priv->side_mmsys_dev) <= 0) {
			DDPMSG("%s, side mmsys unused,%d", __func__,
				atomic_read(&priv->side_mmsys_dev->power.usage_count));
			ret = -6;
			goto err_side_mmsys;
		}
		if (priv->ovlsys_dev && pm_runtime_get_if_in_use(priv->ovlsys_dev) <= 0) {
			DDPMSG("%s, ovlsys unused,%d", __func__,
				atomic_read(&priv->ovlsys_dev->power.usage_count));
			ret = -7;
			goto err_ovlsys;
		}
		if (priv->side_ovlsys_dev && pm_runtime_get_if_in_use(priv->side_ovlsys_dev) <= 0) {
			DDPMSG("%s, side_ovlsys unused,%d", __func__,
				atomic_read(&priv->side_ovlsys_dev->power.usage_count));
			ret = -8;
			goto err_side_ovlsys;
		}
		break;
	}
	return ret;

err_side_ovlsys:
	if (priv->ovlsys_dev)
		pm_runtime_put_sync(priv->ovlsys_dev);
err_ovlsys:
	if (priv->side_mmsys_dev)
		pm_runtime_put_sync(priv->side_mmsys_dev);
err_side_mmsys:
	if (priv->mmsys_dev)
		pm_runtime_put_sync(priv->mmsys_dev);
err_mmsys:
	if (priv->dpc_dev)
		pm_runtime_put(priv->dpc_dev);
err_dpc_dev:
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	if (priv->dsi_phy2_dev)
		pm_runtime_put_sync(priv->dsi_phy2_dev);
err_dsi_phy2:
#endif
	if (priv->dsi_phy1_dev)
		pm_runtime_put_sync(priv->dsi_phy1_dev);
err_dsi_phy1:
	if (priv->dsi_phy0_dev)
		pm_runtime_put_sync(priv->dsi_phy0_dev);
err_dsi_phy0:
	return ret;
}

static void mtk_drm_get_top_clk(struct mtk_drm_private *priv)
{
	struct device *dev = priv->mmsys_dev;
	struct device_node *node = dev->of_node;
	struct clk *clk;
	int ret, i, clk_num;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}

	clk_num = of_count_phandle_with_args(node, "clocks", "#clock-cells");
	if (clk_num == -ENOENT) {
		priv->top_clk_num = -1;
		priv->top_clk = NULL;
		return;
	}
	priv->top_clk_num = clk_num;

	priv->top_clk = devm_kmalloc_array(dev, priv->top_clk_num,
					   sizeof(*priv->top_clk), GFP_KERNEL);

	mtk_drm_pm_ctrl(priv, DISP_PM_GET);
	DDPFUNC("first pm_get\n");

	for (i = 0; i < priv->top_clk_num; i++) {
		clk = of_clk_get(node, i);

		if (IS_ERR(clk)) {
			DDPPR_ERR("%s get %d clk failed\n", __func__, i);
			priv->top_clk_num = -1;
			return;
		}

		priv->top_clk[i] = clk;
		/* TODO: check display enable from lk */
		/* Because of align lk hw power status,
		 * we power on mtcmos at the beginning of
		 * the display initialization.
		 * We will power off mtcmos at the end of
		 * the display initialization.
		 */
		ret = clk_prepare_enable(priv->top_clk[i]);
		if (ret)
			DDPPR_ERR("top clk prepare enable failed:%d\n", i);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}
}

void mtk_drm_top_clk_prepare_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int i;
	bool en = 1;
	int ret;
	unsigned long flags = 0;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int crtc_idx = drm_crtc_index(crtc);

	if (priv->top_clk_num <= 0)
		return;

	//set_swpm_disp_active(true);
	mtk_drm_pm_ctrl(priv, DISP_PM_GET);

	for (i = 0; i < priv->top_clk_num; i++) {
		if (IS_ERR(priv->top_clk[i])) {
			DDPPR_ERR("%s invalid %d clk\n", __func__, i);
			return;
		}
		ret = clk_prepare_enable(priv->top_clk[i]);
		if (ret)
			DDPPR_ERR("top clk prepare enable failed:%d\n", i);
	}

	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_inc(&top_clk_ref);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VIDLE_FULL_SCENARIO)) {
		if (atomic_read(&top_clk_ref) == 1) {
			DDPFENCE("%s:%d power_state = true\n", __func__, __LINE__);
			priv->power_state = true;

			mtk_vidle_enable(true, priv);
			/* turn on ff only when crtc0 exsit */
			if (crtc_idx == 0) {
				CRTC_MMP_MARK(crtc_idx, enter_vidle,
					(0xc10c | 0x10000000), atomic_read(&top_clk_ref));
				mtk_vidle_config_ff(true);
			} else {
				CRTC_MMP_MARK(crtc_idx, leave_vidle,
					(0xc10c | 0x20000000), atomic_read(&top_clk_ref));
				mtk_vidle_config_ff(false);
			}
		} else if (atomic_read(&top_clk_ref) > 1) {
			CRTC_MMP_MARK(crtc_idx, leave_vidle,
				(0xc10c | 0x30000000), atomic_read(&top_clk_ref));
			mtk_vidle_config_ff(false);
		}

	} else {
		if (atomic_read(&top_clk_ref) == 1)  {
			DDPFENCE("%s:%d power_state = true\n", __func__, __LINE__);
			priv->power_state = true;

			mtk_crtc = to_mtk_crtc(priv->crtc[0]);
			if (mtk_crtc &&
				mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_DECOUPLE_MODE))
				mtk_crtc->is_mml_dc = false;
			CRTC_MMP_MARK(crtc_idx, leave_vidle,
				(0xc10c | 0x40000000), atomic_read(&top_clk_ref));
			mtk_vidle_config_ff(false);
			mtk_vidle_enable(mtk_vidle_is_ff_enabled(), priv);
		}
	}

	spin_unlock_irqrestore(&top_clk_lock, flags);

	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
	if (priv->data->sodi_config)
		priv->data->sodi_config(crtc->dev, DDP_COMPONENT_ID_MAX, NULL, &en);

	if (priv->data->disable_merge_irq)
		priv->data->disable_merge_irq(crtc->dev);
}

void mtk_drm_top_clk_disable_unprepare(struct drm_device *drm)
{
	struct mtk_drm_private *priv = drm->dev_private;
	int i = 0, cnt = 0;
	unsigned long flags = 0;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (priv->top_clk_num <= 0)
		return;

	//set_swpm_disp_active(false);
	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_dec(&top_clk_ref);
	if (atomic_read(&top_clk_ref) == 0) {
		while (atomic_read(&top_isr_ref) > 0 &&
		       cnt++ < 10) {
			spin_unlock_irqrestore(&top_clk_lock, flags);
			pr_notice("%s waiting for isr job, %d\n",
				  __func__, cnt);
			usleep_range(20, 40);
			spin_lock_irqsave(&top_clk_lock, flags);
		}
		priv->power_state = false;

		if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VIDLE_FULL_SCENARIO)) {
			CRTC_MMP_MARK(0, leave_vidle,
				(0xc10c0ff | 0x10000000), atomic_read(&top_clk_ref));
			mtk_vidle_config_ff(false);
			mtk_vidle_enable(false, priv);
		} else {
			CRTC_MMP_MARK(0, leave_vidle,
				(0xc10c0ff | 0x20000000), atomic_read(&top_clk_ref));
			mtk_crtc = to_mtk_crtc(priv->crtc[0]);
			if (mtk_crtc &&
				mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_DECOUPLE_MODE))
				mtk_crtc->is_mml_dc = false;
			mtk_vidle_config_ff(false);
			mtk_vidle_enable(false, priv);
		}
	}
	spin_unlock_irqrestore(&top_clk_lock, flags);

	for (i = priv->top_clk_num - 1; i >= 0; i--) {
		if (IS_ERR(priv->top_clk[i])) {
			DDPPR_ERR("%s invalid %d clk\n", __func__, i);
			return;
		}
		clk_disable_unprepare(priv->top_clk[i]);
	}

	if (atomic_read(&top_clk_ref) == 0)
		mtk_drm_pm_ctrl(priv, DISP_PM_PUT_SYNC);
	else
		mtk_drm_pm_ctrl(priv, DISP_PM_PUT);

	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
}

bool mtk_drm_top_clk_isr_get(struct mtk_ddp_comp *comp)
{
	unsigned long flags = 0;
	int ret = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_irqsave(&top_clk_lock, flags);
		if (atomic_read(&top_clk_ref) <= 0) {
			spin_unlock_irqrestore(&top_clk_lock, flags);
			DDPPR_ERR("%s, top clk off at %s\n", __func__, mtk_dump_comp_str(comp));
			return false;
		}
		atomic_inc(&top_isr_ref);
		if (g_dpc_dev) {
			comp->pm_ret = mtk_vidle_user_power_keep(DISP_VIDLE_USER_TOP_CLK_ISR);
			if (comp->pm_ret == VOTER_PM_LATER) {
				DRM_MMP_EVENT_START(top_clk, comp->id, 0);
				ret = pm_runtime_resume_and_get(g_dpc_dev);
				if (unlikely(ret)) {
					comp->pm_ret = 0;
					spin_unlock_irqrestore(&top_clk_lock, flags);
					DDPMSG("%s pm_runtime_resume_and_get fail\n", __func__);
					return ret;
				}
				DRM_MMP_EVENT_END(top_clk, comp->id, 0);
			}
		}
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
	return true;
}

void mtk_drm_top_clk_isr_put(struct mtk_ddp_comp *comp)
{
	unsigned long flags = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {

		spin_lock_irqsave(&top_clk_lock, flags);

		/* when timeout of polling isr ref in unpreare top clk*/
		if (atomic_read(&top_clk_ref) <= 0) {
			spin_unlock_irqrestore(&top_clk_lock, flags);
			DDPPR_ERR("%s, top clk off at %s\n", __func__, mtk_dump_comp_str(comp));
			return;
		}
		atomic_dec(&top_isr_ref);
		if (g_dpc_dev) {
			if (comp->pm_ret == VOTER_PM_LATER) {
				pm_runtime_put(g_dpc_dev);
				comp->pm_ret = 0;
			}
			mtk_vidle_user_power_release(DISP_VIDLE_USER_TOP_CLK_ISR);
		}
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
}

void mtk_vidle_multi_crtc_stop(unsigned int crtc_id)
{
	if (crtc_id == 0) {
		/* crtc0 case, if no other crtc, vidle work */
		if (atomic_read(&top_clk_ref) == 1)
			mtk_set_vidle_stop_flag(VIDLE_STOP_MULTI_CRTC, 0);
	} else {
		/* multi crtc, stop vidle */
		mtk_set_vidle_stop_flag(VIDLE_STOP_MULTI_CRTC, 1);
	}
}


#if IS_ENABLED(CONFIG_SMCDSD_PANEL)
static void mtk_drm_panel_first_enable(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_funcs *panel_funcs = mtk_drm_get_lcm_ext_funcs(crtc);
	struct mtk_panel_params *panel_params = mtk_drm_get_lcm_ext_params(crtc);

	if (drm_crtc_index(crtc))
		return;

	DDPINFO("%s\n", __func__);

	if (!panel_funcs || !panel_params)
		return;

	if (!panel_funcs->late_register || !panel_params->drm_panel)
		return;

	panel_funcs->late_register(panel_params->drm_panel);
}
#endif

static void mtk_drm_first_enable(struct drm_device *drm)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm) {
		if (mtk_drm_is_enable_from_lk(crtc))
			mtk_drm_crtc_first_enable(crtc);
		else
			mtk_drm_crtc_init_para(crtc);
		lcm_fps_ctx_init(crtc);

#if IS_ENABLED(CONFIG_SMCDSD_PANEL)
		mtk_drm_panel_first_enable(crtc);
#endif
	}
}

bool mtk_drm_session_mode_is_decouple_mode(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->session_mode == MTK_DRM_SESSION_DC_MIRROR)
		return true;

	return false;
}

bool mtk_drm_session_mode_is_mirror_mode(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->session_mode == MTK_DRM_SESSION_DC_MIRROR)
		return true;

	return false;
}

struct mtk_panel_params *mtk_drm_get_lcm_ext_params(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);

	if (!panel_ext || !panel_ext->params)
		return NULL;

	return panel_ext->params;
}

struct mtk_panel_funcs *mtk_drm_get_lcm_ext_funcs(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);

	if (!panel_ext || !panel_ext->funcs)
		return NULL;

	return panel_ext->funcs;
}

int mtk_drm_get_display_caps_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_drm_disp_caps_info *caps_info = data;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[0]);

	memset(caps_info, 0, sizeof(*caps_info));

	caps_info->hw_ver = private->data->mmsys_id;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_DISP_SELF_REFRESH;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_HRT))
		caps_info->disp_feature_flag |= DRM_DISP_FEATURE_HRT;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_RPO)) {
		caps_info->disp_feature_flag |= DRM_DISP_FEATURE_RPO;
		caps_info->rsz_in_max[0] = private->rsz_in_max[0];
		caps_info->rsz_in_max[1] = private->rsz_in_max[1];
	}
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_FBDC;

	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		caps_info->lcm_degree = 180;

	/* setting lcm_color_mode to HWC change to DSI_FILL_CONNECTOR_PROP_CAPS */
	caps_info->lcm_color_mode = MTK_DRM_COLOR_MODE_NATIVE;
	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_OVL_WCG)) {
		if (params)
			caps_info->lcm_color_mode = params->lcm_color_mode;
		else
			DDPPR_ERR("%s: failed to get lcm color mode\n",
				  __func__);
	}

	if (params) {
		caps_info->min_luminance = params->min_luminance;
		caps_info->average_luminance = params->average_luminance;
		caps_info->max_luminance = params->max_luminance;
	} else
		DDPPR_ERR("%s: failed to get lcm luminance\n", __func__);

#ifdef DRM_MMPATH
	private->HWC_gpid = task_tgid_nr(current);
#endif

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_SF_PF) &&
	    !mtk_crtc_is_frame_trigger_mode(private->crtc[0]))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_SF_PRESENT_FENCE;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_PQ_34_COLOR_MATRIX))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_PQ_34_COLOR_MATRIX;
	/* Msync 2.0
	 * according to panel
	 */
	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_MSYNC2_0)) {
		if (params && params->msync2_enable) {
			caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_MSYNC2_0;
			caps_info->msync_level_num =
				params->msync_cmd_table.msync_level_num;
		}
	}

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_MML_PRIMARY))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_MML_PRIMARY;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_VIRTUAL_DISP))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_VIRUTAL_DISPLAY;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_USE_M4U))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_IOMMU;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_OVL_BW_MONITOR;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_GPU_CACHE))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_GPU_CACHE;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_SPHRT))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_SPHRT;
#if defined(DRM_PARTIAL_UPDATE)
	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_PARTIAL_UPDATE))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_PARTIAL_UPDATE;
#endif
#ifndef DRM_BYPASS_PQ
	{
		struct mtk_ddp_comp *ddp_comp;

		ddp_comp = private->ddp_comp[DDP_COMPONENT_CHIST0];
		if (ddp_comp) {
			struct mtk_disp_chist *chist_data = comp_to_chist(ddp_comp);

			if (chist_data && chist_data->data) {
				caps_info->color_format = chist_data->data->color_format;
				caps_info->max_channel = chist_data->data->max_channel;
				caps_info->max_bin = chist_data->data->max_bin;
			}
		}
	}
#endif
	return ret;
}

/* runtime calculate vsync fps*/
int lcm_fps_ctx_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	unsigned int index;

	if (!crtc || crtc->index >= MAX_CRTC) {
		DDPPR_ERR("%s:invalid crtc or invalid index :%d\n",
			  __func__, crtc ? crtc->index : -EINVAL);
		return -EINVAL;
	}
	index = crtc->index;

	if (atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:CRTC:%d invalid output comp\n",
			  __func__, index);
		return -EINVAL;
	}

	memset(&lcm_fps_ctx[index], 0, sizeof(struct lcm_fps_ctx_t));
	spin_lock_init(&lcm_fps_ctx[index].lock);
	atomic_set(&lcm_fps_ctx[index].skip_update, 0);
	if ((mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) &&
			!mtk_dsi_is_cmd_mode(output_comp))
		lcm_fps_ctx[index].dsi_mode = 1;
	else
		lcm_fps_ctx[index].dsi_mode = 0;
	atomic_set(&lcm_fps_ctx[index].is_inited, 1);

	DDPINFO("%s CRTC:%d done\n", __func__, index);
	return 0;
}

int lcm_fps_ctx_reset(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	unsigned long flags = 0;
	unsigned int index;

	if (!crtc || crtc->index >= MAX_CRTC)
		return -EINVAL;
	index = crtc->index;

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	if (atomic_read(&lcm_fps_ctx[index].skip_update))
		return 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:CRTC:%d invalid output comp\n",
			  __func__, index);
		return -EINVAL;
	}

	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);
	if (!mtk_dsi_is_cmd_mode(output_comp))
		lcm_fps_ctx[index].dsi_mode = 1;
	else
		lcm_fps_ctx[index].dsi_mode = 0;
	lcm_fps_ctx[index].head_idx = 0;
	lcm_fps_ctx[index].num = 0;
	lcm_fps_ctx[index].last_ns = 0;
	memset(&lcm_fps_ctx[index].array, 0, sizeof(lcm_fps_ctx[index].array));
	atomic_set(&lcm_fps_ctx[index].skip_update, 0);
	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);

	DDPINFO("%s CRTC:%d done\n", __func__, index);

	return 0;
}

int lcm_fps_ctx_update(unsigned long long cur_ns,
		unsigned int crtc_id, unsigned int mode)
{
	unsigned int idx, index = crtc_id;
	unsigned long long delta;
	unsigned long flags = 0;

	if (index >= MAX_CRTC)
		return -EINVAL;

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	if (lcm_fps_ctx[index].dsi_mode != mode)
		return 0;

	if (atomic_read(&lcm_fps_ctx[index].skip_update))
		return 0;

	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);

	delta = cur_ns - lcm_fps_ctx[index].last_ns;
	if (delta == 0 || lcm_fps_ctx[index].last_ns == 0) {
		lcm_fps_ctx[index].last_ns = cur_ns;
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return 0;
	}

	idx = (lcm_fps_ctx[index].head_idx +
		lcm_fps_ctx[index].num) % LCM_FPS_ARRAY_SIZE;
	lcm_fps_ctx[index].array[idx] = delta;

	if (lcm_fps_ctx[index].num < LCM_FPS_ARRAY_SIZE)
		lcm_fps_ctx[index].num++;
	else
		lcm_fps_ctx[index].head_idx = (lcm_fps_ctx[index].head_idx +
			 1) % LCM_FPS_ARRAY_SIZE;

	lcm_fps_ctx[index].last_ns = cur_ns;

	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);

	/* DDPINFO("%s CRTC:%d update %lld to index %d\n",
	 *	__func__, index, delta, idx);
	 */

	return 0;
}

unsigned int lcm_fps_ctx_get(unsigned int crtc_id)
{
	unsigned int i;
	unsigned long long duration_avg = 0;
	unsigned long long duration_min = (1ULL << 63) - 1ULL;
	unsigned long long duration_max = 0;
	unsigned long long duration_sum = 0;
	unsigned long long fps = 100000000000;
	unsigned long flags = 0;
	unsigned int index = crtc_id;
	unsigned int fps_num = 0;
	unsigned long long fps_array[LCM_FPS_ARRAY_SIZE] = {0};
	unsigned long long start_time = 0, diff = 0;

	if (crtc_id >= MAX_CRTC) {
		DDPPR_ERR("%s:invalid crtc:%u\n",
			  __func__, crtc_id);
		return 0;
	}

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	start_time = sched_clock();
	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);

	if (atomic_read(&lcm_fps_ctx[index].skip_update) &&
	    lcm_fps_ctx[index].fps) {
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return lcm_fps_ctx[index].fps;
	}

	if (lcm_fps_ctx[index].num <= 6) {
		unsigned int ret = (lcm_fps_ctx[index].fps ?
			lcm_fps_ctx[index].fps : 0);

		DDPINFO("%s CRTC:%d num %d < 6, return fps:%u\n",
			__func__, index, lcm_fps_ctx[index].num, ret);
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return ret;
	}

	fps_num = lcm_fps_ctx[index].num;
	memcpy(fps_array, lcm_fps_ctx[index].array, sizeof(fps_array));

	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
	diff = sched_clock() - start_time;
	if (diff > 1000000)
		DDPMSG("%s diff = %llu, > 1 ms\n", __func__, diff);

	for (i = 0; i < fps_num; i++) {
		duration_sum += fps_array[i];
		duration_min = min(duration_min, fps_array[i]);
		duration_max = max(duration_max, fps_array[i]);
	}
	duration_sum -= duration_min + duration_max;
	duration_avg = DO_COMMON_DIV(duration_sum, (fps_num - 2));
	do_div(fps, duration_avg);
	lcm_fps_ctx[index].fps = (unsigned int)fps;

	DDPMSG("%s CRTC:%d max=%lld, min=%lld, sum=%lld, num=%d, fps=%u\n",
		__func__, index, duration_max, duration_min,
		duration_sum, fps_num, (unsigned int)fps);

	if (fps_num >= LCM_FPS_ARRAY_SIZE) {
		atomic_set(&lcm_fps_ctx[index].skip_update, 1);
		DDPINFO("%s set skip_update\n", __func__);
	}

	return (unsigned int)fps;
}

#ifdef DRM_OVL_SELF_PATTERN
static struct mtk_ddp_comp *mtk_drm_disp_test_get_top_plane(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j, type;
	int lay_num;
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_comp_state comp_state;
	struct mtk_ddp_comp *ovl_comp = NULL;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		type = mtk_ddp_comp_get_type(comp->id);
		if ((type == MTK_DISP_OVL) || (type == MTK_OVL_EXDMA))
			ovl_comp = comp;
		else if (type == MTK_DISP_RDMA)
			break;
	}

	if (ovl_comp == NULL) {
		DDPPR_ERR("No proper ovl module in CRTC %s\n",
			  mtk_crtc->base.name);
		return NULL;
	}

	lay_num = mtk_ovl_layer_num(ovl_comp);
	if (lay_num < 0) {
		DDPPR_ERR("invalid layer number:%d\n", lay_num);
		return NULL;
	}

	for (i = 0 ; i < mtk_crtc->layer_nr; ++i) {
		plane = &mtk_crtc->planes[i].base;
		plane_state = to_mtk_plane_state(plane->state);
		comp_state = plane_state->comp_state;

		/*find been cascaded ovl comp */
		if (comp_state.comp_id == ovl_comp->id &&
		    comp_state.lye_id == lay_num - 1) {
			plane_state->pending.enable = false;
			plane_state->pending.dirty = 1;
		}
	}

	return ovl_comp;
}

void mtk_drm_disp_test_init(struct drm_device *dev)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_crtc *crtc;
	u32 width, height, size;
	unsigned int i, j;

	DDPINFO("%s+\n", __func__);

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
		typeof(*crtc), head);

	width = crtc->mode.hdisplay;
	height = crtc->mode.vdisplay;
	size = ALIGN_TO(width, 32) * height * 4;

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem)) {
		DDPINFO("alloc buffer fail\n");
		drm_gem_object_release(&mtk_gem->base);
		return;
	}

	//Avoid kmemleak to scan
	kmemleak_ignore(mtk_gem);

	test_va = mtk_gem->kvaddr;
	test_pa = mtk_gem->dma_addr;

	if (!test_pa || !test_va) {
		DDPPR_ERR("init DAL without proper buffer\n");
		return;
	}
	DDPINFO("%s[%d] test_va=0x%llx, test_pa=0x%llx\n",
		__func__, __LINE__, (unsigned long long)test_va, (unsigned long long)test_pa);

	for (i = 0; i < height; i++)
		for (j = 0; j < width; j++) {
			int x = (i * ALIGN_TO(width, 32) + j) * 4;

			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = 255;
		}

	test_crtc = crtc;
}

static struct mtk_plane_state *mtk_drm_disp_test_set_plane_state(struct drm_crtc *crtc,
						       bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct mtk_ddp_comp *ovl_comp = mtk_drm_disp_test_get_top_plane(mtk_crtc);
	int layer_id = -1;

	if (ovl_comp)
		layer_id = mtk_ovl_layer_num(ovl_comp) - 1;

	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return NULL;
	}

	plane = &mtk_crtc->planes[mtk_crtc->layer_nr - 1].base;
	plane_state = to_mtk_plane_state(plane->state);
	pending = &plane_state->pending;

	plane_state->pending.addr = test_pa;
	plane_state->pending.pitch = ALIGN_TO(crtc->mode.hdisplay, 32) * 4;
	plane_state->pending.format = DRM_FORMAT_ARGB8888;
	plane_state->pending.src_x = 0;
	plane_state->pending.src_y = 0;
	plane_state->pending.dst_x = 0;
	plane_state->pending.dst_y = 0;
	plane_state->pending.height = crtc->mode.vdisplay;
	plane_state->pending.width = crtc->mode.hdisplay;
	plane_state->pending.config = 1;
	plane_state->pending.dirty = 1;
	plane_state->pending.enable = !!enable;
	plane_state->pending.is_sec = false;

	//pending->prop_val[PLANE_PROP_ALPHA_CON] = 0x1;
	//pending->prop_val[PLANE_PROP_PLANE_ALPHA] = 0x80;
	pending->prop_val[PLANE_PROP_COMPRESS] = 0;

	plane_state->comp_state.comp_id = ovl_comp->id;
	plane_state->comp_state.lye_id = layer_id;
	plane_state->comp_state.ext_lye_id = 0;
	plane_state->base.crtc = crtc;

	return plane_state;
}

int mtk_drm_disp_test_show(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_state *plane_state;
	struct mtk_ddp_comp *ovl_comp = mtk_drm_disp_test_get_top_plane(mtk_crtc);
	struct cmdq_pkt *cmdq_handle;
	int layer_id;

	DDPINFO("%s+\n", __func__);

	if (ovl_comp == NULL) {
		DDPPR_ERR("%s: can't find ovl comp\n", __func__);
		return 0;
	}
	layer_id = mtk_ovl_layer_num(ovl_comp) - 1;
	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return 0;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	plane_state = mtk_drm_disp_test_set_plane_state(crtc, enable);
	if (!plane_state) {
		DDPPR_ERR("%s: can't set dal plane_state\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	cmdq_handle = mtk_crtc_gce_commit_begin(crtc, NULL, NULL, false);

	if (mtk_crtc->is_dual_pipe) {
		struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
		struct mtk_plane_state plane_state_l;
		struct mtk_plane_state plane_state_r;
		struct mtk_ddp_comp *comp;

		if (plane_state->comp_state.comp_id == 0)
			plane_state->comp_state.comp_id = ovl_comp->id;

		mtk_drm_layer_dispatch_to_dual_pipe(mtk_crtc, priv->data->mmsys_id,
			plane_state, &plane_state_l, &plane_state_r,
			crtc->state->adjusted_mode.hdisplay);

		comp = priv->ddp_comp[plane_state_r.comp_state.comp_id];
		mtk_ddp_comp_layer_config(comp, layer_id,
					&plane_state_r, cmdq_handle);
		DDPINFO("%s+ comp_id:%d, comp_id:%d\n",
			__func__, comp->id,
			plane_state_r.comp_state.comp_id);

		mtk_ddp_comp_layer_config(ovl_comp, layer_id, &plane_state_l,
					  cmdq_handle);
	} else {
		mtk_ddp_comp_layer_config(ovl_comp, layer_id, plane_state, cmdq_handle);
	}
	mtk_vidle_user_power_release_by_gce(DISP_VIDLE_USER_DISP_CMDQ, cmdq_handle);

#ifndef DRM_CMDQ_DISABLE
	mtk_crtc_gce_flush(crtc, NULL, cmdq_handle, cmdq_handle);
#endif

#ifdef MTK_DRM_CMDQ_ASYNC
	cmdq_pkt_wait_complete(cmdq_handle);
#endif

	cmdq_pkt_destroy(cmdq_handle);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

#ifdef DRM_CMDQ_DISABLE
	DDPMSG("%s: trigger without cmdq\n", __func__);
	trigger_without_cmdq(crtc);
	// double check display trigger works fine
	trigger_without_cmdq(crtc);
	trigger_without_cmdq(crtc);
#else
	DDPMSG("%s: trigger cmdq\n", __func__);
	mtk_crtc_hw_block_ready(crtc);
	CRTC_MMP_MARK(0, set_dirty, TEST_SHOW, __LINE__);
	mtk_crtc_set_dirty(mtk_crtc);
#endif

	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);

	DDPINFO("%s-\n", __func__);
	return 0;
}
#endif

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps)
{
#ifndef CONFIG_MTK_DISP_NO_LK
	struct device_node *chosen_node;

	*fps = 6000;
	chosen_node = of_find_node_by_path("/chosen");

	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb",
			(int *)&size);
		if (videolfb_tag) {
			*vramsize = videolfb_tag->vram;
			*fb_base = videolfb_tag->fb_base;
			*fps = videolfb_tag->fps;
			if (*fps == 0)
				*fps = 6000;
			goto found;
		} else {
			DDPINFO("[DT][videolfb] videolfb_tag not found\n");
			return -1;
		}
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
		return -1;
	}

found:
	DDPINFO("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)*fb_base);
	DDPINFO("[DT][videolfb] vram       = 0x%x (%d)\n", *vramsize,
		*vramsize);
	DDPINFO("[DT][videolfb] fps	   = %d\n", *fps);

	return 0;
#else
	return -1;
#endif
}

unsigned int mtk_disp_num_from_atag(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb",
			(int *)&size);
		if (videolfb_tag)
			return ((videolfb_tag->islcmfound >> 16) & 0xFFFF);

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}

	return 0;
}

int mtk_drm_get_panel_info(struct drm_device *dev,
			     struct drm_mtk_session_info *info, unsigned int crtc_id)
{
	int ret = 0;
	long rotate = 0;
	unsigned int vramsize = 0, fps = 0;
	phys_addr_t fb_base = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[crtc_id]);

	if (params) {
		info->physicalWidthUm = params->physical_width_um;
		info->physicalHeightUm = params->physical_height_um;
		info->physical_width = params->physical_width;
		info->physical_height = params->physical_height;
	} else
		DDPPR_ERR("Cannot get lcm_ext_params\n");

	info->vsyncFPS = lcm_fps_ctx_get(crtc_id);
	if (!info->vsyncFPS) {
		_parse_tag_videolfb(&vramsize, &fb_base, &fps);
		info->vsyncFPS = fps;
	}

	/* kstrtol(CONFIG_MTK_LCM_PHYSICAL_ROTATION, 10, &rotate); */
	info->rotate = (int)rotate;
	DDPINFO("%s info->physicalWidthUm:%d, physicalHeightUm:%d, physical_width:%d, physical_height:%d, rotate:%d",
		__func__, info->physicalWidthUm, info->physicalHeightUm, info->physical_width,info->physical_height,
		info->rotate);

	return ret;
}

static enum mtk_ddp_comp_id panel_crtc_map[] = {
	[MTK_PANEL_DSI0] = DDP_COMPONENT_DSI0,
	[MTK_PANEL_EDP] = DDP_COMPONENT_DISP_DVO,
	[MTK_PANEL_DP0] = DDP_COMPONENT_DP_INTF0,
	[MTK_PANEL_DP1] = DDP_COMPONENT_DP_INTF1,
	[MTK_PANEL_DSI1_0] = DDP_COMPONENT_DSI1,
	[MTK_PANEL_DSI1_1] = DDP_COMPONENT_DSI1,
	[MTK_PANEL_DSI2] = DDP_COMPONENT_DSI2,
};

static int mtk_drm_get_crtc_id(enum MTK_PANEL_ID panel_id, struct mtk_drm_private *private)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;

	for (int crtc_id = 0; crtc_id < MAX_CRTC; crtc_id++) {
		mtk_crtc = to_mtk_crtc(private->crtc[crtc_id]);
		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp == NULL)
			continue;
		if (comp->id == panel_crtc_map[panel_id]) {
			DDPINFO("panel_id:%d, output comp id:%d, crtc id:%d\n", panel_id, comp->id, crtc_id);
			return crtc_id;
		}
	}

	DDPMSG("%s %d get crtc id by panel id fail, return crtc id 0", __func__, __LINE__);

	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static DEFINE_MUTEX(se_lock);

static int mtk_drm_se_enable(struct drm_device *dev, struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	struct drm_connector_state *conn_state;
	int ret;
	bool found = false;

	if (mtk_crtc->enabled) {
		DDPINFO("%s %d crtc already enable\n", __func__, __LINE__);
		return 0;
	}

	crtc = &mtk_crtc->base;
	DDPMSG("%s %d crtc%d\n", __func__, __LINE__, drm_crtc_index(crtc));

	if (mtk_crtc->virtual_path && mtk_crtc->phys_mtk_crtc)
		mtk_drm_se_enable(dev, mtk_crtc->phys_mtk_crtc);

	drm_modeset_acquire_init(&ctx, 0);
	state = drm_atomic_state_alloc(crtc->dev);
	if (!state) {
		DDPMSG("%s drm_atomic_state_alloc fail!\n", __func__);
		return -ENOMEM;
	}

	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}
	crtc_state->active = true;
	crtc_state->mode_changed = true;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (encoder->possible_crtcs & drm_crtc_mask(crtc)) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	mutex_lock(&dev->mode_config.mutex);
	if (!connector) {
		ret = -EINVAL;
		mutex_unlock(&dev->mode_config.mutex);
		DDPMSG("%s connector is not ready!", __func__);
		goto out;
	}

	if (connector->funcs->detect(connector, false) != connector_status_connected) {
		ret = -EINVAL;
		mutex_unlock(&dev->mode_config.mutex);
		DDPMSG("%s connector status is not connected(%d)!", __func__, connector->status);
		goto out;
	}

	DDPMSG("%s conn:%d, enc:%d, poss crtc:0x%x\n", __func__,
		connector->base.id, encoder->base.id, encoder->possible_crtcs);

	connector->funcs->fill_modes(connector, dev->mode_config.max_width,
						dev->mode_config.max_height);
	mutex_unlock(&dev->mode_config.mutex);

	conn_state = drm_atomic_get_connector_state(state, connector);
	ret = drm_atomic_set_crtc_for_connector(conn_state, crtc);

	mode = list_first_entry(&connector->modes, typeof(*mode), head);
	drm_mode_debug_printmodeline(mode);
	drm_mode_set_crtcinfo(mode, 0);
	ret |= drm_atomic_set_mode_for_crtc(crtc_state, mode);
	ret |= drm_atomic_commit(state);
 out:
	drm_atomic_state_put(state);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
	DDPMSG("%s crtc%d ret %d\n", __func__, drm_crtc_index(crtc), ret);

	return ret;
}
#endif

int mtk_drm_get_info_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_session_info *info = data;
	int s_type = MTK_SESSION_TYPE(info->session_id);

	if (s_type == MTK_SESSION_PRIMARY) {
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		ret = mtk_drm_get_panel_info(dev, info, 0);
#else
		int s_dev = MTK_SESSION_DEV(info->session_id);
		DDPMSG("%s %d, s_dev:%d ", __func__, __LINE__, s_dev);

		struct mtk_drm_private *priv = dev->dev_private;
		int crtc_id = mtk_drm_get_crtc_id(s_dev, priv);
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[crtc_id]);

		mutex_lock(&se_lock);
		mtk_drm_se_enable(dev, mtk_crtc);
		mutex_unlock(&se_lock);

		if (s_dev == MTK_PANEL_EDP) {
			ret = mtk_drm_dvo_get_info(dev, info);
		} else if ((s_dev == MTK_PANEL_DP0) || (s_dev == MTK_PANEL_DP1)) {
			struct mtk_ddp_comp *output_comp;
			int alias_id = 0;

			if (!mtk_crtc) {
				DDPINFO("invalid CRTC%d\n", crtc_id);
				return -EINVAL;
			}

			output_comp = mtk_ddp_comp_request_output(mtk_crtc);
			if (unlikely(!output_comp)) {
				DDPINFO("%s:CRTC%d invalid output comp\n",
					  __func__, crtc_id);
				return -EINVAL;
			}

			alias_id = mtk_ddp_comp_get_alias(output_comp->id);
			if (alias_id >= 0) {
				ret = mtk_drm_dp_get_info_by_id(dev, info, alias_id);
				DDPINFO("%s crtc_id:%d output_comp:%d alias_id:%d\n",
					__func__, crtc_id, output_comp->id, alias_id);
			} else {
				DDPINFO("%d get alias fail\n", __func__);
				return -EINVAL;
			}
		} else
			ret = mtk_drm_get_panel_info(dev, info, 0);
		DDPMSG("%s panel w%d h%d\n", __func__, info->physical_width, info->physical_height);
#endif
		return ret;
	} else if (s_type == MTK_SESSION_EXTERNAL) {
		struct mtk_drm_private *priv = dev->dev_private;
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[1]);
		struct mtk_ddp_comp *output_comp;
		enum mtk_ddp_comp_type type;

		if (!mtk_crtc) {
			DDPPR_ERR("invalid CRTC1\n");
			return -EINVAL;
		}
		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (unlikely(!output_comp)) {
			DDPPR_ERR("%s:CRTC1 invalid output comp\n",
				  __func__);
			return -EINVAL;
		}
		type = mtk_ddp_comp_get_type(output_comp->id);
		if (type == MTK_DSI)
			ret = mtk_drm_get_panel_info(dev, info, 1);
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_DPTX_AUTO)
		else if (type == MTK_DP_INTF)
			ret = mtk_drm_dp_get_info(dev, info);
#endif
		else {
			DDPPR_ERR("invalid comp %s type: %d\n", mtk_dump_comp_str(output_comp), type);
			ret = -EINVAL;
		}
		return ret;
	} else if (s_type == MTK_SESSION_MEMORY) {
		return ret;
	} else if (s_type >= MTK_SESSION_SP0 && s_type < MTK_SESSION_MAX) {
		ret = mtk_drm_get_panel_info(dev, info, 3 + (s_type - MTK_SESSION_SP0));
		return ret;
	}
	DDPPR_ERR("invalid session type:0x%08x\n", info->session_id);

	return -EINVAL;
}

int mtk_drm_get_master_info_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	int *ret = (int *)data;

	*ret = drm_is_current_master(file_priv);

	return 0;
}


int mtk_drm_set_ddp_mode(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *session_mode = data;

	ret = mtk_session_set_mode(dev, *session_mode);
	return ret;
}

int mtk_drm_ioctl_get_lcm_index(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *info = data;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[0]);

	if (params) {
		*info = params->lcm_index;
	} else {
		*info = 0;
		DDPPR_ERR("Cannot get lcm_ext_params\n");
	}

	return ret;
}

int mtk_drm_ioctl_kick_idle(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *crtc_id = data;
	struct drm_crtc *crtc;

	crtc = drm_crtc_find(dev, file_priv, *crtc_id);
	if (!crtc) {
		DDPPR_ERR("Unknown CRTC ID %d\n", *crtc_id);
		ret = -ENOENT;
		return ret;
	}
	DDPINFO("[%s CRTC:%d:%s]\n", __func__, crtc->base.id, crtc->name);

	/* async kick idle */
	mtk_drm_idlemgr_kick_async(crtc);

	return ret;
}

int mtk_drm_hwvsync_on_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *crtc_id = data;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	crtc = drm_crtc_find(dev, file_priv, *crtc_id);
	if (!crtc) {
		DDPPR_ERR("Unknown CRTC ID %d\n", *crtc_id);
		ret = -ENOENT;
		return ret;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is null\n", __func__);
		return  -EFAULT;
	}
	DDPINFO("[%s CRTC:%d:%s]\n", __func__, crtc->base.id, crtc->name);

	/* hwvsync_en*/
	mtk_crtc->hwvsync_en = 1;

	return ret;
}

int mtk_drm_get_mode_ext_info_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_mode_ext_info *args = (struct mtk_drm_mode_ext_info *)data;
	unsigned int *total_offset = NULL;
	unsigned int *real_te_duration = NULL;
	enum SWITCH_MODE_DELAY (**switch_mode_delay) = NULL;
	enum SWITCH_MODE_DELAY (**user_switch_mode_delay) = NULL;
	struct mtk_drm_private *priv = dev->dev_private;
	unsigned int copy_num;
	int i = 0, j = 0;
	struct mtk_panel_ext *panel_ext;

	crtc = drm_crtc_find(dev, file_priv, args->crtc_id);
	if (!crtc) {
		DDPPR_ERR("%s unknown CRTC ID %d\n", __func__, args->crtc_id);
		return -EINVAL;
	}

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is null\n", __func__);
		return  -EFAULT;
	}
	panel_ext = mtk_crtc->panel_ext;

	if (drm_crtc_index(crtc) != 0 || !mtk_crtc_is_frame_trigger_mode(crtc))
		return 0;

	//memory allocate of array, getting user space pointer of first layer of 2D array
	user_switch_mode_delay = kcalloc(1, sizeof(enum SWITCH_MODE_DELAY *)
		* mtk_crtc->avail_modes_num, GFP_KERNEL);
	if (copy_from_user(user_switch_mode_delay, args->switch_mode_delay,
			sizeof(enum SWITCH_MODE_DELAY *) * mtk_crtc->avail_modes_num) != 0) {
		DDPPR_ERR("%s alloc mem fail\n", __func__);
		kfree(user_switch_mode_delay);
		return  -EFAULT;
	}

	total_offset = kcalloc(1, sizeof(unsigned int) * mtk_crtc->avail_modes_num,
			GFP_KERNEL);
	real_te_duration = kcalloc(1, sizeof(unsigned int) * mtk_crtc->avail_modes_num,
			GFP_KERNEL);
	switch_mode_delay = kcalloc(1, sizeof(enum SWITCH_MODE_DELAY *)
		* mtk_crtc->avail_modes_num, GFP_KERNEL);
	for (i = 0;  i < mtk_crtc->avail_modes_num; i++) {
		switch_mode_delay[i] =
			kcalloc(1, sizeof(enum SWITCH_MODE_DELAY) * mtk_crtc->avail_modes_num,
			GFP_KERNEL);
	}
	if (!total_offset || !real_te_duration || !switch_mode_delay) {
		DDPPR_ERR("%s alloc mem fail\n", __func__);
		kfree(real_te_duration);
		kfree(total_offset);
		for (i = 0;  i < mtk_crtc->avail_modes_num; i++)
			kfree(switch_mode_delay[i]);
		kfree(switch_mode_delay);
		kfree(user_switch_mode_delay);
		return -EFAULT;
	}

	for (i = 0;  i < mtk_crtc->avail_modes_num; i++) {
		if (!switch_mode_delay[i]) {
			DDPPR_ERR("%s alloc mem fail\n", __func__);
			kfree(real_te_duration);
			kfree(total_offset);
			for (j = 0;  j < mtk_crtc->avail_modes_num; j++)
				kfree(switch_mode_delay[j]);
			kfree(switch_mode_delay);
			kfree(user_switch_mode_delay);
			return -EFAULT;
		}
	}

	for (i = 0;  i < mtk_crtc->avail_modes_num; i++) {
		unsigned int merge_trigger_offset = 150;
		unsigned int prefetch_te_offset = 150;

		if (mtk_crtc->panel_params[i] == NULL)
			continue;
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_CHECK_TRIGGER_MERGE))
			merge_trigger_offset = 0;
		else if (mtk_crtc->panel_params[i]->merge_trig_offset)
			merge_trigger_offset = mtk_crtc->panel_params[i]->merge_trig_offset;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_PREFETCH_TE))
			prefetch_te_offset = 0;
		else if (mtk_crtc->panel_params[i]->prefetch_offset)
			prefetch_te_offset = mtk_crtc->panel_params[i]->prefetch_offset;

		total_offset[i] = merge_trigger_offset + prefetch_te_offset;

		if ((mtk_crtc->panel_params[i]->real_te_duration != 0) &&
		    !mtk_crtc->panel_params[i]->skip_wait_real_te) {
			real_te_duration[i] = mtk_crtc->panel_params[i]->real_te_duration;
		} else
			real_te_duration[i] = 0; // TE duration is same to SW vsync
	}

	//fill the structure from panel driver
	if(panel_ext && panel_ext->funcs && panel_ext->funcs->get_switch_mode_delay) {
		panel_ext->funcs->get_switch_mode_delay(switch_mode_delay, mtk_crtc->avail_modes_num);
	}

	if (args->mode_num > mtk_crtc->avail_modes_num) {
		copy_num = mtk_crtc->avail_modes_num;
		DDPPR_ERR("%s mode_num:%d > avail_mode_num:%d\n", __func__,
			args->mode_num, mtk_crtc->avail_modes_num);
	} else if (args->mode_num < mtk_crtc->avail_modes_num) {
		copy_num = args->mode_num;
		DDPPR_ERR("%s mode_num:%d < avail_mode_num:%d\n", __func__,
			args->mode_num, mtk_crtc->avail_modes_num);
	} else
		copy_num = mtk_crtc->avail_modes_num;

	if (copy_to_user(args->total_offset, total_offset,
		sizeof(unsigned int) * copy_num)) {
		DDPPR_ERR("%s copy failed:(0x%p,0x%p), size:%ld\n",
			__func__, args->total_offset, total_offset,
			sizeof(unsigned int) * copy_num);

		kfree(real_te_duration);
		kfree(total_offset);
		for (i = 0;  i < mtk_crtc->avail_modes_num; i++)
			kfree(switch_mode_delay[i]);
		kfree(switch_mode_delay);
		kfree(user_switch_mode_delay);
		return -EFAULT;
	}

	if (copy_to_user(args->real_te_duration, real_te_duration,
		sizeof(unsigned int) * copy_num)) {
		DDPPR_ERR("%s copy failed:(0x%p,0x%p), size:%ld\n",
			__func__, args->real_te_duration, real_te_duration,
			sizeof(unsigned int) * copy_num);

		kfree(real_te_duration);
		kfree(total_offset);
		for (i = 0;  i < mtk_crtc->avail_modes_num; i++)
			kfree(switch_mode_delay[i]);
		kfree(switch_mode_delay);
		kfree(user_switch_mode_delay);
		return -EFAULT;
	}

	for (j = 0;  j < copy_num; j++) {
		if (copy_to_user(user_switch_mode_delay[j], switch_mode_delay[j],
			sizeof(enum SWITCH_MODE_DELAY) * copy_num)) {
			DDPPR_ERR("%s copy failed:(0x%p,0x%p), size:%ld\n",
				__func__, user_switch_mode_delay[j], switch_mode_delay[j],
				sizeof(enum SWITCH_MODE_DELAY) * copy_num);

			kfree(real_te_duration);
			kfree(total_offset);
			for (i = 0;  i < mtk_crtc->avail_modes_num; i++)
				kfree(switch_mode_delay[i]);
			kfree(switch_mode_delay);
			kfree(user_switch_mode_delay);
			return -EFAULT;
		}
	}

	args->idle_fps = IDLE_FPS;

	kfree(real_te_duration);
	kfree(total_offset);
	for (i = 0;  i < mtk_crtc->avail_modes_num; i++)
		kfree(switch_mode_delay[i]);
	kfree(switch_mode_delay);
	kfree(user_switch_mode_delay);

	return 0;
}

static void conv_to_crtc_obj_id(struct drm_device *dev,
		struct mtk_drm_panels_info *panel_ctx)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc;
	int crtc_cnt, crtc_flag;
	int i, j;

	for (i = 0; i < panel_ctx->connector_cnt; i++) {
		crtc_cnt = 0;
		crtc_flag = panel_ctx->possible_crtc[i][0];
		panel_ctx->possible_crtc[i][0] = 0;
		for (j = 0; j < MAX_CRTC; j++) {
			if ((crtc_flag & (1 << j)) == 0)
				continue;
			crtc = private->crtc[j];
			if (crtc)
				panel_ctx->possible_crtc[i][crtc_cnt++] = crtc->base.id;
		}
	}
}

int mtk_drm_ioctl_get_all_connector_panel_info(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_ddp_comp *dsi_comp;
	struct mtk_drm_private *private;
	struct mtk_drm_panels_info *panel_ctx =
			(struct mtk_drm_panels_info *)data;
	struct mtk_drm_panels_info __panel_ctx = {0};
	bool check_only_mode;
	void *ptr = NULL;
	void **panel_uptr = NULL;
	void **crtc_uptr = NULL;
	int i, ret = 0;

	if (!dev)
		return -ENODEV;

	private = dev->dev_private;
	/* just need the struct mtk_dsi definition, would not really access HW or SW state */
	dsi_comp = private->ddp_comp[DDP_COMPONENT_DSI0];
	if (!dsi_comp)
		dsi_comp = private->ddp_comp[DDP_COMPONENT_DSI1];
	if (!dsi_comp)
		return -ENXIO;

	if (!panel_ctx) {
		DDPPR_ERR("%s panel_info alloc failed\n", __func__);
		return -EINVAL;
	}

	__panel_ctx.connector_cnt = panel_ctx->connector_cnt;
	check_only_mode = (__panel_ctx.connector_cnt == -1);
	/* need copy_from_user */
	if (check_only_mode == false) {
		ptr = panel_ctx->connector_obj_id;
		__panel_ctx.connector_obj_id =
			vmalloc(sizeof(unsigned int) * __panel_ctx.connector_cnt);
		__panel_ctx.possible_crtc = vzalloc(sizeof(unsigned int *) * __panel_ctx.connector_cnt);
		__panel_ctx.panel_name = vzalloc(sizeof(char *) * __panel_ctx.connector_cnt);
		__panel_ctx.dsi_mode = vzalloc(sizeof(unsigned int) * __panel_ctx.connector_cnt);
		if (!__panel_ctx.connector_obj_id || !__panel_ctx.possible_crtc ||
				!__panel_ctx.panel_name || !__panel_ctx.dsi_mode) {
			DDPPR_ERR("%s ojb_id panel_id or panel_name alloc fail\n", __func__);
			ret = -ENOMEM;
			goto exit0;
		}

		for (i = 0 ; i < __panel_ctx.connector_cnt ; ++i) {
			__panel_ctx.possible_crtc[i] = vzalloc(sizeof(unsigned int) * MAX_CRTC_CNT);
			if (!__panel_ctx.possible_crtc[i]) {
				DDPPR_ERR("%s alloc possible_crtc fail\n", __func__);
				ret = -ENOMEM;
				goto exit1;
			}
		}
		for (i = 0 ; i < __panel_ctx.connector_cnt ; ++i) {
			__panel_ctx.panel_name[i] = vzalloc(sizeof(char) * GET_PANELS_STR_LEN);
			if (!__panel_ctx.panel_name[i]) {
				DDPPR_ERR("%s alloc panel_name fail\n", __func__);
				ret = -ENOMEM;
				goto exit1;
			}
		}
	}

	mtk_ddp_comp_io_cmd(dsi_comp, NULL, GET_ALL_CONNECTOR_PANEL_NAME, &__panel_ctx);

	/* need copy_to_user */
	if (check_only_mode == false) {
		ptr = panel_ctx->connector_obj_id;
		if (copy_to_user((void __user *)ptr, __panel_ctx.connector_obj_id,
				sizeof(unsigned int) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_to_user connector_obj_id fail\n", __func__);
			ret = -EINVAL;
			goto exit1;
		}

		// copy possible_crtc
		conv_to_crtc_obj_id(dev, &__panel_ctx);
		crtc_uptr = vmalloc(sizeof(void __user *) * __panel_ctx.connector_cnt);
		if (!crtc_uptr) {
			DDPPR_ERR("%s alloc possible_crtc fail\n", __func__);
			ret = -ENOMEM;
			goto exit2;
		}
		if (copy_from_user(crtc_uptr, panel_ctx->possible_crtc,
				sizeof(void __user *) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_from_user possible_crtc fail\n", __func__);
			ret = -EINVAL;
			goto exit2;
		}
		for (i = 0 ; i < __panel_ctx.connector_cnt; ++i) {
			if (copy_to_user((void __user *)crtc_uptr[i], __panel_ctx.possible_crtc[i],
					sizeof(unsigned int) * MAX_CRTC_CNT)) {
				DDPPR_ERR("%s copy_to_user possible_crtc fail\n", __func__);
				ret = -EINVAL;
				goto exit2;
			}
		}
		// copy panel name
		panel_uptr = vmalloc(sizeof(void __user *) * __panel_ctx.connector_cnt);
		if (!panel_uptr) {
			DDPPR_ERR("%s alloc panel_name fail\n", __func__);
			ret = -ENOMEM;
			goto exit3;
		}
		if (copy_from_user(panel_uptr, panel_ctx->panel_name,
				sizeof(void __user *) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_from_user panel_name fail\n", __func__);
			ret = -EINVAL;
			goto exit3;
		}
		for (i = 0 ; i < __panel_ctx.connector_cnt; ++i) {
			if (copy_to_user((void __user *)panel_uptr[i], __panel_ctx.panel_name[i],
					sizeof(char) * GET_PANELS_STR_LEN)) {
				DDPPR_ERR("%s copy_to_user panel_name fail\n", __func__);
				ret = -EINVAL;
				goto exit3;
			}
		}
		// copy dsi mode
		ptr = panel_ctx->dsi_mode;
		if (ptr != NULL && copy_to_user((void __user *)ptr, __panel_ctx.dsi_mode,
				sizeof(unsigned int) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_to_user dsi_mode fail\n", __func__);
			ret = -EINVAL;
			goto exit3;
		}
	} else {
		panel_ctx->connector_cnt = __panel_ctx.connector_cnt;
		panel_ctx->default_connector_id = __panel_ctx.default_connector_id;
		return ret;
	}
exit3:
	vfree(panel_uptr);
exit2:
	vfree(crtc_uptr);
exit1:
	for (i = 0 ; i < __panel_ctx.connector_cnt; ++i) {
		vfree(__panel_ctx.possible_crtc[i]);
		vfree(__panel_ctx.panel_name[i]);
	}
exit0:
	vfree(__panel_ctx.connector_obj_id);
	vfree(__panel_ctx.possible_crtc);
	vfree(__panel_ctx.panel_name);
	vfree(__panel_ctx.dsi_mode);

	return ret;
}

void mtk_drm_mmlsys_ddren_cb(struct cmdq_pkt *pkt, bool enable, void *disp_crtc)
{
	struct drm_crtc *crtc = (struct drm_crtc *)disp_crtc;

	if (crtc == NULL)
		return;

	mtk_sodi_ddren(crtc, pkt, enable);
}

void mtk_drm_mmlsys_kick_idle_cb(void *disp_crtc)
{
	struct drm_crtc *crtc = (struct drm_crtc *)disp_crtc;

	if (crtc == NULL)
		return;

	mtk_drm_idlemgr_kick(__func__, crtc, true);
}

void mtk_drm_mmlsys_submit_done_cb(void *cb_param)
{
	struct mtk_mml_cb_para *cb_para = (struct mtk_mml_cb_para *)cb_param;

	if (cb_para == NULL)
		return;

	atomic_set(&(cb_para->mml_job_submit_done), 1);
	wake_up_interruptible(&(cb_para->mml_job_submit_wq));
	DDPINFO("%s-\n", __func__);
}

void mtk_drm_wait_mml_submit_done(struct mtk_mml_cb_para *cb_para)
{
	int ret = 0;

	DDPINFO("%s+ 0x%lx 0x%lx, 0x%lx\n", __func__,
		(unsigned long)cb_para,
		(unsigned long)&(cb_para->mml_job_submit_wq),
		(unsigned long)&(cb_para->mml_job_submit_done));
	ret = wait_event_interruptible_timeout(cb_para->mml_job_submit_wq,
		atomic_read(&cb_para->mml_job_submit_done), msecs_to_jiffies(500));

	if (ret == 0) {
		DDPMSG("%s timeout\n", __func__);
		mml_drm_submit_timeout();
	}

	atomic_set(&(cb_para->mml_job_submit_done), 0);
	DDPINFO("%s- ret:%d\n", __func__, ret);
}

struct mml_drm_ctx *mtk_drm_get_mml_drm_ctx(struct drm_device *dev,
	struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct platform_device *plat_dev = NULL;
	struct platform_device *mml_pdev = NULL;
	struct mml_drm_ctx *mml_ctx = NULL;
	struct mml_drm_param disp_param = {};
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp = NULL;

	if (priv->mml_ctx != NULL)
		return priv->mml_ctx;

	if (drm_crtc_index(crtc) != 0)
		return NULL;

	plat_dev = of_find_device_by_node(priv->mutex_node);
	if (!plat_dev) {
		DDPPR_ERR("%s of_find_device_by_node open fail\n", __func__);
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}

	mml_pdev = mml_get_plat_device(plat_dev);
	if (!mml_pdev) {
		DDPPR_ERR("mtk_drm_ioctl_mml_gem_submit mml_get_plat_device open fail\n");
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}

	disp_param.dual = mtk_crtc->is_dual_pipe;
	disp_param.racing_height = 64;
	disp_param.vdo_mode =  (!mtk_crtc_is_frame_trigger_mode(crtc));
	disp_param.submit_cb = mtk_drm_mmlsys_submit_done_cb;
	disp_param.ddren_cb = mtk_drm_mmlsys_ddren_cb;
	disp_param.kick_idle_cb = mtk_drm_mmlsys_kick_idle_cb;
	disp_param.disp_crtc = (void *)crtc;

	mml_ctx = mml_drm_get_context(mml_pdev, &disp_param);
	if (IS_ERR_OR_NULL(mml_ctx)) {
		DDPPR_ERR("mml_drm_get_context fail. mml_ctx:%p\n", mml_ctx);
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}
	priv->mml_ctx = mml_ctx;
	DDPMSG("%s 2 0x%lx", __func__, (unsigned long)priv->mml_ctx);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && (mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI)) {
		u32 panel_w = mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_VIRTUAL_WIDTH, NULL);
		u32 panel_h = mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_VIRTUAL_HEIGH, NULL);

		if (panel_w) {
			mml_drm_set_panel_pixel(mml_ctx, panel_w, panel_h);
			DDPMSG("%s set panel pixels %ux%u\n", __func__, panel_w, panel_h);
		}
	}

	return priv->mml_ctx;

err_handle_mtk_drm_get_mml_drm_ctx:
	priv->mml_ctx = NULL;
	return priv->mml_ctx;
}

static void mtk_drm_init_dummy_table(struct mtk_drm_private *priv)
{
	struct dummy_mapping *table;
	size_t size;
	int i;

	switch (priv->data->mmsys_id) {
	case MMSYS_MT6983:
		table = mt6983_dispsys_dummy_register;
		size = MT6983_DUMMY_REG_CNT;
		break;
	case MMSYS_MT6895:
		/* mt6895 same with mt6983 */
		table = mt6983_dispsys_dummy_register;
		size = MT6983_DUMMY_REG_CNT;
		break;
	case MMSYS_MT6879:
		table = mt6879_dispsys_dummy_register;
		size = MT6879_DUMMY_REG_CNT;
		break;
	default:
		DDPMSG("no dummy table\n");
		return;
	}

	for (i = 0; i < size; i++) {
		if (table[i].comp_id == DDP_COMPONENT_ID_MAX) {
			if (priv->data->mmsys_id == MMSYS_MT6879) {
				//MT6879 can't use dispsys dummy reg, so change to use mutex
				struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

				table[i].pa_addr = ddp->regs_pa;
				table[i].addr = ddp->regs;
			} else {
				table[i].pa_addr = priv->config_regs_pa;
				table[i].addr = priv->config_regs;
			}
		/* TODO: BWM just for Dujac SW readyGo, Into SQC need to del and use new method */
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(29))) {
			struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

			table[i].pa_addr = ddp->regs_pa;
			table[i].addr = ddp->regs;
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(30))) {
			struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

			table[i].pa_addr = ddp->side_regs_pa;
			table[i].addr = ddp->side_regs;
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(31))) {
			table[i].pa_addr = priv->side_config_regs_pa;
			table[i].addr = priv->side_config_regs;
		} else {
			struct mtk_ddp_comp *comp;

			comp = priv->ddp_comp[table[i].comp_id];
			table[i].pa_addr = comp->regs_pa;
			table[i].addr = comp->regs;
		}
	}
}

static int mtk_drm_init_emi_eff_table(struct drm_device *drm_dev)
{
	struct device *dev = drm_dev->dev;
	unsigned int dram_type = 0;
	int ret = -ENODEV;

	dram_type = mtk_dramc_get_ddr_type();
	if ((dram_type == TYPE_LPDDR4) ||
		(dram_type == TYPE_LPDDR4X) ||
		(dram_type == TYPE_LPDDR4P)) {
		ret = of_property_read_u32(dev->of_node, "default-emi-eff", &default_emi_eff);
		ret = of_property_read_u32_array(dev->of_node, "emi-eff-lp4-table",
				&emi_eff_tb[0], MAX_EMI_EFF_LEVEL);
		if (ret == 0)
			DDPMSG("%s use LP4 emi eff and table\n", __func__);

		return ret;

	} else if ((dram_type == TYPE_LPDDR5) ||
		(dram_type == TYPE_LPDDR5X)) {
		ret = of_property_read_u32(dev->of_node, "default-emi-eff", &default_emi_eff);
		ret = of_property_read_u32_array(dev->of_node, "emi-eff-lp5-table",
				&emi_eff_tb[0], MAX_EMI_EFF_LEVEL);
		if (ret == 0)
			DDPMSG("%s use LP5 emi eff and table\n", __func__);
		return ret;

	} else {
		DDPMSG("%s use default emi eff and table\n", __func__);
		return 0;
	}
}

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static int mtk_drm_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	struct mtk_drm_kernel_pm *kernel_pm = container_of(notifier, typeof(*kernel_pm), nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		DDPMSG("Disabling CRTC wakelock\n");
		atomic_set(&kernel_pm->status, KERNEL_PM_SUSPEND);
		wake_up_interruptible(&kernel_pm->wq);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		atomic_set(&kernel_pm->status, KERNEL_PM_RESUME);
		wake_up_interruptible(&kernel_pm->wq);
		DDPINFO("%s status(%d)\n", __func__, atomic_read(&kernel_pm->status));
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}
#else
static int mtk_drm_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	struct mtk_drm_kernel_pm *kernel_pm = container_of(notifier, typeof(*kernel_pm), nb);

	DDPMSG("%s pm_event %d set pm status(%d)\n",
	       __func__, pm_event, atomic_read(&kernel_pm->status));

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		wake_up_interruptible(&kernel_pm->wq);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		wake_up_interruptible(&kernel_pm->wq);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}
#endif

static void mtk_drm_kms_lateinit(struct kthread_work *work)
{
	struct lateinit_task *task = container_of(work, struct lateinit_task, work);
	struct mtk_drm_private *private = container_of(task, struct mtk_drm_private, lateinit);
	struct drm_device *drm = private->drm;

#ifdef CONFIG_DRM_MEDIATEK_DEBUG_FS
	mtk_drm_debugfs_init(drm, private);
#endif
	disp_dbg_init(drm);
	PanelMaster_Init(drm);

	if (private->dpc_dev)
		mtk_vidle_wait_init();

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_MMDVFS_SUPPORT))
		mtk_drm_mmdvfs_init(drm->dev);
	mtk_drm_init_dummy_table(private);

	/* Load emi efficiency table for ovl bandwidht monitor */
	if (private->data->need_emi_eff)
		mtk_drm_init_emi_eff_table(drm);

	mtk_drm_first_enable(drm);

	/* power off mtcmos */
	/* Because of align lk hw power status,
	 * we power on mtcmos at the beginning of the display initialization.
	 * We power off mtcmos at the end of the display initialization.
	 * Here we only decrease ref count, the power will hold on.
	 */
	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		mtk_drm_top_clk_disable_unprepare(drm);

	/*
	 * When kernel init, SMI larb will get once for keeping
	 * MTCMOS on. Then, this keeping will be released after
	 * display keep MTCMOS by itself.
	 * Trigger mminfra gipc to hfrp.
	 */
	mtk_smi_init_power_off();
	DDPFUNC("mtk_smi_init_power_off\n");
	mtk_mminfra_off_gipc();
	mtk_dump_mminfra_ck(private);

#ifndef DRM_OVL_SELF_PATTERN
	mtk_drm_assert_init(drm);
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	mtk_drm_crtc_dev_init(drm);
#endif

	if (is_bdg_supported())
		bdg_first_init();
}

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	struct device *dma_dev;
	int ret, i;
	u32 condition_num;
	u32 crtc_path_type, bypass_pc;
	const struct mtk_crtc_path_data *path_data = NULL;

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_USE_M4U)) {
		if (!iommu_present(&platform_bus_type)) {
			DDPINFO("%s, iommu not ready\n", __func__);
			return -EPROBE_DEFER;
		}
	}

	DDPINFO("%s+\n", __func__);

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %s\n",
			private->mutex_node->full_name);
		of_node_put(private->mutex_node);
		return -EPROBE_DEFER;
	}

	private->mutex_dev = &pdev->dev;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 1;
	drm->mode_config.min_height = 1;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &mtk_drm_mode_config_funcs;

	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_config_cleanup;

	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0)
		goto err_component_unbind;
	/*
	 * We currently support two fixed data streams, each optional,
	 * and each statically assigned to a crtc:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 */
	/* TODO: temprorary solution for MT6989 enable full OVL path,*/
	/* remove it after MT6989 MML DLO switch ready */
	if (of_property_read_bool(private->mmsys_dev->of_node, "enable-main-full-ovl-path"))
		path_data = private->data->ext_alter_path_data;
	else {
		int ret_pc, ret_dual;

		ret_pc = of_property_read_u32(private->mmsys_dev->of_node,
			"bypass-pc-path", &bypass_pc);

		ret_dual = of_property_read_u32_index(private->mmsys_dev->of_node, "crtc-path-type",
			0, &crtc_path_type);

		if (bypass_pc && crtc_path_type)
			path_data = private->data->main_bypass_pc_dual_path_data;
		else if (bypass_pc)
			path_data = private->data->main_bypass_pc_path_data;
		else if (crtc_path_type)
			path_data = private->data->main_dual_path_data;
		else
			path_data = private->data->main_path_data;

		DDPMSG("%s path_type[%u] bypass_pc[%u]", __func__, crtc_path_type, bypass_pc);
	}

	ret = mtk_drm_crtc_create(drm, path_data);

	DDPMSG("%s crtc_path_type[%u]", __func__, crtc_path_type);

	if (ret < 0)
		goto err_component_unbind;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		/* ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0. */
		if (of_property_read_bool(private->mmsys_dev->of_node, "enable_ext_alter_path"))
			ret = mtk_drm_crtc_create(drm, private->data->ext_alter_path_data);
		else
			ret = mtk_drm_crtc_create(drm, private->data->ext_path_data);
		if (ret < 0)
			goto err_component_unbind;

		if (of_property_read_u32(private->mmsys_dev->of_node,
			"condition-num", &condition_num)) {
			DDPINFO("CRTC2 condition_num is 0, NO condition_num in dts\n");
			ret = mtk_drm_crtc_create(drm, private->data->third_path_data);
		} else {
			DDPDBG("CRTC2 condition_num is %d\n", condition_num);
			if (condition_num == 2)
				ret = mtk_drm_crtc_create(drm,
					private->data->third_path_data_wo_tdshp);
			else
				ret = mtk_drm_crtc_create(drm, private->data->third_path_data);
		}
		if (ret < 0)
			goto err_component_unbind;
		/*TODO: Need to check path rule*/
		if (private->data->fourth_path_data_secondary
			|| private->data->fourth_path_data_discrete) {
			DDPMSG("CRTC3 Path\n");
			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable-secondary-path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->fourth_path_data_secondary);
			if (ret < 0)
				goto err_component_unbind;

			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable-discrete-path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->fourth_path_data_discrete);
			if (ret < 0)
				goto err_component_unbind;
		}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (private->data->fifth_path_data) {
			DDPMSG("CRTC4 Path\n");
			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable-fifth-path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->fifth_path_data);
			if (ret < 0)
				goto err_component_unbind;
		}

		if (private->data->sixth_path_data) {
			DDPMSG("CRTC5 Path\n");
			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable-sixth-path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->sixth_path_data);
			if (ret < 0)
				goto err_component_unbind;
		}

		if (private->data->seventh_path_data) {
			DDPMSG("CRTC6 Path\n");
			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable-seventh-path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->seventh_path_data);
			if (ret < 0)
				goto err_component_unbind;
		}
#endif
	}
	/* TODO: allow_fb_modifiers = 1 and format_modifiers = null make drm_warn_on.
	 * so we set allow_fb_modifiers = 1 after mtk_plane_init
	 */
	//drm->mode_config.allow_fb_modifiers = true;
	//drm->mode_config.fb_modifiers_not_supported = true;
	drm->mode_config.fb_modifiers_not_supported = false;

	/* Use OVL device for all DMA memory allocations */
	if (private->data->main_path_data->ovl_path_len[DDP_MAJOR][0] > 0)
		np = private->comp_node[private->data->main_path_data
					->ovl_path[DDP_MAJOR][0][0]];
	else
		np = private->comp_node[private->data->main_path_data
					->path[DDP_MAJOR][0][0]];
	if (np == NULL) {
		if (of_property_read_bool(private->mmsys_dev->of_node, "enable_ext_alter_path"))
			np = private->comp_node[private->data->ext_alter_path_data
						->path[DDP_MAJOR][0][0]];
		else
			np = private->comp_node[private->data->ext_path_data
						->path[DDP_MAJOR][0][0]];
	}
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	private->dma_dev = &pdev->dev;

	/*
	 * Configure the DMA segment size to make sure we get contiguous IOVA
	 * when importing PRIME buffers.
	 */
	dma_dev = drm->dev;
	if (!dma_dev->dma_parms) {
		private->dma_parms_allocated = true;
		dma_dev->dma_parms =
			devm_kzalloc(drm->dev, sizeof(*dma_dev->dma_parms),
				     GFP_KERNEL);
	}
	if (!dma_dev->dma_parms) {
		ret = -ENOMEM;
		goto put_dma_dev;
	}

	ret = dma_set_max_seg_size(dma_dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dma_dev, "Failed to set DMA segment size\n");
		goto err_unset_dma_parms;
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	drm_kms_helper_poll_init(drm);
#endif
	drm_mode_config_reset(drm);

	INIT_WORK(&private->unreference.work, mtk_unreference_work);
	INIT_LIST_HEAD(&private->unreference.list);
	INIT_LIST_HEAD(&private->lyeblob_head);
	spin_lock_init(&private->unreference.lock);
	mutex_init(&private->commit.lock);
	mutex_init(&private->lyeblob_list_mutex);

	init_waitqueue_head(&private->repaint_data.wq);
	INIT_LIST_HEAD(&private->repaint_data.job_queue);
	INIT_LIST_HEAD(&private->repaint_data.job_pool);
	for (i = 0; i < MAX_CRTC ; ++i) {
		atomic_set(&private->crtc_present[i], 0);
		atomic_set(&private->crtc_rel_present[i], 0);
	}
	atomic_set(&private->rollback_all, 0);
	mtk_drm_svp_init(drm);

	DDPINFO("%s-\n", __func__);

	private->lateinit.worker = kthread_create_worker(0, "mtk_drm_bind");
	kthread_init_work(&private->lateinit.work, mtk_drm_kms_lateinit);
	kthread_queue_work(private->lateinit.worker, &private->lateinit.work);

	return 0;

err_unset_dma_parms:
	if (private->dma_parms_allocated)
		dma_dev->dma_parms = NULL;
put_dma_dev:
	put_device(drm->dev);
err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;

	drm_kms_helper_poll_fini(drm);

	if (private->dma_parms_allocated)
		drm->dev->dma_parms = NULL;

	//drm_vblank_cleanup(drm);
	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);

	disp_dbg_deinit();
	PanelMaster_Deinit();

	if (private->mml_ctx) {
		mml_drm_kick_done(private->mml_ctx);
		mml_drm_put_context(private->mml_ctx);
		private->mml_ctx = NULL;
	}
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
static void mtk_drm_se_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

static int mtk_drm_se_plane_config(struct mtk_drm_crtc *mtk_crtc)
{
	int index = drm_crtc_index(&mtk_crtc->base);
	struct mtk_cmdq_cb_data *cb_data;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp;
	int i = 0, ret;
	struct mtk_plane_state tmp_state;
	struct mtk_plane_comp_state *t_s = &tmp_state.comp_state;

	DDPINFO("%s line:%d", __func__, __LINE__);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPMSG("%s cb data creation failed\n", __func__);
		return -1;
	}

	cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cb_data->cmdq_handle = cmdq_handle;
	cb_data->crtc = &mtk_crtc->base;

	//disable panel layers
	if (mtk_crtc->se_state == DISP_SE_START) {
		if (mtk_crtc->se_panel & (1 << MTK_PANEL_DP0)) {
			t_s->comp_id = DDP_COMPONENT_OVL2_2L;
			t_s->lye_id =  1;
			t_s->ext_lye_id = 0;
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base, &tmp_state);
			mtk_ddp_comp_layer_off(comp, t_s->lye_id, 0, cmdq_handle);
		} else if (mtk_crtc->se_panel & (1 << MTK_PANEL_DP1)) {
			//if (mtk_dpi_get_split_count() > 1) {
			//	t_s->comp_id = DDP_COMPONENT_OVL2_2L;
			//	t_s->lye_id =  0;
			//	t_s->ext_lye_id = 0;
			//	comp = mtk_crtc_get_plane_comp(&mtk_crtc->base, &tmp_state);
			//	mtk_ddp_comp_layer_off(comp, t_s->lye_id, 0, cmdq_handle);
			//} else {
			//	mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle, 0);
			//}
		} //else
			//mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle);
		mtk_crtc->se_state = DISP_SE_RUNNING;
	}

	for (i = 0; i < MTK_FB_SE_NUM; i++) {
		DDPINFO("%s %d mtk_crtc->se_plane[%d].state.comp_state.comp_id:%d", __func__, __LINE__,
			i, mtk_crtc->se_plane[i].state.comp_state.comp_id);
		if (mtk_crtc->se_plane[i].panel_id >= 0 &&
		    mtk_crtc->se_plane[i].state.comp_state.comp_id != 0) {
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base,
				&mtk_crtc->se_plane[i].state);
			DDPINFO("se crtc%d i%d comp%d,layer%d,size(%d %d %d %d)addr0x%lx\n",
				index, i, mtk_crtc->se_plane[i].state.comp_state.comp_id,
				mtk_crtc->se_plane[i].state.comp_state.lye_id,
				mtk_crtc->se_plane[i].state.pending.dst_x,
				mtk_crtc->se_plane[i].state.pending.dst_y,
				mtk_crtc->se_plane[i].state.pending.width,
				mtk_crtc->se_plane[i].state.pending.height,
				mtk_crtc->se_plane[i].state.pending.addr);
			//pts
			if (mtk_crtc->se_plane[i].state.pending.pts != 0) {
				DDPINFO("LATENCY_TEST %s t=%lld", __func__,
					mtk_crtc->se_plane[i].state.pending.pts);
			}
			if (mtk_crtc->se_plane[i].state.pending.enable)
				mtk_ddp_comp_layer_config(comp, 0,
					&mtk_crtc->se_plane[i].state,
					cmdq_handle);
			else {
				mtk_ddp_comp_layer_off(comp,
					mtk_crtc->se_plane[i].state.comp_state.lye_id,
					mtk_crtc->se_plane[i].state.comp_state.ext_lye_id,
					cmdq_handle);
				mtk_crtc->se_plane[i].panel_id = -1;
			}
		}
	}

	if (mtk_crtc->se_state == DISP_SE_STOP) {
		mtk_crtc->se_panel = 0;
		for (i = 0; i < mtk_crtc->static_plane.index; i++) {
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base,
				&mtk_crtc->static_plane.state[i]);
			DDPMSG("%s: back i %d, comp %d %d layer %d %d\n", __func__, i, comp->id,
				mtk_crtc->static_plane.state[i].comp_state.comp_id,
				mtk_crtc->static_plane.state[i].comp_state.lye_id,
				mtk_crtc->static_plane.state[i].comp_state.ext_lye_id);
			if (mtk_crtc->static_plane.state[i].pending.enable)
				mtk_ddp_comp_layer_config(comp, 0,
					&mtk_crtc->static_plane.state[i],
					cmdq_handle);
		}
		mtk_crtc->se_state = DISP_SE_STOPPED;
	}

	ret = cmdq_pkt_flush_threaded(cmdq_handle, mtk_drm_se_cmdq_cb, cb_data);
	if (ret < 0) {
		DDPMSG("%s[%d] cmdq_pkt_flush_threaded failed!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (mtk_crtc->se_state == DISP_SE_STOPPED) {
		for (i = 0; i < MTK_FB_SE_NUM; i++) {
			memset((void *)&mtk_crtc->se_plane[i].state, 0,
				sizeof(struct mtk_plane_state));
			mtk_crtc->se_plane[i].panel_id = -1;
		}
	}

	return 0;
}

static int mtk_drm_set_ovl_layer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_plane_state *state;
	struct mtk_drm_layer_info *layer_info = (struct mtk_drm_layer_info *)data;
	struct mtk_drm_private *private = dev->dev_private;
	int i = 0, enable_cnt = 0, ret = -1;
	struct mtk_panel_params *params;
	int index = 0, crtc_id = 0;
	struct mtk_crtc_se_plane *se_plane;
	u64 sys_time;
	struct mtk_ddp_comp *comp = NULL;
#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
	static bool ultrarvc_start = true;
#endif

	unsigned int layer_id = 0;

	sys_time = ktime_to_ns(ktime_get_boottime());
	if(layer_info->pts != 0)
		DDPINFO("LATENCY_TEST %s %lld sys_time %lld\n",
			__func__, layer_info->pts, sys_time);

	crtc_id = mtk_drm_get_crtc_id(layer_info->panel_id, private);
	if (crtc_id >= MAX_CRTC) {
		DDPMSG("the crtc_id %d >= max %d\n", crtc_id, MAX_CRTC);
		return -1;
	}

	DDPINFO("%s crtc%d L:%d\n", __func__, crtc_id, __LINE__);

	if (layer_info->layer_id >= MTK_FB_SE_NUM) {
		DDPMSG("%s: panel:%d invalid layer id:%d\n", __func__,
			layer_info->panel_id, layer_info->layer_id);
		return -EINVAL;
	}

	if ((layer_info->panel_id == MTK_PANEL_DP0 ||
	    layer_info->panel_id == MTK_PANEL_DP1) &&
	    layer_info->layer_id >= 1) {
		DDPMSG("%s: panel:%d invalid layer id:%d\n", __func__,
			layer_info->panel_id, layer_info->layer_id);
		return -EINVAL;
	}

	mtk_crtc = to_mtk_crtc(private->crtc[crtc_id]);

	mutex_lock(&se_lock);
	ret = mtk_drm_se_enable(dev, mtk_crtc);
	mutex_unlock(&se_lock);
	if (ret < 0) {
		DDPMSG("%s connector is not ready, stop config\n", __func__);
		return ret;
	}

	index = drm_crtc_index(&mtk_crtc->base);

	DDP_MUTEX_LOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);

	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);
		DDPMSG("crtc%d still disable\n", index);
		return 0;
	}

	if (mtk_crtc->se_state != DISP_SE_START &&
	    mtk_crtc->se_state != DISP_SE_RUNNING &&
	    !layer_info->layer_en) {
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);
		DDPMSG("se still stop, not config\n");
		return 0;
	}

	if (layer_info->user_type == MTK_USER_NORMAL)
		layer_id = mtk_crtc->layer_nr - 1;
	else
		layer_id = layer_info->layer_id;

	params = mtk_drm_get_lcm_ext_params(&mtk_crtc->base);

	if (layer_info->layer_en)
		mtk_crtc->se_panel |= 1 << layer_info->panel_id;

	se_plane = &mtk_crtc->se_plane[layer_id];
	se_plane->panel_id = layer_info->panel_id;
	state = &se_plane->state;
	state->pending.enable = layer_info->layer_en;
	state->pending.src_x = layer_info->src_x;
	state->pending.src_y = layer_info->src_y;
	state->pending.dst_x = layer_info->tgt_x;
	state->pending.dst_y = layer_info->tgt_y;
	state->pending.width = layer_info->tgt_w;
	state->pending.height = layer_info->tgt_h;
	state->pending.format = layer_info->src_format;
	state->pending.pitch = layer_info->src_pitch;
	state->pending.modifier  = 1;
	state->pending.is_sec = 0;
	state->pending.addr = (dma_addr_t)layer_info->src_mvaddr;

	state->pending.prop_val[PLANE_PROP_COMPRESS] = layer_info->compress;
	state->base.alpha = 0xff << 8;

	DDPINFO("%s line:%d panel_id:%d en:%d, (%d %d %d %d w%d h%d) fmt:%d picth:%d addr:0x%llx, compress:%llu",
		__func__, __LINE__, se_plane->panel_id, state->pending.enable, state->pending.src_x,
		state->pending.src_y, state->pending.dst_x, state->pending.dst_y, state->pending.width,
		state->pending.height, state->pending.format, state->pending.pitch, state->pending.addr,
		state->pending.prop_val[PLANE_PROP_COMPRESS]);

	state->pending.dirty = true;
	state->comp_state.lye_id = layer_id;

	state->comp_state.ext_lye_id = LYE_NORMAL;
	state->pending.pts = layer_info->pts;

	comp = mtk_crtc_get_comp_with_index(mtk_crtc, state);
	if (!comp) {
		DDPMSG("%s invalid comp\n", __func__);
		return -EINVAL;
	}

	state->comp_state.comp_id = comp->id;
	state->comp_state.lye_id = to_crtc_plane_index(layer_info->layer_id);

#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
		/*{ultrarvc +*/
		if ((layer_info->panel_id == MTK_PANEL_DSI0_0) &&
			(layer_info->user_type == MTK_USER_AVM)) {
			comp = mtk_crtc_get_plane_comp_by_id(&mtk_crtc->base, DDP_COMPONENT_OVL0);
			if (is_ultrarvc_enable(comp)) {
				state->comp_state.comp_id = DDP_COMPONENT_OVL0;
#if (CONFIG_MTK_MULTI_DSI_PATH == 2)
				state->comp_state.lye_id = 1;
#else
				state->comp_state.lye_id = 3;
#endif
			}
		}
		/*ultrarvc -}*/
#endif

	if (mtk_crtc->sideband_layer >= 0)
		state->comp_state.lye_id = mtk_crtc->sideband_layer;

	for (i = 0; i < MTK_FB_SE_NUM; i++) {
		if (mtk_crtc->se_plane[i].state.pending.enable) {
			enable_cnt++;
			break;
		}
	}

#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
		if (ultrarvc_start && (layer_info->user_type == MTK_USER_AVM) &&
			(mtk_crtc->se_state == DISP_SE_RUNNING)) {
			if (mtk_cam_fe_send_ipc_msg(MTK_CAM_FE_IPC_OPEN, CMD_RESPOND_NEEDED,
				NULL, 0) == SUCCESS) {
				ultrarvc_start = false;
				mtk_cam_fe_send_ipc_msg(MTK_CAM_FE_IPC_REVERSE,
					CMD_RESPOND_NEEDED, &ultrarvc_start, sizeof(bool));
				DDPMSG("disable ultrarvc\n");
			} else
				DDPMSG("open cam be failed\n");
		}
#endif

	DDPINFO("crtc%d panel%d comp_id%d layer_id%d\n", index,
		layer_info->panel_id, state->comp_state.comp_id, state->comp_state.lye_id);

	if ((mtk_crtc->se_state != DISP_SE_START) &&
		(mtk_crtc->se_state != DISP_SE_RUNNING) &&
		(enable_cnt))  {
		DDPMSG("crtc%d se first start\n", index);
		mtk_crtc->se_state = DISP_SE_START;
	}

	if (!enable_cnt) {
		DDPMSG("crtc%d se stop\n", index);
		mtk_crtc->se_state = DISP_SE_STOP;
	}

	mtk_drm_se_plane_config(mtk_crtc);

	DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);

	return 0;
}

static int mtk_drm_map_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct mtk_drm_dma_buf *dma_map = (struct mtk_drm_dma_buf *)data;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_se_dma_map *map_list;
	dma_addr_t mva;

	dma_map->mva = 0;

	if (dma_map->fd <= 0)
		return -1;

	map_list = kmalloc(sizeof(struct mtk_se_dma_map), GFP_KERNEL);
	if (!map_list) {
		DDPMSG("%s alloc fail\n", __func__);
		return -1;
	}

	map_list->fd = dma_map->fd;

	map_list->dmabuf = dma_buf_get(dma_map->fd);
	if (IS_ERR(map_list->dmabuf)) {
		DDPMSG("%s:%d error! hnd:0x%p, fd:%d\n",
				__func__, __LINE__, map_list->dmabuf, dma_map->fd);
		goto release;
	}
	map_list->attach = dma_buf_attach(map_list->dmabuf, private->dma_dev);
	if (IS_ERR(map_list->attach)) {
		DDPMSG("%s:%d error! attach:0x%p, fd:%d\n",
				__func__, __LINE__, map_list->attach, dma_map->fd);
		goto fail_get;
	}

	map_list->sgt = dma_buf_map_attachment(map_list->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(map_list->sgt))
		goto fail_detach;

	mva = sg_dma_address(map_list->sgt->sgl);

	dma_map->mva = mva;

	DDPINFO("dma fd is %d mva 0x%lx\n", dma_map->fd, dma_map->mva);

	list_add_tail(&map_list->list, &dma_map_list.list);

	return 0;

fail_detach:
	dma_buf_detach(map_list->dmabuf, map_list->attach);
fail_get:
	dma_buf_put(map_list->dmabuf);
release:
	kfree(map_list);
	map_list = NULL;
	return -1;
}

static int mtk_drm_unmap_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct mtk_se_dma_map *map_list = NULL;
	struct mtk_se_dma_map *n = NULL;
	int *fd = (int *)data;

	list_for_each_entry_safe(map_list, n, &dma_map_list.list, list) {
		if (*fd == map_list->fd) {
			DDPINFO("dma fd is %d\n", *fd);
			list_del_init(&map_list->list);
			dma_buf_unmap_attachment(map_list->attach, map_list->sgt,
				DMA_BIDIRECTIONAL);
			dma_buf_detach(map_list->dmabuf, map_list->attach);
			dma_buf_put(map_list->dmabuf);

			kfree(map_list);
			map_list = NULL;
			break;
		}
	}

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
int mtk_drm_fm_lcm_auto_test(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	int *result = data;
	int ret = 0;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
			typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled || mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDPINFO("crtc 0 is already sleep, skip\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	private = dev->dev_private;
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR)) {
		mtk_drm_set_idlemgr(crtc, 0, 0);
	}

	ret = mtk_crtc_lcm_ATA(crtc);

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR))
		mtk_drm_set_idlemgr(crtc, 1, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* ret = 0 fail, ret = 1 pass */
	if (ret == 0) {
		DDPPR_ERR("ATA LCM failed\n");
		*result = 0;
	} else {
		DDPMSG("ATA LCM passed\n");
		*result = 1;
	}
	return 0;
}
#endif

int mtk_drm_get_mml_mode_caps(void)
{
	return mml_mode_caps;
}
int mtk_drm_get_mml_hw_caps(void)
{
	return mml_hw_caps;
}

void mtk_drm_set_mml_caps(int hw_caps, int mode_caps)
{
	mml_hw_caps = hw_caps;
	mml_mode_caps = mode_caps;
	DDPMSG("%s,hw:0x%x,mode:0x%x", __func__, mml_hw_caps, mml_mode_caps);
}

static int mtk_drm_mml_ctrl_caps(struct mtk_drm_mml_caps_info *mml_caps, struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct platform_device *plat_dev;
	struct platform_device *mml_pdev;
	u32 mode_caps = 0;
	int ret;

	plat_dev = of_find_device_by_node(priv->mutex_node);
	DDPFUNC("platdev %p", plat_dev);
	if (!plat_dev) {
		DDPMSG("%s of_find_device_by_node open fail\n", __func__);
		return -EINVAL;
	}

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761 ||
		priv->data->mmsys_id == MMSYS_MT6781 ||
		priv->data->mmsys_id == MMSYS_MT6853 ||
		priv->data->mmsys_id == MMSYS_MT6885 ||
		priv->data->mmsys_id == MMSYS_MT6833 ||
		priv->data->mmsys_id == MMSYS_MT6877) {
		mml_caps->mode_caps = MTK_MML_DISP_MDP_LAYER;
		return 0;
	}

	mml_pdev = mml_get_plat_device(plat_dev);
	if (!mml_pdev) {
		DDPMSG("%s mml_get_plat_device open fail\n", __func__);
		return -ENXIO;
	}

	ret = mml_drm_get_hw_caps(&mode_caps, &mml_caps->hw_caps);
	if (ret < 0)
		return ret;

	mml_caps->mode_caps = 0;
	if (mode_caps & BIT(MML_MODE_DIRECT_LINK))
		mml_caps->mode_caps |= MTK_MML_DISP_DECOUPLE_LAYER;
	if (mode_caps & BIT(MML_MODE_RACING))
		mml_caps->mode_caps |= MTK_MML_DISP_DIRECT_DECOUPLE_LAYER;
	if (mode_caps & BIT(MML_MODE_MML_DECOUPLE))
		mml_caps->mode_caps |= MTK_MML_DISP_DECOUPLE_LAYER;
	if (mode_caps & BIT(MML_MODE_MML_DECOUPLE2))
		mml_caps->mode_caps |= MTK_MML_DISP_DECOUPLE2_LAYER;
	if (mode_caps & BIT(MML_MODE_MDP_DECOUPLE))
		mml_caps->mode_caps |= MTK_MML_DISP_MDP_LAYER;

	mtk_drm_set_mml_caps(mml_caps->hw_caps, mml_caps->mode_caps);

	return 0;
}

static int mtk_drm_mml_ctrl_query_hw_support(struct mtk_drm_mml_query_hw_support *query)
{
	struct mml_frame_info info;

	if (copy_from_user(&info, query->info, sizeof(info))) {
		DDPINFO("%s copy_from_user mml frame info failed\n", __func__);
		return -EINVAL;
	}

	query->support = mml_drm_query_hw_support(&info);

	return 0;
}

int mtk_drm_ioctl_mml_ctrl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct mtk_drm_mml_ctrl *mml_ctrl = data;
	int ret;

	switch (mml_ctrl->func) {
	case mtk_drm_mml_func_get_caps:
		ret = mtk_drm_mml_ctrl_caps(&mml_ctrl->caps, dev);
		break;
	case mtk_drm_mml_func_query_hw_support:
		ret = mtk_drm_mml_ctrl_query_hw_support(&mml_ctrl->query);
		break;
	default:
		DDPMSG("%s wrong func %u\n", __func__, mml_ctrl->func);
		ret = -EINVAL;
		break;
	};

	return ret;
}

static const struct drm_ioctl_desc mtk_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MTK_GEM_CREATE, mtk_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GEM_MAP_OFFSET, mtk_gem_map_offset_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GEM_SUBMIT, mtk_gem_submit_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SESSION_CREATE, mtk_drm_session_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SESSION_DESTROY, mtk_drm_session_destroy_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_LAYERING_RULE, mtk_layering_rule_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_GETFENCE, mtk_drm_crtc_getfence_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_FENCE_REL, mtk_drm_crtc_fence_release_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_GETSFFENCE,
			  mtk_drm_crtc_get_sf_fence_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SET_MSYNC_PARAMS,
			  mtk_drm_set_msync_params_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_MSYNC_PARAMS,
			  mtk_drm_get_msync_params_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_WAIT_REPAINT, mtk_drm_wait_repaint_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_DISPLAY_CAPS, mtk_drm_get_display_caps_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SET_DDP_MODE, mtk_drm_set_ddp_mode,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_SESSION_INFO, mtk_drm_get_info_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_MASTER_INFO, mtk_drm_get_master_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SUPPORT_COLOR_TRANSFORM,
				mtk_drm_ioctl_ccorr_support_color_matrix,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_LCM_INDEX, mtk_drm_ioctl_get_lcm_index,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_PANELS_INFO, mtk_drm_ioctl_get_all_connector_panel_info,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_GET_DEV_INFO, mtk_drm_dp_get_dev_info,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_AUDIO_ENABLE, mtk_drm_dp_audio_enable,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_AUDIO_CONFIG, mtk_drm_dp_audio_config,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_GET_CAPABILITY, mtk_drm_dp_get_cap,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_MML_GEM_SUBMIT, mtk_drm_ioctl_mml_gem_submit,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_CHIST, mtk_drm_ioctl_chist_get_hist,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_CHIST_CAPS, mtk_drm_ioctl_chist_get_caps,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_CHIST_CONFIG, mtk_drm_ioctl_chist_set_config,
			  DRM_UNLOCKED),
#if IS_ENABLED(CONFIG_DEBUG_FS)
	DRM_IOCTL_DEF_DRV(MTK_FACTORY_LCM_AUTO_TEST, mtk_drm_fm_lcm_auto_test,
			  DRM_UNLOCKED),
#endif
	DRM_IOCTL_DEF_DRV(MTK_GET_PQ_CAPS, mtk_drm_ioctl_ccorr_get_pq_caps,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DRM_SET_LEASE_INFO, mtk_drm_set_lease_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DRM_GET_LEASE_INFO, mtk_drm_get_lease_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_ODDMR_LOAD_PARAM, mtk_drm_ioctl_oddmr_load_param,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_ODDMR_CTL, mtk_drm_ioctl_oddmr_ctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_KICK_IDLE, mtk_drm_ioctl_kick_idle,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_PQ_FRAME_CONFIG, mtk_drm_ioctl_pq_frame_config,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_MODE_EXT_INFO, mtk_drm_get_mode_ext_info_ioctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_PQ_PROXY_IOCTL, mtk_drm_ioctl_pq_proxy,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HWVSYNC_ON, mtk_drm_hwvsync_on_ioctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DUMMY_CMD_ON, mtk_drm_dummy_cmd_on_ioctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_ESD_STAT_CHK, mtk_drm_esd_recovery_check_ioctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_MML_CTRL, mtk_drm_ioctl_mml_ctrl, DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DEBUG_LOG, mtk_disp_ioctl_debug_log_switch,
					DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SEC_HND_TO_GEM_HND, mtk_drm_sec_hnd_to_gem_hnd,
			DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	DRM_IOCTL_DEF_DRV(MTK_SET_OVL_LAYER, mtk_drm_set_ovl_layer,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_MAP_DMA_BUF, mtk_drm_map_dma_buf,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_UNMAP_DMA_BUF, mtk_drm_unmap_dma_buf,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
#endif
};

#if IS_ENABLED(CONFIG_COMPAT)
struct drm_ioctl32_desc {
	drm_ioctl_compat_t *fn;
	char *name;
};

static const struct drm_ioctl32_desc mtk_compat_ioctls[] = {
#define DRM_IOCTL32_DEF_DRV(n, f)[DRM_##n] = {.fn = f, .name = #n}
	DRM_IOCTL32_DEF_DRV(MTK_GEM_CREATE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GEM_MAP_OFFSET, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GEM_SUBMIT, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SESSION_CREATE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SESSION_DESTROY, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_LAYERING_RULE, mtk_layering_rule_ioctl_compat),
	DRM_IOCTL32_DEF_DRV(MTK_CRTC_GETFENCE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_CRTC_FENCE_REL, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_CRTC_GETSFFENCE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SET_MSYNC_PARAMS, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_MSYNC_PARAMS, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_WAIT_REPAINT, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_DISPLAY_CAPS, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SET_DDP_MODE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_SESSION_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_MASTER_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SUPPORT_COLOR_TRANSFORM, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_LCM_INDEX, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_LCM_INDEX, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_HDMI_GET_DEV_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_HDMI_AUDIO_ENABLE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_HDMI_AUDIO_CONFIG, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_HDMI_GET_CAPABILITY, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_MML_GEM_SUBMIT, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_CHIST, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_CHIST_CAPS, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SET_CHIST_CONFIG, NULL),
#if IS_ENABLED(CONFIG_DEBUG_FS)
	DRM_IOCTL32_DEF_DRV(MTK_FACTORY_LCM_AUTO_TEST, NULL),
#endif
	DRM_IOCTL32_DEF_DRV(MTK_GET_PQ_CAPS, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_DRM_SET_LEASE_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_DRM_GET_LEASE_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_ODDMR_LOAD_PARAM, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_ODDMR_CTL, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_KICK_IDLE, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_PQ_FRAME_CONFIG, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_GET_MODE_EXT_INFO, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_PQ_PROXY_IOCTL, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_HWVSYNC_ON, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_DUMMY_CMD_ON, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_ESD_STAT_CHK, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_MML_CTRL, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_DEBUG_LOG, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SEC_HND_TO_GEM_HND, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_SET_OVL_LAYER, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_MAP_DMA_BUF, NULL),
	DRM_IOCTL32_DEF_DRV(MTK_UNMAP_DMA_BUF, NULL),
};

long mtk_drm_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	struct drm_file *file_priv = filp->private_data;
	drm_ioctl_compat_t *fn;
	int ret;

#ifdef CONFIG_MTK_HDMI_SUPPORT
	if (file_priv)
		file_priv->authenticated = 1;
#endif
	if (nr < DRM_COMMAND_BASE ||
	    nr >= DRM_COMMAND_BASE + ARRAY_SIZE(mtk_compat_ioctls))
		return drm_compat_ioctl(filp, cmd, arg);

	fn = mtk_compat_ioctls[nr - DRM_COMMAND_BASE].fn;
	if (!fn)
		return drm_compat_ioctl(filp, cmd, arg);

	if (file_priv)
		DDPDBG("%s: pid=%d, dev=0x%lx, auth=%d, %s\n",
			  __func__, task_pid_nr(current),
			  (long)old_encode_dev(file_priv->minor->kdev->devt),
			  file_priv->authenticated,
			  mtk_compat_ioctls[nr - DRM_COMMAND_BASE].name);
	ret = (*fn)(filp, cmd, arg);
	if (ret)
		DDPDBG("%s: %s: ret = %d\n",
			  __func__,
			  mtk_compat_ioctls[nr - DRM_COMMAND_BASE].name, ret);
	return ret;
}
EXPORT_SYMBOL(mtk_drm_compat_ioctl);
#else
#define mtk_drm_compat_ioctl NULL
#endif

static const struct file_operations mtk_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mtk_drm_compat_ioctl,
#endif
};

static struct drm_driver mtk_drm_driver = {
	.driver_features =
		DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,

	/* .get_vblank_counter = drm_vblank_no_hw_counter, */

	.dumb_create = mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.gem_prime_import = mtk_gem_prime_import,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.ioctls = mtk_ioctls,
	.num_ioctls = ARRAY_SIZE(mtk_ioctls),
	.fops = &mtk_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_bind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;
	int ret;

	DDPINFO("%s+\n", __func__);
	drm = drm_dev_alloc(&mtk_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0)
		goto err_free;

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_deinit;

	mtk_layering_rule_init(drm);
#ifdef DRM_OVL_SELF_PATTERN
	mtk_drm_disp_test_init(drm);
	mtk_drm_disp_test_show(test_crtc, 1);
#endif
	DDPINFO("%s-\n", __func__);

	return 0;

err_deinit:
	mtk_drm_kms_deinit(drm);
err_free:
	drm_dev_put(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_dev_unregister(private->drm);
	drm_dev_put(private->drm);
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind = mtk_drm_bind, .unbind = mtk_drm_unbind,
};

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{.compatible = "mediatek,mt2701-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6779-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6761-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6765-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6768-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6873-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6853-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6833-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6877-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6781-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6879-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6855-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6835-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt8173-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6885-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6983-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6985-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6989-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6899-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6897-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6895-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6886-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt2701-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6779-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6761-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6765-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6768-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6873-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6853-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6833-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6877-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6781-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6879-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6855-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6835-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt8173-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6885-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6983-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
//	{.compatible = "mediatek,mt6985-disp-rdma",
//	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6895-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6886-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6985-disp-mdp-rdma",
	 .data = (void *)MTK_DISP_MDP_RDMA},
	{.compatible = "mediatek,mt6989-disp-mdp-rdma",
	 .data = (void *)MTK_DISP_MDP_RDMA},
//	{.compatible = "mediatek,mt6899-disp-mdp-rdma",
//	 .data = (void *)MTK_DISP_MDP_RDMA},
	{.compatible = "mediatek,mt6991-disp-mdp-rdma",
	 .data = (void *)MTK_DISP_MDP_RDMA},
	{.compatible = "mediatek,mt8173-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6779-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6761-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6765-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6768-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6885-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6873-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6853-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6833-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6877-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6781-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6879-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6855-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6835-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6983-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6985-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6989-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6899-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6991-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6897-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6895-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6886-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6779-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6761-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6765-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6768-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6885-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6983-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6985-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6989-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6899-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6991-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6897-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6895-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6886-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6879-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6855-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6835-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6873-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6853-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6833-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6877-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6781-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6983-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6985-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6989-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6899-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6991-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6897-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6895-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6886-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6879-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6983-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6985-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6989-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6899-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6991-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6897-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6895-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6886-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6879-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6983-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6985-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6989-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6899-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6991-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6897-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6895-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6879-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6855-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt2701-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6779-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6761-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6765-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6768-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6873-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6853-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6983-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6985-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6989-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6899-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6991-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6897-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6895-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6886-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6879-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6855-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6835-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6833-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6877-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6781-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt8173-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6885-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6779-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6761-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6765-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6768-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6873-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6853-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6833-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6877-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6781-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt8173-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6885-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6983-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6985-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6989-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6899-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6991-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6897-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6895-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6886-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6879-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6855-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6835-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6779-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6761-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6765-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6768-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt8173-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6885-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6873-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6853-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6833-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6877-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6781-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6983-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6985-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6989-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6899-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6991-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6897-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6895-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6886-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6879-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6855-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6835-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6779-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6761-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6765-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6768-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6885-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6983-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6985-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6989-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6899-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6991-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6897-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6895-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6886-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6879-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6855-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6835-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6873-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6853-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6833-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6877-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6781-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt8173-disp-ufoe",
	 .data = (void *)MTK_DISP_UFOE},
	{.compatible = "mediatek,mt2701-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6779-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6761-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6765-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6768-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt8173-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6885-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6983-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6985-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6989-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6899-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6991-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6897-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6895-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6886-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6885-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6983-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6985-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6989-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6899-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6991-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6991-edp-dvo",
	 .data = (void *)MTK_DISP_DVO},
	{.compatible = "mediatek,mt6897-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6895-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6873-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6853-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6833-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6877-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6781-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6879-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6855-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6835-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt2712-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt6779-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt8173-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt2701-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt2712-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6779-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6761-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6765-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6768-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt8173-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6885-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6983-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6985-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6989-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6899-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6991-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6897-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6895-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6886-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6873-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6853-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6833-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6877-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6781-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6879-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6855-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6835-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt2701-disp-pwm",
	 .data = (void *)MTK_DISP_BLS},
	{.compatible = "mediatek,mt6779-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6761-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6765-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6768-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt8173-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6885-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6983-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6985-disp-pwm0",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6989-disp-pwm0",
	 .data = (void *)MTK_DISP_PWM},
	//{.compatible = "mediatek,mt6899-disp-pwm0",
	// .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6991-disp-pwm0",
	 .data = (void *)MTK_DISP_PWM},
	//{.compatible = "mediatek,mt6897-disp-pwm0",
	// .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6895-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6886-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6873-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6853-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6833-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6877-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6781-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6879-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6855-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6835-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt8173-disp-od",
	 .data = (void *)MTK_DISP_OD},
	{.compatible = "mediatek,mt6779-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6761-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6765-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6768-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6885-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6873-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6853-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6833-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6877-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6781-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6879-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6835-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6855-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6983-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6985-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6989-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6899-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6991-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6897-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6895-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6886-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6985-disp-mdp-rsz",
	 .data = (void *)MTK_DISP_MDP_RSZ},
	{.compatible = "mediatek,mt6989-disp-mdp-rsz",
	 .data = (void *)MTK_DISP_MDP_RSZ},
	{.compatible = "mediatek,mt6899-disp-mdp-rsz",
	 .data = (void *)MTK_DISP_MDP_RSZ},
	{.compatible = "mediatek,mt6991-disp-mdp-rsz",
	 .data = (void *)MTK_DISP_MDP_RSZ},
	{.compatible = "mediatek,mt6897-disp-mdp-rsz",
	 .data = (void *)MTK_DISP_MDP_RSZ},
	{.compatible = "mediatek,mt6779-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6885-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6983-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6985-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6989-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6899-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6991-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6897-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6895-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6886-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6879-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6855-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6835-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6873-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6853-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6833-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6877-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6781-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6983-disp-cm",
	 .data = (void *)MTK_DISP_CM},
//	{.compatible = "mediatek,mt6985-disp-cm",
//	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6895-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6886-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6879-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6983-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6985-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6989-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6899-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6991-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6897-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6895-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6886-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6879-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6985-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6989-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6899-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6991-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6897-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6885-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6983-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6985-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6989-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6899-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6991-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6897-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6895-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6886-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6879-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6855-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6873-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6853-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6877-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6781-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6885-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6983-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6985-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6989-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
//	{.compatible = "mediatek,mt6899-disp-merge",
//	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6991-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6897-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6895-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6885-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6983-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6985-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6989-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6899-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	{.compatible = "mediatek,mt6991-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
#endif
	{.compatible = "mediatek,mt6895-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6897-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6885-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6983-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6985-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6989-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6899-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6991-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6897-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6895-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6873-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6983-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6985-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6989-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6991-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6897-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6899-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6886-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6989-disp1-r2y",
	 .data = (void *)MTK_DISP_R2Y},
//	{.compatible = "mediatek,mt6899-disp1-r2y",
//	 .data = (void *)MTK_DISP_R2Y},
	{.compatible = "mediatek,mt6991-disp1-r2y",
	 .data = (void *)MTK_DISP_R2Y},
	{.compatible = "mediatek,mt6991-disp-ovl-exdma",
	 .data = (void *)MTK_OVL_EXDMA},
	{.compatible = "mediatek,mt6991-disp-ovl-blender",
	 .data = (void *)MTK_OVL_BLENDER},
	{.compatible = "mediatek,mt6991-disp-ovl-outproc",
	 .data = (void *)MTK_OVL_OUTPROC},
	{.compatible = "mediatek,mt6991-vdisp-ao",
	 .data = (void *)MTK_DISP_VDISP_AO},
	/* MML */
	{.compatible = "mediatek,mt6983-mml_rsz",
	 .data = (void *)MTK_MML_RSZ},
	{.compatible = "mediatek,mt6983-mml_hdr",
	 .data = (void *)MTK_MML_HDR},
	{.compatible = "mediatek,mt6983-mml_aal",
	 .data = (void *)MTK_MML_AAL},
	{.compatible = "mediatek,mt6983-mml_tdshp",
	 .data = (void *)MTK_MML_TDSHP},
	{.compatible = "mediatek,mt6983-mml_color",
	 .data = (void *)MTK_MML_COLOR},
	{.compatible = "mediatek,mt6886-mml_rsz",
	 .data = (void *)MTK_MML_RSZ},
	{.compatible = "mediatek,mt6886-mml_hdr",
	 .data = (void *)MTK_MML_HDR},
	{.compatible = "mediatek,mt6886-mml_aal",
	 .data = (void *)MTK_MML_AAL},
	{.compatible = "mediatek,mt6886-mml_tdshp",
	 .data = (void *)MTK_MML_TDSHP},
	{.compatible = "mediatek,mt6886-mml_color",
	 .data = (void *)MTK_MML_COLOR},
	{.compatible = "mediatek,mt6983-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6985-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6989-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6899-mml1",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6991-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6897-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6886-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6983-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6985-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6989-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6991-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6991-mmlt_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6897-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6899-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6886-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6983-mml_wrot",
	 .data = (void *)MTK_MML_WROT},
	{.compatible = "mediatek,mt6886-mml_wrot",
	 .data = (void *)MTK_MML_WROT},
	{.compatible = "mediatek,mt6983-disp-dlo-async3",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6886-disp-dlo-async3",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6985-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6989-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6899-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6991-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6897-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6983-disp-dli-async3",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6886-disp-dli-async3",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6985-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6989-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6899-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6991-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6897-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6983-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6895-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6985-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6989-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6899-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6897-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6886-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6983-mmlsys-bypass",
	 .data = (void *)MTK_MMLSYS_BYPASS},
	{.compatible = "mediatek,mt6985-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{.compatible = "mediatek,mt6989-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{.compatible = "mediatek,mt6899-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{.compatible = "mediatek,mt6991-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{.compatible = "mediatek,mt6897-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{.compatible = "mediatek,mt6991-disp-splitter",
	 .data = (void *)MTK_DISP_SPLITTER},
	{} };

static struct disp_iommu_device disp_iommu;
static struct platform_device mydev;

struct disp_iommu_device *disp_get_iommu_dev(void)
{
	struct device_node *larb_node[DISP_LARB_COUNT];
	struct platform_device *larb_pdev[DISP_LARB_COUNT];
	int larb_idx = 0;
	struct device_node *np;

	if (disp_iommu.inited)
		return &disp_iommu;

	for (larb_idx = 0; larb_idx < DISP_LARB_COUNT; larb_idx++) {

		larb_node[larb_idx] = of_parse_phandle(mydev.dev.of_node,
						"mediatek,larb", larb_idx);
		if (!larb_node[larb_idx]) {
			DDPINFO("disp driver get larb %d fail\n", larb_idx);
			return NULL;
		}
		larb_pdev[larb_idx] =
			of_find_device_by_node(larb_node[larb_idx]);
		of_node_put(larb_node[larb_idx]);
		if ((!larb_pdev[larb_idx]) ||
		    (!larb_pdev[larb_idx]->dev.driver)) {
			if (!larb_pdev[larb_idx])
				DDPINFO("earlier than SMI, larb_pdev null\n");
			else
				DDPINFO("earlier than SMI, larb drv null\n");
		}

		disp_iommu.larb_pdev[larb_idx] = larb_pdev[larb_idx];
	}
	/* add for mmp dump mva->pa */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt-pseudo_m4u-port");
	if (np == NULL) {
		DDPINFO("DT,mediatek,mt-pseudo_m4u-port is not found\n");
	} else {
		disp_iommu.iommu_pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!disp_iommu.iommu_pdev)
			DDPINFO("get iommu device failed\n");
	}
	disp_iommu.inited = 1;
	return &disp_iommu;
}

struct device *mtk_drm_get_pd_device(struct device *dev, const char *id)
{
	int index;
	struct device_node *np = NULL;
	struct platform_device *pd_pdev;
	struct device *pd_dev;

	index = of_property_match_string(dev->of_node, "pd-names", id);
	if (index < 0) {
		DDPPR_ERR("can't match %s device node\n", id);
		return NULL;
	}
	np = of_parse_phandle(dev->of_node, "pd-others", index);
	if (!np) {
		DDPPR_ERR("can't find %s device node\n", id);
		return NULL;
	}

	DDPINFO("get %s power-domain at %s\n", id, np->full_name);

	pd_pdev = of_find_device_by_node(np);
	if (!pd_pdev) {
		DDPPR_ERR("can't get %s pdev\n", id);
		return NULL;
	}

	pd_dev = get_device(&pd_pdev->dev);
	of_node_put(np);

	return pd_dev;
}

static int mtk_drm_get_segment_id(struct platform_device *pdev,
	struct mtk_drm_private *private)
{
	int ret = 0;
	unsigned int segment_id = 0;
#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	struct nvmem_cell *efuse_cell;
	unsigned int *efuse_buf;
	size_t efuse_len;
#endif

	if (IS_ERR_OR_NULL(private)) {
		DDPPR_ERR("%s, private is NULL\n", __func__);
		ret = EFAULT;
		return ret;
	}

#if IS_ENABLED(CONFIG_MTK_DEVINFO)
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_seg_disp_cell");
	if (IS_ERR(efuse_cell)) {
		DDPPR_ERR("%s, fail to get efuse_segment_cell (%ld)\n",
			__func__, PTR_ERR(efuse_cell));
		ret = PTR_ERR(efuse_cell);
		goto done;
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	if (IS_ERR(efuse_buf)) {
		DDPPR_ERR("%s, fail to get efuse_buf (%ld)\n",
			__func__, PTR_ERR(efuse_buf));
		ret = PTR_ERR(efuse_buf);
		goto done;
	}

	segment_id = (*efuse_buf & 0xFF);
	kfree(efuse_buf);
#endif

done:
	DDPINFO("%s, segment_id: %d", __func__, segment_id);

	private->seg_id = segment_id;
	return ret;
}

bool mtk_disp_is_svp_on_mtee(void)
{
	static int mtee_on = -1;
	struct device_node *dt_node;

	if (mtee_on != -1)
		return mtee_on;
	dt_node = of_find_node_by_name(NULL, "MTEE");
	if (!dt_node) {
		mtee_on = 0;
		return false;
	}
	mtee_on = 1;
	return true;
}
EXPORT_SYMBOL_GPL(mtk_disp_is_svp_on_mtee);

void _mtk_sent_aod_scp_sema(void __iomem *_SPM_SEMA_AP)
{
	if (vdisp_func.sent_aod_scp_sema)
		vdisp_func.sent_aod_scp_sema(_SPM_SEMA_AP);
	else
		DDPMSG("WARNING: sent aod scp semaphore fail!\n");
}
EXPORT_SYMBOL_GPL(_mtk_sent_aod_scp_sema);

static bool init_secure_static_path_switch(struct device *dev, struct mtk_drm_private *priv)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(dev->of_node, "SecureVideoPath");
	if (!dt_node) {
		priv->secure_static_path_switch = false;
		return false;
	}
	if (priv->data->mmsys_id == MMSYS_MT6768) {
		mt6768_mtk_main_path_data.path[DDP_MAJOR][0] = mt6768_mtk_ddp_sec_main;
		mt6768_mtk_main_path_data.path_len[DDP_MAJOR][0] =
			ARRAY_SIZE(mt6768_mtk_ddp_sec_main);
		mt6768_mtk_main_path_data.addon_data = mt6768_addon_sec_main;
		priv->secure_static_path_switch = true;
		return true;
	}
	priv->secure_static_path_switch = false;
	return false;
}

void mtk_drm_get_chipid(struct mtk_drm_private *private)
{
	struct device_node *node;
	struct tag_chipid *chip_id = NULL;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");
	if (node) {
		chip_id = (struct tag_chipid *) of_get_property(node, "atag,chipid", &len);
		if (!chip_id) {
			DDPMSG("could not found atag,chipid in chosen\n");
			return;
		}
	} else {
		DDPMSG("chosen node not found in device tree\n");
		return;
	}
	if (chip_id)
		private->sw_ver = chip_id->sw_ver;

	DDPMSG("sw_ver:0x%x hw_ver:0x%x hw_code:0x%x hw_subcode:0x%x\n",
		private->sw_ver, chip_id->hw_ver, chip_id->hw_code, chip_id->hw_subcode);
}

static int mtk_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_drm_private *private;
	struct resource *mem;
	struct device_node *node;
	struct component_match *match = NULL;
	unsigned int dispsys_num = 0, ovlsys_num = 0, pq_path_sel = 1;
	int ret, len;
	int i;
	struct platform_device *side_pdev;
	struct device *side_dev = NULL;
	struct device_node *side_node = NULL;
	struct device_node *aod_scp_node = NULL;
	struct device_node *disp_plat_dbg_node = pdev->dev.of_node;
	const __be32 *ranges = NULL;
	bool mml_found = false;

	disp_dbg_probe();
	PanelMaster_probe();
	DDPINFO("%s+\n", __func__);

	//drm_debug = 0x1F; /* DRIVER messages */
	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	private->data = of_device_get_match_data(dev);

	private->reg_data = mtk_ddp_get_mmsys_reg_data(private->data->mmsys_id);
	if (IS_ERR(private->reg_data)) {
		ret = PTR_ERR(private->config_regs);
		DDPPR_ERR("Failed to get mmsys register data: %d\n", ret);
		return ret;
	}

	mutex_init(&private->commit.lock);
	INIT_WORK(&private->commit.work, mtk_atomic_work);

	mtk_drm_helper_init(dev, &private->helper_opt);

	mtk_drm_get_chipid(private);

	/* Init disp_global_stage from platform dts */
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_STAGE) == DISP_HELPER_STAGE_NORMAL)
		disp_helper_set_stage(DISP_HELPER_STAGE_NORMAL);
	else
		disp_helper_set_stage(DISP_HELPER_STAGE_BRING_UP);

	if (of_property_read_bool(dev->of_node, "is-tablet")) {
		private->is_tablet = true;
		DDPFUNC("is tablet!\n");
	}
	init_secure_static_path_switch(dev, private);
	if (private->secure_static_path_switch == true)
		mtk_drm_helper_set_opt_by_name(private->helper_opt, "MTK_DRM_OPT_RPO", 0);

	if (private->data->mmsys_id == MMSYS_MT6897) {
		if (mtk_drm_get_segment_id(pdev, private))
			DDPPR_ERR("%s, segment get fail\n", __func__);
	}

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret)
			DDPPR_ERR("Failed to set_coherent_mask: %d\n", ret);
	}

	ret = of_property_read_u32(dev->of_node,
				"dispsys-num", &dispsys_num);
	if (ret) {
		dev_err(dev,
			"no dispsys_config dispsys_num %d\n", ret);
		dispsys_num = 1;
	}

	private->dispsys_num = dispsys_num;

	ret = of_property_read_u32(dev->of_node,
				"ovlsys-num", &ovlsys_num);
	private->ovlsys_num = ovlsys_num;

	ret = of_property_read_u32(dev->of_node,
				"pq-path-sel", &pq_path_sel);
	if (pq_path_sel > 0)
		private->pq_path_sel = pq_path_sel;
	else
		private->pq_path_sel = 1;
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}
	private->config_regs_pa = mem->start;
	private->mmsys_dev = dev;

	if (dispsys_num <= 1)
		goto SKIP_SIDE_DISP;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (mem) {
		private->side_config_regs = devm_ioremap_resource(dev, mem);
		if (IS_ERR(private->side_config_regs)) {
			ret = PTR_ERR(private->side_config_regs);
			dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
				ret);
			return ret;
		}
		private->side_config_regs_pa = mem->start;
	}

	side_dev = mtk_drm_get_pd_device(dev, "side_dispsys");
	if (!side_dev) {
		/* tricky method to handle dispsys1 power domain */
		side_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_mutex0");
		if (side_node) {
			side_pdev = of_find_device_by_node(side_node);
			if (!side_pdev)
				DDPPR_ERR("can't get side_mmsys_dev\n");
			else
				side_dev = get_device(&side_pdev->dev);
		} else {
			DDPPR_ERR("can't find side_mmsys node");
		}
		of_node_put(side_node);
	}
	private->side_mmsys_dev = side_dev;

SKIP_SIDE_DISP:

	if (private->ovlsys_num == 0)
		goto SKIP_OVLSYS_CONFIG;

	/* ovlsys config */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!mem)
		goto SKIP_OVLSYS_CONFIG;

	private->ovlsys0_regs_pa = mem->start;
	private->ovlsys0_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->ovlsys0_regs)) {
		ret = PTR_ERR(private->ovlsys0_regs);
		dev_err(dev, "Failed to ioremap ovlsys0-config resource: %d\n",
			ret);
		return ret;
	}

	side_dev = mtk_drm_get_pd_device(dev, "ovlsys");
	private->ovlsys_dev = side_dev;

	if (private->ovlsys_num == 1)
		goto SKIP_OVLSYS_CONFIG;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!mem)
		goto SKIP_OVLSYS_CONFIG;

	private->ovlsys1_regs_pa = mem->start;
	private->ovlsys1_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->ovlsys1_regs)) {
		ret = PTR_ERR(private->ovlsys1_regs);
		dev_err(dev, "Failed to ioremap ovlsys1-config resource: %d\n",
			ret);
		return ret;
	}

	side_dev = mtk_drm_get_pd_device(dev, "side_ovlsys");
	private->side_ovlsys_dev = side_dev;

SKIP_OVLSYS_CONFIG:

	if (private->data->bypass_infra_ddr_control &&
		!private->data->use_infra_mem_res) {
		struct device_node *infra_node;
		struct platform_device *infra_pdev;
		struct device *infra_dev;
		struct resource *infra_mem;

		infra_node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao_mem");
		if (infra_node == NULL) {
			DDPPR_ERR("mediatek,infracfg_ao_mem is not found\n");
		} else {
			infra_pdev = of_find_device_by_node(infra_node);
			if (!infra_pdev) {
				dev_err(dev, "Waiting for infra-ao-mem device %s\n",
					private->mutex_node->full_name);
				of_node_put(infra_node);
				return -EPROBE_DEFER;
			}
			infra_dev = get_device(&infra_pdev->dev);
			infra_mem = platform_get_resource(infra_pdev, IORESOURCE_MEM, 0);
			private->infra_regs_pa = infra_mem->start;
			private->infra_regs = devm_ioremap_resource(infra_dev, infra_mem);
			if (IS_ERR(private->infra_regs))
				DDPPR_ERR("%s: infra_ao_base of_iomap failed\n", __func__);
			else
				DDPMSG("%s, infra_regs:0x%p, infra_regs_pa:0x%pa\n",
					__func__, (void *)private->infra_regs,
					&private->infra_regs_pa);
		}
		of_node_put(infra_node);
	}
	if (private->data->bypass_infra_ddr_control &&
		private->data->use_infra_mem_res) {
		struct device_node *infra_node;
		struct resource infra_mem_res;

		infra_node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
		if (infra_node == NULL) {
			DDPPR_ERR("mediatek,infracfg_ao is not found\n");
		} else {
			if (of_address_to_resource(infra_node, 0, &infra_mem_res) != 0) {
				DDPPR_ERR("%s: missing reg in %s node\n",
						__func__, infra_node->full_name);
				of_node_put(infra_node);
				return -EPROBE_DEFER;
			}
			private->infra_regs_pa = infra_mem_res.start;
			private->infra_regs = of_iomap(infra_node, 0);
			if (IS_ERR(private->infra_regs))
				DDPPR_ERR("%s: infra_ao_base of_iomap failed\n", __func__);
			else
				DDPMSG("%s, infra_regs:0x%p, infra_regs_pa:0x%pa\n",
					__func__, (void *)private->infra_regs,
					&private->infra_regs_pa);
		}
		of_node_put(infra_node);
	}

	atomic_set(&private->kernel_pm.wakelock_cnt, 0);
	atomic_set(&private->kernel_pm.status, KERNEL_PM_RESUME);
	init_waitqueue_head(&private->kernel_pm.wq);
	private->kernel_pm.nb.notifier_call = mtk_drm_pm_notifier;
	ret = register_pm_notifier(&private->kernel_pm.nb);
	if (ret)
		DDPMSG("register_pm_notifier failed %d", ret);

	private->dsi_phy0_dev = mtk_drm_get_pd_device(dev, "dsi_phy0");
	private->dsi_phy1_dev = mtk_drm_get_pd_device(dev, "dsi_phy1");
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	private->dsi_phy2_dev = mtk_drm_get_pd_device(dev, "dsi_phy2");
#endif

	private->dpc_dev = mtk_drm_get_pd_device(dev, "mminfra_in_dpc");
	if (private->dpc_dev) {
		pm_runtime_irq_safe(private->dpc_dev);
		g_dpc_dev = private->dpc_dev;
	}
	mtk_drm_pm_ctrl(private, DISP_PM_ENABLE);

	/* Get and enable top clk align to HW */
	mtk_drm_get_top_clk(private);

	/* Get AOD-SCP config */
	aod_scp_node = of_find_node_by_name(NULL, "AOD-SCP-ON");
	if (!aod_scp_node) {
		aod_scp_flag = 0;
		DDPMSG("%s AOD-SCP OFF\n", __func__);
	}	else {
		aod_scp_flag = 1;
		DDPMSG("%s AOD-SCP ON\n", __func__);
	}

	ret = of_property_read_u32(disp_plat_dbg_node, "disp_plat_dbg_addr", &g_disp_plat_dbg_addr);
	if (ret) {
		g_disp_plat_dbg_addr = 0;
		pr_info("%s: get disp_plat_dbg_addr fail\n", __func__);
	}

	ret = of_property_read_u32(disp_plat_dbg_node, "disp_plat_dbg_size", &g_disp_plat_dbg_size);
	if (ret) {
		g_disp_plat_dbg_size = 0;
		pr_info("%s: get disp_plat_dbg_size fail\n", __func__);
	}

	if ((g_disp_plat_dbg_addr > 0) && (g_disp_plat_dbg_size > 0)) {
		g_disp_plat_dbg_buf_addr = ioremap_wc((phys_addr_t)g_disp_plat_dbg_addr,
			g_disp_plat_dbg_size);
		DDPMSG("disp_plat_dbg addr=0x%x, size=0x%x, buf_addr=0x%lx, %s\n",
			g_disp_plat_dbg_addr, g_disp_plat_dbg_size,
			(unsigned long)g_disp_plat_dbg_buf_addr, __func__);
		disp_plat_dbg_buf_attr.size = g_disp_plat_dbg_size;
		ret = sysfs_create_bin_file(&pdev->dev.kobj, &disp_plat_dbg_buf_attr);
		if (ret) {
			dev_err(&pdev->dev, "Failed to create disp_plat_dbg buf file\n");
			return ret;
		}
		disp_plat_dbg_init();
	}
#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
	private->hrt_bw_request =
		of_icc_get(dev, "disp-perf-bw");
	ret = of_count_phandle_with_args(dev->of_node, "dvfsrc-required-opps", NULL);
	if (ret > 0) {
		disp_perfs = devm_kzalloc(dev,
				ret * sizeof(u32),
				GFP_KERNEL);
		if (!disp_perfs) {
			ret = -ENOMEM;
			goto err_node;
		}
		for (i = 0; i < ret; i++) {
			disp_perfs[i] =
				dvfsrc_get_required_opp_peak_bw_legacy(dev->of_node, i);
			DDPINFO("%s(),%d,disp_perfs[%d] = %d\n", __func__, __LINE__, i, disp_perfs[i]);
		}
	}
#else
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		private->hrt_bw_request =
			of_mtk_icc_get(dev, "disp_hrt_qos");

		if (mtk_drm_helper_get_opt(private->helper_opt,
				MTK_DRM_OPT_HRT_BY_LARB)) {
			private->hrt_by_larb =
				of_mtk_icc_get(dev, "disp_hrt_by_larb");
			private->dp_hrt_by_larb  =
				of_mtk_icc_get(dev, "disp_dp_hrt_by_larb");
		}
	}
#endif
	/* Iterate over sibling DISP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		const struct of_device_id *of_id;
		enum mtk_ddp_comp_type comp_type;
		enum mtk_ddp_comp_id comp_id;

		of_id = of_match_node(mtk_ddp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled component %s\n",
				node->full_name);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		DDPINFO("%s line:%d, comp_type:%d\n", __func__, __LINE__,
			comp_type);

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if ((int)comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %s\n",
				 node->full_name);
			continue;
		}
		DDPINFO("%s line:%d, comp_id:%d\n", __func__, __LINE__,
			comp_id);
		/*comp node is registered here including PQ module*/
		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the COLOR, OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */
		if (comp_type == MTK_DISP_OVL || comp_type == MTK_OVL_EXDMA ||
		    comp_type == MTK_DISP_SPLITTER ||
		    comp_type == MTK_OVL_BLENDER || comp_type == MTK_DISP_VDISP_AO ||
		    comp_type == MTK_DISP_MERGE || comp_type == MTK_OVL_OUTPROC ||
		    comp_type == MTK_DISP_RDMA || comp_type == MTK_DISP_MDP_RDMA
		    || comp_type == MTK_DISP_WDMA || comp_type == MTK_DISP_RSZ
		    || comp_type == MTK_DISP_MDP_RSZ ||
		    comp_type == MTK_DISP_POSTMASK || comp_type == MTK_DSI
		    || comp_type == MTK_DISP_DSC || comp_type == MTK_DPI
#ifndef DRM_BYPASS_PQ
		    || comp_type == MTK_DISP_TDSHP || comp_type == MTK_DISP_CHIST
		    || comp_type == MTK_DISP_C3D || comp_type == MTK_DISP_COLOR ||
		    comp_type == MTK_DISP_CCORR ||
		    comp_type == MTK_DISP_GAMMA || comp_type == MTK_DISP_AAL ||
		    comp_type == MTK_DISP_DITHER ||
		    comp_type == MTK_DISP_CM || comp_type == MTK_DISP_SPR ||
		    comp_type == MTK_DISP_POSTALIGN || comp_type == MTK_DMDP_AAL
		    || comp_type == MTK_DISP_ODDMR
#endif
		    || comp_type == MTK_DP_INTF || comp_type == MTK_DISP_DPTX
		    || comp_type == MTK_DISP_Y2R || comp_type == MTK_DISP_INLINE_ROTATE
		    || comp_type == MTK_DISP_DLI_ASYNC || comp_type == MTK_DISP_DLO_ASYNC
		    || comp_type == MTK_DISP_R2Y
		    || comp_type == MTK_DISP_DVO
		) {
			dev_info(dev, "Adding component match for %s, comp_id:%d\n",
				 node->full_name, comp_id);
			component_match_add(dev, &match, compare_of, node);
		} else if (comp_type == MTK_MML_MML || comp_type == MTK_MML_MUTEX ||
			   comp_type == MTK_MML_RSZ || comp_type == MTK_MML_HDR ||
			   comp_type == MTK_MML_AAL || comp_type == MTK_MML_TDSHP ||
			   comp_type == MTK_MML_COLOR || comp_type == MTK_MML_WROT) {
			dev_info(dev, "Adding component match for %s, comp_id:%d\n",
				 node->full_name, comp_id);
			component_match_add(dev, &match, compare_of, node);
			mml_found = true;
		} else {
			struct mtk_ddp_comp *comp;

			comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
			if (!comp) {
				ret = -ENOMEM;
				goto err_node;
			}

			ret = mtk_ddp_comp_init(dev, node, comp, comp_id, NULL);
			if (ret)
				goto err_node;

		      private
			->ddp_comp[comp_id] = comp;
		}
	}

	if (!mml_found) { /* if mmlsys is not a sibling of dispsys */
		const char *path;
		struct device_node *alias_node = of_find_node_by_path("/aliases");

		if (!of_property_read_string(alias_node, "mmlsys-cfg", &path)) {
			node = of_find_node_by_path(path);
			dev_info(dev, "Adding component match for %s\n", node->full_name);
			component_match_add(dev, &match, compare_of, node);
			of_node_put(node);
		}
		if (!of_property_read_string(alias_node, "mml-mutex", &path)) {
			node = of_find_node_by_path(path);
			dev_info(dev, "Adding component match for %s\n", node->full_name);
			component_match_add(dev, &match, compare_of, node);
			of_node_put(node);
		}
	}

	if (!private->mutex_node) {
		dev_err(dev, "Failed to find disp-mutex node\n");
		ret = -ENODEV;
		goto err_node;
	}

	platform_set_drvdata(pdev, private);

	ret = component_master_add_with_match(dev, &mtk_drm_ops, match);
	DDPINFO("%s- ret:%d\n", __func__, ret);
	if (ret)
		goto err_pm;

	for (i = 0 ; i < MAX_CRTC ; ++i) {
		unsigned int value;

		ret = of_property_read_u32_index(dev->of_node, "pre-define-bw", i, &value);
		if (unlikely(ret < 0))
			value = 0;

		private->pre_defined_bw[i] = value;

		ret = of_property_read_u32_index(dev->of_node, "crtc-ovl-usage", i, &value);
		if (unlikely(ret < 0))
			value = 0;
		private->ovlsys_usage[i] = value;

		private->ovl_usage[i] = mtk_ddp_ovl_usage_trans(private, private->ovlsys_usage[i]);
		DDPINFO("CRTC %d available BW:%x OVL usage:%x\n", i,
				private->pre_defined_bw[i], private->ovl_usage[i]);
	}

	mtk_fence_init();

#if IS_ENABLED(CONFIG_MTK_SE_SUPPORT)
	memset((void *)&dma_map_list, 0, sizeof(dma_map_list));
	INIT_LIST_HEAD(&dma_map_list.list);
#endif
	private->uevent_data.name = "lcm_disconnect";
	uevent_dev_register(&private->uevent_data);
	mtk_notifier_activate();

	DDPINFO("%s-\n", __func__);

	disp_dts_gpio_init(dev, private);

	memcpy(&mydev, pdev, sizeof(mydev));

	return 0;

err_pm:
	mtk_drm_pm_ctrl(private, DISP_PM_DISABLE);

err_node:
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	sysfs_remove_bin_file(&pdev->dev.kobj, &disp_plat_dbg_buf_attr);

	return ret;
}

static void mtk_drm_shutdown(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	struct drm_device *drm = private->drm;
	int ret = 0;

	if (drm) {
		/* skip all next atomic commit */
		atomic_set(&private->kernel_pm.status, KERNEL_SHUTDOWN);
		DDPMSG("%s status(%d)\n", __func__, atomic_read(&private->kernel_pm.status));
		mtk_drm_pm_ctrl(private, DISP_PM_GET);
		ret = mtk_vidle_force_power_ctrl_by_cpu(true);
		drm_atomic_helper_shutdown(drm);
		if (!ret)
			ret = mtk_vidle_force_power_ctrl_by_cpu(false);
		else if (ret < 0)
			DDPMSG("%s ret[%d]", __func__, ret);
		mtk_drm_pm_ctrl(private, DISP_PM_PUT);
	}
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	struct drm_device *drm = private->drm;
	int i;

	drm_dev_unregister(drm);
	mtk_drm_kms_deinit(drm);
	drm_dev_put(drm);

	component_master_del(&pdev->dev, &mtk_drm_ops);

	mtk_drm_pm_ctrl(private, DISP_PM_DISABLE);

	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);
#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
	kfree(disp_perfs);
#endif
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_drm_sys_suspend(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	int i;
	bool wake_state = false;

	/* check each CRTC suspend already */
	for (i = 0; i < 3; i++) {
		crtc = private->crtc[i];
		if (!crtc)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);
		wake_state |= mtk_crtc->wk_lock->active;
	}

	if (wake_state == false)
		goto OUT;

	//DDPMSG("%s\n");
	drm_kms_helper_poll_disable(drm);

	private->suspend_state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(private->suspend_state)) {
		drm_kms_helper_poll_enable(drm);
		return PTR_ERR(private->suspend_state);
	}
OUT:
#ifdef MTK_DRM_FENCE_SUPPORT
	mtk_drm_suspend_release_fence(dev);
#endif

	DRM_DEBUG_DRIVER("%s\n", __func__);
	return 0;
}

static int mtk_drm_sys_resume(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;

	drm = private->drm;
	if (!drm)
		return 0;

	if (!private->suspend_state) {
		DDPINFO("suspend_state is null\n");
		return 0;
	}

	drm_atomic_helper_resume(drm, private->suspend_state);
	drm_kms_helper_poll_enable(drm);

	private->suspend_state = NULL;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static const struct of_device_id mtk_drm_of_ids[] = {
	{.compatible = "mediatek,mt2701-mmsys",
	 .data = &mt2701_mmsys_driver_data},
	{.compatible = "mediatek,mt2712-display",
	 .data = &mt2712_mmsys_driver_data},
	{.compatible = "mediatek,mt6779-mmsys",
	 .data = &mt6779_mmsys_driver_data},
	{.compatible = "mediatek,mt6761-mmsys",
	 .data = &mt6761_mmsys_driver_data},
	{.compatible = "mediatek,mt6765-mmsys",
	 .data = &mt6765_mmsys_driver_data},
	{.compatible = "mediatek,mt6768-mmsys",
	 .data = &mt6768_mmsys_driver_data},
	{.compatible = "mediatek,mt8173-mmsys",
	 .data = &mt8173_mmsys_driver_data},
	{.compatible = "mediatek,mt6885-mmsys",
	 .data = &mt6885_mmsys_driver_data},
	{.compatible = "mediatek,mt6893-disp",
	 .data = &mt6885_mmsys_driver_data},
	{.compatible = "mediatek,mt6983-disp",
	 .data = &mt6983_mmsys_driver_data},
	{.compatible = "mediatek,mt6985-disp",
	 .data = &mt6985_mmsys_driver_data},
	{.compatible = "mediatek,mt6989-disp",
	 .data = &mt6989_mmsys_driver_data},
	{.compatible = "mediatek,mt6899-disp",
	 .data = &mt6899_mmsys_driver_data},
	{.compatible = "mediatek,mt6991-disp",
	 .data = &mt6991_mmsys_driver_data},
	{.compatible = "mediatek,mt6897-disp",
	 .data = &mt6897_mmsys_driver_data},
	{.compatible = "mediatek,mt6895-disp",
	 .data = &mt6895_mmsys_driver_data},
	{.compatible = "mediatek,mt6886-disp",
	 .data = &mt6886_mmsys_driver_data},
	{.compatible = "mediatek,mt6873-mmsys",
	 .data = &mt6873_mmsys_driver_data},
	{.compatible = "mediatek,mt6853-mmsys",
	 .data = &mt6853_mmsys_driver_data},
	{.compatible = "mediatek,mt6833-disp",
	 .data = &mt6833_mmsys_driver_data},
	{.compatible = "mediatek,mt6877-mmsys",
	 .data = &mt6877_mmsys_driver_data},
	{.compatible = "mediatek,mt6781-mmsys",
	 .data = &mt6781_mmsys_driver_data},
	{.compatible = "mediatek,mt6879-mmsys",
	 .data = &mt6879_mmsys_driver_data},
	{.compatible = "mediatek,mt6855-mmsys",
	 .data = &mt6855_mmsys_driver_data},
	{.compatible = "mediatek,mt6835-mmsys",
	 .data = &mt6835_mmsys_driver_data},
	{} };

static struct platform_driver mtk_drm_platform_driver = {
	.probe = mtk_drm_probe,
	.remove = mtk_drm_remove,
	.shutdown = mtk_drm_shutdown,
	.driver = {

			.name = "mediatek-drm",
			.of_match_table = mtk_drm_of_ids,
			.pm = &mtk_drm_pm_ops,
		},
};

MODULE_DEVICE_TABLE(of, mtk_drm_of_ids);

static struct platform_driver *const mtk_drm_drivers[] = {
	&mtk_drm_platform_driver,
	&mtk_vdisp_ao_driver,
	&mtk_ddp_driver,
	&mtk_disp_tdshp_driver,
	&mtk_disp_color_driver,
	&mtk_disp_ccorr_driver,
	&mtk_disp_c3d_driver,
	&mtk_disp_gamma_driver,
	&mtk_disp_aal_driver,
	&mtk_dmdp_aal_driver,
	&mtk_disp_postmask_driver,
	&mtk_disp_dither_driver,
	&mtk_disp_chist_driver,
	&mtk_disp_ovl_driver,
	&mtk_ovl_exdma_driver,
	&mtk_ovl_blender_driver,
	&mtk_ovl_outproc_driver,
	&mtk_disp_rdma_driver,
	&mtk_disp_mdp_rdma_driver,
	&mtk_disp_wdma_driver,
	&mtk_disp_rsz_driver,
	&mtk_mipi_tx_driver,
	&mtk_dsi_driver,
	&mtk_dp_intf_driver,
	&mtk_disp_mdp_rsz_driver,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI)
	&mtk_dpi_driver,
	&mtk_lvds_driver,
	&mtk_lvds_tx_driver,
#endif
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_CEC)
	&mtk_cec_driver,
#endif
	&mtk_disp_cm_driver,
	&mtk_disp_spr_driver,
	&mtk_disp_postalign_driver,
	&mtk_disp_oddmr_driver,
	&mtk_disp_dsc_driver,
	&mtk_dp_tx_driver,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
	&mtk_dvo_driver,
#endif
	&mtk_disp_y2r_driver,
	&mtk_disp_r2y_driver,
	&mtk_disp_inlinerotate_driver,
	&mtk_disp_dlo_async_driver,
	&mtk_disp_dli_async_driver,
	&mtk_mmlsys_bypass_driver,
	&mtk_disp_merge_driver,
	&mtk_disp_splitter_driver
};

static int __init mtk_drm_init(void)
{
	int ret;
	int i;

	DDPINFO("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mtk_drm_drivers); i++) {
		DDPINFO("%s register %s driver\n",
			__func__, mtk_drm_drivers[i]->driver.name);
		ret = platform_driver_register(mtk_drm_drivers[i]);
		if (ret < 0) {
			DDPPR_ERR("Failed to register %s driver: %d\n",
				  mtk_drm_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	DDPINFO("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_drivers[i]);

	return ret;
}

static void __exit mtk_drm_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_drm_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_drivers[i]);
}
module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

#if (IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT) && IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT))
MODULE_SOFTDEP("pre: mtk-drm-edp panel-serdes-max96789");
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
MODULE_SOFTDEP("pre: mtk-drm-edp");
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
MODULE_SOFTDEP("pre: panel-serdes-max96789");
#endif
MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
