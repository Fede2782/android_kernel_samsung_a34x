/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2021 The Hafnium Authors.
 */

#pragma once

#if !defined(__cplusplus)

#ifndef PLAT_LOG_LEVEL_ASSERT
#define PLAT_LOG_LEVEL_ASSERT LOG_LEVEL
#endif
#define assert(e) ((void)0)
#else
#include <assert.h>
#endif /* !defined(__cplusplus) */
