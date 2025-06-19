/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _MT6681_H_
#define _MT6681_H_

#include <linux/mfd/mt6681-private.h>

/* AUDENC_ANA_CON16: */
#define RG_AUD_MICBIAS1_LOWP_EN (1 << MT6681_RG_AUDMICBIAS1LOWPEN_SHIFT)
/* AUDENC_ANA_CON18: */
#define RG_ACCDET_MODE_ANA11_MODE1 (0x000F)
#define RG_ACCDET_MODE_ANA11_MODE2 (0x008F)
#define RG_ACCDET_MODE_ANA11_MODE6 (0x008F)

/* ------Register_AUXADC_REG  Bit Define------ */
/* AUXADC_ADC5:  Auxadc CH5 read data */
#define AUXADC_DATA_RDY_CH5 (1 << 15)
#define AUXADC_DATA_PROCEED_CH5 (0 << 15)
#define AUXADC_DATA_MASK (0x0FFF)

/* AUXADC_RQST0_SET:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_SET (1 << 5)
/* AUXADC_RQST0_CLR:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_CLR (1 << 5)

/* -----Register_EFUSE_REG  Bit Define-------- */
#define ACCDET_CALI_MASK0 (0xFF)
#define ACCDET_CALI_MASK1 (0xFF << 8)
#define ACCDET_CALI_MASK2 (0xFF)
#define ACCDET_CALI_MASK3 (0xFF << 8)
#define ACCDET_CALI_MASK4 (0xFF)
/* -----Register_ACCDET_REG  Bit Define------- */
#define ACCDET_EINT1_IRQ_CLR_B11 (0x01 << MT6681_ACCDET_EINT1_IRQ_CLR_SHIFT)
#define ACCDET_EINT0_IRQ_CLR_B10 (0x01 << MT6681_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_EINT_IRQ_CLR_B10_11 (0x03 << MT6681_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_IRQ_CLR_B8 (0x01 << MT6681_ACCDET_IRQ_CLR_SHIFT)
#define ACCDET_EINT1_IRQ_B3 (0x01 << MT6681_ACCDET_EINT1_IRQ_SHIFT)
#define ACCDET_EINT0_IRQ_B2 (0x01 << MT6681_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_EINT_IRQ_B2_B3 (0x03 << MT6681_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_IRQ_B0 (0x01 << MT6681_ACCDET_IRQ_SHIFT)
/* ACCDET_CON25: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET (MT6681_ACCDET_MEM_IN_SHIFT)
#define ACCDET_STATE_AB_MASK (0x03)
#define ACCDET_STATE_AB_00 (0x00)
#define ACCDET_STATE_AB_01 (0x01)
#define ACCDET_STATE_AB_10 (0x02)
#define ACCDET_STATE_AB_11 (0x03)
/* ACCDET_CON19 */
#define ACCDET_EINT0_STABLE_VAL                                                \
	((1 << MT6681_ACCDET_DA_STABLE_SHIFT)                                  \
	 | (1 << MT6681_ACCDET_EINT0_EN_STABLE_SHIFT)                          \
	 | (1 << MT6681_ACCDET_EINT0_CMPEN_STABLE_SHIFT)                       \
	 | (1 << MT6681_ACCDET_EINT0_CEN_STABLE_SHIFT))
#define ACCDET_EINT1_STABLE_VAL                                                \
	((1 << MT6681_ACCDET_DA_STABLE_SHIFT)                                  \
	 | (1 << MT6681_ACCDET_EINT1_EN_STABLE_SHIFT)                          \
	 | (1 << MT6681_ACCDET_EINT1_CMPEN_STABLE_SHIFT)                       \
	 | (1 << MT6681_ACCDET_EINT1_CEN_STABLE_SHIFT))

