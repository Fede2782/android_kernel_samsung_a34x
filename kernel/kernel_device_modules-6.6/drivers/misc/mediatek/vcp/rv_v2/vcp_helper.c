// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/aee.h>
#include <linux/delay.h>
#include <aee.h>
#include <soc/mediatek/smi.h>
#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
#include <linux/soc/mediatek/devapc_public.h>
#endif
#include "vcp_feature_define.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_vcpctl.h"
#include "vcp_status.h"
#include "vcp.h"
#include "mmup.h"
#ifdef VCP_CLK_FMETER
#include "clk-fmeter.h"
#endif
#include "clk-mtk.h"
#include "clkchk.h"

/* SMMU related header file */
#include "mtk-smmu-v3.h"

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#include <linux/of_reserved_mem.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <mtk_heap.h>
#include "vcp_reservedmem_define.h"
#endif

#if VCP_LOGGER_ENABLE
#include <mt-plat/mrdump.h>
#endif

/* vcp mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include "vcp_ipi_table.h"

#include "vcp_hwvoter_dbg.h"

/* vcp semaphore timeout count definition */
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* vcp ready timeout definition */
#define VCP_30MHZ 30000
#define VCP_READY_TIMEOUT (HZ / 5 * 4) /* 800 milliseconds*/
#define VCP_READY_TIMEOUT_MS 800
#define VCP_A_TIMER 0

/* vcp ipi message buffer */
uint32_t slp_ipi_ack_data;
uint32_t msg_vcp_ready0;

/* vcp ready status for notify*/
unsigned int vcp_ready[VCP_CORE_TOTAL];
DEFINE_MUTEX(vcp_ready_mutex);

/* vcp dvfs variable*/
unsigned int vcp_support;
unsigned int vcp_dbg_log;

/* set flag after driver initial done */
bool driver_init_done;
EXPORT_SYMBOL_GPL(driver_init_done);

/*vcp awake variable*/
int vcp_awake_counts[VCP_CORE_TOTAL];

/*  vcp_reset_status
 *  0: vcp not in reset status
 *  1: vcp in reset status
 */
atomic_t vcp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);
unsigned int vcp_reset_by_cmd;

/*halt user name*/
char *halt_user;

struct vcp_work_struct vcp_sys_reset_work;
struct wakeup_source *vcp_reset_lock;

DEFINE_SPINLOCK(vcp_reset_spinlock);

phys_addr_t vcp_mem_base_phys;
phys_addr_t vcp_mem_base_virt;
phys_addr_t vcp_sec_dump_base_phys;
phys_addr_t vcp_sec_dump_base_virt;
phys_addr_t vcp_mem_logger_phys;
phys_addr_t vcp_mem_logger_virt;
phys_addr_t vcp_mem_size;
phys_addr_t vcp_mem_logger_size;
bool vcp_hwvoter_support = true;
bool is_vcp_shutdown;
struct vcp_regs vcpreg;

static struct workqueue_struct *vcp_workqueue;

static struct workqueue_struct *vcp_reset_workqueue;

#if VCP_LOGGER_ENABLE
static struct workqueue_struct *vcp_logger_workqueue;
#endif
#if VCP_BOOT_TIME_OUT_MONITOR
struct vcp_timer {
	struct timer_list tl;
	enum vcp_core_id core_id;
};
static struct vcp_timer vcp_ready_timer[VCP_CORE_TOTAL];
#endif
static struct vcp_work_struct vcp_A_notify_work[VCP_CORE_TOTAL];
static BLOCKING_NOTIFIER_HEAD(mmup_notifier_list);
static BLOCKING_NOTIFIER_HEAD(vcp_notifier_list);

#if VCP_BOOT_TIME_OUT_MONITOR
static unsigned int vcp_timeout_times;
#endif

static const char vcp_task_list[][2][32] = {
	{"VencCqRoutine0", "VENC"},
	{"VencCqRoutine1", "VENC"},
	{"VencCqRoutine2", "VENC"},
	{"VencCqWaitEvent", "VENC"},
	{"venc_srv", "VENC"},
	{"vdec_core", "VDEC"},
	{"vdec_res", "VDEC"},
	{"vdec_res_srv", "VDEC"},
	{"vdec_srv", "VDEC"},
	{"IDLE", "VCP"},
	{"clk_mminfra_hwv_off", "HWCCF"},
	{"clk_mminfra_hwv_on", "HWCCF"},
	{"clk_serror", "HWCCF"},
	{"hwv_cg_timeout", "HWCCF"},
	{"scpsys_hwv_off", "HWCCF"},
	{"scpsys_hwv_on", "HWCCF"},
	{"scpsys_mminfra_hwv_off", "HWCCF"},
	{"scpsys_mminfra_hwv_on", "HWCCF"},
	{"mmdvfs_dump_bottom_task", "MMDVFS"},
	{"mmdvfs_ipi_task", "MMDVFS"},
	{"mmdvfs_mux_cb_task", "MMDVFS"},
	{"mmdvfs_pd_cb_task", "MMDVFS"},
	{"mmdvfs_stress_task", "MMDVFS"},
	{"mmdvfs_task", "MMDVFS"},
	{"mmdvfsrc_dump_bottom_task", "MMDVFS"},
	{"vmrc_bottom_task", "MMDVFS"},
	{"mmqos_ipi_task", "MMQOS"},
	{"vmm_thread", "VMM"},
};

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define vcp_aee_print(string, args...) do {\
	char vcp_name[100];\
	int ret;\
	ret = snprintf(vcp_name, 100, "[VCP] "string, ##args); \
	if (ret > 0)\
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
			vcp_name, "[VCP] error:"string, ##args); \
		pr_info("[VCP] error:"string, ##args);  \
	} while (0)
#else
#define vcp_aee_print(string, args...) \
	pr_info("[VCP] error:"string, ##args)

#endif

DEFINE_MUTEX(vcp_pw_clk_mutex);
static DEFINE_MUTEX(vcp_A_notify_mutex);
static DEFINE_MUTEX(vcp_feature_mutex);

DEFINE_SPINLOCK(vcp_awake_spinlock);
struct vcp_ipi_irq {
	const char *name;
	int order;
	unsigned int irq_no;
};

struct vcp_ipi_irq vcp_ipi_irqs[] = {
	/* VCP WDT */
	{ "mediatek,vcp", 0, 0},
	/* VCP reserved */
	{ "mediatek,vcp", 1, 0},
	/* MBOX_0 */
	{ "mediatek,vcp", 2, 0},
	/* MBOX_1 */
	{ "mediatek,vcp", 3, 0},
	/* MBOX_2 */
	{ "mediatek,vcp", 4, 0},
	/* MBOX_3 */
	{ "mediatek,vcp", 5, 0},
	/* MBOX_4 */
	{ "mediatek,vcp", 6, 0},
};
#define IRQ_NUMBER  (sizeof(vcp_ipi_irqs)/sizeof(struct vcp_ipi_irq))
struct device *vcp_io_devs[VCP_IOMMU_DEV_NUM];
struct device *vcp_power_devs;

int pwclkcnt;
bool is_suspending;
bool vcp_ao;

struct vcp_status_fp vcp_helper_fp = {
	.vcp_get_reserve_mem_phys   = vcp_get_reserve_mem_phys,
	.vcp_get_reserve_mem_virt   = vcp_get_reserve_mem_virt,
	.vcp_get_reserve_mem_size   = vcp_get_reserve_mem_size,
	.vcp_get_sram_virt          = vcp_get_sram_virt,
	.vcp_register_feature       = vcp_register_feature,
	.vcp_deregister_feature     = vcp_deregister_feature,
	.is_vcp_ready               = is_vcp_ready,
	.vcp_A_register_notify      = vcp_A_register_notify,
	.vcp_A_unregister_notify    = vcp_A_unregister_notify,
	.vcp_cmd                    = vcp_cmd,
	.mmup_enable_count          = mmup_enable_count,
	.is_mmup_enable             = is_mmup_enable,
	.is_vcp_suspending          = is_vcp_suspending,
	.is_vcp_ao                  = is_vcp_ao,
	.get_ipidev                 = get_ipidev,
	.vcp_get_io_device          = vcp_get_io_device,
	.vcp_register_mminfra_cb    = vcp_register_mminfra_cb,
};

#undef pr_debug
#define pr_debug(fmt, arg...) do { \
		if (vcp_dbg_log) \
			pr_info(fmt, ##arg); \
	} while (0)

static void vcp_enable_irqs(void)
{
	int i = 0;

	tasklet_enable(&vcp_A_irq0_tasklet);
	tasklet_enable(&vcp_A_irq1_tasklet);

	for (i = 0; i < vcp_mboxdev.count; i++)
		enable_irq(vcp_mboxdev.info_table[i].irq_num);

	if (vcpreg.core_nums == 2)
		mmup_enable_irqs();

	enable_irq(vcpreg.irq0);
	pr_debug("[VCP] VCP IRQ enabled\n");
}

static void vcp_disable_irqs(void)
{
	int i = 0;

	disable_irq(vcpreg.irq0);

	for (i = 0; i < vcp_mboxdev.count; i++)
		disable_irq(vcp_mboxdev.info_table[i].irq_num);

	if (vcpreg.core_nums == 2)
		mmup_disable_irqs();

	tasklet_disable(&vcp_A_irq0_tasklet);
	tasklet_disable(&vcp_A_irq1_tasklet);

	pr_debug("[VCP] VCP IRQ disabled\n");
}

static void vcp_wait_awake_count(void)
{
	int i = 0;
	unsigned long spin_flags;

	while (vcp_awake_counts[VCP_ID] != 0 || vcp_awake_counts[MMUP_ID] != 0) {
		i += 1;
		mdelay(1);
		if (i > 200) {
			pr_info("wait vcp_awake_counts timeout %d %d\n",
				vcp_awake_counts[VCP_ID], vcp_awake_counts[MMUP_ID]);
			break;
		}
	}

	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	vcp_reset_awake_counts();
	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);
}

/*
 * memory copy to vcp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_to_vcp(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}


/*
 * memory copy from vcp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_from_vcp(void *trg, const void __iomem *src, int size)
{
	int i;
	u32 *t = trg;
	const u32 __iomem *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return  1 :get sema success
 *        -1 :get sema timeout
 */
int get_vcp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), VCP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), VCP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[VCP] get vcp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_notice("[VCP] already hold vcp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return ret;
}

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_vcp_semaphore(int flag)
{
	int read_back;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), VCP_SEMAPHORE);
		read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[VCP] release vcp sema. %d failed\n", flag);
	} else {
		pr_notice("[VCP] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return ret;
}

int mmup_enable_count(void)
{
	return pwclkcnt;
}

bool is_mmup_enable(void)
{
	if (vcpreg.core_nums > 1)
		return true;
	return false;
}

int vcp_register_mminfra_cb(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func)
{
	return 0;
}

enum vcp_core_id get_core_by_feature(enum feature_id id)
{
	int i = 0;

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			return feature_table[i].core_id;
	}

	pr_info("[VCP] %s unsupported feature id %d\n",
		__func__, id);

	return VCP_ID;
}

/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void vcp_A_register_notify(enum feature_id id, struct notifier_block *nb)
{
	enum vcp_core_id core_id = get_core_by_feature(id);

	mutex_lock(&vcp_A_notify_mutex);
	switch (core_id) {
	case VCP_ID:
		blocking_notifier_chain_register(&vcp_notifier_list, nb);
		pr_debug("[VCP] feature %d register vcp notify callback\n", id);
		if (is_vcp_ready_by_coreid(VCP_ID))
			nb->notifier_call(nb, VCP_EVENT_READY, NULL);
		break;
	case MMUP_ID:
		blocking_notifier_chain_register(&mmup_notifier_list, nb);
		pr_debug("[VCP] feature %d register mmup notify callback\n", id);
		if (is_vcp_ready_by_coreid(MMUP_ID))
			nb->notifier_call(nb, VCP_EVENT_READY, NULL);
		break;
	default:
		break;
	}
	mutex_unlock(&vcp_A_notify_mutex);
}

/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void vcp_A_unregister_notify(enum feature_id id, struct notifier_block *nb)
{
	enum vcp_core_id core_id = get_core_by_feature(id);

	mutex_lock(&vcp_A_notify_mutex);
	switch (core_id) {
	case VCP_ID:
		blocking_notifier_chain_unregister(&vcp_notifier_list, nb);
		pr_debug("[VCP] feature %d unregister vcp notify callback\n", id);
		break;
	case MMUP_ID:
		blocking_notifier_chain_unregister(&mmup_notifier_list, nb);
		pr_debug("[VCP] feature %d unregister mmup notify callback\n", id);
		break;
	default:
		break;
	}
	mutex_unlock(&vcp_A_notify_mutex);
}

struct mtk_ipi_device *get_ipidev(enum feature_id id)
{
	enum vcp_core_id core_id = get_core_by_feature(id);

	if (vcpreg.core_nums == 2) {
		if (core_id == MMUP_ID)
			return mmup_get_ipidev();
		else
			return &vcp_ipidev;
	} else
		return &vcp_ipidev;
}

void vcp_schedule_work(struct vcp_work_struct *vcp_ws)
{
	/* for pm disable wait ready, no power check*/
	if (!vcp_workqueue)
		pr_notice("[VCP] vcp_workqueue is NULL\n");
	else
		queue_work(vcp_workqueue, &vcp_ws->work);
}

void vcp_schedule_reset_work(struct vcp_work_struct *vcp_ws)
{
	if (!vcp_reset_workqueue)
		pr_notice("[VCP] vcp_reset_workqueue is NULL\n");
	else
		queue_work(vcp_reset_workqueue, &vcp_ws->work);

}

#if VCP_LOGGER_ENABLE
void vcp_schedule_logger_work(struct vcp_work_struct *vcp_ws)
{
	if (!vcp_logger_workqueue)
		pr_notice("[VCP] vcp_logger_workqueue is NULL\n");
	else
		queue_work(vcp_logger_workqueue, &vcp_ws->work);
}
#endif

