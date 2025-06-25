// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"

#include "inc/mt6991.h"
#include "inc/mt6991_debug.h"
#if UARTHUB_SUPPORT_DVT
#include "test/mt6991_test_api.h"
#endif

#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/types.h>

void __iomem *gpio_base_remap_addr_mt6991;
void __iomem *pericfg_ao_remap_addr_mt6991;
void __iomem *topckgen_base_remap_addr_mt6991;
void __iomem *intfhub_base_remap_addr_mt6991;
void __iomem *uarthub_wakeup_base_remap_addr_mt6991;
void __iomem *apdma_uart_tx_int_remap_addr_mt6991;
void __iomem *spm_remap_addr_mt6991;
void __iomem *apmixedsys_remap_addr_mt6991;
void __iomem *iocfg_tm3_remap_addr_mt6991;
void __iomem *sys_sram_remap_addr_mt6991;

void __iomem *uartip_base_map_mt6991[UARTHUB_MAX_NUM_DEV_HOST + 1] = { 0 };
void __iomem *apuart_base_map_mt6991[4] = { 0 };

int uarthub_dev_baud_rate_mt6991[UARTHUB_MAX_NUM_DEV_HOST + 1] = {
	UARTHUB_DEV_0_BAUD_RATE,
	UARTHUB_DEV_1_BAUD_RATE,
	UARTHUB_DEV_2_BAUD_RATE,
	UARTHUB_CMM_BAUD_RATE
};

unsigned long INTFHUB_BASE_MT6991;
unsigned long UARTHUB_WAKEUP_BASE_MT6991;

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
static atomic_t g_uarthub_pll_clk_on = ATOMIC_INIT(0);
#if UNIVPLL_CTRL_EN
static struct clk *clk_top_uarthub_bclk_sel;
static struct clk *clk_top_uart_sel;
static struct clk *clk_top_adsp_uarthub_bclk_sel;
static struct clk *clk_top_univpll_d6_d2_208m;
static struct clk *clk_top_univpll_d6_d4_104m;
static struct clk *clk_top_tck_26m_mx9;
#endif
#endif

static int uarthub_is_ready_state_mt6991(void);
static int uarthub_config_host_baud_rate_mt6991(int dev_index, int rate_index);
static int uarthub_config_cmm_baud_rate_mt6991(int rate_index);
static int uarthub_irq_mask_ctrl_mt6991(int mask);
static int uarthub_irq_clear_ctrl_mt6991(int irq_type);
static int uarthub_is_uarthub_clk_enable_mt6991(void);
static int uarthub_reset_to_ap_enable_only_mt6991(int ap_only);
static int uarthub_reset_flow_control_mt6991(void);
static int uarthub_is_assert_state_mt6991(void);
static int uarthub_assert_state_ctrl_mt6991(int assert_ctrl);
static int uarthub_get_host_status_mt6991(int dev_index);
static int uarthub_get_host_wakeup_status_mt6991(void);
static int uarthub_get_host_set_fw_own_status_mt6991(void);
static int uarthub_is_host_trx_idle_mt6991(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_host_byte_cnt_mt6991(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_cmm_byte_cnt_mt6991(enum uarthub_trx_type trx);
static int uarthub_config_crc_ctrl_mt6991(int enable);
static int uarthub_config_host_fifoe_ctrl_mt6991(int dev_index, int enable);
static int uarthub_get_rx_error_crc_info_mt6991(
	int dev_index, int *p_crc_error_data, int *p_crc_result);
static int uarthub_get_trx_timeout_info_mt6991(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter);
static int uarthub_get_irq_err_type_mt6991(void);
static int uarthub_init_remap_reg_mt6991(void);
static int uarthub_deinit_unmap_reg_mt6991(void);
static int uarthub_pll_clk_on_mt6991(int on, const char *tag);

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
static int uarthub_init_default_config_mt6991(void);
static int uarthub_init_trx_timeout_mt6991(void);
static int uarthub_init_debug_monitor_mt6991(void);
static int uarthub_uart_src_clk_ctrl(enum uarthub_clk_opp clk_opp);
static int uarthub_uarthub_mux_sel_ctrl(enum uarthub_clk_opp clk_opp);
static int uarthub_uart_mux_sel_ctrl(enum uarthub_clk_opp clk_opp);
static int uarthub_adsp_uarthub_mux_sel_ctrl(enum uarthub_clk_opp clk_opp);
static int uarthub_univpll_clk_init(struct platform_device *pdev);
static int uarthub_univpll_clk_exit(void);
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
static int uarthub_sspm_irq_clear_ctrl_mt6991(int irq_type);
static int uarthub_sspm_irq_get_sta_mt6991(void);
static int uarthub_sspm_irq_mask_ctrl_mt6991(int irq_type, int is_mask);
#endif

static int uarthub_get_bt_sleep_flow_hw_mech_en_mt6991(void);
static int uarthub_set_bt_sleep_flow_hw_mech_en_mt6991(int enable);
static int uarthub_get_host_awake_sta_mt6991(int dev_index);
static int uarthub_set_host_awake_sta_mt6991(int dev_index);
static int uarthub_clear_host_awake_sta_mt6991(int dev_index);
static int uarthub_get_host_bt_awake_sta_mt6991(int dev_index);
static int uarthub_get_cmm_bt_awake_sta_mt6991(void);
static int uarthub_get_bt_awake_sta_mt6991(void);

struct uarthub_ops_struct mt6991_plat_data = {
	.core_ops = &mt6991_plat_core_data,
	.debug_ops = &mt6991_plat_debug_data,
#if (UARTHUB_SUPPORT_DVT)
	.ut_test_ops = &mt6991_plat_ut_test_data,
#endif
};

struct uarthub_core_ops_struct mt6991_plat_core_data = {
	.uarthub_plat_is_apb_bus_clk_enable = uarthub_is_apb_bus_clk_enable_mt6991,
	.uarthub_plat_get_hwccf_univpll_on_info = uarthub_get_hwccf_univpll_on_info_mt6991,
	.uarthub_plat_is_ready_state = uarthub_is_ready_state_mt6991,
	.uarthub_plat_uarthub_init = uarthub_uarthub_init_mt6991,
	.uarthub_plat_uarthub_exit = uarthub_uarthub_exit_mt6991,
	.uarthub_plat_uarthub_open = uarthub_uarthub_open_mt6991,
	.uarthub_plat_uarthub_close = uarthub_uarthub_close_mt6991,
	.uarthub_plat_config_host_baud_rate = uarthub_config_host_baud_rate_mt6991,
	.uarthub_plat_config_cmm_baud_rate = uarthub_config_cmm_baud_rate_mt6991,
	.uarthub_plat_irq_mask_ctrl = uarthub_irq_mask_ctrl_mt6991,
	.uarthub_plat_irq_clear_ctrl = uarthub_irq_clear_ctrl_mt6991,
	.uarthub_plat_is_uarthub_clk_enable = uarthub_is_uarthub_clk_enable_mt6991,
	.uarthub_plat_set_host_loopback_ctrl = uarthub_set_host_loopback_ctrl_mt6991,
	.uarthub_plat_set_cmm_loopback_ctrl = uarthub_set_cmm_loopback_ctrl_mt6991,
	.uarthub_plat_reset_to_ap_enable_only = uarthub_reset_to_ap_enable_only_mt6991,
	.uarthub_plat_reset_flow_control = uarthub_reset_flow_control_mt6991,
	.uarthub_plat_is_assert_state = uarthub_is_assert_state_mt6991,
	.uarthub_plat_assert_state_ctrl = uarthub_assert_state_ctrl_mt6991,
	.uarthub_plat_is_bypass_mode = uarthub_is_bypass_mode_mt6991,
	.uarthub_plat_get_host_status = uarthub_get_host_status_mt6991,
	.uarthub_plat_get_host_wakeup_status = uarthub_get_host_wakeup_status_mt6991,
	.uarthub_plat_get_host_set_fw_own_status = uarthub_get_host_set_fw_own_status_mt6991,
	.uarthub_plat_is_host_trx_idle = uarthub_is_host_trx_idle_mt6991,
	.uarthub_plat_set_host_trx_request = uarthub_set_host_trx_request_mt6991,
	.uarthub_plat_clear_host_trx_request = uarthub_clear_host_trx_request_mt6991,
	.uarthub_plat_get_host_byte_cnt = uarthub_get_host_byte_cnt_mt6991,
	.uarthub_plat_get_cmm_byte_cnt = uarthub_get_cmm_byte_cnt_mt6991,
	.uarthub_plat_config_crc_ctrl = uarthub_config_crc_ctrl_mt6991,
	.uarthub_plat_config_bypass_ctrl = uarthub_config_bypass_ctrl_mt6991,
	.uarthub_plat_config_host_fifoe_ctrl = uarthub_config_host_fifoe_ctrl_mt6991,
	.uarthub_plat_get_rx_error_crc_info = uarthub_get_rx_error_crc_info_mt6991,
	.uarthub_plat_get_trx_timeout_info = uarthub_get_trx_timeout_info_mt6991,
	.uarthub_plat_get_irq_err_type = uarthub_get_irq_err_type_mt6991,
	.uarthub_plat_init_remap_reg = uarthub_init_remap_reg_mt6991,
	.uarthub_plat_deinit_unmap_reg = uarthub_deinit_unmap_reg_mt6991,
#if !(UARTHUB_SUPPORT_FPGA)
	.uarthub_plat_get_spm_sys_timer = uarthub_get_spm_sys_timer_mt6991,
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	.uarthub_plat_sspm_irq_clear_ctrl = uarthub_sspm_irq_clear_ctrl_mt6991,
	.uarthub_plat_sspm_irq_get_sta = uarthub_sspm_irq_get_sta_mt6991,
	.uarthub_plat_sspm_irq_mask_ctrl = uarthub_sspm_irq_mask_ctrl_mt6991,
#endif

	.uarthub_plat_inband_enable_ctrl = uarthub_inband_enable_ctrl_mt6991,
	.uarthub_plat_inband_irq_mask_ctrl = uarthub_inband_irq_mask_ctrl_mt6991,
	.uarthub_plat_inband_irq_clear_ctrl = uarthub_inband_irq_clear_ctrl_mt6991,
	.uarthub_plat_inband_irq_get_sta = uarthub_inband_irq_get_sta_mt6991,
	.uarthub_plat_inband_get_esc_sta = uarthub_inband_get_esc_sta_mt6991,
	.uarthub_plat_inband_clear_esc_sta = uarthub_inband_clear_esc_sta_mt6991,
	.uarthub_plat_inband_set_esc_char = uarthub_inband_set_esc_char_mt6991,
	.uarthub_plat_inband_set_esc_sta = uarthub_inband_set_esc_sta_mt6991,
	.uarthub_plat_inband_is_tx_complete = uarthub_inband_is_tx_complete_mt6991,
	.uarthub_plat_inband_trigger_ctrl = uarthub_inband_trigger_ctrl_mt6991,

	.uarthub_plat_get_bt_sleep_flow_hw_mech_en = uarthub_get_bt_sleep_flow_hw_mech_en_mt6991,
	.uarthub_plat_set_bt_sleep_flow_hw_mech_en = uarthub_set_bt_sleep_flow_hw_mech_en_mt6991,
	.uarthub_plat_get_host_awake_sta = uarthub_get_host_awake_sta_mt6991,
	.uarthub_plat_set_host_awake_sta = uarthub_set_host_awake_sta_mt6991,
	.uarthub_plat_clear_host_awake_sta = uarthub_clear_host_awake_sta_mt6991,
	.uarthub_plat_get_host_bt_awake_sta = uarthub_get_host_bt_awake_sta_mt6991,
	.uarthub_plat_get_cmm_bt_awake_sta = uarthub_get_cmm_bt_awake_sta_mt6991,
	.uarthub_plat_get_bt_awake_sta = uarthub_get_bt_awake_sta_mt6991,
	.uarthub_plat_bt_on_count_inc = uarthub_bt_on_count_inc_mt6991,
};

int uarthub_is_apb_bus_clk_enable_mt6991(void)
{
#if !(UARTHUB_SUPPORT_FPGA)
	int state = 0;

	if (!pericfg_ao_remap_addr_mt6991) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_2[20] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_2[19] = PERI_UARTHUB_hclk_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6991 + PERI_CG_2,
		PERI_CG_2_UARTHUB_CLK_CG_MASK) >> PERI_CG_2_UARTHUB_CLK_CG_SHIFT);

	if (state != 0x0) {
		pr_notice("[%s] UARTHUB PCLK/HCLK CG gating(0x%x,exp:0x0)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_start(CON1_ADDR);

	if (state != 0x81) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x81)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_end(CON1_ADDR);

	if (state != 0x85) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x85)\n", __func__, state);
		return 0;
	}
