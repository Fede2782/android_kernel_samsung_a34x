/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CEC_H
#define _MTK_CEC_H

#include <linux/cec.h>
#include <uapi/linux/cec.h>
#include <media/cec.h>
#include <media/cec-notifier.h>

#define CEC_HEADER_BLOCK_SIZE 1

enum mtk_cec_clk_id {
	MTK_CEC_66M_H,
	MTK_CEC_66M_B,
	MTK_HDMI_32K,
	MTK_HDMI_26M,
	MTK_CEC_CLK_COUNT,
};

enum cec_tx_status {
	CEC_TX_START,
	CEC_TX_Transmitting,
	CEC_TX_COMPLETE,
	CEC_TX_FAIL,
	CEC_TX_FAIL_DNAK,
	CEC_TX_FAIL_HNAK,
	CEC_TX_FAIL_RETR,
	CEC_TX_FAIL_DATA,
	CEC_TX_FAIL_HEAD,
	CEC_TX_FAIL_SRC,
	CEC_TX_FAIL_LOW,
	CEC_TX_STATUS_NUM,
};

enum cec_rx_status {
	CEC_RX_START,
	CEC_RX_Receiving,
	CEC_RX_COMPLETE,
	CEC_RX_FAIL,
	CEC_RX_STATUS_NUM,
};

struct cec_frame {
	struct cec_msg *msg;
	unsigned char retry_count;
	union {
		enum cec_tx_status tx_status;
		enum cec_rx_status rx_status;
	} status;
};

struct mtk_cec {
	void __iomem *regs;
	struct clk *clk[MTK_CEC_CLK_COUNT];
	int irq;
	struct device *hdmi_dev;
	spinlock_t lock;
	struct cec_adapter *adap;
	struct cec_notifier	*notifier;
	struct cec_frame transmitting;
	struct cec_frame received;
	struct cec_msg rx_msg;	/* dynamic alloc or fixed memory?? */
	bool cec_enabled;
	struct work_struct cec_tx_work;
	struct work_struct cec_rx_work;
};

enum cec_inner_clock {
	CLK_27M_SRC = 0,
};

enum cec_la_num {
	DEVICE_ADDR_1 = 1,
	DEVICE_ADDR_2 = 2,
	DEVICE_ADDR_3 = 3,
};

struct cec_msg_header_block {
	unsigned char destination:4;
	unsigned char initiator:4;
};

int mtk_cec_clk_enable(struct mtk_cec *cec, bool enable);
extern struct mtk_cec *mtk_global_cec;

#endif /* _MTK_CEC_H */
