// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "mvpu_plat.h"
#include "mvpu20_ipi.h"
#include "mvpu20_handler.h"
#include "mvpu20_sec.h"

struct mvpu_sec_ops mvpu20_sec_ops = {
	.mvpu_sec_init = mvpu20_sec_init,
	.mvpu_load_img = mvpu20_load_img,
	.mvpu_sec_sysfs_init = mvpu20_sec_sysfs_init
};

struct mvpu_ops mvpu20_ops = {
	.mvpu_ipi_init = mvpu20_ipi_init,
	.mvpu_ipi_deinit = mvpu20_ipi_deinit,
	.mvpu_ipi_send = mvpu20_ipi_send,
	.mvpu_handler_lite_init = mvpu20_handler_lite_init,
	.mvpu_handler_lite = mvpu20_handler_lite,
};

struct mvpu_platdata mvpu_mt6983_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};

struct mvpu_platdata mvpu_mt8139_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};

struct mvpu_platdata mvpu_mt6879_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};

struct mvpu_platdata mvpu_mt6895_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};

struct mvpu_platdata mvpu_mt6985_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};

struct mvpu_platdata mvpu_mt6886_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU20,
	.ops = &mvpu20_ops,
	.sec_ops = &mvpu20_sec_ops
};