/*
 * callback function for work struct
 * notify apps to start their tasks
 * or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void vcp_A_notify_ws(struct work_struct *ws)
{
	struct vcp_work_struct *sws =
		container_of(ws, struct vcp_work_struct, work);
	enum vcp_core_id core_id = sws->flags;

	if (core_id < VCP_CORE_TOTAL) {
		mutex_lock(&vcp_ready_mutex);
		vcp_ready[core_id] = 1;
		mutex_unlock(&vcp_ready_mutex);

		mutex_lock(&vcp_A_notify_mutex);
#if VCP_RECOVERY_SUPPORT
		atomic_set(&vcp_reset_status, RESET_STATUS_STOP);
		__pm_relax(vcp_reset_lock);
#endif
		halt_user = NULL;

		vcp_extern_notify(core_id, VCP_EVENT_READY);
		mutex_unlock(&vcp_A_notify_mutex);

		/*clear reset status and unlock wake lock*/
		pr_notice("[VCP] %s core id %u ready\n", __func__, core_id);
	} else
		pr_notice("[VCP] %s wrong core id %u\n", __func__, core_id);
}

/*
 * mark notify flag to 1 to notify apps to start their tasks
 */
static void vcp_A_set_ready(enum vcp_core_id core_id)
{
	pr_debug("[VCP] %s core id %d()\n", __func__, core_id);

	if (core_id < VCP_CORE_TOTAL) {
#if VCP_BOOT_TIME_OUT_MONITOR
		del_timer(&vcp_ready_timer[core_id].tl);
#endif
		vcp_A_notify_work[core_id].flags = core_id;
		vcp_schedule_work(&vcp_A_notify_work[core_id]);
	}
}

/*
 * callback for reset timer
 * mark notify flag to 0 to generate an exception
 * @param data: unuse
 */
#if VCP_BOOT_TIME_OUT_MONITOR
static void vcp_wait_ready_timeout(struct timer_list *t)
{
	struct vcp_timer *timeout_timer = container_of(t, struct vcp_timer, tl);
	enum vcp_core_id core_id = timeout_timer->core_id;

	vcp_dump_last_regs(1);
#if VCP_RECOVERY_SUPPORT
	if (vcp_timeout_times < 10 && core_id < VCP_CORE_TOTAL)
		vcp_send_reset_wq(core_id, RESET_TYPE_TIMEOUT);
	else
		__pm_relax(vcp_reset_lock);
#endif
	vcp_timeout_times++;
	pr_notice("[VCP] core id %u timeout times=%x\n", core_id, vcp_timeout_times);
}
#endif

/*
 * handle notification from vcp
 * mark vcp is ready for running tasks
 * It is important to call vcp_ram_dump_init() in this IPI handler. This
 * timing is necessary to ensure that the region_info has been initialized.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
int vcp_A_ready_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	unsigned int vcp_image_size = *(unsigned int *)data;

	switch (id) {
	case IPI_IN_VCP_READY_0:
		if (!is_vcp_ready_by_coreid(VCP_ID))
			vcp_A_set_ready(VCP_ID);
		break;
	case IPI_IN_VCP_READY_1:
		if (!is_vcp_ready_by_coreid(MMUP_ID))
			vcp_A_set_ready(MMUP_ID);
		break;
	default:
		break;
	}

	/*verify vcp image size*/
	if (vcp_image_size != VCP_A_TCM_SIZE) {
		pr_notice("[VCP]image size ERROR! AP=0x%x,VCP=0x%x\n",
					VCP_A_TCM_SIZE, vcp_image_size);
		WARN_ON(1);
	}

	return 0;
}

static const char *get_module_by_taskname(const char *taskname)
{
	int i;
	int array_size = ARRAY_SIZE(vcp_task_list);

	if (!taskname)
		return "VCP";

	for (i = 0; i < array_size; i++) {
		if (!strcmp(vcp_task_list[i][0], taskname))
			return vcp_task_list[i][1];
	}

	return "VCP";
}

void trigger_vcp_dump(enum vcp_core_id core_id, char *user)
{
	int i, j;
	int timeout = 50000; /* max wait 0.5s */

	i = 0;
	while (!mutex_trylock(&vcp_pw_clk_mutex)) {
		i += 5;
		mdelay(5);
		if (i > VCP_SYNC_TIMEOUT_MS) {
			pr_notice("[VCP] %s lock fail\n", __func__);
			return;
		}
	}

	if (!is_suspending && is_vcp_ready_by_coreid(core_id)) {
		pr_notice("[VCP] %s trigger vcp core %d dump vcp clk: %d\n", user, core_id,
			mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
		pr_notice("[VCP] Module:%s\n", get_module_by_taskname(user));

		vcp_dump_last_regs(1);
		clkchk_external_dump();

		/* trigger vcp dump */
		if (vcpreg.core_nums == 2) {
			writel(GIPC_MMUP_DUMP, R_GIPC_IN_SET);
			while ((readl(R_GIPC_IN_SET) & GIPC_MMUP_DUMP)) {
				if (timeout == 0) {
					pr_notice("[MMUP] %s failed %x %x\n", __func__,
						readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR));
					break;
				}
				timeout --;
				udelay(10);
			}
		}
		writel(GIPC_VCP_HART0_DUMP, R_GIPC_IN_SET);
		for (j = 0; j < NUM_FEATURE_ID; j++)
			if (feature_table[j].enable)
				pr_info("[VCP] Active feature id %d cnt %d\n",
					feature_table[j].feature, feature_table[j].enable);

		mtk_smi_dbg_hang_detect("VCP dump");
	}
	mutex_unlock(&vcp_pw_clk_mutex);
}

/*
 * @return: 1 if vcp is ready for running tasks
 */
