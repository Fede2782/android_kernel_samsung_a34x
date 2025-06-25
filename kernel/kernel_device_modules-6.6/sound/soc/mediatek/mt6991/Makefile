# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include

# spm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# pbm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm

# common include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

# VIP include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/eas
subdir-ccflags-y += -I$(srctree)/kernel/sched/

# platform driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6991) += snd-soc-mt6991-afe.o
snd-soc-mt6991-afe-objs += \
	mt6991-afe-pcm.o \
	mt6991-afe-clk.o \
	mt6991-afe-gpio.o \
	mt6991-dai-adda.o \
	mt6991-afe-control.o \
	mt6991-dai-i2s.o \
	mt6991-dai-hw-gain.o \
	mt6991-dai-src.o \
	mt6991-dai-pcm.o \
	mt6991-dai-hostless.o \
	mt6991-misc-control.o \
	mt6991-dai-tdm.o \
	mt6991-afe-cm.o

# machine driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6991_MT6681) += mt6991-mt6681.o
