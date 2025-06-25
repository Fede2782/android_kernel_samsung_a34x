/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __AP_MD_REG_DUMP_H__
#define __AP_MD_REG_DUMP_H__

#include "md_sys1_platform.h"
#include "modem_reg_base.h"
#include "modem_secure_base.h"
#include "modem_sys.h"

enum MD_REG_ID {
	MD_REG_SET_DBGSYS_TIME_OUT_ADDR = 0,
	MD_REG_PC_MONITOR_ADDR,
	MD_REG_BUSMON_ADDR_0,
	MD_REG_BUSMON_ADDR_1,
	MD_REG_USIP_ADDR_0,
	MD_REG_USIP_ADDR_1,
	MD_REG_USIP_ADDR_2,
	MD_REG_USIP_ADDR_3,
	MD_REG_USIP_ADDR_4,
	MD_REG_USIP_ADDR_5,
};

enum md_reg_id {
	MD_REG_DUMP_START = 0,
	MD_REG_DUMP_STAGE,
	MD_REG_GET_DUMP_ADDRESS,
};

/* res.a2 in MD_REG_DUMP_STAGE OP */
enum md_dump_flag {
	DUMP_FINISHED,
	DUMP_UNFINISHED,
	DUMP_DELAY_us,
};

#define MD_REG_DUMP_MAGIC   (0x44554D50) /* DUMP */
extern struct ccci_plat_val md_cd_plat_val_ptr;

void legacy_md_dump_regmd_dump_register(struct ccci_modem *md);
void md_dump_reg(struct ccci_modem *md);
extern u32 get_expected_boot_status_val(void);
extern void md_cd_lock_modem_clock_src(int locked);
size_t mdreg_write32(size_t reg_id, size_t value);

// md gen93 that dump md register in kernel
extern void md_dump_register_for_mt6761(struct ccci_modem *md);
extern void md_dump_register_for_mt6765(struct ccci_modem *md);
extern void md_dump_register_for_mt6768(struct ccci_modem *md);

// md gen95 that dump md register in kernel
extern void md_dump_register_for_mt6781(struct ccci_modem *md);

// md gen97 that dump md register in kernel
extern void md_dump_register_for_mt6877(struct ccci_modem *md);
extern void md_dump_register_for_mt6885(struct ccci_modem *md);

#endif
