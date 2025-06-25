/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SWPM Module with Power Service Pack
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __APMCUPM_SCMI_V2_H__
#define __APMCUPM_SCMI_V2_H__

#define APMCU_SCMI_MAGIC     (0xBD)

#define APMCU_SCMI_UID_MASK    (0xFF)
#define APMCU_SCMI_UUID_MASK   (0xFF)
#define APMCU_SCMI_ACT_MASK    (0xFF)
#define APMCU_SCMI_MG_MASK     (0xFF)

#define APMCU_SCMI_UID_SHIFT   (0)
#define APMCU_SCMI_UUID_SHIFT  (8)
#define APMCU_SCMI_ACT_SHIFT   (16)
#define APMCU_SCMI_MG_SHIFT    (24)

#define APMCU_SET_UID(uid) (((uid) & APMCU_SCMI_UID_MASK) << (APMCU_SCMI_UID_SHIFT))
#define APMCU_SET_UUID(uuid) (((uuid) & APMCU_SCMI_UUID_MASK) << (APMCU_SCMI_UUID_SHIFT))
#define APMCU_SET_ACT(act) (((act) & APMCU_SCMI_ACT_MASK) << (APMCU_SCMI_ACT_SHIFT))
#define APMCU_SET_MG (((APMCU_SCMI_MAGIC) & APMCU_SCMI_MG_MASK) << (APMCU_SCMI_MG_SHIFT))

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
/* Define the 1st-level user here */
enum APMCU_SCMI_UID {
	APMCU_SCMI_UID_PMSR = 0,
	APMCU_SCMI_UID_SWPM = 1,

	APMCU_SCMI_UID_NUM,
};

/* Data format of "user_info" in
 *  struct pmsr_tool_scmi_set_input
 *  unsigned int uid     : 8
 *  unsigned int uuid    : 8
 *  unsigned int action  : 8
 *  unsigned int magic   : 8
 */
struct apmcupm_scmi_set_input {
	unsigned int user_info;
	unsigned int in1;
	unsigned int in2;
	unsigned int in3;
	unsigned int in4;
};
#endif

#endif /*__APMCUPM_SCMI_V2_H__*/
