# SPDX-License-Identifier: GPL-2.0

# obj-y += imx586mipiraw_Sensor.o
# obj-y += imx586_eeprom.o

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/imx586mipiraw_Sensor.o \
	$(subdrv-rpath)/imx586_eeprom.o \

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif
