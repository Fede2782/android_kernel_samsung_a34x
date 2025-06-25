// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/soc/mediatek/devapc_public.h>

#include "devapc-mt6991.h"

#define EXCEPTION_NODE_NAME    "exception"

#define DEVAPC_EXCEP_NAME_LEN	(32)

enum DEVAPC_EXCEP_HANDLER_TYPE {
	DEVAPC_EXCEP_HANDLER_DEBUGSYS,
	DEVAPC_EXCEP_HANDLER_MAX,
};

struct tag_chipid {
	uint32_t size;
	uint32_t hw_code;
	uint32_t hw_subcode;
	uint32_t hw_ver;
	uint32_t sw_ver;
};

struct devapc_excep_entry {
	char name[DEVAPC_EXCEP_NAME_LEN];
	uint32_t excep_type;
	uint32_t slave_type;
	void __iomem *base;
};

typedef bool (*devpapc_excep_cb_t)(struct devapc_excep_entry *excep);

struct devapc_excep_info {
	uint32_t entry_cnt;
	devpapc_excep_cb_t excep_type_callback[DEVAPC_EXCEP_HANDLER_MAX];
	struct devapc_excep_entry excep_list[DEVAPC_EXCEP_HANDLER_MAX];
};

static int g_sw_ver = 1;

static struct devapc_excep_info excep_info;

static struct mtk_device_num mtk6991_devices_num[] = {
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_IO,
		IRQ_TYPE_APINFRA_IO,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_IO_CTRL,
		IRQ_TYPE_APINFRA_IO_CTRL,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_IO_INTF,
		IRQ_TYPE_APINFRA_IO_INTF,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_BIG4,
		IRQ_TYPE_APINFRA_BIG4,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_DRAMC,
		IRQ_TYPE_APINFRA_DRAMC,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_EMI,
		IRQ_TYPE_APINFRA_EMI,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_SSR,
		IRQ_TYPE_APINFRA_SSR,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_MEM,
		IRQ_TYPE_APINFRA_MEM,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_MEM_CTRL,
		IRQ_TYPE_APINFRA_MEM_CTRL,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_MEM_INTF,
		IRQ_TYPE_APINFRA_MEM_INTF,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_INT,
		IRQ_TYPE_APINFRA_INT,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_MMU,
		IRQ_TYPE_APINFRA_MMU,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_INFRA,
		VIO_SLAVE_NUM_APINFRA_SLB,
		IRQ_TYPE_APINFRA_SLB,
		DEVAPC_GET_INFRA
	},
	{
		DEVAPC_TYPE_PERI_PAR,
		VIO_SLAVE_NUM_PERI_PAR,
		IRQ_TYPE_PERI,
		DEVAPC_GET_PERI
	},
	{
		DEVAPC_TYPE_VLP,
		VIO_SLAVE_NUM_VLP,
		IRQ_TYPE_VLP,
		DEVAPC_GET_VLP
	},
	{
		DEVAPC_TYPE_ADSP,
		VIO_SLAVE_NUM_ADSP,
		IRQ_TYPE_ADSP,
		DEVAPC_GET_ADSP
	},
	{
		DEVAPC_TYPE_MMINFRA,
		VIO_SLAVE_NUM_MMINFRA,
		IRQ_TYPE_MMINFRA,
		DEVAPC_GET_MMINFRA
	},
	{
		DEVAPC_TYPE_MMUP,
		VIO_SLAVE_NUM_MMUP,
		IRQ_TYPE_MMUP,
		DEVAPC_GET_MMUP
	},
	{
		DEVAPC_TYPE_GPU,
		VIO_SLAVE_NUM_GPU,
		IRQ_TYPE_GPU,
		DEVAPC_GET_GPU
	},
	{
		DEVAPC_TYPE_GPU1,
		VIO_SLAVE_NUM_GPU1,
		IRQ_TYPE_GPU,
		DEVAPC_GET_GPU
	},
};

