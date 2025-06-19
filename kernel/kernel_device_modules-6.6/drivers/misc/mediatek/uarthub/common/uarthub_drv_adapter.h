/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_ADAPTER_H
#define UARTHUB_DRV_ADAPTER_H

enum UARTHUB_baud_rate {
	baud_rate_unknown = -1,
	baud_rate_115200 = 115200,
	baud_rate_3m = 3000000,
	baud_rate_4m = 4000000,
	baud_rate_12m = 12000000,
	baud_rate_16m = 16000000,
	baud_rate_24m = 24000000,
};

enum UARTHUB_irq_err_type {
	uarthub_unknown_irq_err = -1,
	dev0_crc_err = 0,
	dev1_crc_err,
	dev2_crc_err,
	dev0_tx_timeout_err,
	dev1_tx_timeout_err,
	dev2_tx_timeout_err,
	dev0_tx_pkt_type_err,
	dev1_tx_pkt_type_err,
	dev2_tx_pkt_type_err,
	dev0_rx_timeout_err,
	dev1_rx_timeout_err,
	dev2_rx_timeout_err,
	rx_pkt_type_err,
	intfhub_restore_err,
	intfhub_dev_rx_err,
	intfhub_dev0_tx_err,
	intfhub_dev1_tx_err,
	intfhub_dev2_tx_err,
	irq_err_type_max,
};

enum debug_dump_tx_rx_index {
	DUMP0 = 0,
	DUMP1,
};
#endif /* UARTHUB_DRV_ADAPTER_H */