#endif

	return 1;
}

int uarthub_get_hwccf_univpll_on_info_mt6991(void)
{
#if UNIVPLL_CTRL_EN
	if (!apmixedsys_remap_addr_mt6991) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr_mt6991 + FENC_STATUS_CON0,
		RG_UNIVPLL_FENC_STATUS_MASK) >> RG_UNIVPLL_FENC_STATUS_SHIFT);
#else
	return 1;
#endif
}

int uarthub_is_ready_state_mt6991(void)
{
	return DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
}

int uarthub_config_baud_rate_m6991(void __iomem *dev_base, int rate_index)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	if (rate_index == 115200) {
#if UARTHUB_SUPPORT_FPGA
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x55); /* 0x28 = 0x55 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x2B); /* 0x2c = 0x2B */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xEF);    /* 0x54 = 0xEF */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
#else
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0xe0); /* 0x28 = 0xe0 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x70); /* 0x2c = 0x70 */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x4);           /* 0x90 = 0x4  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xf6);    /* 0x54 = 0xf6 */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
#endif
	} else if (rate_index == 3000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x21); /* 0x28 = 0x21 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x11); /* 0x2c = 0x11 */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xdb);    /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
	} else if (rate_index == 4000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x19); /* 0x28 = 0x19 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0xd);  /* 0x2c = 0xd  */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0x0);     /* 0x54 = 0x0  */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x0);     /* 0x58 = 0x0  */
	} else if (rate_index == 12000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x7);  /* 0x28 = 0x7  */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x4);  /* 0x2c = 0x4  */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xdb);    /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
	} else if (rate_index == 16000000) {
		/* TODO: support 16M baud rate */
	} else if (rate_index == 24000000) {
		/* TODO: support 24M baud rate */
	} else {
		pr_notice("[%s] not support rate_index(%d)\n", __func__, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_config_host_baud_rate_mt6991(int dev_index, int rate_index)
{
	int iRtn = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	iRtn = uarthub_config_baud_rate_m6991(uartip_base_map_mt6991[dev_index], rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, dev_index=[%d], rate_index=[%d]\n",
			__func__, dev_index, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_config_cmm_baud_rate_mt6991(int rate_index)
{
	int iRtn = 0;

	iRtn = uarthub_config_baud_rate_m6991(
		uartip_base_map_mt6991[uartip_id_cmm], rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, rate_index=[%d]\n",
			__func__, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_irq_mask_ctrl_mt6991(int mask)
{
	if (mask == 0)
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0x0);
	else
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0xFFFFFFFF);

	return 0;
}

int uarthub_irq_clear_ctrl_mt6991(int irq_type)
{
	UARTHUB_SET_BIT(DEV0_IRQ_CLR_ADDR, irq_type);
	return 0;
}

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_sspm_irq_clear_ctrl_mt6991(int irq_type)
{
	UARTHUB_SET_BIT(IRQ_CLR_ADDR, irq_type);
	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_sspm_irq_get_sta_mt6991(void)
{
	int mask = 0;

	mask = REG_FLD_MASK(IRQ_STA_FLD_intfhub_restore_req) |
		REG_FLD_MASK(IRQ_STA_FLD_intfhub_ckoff_req) |
		REG_FLD_MASK(IRQ_STA_FLD_intfhub_ckon_req);

	return UARTHUB_REG_READ_BIT(IRQ_STA_ADDR, mask);
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_sspm_irq_mask_ctrl_mt6991(int irq_type, int is_mask)
{
	int mask = 0;

	mask = REG_FLD_MASK(IRQ_MASK_FLD_intfhub_restore_req_mask) |
		REG_FLD_MASK(IRQ_MASK_FLD_intfhub_ckoff_req_mask) |
		REG_FLD_MASK(IRQ_MASK_FLD_intfhub_ckon_req_mask);

	UARTHUB_REG_WRITE_MASK(IRQ_MASK_ADDR,
		((is_mask == 1) ? irq_type : 0x0), (irq_type & mask));

	return 0;
}
#endif

int uarthub_is_uarthub_clk_enable_mt6991(void)
{
#if !(UARTHUB_SUPPORT_FPGA)
	int state = 0;
	int spm_res_uarthub = 0, spm_res_internal = 0, spm_res_26m_off = 0;

	if (!pericfg_ao_remap_addr_mt6991) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	if (!apmixedsys_remap_addr_mt6991) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6991) {
		pr_notice("[%s] spm_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_2[20] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_2[19] = PERI_UARTHUB_hclk_CG */
	/* PERI_CG_2[18] = PERI_UARTHUB_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6991 + PERI_CG_2,
		PERI_CG_2_UARTHUB_CG_MASK) >> PERI_CG_2_UARTHUB_CG_SHIFT);

	if (state != 0x0) {
		pr_notice("[%s] UARTHUB CG gating(0x%x,exp:0x0)\n", __func__, state);
		return 0;
	}

	state = uarthub_get_hwccf_univpll_on_info_mt6991();

	if (state != 0x1) {
		pr_notice("[%s] UNIVPLL CLK is OFF(0x%x,exp:0x1)\n", __func__, state);
		return 0;
	}

	state = uarthub_get_spm_res_info_mt6991(
		&spm_res_uarthub, &spm_res_internal, &spm_res_26m_off);

	if (state < 1) {
		pr_notice("[%s] UARTHUB SPM REQ(0x%x/0x%x/0x%x,exp:0x1E/0x17/0x0)\n",
			__func__, spm_res_uarthub, spm_res_internal, spm_res_26m_off);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_start(CON1_ADDR);

	if (state != 0x81) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x81)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_end(CON1_ADDR);

	if (state != 0x85) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x85)\n", __func__, state);
		return 0;
	}

	state = DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR) +
		DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR);

	if (state == 0) {
		pr_notice("[%s] AP host clear the sw tx/rx req\n", __func__);
		return 0;
	}
#endif

	return 1;
}

int uarthub_set_host_loopback_ctrl_mt6991(int dev_index, int tx_to_rx, int enable)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev0_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev0_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	} else if (dev_index == 1) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev1_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev1_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	} else if (dev_index == 2) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev2_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev2_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	}

	return 0;
}

int uarthub_set_cmm_loopback_ctrl_mt6991(int tx_to_rx, int enable)
{
	if (tx_to_rx == 0)
		LOOPBACK_SET_dev_bt_rx2tx_loopback(LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	else
		LOOPBACK_SET_dev_cmm_tx2rx_loopback(LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));

	return 0;
}

int uarthub_reset_to_ap_enable_only_mt6991(int ap_only)
{
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int trx_mask = 0, trx_state = 0;
	int dev1_fifoe = -1, dev2_fifoe = -1;

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		if (dev0_sta == 0x300 ||  dev0_sta == 0x0) {
			pr_notice("[%s] all host sta is[0x%x]\n", __func__, dev0_sta);
			return -1;
		}
	}

	dev1_fifoe = FCR_RD_GET_FIFOE(FCR_RD_ADDR(uartip_base_map_mt6991[uartip_id_md]));
	dev2_fifoe = FCR_RD_GET_FIFOE(FCR_RD_ADDR(uartip_base_map_mt6991[uartip_id_adsp]));

	trx_mask = REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_rx_sta) |
		REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_tx_sta);
	trx_state = UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, trx_mask);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev1/2_fifoe=[%d/%d], trx_state=[0x%x], trx_state=[0x%x]\n",
		__func__, dev1_fifoe, dev2_fifoe, trx_mask, trx_state);
#endif

	/* set trx request */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_mask);

	/* disable and clear uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0xC0);
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_ap]), 0xC0);
#if MD_CHANNEL_EN
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_md]), 0xC0);
#endif
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_adsp]), 0xC0);

	/* sw_rst4 */
	CON4_SET_sw4_rst(CON4_ADDR, 1);

	/* enable uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0xC1);
	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_ap]), 0xC1);

#if MD_CHANNEL_EN
	if (dev1_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_md]), 0xC1);
#endif

	if (dev2_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[uartip_id_adsp]), 0xC1);

	/* restore trx request state */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_state);

	return 0;
}

int uarthub_reset_flow_control_mt6991(void)
{
	void __iomem *uarthub_dev_base = NULL;
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int retry = 0;
	int i = 0;

	debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_ap]));
	debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_md]));
	debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_adsp]));
	debug8.cmm = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_cmm]));

	if (UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0) == 0 &&
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1) == 0 &&
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2) == 0 &&
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm) == 0)
		return 0;

	debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_ap]));
	debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_md]));
	debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_adsp]));
	debug1.cmm = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_cmm]));

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][BEGIN] xcstate(wsend_xoff)=[%d-%d-%d-%d]",
		__func__,
		UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev0),
		UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev1),
		UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev2),
		UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.cmm));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",swtxdis(det_xoff)=[%d-%d-%d-%d]",
		UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0),
		UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1),
		UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2),
		UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm));

	pr_info("%s\n", dmp_info_buf);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		uarthub_dev_base = uartip_base_map_mt6991[i];

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		if (retry <= 0)
			pr_info("[%s][init] txstate[0x%x] is not idle state[0x0]\n",
				__func__, (val & 0x1f));

		UARTHUB_REG_WRITE(MCR_ADDR(uarthub_dev_base), 0x10);
		UARTHUB_REG_WRITE(DMA_EN_ADDR(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(FCR_ADDR(uarthub_dev_base), 0xC0);
		UARTHUB_REG_WRITE(SLEEP_REQ_ADDR(uarthub_dev_base), 0x1);
		UARTHUB_REG_WRITE(SLEEP_EN_ADDR(uarthub_dev_base), 0x1);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		if (retry <= 0)
			pr_info("[%s][slp_req_en] txstate[0x%x] is not idle state[0x0]\n",
				__func__, (val & 0x1f));

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_ap]));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_md]));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_adsp]));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_cmm]));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_ap]));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_md]));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_adsp]));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_cmm]));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ][%d] xcstate(wsend_xoff)=[%d-%d-%d-%d]",
			__func__, i,
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev0),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev1),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev2),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.cmm));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",swtxdis(det_xoff)=[%d-%d-%d-%d]",
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm));

		pr_info("%s\n", dmp_info_buf);

		UARTHUB_REG_WRITE(SLEEP_REQ_ADDR(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(SLEEP_EN_ADDR(uarthub_dev_base), 0x0);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		if (retry <= 0)
			pr_info("[%s][slp_req_dis] txstate[0x%x] is not idle state[0x0]\n",
				__func__, (val & 0x1f));

		UARTHUB_REG_WRITE(FCR_ADDR(uarthub_dev_base), 0xC1);
		UARTHUB_REG_WRITE(DMA_EN_ADDR(uarthub_dev_base), 0x3);
		UARTHUB_REG_WRITE(MCR_ADDR(uarthub_dev_base), 0x0);

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_ap]));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_md]));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_adsp]));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(uartip_base_map_mt6991[uartip_id_cmm]));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_ap]));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_md]));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_adsp]));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(uartip_base_map_mt6991[uartip_id_cmm]));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ_DIS][%d] xcstate(wsend_xoff)=[%d-%d-%d-%d]",
			__func__, i,
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev0),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev1),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev2),
			UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.cmm));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",swtxdis(det_xoff)=[%d-%d-%d-%d]",
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2),
			UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm));

		pr_info("%s\n", dmp_info_buf);
	}

	return 0;
}

