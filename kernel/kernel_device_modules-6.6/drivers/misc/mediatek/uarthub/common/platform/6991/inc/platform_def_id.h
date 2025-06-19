/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef PLATFORM_DEF_ID_H
#define PLATFORM_DEF_ID_H

#define GPIO_BASE_ADDR                            0x1002D000
#define GPIO_HUB_MODE_TX                          0x4D0
#define GPIO_HUB_MODE_TX_MASK                     0x700000
#define GPIO_HUB_MODE_TX_VALUE                    0x100000
#define GPIO_HUB_MODE_RX                          0x4D0
#define GPIO_HUB_MODE_RX_MASK                     0x7000000
#define GPIO_HUB_MODE_RX_VALUE                    0x1000000
#define GPIO_HUB_DIR_TX                           0x70
#define GPIO_HUB_DIR_TX_MASK                      (0x1 << 13)
#define GPIO_HUB_DIR_TX_SHIFT                     13
#define GPIO_HUB_DIR_RX                           0x70
#define GPIO_HUB_DIR_RX_MASK                      (0x1 << 14)
#define GPIO_HUB_DIR_RX_SHIFT                     14
#define GPIO_HUB_DIN_RX                           0x270
#define GPIO_HUB_DIN_RX_MASK                      (0x1 << 14)
#define GPIO_HUB_DIN_RX_SHIFT                     14

#define IOCFG_TM3_BASE_ADDR                       0x13860000
#define GPIO_HUB_IES_TX                           0x40
#define GPIO_HUB_IES_TX_MASK                      (0x1 << 6)
#define GPIO_HUB_IES_TX_SHIFT                     6
#define GPIO_HUB_IES_RX                           0x40
#define GPIO_HUB_IES_RX_MASK                      (0x1 << 5)
#define GPIO_HUB_IES_RX_SHIFT                     5
#define GPIO_HUB_PU_TX                            0x90
#define GPIO_HUB_PU_TX_MASK                       (0x1 << 6)
#define GPIO_HUB_PU_TX_SHIFT                      6
#define GPIO_HUB_PU_RX                            0x90
#define GPIO_HUB_PU_RX_MASK                       (0x1 << 5)
#define GPIO_HUB_PU_RX_SHIFT                      5
#define GPIO_HUB_PD_TX                            0x70
#define GPIO_HUB_PD_TX_MASK                       (0x1 << 6)
#define GPIO_HUB_PD_TX_SHIFT                      6
#define GPIO_HUB_PD_RX                            0x70
#define GPIO_HUB_PD_RX_MASK                       (0x1 << 5)
#define GPIO_HUB_PD_RX_SHIFT                      5
#define GPIO_HUB_DRV_TX                           0x0
#define GPIO_HUB_DRV_TX_MASK                      (0x7 << 6)
#define GPIO_HUB_DRV_TX_SHIFT                     6
#define GPIO_HUB_DRV_RX                           0x0
#define GPIO_HUB_DRV_RX_MASK                      (0x7 << 6)
#define GPIO_HUB_DRV_RX_SHIFT                     6
#define GPIO_HUB_SMT_TX                           0xE0
#define GPIO_HUB_SMT_TX_MASK                      (0x1 << 2)
#define GPIO_HUB_SMT_TX_SHIFT                     2
#define GPIO_HUB_SMT_RX                           0xE0
#define GPIO_HUB_SMT_RX_MASK                      (0x1 << 2)
#define GPIO_HUB_SMT_RX_SHIFT                     2
#define GPIO_HUB_TDSEL_TX                         0xF0
#define GPIO_HUB_TDSEL_TX_MASK                    (0xF << 8)
#define GPIO_HUB_TDSEL_TX_SHIFT                   8
#define GPIO_HUB_TDSEL_RX                         0xF0
#define GPIO_HUB_TDSEL_RX_MASK                    (0xF << 8)
#define GPIO_HUB_TDSEL_RX_SHIFT                   8
#define GPIO_HUB_RDSEL_TX                         0xC0
#define GPIO_HUB_RDSEL_TX_MASK                    (0x3 << 4)
#define GPIO_HUB_RDSEL_TX_SHIFT                   4
#define GPIO_HUB_RDSEL_RX                         0xC0
#define GPIO_HUB_RDSEL_RX_MASK                    (0x3 << 4)
#define GPIO_HUB_RDSEL_RX_SHIFT                   4
#define GPIO_HUB_SEC_EN_TX                        0xA00
#define GPIO_HUB_SEC_EN_TX_MASK                   (0x1 << 8)
#define GPIO_HUB_SEC_EN_TX_SHIFT                  8
#define GPIO_HUB_SEC_EN_RX                        0xA00
#define GPIO_HUB_SEC_EN_RX_MASK                   (0x1 << 9)
#define GPIO_HUB_SEC_EN_RX_SHIFT                  9

