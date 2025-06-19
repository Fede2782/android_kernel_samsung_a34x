# MTK include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

snd-soc-ak7709-objs := ak7709.o
obj-$(CONFIG_SND_SOC_AK7709) += snd-soc-ak7709.o
