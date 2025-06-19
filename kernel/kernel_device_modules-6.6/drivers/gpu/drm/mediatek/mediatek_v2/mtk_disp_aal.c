// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y, m, n) do { } while (0)
#endif
#define MT65XX_LED_MODE_CUST_LCM (4)

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"
#include "mtk_drm_mmp.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_disp_gamma.h"
#include "mtk_dmdp_aal.h"
#include "mtk_drm_trace.h"

#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
#include "mmevent_function.h"
#define MME_AAL_BUFFER_SIZE (240 * 1024)
#endif

#undef pr_fmt
#define pr_fmt(fmt) "[disp_aal]" fmt
#define AALERR(fmt, arg...) pr_notice("[ERR]%s:" fmt, __func__, ##arg)


#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
static bool debug_flow_log;
#define AALFLOW_LOG(fmt, arg...) do { \
	MME_INFO(MME_MODULE_DISP, MME_BUFFER_INDEX_9, fmt, ##arg);      \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define AALAPI_LOG(fmt, arg...) do { \
	MME_INFO(MME_MODULE_DISP, MME_BUFFER_INDEX_9, fmt, ##arg);      \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_irq_log;
#define AALIRQ_LOG(fmt, arg...) do { \
	MME_INFO(MME_MODULE_DISP, MME_BUFFER_INDEX_9, fmt, ##arg);      \
	if (debug_irq_log) \
		pr_notice("[IRQ]%s:" fmt, __func__, ##arg); \
	} while (0)

#else

static bool debug_flow_log;
#define AALFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define AALAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_irq_log;
#define AALIRQ_LOG(fmt, arg...) do { \
	if (debug_irq_log) \
		pr_notice("[IRQ]%s:" fmt, __func__, ##arg); \
	} while (0)
#endif

#define AALDUMP_LOG(fmt, arg...) pr_notice("[AAL_DUMP]:" fmt, ##arg)

/*******************************/
/* reg definition */
/* ------------------------------------------------------------- */
/* AAL */
#define DISP_AAL_EN                             (0x000)
#define DISP_AAL_RESET                          (0x004)
#define DISP_AAL_INTEN                          (0x008)
#define AAL_IRQ_OF_END BIT(1)
#define AAL_IRQ_IF_END BIT(0)
#define DISP_AAL_INTSTA                         (0x00c)
#define DISP_AAL_STATUS                         (0x010)
#define DISP_AAL_CFG                            (0x020)
#define AAL_RELAY_MODE BIT(0)
#define FLD_RELAY_MODE			REG_FLD_MSB_LSB(0, 0)
#define FLD_AAL_ENGINE_EN		REG_FLD_MSB_LSB(1, 1)
#define FLD_AAL_HIST_EN			REG_FLD_MSB_LSB(2, 2)
#define FLD_BLK_HIST_EN			REG_FLD_MSB_LSB(5, 5)
#define FLD_AAL_8BIT_SWITCH		REG_FLD_MSB_LSB(8, 8)
#define FLD_FRAME_DONE_DELAY		REG_FLD_MSB_LSB(23, 16)
#define DISP_AAL_IN_CNT                         (0x024)
#define DISP_AAL_OUT_CNT                        (0x028)
#define DISP_AAL_CHKSUM                         (0x02c)
#define DISP_AAL_SIZE                           (0x030)
#define DISP_AAL_SHADOW_CTL                     (0x0B0)
#define DISP_AAL_DUMMY_REG                      (0x0c0)
#define DISP_AAL_SHADOW_CTRL                    (0x0f0)
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DISP_AAL_MAX_HIST_CONFIG_00             (0x204)
#define DISP_AAL_CABC_00                        (0x20c)
#define DISP_AAL_CABC_02                        (0x214)
#define DISP_AAL_CABC_04                        (0x21c)
#define DISP_AAL_STATUS_00                      (0x224)
/* 00 ~ 32: max histogram */
#define DISP_AAL_STATUS_32                      (0x2a4)
/* bit 8: dre_gain_force_en */
#define DISP_AAL_DRE_GAIN_FILTER_00             (0x354)
#define DISP_AAL_DRE_FLT_FORCE(idx) \
	(0x358 + (idx) * 4)
#define DISP_AAL_DRE_CRV_CAL_00                 (0x344)
#define DISP_AAL_DRE_MAPPING_00                 (0x3b4)
#define GKI_DISP_AAL_DRE_MAPPING_00             (0x3b0)
#define DISP_AAL_CABC_GAINLMT_TBL_00            (0x410)
#define GKI_DISP_AAL_CABC_GAINLMT_TBL_00        (0x40c)
#define DISP_AAL_CABC_GAINLMT_TBL(addr, idx) \
	(addr + (idx) * 4)
#define DISP_AAL_DBG_CFG_MAIN                   (0x45c)
#define MAX_DRE_FLT_NUM                         (16)
#define DRE_FLT_NUM                             (12)
#define DISP_AAL_DRE_FLT_FORCE_11               (0x44C)
#define DISP_AAL_DRE_FLT_FORCE_12               (0x450)
#define DISP_AAL_DUAL_PIPE_INFO_00              (0x4d0)
#define DISP_AAL_DUAL_PIPE_INFO_01              (0x4d4)
#define DISP_AAL_OUTPUT_SIZE                    (0x4d8)
#define DISP_AAL_OUTPUT_OFFSET                  (0x4dc)
#define DISP_Y_HISTOGRAM_00                     (0x504)
#define DISP_CMB_MAIN_0                         (0x604)
#define NEW_CBOOST_EN	BIT(13)

/* common */
#define DISP_AAL_DRE_BLOCK_INFO_00              (0x468)
#define DISP_AAL_DRE_BLOCK_INFO_01              (0x46c)
#define DISP_AAL_DRE_BLOCK_INFO_02              (0x470)
#define DISP_AAL_DRE_BLOCK_INFO_03              (0x474)
#define DISP_AAL_DRE_BLOCK_INFO_04              (0x478)
#define DISP_AAL_DRE_CHROMA_HIST_00             (0x480)
#define DISP_AAL_DRE_CHROMA_HIST_01             (0x484)
#define DISP_AAL_DRE_ALPHA_BLEND_00             (0x488)
#define DISP_AAL_DRE_BITPLUS_00                 (0x48c)
#define DISP_AAL_DRE_BITPLUS_01                 (0x490)
#define DISP_AAL_DRE_BITPLUS_02                 (0x494)
#define DISP_AAL_DRE_BITPLUS_03                 (0x498)
#define DISP_AAL_DRE_BITPLUS_04                 (0x49c)
#define DISP_AAL_DRE_BLOCK_INFO_05              (0x4b4)
#define DISP_AAL_DRE_BLOCK_INFO_06              (0x4b8)

/* DRE 3.0 */
#define DMDP_AAL_CFG_MAIN                       (0x200)
#define DMDP_AAL_SRAM_CFG                       (0x0c4)
#define REG_HIST_SRAM_PP_HALT     REG_FLD_MSB_LSB(1, 1)
#define REG_FORCE_HIST_SRAM_EN    REG_FLD_MSB_LSB(4, 4)
#define REG_FORCE_HIST_SRAM_APB   REG_FLD_MSB_LSB(5, 5)
#define REG_FORCE_HIST_SRAM_INT   REG_FLD_MSB_LSB(6, 6)
#define REG_CURVE_SRAM_PP_HALT    REG_FLD_MSB_LSB(7, 7)
#define REG_FORCE_CURVE_SRAM_EN   REG_FLD_MSB_LSB(8, 8)
#define REG_FORCE_CURVE_SRAM_APB  REG_FLD_MSB_LSB(9, 9)
#define REG_FORCE_CURVE_SRAM_INT  REG_FLD_MSB_LSB(10, 10)
#define REG_CURVE_SRAM_RREQ_EN    REG_FLD_MSB_LSB(11, 14)
#define REG_SRAM_RREQ_EN          REG_FLD_MSB_LSB(16, 19)
#define REG_SRAM_WREQ_EN          REG_FLD_MSB_LSB(20, 21)
#define REG_SRAM_8X1_RREQ_EN      REG_FLD_MSB_LSB(24, 24)
#define REG_SRAM_8X1_WREQ_EN      REG_FLD_MSB_LSB(25, 25)
#define REG_SRAM_RW_SEL           REG_FLD_MSB_LSB(26, 26)
#define REG_CURVE_SRAM_WREQ_EN    REG_FLD_MSB_LSB(27, 28)
#define REG_SRAM_SOF_RST_SEL      REG_FLD_MSB_LSB(29, 29)
#define DMDP_AAL_SRAM_STATUS                    (0x0c8)
#define DMDP_AAL_SRAM_RW_IF_0                   (0x0cc)
#define DMDP_AAL_SRAM_RW_IF_1                   (0x0d0)
#define DMDP_AAL_SRAM_RW_IF_2                   (0x0d4)
#define DMDP_AAL_SRAM_RW_IF_3                   (0x0d8)
#define DMDP_AAL_CURVE_SRAM_RW_IF_0              (0x690)
#define DMDP_AAL_CURVE_SRAM_RW_IF_1              (0x694)
#define DMDP_AAL_CURVE_SRAM_RW_IF_2              (0x698)
#define DMDP_AAL_CURVE_SRAM_RW_IF_3              (0x69c)
#define DMDP_AAL_CURVE_SRAM_WADDR      DMDP_AAL_CURVE_SRAM_RW_IF_0
#define DMDP_AAL_CURVE_SRAM_WDATA      DMDP_AAL_CURVE_SRAM_RW_IF_1
#define DMDP_AAL_CURVE_SRAM_RADDR      DMDP_AAL_CURVE_SRAM_RW_IF_2
#define DMDP_AAL_CURVE_SRAM_RDATA      DMDP_AAL_CURVE_SRAM_RW_IF_3

#define DMDP_AAL_DRE_BLOCK_INFO_07              (0x0f8)
#define DMDP_AAL_TILE_00				(0x4EC)
#define DMDP_AAL_TILE_01				(0x4F0)
#define DMDP_AAL_TILE_02				(0x0F4)

#define DMDP_AAL_DUAL_PIPE00				(0x500)
#define DMDP_AAL_DUAL_PIPE08				(0x544)
#define DMDP_AAL_DRE_ROI_00						(0x520)
#define DMDP_AAL_DRE_ROI_01						(0x524)
#define DMDP_AAL_STATUS_00                      (0x224)
#define DMDP_AAL_Y_HISTOGRAM_00                     (0x604)

/* AAL Calarty */
#define DMDP_AAL_DRE_BILATEAL                    (0x53C)
#define DMDP_AAL_DRE_BILATERAL_Blending_00       (0x564)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT1_00      (0x568)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT1_01      (0x56C)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT1_02      (0x570)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT2_00      (0x574)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT2_01      (0x578)
#define DMDP_AAL_DRE_BILATERAL_CUST_FLT2_02      (0x57C)
#define DMDP_AAL_DRE_BILATERAL_FLT_CONFIG        (0x580)
#define DMDP_AAL_DRE_BILATERAL_FREQ_BLENDING     (0x584)
#define DMDP_AAL_DRE_BILATERAL_STATUS_00         (0x588)
#define DMDP_AAL_DRE_BILATERAL_REGION_PROTECTION (0x5A8)
#define DMDP_AAL_DRE_BILATERAL_STATUS_ROI_X      (0x5AC)
#define DMDP_AAL_DRE_BILATERAL_STATUS_ROI_Y      (0x5B0)
#define DMDP_AAL_DRE_BILATERAL_Blending_01       (0x5B4)
#define DMDP_AAL_DRE_BILATERAL_STATUS_CTRL       (0x5B8)

/* TDSHP Clarity */
#define MDP_TDSHP_00                            (0x000)
#define MDP_TDSHP_CFG                           (0x110)
#define MDP_HIST_CFG_00                         (0x064)
#define MDP_HIST_CFG_01                         (0x068)
#define MDP_LUMA_HIST_00                        (0x06C)
#define MDP_LUMA_SUM                            (0x0B4)
#define MDP_TDSHP_SRAM_1XN_OUTPUT_CNT           (0x0B8)
#define MDP_Y_FTN_1_0_MAIN                      (0x0BC)
#define MDP_TDSHP_STATUS_00                     (0x644)
#define MIDBAND_COEF_V_CUST_FLT1_00             (0x584)
#define MIDBAND_COEF_V_CUST_FLT1_01             (0x588)
#define MIDBAND_COEF_V_CUST_FLT1_02             (0x58C)
#define MIDBAND_COEF_V_CUST_FLT1_03             (0x590)
#define MIDBAND_COEF_H_CUST_FLT1_00             (0x594)
#define MIDBAND_COEF_H_CUST_FLT1_01             (0x598)
#define MIDBAND_COEF_H_CUST_FLT1_02             (0x59C)
#define MIDBAND_COEF_H_CUST_FLT1_03             (0x600)

#define HIGHBAND_COEF_V_CUST_FLT1_00            (0x604)
#define HIGHBAND_COEF_V_CUST_FLT1_01            (0x608)
#define HIGHBAND_COEF_V_CUST_FLT1_02            (0x60C)
#define HIGHBAND_COEF_V_CUST_FLT1_03            (0x610)
#define HIGHBAND_COEF_H_CUST_FLT1_00            (0x614)
#define HIGHBAND_COEF_H_CUST_FLT1_01            (0x618)
#define HIGHBAND_COEF_H_CUST_FLT1_02            (0x61C)
#define HIGHBAND_COEF_H_CUST_FLT1_03            (0x620)
#define HIGHBAND_COEF_RD_CUST_FLT1_00           (0x624)
#define HIGHBAND_COEF_RD_CUST_FLT1_01           (0x628)
#define HIGHBAND_COEF_RD_CUST_FLT1_02           (0x62C)
#define HIGHBAND_COEF_RD_CUST_FLT1_03           (0x630)
#define HIGHBAND_COEF_LD_CUST_FLT1_00           (0x634)
#define HIGHBAND_COEF_LD_CUST_FLT1_01           (0x638)
#define HIGHBAND_COEF_LD_CUST_FLT1_02           (0x63C)
#define HIGHBAND_COEF_LD_CUST_FLT1_03           (0x640)
#define MDP_TDSHP_SIZE_PARA                     (0x674)
#define MDP_TDSHP_FREQUENCY_WEIGHTING	        (0x678)
#define MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL	(0x67C)
#define SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH	(0x680)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE	        (0x684)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_BIG	        (0x688)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM	(0x68C)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL	(0x690)
#define ACTIVE_PARA_FREQ_M	                (0x694)
#define ACTIVE_PARA_FREQ_H	                (0x698)
#define ACTIVE_PARA_FREQ_D	                (0x69C)
#define ACTIVE_PARA_FREQ_L	                (0x700)
#define ACTIVE_PARA	                        (0x704)
#define CLASS_0_2_GAIN	                        (0x708)
#define CLASS_3_5_GAIN	                        (0x70C)
#define CLASS_6_8_GAIN	                        (0x710)
#define LUMA_CHROMA_PARAMETER	                (0x714)
#define MDP_TDSHP_STATUS_ROI_X	                (0x718)
#define MDP_TDSHP_STATUS_ROI_Y	                (0x71C)
#define FRAME_WIDTH_HIGHT	                (0x720)
#define MDP_TDSHP_SHADOW_CTRL	                (0x724)

#define AAL_SERVICE_FORCE_UPDATE 0x1
#define AAL_DRE3_POINT_NUM		(17)
#define AAL_DRE_GAIN_POINT16_START	(512)

#define aal_min(a, b)			(((a) < (b)) ? (a) : (b))

enum AAL_USER_CMD {
	FLIP_SRAM,
	FLIP_CURVE_SRAM,
};

enum AAL_DRE_MODE {
	DRE_EN_BY_CUSTOM_LIB = 0xFFFF,
	DRE_OFF = 0,
	DRE_ON = 1
};

enum AAL_ESS_MODE {
	ESS_EN_BY_CUSTOM_LIB = 0xFFFF,
	ESS_OFF = 0,
	ESS_ON = 1
};

enum AAL_ESS_LEVEL {
	ESS_LEVEL_BY_CUSTOM_LIB = 0xFFFF
};

enum DISP_AAL_REFRESH_LATENCY {
	AAL_REFRESH_17MS = 17,
	AAL_REFRESH_33MS = 33
};

static int dump_blk_x = -1;
static int dump_blk_y = -1;
static struct drm_device *g_drm_dev;
static bool debug_bypass_alg_mode;
static bool debug_skip_set_param;
static bool debug_dump_input_param;
static bool debug_dump_aal_hist;
static bool debug_dump_init_reg;
static bool debug_dump_clarity_regs;

static void print_uint_array(const char *tag, const unsigned int *arr, int length, int elements_per_line)
{
	char buf[256];
	int offset = 0, start = 0, end = start + elements_per_line - 1;

	for (int i = 0; i < length; i++) {
		if (i % elements_per_line == 0) {
			start = i;
			end = start + elements_per_line - 1;
			offset += snprintf(buf + offset, sizeof(buf) - offset, "[%s:%d-%d] ", tag, start, end);
			if (offset >= sizeof(buf)) {
				buf[sizeof(buf) - 1] = '\0';  // Ensure null-termination
				AALDUMP_LOG("%s overflow\n", buf);
				offset = 0;
				offset += snprintf(buf + offset, sizeof(buf) - offset, "[%s:%d-%d] ", tag, start, end);
			}
		}
		offset += snprintf(buf + offset, sizeof(buf) - offset, "%u ", arr[i]);
		if (offset >= sizeof(buf)) {
			buf[sizeof(buf) - 1] = '\0';  // Ensure null-termination
			AALDUMP_LOG("%s overflow\n", buf);
			offset = 0;
			start = i;
			end = start + elements_per_line - 1;
			offset += snprintf(buf + offset, sizeof(buf) - offset, "[%s:%d-%d] ", tag, start, end);
			offset += snprintf(buf + offset, sizeof(buf) - offset, "%u ", arr[i]);
		}
		if ((i + 1) % elements_per_line == 0 || i == length - 1) {
			if (offset > 0)
				buf[offset-1] = '\0';  // Replace the last space with a newline character
			else
				buf[0] = '\0';  // Replace the last space with a newline character
			AALDUMP_LOG("%s\n", buf);
			offset = 0;  // Reset the offset
			start = i;
			end = start + elements_per_line - 1;
		}
	}
}

static inline phys_addr_t disp_aal_dre3_pa(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.pa : comp->regs_pa;
}

static inline void __iomem *disp_aal_dre3_va(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.va : comp->regs;
}
static inline void disp_aal_relay_control(struct mtk_ddp_comp *comp, bool relay)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_pq_relay_enable relayCtlSet;
	int delay_trigger = atomic_read(&aal_data->primary_data->force_delay_check_trig);

	relayCtlSet.wait_hw_config_done = true;
	relayCtlSet.relay_engines = MTK_DISP_PQ_AAL_RELAY | MTK_DISP_PQ_DMDP_AAL_RELAY;
	relayCtlSet.caller = PQ_FEATURE_HAL_AAL_FUNC_CHG;
	relayCtlSet.enable = relay;

	if (relay) {
		atomic_set(&aal_data->primary_data->should_stop, 1);
		atomic_set(&aal_data->primary_data->func_flag, 0);
		disp_pq_proxy_virtual_relay_engines(&comp->mtk_crtc->base, &relayCtlSet);
	} else {
		disp_pq_proxy_virtual_relay_engines(&comp->mtk_crtc->base, &relayCtlSet);
		atomic_set(&aal_data->primary_data->should_stop, 0);
		atomic_set(&aal_data->primary_data->func_flag, 1);
		atomic_set(&aal_data->primary_data->hal_force_update, 1);
		atomic_set(&aal_data->dre20_hist_is_ready, 0);
		atomic_set(&aal_data->hist_available, 0);
		atomic_set(&aal_data->first_frame, 1);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

			atomic_set(&aal1_data->dre20_hist_is_ready, 0);
			atomic_set(&aal1_data->hist_available, 0);
			atomic_set(&aal1_data->first_frame, 1);
		}
		mtk_crtc_check_trigger(comp->mtk_crtc, delay_trigger, true);
	}
}

static void disp_aal_set_interrupt(struct mtk_ddp_comp *comp,
	int enable, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int bypass;

	if (!aal_data->primary_data->aal_fo->mtk_aal_support) {
		AALIRQ_LOG("aal is not support\n");
		return;
	}

	if (aal_data->is_right_pipe)
		return;

	bypass = aal_data->primary_data->relay_state != 0 ? 1 : 0;
	if (enable &&
		(bypass != 1 || pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])) {
		/* Enable output frame end interrupt */
		mtk_ddp_write_relaxed(comp, AAL_IRQ_OF_END, DISP_AAL_INTEN, handle);

		atomic_set(&aal_data->primary_data->eof_irq_en, 1);
		AALIRQ_LOG("interrupt enabled\n");
	} else if (!enable) {
		mtk_ddp_write_relaxed(comp, 0x0, DISP_AAL_INTEN, handle);
		mtk_ddp_write_relaxed(comp, 0x0, DISP_AAL_INTSTA, handle);
		atomic_set(&aal_data->primary_data->eof_irq_en, 0);
		AALIRQ_LOG("interrupt disabled\n");
	}
}

