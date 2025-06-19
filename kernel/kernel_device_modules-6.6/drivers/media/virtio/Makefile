# SPDX-License-Identifier: GPL-2.0+

subdir-ccflags-y += -Werror

virtio-video-objs := \
    virtio_video_driver.o \
    virtio_video_device.o \
    virtio_video_vq.o \
    virtio_video_dec.o \
    virtio_video_enc.o \
    virtio_video_caps.o \
    virtio_video_helpers.o \
    virtio_video_util.o

obj-$(CONFIG_VIRTIO_VIDEO) += virtio-video.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
