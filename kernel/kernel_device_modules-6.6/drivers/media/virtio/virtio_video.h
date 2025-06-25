/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/* Common header for virtio video driver.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 */

#ifndef _VIRTIO_VIDEO_H
#define _VIRTIO_VIDEO_H
#include <linux/version.h>

#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/virtio_video.h>
#include <linux/list.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-dma-contig.h>

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
#include "virtio_video_v4l2.h"

#define MTK_VCODEC_ENC_NAME     "mtk-vcodec-enc"
#define MTK_VCODEC_DEC_NAME     "mtk-vcodec-dec"
#define MTK_MAX_CTRLS_HINT	64
#endif

#define DRIVER_NAME "virtio-video"

#define MIN_BUFS_MIN 0
#define MIN_BUFS_MAX 32
#define MIN_BUFS_STEP 1
#define MIN_BUFS_DEF 1

enum virtio_video_device_type {
	VIRTIO_VIDEO_DEVICE_ENCODER = 0x0100,
	VIRTIO_VIDEO_DEVICE_DECODER,
};

struct video_format_frame {
	struct virtio_video_format_frame frame;
	struct virtio_video_format_range *frame_rates;
};

struct video_format {
	struct list_head formats_list_entry;
	struct virtio_video_format_desc desc;
	struct video_format_frame *frames;
};

struct video_control_fmt_data {
	uint32_t min;
	uint32_t max;
	uint32_t num;
	uint32_t skip_mask;
	uint32_t *entries;
};

struct video_control_format {
	struct list_head controls_list_entry;
	uint32_t format;
	struct video_control_fmt_data *profile;
	struct video_control_fmt_data *level;
};

struct video_plane_format {
	uint32_t plane_size;
	uint32_t stride;
};

struct video_format_info {
	uint32_t fourcc_format;
	uint32_t frame_rate;
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t min_buffers;
	uint32_t max_buffers;
	struct virtio_video_crop crop;
	uint32_t num_planes;
	struct video_plane_format plane_format[VIRTIO_VIDEO_MAX_PLANES];
	bool is_updated;
};

struct video_control_info {
	uint32_t profile;
	uint32_t level;
	uint32_t bitrate;
	bool is_updated;
};

struct virtio_video;
struct virtio_video_vbuffer;

typedef void (*virtio_video_resp_cb)(struct virtio_video *vv,
				     struct virtio_video_vbuffer *vbuf);

struct virtio_video_vbuffer {
	char *buf;
	int size;

	void *data_buf;
	uint32_t data_size;

	char *resp_buf;
	int resp_size;

	void *priv;
	virtio_video_resp_cb resp_cb;

	struct list_head list;
};

struct virtio_video_queue {
	struct virtqueue *vq;
	spinlock_t qlock;
	wait_queue_head_t ack_queue;
	struct work_struct dequeue_work;
};

enum virtio_video_resource_type {
	RESOURCE_TYPE_GUEST_PAGES = 0,
	RESOURCE_TYPE_VIRTIO_OBJECT,
};

struct virtio_video {
	struct v4l2_device v4l2_dev;
	int instance;

	enum virtio_video_resource_type res_type;

	struct virtio_device *vdev;
	struct virtio_video_queue commandq;
	struct virtio_video_queue eventq;
	wait_queue_head_t wq;
	bool vq_ready;

	struct kmem_cache *vbufs;

	struct idr resource_idr;
	spinlock_t resource_idr_lock;
	struct idr stream_idr;
	spinlock_t stream_idr_lock;

	uint32_t max_caps_len;
	uint32_t max_resp_len;
	bool got_caps;
	bool got_control;

	bool has_iommu;
	bool supp_non_contig;
	struct list_head devices_list;

	int debug;
	int use_dma_mem;
};

struct virtio_video_device {
	struct virtio_video *vv;
	struct video_device video_dev;
	struct mutex video_dev_mutex;

	struct v4l2_m2m_dev *m2m_dev;

	struct workqueue_struct *workqueue;

	struct list_head devices_list_entry;
	/* VIRTIO_VIDEO_FUNC_ */
	uint32_t type;

	uint32_t num_input_fmts;
	struct list_head input_fmt_list;

	uint32_t num_output_fmts;
	struct list_head output_fmt_list;

	struct list_head controls_fmt_list;

