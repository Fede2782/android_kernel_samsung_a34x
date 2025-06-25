/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Mediatek Inc.
 */

#ifndef MTK_8250_MTK_H
#define MTK_8250_MTK_H

#include "../../../misc/mediatek/uarthub/common/uarthub_drv_export.h"

#if IS_ENABLED(CONFIG_MTK_UARTHUB)
void uarthub_drv_callbacks_register(struct uarthub_drv_cbs *cb);
void uarthub_drv_callbacks_unregister(void);
#endif

#endif
