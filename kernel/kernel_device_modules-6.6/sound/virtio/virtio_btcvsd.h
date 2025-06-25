/* SPDX-License-Identifier: GPL-2.0+ */
//
// virtio-snd: Mediatek BT SCO CVSD/MSBC Driver
// Copyright (c) 2023 MediaTek Inc.
//

#ifndef VIRTIO_SND_BTCVSD_H
#define VIRTIO_SND_BTCVSD_H

#include <linux/slab.h>
#include <linux/virtio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <uapi/linux/virtio_snd.h>
#include <uapi/sound/asound.h>

#define BT_CVSD_TX_NREADY	BIT(21)
#define BT_CVSD_RX_READY	BIT(22)
#define BT_CVSD_TX_UNDERFLOW	BIT(23)
#define BT_CVSD_RX_OVERFLOW	BIT(24)
#define BT_CVSD_INTERRUPT	BIT(31)

#define BT_CVSD_CLEAR \
	(BT_CVSD_TX_NREADY | BT_CVSD_RX_READY | BT_CVSD_RX_OVERFLOW |\
	 BT_CVSD_INTERRUPT)

/* TX */
#define SCO_TX_ENCODE_SIZE (60)
/* 18 = 6 * 180 / SCO_TX_ENCODE_SIZE */
#define SCO_TX_PACKER_BUF_NUM (18)

/* RX */
#define SCO_RX_PLC_SIZE (30)
#define SCO_RX_PACKER_BUF_NUM (64)
#define SCO_RX_PACKET_MASK (0x3F)

#define SCO_CVSD_PACKET_VALID_SIZE 2

#define SCO_PACKET_120 120
#define SCO_PACKET_180 180

#define BTCVSD_RX_PACKET_SIZE (SCO_RX_PLC_SIZE + SCO_CVSD_PACKET_VALID_SIZE)
#define BTCVSD_TX_PACKET_SIZE (SCO_TX_ENCODE_SIZE)

#define BTCVSD_RX_BUF_SIZE (BTCVSD_RX_PACKET_SIZE * SCO_RX_PACKER_BUF_NUM)
#define BTCVSD_TX_BUF_SIZE (BTCVSD_TX_PACKET_SIZE * SCO_TX_PACKER_BUF_NUM)

enum bt_sco_state {
	BT_SCO_STATE_IDLE,
	BT_SCO_STATE_RUNNING,
	BT_SCO_STATE_ENDING,
	BT_SCO_STATE_LOOPBACK,
};

enum bt_sco_direct {
	BT_SCO_DIRECT_BT2ARM,
	BT_SCO_DIRECT_ARM2BT,
};

enum bt_sco_packet_len {
	BT_SCO_CVSD_30 = 0,
	BT_SCO_CVSD_60,
	BT_SCO_CVSD_90,
	BT_SCO_CVSD_120,
	BT_SCO_CVSD_10,
	BT_SCO_CVSD_20,
	BT_SCO_CVSD_MAX,
};

enum BT_SCO_BAND {
	BT_SCO_NB,
	BT_SCO_WB,
};

struct virtsnd_btcvsd_snd_hw_info {
	unsigned int num_valid_addr;
	unsigned long bt_sram_addr[20];
	unsigned int packet_length;
	unsigned int packet_num;
};

struct virtsnd_btcvsd_snd_stream {
	struct snd_pcm_substream *substream;
	int stream;

	enum bt_sco_state state;

	unsigned int packet_size;
	unsigned int buf_size;
	u8 temp_packet_buf[SCO_PACKET_180];

	int packet_w;
	int packet_r;
	snd_pcm_uframes_t prev_frame;
	int prev_packet_idx;

	unsigned int xrun:1;
	unsigned int timeout:1;
	unsigned int mute:1;
	unsigned int trigger_start:1;
	unsigned int wait_flag:1;
	unsigned int rw_cnt;

	unsigned long long time_stamp;
	unsigned long long buf_data_equivalent_time;

	struct virtsnd_btcvsd_snd_hw_info buffer_info;
};

struct virtsnd_btcvsd_snd {
	struct device *dev;
	struct device_node *of_node;
	struct snd_pcm *pcm;
	int irq_id;

	struct regmap *infra;
	void __iomem *bt_pkv_base;
	void __iomem *bt_sram_bank2_base;

	unsigned int infra_misc_offset;
	unsigned int conn_bt_cvsd_mask;
	unsigned int cvsd_mcu_read_offset;
	unsigned int cvsd_mcu_write_offset;
	unsigned int cvsd_packet_indicator;

	u32 *bt_reg_pkt_r;
	u32 *bt_reg_pkt_w;
	u32 *bt_reg_ctl;

	unsigned int irq_disabled:1;
	unsigned int bypass_bt_access:1;

	spinlock_t tx_lock;	/* spinlock for bt tx stream control */
	spinlock_t rx_lock;	/* spinlock for bt rx stream control */
	wait_queue_head_t tx_wait;
	wait_queue_head_t rx_wait;

	struct virtsnd_btcvsd_snd_stream *tx;
	struct virtsnd_btcvsd_snd_stream *rx;
	u8 tx_packet_buf[BTCVSD_TX_BUF_SIZE];
	u8 rx_packet_buf[BTCVSD_RX_BUF_SIZE];
	u8 disable_write_silence;
	u8 write_tx:1;

	enum BT_SCO_BAND band;
};

struct virtsnd_btcvsd_snd_time_buffer_info {
	unsigned long long data_count_equi_time;
	unsigned long long time_stamp_us;
};

int virtsnd_btcvsd_build_devs(struct virtio_snd *snd);

#endif /* VIRTIO_SND_CVSD_H */
