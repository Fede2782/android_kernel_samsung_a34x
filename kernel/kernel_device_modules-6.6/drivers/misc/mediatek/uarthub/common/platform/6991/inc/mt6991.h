/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6991_H
#define MT6991_H

#define MT6991_UARTHUB_DUMP_VERSION    "20230807"

#define UARTHUB_SUPPORT_FPGA           0
#define UARTHUB_SUPPORT_DVT            0
#define SPM_RES_CHK_EN                 1
#define SSPM_DRIVER_EN                 1
#define UNIVPLL_CTRL_EN                1
#define MD_CHANNEL_EN                  1
#define UARTHUB_WAKEUP_DEBUG_EN        0
#define SUPPORT_HWCCF                  0

#include "INTFHUB_c_header.h"
#include "UARTHUB_UART0_c_header.h"
#include "UARTHUB_WAKEUP_c_header.h"
#include "common_def_id.h"
#include "platform_def_id.h"

extern void __iomem *gpio_base_remap_addr_mt6991;
extern void __iomem *pericfg_ao_remap_addr_mt6991;
extern void __iomem *topckgen_base_remap_addr_mt6991;
extern void __iomem *apdma_uart_tx_int_remap_addr_mt6991;
extern void __iomem *spm_remap_addr_mt6991;
extern void __iomem *apmixedsys_remap_addr_mt6991;
extern void __iomem *iocfg_tm3_remap_addr_mt6991;
extern void __iomem *sys_sram_remap_addr_mt6991;

enum uarthub_uartip_id {
	uartip_id_ap = 0,
	uartip_id_md,
	uartip_id_adsp,
	uartip_id_cmm,
};

enum uarthub_clk_opp {
	uarthub_clk_topckgen = 0,
	uarthub_clk_26m,
	uarthub_clk_52m,
	uarthub_clk_104m,
	uarthub_clk_208m,
};

#define UARTHUB_MAX_NUM_DEV_HOST   3

extern void __iomem *uartip_base_map_mt6991[UARTHUB_MAX_NUM_DEV_HOST + 1];
extern void __iomem *apuart_base_map_mt6991[4];

extern struct uarthub_core_ops_struct mt6991_plat_core_data;
extern struct uarthub_debug_ops_struct mt6991_plat_debug_data;
#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
extern struct uarthub_ut_test_ops_struct mt6991_plat_ut_test_data;
#endif

#define UARTHUB_CMM_BASE_ADDR      0x16060000
#define UARTHUB_DEV_0_BASE_ADDR    0x16070100
#define UARTHUB_DEV_1_BASE_ADDR    0x16080200
#define UARTHUB_DEV_2_BASE_ADDR    0x16090300
#define UARTHUB_INTFHUB_BASE_ADDR  0x160A0400
#define UARTHUB_WAKEUP_BASE_ADDR   0x1C03B000

#if UARTHUB_SUPPORT_FPGA
#define UARTHUB_DEV_0_BAUD_RATE    115200
#define UARTHUB_DEV_1_BAUD_RATE    115200
#define UARTHUB_DEV_2_BAUD_RATE    115200
#define UARTHUB_CMM_BAUD_RATE      115200
#else
#define UARTHUB_DEV_0_BAUD_RATE    12000000
#define UARTHUB_DEV_1_BAUD_RATE    4000000
#define UARTHUB_DEV_2_BAUD_RATE    12000000
#define UARTHUB_CMM_BAUD_RATE      12000000
#endif

#define TRX_BUF_LEN                64

