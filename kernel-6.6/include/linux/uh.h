/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UH_H__
#define __UH_H__

#ifndef __ASSEMBLY__

#ifdef CONFIG_KVM
#include <linux/arm-smccc.h>

#define KVM_HOST_SMCCC_ID(id)						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_64,				\
			   ARM_SMCCC_OWNER_VENDOR_HYP,			\
			   (id))
#endif

#define UH_PREFIX			UL(0x83000000)
#define UH_APPID(APP_ID)	(((UL(APP_ID) << 8) & UL(0xFF00)) | UH_PREFIX)
unsigned long _uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);
static inline void uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
#ifdef CONFIG_UH_PKVM
	_uh_call(KVM_HOST_SMCCC_ID(128), app_id | command, arg0, arg1, arg2, arg3);
#else
	_uh_call(app_id | command, arg0, arg1, arg2, arg3, 0);
#endif
}

/*
 * //Qualcomm
#define UH_PREFIX		UL(0xc300c000)
#define UH_APPID(APP_ID)	((UL(APP_ID) & UL(0xFF)) | UH_PREFIX)
unsigned long uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);
*/

/* For uH Command */
#define APP_INIT	0
#define APP_RKP		1
#define APP_KDP		2
#define APP_HDM		6

enum __UH_APP_ID {
	UH_APP_INIT     = UH_APPID(APP_INIT),
	UH_APP_RKP      = UH_APPID(APP_RKP),
	UH_APP_KDP      = UH_APPID(APP_KDP),
	UH_APP_HDM      = UH_APPID(APP_HDM),
};

enum __UH_CMD_ID {
	UH_EVENT_REGISTER_SYSINIT = 0x0,
	UH_EVENT_KDP_SET_BOOT_STATE = 0x1,
	UH_EVENT_REGISTER_MEMLIST = 0x2,
	UH_EVENT_MEMORY_POOL_INIT = 0x4,
	UH_EVENT_LOG_REGION_INFO = 0x5,
};
#endif //__ASSEMBLY__
#endif //__UH_H__
