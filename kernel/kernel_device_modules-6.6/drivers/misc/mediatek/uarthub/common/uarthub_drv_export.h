/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_EXPORT_H
#define UARTHUB_DRV_EXPORT_H

//#include <mtk_wcn_cmb_stub.h>
#include <linux/types.h>
#include <linux/fs.h>

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

typedef void (*UARTHUB_IRQ_CB) (unsigned int err_type);
typedef void (*UARTHUB_INBAND_IRQ_CB) (unsigned char esc_sta);

struct uarthub_drv_cbs {
	int (*open) (void);
	int (*close) (void);
	int (*dev0_is_uarthub_ready) (const char *tag);
	int (*get_host_wakeup_status) (void);
	int (*get_host_set_fw_own_status) (void);
	int (*dev0_is_txrx_idle) (int);
	int (*dev0_set_tx_request) (void);
	int (*dev0_set_rx_request) (void);
	int (*dev0_set_txrx_request) (void);
	int (*dev0_clear_tx_request) (void);
	int (*dev0_clear_rx_request) (void);
	int (*dev0_clear_txrx_request) (void);
	int (*get_uart_cmm_rx_count) (void);
	int (*is_assert_state) (void);
	int (*irq_register_cb) (UARTHUB_IRQ_CB irq_callback);
	int (*bypass_mode_ctrl) (int enable);
	int (*is_bypass_mode) (void);
	int (*config_internal_baud_rate) (int dev_index, int rate);
	int (*config_external_baud_rate) (int rate);
	int (*assert_state_ctrl) (int assert_ctrl);
	int (*reset_flow_control) (void);
	int (*sw_reset) (void);
	int (*md_adsp_fifo_ctrl) (int enable);
	int (*dump_debug_info) (void);
	int (*dump_debug_info_with_tag) (const char *tag);
	int (*debug_dump_tx_rx_count) (const char *tag, int trigger_point);
	int (*inband_irq_register_cb) (UARTHUB_INBAND_IRQ_CB inband_irq_callback);
	int (*debug_bus_status_info) (const char *tag);
	int (*get_bt_sleep_flow_hw_mech_en) (void);
	int (*set_bt_sleep_flow_hw_mech_en) (int enable);
	int (*get_host_awake_sta) (int dev_index);
	int (*set_host_awake_sta) (int dev_index);
	int (*clear_host_awake_sta) (int dev_index);
	int (*get_host_bt_awake_sta) (int dev_index);
	int (*get_cmm_bt_awake_sta) (void);
	int (*get_bt_awake_sta) (void);
	int (*bt_on_count_inc) (void);
	int (*inband_set_esc_sta) (unsigned char esc_sta);
	int (*inband_trigger_ctrl) (void);
	int (*inband_is_tx_complete) (void);
	int (*inband_enable_ctrl) (int enable);
	int (*inband_is_support) (void);
};
extern void uarthub_drv_callbacks_register(struct uarthub_drv_cbs *cb);
extern void uarthub_drv_callbacks_unregister(void);

/* FPGA test only */
int UARTHUB_is_host_uarthub_ready_state(int dev_index);
int UARTHUB_set_host_txrx_request(int dev_index, int trx);
int UARTHUB_clear_host_txrx_request(int dev_index, int trx);
int UARTHUB_get_host_irq_sta(int dev_index);
int UARTHUB_clear_host_irq(int dev_index);
int UARTHUB_mask_host_irq(int dev_index, int mask_bit, int is_mask);
int UARTHUB_config_host_irq_ctrl(int dev_index, int enable);
int UARTHUB_get_host_rx_fifo_size(int dev_index);
int UARTHUB_get_cmm_rx_fifo_size(void);
int UARTHUB_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable);
int UARTHUB_reset_fifo_trx(void);
int UARTHUB_uartip_write_data_to_tx_buf(int dev_index, int tx_data);
int UARTHUB_uartip_read_data_from_rx_buf(int dev_index);
int UARTHUB_is_uartip_tx_buf_empty_for_writing(int dev_index);
int UARTHUB_is_uartip_rx_buf_ready_for_reading(int dev_index);
int UARTHUB_is_uartip_throw_xoff(int dev_index);
int UARTHUB_config_uartip_rx_fifo_trig_threshold(int dev_index, int size);
int UARTHUB_ut_ip_verify_pkt_hdr_fmt(void);
int UARTHUB_ut_ip_verify_trx_not_ready(void);
int UARTHUB_get_intfhub_active_sta(void);
int UARTHUB_debug_byte_cnt_info(const char *tag);

#endif /* UARTHUB_DRV_EXPORT_H */