void disp_aal_refresh_by_kernel(struct mtk_disp_aal *aal_data, int need_lock)
{
	struct mtk_ddp_comp *comp = &aal_data->ddp_comp;
	bool delay_trig = atomic_read(&aal_data->primary_data->force_delay_check_trig);

	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1) {
		if (need_lock)
			DDP_MUTEX_LOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
		atomic_set(&aal_data->primary_data->force_event_en, 1);
		atomic_set(&aal_data->primary_data->event_en, 1);

		/*
		 * Backlight or Kernel API latency should be smallest
		 * only need to trigger when calling not from atomic
		 */
		mtk_crtc_check_trigger(comp->mtk_crtc, delay_trig, false);
		if (need_lock)
			DDP_MUTEX_UNLOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
	}
}

static void disp_aal_refresh_trigger(struct work_struct *work_item)
{
	struct work_struct_aal_data *work_data = container_of(work_item,
						struct work_struct_aal_data, task);
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	if (!work_data->data)
		return;
	comp  = (struct mtk_ddp_comp *)work_data->data;
	aal_data = comp_to_aal(work_data->data);

	AALFLOW_LOG("start\n");

	if (atomic_read(&aal_data->primary_data->force_delay_check_trig) == 1)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
	else
		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);
}

void disp_aal_notify_backlight_changed(struct mtk_ddp_comp *comp,
		int trans_backlight, int panel_nits, int max_backlight, int need_lock)
{
	unsigned long flags;
	unsigned int service_flags;
	int prev_backlight;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);
	AALAPI_LOG("connector_id = %d, bl %d/%d nits %d type %d\n", connector_id,
		trans_backlight, max_backlight, panel_nits, aal_data->primary_data->led_type);

	if ((max_backlight != -1) && (trans_backlight > max_backlight))
		trans_backlight = max_backlight;

	prev_backlight = atomic_read(&aal_data->primary_data->backlight_notified);
	atomic_set(&aal_data->primary_data->backlight_notified, trans_backlight);
	CRTC_MMP_MARK(0, notify_backlight, trans_backlight, prev_backlight);

	service_flags = 0;
	if ((prev_backlight == 0) && (prev_backlight != trans_backlight))
		service_flags = AAL_SERVICE_FORCE_UPDATE;

	if (trans_backlight == 0) {
		aal_data->primary_data->backlight_set = trans_backlight;

		if (aal_data->primary_data->led_type != TYPE_ATOMIC)
			mtk_leds_brightness_set(connector_id, 0, 0, (0X1<<SET_BACKLIGHT_LEVEL));
		/* set backlight = 0 may be not from AAL, */
		/* we have to let AALService can turn on backlight */
		/* on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 0 ||
		((aal_data->primary_data->relay_state != 0) &&
		!pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])) {
		/* AAL Service is not running */

		if (aal_data->primary_data->led_type != TYPE_ATOMIC)
			mtk_leds_brightness_set(connector_id, trans_backlight,
						0, (0X1<<SET_BACKLIGHT_LEVEL));
	}

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);
	aal_data->primary_data->hist.backlight = trans_backlight;
	aal_data->primary_data->hist.panel_nits = panel_nits;
	aal_data->primary_data->hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
	// always notify aal service for LED changed
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, need_lock);
	if (aal_data->primary_data->led_type != TYPE_ATOMIC)
		disp_aal_refresh_by_kernel(aal_data, need_lock);
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event_to_aal(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_conf_info *led_conf;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct pq_common_data *pq_data = NULL;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	led_conf = (struct led_conf_info *)v;
	if (!led_conf) {
		DDPPR_ERR("%s: led_conf is NULL!\n", __func__);
		return -1;
	}
	crtc = disp_pq_get_crtc_from_connector(led_conf->connector_id, g_drm_dev);
	if (crtc == NULL) {
		led_conf->aal_enable = 0;
		DDPPR_ERR("%s: connector_id(%d) failed to get crtc!\n", __func__,
				led_conf->connector_id);
		return NOTIFY_DONE;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!(mtk_crtc->crtc_caps.crtc_ability & ABILITY_PQ) ||
			atomic_read(&mtk_crtc->pq_data->pipe_info_filled) != 1) {
		DDPINFO("%s, bl %d no need pq, connector_id:%d, crtc_id:%d\n", __func__,
				led_conf->cdev.brightness, led_conf->connector_id, drm_crtc_index(crtc));
		led_conf->aal_enable = 0;
		return NOTIFY_DONE;
	}

	pq_data = mtk_crtc->pq_data;
	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		led_conf->aal_enable = 0;
		DDPINFO("%s: connector_id: %d, crtc_id: %d, has no DISP_AAL comp!\n", __func__,
				led_conf->connector_id, drm_crtc_index(crtc));
		return NOTIFY_DONE;
	}

	switch (event) {
	case LED_BRIGHTNESS_CHANGED:
		trans_level = led_conf->cdev.brightness;

		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		led_conf->aal_enable = 1;
		aal_data = comp_to_aal(comp);
		if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS] &&
			((aal_data->primary_data->relay_state == 0))) {
			disp_aal_notify_backlight_changed(comp, trans_level, -1,
				led_conf->cdev.max_brightness, 1);
		} else {
			trans_level = (
				led_conf->max_hw_brightness
				* led_conf->cdev.brightness
				+ (led_conf->cdev.max_brightness / 2))
				/ led_conf->cdev.max_brightness;
			if (led_conf->cdev.brightness != 0 &&
				trans_level == 0)
				trans_level = 1;

			disp_aal_notify_backlight_changed(comp, trans_level, -1,
				led_conf->max_hw_brightness, 1);
		}

		AALAPI_LOG("brightness changed: %d(%d)\n",
			trans_level, led_conf->cdev.brightness);
		break;
	case LED_STATUS_SHUTDOWN:
		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])
			disp_aal_notify_backlight_changed(comp, 0, -1,
				led_conf->cdev.max_brightness, 1);
		else
			disp_aal_notify_backlight_changed(comp, 0, -1,
				led_conf->max_hw_brightness, 1);
		break;
	case LED_TYPE_CHANGED:
		pr_info("[leds -> aal] led type changed: %d", led_conf->led_type);

		aal_data = comp_to_aal(comp);
		aal_data->primary_data->led_type = (unsigned int)led_conf->led_type;

		// force set aal_enable to 1 for ELVSS
		if (led_conf->led_type == LED_TYPE_ATOMIC)
			led_conf->aal_enable = 1;

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event_to_aal,
};
#endif

static void disp_aal_dump_clarity_regs(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (debug_dump_clarity_regs && !aal_data->is_right_pipe) {
		print_uint_array("aal0 clarity readback", aal_data->primary_data->hist.aal0_clarity, 6, 6);
		print_uint_array("tdshp0 clarity readback", aal_data->primary_data->hist.tdshp0_clarity, 12, 12);
	} else if (debug_dump_clarity_regs && aal_data->is_right_pipe) {
		print_uint_array("aal1 clarity readback", aal_data->primary_data->hist.aal1_clarity, 6, 6);
		print_uint_array("tdshp1 clarity readback", aal_data->primary_data->hist.tdshp1_clarity, 12, 12);
	}
}

static void disp_aal_dump_ghist(struct mtk_ddp_comp *comp, struct DISP_AAL_HIST *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	print_uint_array("aal0_maxHist", data->aal0_maxHist, 33, 11);
	print_uint_array("aal0_yHist", data->aal0_yHist, 33, 11);
	if (aal_data->data->mdp_aal_ghist_support) {
		print_uint_array("mdp_aal0_maxHist", data->mdp_aal0_maxHist, 33, 11);
		print_uint_array("mdp_aal0_yHist", data->mdp_aal0_yHist, 33, 11);
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		print_uint_array("aal1_maxHist", data->aal1_maxHist, 33, 11);
		print_uint_array("aal1_yHist", data->aal1_yHist, 33, 11);
		if (aal_data->data->mdp_aal_ghist_support) {
			print_uint_array("mdp_aal1_maxHist", data->mdp_aal1_maxHist, 33, 11);
			print_uint_array("mdp_aal1_yHist", data->mdp_aal1_yHist, 33, 11);
		}
	}

	pr_notice("%s serviceFlags:%u, backlight: %d, colorHist: %d\n", __func__,
			data->serviceFlags, data->backlight, data->aal0_colorHist);
	pr_notice("%s requestPartial:%d, panel_type: %u\n", __func__,
			data->requestPartial, data->panel_type);
	pr_notice("%s essStrengthIndex:%d, ess_enable: %d, dre_enable: %d\n", __func__,
			data->essStrengthIndex, data->ess_enable,
			data->dre_enable);
}

void disp_aal_dump_ess_voltage_info(struct DISP_PANEL_BASE_VOLTAGE *data)
{
	int i = 0;

	if (data->flag) {
		pr_notice("%s Anodeoffset:\n", __func__);
		for (i = 0; i < 23; i++)
			pr_notice("%s AnodeOffset[%d] = %d\n", __func__, i, data->AnodeOffset[i]);

		pr_notice("%s ELVSSoffset:\n", __func__);
		for (i = 0; i < 23; i++)
			pr_notice("%s ELVSSBase[%d] = %d\n", __func__, i, data->ELVSSBase[i]);
	} else
		pr_notice("%s invalid base voltage\n", __func__);
}

#define PRINT_INIT_REG(x1) pr_notice("[INIT]%s=0x%x\n", #x1, data->x1)
void disp_aal_dump_init_reg(struct DISP_AAL_INITREG *data)
{
	PRINT_INIT_REG(dre_s_lower);
	PRINT_INIT_REG(dre_s_upper);
	PRINT_INIT_REG(dre_y_lower);
	PRINT_INIT_REG(dre_y_upper);
	PRINT_INIT_REG(dre_h_lower);
	PRINT_INIT_REG(dre_h_upper);
	PRINT_INIT_REG(dre_h_slope);
	PRINT_INIT_REG(dre_s_slope);
	PRINT_INIT_REG(dre_y_slope);
	PRINT_INIT_REG(dre_x_alpha_base);
	PRINT_INIT_REG(dre_x_alpha_shift_bit);
	PRINT_INIT_REG(dre_y_alpha_base);
	PRINT_INIT_REG(dre_y_alpha_shift_bit);
	PRINT_INIT_REG(dre_blk_x_num);
	PRINT_INIT_REG(dre_blk_y_num);
	PRINT_INIT_REG(dre_blk_height);
	PRINT_INIT_REG(dre_blk_width);
	PRINT_INIT_REG(dre_blk_area);
	PRINT_INIT_REG(dre_blk_area_min);
	PRINT_INIT_REG(hist_bin_type);
	PRINT_INIT_REG(dre_flat_length_slope);
	PRINT_INIT_REG(dre_flat_length_th);
	PRINT_INIT_REG(blk_num_x_start);
	PRINT_INIT_REG(blk_num_x_end);
	PRINT_INIT_REG(dre0_blk_num_x_start);
	PRINT_INIT_REG(dre0_blk_num_x_end);
	PRINT_INIT_REG(dre1_blk_num_x_start);
	PRINT_INIT_REG(dre1_blk_num_x_end);
	PRINT_INIT_REG(blk_cnt_x_start);
	PRINT_INIT_REG(blk_cnt_x_end);
	PRINT_INIT_REG(blk_num_y_start);
	PRINT_INIT_REG(blk_num_y_end);
	PRINT_INIT_REG(blk_cnt_y_start);
	PRINT_INIT_REG(blk_cnt_y_end);
	PRINT_INIT_REG(dre0_blk_cnt_x_start);
	PRINT_INIT_REG(dre0_blk_cnt_x_end);
	PRINT_INIT_REG(dre1_blk_cnt_x_start);
	PRINT_INIT_REG(dre1_blk_cnt_x_end);
	PRINT_INIT_REG(act_win_x_start);
	PRINT_INIT_REG(act_win_x_end);
	PRINT_INIT_REG(dre0_act_win_x_start);
	PRINT_INIT_REG(dre0_act_win_x_end);
	PRINT_INIT_REG(dre1_act_win_x_start);
	PRINT_INIT_REG(dre1_act_win_x_end);
}

void disp_aal_dump_param(const struct DISP_AAL_PARAM *param)
{
	print_uint_array("DREGainFltStatus", param->DREGainFltStatus, sizeof(param->DREGainFltStatus) / 4, 10);
	print_uint_array("cabc_gainlmt", param->cabc_gainlmt, sizeof(param->cabc_gainlmt) / 4, 11);
	pr_notice("cabc_fltgain_force: %d, FinalBacklight: %d", param->cabc_fltgain_force, param->FinalBacklight);
	pr_notice("allowPartial: %d, refreshLatency: %d", param->allowPartial, param->refreshLatency);
}