	/* vid_dev_nr - try register starting at video device number */
	int vid_dev_nr;
};

enum video_stream_state {
	STREAM_STATE_IDLE = 0,
	STREAM_STATE_INIT,
	STREAM_STATE_METADATA, /* specific to decoder */
	STREAM_STATE_RUNNING,
	STREAM_STATE_DRAIN,
	STREAM_STATE_STOPPED,
	STREAM_STATE_RESET, /* specific to encoder */
};

struct virtio_video_stream {
	uint32_t stream_id;
	enum video_stream_state state;
	struct video_device *video_dev;
	struct v4l2_fh fh;
	struct mutex vq_mutex;
	struct v4l2_ctrl_handler ctrl_handler;
	struct video_format_info in_info;
	struct video_format_info out_info;
	struct video_control_info control;
	bool src_cleared;
	bool dst_cleared;
	bool src_destroyed;
	bool dst_destroyed;
	struct work_struct work;
	struct video_format_frame *current_frame;
};

struct virtio_video_general_buffer {
	int gen_buf_fd;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *buf_att;
	struct sg_table *sgt;
};

struct virtio_video_buffer {
	struct v4l2_m2m_buffer v4l2_m2m_vb;
	uint32_t resource_id;
	bool queued;

	/* Only for virtio object buffer */
	uuid_t uuid;

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	struct virtio_video_general_buffer general_buf;
#endif
};

static inline gfp_t
virtio_video_gfp_flags(struct virtio_video *vv)
{
	if (vv->use_dma_mem)
		return GFP_DMA;
	else
		return 0;
}

static inline const struct vb2_mem_ops *
virtio_video_mem_ops(struct virtio_video *vv)
{
	if (vv->supp_non_contig)
		return &vb2_dma_sg_memops;
	else
		return &vb2_dma_contig_memops;
}

static inline struct virtio_video_device *
to_virtio_vd(struct video_device *video_dev)
{
	return container_of(video_dev, struct virtio_video_device,
			 video_dev);
}

static inline struct virtio_video_stream *file2stream(struct file *file)
{
	return container_of(file->private_data, struct virtio_video_stream, fh);
}

static inline struct virtio_video_stream *ctrl2stream(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct virtio_video_stream,
			    ctrl_handler);
}

static inline struct virtio_video_stream *work2stream(struct work_struct *work)
{
	return container_of(work, struct virtio_video_stream, work);
}

static inline struct virtio_video_buffer *to_virtio_vb(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_vb = to_vb2_v4l2_buffer(vb);

	return container_of(v4l2_vb, struct virtio_video_buffer,
			    v4l2_m2m_vb.vb);
}

static inline uint32_t to_virtio_queue_type(enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return VIRTIO_VIDEO_QUEUE_TYPE_INPUT;
	else
		return VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT;
}

static inline bool within_range(uint32_t min, uint32_t val, uint32_t max)
{
	return ((val - min) <= (max - min));
}

static inline bool needs_alignment(uint32_t val, uint32_t a)
{
	if (a == 0 || IS_ALIGNED(val, a))
		return false;

	return true;
}

int virtio_video_alloc_vbufs(struct virtio_video *vv);
void virtio_video_free_vbufs(struct virtio_video *vv);
int virtio_video_alloc_events(struct virtio_video *vv, size_t num);

int virtio_video_device_init(struct virtio_video *vv, void *input_buf,
			     void *output_buf);
void virtio_video_device_deinit(struct virtio_video *vv);

void virtio_video_stream_id_get(struct virtio_video *vv,
				struct virtio_video_stream *stream,
				uint32_t *id);
void virtio_video_stream_id_put(struct virtio_video *vv, uint32_t id);
void virtio_video_resource_id_get(struct virtio_video *vv, uint32_t *id);
void virtio_video_resource_id_put(struct virtio_video *vv, uint32_t id);

int virtio_video_cmd_stream_create(struct virtio_video *vv, uint32_t stream_id,
				   enum virtio_video_format format,
				   const char *tag);
int virtio_video_cmd_stream_destroy(struct virtio_video *vv,
				    uint32_t stream_id);
int virtio_video_cmd_stream_drain(struct virtio_video *vv, uint32_t stream_id);
int virtio_video_cmd_resource_create_page(
	struct virtio_video *vv, uint32_t stream_id, uint32_t resource_id,
	uint32_t queue_type, unsigned int num_planes, unsigned int *num_entries,
	struct virtio_video_mem_entry *ents);