void trigger_vcp_halt(enum vcp_core_id core_id, char *user)
{
	int i, j;

	i = 0;
	while (!mutex_trylock(&vcp_pw_clk_mutex)) {
		i += 5;
		mdelay(5);
		if (i > VCP_SYNC_TIMEOUT_MS) {
			pr_notice("[VCP] %s lock fail\n", __func__);
			return;
		}
	}

	if (!is_suspending && is_vcp_ready_by_coreid(core_id)) {
		if (halt_user == NULL) {
			halt_user = user;

			pr_notice("[VCP] %s trigger vcp core %d halt vcp clk %d\n", user, core_id,
				mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
			pr_notice("[VCP] Module:%s\n", get_module_by_taskname(user));
		}

		vcp_dump_last_regs(1);

		/* trigger halt isr, force vcp enter wfi */
		writel(GIPC_VCP_HART0_HALT, R_GIPC_IN_SET);
		for (j = 0; j < NUM_FEATURE_ID; j++)
			if (feature_table[j].enable)
				pr_info("[VCP] Active feature id %d cnt %d\n",
					feature_table[j].feature, feature_table[j].enable);
		mtk_smi_dbg_hang_detect("VCP EE");
	} else
		pr_notice("[VCP] %s tigger vcp core %d halt but VCP not ready\n", user, core_id);
	mutex_unlock(&vcp_pw_clk_mutex);
}

/*
 * @return: 1 if vcp is ready for running tasks
 */
unsigned int is_vcp_ready_by_coreid(enum vcp_core_id core_id)
{
	switch (core_id) {
	case VCP_ID:
		return vcp_ready[VCP_ID];
	case MMUP_ID:
		if (vcpreg.core_nums == 2)
			return vcp_ready[MMUP_ID];
		else
			return vcp_ready[VCP_ID];
	case VCP_CORE_TOTAL:
	default:
		if (vcpreg.core_nums == 2) {
			if (vcp_ready[MMUP_ID] == 1 && vcp_ready[VCP_ID] == 1)
				return 1;
			else
				return 0;
		} else
			return vcp_ready[VCP_ID];
	}
}

unsigned int is_vcp_ready(enum feature_id id)
{
	enum vcp_core_id core_id = get_core_by_feature(id);

	return is_vcp_ready_by_coreid(core_id);
}

unsigned int is_vcp_suspending(void)
{
	return is_suspending;
}

unsigned int is_vcp_ao(void)
{
	return 1;
}

/*
 * @return: generaltion count of vcp (reset count)
 */
unsigned int get_vcp_generation(void)
{
	return vcp_reset_counts;
}

unsigned int vcp_cmd(enum feature_id id, enum vcp_cmd_id cmd_id, char *user)
{
	enum vcp_core_id core_id = get_core_by_feature(id);

	switch (cmd_id) {
	case VCP_SET_HALT:
	case VCP_SET_HALT_MMINFRA:
		if (core_id < VCP_CORE_TOTAL)
			trigger_vcp_halt(core_id, user);
		break;
	case VCP_GET_GEN:
		return get_vcp_generation();
	case VCP_DUMP:
	case VCP_DUMP_MMINFRA:
		if (core_id < VCP_CORE_TOTAL)
			trigger_vcp_dump(core_id, user);
		break;
	default:
		pr_notice("[VCP] %s wrong cmd id %d", __func__, cmd_id);
		break;
	}
	return 0;
}

uint32_t vcp_wait_ready_sync(void)
{
	int i = 0;
	int j = 0;

	while (!is_vcp_ready_by_coreid(VCP_CORE_TOTAL)) {
		i += 5;
		mdelay(5);
		if (i > VCP_SYNC_TIMEOUT_MS) {
			vcp_dump_last_regs(1);
			for (j = 0; j < NUM_FEATURE_ID; j++)
				if (feature_table[j].enable)
					pr_info("[VCP] feat. id %d cnt %d\n",
						feature_table[j].feature, feature_table[j].enable);
			break;
		}
	}

	return i;
}

#ifdef VCP_CLK_FMETER
extern unsigned int mt_get_fmeter_freq(unsigned int id, enum FMETER_TYPE type);
#endif
int vcp_enable_pm_clk(enum feature_id id)
{
	int ret = 0;
	struct slp_ctrl_data ipi_data;

	if (!vcp_support)
		return -1;

	mutex_lock(&vcp_pw_clk_mutex);
	while (is_suspending) {
		pr_notice("[VCP] %s blocked %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_unlock(&vcp_pw_clk_mutex);
		usleep_range(10000, 20000);
		mutex_lock(&vcp_pw_clk_mutex);
	}

	if (pwclkcnt == 0) {
		vcp_enable_irqs();

		if (!is_vcp_ready_by_coreid(VCP_CORE_TOTAL)) {
			if (reset_vcp()) {
				vcp_disable_irqs();
				mutex_unlock(&vcp_pw_clk_mutex);
				return -1;
			}
		}
	}
	pwclkcnt++;
	if (id != RTOS_FEATURE_ID) {
		ipi_data.cmd = SLP_WAKE_LOCK;
		ipi_data.feature = id;
		ret = mtk_ipi_send_compl(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	}
#ifdef VCP_CLK_FMETER
	pr_notice("[VCP] %s id %d done %d clk %d\n", __func__, id,
		pwclkcnt, mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
#endif
	mutex_unlock(&vcp_pw_clk_mutex);
	return ret;
}

int vcp_disable_pm_clk(enum feature_id id)
{
	int ret = 0;
	int i = 0;
	struct slp_ctrl_data ipi_data;

	if (!vcp_support)
		return -1;

	mutex_lock(&vcp_pw_clk_mutex);
	while (is_suspending) {
		pr_notice("[VCP] %s blocked %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_unlock(&vcp_pw_clk_mutex);
		usleep_range(10000, 20000);
		mutex_lock(&vcp_pw_clk_mutex);
	}

	pr_notice("[VCP] %s id %d entered %d ready %d %d\n", __func__, id,
		pwclkcnt, is_vcp_ready_by_coreid(VCP_ID), is_vcp_ready_by_coreid(MMUP_ID));

	if (id != RTOS_FEATURE_ID) {
		ipi_data.cmd = SLP_WAKE_UNLOCK;
		ipi_data.feature = id;
		ret = mtk_ipi_send_compl(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
					IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
	}
	pwclkcnt--;

	if (pwclkcnt <= 0) {
		for (i = 0; i < NUM_FEATURE_ID; i++)
			pr_info("[VCP][Warning] %s Check feature id %d enable cnt %d\n",
				__func__, feature_table[i].feature, feature_table[i].enable);
		pwclkcnt = 0;
	}
	mutex_unlock(&vcp_pw_clk_mutex);

	return ret;
}

static int vcp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	uint32_t waitCnt = 0, i = 0;
	int ret;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&vcp_A_notify_mutex);
		vcp_extern_notify(VCP_ID, VCP_EVENT_SUSPEND);
		vcp_extern_notify(MMUP_ID, VCP_EVENT_SUSPEND);
		mutex_unlock(&vcp_A_notify_mutex);

		mutex_lock(&vcp_pw_clk_mutex);
		if (!IS_ERR((void const *) vcpreg.vcp_vlp_ao_rsvd7))
			pr_notice("[VCP] PM_SUSPEND_PREPARE entered %d %d rdy %x\n",
				pwclkcnt, is_suspending, readl(VLP_AO_RSVD7));
		else
			pr_notice("[VCP] PM_SUSPEND_PREPARE entered %d %d\n",
				pwclkcnt, is_suspending);
		if ((!is_suspending) && pwclkcnt) {
			is_suspending = true;
#if VCP_RECOVERY_SUPPORT
				/* make sure all reset done */
			flush_workqueue(vcp_reset_workqueue);
#endif
			waitCnt = vcp_wait_ready_sync();
			flush_workqueue(vcp_workqueue);

			mutex_lock(&vcp_ready_mutex);
			for (i = 0; i < VCP_CORE_TOTAL ; i++)
				vcp_ready[i] = 0;
			mutex_unlock(&vcp_ready_mutex);

			vcp_wait_suspend_resume(1);

#if VCP_LOGGER_ENABLE
			flush_workqueue(vcp_logger_workqueue);
#endif
#if VCP_BOOT_TIME_OUT_MONITOR
			for (i = 0; i < VCP_CORE_TOTAL ; i++)
				del_timer(&vcp_ready_timer[i].tl);
#endif
			vcp_wait_core_stop_timeout(VCP_CORE_TOTAL);
			vcp_wait_awake_count();

			if(!vcp_ao) {
				ret = pm_runtime_put_sync(vcp_power_devs);
				if (ret)
					pr_notice("[VCP] %s: pm_runtime_put_sync %d\n"
						, __func__, ret);
				/* wait vcp clr rdy bit */
				vcp_wait_rdy_bit(0);
			}
		}
		is_suspending = true;
		mutex_unlock(&vcp_pw_clk_mutex);

		// SMC call to TFA / DEVAPC
		// arm_smccc_smc(MTK_SIP_KERNEL_VCP_CONTROL, MTK_TINYSYS_VCP_KERNEL_OP_XXX,
		// 0, 0, 0, 0, 0, 0, &res);
		pr_debug("[VCP] PM_SUSPEND_PREPARE ok, waitCnt=%u\n", waitCnt);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		mutex_lock(&vcp_pw_clk_mutex);
		if (!IS_ERR((void const *) vcpreg.vcp_vlp_ao_rsvd7))
			pr_notice("[VCP] PM_POST_SUSPEND entered %d %d rdy %x\n",
				pwclkcnt, is_suspending, readl(VLP_AO_RSVD7));
		else
			pr_notice("[VCP] PM_POST_SUSPEND entered %d %d\n",
				pwclkcnt, is_suspending);
		if (is_suspending && pwclkcnt) {
			if(!vcp_ao) {
				ret = pm_runtime_get_sync(vcp_power_devs);
				if (ret)
					pr_notice("[VCP] %s: pm_runtime_get_sync %d\n"
						, __func__, ret);
				/* wait vcp set rdy bit */
				vcp_wait_rdy_bit(1);
			}
			if (!is_vcp_shutdown) {
				vcp_wait_suspend_resume(0);
				pr_notice("[VCP] core 0x%x 0x%x GPIC 0x%x 0x%x 0x%x 0x%x flag 0x%x 0x%x\n",
					readl(R_CORE0_STATUS), readl(R_CORE1_STATUS),
					readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR),
					readl(AP_R_GPR2), readl(AP_R_GPR3),
					readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));

#if VCP_RECOVERY_SUPPORT
				is_suspending = false;
				waitCnt = vcp_wait_ready_sync();
#endif
			} else {
				/* shutdown flow */
				mutex_lock(&vcp_ready_mutex);
				for (i = 0; i < VCP_CORE_TOTAL ; i++)
					vcp_ready[i] = 0;
				mutex_unlock(&vcp_ready_mutex);

				// trigger halt isr to change spm control power
				writel(GIPC_VCP_HART0_SHUT, R_GIPC_IN_SET);
				if (vcpreg.core_nums == 2) {
					wait_vcp_ready_to_reboot(VCP_ID);
					writel(GIPC_MMUP_SHUT, R_GIPC_IN_SET);
				}
				is_suspending = false;
				pr_notice("[VCP] %s shutdown done\n", __func__);
				mutex_unlock(&vcp_pw_clk_mutex);
				return NOTIFY_OK;
			}
		}
		is_suspending = false;
		mutex_unlock(&vcp_pw_clk_mutex);

		mutex_lock(&vcp_A_notify_mutex);
		vcp_extern_notify(MMUP_ID, VCP_EVENT_RESUME);
		vcp_extern_notify(VCP_ID, VCP_EVENT_RESUME);
		mutex_unlock(&vcp_A_notify_mutex);
		// SMC call to TFA / DEVAPC
		// arm_smccc_smc(MTK_SIP_KERNEL_VCP_CONTROL, MTK_TINYSYS_VCP_KERNEL_OP_XXX,
		// 0, 0, 0, 0, 0, 0, &res);
		pr_debug("[VCP] PM_POST_SUSPEND ok, waitCnt=%u\n", waitCnt);
		return NOTIFY_OK;
	case PM_POST_HIBERNATION:
		pr_notice("[VCP] %s: PM_POST_HIBERNATION\n", __func__);
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static struct notifier_block vcp_pm_notifier_block = {
	.notifier_call = vcp_pm_event,
	.priority = 0,
};

void vcp_set_clk(void)
{
	int ret;

#ifdef VCP_CLK_FMETER
	ret = mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type);
#endif

#ifdef VCP_CLK_FMETER
	if (ret < VCP_30MHZ)
		pr_notice("[VCP] %s: fail clk %d(%d)\n", __func__,
		ret, mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
#endif
}

#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
static bool devapc_power_cb(void)
{
	pr_info("[VCP] %s %d\n", __func__, is_suspending);
	return !is_suspending;
}

static struct devapc_power_callbacks devapc_power_handle = {
	.type = DEVAPC_TYPE_MMUP,
	.query_power = devapc_power_cb,
};
#endif

/*
 * reset vcp and create a timer waiting for vcp notify
 * apps to stop their tasks if needed
 * generate error if reset fail
 * NOTE: this function may be blocked
 *       and should not be called in interrupt context
 * @param reset:    bit[0-3]=0 for vcp enable, =1 for reboot
 *                  bit[4-7]=0 for All, =1 for vcp_A, =2 for vcp_B
 * @return:         0 if success
 */
int reset_vcp(void)
{
	struct arm_smccc_res res;
	uint32_t timeout = VCP_READY_TIMEOUT_MS * 10;

	vcp_set_clk();

	if (vcpreg.core_nums == 2) {
		/* write vcp reserved memory address/size to GRP1/GRP2
		 * to let vcp setup MPU
		 */
		writel((unsigned int)VCP_PACK_IOVA(vcp_mem_base_phys),
			DRAM_RESV_ADDR_REG_MMUP);
		writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG_MMUP);
		writel((unsigned int)VCP_PACK_IOVA(vcp_mem_logger_phys),
			DRAM_RESV_LOGGER_REG_MMUP);

#if VCP_BOOT_TIME_OUT_MONITOR
		vcp_ready_timer[MMUP_ID].tl.expires = jiffies + VCP_READY_TIMEOUT;
		add_timer(&vcp_ready_timer[MMUP_ID].tl);
#endif
		/* To do: fix 33 smc */
		arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_MMUP_KERNEL_OP_RESET_RELEASE,
			1, 0, 0, 0, 0, 0, &res);

#ifdef VCP_CLK_FMETER
		pr_notice("[VCP] %s: CORE1_RSTN_CLR %x %x %x %x ret %lu clk %d\n", __func__,
			readl(DRAM_RESV_ADDR_REG_MMUP), readl(DRAM_RESV_SIZE_REG_MMUP),
			readl(DRAM_RESV_LOGGER_REG_MMUP), readl(R_CORE1_SW_RSTN_CLR), res.a0,
			mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
#endif

		while (!is_vcp_ready_by_coreid(MMUP_ID)) {
			if (timeout == 0) {
				pr_notice("[VCP] MMUP_ID bootup timeout. Stop vcp booting\n");
				return -1;
			}
			timeout --;
			usleep_range(100, 200);
		}
	}

	timeout = VCP_READY_TIMEOUT_MS * 10;

	/* write vcp reserved memory address/size to GRP1/GRP2
	 * to let vcp setup MPU
	 */
	writel((unsigned int)VCP_PACK_IOVA(vcp_mem_base_phys), DRAM_RESV_ADDR_REG_VCP);
	writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG_VCP);
	writel((unsigned int)VCP_PACK_IOVA(vcp_mem_logger_phys + vcp_mem_logger_size),
		DRAM_RESV_LOGGER_REG_VCP);

#if VCP_BOOT_TIME_OUT_MONITOR
	vcp_ready_timer[VCP_ID].tl.expires = jiffies + VCP_READY_TIMEOUT;
	add_timer(&vcp_ready_timer[VCP_ID].tl);
#endif
	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
		MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
		1, 0, 0, 0, 0, 0, &res);

#ifdef VCP_CLK_FMETER
	pr_notice("[VCP] %s: CORE0_RSTN_CLR %x %x %x ret %lu clk %d\n", __func__,
		readl(DRAM_RESV_ADDR_REG_VCP), readl(DRAM_RESV_SIZE_REG_VCP),
		readl(R_CORE0_SW_RSTN_CLR), res.a0,
		mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
#endif

	while (!is_vcp_ready_by_coreid(VCP_ID)) {
		if (timeout == 0) {
			pr_notice("[VCP] VCP_ID bootup timeout. Stop vcp booting\n");
			return -1;
		}
		timeout --;
		usleep_range(100, 200);
	}

	pr_notice("[VCP] %s: boot done\n", __func__);

	return 0;
}

static inline ssize_t vcp_register_on_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;
	unsigned int i;
	int ret = IPI_ACTION_DONE;
	struct slp_ctrl_data ipi_data;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			vcp_register_feature(RTOS_FEATURE_ID);
		if (value == 555) {
			for (i = 0; i < NUM_FEATURE_ID; i++)
				pr_notice("[VCP] %s Check feature id %d enable cnt %d\n",
					__func__, feature_table[i].feature, feature_table[i].enable);
		}
		if (value == 444) {
			ipi_data.cmd = SLP_STATUS_DBG;
			ipi_data.feature = RTOS_FEATURE_ID;
			ret = mtk_ipi_send_compl(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
						IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		}
		if (value == 333) {
			ipi_data.cmd = SLP_WAKE_LOCK;
			ipi_data.feature = RTOS_FEATURE_ID;
			ret = mtk_ipi_send_compl(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
						IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);
		}
		if (ret != IPI_ACTION_DONE)
			pr_notice("[VCP] %s send ipi fail %d\n", __func__, ret);
	}

	return count;
}
DEVICE_ATTR_WO(vcp_register_on);

static inline ssize_t vcp_deregister_off_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;
	int ret = IPI_ACTION_DONE;
	struct slp_ctrl_data ipi_data;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			vcp_deregister_feature(RTOS_FEATURE_ID);
		if (value == 333) {
			ipi_data.cmd = SLP_WAKE_UNLOCK;
			ipi_data.feature = RTOS_FEATURE_ID;
			ret = mtk_ipi_send_compl(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
						IPI_SEND_WAIT, &ipi_data, PIN_OUT_C_SIZE_SLEEP_0, 500);

			if (ret != IPI_ACTION_DONE)
				pr_notice("[VCP] %s send ipi fail %d\n", __func__, ret);
		}
	}
	return count;
}
DEVICE_ATTR_WO(vcp_deregister_off);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
static inline ssize_t vcp_A_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (is_vcp_ready_by_coreid(VCP_ID))
		len += scnprintf(buf + len, PAGE_SIZE, "VCP is ready\n");
	else
		len += scnprintf(buf + len, PAGE_SIZE, "VCP is not ready\n");

	if (is_vcp_ready_by_coreid(MMUP_ID))
		len += scnprintf(buf + len, PAGE_SIZE, "MMUP is ready\n");
	else
		len += scnprintf(buf + len, PAGE_SIZE, "MMUP is not ready\n");

	return len;
}

DEVICE_ATTR_RO(vcp_A_status);

static inline ssize_t vcp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	vcp_dump_last_regs(mmup_enable_count());

	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_status = %08x\n", c0_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc = %08x\n", c0_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr = %08x\n", c0_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp = %08x\n", c0_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc_latch = %08x\n", c0_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr_latch = %08x\n", c0_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp_latch = %08x\n", c0_m->sp_latch);
	if (!vcpreg.twohart)
		goto core1;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_pc = %08x\n", c0_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_lr = %08x\n", c0_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_sp = %08x\n", c0_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_pc_latch = %08x\n", c0_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_lr_latch = %08x\n", c0_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_sp_latch = %08x\n", c0_t1_m->sp_latch);
core1:
	if (vcpreg.core_nums == 1)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_status = %08x\n", c1_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc = %08x\n", c1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr = %08x\n", c1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp = %08x\n", c1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc_latch = %08x\n", c1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr_latch = %08x\n", c1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp_latch = %08x\n", c1_m->sp_latch);
	if (!vcpreg.twohart_core1)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_pc = %08x\n", c1_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_lr = %08x\n", c1_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_sp = %08x\n", c1_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_pc_latch = %08x\n", c1_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_lr_latch = %08x\n", c1_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_sp_latch = %08x\n", c1_t1_m->sp_latch);

end:
	return len;
}

DEVICE_ATTR_RO(vcp_A_reg_status);

static inline ssize_t vcp_A_db_test_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666) {
			vcp_aed(RESET_TYPE_CMD, VCP_ID);
			if (is_vcp_ready_by_coreid(VCP_ID))
				pr_debug("dumping VCP db\n");
			else
				pr_debug("VCP is not ready, try to dump EE\n");
		}
	}

	return count;
}

DEVICE_ATTR_WO(vcp_A_db_test);

static ssize_t vcp_excep_mode_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", vcp_excep_mode);
}

static ssize_t vcp_excep_mode_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == VCP_NO_EXCEP)
			vcp_excep_mode = VCP_NO_EXCEP;
		else if (value == VCP_KE_ENABLE)
			vcp_excep_mode = VCP_KE_ENABLE;
		else if (value == VCP_EE_ENABLE)
			vcp_excep_mode = VCP_KE_ENABLE;
		else if (value == 100)
			vcp_dbg_log = 0;
		else if (value == 101)
			vcp_dbg_log = 1;

		pr_debug("[VCP] vcp_excep_mode = %d, vcp_dbg_log = %d (1:enable, 0:disable)\n"
				, vcp_excep_mode, vcp_dbg_log);
	}
	return n;
}
DEVICE_ATTR_RW(vcp_excep_mode);