static bool disp_aal_read_ghist(struct mtk_ddp_comp *comp)
{
	bool read_success = true;
	int i;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *disp_tdshp = aal_data->comp_tdshp;
	void __iomem *dre3_va = disp_aal_dre3_va(comp);

	if (atomic_read(&aal_data->hw_hist_ready) == 0) {
		AALIRQ_LOG("no eof, skip\n");
		return false;
	}
	if (!aal_data->is_right_pipe) {
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal0_maxHist[i] = readl(comp->regs +
					DISP_AAL_STATUS_00 + (i << 2));
		}
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal0_yHist[i] = readl(comp->regs +
					DISP_Y_HISTOGRAM_00 + (i << 2));
		}
		if (aal_data->data->mdp_aal_ghist_support &&
			aal_data->primary_data->aal_fo->mtk_dre30_support) {
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 1;
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal0_maxHist[i] = readl(dre3_va +
						DMDP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal0_yHist[i] = readl(dre3_va +
						DMDP_AAL_Y_HISTOGRAM_00 + (i << 2));
			}
		} else
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 0;
		read_success = disp_color_reg_get(aal_data->comp_color,
				"disp_color_two_d_w1_result",
				&aal_data->primary_data->hist.aal0_colorHist);

		// for Display Clarity
		if (aal_data->primary_data->disp_clarity_support) {
			for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.aal0_clarity[i] =
				readl(dre3_va + DMDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
			}

			for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.tdshp0_clarity[i] =
				readl(disp_tdshp->regs + MDP_TDSHP_STATUS_00 + (i << 2));
			}

			disp_aal_dump_clarity_regs(comp);
		}
	} else {
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal1_maxHist[i] = readl(comp->regs +
					DISP_AAL_STATUS_00 + (i << 2));
		}
		for (i = 0; i < AAL_HIST_BIN; i++) {
			aal_data->primary_data->hist.aal1_yHist[i] = readl(comp->regs +
					DISP_Y_HISTOGRAM_00 + (i << 2));
		}
		if (aal_data->data->mdp_aal_ghist_support &&
			aal_data->primary_data->aal_fo->mtk_dre30_support) {
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 1;
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal1_maxHist[i] = readl(dre3_va +
						DMDP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				aal_data->primary_data->hist.mdp_aal1_yHist[i] = readl(dre3_va +
						DMDP_AAL_Y_HISTOGRAM_00 + (i << 2));
			}
		} else
			aal_data->primary_data->hist.mdp_aal_ghist_valid = 0;
		read_success = disp_color_reg_get(aal_data->comp_color,
				"disp_color_two_d_w1_result",
				&aal_data->primary_data->hist.aal1_colorHist);

		// for Display Clarity
		if (aal_data->primary_data->disp_clarity_support) {
			for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.aal1_clarity[i] =
				readl(dre3_va + DMDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
			}

			for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
				aal_data->primary_data->hist.tdshp1_clarity[i] =
				readl(disp_tdshp->regs + MDP_TDSHP_STATUS_00 + (i << 2));
			}

			disp_aal_dump_clarity_regs(comp);
		}
	}
	return read_success;
}

static bool disp_aal_read_dre3_hist(struct mtk_ddp_comp *comp,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int hist_offset;
	int arry_offset = 0;
	unsigned int read_value;
	int dump_start = -1;
	u32 dump_table[6] = {0};
	int i = 0, j = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_auto_inc = aal_data->data->aal_dre3_auto_inc;
	void __iomem *dre3_va = disp_aal_dre3_va(comp);

	/* Read Global histogram for ESS */
	//if (disp_aal_read_ghist(comp) != true)
		//return false;
	if (atomic_read(&aal_data->hw_hist_ready) == 0) {
		AALIRQ_LOG("no eof, skip\n");
		return false;
	}
	atomic_set(&aal_data->hw_hist_ready, 0);
	AALIRQ_LOG("start\n");
	if (dump_blk_x >= 0 && dump_blk_x < 16
		&& dump_blk_y >= 0 && dump_blk_y < 8)
		dump_start = 6 * (dump_blk_x + dump_blk_y * 16);

	/* Read Local histogram for DRE 3 */
	hist_offset = aal_data->data->aal_dre_hist_start;
	writel(hist_offset, dre3_va + DMDP_AAL_SRAM_RW_IF_2);
	for (; hist_offset <= aal_data->data->aal_dre_hist_end;
			hist_offset += 4) {
		if (!aal_dre3_auto_inc)
			writel(hist_offset, dre3_va + DMDP_AAL_SRAM_RW_IF_2);
		read_value = readl(dre3_va + DMDP_AAL_SRAM_RW_IF_3);

		if (arry_offset >= AAL_DRE30_HIST_REGISTER_NUM)
			return false;
		if (dump_start >= 0 && arry_offset >= dump_start
			&& arry_offset < (dump_start + 6))
			dump_table[arry_offset-dump_start] = read_value;
		if (aal_data->is_right_pipe) {
			aal_data->primary_data->dre30_hist.aal1_dre_hist[arry_offset++] =
						read_value;
		} else {
			aal_data->primary_data->dre30_hist.aal0_dre_hist[arry_offset++] =
						read_value;
		}
	}

	if (!aal_data->is_right_pipe) {
		for (i = 0; i < 8; i++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe0[i] = readl(dre3_va +
				DMDP_AAL_DUAL_PIPE00 + (i << 2));
			}
		for (j = 0; j < 8; j++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe0[j+i] = readl(dre3_va +
				DMDP_AAL_DUAL_PIPE08 + (j << 2));
		}
	} else {
		for (i = 0; i < 8; i++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe1[i] = readl(dre3_va +
				DMDP_AAL_DUAL_PIPE00 + (i << 2));
		}
		for (j = 0; j < 8; j++) {
			aal_data->primary_data->hist.MaxHis_denominator_pipe1[j+i] = readl(dre3_va +
				DMDP_AAL_DUAL_PIPE08 + (j << 2));
		}
	}

	if (dump_start >= 0)
		pr_notice("[DRE3][HIST][%d-%d] %08x %08x %08x %08x %08x %08x\n",
			dump_blk_x, dump_blk_y,
			dump_table[0], dump_table[1], dump_table[2],
			dump_table[3], dump_table[4], dump_table[5]);

	return true;
}

static void disp_aal_single_pipe_hist_update(struct mtk_ddp_comp *comp, unsigned int status)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool read_success = false;

	CRTC_MMP_EVENT_START(0, aal_dre20_rh, comp->id, 0);
	/* Only process end of frame state */
	if ((status & AAL_IRQ_OF_END) == 0x0) {
		AALERR("break comp %u status 0x%x\n", comp->id, status);
		CRTC_MMP_EVENT_END(0, aal_dre20_rh, comp->id, 1);
		return;
	}

	CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 2);
	if (spin_trylock_irqsave(&aal_data->primary_data->hist_lock, flags)) {
		read_success = disp_aal_read_ghist(comp);
		spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
		if (read_success == true) {
			atomic_set(&aal_data->dre20_hist_is_ready, 1);
			AALIRQ_LOG("%s read_success = %d\n",mtk_dump_comp_str(comp), read_success);

			if (atomic_read(&aal_data->first_frame) == 1) {
				atomic_set(&aal_data->first_frame, 0);
				aal_data->primary_data->refresh_task.data = (void *)comp;
				queue_work(aal_data->primary_data->refresh_wq,
					&aal_data->primary_data->refresh_task.task);
			}
		}
	} else {
		AALIRQ_LOG("comp %d hist not retrieved\n", comp->id);
		CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 0xEE);
	}
	CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 3);
	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 0)
		disp_aal_set_interrupt(comp, 0, NULL);
	CRTC_MMP_EVENT_END(0, aal_dre20_rh, comp->id, 4);
}

static bool disp_aal_write_dre3_curve(struct mtk_ddp_comp *comp, bool force_write)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;
	unsigned int sram_waddr, sram_wdata;

	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem *dre3_va = disp_aal_dre3_va(comp);

	/* Write Local Gain Curve for DRE 3 */
	AALIRQ_LOG("start\n");
	if (aal_data->data->aal_dre3_curve_sram &&
	    !atomic_read(&aal_data->primary_data->dre30_write) && !force_write) {
		AALIRQ_LOG("no need to write dre3\n");
		return true;
	}
	if (aal_data->data->aal_dre3_curve_sram) {
		sram_waddr = DMDP_AAL_CURVE_SRAM_WADDR;
		sram_wdata = DMDP_AAL_CURVE_SRAM_WDATA;
	} else {
		sram_waddr = DMDP_AAL_SRAM_RW_IF_0;
		sram_wdata = DMDP_AAL_SRAM_RW_IF_1;
	}
	writel(aal_data->data->aal_dre_gain_start, dre3_va + sram_waddr);
	for (gain_offset = aal_data->data->aal_dre_gain_start;
		gain_offset <= aal_data->data->aal_dre_gain_end;
			gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = aal_data->primary_data->dre30_gain.dre30_gain[arry_offset++];
		if (!aal_data->data->aal_dre3_auto_inc)
			writel(gain_offset, dre3_va + sram_waddr);
		writel(write_value, dre3_va + sram_wdata);
	}
	return true;
}

static int disp_aal_update_dre3_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	bool result = false;
	unsigned long flags;
	int dre_blk_x_num, dre_blk_y_num;
	unsigned int read_value;
	int hist_apb = 0, hist_int = 0, curve_apb = 0, curve_int;
	void __iomem *dre3_va = disp_aal_dre3_va(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *comp1 = NULL;
	struct mtk_disp_aal *aal1_data = NULL;

	AALIRQ_LOG("first_frame = %d\n", atomic_read(&aal_data->first_frame));
	AALIRQ_LOG("hist_available = %d\n", atomic_read(&aal_data->hist_available));
	AALIRQ_LOG("hw_hist_ready = %d\n", atomic_read(&aal_data->hw_hist_ready));
	if (comp->mtk_crtc->is_dual_pipe) {
		comp1 = aal_data->companion;
		aal1_data = comp_to_aal(comp1);
	}
	// reset g_aal_eof_irq to 0 when first frame,
	// to avoid timing issue
	if (atomic_read(&aal_data->first_frame) == 1)
		atomic_set(&aal_data->hw_hist_ready, 0);
	if (aal1_data && atomic_read(&aal1_data->first_frame) == 1)
		atomic_set(&aal1_data->hw_hist_ready, 0);

	CRTC_MMP_EVENT_START(0, aal_dre30_rw, comp->id, 0);
	if (check_sram) {
		read_value = readl(dre3_va + DMDP_AAL_SRAM_CFG);
		hist_apb = (read_value >> 5) & 0x1;
		hist_int = (read_value >> 6) & 0x1;
		AALIRQ_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) compID:%d\n",
			hist_apb, hist_int, read_value, comp->id);
		if (hist_int != atomic_read(&aal_data->force_hist_apb)) {
			AALIRQ_LOG("dre3: hist config %d != %d config?\n",
				hist_int, atomic_read(&aal_data->force_hist_apb));
			return -1;
		}
	}
	if (check_sram && aal_data->data->aal_dre3_curve_sram) {
		curve_apb = (read_value >> 9) & 0x1;
		curve_int = (read_value >> 10) & 0x1;
		AALIRQ_LOG("[SRAM] curve_apb(%d) curve_int(%d) 0x%08x in (SOF) compID:%d\n",
			curve_apb, curve_int, read_value, comp->id);
		if (curve_int != atomic_read(&aal_data->force_curve_sram_apb)) {
			AALIRQ_LOG("dre3: curve config %d != %d config?\n",
				curve_int, atomic_read(&aal_data->force_curve_sram_apb));
			return -1;
		}
	}
	dre_blk_x_num = aal_data->primary_data->init_regs.dre_blk_x_num;
	dre_blk_y_num = aal_data->primary_data->init_regs.dre_blk_y_num;
	mtk_drm_trace_begin("read_dre3_hist");
	if (spin_trylock_irqsave(&aal_data->primary_data->hist_lock, flags)) {
		result = disp_aal_read_dre3_hist(comp, dre_blk_x_num, dre_blk_y_num);
		if (result) {
			aal_data->primary_data->dre30_hist.dre_blk_x_num = dre_blk_x_num;
			aal_data->primary_data->dre30_hist.dre_blk_y_num = dre_blk_y_num;
			atomic_set(&aal_data->hist_available, 1);
		}
		if (comp1) {
			result = disp_aal_read_dre3_hist(comp1, dre_blk_x_num, dre_blk_y_num);
			if (result)
				atomic_set(&aal1_data->hist_available, 1);
		}
		spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
		if (result) {
			AALIRQ_LOG("wake up dre3\n");
			wake_up_interruptible(&aal_data->primary_data->hist_wq);
		} else
			AALIRQ_LOG("skip wake up dre3\n");
	} else {
		AALIRQ_LOG("comp %d hist not retrieved\n", comp->id);
		CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 0xEE);
	}
	CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 1);
	mtk_drm_trace_end();

	/* Write DRE 3.0 gain */
	mtk_drm_trace_begin("write_dre3_curve");
	if (!atomic_read(&aal_data->first_frame)) {
		mutex_lock(&aal_data->primary_data->config_lock);
		disp_aal_write_dre3_curve(comp, false);
		if (comp1)
			disp_aal_write_dre3_curve(comp1, false);
		mutex_unlock(&aal_data->primary_data->config_lock);
	}
	mtk_drm_trace_end();
	CRTC_MMP_EVENT_END(0, aal_dre30_rw, comp->id, 2);
	return 0;
}

static void disp_aal_write_dre3_curve_full(struct mtk_ddp_comp *comp)
{
	void __iomem *dre3_va = disp_aal_dre3_va(comp);
	uint32_t reg_value = 0, reg_mask = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *dmdp_aal = aal_data->comp_dmdp_aal;
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;

	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_INT);
	}
	mtk_ddp_write_mask_cpu(dmdp_aal, reg_value, DMDP_AAL_SRAM_CFG, reg_mask);
	disp_aal_write_dre3_curve(comp, true);

	reg_value = 0;
	reg_mask = 0;
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(reg_value, reg_mask, 1, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(reg_value, reg_mask, 0, REG_FORCE_CURVE_SRAM_INT);
	}
	mtk_ddp_write_mask_cpu(dmdp_aal, reg_value, DMDP_AAL_SRAM_CFG, reg_mask);
	disp_aal_write_dre3_curve(comp, true);

	atomic_set(&aal_data->force_hist_apb, 0);
	if (aal_dre3_curve_sram)
		atomic_set(&aal_data->force_curve_sram_apb, 0);
}

static bool disp_aal_dre3_write_linear_curve(struct mtk_disp_aal *aal_data, const unsigned int *dre3_gain,
	const int block_x, const int block_y, const int dre_blk_x_num, int check)
{
	bool return_value = false;
	uint32_t block_offset = 4 * (block_y * dre_blk_x_num + block_x);
	uint32_t value;

	do {
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[0] & 0xff) |
			((dre3_gain[1] & 0xff) << 8) |
			((dre3_gain[2] & 0xff) << 16) |
			((dre3_gain[3] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[4] & 0xff) |
			((dre3_gain[5] & 0xff) << 8) |
			((dre3_gain[6] & 0xff) << 16) |
			((dre3_gain[7] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[8] & 0xff) |
			((dre3_gain[9] & 0xff) << 8) |
			((dre3_gain[10] & 0xff) << 16) |
			((dre3_gain[11] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		value = ((dre3_gain[12] & 0xff) |
			((dre3_gain[13] & 0xff) << 8) |
			((dre3_gain[14] & 0xff) << 16) |
			((dre3_gain[15] & 0xff) << 24));
		if (check && value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
			       __func__, __LINE__, dre_blk_x_num, block_x, block_y, block_offset,
			       value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
			return return_value;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] = value;

		return_value = true;
	} while (0);

	return return_value;
}

static bool disp_aal_dre3_write_linear_curve16(struct mtk_disp_aal *aal_data, const unsigned int *dre3_gain,
	const int dre_blk_x_num, const int dre_blk_y_num, int check)
{
	int32_t blk_x, blk_y;
	const int32_t blk_num_max = dre_blk_x_num * dre_blk_y_num;
	unsigned int write_value = 0x0;
	uint32_t bit_shift = 0;
	uint32_t block_offset = AAL_DRE_GAIN_POINT16_START;

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			write_value |=
				((dre3_gain[16] & 0xff) << (8*bit_shift));
			bit_shift++;

			if (bit_shift >= 4) {
				if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
					return false;
				if (check &&
				    write_value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
					DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
					       __func__, __LINE__, dre_blk_x_num,
						   blk_x, blk_y, block_offset, write_value,
						   aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
					return false;
				}
				aal_data->primary_data->dre30_gain.dre30_gain[block_offset++] =
					write_value;

				write_value = 0x0;
				bit_shift = 0;
			}
		}
	}

	if ((blk_num_max>>2)<<2 != blk_num_max) {
		/* configure last curve */
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		if (check && write_value != aal_data->primary_data->dre30_gain.dre30_gain[block_offset]) {
			DDPAEE("%s,%d:x_max %d, blk(%d, %d) %u expect 0x%x but 0x%x\n",
			       __func__, __LINE__, dre_blk_x_num, blk_x, blk_y, block_offset,
			       write_value, aal_data->primary_data->dre30_gain.dre30_gain[block_offset]);
			return false;
		}
		aal_data->primary_data->dre30_gain.dre30_gain[block_offset] = write_value;
	}

	return true;
}

static void disp_aal_dre3_reset_to_linear(struct mtk_ddp_comp *comp, int check)
{
	const int dre_blk_x_num = 8;
	const int dre_blk_y_num = 16;
	int blk_x, blk_y, curve_point;
	unsigned int dre3_gain[AAL_DRE3_POINT_NUM];
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("start\n");
	for (curve_point = 0; curve_point < AAL_DRE3_POINT_NUM;
		curve_point++) {
		/* assign initial gain curve */
		dre3_gain[curve_point] = aal_min(255, 16 * curve_point);
	}

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			/* write each block dre curve */
			if (!disp_aal_dre3_write_linear_curve(aal_data,
				dre3_gain, blk_x, blk_y, dre_blk_x_num, check)) {
				AALERR("%s write_linear_curve error\n");
				return;
			}
		}
	}
	/* write each block dre curve last point */
	if (!disp_aal_dre3_write_linear_curve16(aal_data,
		dre3_gain, dre_blk_x_num, dre_blk_y_num, check))
		AALERR("%s write_linear_curve16 error\n");
}

