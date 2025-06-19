// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_export.h"
#include "uarthub_drv_core.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/cdev.h>

#include <linux/interrupt.h>
#include <linux/ratelimit.h>

int uarthub_export_dump_debug_info(void)
{
	return uarthub_core_debug_info(NULL);
}
int uarthub_export_dump_debug_info_with_tag(const char *tag)
{
	return uarthub_core_debug_info(tag);
}

struct uarthub_drv_cbs uarthub_drv_export_cbs = {
	.open = uarthub_core_open,
	.close = uarthub_core_close,
	.dev0_is_uarthub_ready = uarthub_core_dev0_is_uarthub_ready,
	.get_host_wakeup_status = uarthub_core_get_host_wakeup_status,
	.get_host_set_fw_own_status = uarthub_core_get_host_set_fw_own_status,
	.dev0_is_txrx_idle = uarthub_core_dev0_is_txrx_idle,
	.dev0_set_tx_request = uarthub_core_dev0_set_tx_request,
	.dev0_set_rx_request = uarthub_core_dev0_set_rx_request,
	.dev0_set_txrx_request = uarthub_core_dev0_set_txrx_request,
	.dev0_clear_tx_request = uarthub_core_dev0_clear_tx_request,
	.dev0_clear_rx_request = uarthub_core_dev0_clear_rx_request,
	.dev0_clear_txrx_request = uarthub_core_dev0_clear_txrx_request,
	.get_uart_cmm_rx_count = uarthub_core_get_uart_cmm_rx_count,
	.is_assert_state = uarthub_core_is_assert_state,
	.irq_register_cb = uarthub_core_irq_register_cb,
	.bypass_mode_ctrl = uarthub_core_bypass_mode_ctrl,
	.is_bypass_mode = uarthub_core_is_bypass_mode,
	.config_internal_baud_rate = uarthub_core_config_internal_baud_rate,
	.config_external_baud_rate =uarthub_core_config_external_baud_rate,
	.assert_state_ctrl = uarthub_core_assert_state_ctrl,
	.reset_flow_control = uarthub_core_reset_flow_control,
	.sw_reset = uarthub_core_reset,
	.md_adsp_fifo_ctrl = uarthub_core_md_adsp_fifo_ctrl,
	.dump_debug_info = uarthub_export_dump_debug_info,
	.dump_debug_info_with_tag = uarthub_export_dump_debug_info_with_tag,
	.debug_dump_tx_rx_count = uarthub_core_debug_dump_tx_rx_count,
	.inband_irq_register_cb = uarthub_core_inband_irq_register_cb,
	.debug_bus_status_info = uarthub_core_debug_bus_status_info,
	.get_bt_sleep_flow_hw_mech_en = uarthub_core_get_bt_sleep_flow_hw_mech_en,
	.set_bt_sleep_flow_hw_mech_en = uarthub_core_set_bt_sleep_flow_hw_mech_en,
	.get_host_awake_sta = uarthub_core_get_host_awake_sta,
	.set_host_awake_sta = uarthub_core_set_host_awake_sta,
	.clear_host_awake_sta = uarthub_core_clear_host_awake_sta,
	.get_host_bt_awake_sta = uarthub_core_get_host_bt_awake_sta,
	.get_cmm_bt_awake_sta = uarthub_core_get_cmm_bt_awake_sta,
	.get_bt_awake_sta = uarthub_core_get_bt_awake_sta,
	.bt_on_count_inc = uarthub_core_bt_on_count_inc,
	.inband_set_esc_sta = uarthub_core_inband_set_esc_sta,
	.inband_trigger_ctrl = uarthub_core_inband_trigger_ctrl,
	.inband_is_tx_complete = uarthub_core_inband_is_tx_complete,
	.inband_enable_ctrl = uarthub_core_inband_enable_ctrl,
	.inband_is_support = uarthub_core_inband_is_support
};

