load("//kernel-6.6:modules.bzl", "get_gki_modules_list", "get_kunit_modules_list")

COMMON_GKI_MODULES_LIST = get_gki_modules_list("arm64") + get_kunit_modules_list("arm64")

mgk_module_outs = COMMON_GKI_MODULES_LIST

mgk_module_ext_outs = [
    "drivers/firmware/arm_ffa/ffa-module.ko",
    "drivers/gpu/drm/display/drm_display_helper.ko",
    "drivers/gpu/drm/drm_dma_helper.ko",
    "drivers/hwtracing/coresight/coresight.ko",
    "drivers/hwtracing/coresight/coresight-dummy.ko",
    "drivers/hwtracing/coresight/coresight-etm4x.ko",
    "drivers/hwtracing/coresight/coresight-funnel.ko",
    "drivers/hwtracing/coresight/coresight-replicator.ko",
    "drivers/hwtracing/coresight/coresight-tmc.ko",
    "drivers/iio/buffer/industrialio-triggered-buffer.ko",
    "drivers/iio/buffer/kfifo_buf.ko",
    "drivers/leds/leds-pwm.ko",
    "drivers/media/v4l2-core/v4l2-flash-led-class.ko",
    "drivers/net/ethernet/microchip/lan743x.ko",
    "drivers/net/pcs/pcs_xpcs.ko",
    "drivers/perf/arm_dsu_pmu.ko",
    "drivers/power/reset/reboot-mode.ko",
    "drivers/power/reset/syscon-reboot-mode.ko",
    "drivers/tee/tee.ko",
    "drivers/thermal/thermal-generic-adc.ko",
    "drivers/usb/phy/phy-generic.ko",
    "net/wireless/cfg80211.ko",
    "net/mac80211/mac80211.ko",
]

mgk_module_eng_outs = mgk_module_ext_outs + [
    "fs/pstore/pstore_blk.ko",
    "fs/pstore/pstore_zone.ko",
]

mgk_module_userdebug_outs = mgk_module_ext_outs + [
    "fs/pstore/pstore_blk.ko",
    "fs/pstore/pstore_zone.ko",
]

mgk_module_user_outs = mgk_module_ext_outs + [
]