int uarthub_uarthub_init_mt6991(struct platform_device *pdev);
int uarthub_uarthub_exit_mt6991(void);
int uarthub_uarthub_open_mt6991(void);
int uarthub_uarthub_close_mt6991(void);
int uarthub_is_apb_bus_clk_enable_mt6991(void);
int uarthub_get_hwccf_univpll_on_info_mt6991(void);
int uarthub_set_host_loopback_ctrl_mt6991(int dev_index, int tx_to_rx, int enable);
int uarthub_set_cmm_loopback_ctrl_mt6991(int tx_to_rx, int enable);
int uarthub_is_bypass_mode_mt6991(void);
int uarthub_set_host_trx_request_mt6991(int dev_index, enum uarthub_trx_type trx);
int uarthub_clear_host_trx_request_mt6991(int dev_index, enum uarthub_trx_type trx);
int uarthub_config_bypass_ctrl_mt6991(int enable);
int uarthub_config_baud_rate_m6991(void __iomem *dev_base, int rate_index);
int uarthub_usb_rx_pin_ctrl_mt6991(void __iomem *dev_base, int enable);
#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_spm_res_info_mt6991(
	int *p_spm_res_uarthub, int *p_spm_res_internal, int *p_spm_res_26m_off);
int uarthub_get_uarthub_cg_info_mt6991(int *p_topckgen_cg, int *p_peri_cg);
int uarthub_get_uart_src_clk_info_mt6991(void);
int uarthub_get_spm_sys_timer_mt6991(uint32_t *hi, uint32_t *lo);
#endif
int uarthub_get_uart_mux_info_mt6991(void);
int uarthub_get_uarthub_mux_info_mt6991(void);

int uarthub_inband_enable_ctrl_mt6991(int enable);
int uarthub_inband_irq_mask_ctrl_mt6991(int mask);
int uarthub_inband_irq_clear_ctrl_mt6991(void);
int uarthub_inband_irq_get_sta_mt6991(void);
unsigned char uarthub_inband_get_esc_sta_mt6991(void);
int uarthub_inband_clear_esc_sta_mt6991(void);
int uarthub_inband_set_esc_char_mt6991(unsigned char esc_char);
int uarthub_inband_set_esc_sta_mt6991(unsigned char esc_sta);
int uarthub_inband_is_tx_complete_mt6991(void);
int uarthub_inband_trigger_ctrl_mt6991(void);
int uarthub_inband_trigger_with_esc_sta_mt6991(unsigned char esc_sta);
int uarthub_get_bt_on_count_mt6991(void);
int uarthub_bt_on_count_inc_mt6991(void);

/* debug API */
int uarthub_get_intfhub_base_addr_mt6991(void);
int uarthub_get_uartip_base_addr_mt6991(int dev_index);
int uarthub_dump_uartip_debug_info_mt6991(
	const char *tag, struct mutex *uartip_lock);
int uarthub_dump_intfhub_debug_info_mt6991(const char *tag);
#if UARTHUB_WAKEUP_DEBUG_EN
int uarthub_dump_sspm_wakeup_debug_info_mt6991(const char *tag);
int uarthub_sspm_wakeup_enable_mt6991(void);
#endif
int uarthub_dump_debug_monitor_mt6991(const char *tag);
int uarthub_debug_monitor_ctrl_mt6991(int enable, int mode, int ctrl);
int uarthub_debug_monitor_stop_mt6991(int stop);
int uarthub_debug_monitor_clr_mt6991(void);
int uarthub_dump_inband_irq_debug_mt6991(const char *tag);
int uarthub_dump_debug_tx_rx_count_mt6991(const char *tag, int trigger_point);
int uarthub_dump_debug_clk_info_mt6991(const char *tag);
int uarthub_dump_debug_byte_cnt_info_mt6991(const char *tag);
int uarthub_dump_debug_apdma_uart_info_mt6991(const char *tag);
int uarthub_dump_debug_bus_status_info_mt6991(const char *tag);
int uarthub_dump_sspm_log_mt6991(const char *tag);
int uarthub_trigger_fpga_testing_mt6991(int type);
int uarthub_trigger_dvt_ut_testing_mt6991(int type);
int uarthub_trigger_dvt_it_testing_mt6991(int type);
int uarthub_read_dbg_monitor(int *sel, int *tx_monitor, int *rx_monitor);
int uarthub_record_check_data_mode_sta_to_buffer(
	unsigned char *dmp_info_buf, int len,
	int debug_monitor_sel,
	int *tx_monitor, int *rx_monitor,
	int tx_monitor_pointer, int rx_monitor_pointer,
	int check_data_mode_sel, const char *tag);

#endif /* MT6991_H */