static void disp_aal_init_dre3_curve(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	mutex_lock(&aal_data->primary_data->config_lock);
	disp_aal_dre3_reset_to_linear(comp, 0);
	disp_aal_write_dre3_curve_full(comp);
	mutex_unlock(&aal_data->primary_data->config_lock);
}

void disp_aal_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	u32 hist_apb = 0, hist_int = 0, sram_cfg = 0, sram_mask = 0;
	u32 curve_apb = 0, curve_int = 0;
	phys_addr_t dre3_pa = disp_aal_dre3_pa(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;
	int dre30_write = atomic_read(&aal_data->primary_data->dre30_write);
	atomic_t *curve_sram_apb = &aal_data->force_curve_sram_apb;

	if (!aal_data->primary_data->aal_fo->mtk_dre30_support)
		return;

	if (atomic_read(&aal_data->dre_config) == 1 && !atomic_read(&aal_data->first_frame)) {
		AALFLOW_LOG("[SRAM] dre_config not 0 in %s\n", caller);
		return;
	}

	atomic_set(&aal_data->dre_config, 1);
	if (atomic_cmpxchg(&aal_data->force_hist_apb, 0, 1) == 0) {
		hist_apb = 0;
		hist_int = 1;
	} else if (atomic_cmpxchg(&aal_data->force_hist_apb, 1, 0) == 1) {
		hist_apb = 1;
		hist_int = 0;
	} else
		AALERR("[SRAM] Error when get hist_apb in %s\n", caller);

	if (aal_dre3_curve_sram) {
		if (dre30_write) {
			if (atomic_cmpxchg(curve_sram_apb, 0, 1) == 0) {
				curve_apb = 0;
				curve_int = 1;
			} else if (atomic_cmpxchg(curve_sram_apb, 1, 0) == 1) {
				curve_apb = 1;
				curve_int = 0;
			} else
				AALERR("[SRAM] Error when get curve_apb in %s\n", caller);
		} else {
			if (atomic_read(curve_sram_apb) == 0) {
				curve_apb = 1;
				curve_int = 0;
			} else if (atomic_read(curve_sram_apb) == 1) {
				curve_apb = 0;
				curve_int = 1;
			} else
				AALERR("[SRAM] Error when get curve_apb in %s\n", caller);
		}
	}
	SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_HIST_SRAM_EN);
	SET_VAL_MASK(sram_cfg, sram_mask, hist_apb, REG_FORCE_HIST_SRAM_APB);
	SET_VAL_MASK(sram_cfg, sram_mask, hist_int, REG_FORCE_HIST_SRAM_INT);
	if (aal_dre3_curve_sram) {
		SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_CURVE_SRAM_EN);
		SET_VAL_MASK(sram_cfg, sram_mask, curve_apb, REG_FORCE_CURVE_SRAM_APB);
		SET_VAL_MASK(sram_cfg, sram_mask, curve_int, REG_FORCE_CURVE_SRAM_INT);
	}
	AALFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x comp_id[%d] in %s\n",
		hist_apb, hist_int, sram_cfg, comp->id, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_SRAM_CFG, sram_cfg, sram_mask);
}

void disp_aal_flip_curve_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	u32 sram_cfg = 0, sram_mask = 0;
	u32 curve_apb = 0, curve_int = 0;
	phys_addr_t dre3_pa = disp_aal_dre3_pa(comp);
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool aal_dre3_curve_sram = aal_data->data->aal_dre3_curve_sram;
	atomic_t *curve_sram_apb = &aal_data->force_curve_sram_apb;

	if (!aal_data->primary_data->aal_fo->mtk_dre30_support || !aal_dre3_curve_sram)
		return;

	if (atomic_cmpxchg(curve_sram_apb, 0, 1) == 0) {
		curve_apb = 0;
		curve_int = 1;
	} else if (atomic_cmpxchg(curve_sram_apb, 1, 0) == 1) {
		curve_apb = 1;
		curve_int = 0;
	} else
		AALERR("[SRAM] Error when get curve_apb in %s\n", caller);
	SET_VAL_MASK(sram_cfg, sram_mask, 1, REG_FORCE_CURVE_SRAM_EN);
	SET_VAL_MASK(sram_cfg, sram_mask, curve_apb, REG_FORCE_CURVE_SRAM_APB);
	SET_VAL_MASK(sram_cfg, sram_mask, curve_int, REG_FORCE_CURVE_SRAM_INT);
	AALFLOW_LOG("[SRAM] 0x%08x comp_id[%d] in %s\n", sram_cfg, comp->id, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_SRAM_CFG, sram_cfg, sram_mask);
}

static void disp_aal_sof_handle_by_cpu(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	int pm_ret = 0;
	int first_frame;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal *aal1_data = NULL;

	CRTC_MMP_EVENT_START(0, aal_sof_thread, 0, 0);
	mtk_drm_trace_begin("aal_sof_thread");
	AALIRQ_LOG("[SRAM] dre_config(%d) in SOF\n",
			atomic_read(&aal_data->dre_config));
	pm_ret = mtk_vidle_pq_power_get(__func__);
	if (pm_ret < 0) {
		CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 0xe);
		mtk_drm_trace_end();
		AALERR("pm get error %d\n",pm_ret);
		return;
	}
	mutex_lock(&aal_data->primary_data->clk_lock);
	first_frame = atomic_read(&aal_data->first_frame);
	if (atomic_read(&aal_data->is_clock_on) != 1) {
		AALIRQ_LOG("clock is off\n");
		mutex_unlock(&aal_data->primary_data->clk_lock);
		if (!pm_ret)
			mtk_vidle_pq_power_put(__func__);
		return;
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		aal1_data = comp_to_aal(aal_data->companion);
		first_frame = first_frame && atomic_read(&aal1_data->first_frame);
		if (atomic_read(&aal1_data->is_clock_on) != 1) {
			AALIRQ_LOG("aal1 clock is off\n");
			mutex_unlock(&aal_data->primary_data->clk_lock);
			if (!pm_ret)
				mtk_vidle_pq_power_put(__func__);
			return;
		}
	}
	ret = disp_aal_update_dre3_sram(comp, true);
	mutex_unlock(&aal_data->primary_data->clk_lock);
	if (!pm_ret)
		mtk_vidle_pq_power_put(__func__);
	CRTC_MMP_MARK(0, aal_sof_thread, 0, 1);

	DDP_MUTEX_LOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
	if (aal_data->primary_data->dre30_enabled && !ret &&
	    (first_frame == 1 || atomic_read(&aal_data->primary_data->event_en) == 1))
		mtk_crtc_user_cmd_impl(&comp->mtk_crtc->base, comp, FLIP_SRAM, NULL, false);

	if (atomic_read(&aal_data->primary_data->dre30_write) == 1 ||
	    (first_frame == 1 && aal_data->primary_data->dre30_enabled)) {
		mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
		atomic_set(&aal_data->primary_data->dre30_write, 0);
	}
	DDP_MUTEX_UNLOCK(&comp->mtk_crtc->lock, __func__, __LINE__);
	if (first_frame == 1) {
		atomic_set(&aal_data->first_frame, 0);
		if (comp->mtk_crtc->is_dual_pipe)
			atomic_set(&aal1_data->first_frame, 0);
		CRTC_MMP_MARK(0, aal_sof_thread, 0, 3);
	}
	CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 4);
	mtk_drm_trace_end();
}

static void disp_aal_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	if (atomic_read(&aal_data->primary_data->event_en) == 0 ||
			atomic_read(&aal_data->primary_data->should_stop)) {
		atomic_set(&aal_data->primary_data->eof_irq_skip, 1);
		AALIRQ_LOG("%s, skip irq\n", __func__);
		return;
	}
	atomic_set(&aal_data->primary_data->eof_irq_skip, 0);
	AALIRQ_LOG("dre3 %d %d, hist %d:%d relay %d stop %d 0x%x crtc %d\n",
		aal_data->primary_data->aal_fo->mtk_dre30_support,
		aal_data->primary_data->dre30_enabled,
		atomic_read(&aal_data->hist_available),
		atomic_read(&aal_data->dre20_hist_is_ready),
		aal_data->primary_data->relay_state != 0 ? 1 : 0,
		atomic_read(&aal_data->primary_data->should_stop),
		atomic_read(&aal_data->primary_data->change_to_dre30),
		mtk_crtc->enabled);

	if (!aal_data->primary_data->aal_fo->mtk_dre30_support || !aal_data->primary_data->dre30_enabled) {
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

			if (atomic_read(&aal_data->dre20_hist_is_ready) &&
				atomic_read(&aal1_data->dre20_hist_is_ready)) {
				atomic_set(&aal_data->hist_available, 1);
				atomic_set(&aal1_data->hist_available, 1);
				atomic_set(&aal_data->hw_hist_ready, 0);
				atomic_set(&aal1_data->hw_hist_ready, 0);
				AALIRQ_LOG("wake up dre2\n");
				wake_up_interruptible(&aal_data->primary_data->hist_wq);
			}
		} else {
			if (atomic_read(&aal_data->dre20_hist_is_ready)) {
				atomic_set(&aal_data->hist_available, 1);
				atomic_set(&aal_data->hw_hist_ready, 0);
				AALIRQ_LOG("wake up dre2\n");
				wake_up_interruptible(&aal_data->primary_data->hist_wq);
			}
		}
		return;
	}

	if ((aal_data->primary_data->relay_state != 0) &&
		!pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS])
		return;
	if (atomic_read(&aal_data->primary_data->change_to_dre30) != 0x3)
		return;

	if (!atomic_read(&aal_data->primary_data->sof_irq_available)) {
		atomic_set(&aal_data->primary_data->sof_irq_available, 1);
		AALIRQ_LOG("wake up dre3\n");
		wake_up_interruptible(&aal_data->primary_data->sof_irq_wq);
	}
}

static int disp_aal_sof_kthread(void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	int ret;

	while (!kthread_should_stop()) {
		if (atomic_read(&aal_data->primary_data->sof_irq_available) == 0) {
			AALFLOW_LOG("wait_event_interruptible\n");
			ret = wait_event_interruptible(aal_data->primary_data->sof_irq_wq,
					atomic_read(&aal_data->primary_data->sof_irq_available) == 1);
			if (ret == 0) {
				AALFLOW_LOG("sof_irq_available = 1, waken up, ret = %d\n", ret);
				disp_aal_sof_handle_by_cpu(comp);
			}
		} else
			AALFLOW_LOG("sof_irq_available = 0\n");
		atomic_set(&aal_data->primary_data->sof_irq_available, 0);
	}

	return 0;
}

static unsigned int disp_aal_read_clear_irq(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;

	/* Check current irq status */
	intsta = readl(comp->regs + DISP_AAL_INTSTA);
	writel(intsta & ~0x3, comp->regs + DISP_AAL_INTSTA);
	AALIRQ_LOG("AAL Module compID:%d\n", comp->id);
	return intsta;
}

void disp_aal_on_end_of_frame(struct mtk_ddp_comp *comp, unsigned int status)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	//For 120Hz rotation issue
	ktime_get_ts64(&aal_data->primary_data->start);

	atomic_set(&aal_data->hw_hist_ready, 1);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support
			&& aal_data->primary_data->dre30_enabled)
		disp_aal_read_ghist(comp);
	else
		disp_aal_single_pipe_hist_update(comp, status);

	AALIRQ_LOG("[SRAM] clean dre_config in (EOF)  comp->id = %d dre_en %d\n", comp->id,
		aal_data->primary_data->dre30_enabled);

	atomic_set(&aal_data->dre_config, 0);
}

static irqreturn_t disp_aal_irq_handler(int irq, void *dev_id)
{
	unsigned int status0 = 0, status1 = 0;
	struct mtk_disp_aal *aal = dev_id;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_ddp_comp *comp1 = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	irqreturn_t ret = IRQ_NONE;

	if (IS_ERR_OR_NULL(aal))
		return IRQ_NONE;

	comp = &aal->ddp_comp;
	if (mtk_drm_top_clk_isr_get(comp) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	mtk_crtc = aal->ddp_comp.mtk_crtc;
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	comp1 = aal->companion;
	status0 = disp_aal_read_clear_irq(comp);
	if (mtk_crtc->is_dual_pipe && comp1)
		status1 = disp_aal_read_clear_irq(comp1);

	AALIRQ_LOG("irq, val:0x%x,0x%x\n", status0, status1);

	if (atomic_read(&aal->primary_data->eof_irq_skip) == 1) {
		AALIRQ_LOG("%s, skip irq\n", __func__);
		ret = IRQ_HANDLED;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, status0);
	DRM_MMP_MARK(aal0, status0, status1);

	disp_aal_on_end_of_frame(comp, status0);
	if (mtk_crtc->is_dual_pipe && comp1)
		disp_aal_on_end_of_frame(comp1, status1);

	DRM_MMP_MARK(aal0, status0, 1);

	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put(comp);

	return ret;
}

static void disp_aal_init_dre3_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	const struct DISP_AAL_INITREG *init_regs)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	phys_addr_t dre3_pa = disp_aal_dre3_pa(comp);
	int dre_alg_mode = 1;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	uint32_t dre_mapping_00;

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761)
		dre_mapping_00 = GKI_DISP_AAL_DRE_MAPPING_00;
	else
		dre_mapping_00 = DISP_AAL_DRE_MAPPING_00;

	AALFLOW_LOG("start, bitShift: %d  compId%d\n", aal_data->data->bitShift, comp->id);

	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + dre_mapping_00,
		(init_regs->dre_map_bypass << 4), 1 << 4);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_01,
		(init_regs->dre_blk_y_num << 5) | init_regs->dre_blk_x_num,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_02,
		(init_regs->dre_blk_height << (aal_data->data->bitShift)) |
		init_regs->dre_blk_width, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_04,
		(init_regs->dre_flat_length_slope << 13) |
		init_regs->dre_flat_length_th, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_00,
		(init_regs->dre_s_upper << 24) |
		(init_regs->dre_s_lower << 16) |
		(init_regs->dre_y_upper << 8) | init_regs->dre_y_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_01,
		(init_regs->dre_h_slope << 24) |
		(init_regs->dre_s_slope << 20) |
		(init_regs->dre_y_slope << 16) |
		(init_regs->dre_h_upper << 8) | init_regs->dre_h_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_ALPHA_BLEND_00,
		(init_regs->dre_y_alpha_shift_bit << 25) |
		(init_regs->dre_y_alpha_base << 16) |
		(init_regs->dre_x_alpha_shift_bit << 9) |
		init_regs->dre_x_alpha_base, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_05,
		init_regs->dre_blk_area, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_06,
		init_regs->dre_blk_area_min, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_DRE_BLOCK_INFO_07,
		(init_regs->height - 1) << (aal_data->data->bitShift), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_SRAM_CFG,
		init_regs->hist_bin_type, 0x1);
	//TODO DMDP_AAL_TILE_00 DMDP_AAL_TILE_02 may have timing issue with pu
	if (comp->mtk_crtc->is_dual_pipe) {
		if (!aal_data->is_right_pipe) {
			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_00,
				(0x0 << 23) | (0x1 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre0_blk_num_x_end << 15) |
				(init_regs->dre0_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);

			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_01,
				(init_regs->dre0_blk_cnt_x_end << (aal_data->data->bitShift)) |
				init_regs->dre0_blk_cnt_x_start, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(init_regs->dre0_act_win_x_end << (aal_data->data->bitShift)) |
				init_regs->dre0_act_win_x_start, ~0);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_00,
				(0x1 << 23) | (0x0 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre1_blk_num_x_end << 15) |
				(init_regs->dre1_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);

			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_01,
				(init_regs->dre1_blk_cnt_x_end << (aal_data->data->bitShift)) |
				init_regs->dre1_blk_cnt_x_start << 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(init_regs->dre1_act_win_x_end << (aal_data->data->bitShift)) |
				init_regs->dre1_act_win_x_start, ~0);
		}
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_00,
			(0x1 << 21) | (0x1 << 20) |
			(init_regs->blk_num_x_end << 15) |
			(init_regs->blk_num_x_start << 10) |
			(init_regs->blk_num_y_end << 5) |
			init_regs->blk_num_y_start, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_TILE_01,
			(init_regs->blk_cnt_x_end << (aal_data->data->bitShift)) |
			init_regs->blk_cnt_x_start << 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(init_regs->act_win_x_end << (aal_data->data->bitShift)) |
			init_regs->act_win_x_start, ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_TILE_02,
		(init_regs->blk_cnt_y_end << (aal_data->data->bitShift)) |
		init_regs->blk_cnt_y_start, ~0);

	disp_mdp_aal_init_data_update(aal_data->comp_dmdp_aal, init_regs);
	/* Change to Local DRE version */
	if (debug_bypass_alg_mode)
		dre_alg_mode = 0;
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DMDP_AAL_CFG_MAIN,
		(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));
}

