/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 *
 */

/*
 * Copyright (c) 2018 GoldenRiver Technology Co., Ltd. All rights reserved
 *
 */

/*
 * Copyright (c) 2013-2016 Google Inc. All rights reserved
 *
 */

/* To prevent redefined with smcall.h */

#ifndef __HV_CALL_H__
#define __HV_CALL_H__

/* To prevent redefined with smcall.h */
#ifndef SMC_NUM_ENTITIES
#define SMC_NUM_ENTITIES        64
#define SMC_NUM_ARGS            4
#define SMC_NUM_PARAMS          (SMC_NUM_ARGS - 1)

#define SMC_IS_FASTCALL(smc_nr) ((smc_nr) & 0x80000000)
#define SMC_IS_SMC64(smc_nr)    ((smc_nr) & 0x40000000)
#define SMC_ENTITY(smc_nr)      (((smc_nr) & 0x3F000000) >> 24)
#define SMC_FUNCTION(smc_nr)    ((smc_nr) & 0x0000FFFF)

#define SMC_NR(entity, fn, fastcall, smc64) \
	((((fastcall) & 0x1) << 31) | \
	(((smc64) & 0x1) << 30) | \
	(((entity) & 0x3F) << 24) | \
	((fn) & 0xFFFF))

#define SMC_FASTCALL_NR(entity, fn)     SMC_NR((entity), (fn), 1, 0)
#define SMC_STDCALL_NR(entity, fn)      SMC_NR((entity), (fn), 0, 0)
#define SMC_FASTCALL64_NR(entity, fn)   SMC_NR((entity), (fn), 1, 1)
#define SMC_STDCALL64_NR(entity, fn)    SMC_NR((entity), (fn), 0, 1)
#endif /* end of SMC_NUM_ENTITIES */

#define	SMC_ENTITY_NEBULA   52  /* Nebula SMC Calls */
#define	SMC_ENTITY_PLAT     53  /* PLATFORM SMC Calls */

#define SMC_DEFAULT_SECURE_ID  0  /* SMC call for default target */
#define SMC_VTEE_SECURE_ID     1  /* SMC call for virtual TEE */

/****************************************************/
/********** Nebula Specific SMC Calls ***************/
/****************************************************/
#define SMC_FC_NBL_CPU_HOTPLUG_ON           SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 110)
#define SMC_FC_NBL_CPU_HOTPLUG_OFF          SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 111)
#define SMC_FC_NBL_KERNEL_SUSPEND_OFF       SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 112)
#define SMC_FC_NBL_KERNEL_SUSPEND_ON        SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 113)
#define SMC_FC_NBL_TEST_ADD                 SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 200)
#define SMC_FC_NBL_TEST_MULTIPLY            SMC_FASTCALL_NR(SMC_ENTITY_NEBULA, 201)

#define SMC_SC_NBL_SMC_RETURN               SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 100)
#define SMC_SC_NBL_STDCALL_DONE             SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 101)
#define SMC_SC_NBL_SHARED_LOG_VERSION       SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 110)
#define SMC_SC_NBL_SHARED_LOG_ADD           SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 111)
#define SMC_SC_NBL_SHARED_LOG_RM            SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 112)
#define SMC_SC_NBL_NOP                      SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 120)
#define SMC_SC_NBL_LOCKED_NOP               SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 121)
#define SMC_SC_NBL_RESTART_LAST             SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 122)
#define SMC_SC_NBL_VIRTIO_GET_DESCR         SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 130)
#define SMC_SC_NBL_VIRTIO_START             SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 131)
#define SMC_SC_NBL_VIRTIO_STOP              SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 132)
#define SMC_SC_NBL_VDEV_RESET               SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 133)
#define SMC_SC_NBL_VDEV_KICK_VQ             SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 134)
#define SMC_NC_NBL_VDEV_KICK_VQ             SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 135)
#define SMC_SC_NBL_TEST_ADD                 SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 200)
#define SMC_SC_NBL_TEST_MULTIPLY            SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 201)
#define SMC_SC_NBL_VHM_REQ                  SMC_STDCALL_NR(SMC_ENTITY_NEBULA, 256)

/****************************************************/
/********** PLATFORM Specific SMC Calls *************/
/****************************************************/
#define SMC_FC_PLAT_GET_UART_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 100)
#define SMC_FC_PLAT_REGISTER_IRQ            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 101)
#define SMC_FC_PLAT_GET_NEXT_IRQ            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 102)
#define SMC_FC_PLAT_GET_GIC_VERSION         SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 103)
#define SMC_FC_PLAT_GET_GICD_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 104)
#define SMC_FC_PLAT_GET_GICR_PA             SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 105)
#define SMC_FC_PLAT_REGISTER_BOOT_ENTRY     SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 106)
#define SMC_FC_PLAT_GET_RPMB_KEY            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 107)
#define SMC_FC_PLAT_PRINT_CHAR              SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 108)
#define SMC_FC_PLAT_INIT_SHARE_MEMORY       SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 109)
#define SMC_FC_PLAT_GET_DEVICE_ID           SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 110)
#define SMC_FC_PLAT_GET_RANDOM_NUMBER       SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 111)
#define SMC_FC_PLAT_INIT_CELLINFO           SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 112)
#define SMC_FC_PLAT_GET_NEXT_IPI            SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 113)
#define SMC_FC_PLAT_GET_RPMB_RANGE          SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 114)
#define SMC_FC_PLAT_GET_VM_MEM_SIZE         SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 115)
#define SMC_FC_PLAT_INIT_DONE               SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 116)
#define SMC_FC_PLAT_STDCALL_SWITCH          SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 117)
#define SMC_FC_PLAT_ARITHMETIC              SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 32768)
#define SMC_FC_PLAT_GET_RTC                 SMC_FASTCALL_NR(SMC_ENTITY_PLAT, 32769)

#define SMC_SC_PLAT_TEST_MULTIPLY           SMC_STDCALL_NR(SMC_ENTITY_PLAT, 115)
#define SMC_SC_PLAT_MTEE_SERVICE_CMD        SMC_STDCALL_NR(SMC_ENTITY_PLAT, 121)

#endif /* __HV_CALL_H__ */
