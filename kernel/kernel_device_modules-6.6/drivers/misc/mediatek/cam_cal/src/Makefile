# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP3_Z), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP3_Z) += isp3_z/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP3_M), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP3_M) += isp3_m/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP4_T), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP4_T) += isp4_t/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP4_C), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP4_C) += isp4_c/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL), m))
obj-$(CONFIG_MTK_CAM_CAL) += custom/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MON), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MON) += isp6s_mon/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP6S_MOU), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_MOU) += isp6s_mou/
endif

ifneq (,$(filter $(CONFIG_MTK_CAM_CAL_ISP6S_LAG), m y))
obj-$(CONFIG_MTK_CAM_CAL_ISP6S_LAG) += isp6s_lag/
endif