#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20) | ((v1) << 10) | (v0))
static int disp_aal_write_init_regs(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle)
{
	int ret = -EFAULT;
	int i = 0, j = 0;
	int *gain = NULL;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	uint32_t cabc_gainlmt_tbl_00, dre_mapping_00;

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761) {
		cabc_gainlmt_tbl_00 = GKI_DISP_AAL_CABC_GAINLMT_TBL_00;
		dre_mapping_00 = GKI_DISP_AAL_DRE_MAPPING_00;
	} else {
		cabc_gainlmt_tbl_00 = DISP_AAL_CABC_GAINLMT_TBL_00;
		dre_mapping_00 = DISP_AAL_DRE_MAPPING_00;
	}
	if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1) {
		struct DISP_AAL_INITREG *init_regs = &aal_data->primary_data->init_regs;

		gain = init_regs->cabc_gainlmt;
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + dre_mapping_00,
			(init_regs->dre_map_bypass << 4), 1 << 4);

		for (i = 0; i <= 10; i++) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(cabc_gainlmt_tbl_00, i),
				CABC_GAINLMT(gain[j], gain[j + 1], gain[j + 2]), ~0);
			j += 3;
		}

		if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
			disp_aal_init_dre3_reg(comp, handle, init_regs);
			atomic_or(0x1, &aal_data->primary_data->change_to_dre30);
		}
		AALFLOW_LOG("init done\n");
		ret = 0;
	}

	return ret;
}

static int disp_aal_set_init_reg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct DISP_AAL_INITREG *user_regs)
{
	int ret = -EFAULT;
	struct DISP_AAL_INITREG *init_regs;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (!aal_data->primary_data->aal_fo->mtk_aal_support)
		return ret;

	init_regs = &aal_data->primary_data->init_regs;

	memcpy(init_regs, user_regs, sizeof(*init_regs));
	if (debug_dump_init_reg)
		disp_aal_dump_init_reg(init_regs);

	atomic_set(&aal_data->primary_data->is_init_regs_valid, 1);

	AALFLOW_LOG("Set init reg: %lu, dre_map_bypass: %d\n",
		sizeof(*init_regs), init_regs->dre_map_bypass);
	ret = disp_aal_write_init_regs(comp, handle);
	if (comp->mtk_crtc->is_dual_pipe)
		ret = disp_aal_write_init_regs(aal_data->companion, handle);

	AALFLOW_LOG("ret = %d\n", ret);

	return ret;
}

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | \
	((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))
static int disp_aal_write_dre_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	const int *gain;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	uint32_t dre_mapping_00;

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761)
		dre_mapping_00 = GKI_DISP_AAL_DRE_MAPPING_00;
	else
		dre_mapping_00 = DISP_AAL_DRE_MAPPING_00;

	gain = param->DREGainFltStatus;
	if (aal_data->primary_data->ess20_spect_param.flag & 0x3)
		CRTC_MMP_MARK(0, aal_ess20_curve, comp->id, 0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + dre_mapping_00,
		(aal_data->primary_data->init_regs.dre_map_bypass << 4), 1 << 4);

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(0),
		    DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(1),
			DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(2),
			DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(3),
			DRE_REG_2(gain[6], 0, gain[7], 12), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(4),
			DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(5),
			DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(6),
			DRE_REG_2(gain[12], 0, gain[13], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(7),
			DRE_REG_2(gain[14], 0, gain[15], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(8),
			DRE_REG_3(gain[16], 0, gain[17], 10, gain[18], 20), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(9),
			DRE_REG_3(gain[19], 0, gain[20], 10, gain[21], 19), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(10),
			DRE_REG_3(gain[22], 0, gain[23], 9, gain[24], 18), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE_11,
			DRE_REG_3(gain[25], 0, gain[26], 9, gain[27], 18), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE_12, gain[28], ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(0),
			DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(1),
			DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(2),
			DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(3),
			DRE_REG_2(gain[6], 0, gain[7], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(4),
			DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(5),
			DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(6),
			DRE_REG_3(gain[12], 0, gain[13], 11, gain[14], 22), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(7),
			DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(8),
			DRE_REG_3(gain[18], 0, gain[19], 10, gain[20], 20), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(9),
			DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(10),
			DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
		/* Write dre curve to different register */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(11),
			DRE_REG_2(gain[27], 0, gain[28], 9), ~0);
	}

	return 0;
}

static int disp_aal_write_cabc_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	uint32_t cabc_gainlmt_tbl_00;

	AALFLOW_LOG("\n");
	if(aal_data->primary_data->aal_fo->mtk_cabc_no_support) {
		pr_notice("mtk_cabc_no_support is true\n");
		return 0;
	}
	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6765 ||
		priv->data->mmsys_id == MMSYS_MT6761)
		cabc_gainlmt_tbl_00 = GKI_DISP_AAL_CABC_GAINLMT_TBL_00;
	else
		cabc_gainlmt_tbl_00 = DISP_AAL_CABC_GAINLMT_TBL_00;

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CABC_00, 1 << 31, 1 << 31);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CABC_02, param->cabc_fltgain_force, 0x3ff);

	gain = param->cabc_gainlmt;
	for (i = 0; i <= 10; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(cabc_gainlmt_tbl_00, i),
			CABC_GAINLMT(gain[0], gain[1], gain[2]), ~0);
		gain += 3;
	}

	return 0;
}

static int disp_aal_set_dre3_curve(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct DISP_DRE30_PARAM dre30_gain;

	AALFLOW_LOG("\n");
	if (atomic_read(&aal_data->primary_data->change_to_dre30) == 0x3) {
		if (copy_from_user(&dre30_gain, (struct DISP_DRE30_PARAM *)param->dre30_gain,
				    sizeof(struct DISP_DRE30_PARAM)) == 0) {
			mutex_lock(&aal_data->primary_data->config_lock);
			memcpy(&aal_data->primary_data->dre30_gain, &dre30_gain, sizeof(struct DISP_DRE30_PARAM));
			mutex_unlock(&aal_data->primary_data->config_lock);
		} else
			return -1;
	}

	return 0;
}

int disp_aal_set_param(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		struct DISP_AAL_PARAM *param)
{
	int ret = 0;
	u64 time_use = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (debug_dump_input_param)
		disp_aal_dump_param(&aal_data->primary_data->aal_param);
	//For 120Hz rotation issue
	ktime_get_ts64(&aal_data->primary_data->end);
	time_use = (aal_data->primary_data->end.tv_sec
		- aal_data->primary_data->start.tv_sec) * 1000000
		+ (aal_data->primary_data->end.tv_nsec
		- aal_data->primary_data->start.tv_nsec) / NSEC_PER_USEC;
	//pr_notice("set_param time_use is %lu us\n",time_use);
	// tbd. to be fixd
	if (time_use < 260) {
		// Workaround for 120hz rotation,do not let
		//aal command too fast,else it will merged with
		//DISP commmand and caused trigger loop clear EOF
		//before config loop.The DSI EOF has 100 us later then
		//RDMA EOF,and the worst DISP config time is 153us,
		//so if intervel less than 260 should delay
		usleep_range(260-time_use, 270-time_use);
	}

	if (aal_data->primary_data->aal_fo->mtk_dre30_support && aal_data->primary_data->dre30_enabled)
		ret = disp_aal_set_dre3_curve(comp, handle, param);
	disp_aal_write_dre_to_reg(comp, handle, &aal_data->primary_data->aal_param);
	disp_aal_write_cabc_to_reg(comp, handle, &aal_data->primary_data->aal_param);
	if (comp->mtk_crtc->is_dual_pipe) {
		disp_aal_write_dre_to_reg(aal_data->companion, handle, &aal_data->primary_data->aal_param);
		disp_aal_write_cabc_to_reg(aal_data->companion, handle, &aal_data->primary_data->aal_param);
	}
	atomic_set(&aal_data->primary_data->dre30_write, 1);
	aal_data->primary_data->aal_param_valid = true;
	if (aal_data->primary_data->aal_fo->mtk_dre30_support)
		disp_mdp_aal_set_valid(aal_data->comp_dmdp_aal, true);
	return ret;
}

static int disp_aal_set_clarity_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_CLARITY_REG *clarity_regs)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *comp_tdshp = aal_data->comp_tdshp;
	phys_addr_t dre3_pa = disp_aal_dre3_pa(comp);

	if (clarity_regs == NULL)
		return -1;

	// aal clarity set registers
	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 1);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATEAL,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_impulse_noise_en << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_detect_en << 8 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_slope << 4 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_flt_en << 1 |
		clarity_regs->mdp_aal_clarity_regs.have_bilateral_filter << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_CFG_MAIN,
		clarity_regs->mdp_aal_clarity_regs.dre_output_mode, 0x1 << 5);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_Blending_00,
		(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_D << 27 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_C << 23 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_B << 19 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_A << 15 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_wgt_gain << 11 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt_mode << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt << 4 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_en << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_Blending_01,
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_size_blending_wgt << 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT1_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT1_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT1_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT2_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT2_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_CUST_FLT2_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_FLT_CONFIG,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_wgt << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_slope << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_gain << 3 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_gain << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_FREQ_BLENDING,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_out_wgt << 20 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_out_wgt << 15 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_out_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_out_wgt << 5 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_out_wgt << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_DRE_BILATERAL_REGION_PROTECTION,
	(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_input_shift_bit << 25 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_D << 21 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_C << 13 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_B << 5 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_A << 1 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_region_protection_en << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.mid_negative_offset << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_positive_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_negative_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_positive_offset << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_D,
		(clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_H,
		(clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_L,
		(clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_M,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_SIZE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_SMALL_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_MEDIUM_BIG << 6 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_BIG_HUGE << 12), 0x3FFFF);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_HUGE << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_HUGE << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_HUGE << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_HUGE << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_BIG,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_BIG << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_BIG << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_BIG << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_BIG << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_MEDIUM << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_MEDIUM << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_MEDIUM << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_SMALL << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_SMALL << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_SMALL << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_SMALL << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_CFG,
		(clarity_regs->disp_tdshp_clarity_regs.FREQ_EXTRACT_ENHANCE << 12 |
		clarity_regs->disp_tdshp_clarity_regs.FILTER_HIST_EN << 16),
		((0x1 << 16) | (0x1 << 12)));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_H_weighting << 4 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_weighting << 8 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_weighting << 12), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_final_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_final_weighting << 5 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_final_weighting << 10 |
		clarity_regs->disp_tdshp_clarity_regs.freq_WH_final_weighting << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + LUMA_CHROMA_PARAMETER,
		(clarity_regs->disp_tdshp_clarity_regs.luma_low_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.luma_low_index << 3 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_index << 8 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_gain << 13 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_gain << 16 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_index << 19 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_index << 24 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_gain << 29), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH,
		(clarity_regs->disp_tdshp_clarity_regs.Luma_adaptive_mode << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_adaptive_mode << 1 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARAMETER << 2 |
		clarity_regs->disp_tdshp_clarity_regs.Luma_shift << 12 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_shift << 15),
		(0x7 << 15) | (0x7 << 12) | (0x1F << 2) | (1 << 1) | (1 << 0));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + CLASS_0_2_GAIN,
		(clarity_regs->disp_tdshp_clarity_regs.class_0_positive_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.class_0_negative_gain << 5),
		0x3FF);

	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 2);

	return ret;
}

static int disp_aal_act_eventctl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int ret = 0;
	unsigned int events = *(unsigned int *)data;
	int enable = !!(events & AAL_EVENT_EN);
	int bypass = !!(events & AAL_EVENT_FUNC_OFF);
	int unbypass = !!(events & AAL_EVENT_FUNC_ON);
	int delay_trigger;

	AALFLOW_LOG("0x%x\n", events);
	CRTC_MMP_MARK(0, aal_event_ctl, events, 0);
	delay_trigger = atomic_read(&aal_data->primary_data->force_delay_check_trig);
	if (priv->data->mmsys_id == MMSYS_MT6768 || priv->data->mmsys_id == MMSYS_MT6761) {
		if(enable && (enable != aal_data->primary_data->pre_enable))
			mtk_crtc_check_trigger(comp->mtk_crtc, delay_trigger, true);
	} else {
		if(enable)
			mtk_crtc_check_trigger(comp->mtk_crtc, delay_trigger, true);
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	disp_aal_set_interrupt(comp, enable, NULL);
#endif
	if (atomic_read(&aal_data->primary_data->force_event_en))
		enable = 1;
	atomic_set(&aal_data->primary_data->event_en, enable);
	aal_data->primary_data->pre_enable = enable;

	if (bypass) {
		disp_aal_relay_control(comp, true);
		CRTC_MMP_MARK(0, aal_event_ctl, events, 1);
	}
	if (unbypass) {
		disp_aal_relay_control(comp, false);
		CRTC_MMP_MARK(0, aal_event_ctl, events, 2);
	}

	return ret;
}

static void disp_aal_wait_hist(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

		if ((atomic_read(&aal_data->hist_available) == 0) ||
				(atomic_read(&aal1_data->hist_available) == 0)) {
			ret = wait_event_interruptible(aal_data->primary_data->hist_wq,
					(atomic_read(&aal_data->hist_available) == 1) &&
					(atomic_read(&aal1_data->hist_available) == 1) &&
					comp->mtk_crtc->enabled &&
					!atomic_read(&aal_data->primary_data->should_stop));
			if (ret == -ERESTARTSYS)
				DDPMSG("%s: interrupted unexpected by signal\n", __func__);
		}
		AALFLOW_LOG("aal0 and aal1 hist_available = 1, waken up, ret = %d\n", ret);
	} else if (atomic_read(&aal_data->hist_available) == 0) {
		AALFLOW_LOG("comp_id:%d wait_event_interruptible\n", comp->id);
		ret = wait_event_interruptible(aal_data->primary_data->hist_wq,
				atomic_read(&aal_data->hist_available) == 1 &&
				comp->mtk_crtc->enabled &&
				!atomic_read(&aal_data->primary_data->should_stop));
		if (ret == -ERESTARTSYS)
			DDPMSG("%s: interrupted unexpected by signal\n", __func__);
		AALFLOW_LOG("comp_id:%d hist_available = 1, waken up, ret = %d\n", comp->id, ret);
	} else
		AALFLOW_LOG("comp_id:%d hist_available = 0\n", comp->id);
}

static int disp_aal_copy_hist_to_user(struct mtk_ddp_comp *comp,
	struct DISP_AAL_HIST *hist)
{
	unsigned long flags;
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct DISP_DRE30_HIST dre30_hist;

	if (hist == NULL) {
		AALERR("%s DstHist is NULL\n", __func__);
		return -1;
	}

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);
	if (aal_data->primary_data->aal_fo->mtk_dre30_support && aal_data->primary_data->dre30_enabled)
		memcpy(&dre30_hist, &aal_data->primary_data->dre30_hist, sizeof(struct DISP_DRE30_HIST));
	aal_data->primary_data->hist.panel_type = atomic_read(&aal_data->primary_data->panel_type);
	aal_data->primary_data->hist.essStrengthIndex = aal_data->primary_data->ess_level;
	aal_data->primary_data->hist.ess_enable = aal_data->primary_data->ess_en;
	aal_data->primary_data->hist.dre_enable = aal_data->primary_data->dre_en;
	aal_data->primary_data->hist.fps = aal_data->primary_data->fps;
	AALFLOW_LOG("%s fps:%d\n", __func__, aal_data->primary_data->fps);
	if (comp->mtk_crtc->is_dual_pipe) {
		aal_data->primary_data->hist.pipeLineNum = 2;
		aal_data->primary_data->hist.srcWidth = aal_data->primary_data->dual_size.width;
		aal_data->primary_data->hist.srcHeight = aal_data->primary_data->dual_size.height;
	} else {
		aal_data->primary_data->hist.pipeLineNum = 1;
		aal_data->primary_data->hist.srcWidth = aal_data->primary_data->size.width;
		aal_data->primary_data->hist.srcHeight = aal_data->primary_data->size.height;
	}

	// set AAL_SERVICE_FORCE_UPDATE avoid backlight drop on AALService
	if (atomic_read(&aal_data->primary_data->hal_force_update) == 1) {
		aal_data->primary_data->hist.serviceFlags |= AAL_SERVICE_FORCE_UPDATE;
		atomic_set(&aal_data->primary_data->hal_force_update, 0);
	}

	// dre30_hist ptr should be always from userspace, valid or not
	aal_data->primary_data->hist.dre30_hist = hist->dre30_hist;
	memcpy(hist, &aal_data->primary_data->hist, sizeof(aal_data->primary_data->hist));
	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support && aal_data->primary_data->dre30_enabled)
		ret = copy_to_user((void *)hist->dre30_hist, &dre30_hist, sizeof(struct DISP_DRE30_HIST));
	aal_data->primary_data->hist.serviceFlags = 0;
	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

		atomic_set(&aal1_data->hist_available, 0);
		atomic_set(&aal1_data->dre20_hist_is_ready, 0);
	}
	atomic_set(&aal_data->primary_data->force_event_en, 0);

	return ret;
}

int disp_aal_act_get_hist(struct mtk_ddp_comp *comp, void *data)
{
	disp_aal_wait_hist(comp);
	if (disp_aal_copy_hist_to_user(comp, (struct DISP_AAL_HIST *) data) < 0)
		return -EFAULT;
	if (debug_dump_aal_hist)
		disp_aal_dump_ghist(comp, data);

	return 0;
}

int disp_aal_act_set_ess20_spect_param(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISP_AAL_ESS20_SPECT_PARAM *param = (struct DISP_AAL_ESS20_SPECT_PARAM *) data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	memcpy(&aal_data->primary_data->ess20_spect_param, param, sizeof(*param));
	AALAPI_LOG("[aal_kernel]ELVSSPN = %d, flag = %d\n",
		aal_data->primary_data->ess20_spect_param.ELVSSPN,
		aal_data->primary_data->ess20_spect_param.flag);

	return ret;
}