int virtio_video_cmd_resource_create_object(
	struct virtio_video *vv, uint32_t stream_id, uint32_t resource_id,
	uint32_t queue_type, unsigned int num_planes, struct vb2_plane *planes,
	struct virtio_video_object_entry *ents);
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
int virtio_video_cmd_resource_create_page_mtk(
	struct virtio_video *vv, uint32_t stream_id,
	unsigned int num_entries,
	struct virtio_video_mem_entry *ents);
#endif
int virtio_video_cmd_resource_destroy_all(struct virtio_video *vv,
					  struct virtio_video_stream *stream,
					  uint32_t queue_type);
int virtio_video_cmd_resource_queue(struct virtio_video *vv, uint32_t stream_id,
				    struct virtio_video_buffer *virtio_vb,
				    uint32_t data_size[], uint8_t num_data_size,
				    uint32_t queue_type);
int virtio_video_cmd_queue_clear(struct virtio_video *vv,
				 struct virtio_video_stream *stream,
				 uint32_t queue_type);
int virtio_video_query_capability(struct virtio_video *vv, void *resp_buf,
				  size_t resp_size, uint32_t queue_type);
int virtio_video_query_control_profile(struct virtio_video *vv, void *resp_buf,
				       size_t resp_size, uint32_t format);
int virtio_video_query_control_level(struct virtio_video *vv, void *resp_buf,
				     size_t resp_size, uint32_t format);
int virtio_video_cmd_set_params(struct virtio_video *vv,
				struct virtio_video_stream *stream,
				struct video_format_info *format_info,
				uint32_t queue_type);
int virtio_video_cmd_get_params(struct virtio_video *vv,
				struct virtio_video_stream *stream,
				uint32_t queue_type);
int virtio_video_cmd_set_control(struct virtio_video *vv,
				 uint32_t stream_id,
				 uint32_t control, uint32_t val);
int virtio_video_cmd_get_control(struct virtio_video *vv,
				 struct virtio_video_stream *stream,
				 uint32_t ctrl);

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
struct mtk_venc_multi_ref {
	__u32	multi_ref_en;
	__u32	intra_period;
	__u32	superp_period;
	__u32	superp_ref_type;
	__u32	ref0_distance;
	__u32	ref1_dsitance;
	__u32	max_distance;
	__u32	reserved;
};

struct mtk_venc_vui_info {
	__u32	aspect_ratio_idc;
	__u32	sar_width;
	__u32	sar_height;
	__u32	reserved;
};

struct mtk_color_desc {
	__u32	color_primaries;
	__u32	transform_character;
	__u32	matrix_coeffs;
	__u32	display_primaries_x[3];
	__u32	display_primaries_y[3];
	__u32	white_point_x;
	__u32	white_point_y;
	__u32	max_display_mastering_luminance;
	__u32	min_display_mastering_luminance;
	__u32	max_content_light_level;
	__u32	max_pic_light_level;
	__u32	hdr_type;
	__u32	full_range;
	__u32	reserved;
};

struct venc_resolution_change {
	__u32 width;
	__u32 height;
	__u32 framerate;
	__u32 resolutionchange;
};

struct mtk_venc_visual_quality {
	__s32	quant;
	__s32	rd;
};

struct mtk_venc_init_qp {
	__s32	enable;
	__s32	qpi;
	__s32	qpp;
	__s32	qpb;
};

struct mtk_venc_frame_qp_range {
	__s32	enable;
	__s32	max;
	__s32	min;
};

int virtio_video_cmd_get_control_mtk_integer(struct virtio_video *vv,
					     struct virtio_video_stream *stream,
					     uint32_t id, int32_t *value);
int virtio_video_cmd_get_control_mtk_u32(struct virtio_video *vv,
					 struct virtio_video_stream *stream,
					 uint32_t id, uint32_t *value,
					 size_t value_len);
int virtio_video_cmd_get_control_mtk_string(struct virtio_video *vv,
					 struct virtio_video_stream *stream,
					 uint32_t id, char *value);
int virtio_video_cmd_set_control_mtk_integer(struct virtio_video *vv,
					     uint32_t stream_id, uint32_t id,
					     int32_t value);
