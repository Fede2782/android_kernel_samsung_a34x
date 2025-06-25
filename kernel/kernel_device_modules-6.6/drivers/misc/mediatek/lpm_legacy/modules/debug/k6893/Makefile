
LPM_MODULES_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm_legacy/modules

MTK_LPM_DBG_VERSION = v1
MTK_LPM_CPC_VERSION = cpc_v3


ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/$(MTK_LPM_CPC_VERSION)/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/k6893/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/k6893/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm_legacy/modules/include/mt6983/

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt6893-legacy

obj-$(CONFIG_MTK_LPM_LEGACY) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

#${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_dbg_init_test.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_dbg_init.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_dbg_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_dbg_spm_fs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_lpm_logger.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_lpm_trace_event.o

ifeq ($(CONFIG_MTK_LPM_GS_DUMP_SUPPORT),y)
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mt6893_dbg_power_gs.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6359_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_1_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_2_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_power_gs_6315_3_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_dcm_array.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += mtk_cg_array.o
endif

