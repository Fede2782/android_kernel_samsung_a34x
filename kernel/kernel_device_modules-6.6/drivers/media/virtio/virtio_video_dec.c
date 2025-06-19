// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/* Decoder for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 */

#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>

#include "virtio_video.h"
#include "virtio_video_dec.h"

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
#include "virtio_video_util.h"
#endif
static void virtio_video_dec_buf_queue(struct vb2_buffer *vb)
{
	int i, ret;
	struct vb2_buffer *src_buf;
	struct vb2_v4l2_buffer *src_vb;
	struct virtio_video_buffer *virtio_vb;
	uint32_t data_size[VB2_MAX_PLANES] = {0};
	struct vb2_v4l2_buffer *v4l2_vb = to_vb2_v4l2_buffer(vb);
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;

	v4l2_m2m_buf_queue(stream->fh.m2m_ctx, v4l2_vb);

	if ((stream->state != STREAM_STATE_INIT) ||
	    !V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		return;

	src_vb = v4l2_m2m_next_src_buf(stream->fh.m2m_ctx);
	if (!src_vb) {
		v4l2_err(&vv->v4l2_dev, "no src buf during initialization\n");
		return;
	}

	src_buf = &src_vb->vb2_buf;
	for (i = 0; i < src_buf->num_planes; ++i)
		data_size[i] = src_buf->planes[i].bytesused;

	virtio_vb = to_virtio_vb(src_buf);

	ret = virtio_video_cmd_resource_queue(vv, stream->stream_id,
					      virtio_vb, data_size,
					      src_buf->num_planes,
					      VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to queue an src buffer\n");
		return;
	}

	virtio_vb->queued = true;
	stream->src_cleared = false;
	src_vb = v4l2_m2m_src_buf_remove(stream->fh.m2m_ctx);
}

static int virtio_video_dec_start_streaming(struct vb2_queue *vq,
					    unsigned int count)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);

	if (!V4L2_TYPE_IS_OUTPUT(vq->type) &&
	    stream->state >= STREAM_STATE_INIT)
		stream->state = STREAM_STATE_RUNNING;

	return 0;
}

static void virtio_video_dec_stop_streaming(struct vb2_queue *vq)
{
	int ret, queue_type;
	bool *cleared;
	bool is_v4l2_output = V4L2_TYPE_IS_OUTPUT(vq->type);
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct vb2_v4l2_buffer *v4l2_vb;

	if (is_v4l2_output) {
		cleared = &stream->src_cleared;
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_INPUT;
	} else {
		cleared = &stream->dst_cleared;
		queue_type = VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT;
	}

	ret = virtio_video_cmd_queue_clear(vv, stream, queue_type);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "failed to clear queue\n");
		return;
	}

	ret = wait_event_timeout(vv->wq, *cleared, 5 * HZ);
	if (ret == 0) {
		v4l2_err(&vv->v4l2_dev, "timed out waiting for queue clear\n");
		return;
	}

	for (;;) {
		if (is_v4l2_output)
			v4l2_vb = v4l2_m2m_src_buf_remove(stream->fh.m2m_ctx);
		else
			v4l2_vb = v4l2_m2m_dst_buf_remove(stream->fh.m2m_ctx);
		if (!v4l2_vb)
			break;
		v4l2_m2m_buf_done(v4l2_vb, VB2_BUF_STATE_ERROR);
	}
}

static const struct vb2_ops virtio_video_dec_qops = {
	.queue_setup	 = virtio_video_queue_setup,
	.buf_init	 = virtio_video_buf_init,
	.buf_prepare	 = virtio_video_buf_prepare,
	.buf_cleanup	 = virtio_video_buf_cleanup,
	.buf_queue	 = virtio_video_dec_buf_queue,
	.start_streaming = virtio_video_dec_start_streaming,
	.stop_streaming  = virtio_video_dec_stop_streaming,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
};

