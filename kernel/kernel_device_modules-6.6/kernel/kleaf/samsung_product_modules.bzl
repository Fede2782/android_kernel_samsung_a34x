product_device_modules = [
    # "drivers/samsung/sec_reboot.ko",
    # "drivers/samsung/sec_reset.ko",
    # "drivers/samsung/sec_ext.ko",
    "drivers/tee/tui/tuihw.ko",
    "drivers/tee/tui/tuihw-inf.ko",
    "sound/soc/codecs/tfa9878/snd-soc-tfa_sysfs.ko",
    "sound/soc/codecs/tfa9878/snd-soc-tfa98xx.ko",
    "drivers/input/touchscreen/goodix/berlin/goodix_ts_berlin.ko",
    "drivers/input/sec_input/sec_tsp_log.ko",
    "drivers/input/sec_input/sec_tsp_dumpkey.ko",
    "drivers/input/sec_input/sec_common_fn.ko",
    "drivers/input/sec_input/sec_cmd.ko",
    "drivers/input/sec_input/sec_input_notifier.ko",
    "drivers/gpu/drm/panel/smcdsd_panel/panels/smcdsd_panel.ko",
    "drivers/leds/leds-mt6360.ko",
    "drivers/misc/mediatek/vow/ver02/mtk-vow.ko",
    "sound/soc/mediatek/vow/mtk-scp-vow.ko",
    "drivers/samsung/pm/sec_wakeup_cpu_allocator.ko",
]

product_gki_modules = [
    # "drivers/watchdog/softdog.ko",
    "drivers/i2c/busses/i2c-gpio.ko",
    "drivers/i2c/algos/i2c-algo-bit.ko",
]