static inline ssize_t vcp_A_awake_lock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (is_vcp_ready_by_coreid(VCP_ID)) {
		vcp_awake_lock((void *)VCP_ID);
		len += scnprintf(buf, PAGE_SIZE, "VCP awake lock\n");
	} else
		len += scnprintf(buf, PAGE_SIZE, "VCP is not ready\n");

	if (is_vcp_ready_by_coreid(MMUP_ID)) {
		vcp_awake_lock((void *)MMUP_ID);
		len += scnprintf(buf, PAGE_SIZE, "MMUP awake lock\n");
	} else
		len += scnprintf(buf, PAGE_SIZE, "MMUP is not ready\n");

	return len;
}

DEVICE_ATTR_RO(vcp_A_awake_lock);

static inline ssize_t vcp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (is_vcp_ready_by_coreid(VCP_ID)) {
		vcp_awake_unlock((void *)VCP_ID);
		len += scnprintf(buf, PAGE_SIZE, "VCP awake unlock\n");
	} else
		len += scnprintf(buf, PAGE_SIZE, "VCP is not ready\n");

	if (is_vcp_ready_by_coreid(MMUP_ID)) {
		vcp_awake_unlock((void *)MMUP_ID);
		len += scnprintf(buf, PAGE_SIZE, "MMUP awake unlock\n");
	} else
		len += scnprintf(buf, PAGE_SIZE, "MMUP is not ready\n");

	return len;
}

DEVICE_ATTR_RO(vcp_A_awake_unlock);

enum ipi_debug_opt {
	IPI_TRACKING_OFF,
	IPI_TRACKING_ON,
	IPIMON_SHOW,
	IPI_PROFILING,
	IPI_PBFR,
	IPI_VCP_TEST = 100,
	IPI_VCP_TEST_END = 199,
	IPI_MMUP_TEST = 200,
	IPI_MMUP_TEST_END = 299,
	IPI_DEBUG_MAX = 300,
};

static inline ssize_t vcp_ipi_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	u64 timetick;
	struct vcp_ipi_profile cmd;
	int ret;
	int len = 0;

	if (is_vcp_ready_by_coreid(VCP_ID)) {
		timetick = arch_timer_read_counter();
		cmd.type = IPI_PROFILING;
		cmd.ipi_time_h = (timetick >> 32) & 0xFFFFFFFF;
		cmd.ipi_time_l = timetick & 0xFFFFFFFF;

		ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_TEST_0, 0, &cmd,
			PIN_OUT_SIZE_TEST_0, 0);

		pr_notice("[VCP] tick: %llu\n", timetick);

		len += scnprintf(buf, PAGE_SIZE, "VCP ipi send ret=%d\n", ret);
	} else
		len += scnprintf(buf, PAGE_SIZE, "VCP is not ready\n");

	if (is_vcp_ready_by_coreid(MMUP_ID)) {
		timetick = arch_timer_read_counter();
		cmd.ipi_time_h = (timetick >> 32) & 0xFFFFFFFF;
		cmd.ipi_time_l = timetick & 0xFFFFFFFF;

		ret = mtk_ipi_send(mmup_get_ipidev(), IPI_OUT_TEST_1, 0, &cmd,
			PIN_OUT_SIZE_TEST_1, 0);

		pr_notice("[MMUP] tick: %llu\n", timetick);

		len += scnprintf(buf, PAGE_SIZE, "MMUP ipi send ret=%d\n", ret);
	} else
		len += scnprintf(buf, PAGE_SIZE, "MMUP is not ready\n");

	return len;
}

static inline ssize_t vcp_ipi_test_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int opt, i;
	u64 timetick;
	int ret = IPI_ACTION_DONE;
	struct vcp_ipi_profile cmd;

	if (kstrtouint(buf, 10, &opt) != 0)
		return -EINVAL;

	switch (opt) {
	case IPI_TRACKING_ON:
	case IPI_TRACKING_OFF:
		mtk_ipi_tracking(&vcp_ipidev, opt);
		mtk_ipi_tracking(mmup_get_ipidev(), opt);
		break;
	case IPIMON_SHOW:
		ipi_monitor_dump(&vcp_ipidev);
		ipi_monitor_dump(mmup_get_ipidev());
		break;
	case IPI_PROFILING:
		for (i = 0; i < 100; i++) {
			cmd.type = IPI_PROFILING;
			timetick = arch_timer_read_counter();
			cmd.ipi_time_h = (timetick >> 32) & 0xFFFFFFFF;
			cmd.ipi_time_l = timetick & 0xFFFFFFFF;

			ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_TEST_0, 0, &cmd,
				PIN_OUT_SIZE_TEST_0, 0);

			pr_notice("[VCP] times: %d tick: %llu\n", i, timetick);

			timetick = arch_timer_read_counter();
			cmd.ipi_time_h = (timetick >> 32) & 0xFFFFFFFF;
			cmd.ipi_time_l = timetick & 0xFFFFFFFF;

			ret = mtk_ipi_send(mmup_get_ipidev(), IPI_OUT_TEST_1, 0, &cmd,
				PIN_OUT_SIZE_TEST_1, 0);

			pr_notice("[MMUP] times: %d tick: %llu\n", i, timetick);

			udelay(1000);
		}
		break;
	case IPI_PBFR:
		cmd.type = IPI_PBFR;
		ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_TEST_0, 0, &cmd,
			PIN_OUT_SIZE_TEST_0, 0);

		ret = mtk_ipi_send(mmup_get_ipidev(), IPI_OUT_TEST_1, 0, &cmd,
			PIN_OUT_SIZE_TEST_1, 0);
		break;
	case IPI_VCP_TEST ... IPI_VCP_TEST_END:
		cmd.type = opt;
		ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_TEST_0, 0, &cmd,
			PIN_OUT_SIZE_TEST_0, 0);
		break;
	case IPI_MMUP_TEST ... IPI_MMUP_TEST_END:
		cmd.type = opt;
		ret = mtk_ipi_send(mmup_get_ipidev(), IPI_OUT_TEST_1, 0, &cmd,
			PIN_OUT_SIZE_TEST_1, 0);
		break;
	default:
		pr_info("cmd '%d' is not supported.\n", opt);
		break;
	}

	if (ret != IPI_ACTION_DONE)
		pr_notice("[VCP] opt %u send ipi fail %d\n", opt, ret);

	return n;
}

DEVICE_ATTR_RW(vcp_ipi_test);
#endif  //  CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT

#if VCP_RECOVERY_SUPPORT
void vcp_wdt_reset(int cpu_id)
{
	switch (cpu_id) {
	case 0:
		writel(V_INSTANT_WDT, R_CORE0_WDT_CFG);
		break;
	case 1:
		writel(V_INSTANT_WDT, R_CORE1_WDT_CFG);
		break;
	}
}
EXPORT_SYMBOL(vcp_wdt_reset);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
/*
 * trigger wdt manually (debug use)
 * Warning! watch dog may be refresh just after you set
 */
static ssize_t wdt_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_notice("[VCP] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			vcp_wdt_reset(0);
		else if (value == 667)
			vcp_wdt_reset(1);
		else if (value == 668)
			trigger_vcp_dump(MMUP_ID, "TEST");
		else if (value == 669)
			trigger_vcp_halt(MMUP_ID, "TEST");
		else if (value == 670)
			trigger_vcp_dump(VCP_ID, "TEST");
		else if (value == 671)
			trigger_vcp_halt(VCP_ID, "TEST");
	}
	return count;
}

DEVICE_ATTR_WO(wdt_reset);

/*
 * trigger vcp reset manually (debug use)
 */
static ssize_t vcp_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int magic, trigger, counts;

	if (sscanf(buf, "%d %d %d", &magic, &trigger, &counts) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", __func__, magic, trigger, counts);

	if (magic != 666)
		return -EINVAL;

	vcp_reset_counts = counts;
	if (trigger == 1) {
		vcp_reset_by_cmd = 1;
		vcp_send_reset_wq(VCP_ID, RESET_TYPE_CMD);
	} else if (trigger == 2) {
		vcp_reset_by_cmd = 1;
		vcp_send_reset_wq(MMUP_ID, RESET_TYPE_CMD);
	}
	return n;
}

DEVICE_ATTR_WO(vcp_reset);
/*
 * trigger wdt manually
 * debug use
 */

#endif  //  CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT
#endif

/******************************************************************************
 *****************************************************************************/
static struct miscdevice vcp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vcp",
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
	.fops = &vcp_A_log_file_ops
#endif  //  CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT
};


/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
static int create_files(void)
{
	int ret;

	ret = misc_register(&vcp_device);
	if (unlikely(ret != 0)) {
		pr_notice("[VCP] misc register failed\n");
		return ret;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
#if VCP_LOGGER_ENABLE
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_logger_wakeup_AP);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_LOGGER_ENABLE

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_status);
	if (unlikely(ret != 0))
		return ret;
#endif  // CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT

	ret = device_create_bin_file(vcp_device.this_device
					, &bin_attr_vcp_dump);
	if (unlikely(ret != 0))
		return ret;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT)
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_reg_status);
	if (unlikely(ret != 0))
		return ret;

	/*only support debug db test in engineer build*/
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_db_test);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_excep_mode);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_awake_lock);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_awake_unlock);
	if (unlikely(ret != 0))
		return ret;

	/* VCP IPI Debug sysfs*/
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_ipi_test);
	if (unlikely(ret != 0))
		return ret;

#if VCP_RECOVERY_SUPPORT
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_wdt_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_reset);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_RECOVERY_SUPPORT
	ret = device_create_file(vcp_device.this_device,
					&dev_attr_vcp_register_on);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device,
					&dev_attr_vcp_deregister_off);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device,
					&dev_attr_log_filter);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcpctl);

	if (unlikely(ret != 0))
		return ret;
#endif  //  CONFIG_MTK_TINYSYS_VCP_DEBUG_SUPPORT

	return 0;
}


struct device *vcp_get_io_device(enum VCP_IOMMU_DEV io_num)
{
	return ((io_num >= VCP_IOMMU_DEV_NUM) ? NULL:vcp_io_devs[io_num]);
}

#if VCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)
#define VCP_MEM_RESERVED_KEY "mediatek,reserve-memory-vcp_share"
int vcp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	pr_notice("[VCP]%s %pa %pa\n", __func__, &rmem->base, &rmem->size);
	vcp_mem_base_phys = (phys_addr_t) rmem->base;
	vcp_mem_size = (phys_addr_t) rmem->size;

	return 0;
}

RESERVEDMEM_OF_DECLARE(vcp_reserve_mem_init
			, VCP_MEM_RESERVED_KEY, vcp_reserve_mem_of_init);
#endif  // VCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)

phys_addr_t vcp_get_reserve_mem_phys(enum vcp_reserve_mem_id_t id)
{
	phys_addr_t base_addr = 0;

	if (id >= NUMS_MEM_ID) {
		pr_notice("[VCP] no reserve memory for %d", id);
		return 0;
	}
	base_addr = vcp_reserve_mblock[id].start_phys;
	return base_addr;
}

phys_addr_t vcp_get_reserve_mem_virt(enum vcp_reserve_mem_id_t id)
{
	phys_addr_t base_addr = 0;

	if (id >= NUMS_MEM_ID) {
		pr_notice("[VCP] no reserve memory for %d", id);
		return 0;
	}
	base_addr = vcp_reserve_mblock[id].start_virt;
	return base_addr;
}

phys_addr_t vcp_get_reserve_mem_size(enum vcp_reserve_mem_id_t id)
{
	if (id >= 0U && id < NUMS_MEM_ID)
		return vcp_reserve_mblock[id].size;

	pr_notice("[VCP] no reserve memory for %d", id);
	return 0;
}

void __iomem *vcp_get_sram_virt(void)
{
	return vcpreg.sram;
}

#if VCP_RESERVED_MEM && defined(CONFIG_OF)
static int vcp_alloc_iova(struct device *dev, __u32 size, __u64 *start_phys, __u64 *start_virt)
{
	struct dma_heap *dma_heap = NULL;
	struct dma_buf *dbuf = NULL;
	struct iosys_map map;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;

	/* normal iova */
	dma_heap = dma_heap_find("mtk_mm-uncached");

	if (!dma_heap) {
		pr_notice("[VCP] cannot get heap\n");
		return -EPERM;
	}

	dbuf = dma_heap_buffer_alloc(dma_heap, size,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_notice("[VCP] buffer alloc fail\n");
		return PTR_ERR(dbuf);
	}

	mtk_dma_buf_set_name(dbuf, "vcp_uncache");

	memset(&map, 0, sizeof(struct iosys_map));
	if (dma_buf_vmap_unlocked(dbuf, &map)) {
		pr_notice("[VCP] vmap fail\n");
		return -ENOMEM;
	}

	*start_virt = (__u64)map.vaddr;

	attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_notice("[VCP] buffer attach fail, return\n");
		dma_heap_buffer_free(dbuf);
		return PTR_ERR(attach);
	}

	sgt = dma_buf_map_attachment_unlocked(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		pr_notice("[VCP] buffer map failed, detach and return\n");
		dma_buf_detach(dbuf, attach);
		dma_heap_buffer_free(dbuf);
		return PTR_ERR(sgt);
	}

	*start_phys = (__u64)sg_dma_address(sgt->sgl);

	if (*start_phys == (__u64)NULL || *start_virt == (__u64)NULL) {
		pr_notice("[VCP] allocate failed, va 0x%llx iova 0x%llx len %d\n",
			*start_virt, *start_phys, size);
		return -EPERM;
	}

	return 0;

}

