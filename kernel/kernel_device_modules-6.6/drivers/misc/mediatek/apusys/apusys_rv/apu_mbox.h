/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_MBOX_H
#define APU_MBOX_H

struct apu_mbox_hdr {
	unsigned int id;
	unsigned int len;
	unsigned int serial_no;
	unsigned int csum;
#if IS_ENABLED(CONFIG_VHOST_APU)
	unsigned int vm_id;
	unsigned int virtio_send_bufer_da;
	unsigned int virtio_recv_bufer_da;
#endif
};

#define APU_MBOX_SLOT_SIZE	(4)
#define APU_MBOX_HDR_SLOTS \
		(sizeof(struct apu_mbox_hdr) / APU_MBOX_SLOT_SIZE)

void apu_mbox_ack_outbox(struct mtk_apu *apu);
void apu_mbox_read_outbox(struct mtk_apu *apu, struct apu_mbox_hdr *hdr);
int apu_mbox_wait_inbox(struct mtk_apu *apu);
void apu_mbox_write_inbox(struct mtk_apu *apu, struct apu_mbox_hdr *hdr);
void apu_mbox_inbox_init(struct mtk_apu *apu);
void apu_mbox_hw_init(struct mtk_apu *apu);
void apu_mbox_hw_exit(struct mtk_apu *apu);

#if IS_ENABLED(CONFIG_VHOST_APU)
typedef void (*vhost_apu_ipi_int_handler)(struct apu_mbox_hdr hdr, uint32_t vm_id);
struct vhost_apu_platform_fp {
	vhost_apu_ipi_int_handler vhost_apu_handler;
};
#endif

#endif /* APU_MBOX_H */
