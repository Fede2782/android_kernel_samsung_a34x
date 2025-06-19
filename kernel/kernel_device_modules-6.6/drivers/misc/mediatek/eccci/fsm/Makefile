#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), m)

ccflags-y += -DCCCI_KMODULE_ENABLE

#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/port
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/platform
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/hif/

ccci_fsm_scp-y := ccci_fsm_scp_c.o
obj-$(CONFIG_MTK_ECCCI_DRIVER) += ccci_fsm_scp.o

endif

ifeq ($(CONFIG_MTK_ECCCI_DRIVER), y)

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccmni/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/port
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/mt6873
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/hif/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mdpm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/memory-amms/

# fsm objects
obj-$(CONFIG_MTK_ECCCI_DRIVER) := ccci_fsm_all.o

ccci_fsm_all-y := ap_md_mem.o \
			ap_md_reg_dump.o \
			ap_md_reg_dump_gen93.o \
			ap_md_reg_dump_gen95.o \
			ap_md_reg_dump_gen97.o \
			ccci_fsm.o \
			ccci_fsm_ioctl.o \
			ccci_fsm_monitor.o \
			ccci_fsm_poller.o \
			ccci_fsm_scp_c.o \
			ccci_fsm_ee.o \
			ccci_fsm_sys.o \
			md_sys1_platform.o \
			modem_sys1.o \
			mdee_dumper_v1.o \
			mdee_dumper_v2.o \
			mdee_dumper_v3.o \
			mdee_dumper_v5.o \
			mdee_dumper_v6.o

endif
