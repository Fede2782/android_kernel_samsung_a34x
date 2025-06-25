/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_DAEMON_H__
#define __MTK_DAEMON_H__

#define MAX_MSG_LEN_SND		4096
#define MAX_MSG_LEN_RCV		9200
#define AFW_MSG_HEADER_LEN	(sizeof(struct afw_header) - AFW_MSG_MAX_LEN)
#define AFW_MAGIC		2015060303
#define AFW_MSG_MAX_LEN		9200
#define LOG_BUF_MAX		(MAX_MSG_LEN_SND - AFW_MSG_HEADER_LEN - 1)
#define INSTANCE_MAX		5

struct afw_header {
	unsigned char instance_id;
	unsigned char datatype;
	unsigned int cmd;
	unsigned int hash;
	unsigned int subcmd;
	unsigned int subcmd_para1;
	unsigned int data_len;
	unsigned int ret_data_len;
	unsigned int identity;
	char data[AFW_MSG_MAX_LEN];
};

extern int mtk_battery_daemon_init(struct platform_device *pdev);
extern int wakeup_fg_daemon(struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1);


void sw_check_bat_plugout(struct mtk_battery *gm);
void fg_sw_bat_cycle_accu(struct mtk_battery *gm);

#define DATA_SIZE 2048
struct afw_data_param {
	unsigned int total_size;
	unsigned int size;
	unsigned int idx;
	char input[DATA_SIZE];
};

struct fgd_cmd_param_t_8 {
	int size;
	int data[128];
};

#endif
