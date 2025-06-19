/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6991_TEST_API_H
#define MT6991_TEST_API_H

#include "common_def_id.h"
#include "../inc/INTFHUB_c_header.h"
#include "../inc/platform_def_id.h"
#include "test_dat.h"

extern unsigned char verify_inband_esc_sta;

#define DBGM_MODE_PKT_INFO		0
#define DBGM_MODE_CHECK_DATA	1
#define DBGM_MODE_CRC_RESULT	2

#define __PACKET_INFO(typ, esc, byte) ((((typ) | ((esc)<<1)) << 27) | (byte))
#define PKT_INFO_TYP_AP		0x10
#define PKT_INFO_TYP_MD		0x8
#define PKT_INFO_TYP_ADSP	0x4

int uarthub_is_ut_testing_mt6991(void);
int uarthub_is_host_uarthub_ready_state_mt6991(int dev_index);
int uarthub_get_host_irq_sta_mt6991(int dev_index);
int uarthub_get_intfhub_active_sta_mt6991(void);
int uarthub_clear_host_irq_mt6991(int dev_index, int irq_type);
int uarthub_mask_host_irq_mt6991(int dev_index, int irq_type, int is_mask);
int uarthub_config_host_irq_ctrl_mt6991(int dev_index, int enable);
int uarthub_get_host_rx_fifo_size_mt6991(int dev_index);
int uarthub_get_cmm_rx_fifo_size_mt6991(void);
int uarthub_config_uartip_dma_en_ctrl_mt6991(int dev_index, enum uarthub_trx_type trx, int enable);
int uarthub_reset_fifo_trx_mt6991(void);
int uarthub_reset_intfhub_mt6991(void);
int uarthub_uartip_write_data_to_tx_buf_mt6991(int dev_index, int tx_data);
int uarthub_uartip_read_data_from_rx_buf_mt6991(int dev_index);
int uarthub_is_uartip_tx_buf_empty_for_write_mt6991(int dev_index);
int uarthub_is_uartip_rx_buf_ready_for_read_mt6991(int dev_index);
int uarthub_is_uartip_throw_xoff_mt6991(int dev_index);
int uarthub_config_uartip_rx_fifo_trig_thr_mt6991(int dev_index, int size);
int uarthub_uartip_write_tx_data_mt6991(int dev_index, unsigned char *p_tx_data, int tx_len);
int uarthub_uartip_read_rx_data_mt6991(
	int dev_index, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len);
int uarthub_is_apuart_tx_buf_empty_for_write_mt6991(int port_no);
int uarthub_is_apuart_rx_buf_ready_for_read_mt6991(int port_no);
int uarthub_apuart_write_data_to_tx_buf_mt6991(int port_no, int tx_data);
int uarthub_apuart_read_data_from_rx_buf_mt6991(int port_no);
int uarthub_apuart_write_tx_data_mt6991(int port_no, unsigned char *p_tx_data, int tx_len);
int uarthub_apuart_read_rx_data_mt6991(
	int port_no, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len);
int uarthub_init_default_apuart_config_mt6991(void);
int uarthub_clear_all_ut_irq_sta_mt6991(void);
enum uarthub_pkt_fmt_type uarthub_check_packet_format(int dev_index, unsigned char byte1);
int uarthub_check_packet_is_complete(int dev_index, unsigned char *pData, int length);
int uarthub_get_crc(unsigned char *pData, int length, unsigned char *pData_CRC);
int uarthub_uartip_send_data_internal_mt6991(
	int dev_index, unsigned char *p_tx_data, int tx_len, int dump_trxlog);
int uarthub_verify_internal_loopback_mt6991(
	int dev_index, unsigned char *p_tx_data, int tx_len);
int uarthub_ut_loopback_send_and_recv(
	int tx_dev_id, const unsigned char *pCmd, int szCmd,
	int rx_dev_id, const unsigned char *pEvt, int szEvt);
int uarthub_inband_irq_handle_mt6991(void);

static inline int __uh_test_debug_dump(int sspm, int intfhub, int uartip, int dbgm)
{
	if (sspm)
		uarthub_dump_sspm_log_mt6991(__func__);
	if (intfhub)
		uarthub_dump_intfhub_debug_info_mt6991(__func__);
	if (uartip)
		uarthub_dump_uartip_debug_info_mt6991(__func__, NULL);
	if (dbgm)
		uarthub_dump_debug_monitor_mt6991(__func__);
	return 0;
}

#endif /* MT6991_TEST_API_H */