int uarthub_is_assert_state_mt6991(void)
{
	int state = 0;

	state = STA0_GET_bt_on_sta(STA0_ADDR);
	return state;
}

int uarthub_assert_state_ctrl_mt6991(int assert_ctrl)
{
	if (assert_ctrl == uarthub_is_assert_state_mt6991()) {
		if (assert_ctrl == 1)
			pr_info("[%s] assert state has been set\n", __func__);
		else
			pr_info("[%s] assert state has been cleared\n", __func__);
		return 0;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] assert_ctrl=[%d]\n", __func__, assert_ctrl);
#endif

	if (assert_ctrl == 1) {
		STA0_SET_bt_on_sta(STA0_ADDR, 0x1);
	} else {
		STA0_SET_bt_on_sta(STA0_ADDR, 0x0);
		uarthub_irq_clear_ctrl_mt6991(BIT_0xFFFF_FFFF);
		uarthub_irq_mask_ctrl_mt6991(0);
	}

	return 0;
}

int uarthub_is_bypass_mode_mt6991(void)
{
	int state = 0;

	state = CON2_GET_intfhub_bypass(CON2_ADDR);
	return state;
}

int uarthub_usb_rx_pin_ctrl_mt6991(void __iomem *dev_base, int enable)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	UARTHUB_REG_WRITE(
		USB_RX_SEL_ADDR(dev_base), ((enable == 1) ? 0x1 : 0x0));
	return 0;
}

#if UARTHUB_WAKEUP_DEBUG_EN
int uarthub_sspm_wakeup_enable_mt6991(void)
{
	SPM_26M_ULPOSC_REQ_CTRL_SET_spm_26m_ulposc_req(SPM_26M_ULPOSC_REQ_CTRL_ADDR, 0x1);
	UARTHUB_REG_WRITE(SSPM_WAKEUP_IRQ_STA_CLR_ADDR, 0xF);
	UARTHUB_REG_WRITE(SSPM_WAKEUP_IRQ_STA_MASK_ADDR, 0x0);
	UARTHUB_REG_WRITE(SSPM_WAKEUP_IRQ_STA_EN_ADDR, 0xF);
	return 0;
}
#endif

