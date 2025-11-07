/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUEB_DEBUG_H__
#define __GPUEB_DEBUG_H__

#include <linux/platform_device.h>

/**************************************************
 * Definition
 **************************************************/
#define WDT_EXCEPTION_EN                                "5A5A5A5A"

#define GPUEB_INTC_BASE                       (g_gpueb_intc_base)
#define GPUEB_INTC_IRQ_EN_L                   (GPUEB_INTC_BASE + 0x0000)
#define GPUEB_INTC_IRQ_STA_L                  (GPUEB_INTC_BASE + 0x0038)
#define GPUEB_INTC_IRQ_RAW_STA_L              (GPUEB_INTC_BASE + 0x003C)

#define GPUEB_CFGREG_BASE                     (g_gpueb_cfgreg_base)
#define GPUEB_CFGREG_SW_RSTN                  (GPUEB_CFGREG_BASE + 0x0000)
#define GPUEB_CFGREG_AXI_STA                  (GPUEB_CFGREG_BASE + 0x0010)
#define GPUEB_CFGREG_WDT_CON                  (GPUEB_CFGREG_BASE + 0x0018)
#define GPUEB_CFGREG_WDT_KICK                 (GPUEB_CFGREG_BASE + 0x001C)
#define GPUEB_CFGREG_MDSP_CFG                 (GPUEB_CFGREG_BASE + 0x0034)
#define MDSP_HALT_BIT                         (0x1U << 4)
#define MDSP_GATED_BIT                        (0x1U << 5)
#define GPUEB_ON_WFI_BITS                     (MDSP_HALT_BIT | MDSP_GATED_BIT)
#define GPUEB_CFGREG_DBG_APB_PC               (GPUEB_CFGREG_BASE + 0x011C)
#define GPUEB_CFGREG_DBG_APB_LR               (GPUEB_CFGREG_BASE + 0x0120)
#define GPUEB_CFGREG_DBG_APB_SP               (GPUEB_CFGREG_BASE + 0x0124)
#define MFG_GPUEB_AXI_BIST_CON_DEBUG          (GPUEB_CFGREG_BASE + 0x01D0)
#define MFG_GPUEB_GPUEB_AXI_BIST_CON_CONFIG   (GPUEB_CFGREG_BASE + 0x01D4)

#define GPUEB_DMA_BASE_CH0                    (g_gpueb_dma_base + 0x0000)
#define GPUEB_DMA_BASE_CH1                    (g_gpueb_dma_base + 0x0100)
#define GPUEB_DMA_STATE_CH0                   (GPUEB_DMA_BASE_CH0 + 0x0038)
#define GPUEB_DMA_STATE_CH1                   (GPUEB_DMA_BASE_CH1 + 0x0038)

#if defined(CONFIG_PROC_FS)
#define PROC_FOPS_RW(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				pde_data(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
		.proc_write = name ## _proc_write, \
	}

#define PROC_FOPS_RO(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				pde_data(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
	}

#define PROC_ENTRY(name)              \
	{                                 \
		__stringify(name),            \
		&name ## _proc_fops           \
	}
#endif /* CONFIG_PROC_FS */

/**************************************************
 * Enumeration and Structure
 **************************************************/
static char *gpueb_dram_user_name[] = {
	"GPU_PWR_ON",          //0
	"GPUFREQ",             //1
	"GPUMPU",              //2
	"GPUEB_MET",           //3
	"LOGGER",              //4
	"PLATSERV",            //5
	"REMAP_TEST",          //6
};

/**************************************************
 * Function
 **************************************************/
void gpueb_debug_init(struct platform_device *pdev);
void gpueb_trigger_wdt(const char *name);
void gpueb_dump_status(char *log_buf, int *log_len, int log_size);

#endif /* __GPUEB_DEBUG_H__ */