static int vcp_reserve_memory_ioremap(struct platform_device *pdev, struct device *dma_dev)
{
#define MEMORY_TBL_ELEM_NUM (2)
	unsigned int num = (unsigned int)(sizeof(vcp_reserve_mblock)
			/ sizeof(vcp_reserve_mblock[0]));
	enum vcp_reserve_mem_id_t id;
	unsigned int accumlate_memory_size = 0;
	unsigned int vcp_mem_num = 0;
	unsigned int i = 0, m_idx = 0, m_size = 0;
	int ret;
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	const char *mem_key;
	unsigned int vcp_sec_dump_offset, vcp_sec_dump_size;
	phys_addr_t mblock_start_phys, mblock_start_virt, offset;

	if (num != NUMS_MEM_ID) {
		pr_notice("[VCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		WARN_ON(1); //BUG_ON(1);
		return -1;
	}

// Secure dump
	/* Get reserved memory for secure dump*/
	ret = of_property_read_string(pdev->dev.of_node, "vcp-sec-dump-key",
			&mem_key);
	if (ret) {
		pr_info("[VCP] cannot find property\n");
		return -EINVAL;
	}

	rmem_node = of_find_compatible_node(NULL, NULL, mem_key);

	if (!rmem_node) {
		pr_info("[VCP] no node for reserved memory\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("[VCP] cannot lookup reserved memory\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"vcp-secure-dump-offset",
		&vcp_sec_dump_offset);

	if (ret) {
		pr_info("[VCP] cannot find vcp sec dump offset property\n");
		vcpreg.secure_dump = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"vcp-secure-dump-size",
		&vcp_sec_dump_size);

	if (ret) {
		pr_info("[VCP] cannot find vcp sec dump size property\n");
		vcpreg.secure_dump = 0;
	}

	vcp_sec_dump_base_phys = (phys_addr_t) rmem->base + vcp_sec_dump_offset;

	if (vcp_sec_dump_offset + vcp_sec_dump_size > rmem->size) {
		pr_info("[VCP] reserved size is not enough for sec dump");
		vcpreg.secure_dump = 0;
	}
	vcp_sec_dump_base_virt =
		(phys_addr_t)(size_t)ioremap_wc(vcp_sec_dump_base_phys,
		vcp_sec_dump_size);
	pr_notice("[VCP] secure dump, 0x%llx (0x%llx), (0x%x + 0x%x) <= 0x%x",
		vcp_sec_dump_base_phys, vcp_sec_dump_base_virt,
		vcp_sec_dump_offset, vcp_sec_dump_size,
		(unsigned int)rmem->size);


	/* Set reserved memory table */
	vcp_mem_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"vcp-mem-tbl")
				/ MEMORY_TBL_ELEM_NUM;
	if (vcp_mem_num <= 0) {
		pr_notice("[VCP] vcp-mem-tbl not found\n");
		vcp_mem_num = 0;
	}
	pr_info("[VCP] vcp-mem-tbl vcp_mem_num %d\n", vcp_mem_num);

	for (i = 0; i < vcp_mem_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"vcp-mem-tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			pr_notice("Cannot get memory index(%d)\n", i);
			return -1;
		}

		if (m_idx == VCP_SECURE_DUMP_ID && vcpreg.secure_dump) {
			/* secure_dump */
			m_size = vcp_sec_dump_size;
		} else {
			ret = of_property_read_u32_index(pdev->dev.of_node,
				"vcp-mem-tbl",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		}

		if (ret) {
			pr_notice("Cannot get memory size(%d)(%d)\n", i, m_idx);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			pr_notice("[VCP] skip unexpected index, %d\n", m_idx);
			continue;
		}

		vcp_reserve_mblock[m_idx].size = m_size;
		pr_notice("@@@@ reserved: <%d  %d>\n", m_idx, m_size);
	}

	if (!dma_dev) {
		pr_notice("[VCP] memory device is null\n");
		return -1;
	}


	ret = dma_set_mask_and_coherent(dma_dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_info(dma_dev, "64-bit DMA enable failed\n");
		return ret;
	}
	if (!dma_dev->dma_parms) {
		dma_dev->dma_parms =
			devm_kzalloc(dma_dev,
			sizeof(*dma_dev->dma_parms), GFP_KERNEL);
	}
	if (dma_dev->dma_parms) {
		ret = dma_set_max_seg_size(dma_dev,
			(unsigned int)DMA_BIT_MASK(64));
		if (ret)
			dev_info(dma_dev, "Failed to set DMA segment size\n");
	}

	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (vcp_reserve_mblock[id].size == 0)
			continue;

		if (id == VCP_SECURE_DUMP_ID) {
			vcp_reserve_mblock[id].start_phys = vcp_sec_dump_base_phys;
			vcp_reserve_mblock[id].start_virt = vcp_sec_dump_base_virt;
		} else
			accumlate_memory_size += vcp_reserve_mblock[id].size;

	}

	ret = vcp_alloc_iova(dma_dev,
			accumlate_memory_size,
			&mblock_start_phys,
			&mblock_start_virt);

	if (ret) {
		pr_notice("[VCP] alloc iova fail, size %d\n", accumlate_memory_size);
			return ret;
	}

	offset = 0;
	for (i = 0; i < (int)ARRAY_SIZE(vcp_reserve_mblock_remap); i++) {
		id = vcp_reserve_mblock_remap[i];
		if (vcp_reserve_mblock[id].size == 0 || id == VCP_SECURE_DUMP_ID)
			continue;

		vcp_reserve_mblock[id].start_phys = mblock_start_phys + offset;
		vcp_reserve_mblock[id].start_virt = mblock_start_virt + offset;
		offset += (phys_addr_t)vcp_reserve_mblock[id].size;

		if (id == VDEC_MEM_ID) {
			mrdump_mini_add_extra_file(
				vcp_reserve_mblock[id].start_virt,  0,
				DRAM_VDEC_VSI_BUF_LEN + DRAM_VENC_VSI_BUF_LEN +
				DRAM_LOG_BUF_LEN, "VCP_VSI_LAST_LOG");
			pr_notice("[VCP] add vsi and log buffer to mrdump iova:0x%llx, virt:0x%llx\n",
						(uint64_t)vcp_reserve_mblock[id].start_phys,
						(uint64_t)vcp_reserve_mblock[id].start_virt);
		}
		pr_notice("[VCP] [%d] iova:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)vcp_reserve_mblock[id].start_phys,
			(uint64_t)vcp_reserve_mblock[id].start_virt,
			(uint64_t)vcp_reserve_mblock[id].size);
	}

	vcp_mem_base_phys = (phys_addr_t)mblock_start_phys;
	vcp_mem_size = (phys_addr_t)accumlate_memory_size;
	vcp_mem_logger_phys = (phys_addr_t)vcp_reserve_mblock[VCP_A_LOGGER_MEM_ID].start_phys;
	vcp_mem_logger_size = (phys_addr_t)vcp_reserve_mblock[VCP_A_LOGGER_MEM_ID].size;

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		uint64_t start_phys = (uint64_t)vcp_get_reserve_mem_phys(id);
		uint64_t start_virt = (uint64_t)vcp_get_reserve_mem_virt(id);
		uint64_t len = (uint64_t)vcp_get_reserve_mem_size(id);

		pr_notice("[VCP][rsrv_mem-%d] phy:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_phys, start_phys + len - 1, len);
		pr_notice("[VCP][rsrv_mem-%d] vir:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_virt, start_virt + len - 1, len);
	}
#endif  // DEBUG
	return 0;
}
#endif

int vcp_register_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	if (id >= NUM_FEATURE_ID) {
		pr_info("[VCP][Warning] %s unsupported feature id %d\n",
			__func__, id);
		return -1;
	}
	mutex_lock(&vcp_feature_mutex);
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable++;
	}
	ret = vcp_enable_pm_clk(id);
	mutex_unlock(&vcp_feature_mutex);

	return ret;
}

int vcp_deregister_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	if (id >= NUM_FEATURE_ID) {
		pr_info("[VCP][Warning] %s unsupported feature id %d\n",
			__func__, id);
		return -1;
	}
	mutex_lock(&vcp_feature_mutex);
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id) {
			if (feature_table[i].enable == 0) {
				pr_info("[VCP][Warning] %s unbalanced feature id %d enable cnt %d\n",
					__func__, id, feature_table[i].enable);
				mutex_unlock(&vcp_feature_mutex);
				return -1;
			}
			feature_table[i].enable--;
		}
	}
	ret = vcp_disable_pm_clk(id);
	mutex_unlock(&vcp_feature_mutex);

	return ret;
}

/*
 * apps notification
 */
void vcp_extern_notify(enum vcp_core_id core_id, enum VCP_NOTIFY_EVENT notify_status)
{
	pr_debug("[VCP] core id %d %s %d +\n", core_id, __func__, notify_status);
	switch (core_id) {
	case VCP_ID:
		blocking_notifier_call_chain(&vcp_notifier_list, notify_status, NULL);
		break;
	case MMUP_ID:
		blocking_notifier_call_chain(&mmup_notifier_list, notify_status, NULL);
		break;
	default:
		break;
	}
	pr_debug("[VCP] core id %d %s %d -\n", core_id, __func__, notify_status);
}

/*
 * reset awake counter
 */
void vcp_reset_awake_counts(void)
{
	int i;

	/* vcp ready static flag initialise */
	for (i = 0; i < VCP_CORE_TOTAL ; i++)
		vcp_awake_counts[i] = 0;
}

void vcp_awake_init(void)
{
	vcp_reset_awake_counts();
}

void vcp_wait_suspend_resume(bool suspend)
{
	int timeout = 50000; /* max wait 0.5s */

	if (suspend) {
		writel(B_CORE0_SUSPEND, AP_R_GPR2);
		writel(B_CORE1_SUSPEND, AP_R_GPR3);
		writel(0x87654321, R_CORE0_GENERAL_REG0);
		writel(0x87654321, R_CORE1_GENERAL_REG0);
		if (!readl(AP_R_GPR2) || !readl(AP_R_GPR3)) {
			pr_notice("[VCP] [%s] AP_R_GPR2/3 is null %x %x %x %x flag %x %x\n",
				suspend ? "suspend" : "resume",
				readl(AP_R_GPR2), readl(AP_R_GPR3),
				readl(R_CORE0_GENERAL_REG0), readl(R_CORE1_GENERAL_REG0),
				readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
			vcp_dump_last_regs(1);
		}
		writel(B_GIPC4_SETCLR_3, R_GIPC_IN_SET);
	} else {
		writel(B_CORE0_RESUME, AP_R_GPR2);
		writel(B_CORE1_RESUME, AP_R_GPR3);
		writel(0x12345678, R_CORE0_GENERAL_REG0);
		writel(0x12345678, R_CORE1_GENERAL_REG0);
		if (!readl(AP_R_GPR2) || !readl(AP_R_GPR3)) {
			pr_notice("[VCP] [%s] AP_R_GPR2/3 is null %x %x %x %x flag %x %x\n",
				suspend ? "suspend" : "resume",
				readl(AP_R_GPR2), readl(AP_R_GPR3),
				readl(R_CORE0_GENERAL_REG0), readl(R_CORE1_GENERAL_REG0),
				readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
			vcp_dump_last_regs(1);
		}
		writel(B_GIPC4_SETCLR_3, R_GIPC_IN_SET);
	}

	while (--timeout) {
		if (suspend && (readl(R_GPR3_CFGREG_SEC) & (VCP_AP_SUSPEND))
			&& (readl(R_GPR2_CFGREG_SEC) & (MMUP_AP_SUSPEND)))
			break;
		else if (!suspend && !(readl(R_GPR3_CFGREG_SEC) & (VCP_AP_SUSPEND))
			&& !(readl(R_GPR2_CFGREG_SEC) & (MMUP_AP_SUSPEND)))
			break;

		udelay(10);
	}
	if (timeout <= 0) {
		pr_notice("[VCP] vcp %s timeout GPIC 0x%x 0x%x 0x%x 0x%x flag 0x%x 0x%x\n",
			suspend ? "suspend" : "resume",
			readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR),
			readl(AP_R_GPR2), readl(AP_R_GPR3),
			readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
		vcp_dump_last_regs(1);
	}
}

void vcp_wait_rdy_bit(bool rdy)
{
	int timeout = 50000; /* max wait 0.5s */

	if (!IS_ERR((void const *) vcpreg.vcp_vlp_ao_rsvd7)) {
		while (--timeout) {
			if (rdy && (readl(VLP_AO_RSVD7) & (READY_BIT)))
				break;
			else if (!rdy && !(readl(VLP_AO_RSVD7) & (READY_BIT)))
				break;

			udelay(10);
		}
		if (timeout <= 0)
			pr_notice("[VCP] wait vcp %s timeout 0x%x\n",
				rdy ? "set rdy bit" : "clr rdy bit", readl(VLP_AO_RSVD7));
	}
}

void vcp_wait_core_stop_timeout(enum vcp_core_id core_id)
{
	uint32_t core0_halt = 1, core1_halt = 1;
	uint32_t core0_axi = 0, core1_axi = 0;
	uint32_t core0_status = 0, core1_status = 0;
	uint32_t status = 0;

	/* make sure vcp is in idle state */
	int timeout = 50000; /* max wait 0.5s */

	while (--timeout) {
		switch (core_id) {
		case VCP_ID:
			core0_status = readl(R_CORE0_STATUS);
			status = (vcpreg.twohart ? (B_CORE_GATED | B_HART0_HALT | B_HART1_HALT):
				(B_CORE_GATED | B_HART0_HALT));
			core0_halt = ((core0_status & status) == status);
			core0_axi = core0_status & (B_CORE_AXIS_BUSY);
			break;
		case MMUP_ID:
			if (vcpreg.core_nums == 2) {
				core1_status = readl(R_CORE1_STATUS);
				status = (vcpreg.twohart_core1 ?
					(B_CORE_GATED | B_HART0_HALT | B_HART1_HALT):
					(B_CORE_GATED | B_HART0_HALT));
				core1_halt = ((core1_status & status) == status);
				core1_axi = core1_status & (B_CORE_AXIS_BUSY);
			}
			break;
		case VCP_CORE_TOTAL:
		default:
			core0_status = readl(R_CORE0_STATUS);
			status = (vcpreg.twohart ? (B_CORE_GATED | B_HART0_HALT | B_HART1_HALT):
				(B_CORE_GATED | B_HART0_HALT));
			core0_halt = ((core0_status & status) == status);
			core0_axi = core0_status & (B_CORE_AXIS_BUSY);

			if (vcpreg.core_nums == 2) {
				core1_status = readl(R_CORE1_STATUS);
				status = (vcpreg.twohart_core1 ?
					(B_CORE_GATED | B_HART0_HALT | B_HART1_HALT):
					(B_CORE_GATED | B_HART0_HALT));
				core1_halt = ((core1_status & status) == status);
				core1_axi = core1_status & (B_CORE_AXIS_BUSY);
			}
			break;
		}

		pr_debug("[VCP] debug CORE_STATUS vcp: 0x%x, 0x%x\n",
			core0_status, core1_status);

		if (core0_halt && core1_halt && (!core0_axi) && (!core1_axi)) {
			pr_notice("[VCP] idle 0x%x 0x%x GPIC 0x%x 0x%x 0x%x 0x%x flag 0x%x 0x%x\n",
				core0_status, core1_status,
				readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR),
				readl(AP_R_GPR2), readl(AP_R_GPR3),
				readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
			/* VCP stops any activities
			 * and parks at wfi
			 */
			break;
		}
		udelay(10);
	}

	if (timeout <= 0) {
		pr_notice("[VCP] rst timeout 0x%x 0x%x GPIC 0x%x 0x%x 0x%x 0x%x flag 0x%x 0x%x\n",
			core0_status, core1_status,
			readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR),
			readl(AP_R_GPR2), readl(AP_R_GPR3),
			readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
		vcp_dump_last_regs(1);
	}
}

#if VCP_RECOVERY_SUPPORT
/*
 * vcp_set_reset_status, set and return vcp reset status function
 * return value:
 *   0: vcp not in reset status
 *   1: vcp in reset status
 */
unsigned int vcp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&vcp_reset_spinlock, spin_flags);
	if (atomic_read(&vcp_reset_status) == RESET_STATUS_START) {
		spin_unlock_irqrestore(&vcp_reset_spinlock, spin_flags);
		return 1;
	}
	/* vcp not in reset status, set it and return*/
	atomic_set(&vcp_reset_status, RESET_STATUS_START);
	spin_unlock_irqrestore(&vcp_reset_spinlock, spin_flags);
	return 0;
}


