ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
ifneq ($(wildcard $(srctree)/$(src)/mt6761_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6761) += mt6761_dcm.o
mt6761_dcm-objs := mt6761_dcm_internal.o mt6761_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6765_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6765) += mt6765_dcm.o
mt6765_dcm-objs := mt6765_dcm_internal.o mt6765_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6768_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6768) += mt6768_dcm.o
mt6768_dcm-objs := mt6768_dcm_internal.o mt6768_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6781_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6781) += mt6781_dcm.o
mt6781_dcm-objs := mt6781_dcm_internal.o mt6781_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6833_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6833) += mt6833_dcm.o
mt6833_dcm-objs := mt6833_dcm_internal.o mt6833_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6853_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6853) += mt6853_dcm.o
mt6853_dcm-objs := mt6853_dcm_internal.o mt6853_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6885_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6885) += mt6885_dcm.o
mt6885_dcm-objs := mt6885_dcm_internal.o mt6885_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6877_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6877) += mt6877_dcm.o
mt6877_dcm-objs := mt6877_dcm_internal.o mt6877_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6985_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6985) += mt6985_dcm.o
mt6985_dcm-objs := mt6985_dcm_internal.o mt6985_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6878_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6878) += mt6878_dcm.o
mt6878_dcm-objs := mt6878_dcm_internal.o mt6878_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6897_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6897) += mt6897_dcm.o
mt6897_dcm-objs := mt6897_dcm_internal.o mt6897_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6899_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6899) += mt6899_dcm.o
mt6899_dcm-objs := mt6899_dcm_internal.o mt6899_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6989_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6989) += mt6989_dcm.o
mt6989_dcm-objs := mt6989_dcm_internal.o mt6989_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6991_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6991) += mt6991_dcm.o
mt6991_dcm-objs := mt6991_dcm_internal.o mt6991_dcm_autogen.o
endif
