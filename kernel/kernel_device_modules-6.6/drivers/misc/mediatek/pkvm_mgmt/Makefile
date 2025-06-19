obj-$(CONFIG_MTK_PKVM_MTK_SMC_HANDLER) += pkvm_mgmt.o


$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S

pkvm_mgmt-objs += pkvm_mgmt_host.o hyp/kvm_nvhe.o
