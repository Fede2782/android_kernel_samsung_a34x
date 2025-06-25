/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __UARTHUB_WAKEUP_REGS_MT6991_H__
#define __UARTHUB_WAKEUP_REGS_MT6991_H__

#include "common_def_id.h"

#ifndef REG_BASE_C_MODULE
// ----------------- INTFHUB Bit Field Definitions -------------------

extern unsigned long UARTHUB_WAKEUP_BASE_MT6991;

#define SPM_26M_ULPOSC_REQ_CTRL_ADDR            (UARTHUB_WAKEUP_BASE_MT6991 + 0x00)
#define SPM_26M_ULPOSC_ACK_STA_ADDR             (UARTHUB_WAKEUP_BASE_MT6991 + 0x04)
#define SSPM_WAKEUP_IRQ_STA_ADDR                (UARTHUB_WAKEUP_BASE_MT6991 + 0x08)
#define SSPM_WAKEUP_IRQ_STA_EN_ADDR             (UARTHUB_WAKEUP_BASE_MT6991 + 0x0C)
#define SSPM_WAKEUP_IRQ_STA_CLR_ADDR            (UARTHUB_WAKEUP_BASE_MT6991 + 0x10)
#define SSPM_WAKEUP_IRQ_STA_MASK_ADDR           (UARTHUB_WAKEUP_BASE_MT6991 + 0x14)
#define SPM_NOACK_TIMEOUT_SET_ADDR              (UARTHUB_WAKEUP_BASE_MT6991 + 0x18)
#define UARTHUB_NOACK_TIMEOUT_SET_ADDR          (UARTHUB_WAKEUP_BASE_MT6991 + 0x1C)

#endif


#define SPM_26M_ULPOSC_REQ_CTRL_FLD_spm_26m_ulposc_req         REG_FLD(1, 0)

#define SPM_26M_ULPOSC_ACK_STA_FLD_spm_26m_ulposc_ack          REG_FLD(1, 0)

#define SSPM_WAKEUP_IRQ_STA_FLD_uarhub2sspm_irq_b              REG_FLD(1, 4)
#define SSPM_WAKEUP_IRQ_STA_FLD_uarthub_noack_timeout          REG_FLD(1, 3)
#define SSPM_WAKEUP_IRQ_STA_FLD_spm_ack_irq                    REG_FLD(1, 2)
#define SSPM_WAKEUP_IRQ_STA_FLD_spm_noack_timeout              REG_FLD(1, 1)
#define SSPM_WAKEUP_IRQ_STA_FLD_rx_data_irq                    REG_FLD(1, 0)

#define SSPM_WAKEUP_IRQ_STA_EN_FLD_uarthub_noack_timeout_en    REG_FLD(1, 3)
#define SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_ack_irq_en              REG_FLD(1, 2)
#define SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_noack_timeout_en        REG_FLD(1, 1)
#define SSPM_WAKEUP_IRQ_STA_EN_FLD_rx_data_irq_en              REG_FLD(1, 0)

#define SSPM_WAKEUP_IRQ_STA_CLR_FLD_uarthub_noack_timeout_clr  REG_FLD(1, 3)
#define SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_ack_irq_clr            REG_FLD(1, 2)
#define SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_noack_timeout_clr      REG_FLD(1, 1)
#define SSPM_WAKEUP_IRQ_STA_CLR_FLD_rx_data_irq_clr            REG_FLD(1, 0)

#define SSPM_WAKEUP_IRQ_STA_MASK_FLD_uarthub_noack_timeout_mask REG_FLD(1, 3)
#define SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_ack_irq_mask          REG_FLD(1, 2)
#define SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_noack_timeout_mask    REG_FLD(1, 1)
#define SSPM_WAKEUP_IRQ_STA_MASK_FLD_rx_data_irq_mask          REG_FLD(1, 0)

#define SPM_NOACK_TIMEOUT_SET_FLD_spm_noack_timeout_set        REG_FLD(24, 0)

#define UARTHUB_NOACK_TIMEOUT_SET_FLD_uarthub_noack_timeout_set REG_FLD(24, 0)

#define SPM_26M_ULPOSC_REQ_CTRL_GET_spm_26m_ulposc_req(reg32)  REG_FLD_GET(SPM_26M_ULPOSC_REQ_CTRL_FLD_spm_26m_ulposc_req, (reg32))

#define SPM_26M_ULPOSC_ACK_STA_GET_spm_26m_ulposc_ack(reg32)   REG_FLD_GET(SPM_26M_ULPOSC_ACK_STA_FLD_spm_26m_ulposc_ack, (reg32))