int virtio_video_cmd_set_control_mtk_string(struct virtio_video *vv,
					    uint32_t stream_id,
					    uint32_t id, char *value);
int virtio_video_cmd_set_control_mtk_u32(struct virtio_video *vv,
					 uint32_t stream_id,
					 uint32_t id, uint32_t *value,
					 size_t value_len);

void virtio_video_queue_error_event(struct virtio_video_stream *stream);
#endif

void virtio_video_queue_res_chg_event(struct virtio_video_stream *stream);
void virtio_video_queue_eos_event(struct virtio_video_stream *stream);
void virtio_video_cmd_ack(struct virtqueue *vq);
void virtio_video_event_ack(struct virtqueue *vq);
void virtio_video_dequeue_cmd_func(struct work_struct *work);
void virtio_video_dequeue_event_func(struct work_struct *work);
void virtio_video_buf_done(struct virtio_video_buffer *virtio_vb,
			   uint32_t flags, uint64_t timestamp, uint32_t size);
int virtio_video_buf_plane_init(uint32_t idx, uint32_t resource_id,
				struct virtio_video_device *vvd,
				struct virtio_video_stream *stream,
				struct vb2_buffer *vb);
void virtio_video_mark_drain_complete(struct virtio_video_stream *stream,
				      struct vb2_v4l2_buffer *v4l2_vb);

int virtio_video_queue_setup(struct vb2_queue *vq, unsigned int *num_buffers,
			     unsigned int *num_planes, unsigned int sizes[],
			     struct device *alloc_devs[]);
int virtio_video_buf_prepare(struct vb2_buffer *vb);
int virtio_video_buf_init(struct vb2_buffer *vb);
void virtio_video_buf_cleanup(struct vb2_buffer *vb);
int virtio_video_querycap(struct file *file, void *fh,
			  struct v4l2_capability *cap);
int virtio_video_enum_framesizes(struct file *file, void *fh,
				 struct v4l2_frmsizeenum *f);
int virtio_video_enum_framemintervals(struct file *file, void *fh,
				      struct v4l2_frmivalenum *f);
int virtio_video_g_fmt(struct file *file, void *fh, struct v4l2_format *f);
int virtio_video_s_fmt(struct file *file, void *fh, struct v4l2_format *f);
int virtio_video_try_fmt(struct virtio_video_stream *stream,
			 struct v4l2_format *f);
int virtio_video_reqbufs(struct file *file, void *priv,
			 struct v4l2_requestbuffers *rb);
int virtio_video_subscribe_event(struct v4l2_fh *fh,
				 const struct v4l2_event_subscription *sub);

void virtio_video_free_caps_list(struct list_head *caps_list);
int virtio_video_parse_virtio_capability(struct virtio_video_device *vvd,
					  void *input_buf, void *output_buf);
void virtio_video_clean_capability(struct virtio_video_device *vvd);
int virtio_video_parse_virtio_control(struct virtio_video_device *vvd);
void virtio_video_clean_control(struct virtio_video_device *vvd);

uint32_t virtio_video_format_to_v4l2(uint32_t format);
uint32_t virtio_video_control_to_v4l2(uint32_t control);
uint32_t virtio_video_profile_to_v4l2(uint32_t profile);
uint32_t virtio_video_level_to_v4l2(uint32_t level);
uint32_t virtio_video_v4l2_format_to_virtio(uint32_t v4l2_format);
uint32_t virtio_video_v4l2_control_to_virtio(uint32_t v4l2_control);
uint32_t virtio_video_v4l2_profile_to_virtio(uint32_t v4l2_profile);
uint32_t virtio_video_v4l2_level_to_virtio(uint32_t v4l2_level);

struct video_format *find_video_format(struct list_head *fmts_list,
				       uint32_t fourcc);
void virtio_video_format_from_info(struct video_format_info *info,
				   struct v4l2_pix_format_mplane *pix_mp);
void virtio_video_format_fill_default_info(struct video_format_info *dst_info,
					   struct video_format_info *src_info);

int virtio_video_g_selection(struct file *file, void *fh,
			     struct v4l2_selection *sel);
int virtio_video_s_selection(struct file *file, void *fh,
			     struct v4l2_selection *sel);

#endif /* _VIRTIO_VIDEO_H */
