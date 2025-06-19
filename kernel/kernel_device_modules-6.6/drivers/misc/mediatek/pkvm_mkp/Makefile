obj-$(CONFIG_MTK_PKVM_MKP) += pkvm_mkp.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

demo_mkp = true
ifdef demo_mkp
ccflags-y += -DDEMO_MKP
ccflags-y += -DDEBUG_MKP_ENABLED
endif

subdir-ccflags-y += -I$(srctree)/drivers/android/ # use debug_kinfo

ccflags-y += -I$(srctree)/$(src)              # needed for trace events
ccflags-y += -DTEXT_OFFSET=$(TEXT_OFFSET)
ccflags-y += -I$(srctree)/arch/arm/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mkp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/aee/mrdump
ccflags-y += -I$(srctree)/security/selinux/include/

pkvm_mkp-objs := mkp_main.o
pkvm_mkp-objs += policy.o mkp_hvc.o mkp_api.o ksym.o hyp/kvm_nvhe.o
ifdef demo_mkp
pkvm_mkp-objs += mkp_demo.o
pkvm_mkp-objs += mkp_rbtree.o
pkvm_mkp-objs += helper.o
pkvm_mkp-objs += mkp_module.o
endif
pkvm_mkp-objs += ext/mkp_pcpu.o
