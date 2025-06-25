/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

enum {
	SCP_CCCI_STATE_INVALID = 0,
	SCP_CCCI_STATE_BOOTING = 1,
	SCP_CCCI_STATE_RBREADY = 2,
	SCP_CCCI_STATE_STOP = 3,
};

#define SCP_BOOT_TIMEOUT (30*1000)

#define SCP_MSG_CHECK_A 0xABCDDCBA
#define SCP_MSG_CHECK_B 0xAABBCCDD

struct ccci_ipi_msg_out {
	u16 md_id; //compatibility member
	u16 op_id;
	u32 data[];
} __packed;

struct ccci_ipi_msg_in {
	u16 md_id; //compatibility member
	u16 op_id;
	u32 data;
} __packed;

struct ccci_fsm_scp {
	enum MD_STATE old_state;
	struct work_struct scp_md_state_sync_work;
	void __iomem *ccif2_ap_base;
	void __iomem *ccif2_md_base;
	unsigned int scp_clk_free_run;
	unsigned int ipi_msg_out_data_num;
};

struct scp_ipi_info {
	int chan_id;
	int mbox_id;
	int msg_size;
};
