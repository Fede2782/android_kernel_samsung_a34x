# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MEDIATEK) += common/
ifneq ($(wildcard $(srctree)/$(src)/mt6853/Makefile),)
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6873/Makefile),)
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6885/Makefile),)
obj-$(CONFIG_SND_SOC_MT6885) += mt6885/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8188/Makefile),)
obj-$(CONFIG_SND_SOC_MT8188) += mt8188/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6983/Makefile),)
obj-$(CONFIG_SND_SOC_MT6983) += mt6983/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6985/Makefile),)
obj-$(CONFIG_SND_SOC_MT6985) += mt6985/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6886/Makefile),)
obj-$(CONFIG_SND_SOC_MT6886) += mt6886/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6897/Makefile),)
obj-$(CONFIG_SND_SOC_MT6897) += mt6897/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6989/Makefile),)
obj-$(CONFIG_SND_SOC_MT6989) += mt6989/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6768/Makefile),)
obj-$(CONFIG_SND_SOC_MT6768) += mt6768/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6877/Makefile),)
obj-$(CONFIG_SND_SOC_MT6877) += mt6877/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6991/Makefile),)
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6991) += mt6991/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6899/Makefile),)
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6899) += mt6899/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6781/Makefile),)
obj-$(CONFIG_SND_SOC_MT6781) += mt6781/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6833/Makefile),)
obj-$(CONFIG_SND_SOC_MT6833) += mt6833/
endif
# vow related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/

# audio sound dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += ultrasound/

obj-$(CONFIG_MTK_SCP_AUDIO) += audio_scp/

# include path
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/
# New architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += common/
# flags for not ready feature, uncomment after ready
subdir-ccflags-y += -DAUDIO_USING_WRAP_DRIVER
subdir-ccflags-y += -D_MT_IDLE_HEADER
subdir-ccflags-y += -D_REG_SYNC_WRITE
# PMIC include path
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/codec
# AP include path
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common_int

ifeq ($(CONFIG_SND_SOC_MT6739),m)
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/mt6739
else
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/mt6765
endif

subdir-ccflags-y += -Wno-incompatible-pointer-types
endif
# Update pmic built flow to avoid codec init later than codec probe
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += codec/
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += common_int/
obj-$(CONFIG_MTK_SND_SOC_NEW_ARCH_COMMON) += mt6765/
endif
