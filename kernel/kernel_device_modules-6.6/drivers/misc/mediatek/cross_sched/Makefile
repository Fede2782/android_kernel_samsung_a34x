obj-$(CONFIG_MTK_CROSS_SCHEDULER) += cross_sched.o

cross_sched-y += cross_sched_host_main.o

subdir-ccflags-y += -I$(srctree)/kernel/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/vhost/	\
			 -I$(KERNEL_SRC)/drivers/vhost/

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/performance/perf_ioctl
