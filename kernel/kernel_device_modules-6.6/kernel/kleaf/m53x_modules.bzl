product_device_modules = [
    # "drivers/samsung/sec_reboot.ko",
    # "drivers/samsung/sec_reset.ko",
    # "drivers/samsung/sec_ext.ko",
    "drivers/firmware/cirrus/cs_dsp.ko",
    "drivers/tee/tui/tuihw.ko",
    "drivers/tee/tui/tuihw-inf.ko",
    "sound/soc/codecs/snd-soc-wm-adsp.ko",
    "sound/soc/codecs/snd-soc-cirrus-amp.ko",
    "sound/soc/codecs/snd-soc-cs35l41-i2c.ko",
    "drivers/gpu/drm/panel/smcdsd_panel/panels/smcdsd_panel.ko",
    "drivers/input/sec_input/synaptics/synaptics_ts.ko",
    "drivers/input/sec_input/sec_tsp_log.ko",
    "drivers/input/sec_input/sec_tsp_dumpkey.ko",
    "drivers/input/sec_input/sec_common_fn.ko",
    "drivers/input/sec_input/sec_cmd.ko",
    "drivers/input/sec_input/sec_tclm_v2.ko",
    "drivers/leds/leds-mt6360.ko",
]

product_gki_modules = [
    # "drivers/watchdog/softdog.ko",
	"drivers/i2c/busses/i2c-gpio.ko",
    "drivers/i2c/algos/i2c-algo-bit.ko",
]