#define SSPM_WAKEUP_IRQ_STA_GET_uarhub2sspm_irq_b(reg32)       REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_FLD_uarhub2sspm_irq_b, (reg32))
#define SSPM_WAKEUP_IRQ_STA_GET_uarthub_noack_timeout(reg32)   REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_FLD_uarthub_noack_timeout, (reg32))
#define SSPM_WAKEUP_IRQ_STA_GET_spm_ack_irq(reg32)             REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_FLD_spm_ack_irq, (reg32))
#define SSPM_WAKEUP_IRQ_STA_GET_spm_noack_timeout(reg32)       REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_FLD_spm_noack_timeout, (reg32))
#define SSPM_WAKEUP_IRQ_STA_GET_rx_data_irq(reg32)             REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_FLD_rx_data_irq, (reg32))

#define SSPM_WAKEUP_IRQ_STA_EN_GET_uarthub_noack_timeout_en(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_EN_FLD_uarthub_noack_timeout_en, (reg32))
#define SSPM_WAKEUP_IRQ_STA_EN_GET_spm_ack_irq_en(reg32)       REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_ack_irq_en, (reg32))
#define SSPM_WAKEUP_IRQ_STA_EN_GET_spm_noack_timeout_en(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_noack_timeout_en, (reg32))
#define SSPM_WAKEUP_IRQ_STA_EN_GET_rx_data_irq_en(reg32)       REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_EN_FLD_rx_data_irq_en, (reg32))

#define SSPM_WAKEUP_IRQ_STA_CLR_GET_uarthub_noack_timeout_clr(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_uarthub_noack_timeout_clr, (reg32))
#define SSPM_WAKEUP_IRQ_STA_CLR_GET_spm_ack_irq_clr(reg32)     REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_ack_irq_clr, (reg32))
#define SSPM_WAKEUP_IRQ_STA_CLR_GET_spm_noack_timeout_clr(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_noack_timeout_clr, (reg32))
#define SSPM_WAKEUP_IRQ_STA_CLR_GET_rx_data_irq_clr(reg32)     REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_rx_data_irq_clr, (reg32))

#define SSPM_WAKEUP_IRQ_STA_MASK_GET_uarthub_noack_timeout_mask(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_uarthub_noack_timeout_mask, (reg32))
#define SSPM_WAKEUP_IRQ_STA_MASK_GET_spm_ack_irq_mask(reg32)   REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_ack_irq_mask, (reg32))
#define SSPM_WAKEUP_IRQ_STA_MASK_GET_spm_noack_timeout_mask(reg32) REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_noack_timeout_mask, (reg32))
#define SSPM_WAKEUP_IRQ_STA_MASK_GET_rx_data_irq_mask(reg32)   REG_FLD_GET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_rx_data_irq_mask, (reg32))

#define SPM_NOACK_TIMEOUT_SET_GET_spm_noack_timeout_set(reg32) REG_FLD_GET(SPM_NOACK_TIMEOUT_SET_FLD_spm_noack_timeout_set, (reg32))

#define UARTHUB_NOACK_TIMEOUT_SET_GET_uarthub_noack_timeout_set(reg32) REG_FLD_GET(UARTHUB_NOACK_TIMEOUT_SET_FLD_uarthub_noack_timeout_set, (reg32))

#define SPM_26M_ULPOSC_REQ_CTRL_SET_spm_26m_ulposc_req(reg32, val) REG_FLD_SET(SPM_26M_ULPOSC_REQ_CTRL_FLD_spm_26m_ulposc_req, (reg32), (val))

#define SPM_26M_ULPOSC_ACK_STA_SET_spm_26m_ulposc_ack(reg32, val) REG_FLD_SET(SPM_26M_ULPOSC_ACK_STA_FLD_spm_26m_ulposc_ack, (reg32), (val))

#define SSPM_WAKEUP_IRQ_STA_SET_uarhub2sspm_irq_b(reg32, val)  REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_FLD_uarhub2sspm_irq_b, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_SET_uarthub_noack_timeout(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_FLD_uarthub_noack_timeout, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_SET_spm_ack_irq(reg32, val)        REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_FLD_spm_ack_irq, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_SET_spm_noack_timeout(reg32, val)  REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_FLD_spm_noack_timeout, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_SET_rx_data_irq(reg32, val)        REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_FLD_rx_data_irq, (reg32), (val))

#define SSPM_WAKEUP_IRQ_STA_EN_SET_uarthub_noack_timeout_en(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_EN_FLD_uarthub_noack_timeout_en, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_EN_SET_spm_ack_irq_en(reg32, val)  REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_ack_irq_en, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_EN_SET_spm_noack_timeout_en(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_noack_timeout_en, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_EN_SET_rx_data_irq_en(reg32, val)  REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_EN_FLD_rx_data_irq_en, (reg32), (val))

#define SSPM_WAKEUP_IRQ_STA_CLR_SET_uarthub_noack_timeout_clr(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_uarthub_noack_timeout_clr, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_SET_spm_ack_irq_clr(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_ack_irq_clr, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_SET_spm_noack_timeout_clr(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_noack_timeout_clr, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_SET_rx_data_irq_clr(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_CLR_FLD_rx_data_irq_clr, (reg32), (val))