/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
void vcp_sys_reset_ws(struct work_struct *ws)
{
	struct vcp_work_struct *sws = container_of(ws
					, struct vcp_work_struct, work);
	unsigned int vcp_reset_type = sws->flags;
	enum vcp_core_id core_id = sws->id;
	unsigned long spin_flags;
	unsigned long c0_rstn = 0, c1_rstn = 0;
	struct arm_smccc_res res;

	pr_notice("[VCP] %s(): vcp_reset_type %d remain %x times, core id %u\n",
		__func__, vcp_reset_type, vcp_reset_counts, core_id);

	if (is_suspending || core_id >= VCP_CORE_TOTAL)
		return;

	/*
	 *   vcp_ready:
	 *   VCP_PLATFORM_STOP  = 0,
	 *   VCP_PLATFORM_READY = 1,
	 */
	mutex_lock(&vcp_ready_mutex);
	vcp_ready[core_id] = 0;
	mutex_unlock(&vcp_ready_mutex);

	/* wake lock AP*/
	__pm_stay_awake(vcp_reset_lock);

	/*workqueue for vcp ee, vcp reset by cmd will not trigger vcp ee*/
	if (vcp_reset_by_cmd == 0 && vcp_excep_mode != VCP_NO_EXCEP) {
		vcp_aed(vcp_reset_type, core_id);
		/* vcp_aee_print("[VCP] %s(): vcp_reset_type %d remain %x times, encnt %d\n",
		 *	__func__, vcp_reset_type, vcp_reset_counts, mmup_enable_count());
		 */
	}

	c0_rstn = readl(R_CORE0_SW_RSTN_SET);
	if (vcpreg.core_nums == 2)
		c1_rstn = readl(R_CORE1_SW_RSTN_SET);

	/* vcp reset by CMD, WDT or awake fail */
	if ((vcp_reset_type == RESET_TYPE_TIMEOUT) ||
		(vcp_reset_type == RESET_TYPE_AWAKE)) {
		/* stop vcp */
		arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
			0, 0, 0, 0, 0, 0, &res);

		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %lx core1 %lx ret %lu\n",
			c0_rstn, c1_rstn, res.a0);
	} else {
		/* reset type vcp WDT or CMD*/
		/* make sure vcp is in idle state */
		vcp_wait_core_stop_timeout(VCP_ID);
		arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
			1, 0, 0, 0, 0, 0, &res);

		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %lx core1 %lx ret %lu\n",
			c0_rstn, c1_rstn, res.a0);
	}

	mutex_lock(&vcp_A_notify_mutex);
	vcp_extern_notify(VCP_ID, VCP_EVENT_STOP);
	mutex_unlock(&vcp_A_notify_mutex);

	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	vcp_awake_counts[VCP_ID] = 0;
	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	vcp_set_clk();


	/* Setup dram reserved address and size for vcp*/
	writel((unsigned int)VCP_PACK_IOVA(vcp_mem_base_phys), DRAM_RESV_ADDR_REG_VCP);
	writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG_VCP);
	writel((unsigned int)VCP_PACK_IOVA(vcp_mem_logger_phys + vcp_mem_logger_size),
		DRAM_RESV_LOGGER_REG_VCP);

	/* start vcp */
	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
			1, 0, 0, 0, 0, 0, &res);

#ifdef VCP_CLK_FMETER
	pr_notice("[VCP] %s: CORE0_RSTN_CLR %x %x %x %x ret %lu clk %d\n", __func__,
		readl(DRAM_RESV_ADDR_REG_VCP), readl(DRAM_RESV_SIZE_REG_VCP),
		readl(DRAM_RESV_LOGGER_REG_VCP), readl(R_CORE0_SW_RSTN_CLR), res.a0,
		mt_get_fmeter_freq(vcpreg.fmeter_ck, vcpreg.fmeter_type));
#endif

	dsb(SY); /* may take lot of time */

#if VCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&vcp_ready_timer[VCP_ID].tl, jiffies + VCP_READY_TIMEOUT);
#endif
	/* clear vcp reset by cmd flag*/
	vcp_reset_by_cmd = 0;
}


/*
 * schedule a work to reset vcp
 * @param type: exception type
 */
void vcp_send_reset_wq(enum vcp_core_id core_id, enum VCP_RESET_TYPE type)
{
	vcp_sys_reset_work.flags = (unsigned int) type;
	vcp_sys_reset_work.id = core_id;
	vcp_reset_counts--;
	vcp_schedule_reset_work(&vcp_sys_reset_work);
}
#endif

void vcp_recovery_init(void)
{
#if VCP_RECOVERY_SUPPORT
	/*create wq for vcp reset*/
	vcp_reset_workqueue = create_singlethread_workqueue("VCP_RESET_WQ");
	if (!vcp_reset_workqueue)
		pr_notice("[VCP] vcp_reset_workqueue create fail\n");

	/*init reset work*/
	INIT_WORK(&vcp_sys_reset_work.work, vcp_sys_reset_ws);

	vcp_reset_lock = wakeup_source_register(NULL, "vcp reset wakelock");
	/* init reset by cmd flag */
	vcp_reset_by_cmd = 0;
#endif
}

static bool vcp_ipi_table_init(struct mtk_mbox_device *vcp_mboxdev, struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i = 0, ret = 0, mbox_id = 0, recv_opt = 0;

	of_property_read_u32(pdev->dev.of_node, "mbox-count", &vcp_mboxdev->count);
	if (!vcp_mboxdev->count) {
		pr_notice("[VCP] mbox count not found\n");
		return false;
	}

	vcp_mboxdev->send_count =
		of_property_count_u32_elems(pdev->dev.of_node, "send-table") / send_item_num;
	if (vcp_mboxdev->send_count <= 0) {
		pr_notice("[VCP] vcp send table not found\n");
		return false;
	}

	vcp_mboxdev->recv_count =
		of_property_count_u32_elems(pdev->dev.of_node, "recv-table") / recv_item_num;
	if (vcp_mboxdev->recv_count <= 0) {
		pr_notice("[VCP] vcp recv table not found\n");
		return false;
	}
	/* alloc and init vcp_mbox_info */
	vcp_mboxdev->info_table = vzalloc(sizeof(struct mtk_mbox_info) * vcp_mboxdev->count);
	if (!vcp_mboxdev->info_table)
		return false;
	vcp_mbox_info = vcp_mboxdev->info_table;
	for (i = 0; i < vcp_mboxdev->count; ++i) {
		vcp_mbox_info[i].id = i;
		vcp_mbox_info[i].slot = 64;
		vcp_mbox_info[i].enable = 1;
		vcp_mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	vcp_mboxdev->pin_send_table =
		vzalloc(sizeof(struct mtk_mbox_pin_send) * vcp_mboxdev->send_count);
	if (!vcp_mboxdev->pin_send_table)
		return false;
	vcp_mbox_pin_send = vcp_mboxdev->pin_send_table;
	for (i = 0; i < vcp_mboxdev->send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num,
				&vcp_mbox_pin_send[i].chan_id);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_send[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send-table",
				i * send_item_num + 2,
				&vcp_mbox_pin_send[i].msg_size);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
	}
	/* alloc and init recv table */
	vcp_mboxdev->pin_recv_table =
		vzalloc(sizeof(struct mtk_mbox_pin_recv) * vcp_mboxdev->recv_count);
	if (!vcp_mboxdev->pin_recv_table)
		return false;
	vcp_mbox_pin_recv = vcp_mboxdev->pin_recv_table;
	for (i = 0; i < vcp_mboxdev->recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num,
				&vcp_mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get ipi id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_recv[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 2,
				&vcp_mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv-table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get recv opt (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_recv[i].recv_opt = recv_opt;
	}

	return true;
}

static int vcp_io_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *smmu_dev = NULL;
	int ret;

	pr_debug("[VCP_IO] %s", __func__);

	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0 || vcp_support == 1) {
		pr_info("Bypass the VCP driver probe\n");
		return -1;
	}

	// VCP iommu devices
	if (smmu_v3_enabled()) {
		dev_info(dev, "[VCP] smmu enable\n");
		smmu_dev = mtk_smmu_get_shared_device(dev);
		if (smmu_dev)
			vcp_io_devs[vcp_support-1] = smmu_dev;
		else
			dev_info(dev, "[VCP] cannot get smmu shared dev\n");
	} else
		vcp_io_devs[vcp_support-1] = dev;

	ret = dma_set_mask_and_coherent(vcp_io_devs[vcp_support-1], DMA_BIT_MASK(64));
	if (ret) {
		dev_info(vcp_io_devs[vcp_support-1], "64-bit DMA enable failed\n");
		return ret;
	}
	if (!vcp_io_devs[vcp_support-1]->dma_parms) {
		vcp_io_devs[vcp_support-1]->dma_parms =
			devm_kzalloc(vcp_io_devs[vcp_support-1],
			sizeof(*vcp_io_devs[vcp_support-1]->dma_parms), GFP_KERNEL);
	}
	if (vcp_io_devs[vcp_support-1]->dma_parms) {
		ret = dma_set_max_seg_size(vcp_io_devs[vcp_support-1],
			(unsigned int)DMA_BIT_MASK(64));
		if (ret)
			dev_info(vcp_io_devs[vcp_support-1], "Failed to set DMA segment size\n");
	}

	return 0;
}

static int vcp_io_device_remove(struct platform_device *dev)
{
	return 0;
}

