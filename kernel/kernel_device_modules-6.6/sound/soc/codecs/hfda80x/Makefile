# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

snd-soc-hfda80x-objs:= hfda80x.o

obj-$(CONFIG_SND_SOC_HFDA80X) += snd-soc-hfda80x.o