/* hw gain */
static const uint32_t kHWGainMap[] = {
	0x00000, //   0, -64.0 dB (mute)
	0x0015E, //   1, -63.5 dB
	0x00173, //   2, -63.0 dB
	0x00189, //   3, -62.5 dB
	0x001A0, //   4, -62.0 dB
	0x001B9, //   5, -61.5 dB
	0x001D3, //   6, -61.0 dB
	0x001EE, //   7, -60.5 dB
	0x0020C, //   8, -60.0 dB
	0x0022B, //   9, -59.5 dB
	0x0024C, //  10, -59.0 dB
	0x0026F, //  11, -58.5 dB
	0x00294, //  12, -58.0 dB
	0x002BB, //  13, -57.5 dB
	0x002E4, //  14, -57.0 dB
	0x00310, //  15, -56.5 dB
	0x0033E, //  16, -56.0 dB
	0x00370, //  17, -55.5 dB
	0x003A4, //  18, -55.0 dB
	0x003DB, //  19, -54.5 dB
	0x00416, //  20, -54.0 dB
	0x00454, //  21, -53.5 dB
	0x00495, //  22, -53.0 dB
	0x004DB, //  23, -52.5 dB
	0x00524, //  24, -52.0 dB
	0x00572, //  25, -51.5 dB
	0x005C5, //  26, -51.0 dB
	0x0061D, //  27, -50.5 dB
	0x00679, //  28, -50.0 dB
	0x006DC, //  29, -49.5 dB
	0x00744, //  30, -49.0 dB
	0x007B2, //  31, -48.5 dB
	0x00827, //  32, -48.0 dB
	0x008A2, //  33, -47.5 dB
	0x00925, //  34, -47.0 dB
	0x009B0, //  35, -46.5 dB
	0x00A43, //  36, -46.0 dB
	0x00ADF, //  37, -45.5 dB
	0x00B84, //  38, -45.0 dB
	0x00C32, //  39, -44.5 dB
	0x00CEC, //  40, -44.0 dB
	0x00DB0, //  41, -43.5 dB
	0x00E7F, //  42, -43.0 dB
	0x00F5B, //  43, -42.5 dB
	0x01044, //  44, -42.0 dB
	0x0113B, //  45, -41.5 dB
	0x01240, //  46, -41.0 dB
	0x01355, //  47, -40.5 dB
	0x0147A, //  48, -40.0 dB
	0x015B1, //  49, -39.5 dB
	0x016FA, //  50, -39.0 dB
	0x01857, //  51, -38.5 dB
	0x019C8, //  52, -38.0 dB
	0x01B4F, //  53, -37.5 dB
	0x01CED, //  54, -37.0 dB
	0x01EA4, //  55, -36.5 dB
	0x02075, //  56, -36.0 dB
	0x02261, //  57, -35.5 dB
	0x0246B, //  58, -35.0 dB
	0x02693, //  59, -34.5 dB
	0x028DC, //  60, -34.0 dB
	0x02B48, //  61, -33.5 dB
	0x02DD9, //  62, -33.0 dB
	0x03090, //  63, -32.5 dB
	0x03371, //  64, -32.0 dB
	0x0367D, //  65, -31.5 dB
	0x039B8, //  66, -31.0 dB
	0x03D24, //  67, -30.5 dB
	0x040C3, //  68, -30.0 dB
	0x04499, //  69, -29.5 dB
	0x048AA, //  70, -29.0 dB
	0x04CF8, //  71, -28.5 dB
	0x05188, //  72, -28.0 dB
	0x0565D, //  73, -27.5 dB
	0x05B7B, //  74, -27.0 dB
	0x060E6, //  75, -26.5 dB
	0x066A4, //  76, -26.0 dB
	0x06CB9, //  77, -25.5 dB
	0x0732A, //  78, -25.0 dB
	0x079FD, //  79, -24.5 dB
	0x08138, //  80, -24.0 dB
	0x088E0, //  81, -23.5 dB
	0x090FC, //  82, -23.0 dB
	0x09994, //  83, -22.5 dB
	0x0A2AD, //  84, -22.0 dB
	0x0AC51, //  85, -21.5 dB
	0x0B687, //  86, -21.0 dB
	0x0C157, //  87, -20.5 dB
	0x0CCCC, //  88, -20.0 dB
	0x0D8EF, //  89, -19.5 dB
	0x0E5CA, //  90, -19.0 dB
	0x0F367, //  91, -18.5 dB
	0x101D3, //  92, -18.0 dB
	0x1111A, //  93, -17.5 dB
	0x12149, //  94, -17.0 dB
	0x1326D, //  95, -16.5 dB
	0x14496, //  96, -16.0 dB
	0x157D1, //  97, -15.5 dB
	0x16C31, //  98, -15.0 dB
	0x181C5, //  99, -14.5 dB
	0x198A1, // 100, -14.0 dB
	0x1B0D7, // 101, -13.5 dB
	0x1CA7D, // 102, -13.0 dB
	0x1E5A8, // 103, -12.5 dB
	0x2026F, // 104, -12.0 dB
	0x220EA, // 105, -11.5 dB
	0x24134, // 106, -11.0 dB
	0x26368, // 107, -10.5 dB
	0x287A2, // 108, -10.0 dB
	0x2AE02, // 109,  -9.5 dB
	0x2D6A8, // 110,  -9.0 dB
	0x301B7, // 111,  -8.5 dB
	0x32F52, // 112,  -8.0 dB
	0x35FA2, // 113,  -7.5 dB
	0x392CE, // 114,  -7.0 dB
	0x3C903, // 115,  -6.5 dB
	0x4026E, // 116,  -6.0 dB
	0x43F40, // 117,  -5.5 dB
	0x47FAC, // 118,  -5.0 dB
	0x4C3EA, // 119,  -4.5 dB
	0x50C33, // 120,  -4.0 dB
	0x558C4, // 121,  -3.5 dB
	0x5A9DF, // 122,  -3.0 dB
	0x5FFC8, // 123,  -2.5 dB
	0x65AC8, // 124,  -2.0 dB
	0x6BB2D, // 125,  -1.5 dB
	0x72148, // 126,  -1.0 dB
	0x78D6F, // 127,  -0.5 dB
	0x80000, // 128,   0.0 dB
	// positive gain
	0x8795A, // 0.5
	0x8F9E4, // 1
	0x9820D, // 1.5
	0xA1248, // 2
	0xAAB0D, // 2.5
	0xB4CE4, // 3
	0xBF84A, // 3.5
	0xCADDC, // 4
	0xD6E30, // 4.5
	0xE39EA, // 5
	0xF11B6, // 5.5
	0xFF64A, // 6
	0x10E86C, // 6.5
	0x11E8E2, // 7
	0x12F892, // 7.5
	0x141856, // 8
	0x15492A, // 8.5
	0x168C08, // 9
	0x17E210, // 9.5
	0x194C54, // 10
	0x1ACC17, // 10.5
	0x1C6290, // 11
	0x1E1126, // 11.5
	0x1FD93E, // 12
	0x21BC58, // 12.5
	0x23BC16, // 13
	0x25DA23, // 13.5
	0x28184C, // 14
	0x2A7883, // 14.5
	0x2CFCC2, // 15
	0x2FA729, // 15.5
	0x3279FE, // 16
	0x3577AE, // 16.5
	0x38A2B6, // 17
	0x3BFDD5, // 17.5
	0x3F8BDA, // 18
	0x434FC5, // 18.5
	0x474CD0, // 19
	0x4B865D, // 19.5
	0x500000, // 20
	0x54BD84, // 20.5
	0x59C2F2, // 21
	0x5F1486, // 21.5
	0x64B6C6, // 22
	0x6AAE84, // 22.5
	0x7100C0, // 23
	0x77B2E7, // 23.5
	0x7ECA98, // 24
	0x864DE8, // 24.5
	0x8E432E, // 25
};