static void mbox_setup_pin_table(unsigned int mbox)
{
	int i, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;

	for (i = 0; i < vcp_mboxdev.send_count; i++) {
		if (mbox == vcp_mbox_pin_send[i].mbox) {
			vcp_mbox_pin_send[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_send[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_send[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_send[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_send[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_send[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < vcp_mboxdev.recv_count; i++) {
		if (mbox == vcp_mbox_pin_recv[i].mbox) {
			vcp_mbox_pin_recv[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_recv[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_recv[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_recv[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_recv[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_recv[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
	   (last_ofs + last_slot) > (vcp_mbox_info[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

static int vcp_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct resource *res;
	const char *vcp_hwvoter = NULL;
	struct device *dev = &pdev->dev;
	struct device *smmu_dev = NULL;
	struct device_node *node;
	struct device_link	*link;
	struct device_node	*smi_node;
	struct platform_device	*psmi_com_dev;
	unsigned int temp_value;

	pr_debug("[VCP] %s", __func__);

	pm_runtime_enable(&pdev->dev);
	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0) {
		pr_info("Bypass VCP driver probe\n");
		return 0;
	}
	// VCP iommu devices
	if (smmu_v3_enabled()) {
		dev_info(dev, "[VCP] smmu enable\n");
		smmu_dev = mtk_smmu_get_shared_device(dev);
		if (smmu_dev)
			vcp_io_devs[vcp_support-1] = smmu_dev;
		else
			dev_info(dev, "[VCP] cannot get smmu shared dev\n");
	} else
		vcp_io_devs[vcp_support-1] = dev;

	if (vcp_support > 1)
		return 0;

	vcp_power_devs = dev;

	vcp_ao = of_property_read_bool(pdev->dev.of_node, "vcp-ao-feature");
	pr_info("[VCP] vcp-ao %s", vcp_ao ? "support":"non-support");
	if (!vcp_ao) {
		pm_runtime_irq_safe(vcp_power_devs);
		ret = pm_runtime_get_sync(vcp_power_devs);
		if (ret)
			pr_notice("[VCP] %s: pm_runtime_get_sync %d\n", __func__, ret);
	}

	is_vcp_shutdown = false;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_sram_base");
	if (res == NULL) {
		pr_notice("[VCP] platform resource was queryed fail by name.\n");
		return -1;
	}
	vcpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.sram)) {
		pr_notice("[VCP] vcpreg.sram error\n");
		return -1;
	}
	if (res == NULL) {
		pr_notice("[VCP] platform_get_resource_byname error\n");
		return -1;
	}
	vcpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[VCP] sram base = 0x%p %x\n"
		, vcpreg.sram, vcpreg.total_tcmsize);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg");
	vcpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg)) {
		pr_notice("[VCP] vcpreg.cfg error\n");
		return -1;
	}
	pr_debug("[VCP] cfg base = 0x%p\n", vcpreg.cfg);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_core0");
	vcpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core0)) {
		pr_debug("[VCP] vcpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core0 base = 0x%p\n", vcpreg.cfg_core0);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_intc_core0");
	vcpreg.cfg_intc_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_intc_core0)) {
		pr_debug("[VCP] vcpreg.cfg_intc_core0 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_intc_core0 base = 0x%p\n", vcpreg.cfg_intc_core0);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_intc_core1");
	vcpreg.cfg_intc_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_intc_core1)) {
		pr_debug("[VCP] vcpreg.cfg_intc_core1 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_intc_core1 base = 0x%p\n", vcpreg.cfg_intc_core1);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_pwr_ctl");
	vcpreg.cfg_pwr = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_pwr))
		pr_debug("[VCP] vcpreg.cfg_pwr not support\n");
	pr_debug("[VCP] per_ctl base = 0x%p\n", vcpreg.cfg_pwr);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_core1");
	vcpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core1)) {
		pr_debug("[VCP] vcpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core1 base = 0x%p\n", vcpreg.cfg_core1);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_bus_debug");
	vcpreg.bus_debug = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.bus_debug)) {
		pr_debug("[VCP] vcpreg.bus_debug error\n");
		return -1;
	}
	pr_debug("[VCP] bus_debug base = 0x%p\n", vcpreg.bus_debug);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_bus_tracker");
	vcpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.bus_tracker)) {
		pr_debug("[VCP] vcpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[VCP] bus_tracker base = 0x%p\n", vcpreg.bus_tracker);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_bus_prot");
	vcpreg.bus_prot = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.bus_prot)) {
		pr_notice("[VCP] vcpreg.bus_prot error\n");
		vcpreg.bus_prot = NULL;
	}
	pr_debug("[VCP] bus_prot base = 0x%p\n", vcpreg.bus_prot);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_ap");
	vcpreg.cfgreg_ap = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfgreg_ap)) {
		pr_debug("[VCP] vcpreg.cfgreg_ap error\n");
		return -1;
	}
	pr_debug("[VCP] cfgreg_ap base = 0x%p\n", vcpreg.cfgreg_ap);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_sec_gpr");
	vcpreg.cfg_sec_gpr = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_sec_gpr))
		pr_notice("[VCP] vcpreg.cfg_sec_gpr not support\n");
	pr_debug("[VCP] cfg_sec_gpr base = 0x%p\n", vcpreg.cfg_sec_gpr);

	of_property_read_u32(pdev->dev.of_node, "vcp-sram-size"
						, &vcpreg.vcp_tcmsize);
	if (!vcpreg.vcp_tcmsize) {
		pr_notice("[VCP] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[VCP] vcpreg.vcp_tcmsize = %d\n", vcpreg.vcp_tcmsize);

	of_property_read_string(pdev->dev.of_node, "vcp-hwvoter", &vcp_hwvoter);
	if (vcp_hwvoter) {
		if (strcmp(vcp_hwvoter, "enable") != 0) {
			pr_notice("[VCP] vcp_hwvoter not enable\n");
			vcp_hwvoter_support = false;
		} else {
			pr_notice("[VCP] vcp_hwvoter enable\n");
			vcp_hwvoter_support = true;
		}
	} else {
		vcp_hwvoter_support = true;
		pr_notice("[VCP] vcp_hwvoter support by default: %d\n", vcp_hwvoter_support);
	}

	of_property_read_u32(pdev->dev.of_node, "core-nums"
						, &vcpreg.core_nums);
	if (!vcpreg.core_nums) {
		pr_notice("[VCP] core number not found\n");
		return -ENODEV;
	}
	pr_notice("[VCP] vcpreg.core_nums = %d\n", vcpreg.core_nums);

#ifdef VCP_CLK_FMETER
	of_property_read_u32(pdev->dev.of_node, "fmeter-ck"
						, &vcpreg.fmeter_ck);
	of_property_read_u32(pdev->dev.of_node, "fmeter-type"
						, &vcpreg.fmeter_type);
	pr_notice("[VCP] vcpreg.fmeter_ck = %d vcpreg.fmeter_type = %d\n",
		vcpreg.fmeter_ck, vcpreg.fmeter_type);
#endif
	of_property_read_u32(pdev->dev.of_node, "twohart"
						, &vcpreg.twohart);
	pr_notice("[VCP] vcpreg.twohart = %d\n", vcpreg.twohart);

	vcp_excep_mode = VCP_NO_EXCEP;
	vcpreg.secure_dump = 0;
	of_property_read_u32(pdev->dev.of_node, "vcp-secure-dump"
						, &vcpreg.secure_dump);
	of_property_read_u32(pdev->dev.of_node, "vcp-excep-mode"
						, &vcp_excep_mode);

	if (vcp_excep_mode == VCP_EE_ENABLE) {
		pr_notice("[VCP] not support %u\n", vcp_excep_mode);
		vcp_excep_mode = VCP_KE_ENABLE;
	}
	pr_notice("[VCP] vcpreg.secure_dump = %d, vcp_excep_mode = %d\n",
			vcpreg.secure_dump, vcp_excep_mode);

	vcpreg.bus_debug_num_ports = 0;
	of_property_read_u32(pdev->dev.of_node, "bus-debug-num-ports"
						, &vcpreg.bus_debug_num_ports);
	if (!vcpreg.bus_debug_num_ports)
		pr_notice("[VCP] bus debug num ports not found\n");
	pr_debug("[VCP] vcpreg.bus_debug_num_ports = %d\n", vcpreg.bus_debug_num_ports);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_vlp_ao_rsvd7");
	vcpreg.vcp_vlp_ao_rsvd7 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.vcp_vlp_ao_rsvd7))
		pr_debug("[VCP] vcpreg.vcp_vlp_ao_rsvd7 error\n");
	else
		pr_notice("[VCP] VLP_AO_RSVD7 value = 0x%x\n", readl(VLP_AO_RSVD7));

	temp_value = 0;
	of_property_read_u32(pdev->dev.of_node, "vcp-clk-sys", &temp_value);
	if (!temp_value)
		pr_notice("[VCP] clk sys register not found\n");
	else
		vcpreg.vcp_clk_sys = ioremap(temp_value, 0x1000);
	if (IS_ERR((void const *) vcpreg.vcp_clk_sys))
		pr_notice("[VCP] vcpreg.vcp_clk_sys error\n");

	temp_value = 0;
	of_property_read_u32(pdev->dev.of_node, "vcp-pwr-status", &temp_value);
	if (!temp_value)
		pr_notice("[VCP] pwr status register not found\n");
	else
		vcpreg.vcp_pwr_ack = ioremap(temp_value, 4);
	if (IS_ERR((void const *) vcpreg.vcp_pwr_ack))
		pr_debug("[VCP] vcpreg.vcp_pwr_ack error\n");
	else
		pr_debug("[VCP] VCP_PWR_ACK value = %x\n", readl(VCP_PWR_ACK));

	vcpreg.irq0 = platform_get_irq_byname(pdev, "wdt");
	if (vcpreg.irq0 < 0)
		pr_notice("[VCP] wdt irq not exist\n");
	else {
		pr_debug("wdt %d\n", vcpreg.irq0);
		ret = request_irq(vcpreg.irq0, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP wdt", NULL);
		if (ret < 0)
			pr_notice("[VCP] wdt require fail %d %d\n",
				vcpreg.irq0, ret);
		else {
			vcp_A_irq0_tasklet.data = vcpreg.irq0;
			ret = enable_irq_wake(vcpreg.irq0);
			if (ret < 0)
				pr_notice("[VCP] wdt wake fail:%d,%d\n",
					vcpreg.irq0, ret);
		}
	}

	vcpreg.irq1 = platform_get_irq_byname(pdev, "reserved");
	if (vcpreg.irq1 < 0)
		pr_notice("[VCP] reserved irq not exist\n");
	else {
		pr_debug("reserved %d\n", vcpreg.irq1);
		ret = request_irq(vcpreg.irq1, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP reserved", NULL);
		if (ret < 0)
			pr_notice("[VCP] reserved require irq fail %d %d\n",
				vcpreg.irq1, ret);
		else {
			vcp_A_irq1_tasklet.data = vcpreg.irq1;
			ret = enable_irq_wake(vcpreg.irq1);
			if (ret < 0)
				pr_notice("[VCP] reserved wake fail:%d,%d\n",
					vcpreg.irq1, ret);
		}
	}

	/* probe mbox info from dts */
	if (!vcp_ipi_table_init(&vcp_mboxdev, pdev))
		return -ENODEV;
	/* create mbox dev */
	pr_debug("[VCP] mbox probe\n");
	for (i = 0; i < vcp_mboxdev.count; i++) {
		vcp_mbox_info[i].mbdev = &vcp_mboxdev;
		ret = mtk_mbox_probe(pdev, vcp_mbox_info[i].mbdev, i);
		if (ret < 0 || vcp_mboxdev.info_table[i].irq_num < 0) {
			pr_notice("[VCP] mbox%d probe fail %d\n", i, ret);
			continue;
		}

		ret = enable_irq_wake(vcp_mboxdev.info_table[i].irq_num);
		if (ret < 0) {
			pr_notice("[VCP]mbox%d enable irq fail %d\n", i, ret);
			continue;
		}
		mbox_setup_pin_table(i);
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (vcp_ipi_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
					      vcp_ipi_irqs[i].name);
		if (!node) {
			pr_info("[VCP] find '%s' node failed\n",
				vcp_ipi_irqs[i].name);
			continue;
		}
		vcp_ipi_irqs[i].irq_no =
			irq_of_parse_and_map(node, vcp_ipi_irqs[i].order);
		if (!vcp_ipi_irqs[i].irq_no)
			pr_info("[VCP] get '%s' fail\n", vcp_ipi_irqs[i].name);
	}

	ret = mtk_ipi_device_register(&vcp_ipidev, pdev, &vcp_mboxdev,
				      VCP_IPI_COUNT);
	if (ret)
		pr_notice("[VCP] ipi_dev_register fail, ret %d\n", ret);

#if VCP_RESERVED_MEM && defined(CONFIG_OF)
	/*vcp resvered memory*/
	pr_notice("[VCP] vcp_reserve_memory_ioremap 1\n");
	ret = vcp_reserve_memory_ioremap(pdev, vcp_io_devs[vcp_support-1]);
	if (ret) {
		pr_notice("[VCP]vcp_reserve_memory_ioremap failed ret = %d\n", ret);
		return ret;
	}
#endif
	/* device link to SMI for Dram iommu */
	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	psmi_com_dev = of_find_device_by_node(smi_node);
	if (psmi_com_dev) {
		link = device_link_add(dev, &psmi_com_dev->dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		pr_info("[VCP] device link to %s\n", dev_name(&psmi_com_dev->dev));
		if (!link) {
			dev_info(dev, "Unable to link Dram %s.\n",
				dev_name(&psmi_com_dev->dev));
			ret = -EINVAL;
			return ret;
		}
	}

	/* device link to SMI for SLB/Infra */
	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 1);
	psmi_com_dev = of_find_device_by_node(smi_node);
	if (psmi_com_dev) {
		link = device_link_add(dev, &psmi_com_dev->dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		pr_info("[VCP] device link to %s\n", dev_name(&psmi_com_dev->dev));
		if (!link) {
			dev_info(dev, "Unable to link SLB/Infra %s.\n",
				dev_name(&psmi_com_dev->dev));
			ret = -EINVAL;
			return ret;
		}
	}

	ret = vcp_dump_size_probe(pdev);
	if (ret) {
		vcpreg.secure_dump = 0;
		pr_info("[VCP] Unable to get memory dump size.\n");
	}

	pr_info("[VCP] %s done\n", __func__);

	return ret;
}
void dump_vcp_irq_status(void)
{
	int i;

	pr_info("[VCP] %s Dump wdt irq %d status\n", __func__, vcpreg.irq0);
	mt_irq_dump_status(vcpreg.irq0);

	pr_info("[VCP] %s Dump reserve irq %d status\n", __func__, vcpreg.irq1);
	mt_irq_dump_status(vcpreg.irq1);

	for (i = 0; i < vcp_mboxdev.count; i++) {
		pr_info("[VCP] %s Dump mbox%d irq %d status\n", __func__,
			i, vcp_mboxdev.info_table[i].irq_num);
		mt_irq_dump_status(vcp_mboxdev.info_table[i].irq_num);
	}

	if (vcpreg.core_nums == 2)
		dump_mmup_irq_status();

}
EXPORT_SYMBOL_GPL(dump_vcp_irq_status);

static int vcp_device_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	kfree(vcp_mbox_info);
	vcp_mbox_info = NULL;
	kfree(vcp_mbox_pin_recv);
	vcp_mbox_pin_recv = NULL;
	kfree(vcp_mbox_pin_send);
	vcp_mbox_pin_send = NULL;

	return 0;
}

static void vcp_device_shutdown(struct platform_device *pdev)
{
	int i;

	if (!vcp_support) {
		pr_notice("[VCP] %s bypass\n", __func__);
		return;
	}

	mutex_lock(&vcp_pw_clk_mutex);
	is_vcp_shutdown = true;
	if (is_suspending) {
		pr_notice("[VCP] vcp suspending %s bypass\n", __func__);
		mutex_unlock(&vcp_pw_clk_mutex);
		return;
	}
	mutex_unlock(&vcp_pw_clk_mutex);

	mutex_lock(&vcp_ready_mutex);
	for (i = 0; i < VCP_CORE_TOTAL ; i++)
		vcp_ready[i] = 0;
	mutex_unlock(&vcp_ready_mutex);

	mutex_lock(&vcp_A_notify_mutex);
	vcp_extern_notify(VCP_ID, VCP_EVENT_STOP);
	vcp_extern_notify(MMUP_ID, VCP_EVENT_STOP);
	mutex_unlock(&vcp_A_notify_mutex);

	// trigger halt isr to change spm control power
	writel(GIPC_VCP_HART0_SHUT, R_GIPC_IN_SET);
	if (vcpreg.core_nums == 2) {
		wait_vcp_ready_to_reboot(VCP_ID);
		writel(GIPC_MMUP_SHUT, R_GIPC_IN_SET);
	}
	pr_notice("[VCP] %s done\n", __func__);
}

static int mtk_vcp_suspend(struct device *pdev)
{
	pr_notice("[VCP] %s done\n", __func__);
	return 0;
}

static int mtk_vcp_resume(struct device *pdev)
{
	pr_notice("[VCP] %s done\n", __func__);
	return 0;
}

static const struct dev_pm_ops mtk_vcp_pm_ops = {
	.suspend = mtk_vcp_suspend,
	.resume = mtk_vcp_resume,
};

static const struct of_device_id vcp_of_ids[] = {
	{ .compatible = "mediatek,vcp", },
	{}
};

static struct platform_driver mtk_vcp_device = {
	.probe = vcp_device_probe,
	.remove = vcp_device_remove,
	.shutdown = vcp_device_shutdown,
	.driver = {
		.name = "vcp",
		.owner = THIS_MODULE,
		.pm = &mtk_vcp_pm_ops,
		.of_match_table = vcp_of_ids,
	},
};

static const struct of_device_id vcp_vdec_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-vdec", },
	{}
};
static const struct of_device_id vcp_venc_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-venc", },
	{}
};
static const struct of_device_id vcp_work_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-work", },
	{}
};
static const struct of_device_id vcp_ube_lat_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-ube-lat", },
	{}
};
static const struct of_device_id vcp_ube_core_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-ube-core", },
	{}
};
static const struct of_device_id vcp_sec_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-sec", },
	{}
};
static const struct of_device_id vcp_acp_vdec_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-acp-vdec", },
	{}
};
static const struct of_device_id vcp_acp_venc_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-acp-venc", },
	{}
};
static const struct of_device_id vcp_acp_codec_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-acp-codec", },
	{}
};

#if IS_ENABLED(CONFIG_MTK_SENTRY_MODE)
static const struct of_device_id vcp_sentry_mode_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-sentrymode", },
	{}
};
static const struct of_device_id vcp_sentry_mode_extra_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-sentrymode_extra", },
	{}
};
#endif

static struct platform_driver mtk_vcp_io_vdec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_vdec",
		.owner = THIS_MODULE,
		.of_match_table = vcp_vdec_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_venc = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_venc",
		.owner = THIS_MODULE,
		.of_match_table = vcp_venc_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_work = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_work",
		.owner = THIS_MODULE,
		.of_match_table = vcp_work_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_ube_lat = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_ube_lat",
		.owner = THIS_MODULE,
		.of_match_table = vcp_ube_lat_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_ube_core = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_ube_core",
		.owner = THIS_MODULE,
		.of_match_table = vcp_ube_core_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_sec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_sec",
		.owner = THIS_MODULE,
		.of_match_table = vcp_sec_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_acp_vdec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_acp_vdec",
		.owner = THIS_MODULE,
		.of_match_table = vcp_acp_vdec_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_acp_venc = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_acp_venc",
		.owner = THIS_MODULE,
		.of_match_table = vcp_acp_venc_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_acp_codec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_acp_codec",
		.owner = THIS_MODULE,
		.of_match_table = vcp_acp_codec_of_ids,
	},
};

#if IS_ENABLED(CONFIG_MTK_SENTRY_MODE)
static struct platform_driver mtk_vcp_io_sentry_mode = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_sentry_mode",
		.owner = THIS_MODULE,
		.of_match_table = vcp_sentry_mode_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_sentry_mode_extra = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_sentry_mode_extra",
		.owner = THIS_MODULE,
		.of_match_table = vcp_sentry_mode_extra_of_ids,
	},
};
#endif

/*
 * driver initialization entry point
 */
static int __init vcp_init(void)
{
	int ret = 0;
	int i = 0;
	struct arm_smccc_res res;

	/* vcp platform initialise */
	pr_info("[VCP] %s v2 begins\n", __func__);

#if VCP_BOOT_TIME_OUT_MONITOR
	vcp_timeout_times = 0;
#endif
	/* vcp ready static flag initialise */
	for (i = 0; i < VCP_CORE_TOTAL ; i++) {
#if VCP_BOOT_TIME_OUT_MONITOR
		vcp_ready_timer[i].core_id = i;
		timer_setup(&(vcp_ready_timer[i].tl), vcp_wait_ready_timeout, 0);
#endif
		vcp_ready[i] = 0;
	}
	vcp_support = 1;
	vcp_dbg_log = 0;

	/* vco io device initialise */
	for (i = 0; i < VCP_IOMMU_DEV_NUM; i++)
		vcp_io_devs[i] = NULL;
	vcp_power_devs = NULL;

	if (platform_driver_register(&mtk_vcp_device)) {
		pr_info("[VCP] vcp probe fail\n");
		goto err_vcp;
	}

	if (platform_driver_register(&mtk_vcp_io_ube_lat)) {
		pr_info("[VCP] mtk_vcp_io_ube_lat  not exist\n");
		goto err_io_ube_lat;
	}
	if (platform_driver_register(&mtk_vcp_io_ube_core)) {
		pr_info("[VCP] mtk_vcp_io_ube_core not exist\n");
		goto err_io_ube_core;
	}
	if (platform_driver_register(&mtk_vcp_io_vdec)) {
		pr_info("[VCP] mtk_vcp_io_vdec probe fail\n");
		goto err_io_vdec;
	}
	if (platform_driver_register(&mtk_vcp_io_venc)) {
		pr_info("[VCP] mtk_vcp_io_venc probe fail\n");
		goto err_io_venc;
	}
	if (platform_driver_register(&mtk_vcp_io_work)) {
		pr_info("[VCP] mtk_vcp_io_work probe fail\n");
		goto err_io_work;
	}
	if (platform_driver_register(&mtk_vcp_io_sec)) {
		pr_info("[VCP] mtk_vcp_io_sec probe fail\n");
		goto err_io_sec;
	}
	if (platform_driver_register(&mtk_vcp_io_acp_vdec)) {
		pr_info("[VCP] mtk_vcp_io_acp_vdec probe fail\n");
		goto err_io_acp_vdec;
	}
	if (platform_driver_register(&mtk_vcp_io_acp_venc)) {
		pr_info("[VCP] mtk_vcp_io_acp_venc probe fail\n");
		goto err_io_acp_venc;
	}
	if (platform_driver_register(&mtk_vcp_io_acp_codec)) {
		pr_info("[VCP] mtk_vcp_io_acp_codec probe fail\n");
		goto err_io_acp_codec;
	}
#if IS_ENABLED(CONFIG_MTK_SENTRY_MODE)
	if (platform_driver_register(&mtk_vcp_io_sentry_mode)) {
		pr_info("[VCP] mtk_vcp_io_sentry_mode probe fail\n");
		goto err_io_sentry_mode;
	}
	if (platform_driver_register(&mtk_vcp_io_sentry_mode_extra)) {
		pr_info("[VCP] mtk_vcp_io_sentry_mode_eatra probe fail\n");
		goto err_io_sentry_mode_extra;
	}
#endif
	if (!vcp_support)
		return 0;

	if (vcpreg.core_nums == 2) {
		if (mmup_init() < 0) {
			pr_notice("[VCP] mmup init fail\n");
			goto err;
		}
	}

	/* vcp platform initialise */
	pr_debug("[VCP] platform init\n");
	vcp_awake_init();
	vcp_workqueue = create_singlethread_workqueue("VCP_WQ");
	if (!vcp_workqueue)
		pr_notice("[VCP] vcp_workqueue create fail\n");

	ret = vcp_excep_init();
	if (ret) {
		pr_notice("[VCP]Excep Init Fail\n");
		goto err;
	}

	for (i = 0; i < VCP_CORE_TOTAL; i++)
		INIT_WORK(&vcp_A_notify_work[i].work, vcp_A_notify_ws);

	mtk_ipi_register(&vcp_ipidev, IPI_OUT_C_SLEEP_0,
		NULL, NULL, &slp_ipi_ack_data);

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_READY_0,
			(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready0);

	ret = register_pm_notifier(&vcp_pm_notifier_block);
	if (ret)
		pr_notice("[VCP] failed to register PM notifier %d\n", ret);

	/* vcp sysfs initialise */
	pr_debug("[VCP] sysfs init\n");
	ret = create_files();
	if (unlikely(ret != 0)) {
		pr_notice("[VCP] create files failed\n");
		goto err;
	}

	/* scp hwvoter debug init */
	if (vcp_hwvoter_support)
		vcp_hw_voter_dbg_init();

#if VCP_LOGGER_ENABLE
	/* vcp logger initialise */
	pr_debug("[VCP] logger init\n");
	/*create wq for vcp logger*/
	vcp_logger_workqueue = create_singlethread_workqueue("VCP_LOG_WQ");
	if (!vcp_logger_workqueue)
		pr_notice("[VCP] vcp_logger_workqueue create fail\n");

	if (vcp_logger_init(vcp_get_reserve_mem_virt(VCP_A_LOGGER_MEM_ID),
			vcp_get_reserve_mem_size(VCP_A_LOGGER_MEM_ID)) == -1) {
		pr_notice("[VCP] vcp_logger_init_fail\n");
		goto err;
	}
#endif

	vcp_recovery_init();
	vcp_disable_irqs();
	driver_init_done = true;
	is_suspending = false;
	pwclkcnt = 0;

	vcp_set_fp(&vcp_helper_fp);

#if IS_ENABLED(CONFIG_DEVAPC_ARCH_MULTI)
	register_devapc_power_callback(&devapc_power_handle);
#endif

	pr_notice("[VCP] %s core0 status: 0x%x, core1 status: 0x%x\n",
		__func__, readl(R_CORE0_STATUS), readl(R_CORE1_STATUS));
	/* core 0 */
	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
		MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
		1, 0, 0, 0, 0, 0, &res);

	/* core 1 */
	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
		MTK_TINYSYS_MMUP_KERNEL_OP_RESET_SET,
		1, 0, 0, 0, 0, 0, &res);

	if (vcp_register_feature(RTOS_FEATURE_ID) < 0)
		pr_notice("[VCP] bootup fail\n");

	return ret;
err:
	platform_driver_unregister(&mtk_vcp_io_acp_codec);
#if IS_ENABLED(CONFIG_MTK_SENTRY_MODE)
err_io_sentry_mode_extra:
	platform_driver_unregister(&mtk_vcp_io_sentry_mode_extra);
err_io_sentry_mode:
	platform_driver_unregister(&mtk_vcp_io_sentry_mode);
#endif
err_io_acp_codec:
	platform_driver_unregister(&mtk_vcp_io_acp_venc);
err_io_acp_venc:
	platform_driver_unregister(&mtk_vcp_io_acp_vdec);
err_io_acp_vdec:
	platform_driver_unregister(&mtk_vcp_io_sec);
err_io_sec:
	platform_driver_unregister(&mtk_vcp_io_work);
err_io_work:
	platform_driver_unregister(&mtk_vcp_io_venc);
err_io_venc:
	platform_driver_unregister(&mtk_vcp_io_vdec);
err_io_vdec:
	platform_driver_unregister(&mtk_vcp_io_ube_core);
err_io_ube_core:
	platform_driver_unregister(&mtk_vcp_io_ube_lat);
err_io_ube_lat:
	platform_driver_unregister(&mtk_vcp_device);
err_vcp:
	return -1;
}

/*
 * driver exit point
 */
static void __exit vcp_exit(void)
{
#if VCP_BOOT_TIME_OUT_MONITOR
	int i = 0;
#endif

#if VCP_LOGGER_ENABLE
	vcp_logger_uninit();
#endif

	free_irq(vcpreg.irq0, NULL);
	free_irq(vcpreg.irq1, NULL);
	misc_deregister(&vcp_device);

	flush_workqueue(vcp_workqueue);
	destroy_workqueue(vcp_workqueue);

#if VCP_RECOVERY_SUPPORT
	flush_workqueue(vcp_reset_workqueue);
	destroy_workqueue(vcp_reset_workqueue);
#endif

#if VCP_LOGGER_ENABLE
	flush_workqueue(vcp_logger_workqueue);
	destroy_workqueue(vcp_logger_workqueue);
#endif

#if VCP_BOOT_TIME_OUT_MONITOR
	for (i = 0; i < VCP_CORE_TOTAL ; i++)
		del_timer(&vcp_ready_timer[i].tl);
#endif
	platform_driver_unregister(&mtk_vcp_io_acp_codec);
	platform_driver_unregister(&mtk_vcp_io_acp_venc);
	platform_driver_unregister(&mtk_vcp_io_acp_vdec);
	platform_driver_unregister(&mtk_vcp_io_sec);
	platform_driver_unregister(&mtk_vcp_io_work);
	platform_driver_unregister(&mtk_vcp_io_venc);
	platform_driver_unregister(&mtk_vcp_io_vdec);
	platform_driver_unregister(&mtk_vcp_io_ube_core);
	platform_driver_unregister(&mtk_vcp_io_ube_lat);
	platform_driver_unregister(&mtk_vcp_device);

	if (vcpreg.core_nums == 2)
		mmup_exit();
}

device_initcall_sync(vcp_init);
module_exit(vcp_exit);

MODULE_DESCRIPTION("MEDIATEK Module VCP driver");
MODULE_AUTHOR("Mediatek");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