int uarthub_uarthub_init_mt6991(struct platform_device *pdev)
{
	/* default assert mode enable */
	/* assert mode enable --> BT off or assert state*/
	uarthub_assert_state_ctrl_mt6991(1);

#if UARTHUB_WAKEUP_DEBUG_EN
	uarthub_sspm_wakeup_enable_mt6991();
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	/* init UNIVPLL clk from dts node */
	uarthub_univpll_clk_init(pdev);

	/* init uarthub config */
	uarthub_init_default_config_mt6991();

	/* bypass mode enable */
	uarthub_config_bypass_ctrl_mt6991(0);

	/* crc enable */
	uarthub_config_crc_ctrl_mt6991(1);

	/* config tx/rx timeout */
	uarthub_init_trx_timeout_mt6991();

	/* config debug monitor to check data mode */
	uarthub_init_debug_monitor_mt6991();

	/* config dx-4 wakeup/sleep flow */
	FEEDBACK_DATA1_SET_feedback_data1(FEEDBACK_DATA1_ADDR, 0x02FDFF01);
	CON3_SET_feedback_header_mode(CON3_ADDR, 0x0);
	STA0_SET_feedback_pkt_type(STA0_ADDR, 0x1);

	/* mask sspm irq, feedback_host_awake_tx_done */
	IRQ_MASK_SET_feedback_host_awake_tx_done_mask(IRQ_MASK_ADDR, 1);

	/* disable not_ready_block_rx_data*/
	RX_DATA_REQ_MASK_SET_not_ready_block_rx_data_en(RX_DATA_REQ_MASK_ADDR, 0);

#if UARTHUB_INFO_LOG
	uarthub_dump_intfhub_debug_info_mt6991(__func__);
	uarthub_dump_uartip_debug_info_mt6991(__func__, NULL);
#endif
#else
#if UARTHUB_INFO_LOG
	uarthub_dump_debug_clk_info_mt6991(__func__);
#endif
#endif

	return 0;
}

int uarthub_uarthub_exit_mt6991(void)
{
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	uarthub_univpll_clk_exit();
#endif
	return 0;
}

int uarthub_pll_clk_on_mt6991(int on, const char *tag)
{
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	if (on != 0 && on != 1) {
		pr_info("[%s] parameter error, on=[%d]\n", __func__, on);
		return UARTHUB_ERR_PARA_WRONG;
	}

	if (atomic_read(&g_uarthub_pll_clk_on) == on) {
		pr_info("[%s] on=[%d], g_uarthub_pll_clk_on=[%d]\n",
			__func__, on, atomic_read(&g_uarthub_pll_clk_on));
		return 0;
	}

	/* config ap_uart source clock to TOPCKGEN */
	uarthub_uart_src_clk_ctrl((on == 1) ? uarthub_clk_topckgen : uarthub_clk_26m);

	/* config TOPCKGEN ap_uart mux to 208m */
	uarthub_uart_mux_sel_ctrl((on == 1) ? uarthub_clk_208m : uarthub_clk_26m);

	/* config TOPCKGEN uarthub mux to 208m */
	uarthub_uarthub_mux_sel_ctrl((on == 1) ? uarthub_clk_208m : uarthub_clk_26m);

	/* config TOPCKGEN adsp_uarthub mux to 208m */
	uarthub_adsp_uarthub_mux_sel_ctrl((on == 1) ? uarthub_clk_208m : uarthub_clk_26m);

	atomic_set(&g_uarthub_pll_clk_on, on);

#if UARTHUB_INFO_LOG
	uarthub_dump_debug_clk_info_mt6991(tag);
#endif
#endif

	return 0;
}

int uarthub_uarthub_open_mt6991(void)
{
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	uarthub_pll_clk_on_mt6991(1, __func__);
	uarthub_init_default_config_mt6991();
	uarthub_config_crc_ctrl_mt6991(1);
	uarthub_init_trx_timeout_mt6991();
	uarthub_init_debug_monitor_mt6991();

#if UARTHUB_INFO_LOG
	uarthub_dump_intfhub_debug_info_mt6991(__func__);
	uarthub_dump_uartip_debug_info_mt6991(__func__, NULL);
#endif
#endif

	return 0;
}

