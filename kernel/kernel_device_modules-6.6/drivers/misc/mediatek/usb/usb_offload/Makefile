# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#

### for adsp
ifneq ($(CONFIG_MTK_AUDIODSP_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
endif

### for scp
ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/

ifneq ($(CONFIG_DEVICE_MODULES_USB_XHCI_MTK), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/usb/usb_xhci
else
ifneq ($(CONFIG_DEVICE_MODULES_USB_XHCI_MTK), )
ccflags-y += -I$(srctree)/drivers/usb/host
endif
endif

ifneq ($(CONFIG_DEVICE_MODULES_USB_MTU3), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/usb/mtu3
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/usb/usb_boost

ifneq ($(CONFIG_SND_USB_AUDIO), )
ccflags-y += -I$(srctree)/sound/usb
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common/

# define_trace.h needs to know how to find our header
CFLAGS_usb_offload_trace.o	:= -I$(srctree)/$(src)

obj-$(CONFIG_MTK_USB_OFFLOAD)	+= usb_offload.o
ifneq ($(CONFIG_MTK_USB_OFFLOAD), )
usb_offload-y := usb_offload_common.o
usb_offload-y += usb_offload_mem.o
usb_offload-y += usb_offload_hid.o
usb_offload-y += usb_offload_trace.o
usb_offload-y += usb_offload_ipi.o
endif