int disp_aal_act_init_dre30(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct DISP_DRE30_INIT *init_dre3;

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		AALFLOW_LOG("\n");
		atomic_or(0x2, &aal_data->primary_data->change_to_dre30);
	} else
		AALFLOW_LOG("DRE30 not support\n");

	return 0;
}

static int disp_aal_wait_size(struct mtk_disp_aal *aal_data, unsigned long timeout)
{
	int ret = 0;

	if (aal_data->primary_data->get_size_available == false) {
		ret = wait_event_interruptible(aal_data->primary_data->size_wq,
		aal_data->primary_data->get_size_available == true);
		pr_notice("size_available = 1, Waken up, ret = %d\n",
			ret);
	} else {
		/* If g_aal_get_size_available is already set, */
		/* means AALService was delayed */
		pr_notice("size_available = 0\n");
	}
	return ret;
}

int disp_aal_act_get_size(struct mtk_ddp_comp *comp, void *data)
{
	struct DISP_AAL_DISPLAY_SIZE *dst =
		(struct DISP_AAL_DISPLAY_SIZE *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	disp_aal_wait_size(aal_data, 60);

	if (comp == NULL || comp->mtk_crtc == NULL) {
		AALERR("%s null pointer!\n", __func__);
		return -1;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		memcpy(dst, &aal_data->primary_data->dual_size,
				sizeof(aal_data->primary_data->dual_size));
	else
		memcpy(dst, &aal_data->primary_data->size, sizeof(aal_data->primary_data->size));

	return 0;
}

int disp_aal_act_set_trigger_state(struct mtk_ddp_comp *comp, void *data)
{
	unsigned long flags;
	unsigned int dre3EnState;
	struct DISP_AAL_TRIG_STATE *trigger_state = (struct DISP_AAL_TRIG_STATE *)data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal_primary *pdata = aal_data->primary_data;

	dre3EnState = trigger_state->dre3_en_state;

	AALFLOW_LOG("compid %d, dre3EnState: 0x%x, trigger_state: %d, ali: %d, aliThres: %d\n",
			comp->id, dre3EnState,
			trigger_state->dre_frm_trigger,
			trigger_state->curAli, trigger_state->aliThreshold);

	if (dre3EnState & 0x2) {
		AALFLOW_LOG("dre change to open!\n");

		spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);
		if ((pdata->aal_fo->mtk_dre30_support)
			&& (!pdata->dre30_enabled)) {
			pdata->prv_dre30_enabled = pdata->dre30_enabled;
			pdata->dre30_enabled = true;
		}
		spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

		if (!pdata->prv_dre30_enabled && pdata->dre30_enabled) {
			// need flip sram to get local histogram
			mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, FLIP_SRAM, NULL);
			pdata->prv_dre30_enabled = pdata->dre30_enabled;
		}

		trigger_state->dre3_krn_flag = pdata->dre30_enabled ? 1 : 0;
		return 0;
	}

	if (dre3EnState & 0x1) {
		//spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);
		if (trigger_state->dre_frm_trigger == 0) {
			if ((pdata->aal_fo->mtk_dre30_support)
				&& pdata->dre30_enabled) {
				pdata->dre30_enabled = false;
				mutex_lock(&aal_data->primary_data->config_lock);
#if IS_ENABLED(CONFIG_MTK_DISP_DEBUG)
				disp_aal_dre3_reset_to_linear(comp, 1);
#else
				disp_aal_dre3_reset_to_linear(comp, 0);
#endif
				mutex_unlock(&aal_data->primary_data->config_lock);
				AALFLOW_LOG("dre change to close!\n");
			}
		}

		trigger_state->dre3_krn_flag = pdata->dre30_enabled ? 1 : 0;
		//spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);
	}

	return 0;
}

int disp_aal_act_get_base_voltage(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISP_PANEL_BASE_VOLTAGE *dst_baseVoltage = (struct DISP_PANEL_BASE_VOLTAGE *)data;
	struct DISP_PANEL_BASE_VOLTAGE src_baseVoltage;
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);
	if (!output_comp) {
		DDPPR_ERR("%s:invalid output comp\n", __func__);
		return -EFAULT;
	}

	AALFLOW_LOG("get base_voltage\n");

	/* DSI_SEND_DDIC_CMD */
	if (output_comp) {
		ret = mtk_ddp_comp_io_cmd(output_comp, NULL,
			DSI_READ_ELVSS_BASE_VOLTAGE, &src_baseVoltage);
		if (ret < 0)
			DDPPR_ERR("%s:read elvss base voltage failed\n", __func__);
		else {
			memcpy(dst_baseVoltage, &src_baseVoltage, sizeof(struct DISP_PANEL_BASE_VOLTAGE));
			if (debug_dump_aal_hist)
				disp_aal_dump_ess_voltage_info(&src_baseVoltage);
		}
	}
	return ret;
}

static int disp_aal_cfg_clarity_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct DISP_MDP_AAL_CLARITY_REG *mdp_aal_clarity =
		&aal_data->primary_data->disp_clarity_regs->mdp_aal_clarity_regs;
	struct DISP_TDSHP_CLARITY_REG *tdshp_clarity =
		&aal_data->primary_data->disp_clarity_regs->disp_tdshp_clarity_regs;

	if (!aal_data->primary_data->disp_clarity_regs) {
		aal_data->primary_data->disp_clarity_regs =
				vmalloc(sizeof(struct DISP_CLARITY_REG));
		if (aal_data->primary_data->disp_clarity_regs == NULL) {
			DDPMSG("%s: no memory\n", __func__);
			return -EFAULT;
		}
	}

	if (data == NULL)
		return -EFAULT;

	mutex_lock(&aal_data->primary_data->config_lock);
	memcpy(aal_data->primary_data->disp_clarity_regs, (struct DISP_CLARITY_REG *)data,
		sizeof(struct DISP_CLARITY_REG));

	if (debug_dump_clarity_regs) {
		print_uint_array("aal_clarity_regs", (uint32_t *)mdp_aal_clarity,
				sizeof(struct DISP_MDP_AAL_CLARITY_REG) / 4, 10);
		print_uint_array("tdshp_clarity_regs", (uint32_t *)tdshp_clarity,
				sizeof(struct DISP_TDSHP_CLARITY_REG) / 4, 10);
	}

	CRTC_MMP_EVENT_START(0, clarity_set_regs, 0, 0);
	if (disp_aal_set_clarity_reg(comp, handle, aal_data->primary_data->disp_clarity_regs) < 0) {
		DDPMSG("[Pipe0] %s: clarity_set_reg failed\n", __func__);
		CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 4);
		mutex_unlock(&aal_data->primary_data->config_lock);

		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe && aal_data->companion) {
		if (disp_aal_set_clarity_reg(aal_data->companion, handle,
				aal_data->primary_data->disp_clarity_regs) < 0) {
			DDPMSG("[Pipe1] %s: clarity_set_reg failed\n", __func__);
			CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 5);
			mutex_unlock(&aal_data->primary_data->config_lock);

			return -EFAULT;
		}
	}
	CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 3);
	mutex_unlock(&aal_data->primary_data->config_lock);
	return 0;
}


int disp_gamma_set_silky_brightness_gain(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, struct DISP_AAL_PARAM *aal_param, unsigned int bl, void *param)
{
	struct DISP_AAL_ESS20_SPECT_PARAM *ess20_spect_param = param;
	struct mtk_ddp_comp *comp;
	unsigned int *gain = (unsigned int *)aal_param->silky_bright_gain;
	unsigned int silky_gain_range = (unsigned int)aal_param->silky_gain_range;
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_GAMMA, 0);
	if ((!comp) || (param == NULL)) {
		DDPINFO("[aal_kernel] comp is null\n");
		return -EFAULT;
	}

	CRTC_MMP_MARK(0, gamma_backlight, gain[gain_r], bl);
	mtk_leds_brightness_set(connector_id, bl, ess20_spect_param->ELVSSPN,
				ess20_spect_param->flag);
	DDPINFO("%s connector_id:%d, backlight:%d, flag:%d, elvss:%d\n",
		__func__, connector_id, bl, ess20_spect_param->flag,
		ess20_spect_param->ELVSSPN);

	disp_gamma_set_gain(comp, handle, gain, silky_gain_range);

	return 0;
}

static int disp_aal_cfg_set_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	int prev_backlight = 0;
	struct DISP_AAL_PARAM *param = (struct DISP_AAL_PARAM *) data;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;
	int prev_elvsspn = 0;

	if (debug_skip_set_param) {
		pr_notice("skip_set_param for debug\n");
		return ret;
	}
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);

	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	memcpy(&aal_data->primary_data->aal_param, param, sizeof(*param));

	prev_backlight = aal_data->primary_data->backlight_set;
	aal_data->primary_data->backlight_set = aal_data->primary_data->aal_param.FinalBacklight;
	prev_elvsspn = aal_data->primary_data->elvsspn_set;
	aal_data->primary_data->elvsspn_set = aal_data->primary_data->ess20_spect_param.ELVSSPN;

	ret = disp_aal_set_param(comp, handle, data);
	if (ret < 0)
		AALERR("SET_PARAM: fail\n");

	atomic_set(&aal_data->primary_data->allowPartial,
				aal_data->primary_data->aal_param.allowPartial);

	if (atomic_read(&aal_data->primary_data->backlight_notified) == 0)
		aal_data->primary_data->backlight_set = 0;

	if ((prev_backlight == aal_data->primary_data->backlight_set) ||
			(aal_data->primary_data->led_type == TYPE_ATOMIC))
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_BACKLIGHT_LEVEL));
	else
		aal_data->primary_data->ess20_spect_param.flag |= (1 << SET_BACKLIGHT_LEVEL);

	if (prev_elvsspn == aal_data->primary_data->elvsspn_set && prev_backlight)
		aal_data->primary_data->ess20_spect_param.flag &= (~(1 << SET_ELVSS_PN));

	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (aal_data->primary_data->aal_param.silky_bright_flag == 0) {
			AALAPI_LOG("connector_id:%d, bl:%d, silky_bright_flag:%d, ELVSSPN:%u, flag:%u\n",
				connector_id, aal_data->primary_data->backlight_set,
				aal_data->primary_data->aal_param.silky_bright_flag,
				aal_data->primary_data->ess20_spect_param.ELVSSPN,
				aal_data->primary_data->ess20_spect_param.flag);

			if(!aal_data->primary_data->ess20_spect_param.flag)
				mtk_leds_brightness_set(connector_id,
					aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
		}
	} else if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS]) {
		//if (pre_bl != cur_bl)
		AALAPI_LOG("gian:%u, backlight:%d, ELVSSPN:%u, flag:%u\n",
			aal_data->primary_data->aal_param.silky_bright_gain[0],
			aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);

		disp_gamma_set_silky_brightness_gain(mtk_crtc, handle,
			&aal_data->primary_data->aal_param,
			aal_data->primary_data->backlight_set,
			(void *)&aal_data->primary_data->ess20_spect_param);
	} else {
		AALAPI_LOG("connector_id:%d, pre_bl:%d, bl:%d, pn:%u, flag:%u\n",
			connector_id, prev_backlight, aal_data->primary_data->backlight_set,
			aal_data->primary_data->ess20_spect_param.ELVSSPN,
			aal_data->primary_data->ess20_spect_param.flag);

		if (aal_data->primary_data->ess20_spect_param.flag)
			mtk_leds_brightness_set(connector_id, aal_data->primary_data->backlight_set,
					aal_data->primary_data->ess20_spect_param.ELVSSPN,
					aal_data->primary_data->ess20_spect_param.flag);
	}

	return ret;
}

static int disp_aal_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	/* will only call left path */
	switch (cmd) {
	/* TYPE2 user_cmd alone */
	case PQ_AAL_INIT_REG:
		ret = disp_aal_set_init_reg(comp, handle, (struct DISP_AAL_INITREG *) data);
		break;
	case PQ_AAL_CLARITY_SET_REG:
		ret = disp_aal_cfg_clarity_set_reg(comp, handle, data, data_size);
		break;
	/* TYPE3 combine use_cmd & others*/
	case PQ_AAL_SET_PARAM:
		/* TODO move silky gamma to gamma */
		ret = disp_aal_cfg_set_param(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_aal_ioctl_transact(struct mtk_ddp_comp *comp,
		      unsigned int cmd, void *params, unsigned int size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_AAL_EVENTCTL:
		ret = disp_aal_act_eventctl(comp, params);
		break;
	case PQ_AAL_INIT_DRE30:
		ret = disp_aal_act_init_dre30(comp, params);
		break;
	case PQ_AAL_GET_HIST:
		ret = disp_aal_act_get_hist(comp, params);
		break;
	case PQ_AAL_GET_SIZE:
		ret = disp_aal_act_get_size(comp, params);
		break;
	case PQ_AAL_SET_ESS20_SPECT_PARAM:
		ret = disp_aal_act_set_ess20_spect_param(comp, params);
		break;
	case PQ_AAL_SET_TRIGGER_STATE:
		ret = disp_aal_act_set_trigger_state(comp, params);
		break;
	case PQ_AAL_GET_BASE_VOLTAGE:
		ret = disp_aal_act_get_base_voltage(comp, params);
		break;
	default:
		break;
	}
	return ret;
}

static int disp_aal_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s set partial update, height:%d, enable:%d\n",
			mtk_dump_comp_str(comp), partial_roi.height, enable);

	aal_data->set_partial_update = enable;
	aal_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : aal_data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (aal_data->set_partial_update == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_SIZE, aal_data->roi_height + overhead_v * 2, 0x0FFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_SIZE, aal_data->roi_height + overhead_v * 2, 0x0FFF);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_SIZE, full_height, 0x0FFF);
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_SIZE, full_height, 0x0FFF);
	}

	return 0;
}

static void disp_aal_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	uint32_t value = 0, mask = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	AALFLOW_LOG("+ comd id :%d\n", comp->id);
	if (cfg->source_bpc == 8)
		SET_VAL_MASK(value, mask, 1, FLD_AAL_8BIT_SWITCH);
	else if (cfg->source_bpc == 10)
		SET_VAL_MASK(value, mask, 0, FLD_AAL_8BIT_SWITCH);
	else
		DDPPR_ERR("%s invalid bpc %u\n", __func__, cfg->source_bpc);

	if (aal_data->primary_data->relay_state != 0) {
		AALFLOW_LOG("g_aal_force_relay\n");
		SET_VAL_MASK(value, mask, 1, FLD_RELAY_MODE);
	} else
		SET_VAL_MASK(value, mask, 0, FLD_RELAY_MODE);
	SET_VAL_MASK(value, mask, 1, FLD_AAL_ENGINE_EN);
	SET_VAL_MASK(value, mask, 1, FLD_AAL_HIST_EN);
	if (priv->data->mmsys_id != MMSYS_MT6768 &&
		 priv->data->mmsys_id != MMSYS_MT6765 &&
		 priv->data->mmsys_id != MMSYS_MT6761)
		SET_VAL_MASK(value, mask, 1, FLD_BLK_HIST_EN);
	SET_VAL_MASK(value, mask, 0x40, FLD_FRAME_DONE_DELAY);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CFG, value, mask);

	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);
	atomic_set(&aal_data->primary_data->sof_irq_available, 0);
	atomic_set(&aal_data->hw_hist_ready, 0);
}

static void disp_aal_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	atomic_set(&aal_data->hist_available, 0);
	atomic_set(&aal_data->dre20_hist_is_ready, 0);
	atomic_set(&aal_data->hw_hist_ready, 0);
	atomic_set(&aal_data->first_frame, 1);
	atomic_set(&aal_data->force_curve_sram_apb, 0);
	atomic_set(&aal_data->force_hist_apb, 0);
	atomic_set(&aal_data->dre_config, 0);

	if (!aal_data->is_right_pipe) {
		aal_data->comp_tdshp = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_TDSHP, 0);
		aal_data->comp_gamma = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_GAMMA, 0);
		aal_data->comp_color = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DISP_COLOR, 0);
		aal_data->comp_dmdp_aal = mtk_ddp_comp_sel_in_cur_crtc_path(comp->mtk_crtc,
									 MTK_DMDP_AAL, 0);
	} else {
		aal_data->comp_tdshp = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_TDSHP, 0);
		aal_data->comp_gamma = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_GAMMA, 0);
		aal_data->comp_color = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DISP_COLOR, 0);
		aal_data->comp_dmdp_aal = mtk_ddp_comp_sel_in_dual_pipe(comp->mtk_crtc,
								     MTK_DMDP_AAL, 0);
	}
}

