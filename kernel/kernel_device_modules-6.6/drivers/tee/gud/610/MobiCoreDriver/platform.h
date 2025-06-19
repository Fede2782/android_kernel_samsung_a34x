/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2024 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MC_DRV_PLATFORM_H
#define MC_DRV_PLATFORM_H

#include <linux/version.h> /* KERNEL_VERSION */
#include <linux/kconfig.h> /* IS_REACHABLE */

/* TEE Interrupt should be defined in Device Tree
 * in kernel source code : arch/arm64/boot/dts/
 * else you can define it here using
 * #define MC_INTR_SSIQ	0
 */
#ifndef MTK_ADAPTED
#define MC_TEE_HOTPLUG
#endif

/* Enable Paravirtualization support */
// #define MC_FEBE

/* Xen virtualization support */
#if defined(CONFIG_XEN)
#if defined(MC_FEBE)
#define MC_XEN_FEBE
#endif /* MC_XEN_FEBE */
#endif /* CONFIG_XEN */

/* ARM FFA protocol support */
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#if IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT)
#define MC_FFA_FASTCALL
// #define MC_SHADOW_BUFFER
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
#define TRUSTONIC_USES_FFA_1_1
#ifndef MTK_ADAPTED
#define MC_SHADOW_BUFFER
#endif
/* WARNING: Only use FFA_NOTIFICATION fo SPMC-EL2*/
#define MC_FFA_NOTIFICATION 1
#endif
#endif /* CONFIG_ARM_FFA_TRANSPORT */
#endif /* KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE */

/* Probe TEE driver even if node not defined in Device Tree */
#define MC_PROBE_WITHOUT_DEVICE_TREE

#ifdef MTK_ADAPTED
#define MC_DEVICE_PROPNAME "trustonic,mobicore"
#define PLAT_DEFAULT_TEE_AFFINITY_MASK 0x70
#else
#define MC_DEVICE_PROPNAME "arm,mcd"
#endif

#endif /* MC_DRV_PLATFORM_H */
