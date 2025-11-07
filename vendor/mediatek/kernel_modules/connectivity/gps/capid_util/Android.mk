LOCAL_PATH := $(call my-dir)

ifeq ($(MTK_GPS_SUPPORT), yes)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := capid_util.c pmic_op.c chip/pmic_chip_data.c chip/mt6685_pmic_chip.c
LOCAL_MODULE := gps_meta_capid_util.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

include $(MTK_KERNEL_MODULE)

endif