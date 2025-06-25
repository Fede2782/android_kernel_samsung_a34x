ccflags-y += -Wno-frame-address
# Include mtk_idle_internal.h for MTK_IDLE_OPT_XXX
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm/common_v1/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spm/plat_k68/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ecci/port/
# Include mt-plat/upmu_common.h
# Include mt-plat/mtk_ccci_common.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
# Include cpuidle_v3 headers mtk_cpuidle.h for atf/dormant driver
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/cpuidle/
# Include dram header "mtk_dramc.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
# Include sspm header "sspm_ipi.h"
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -DUSE_TINYSYS_SSPM_FOR_SPM
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif
endif
# Include mtk idle headers and cpuidle driver header for external modules
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm/
# Include mtk_eem.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/eem_v2/$(MTK_PLATFORM)/inc/
# Include mmdvfs_mgr.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
# Include uart header "mtk_uart_api.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/uart/include/
# Include teei header teei_client_main.h
ifeq ($(CONFIG_THERMAL), y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/inc/mach/
endif
ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)),)
include $(DEVICE_MODULES_PATH)/drivers/tee/teei/Makefile.include
endif
ifeq ($(CONFIG_MTK_DRAMC), y)
# Include dramc header "mtk_dramc.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
endif
# Include sspm header "sspm_ipi.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/plat_k68/
#obj-$(CONFIG_MTK_SPM) += mtk_spm.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_twam.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_irq.o
#ifeq ($(CONFIG_MTK_DRAMC), y)
#obj-$(CONFIG_MTK_SPM) += mtk_spm_dram.o
#endif
#obj-$(CONFIG_MTK_SPM) += mtk_spm_internal.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_suspend.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_suspend_process.o
#ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
#obj-$(CONFIG_MTK_SPM) += mtk_sspm.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_power.o
#endif
#obj-$(CONFIG_MTK_SPM) += mtk_spm_fs.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_utils.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_cond_check.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_idle.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_vcore_dvfs.o
#obj-$(CONFIG_MTK_SPM) += mtk_idle_module_plat.o
#obj-$(CONFIG_MTK_SPM) += mtk_spm_resource_req_console.o
#obj-$(CONFIG_MTK_SPM) += models/mtk_idle_dram.o
#obj-$(CONFIG_MTK_SPM) += models/mtk_idle_syspll.o
#obj-$(CONFIG_MTK_SPM) += models/mtk_idle_bus26m.o

obj-$(CONFIG_MTK_SPM) += MTK_INTERNAL_SPM.o
MTK_INTERNAL_SPM-y += mtk_spm.o
MTK_INTERNAL_SPM-y += mtk_spm_twam.o
MTK_INTERNAL_SPM-y += mtk_spm_irq.o
#ifeq ($(CONFIG_MTK_DRAMC), y)
MTK_INTERNAL_SPM-y += mtk_spm_dram.o
#endif
MTK_INTERNAL_SPM-y += mtk_spm_internal.o
MTK_INTERNAL_SPM-y += mtk_spm_suspend.o
MTK_INTERNAL_SPM-y += mtk_spm_suspend_process.o
#ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
MTK_INTERNAL_SPM-y += mtk_sspm.o
endif
endif
MTK_INTERNAL_SPM-y += mtk_spm_power.o
MTK_INTERNAL_SPM-y += mtk_spm_fs.o
MTK_INTERNAL_SPM-y += mtk_spm_utils.o
MTK_INTERNAL_SPM-y += mtk_idle_cond_check.o
MTK_INTERNAL_SPM-y += mtk_spm_idle.o
MTK_INTERNAL_SPM-y += mtk_spm_vcore_dvfs.o
MTK_INTERNAL_SPM-y += mtk_idle_module_plat.o
MTK_INTERNAL_SPM-y += mtk_spm_resource_req_console.o
MTK_INTERNAL_SPM-y += models/mtk_idle_dram.o
MTK_INTERNAL_SPM-y += models/mtk_idle_syspll.o
MTK_INTERNAL_SPM-y += models/mtk_idle_bus26m.o

MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_select.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_internal.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_profile.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_twam.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_spm_resource_req.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_sleep.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_sleep_init.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_vcore_dvfs.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_module.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_export_legacy.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_fs/mtk_idle_sysfs.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_fs/mtk_lp_kernfs.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_fs/mtk_lp_sysfs.o
MTK_INTERNAL_SPM-y += ../common_v1/mtk_idle_fs/mtk_lp_sysfs_procfs.o

