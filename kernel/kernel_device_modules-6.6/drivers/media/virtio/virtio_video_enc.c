// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/* Encoder for virtio video device.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 */

#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>

#include "virtio_video.h"
#include "virtio_video_enc.h"
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
#include "virtio_video_util.h"
#endif

static void virtio_video_enc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_vb = to_vb2_v4l2_buffer(vb);
	struct virtio_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(stream->fh.m2m_ctx, v4l2_vb);

}

static int virtio_video_enc_start_streaming(struct vb2_queue *vq,
					unsigned int count)
{
	struct virtio_video_stream *stream = vb2_get_drv_priv(vq);
	bool input_queue = V4L2_TYPE_IS_OUTPUT(vq->type);

	if (stream->state == STREAM_STATE_INIT ||
	    (!input_queue && stream->state == STREAM_STATE_RESET) ||
	    (input_queue && stream->state == STREAM_STATE_STOPPED))
		stream->state = STREAM_STATE_RUNNING;

	return 0;
}

static void virtio_video_enc_stop_streaming(struct vb2_queue *vq)
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

	if (is_v4l2_output)
		stream->state = STREAM_STATE_STOPPED;
	else
		stream->state = STREAM_STATE_RESET;
}

static const struct vb2_ops virtio_video_enc_qops = {
	.queue_setup	 = virtio_video_queue_setup,
	.buf_init	 = virtio_video_buf_init,
	.buf_cleanup	 = virtio_video_buf_cleanup,
	.buf_queue	 = virtio_video_enc_buf_queue,
	.start_streaming = virtio_video_enc_start_streaming,
	.stop_streaming  = virtio_video_enc_stop_streaming,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
};

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
static int virtio_video_enc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct virtio_video_stream *stream = ctrl2stream(ctrl);
	struct virtio_video_device *vd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vd->vv;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
	case V4L2_CID_MPEG_VIDEO_BITRATE:
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE:
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	case V4L2_CID_MPEG_MTK_ENCODE_SCENARIO:
	case V4L2_CID_MPEG_MTK_ENCODE_NONREFP:
	case V4L2_CID_MPEG_MTK_ENCODE_NONREFP_FREQ:
	case V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE:
	case V4L2_CID_MPEG_MTK_ENCODE_RFS_ON:
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
	case V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE:
	case V4L2_CID_MPEG_MTK_ENCODE_ROI_ON:
	case V4L2_CID_MPEG_MTK_MAX_WIDTH:
	case V4L2_CID_MPEG_MTK_MAX_HEIGHT:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP:
	case V4L2_CID_MPEG_MTK_SEC_ENCODE:
	case V4L2_CID_MPEG_MTK_ENCODE_MAX_LTR_FRAMES:
	case V4L2_CID_MPEG_MTK_ENCODE_WPP_MODE:
	case V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_MODE:
	case V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_WFD:
	case V4L2_CID_MPEG_MTK_ENCODE_ENABLE_HIGHQUALITY:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_MAX_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_MIN_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_I_P_QP_DELTA:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_FRAME_LEVEL_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_RC_QP_CONTROL_MODE:
	case V4L2_CID_MPEG_MTK_ENCODE_ENABLE_DUMMY_NAL:
	case V4L2_CID_MPEG_MTK_CALLING_PID:
	case V4L2_CID_MPEG_MTK_ENCODE_SLICE_CNT:
	case V4L2_CID_MPEG_MTK_ENCODE_QPVBR:
	case V4L2_CID_MPEG_MTK_ENCODE_CHROMA_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_MB_RC_TK_SPD:
	case V4L2_CID_MPEG_MTK_ENCODE_FRM_QP_LTR:
	case V4L2_CID_MPEG_MTK_ENCODE_VISUAL_QUALITY:
	case V4L2_CID_MPEG_MTK_ENCODE_INIT_QP:
	case V4L2_CID_MPEG_MTK_ENCODE_FRAME_QP_RANGE:
	case V4L2_CID_MPEG_MTK_ENCODE_ENABLE_MLVEC_MODE:
		ret = virtio_video_cmd_set_control_mtk_integer(
			vv, stream->stream_id, ctrl->id, ctrl->val);
		if (ret < 0) {
			virtio_v4l2_err("[%d]failed to set control(id=%d)\n",
				stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_COLOR_DESC:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE:
	case V4L2_CID_MPEG_VIDEO_ENABLE_TSVC:
	case V4L2_CID_MPEG_MTK_ENCODE_MULTI_REF:
	case V4L2_CID_MPEG_MTK_ENCODE_TEMPORAL_LAYER_COUNT:
		ret = virtio_video_cmd_set_control_mtk_u32(vv,
				stream->stream_id,
				ctrl->id, ctrl->p_new.p_u32,
				sizeof(struct mtk_color_desc));
		if (ret < 0) {
			virtio_v4l2_err("[%d]failed to set control(id=%d)\n",
				stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_LOG:
	case V4L2_CID_MPEG_MTK_VCP_PROP:
		ret = virtio_video_cmd_set_control_mtk_string(
			vv, stream->stream_id, ctrl->id, ctrl->p_new.p_char);
		if (ret < 0) {
			virtio_v4l2_err("[%d]failed to set control(id=%d)\n",
				stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#else

static int virtio_video_enc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	struct virtio_video_stream *stream = ctrl2stream(ctrl);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	uint32_t control, value;

	control = virtio_video_v4l2_control_to_virtio(ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		ret = virtio_video_cmd_set_control(vv, stream->stream_id,
						   control, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		value = virtio_video_v4l2_level_to_virtio(ctrl->val);
		ret = virtio_video_cmd_set_control(vv, stream->stream_id,
						   control, value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		value = virtio_video_v4l2_profile_to_virtio(ctrl->val);
		ret = virtio_video_cmd_set_control(vv, stream->stream_id,
						   control, value);
		break;
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		ret = virtio_video_cmd_set_control(vv, stream->stream_id,
						   control, 1 /*ignored*/);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

static int virtio_video_enc_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct virtio_video_stream *stream = ctrl2stream(ctrl);
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	struct virtio_video_device *vd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vd->vv;
#endif
	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		if (stream->state >= STREAM_STATE_INIT)
			ctrl->val = stream->in_info.min_buffers;
		else
			ctrl->val = 0;
		break;
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	case V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE:
		ret = virtio_video_cmd_get_control_mtk_u32(
			vv, stream, ctrl->id, ctrl->p_new.p_u32,
			sizeof(struct venc_resolution_change));
		if (ret < 0) {
			virtio_v4l2_err(
				"[%d]failed to get control(id=0x%x)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP:
		ret = virtio_video_cmd_get_control_mtk_integer(
			vv, stream, ctrl->id, &ctrl->val);
		if (ret < 0) {
			virtio_v4l2_err(
				"[%d]failed to get control(id=0x%x)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_GET_LOG:
	case V4L2_CID_MPEG_MTK_GET_VCP_PROP:
		ret = virtio_video_cmd_get_control_mtk_string(
			vv, stream, ctrl->id, ctrl->p_new.p_char);
		if (ret < 0) {
			virtio_v4l2_err(
				"[%d]failed to get control(id=0x%x)\n", stream->stream_id, ctrl->id);
			ret = -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops virtio_video_enc_ctrl_ops = {
	.g_volatile_ctrl	= virtio_video_enc_g_volatile_ctrl,
	.s_ctrl			= virtio_video_enc_s_ctrl,
};

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
void virtio_video_enc_custom_ctrls_check(struct v4l2_ctrl_handler *hdl,
			const struct v4l2_ctrl_config *cfg, void *priv)
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

static const struct v4l2_ctrl_config mtk_enc_vui_sar_ctrl = {
	.ops = &virtio_video_enc_ctrl_ops,
	.id = V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE,
	.name = "Video encode vui sar description for Extended_SAR",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_WRITE_ONLY,
	.min = 0x00000000,
	.max = 0xffffffff,
	.step = 1,
	.def = 0,
	.dims = { sizeof(struct mtk_venc_vui_info)/sizeof(u32) }
};

static void mtk_enc_ctrls_setup(struct virtio_video_stream *stream)
{
	struct v4l2_ctrl_handler *handler = &stream->ctrl_handler;
	const struct v4l2_ctrl_ops *ops = &virtio_video_enc_ctrl_ops;
	struct v4l2_ctrl_config cfg;

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_BITRATE,
			  0, 400000000, 1, 20000000);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_B_FRAMES,
			  0, 3, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
			  0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
			  0, 51, 1, 51);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
			  0, 65535, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_GOP_SIZE,
			  0, 65535, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
			  0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
			0, 1, 1, 0);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		0, V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10,
		0, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		V4L2_MPEG_VIDEO_H264_LEVEL_6_2,
		0, V4L2_MPEG_VIDEO_H264_LEVEL_1_0);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		0, V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);
	v4l2_ctrl_new_std_menu(handler, ops,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
		0, V4L2_MPEG_VIDEO_HEVC_LEVEL_1);
	v4l2_ctrl_new_std_menu(handler, ops,
		V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		0, V4L2_MPEG_VIDEO_HEVC_TIER_MAIN);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		0, V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
		0, V4L2_MPEG_VIDEO_MPEG4_LEVEL_0);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
		0, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode nonrefp";
	cfg.min = 0;
	cfg.max = 65535;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	virtio_video_enc_custom_ctrls_check(handler,&mtk_enc_vui_sar_ctrl, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_SCENARIO;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode scenario";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_NONREFP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode nonrefp";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_NONREFP_FREQ;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode nonrefp";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode detect framerate";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RFS_ON;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode slice loss indication";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode slice loss indication";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode operation rate";
	cfg.min = 0;
	cfg.max = 2048;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ROI_ON;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode roi switch";
	cfg.min = 0;
	cfg.max = 8;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode heif grid size";
	cfg.min = 0;
	cfg.max = (3840<<16)+2176;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_COLOR_DESC;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode color description for HDR";
	cfg.min = 0x00000000;
	cfg.max = 0xffffffff;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	cfg.dims[0] = (sizeof(struct mtk_color_desc)/sizeof(u32));
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_MAX_WIDTH;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode max width";
	cfg.min = 0;
	cfg.max = 3840;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_MAX_HEIGHT;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode max height";
	cfg.min = 0;
	cfg.max = 3840;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "I-Frame QP Value";
	cfg.min = 0;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = 51;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "P-Frame QP Value";
	cfg.min = 0;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = 51;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "B-Frame QP Value";
	cfg.min = 0;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = 51;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_SEC_ENCODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video Sec Encode path";
	cfg.min = 0;
	cfg.max = 2;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_MAX_LTR_FRAMES;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode qp control mode";
	cfg.min = 0;
	cfg.max = 3;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_VIDEO_ENABLE_TSVC;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode tsvc";
	cfg.min = 0;
	cfg.max = 15;
	cfg.step = 1;
	cfg.def = 0;
	cfg.dims[0] = 2;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_MULTI_REF;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode multi ref";
	cfg.min = 0;
	cfg.max = 65535;
	cfg.step = 1;
	cfg.def = 0;
	cfg.dims[0] = sizeof(struct mtk_venc_multi_ref)/sizeof(u32);
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_WPP_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "encode wpp";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "encode low latency";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ENABLE_HIGHQUALITY;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode enable highquality";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_MAX_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode max qp";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_MIN_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode min qp";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_I_P_QP_DELTA;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode ip qp delta";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_FRAME_LEVEL_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode frame level qp";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RC_QP_CONTROL_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode qp control mode";
	cfg.min = 0;
	cfg.max = 8;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ENABLE_DUMMY_NAL;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode enable dummynal";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_LOW_LATENCY_WFD;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode enable lowlatencywfd";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_LOG;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video Log";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_VCP_PROP;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video VCP Property";
	cfg.min = 0;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_GET_LOG;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "Get Video Log";
	cfg.min = 0;
	cfg.max = LOG_PROPERTY_SIZE;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_GET_VCP_PROP;
	cfg.type = V4L2_CTRL_TYPE_STRING;
	cfg.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE;
	cfg.name = "Get Video VCP Property";
	cfg.min = 0;
	cfg.max = LOG_PROPERTY_SIZE;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_TEMPORAL_LAYER_COUNT;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video temporal layer count";
	cfg.min = 0;
	cfg.max = 4;
	cfg.step = 1;
	cfg.def = 0;
	cfg.dims[0] = 2;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_CALLING_PID;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video Caller Proccess ID";
	cfg.min = 0;
	cfg.max = 0x7fffffff;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	/* g_volatile_ctrl */
	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_READ_ONLY;
	cfg.name = "Video encode roi rc qp";
	cfg.min = 0;
	cfg.max = 2048;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_READ_ONLY;
	cfg.name = "Video encode resolution change";
	cfg.min = 0x00000000;
	cfg.max = 0x00ffffff;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	cfg.dims[0] = sizeof(struct venc_resolution_change)/sizeof(u32);
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_SLICE_CNT;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode set slice count";
	cfg.min = 1;
	cfg.max = 8;
	cfg.step = 1;
	cfg.def = 1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_QPVBR;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode QPVBR";
	cfg.min = -1;
	cfg.max = 255;
	cfg.step = 1;
	cfg.def = -1;
	cfg.dims[0] = 3;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);


	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_CHROMA_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode Chroma QP";
	cfg.min = -12;
	cfg.max = 99;
	cfg.step = 1;
	cfg.def = 99;
	cfg.dims[0] = 2;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_MB_RC_TK_SPD;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode MB RC Tracking Speed";
	cfg.min = -1;
	cfg.max = 63;
	cfg.step = 1;
	cfg.def = -1;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_FRM_QP_LTR;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode Frame QP limiter";
	cfg.min = -1;
	cfg.max = 30;
	cfg.step = 1;
	cfg.def = -1;
	cfg.dims[0] = 3;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);


	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_VISUAL_QUALITY;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode Visual Quality";
	cfg.min = -1;
	cfg.max = 63;
	cfg.step = 1;
	cfg.def = -1;
	cfg.dims[0] = (sizeof(struct mtk_venc_visual_quality)/sizeof(s32));
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_INIT_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode Initial QP";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.dims[0] = (sizeof(struct mtk_venc_init_qp)/sizeof(s32));
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_FRAME_QP_RANGE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode Frame QP Range";
	cfg.min = -1;
	cfg.max = 51;
	cfg.step = 1;
	cfg.def = -1;
	cfg.dims[0] = (sizeof(struct mtk_venc_frame_qp_range)/sizeof(s32));
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ENABLE_MLVEC_MODE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode inputDyanmicCtrl";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	virtio_video_enc_custom_ctrls_check(handler, &cfg, NULL);
}
#endif

int virtio_video_enc_init_ctrls(struct virtio_video_stream *stream)
{
	struct v4l2_ctrl *ctrl;
#if !IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct video_control_format *c_fmt = NULL;

	v4l2_ctrl_handler_init(&stream->ctrl_handler, 1);
#else

	v4l2_ctrl_handler_init(&stream->ctrl_handler, MTK_MAX_CTRLS_HINT);
#endif

	ctrl = v4l2_ctrl_new_std(&stream->ctrl_handler,
				&virtio_video_enc_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
				MIN_BUFS_MIN, MIN_BUFS_MAX, MIN_BUFS_STEP,
				MIN_BUFS_DEF);

	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	mtk_enc_ctrls_setup(stream);
#else
	list_for_each_entry(c_fmt, &vvd->controls_fmt_list,
			    controls_list_entry) {
		switch (c_fmt->format) {
		case V4L2_PIX_FMT_H264:
			if (c_fmt->profile)
				v4l2_ctrl_new_std_menu
					(&stream->ctrl_handler,
					 &virtio_video_enc_ctrl_ops,
					 V4L2_CID_MPEG_VIDEO_H264_PROFILE,
					 c_fmt->profile->max,
					 c_fmt->profile->skip_mask,
					 c_fmt->profile->min);

			if (c_fmt->level)
				v4l2_ctrl_new_std_menu
					(&stream->ctrl_handler,
					 &virtio_video_enc_ctrl_ops,
					 V4L2_CID_MPEG_VIDEO_H264_LEVEL,
					 c_fmt->level->max,
					 c_fmt->level->skip_mask,
					 c_fmt->level->min);
			break;
		default:
			v4l2_dbg(1, vv->debug,
				 &vv->v4l2_dev, "unsupported format\n");
			break;
		}
	}

	if (stream->control.bitrate) {
		v4l2_ctrl_new_std(&stream->ctrl_handler,
				  &virtio_video_enc_ctrl_ops,
				  V4L2_CID_MPEG_VIDEO_BITRATE,
				  // Set max to 1GBs to cover most use cases.
				  1, 1000000000,
				  1, stream->control.bitrate);
	}

	v4l2_ctrl_new_std(&stream->ctrl_handler,
			  &virtio_video_enc_ctrl_ops,
			  V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
			  0, 0, 0, 0);
#endif

	if (stream->ctrl_handler.error)
		return stream->ctrl_handler.error;

	v4l2_ctrl_handler_setup(&stream->ctrl_handler);

	return 0;
}

int virtio_video_enc_init_queues(void *priv, struct vb2_queue *src_vq,
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
	src_vq->ops = &virtio_video_enc_qops;
	src_vq->mem_ops = virtio_video_mem_ops(vv);
	src_vq->min_buffers_needed = stream->in_info.min_buffers;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &stream->vq_mutex;
	src_vq->gfp_flags = virtio_video_gfp_flags(vv);
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	src_vq->allow_zero_bytesused = true;
#endif
	src_vq->dev = dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = stream;
	dst_vq->buf_struct_size = sizeof(struct virtio_video_buffer);
	dst_vq->ops = &virtio_video_enc_qops;
	dst_vq->mem_ops = virtio_video_mem_ops(vv);
	dst_vq->min_buffers_needed = stream->out_info.min_buffers;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &stream->vq_mutex;
	dst_vq->gfp_flags = virtio_video_gfp_flags(vv);
#if IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	dst_vq->allow_zero_bytesused = true;
#endif
	dst_vq->dev = dev;

	return vb2_queue_init(dst_vq);
}

static int virtio_video_try_encoder_cmd(struct file *file, void *fh,
					struct v4l2_encoder_cmd *cmd)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	if (stream->state == STREAM_STATE_DRAIN)
		return -EBUSY;

	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
	case V4L2_ENC_CMD_START:
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

static int virtio_video_encoder_cmd(struct file *file, void *fh,
				    struct v4l2_encoder_cmd *cmd)
{
	int ret;
	struct vb2_queue *src_vq, *dst_vq;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = video_drvdata(file);
	struct virtio_video *vv = vvd->vv;

	ret = virtio_video_try_encoder_cmd(file, fh, cmd);
	if (ret < 0)
		return ret;

	dst_vq = v4l2_m2m_get_vq(stream->fh.m2m_ctx,
				 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	switch (cmd->cmd) {
	case V4L2_ENC_CMD_START:
		vb2_clear_last_buffer_dequeued(dst_vq);
		stream->state = STREAM_STATE_RUNNING;
		break;
	case V4L2_ENC_CMD_STOP:
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

		ret = virtio_video_cmd_stream_drain(vv, stream->stream_id);
		if (ret) {
			v4l2_err(&vv->v4l2_dev, "failed to drain stream\n");
			return ret;
		}

		stream->state = STREAM_STATE_DRAIN;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int virtio_video_enc_enum_fmt_vid_cap(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format *fmt = NULL;
	int idx = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_output_fmts)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->output_fmt_list, formats_list_entry) {
		if (f->index == idx) {
			f->pixelformat = fmt->desc.format;
			return 0;
		}
		idx++;
	}
	return -EINVAL;
}

static int virtio_video_enc_enum_fmt_vid_out(struct file *file, void *fh,
					     struct v4l2_fmtdesc *f)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct video_format_info *info = NULL;
	struct video_format *fmt = NULL;
	unsigned long output_mask = 0;
	int idx = 0, bit_num = 0;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	if (f->index >= vvd->num_input_fmts)
		return -EINVAL;

	info = &stream->out_info;
	list_for_each_entry(fmt, &vvd->output_fmt_list, formats_list_entry) {
		if (info->fourcc_format == fmt->desc.format) {
			output_mask = fmt->desc.mask;
			break;
		}
	}

	if (output_mask == 0)
		return -EINVAL;

	list_for_each_entry(fmt, &vvd->input_fmt_list, formats_list_entry) {
		if (test_bit(bit_num, &output_mask)) {
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

static int virtio_video_enc_s_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	int ret;
	struct virtio_video_stream *stream = file2stream(file);

	ret = virtio_video_s_fmt(file, fh, f);
	if (ret)
		return ret;

	if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (stream->state == STREAM_STATE_IDLE)
			stream->state = STREAM_STATE_INIT;
	}

	return 0;
}

#if !IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
static int virtio_video_enc_try_framerate(struct virtio_video_stream *stream,
					  unsigned int fps)
{
	int rate_idx;
	struct video_format_frame *frame = NULL;

	if (stream->current_frame == NULL)
		return -EINVAL;

	frame = stream->current_frame;
	for (rate_idx = 0; rate_idx < frame->frame.num_rates; rate_idx++) {
		struct virtio_video_format_range *frame_rate =
			&frame->frame_rates[rate_idx];

		if (within_range(frame_rate->min, fps, frame_rate->max))
			return 0;
	}

	return -EINVAL;
}
#endif

static void virtio_video_timeperframe_from_info(struct video_format_info *info,
						struct v4l2_fract *timeperframe)
{
	timeperframe->numerator = info->frame_rate;
	timeperframe->denominator = 1;
}

static int virtio_video_enc_g_parm(struct file *file, void *priv,
				   struct v4l2_streamparm *a)
{
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct v4l2_outputparm *out = &a->parm.output;
	struct v4l2_fract *timeperframe = &out->timeperframe;

	if (!V4L2_TYPE_IS_OUTPUT(a->type)) {
		v4l2_err(&vv->v4l2_dev,
			 "getting FPS is only possible for the output queue\n");
		return -EINVAL;
	}

	out->capability = V4L2_CAP_TIMEPERFRAME;
	virtio_video_timeperframe_from_info(&stream->in_info, timeperframe);

	return 0;
}

static int virtio_video_enc_s_parm(struct file *file, void *priv,
				   struct v4l2_streamparm *a)
{
	int ret;
	u64 frame_interval, frame_rate;
	struct video_format_info info;
	struct virtio_video_stream *stream = file2stream(file);
	struct virtio_video_device *vvd = to_virtio_vd(stream->video_dev);
	struct virtio_video *vv = vvd->vv;
	struct v4l2_outputparm *out = &a->parm.output;
	struct v4l2_fract *timeperframe = &out->timeperframe;

	if (V4L2_TYPE_IS_OUTPUT(a->type)) {
		frame_interval = timeperframe->numerator * (u64)USEC_PER_SEC;
		do_div(frame_interval, timeperframe->denominator);
		if (!frame_interval) {
			v4l2_err(&vv->v4l2_dev, "invalid frame rate setting\n");
			return -EINVAL;
		}

		frame_rate = (u64)USEC_PER_SEC;
		do_div(frame_rate, frame_interval);
	} else {
		v4l2_err(&vv->v4l2_dev,
			 "setting FPS is only possible for the output queue\n");
		return -EINVAL;
	}

#if !IS_ENABLED(CONFIG_VIRTIO_VIDEO_MTK_EXTENSION)
	ret = virtio_video_enc_try_framerate(stream, frame_rate);
	if (ret) {
		v4l2_err(&vv->v4l2_dev, "try framerate failed\n");
		return ret;
	}
#endif

	virtio_video_format_fill_default_info(&info, &stream->in_info);
	info.frame_rate = frame_rate;

	ret = virtio_video_cmd_set_params(vv, stream, &info,
				    VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	if (ret)
		virtio_v4l2_err("[%d]set_params fail, ret:%d", stream->stream_id, ret);

	ret = virtio_video_cmd_get_params(vv, stream, VIRTIO_VIDEO_QUEUE_TYPE_INPUT);
	if (ret)
		virtio_v4l2_err("[%d]get_params_input fail, ret:%d", stream->stream_id, ret);

	ret = virtio_video_cmd_get_params(vv, stream, VIRTIO_VIDEO_QUEUE_TYPE_OUTPUT);
	if (ret)
		virtio_v4l2_err("[%d]get_params_output fail, ret:%d", stream->stream_id, ret);

	out->capability = V4L2_CAP_TIMEPERFRAME;
	virtio_video_timeperframe_from_info(&stream->in_info, timeperframe);

	return 0;
}

static const struct v4l2_ioctl_ops virtio_video_enc_ioctl_ops = {
	.vidioc_querycap	= virtio_video_querycap,

	.vidioc_enum_fmt_vid_cap = virtio_video_enc_enum_fmt_vid_cap,

	.vidioc_enum_fmt_vid_cap    = virtio_video_enc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out    = virtio_video_enc_enum_fmt_vid_out,

	.vidioc_g_fmt_vid_cap	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap	= virtio_video_enc_s_fmt,

	.vidioc_g_fmt_vid_cap_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= virtio_video_enc_s_fmt,

	.vidioc_enum_fmt_vid_out = virtio_video_enc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out	= virtio_video_enc_s_fmt,

	.vidioc_g_fmt_vid_out_mplane	= virtio_video_g_fmt,
	.vidioc_s_fmt_vid_out_mplane	= virtio_video_enc_s_fmt,

	.vidioc_try_encoder_cmd	= virtio_video_try_encoder_cmd,
	.vidioc_encoder_cmd	= virtio_video_encoder_cmd,
	.vidioc_enum_frameintervals = virtio_video_enum_framemintervals,
	.vidioc_enum_framesizes = virtio_video_enum_framesizes,

	.vidioc_g_selection = virtio_video_g_selection,
	.vidioc_s_selection = virtio_video_s_selection,

	.vidioc_reqbufs		= virtio_video_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf		= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
	.vidioc_prepare_buf	= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_s_parm		= virtio_video_enc_s_parm,
	.vidioc_g_parm		= virtio_video_enc_g_parm,

	.vidioc_subscribe_event = virtio_video_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

int virtio_video_enc_init(struct video_device *vd)
{
	vd->ioctl_ops = &virtio_video_enc_ioctl_ops;
	strscpy(vd->name, "stateful-encoder", sizeof(vd->name));

	return 0;
}
