#
# Makefile for the Defex
#

# Features to Enable
PED_ENABLE=true
SAFEPLACE_ENABLE=true
IMMUTABLE_ENABLE=true
IMMUTABLE_ROOT_ENABLE=false
IMMUTABLE_ROOT_V2_ENABLE=false
LP_ENABLE=true
UMH_RESTRICTION_ENABLE=true
TRUSTED_MAP_ENABLE=false
USER_BUILD=false
GKI_BUILD=false
FACTORY_BUILD=false

# Additional debug
LOG_BUFFER_ENABLE=false
LOG_FILE_ENABLE=false
SHOW_RULES_ENABLE=false

# caches to enable
CACHES_ENABLE=true

# OEM Unlock dependency
OEM_UNLOCK_DEPENDENCY=true

# use the ramdisk or system_root to store rules file
RAMDISK_ENABLE=true

# do signing for rules
SIGN_ENABLE=true

# Integrity Feature is applied with permissive mode first.
DEFEX_DEFINES := -DDEFEX_PERMISSIVE_INT

defex-y := core/defex_common.o
defex-y += core/defex_lsm.o
defex-y += core/defex_main.o
defex-y += core/defex_config.o
defex-y += core/defex_rules_proc.o
defex-y += core/defex_tree.o
defex-y += core/defex_tailer.o
defex-y += catch_engine/defex_catch_list.o
defex-y += catch_engine/defex_ht.o
defex-y += defex_rules.o
defex-$(CONFIG_COMPAT) += catch_engine/defex_catch_list_compat.o

ifeq ($(CONFIG_SECURITY_DEFEX_GKI),y)
    ifeq ($(CONFIG_DEFEX_KERNEL_ONLY),y)
        $(warning [DEFEX] Kernel_only BUILD)
    else
        $(warning [DEFEX] GKI BUILD)
        USER_BUILD := true
    endif
    GKI_BUILD := true
endif

ifeq ($(CONFIG_SECURITY_DEFEX_NOBOOTPART),y)
    $(warning [DEFEX] Disable boot partition)
    GKI_BUILD := true
endif

ifeq (,$(CONFIG_SEC_FACTORY))
    ifeq ($(CONFIG_SECURITY_DEFEX_SEC_FACTORY),y)
        FACTORY_BUILD := true
    endif
else
    ifeq ($(CONFIG_SEC_FACTORY),y)
        FACTORY_BUILD := true
    endif
endif

ifeq ($(CONFIG_SECURITY_DEFEX_DTM),y)
    ifneq ($(FACTORY_BUILD), true)
        $(warning [DEFEX] Enable DTM)
        TRUSTED_MAP_ENABLE=true
    endif
endif

ifeq ($(CONFIG_SECURITY_DEFEX_IMR),y)
    ifneq ($(FACTORY_BUILD), true)
        $(warning [DEFEX] Enable Immutable root)
        IMMUTABLE_ROOT_ENABLE=true
    endif
endif

ifeq ($(CONFIG_SECURITY_DEFEX_IMR_V2),y)
    ifneq ($(FACTORY_BUILD), true)
        $(warning [DEFEX] Enable Immutable root v2)
        IMMUTABLE_ROOT_V2_ENABLE=true
    endif
endif

ifeq (,$(TARGET_BUILD_VARIANT))
    ifeq ($(CONFIG_SECURITY_DEFEX_USER),y)
        USER_BUILD := true
    endif
else
    ifeq ($(TARGET_BUILD_VARIANT),user)
        USER_BUILD := true
    endif
endif

ifneq ($(wildcard $(srctree)/include/crypto/internal/rsa.h),)
    $(warning [DEFEX] INTEGRITY_ENABLE)
    INTEGRITY_ENABLE=true
endif

ifeq ($(GKI_BUILD), true)
    DEFEX_DEFINES += -DDEFEX_GKI
    ifeq ($(CONFIG_DEFEX_KERNEL_ONLY),y)
        $(warning [DEFEX] Kernel_only BUILD)
    endif
endif