static void disp_aal_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal *companion_aal_data = comp_to_aal(aal_data->companion);
	char thread_name[20] = "aal_sof_0";
	struct sched_param param = {.sched_priority = 85 };
	struct cpumask mask;

	if (aal_data->is_right_pipe) {
		kfree(aal_data->primary_data);
		aal_data->primary_data = NULL;
		aal_data->primary_data = companion_aal_data->primary_data;
		return;
	}

	init_waitqueue_head(&aal_data->primary_data->hist_wq);
	init_waitqueue_head(&aal_data->primary_data->sof_irq_wq);
	init_waitqueue_head(&aal_data->primary_data->size_wq);

	spin_lock_init(&aal_data->primary_data->hist_lock);

	mutex_init(&aal_data->primary_data->config_lock);
	mutex_init(&aal_data->primary_data->clk_lock);

	atomic_set(&(aal_data->primary_data->sof_irq_available), 0);
	atomic_set(&(aal_data->primary_data->is_init_regs_valid), 0);
	atomic_set(&(aal_data->primary_data->backlight_notified), 0);
	atomic_set(&(aal_data->primary_data->allowPartial), 0);
	atomic_set(&(aal_data->primary_data->should_stop), 0);
	atomic_set(&(aal_data->primary_data->dre30_write), 0);
	atomic_set(&(aal_data->primary_data->event_en), 1);
	atomic_set(&(aal_data->primary_data->force_event_en), 0);
	atomic_set(&(aal_data->primary_data->force_delay_check_trig), 0);
	atomic_set(&(aal_data->primary_data->dre_halt), 0);
	atomic_set(&(aal_data->primary_data->change_to_dre30), 0);
	atomic_set(&(aal_data->primary_data->panel_type), 0);
	atomic_set(&aal_data->primary_data->hal_force_update, 0);
	atomic_set(&aal_data->primary_data->func_flag, 1);

	memset(&(aal_data->primary_data->start), 0,
		sizeof(aal_data->primary_data->start));
	memset(&(aal_data->primary_data->end), 0,
		sizeof(aal_data->primary_data->end));
	memset(&(aal_data->primary_data->hist), 0,
		sizeof(aal_data->primary_data->hist));
	aal_data->primary_data->hist.backlight = -1;
	aal_data->primary_data->hist.essStrengthIndex = ESS_LEVEL_BY_CUSTOM_LIB;
	aal_data->primary_data->hist.ess_enable = ESS_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->hist.dre_enable = DRE_EN_BY_CUSTOM_LIB;

	memset(&(aal_data->primary_data->dre30_gain), 0,
		sizeof(aal_data->primary_data->dre30_gain));
	memset(&(aal_data->primary_data->dre30_hist), 0,
		sizeof(aal_data->primary_data->dre30_hist));
	memset(&(aal_data->primary_data->size), 0,
		sizeof(aal_data->primary_data->size));
	memset(&(aal_data->primary_data->dual_size), 0,
		sizeof(aal_data->primary_data->dual_size));
	memset(&(aal_data->primary_data->aal_param), 0,
		sizeof(aal_data->primary_data->aal_param));

	aal_data->primary_data->ess20_spect_param.ClarityGain = 0;
	aal_data->primary_data->ess20_spect_param.ELVSSPN = 0;
	aal_data->primary_data->ess20_spect_param.flag = 0x01<<SET_BACKLIGHT_LEVEL;

	aal_data->primary_data->sof_irq_event_task = NULL;
	aal_data->primary_data->refresh_wq = NULL;
	aal_data->primary_data->disp_clarity_regs = NULL;

	aal_data->primary_data->backlight_set = 0;
	aal_data->primary_data->get_size_available = 0;
	aal_data->primary_data->ess_level = ESS_LEVEL_BY_CUSTOM_LIB;
	aal_data->primary_data->dre_en = DRE_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->ess_en = ESS_EN_BY_CUSTOM_LIB;
	aal_data->primary_data->ess_level_cmd_id = 0;
	aal_data->primary_data->dre_en_cmd_id = 0;
	aal_data->primary_data->ess_en_cmd_id = 0;
	//aal_data->primary_data->led_type = TYPE_FILE;
	aal_data->primary_data->relay_state = 0x0 << PQ_FEATURE_DEFAULT;
	aal_data->primary_data->pre_enable = 0;

	aal_data->primary_data->refresh_wq = create_singlethread_workqueue("aal_refresh_trigger");
	INIT_WORK(&aal_data->primary_data->refresh_task.task, disp_aal_refresh_trigger);

	// start thread for aal sof
	sprintf(thread_name, "aal_sof_%d", comp->id);
	aal_data->primary_data->sof_irq_event_task = kthread_create(disp_aal_sof_kthread, comp, thread_name);

	cpumask_setall(&mask);
	cpumask_clear_cpu(0, &mask);
	set_cpus_allowed_ptr(aal_data->primary_data->sof_irq_event_task, &mask);
	if (sched_setscheduler(aal_data->primary_data->sof_irq_event_task, SCHED_RR, &param))
		pr_notice("aal_sof_irq_event_task setschedule fail");

	wake_up_process(aal_data->primary_data->sof_irq_event_task);
}

static int disp_aal_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	case FLIP_SRAM:
		disp_aal_flip_sram(comp, handle, __func__);
		if (comp->mtk_crtc->is_dual_pipe)
			disp_aal_flip_sram(aal_data->companion, handle, __func__);
		break;
	case FLIP_CURVE_SRAM:
	{
		disp_aal_flip_curve_sram(comp, handle, __func__);
		if (comp->mtk_crtc->is_dual_pipe)
			disp_aal_flip_curve_sram(aal_data->companion, handle, __func__);
	}
		break;
	default:
		AALERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

int disp_aal_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	uint32_t force_delay_trigger;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	switch (cmd) {
	case FRAME_DIRTY:
	{
		if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1) {
			disp_aal_set_interrupt(comp, 1, handle);
			atomic_set(&aal_data->primary_data->event_en, 1);
		}
	}
		break;
	case FORCE_TRIG_CTL:
	{
		force_delay_trigger = *(uint32_t *)params;
		atomic_set(&aal_data->primary_data->force_delay_check_trig, force_delay_trigger);
	}
		break;
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_aal *data = comp_to_aal(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_aal *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = disp_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_aal(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		disp_aal_data_init(comp);
		disp_aal_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion) {
			disp_aal_data_init(data->companion);
			disp_aal_primary_data_init(data->companion);
		}
	}
		break;
	case NOTIFY_MODE_SWITCH:
	{
		struct mtk_modeswitch_param *modeswitch_param = (struct mtk_modeswitch_param *)params;

		aal_data->primary_data->fps = modeswitch_param->fps;
		AALFLOW_LOG("AAL_FPS_CHG fps:%d\n", aal_data->primary_data->fps);
	}
		break;
	case IRQ_LEVEL_ALL:
	case IRQ_LEVEL_NORMAL:
	{
		if (atomic_read(&aal_data->primary_data->is_init_regs_valid) == 1)
			disp_aal_set_interrupt(comp, 1, handle);
	}
		break;
	case IRQ_LEVEL_IDLE:
	{
		disp_aal_set_interrupt(comp, 0, handle);
	}
		break;
	default:
		break;
	}
	AALFLOW_LOG("end\n");
	return 0;
}

static void disp_aal_bypass(struct mtk_ddp_comp *comp, int bypass,
	int caller, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_disp_aal_primary *primary_data = aal_data->primary_data;
	struct mtk_ddp_comp *companion = aal_data->companion;

	DDPINFO("%s: comp: %s, bypass: %d, caller: %d, relay_state: 0x%x\n",
		__func__, mtk_dump_comp_str(comp), bypass, caller, primary_data->relay_state);

	if (bypass == 1) {
		if (primary_data->relay_state == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CFG, AAL_RELAY_MODE, AAL_RELAY_MODE);
			if (comp->mtk_crtc->is_dual_pipe && companion)
				cmdq_pkt_write(handle, companion->cmdq_base,
					companion->regs_pa + DISP_AAL_CFG,
					AAL_RELAY_MODE, AAL_RELAY_MODE);
		}
		primary_data->relay_state |= (1 << caller);
	} else {
		if (primary_data->relay_state != 0) {
			primary_data->relay_state &= ~(1 << caller);
			if (primary_data->relay_state == 0) {
				// unrelay & Enable AAL Histogram
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_AAL_CFG,
					0x6, (0x3 << 1) | AAL_RELAY_MODE);
				if (comp->mtk_crtc->is_dual_pipe && companion) {
					// unrelay & Enable AAL Histogram
					cmdq_pkt_write(handle, companion->cmdq_base,
						companion->regs_pa + DISP_AAL_CFG,
						0x6, (0x3 << 1) | AAL_RELAY_MODE);
				}
			}
		}
	}
}

static void disp_aal_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	AALFLOW_LOG("\n");
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_EN, 0x1, ~0);
}

static void disp_aal_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_EN, 0, ~0);
}

static void disp_aal_prepare(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	mtk_ddp_comp_clk_prepare(comp);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk) {
			ret = clk_prepare(aal_data->dre3_hw.clk);
			if (ret < 0)
				DDPPR_ERR("failed to prepare dre3_hw.clk\n");
		}
	}
	AALFLOW_LOG("%s clk %d\n", mtk_dump_comp_str(comp), atomic_read(&aal_data->is_clock_on));
}

static void disp_aal_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	mutex_lock(&aal_data->primary_data->clk_lock);
	atomic_set(&aal_data->is_clock_on, 0);
	mutex_unlock(&aal_data->primary_data->clk_lock);
	mtk_ddp_comp_clk_unprepare(comp);
	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk)
			clk_unprepare(aal_data->dre3_hw.clk);
	}
}

static void disp_aal_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!aal_data->is_right_pipe) {
			aal_data->overhead.comp_overhead = 8;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead += aal_data->overhead.comp_overhead;
			cfg->tile_overhead.left_in_width += aal_data->overhead.comp_overhead;
			/*copy from total overhead info*/
			aal_data->overhead.in_width = cfg->tile_overhead.left_in_width;
			aal_data->overhead.total_overhead = cfg->tile_overhead.left_overhead;
		} else {
			aal_data->overhead.comp_overhead = 8;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead += aal_data->overhead.comp_overhead;
			cfg->tile_overhead.right_in_width += aal_data->overhead.comp_overhead;
			/*copy from total overhead info*/
			aal_data->overhead.in_width = cfg->tile_overhead.right_in_width;
			aal_data->overhead.total_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void disp_aal_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	aal_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		aal_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	aal_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void disp_aal_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = 0, out_val = 0;
	unsigned int overhead_v = 0;
	int width = cfg->w, height = cfg->h;
	int out_width = cfg->w;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		width = aal_data->overhead.in_width;
		out_width = width - aal_data->overhead.comp_overhead;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;

		out_width = width;
	}

	AALFLOW_LOG("(w,h)=(%d,%d)+, %d\n",
		width, height, aal_data->primary_data->get_size_available);

	atomic_set(&aal_data->is_clock_on, 1);
	atomic_set(&aal_data->first_frame, 1);
	aal_data->primary_data->size.height = height;
	aal_data->primary_data->size.width = cfg->w;
	aal_data->primary_data->dual_size.height = height;
	aal_data->primary_data->dual_size.width = cfg->w;
	aal_data->primary_data->size.isdualpipe = comp->mtk_crtc->is_dual_pipe;
	aal_data->primary_data->dual_size.isdualpipe = comp->mtk_crtc->is_dual_pipe;
	if (aal_data->primary_data->get_size_available == false) {
		aal_data->primary_data->get_size_available = true;
		wake_up_interruptible(&aal_data->primary_data->size_wq);
		AALFLOW_LOG("size available: (w,h)=(%d,%d)+\n", width, height);
	}

	if (aal_data->set_partial_update != 1) {
		val = (width << 16) | (height);
		out_val = (out_width << 16) | height;
	} else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : aal_data->tile_overhead_v.overhead_v;
		val = (width << 16) | (aal_data->roi_height + overhead_v * 2);
		out_val = (out_width << 16) | (aal_data->roi_height + overhead_v * 2);
	}
	if (aal_data->primary_data->aal_param_valid) {
		disp_aal_write_dre_to_reg(comp, handle, &aal_data->primary_data->aal_param);
		disp_aal_write_cabc_to_reg(comp, handle, &aal_data->primary_data->aal_param);
	} else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_DRE_MAPPING_00, 1, 1 << 4);

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		mutex_lock(&aal_data->primary_data->config_lock);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
		if (aal_data->dre3_curve_need_reset) {
			disp_aal_dre3_reset_to_linear(comp, 0);
			aal_data->dre3_curve_need_reset = false;
		}
#endif
		disp_aal_write_dre3_curve_full(comp);
		mutex_unlock(&aal_data->primary_data->config_lock);
	}

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_OUTPUT_SIZE, out_val, ~0);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (!aal_data->is_right_pipe) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_OFFSET, 0x0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				(aal_data->primary_data->size.width/2 - 1) << 13, ~0);
		} else {
			struct mtk_disp_aal *aal1_data = comp_to_aal(aal_data->companion);

			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
				(aal1_data->overhead.comp_overhead << 16) | 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
				((aal_data->primary_data->size.width / 2
				+ aal1_data->overhead.total_overhead - 1) << 13)
				| aal1_data->overhead.total_overhead, ~0);
		}

		aal_data->primary_data->dual_size.aaloverhead = aal_data->overhead.total_overhead;
	} else if (comp->mtk_crtc->is_dual_pipe) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
			(0 << 16) | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(aal_data->primary_data->size.width/2 - 1) << 13, ~0);

		aal_data->primary_data->dual_size.aaloverhead = 0;
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
			(0 << 16) | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_DRE_BLOCK_INFO_00,
			(aal_data->primary_data->size.width - 1) << 13, ~0);

		aal_data->primary_data->size.aaloverhead = 0;
	}

	if (aal_data->primary_data->aal_fo->mtk_dre30_support) {
		phys_addr_t dre3_pa = disp_aal_dre3_pa(comp);
		int dre_alg_mode = 0;

		if (atomic_read(&aal_data->primary_data->change_to_dre30) & 0x1)
			dre_alg_mode = 1;
		if (debug_bypass_alg_mode)
			dre_alg_mode = 0;
		cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DMDP_AAL_CFG_MAIN,
			(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));
		if (atomic_read(&aal_data->primary_data->change_to_dre30) == 0x3)
			disp_aal_init_dre3_reg(comp, handle, &aal_data->primary_data->init_regs);
		else
			cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + DISP_AAL_DRE_MAPPING_00, 1, 1 << 4);
		if (aal_data->primary_data->disp_clarity_support)
			cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + DMDP_AAL_DRE_BILATERAL_STATUS_CTRL,
			0x3 << 1, 0x3 << 1);
	}

	disp_aal_init(comp, cfg, handle);
	disp_aal_set_interrupt(comp, !!atomic_read(&aal_data->primary_data->eof_irq_en), handle);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_CMB_MAIN_0, 0, NEW_CBOOST_EN);
	// for Display Clarity
	if (aal_data->primary_data->disp_clarity_regs != NULL) {
		mutex_lock(&aal_data->primary_data->config_lock);
		if (disp_aal_set_clarity_reg(comp, handle,
			aal_data->primary_data->disp_clarity_regs) < 0)
			DDPMSG("%s: clarity_set_reg failed\n", __func__);
		mutex_unlock(&aal_data->primary_data->config_lock);
	}
	if (aal_data->data->need_bypass_shadow)
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_SHADOW_CTRL, 1, AAL_BYPASS_SHADOW);
	else
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_SHADOW_CTRL, 0, AAL_BYPASS_SHADOW);
}

void disp_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct drm_display_mode *mode;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("\n");
	mode = mtk_crtc_get_display_mode_by_comp(__func__, &mtk_crtc->base, comp, false);
	if ((mode != NULL) && (aal_data != NULL)) {
		aal_data->primary_data->fps = drm_mode_vrefresh(mode);
		DDPMSG("%s: first config set fps: %d\n", __func__, aal_data->primary_data->fps);
	}

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	if ((aal_data != NULL) && (aal_data->primary_data != NULL) &&
		(aal_data->primary_data->aal_fo != NULL) &&
		(aal_data->primary_data->aal_fo->mtk_dre30_support))
		disp_aal_init_dre3_curve(comp);
#endif

	disp_aal_config(comp, cfg, handle);
}

static int disp_aal_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	if (!g_drm_dev)
		g_drm_dev = drm_dev;
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void disp_aal_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_aal_funcs = {
	.config = disp_aal_config,
	.first_cfg = disp_aal_first_cfg,
	.start = disp_aal_start,
	.stop = disp_aal_stop,
	.bypass = disp_aal_bypass,
	.user_cmd = disp_aal_user_cmd,
	.io_cmd = disp_aal_io_cmd,
	.prepare = disp_aal_prepare,
	.unprepare = disp_aal_unprepare,
	.config_overhead = disp_aal_config_overhead,
	.config_overhead_v = disp_aal_config_overhead_v,
	.pq_frame_config = disp_aal_frame_config,
	.pq_ioctl_transact = disp_aal_ioctl_transact,
	.mutex_sof_irq = disp_aal_on_start_of_frame,
	.partial_update = disp_aal_set_partial_update,
};

static const struct component_ops mtk_disp_aal_component_ops = {
	.bind	= disp_aal_bind,
	.unbind = disp_aal_unbind,
};