int uarthub_uarthub_close_mt6991(void)
{
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	uarthub_pll_clk_on_mt6991(0, __func__);
#if UARTHUB_INFO_LOG
	uarthub_dump_intfhub_debug_info_mt6991(__func__);
	uarthub_dump_uartip_debug_info_mt6991(__func__, NULL);
#endif
#endif

	return 0;
}

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_uart_src_clk_ctrl(enum uarthub_clk_opp clk_opp)
{
	if (!pericfg_ao_remap_addr_mt6991) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	if (clk_opp != uarthub_clk_topckgen && clk_opp != uarthub_clk_26m) {
		pr_notice("[%s] clk_opp value is not support(%d)\n", __func__, clk_opp);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE_MASK(pericfg_ao_remap_addr_mt6991 + PERI_CLOCK_CON,
		((clk_opp == uarthub_clk_topckgen) ? PERI_UART_FBCLK_CKSEL_UART_CK :
		PERI_UART_FBCLK_CKSEL_26M_CK), PERI_UART_FBCLK_CKSEL_MASK);

	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_uart_mux_sel_ctrl(enum uarthub_clk_opp clk_opp)
{
#if UNIVPLL_CTRL_EN
	int ret = 0;

	if (clk_opp != uarthub_clk_26m && clk_opp != uarthub_clk_104m && clk_opp != uarthub_clk_208m) {
		pr_notice("[%s] clk_opp value is not support(%d)\n", __func__, clk_opp);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

#if SUPPORT_HWCCF
	if (IS_ERR_OR_NULL(clk_top_uart_sel)) {
		pr_notice("[%s] clk_top_uart_sel is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d2_208m)) {
		pr_notice("[%s] clk_top_univpll_d6_d2_208m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d4_104m)) {
		pr_notice("[%s] clk_top_univpll_d6_d4_104m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_tck_26m_mx9)) {
		pr_notice("[%s] clk_top_tck_26m_mx9 is not init\n", __func__);
		return -1;
	}

	ret = clk_set_parent(clk_top_uart_sel,
		((clk_opp == uarthub_clk_208m) ? clk_top_univpll_d6_d2_208m :
		((clk_opp == uarthub_clk_104m) ? clk_top_univpll_d6_d4_104m : clk_top_tck_26m_mx9)));

	if (ret)
		pr_notice("[%s] uart_sel set clk parent to %s failed, %d\n",
			__func__,
			((clk_opp == uarthub_clk_208m) ? "clk208m" :
			((clk_opp == uarthub_clk_104m) ? "clk104m" : "clk26m")), ret);
#else
	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_6_CLR,
		CLK_CFG_6_UART_SEL_MASK);

	if (clk_opp == uarthub_clk_104m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_6_SET,
			CLK_CFG_6_UART_SEL_104M);
	else if (clk_opp == uarthub_clk_208m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_6_SET,
			CLK_CFG_6_UART_SEL_208M);

	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_UPDATE,
		CLK_CFG_UPDATE_UART_CK_UPDATE_MASK);
#endif
	return ret;
#else
	return 0;
#endif
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_uarthub_mux_sel_ctrl(enum uarthub_clk_opp clk_opp)
{
#if UNIVPLL_CTRL_EN
	int ret = 0;

	if (clk_opp != uarthub_clk_26m && clk_opp != uarthub_clk_104m && clk_opp != uarthub_clk_208m) {
		pr_notice("[%s] clk_opp value is not support(%d)\n", __func__, clk_opp);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

#if SUPPORT_HWCCF
	if (IS_ERR_OR_NULL(clk_top_uarthub_bclk_sel)) {
		pr_notice("[%s] clk_top_uarthub_bclk_sel is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d2_208m)) {
		pr_notice("[%s] clk_top_univpll_d6_d2_208m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d4_104m)) {
		pr_notice("[%s] clk_top_univpll_d6_d4_104m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_tck_26m_mx9)) {
		pr_notice("[%s] clk_top_tck_26m_mx9 is not init\n", __func__);
		return -1;
	}

	ret = clk_set_parent(clk_top_uarthub_bclk_sel,
		((clk_opp == uarthub_clk_208m) ? clk_top_univpll_d6_d2_208m :
		((clk_opp == uarthub_clk_104m) ? clk_top_univpll_d6_d4_104m : clk_top_tck_26m_mx9)));

	if (ret)
		pr_notice("[%s] uarthub_sel set clk parent to %s failed, %d\n",
			__func__,
			((clk_opp == uarthub_clk_208m) ? "clk208m" :
			((clk_opp == uarthub_clk_104m) ? "clk104m" : "clk26m")), ret);
#else
	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_16_CLR,
		CLK_CFG_16_UARTHUB_BCLK_SEL_MASK);

	if (clk_opp == uarthub_clk_104m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_16_SET,
			CLK_CFG_16_UARTHUB_BCLK_SEL_104M);
	else if (clk_opp == uarthub_clk_208m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_16_SET,
			CLK_CFG_16_UARTHUB_BCLK_SEL_208M);

	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_UPDATE2,
		CLK_CFG_UPDATE2_UARTHUB_BCLK_UPDATE_MASK);
#endif
	return ret;
#else
	return 0;
#endif
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_adsp_uarthub_mux_sel_ctrl(enum uarthub_clk_opp clk_opp)
{
#if UNIVPLL_CTRL_EN
	int ret = 0;

	if (clk_opp != uarthub_clk_26m && clk_opp != uarthub_clk_104m && clk_opp != uarthub_clk_208m) {
		pr_notice("[%s] clk_opp value is not support(%d)\n", __func__, clk_opp);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

#if SUPPORT_HWCCF
	if (IS_ERR_OR_NULL(clk_top_adsp_uarthub_bclk_sel)) {
		pr_notice("[%s] clk_top_adsp_uarthub_bclk_sel is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d2_208m)) {
		pr_notice("[%s] clk_top_univpll_d6_d2_208m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d4_104m)) {
		pr_notice("[%s] clk_top_univpll_d6_d4_104m is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_tck_26m_mx9)) {
		pr_notice("[%s] clk_top_tck_26m_mx9 is not init\n", __func__);
		return -1;
	}

	ret = clk_set_parent(clk_top_adsp_uarthub_bclk_sel,
		((clk_opp == uarthub_clk_208m) ? clk_top_univpll_d6_d2_208m :
		((clk_opp == uarthub_clk_104m) ? clk_top_univpll_d6_d4_104m : clk_top_tck_26m_mx9)));

	if (ret)
		pr_notice("[%s] adsp_uarthub_sel set clk parent to %s failed, %d\n",
			__func__,
			((clk_opp == uarthub_clk_208m) ? "clk208m" :
			((clk_opp == uarthub_clk_104m) ? "clk104m" : "clk26m")), ret);
#else
	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_13_CLR,
		CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_MASK);

	if (clk_opp == uarthub_clk_104m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_13_SET,
			CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_104M);
	else if (clk_opp == uarthub_clk_208m)
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_13_SET,
			CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_208M);

	UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6991 + CLK_CFG_UPDATE1,
		CLK_CFG_UPDATE1_ADSP_UARTHUB_BCLK_UPDATE_MASK);
#endif
	return ret;
#else
	return 0;
#endif
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_univpll_clk_init(struct platform_device *pdev)
{
#if UNIVPLL_CTRL_EN
	int ret = 0;

	if (!pdev) {
		pr_notice("[%s] pdev is NULL\n", __func__);
		return UARTHUB_ERR_PARA_WRONG;
	}

	clk_top_uarthub_bclk_sel = devm_clk_get(&pdev->dev, "clk_top_uarthub_bclk_sel");
	if (IS_ERR_OR_NULL(clk_top_uarthub_bclk_sel)) {
		pr_notice("[%s] cannot get clk_top_uarthub_bclk_sel clock.\n",
			__func__);
		return PTR_ERR(clk_top_uarthub_bclk_sel);
	}
	pr_info("[%s] clk_top_uarthub_bclk_sel=[%p]\n",
		__func__, clk_top_uarthub_bclk_sel);

	clk_top_uart_sel = devm_clk_get(&pdev->dev, "clk_top_uart_sel");
	if (IS_ERR_OR_NULL(clk_top_uart_sel)) {
		pr_notice("[%s] cannot get clk_top_uart_sel clock.\n",
			__func__);
		return PTR_ERR(clk_top_uart_sel);
	}
	pr_info("[%s] clk_top_uart_sel=[%p]\n",
		__func__, clk_top_uart_sel);

	clk_top_adsp_uarthub_bclk_sel = devm_clk_get(&pdev->dev, "clk_top_adsp_uarthub_bclk_sel");
	if (IS_ERR_OR_NULL(clk_top_adsp_uarthub_bclk_sel)) {
		pr_notice("[%s] cannot get clk_top_adsp_uarthub_bclk_sel clock.\n",
			__func__);
		return PTR_ERR(clk_top_adsp_uarthub_bclk_sel);
	}
	pr_info("[%s] clk_top_adsp_uarthub_bclk_sel=[%p]\n",
		__func__, clk_top_adsp_uarthub_bclk_sel);

	clk_top_univpll_d6_d2_208m = devm_clk_get(&pdev->dev, "clk_top_univpll_d6_d2_208m");
	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d2_208m)) {
		pr_notice("[%s] cannot get clk_top_univpll_d6_d2_208m clock.\n",
			__func__);
		return PTR_ERR(clk_top_univpll_d6_d2_208m);
	}
	pr_info("[%s] clk_top_univpll_d6_d2_208m=[%p]\n",
		__func__, clk_top_univpll_d6_d2_208m);

	clk_top_univpll_d6_d4_104m = devm_clk_get(&pdev->dev, "clk_top_univpll_d6_d4_104m");
	if (IS_ERR_OR_NULL(clk_top_univpll_d6_d4_104m)) {
		pr_notice("[%s] cannot get clk_top_univpll_d6_d4_104m clock.\n",
			__func__);
		return PTR_ERR(clk_top_univpll_d6_d4_104m);
	}
	pr_info("[%s] clk_top_univpll_d6_d4_104m=[%p]\n",
		__func__, clk_top_univpll_d6_d4_104m);

	clk_top_tck_26m_mx9 = devm_clk_get(&pdev->dev, "clk_top_tck_26m_mx9");
	if (IS_ERR_OR_NULL(clk_top_tck_26m_mx9)) {
		pr_notice("[%s] cannot get clk_top_tck_26m_mx9 clock.\n",
			__func__);
		return PTR_ERR(clk_top_tck_26m_mx9);
	}
	pr_info("[%s] clk_top_tck_26m_mx9=[%p]\n",
		__func__, clk_top_tck_26m_mx9);

	ret = clk_prepare_enable(clk_top_uarthub_bclk_sel);
	if (ret) {
		pr_notice("[%s] prepare uarthub_sel fail(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(clk_top_uart_sel);
	if (ret) {
		pr_notice("[%s] prepare uart_sel fail(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(clk_top_adsp_uarthub_bclk_sel);
	if (ret) {
		pr_notice("[%s] prepare adsp_uarthub_sel fail(%d)\n",
			__func__, ret);
		return ret;
	}
#endif

	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_univpll_clk_exit(void)
{
#if UNIVPLL_CTRL_EN
	if (IS_ERR_OR_NULL(clk_top_uarthub_bclk_sel)) {
		pr_notice("[%s] clk_top_uarthub_bclk_sel is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_uart_sel)) {
		pr_notice("[%s] clk_top_uart_sel is not init\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(clk_top_adsp_uarthub_bclk_sel)) {
		pr_notice("[%s] clk_top_adsp_uarthub_bclk_sel is not init\n", __func__);
		return -1;
	}

	clk_disable_unprepare(clk_top_uarthub_bclk_sel);
	clk_disable_unprepare(clk_top_uart_sel);
	clk_disable_unprepare(clk_top_adsp_uarthub_bclk_sel);
#endif

	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_init_default_config_mt6991(void)
{
	void __iomem *uarthub_dev_base = NULL;
	int baud_rate = 0;
	int i = 0;

	if (uartip_base_map_mt6991[uartip_id_ap] == NULL ||
			uartip_base_map_mt6991[uartip_id_md] == NULL ||
			uartip_base_map_mt6991[uartip_id_adsp] == NULL ||
			uartip_base_map_mt6991[uartip_id_cmm] == NULL) {
		pr_notice("[%s] uartip_base_map_mt6991[ap/md/adsp/cmm] is not all init\n",
			__func__);
		return -1;
	}

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		uarthub_usb_rx_pin_ctrl_mt6991(uartip_base_map_mt6991[i], 1);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		uarthub_dev_base = uartip_base_map_mt6991[i];
		baud_rate = uarthub_dev_baud_rate_mt6991[i];

		if (baud_rate >= 0)
			uarthub_config_baud_rate_m6991(uarthub_dev_base, baud_rate);

		/* 0x0c = 0x3,  byte length: 8 bit*/
		UARTHUB_REG_WRITE(LCR_ADDR(uarthub_dev_base), 0x3);
		/* 0x98 = 0xa,  xon1/xoff1 flow control enable */
		UARTHUB_REG_WRITE(EFR_ADDR(uarthub_dev_base), 0xa);
		/* 0xa8 = 0x13, xoff1 keyword */
		UARTHUB_REG_WRITE(XOFF1_ADDR(uarthub_dev_base), 0x13);
		/* 0xa0 = 0x11, xon1 keyword */
		UARTHUB_REG_WRITE(XON1_ADDR(uarthub_dev_base), 0x11);
		/* 0xac = 0x13, xoff2 keyword */
		UARTHUB_REG_WRITE(XOFF2_ADDR(uarthub_dev_base), 0x13);
		/* 0xa4 = 0x11, xon2 keyword */
		UARTHUB_REG_WRITE(XON2_ADDR(uarthub_dev_base), 0x11);
		/* 0x44 = 0x1,  esc char enable */
		UARTHUB_REG_WRITE(ESCAPE_EN_ADDR(uarthub_dev_base), 0x1);
		/* 0x40 = 0xdb, esc char */
		UARTHUB_REG_WRITE(ESCAPE_DAT_ADDR(uarthub_dev_base), 0xdb);
	}

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		uarthub_usb_rx_pin_ctrl_mt6991(uartip_base_map_mt6991[i], 0);

	mdelay(2);

	/* 0x4c = 0x3,  rx/tx channel dma enable */
	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		UARTHUB_REG_WRITE(DMA_EN_ADDR(uartip_base_map_mt6991[i]), 0x3);

	/* 0x08 = 0xC7, fifo control register */
	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		UARTHUB_REG_WRITE(RXTRI_AD_ADDR(uartip_base_map_mt6991[i]), 0x19);
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[i]), 0xC7);
	}

	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_init_trx_timeout_mt6991(void)
{
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev1_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev2_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev0_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev1_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev2_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev_timeout_time(TIMEOUT_ADDR, 0xF);
	return 0;
}
#endif

#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
int uarthub_init_debug_monitor_mt6991(void)
{
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);
	return 0;
}
#endif

int uarthub_init_remap_reg_mt6991(void)
{
	gpio_base_remap_addr_mt6991 = ioremap(GPIO_BASE_ADDR, 0x1000);
	pericfg_ao_remap_addr_mt6991 = ioremap(PERICFG_AO_BASE_ADDR, 0x1000);
	topckgen_base_remap_addr_mt6991 = ioremap(TOPCKGEN_BASE_ADDR, 0x1000);
	uartip_base_map_mt6991[uartip_id_ap] = ioremap(UARTHUB_DEV_0_BASE_ADDR, 0x100);
	uartip_base_map_mt6991[uartip_id_md] = ioremap(UARTHUB_DEV_1_BASE_ADDR, 0x100);
	uartip_base_map_mt6991[uartip_id_adsp] = ioremap(UARTHUB_DEV_2_BASE_ADDR, 0x100);
	uartip_base_map_mt6991[uartip_id_cmm] = ioremap(UARTHUB_CMM_BASE_ADDR, 0x100);
	intfhub_base_remap_addr_mt6991 = ioremap(UARTHUB_INTFHUB_BASE_ADDR, 0x100);
	uarthub_wakeup_base_remap_addr_mt6991 = ioremap(UARTHUB_WAKEUP_BASE_ADDR, 0x100);
	apuart_base_map_mt6991[1] = ioremap(UART1_BASE_ADDR, 0x100);
	apuart_base_map_mt6991[2] = ioremap(UART2_BASE_ADDR, 0x100);
	apuart_base_map_mt6991[3] = ioremap(UART3_BASE_ADDR, 0x100);
	apdma_uart_tx_int_remap_addr_mt6991 = ioremap(AP_DMA_UART_TX_INT_FLAG_ADDR, 0x100);
#if !(UARTHUB_SUPPORT_FPGA)
	spm_remap_addr_mt6991 = ioremap(SPM_BASE_ADDR, 0x1000);
#endif
	apmixedsys_remap_addr_mt6991 = ioremap(APMIXEDSYS_BASE_ADDR, 0x500);
	iocfg_tm3_remap_addr_mt6991 = ioremap(IOCFG_TM3_BASE_ADDR, 0x1000);
	sys_sram_remap_addr_mt6991 = ioremap(SYS_SRAM_BASE_ADDR, 0x200);

	INTFHUB_BASE_MT6991 = (unsigned long) intfhub_base_remap_addr_mt6991;
	UARTHUB_WAKEUP_BASE_MT6991 = (unsigned long) uarthub_wakeup_base_remap_addr_mt6991;

	return 0;
}

int uarthub_deinit_unmap_reg_mt6991(void)
{
	int i = 0;
#if UARTHUB_SUPPORT_FPGA
	uarthub_core_close();
#endif

	if (gpio_base_remap_addr_mt6991) {
		iounmap(gpio_base_remap_addr_mt6991);
		gpio_base_remap_addr_mt6991 = NULL;
	}

	if (pericfg_ao_remap_addr_mt6991) {
		iounmap(pericfg_ao_remap_addr_mt6991);
		pericfg_ao_remap_addr_mt6991 = NULL;
	}

	if (topckgen_base_remap_addr_mt6991) {
		iounmap(topckgen_base_remap_addr_mt6991);
		topckgen_base_remap_addr_mt6991 = NULL;
	}

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (uartip_base_map_mt6991[i]) {
			iounmap(uartip_base_map_mt6991[i]);
			uartip_base_map_mt6991[i] = NULL;
		}
	}

	for (i = 0; i < 4; i++) {
		if (apuart_base_map_mt6991[i]) {
			iounmap(apuart_base_map_mt6991[i]);
			apuart_base_map_mt6991[i] = NULL;
		}
	}

	if (apdma_uart_tx_int_remap_addr_mt6991) {
		iounmap(apdma_uart_tx_int_remap_addr_mt6991);
		apdma_uart_tx_int_remap_addr_mt6991 = NULL;
	}

	if (spm_remap_addr_mt6991) {
		iounmap(spm_remap_addr_mt6991);
		spm_remap_addr_mt6991 = NULL;
	}

	if (apmixedsys_remap_addr_mt6991) {
		iounmap(apmixedsys_remap_addr_mt6991);
		apmixedsys_remap_addr_mt6991 = NULL;
	}

	if (iocfg_tm3_remap_addr_mt6991) {
		iounmap(iocfg_tm3_remap_addr_mt6991);
		iocfg_tm3_remap_addr_mt6991 = NULL;
	}

	if (sys_sram_remap_addr_mt6991) {
		iounmap(sys_sram_remap_addr_mt6991);
		sys_sram_remap_addr_mt6991 = NULL;
	}

	return 0;
}

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_spm_sys_timer_mt6991(uint32_t *hi, uint32_t *lo)
{
	if (hi == NULL || lo == NULL) {
		pr_notice("[%s] invalid argument\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6991) {
		pr_notice("[%s] spm_remap_addr_mt6991 is NULL\n", __func__);
		return -1;
	}

	*hi = UARTHUB_REG_READ(spm_remap_addr_mt6991 + SPM_SYS_TIMER_H);
	*lo = UARTHUB_REG_READ(spm_remap_addr_mt6991 + SPM_SYS_TIMER_L);

	return 1;
}
#endif

int uarthub_get_host_status_mt6991(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = UARTHUB_REG_READ(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = UARTHUB_REG_READ(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = UARTHUB_REG_READ(DEV2_STA_ADDR);

	return state;
}

int uarthub_get_host_wakeup_status_mt6991(void)
{
	int state0 = 0, state1 = 0, state2 = 0;
	int dev0_sw_rx_mask = 0, dev1_sw_rx_mask = 0, dev2_sw_rx_mask = 0;
	int dev0_sw_tx_mask = 0, dev1_sw_tx_mask = 0, dev2_sw_tx_mask = 0;

	dev0_sw_rx_mask = REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_rx_sta);
	dev1_sw_rx_mask = REG_FLD_MASK(DEV1_STA_FLD_dev1_sw_rx_sta);
	dev2_sw_rx_mask = REG_FLD_MASK(DEV2_STA_FLD_dev2_sw_rx_sta);

	dev0_sw_tx_mask = REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_tx_sta);
	dev1_sw_tx_mask = REG_FLD_MASK(DEV1_STA_FLD_dev1_sw_tx_sta);
	dev2_sw_tx_mask = REG_FLD_MASK(DEV2_STA_FLD_dev2_sw_tx_sta);

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR,
		(dev0_sw_rx_mask | dev0_sw_tx_mask)) == dev0_sw_tx_mask) ? 1 : 0);

	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR,
		(dev1_sw_rx_mask | dev1_sw_tx_mask)) == dev1_sw_tx_mask) ? 1 : 0);

	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR,
		(dev2_sw_rx_mask | dev2_sw_tx_mask)) == dev2_sw_tx_mask) ? 1 : 0);

	return (state0 | (state1 << 1) | (state2 << 2));
}

int uarthub_get_host_set_fw_own_status_mt6991(void)
{
	int state0 = 0, state1 = 0, state2 = 0;
	int dev0_sw_rx_mask = 0, dev1_sw_rx_mask = 0, dev2_sw_rx_mask = 0;
	int dev0_sw_tx_mask = 0, dev1_sw_tx_mask = 0, dev2_sw_tx_mask = 0;

	dev0_sw_rx_mask = REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_rx_sta);
	dev1_sw_rx_mask = REG_FLD_MASK(DEV1_STA_FLD_dev1_sw_rx_sta);
	dev2_sw_rx_mask = REG_FLD_MASK(DEV2_STA_FLD_dev2_sw_rx_sta);

	dev0_sw_tx_mask = REG_FLD_MASK(DEV0_STA_FLD_dev0_sw_tx_sta);
	dev1_sw_tx_mask = REG_FLD_MASK(DEV1_STA_FLD_dev1_sw_tx_sta);
	dev2_sw_tx_mask = REG_FLD_MASK(DEV2_STA_FLD_dev2_sw_tx_sta);

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR,
		(dev0_sw_rx_mask | dev0_sw_tx_mask)) == dev0_sw_rx_mask) ? 1 : 0);

	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR,
		(dev1_sw_rx_mask | dev1_sw_tx_mask)) == dev1_sw_rx_mask) ? 1 : 0);

	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR,
		(dev2_sw_rx_mask | dev2_sw_tx_mask)) == dev2_sw_rx_mask) ? 1 : 0);

	return (state0 | (state1 << 1) | (state2 << 2));
}

