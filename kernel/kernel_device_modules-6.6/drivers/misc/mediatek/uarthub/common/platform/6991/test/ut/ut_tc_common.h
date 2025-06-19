/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef UT_TC_COMMON_H
#define UT_TC_COMMON_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include "uarthub_drv_core.h"
#include "../../inc/mt6991.h"
#include "../mt6991_test_api.h"
#include "../test_util.h"

#define __PACKET_INFO(typ, esc, byte) ((((typ) | ((esc)<<1)) << 27) | (byte))
#define PKT_INFO_TYP_AP		0x10
#define PKT_INFO_TYP_MD		0x8
#define PKT_INFO_TYP_ADSP	0x4


static inline int __uh_ut_check_uarthub_open(void)
{
	int state = uarthub_core_open();

	UTLOG("uarthub_core_open()...[%d]", state);
	return 0;
}


static inline int __uh_ut_set_hosts_trx_request(void)
{
	uarthub_set_host_trx_request_mt6991(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	if (uarthub_is_host_uarthub_ready_state_mt6991(0) != 1)
		return -1;
	uarthub_set_host_trx_request_mt6991(1, TRX);
	uarthub_set_host_trx_request_mt6991(2, TRX);
	return 0;
}
static inline int __uh_ut_clear_hosts_trx_request(void)
{
	uarthub_clear_host_trx_request_mt6991(0, TRX);
	uarthub_clear_host_trx_request_mt6991(1, TRX);
	uarthub_clear_host_trx_request_mt6991(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return 0;
}
static inline int __uh_ut_enable_loopback_ctrl(void)
{
	uarthub_set_host_loopback_ctrl_mt6991(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6991(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6991(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6991(1, 1);
	return 0;
}
static inline int __uh_ut_disable_loopback_ctrl(void)
{
	uarthub_set_host_loopback_ctrl_mt6991(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6991(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6991(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6991(1, 0);
	return 0;
}
static inline int __uh_ut_dbgm_crtl_set(
	int mode, int bypass_esp, int chk_data_en, int crc_dat_en)
{
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, mode);
	DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR, bypass_esp);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, chk_data_en);
	DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR, crc_dat_en);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);
	mdelay(1);
	return 0;
}
#define __DEFAULT_DBGM_CRTL_SET() __uh_ut_dbgm_crtl_set(1, 0, 1, 0)

#endif
