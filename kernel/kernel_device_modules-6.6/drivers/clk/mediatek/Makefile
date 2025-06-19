# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \

obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-common.o
clk-common-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-mtk.o clk-pll.o clk-gate.o clk-mux.o clkchk.o clk-fmeter.o mtk-pd-chk.o clkdbg.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MEDIATEK) += clk-disable-unused.o

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6781.c),)
obj-$(CONFIG_COMMON_CLK_MT6781) += clk-mt6781.o
obj-$(CONFIG_COMMON_CLK_MT6781) += clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6781) += clkdbg-mt6781.o
obj-$(CONFIG_COMMON_CLK_MT6781) += clk-fmeter-mt6781.o
obj-$(CONFIG_COMMON_CLK_MT6781) += clk-chk-mt6781.o
clk-chk-mt6781-$(CONFIG_COMMON_CLK_MT6781) += clkchk-mt6781.o
obj-$(CONFIG_COMMON_CLK_MT6781) += clk-mt6781-pg.o
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6853.c),)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-mt6853.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU0) += clk-mt6853-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU1) += clk-mt6853-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_VCORE) += clk-mt6853-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6853_APU_CONN) += clk-mt6853-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6853_AUDIOSYS) += clk-mt6853-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_MAIN) += clk-mt6853-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWA) += clk-mt6853-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6853_CAMSYS_RAWB) += clk-mt6853-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS1) += clk-mt6853-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMGSYS2) += clk-mt6853-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_C) += clk-mt6853-impc.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_E) += clk-mt6853-impe.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_N) += clk-mt6853-impn.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_S) += clk-mt6853-imps.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_W) += clk-mt6853-impw.o
obj-$(CONFIG_COMMON_CLK_MT6853_IMP_IIC_WRAP_WS) += clk-mt6853-impws.o
obj-$(CONFIG_COMMON_CLK_MT6853_IPESYS) += clk-mt6853-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6853_MDPSYS_CONFIG) += clk-mt6853-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6853_MFGCFG) += clk-mt6853-mfg.o
obj-$(CONFIG_COMMON_CLK_MT6853_MMSYS_CONFIG) += clk-mt6853-mm.o
obj-$(CONFIG_COMMON_CLK_MT6853_SCP_ADSP) += clk-mt6853-scp_par.o
obj-$(CONFIG_COMMON_CLK_MT6853_VDEC_GCON) += clk-mt6853-vdec.o
obj-$(CONFIG_COMMON_CLK_MT6853_VENC_GCON) += clk-mt6853-venc.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6877.c),)
obj-$(CONFIG_COMMON_CLK_MT6877) += clk-mt6877.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6877) += clk-mt6877-pg.o
obj-$(CONFIG_COMMON_CLK_MT6877) += clkchk-mt6877.o
obj-$(CONFIG_COMMON_CLK_MT6877) += clk-fmeter-mt6877.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6877) += clkdbg-mt6877.o
endif
obj-$(CONFIG_COMMON_CLK_MT6877) += pd-chk-mt6877.o
pd-chk-mt6877-$(CONFIG_COMMON_CLK_MT6877) += mtk-pd-chk-mt6877.o
obj-$(CONFIG_COMMON_CLK_MT6877_APU) += clk-mt6877-apu.o
obj-$(CONFIG_COMMON_CLK_MT6877_AUDIOSYS) += clk-mt6877-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6877_CAM) += clk-mt6877-cam.o
obj-$(CONFIG_COMMON_CLK_MT6877_IMG) += clk-mt6877-img.o
obj-$(CONFIG_COMMON_CLK_MT6877_I2C) += clk-mt6877-i2c.o
obj-$(CONFIG_COMMON_CLK_MT6877_IPESYS) += clk-mt6877-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6877_MDPSYS) += clk-mt6877-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6877_MFGCFG) += clk-mt6877-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6877_DISPSYS) += clk-mt6877-mm.o
obj-$(CONFIG_COMMON_CLK_MT6877_MSDC) += clk-mt6877-msdc.o
obj-$(CONFIG_COMMON_CLK_MT6877_SCP_ADSP) += clk-mt6877-scp_par.o
obj-$(CONFIG_COMMON_CLK_MT6877_VDE) += clk-mt6877-vde.o
obj-$(CONFIG_COMMON_CLK_MT6877_VEN) += clk-mt6877-ven.o
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6897.c),)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-mt6897.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6897_ADSP) += clk-mt6897-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6897_CAM) += clk-mt6897-cam.o
obj-$(CONFIG_COMMON_CLK_MT6897_CCU) += clk-mt6897-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6897_IMG) += clk-mt6897-img.o
obj-$(CONFIG_COMMON_CLK_MT6897_MMSYS) += clk-mt6897-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6897_PERI) += clk-mt6897-peri.o
obj-$(CONFIG_COMMON_CLK_MT6897_BUS) += clk-mt6897-bus.o
obj-$(CONFIG_COMMON_CLK_MT6897_MDPSYS) += clk-mt6897-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6897_VLP) += clk-mt6897-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6897_VCODEC) += clk-mt6897-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6886.c),)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-mt6886.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6886_ADSP) += clk-mt6886-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6886_CAM) += clk-mt6886-cam.o
obj-$(CONFIG_COMMON_CLK_MT6886_CCU) += clk-mt6886-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6886_IMG) += clk-mt6886-img.o
obj-$(CONFIG_COMMON_CLK_MT6886_MMSYS) += clk-mt6886-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6886_PERI) += clk-mt6886-peri.o
obj-$(CONFIG_COMMON_CLK_MT6886_BUS) += clk-mt6886-bus.o
obj-$(CONFIG_COMMON_CLK_MT6886_MDPSYS) += clk-mt6886-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6886_SCP) += clk-mt6886-scp.o
obj-$(CONFIG_COMMON_CLK_MT6886_VCODEC) += clk-mt6886-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6893.c),)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-mt6893.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-mt6893-pg.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU0) += clk-mt6893-apu0.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU1) += clk-mt6893-apu1.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU2) += clk-mt6893-apu2.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_VCORE) += clk-mt6893-apuv.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_CONN) += clk-mt6893-apuc.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_MDLA0) += clk-mt6893-apum0.o
obj-$(CONFIG_COMMON_CLK_MT6893_APU_MDLA1) += clk-mt6893-apum1.o
obj-$(CONFIG_COMMON_CLK_MT6893_AUDIOSYS) += clk-mt6893-audsys.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_MAIN) += clk-mt6893-cam_m.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWA) += clk-mt6893-cam_ra.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWB) += clk-mt6893-cam_rb.o
obj-$(CONFIG_COMMON_CLK_MT6893_CAMSYS_RAWC) += clk-mt6893-cam_rc.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMGSYS1) += clk-mt6893-imgsys1.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMGSYS2) += clk-mt6893-imgsys2.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_C) += clk-mt6893-impc.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_E) += clk-mt6893-impe.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_N) += clk-mt6893-impn.o
obj-$(CONFIG_COMMON_CLK_MT6893_IMP_IIC_WRAP_S) += clk-mt6893-imps.o
obj-$(CONFIG_COMMON_CLK_MT6893_IPESYS) += clk-mt6893-ipe.o
obj-$(CONFIG_COMMON_CLK_MT6893_MDPSYS) += clk-mt6893-mdp.o
obj-$(CONFIG_COMMON_CLK_MT6893_MFGCFG) += clk-mt6893-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6893_MMSYS) += clk-mt6893-mm.o
obj-$(CONFIG_COMMON_CLK_MT6893_SCP_ADSP) += clk-mt6893-scp_adsp.o
obj-$(CONFIG_COMMON_CLK_MT6893_VDECSYS) += clk-mt6893-vde2.o
obj-$(CONFIG_COMMON_CLK_MT6893_VDECSYS_SOC) += clk-mt6893-vde1.o
obj-$(CONFIG_COMMON_CLK_MT6893_VENCSYS_C1) += clk-mt6893-ven2.o
obj-$(CONFIG_COMMON_CLK_MT6893_VENCSYS) += clk-mt6893-ven1.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-mt6983.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6983_ADSP) += clk-mt6983-adsp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_CCU) += clk-mt6983-ccu_main.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMG) += clk-mt6983-img.o
obj-$(CONFIG_COMMON_CLK_MT6983_IMP_IIC_WRAP) += clk-mt6983-imp_iic_wrap.o
obj-$(CONFIG_COMMON_CLK_MT6983_VCODEC) += clk-mt6983-vcodec.o
obj-$(CONFIG_COMMON_CLK_MT6983_MDPSYS) += clk-mt6983-mdp_grp.o
obj-$(CONFIG_COMMON_CLK_MT6983_MFG) += clk-mt6983-mfg_top_config.o
obj-$(CONFIG_COMMON_CLK_MT6983_MMSYS) += clk-mt6983-mm.o
obj-$(CONFIG_COMMON_CLK_MT6983_CAM) += clk-mt6983-cam.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6985.c),)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-mt6985.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6985_ADSP) += clk-mt6985-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6985_CAM) += clk-mt6985-cam.o
obj-$(CONFIG_COMMON_CLK_MT6985_CCU) += clk-mt6985-ccu.o
obj-$(CONFIG_COMMON_CLK_MT6985_IMG) += clk-mt6985-img.o
obj-$(CONFIG_COMMON_CLK_MT6985_MMSYS) += clk-mt6985-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6985_PERI) += clk-mt6985-peri.o
obj-$(CONFIG_COMMON_CLK_MT6985_BUS) += clk-mt6985-bus.o
obj-$(CONFIG_COMMON_CLK_MT6985_MDPSYS) += clk-mt6985-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6985_VLP) += clk-mt6985-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6985_VCODEC) += clk-mt6985-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6989.c),)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-mt6989.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6989_ADSP) += clk-mt6989-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6989_CAM) += clk-mt6989-cam.o
obj-$(CONFIG_COMMON_CLK_MT6989_IMG) += clk-mt6989-img.o
obj-$(CONFIG_COMMON_CLK_MT6989_MMSYS) += clk-mt6989-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6989_PERI) += clk-mt6989-peri.o
obj-$(CONFIG_COMMON_CLK_MT6989_BUS) += clk-mt6989-bus.o
obj-$(CONFIG_COMMON_CLK_MT6989_MDPSYS) += clk-mt6989-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6989_VLP) += clk-mt6989-vlp.o
obj-$(CONFIG_COMMON_CLK_MT6989_VCODEC) += clk-mt6989-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6991.c),)
obj-$(CONFIG_COMMON_CLK_MT6991) += clk-mt6991.o
obj-$(CONFIG_COMMON_CLK_MT6991_ADSP) += clk-mt6991-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6991_BUS) += clk-mt6991-bus.o
obj-$(CONFIG_COMMON_CLK_MT6991_CAM) += clk-mt6991-cam.o
obj-$(CONFIG_COMMON_CLK_MT6991_IMG) += clk-mt6991-img.o
obj-$(CONFIG_COMMON_CLK_MT6991_MMSYS) += clk-mt6991-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6991_PERI) += clk-mt6991-peri.o
obj-$(CONFIG_COMMON_CLK_MT6991_MDPSYS) += clk-mt6991-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6991_VCODEC) += clk-mt6991-vcodec.o
obj-$(CONFIG_COMMON_CLK_MT6991_IVI) += clk-mt6991-ivi.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6899.c),)
obj-$(CONFIG_COMMON_CLK_MT6899) += clk-mt6899.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6899_ADSP) += clk-mt6899-adsp.o
obj-$(CONFIG_COMMON_CLK_MT6899_CAM) += clk-mt6899-cam.o
obj-$(CONFIG_COMMON_CLK_MT6899_IMG) += clk-mt6899-img.o
obj-$(CONFIG_COMMON_CLK_MT6899_MMSYS) += clk-mt6899-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6899_PERI) += clk-mt6899-peri.o
obj-$(CONFIG_COMMON_CLK_MT6899_INFRA_INFRACFG_AO_REG) += clk-mt6899-infracfg_ao.o
obj-$(CONFIG_COMMON_CLK_MT6899_MDPSYS) += clk-mt6899-mdpsys.o
obj-$(CONFIG_COMMON_CLK_MT6899_VCODEC) += clk-mt6899-vcodec.o
endif

obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192) += clk-mt8192.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU0) += clk-mt8192-apu0.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU1) += clk-mt8192-apu1.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_VCORE) += clk-mt8192-apu_vcore.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_CONN) += clk-mt8192-apu_conn.o
obj-$(CONFIG_COMMON_CLK_MT8192_APU_MDLA0) += clk-mt8192-apu_mdla0.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_AUDSYS) += clk-mt8192-aud.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_CAMSYS) += clk-mt8192-cam.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IMGSYS) += clk-mt8192-img.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IMP_IIC_WRAP) += clk-mt8192-imp_iic_wrap.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_IPESYS) += clk-mt8192-ipe.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MDPSYS) += clk-mt8192-mdp.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MFGCFG) += clk-mt8192-mfg.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MMSYS) += clk-mt8192-mm.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_MSDC) += clk-mt8192-msdc.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_SCP_ADSP) += clk-mt8192-scp_adsp.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_VDECSYS) += clk-mt8192-vdec.o
obj-$(CONFIG_DEVICE_MODULES_COMMON_CLK_MT8192_VENCSYS) += clk-mt8192-venc.o
ifneq ($(wildcard $(srctree)/$(src)/clk-mt8188.c),)
obj-$(CONFIG_COMMON_CLK_MT8188) += clk-mt8188.o  clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT8188_AUDSYS_SRC) += clk-mt8188-audio_src.o
obj-$(CONFIG_COMMON_CLK_MT8188_CAMSYS) += clk-mt8188-cam.o
obj-$(CONFIG_COMMON_CLK_MT8188_IMGSYS) += clk-mt8188-img.o
obj-$(CONFIG_COMMON_CLK_MT8188_IMP_IIC_WRAP) += clk-mt8188-imp_iic_wrap.o
obj-$(CONFIG_COMMON_CLK_MT8188_IPESYS) += clk-mt8188-ipe.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDECSYS) += clk-mt8188-vdec.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDOSYS0) += clk-mt8188-vdo0.o
obj-$(CONFIG_COMMON_CLK_MT8188_VDOSYS1) += clk-mt8188-vdo1.o
obj-$(CONFIG_COMMON_CLK_MT8188_VENCSYS) += clk-mt8188-venc.o
obj-$(CONFIG_COMMON_CLK_MT8188_VPPSYS0) += clk-mt8188-vpp0.o
obj-$(CONFIG_COMMON_CLK_MT8188_VPPSYS1) += clk-mt8188-vpp1.o
obj-$(CONFIG_COMMON_CLK_MT8188_WPESYS) += clk-mt8188-wpe.o
obj-$(CONFIG_COMMON_CLK_MT8188_CCUSYS) += clk-mt8188-ccu.o
obj-$(CONFIG_COMMON_CLK_MT8188_MFGCFG) += clk-mt8188-mfgcfg.o
endif
obj-$(CONFIG_COMMON_CLK_MT8516) += clk-mt8516.o
obj-$(CONFIG_COMMON_CLK_MT8516_AUDSYS) += clk-mt8516-aud.o
obj-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING) += fhctl.o
fhctl-y += clk-fhctl.o
fhctl-y += clk-fhctl-ap.o
fhctl-y += clk-fhctl-pll.o
fhctl-$(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_SSPM) += clk-fhctl-sspm.o
ifeq ($(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_MCUPM),y)
fhctl-y += clk-fhctl-mcupm.o
ccflags-y += -DUSE_FHCTL_MCUPM
endif
ifeq ($(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_GPUEB),y)
fhctl-y += clk-fhctl-gpueb.o
ccflags-y += -DUSE_FHCTL_GPUEB
endif
fhctl-$(CONFIG_DEBUG_FS) += clk-fhctl-debug.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpueb/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/soc/mediatek/
ifneq ($(strip $(CONFIG_MTK_TINYSYS_VCP_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V2)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V1)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_V0)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v0/
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6853.c),)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-chk-mt6853.o
clk-chk-mt6853-$(CONFIG_COMMON_CLK_MT6853) += clkchk-mt6853.o
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-mt6853-pg.o
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-fmeter-mt6853.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6853) += clk-dbg-mt6853.o
clk-dbg-mt6853-$(CONFIG_COMMON_CLK_MT6853) += clkdbg-mt6853.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6886.c),)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-chk-mt6886.o
clk-chk-mt6886-$(CONFIG_COMMON_CLK_MT6886) += clkchk-mt6886.o
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-fmeter-mt6886.o
obj-$(CONFIG_COMMON_CLK_MT6886) += pd-chk-mt6886.o
pd-chk-mt6886-$(CONFIG_COMMON_CLK_MT6886) += mtk-pd-chk-mt6886.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6886) += clk-dbg-mt6886.o
clk-dbg-mt6886-$(CONFIG_COMMON_CLK_MT6886) += clkdbg-mt6886.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6893.c),)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-chk-mt6893.o
clk-chk-mt6893-$(CONFIG_COMMON_CLK_MT6893) += clkchk-mt6893.o
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-fmeter-mt6893.o
obj-$(CONFIG_COMMON_CLK_MT6893) += pd-chk-mt6893.o
pd-chk-mt6893-$(CONFIG_COMMON_CLK_MT6893) += mtk-pd-chk-mt6893.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6893) += clk-dbg-mt6893.o
clk-dbg-mt6893-$(CONFIG_COMMON_CLK_MT6893) += clkdbg-mt6893.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6897.c),)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-chk-mt6897.o
clk-chk-mt6897-$(CONFIG_COMMON_CLK_MT6897) += clkchk-mt6897.o
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-fmeter-mt6897.o
obj-$(CONFIG_COMMON_CLK_MT6897) += pd-chk-mt6897.o
pd-chk-mt6897-$(CONFIG_COMMON_CLK_MT6897) += mtk-pd-chk-mt6897.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6897) += clk-dbg-mt6897.o
clk-dbg-mt6897-$(CONFIG_COMMON_CLK_MT6897) += clkdbg-mt6897.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6983.c),)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-chk-mt6983.o
clk-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkchk-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-fmeter-mt6983.o
obj-$(CONFIG_COMMON_CLK_MT6983) += pd-chk-mt6983.o
pd-chk-mt6983-$(CONFIG_COMMON_CLK_MT6983) += mtk-pd-chk-mt6983.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6983) += clk-dbg-mt6983.o
clk-dbg-mt6983-$(CONFIG_COMMON_CLK_MT6983) += clkdbg-mt6983.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6985.c),)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-chk-mt6985.o
clk-chk-mt6985-$(CONFIG_COMMON_CLK_MT6985) += clkchk-mt6985.o
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-fmeter-mt6985.o
obj-$(CONFIG_COMMON_CLK_MT6985) += pd-chk-mt6985.o
pd-chk-mt6985-$(CONFIG_COMMON_CLK_MT6985) += mtk-pd-chk-mt6985.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6985) += clk-dbg-mt6985.o
clk-dbg-mt6985-$(CONFIG_COMMON_CLK_MT6985) += clkdbg-mt6985.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6989.c),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_VCP_SUPPORT)),)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-chk-mt6989.o
clk-chk-mt6989-$(CONFIG_COMMON_CLK_MT6989) += clkchk-mt6989.o
obj-$(CONFIG_COMMON_CLK_MT6989) += pd-chk-mt6989.o
pd-chk-mt6989-$(CONFIG_COMMON_CLK_MT6989) += mtk-pd-chk-mt6989.o
endif
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-fmeter-mt6989.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6989) += clk-dbg-mt6989.o
clk-dbg-mt6989-$(CONFIG_COMMON_CLK_MT6989) += clkdbg-mt6989.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6761.c),)
obj-$(CONFIG_COMMON_CLK_MT6761) += clk-mt6761.o
obj-$(CONFIG_COMMON_CLK_MT6761_AUDIOSYS) += clk-mt6761-audio.o
obj-$(CONFIG_COMMON_CLK_MT6761_CAMSYS) += clk-mt6761-cam.o
obj-$(CONFIG_COMMON_CLK_MT6761_MIPI0ASYS) += clk-mt6761-mipi0a.o
obj-$(CONFIG_COMMON_CLK_MT6761_MMSYS) += clk-mt6761-mm.o
obj-$(CONFIG_COMMON_CLK_MT6761_VCODECSYS) += clk-mt6761-vcodec.o
obj-$(CONFIG_COMMON_CLK_MT6761) += clk-mt6761-pg.o
obj-$(CONFIG_COMMON_CLK_MT6761) += clkchk-mt6761.o
#obj-$(CONFIG_COMMON_CLK_MT6761) += mt6761_clkmgr.o
obj-$(CONFIG_COMMON_CLK_MT6761) += pd-chk-mt6761.o
pd-chk-mt6761-$(CONFIG_COMMON_CLK_MT6761) += mtk-pd-chk-mt6761.o
obj-$(CONFIG_COMMON_CLK_MT6761) += clk-fmeter-mt6761.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6761) += clkdbg-mt6761.o
clk-dbg-mt6761-$(CONFIG_COMMON_CLK_MT6761) += clkdbg-mt6761.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6765.c),)
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clk-mt6765.o
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clk-mt6765-pg.o
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clkchk-mt6765.o
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clk-fmeter-mt6765.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clkdbg-mt6765.o
clk-dbg-mt6765-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += clkdbg-mt6765.o
obj-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += mt6765_clkmgr.o
clk-clkmgr-mt6765-$(CONFIG_DEVICE_MODULE_COMMON_CLK_MT6765) += mt6765_clkmgr.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/clk-mt6768.c),)
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-mt6768.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-mt6768-pg.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clkchk-mt6768.o
obj-$(CONFIG_COMMON_CLK_MT6768) += pd-chk-mt6768.o
pd-chk-mt6768-$(CONFIG_COMMON_CLK_MT6768) += mtk-pd-chk-mt6768.o
obj-$(CONFIG_COMMON_CLK_MT6768) += clk-fmeter-mt6768.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6768) += clkdbg-mt6768.o
clk-dbg-mt6768-$(CONFIG_COMMON_CLK_MT6768) += clkdbg-mt6768.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6833.c),)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-mt6833.o clk-bringup.o
obj-$(CONFIG_COMMON_CLK_MT6833_AUDIOSYS) += clk-mt6833-audiosys.o
obj-$(CONFIG_COMMON_CLK_MT6833_CAM) += clk-mt6833-cam.o
obj-$(CONFIG_COMMON_CLK_MT6833_MMSYS) += clk-mt6833-mmsys.o
obj-$(CONFIG_COMMON_CLK_MT6833_IMG) += clk-mt6833-img.o
obj-$(CONFIG_COMMON_CLK_MT6833_I2C) += clk-mt6833-i2c.o
obj-$(CONFIG_COMMON_CLK_MT6833_BUS) += clk-mt6833-bus.o
obj-$(CONFIG_COMMON_CLK_MT6833_MFG) += clk-mt6833-mfgcfg.o
obj-$(CONFIG_COMMON_CLK_MT6833_VCODEC) += clk-mt6833-vcodec.o
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6833.c),)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-chk-mt6833.o
clk-chk-mt6833-$(CONFIG_COMMON_CLK_MT6833) += clkchk-mt6833.o
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-fmeter-mt6833.o
obj-$(CONFIG_COMMON_CLK_MT6833) += pd-chk-mt6833.o
pd-chk-mt6833-$(CONFIG_COMMON_CLK_MT6833) += mtk-pd-chk-mt6833.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6833) += clk-dbg-mt6833.o
clk-dbg-mt6833-$(CONFIG_COMMON_CLK_MT6833) += clkdbg-mt6833.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6991.c),)
ifneq ($(strip $(CONFIG_MTK_TINYSYS_VCP_SUPPORT)),)
obj-$(CONFIG_COMMON_CLK_MT6991) += clk-chk-mt6991.o
clk-chk-mt6991-$(CONFIG_COMMON_CLK_MT6991) += clkchk-mt6991.o
obj-$(CONFIG_COMMON_CLK_MT6991) += clk-fmeter-mt6991.o
obj-$(CONFIG_COMMON_CLK_MT6991) += pd-chk-mt6991.o
pd-chk-mt6991-$(CONFIG_COMMON_CLK_MT6991) += mtk-pd-chk-mt6991.o
endif
obj-$(CONFIG_COMMON_CLK_MT6991) += clk-fmeter-mt6991.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6991) += clk-dbg-mt6991.o
clk-dbg-mt6991-$(CONFIG_COMMON_CLK_MT6991) += clkdbg-mt6991.o
endif
endif
ifneq ($(wildcard $(srctree)/$(src)/clk-mt6899.c),)
obj-$(CONFIG_COMMON_CLK_MT6899) += clk-chk-mt6899.o
clk-chk-mt6899-$(CONFIG_COMMON_CLK_MT6899) += clkchk-mt6899.o
obj-$(CONFIG_COMMON_CLK_MT6899) += clk-fmeter-mt6899.o
obj-$(CONFIG_COMMON_CLK_MT6899) += pd-chk-mt6899.o
pd-chk-mt6899-$(CONFIG_COMMON_CLK_MT6899) += mtk-pd-chk-mt6899.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_COMMON_CLK_MT6899) += clk-dbg-mt6899.o
clk-dbg-mt6899-$(CONFIG_COMMON_CLK_MT6899) += clkdbg-mt6899.o
endif
endif