int uarthub_is_host_trx_idle_mt6991(int dev_index, enum uarthub_trx_type trx)
{
	int state = -1;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			state = DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR);
		} else if (trx == TX) {
			state = DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR);
		} else {
			state = (DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR) << 1) |
				DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR);
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			state = DEV1_STA_GET_dev1_sw_rx_sta(DEV1_STA_ADDR);
		} else if (trx == TX) {
			state = DEV1_STA_GET_dev1_sw_tx_sta(DEV1_STA_ADDR);
		} else {
			state = (DEV1_STA_GET_dev1_sw_tx_sta(DEV1_STA_ADDR) << 1) |
				DEV1_STA_GET_dev1_sw_rx_sta(DEV1_STA_ADDR);
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			state = DEV2_STA_GET_dev2_sw_rx_sta(DEV2_STA_ADDR);
		} else if (trx == TX) {
			state = DEV2_STA_GET_dev2_sw_tx_sta(DEV2_STA_ADDR);
		} else {
			state = (DEV2_STA_GET_dev2_sw_tx_sta(DEV2_STA_ADDR) << 1) |
				DEV2_STA_GET_dev2_sw_rx_sta(DEV2_STA_ADDR);
		}
	}

	return state;
}

int uarthub_set_host_trx_request_mt6991(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	uarthub_pll_clk_on_mt6991(1, __func__);

	if (dev_index == 0) {
		if (trx == RX) {
			DEV0_STA_SET_SET_dev0_sw_rx_set(DEV0_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV0_STA_SET_SET_dev0_sw_tx_set(DEV0_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR,
				(REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_rx_set) |
				REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_tx_set)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			DEV1_STA_SET_SET_dev1_sw_rx_set(DEV1_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV1_STA_SET_SET_dev1_sw_tx_set(DEV1_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_SET_ADDR,
				(REG_FLD_MASK(DEV1_STA_SET_FLD_dev1_sw_rx_set) |
				REG_FLD_MASK(DEV1_STA_SET_FLD_dev1_sw_tx_set)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			DEV2_STA_SET_SET_dev2_sw_rx_set(DEV2_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV2_STA_SET_SET_dev2_sw_tx_set(DEV2_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_SET_ADDR,
				(REG_FLD_MASK(DEV2_STA_SET_FLD_dev2_sw_rx_set) |
				REG_FLD_MASK(DEV2_STA_SET_FLD_dev2_sw_tx_set)));
		}
	}

	return 0;
}

int uarthub_clear_host_trx_request_mt6991(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_hw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_rx_data_req_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_tx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_hw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_rx_data_req_clr)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_hw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_rx_data_req_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_tx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_hw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_rx_data_req_clr)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_hw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_rx_data_req_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_tx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_hw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_rx_data_req_clr)));
		}
	}

	return 0;
}

