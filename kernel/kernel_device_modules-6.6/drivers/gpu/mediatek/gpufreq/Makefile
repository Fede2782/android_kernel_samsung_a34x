#
# Copyright (C) 2021 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ifeq ($(CONFIG_MTK_GPUFREQ_V2),y)
ifeq ($(CONFIG_MTK_GPU_LEGACY),y)
	obj-y += v2_legacy/
else
	obj-y += v2/
endif
else
	obj-y += v1/
endif

$(info [MTK GPU] CONFIG_MTK_GPU_LEGACY = $(CONFIG_MTK_GPU_LEGACY))