static int virtio_video_dec_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct virtio_video_stream *stream = ctrl2stream(ctrl);
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	struct virtio_video_device *vd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vd->vv;
#endif

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE: {
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
		ret = virtio_video_cmd_get_control_mtk_integer(
			vv, stream, ctrl->id, &ctrl->val);
		if (ret < 0) {
			virtio_v4l2_err("[%d]failed to get control(id=0x%x)\n",
				stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		stream->out_info.min_buffers = ctrl->val;
		virtio_v4l2_debug(VIRTIO_DBG_L1,
			"[%d]capture, dpb:%d\n", stream->stream_id, stream->out_info.min_buffers);
#else
		if (stream->state >= STREAM_STATE_METADATA)
			ctrl->val = stream->out_info.min_buffers;
		else
			ctrl->val = 0;
#endif
		break;
	}

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	case V4L2_CID_MPEG_MTK_COLOR_DESC: {
		ret = virtio_video_cmd_get_control_mtk_u32(
			vv, stream, ctrl->id, ctrl->p_new.p_u32,
			sizeof(struct mtk_color_desc));
		if (ret < 0) {
			virtio_v4l2_err(
				"[%d]failed to get control(id=0x%x)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	}
	case V4L2_CID_VDEC_SLC_SUPPORT_VER:
	case V4L2_CID_MPEG_MTK_FIX_BUFFERS:
	case V4L2_CID_MPEG_MTK_INTERLACING_FIELD_SEQ:
	case V4L2_CID_MPEG_MTK_INTERLACING: {
		ret = virtio_video_cmd_get_control_mtk_integer(
			vv, stream, ctrl->id, &ctrl->val);
		if (ret < 0) {
			virtio_v4l2_err(
				"[%d]failed to get control(id=0x%x)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	}
#endif
	default:
		ret = -EINVAL;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
static int virtio_video_dec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct virtio_video_stream *stream = ctrl2stream(ctrl);
	struct virtio_video_device *vd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vd->vv;

	virtio_v4l2_debug_enter();

	switch (ctrl->id) {
	case V4L2_CID_MPEG_MTK_CRC_PATH:
	case V4L2_CID_MPEG_MTK_GOLDEN_PATH:
		ret = virtio_video_cmd_set_control_mtk_string(
			vv, stream->stream_id, ctrl->id, ctrl->p_new.p_char);
		if (ret < 0) {
			virtio_v4l2_err(
				 "[%d]failed to set control(id=%d)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_DECODE_MODE:
	case V4L2_CID_MPEG_MTK_SEC_DECODE:
	case V4L2_CID_MPEG_MTK_SET_DECODE_ERROR_HANDLE_MODE:
	case V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER:
	case V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME:
	case V4L2_CID_MPEG_MTK_OPERATING_RATE:
	case V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT:
	case V4L2_CID_MPEG_MTK_REAL_TIME_PRIORITY:
		ret = virtio_video_cmd_set_control_mtk_integer(
			vv, stream->stream_id, ctrl->id, ctrl->val);
		if (ret < 0) {
			virtio_v4l2_err(
				 "[%d]failed to set control(id=%d)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}
	virtio_v4l2_debug_leave();
	return ret;
}
#endif

static const struct v4l2_ctrl_ops virtio_video_dec_ctrl_ops = {
	.g_volatile_ctrl	= virtio_video_dec_g_volatile_ctrl,
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	.s_ctrl			= virtio_video_dec_s_ctrl,
#endif
};

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
void virtio_video_dec_custom_ctrls_check(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_config *cfg)
{
	v4l2_ctrl_new_custom(hdl, cfg, NULL);

	if (hdl->error) {
		virtio_v4l2_debug(VIRTIO_DBG_L0, "Adding control failed %s %x %d",
			cfg->name, cfg->id, hdl->error);
	} else {
		virtio_v4l2_debug(VIRTIO_DBG_L4, "Adding control %s %x %d",
			cfg->name, cfg->id, hdl->error);
	}
}

static int mtk_dec_ctrls_setup(struct virtio_video_stream *stream)
{
	struct v4l2_ctrl_handler *handler = &stream->ctrl_handler;
	struct v4l2_ctrl_config cfg;
	const struct v4l2_ctrl_ops *ops = &virtio_video_dec_ctrl_ops;

	/* g_volatile_ctrl */
	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_FIX_BUFFERS;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "Video fix buffers";
	cfg.min = 0;
	cfg.max = 0xF;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_INTERLACING;
	cfg.type = V4L2_CTRL_TYPE_BOOLEAN;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "MTK Query Interlacing";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_INTERLACING_FIELD_SEQ;
	cfg.type = V4L2_CTRL_TYPE_BOOLEAN;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "MTK Query Interlacing FieldSeq";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_COLOR_DESC;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "MTK vdec Color Description for HDR";
	cfg.min = 0;
	cfg.max = 0xffffffff;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	cfg.dims[0] = (sizeof(struct mtk_color_desc)/sizeof(u32));
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_VDEC_SLC_SUPPORT_VER;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "MTK vdec SLC support ver";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	/* s_ctrl */
	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_DECODE_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video decode mode";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_SEC_DECODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video Sec Decode path";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video fixed maximum frame size";
	cfg.min = 0;
	cfg.max = 65535;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	/* width/height/mode */
	cfg.dims[0] = 3;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_CRC_PATH;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video crc path";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_GOLDEN_PATH;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video golden path";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Wait key frame";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_SET_DECODE_ERROR_HANDLE_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Decode Error Handle Mode";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_OPERATING_RATE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Vdec Operating Rate";
	cfg.min = 0;
	cfg.max = 4096;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_REAL_TIME_PRIORITY;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Vdec Real Time Priority";
	cfg.min = -1;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video queued frame buf count";
	cfg.min = 0;
	cfg.max = 64;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_dec_custom_ctrls_check(handler, &cfg);

	if (stream->ctrl_handler.error) {
		virtio_v4l2_err("Adding control failed %d",
					 stream->ctrl_handler.error);
		return stream->ctrl_handler.error;
	}
	return 0;
}
#endif

int virtio_video_dec_init_ctrls(struct virtio_video_stream *stream)
{
	struct v4l2_ctrl *ctrl;

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	v4l2_ctrl_handler_init(&stream->ctrl_handler, MTK_MAX_CTRLS_HINT);
#else
	v4l2_ctrl_handler_init(&stream->ctrl_handler, 1);
#endif

	ctrl = v4l2_ctrl_new_std(&stream->ctrl_handler,
				&virtio_video_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				MIN_BUFS_MIN, MIN_BUFS_MAX, MIN_BUFS_STEP,
				MIN_BUFS_DEF);

	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	mtk_dec_ctrls_setup(stream);
#endif

	if (stream->ctrl_handler.error) {
		int err = stream->ctrl_handler.error;

		v4l2_ctrl_handler_free(&stream->ctrl_handler);
		return err;
	}

	v4l2_ctrl_handler_setup(&stream->ctrl_handler);

	return 0;
}

int virtio_video_dec_init_queues(void *priv, struct vb2_queue *src_vq,
				 struct vb2_queue *dst_vq)
{
	int ret;
	struct virtio_video_stream *stream = priv;
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct device *dev = vv->v4l2_dev.dev;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->drv_priv = stream;
	src_vq->buf_struct_size = sizeof(struct virtio_video_buffer);
	src_vq->ops = &virtio_video_dec_qops;
	src_vq->mem_ops = virtio_video_mem_ops(vv);
	src_vq->min_buffers_needed = stream->in_info.min_buffers;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &stream->vq_mutex;
	src_vq->gfp_flags = virtio_video_gfp_flags(vv);
	src_vq->dev = dev;
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	src_vq->allow_zero_bytesused = true;
#endif

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = stream;
	dst_vq->buf_struct_size = sizeof(struct virtio_video_buffer);
	dst_vq->ops = &virtio_video_dec_qops;
	dst_vq->mem_ops = virtio_video_mem_ops(vv);
	dst_vq->min_buffers_needed = stream->out_info.min_buffers;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &stream->vq_mutex;
	dst_vq->gfp_flags = virtio_video_gfp_flags(vv);
	dst_vq->dev = dev;
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	dst_vq->allow_zero_bytesused = true;
#endif

	return vb2_queue_init(dst_vq);
}

static int virtio_video_try_decoder_cmd(struct file *file, void *fh,
					struct v4l2_decoder_cmd *cmd)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	if (stream->state == STREAM_STATE_DRAIN)
		return -EBUSY;

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
	case V4L2_DEC_CMD_START:
		if (cmd->flags != 0) {
			v4l2_err(&vv->v4l2_dev, "flags=%u are not supported",
				 cmd->flags);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int virtio_video_decoder_cmd(struct file *file, void *fh,
				    struct v4l2_decoder_cmd *cmd)
{
	int ret;
	struct vb2_queue *src_vq, *dst_vq;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;
	int current_state;

	ret = virtio_video_try_decoder_cmd(file, fh, cmd);
	if (ret < 0)
		return ret;

	dst_vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx,
				 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_START:
		vb2_clear_last_buffer_dequeued(dst_vq);
		if (stream->state == STREAM_STATE_STOPPED) {
			stream->state = STREAM_STATE_RUNNING;
		} else {
			v4l2_warn(&vv->v4l2_dev, "state(%d) is not STOPPED\n",
				  stream->state);
		}
		break;
	case V4L2_DEC_CMD_STOP:
		src_vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx,
					 V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

		if (!vb2_is_streaming(src_vq)) {
			v4l2_dbg(1, vv->debug,
				 &vv->v4l2_dev, "output is not streaming\n");
			return 0;
		}

		if (!vb2_is_streaming(dst_vq)) {
			v4l2_dbg(1, vv->debug,
				 &vv->v4l2_dev, "capture is not streaming\n");
			return 0;
		}

		current_state = stream->state;
		stream->state = STREAM_STATE_DRAIN;
		ret = virtio_video_cmd_stream_drain(vv, stream->stream_id);
		if (ret) {
			stream->state = current_state;
			v4l2_err(&vv->v4l2_dev, "failed to drain stream\n");
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int virtio_video_dec_enum_fmt_vid_cap(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format_info *info = NULL;
	struct video_format *fmt = NULL;
	unsigned long input_mask = 0;
	int idx = 0, bit_num = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_output_fmts)
		return -EINVAL;

	info = &stream->in_info;
	list_for_each_entry(fmt, &vvd->input_fmt_list, formats_list_entry) {
		if (info->fourcc_format == fmt->desc.format) {
			input_mask = fmt->desc.mask;
			break;
		}
	}

	if (input_mask == 0)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->output_fmt_list, formats_list_entry) {
		if (test_bit(bit_num, &input_mask)) {
			if (f->index == idx) {
				f->pixelformat = fmt->desc.format;
				return 0;
			}
			idx++;
		}
		bit_num++;
	}
	return -EINVAL;
}


static int virtio_video_dec_enum_fmt_vid_out(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	int idx = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_input_fmts)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->input_fmt_list, formats_list_entry) {
		if (f->index == idx) {
			f->pixelformat = fmt->desc.format;
			return 0;
		}
		idx++;
	}
	return -EINVAL;
}

static int virtio_video_dec_s_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	int ret;
	struct virtio_video_stream *stream = file2stream(file);

	ret = virtio_video_s_fmt(file, fh, f);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (stream->state == STREAM_STATE_IDLE)
			stream->state = STREAM_STATE_INIT;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
int transfer_general_buffer(struct vb2_queue *vq, struct vb2_buffer *vb)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct virtio_video_buffer *virtio_vb = to_virtio_vb(vb);
	struct virtio_video_mem_entry *ents;
	struct dma_buf_attachment *buf_att;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct scatterlist *sg;
	unsigned int num_ents;
	int ret, i;

	virtio_v4l2_debug_enter();
	dmabuf = dma_buf_get(virtio_vb->general_buf.gen_buf_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		virtio_v4l2_err("[%d]dma_buf_get:%d fail ret %ld",
			stream->stream_id, virtio_vb->general_buf.gen_buf_fd, PTR_ERR(dmabuf));
		return -EINVAL;
	}
	virtio_vb->general_buf.dmabuf = dmabuf;

	buf_att = dma_buf_attach(dmabuf, vq->dev);
	if (IS_ERR_OR_NULL(buf_att)) {
		virtio_v4l2_err("[%d]attach fail ret %ld", stream->stream_id, PTR_ERR(buf_att));
		ret = -EINVAL;
		goto err_out_put;
	}

	sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(sgt)) {
		virtio_v4l2_err("[%d]map attachment fail ret %ld", stream->stream_id, PTR_ERR(sgt));
		ret = -EINVAL;
		goto err_out_detach;
	}

	ents = kcalloc(sgt->nents, sizeof(*ents), GFP_KERNEL);
	if (!ents) {
		virtio_v4l2_err("[%d]kcalloc ents fail", stream->stream_id);
		ret = -ENOMEM;
		goto err_out_unmap;
	}

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		ents[i].addr = cpu_to_le64(vv->has_iommu
						 ? sg_dma_address(sg)
						 : sg_phys(sg));

		ents[i].length = cpu_to_le32(sg->length);
	}

	num_ents = sgt->nents;
	ret = virtio_video_cmd_resource_create_page_mtk(
	    vv, stream->stream_id, num_ents, ents);

	if (ret) {
		virtio_v4l2_err("[%d]create general buffer page fail", stream->stream_id);
		goto err_free_ents;
	}


	virtio_vb->general_buf.buf_att = buf_att;
	virtio_vb->general_buf.sgt = sgt;

	virtio_v4l2_debug_leave();

	return 0;
err_free_ents:
	kfree(ents);
err_out_unmap:
	dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);
err_out_detach:
	dma_buf_detach(dmabuf, buf_att);
err_out_put:
	dma_buf_put(dmabuf);
	virtio_vb->general_buf.dmabuf = NULL;
	return ret;
}

static int v4l2_mtk_vdec_qbuf(struct file *file, void *priv,
			      struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_buffer *video_buf;
	int ret;

	virtio_v4l2_debug_enter();

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx, buf->type);
		if (vq == NULL) {
			virtio_v4l2_err("[%d]Error! vq is null!", stream->stream_id);
			return -EINVAL;
		}
		if (buf->index >= vq->num_buffers) {
			virtio_v4l2_err("[%d] buffer index %d out of range %d",
				stream->stream_id, buf->index, vq->num_buffers);
			return -EINVAL;
		}

		if (IS_ERR_OR_NULL(buf->m.planes) || buf->length == 0) {
			virtio_v4l2_err(
				"[%d]buffer index %d planes address %p 0x%llx or length %d invalid",
				stream->stream_id, buf->index, buf->m.planes,
				(unsigned long long)buf->m.planes, buf->length);
		return -EINVAL;
	}
		vb = vq->bufs[buf->index];
		video_buf = to_virtio_vb(vb);

		if (buf->reserved != 0xFFFFFFFF) {
			video_buf->general_buf.gen_buf_fd = buf->reserved;
			virtio_v4l2_debug(VIRTIO_DBG_L1,
				"[%d]gen_buf_fd:%d\n", stream->stream_id, buf->reserved);
			ret = transfer_general_buffer(vq, vb);
			if (ret) {
				virtio_v4l2_err(
					 "[%d]failed to transfer general buffer, ret:%d\n",
					 stream->stream_id, ret);
				return ret;
			}
		} else {
			video_buf->general_buf.gen_buf_fd = -1;
		}
	}

	return v4l2_m2m_ioctl_qbuf(file, priv, buf);
}

static int v4l2_mtk_vdec_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_buffer *video_buf;

	virtio_v4l2_debug_enter();

	ret = v4l2_m2m_ioctl_dqbuf(file, priv, buf);
	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
		ret == 0) {
		vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx, buf->type);
		if (vq == NULL) {
			virtio_v4l2_err("[%d]Error! vq is null!", stream->stream_id);
			return -EINVAL;
		}
		if (buf->index >= vq->num_buffers) {
			virtio_v4l2_err("[%d] buffer index %d out of range %d",
				stream->stream_id, buf->index, vq->num_buffers);
			return -EINVAL;
		}
		vb = vq->bufs[buf->index];
		video_buf = to_virtio_vb(vb);

		if (video_buf->general_buf.dmabuf) {
			buf->reserved = video_buf->general_buf.gen_buf_fd;
			virtio_v4l2_debug(VIRTIO_DBG_L1, "[%d]gen_buf_fd:%d\n",
				stream->stream_id, buf->reserved);
			dma_buf_unmap_attachment(video_buf->general_buf.buf_att,
				video_buf->general_buf.sgt, DMA_TO_DEVICE);
			dma_buf_detach(video_buf->general_buf.dmabuf,
				video_buf->general_buf.buf_att);
			dma_buf_put(video_buf->general_buf.dmabuf);
		} else {
			buf->reserved = 0xFFFFFFFF;
		}
	}
	virtio_v4l2_debug_leave();

	return ret;
}

int v4l2_mtk_vdec_streamoff(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	int ret = 0;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	virtio_v4l2_debug(VIRTIO_DBG_L1, "[%d]type:%d\n", stream->stream_id, type);

	ret = v4l2_m2m_ioctl_streamoff(file, priv, type);
	if (ret) {
		virtio_v4l2_err("[%d]failed to streamoff\n", stream->stream_id);
		return ret;
	}

	/* Until STREAMOFF is called on the CAPTURE queue
	 * (acknowledging the event), the driver operates
	 * as if the resolution hasn't changed yet, i.e.
	 * VIDIOC_G_FMT< etc. return previous resolution.
	 * So update format here.
	 */
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = virtio_video_cmd_get_params(vv, stream,
					   VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
		if (ret)
			virtio_v4l2_err(
				"[%d]failed to get stream in params when streamoff\n", stream->stream_id);
	}
	return ret;
}
#endif

static const struct v4l2_ioctl_ops virtio_video_dec_ioctl_ops = {
	.vidioc_querycap	= virtio_video_querycap,

	.vidioc_enum_fmt_vid_cap = virtio_video_dec_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap	= virtio_video_dec_s_fmt,

	.vidioc_enum_fmt_vid_cap	= virtio_video_dec_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= virtio_video_dec_enum_fmt_vid_out,

	.vidioc_g_fmt_vid_cap_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= virtio_video_dec_s_fmt,

	.vidioc_enum_fmt_vid_out = virtio_video_dec_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out	= virtio_video_dec_s_fmt,

	.vidioc_g_fmt_vid_out_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out_mplane	= virtio_video_dec_s_fmt,

	.vidioc_g_selection = virtio_video_g_selection,
	.vidioc_s_selection = virtio_video_s_selection,

	.vidioc_try_decoder_cmd	= virtio_video_try_decoder_cmd,
	.vidioc_decoder_cmd	= virtio_video_decoder_cmd,
	.vidioc_enum_frameintervals = virtio_video_enum_framemintervals,
	.vidioc_enum_framesizes = virtio_video_enum_framesizes,

	.vidioc_reqbufs		= virtio_video_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	.vidioc_qbuf		= v4l2_mtk_vdec_qbuf,
	.vidioc_dqbuf		= v4l2_mtk_vdec_dqbuf,
#else
	.vidioc_qbuf		= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
#endif
	.vidioc_prepare_buf	= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	.vidioc_streamoff	= v4l2_mtk_vdec_streamoff,
#else
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,
#endif

	.vidioc_subscribe_event = virtio_video_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

int virtio_video_dec_init(struct video_device *vd)
{
	vd->ioctl_ops = &virtio_video_dec_ioctl_ops;
	strscpy(vd->name, "stateful-decoder", sizeof(vd->name));

	return 0;
}

MODULE_IMPORT_NS(DMA_BUF);
