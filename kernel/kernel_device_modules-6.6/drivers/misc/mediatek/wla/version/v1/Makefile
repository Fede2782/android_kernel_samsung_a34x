WLA_BUILD_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/wla
#source files
MTK_WLA_OBJS += wla.o
MTK_WLA_OBJS += wla_mon.o
MTK_WLA_DBG_OBJS += wla_dbg_fs.o

WLA_NAME=wla-v1
WLA_DBG_NAME=wla-v1-dbg

ccflags-y += -I$(WLA_BUILD_FOLDER)/version/v1/
#mtk-lpm fs source files
include $(WLA_BUILD_FOLDER)/wla_fs/Makefile
ccflags-y += -I$(WLA_BUILD_FOLDER)/wla_fs/
WLA_FS = $(wla_fs_objs:%.o=../../wla_fs/%.o)

#Generic build
obj-$(CONFIG_MTK_WLA) += ${WLA_NAME}.o
obj-$(CONFIG_MTK_WLA) += ${WLA_DBG_NAME}.o

WLA_DBG_SRC = $(MTK_WLA_DBG_OBJS) $(WLA_FS)
${WLA_NAME}-objs += $(MTK_WLA_OBJS)
${WLA_DBG_NAME}-objs += $(WLA_DBG_SRC)
