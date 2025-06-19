subdir-ccflags-y := -Wformat
obj-$(CONFIG_AFC_CHARGER) += muic_afc_gpio.o
muic_afc_gpio-y			:= afc_charger.o

ifeq ($(CONFIG_SEC_KUNIT), y)
obj-$(CONFIG_SEC_KUNIT)			+= kunit_test/
GCOV_PROFILE_afc_charger.o		:= $(CONFIG_SEC_KUNIT)
endif