#define PERICFG_AO_BASE_ADDR                      0x16640000
#define PERI_CG_2                                 0x18
#define PERI_CG_2_UARTHUB_CG_MASK                 (0x7 << 18)
#define PERI_CG_2_UARTHUB_CG_SHIFT                18
#define PERI_CG_2_UARTHUB_CLK_CG_MASK             (0x3 << 19)
#define PERI_CG_2_UARTHUB_CLK_CG_SHIFT            19
#define PERI_CLOCK_CON                            0x20
#define PERI_UART_FBCLK_CKSEL_MASK                (0x1 << 3)
#define PERI_UART_FBCLK_CKSEL_SHIFT               3
#define PERI_UART_FBCLK_CKSEL_UART_CK             (0x1 << 3)
#define PERI_UART_FBCLK_CKSEL_26M_CK              (0x0 << 3)
#define PERI_UART_WAKEUP                          0x50
#define PERI_UART_WAKEUP_UART_GPHUB_SEL_MASK      0x10
#define PERI_UART_WAKEUP_UART_GPHUB_SEL_SHIFT     4

#define APMIXEDSYS_BASE_ADDR                      0x10000800
#define FENC_STATUS_CON0                          0x3c
#define RG_UNIVPLL_FENC_STATUS_MASK               (0x1 << 6)
#define RG_UNIVPLL_FENC_STATUS_SHIFT              6

#define TOPCKGEN_BASE_ADDR                        0x10000000
#define CLK_CFG_6                                 0x70
#define CLK_CFG_6_SET                             0x74
#define CLK_CFG_6_CLR                             0x78
#define CLK_CFG_6_UART_SEL_26M                    (0x0 << 24)
#define CLK_CFG_6_UART_SEL_104M                   (0x2 << 24)
#define CLK_CFG_6_UART_SEL_208M                   (0x3 << 24)
#define CLK_CFG_6_UART_SEL_MASK                   (0x3 << 24)
#define CLK_CFG_6_UART_SEL_SHIFT                  24
#define CLK_CFG_UPDATE                            0x4
#define CLK_CFG_UPDATE_UART_CK_UPDATE_MASK        (0x1 << 27)
#define CLK_CFG_16                                0x110
#define CLK_CFG_16_SET                            0x114
#define CLK_CFG_16_CLR                            0x118
#define CLK_CFG_16_UARTHUB_BCLK_SEL_26M           (0x0 << 24)
#define CLK_CFG_16_UARTHUB_BCLK_SEL_104M          (0x1 << 24)
#define CLK_CFG_16_UARTHUB_BCLK_SEL_208M          (0x2 << 24)
#define CLK_CFG_16_UARTHUB_BCLK_SEL_MASK          (0x3 << 24)
#define CLK_CFG_16_UARTHUB_BCLK_SEL_SHIFT         24
#define CLK_CFG_UPDATE2                           0xC
#define CLK_CFG_UPDATE2_UARTHUB_BCLK_UPDATE_MASK  (0x1 << 5)
#define CLK_CFG_13                                0xE0
#define CLK_CFG_13_SET                            0xE4
#define CLK_CFG_13_CLR                            0xE8
#define CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_26M      (0x0 << 24)
#define CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_104M     (0x1 << 24)
#define CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_208M     (0x2 << 24)
#define CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_MASK     (0x3 << 24)
#define CLK_CFG_13_ADSP_UARTHUB_BCLK_SEL_SHIFT    24
#define CLK_CFG_UPDATE1                           0x8
#define CLK_CFG_UPDATE1_ADSP_UARTHUB_BCLK_UPDATE_MASK (0x1 << 24)
#define CLK_CFG_16_PDN_UARTHUB_BCLK_MASK          (0x1 << 31)
#define CLK_CFG_16_PDN_UARTHUB_BCLK_SHIFT         31

#define SPM_BASE_ADDR                             0x1C004000

#define SPM_SYS_TIMER_L                           0x504
#define SPM_SYS_TIMER_H                           0x508

#define SPM_REQ_STA_14                            0x898
#define SPM_REQ_STA_14_UARTHUB_REQ_MASK           (0x1 << 31)
#define SPM_REQ_STA_14_UARTHUB_REQ_SHIFT          31

#define SPM_REQ_STA_15                            0x89C
#define SPM_REQ_STA_15_UARTHUB_REQ_MASK           (0xF << 0)
#define SPM_REQ_STA_15_UARTHUB_REQ_SHIFT          0

#define MD32PCM_SCU_CTRL0                         0x100
#define MD32PCM_SCU_CTRL0_SC_MD26M_CK_OFF_MASK    (0x1 << 5)
#define MD32PCM_SCU_CTRL0_SC_MD26M_CK_OFF_SHIFT   5

#define MD32PCM_SCU_CTRL1                         0x104
#define MD32PCM_SCU_CTRL1_SPM_HUB_INTL_ACK_MASK   (0x17 << 17)
#define MD32PCM_SCU_CTRL1_SPM_HUB_INTL_ACK_SHIFT  17

#define UART1_BASE_ADDR                           0x16010000
#define UART2_BASE_ADDR                           0x16020000
#define UART3_BASE_ADDR                           0x16030000
#define AP_DMA_UART_TX_INT_FLAG_ADDR              0x114F0000

#define SYS_SRAM_BASE_ADDR                        0x00113C00

#endif /* PLATFORM_DEF_ID_H */
