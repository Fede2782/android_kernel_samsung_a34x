/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ISA1000_VIBRATOR_H__
#define __ISA1000_VIBRATOR_H__

#define ISA1000_DIVIDER		128
#define FREQ_DIVIDER		(NSEC_PER_SEC / ISA1000_DIVIDER * 10)

#define isa_err(format, ...) \
	pr_err("[VIB] %s: " format "\n", __func__, ##__VA_ARGS__)

#define isa_info(format, ...) \
	pr_info("[VIB] %s: " format "\n", __func__, ##__VA_ARGS__)


#if IS_ENABLED(CONFIG_MTK_PWM)

//#include <mt-plat/mtk_pwm.h>   //chipset based inclusion selection though MTK_PLATFORM in makefile
//#include <mach/mtk_pwm_hal.h>

struct pwm_spec_config {
	u32 pwm_no;
	u32 mode;
	u32 clk_div;
	u32 clk_src;
	u8 intr;
	u8 pmic_pad;

	union {
		/* for old mode */
		struct _PWM_OLDMODE_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u16 GDURATION;
			u16 WAVE_NUM;
			u16 DATA_WIDTH;
			u16 THRESH;
		} PWM_MODE_OLD_REGS;

		/* for fifo mode */
		struct _PWM_MODE_FIFO_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u32 GDURATION;
			u32 SEND_DATA0;
			u32 SEND_DATA1;
			u32 WAVE_NUM;
		} PWM_MODE_FIFO_REGS;

		/* for memory mode */
		struct _PWM_MODE_MEMORY_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			u16 WAVE_NUM;
		} PWM_MODE_MEMORY_REGS;

		/* for RANDOM mode */
		struct _PWM_MODE_RANDOM_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			dma_addr_t BUF1_BASE_ADDR;
			u32 BUF1_SIZE;
			u16 WAVE_NUM;
			u32 VALID;
		} PWM_MODE_RANDOM_REGS;

		/* for seq mode */
		struct _PWM_MODE_DELAY_REGS {
			/* u32 ENABLE_DELAY_VALUE; */
			u16 PWM3_DELAY_DUR;
			u32 PWM3_DELAY_CLK;
			/* 0: block clock source, 1: block/1625 clock source */
			u16 PWM4_DELAY_DUR;
			u32 PWM4_DELAY_CLK;
			u16 PWM5_DELAY_DUR;
			u32 PWM5_DELAY_CLK;
		} PWM_MODE_DELAY_REGS;

	};
};

enum INFRA_CLK_SRC_CTRL {
	CLK_32K = 0x00,
		CLK_26M = 0x01,
	CLK_78M = 0x2,
	CLK_SEL_TOPCKGEN = 0x3,
};

enum PWM_MODE_ENUM {
	PWM_MODE_MIN,
		PWM_MODE_OLD = PWM_MODE_MIN,
	PWM_MODE_FIFO,
	PWM_MODE_MEMORY,
	PWM_MODE_RANDOM,
	PWM_MODE_DELAY,
	PWM_MODE_INVALID,
};

enum PWM_CLK_DIV {
	CLK_DIV_MIN,
	CLK_DIV1 = CLK_DIV_MIN,
	CLK_DIV2,
	CLK_DIV4,
	CLK_DIV8,
	CLK_DIV16,
	CLK_DIV32,
	CLK_DIV64,
	CLK_DIV128,
	CLK_DIV_MAX
};

enum PWM_CLOCK_SRC_ENUM {
	PWM_CLK_SRC_MIN,
	PWM_CLK_OLD_MODE_BLOCK = PWM_CLK_SRC_MIN,
	PWM_CLK_OLD_MODE_32K,
	PWM_CLK_NEW_MODE_BLOCK,
	PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625,
	PWM_CLK_SRC_NUM,
	PWM_CLK_SRC_INVALID,
};

enum PWM_CON_IDLE_BIT {
	IDLE_FALSE,
	IDLE_TRUE,
	IDLE_MAX
};

enum PWM_CON_GUARD_BIT {
	GUARD_FALSE,
	GUARD_TRUE,
	GUARD_MAX
};

s32 pwm_set_spec_config(struct pwm_spec_config *conf);
void mt_pwm_disable(u32 pwm_no, u8 pmic_pad);
void mt_pwm_clk_sel_hal(u32 pwm, u32 clk_src);
void mt_pwm_power_on(u32 pwm_no, bool pmic_pad);

static struct pwm_spec_config motor_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_FIFO,                           /*mode used for pwm generation*/
	.clk_div = CLK_DIV2,                             /*clk src division factor*/
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 61,      /*(value + 1) number of bits used in pwm generation*/
	.PWM_MODE_FIFO_REGS.HDURATION = 7,               /*number of clk cycles for which high bit is set*/
	.PWM_MODE_FIFO_REGS.LDURATION = 7,               /*number of clk cycles for which low bit is set*/
	.PWM_MODE_FIFO_REGS.GDURATION = 0,
	.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xFFFFFFFE,     /*first pwm data block writing starts from LSB*/
	.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0x1FFFFF,       /*second pwm data block writing starts from LSB*/
	.PWM_MODE_FIFO_REGS.WAVE_NUM = 0,
};

#endif //CONFIG_MTK_PWM

struct isa1000_pdata {
	int gpio_en;
	const char *regulator_name;
	struct pwm_device *pwm;
	struct regulator *regulator;
	const char *motor_type;

	int frequency;
	int freq_default;
	int duty_ratio;
	int freq_nums;
	u32 *freq_array;

#if defined(CONFIG_SEC_VIBRATOR)
	bool calibration;
	int steps;
	int *intensities;
	int *haptic_intensities;
#endif
};

struct isa1000_ddata {
	struct isa1000_pdata *pdata;
	struct sec_vibrator_drvdata sec_vib_ddata;
	int max_duty;
	int duty;
	int period;
};


#endif
