// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include "conap_platform_data.h"

#define CONNSCP_DEFAULT_MAX_MSG_SIZE	1024

/* 6893 */
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6893)

struct conap_scp_shm_config g_adp_shm_mt6893 = {
	.conap_scp_shm_offset = 0x1E0000,
	.conap_scp_shm_size = 0x50000,
	.conap_scp_ipi_mbox_size = 40,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6983)
struct conap_scp_shm_config g_adp_shm_mt6983 = {
	.conap_scp_shm_offset = 0x2100000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6895)
struct conap_scp_shm_config g_adp_shm_mt6895 = {
	.conap_scp_shm_offset = 0x2100000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6886)
struct conap_scp_shm_config g_adp_shm_mt6886 = {
	.conap_scp_shm_offset = 0,
	.conap_scp_shm_size = 0,
	.conap_scp_ipi_mbox_size = 64,
};
#endif

struct conap_scp_shm_config g_adp_shm;
struct conap_scp_batching_config g_adp_batching;
struct conap_dfd_config g_adp_dfd_cmd;
struct conap_dfd_config g_adp_dfd_value;

uint32_t connsys_scp_shm_get_addr(void)
{
	return 0;
}

uint32_t connsys_scp_shm_get_size(void)
{
	return 0;
}

uint32_t connsys_scp_get_max_msg_size(void)
{
	return g_adp_shm.conap_scp_max_msg_size;
}

uint32_t connsys_scp_ipi_mbox_size(void)
{
	return g_adp_shm.conap_scp_ipi_mbox_size;
}

phys_addr_t connsys_scp_shm_get_batching_addr(void)
{
	return (g_adp_batching.buff_offset & 0xFFFFFFFF);
}

uint32_t connsys_scp_shm_get_batching_size(void)
{
	return g_adp_batching.buff_size;
}


phys_addr_t connsys_scp_get_dfd_cmd_addr(void)
{
	return g_adp_dfd_cmd.addr;
}

uint32_t connsys_scp_get_dfd_cmd_size(void)
{
	return g_adp_dfd_cmd.size;
}

phys_addr_t connsys_scp_get_dfd_value_addr(void)
{
	return g_adp_dfd_value.addr;
}

uint32_t connsys_scp_get_dfd_value_size(void)
{
	return g_adp_dfd_value.size;
}

int connsys_scp_plt_data_init(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	u32 value;
	u32 ipi_mbox_size = 0, max_msg_size = 0;
	u32 batching_buf_sz = 0;
	u64 value64 = 0, batching_buf_addr = 0;
	u32 dfd_cmd_sz = 0, dfd_value_sz = 0;
	u64 dfd_cmd_addr = 0, dfd_value_addr = 0;

	node = pdev->dev.of_node;

	/* ipi mbox setting */
	ret = of_property_read_u32(node, "max-msg-size", &value);
	if (ret < 0) {
		pr_notice("[%s] prop max-msg-size fail %d", __func__, ret);
		max_msg_size = CONNSCP_DEFAULT_MAX_MSG_SIZE;
	} else
		max_msg_size = value;

	/* ipi mbox setting */
	ret = of_property_read_u32(node, "ipi-mbox-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop ipi-mbox-size fail %d", __func__, ret);
	else
		ipi_mbox_size = value;

	/* report location setting */
	ret = of_property_read_u32(node, "report-buf-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop batching-buf-size fail %d", __func__, ret);
	else
		batching_buf_sz = value;

	ret = of_property_read_u64(node, "report-buf-addr", &value64);
	if (ret < 0)
		pr_notice("[%s] prop batching-buf-addr fail %d", __func__, ret);
	else
		batching_buf_addr = value64;

	/* dfd cmd addr */
	ret = of_property_read_u64(node, "dfd-cmd-addr", &value64);
	if (ret < 0)
		pr_notice("[%s] prop dfd-value-addr fail %d", __func__, ret);
	else
		dfd_cmd_addr = value64;

	/* dfd value size */
	ret = of_property_read_u32(node, "dfd-cmd-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop dfd-value-size fail %d", __func__, ret);
	else
		dfd_cmd_sz = value;

	/* dfd value addr */
	ret = of_property_read_u64(node, "dfd-value-addr", &value64);
	if (ret < 0)
		pr_notice("[%s] prop dfd-value-addr fail %d", __func__, ret);
	else
		dfd_value_addr = value64;

	/* dfd value size */
	ret = of_property_read_u32(node, "dfd-value-size", &value);
	if (ret < 0)
		pr_notice("[%s] prop dfd-value-size fail %d", __func__, ret);
	else
		dfd_value_sz = value;

	pr_info("[%s] mx_msg_isze=[%x] ipi_mbox_size=[%x] batching=[%x][%llx] dfd=[%llx][%x]", __func__,
			max_msg_size, ipi_mbox_size,
			batching_buf_sz, batching_buf_addr,
			dfd_value_addr, dfd_value_sz);

	memset(&g_adp_shm, 0, sizeof(g_adp_shm));
	memset(&g_adp_batching, 0, sizeof(g_adp_batching));
	memset(&g_adp_dfd_cmd, 0, sizeof(g_adp_dfd_cmd));
	memset(&g_adp_dfd_value, 0, sizeof(g_adp_dfd_value));

	g_adp_shm.conap_scp_ipi_mbox_size = ipi_mbox_size;
	g_adp_shm.conap_scp_max_msg_size = max_msg_size;

	g_adp_batching.buff_offset = batching_buf_addr;
	g_adp_batching.buff_size = batching_buf_sz;

	g_adp_dfd_cmd.addr = dfd_cmd_addr;
	g_adp_dfd_cmd.size = dfd_cmd_sz;

	g_adp_dfd_value.addr = dfd_value_addr;
	g_adp_dfd_value.size = dfd_value_sz;

	return 0;
}