#define SSPM_WAKEUP_IRQ_STA_MASK_SET_uarthub_noack_timeout_mask(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_uarthub_noack_timeout_mask, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_SET_spm_ack_irq_mask(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_ack_irq_mask, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_SET_spm_noack_timeout_mask(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_noack_timeout_mask, (reg32), (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_SET_rx_data_irq_mask(reg32, val) REG_FLD_SET(SSPM_WAKEUP_IRQ_STA_MASK_FLD_rx_data_irq_mask, (reg32), (val))

#define SPM_NOACK_TIMEOUT_SET_SET_spm_noack_timeout_set(reg32, val) REG_FLD_SET(SPM_NOACK_TIMEOUT_SET_FLD_spm_noack_timeout_set, (reg32), (val))

#define UARTHUB_NOACK_TIMEOUT_SET_SET_uarthub_noack_timeout_set(reg32, val) REG_FLD_SET(UARTHUB_NOACK_TIMEOUT_SET_FLD_uarthub_noack_timeout_set, (reg32), (val))

#define SPM_26M_ULPOSC_REQ_CTRL_VAL_spm_26m_ulposc_req(val)    REG_FLD_VAL(SPM_26M_ULPOSC_REQ_CTRL_FLD_spm_26m_ulposc_req, (val))

#define SPM_26M_ULPOSC_ACK_STA_VAL_spm_26m_ulposc_ack(val)     REG_FLD_VAL(SPM_26M_ULPOSC_ACK_STA_FLD_spm_26m_ulposc_ack, (val))

#define SSPM_WAKEUP_IRQ_STA_VAL_uarhub2sspm_irq_b(val)         REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_FLD_uarhub2sspm_irq_b, (val))
#define SSPM_WAKEUP_IRQ_STA_VAL_uarthub_noack_timeout(val)     REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_FLD_uarthub_noack_timeout, (val))
#define SSPM_WAKEUP_IRQ_STA_VAL_spm_ack_irq(val)               REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_FLD_spm_ack_irq, (val))
#define SSPM_WAKEUP_IRQ_STA_VAL_spm_noack_timeout(val)         REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_FLD_spm_noack_timeout, (val))
#define SSPM_WAKEUP_IRQ_STA_VAL_rx_data_irq(val)               REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_FLD_rx_data_irq, (val))

#define SSPM_WAKEUP_IRQ_STA_EN_VAL_uarthub_noack_timeout_en(val) REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_EN_FLD_uarthub_noack_timeout_en, (val))
#define SSPM_WAKEUP_IRQ_STA_EN_VAL_spm_ack_irq_en(val)         REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_ack_irq_en, (val))
#define SSPM_WAKEUP_IRQ_STA_EN_VAL_spm_noack_timeout_en(val)   REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_EN_FLD_spm_noack_timeout_en, (val))
#define SSPM_WAKEUP_IRQ_STA_EN_VAL_rx_data_irq_en(val)         REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_EN_FLD_rx_data_irq_en, (val))

#define SSPM_WAKEUP_IRQ_STA_CLR_VAL_uarthub_noack_timeout_clr(val) REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_CLR_FLD_uarthub_noack_timeout_clr, (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_VAL_spm_ack_irq_clr(val)       REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_ack_irq_clr, (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_VAL_spm_noack_timeout_clr(val) REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_CLR_FLD_spm_noack_timeout_clr, (val))
#define SSPM_WAKEUP_IRQ_STA_CLR_VAL_rx_data_irq_clr(val)       REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_CLR_FLD_rx_data_irq_clr, (val))

#define SSPM_WAKEUP_IRQ_STA_MASK_VAL_uarthub_noack_timeout_mask(val) REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_MASK_FLD_uarthub_noack_timeout_mask, (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_VAL_spm_ack_irq_mask(val)     REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_ack_irq_mask, (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_VAL_spm_noack_timeout_mask(val) REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_MASK_FLD_spm_noack_timeout_mask, (val))
#define SSPM_WAKEUP_IRQ_STA_MASK_VAL_rx_data_irq_mask(val)     REG_FLD_VAL(SSPM_WAKEUP_IRQ_STA_MASK_FLD_rx_data_irq_mask, (val))

#define SPM_NOACK_TIMEOUT_SET_VAL_spm_noack_timeout_set(val)   REG_FLD_VAL(SPM_NOACK_TIMEOUT_SET_FLD_spm_noack_timeout_set, (val))

#define UARTHUB_NOACK_TIMEOUT_SET_VAL_uarthub_noack_timeout_set(val) REG_FLD_VAL(UARTHUB_NOACK_TIMEOUT_SET_FLD_uarthub_noack_timeout_set, (val))

#endif // __UARTHUB_WAKEUP_REGS_MT6991_H__
