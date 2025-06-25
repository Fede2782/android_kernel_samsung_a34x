/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virtio-snd: Virtio sound device
 * Copyright (C) 2021 OpenSynergy GmbH
 */
#ifndef VIRTIO_SND_CARD_H
#define VIRTIO_SND_CARD_H

#include <linux/slab.h>
#include <linux/virtio.h>
#include <sound/core.h>
#include <uapi/linux/virtio_snd.h>

#include "virtio_ctl_msg.h"
#include "virtio_pcm.h"
#include "virtio_btcvsd.h"

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
#include "../soc/mediatek/common/mtk-base-afe.h"
#include "../soc/mediatek/common/mtk-sram-manager.h"
#include "../soc/mediatek/common/mtk-mmap-ion.h"
#endif

#define VIRTIO_SND_CARD_DRIVER	"virtio-snd"
#define VIRTIO_SND_CARD_NAME	"VirtIO SoundCard"
#define VIRTIO_SND_PCM_NAME	"VirtIO PCM"

struct dentry;
struct virtio_jack;
struct virtio_pcm_substream;

/**
 * struct virtio_snd_queue - Virtqueue wrapper structure.
 * @lock: Used to synchronize access to a virtqueue.
 * @vqueue: Underlying virtqueue.
 */
struct virtio_snd_queue {
	spinlock_t lock;
	struct virtqueue *vqueue;
};

/**
 * struct virtio_kctl - VirtIO control element.
 * @kctl: ALSA control element.
 * @items: Items for the ENUMERATED element type.
 */
struct virtio_kctl {
	struct snd_kcontrol *kctl;
	struct virtio_snd_ctl_enum_item *items;
};


/**
 * struct virtio_snd - VirtIO sound card device.
 * @vdev: Underlying virtio device.
 * @queues: Virtqueue wrappers.
 * @card: ALSA sound card.
 * @ctl_msgs: Pending control request list.
 * @event_msgs: Device events.
 * @pcm_list: VirtIO PCM device list.
 * @jacks: VirtIO jacks.
 * @njacks: Number of jacks.
 * @substreams: VirtIO PCM substreams.
 * @nsubstreams: Number of PCM substreams.
 * @chmaps: VirtIO channel maps.
 * @nchmaps: Number of channel maps.
 * @kctl_infos: VirtIO control element information.
 * @kctls: VirtIO control elements.
 * @nkctls: Number of control elements.
 */
struct virtio_snd {
	struct virtio_device *vdev;
	struct virtio_snd_queue queues[VIRTIO_SND_VQ_MAX];
	struct snd_card *card;
	struct list_head ctl_msgs;
	struct virtio_snd_event *event_msgs;
	struct list_head pcm_list;
	struct virtio_jack *jacks;
	u32 njacks;
	struct virtio_pcm_substream *substreams;
	u32 nsubstreams;
	struct virtio_snd_chmap_info *chmaps;
	u32 nchmaps;

	struct virtio_snd_ctl_info *kctl_infos;
	struct virtio_kctl *kctls;
	u32 nkctls;

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	// void __iomem *base_addr;
	// struct regmap *regmap;
	struct resource res;
	struct mtk_base_afe_irq *irqs;
	int (*irq_fs)(struct snd_pcm_substream *substream,
		      unsigned int rate);
	int irqs_size;
	void *sram;
	struct virtio_kctl *remote_use_dram_only_ctl;
	struct virtio_kctl *remote_sram_mode_ctl;
	struct virtio_kctl *remote_passthrough_shm_ctl;
	enum mtk_audio_sram_mode sram_mode;
	struct work_struct sram_reg_control;
	int result;
#endif

	struct virtsnd_btcvsd_snd *btcvsd;

	struct dentry *debugfs;
};

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
#define VIRTIO_PASSTHROUGH_SHM_CMD_REG_DRAM 0
#define VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_DRAM 1
#define VIRTIO_PASSTHROUGH_SHM_CMD_REG_SRAM 2
#define VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_SRAM 3

struct virtio_passthrough_shm_msg {
	uint8_t cmd;
	uint8_t pcm_id;
	uint8_t padding[6];
	uint64_t pa;
	uint64_t bytes;
};
#endif

/* Message completion timeout in milliseconds (module parameter). */
extern u32 virtsnd_msg_timeout_ms;

static inline struct virtio_snd_queue *
virtsnd_control_queue(struct virtio_snd *snd)
{
	return &snd->queues[VIRTIO_SND_VQ_CONTROL];
}

static inline struct virtio_snd_queue *
virtsnd_event_queue(struct virtio_snd *snd)
{
	return &snd->queues[VIRTIO_SND_VQ_EVENT];
}

static inline struct virtio_snd_queue *
virtsnd_tx_queue(struct virtio_snd *snd)
{
	return &snd->queues[VIRTIO_SND_VQ_TX];
}

static inline struct virtio_snd_queue *
virtsnd_rx_queue(struct virtio_snd *snd)
{
	return &snd->queues[VIRTIO_SND_VQ_RX];
}

static inline struct virtio_snd_queue *
virtsnd_pcm_queue(struct virtio_pcm_substream *vss)
{
	if (vss->direction == SNDRV_PCM_STREAM_PLAYBACK)
		return virtsnd_tx_queue(vss->snd);
	else
		return virtsnd_rx_queue(vss->snd);
}

int virtsnd_jack_parse_cfg(struct virtio_snd *snd);

int virtsnd_jack_build_devs(struct virtio_snd *snd);

void virtsnd_jack_event(struct virtio_snd *snd,
			struct virtio_snd_event *event);

int virtsnd_chmap_parse_cfg(struct virtio_snd *snd);

int virtsnd_chmap_build_devs(struct virtio_snd *snd);

int virtsnd_kctl_parse_cfg(struct virtio_snd *snd);

int virtsnd_kctl_build_devs(struct virtio_snd *snd);

void virtsnd_kctl_event(struct virtio_snd *snd, struct virtio_snd_event *event);

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
int virtsnd_kctl_find_by_name(struct virtio_snd *snd, const char *name,
		struct virtio_kctl **out);
int virtsnd_info_find_by_name(struct virtio_snd *snd, const char *name,
		struct virtio_snd_ctl_info **out);
int virtsnd_kctl_get_use_dram_only(struct virtio_snd *snd, int pcm_id);
int virtsnd_kctl_reg_shm(struct virtio_snd *snd, int pcm_id,
		dma_addr_t dma_area, size_t bytes, int sram);
int virtsnd_kctl_unreg_shm(struct virtio_snd *snd, int pcm_id, int sram);
int virtio_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode);
#endif

#endif /* VIRTIO_SND_CARD_H */
