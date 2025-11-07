/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef PLATFORM_DEF_ID_H
#define PLATFORM_DEF_ID_H

/*GPIO*/
#define GPIO_BASE_ADDR                            (0x1002D000)
#define GPIO_HUB_MODE_TX                          (0x4D0)
#define GPIO_HUB_MODE_TX_SHIFT                    (20)
#define GPIO_HUB_MODE_TX_MASK                     (0xF << GPIO_HUB_MODE_TX_SHIFT)
#define GPIO_HUB_MODE_TX_VALUE                    (0x1 << GPIO_HUB_MODE_TX_SHIFT)
#define GPIO_HUB_MODE_RX                          (GPIO_HUB_MODE_TX)
#define GPIO_HUB_MODE_RX_SHIFT                    (24)
#define GPIO_HUB_MODE_RX_MASK                     (0xF << GPIO_HUB_MODE_RX_SHIFT)
#define GPIO_HUB_MODE_RX_VALUE                    (0x1 << GPIO_HUB_MODE_RX_SHIFT)

#define GPIO_HUB_MODE_TX_DIR                      (0x70)
#define GPIO_HUB_MODE_TX_DIR_SHIFT                (13)
#define GPIO_HUB_MODE_TX_DIR_MASK                 (0x1 << GPIO_HUB_MODE_TX_DIR_SHIFT)
#define GPIO_HUB_MODE_RX_DIR                      (0x70)
#define GPIO_HUB_MODE_RX_DIR_SHIFT                (14)
#define GPIO_HUB_MODE_RX_DIR_MASK                 (0x1 << GPIO_HUB_MODE_RX_DIR_SHIFT)

#define GPIO_HUB_MODE_TX_DATAOUT                  (0x170)
#define GPIO_HUB_MODE_TX_DATAOUT_SHIFT            (13)
#define GPIO_HUB_MODE_TX_DATAOUT_MASK             (0x1 << GPIO_HUB_MODE_TX_DATAOUT_SHIFT)
#define GPIO_HUB_MODE_RX_DATAOUT                  (0x170)
#define GPIO_HUB_MODE_RX_DATAOUT_SHIFT            (14)
#define GPIO_HUB_MODE_RX_DATAOUT_MASK             (0x1 << GPIO_HUB_MODE_RX_DATAOUT_SHIFT)

#define GPIO_BT_RST_PIN                           (0x4F0)
#define GPIO_BT_RST_PIN_SHIFT                     (0)
#define GPIO_BT_RST_PIN_MASK                      (0xF << GPIO_BT_RST_PIN_SHIFT)
#define GPIO_BT_RST_PIN_DIR                       (0x70)
#define GPIO_BT_RST_PIN_DIR_SHIFT                 (24)
#define GPIO_BT_RST_PIN_DIR_MASK                  (0x1 << GPIO_BT_RST_PIN_DIR_SHIFT)
#define GPIO_BT_RST_PIN_DATAOUT                   (0x170)
#define GPIO_BT_RST_PIN_DATAOUT_SHIFT             (24)
#define GPIO_BT_RST_PIN_DATAOUT_MASK              (0x1 << GPIO_BT_RST_PIN_DATAOUT_SHIFT)

/*IOCFG_TM3*/
#define IOCFG_TM3_BASE_ADDR                       (0x13860000)
#define GPIO_HUB_MODE_TX_PU                       (0x90)
#define GPIO_HUB_MODE_TX_PU_SHIFT                 (6)
#define GPIO_HUB_MODE_TX_PU_MASK                  (0x1 << GPIO_HUB_MODE_TX_PU_SHIFT)
#define GPIO_HUB_MODE_RX_PU                       (0x90)
#define GPIO_HUB_MODE_RX_PU_SHIFT                 (5)
#define GPIO_HUB_MODE_RX_PU_MASK                  (0x1 << GPIO_HUB_MODE_RX_PU_SHIFT)
#define GPIO_BT_RST_PIN_PU                        (0x90)
#define GPIO_BT_RST_PIN_PU_SHIFT                  (4)
#define GPIO_BT_RST_PIN_PU_MASK                   (0x1 << GPIO_BT_RST_PIN_PU_SHIFT)

#define GPIO_HUB_MODE_TX_PD                       (0x70)
#define GPIO_HUB_MODE_TX_PD_SHIFT                 (6)
#define GPIO_HUB_MODE_TX_PD_MASK                  (0x1 << GPIO_HUB_MODE_TX_PD_SHIFT)
#define GPIO_HUB_MODE_RX_PD                       (0x70)
#define GPIO_HUB_MODE_RX_PD_SHIFT                 (5)
#define GPIO_HUB_MODE_RX_PD_MASK                  (0x1 << GPIO_HUB_MODE_RX_PD_SHIFT)
#define GPIO_BT_RST_PIN_PD                        (0x70)
#define GPIO_BT_RST_PIN_PD_SHIFT                  (4)
#define GPIO_BT_RST_PIN_PD_MASK                   (0x1 << GPIO_BT_RST_PIN_PD_SHIFT)

/*PERI CFG*/
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
