# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_MTK_OPTEE_SUPPORT) += tee.o
tee-y := common/tee_core.o \
         common/tee_shm_pool.o \
         common/tee_shm.o

obj-$(CONFIG_MTK_OPTEE_SUPPORT) += optee.o
optee-y := call.o \
           core.o \
           device.o \
           ffa_abi.o \
           notif.o \
           rpc.o \
           smc_abi.o \
           supp.o

ccflags-$(CONFIG_OPTEE_RAMLOG) += -DSET_USER_ID=105
optee-$(CONFIG_OPTEE_RAMLOG) += optee_ramlog.o
optee-$(CONFIG_OPTEE_REE_CONSOLE) += optee_kreeconsole.o
optee-$(CONFIG_OPTEE_REE_CLK_CTRL) += optee_clk_ctrl.o

# for tracing framework to find optee_trace.h
CFLAGS_smc_abi.o := -I$(DEVICE_MODULES_PATH)/drivers/tee/optee/tee
