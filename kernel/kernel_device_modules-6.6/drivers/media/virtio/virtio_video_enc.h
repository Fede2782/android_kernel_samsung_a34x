/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/* Encoder header for virtio video driver.
 *
 * Copyright 2019 OpenSynergy GmbH.
 *
 */

#ifndef _VIRTIO_VIDEO_ENC_H
#define _VIRTIO_VIDEO_ENC_H

#include "virtio_video.h"

int virtio_video_enc_init(struct video_device *vd);
int virtio_video_enc_init_ctrls(struct virtio_video_stream *stream);
int virtio_video_enc_init_queues(void *priv, struct vb2_queue *src_vq,
				 struct vb2_queue *dst_vq);

#endif /* _VIRTIO_VIDEO_ENC_H */
