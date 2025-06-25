/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2020 The Hafnium Authors.
 */

#pragma once

/** AArch64-specific mapping modes */

/** Mapping mode defining MMU Stage-1 block/page non-secure bit */
#define MM_MODE_NS UINT32_C(0x0080)

/** Page mapping mode for tagged normal memory. */
#define MM_MODE_T UINT32_C(0x0400)
