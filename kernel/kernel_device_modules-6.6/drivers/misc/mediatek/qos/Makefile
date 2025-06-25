ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
	ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
	ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v0/
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/mt6873
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V3),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v3
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi
ccflags-y += -DMTK_SCMI
endif

ccflags-y += -I$(srctree)/$(src)

ifeq ($(CONFIG_MTK_QOS_LEGACY),y)
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos_legacy.o
mtk_qos_legacy-objs := mtk_qos_common.o mtk_qos_bound.o mtk_qos_sysfs.o qos-legacy.o mtk_qos_ipi_v1.o
else
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos.o
ifeq ($(CONFIG_MTK_QOS_MT6893),y)
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o mtk_qos_share.o qos-v6893.o
else ifeq ($(CONFIG_MTK_QOS_MT6833),y)
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o mtk_qos_share.o qos-v6833.o
else
mtk_qos-objs := mtk_qos_common.o  mtk_qos_bound.o  mtk_qos_sysfs.o mtk_qos_share.o qos-v6873.o
endif
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
mtk_qos-objs +=  mtk_qos_ipi_v2.o
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V3),y)
mtk_qos-objs +=  mtk_qos_scmi.o
endif