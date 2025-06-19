SCP_VERSION := v01
SCP_VERSION_PATH := ./$(SCP_VERSION)

obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_feature_table.o scp_awake.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_dvfs.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_helper.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_excep.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_ipi.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_irq.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_logger.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_l1c.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += $(SCP_VERSION_PATH)/scp_scpctl.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/cm4/


ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/devfreq/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/mfd/mt6358/

ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6335)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6335
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6353)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6353
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6355
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6357
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6358
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359)+= -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/include/mt6359


