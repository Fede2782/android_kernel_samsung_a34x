obj-$(CONFIG_MTK_PKVM_SMMU) += pkvm_smmu.o

$(obj)/hyp/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/hyp $(obj)/hyp/kvm_nvhe.o

clean-files := hyp/hyp.lds hyp/hyp-reloc.S
pkvm_smmu-objs += pkvm_smmu_host.o hyp/kvm_nvhe.o