/* FPGA test only */
int UARTHUB_is_host_uarthub_ready_state(int dev_index)
{
	return uarthub_core_is_host_uarthub_ready_state(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_host_uarthub_ready_state);

int UARTHUB_set_host_txrx_request(int dev_index, int trx)
{
	return uarthub_core_set_host_txrx_request(dev_index, trx);
}
EXPORT_SYMBOL(UARTHUB_set_host_txrx_request);

int UARTHUB_clear_host_txrx_request(int dev_index, int trx)
{
	return uarthub_core_clear_host_txrx_request(dev_index, trx);
}
EXPORT_SYMBOL(UARTHUB_clear_host_txrx_request);

int UARTHUB_get_host_irq_sta(int dev_index)
{
	return uarthub_core_get_host_irq_sta(dev_index);
}
EXPORT_SYMBOL(UARTHUB_get_host_irq_sta);

int UARTHUB_clear_host_irq(int dev_index)
{
	return uarthub_core_clear_host_irq(dev_index);
}
EXPORT_SYMBOL(UARTHUB_clear_host_irq);

int UARTHUB_mask_host_irq(int dev_index, int mask_bit, int is_mask)
{
	return uarthub_core_mask_host_irq(dev_index, mask_bit, is_mask);
}
EXPORT_SYMBOL(UARTHUB_mask_host_irq);

int UARTHUB_config_host_irq_ctrl(int dev_index, int enable)
{
	return uarthub_core_config_host_irq_ctrl(dev_index, enable);
}
EXPORT_SYMBOL(UARTHUB_config_host_irq_ctrl);

int UARTHUB_get_host_rx_fifo_size(int dev_index)
{
	return uarthub_core_get_host_rx_fifo_size(dev_index);
}
EXPORT_SYMBOL(UARTHUB_get_host_rx_fifo_size);

int UARTHUB_get_cmm_rx_fifo_size(void)
{
	return uarthub_core_get_cmm_rx_fifo_size();
}
EXPORT_SYMBOL(UARTHUB_get_cmm_rx_fifo_size);

int UARTHUB_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable)
{
	return uarthub_core_config_uartip_dma_en_ctrl(dev_index, trx, enable);
}
EXPORT_SYMBOL(UARTHUB_config_uartip_dma_en_ctrl);

int UARTHUB_reset_fifo_trx(void)
{
	return uarthub_core_reset_fifo_trx();
}
EXPORT_SYMBOL(UARTHUB_reset_fifo_trx);

int UARTHUB_uartip_write_data_to_tx_buf(int dev_index, int tx_data)
{
	return uarthub_core_uartip_write_data_to_tx_buf(dev_index, tx_data);
}
EXPORT_SYMBOL(UARTHUB_uartip_write_data_to_tx_buf);

int UARTHUB_uartip_read_data_from_rx_buf(int dev_index)
{
	return uarthub_core_uartip_read_data_from_rx_buf(dev_index);
}
EXPORT_SYMBOL(UARTHUB_uartip_read_data_from_rx_buf);

int UARTHUB_is_uartip_tx_buf_empty_for_writing(int dev_index)
{
	return uarthub_core_is_uartip_tx_buf_empty_for_write(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_tx_buf_empty_for_writing);

int UARTHUB_is_uartip_rx_buf_ready_for_reading(int dev_index)
{
	return uarthub_core_is_uartip_rx_buf_ready_for_read(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_rx_buf_ready_for_reading);

int UARTHUB_is_uartip_throw_xoff(int dev_index)
{
	return uarthub_core_is_uartip_throw_xoff(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_throw_xoff);

int UARTHUB_config_uartip_rx_fifo_trig_threshold(int dev_index, int size)
{
	return uarthub_core_config_uartip_rx_fifo_trig_thr(dev_index, size);
}
EXPORT_SYMBOL(UARTHUB_config_uartip_rx_fifo_trig_threshold);

int UARTHUB_ut_ip_verify_pkt_hdr_fmt(void)
{
	return uarthub_core_ut_ip_verify_pkt_hdr_fmt();
}
EXPORT_SYMBOL(UARTHUB_ut_ip_verify_pkt_hdr_fmt);

int UARTHUB_ut_ip_verify_trx_not_ready(void)
{
	return uarthub_core_ut_ip_verify_trx_not_ready();
}
EXPORT_SYMBOL(UARTHUB_ut_ip_verify_trx_not_ready);

int UARTHUB_get_intfhub_active_sta(void)
{
	return uarthub_core_get_intfhub_active_sta();
}
EXPORT_SYMBOL(UARTHUB_get_intfhub_active_sta);

int UARTHUB_debug_monitor_stop(int stop)
{
	return uarthub_core_debug_monitor_stop(stop);
}
EXPORT_SYMBOL(UARTHUB_debug_monitor_stop);

int UARTHUB_debug_monitor_clr(void)
{
	return uarthub_core_debug_monitor_clr();
}
EXPORT_SYMBOL(UARTHUB_debug_monitor_clr);

int UARTHUB_debug_byte_cnt_info(const char *tag)
{
	return uarthub_core_debug_byte_cnt_info(tag);
}
EXPORT_SYMBOL(UARTHUB_debug_byte_cnt_info);

MODULE_LICENSE("GPL");
