/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MT6991_DEBUG_KEYWORD_H
#define MT6991_DEBUG_KEYWORD_H

#include "common_def_id.h"
#include "mt6991_debug.h"

typedef struct _BOU_MOD_ID_NAME {
	char *name;
} BOU_MOD_ID_NAME, *P_BOU_MOD_ID_NAME;

BOU_MOD_ID_NAME lst_mod_id_name[] = {
	[mod_undefined] = {"undef_module"},
	[mod_bt_drv]    = {"bt_drv"},
	[mod_tty]       = {"tty"},
	[mod_ap_dma]    = {"ap_dma"},
	[mod_ap_uart]   = {"ap_uart"},
	[mod_adsp_host] = {"adsp_host"},
	[mod_adsp_uart] = {"adsp_uart"},
	[mod_uarthub]   = {"uarthub"},
	[mod_bt_uart]   = {"bt_uart"},
	[mod_bt_mcu]    = {"bt_mcu"},
	[mod_bt_fw]     = {"bt_fw"},
	[mod_md]        = {"md"},
};

typedef struct _BOU_UARTHUB_ERROR_LOG_INFO {
	char *name;
	int mod_id;
	int next_mod_id;
} BOU_UARTHUB_ERROR_LOG_INFO, *P_BOU_UARTHUB_ERROR_LOG_INFO;