int uarthub_get_host_byte_cnt_mt6991(int dev_index, enum uarthub_trx_type trx)
{
	int byte_cnt = -1;
	void __iomem *uarthub_dev_base = NULL;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	uarthub_dev_base = uartip_base_map_mt6991[dev_index];
	if (uarthub_dev_base != NULL) {
		if (trx == RX)
			byte_cnt = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(uarthub_dev_base);
		else if (trx == TX)
			byte_cnt = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(uarthub_dev_base);
	}

	return byte_cnt;
}

int uarthub_get_cmm_byte_cnt_mt6991(enum uarthub_trx_type trx)
{
	int byte_cnt = -1;

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return -1;
	}

	if (trx == RX)
		byte_cnt = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(
			uartip_base_map_mt6991[uartip_id_cmm]);
	else if (trx == TX)
		byte_cnt = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(
			uartip_base_map_mt6991[uartip_id_cmm]);

	return byte_cnt;
}

int uarthub_config_crc_ctrl_mt6991(int enable)
{
	CON2_SET_crc_en(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_bypass_ctrl_mt6991(int enable)
{
	CON2_SET_intfhub_bypass(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_host_fifoe_ctrl_mt6991(int dev_index, int enable)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if !(MD_CHANNEL_EN)
	if (dev_index == 1) {
		pr_notice("[%s] not support md tx data to uart_1\n", __func__);
		return 0;
	}
#endif

	UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map_mt6991[dev_index]), (0xC0 | enable));
	return 0;
}

int uarthub_get_rx_error_crc_info_mt6991(int dev_index, int *p_crc_error_data, int *p_crc_result)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_data(
				DEV0_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_result(
				DEV0_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 1) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_data(
				DEV1_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_result(
				DEV1_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 2) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_data(
				DEV2_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_result(
				DEV2_RX_ERR_CRC_STA_ADDR);
	}

	return 0;
}

int uarthub_get_trx_timeout_info_mt6991(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV0_PKT_CNT_GET_dev0_rx_timeout_cnt(DEV0_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV0_PKT_CNT_GET_dev0_rx_pkt_cnt(DEV0_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV0_PKT_CNT_GET_dev0_tx_timeout_cnt(DEV0_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV0_PKT_CNT_GET_dev0_tx_pkt_cnt(DEV0_PKT_CNT_ADDR);
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV1_PKT_CNT_GET_dev1_rx_timeout_cnt(DEV1_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV1_PKT_CNT_GET_dev1_rx_pkt_cnt(DEV1_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV1_PKT_CNT_GET_dev1_tx_timeout_cnt(DEV1_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV1_PKT_CNT_GET_dev1_tx_pkt_cnt(DEV1_PKT_CNT_ADDR);
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV2_PKT_CNT_GET_dev2_rx_timeout_cnt(DEV2_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV2_PKT_CNT_GET_dev2_rx_pkt_cnt(DEV2_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV2_PKT_CNT_GET_dev2_tx_timeout_cnt(DEV2_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV2_PKT_CNT_GET_dev2_tx_pkt_cnt(DEV2_PKT_CNT_ADDR);
		}
	}

	return 0;
}

int uarthub_get_irq_err_type_mt6991(void)
{
	return UARTHUB_REG_READ_BIT(DEV0_IRQ_STA_ADDR, BIT_0x7FFF_FFFF);
}

int uarthub_inband_enable_ctrl_mt6991(int enable)
{
	int mask = 0;

	if (enable == 1) {
		mask = REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_EN) |
			REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_IRQ_EN) |
			REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_IRQ_CLR) |
			REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_STA_CLR);

		UARTHUB_REG_WRITE_MASK(
			INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), mask, mask);

		uarthub_inband_set_esc_char_mt6991(0xDC);
	} else {
		mask = REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_EN) |
			REG_FLD_MASK(INB_IRQ_CTL_FLD_INB_IRQ_EN);

		UARTHUB_REG_WRITE_MASK(
			INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0, mask);
	}

	return 0;
}

int uarthub_inband_irq_mask_ctrl_mt6991(int mask)
{
	INB_IRQ_CTL_SET_INB_IRQ_EN(
		INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), ((mask == 1) ? 0x0 : 0x1));
	return 0;
}

int uarthub_inband_irq_clear_ctrl_mt6991(void)
{
	INB_IRQ_CTL_SET_INB_IRQ_CLR(INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0x1);
	return 0;
}

int uarthub_inband_irq_get_sta_mt6991(void)
{
	int state_inband_irq = 1;
	int state_uarthub2ap_irq = 1;

	state_inband_irq = INB_IRQ_CTL_GET_INB_IRQ_IND(INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]));
	state_uarthub2ap_irq = DEV0_IRQ_STA_GET_uarthub2ap_irq_b(DEV0_IRQ_STA_ADDR);

	return ((state_inband_irq == 0x0 && state_uarthub2ap_irq == 0x0) ? 0x1 : 0x0);
}

unsigned char uarthub_inband_get_esc_sta_mt6991(void)
{
	return INB_STA_GET_INB_STA(INB_STA_ADDR(uartip_base_map_mt6991[uartip_id_cmm]));
}

int uarthub_inband_clear_esc_sta_mt6991(void)
{
	INB_IRQ_CTL_SET_INB_STA_CLR(INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0x1);
	return 0;
}

int uarthub_inband_set_esc_char_mt6991(unsigned char esc_char)
{
	INB_ESC_CHAR_SET_INB_ESC_CHAR(INB_ESC_CHAR_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), esc_char);
	return 0;
}

int uarthub_inband_set_esc_sta_mt6991(unsigned char esc_sta)
{
	INB_STA_CHAR_SET_INB_STA_CHAR(INB_STA_CHAR_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), esc_sta);
	return 0;
}

int uarthub_inband_is_tx_complete_mt6991(void)
{
	return INB_IRQ_CTL_GET_INB_TX_COMP(INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]));
}

int uarthub_inband_trigger_ctrl_mt6991(void)
{
	INB_IRQ_CTL_SET_INB_TRIG(INB_IRQ_CTL_ADDR(uartip_base_map_mt6991[uartip_id_cmm]), 0x1);
	return 0;
}

int uarthub_inband_trigger_with_esc_sta_mt6991(unsigned char esc_sta)
{
	int retry = 0;
	int val = 0;

	uarthub_inband_set_esc_sta_mt6991(esc_sta);
	uarthub_inband_trigger_ctrl_mt6991();

	retry = 200;
	while (retry-- > 0) {
		val = uarthub_inband_is_tx_complete_mt6991();
		if (val == 1) {
			pr_info("[%s] inband_is_tx_complete pass, esc_sta=[0x%x], retry=[%d]\n",
				__func__, esc_sta, retry);
			break;
		}
		udelay(20);
	}

	if (val == 0) {
		pr_notice("[%s] inband_is_tx_complete fail, esc_sta=[0x%x], retry=[%d]\n",
			__func__, esc_sta, retry);
		uarthub_core_debug_info("inband_trigger_fail");
	}

	return (val == 0) ? -1 : 0;
}

