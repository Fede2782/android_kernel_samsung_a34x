/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifdef VL53LX_NEEDS_IPP
#  undef VL53LX_IPP_API
#  define VL53LX_IPP_API  __declspec(dllimport)
#  pragma comment (lib, "EwokPlus25API_IPP")
#endif