static const struct INFRAAXI_ID_INFO infra_mi_tracer0_id_to_master[] = {
	{"MCU_AP_M",			{ 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"MD_AP_M",			{ 1, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"NOR_M",			{ 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"APDMA_EXT_M",			{ 0, 1, 0, 0, 0, 0, 0, 1, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"MSDC1_M",			{ 0, 1, 0, 0, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"MSDC2_M",			{ 0, 1, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"ETHERNET0_M",			{ 0, 1, 0, 0, 0, 0, 1, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"ETHERNET1_M",			{ 0, 1, 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"UFS0_M",			{ 0, 1, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"SPI0_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI1_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI2_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI3_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"PWM_M@AHB-M",			{ 0, 1, 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI4_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI5_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI6_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI7_M@AHB-M",		{ 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APDMA_INT_M",			{ 0, 1, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"PCIE0_M",			{ 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 } },
	{"PCIE1_M",			{ 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 1, 0, 0 } },
	{"USB0_M",			{ 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 0, 0, 0, 1, 1, 0, 0 } },
	{"HWCCF_M@APB",			{ 0, 1, 1, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"IPU_M@APB",			{ 0, 1, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"APINFRA_IO_BUS_HRE_M@APB",	{ 0, 1, 1, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"NTH_EMI_GMC_M",		{ 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"STH_EMI_GMC_M",		{ 0, 1, 1, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"IRQ2AXI_M",			{ 0, 1, 0, 1, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"DEBUG_M",			{ 0, 1, 1, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CQDMA_M",			{ 0, 1, 1, 1, 0, 1, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CPUM_M",			{ 0, 1, 1, 1, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"GPU_EB_M",			{ 0, 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"MM2SLB1_M",			{ 1, 1, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"CCU2_M",			{ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"CCU3_M",			{ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 } },
	{"HFRP2INFRA_M",		{ 1, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1 } },
	{"GCE_D_M",			{ 1, 1, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"GCE_M_M",			{ 1, 1, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"SSR_M",			{ 1, 1, 0, 1, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMAIF_M",			{ 1, 1, 0, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"ADSPSYS_M1_M",		{ 1, 1, 1, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"CONN_M",			{ 1, 1, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"VLPSYS_M",			{ 1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SCP_M",			{ 1, 1, 1, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"SSPM_M",			{ 1, 1, 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SPM_M",			{ 1, 1, 1, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMSR_AHB_M",			{ 1, 1, 1, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
};

static const struct INFRAAXI_ID_INFO infra_mi_tracer1_id_to_master[] = {
	{"MD_AP_M",			{ 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MM2SLB1_M",			{ 1, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"CCU2_M",			{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"CCU3_M",			{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 } },
	{"HFRP2INFRA_M",		{ 1, 0, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1 } },
	{"GCE_D_M",			{ 1, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"GCE_M_M",			{ 1, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } },
	{"SSR_M",			{ 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMAIF_M",			{ 1, 0, 0, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"ADSPSYS_M1_M",		{ 1, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"CONN_M",			{ 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"VLPSYS_M",			{ 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SCP_M",			{ 1, 0, 1, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"SSPM_M",			{ 1, 0, 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SPM_M",			{ 1, 0, 1, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMSR_AHB_M",			{ 1, 0, 1, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SSR_M",			{ 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MCU_AP_M",			{ 1, 1, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 } },
	{"NOR_M",			{ 1, 1, 1, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"APDMA_EXT_M",			{ 1, 1, 1, 0, 0, 0, 0, 1, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"MSDC1_M",			{ 1, 1, 1, 0, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"MSDC2_M",			{ 1, 1, 1, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"ETHERNET0_M",			{ 1, 1, 1, 0, 0, 0, 1, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"ETHERNET1_M",			{ 1, 1, 1, 0, 0, 0, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"UFS0_M",			{ 1, 1, 1, 0, 0, 0, 2, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 } },
	{"SPI0_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI1_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI2_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI3_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"PWM_M@AHB-M",			{ 1, 1, 1, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI4_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI5_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI6_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"SPI7_M@AHB-M",		{ 1, 1, 1, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"APDMA_INT_M",			{ 1, 1, 1, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"PCIE0_M",			{ 1, 1, 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 } },
	{"PCIE1_M",			{ 1, 1, 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 1, 0, 0 } },
	{"USB0_M",			{ 1, 1, 1, 0, 0, 1, 2, 2, 2, 2, 2, 0, 0, 0, 1, 1, 0, 0 } },
	{"HWCCF_M@APB",			{ 1, 1, 0, 1, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"IPU_M@APB",			{ 1, 1, 0, 1, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"APINFRA_IO_BUS_HRE_M@APB",	{ 1, 1, 0, 1, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"NTH_EMI_GMC_M",		{ 1, 1, 0, 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"STH_EMI_GMC_M",		{ 1, 1, 0, 1, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"DEBUG_M",			{ 1, 1, 1, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"FAXI_ALL_I8B",		{ 1, 1, 1, 1, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CQDMA_M",			{ 1, 1, 1, 1, 0, 1, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CPUM_M",			{ 1, 1, 1, 1, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"GPU_EB_M",			{ 1, 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi12_id_to_master[] = {
	{"DSP_M1",            { 0, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M1",            { 0, 0, 1, 2, 0, 0, 0, 0  } },
	{"DSP_M2",            { 1, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M2",            { 1, 0, 1, 2, 0, 0, 0, 0  } },
	{"DMA_AXI_M1",        { 0, 1, 0, 0, 2, 2, 2, 2  } },
	{"IDMA_M",            { 0, 1, 1, 0, 2, 2, 2, 2  } },
	{"TINYXNNE",          { 0, 1, 0, 1, 2, 2, 2, 2  } },
	{"MASRC",             { 1, 1, 0, 2, 2, 2, 2, 0  } },
	{"AFE_M",             { 1, 1, 1, 2, 0, 0, 0, 0  } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi17_id_to_master[] = {
	{"DSP_M1",            { 0, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M1",            { 0, 0, 1, 2, 0, 0, 0, 0  } },
	{"DSP_M2",            { 1, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M2",            { 1, 0, 1, 2, 0, 0, 0, 0  } },
	{"DMA_AXI_M1",        { 0, 1, 0, 0, 2, 2, 2, 2  } },
	{"IDMA_M",            { 0, 1, 1, 0, 2, 2, 2, 2  } },
	{"TINYXNNE",          { 0, 1, 0, 1, 2, 2, 2, 2  } },
	{"INFRA_M",           { 1, 1, 2, 2, 2, 2, 0, 2  } },
};

static const char * const adsp_domain[] = {
	"AP",
	"SSPM",
	"CONN",
	"SCP",
	"MCUPM",
	"CCU",
	"others",
	"others",
};

static const char * const mminfra_domain[] = {
	"AP",
	"SSPM",
	"GZ",
	"SCP",
	"GCEM",
	"GCEM",
	"GCEM",
	"GCED",
	"GCED",
	"GCED",
	"GCED",
	"GCED",
	"CCU",
	"HFRP",
	"HFRP",
	"others",
};

static const char *infra_mi_tracer0_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(infra_mi_tracer0_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_INFRA_TRACER0";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < APINFRAAXI_MI_BIT_LENGTH; j++) {
			if (infra_mi_tracer0_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					infra_mi_tracer0_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == APINFRAAXI_MI_BIT_LENGTH) {
			pr_info(PFX "%s %s %s\n",
				"catch it from TRACER0",
				"Master is:",
				infra_mi_tracer0_id_to_master[i].master);
			master = infra_mi_tracer0_id_to_master[i].master;
		}
	}

	return master;
}

static const char *infra_mi_tracer1_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(infra_mi_tracer1_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_INFRA_TRACER1";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < APINFRAAXI_MI_BIT_LENGTH; j++) {
			if (infra_mi_tracer1_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					infra_mi_tracer1_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == APINFRAAXI_MI_BIT_LENGTH) {
			pr_info(PFX "%s %s %s\n",
				"catch it from TRACER1",
				"Master is:",
				infra_mi_tracer1_id_to_master[i].master);
			master = infra_mi_tracer1_id_to_master[i].master;
		}
	}

	return master;
}

static const char *infra_mi_trans(uint32_t vio_addr, uint32_t bus_id)
{
	if (((vio_addr >= APINFRA_DEBUG_START) && (vio_addr <= APINFRA_DEBUG_END)) ||
		((vio_addr >= APINFRA_HFRP_START) && (vio_addr <= APINFRA_HFRP_END)) ||
		((vio_addr >= APINFRA_MFG_START) && (vio_addr <= APINFRA_MFG_END))) {
		return infra_mi_tracer1_trans(bus_id);
	} else if (((vio_addr >= APINFRA_PERI_0_START) && (vio_addr <= APINFRA_PERI_0_END)) ||
		((vio_addr >= APINFRA_PERI_1_START) && (vio_addr <= APINFRA_PERI_1_END))) {
		return infra_mi_tracer1_trans(bus_id);
	} else if (((vio_addr >= APINFRA_SSR_START) && (vio_addr <= APINFRA_SSR_END))) {
		return infra_mi_tracer1_trans(bus_id);
	} else if (((vio_addr >= APINFRA_MMU_0_START) && (vio_addr <= APINFRA_MMU_0_END)) ||
		((vio_addr >= APINFRA_MMU_1_START) && (vio_addr <= APINFRA_MMU_1_END)) ||
		((vio_addr >= APINFRA_MMU_2_START) && (vio_addr <= APINFRA_MMU_2_END)) ||
		((vio_addr >= APINFRA_MMU_3_START) && (vio_addr <= APINFRA_MMU_3_END))) {
		if ((bus_id & 0x1))
			return infra_mi_tracer1_trans(bus_id >> 1);
		else
			return "APINFRA_MEM_BUS_HRE_M@APB";
	} else if (((vio_addr >= APINFRA_SLB_0_START) && (vio_addr <= APINFRA_SLB_0_END)) ||
		((vio_addr >= APINFRA_SLB_1_START) && (vio_addr <= APINFRA_SLB_1_END)) ||
		((vio_addr >= APINFRA_SLB_2_START) && (vio_addr <= APINFRA_SLB_2_END)) ||
		((vio_addr >= APINFRA_SLB_3_START) && (vio_addr <= APINFRA_SLB_3_END))) {
		if ((bus_id & 0x1))
			return infra_mi_tracer1_trans(bus_id >> 1);
		else
			return "APINFRA_MEM_BUS_HRE_M@APB";
	} else
		return infra_mi_tracer0_trans(bus_id);

}


static const char *adsp_mi_trans(uint32_t bus_id, int mi)
{
	int master_count = 0;
	const char *master = "UNKNOWN_MASTER_FROM_ADSP";
	int i, j;

	const struct ADSPAXI_ID_INFO *ADSP_mi_id_to_master;

	if (mi == ADSP_MI12) {
		ADSP_mi_id_to_master = ADSP_mi12_id_to_master;
		master_count = ARRAY_SIZE(ADSP_mi12_id_to_master);
	} else {
		ADSP_mi_id_to_master = ADSP_mi17_id_to_master;
		master_count = ARRAY_SIZE(ADSP_mi17_id_to_master);
	}

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < ADSPAXI_MI_BIT_LENGTH; j++) {
			if (ADSP_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
				ADSP_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == ADSPAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from ADSPAXI_MI",
				"Master is:",
				ADSP_mi_id_to_master[i].master);
			master = ADSP_mi_id_to_master[i].master;
		}
	}
	return master;
}

static const char *mt6991_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, uint32_t domain)
{
	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (vio_addr <= SRAM_END_ADDR) {
		pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
		return infra_mi_tracer0_trans(bus_id);
	} else if (slave_type == SLAVE_TYPE_VLP) {
		/* mi3 */
		if ((vio_addr >= VLP_SCP_START_ADDR) && (vio_addr <= VLP_SCP_END_ADDR)) {
			if ((bus_id & 0x3) == 0x0)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return infra_mi_trans(vio_addr, bus_id >> 2);
			else
				return "DPMSR_AHB_M";
		/* mi1 */
		} else if ((vio_addr >= VLP_INFRA_START && vio_addr <= VLP_INFRA_END) ||
			(vio_addr >= VLP_INFRA_1_START)) {
			if ((bus_id & 0x3) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return "SPM_M";
			else
				return "DPMSR_AHB_M";
		/* mi2 */
		} else {
			if ((bus_id & 0x7) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x7) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x7) == 0x2)
				return "SPM_M";
			else if ((bus_id & 0x7) == 0x3)
				return infra_mi_trans(vio_addr, bus_id >> 3);
			else
				return "DPMSR_AHB_M";
		}
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		/* infra slave */
		if ((vio_addr >= ADSP_INFRA_START && vio_addr <= ADSP_INFRA_END) ||
			(vio_addr >= ADSP_INFRA_1_START && vio_addr <= ADSP_INFRA_1_END) ||
			(vio_addr >= ADSP_INFRA_2_START && vio_addr <= ADSP_INFRA_2_END)) {
			return adsp_mi_trans(bus_id, ADSP_MI12);
		/* adsp misc slave */
		} else if (vio_addr >= ADSP_OTHER_START && vio_addr <= ADSP_OTHER_END) {
			if ((bus_id & 0x1) == 0x1)
				return "HRE";
			else if ((bus_id & 0x7) == 0x6)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id >> 1, ADSP_MI17);
		/* adsp audio_pwr, dsp_pwr slave */
		} else {
			if ((bus_id & 0x3) == 0x3)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id, ADSP_MI17);
		}
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		/* ISP slave */
		if (((vio_addr >= IMG_START_ADDR) && (vio_addr <= IMG_END_ADDR)) ||
			((vio_addr >= CAM_START_ADDR) && (vio_addr <= CAM_END_ADDR))) {
			if ((bus_id & 0x1) == 0x0)
				return "GCEM";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(vio_addr, bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "CCU";
			else if ((bus_id & 0xf) == 0x5)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x7)
				return "MMINFRA_HRE2";
			else if ((bus_id & 0xf) == 0x9)
				return "HFRP";
			else if ((bus_id & 0xf) == 0xb)
				return "GCED";
			else
				return mminfra_domain[domain];

		/* VENC/VDEC slave*/
		} else if ((vio_addr >= CODEC_START_ADDR) && (vio_addr <= CODEC_END_ADDR)) {
			if ((bus_id & 0x1) == 0x0)
				return "HFRP";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(vio_addr, bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "CCU";
			else if ((bus_id & 0xf) == 0x5)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x7)
				return "MMINFRA_HRE2";
			else if ((bus_id & 0xf) == 0xb)
				return "GCED";
			else if ((bus_id & 0xf) == 0xd)
				return "GCEM";
			else
				return mminfra_domain[domain];

		/* DISP/OVL/MML */
		} else if (((vio_addr >= DISP_START_ADDR) && (vio_addr <= DISP_END_ADDR)) ||
			((vio_addr >= OVL_START_ADDR) && (vio_addr <= OVL_END_ADDR)) ||
			((vio_addr >= MML_START_ADDR) && (vio_addr <= MML_END_ADDR))) {
			if ((bus_id & 0x1) == 0x0)
				return "GCED";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(vio_addr, bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "CCU";
			else if ((bus_id & 0xf) == 0x5)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x7)
				return "MMINFRA_HRE2";
			else if ((bus_id & 0xf) == 0x9)
				return "HFRP";
			else if ((bus_id & 0xf) == 0xd)
				return "GCEM";
			else
				return mminfra_domain[domain];

		/* other mminfra slave*/
		} else {
			if ((bus_id & 0x7) == 0x0)
				return infra_mi_trans(vio_addr, bus_id >> 3);
			else if ((bus_id & 0x7) == 0x1)
				return "CCU";
			else if ((bus_id & 0x7) == 0x2)
				return "MMINFRA_HRE";
			else if ((bus_id & 0x7) == 0x3)
				return "MMINFRA_HRE2";
			else if ((bus_id & 0xf) == 0x4)
				return "HFRP";
			else if ((bus_id & 0xf) == 0x5)
				return "GCED";
			else if ((bus_id & 0xf) == 0x6)
				return "GCEM";
			else
				return mminfra_domain[domain];
		}
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		return mminfra_domain[domain];
	} else if (slave_type == SLAVE_TYPE_GPU) {
		/* PD_BUS */
		if (domain == 0x6) {
			if (((bus_id & 0xf800) == 0x0) && ((bus_id & 0x3f) == 0x2a)) {
				if (((bus_id >> 6) & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if (((bus_id >> 6) & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if (((bus_id >> 6) & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return "GPU_BRCAST";
		} else
			return infra_mi_trans(vio_addr, bus_id);
	} else if (slave_type == SLAVE_TYPE_GPU1) {
		/* PD_BUS */
		if ((vio_addr >= GPU1_PD_START) && (vio_addr <= GPU1_PD_END)) {
			if (domain == 0x6) {
				if ((bus_id & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if ((bus_id & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if ((bus_id & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return infra_mi_trans(vio_addr, bus_id);
		/* AO_BUS */
		} else {
			if (domain == 0x6) {
				if (((bus_id >> 6) & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if (((bus_id >> 6) & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if (((bus_id >> 6) & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return infra_mi_trans(vio_addr, bus_id);
		}
	} else if ((slave_type == SLAVE_TYPE_APINFRA_SSR) || (slave_type == SLAVE_TYPE_PERI_PAR)) {
		return infra_mi_tracer1_trans(bus_id);
	} else if ((slave_type == SLAVE_TYPE_APINFRA_MEM) || (slave_type == SLAVE_TYPE_APINFRA_MEM_CTRL) ||
		   (slave_type == SLAVE_TYPE_APINFRA_MEM_INTF) || (slave_type == SLAVE_TYPE_APINFRA_MMU) ||
		   (slave_type == SLAVE_TYPE_APINFRA_SLB)) {
		if ((bus_id & 0x1) == 0x0)
			return "APINFRA_MEM_BUS_HRE_M";
		else
			return infra_mi_tracer0_trans(bus_id >> 1);
	} else
		return infra_mi_trans(vio_addr, bus_id);
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	pr_debug(PFX "%s %s %d, %s %d, %s 0x%x\n",
			__func__,
			"slave_type", slave_type,
			"vio_index", vio_index,
			"vio_addr", vio_addr);

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_APINFRA_IO) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_IO; i++) {
			if (vio_index == mt6991_devices_apinfra_io[i].vio_index)
				return mt6991_devices_apinfra_io[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_IO_CTRL) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_IO_CTRL; i++) {
			if (vio_index == mt6991_devices_apinfra_io_ctrl[i].vio_index)
				return mt6991_devices_apinfra_io_ctrl[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_IO_INTF) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_IO_INTF; i++) {
			if (vio_index == mt6991_devices_apinfra_io_intf[i].vio_index)
				return mt6991_devices_apinfra_io_intf[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_BIG4) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_BIG4; i++) {
			if (vio_index == mt6991_devices_apinfra_big4[i].vio_index)
				return mt6991_devices_apinfra_big4[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_DRAMC) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_DRAMC; i++) {
			if (vio_index == mt6991_devices_apinfra_dramc[i].vio_index)
				return mt6991_devices_apinfra_dramc[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_EMI) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_EMI; i++) {
			if (vio_index == mt6991_devices_apinfra_emi[i].vio_index)
				return mt6991_devices_apinfra_emi[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_SSR) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_SSR; i++) {
			if (vio_index == mt6991_devices_apinfra_ssr[i].vio_index)
				return mt6991_devices_apinfra_ssr[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_MEM) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_MEM; i++) {
			if (vio_index == mt6991_devices_apinfra_mem[i].vio_index)
				return mt6991_devices_apinfra_mem[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_MEM_CTRL) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_MEM_CTRL; i++) {
			if (vio_index == mt6991_devices_apinfra_mem_ctrl[i].vio_index)
				return mt6991_devices_apinfra_mem_ctrl[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_MEM_INTF) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_MEM_INTF; i++) {
			if (vio_index == mt6991_devices_apinfra_mem_intf[i].vio_index)
				return mt6991_devices_apinfra_mem_intf[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_INT) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_INT; i++) {
			if (vio_index == mt6991_devices_apinfra_int[i].vio_index)
				return mt6991_devices_apinfra_int[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_MMU) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_MMU; i++) {
			if (vio_index == mt6991_devices_apinfra_mmu[i].vio_index)
				return mt6991_devices_apinfra_mmu[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_APINFRA_SLB) {
		for (i = 0; i < VIO_SLAVE_NUM_APINFRA_SLB; i++) {
			if (vio_index == mt6991_devices_apinfra_slb[i].vio_index)
				return mt6991_devices_apinfra_slb[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if (g_sw_ver) {
			int slave_num = ARRAY_SIZE(mt6991_devices_peri_par_b0);

			for (i = 0; i < slave_num; i++) {
				if (vio_index == mt6991_devices_peri_par_b0[i].vio_index)
					return mt6991_devices_peri_par_b0[i].device;
			}
		} else {
			for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
				if (vio_index == mt6991_devices_peri_par_a0[i].vio_index)
					return mt6991_devices_peri_par_a0[i].device;
			}
		}
	} else if (slave_type == SLAVE_TYPE_VLP) {
		for (i = 0; i < VIO_SLAVE_NUM_VLP; i++) {
			if (vio_index == mt6991_devices_vlp[i].vio_index)
				return mt6991_devices_vlp[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		for (i = 0; i < VIO_SLAVE_NUM_ADSP; i++) {
			if (vio_index == mt6991_devices_adsp[i].vio_index)
				return mt6991_devices_adsp[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_MMINFRA; i++) {
			if (vio_index == mt6991_devices_mminfra[i].vio_index)
				return mt6991_devices_mminfra[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		for (i = 0; i < VIO_SLAVE_NUM_MMUP; i++) {
			if (vio_index == mt6991_devices_mmup[i].vio_index)
				return mt6991_devices_mmup[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_GPU) {
		for (i = 0; i < VIO_SLAVE_NUM_GPU; i++) {
			if (vio_index == mt6991_devices_gpu[i].vio_index)
				return mt6991_devices_gpu[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_GPU1) {
		for (i = 0; i < VIO_SLAVE_NUM_GPU1; i++) {
			if (vio_index == mt6991_devices_gpu1[i].vio_index)
				return mt6991_devices_gpu1[i].device;
		}
	}

	return "OUT_OF_BOUND";
}

static void mm2nd_vio_handler(void __iomem *infracfg,
			      struct mtk_devapc_vio_info *vio_info,
			      bool mdp_vio, bool disp2_vio, bool mmsys_vio)
{
	uint32_t vio_sta, vio_dbg, rw;
	uint32_t vio_sta_num;
	uint32_t vio0_offset;
	char mm_str[64] = {0};
	void __iomem *reg;
	int i;

	if (!infracfg) {
		pr_err(PFX "%s, param check failed, infracfg ptr is NULL\n",
				__func__);
		return;
	}

	if (mdp_vio) {
		vio_sta_num = INFRACFG_MDP_VIO_STA_NUM;
		vio0_offset = INFRACFG_MDP_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MDP_SEC_VIO", sizeof(mm_str));

	} else if (disp2_vio) {
		vio_sta_num = INFRACFG_DISP2_VIO_STA_NUM;
		vio0_offset = INFRACFG_DISP2_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_DISP2_SEC_VIO", sizeof(mm_str));

	} else if (mmsys_vio) {
		vio_sta_num = INFRACFG_MM_VIO_STA_NUM;
		vio0_offset = INFRACFG_MM_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MM_SEC_VIO", sizeof(mm_str));

	} else {
		pr_err(PFX "%s: param check failed, %s:%s, %s:%s, %s:%s\n",
				__func__,
				"mdp_vio", mdp_vio ? "true" : "false",
				"disp2_vio", disp2_vio ? "true" : "false",
				"mmsys_vio", mmsys_vio ? "true" : "false");
		return;
	}

	/* Get mm2nd violation status */
	for (i = 0; i < vio_sta_num; i++) {
		reg = infracfg + vio0_offset + i * 4;
		vio_sta = readl(reg);
		if (vio_sta)
			pr_info(PFX "MM 2nd violation: %s%d:0x%x\n",
					mm_str, i, vio_sta);
	}

	/* Get mm2nd violation address */
	reg = infracfg + vio0_offset + i * 4;
	vio_info->vio_addr = readl(reg);

	/* Get mm2nd violation information */
	reg = infracfg + vio0_offset + (i + 1) * 4;
	vio_dbg = readl(reg);

	vio_info->domain_id = (vio_dbg & INFRACFG_MM2ND_VIO_DOMAIN_MASK) >>
		INFRACFG_MM2ND_VIO_DOMAIN_SHIFT;

	vio_info->master_id = (vio_dbg & INFRACFG_MM2ND_VIO_ID_MASK) >>
		INFRACFG_MM2ND_VIO_ID_SHIFT;

	rw = (vio_dbg & INFRACFG_MM2ND_VIO_RW_MASK) >>
		INFRACFG_MM2ND_VIO_RW_SHIFT;
	vio_info->read = (rw == 0);
	vio_info->write = (rw == 1);
}

static uint32_t mt6991_shift_group_get(int slave_type, uint32_t vio_idx)
{
	return 31;
}

static bool apinfra_vio_callback(int slave_type)
{
	bool ret = false;
	int i;
	struct devapc_excep_entry *excep = NULL;

	for (i = 0; i < excep_info.entry_cnt; i++) {
		if (excep_info.excep_list[i].slave_type != slave_type)
			continue;
		excep = &excep_info.excep_list[i];

		ret = excep_info.excep_type_callback[excep->excep_type](excep);

		if (ret)
			break;
	}

	return ret;
}

static bool devapc_excep_debugsys_callback(struct devapc_excep_entry *excep)
{
	bool ret = false;
	uint32_t w_vio_info, r_vio_info, bus_id;
	const char *vio_master;

	if (excep == NULL)
		return false;

	w_vio_info = readl(excep->base);
	r_vio_info = readl(excep->base + 4);

	if (w_vio_info & DEBUGSYS_VIO_BIT) {
		pr_info(PFX "Write Violation\n");
		bus_id = (w_vio_info & DEBUGSYS_VIO_BUS_ID_MASK) >> 4;
		writel(0x0, excep->base);
		ret = true;
	} else if (r_vio_info & DEBUGSYS_VIO_BIT) {
		pr_info(PFX "Read Violation\n");
		bus_id = (r_vio_info & DEBUGSYS_VIO_BUS_ID_MASK) >> 4;
		writel(0x0, excep->base+4);
		ret = true;
	}

	if (ret) {
		vio_master = infra_mi_tracer1_trans(bus_id);
		pr_info(PFX "%s %s %s %s\n",
				"Violation - master:", vio_master,
				"access violation slave:",
				"DEBUGSYS_APB_S");
		pr_info(PFX "Device APC Violation Issue/%s", vio_master);
	}

	return ret;
}

void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	phys_addr_t test_pa = 0x17a54c50;

	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				"phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}

	if ((phys_addr <= test_pa) && (phys_addr + size > test_pa)) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx), %s:(%pa)\n",
				"catch VENCSYS address mapped!",
				"phys_addr", &phys_addr,
				"size", size, "test_pa", &test_pa);
		BUG_ON(1);
	}
}

static struct mtk_devapc_dbg_status mt6991_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_APINFRA_IO",
	"SLAVE_TYPE_APINFRA_IO_CTRL",
	"SLAVE_TYPE_APINFRA_IO_INTF",
	"SLAVE_TYPE_APINFRA_BIG4",
	"SLAVE_TYPE_APINFRA_DRAMC",
	"SLAVE_TYPE_APINFRA_EMI",
	"SLAVE_TYPE_APINFRA_SSR",
	"SLAVE_TYPE_APINFRA_MEM",
	"SLAVE_TYPE_APINFRA_MEM_CTRL",
	"SLAVE_TYPE_APINFRA_MEM_INTF",
	"SLAVE_TYPE_APINFRA_INT",
	"SLAVE_TYPE_APINFRA_MMU",
	"SLAVE_TYPE_APINFRA_SLB",
	"SLAVE_TYPE_PERI_PAR",
	"SLAVE_TYPE_VLP",
	"SLAVE_TYPE_ADSP",
	"SLAVE_TYPE_MMINFRA",
	"SLAVE_TYPE_MMUP",
	"SLAVE_TYPE_GPU",
	"SLAVE_TYPE_GPU1",
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_APINFRA_IO,
	VIO_MASK_STA_NUM_APINFRA_IO_CTRL,
	VIO_MASK_STA_NUM_APINFRA_IO_INTF,
	VIO_MASK_STA_NUM_APINFRA_BIG4,
	VIO_MASK_STA_NUM_APINFRA_DRAMC,
	VIO_MASK_STA_NUM_APINFRA_EMI,
	VIO_MASK_STA_NUM_APINFRA_SSR,
	VIO_MASK_STA_NUM_APINFRA_MEM,
	VIO_MASK_STA_NUM_APINFRA_MEM_CTRL,
	VIO_MASK_STA_NUM_APINFRA_MEM_INTF,
	VIO_MASK_STA_NUM_APINFRA_INT,
	VIO_MASK_STA_NUM_APINFRA_MMU,
	VIO_MASK_STA_NUM_APINFRA_SLB,
	VIO_MASK_STA_NUM_PERI_PAR,
	VIO_MASK_STA_NUM_VLP,
	VIO_MASK_STA_NUM_ADSP,
	VIO_MASK_STA_NUM_MMINFRA,
	VIO_MASK_STA_NUM_MMUP,
	VIO_MASK_STA_NUM_GPU,
	VIO_MASK_STA_NUM_GPU1,
};

static struct mtk_devapc_vio_info mt6991_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = DISP2_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6991_vio_dbgs = {
	.vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6991_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6991_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
	PD_VIO_DBG3_OFFSET,
};

static struct mtk_devapc_soc mt6991_data = {
	.dbg_stat = &mt6991_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_APINFRA_IO] = mt6991_devices_apinfra_io,
	.device_info[SLAVE_TYPE_APINFRA_IO_CTRL] = mt6991_devices_apinfra_io_ctrl,
	.device_info[SLAVE_TYPE_APINFRA_IO_INTF] = mt6991_devices_apinfra_io_intf,
	.device_info[SLAVE_TYPE_APINFRA_BIG4] = mt6991_devices_apinfra_big4,
	.device_info[SLAVE_TYPE_APINFRA_DRAMC] = mt6991_devices_apinfra_dramc,
	.device_info[SLAVE_TYPE_APINFRA_EMI] = mt6991_devices_apinfra_emi,
	.device_info[SLAVE_TYPE_APINFRA_SSR] = mt6991_devices_apinfra_ssr,
	.device_info[SLAVE_TYPE_APINFRA_MEM] = mt6991_devices_apinfra_mem,
	.device_info[SLAVE_TYPE_APINFRA_MEM_CTRL] = mt6991_devices_apinfra_mem_ctrl,
	.device_info[SLAVE_TYPE_APINFRA_MEM_INTF] = mt6991_devices_apinfra_mem_intf,
	.device_info[SLAVE_TYPE_APINFRA_INT] = mt6991_devices_apinfra_int,
	.device_info[SLAVE_TYPE_APINFRA_MMU] = mt6991_devices_apinfra_mmu,
	.device_info[SLAVE_TYPE_APINFRA_SLB] = mt6991_devices_apinfra_slb,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6991_devices_peri_par_a0,
	.device_info[SLAVE_TYPE_VLP] = mt6991_devices_vlp,
	.device_info[SLAVE_TYPE_ADSP] = mt6991_devices_adsp,
	.device_info[SLAVE_TYPE_MMINFRA] = mt6991_devices_mminfra,
	.device_info[SLAVE_TYPE_MMUP] = mt6991_devices_mmup,
	.device_info[SLAVE_TYPE_GPU] = mt6991_devices_gpu,
	.device_info[SLAVE_TYPE_GPU1] = mt6991_devices_gpu1,
	.ndevices = mtk6991_devices_num,
	.vio_info = &mt6991_devapc_vio_info,
	.vio_dbgs = &mt6991_vio_dbgs,
	.sramrom_sec_vios = &mt6991_sramrom_sec_vios,
	.devapc_pds = mt6991_devapc_pds,
	.irq_type_num = IRQ_TYPE_NUM,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6991_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6991_shift_group_get,
};

static const struct of_device_id mt6991_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6991-devapc" },
	{},
};

static const struct dev_pm_ops devapc_dev_pm_ops = {
	.suspend_noirq	= devapc_suspend_noirq,
	.resume_noirq = devapc_resume_noirq,
};

static struct devapc_excep_callbacks apinfra_excep_handler = {
	.type = DEVAPC_TYPE_INFRA,
	.handle_excep = apinfra_vio_callback,
};

static void devapc_get_chipid(void)
{
	struct tag_chipid *chip_id;
	struct device_node *node = of_find_node_by_path("/chosen");

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (!node) {
		pr_notice("chosen node not found in device tree\n");
		return;
	}

	chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", NULL);
	if (!chip_id) {
		pr_notice("could not found atag,chipid in chosen\n");
		return;
	}

	g_sw_ver = (int)chip_id->sw_ver;

	return;
}

static void register_devapc_excep(struct platform_device *pdev)
{
	int ret;
	uint32_t value;
	struct device_node *np;
	struct device_node *excep_node;
	struct device_node *child;
	struct devapc_excep_entry *excep;
	const char *str;

	np = pdev->dev.of_node;
	excep_node = of_get_child_by_name(np, EXCEPTION_NODE_NAME);
	if (!excep_node) {
		pr_info(PFX"fail to parse exception from dev.\n");
		return;
	}

	for_each_child_of_node(excep_node, child) {
		if (excep_info.entry_cnt >= DEVAPC_EXCEP_HANDLER_MAX) {
			pr_info(PFX"entry cnt over limite\n");
			break;
		}

		excep = &excep_info.excep_list[excep_info.entry_cnt];

		ret = of_property_read_string(child, "excep-name", &str);
		if (ret) {
			pr_info(PFX "fail to parse excep-name from node.\n");
			return;
		}
		strscpy(excep->name, str, DEVAPC_EXCEP_NAME_LEN);

		ret = of_property_read_u32(child, "excep-type", &value);
		if ((ret != 0) || (value >= DEVAPC_EXCEP_HANDLER_MAX)) {
			pr_info(PFX "fail to parse excep-type from node.\n");
			return;
		}
		excep->excep_type = value;

		ret = of_property_read_u32(child, "slave-type", &value);
		if ((ret != 0) || (value >= SLAVE_TYPE_NUM)) {
			pr_info(PFX "fail to parse slave-type from node.\n");
			return;
		}
		excep->slave_type = value;

		ret = of_property_read_u32(child, "reg-base", &value);
		if (ret) {
			pr_info(PFX "fail to parse reg-base from node.\n");
			return;
		}
		excep->base  = ioremap(value, 8);
		excep_info.entry_cnt++;
	}

	excep_info.excep_type_callback[DEVAPC_EXCEP_HANDLER_DEBUGSYS] = devapc_excep_debugsys_callback;

	register_devapc_exception_callback(&apinfra_excep_handler);
}

static int mt6991_devapc_probe(struct platform_device *pdev)
{
	devapc_get_chipid();

	if (g_sw_ver != 0) {
		mt6991_data.device_info[SLAVE_TYPE_PERI_PAR] = mt6991_devices_peri_par_b0;
		mtk6991_devices_num[SLAVE_TYPE_PERI_PAR].vio_slave_num = ARRAY_SIZE(mt6991_devices_peri_par_b0);
		register_devapc_excep(pdev);
	}

	return mtk_devapc_probe(pdev, (struct mtk_devapc_soc *)&mt6991_data);
}

static int mt6991_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6991_devapc_driver = {
	.probe = mt6991_devapc_probe,
	.remove = mt6991_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6991_devapc_dt_match,
		.pm = &devapc_dev_pm_ops,
	},
};

module_platform_driver(mt6991_devapc_driver);

MODULE_DESCRIPTION("Mediatek mt6991 Device APC Driver");
MODULE_AUTHOR("Louis Yeh <louis-cy.yeh@mediatek.com>");
MODULE_LICENSE("GPL");