int uarthub_get_bt_sleep_flow_hw_mech_en_mt6991(void)
{
	return STA0_GET_bt_sleep_flow_hw_mechanism_en(STA0_ADDR);
}

int uarthub_set_bt_sleep_flow_hw_mech_en_mt6991(int enable)
{
	STA0_SET_bt_sleep_flow_hw_mechanism_en(STA0_ADDR, ((enable == 0) ? 0x0 : 0x1));
	return 0;
}

int uarthub_get_host_awake_sta_mt6991(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_STA_GET_dev0_host_awake_sta(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_STA_GET_dev1_host_awake_sta(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = DEV2_STA_GET_dev2_host_awake_sta(DEV2_STA_ADDR);

	return state;
}

int uarthub_host_awake_sta_ctrl_mt6991(int dev_index, int set, const char *tag)
{
#define DUMP_HOST_AWAKE_STA_DEBUG_INFO 1

#if DUMP_HOST_AWAKE_STA_DEBUG_INFO
	int dev_host_awake_sta[2] = {0};
	int dev_host_awake_sent_sta[2] = {0};
	int cmm_bt_awake_sta[2] = {0};
	int feedback_host_awake_tx_done[2] = {0};
	int dev0_sta[2] = {0};
	int dev1_sta[2] = {0};
	int dev2_sta[2] = {0};
	int retry = 0;
	int sent_sta = 0;
	unsigned char result[128];
	int len = 0;
	int ret = 0;
	int check_irq = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int check_data_mode_sel = 0;
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
#endif

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n",
			((tag == NULL) ? __func__ : tag), dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if DUMP_HOST_AWAKE_STA_DEBUG_INFO
	IRQ_CLR_SET_feedback_host_awake_tx_done_clr(IRQ_CLR_ADDR, 0x1);
	dev_host_awake_sta[0] = uarthub_get_host_awake_sta_mt6991(dev_index);
	dev_host_awake_sent_sta[0] = STA0_GET_dev_host_awake_sta(STA0_ADDR);
	sent_sta = ((dev_host_awake_sent_sta[0] & (0x1 << dev_index)) >> dev_index);
	if (sent_sta == set)
		check_irq = 1;
	cmm_bt_awake_sta[0] = uarthub_get_cmm_bt_awake_sta_mt6991();
	feedback_host_awake_tx_done[0] = IRQ_STA_GET_feedback_host_awake_tx_done(IRQ_STA_ADDR);
	dev0_sta[0] = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta[0] = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta[0] = UARTHUB_REG_READ(DEV2_STA_ADDR);

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);
	if (apuart_base_map_mt6991[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	if ((uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) &&
			(debug_monitor_sel == 0x1)) {
		tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		check_data_mode_sel = DEBUG_MODE_CRTL_GET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR);
	}
#endif

	if (set == 1) {
		if (dev_index == 0)
			DEV0_STA_SET_SET_dev0_host_awake_set(DEV0_STA_SET_ADDR, 0x1);
		else if (dev_index == 1)
			DEV1_STA_SET_SET_dev1_host_awake_set(DEV1_STA_SET_ADDR, 0x1);
		else if (dev_index == 2)
			DEV2_STA_SET_SET_dev2_host_awake_set(DEV2_STA_SET_ADDR, 0x1);
	} else {
		if (dev_index == 0)
			DEV0_STA_CLR_SET_dev0_host_awake_clr(DEV0_STA_CLR_ADDR, 0x1);
		else if (dev_index == 1)
			DEV1_STA_CLR_SET_dev1_host_awake_clr(DEV1_STA_CLR_ADDR, 0x1);
		else if (dev_index == 2)
			DEV2_STA_CLR_SET_dev2_host_awake_clr(DEV2_STA_CLR_ADDR, 0x1);
	}

#if DUMP_HOST_AWAKE_STA_DEBUG_INFO
	cmm_bt_awake_sta[1] = uarthub_get_cmm_bt_awake_sta_mt6991();
	dev_host_awake_sta[1] = uarthub_get_host_awake_sta_mt6991(dev_index);
	feedback_host_awake_tx_done[1] = IRQ_STA_GET_feedback_host_awake_tx_done(IRQ_STA_ADDR);

	/* should trigger UARTHUB HW sendt host awake sta to FW */
	len = 0;
	ret = snprintf(result + len, 128 - len, "%s", "null");
	if (ret > 0)
		len += ret;

	if (cmm_bt_awake_sta[0] == 1 && dev_host_awake_sta[0] != set) {
		retry = 20;
		udelay(11);
		while (retry-- > 0) {
			if (check_irq == 1)
				feedback_host_awake_tx_done[1] = IRQ_STA_GET_feedback_host_awake_tx_done(IRQ_STA_ADDR);
			dev_host_awake_sent_sta[1] = STA0_GET_dev_host_awake_sta(STA0_ADDR);
			sent_sta = ((dev_host_awake_sent_sta[1] & (0x1 << dev_index)) >> dev_index);
			if (((check_irq == 0) || (feedback_host_awake_tx_done[1] == 1)) && (sent_sta == set))
				return 0;
			usleep_range(50, 60);
		}

		if (((check_irq == 1) && (feedback_host_awake_tx_done[1] == 0)) || (sent_sta != set)) {
			len = 0;
			ret = snprintf(result + len, 128 - len,
				"%s_%d", "FAIL", retry);
			if (ret > 0)
				len += ret;
		}
	} else {
		feedback_host_awake_tx_done[1] = IRQ_STA_GET_feedback_host_awake_tx_done(IRQ_STA_ADDR);
		dev_host_awake_sent_sta[1] = STA0_GET_dev_host_awake_sta(STA0_ADDR);

		len = 0;
		ret = snprintf(result + len, 128 - len,
			"%s", "CANCEL");
		if (ret > 0)
			len += ret;
	}

	dev0_sta[1] = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta[1] = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta[1] = UARTHUB_REG_READ(DEV2_STA_ADDR);

	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s][%d] hostAwk=[%d/%d],hostAwkSend=[0x%x/0x%x],cmmbtAwk=[%d/%d],irqTXdone(%d)=[%d/%d],IDEVx_STA=[0x%x/0x%x-0x%x/0x%x-0x%x/0x%x]",
		((tag == NULL) ? __func__ : tag), result, dev_index, dev_host_awake_sta[0], dev_host_awake_sta[1],
		dev_host_awake_sent_sta[0], dev_host_awake_sent_sta[1],
		cmm_bt_awake_sta[0], cmm_bt_awake_sta[1], check_irq,
		feedback_host_awake_tx_done[0], feedback_host_awake_tx_done[1],
		dev0_sta[0], dev0_sta[1],
		dev1_sta[0], dev1_sta[1],
		dev2_sta[0], dev2_sta[1]);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",bcnt_B=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);
	if (apuart_base_map_mt6991[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",E=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");

	len = uarthub_record_check_data_mode_sta_to_buffer(
		dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
		tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel, "dataMon_B");

	if ((uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) &&
			(debug_monitor_sel == 0x1)) {
		tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		check_data_mode_sel = DEBUG_MODE_CRTL_GET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR);

		len = uarthub_record_check_data_mode_sta_to_buffer(
			dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
			tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel, "E");
	}

	pr_info("%s\n", dmp_info_buf);
#endif

	return 0;
}

int uarthub_set_host_awake_sta_mt6991(int dev_index)
{
	return uarthub_host_awake_sta_ctrl_mt6991(dev_index, 1, "SET_HOST_AWK");
}

int uarthub_clear_host_awake_sta_mt6991(int dev_index)
{
	return uarthub_host_awake_sta_ctrl_mt6991(dev_index, 0, "CLR_HOST_AWK");
}

int uarthub_get_host_bt_awake_sta_mt6991(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_STA_GET_dev0_bt_awake_sta(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_STA_GET_dev1_bt_awake_sta(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = DEV2_STA_GET_dev2_bt_awake_sta(DEV2_STA_ADDR);

	return state;
}

int uarthub_get_cmm_bt_awake_sta_mt6991(void)
{
	return STA0_GET_cmm_bt_awake_sta(STA0_ADDR);
}

int uarthub_get_bt_awake_sta_mt6991(void)
{
	return STA0_GET_bt_awake_sta(STA0_ADDR);
}

int uarthub_get_bt_on_count_mt6991(void)
{
	return UARTHUB_REG_READ_BIT(DEV0_RESERVE_ADDR, 0xFF);
}

int uarthub_bt_on_count_inc_mt6991(void)
{
	int count = 0;

	count = uarthub_get_bt_on_count_mt6991();
	if (count == 0xFF)
		count = 0;
	else
		count++;

	UARTHUB_REG_WRITE_MASK(DEV0_RESERVE_ADDR, count, 0xFF);
	return count;
}