ifeq ($(CONFIG_SECURITY_DEFEX_SINGLE_FILE),y)
    $(warning [DEFEX] Single rule file MODE.)
    DEFEX_DEFINES += -DDEFEX_SINGLE_RULES_FILE
else
    $(warning [DEFEX] Multiple rule file MODE.)
endif

ifeq ($(FACTORY_BUILD), true)
    DEFEX_DEFINES += -DDEFEX_FACTORY_ENABLE
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_LP
endif

ifeq ($(PED_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_PED_ENABLE
endif

ifeq ($(SAFEPLACE_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_SAFEPLACE_ENABLE
endif

ifeq ($(INTEGRITY_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_INTEGRITY_ENABLE
endif

ifeq ($(IMMUTABLE_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_IMMUTABLE_ENABLE
endif

ifeq ($(IMMUTABLE_ROOT_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_IMMUTABLE_ROOT_ENABLE
    ifeq ($(CONFIG_SECURITY_DEFEX_IMR_PERMISSIVE),y)
		DEFEX_DEFINES += -DDEFEX_PERMISSIVE_IMR
    endif
endif

ifeq ($(IMMUTABLE_ROOT_V2_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_IMMUTABLE_ROOT_V2_ENABLE
    ifeq ($(CONFIG_SECURITY_DEFEX_IMR_V2_PERMISSIVE),y)
        DEFEX_DEFINES += -DDEFEX_PERMISSIVE_IMR_V2
    endif
endif

ifeq ($(LP_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_LP_ENABLE
endif

ifeq ($(UMH_RESTRICTION_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_UMH_RESTRICTION_ENABLE
endif

ifeq ($(CACHES_ENABLE), true)
    defex-y += catch_engine/defex_caches.o
    DEFEX_DEFINES += -DDEFEX_CACHES_ENABLE
endif

ifeq ($(OEM_UNLOCK_DEPENDENCY), true)
    DEFEX_DEFINES += -DDEFEX_DEPENDING_ON_OEMUNLOCK
endif

ifeq ($(RAMDISK_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_RAMDISK_ENABLE
ifeq ($(SIGN_ENABLE), true)
    defex-y += cert/defex_cert.o
    defex-y += cert/defex_sign.o
    DEFEX_DEFINES += -DDEFEX_SIGN_ENABLE
endif
endif

ifeq ($(TRUSTED_MAP_ENABLE), true)
    DEFEX_DEFINES += -DDEFEX_TRUSTED_MAP_ENABLE
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_TM
    defex-y += feature_trusted_map/dtm.o
    defex-y += feature_trusted_map/dtm_engine.o
    defex-y += feature_trusted_map/dtm_log.o
    defex-y += feature_trusted_map/dtm_utils.o
    defex-y += feature_trusted_map/ptree.o
endif

ifeq ($(LOG_BUFFER_ENABLE), true)
    defex-y += debug/defex_debug.o
    DEFEX_DEFINES += -DDEFEX_LOG_BUFFER_ENABLE
endif

ifeq ($(LOG_FILE_ENABLE), true)
    defex-y += debug/defex_debug.o
    DEFEX_DEFINES += -DDEFEX_LOG_BUFFER_ENABLE
    DEFEX_DEFINES += -DDEFEX_LOG_FILE_ENABLE
endif

ifeq ($(USER_BUILD), true)
    $(warning [DEFEX] DEBUG_DISABLE)
    ifeq ($(CONFIG_SECURITY_DSMS), y)
        DEFEX_DEFINES += -DDEFEX_DSMS_ENABLE
    endif
else
    $(warning [DEFEX] DEBUG_ENABLE)
    defex-y += debug/defex_debug.o
    defex-y += core/defex_sysfs.o
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_INT
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_SP
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_TM
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_IM
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_IMR
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_IMR_V2
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_LP
    DEFEX_DEFINES += -DDEFEX_PERMISSIVE_PED
    DEFEX_DEFINES += -DDEFEX_DEBUG_ENABLE
    ifeq ($(SHOW_RULES_ENABLE), true)
        defex-y += debug/defex_rules_show.o
        DEFEX_DEFINES += -DDEFEX_SHOW_RULES_ENABLE
    endif
endif

# kunit tests options:
ifeq ($(CONFIG_SEC_KUNIT), y)
    GCOV_PROFILE := y
    DEFEX_DEFINES += -DDEFEX_KUNIT_ENABLED
    ifeq ($(CACHES_ENABLE), true)
    endif
    ifeq ($(RAMDISK_ENABLE), true)
	ifeq ($(SIGN_ENABLE), true)
	endif
    endif
    ifneq ($(USER_BUILD), true)
    endif
    ifeq ($(TRUSTED_MAP_ENABLE), true)
    endif
else
    DEFEX_DEFINES += -D__visible_for_testing=static
endif

ccflags-y := -Wformat

ifneq ($(wildcard $(src)/pack_rules.c),)
    $(warning DEFEX Use new src)
    ccflags-y += -I$(src)
    asflags-y += -Isecurity/samsung/defex_lsm
    ccflags-y += -I$(src)/cert
    asflags-y += -Isecurity/samsung/defex_lsm/cert
    defex_src := $(src)
else
    $(warning DEFEX Use old src)
    ccflags-y += -I$(srctree)/$(src)
    asflags-y += -Isecurity/samsung/defex_lsm
    ccflags-y += -I$(srctree)/$(src)/cert
    asflags-y += -Isecurity/samsung/defex_lsm/cert
    defex_src := $(srctree)/$(src)
endif

ifneq ($(wildcard  $(defex_src)/pack_rules.c),)
    $(warning DEFEX found pack_rules)
    ccflags-y += $(DEFEX_DEFINES)
    asflags-y += $(DEFEX_DEFINES)
    hostprogs := pack_rules
    hostprogs-y := pack_rules
    HOST_EXTRACFLAGS += $(ccflags-y)
    clean-files := $(obj)/defex_packed_rules.inc
    DEPEND_LIST := $(obj)/pack_rules

    quiet_cmd_pack = PACK    $<
          cmd_pack = $(obj)/pack_rules -p $< $@ $(defex_src)/defex_packed_rules.bin

    quiet_cmd_mkey = MAKEKEY $<
          cmd_mkey = cp -n $< $@ 2>/dev/null || true

    $(obj)/core/defex_rules_proc.o: $(obj)/pack_rules $(obj)/defex_packed_rules.inc

    $(obj)/cert/defex_cert.o: $(obj)/cert/pubkey_eng.der $(obj)/cert/pubkey_user.der

    $(obj)/cert/pubkey_eng.der: $(defex_src)/cert/x509_five_eng.der
	$(call cmd,mkey)

    $(obj)/cert/pubkey_user.der: $(defex_src)/cert/x509_five_user.der
	$(call cmd,mkey)

    SOURCE_RULES := $(defex_src)/defex_rules.c
    ifneq ($(wildcard /$(defex_src)/file_list),)
        $(warning '[DEFEX] file_list found')
        SOURCE_RULES := $(defex_src)/defex_rules_reduced.c
        DEPEND_LIST += $(SOURCE_RULES)
        DEPEND_LIST += $(defex_src)/file_list
        clean-files += $(DEPEND_LIST)

        quiet_cmd_reduce = REDUCE  $<
              cmd_reduce = $(obj)/pack_rules -r $< $@ /$(defex_src)/file_list

        $(defex_src)/defex_rules_reduced.c: /$(defex_src)/defex_rules.c $(obj)/pack_rules
		$(call cmd,reduce)
    endif

    $(obj)/defex_packed_rules.inc: $(SOURCE_RULES) $(DEPEND_LIST)
	$(call cmd,pack)
	@cp -n $(obj)/pack_rules $(defex_src)/pack_rules 2>/dev/null || true

else
    $(warning DEFEX Not found pack_rules)
    ccflags-y += $(DEFEX_DEFINES)
    asflags-y += $(DEFEX_DEFINES)
endif

obj-$(CONFIG_SECURITY_DEFEX) := $(defex-y)