BOU_UARTHUB_ERROR_LOG_INFO lst_uarthub_log_info[] = {
	[log_undefined]                         = {"LOG_UNDEF_ERROR",                   mod_uarthub, mod_undefined},
	[log_uart0_det_xoff]                    = {"UART0_DET_XOFF",                    mod_uarthub, mod_ap_uart},
	[log_uart1_det_xoff]                    = {"UART1_DET_XOFF",                    mod_uarthub, mod_md},
	[log_uart2_det_xoff]                    = {"UART2_DET_XOFF",                    mod_uarthub, mod_adsp_uart},
	[log_uartcmm_det_xoff]                  = {"UARTCMM_DET_XOFF",                  mod_uarthub, mod_bt_uart},
	[log_apuart_det_xoff]                   = {"APUART_DET_XOFF",                   mod_uarthub, mod_uarthub},
	[log_uart0_send_xoff]                   = {"UART0_SEND_XOFF",                   mod_uarthub, mod_uarthub},
	[log_uart1_send_xoff]                   = {"UART1_SEND_XOFF",                   mod_uarthub, mod_uarthub},
	[log_uart2_send_xoff]                   = {"UART2_SEND_XOFF",                   mod_uarthub, mod_uarthub},
	[log_uartcmm_send_xoff]                 = {"UARTCMM_SEND_XOFF",                 mod_uarthub, mod_uarthub},
	[log_apuart_send_xoff]                  = {"APUART_SEND_XOFF",                  mod_uarthub, mod_ap_uart},
	[log_uart0_keep_sending_xoff]           = {"UART0_KEEP_SENDING_XOFF",           mod_uarthub, mod_uarthub},
	[log_uart1_keep_sending_xoff]           = {"UART1_KEEP_SENDING_XOFF",           mod_uarthub, mod_uarthub},
	[log_uart2_keep_sending_xoff]           = {"UART2_KEEP_SENDING_XOFF",           mod_uarthub, mod_uarthub},
	[log_uartcmm_keep_sending_xoff]         = {"UARTCMM_KEEP_SENDING_XOFF",         mod_uarthub, mod_uarthub},
	[log_apuart_keep_sending_xoff]          = {"APUART_KEEP_SENDING_XOFF",          mod_uarthub, mod_ap_uart},
	[log_uart0_keep_sending_xon]            = {"UART0_KEEP_SENDING_XON",            mod_uarthub, mod_uarthub},
	[log_uart1_keep_sending_xon]            = {"UART1_KEEP_SENDING_XON",            mod_uarthub, mod_uarthub},
	[log_uart2_keep_sending_xon]            = {"UART2_KEEP_SENDING_XON",            mod_uarthub, mod_uarthub},
	[log_uartcmm_keep_sending_xon]          = {"UARTCMM_KEEP_SENDING_XON",          mod_uarthub, mod_uarthub},
	[log_apuart_keep_sending_xon]           = {"APUART_KEEP_SENDING_XON",           mod_uarthub, mod_ap_uart},
	[log_uart0_frame_error]                 = {"UART0_FRAME_ERROR",                 mod_uarthub, mod_ap_uart},
	[log_uart1_frame_error]                 = {"UART1_FRAME_ERROR",                 mod_uarthub, mod_md},
	[log_uart2_frame_error]                 = {"UART2_FRAME_ERROR",                 mod_uarthub, mod_adsp_uart},
	[log_uartcmm_frame_error]               = {"UARTCMM_FRAME_ERROR",               mod_uarthub, mod_bt_uart},
	[log_apuart_frame_error]                = {"APUART_FRAME_ERROR",                mod_uarthub, mod_uarthub},
	[log_ap_tx_tmo_tx_pkt_cnt_err]          = {"AP_TX_TMO_TX_PKT_CNT_ERR",          mod_uarthub, mod_ap_uart},
	[log_ap_tx_tmo_rx_pkt_cnt_err]          = {"AP_TX_TMO_RX_PKT_CNT_ERR",          mod_uarthub, mod_bt_uart},
	[log_ap_tx_tmo_apuart_tx_byte_cnt_err]  = {"AP_TX_TMO_APUART_TX_BYTE_CNT_ERR",  mod_uarthub, mod_ap_uart},
	[log_ap_tx_tmo_uartcmm_tx_byte_cnt_err] = {"AP_TX_TMO_UARTCMM_TX_BYTE_CNT_ERR", mod_uarthub, mod_uarthub},
	[log_ap_tx_tmo_uartcmm_rx_byte_cnt_err] = {"AP_TX_TMO_UARTCMM_RX_BYTE_CNT_ERR", mod_uarthub, mod_bt_uart},
	[log_ap_tx_tmo_apuart_rx_byte_cnt_err]  = {"AP_TX_TMO_APUART_RX_BYTE_CNT_ERR",  mod_uarthub, mod_uarthub},
	[log_ap_tx_tmo_btuart_tx_byte_cnt_err]  = {"AP_TX_TMO_BTUART_TX_BYTE_CNT_ERR",  mod_uarthub, mod_bt_uart},
	[log_ap_tx_tmo_apdma_err]               = {"AP_TX_TMO_APDMA_ERR",               mod_uarthub, mod_ap_dma},
	[log_ap_tx_tmo_bypass_err]              = {"AP_TX_TMO_BYPASS_ERR",              mod_uarthub, mod_bt_drv},
	[log_uart0_rx_woffset_not_empty]        = {"UART0_RX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_uarthub},
	[log_uart1_rx_woffset_not_empty]        = {"UART1_RX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_uarthub},
	[log_uart2_rx_woffset_not_empty]        = {"UART2_RX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_uarthub},
	[log_uartcmm_rx_woffset_not_empty]      = {"UARTCMM_RX_WOFFSET_NOT_EMPTY",      mod_uarthub, mod_uarthub},
	[log_apuart_rx_woffset_not_empty]       = {"APUART_RX_WOFFSET_NOT_EMPTY",       mod_uarthub, mod_ap_uart},
	[log_uart0_tx_woffset_not_empty]        = {"UART0_TX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_ap_uart},
	[log_uart1_tx_woffset_not_empty]        = {"UART1_TX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_md},
	[log_uart2_tx_woffset_not_empty]        = {"UART2_TX_WOFFSET_NOT_EMPTY",        mod_uarthub, mod_adsp_uart},
	[log_uartcmm_tx_woffset_not_empty]      = {"UARTCMM_TX_WOFFSET_NOT_EMPTY",      mod_uarthub, mod_uarthub},
	[log_apuart_tx_woffset_not_empty]       = {"APUART_TX_WOFFSET_NOT_EMPTY",       mod_uarthub, mod_uarthub},
	[log_gpio_rx_mode_err]                  = {"GPIO_RX_MODE_ERR",                  mod_uarthub, mod_bt_drv},
	[log_gpio_tx_mode_err]                  = {"GPIO_TX_MODE_ERR",                  mod_uarthub, mod_bt_drv},
	[log_gpio_bt_rst_mode_err]              = {"GPIO_BT_RST_MODE_ERR",              mod_uarthub, mod_bt_drv},
	[log_gpio_bt_rst_dir_err]               = {"GPIO_BT_RST_DIR_ERR",               mod_uarthub, mod_bt_drv},
	[log_gpio_bt_rst_out_err]               = {"GPIO_BT_RST_OUT_ERR",               mod_uarthub, mod_bt_drv},
	[log_dev0_crc_err]                      = {"DEV0_CRC_ERR",                      mod_uarthub, mod_bt_uart},
	[log_dev1_crc_err]                      = {"DEV1_CRC_ERR",                      mod_uarthub, mod_bt_uart},
	[log_dev2_crc_err]                      = {"DEV2_CRC_ERR",                      mod_uarthub, mod_bt_uart},
	[log_dev0_tx_timeout_err]               = {"DEV0_TX_TIMEOUT_ERR",               mod_uarthub, mod_undefined},
	[log_dev1_tx_timeout_err]               = {"DEV1_TX_TIMEOUT_ERR",               mod_uarthub, mod_md},
	[log_dev2_tx_timeout_err]               = {"DEV2_TX_TIMEOUT_ERR",               mod_uarthub, mod_undefined},
	[log_dev0_tx_pkt_type_err]              = {"DEV0_TX_PKT_TYPE_ERR",              mod_uarthub, mod_ap_uart},
	[log_dev1_tx_pkt_type_err]              = {"DEV1_TX_PKT_TYPE_ERR",              mod_uarthub, mod_md},
	[log_dev2_tx_pkt_type_err]              = {"DEV2_TX_PKT_TYPE_ERR",              mod_uarthub, mod_adsp_uart},
	[log_dev0_rx_timeout_err]               = {"DEV0_RX_TIMEOUT_ERR",               mod_uarthub, mod_bt_drv},
	[log_dev1_rx_timeout_err]               = {"DEV1_RX_TIMEOUT_ERR",               mod_uarthub, mod_bt_uart},
	[log_dev2_rx_timeout_err]               = {"DEV2_RX_TIMEOUT_ERR",               mod_uarthub, mod_bt_uart},
	[log_rx_pkt_type_err]                   = {"RX_PKT_TYPE_ERR",                   mod_uarthub, mod_bt_uart},
	[log_dev_rx_err]                        = {"DEV_RX_ERR",                        mod_uarthub, mod_bt_uart},
	[log_dev0_tx_err]                       = {"DEV0_TX_ERR",                       mod_uarthub, mod_uarthub},
	[log_dev1_tx_err]                       = {"DEV1_TX_ERR",                       mod_uarthub, mod_md},
	[log_dev2_tx_err]                       = {"DEV2_TX_ERR",                       mod_uarthub, mod_adsp_host}
};

#define BT_OVER_UAER_DEBUG_LOG_ERROR_KEYWORD "BT_OVER_UART_LOG_ERROR"

#define BT_OVER_UAER_DUMP_LOG(_err, _log, _tag, _detail) \
	do {\
		int _mod_id, _next_mod_id;\
		_mod_id = lst_uarthub_log_info[_log].mod_id;\
		_next_mod_id = lst_uarthub_log_info[_log].next_mod_id;\
		if (_mod_id < 0 || _mod_id >= mod_max)\
			break;\
		if (_next_mod_id < 0 || _next_mod_id >= mod_max)\
			break;\
		pr_notice("%s%s%s%s:%s/%s/%s/%s/%s\n", \
			((_tag == NULL) ? "" : "["), \
			((_tag == NULL) ? "" : _tag), \
			((_tag == NULL) ? "" : "] "), \
			BT_OVER_UAER_DEBUG_LOG_ERROR_KEYWORD, \
			((_err == 0) ? "warning" : "error"), \
			lst_uarthub_log_info[_log].name, \
			lst_mod_id_name[_mod_id].name, \
			lst_mod_id_name[_next_mod_id].name, \
			((_detail == NULL) ? "null" : _detail));\
	} while (0)

#define UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _s, _err) \
	do {\
		if (_v0 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart0_rx_woffset_not_empty, __func__, NULL);\
		if (_v1 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart1_rx_woffset_not_empty, __func__, NULL);\
		if (_v2 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart2_rx_woffset_not_empty, __func__, NULL);\
		if (_v3 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uartcmm_rx_woffset_not_empty, __func__, NULL);\
		if ((apuart_base_map_mt6991[3] != NULL) && (_v4 > _s))\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_apuart_rx_woffset_not_empty, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _s, _err) \
	do {\
		if (_v0 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart0_tx_woffset_not_empty, __func__, NULL);\
		if (_v1 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart1_tx_woffset_not_empty, __func__, NULL);\
		if (_v2 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart2_tx_woffset_not_empty, __func__, NULL);\
		if (_v3 > _s)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uartcmm_tx_woffset_not_empty, __func__, NULL);\
		if ((apuart_base_map_mt6991[3] != NULL) && (_v4 > _s))\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_apuart_tx_woffset_not_empty, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_FRAME_ERROR_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (((_v0 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart0_frame_error, __func__, NULL);\
		if (((_v1 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(1, \
				log_uart1_frame_error, __func__, NULL);\
		if (((_v2 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart2_frame_error, __func__, NULL);\
		if (((_v3& 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uartcmm_frame_error, __func__, NULL);\
		if ((apuart_base_map_mt6991[3] != NULL) && (((_v4 & 0x8) >> 3) == 1))\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_apuart_frame_error, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart0_det_xoff, __func__, NULL);\
		if (_v1 == 1)\
			BT_OVER_UAER_DUMP_LOG(1, \
				log_uart1_det_xoff, __func__, NULL);\
		if (_v2 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uart2_det_xoff, __func__, NULL);\
		if (_v3 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_uartcmm_det_xoff, __func__, NULL);\
		if ((apuart_base_map_mt6991[3] != NULL) && (_v4 == 1))\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_apuart_det_xoff, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 == 2 || _v0 == 4 || _v0 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, ((_v0 == 2) ? log_uart0_keep_sending_xoff : \
				((_v0 == 4) ? log_uart0_send_xoff : log_uart0_keep_sending_xon)), \
				__func__, NULL);\
		if (_v1 == 2 || _v1 == 4 || _v1 == 5)\
			BT_OVER_UAER_DUMP_LOG(1, ((_v1 == 2) ? log_uart1_keep_sending_xoff : \
				((_v1 == 4) ? log_uart1_send_xoff : log_uart1_keep_sending_xon)), \
				__func__, NULL);\
		if (_v2 == 2 || _v2 == 4 || _v2 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, ((_v2 == 2) ? log_uart2_keep_sending_xoff : \
				((_v2 == 4) ? log_uart2_send_xoff : log_uart2_keep_sending_xon)), \
				__func__, NULL);\
		if (_v3 == 2 || _v3 == 4 || _v3 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, ((_v3 == 2) ? log_uartcmm_keep_sending_xoff : \
				((_v3 == 4) ? log_uartcmm_send_xoff : log_uartcmm_keep_sending_xon)), \
				__func__, NULL);\
		if ((apuart_base_map_mt6991[3] != NULL) && (_v4 == 2 || _v4 == 4 || _v4 == 5))\
			BT_OVER_UAER_DUMP_LOG(_err, ((_v4 == 2) ? log_apuart_keep_sending_xoff : \
				((_v4 == 4) ? log_apuart_send_xoff : log_apuart_keep_sending_xon)), \
				__func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_GPIO_DEBUG_KEYWORD(_rm, _tm, _bm, _bd, _bo, _err) \
	do {\
		if (_rm.gpio_value != _rm.value)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_gpio_rx_mode_err, __func__, NULL);\
		if (_tm.gpio_value != _tm.value)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_gpio_tx_mode_err, __func__, NULL);\
		if (_bm.gpio_value != 0)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_gpio_bt_rst_mode_err, __func__, NULL);\
		if (_bd.gpio_value != 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_gpio_bt_rst_dir_err, __func__, NULL);\
		if (_bo.gpio_value != 1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_gpio_bt_rst_out_err, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYPASS_ERR_DEBUG_KEYWORD(_f, _l, _err) \
	do {\
		if (_f != _l)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_ap_tx_tmo_bypass_err, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_PKT_CNT_ERR_DEBUG_KEYWORD(_ft, _lt, _fr, _lr, _err) \
	do {\
		if (_ft == _lt)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_ap_tx_tmo_tx_pkt_cnt_err, __func__, NULL);\
		else if (_fr == _lr)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_ap_tx_tmo_rx_pkt_cnt_err, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYTE_CNT_ERR_DEBUG_KEYWORD(\
_fat, _lat, _far, _lar, _fct, _lct, _fcr, _lcr, _err) \
	do {\
		if (apuart_base_map_mt6991[3] != NULL) {\
			if (_fat == _lat)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_apuart_tx_byte_cnt_err, __func__, NULL);\
			else if (_fct == _lct)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_uartcmm_tx_byte_cnt_err, __func__, NULL);\
			else if (_fcr == _lcr)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_uartcmm_rx_byte_cnt_err, __func__, NULL);\
			else if (_far == _lar)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_apuart_rx_byte_cnt_err, __func__, NULL);\
			else\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_apdma_err, __func__, NULL);\
		} else {\
			if (_fct != _lct && _fcr == _lcr)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_uartcmm_rx_byte_cnt_err, __func__, NULL);\
		}\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYTE_CNT_ERR_DEBUG_BYPASS_KEYWORD(\
_fat, _lat, _far, _lar, _err) \
	do {\
		if (apuart_base_map_mt6991[3] != NULL) {\
			if (_fat == _lat)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_apuart_tx_byte_cnt_err, __func__, NULL);\
			else if (_far == _lar)\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_btuart_tx_byte_cnt_err, __func__, NULL);\
			else\
				BT_OVER_UAER_DUMP_LOG(_err, \
					log_ap_tx_tmo_apdma_err, __func__, NULL);\
		}\
	} while (0)

#define UARTHUB_DEBUG_PRINT_UARTHUB_IRQ_ERR_DEBUG_KEYWORD(_sta, _err) \
	do {\
		if (((_sta >> 0) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev0_crc_err, __func__, NULL);\
		if (((_sta >> 1) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev1_crc_err, __func__, NULL);\
		if (((_sta >> 2) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev2_crc_err, __func__, NULL);\
		if (((_sta >> 3) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev0_tx_timeout_err, __func__, NULL);\
		if (((_sta >> 4) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev1_tx_timeout_err, __func__, NULL);\
		if (((_sta >> 5) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev2_tx_timeout_err, __func__, NULL);\
		if (((_sta >> 6) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev0_tx_pkt_type_err, __func__, NULL);\
		if (((_sta >> 7) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev1_tx_pkt_type_err, __func__, NULL);\
		if (((_sta >> 8) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev2_tx_pkt_type_err, __func__, NULL);\
		if (((_sta >> 9) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev0_rx_timeout_err, __func__, NULL);\
		if (((_sta >> 10) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev1_rx_timeout_err, __func__, NULL);\
		if (((_sta >> 11) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev2_rx_timeout_err, __func__, NULL);\
		if (((_sta >> 12) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_rx_pkt_type_err, __func__, NULL);\
		if (((_sta >> 14) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev_rx_err, __func__, NULL);\
		if (((_sta >> 15) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(0, \
				log_dev0_tx_err, __func__, NULL);\
		if (((_sta >> 16) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev1_tx_err, __func__, NULL);\
		if (((_sta >> 17) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, \
				log_dev2_tx_err, __func__, NULL);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_LSR_REG(_v1, _str, _err) \
	do {\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _v1.dev0, _v1.dev1, _v1.dev2, _v1.cmm, _v1.ap);\
		UARTHUB_DEBUG_PRINT_FRAME_ERROR_DEBUG_KEYWORD(\
			_v1.dev0, _v1.dev1, _v1.dev2, _v1.cmm, _v1.ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_WSEND_XOFF_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = ((_v1.dev0 & 0xE0) >> 5);\
		_d1 = ((_v1.dev1 & 0xE0) >> 5);\
		_d2 = ((_v1.dev2 & 0xE0) >> 5);\
		_cmm = ((_v1.cmm & 0xE0) >> 5);\
		_ap = ((_v1.ap & 0xE0) >> 5);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_DET_XOFF_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = ((_v1.dev0 & 0x8) >> 3);\
		_d1 = ((_v1.dev1 & 0x8) >> 3);\
		_d2 = ((_v1.dev2 & 0x8) >> 3);\
		_cmm = ((_v1.cmm & 0x8) >> 3);\
		_ap = ((_v1.ap & 0x8) >> 3);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_RX_WOFFSET_REG(_v1, _s, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = (_v1.dev0 & 0x3F);\
		_d1 = (_v1.dev1 & 0x3F);\
		_d2 = (_v1.dev2 & 0x3F);\
		_cmm = (_v1.cmm & 0x3F);\
		_ap = (_v1.ap & 0x3F);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _s, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_TX_WOFFSET_REG(_v1, _s, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = (_v1.dev0 & 0x3F);\
		_d1 = (_v1.dev1 & 0x3F);\
		_d2 = (_v1.dev2 & 0x3F);\
		_cmm = (_v1.cmm & 0x3F);\
		_ap = (_v1.ap & 0x3F);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _s, _err);\
	} while (0)

#define UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYPASS_ERR(_prefix1, _cur1, _prefix2, _cur2) \
	do {\
		if (_cur1 == 0)\
			_prefix1##_bypass_mode = -1;\
		else if (_cur1 == 1)\
			_prefix1##_bypass_mode = bypass_mode;\
		if (_cur2 == 0)\
			_prefix2##_bypass_mode = -1;\
		else if (_cur2 == 1)\
			_prefix2##_bypass_mode = bypass_mode;\
	} while (0)

#define UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_PKT_CNT_ERR(_prefix1, _cur1, _prefix2, _cur2) \
	do {\
		if (_cur1 == 0) {\
			_prefix1##_cur_tx_pkt_cnt_d0 = -1;\
			_prefix1##_cur_rx_pkt_cnt_d0 = -1;\
		} else if (_cur1 == 1) {\
			_prefix1##_cur_tx_pkt_cnt_d0 = cur_tx_pkt_cnt_d0;\
			_prefix1##_cur_rx_pkt_cnt_d0 = cur_rx_pkt_cnt_d0;\
		}\
		if (_cur2 == 0) {\
			_prefix2##_cur_tx_pkt_cnt_d0 = -1;\
			_prefix2##_cur_rx_pkt_cnt_d0 = -1;\
		} else if (_cur2 == 1) {\
			_prefix2##_cur_tx_pkt_cnt_d0 = cur_tx_pkt_cnt_d0;\
			_prefix2##_cur_rx_pkt_cnt_d0 = cur_rx_pkt_cnt_d0;\
		}\
	} while (0)

#define UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYTE_CNT_ERR(_prefix1, _cur1, _prefix2, _cur2) \
	do {\
		if (_cur1 == 0) {\
			_prefix1##_ap_tx_bcnt = -1;\
			_prefix1##_ap_rx_bcnt = -1;\
			_prefix1##_cmm_tx_bcnt = -1;\
			_prefix1##_cmm_rx_bcnt = -1;\
		} else if (_cur1 == 1) {\
			_prefix1##_ap_tx_bcnt = ap_tx_bcnt;\
			_prefix1##_ap_rx_bcnt = ap_rx_bcnt;\
			_prefix1##_cmm_tx_bcnt = cmm_tx_bcnt;\
			_prefix1##_cmm_rx_bcnt = cmm_rx_bcnt;\
		}\
		if (_cur2 == 0) {\
			_prefix2##_ap_tx_bcnt = -1;\
			_prefix2##_ap_rx_bcnt = -1;\
			_prefix2##_cmm_tx_bcnt = -1;\
			_prefix2##_cmm_rx_bcnt = -1;\
		} else if (_cur2 == 1) {\
			_prefix2##_ap_tx_bcnt = ap_tx_bcnt;\
			_prefix2##_ap_rx_bcnt = ap_rx_bcnt;\
			_prefix2##_cmm_tx_bcnt = cmm_tx_bcnt;\
			_prefix2##_cmm_rx_bcnt = cmm_rx_bcnt;\
		}\
	} while (0)

#endif
