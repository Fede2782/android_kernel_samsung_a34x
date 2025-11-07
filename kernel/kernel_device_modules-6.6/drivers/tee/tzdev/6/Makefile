ifeq ($(EXTERNAL_BUILD), y)
CONFIG_TZDEV := m
CONFIG_TZDEV_FFA := y
CONFIG_TZ_BOOT_LOG := y
CONFIG_TZLOG := y

ccflags-y += -DCONFIG_TZDEV
ccflags-y += -DCONFIG_TZDEV_MODULE
ccflags-y += -DCONFIG_TZDEV_SK_MULTICORE
ccflags-y += -DCONFIG_TZDEV_USE_ARM_CALLING_CONVENTION
ccflags-y += -DCONFIG_TZ_BOOT_LOG
ccflags-y += -DCONFIG_TZ_BOOT_LOG_PG_CNT=1
ccflags-y += -DCONFIG_TZLOG
ccflags-y += -DCONFIG_TZLOG_PG_CNT=8
ccflags-y += -DCONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES=10
ccflags-y += -DCONFIG_TZDEV_IWI_EVENT=0
ccflags-y += -DCONFIG_TZDEV_IWI_PANIC=1
ccflags-y += -DCONFIG_TZDEV_IW_EVENTS_MAX_EVENTS=65535
ccflags-y += -DCONFIG_TZDEV_FFA
endif

obj-$(CONFIG_TZDEV) += tzdev.o

ifeq ($(CONFIG_TZDEV), m)
tzdev-y += core/subsystem.o
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/$(CONFIG_TEEGRIS_VERSION)/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/$(CONFIG_TEEGRIS_VERSION)
ccflags-$(CONFIG_TZDEV_DEPLOY_TZAR) += -D"_STR(s)=\#s" -D"KBUILD_SRC=_STR($(KBUILD_SRC))"

# Core
tzdev-y						+= core/cdev.o
tzdev-y						+= core/cred.o
tzdev-y						+= core/event.o
tzdev-y						+= core/id_limits.o
tzdev-y						+= core/iwservice.o
tzdev-y						+= core/iwsock.o
tzdev-y						+= core/kthread_pool.o
tzdev-y						+= core/log.o
tzdev-y						+= core/notifier.o
tzdev-y						+= core/ree_time.o
tzdev-y						+= core/sysdep.o
tzdev-y						+= core/tzdev.o
tzdev-y						+= core/uiwsock.o
tzdev-y						+= core/smc_channel.o
tzdev-y						+= core/iw_mem.o
tzdev-y						+= core/iw_events.o
tzdev-y						+= core/iw_shmem.o
tzdev-y						+= core/uiwshmem.o
tzdev-$(CONFIG_TZLOG)				+= core/iwlog.o
tzdev-$(CONFIG_TZDEV_DEPLOY_TZAR)		+= core/deploy_tzar.o

ifeq ($(CONFIG_TZDEV_TVM), y)
tzdev-y					+= core/tvm_shmem.o
tzdev-y					+= core/iw_mem_impl_tvm.o
tzdev-y					+= core/smc_channel_impl_tvm.o
else ifeq ($(CONFIG_TZDEV_FFA), y)
tzdev-y					+= core/iw_mem_impl_ffa.o
tzdev-y					+= core/smc_channel_impl_ffa.o
else
tzdev-y					+= core/iw_mem_impl_tz.o
tzdev-y					+= core/smc_channel_impl_tz.o
endif

ifeq ($(CONFIG_TZDEV_GUNYAH), y)
tzdev-y						+= core/gunyah_platform.o
else
tzdev-y						+= core/platform.o
endif

# FFA Driver
ifeq ($(CONFIG_TZDEV_FFA_DRIVER_WRAPPER), y)
tzdev-y						+= core/ffa_driver_wrapper.o
else
ifeq ($(CONFIG_TZDEV_FFA), y)
tzdev-y						+= core/ffa.o
endif
endif

# Lib
tzdev-y						+= lib/circ_buf.o
tzdev-y						+= lib/circ_buf_packet.o

# Extensions
tzdev-$(CONFIG_TZDEV_BOOST)			+= extensions/boost.o
tzdev-$(CONFIG_TZDEV_CPUFREQ_TRANSITION)	+= extensions/cpufreq_trans.o
tzdev-$(CONFIG_TZDEV_HOTPLUG)			+= extensions/hotplug.o
tzdev-$(CONFIG_TZIRS)				+= extensions/irs.o
tzdev-$(CONFIG_TZ_SCMA)				+= extensions/scma.o
tzdev-$(CONFIG_ION_FD2PHYS)			+= extensions/ion_fd2phys_v$(CONFIG_ION_FD2PHYS_VARIANT).o

# TEE client API
tzdev-y						+= teec/context.o
tzdev-y						+= teec/misc.o
tzdev-y						+= teec/session.o
tzdev-y						+= teec/shared_memory.o

# Debug
tzdev-$(CONFIG_TZPROFILER)			+= debug/profiler.o
tzdev-$(CONFIG_TZ_BOOT_LOG)			+= debug/iw_boot_log.o
tzdev-$(CONFIG_TZ_PANIC_DUMP)			+= debug/panic_dump.o
tzdev-$(CONFIG_TZ_PMF)				+= debug/pmf.o
tzdev-$(CONFIG_TZDEV_TRACING)			+= debug/trace.o
tzdev-$(CONFIG_TZDEV_DEBUG)			+= debug/kmemleak.o

# Tests
tzdev-$(CONFIG_EFUSE_TEST)			+= tests/efuse_test.o
tzdev-$(CONFIG_TZASC_TEST)			+= tests/tzasc_test.o
tzdev-$(CONFIG_TZPC_TEST)			+= tests/tzpc_test.o
tzdev-$(CONFIG_TZ_CHTEST)			+= tests/tz_chtest.o
tzdev-$(CONFIG_TZ_CLK_TEST_HELPER)		+= tests/tz_clk_test_helper.o
tzdev-$(CONFIG_TZ_MEMDUMP_TEST)			+= tests/tz_memdump_test.o
tzdev-$(CONFIG_TZ_MMAP_TEST)			+= tests/tz_mmap_test.o
tzdev-$(CONFIG_TZ_MMAP_FFA_TEST)		+= tests/tz_mmap_ffa_test.o
tzdev-$(CONFIG_TZ_PRINTK_TEST)			+= tests/tz_printk_test.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/tz_tee_kernel_api_test.o
tzdev-$(CONFIG_TZ_SCMA_TEST)			+= tests/tz_scma_test.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test_send_ta.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test_cancellation.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test_null_args.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test_shared_ref.o
tzdev-$(CONFIG_TZ_TEE_KERNEL_API_TEST)		+= tests/teec/tee_test_sleep.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
