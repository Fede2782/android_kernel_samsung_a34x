obj-$(CONFIG_MTK_PKVM_TMEM) += pkvm_tmem.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

pkvm_tmem-objs += pkvm_tmem_host.o hyp/kvm_nvhe.o
