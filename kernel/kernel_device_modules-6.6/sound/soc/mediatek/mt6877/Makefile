# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6877-afe-objs := \
	mt6877-afe-pcm.o \
	mt6877-afe-clk.o \
	mt6877-afe-gpio.o \
	mt6877-dai-adda.o \
	mt6877-afe-control.o \
	mt6877-dai-i2s.o \
	mt6877-dai-hw-gain.o \
	mt6877-dai-src.o \
	mt6877-dai-pcm.o \
	mt6877-dai-hostless.o \
	mt6877-misc-control.o

obj-$(CONFIG_SND_SOC_MT6877) += snd-soc-mt6877-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6877_MT6359P) += mt6877-mt6359.o

# MENU Governor Predict(low power)
ifneq ($(CONFIG_MTK_ACAO_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ifneq ($(CONFIG_MTK_AUDIODSP_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
endif

# spm
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

