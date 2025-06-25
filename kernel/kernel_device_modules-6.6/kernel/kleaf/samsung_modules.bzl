load("//kernel_device_modules-6.6:kernel/kleaf/samsung_product_modules.bzl",
     "product_device_modules",
     "product_gki_modules",
)

# modules in android/kernel/kernel_device_modules-6.x
_device_modules = [
    "block/blk-sec-common.ko",
    "block/blk-sec-stats.ko",
    "block/blk-sec-wb.ko",
    "block/ssg.ko",
    "drivers/samsung/debug/sec_debug.ko",
    "drivers/samsung/debug/sec_extra_info.ko",
    "drivers/samsung/sec_reboot.ko",
    "drivers/samsung/sec_reset.ko",
    "drivers/samsung/sec_ext.ko",
	"drivers/samsung/sec_bootstat.ko",
	"drivers/cpufreq/cpufreq_limit.ko",
    "mm/sec_mm/sec_mm.ko",
	"drivers/input/input_booster/input_booster_lkm.ko",
	"drivers/samsung/pm/sec_pm_debug.ko",
    "sound/soc/samsung/sec_audio_sysfs.ko",
    "sound/soc/samsung/snd_debug_proc.ko",
    "drivers/usb/ss_function/usb_f_conn_gadget.ko",
    "drivers/usb/ss_function/usb_f_dm.ko",
    "drivers/usb/ss_function/usb_f_ss_mon_gadget.ko",
]

samsung_device_modules = _device_modules + product_device_modules

# gki modules in android/kernel-6.x
_gki_modules = [
    "crypto/twofish_common.ko",
    "crypto/twofish_generic.ko",
    "drivers/watchdog/softdog.ko",
]

samsung_gki_modules = _gki_modules + product_gki_modules
