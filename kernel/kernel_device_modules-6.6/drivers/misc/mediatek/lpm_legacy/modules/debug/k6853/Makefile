
LPM_MODULES_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm_legacy/modules

#ifneq ($(or $(findstring mt6853, $(LPM_MODULE_DEBUG_PLAT_NAME)),\
#	$(findstring mt6853, $(LPM_MODULE_DEBUG_PLAT_NAME))),)
MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3
#endif

ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/k6853/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/k6853/

ifneq ($(PROJECT_DTB_NAMES),mediatek/k6853v1_64_6362)
ccflags-y += -DPMIC_6360
endif

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt6853-legacy
obj-$(CONFIG_MTK_LPM_LEGACY) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6853_dbg_init_test.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += dbg_init.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += dbg_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += dbg_spm_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_logger.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_trace_event.o

ifeq ($(CONFIG_MTK_LPM_GS_DUMP_SUPPORT),y)
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += dbg_power_gs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6359_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6362_array.o
endif
