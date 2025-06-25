#
# Copyright (C) 2016 MediaTek Inc.
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

subdir-ccflags-y += -DMTK_VPU_KERNEL
subdir-ccflags-y += -DCCU_AF_ENABLE

$(warning CCU_MAKE_FILE_CALLED_2_1_1)

platform = $(subst $\",,$(CONFIG_MTK_CAMERA_ISP_PLATFORM))

# folder_name = isp6s
# obj-$(CONFIG_MTK_CCU) += $(subst ",,$(folder_name))/

ifeq ($(CONFIG_MTK_CCU),m)
obj-$(CONFIG_MTK_CCU) += isp4/
obj-$(CONFIG_MTK_CCU) += isp6s/
else
obj-$(CONFIG_MTK_CCU_ISP4) += isp4/
obj-$(CONFIG_MTK_CCU_ISP6S) += isp6s/
endif