static int disp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_aal *priv;
	enum mtk_ddp_comp_id comp_id;
	struct device_node *tdshp_node;
	int ret, irq;
	struct device_node *dre3_dev_node;
	struct platform_device *dre3_pdev;
	struct resource dre3_res;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		AALERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_AAL);
	if ((int)comp_id < 0) {
		AALERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	atomic_set(&priv->is_clock_on, 0);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		AALERR("Failed to get irq %d\n", ret);
		goto error_primary;
	}

	priv->primary_data->aal_fo = devm_kzalloc(dev, sizeof(*priv->primary_data->aal_fo),
							GFP_KERNEL);
	if (priv->primary_data->aal_fo == NULL) {
		ret = -ENOMEM;
		AALERR("Failed to alloc aal_fo %d\n", ret);
		goto error_primary;
	}
	// for Display Clarity
	tdshp_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_tdshp0");
	if (!of_property_read_u32(tdshp_node, "mtk-tdshp-clarity-support",
		&priv->primary_data->tdshp_clarity_support)) {
		DDPMSG("disp_tdshp: mtk_tdshp_clarity_support = %d\n",
			priv->primary_data->tdshp_clarity_support);
	}

	if (of_property_read_u32(dev->of_node, "mtk-aal-support",
		&priv->primary_data->aal_fo->mtk_aal_support)) {
		AALERR("comp_id: %d, mtk_aal_support = %d\n",
			comp_id, priv->primary_data->aal_fo->mtk_aal_support);
		priv->primary_data->aal_fo->mtk_aal_support = 0;
	}

	if (of_property_read_u32(dev->of_node, "mtk-cabc-no-support",
		&priv->primary_data->aal_fo->mtk_cabc_no_support)) {
		AALERR("comp_id: %d, mtk-cabc-no-support = %d\n",
			comp_id, priv->primary_data->aal_fo->mtk_cabc_no_support);
		priv->primary_data->aal_fo->mtk_cabc_no_support = 0;
	}

	if (of_property_read_u32(dev->of_node, "mtk-dre30-support",
		&priv->primary_data->aal_fo->mtk_dre30_support)) {
		AALERR("comp_id: %d, mtk_dre30_support = %d\n",
				comp_id, priv->primary_data->aal_fo->mtk_dre30_support);
		priv->primary_data->aal_fo->mtk_dre30_support = 0;
	} else {
		if (priv->primary_data->aal_fo->mtk_dre30_support) {
			if (of_property_read_u32(dev->of_node, "aal-dre3-en",
					&priv->primary_data->dre30_en)) {
				priv->primary_data->dre30_enabled = true;
			} else
				priv->primary_data->dre30_enabled =
					(priv->primary_data->dre30_en == 1) ? true : false;

			// for Display Clarity
			if (!of_property_read_u32(dev->of_node, "mtk-aal-clarity-support",
					&priv->primary_data->aal_clarity_support))
				DDPMSG("mtk_aal_clarity_support = %d\n",
						priv->primary_data->aal_clarity_support);

			if ((priv->primary_data->aal_clarity_support == 1)
					&& (priv->primary_data->tdshp_clarity_support == 1)) {
				priv->primary_data->disp_clarity_support = 1;
				DDPMSG("%s: display clarity support = %d\n",
					__func__, priv->primary_data->disp_clarity_support);
			}
		}
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_YCT)
	priv->dre3_curve_need_reset = true;
#endif

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_aal_funcs);
	if (ret) {
		AALERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, disp_aal_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	do {
		const char *clock_name;

		if (!priv->primary_data->aal_fo->mtk_dre30_support) {
			pr_notice("[debug] dre30 is not support\n");
			break;
		}
		dre3_dev_node = of_parse_phandle(
			pdev->dev.of_node, "aal-dre3", 0);
		if (dre3_dev_node)
			pr_notice("found dre3 aal node, it's another hw\n");
		else
			break;
		dre3_pdev = of_find_device_by_node(dre3_dev_node);
		if (dre3_pdev)
			pr_notice("found dre3 aal device, it's another hw\n");
		else
			break;
		of_node_put(dre3_dev_node);
		priv->dre3_hw.dev = &dre3_pdev->dev;
		priv->dre3_hw.va = of_iomap(dre3_pdev->dev.of_node, 0);
		if (!priv->dre3_hw.va) {
			pr_notice("cannot found allocate dre3 va!\n");
			break;
		}
		ret = of_address_to_resource(
			dre3_pdev->dev.of_node, 0, &dre3_res);
		if (ret) {
			pr_notice("cannot found allocate dre3 resource!\n");
			break;
		}
		priv->dre3_hw.pa = dre3_res.start;

		if (of_property_read_string(dre3_dev_node, "clock-names", &clock_name))
			priv->dre3_hw.clk = of_clk_get_by_name(dre3_dev_node, clock_name);

		if (IS_ERR(priv->dre3_hw.clk)) {
			pr_notice("fail @ dre3 clock. name:%s\n",
				"DRE3_AAL0");
			break;
		}
		pr_notice("dre3 dev:%p va:%p pa:%pa", priv->dre3_hw.dev,
			priv->dre3_hw.va, &priv->dre3_hw.pa);
	} while (0);

	ret = component_add(dev, &mtk_disp_aal_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (comp_id == DDP_COMPONENT_AAL0)
		mtk_leds_register_notifier(&leds_init_notifier);
#endif
#if IS_ENABLED(CONFIG_MTK_DISP_LOGGER)
#if IS_ENABLED(CONFIG_MTK_MME_SUPPORT)
	MME_REGISTER_BUFFER(MME_MODULE_DISP, "DISP_AAL", MME_BUFFER_INDEX_9, MME_AAL_BUFFER_SIZE);
#endif
#endif
	AALFLOW_LOG("-\n");
error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);
	return ret;
}

static int disp_aal_remove(struct platform_device *pdev)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_aal_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (priv->ddp_comp.id == DDP_COMPONENT_AAL0)
		mtk_leds_unregister_notifier(&leds_init_notifier);
#endif

	return 0;
}

static const struct mtk_disp_aal_data mt6768_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.aal_dre_hist_start = 1024,
	.aal_dre_hist_end   = 4092,
	.aal_dre_gain_start = 4096,
	.aal_dre_gain_end   = 6268,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6761_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.aal_dre_hist_start = 1024,
	.aal_dre_hist_end   = 4092,
	.aal_dre_gain_start = 4096,
	.aal_dre_gain_end   = 6268,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6765_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1024,
	.aal_dre_hist_end   = 4092,
	.aal_dre_gain_start = 4096,
	.aal_dre_gain_end   = 6268,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6853_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6885_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.aal_dre_hist_start = 1152,
	.aal_dre_hist_end   = 4220,
	.aal_dre_gain_start = 4224,
	.aal_dre_gain_end   = 6396,
	.bitShift = 13,
};

static const struct mtk_disp_aal_data mt6833_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6877_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6781_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6983_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6895_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6879_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6985_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6897_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6886_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6899_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = true,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6989_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = true,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6878_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = false,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6991_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.aal_dre3_curve_sram = true,
	.aal_dre3_auto_inc = true,
	.mdp_aal_ghist_support = true,
	.bitShift = 16,
};

static const struct of_device_id mtk_disp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6768-disp-aal", .data = &mt6768_aal_driver_data},
	{ .compatible = "mediatek,mt6761-disp-aal", .data = &mt6761_aal_driver_data},
	{ .compatible = "mediatek,mt6885-disp-aal", .data = &mt6885_aal_driver_data},
	{ .compatible = "mediatek,mt6833-disp-aal", .data = &mt6833_aal_driver_data},
	{ .compatible = "mediatek,mt6765-disp-aal", .data = &mt6765_aal_driver_data},
	{ .compatible = "mediatek,mt6877-disp-aal", .data = &mt6877_aal_driver_data},
	{ .compatible = "mediatek,mt6853-disp-aal", .data = &mt6853_aal_driver_data},
	{ .compatible = "mediatek,mt6781-disp-aal", .data = &mt6781_aal_driver_data},
	{ .compatible = "mediatek,mt6983-disp-aal", .data = &mt6983_aal_driver_data},
	{ .compatible = "mediatek,mt6895-disp-aal", .data = &mt6895_aal_driver_data},
	{ .compatible = "mediatek,mt6879-disp-aal", .data = &mt6879_aal_driver_data},
	{ .compatible = "mediatek,mt6985-disp-aal", .data = &mt6985_aal_driver_data},
	{ .compatible = "mediatek,mt6897-disp-aal", .data = &mt6897_aal_driver_data},
	{ .compatible = "mediatek,mt6886-disp-aal", .data = &mt6886_aal_driver_data},
	{ .compatible = "mediatek,mt6835-disp-aal", .data = &mt6835_aal_driver_data},
	{ .compatible = "mediatek,mt6989-disp-aal", .data = &mt6989_aal_driver_data},
	{ .compatible = "mediatek,mt6878-disp-aal", .data = &mt6878_aal_driver_data},
	{ .compatible = "mediatek,mt6991-disp-aal", .data = &mt6991_aal_driver_data},
	{ .compatible = "mediatek,mt6899-disp-aal", .data = &mt6899_aal_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_aal_driver_dt_match);

struct platform_driver mtk_disp_aal_driver = {
	.probe		= disp_aal_probe,
	.remove		= disp_aal_remove,
	.driver		= {
		.name	= "mediatek-disp-aal",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_aal_driver_dt_match,
	},
};

/* Legacy AAL_SUPPORT_KERNEL_API */
#define AAL_CONTROL_CMD(ID, CONTROL) (ID << 16 | CONTROL)
void disp_aal_set_ess_level(struct mtk_ddp_comp *comp, int level)
{
	unsigned long flags;
	int level_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->ess_level_cmd_id += 1;
	aal_data->primary_data->ess_level_cmd_id = aal_data->primary_data->ess_level_cmd_id % 64;
	level_command = AAL_CONTROL_CMD(aal_data->primary_data->ess_level_cmd_id, level);

	aal_data->primary_data->ess_level = level_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("level = %d (cmd = 0x%x)\n", level, level_command);
}

void disp_aal_set_ess_en(struct mtk_ddp_comp *comp, int enable)
{
	unsigned long flags;
	int enable_command = 0;
	int level_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	int func_flag = atomic_read(&aal_data->primary_data->func_flag);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->ess_en_cmd_id += 1;
	aal_data->primary_data->ess_en_cmd_id = aal_data->primary_data->ess_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(aal_data->primary_data->ess_en_cmd_id, enable);

	aal_data->primary_data->ess_en = enable_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("en = %d (cmd = 0x%x) level = 0x%08x (cmd = 0x%x)\n",
		enable, enable_command, ESS_LEVEL_BY_CUSTOM_LIB, level_command);

	if (enable && (func_flag == 0))
		disp_aal_relay_control(comp, false);
}

void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable)
{
	unsigned long flags;
	int enable_command = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	int func_flag = atomic_read(&aal_data->primary_data->func_flag);

	spin_lock_irqsave(&aal_data->primary_data->hist_lock, flags);

	aal_data->primary_data->dre_en_cmd_id += 1;
	aal_data->primary_data->dre_en_cmd_id = aal_data->primary_data->dre_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(aal_data->primary_data->dre_en_cmd_id, enable);

	aal_data->primary_data->dre_en = enable_command;

	spin_unlock_irqrestore(&aal_data->primary_data->hist_lock, flags);

	disp_aal_refresh_by_kernel(aal_data, 1);
	AALAPI_LOG("en = %d (cmd = 0x%x), func_flag: %d\n", enable, enable_command, func_flag);

	if (enable && (func_flag == 0))
		disp_aal_relay_control(comp, false);
}

void disp_aal_debug(struct drm_crtc *crtc, const char *opt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;
	int gain_offset;
	int arry_offset;
	unsigned int gain_arry[4];
	unsigned int *gain_pr;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return;
	}
	aal_data = comp_to_aal(comp);

	pr_notice("[debug]: %s\n", opt);
	if (strncmp(opt, "setparam:", 9) == 0) {
		debug_skip_set_param = strncmp(opt + 9, "skip", 4) == 0;
		pr_notice("[debug] skip_set_param=%d\n", debug_skip_set_param);
	} else if (strncmp(opt, "dre3algmode:", 12) == 0) {
		debug_bypass_alg_mode = strncmp(opt + 12, "bypass", 6) == 0;
		pr_notice("[debug] bypass_alg_mode=%d\n", debug_bypass_alg_mode);
	} else if (strncmp(opt, "dumpdre3hist:", 13) == 0) {
		if (sscanf(opt + 13, "%d,%d\n", &dump_blk_x, &dump_blk_y) == 2)
			pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n", dump_blk_x, dump_blk_y);
		else
			pr_notice("[debug] dump_blk parse fail\n");
	} else if (strncmp(opt, "dumpdre3gain", 12) == 0) {
		arry_offset = 0;
		gain_pr = aal_data->primary_data->dre30_gain.dre30_gain;
		for (gain_offset = aal_data->data->aal_dre_gain_start;
			gain_offset <= aal_data->data->aal_dre_gain_end;
				gain_offset += 16, arry_offset += 4) {
			if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
				break;
			if (arry_offset + 4 <= AAL_DRE30_GAIN_REGISTER_NUM) {
				memcpy(gain_arry, gain_pr + arry_offset, sizeof(unsigned int) * 4);
			} else {
				memset(gain_arry, 0, sizeof(gain_arry));
				memcpy(gain_arry, gain_pr + arry_offset,
					sizeof(unsigned int) * (arry_offset + 5 - AAL_DRE30_GAIN_REGISTER_NUM));
			}
			DDPMSG("[debug] dre30_gain 0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n",
					arry_offset, gain_arry[0], gain_arry[1], gain_arry[2], gain_arry[3]);
		}
	} else if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[debug] debug_flow_log=%d\n", debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_api_log=%d\n", debug_api_log);
	} else if (strncmp(opt, "irq_log:", 8) == 0) {
		debug_irq_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_irq_log=%d\n", debug_irq_log);
	} else if (strncmp(opt, "dump_aal_hist:", 14) == 0) {
		debug_dump_aal_hist = strncmp(opt + 14, "1", 1) == 0;
		pr_notice("[debug] debug_dump_aal_hist=%d\n", debug_dump_aal_hist);
	} else if (strncmp(opt, "dump_input_param:", 17) == 0) {
		debug_dump_input_param = strncmp(opt + 17, "1", 1) == 0;
		pr_notice("[debug] debug_dump_input_param=%d\n", debug_dump_input_param);
	} else if (strncmp(opt, "set_ess_level:", 14) == 0) {
		int debug_ess_level;

		if (sscanf(opt + 14, "%d", &debug_ess_level) == 1) {
			pr_notice("[debug] ess_level=%d\n", debug_ess_level);
			disp_aal_set_ess_level(comp, debug_ess_level);
		} else
			pr_notice("[debug] set_ess_level failed\n");
	} else if (strncmp(opt, "set_ess_en:", 11) == 0) {
		bool debug_ess_en;

		debug_ess_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_ess_en=%d\n", debug_ess_en);
		disp_aal_set_ess_en(comp, debug_ess_en);
	} else if (strncmp(opt, "set_dre_en:", 11) == 0) {
		bool debug_dre_en;

		debug_dre_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_dre_en=%d\n", debug_dre_en);
		disp_aal_set_dre_en(comp, debug_dre_en);
	} else if (strncmp(opt, "dump_clarity_regs:", 18) == 0) {
		debug_dump_clarity_regs = strncmp(opt + 18, "1", 1) == 0;
		pr_notice("[debug] debug_dump_clarity_regs=%d\n", debug_dump_clarity_regs);
	} else if (strncmp(opt, "debugdump", 9) == 0) {
		pr_notice("[debug] skip_set_param=%d\n", debug_skip_set_param);
		pr_notice("[debug] bypass_alg_mode=%d\n", debug_bypass_alg_mode);
		pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n", dump_blk_x, dump_blk_y);
		pr_notice("[debug] debug_flow_log=%d\n", debug_flow_log);
		pr_notice("[debug] debug_api_log=%d\n", debug_api_log);
		pr_notice("[debug] debug_irq_log=%d\n", debug_irq_log);
		pr_notice("[debug] debug_dump_aal_hist=%d\n", debug_dump_aal_hist);
		pr_notice("[debug] debug_dump_input_param=%d\n", debug_dump_input_param);
		pr_notice("[debug] debug_ess_level=%d\n", aal_data->primary_data->ess_level);
		pr_notice("[debug] debug_ess_en=%d\n", aal_data->primary_data->ess_en);
		pr_notice("[debug] debug_dre_en=%d\n", aal_data->primary_data->dre_en);
		pr_notice("[debug] dre30_enabled=%d\n", aal_data->primary_data->dre30_enabled);
		pr_notice("[debug] debug_dump_clarity_regs=%d\n", debug_dump_clarity_regs);
		pr_notice("[debug] hist_available=%d\n", atomic_read(&aal_data->hist_available));
		pr_notice("[debug] dre20_hist_is_ready=%d\n", atomic_read(&aal_data->dre20_hist_is_ready));
		pr_notice("[debug] event_en=%d\n", atomic_read(&aal_data->primary_data->event_en));
		pr_notice("[debug] should_stop=%d\n", atomic_read(&aal_data->primary_data->should_stop));
	}
}

void disp_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, 0x200, 0x10);
}

void disp_aal_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("== %s RELAY_STATE: 0x%x ==\n", mtk_dump_comp_str(comp), aal_data->primary_data->relay_state);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x580; k += 16)
		mtk_cust_dump_reg(baddr, k + 0, k + 0x4, k + 0x8, k + 0xc);
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && aal_data->companion) {
		baddr = aal_data->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(aal_data->companion), aal_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(aal_data->companion));
		for (k = 0; k <= 0x580; k += 16)
			mtk_cust_dump_reg(baddr, k + 0, k + 0x4, k + 0x8, k + 0xc);
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(aal_data->companion));
	}
}

unsigned int disp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_aal *aal_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_AAL, 0);
	if (!comp) {
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return 1;
	}
	aal_data = comp_to_aal(comp);

	return aal_data->primary_data->relay_state != 0 ? 1 : 0;
}