static const long kHWGainRatioMAP[] = {
	0, //   0, -64.0 dB (mute)
	67,
	71,
	75,
	79,
	84,
	89,
	94,
	100,
	106,
	112,
	119,
	126,
	133,
	141,
	150,
	158,
	168,
	178,
	188,
	200,
	211,
	224,
	237,
	251,
	266,
	282,
	299,
	316,
	335,
	355,
	376,
	398,
	422,
	447,
	473,
	501,
	531,
	562,
	596,
	631,
	668,
	708,
	750,
	794,
	841,
	891,
	944,
	1000,
	1059,
	1122,
	1189,
	1259,
	1334,
	1413,
	1496,
	1585,
	1679,
	1778,
	1884,
	1995,
	2113,
	2239,
	2371,
	2512,
	2661,
	2818,
	2985,
	3162,
	3350,
	3548,
	3758,
	3981,
	4217,
	4467,
	4732,
	5012,
	5309,
	5623,
	5957,
	6310,
	6683,
	7079,
	7499,
	7943,
	8414,
	8913,
	9441,
	10000,
	10593,
	11220,
	11885,
	12589,
	13335,
	14125,
	14962,
	15849,
	16788,
	17783,
	18836,
	19953,
	21135,
	22387,
	23714,
	25119,
	26607,
	28184,
	29854,
	31623,
	33497,
	35481,
	37584,
	39811,
	42170,
	44668,
	47315,
	50119,
	53088,
	56234,
	59566,
	63096,
	66834,
	70795,
	74989,
	79433,
	84140,
	89125,
	94406,
	100000,
	112202,
	125893,
	141254,
	158489,
	177828,
	199526,
	223872,
	251189,
	281838,
	316228,
	354813,
	398107,
	446684,
	501187,
	562341,
	630957,
	707946,
	794328,
	891251,
	1000000,
	1122018,
	1258925,
	1412538,
	1584893,
	1778279,
};

#endif /* end _MT6681_H_ */
