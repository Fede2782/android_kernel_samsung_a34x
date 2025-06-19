// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: debug " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/iova.h>
#include <linux/io-pgtable-arm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_STACKTRACE)
#include <linux/stacktrace.h>
#endif
#include <linux/export.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include <trace/hooks/iommu.h>

#include "mtk_iommu.h"
#include "iommu_debug.h"
#include "iommu_port.h"
#include "smmu_reg.h"

#include "../../../iommu/arm/arm-smmu-v3/arm-smmu-v3.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#include <mt-plat/aee.h>
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(12, 2)
#define F_MMU_INT_TF_CCU_MSK		GENMASK(12, 7)
#define F_MMU_INT_TF_LARB(id)		FIELD_GET(GENMASK(13, 7), id)
#define F_MMU_INT_TF_PORT(id)		FIELD_GET(GENMASK(6, 2), id)
#define F_APU_MMU_INT_TF_MSK(id)	FIELD_GET(GENMASK(11, 7), id)

#define F_MMU_INT_TF_SPEC_MSK(port_s_b)		GENMASK(12, port_s_b)
#define F_MMU_INT_TF_SPEC_LARB(id, larb_s_b) \
	FIELD_GET(GENMASK(12, larb_s_b), id)
#define F_MMU_INT_TF_SPEC_PORT(id, larb_s_b, port_s_b) \
	FIELD_GET(GENMASK((larb_s_b-1), port_s_b), id)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)

#else
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)
#endif

#define MAU_CONFIG_INIT(iommu_type, iommu_id, slave, mau, start, end,\
	port_mask, larb_mask, wr, virt, io, start_bit32, end_bit32) {\
	iommu_type, iommu_id, slave, mau, start, end, port_mask, larb_mask,\
	wr, virt, io, start_bit32, end_bit32\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:%s_%s\ntranslation fault:port=%s,mva=0x%llx,pa=0x%llx\n"

#define mau_assert_log_format \
	"CRDISPATCH_KEY:IOMMU\nMAU ASRT:ASRT_ID=0x%x,FALUT_ID=0x%x(%s),ADDR=0x%x(0x%x)\n"

#define iova_warn_log_format \
	"CRDISPATCH_KEY:%s\nIOVA_WARN Dev=%s,tab_id=0x%llx,dom_id=%d,count=%llu\n"

#define FIND_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */
#define MAP_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */

#define IOVA_DUMP_LOG_MAX		(100)
#define IOVA_LATEST_DUMP_MAX		(100)
#define IOVA_LATEST_TRACE_MAX		(250)

#define IOVA_DUMP_RS_INTERVAL		(30 * HZ)
#define IOVA_DUMP_RS_BURST		(1)

#define IOVA_WARN_RS_INTERVAL		(60 * HZ)
#define IOVA_WARN_RS_BURST		(1)
#define IOVA_WARN_COUNT			(10000)
#define IOVA_ENABLE_RBTREE_COUNT	(2000)

#if IS_ENABLED(CONFIG_STACKTRACE)
#define SMMU_STACK_SKIPNR		(7)
#define SMMU_STACK_DEPTH		(10)
#define SMMU_STACK_LINE_MAX_LEN		(100)
#define SMMU_STACK_MAX_LEN		(400)
#endif

#define IOMMU_DEFAULT_IOVA_MAX_ALIGN_SHIFT	9
static unsigned long iommu_max_align_shift __read_mostly = IOMMU_DEFAULT_IOVA_MAX_ALIGN_SHIFT;

struct mtk_iommu_cb {
	int port;
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct proc_dir_entry	*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

/* max value of TYPE_NUM and SMMU_TYPE_NUM */
#define MAX_IOMMU_NUM	4
struct mtk_m4u_plat_data {
	struct peri_iommu_data		*peri_data;
	const struct mtk_iommu_port	*port_list[MAX_IOMMU_NUM];
	u32				port_nr[MAX_IOMMU_NUM];
	const struct mau_config_info	*mau_config;
	u32				mau_config_nr;
	u32				mm_tf_ccu_support;
	u32 (*get_valid_tf_id)(int tf_id, u32 type, int id);
	bool (*tf_id_is_match)(int tf_id, u32 type, int id,
			       struct mtk_iommu_port port);
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	char *(*peri_tf_analyse)(enum peri_iommu bus_id, u32 id);
	int (*smmu_common_id)(u32 type, u32 tbu_id);
	char *(*smmu_port_name)(u32 type, int id, int tf_id);
};

struct peri_iommu_data {
	enum peri_iommu id;
	u32 bus_id;
};

static struct mtk_m4u_data *m4u_data;
static bool smmu_v3_enable;

/**********iommu trace**********/
#define IOMMU_EVENT_COUNT_MAX		(8000)

#define IOMMU_DUMP_TAG_SKIP		((void *)0x1)
#define IOMMU_DUMP_TAG_MRDUMP		((void *)0x2)

mtk_iommu_dump_callback_t iommu_mrdump_proc;

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#define MAX_STRING_SIZE			(256)
#define MAX_IOMMU_MRDUMP_SIZE		(512*1024)
static char *iommu_mrdump_buffer;
static int iommu_mrdump_size;
#endif

#define iommu_dump(file, fmt, args...)				\
	do {							\
		if (file == IOMMU_DUMP_TAG_MRDUMP) {		\
			if (iommu_mrdump_proc != NULL)		\
				iommu_mrdump_proc(fmt, ##args);	\
		} else if (file == IOMMU_DUMP_TAG_SKIP) {	\
			/* skip dump */				\
		} else if (file) {				\
			seq_printf(file, fmt, ##args);		\
		} else {					\
			pr_info(fmt, ##args);			\
		}						\
	} while (0)

struct iommu_event_mgr_t {
	char name[11];
	unsigned int dump_trace;
	unsigned int dump_log;
};

static struct iommu_event_mgr_t event_mgr[IOMMU_EVENT_MAX];

struct iommu_event_t {
	unsigned int event_id;
	u64 time_high;
	u32 time_low;
	unsigned long data1;
	unsigned long data2;
	unsigned long data3;
	struct device *dev;
};

struct iommu_global_t {
	unsigned int enable;
	unsigned int dump_enable;
	unsigned int iova_evt_enable;
	unsigned int iova_alloc_list;
	unsigned int iova_alloc_rbtree;
	unsigned int iova_map_list;
	unsigned int iova_warn_aee;
	unsigned int iova_stack_trace;
	unsigned int start;
	unsigned int write_pointer;
	spinlock_t	lock;
	struct iommu_event_t *record;
};

static struct iommu_global_t iommu_globals;

/* iova statistics info for size and count */
#define IOVA_DUMP_TOP_MAX	(10)

struct iova_count_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	u64 size;
	u32 count;
	struct list_head list_node;
};

struct iova_count_list {
	spinlock_t		lock;
	struct list_head	head;
};

static struct iova_count_list count_list = {};

enum mtk_iova_space {
	MTK_IOVA_SPACE0, /* 0GB ~ 4GB */
	MTK_IOVA_SPACE1, /* 4GB ~ 8GB */
	MTK_IOVA_SPACE2, /* 8GB ~ 12GB */
	MTK_IOVA_SPACE3, /* 12GB ~ 16GB */
	MTK_IOVA_SPACE_NUM
};

/* iova alloc info */
struct iova_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	struct iova_domain *iovad;
	dma_addr_t iova;
	size_t size;
	u64 time_high;
	u32 time_low;
	char *trace_info;
	bool in_rbtree;
	struct list_head list_node;
	struct rb_node iova_rb_node;
};

struct iova_buf_list {
	atomic_t init_flag;
	struct list_head head;
	struct rb_root iova_rb_root;
	u64 count;
	u64 list_only_count;
	spinlock_t lock;
};

static struct iova_buf_list iova_list = {
	.init_flag = ATOMIC_INIT(0),
	.iova_rb_root = RB_ROOT,
	.count = 0,
	.list_only_count = 0,
};

/* iova map info */
struct iova_map_info {
	u64			tab_id;
	u64			iova;
	u64			time_high;
	u32			time_low;
	size_t			size;
	struct list_head	list_node;
};

struct iova_map_list {
	atomic_t		init_flag;
	spinlock_t		lock;
	struct list_head	head[MTK_IOVA_SPACE_NUM];
};

static struct iova_map_list map_list = {.init_flag = ATOMIC_INIT(0)};

static void mtk_iommu_iova_trace(int event, dma_addr_t iova, size_t size,
				 u64 tab_id, struct device *dev);
static void mtk_iommu_iova_alloc_dump_top(struct seq_file *s,
					  struct device *dev);
static int mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev);
static int mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id);
static int mtk_iommu_iova_dump(struct seq_file *s, u64 iova, u64 tab_id);

static inline void mtk_iova_count_inc(void)
{
	if (iova_list.count < ULLONG_MAX)
		iova_list.count += 1;
	else
		pr_info_ratelimited("%s, iova count overflow\n", __func__);
}

static inline void mtk_iova_count_dec(void)
{
	if (iova_list.count > 0)
		iova_list.count -= 1;
	else
		pr_info_ratelimited("%s, iova count underflow\n", __func__);
}

static void mtk_iova_count_check(struct device *dev, dma_addr_t iova, size_t size)
{
	static DEFINE_RATELIMIT_STATE(warn_dump_rs, IOVA_WARN_RS_INTERVAL,
				      IOVA_WARN_RS_BURST);
	struct iommu_fwspec *fwspec;
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (!dev)
		return;

	if (iova_list.count < IOVA_WARN_COUNT)
		return;

	if (!__ratelimit(&warn_dump_rs))
		return;

	fwspec = dev_iommu_fwspec_get(dev);
	if (!fwspec)
		return;

	if (smmu_v3_enable) {
		tab_id = get_smmu_tab_id(dev);
		dom_id = 0;
	} else {
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	pr_info("%s, dev:%s, iova:[0x%llx %d 0x%llx 0x%zx] count:%llu\n",
		__func__, (dev ? dev_name(dev) : "NULL"),
		tab_id, dom_id, (unsigned long long)iova, size, iova_list.count);

	if (iommu_globals.iova_warn_aee == 1) {
		mtk_iommu_iova_alloc_dump_top(NULL, NULL);
		m4u_aee_print(iova_warn_log_format,
			      dev_name(dev), dev_name(dev),
			      tab_id, dom_id, iova_list.count);
	}
}

static void mtk_iommu_system_time(u64 *high, u32 *low)
{
	u64 temp;

	temp = sched_clock();
	do_div(temp, 1000);
	*low = do_div(temp, 1000000);
	*high = temp;
}

void mtk_iova_map(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *iova_buf;

	if (iommu_globals.iova_evt_enable == 0)
		return;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.iova_map_list == 0)
		goto iova_trace;

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->iova = iova;
	iova_buf->size = size;
	spin_lock_irqsave(&map_list.lock, flags);
	list_add(&iova_buf->list_node, &map_list.head[id]);
	spin_unlock_irqrestore(&map_list.lock, flags);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_MAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_map);

void mtk_iova_unmap(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	u64 start_t, end_t;
	unsigned long flags;
	struct iova_map_info *plist;
	struct iova_map_info *tmp_plist;
	int find_iova = 0;
	int i = 0;

	if (iommu_globals.iova_evt_enable == 0)
		return;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.iova_map_list == 0)
		goto iova_trace;

	spin_lock_irqsave(&map_list.lock, flags);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &map_list.head[id], list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->tab_id == tab_id) {
			list_del(&plist->list_node);
			kfree(plist);
			find_iova = 1;
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock_irqrestore(&map_list.lock, flags);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info_ratelimited("%s, find iova:[0x%llx 0x%llx 0x%zx] %d time:%llu\n",
				    __func__, tab_id, iova, size, i, (end_t - start_t));

#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
	if (!find_iova)
		pr_info("%s warnning, iova:[0x%llx 0x%llx 0x%zx] not find in %d\n",
			__func__, tab_id, iova, size, i);
#endif

iova_trace:
	mtk_iommu_iova_trace(IOMMU_UNMAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_unmap);

/* For smmu, tab_id is smmu hardware id */
int mtk_iova_map_dump(u64 iova, u64 tab_id)
{
	return mtk_iommu_iova_map_dump(NULL, iova, tab_id);
}
EXPORT_SYMBOL_GPL(mtk_iova_map_dump);

/* For smmu, tab_id is smmu hardware id */
int mtk_iova_dump(u64 iova, u64 tab_id)
{
	return mtk_iommu_iova_dump(NULL, iova, tab_id);
}
EXPORT_SYMBOL_GPL(mtk_iova_dump);

static int to_smmu_hw_id(u64 tab_id)
{
	return smmu_v3_enable ? smmu_tab_id_to_smmu_id(tab_id) : tab_id;
}

static void mtk_iommu_iova_map_info_dump(struct seq_file *s,
					 struct iova_map_info *plist)
{
	if (!plist)
		return;

	if (smmu_v3_enable) {
		iommu_dump(s, "%-7u 0x%-7u 0x%-12llx 0x%-8zx %llu.%06u\n",
			   smmu_tab_id_to_smmu_id(plist->tab_id),
			   smmu_tab_id_to_asid(plist->tab_id),
			   plist->iova, plist->size,
			   plist->time_high, plist->time_low);
	} else {
		iommu_dump(s, "%-6llu 0x%-12llx 0x%-8zx %llu.%06u\n",
			   plist->tab_id, plist->iova, plist->size,
			   plist->time_high, plist->time_low);
	}
}

/* For smmu, tab_id is smmu hardware id */
static int mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id)
{
	u32 i, id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *plist = NULL;
	struct iova_map_info *n = NULL;
	int dump_count = 0;

	if (iommu_globals.iova_evt_enable == 0 || iommu_globals.iova_map_list == 0)
		return 0;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return -EINVAL;
	}

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu iova map dump:\n");
		iommu_dump(s, "%-7s %-9s %-14s %-10s %17s\n",
			   "smmu_id", "asid", "iova", "size", "time");
	} else {
		iommu_dump(s, "iommu iova map dump:\n");
		iommu_dump(s, "%-6s %-14s %-10s %17s\n",
			   "tab_id", "iova", "size", "time");
	}

	spin_lock_irqsave(&map_list.lock, flags);
	if (!iova) {
		for (i = 0; i < MTK_IOVA_SPACE_NUM; i++) {
			list_for_each_entry_safe(plist, n, &map_list.head[i], list_node)
				if (to_smmu_hw_id(plist->tab_id) == tab_id) {
					mtk_iommu_iova_map_info_dump(s, plist);
					dump_count++;
					if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
						break;
				}
		}
		spin_unlock_irqrestore(&map_list.lock, flags);
		return dump_count;
	}

	list_for_each_entry_safe(plist, n, &map_list.head[id], list_node)
		if (to_smmu_hw_id(plist->tab_id) == tab_id &&
		    iova <= (plist->iova + plist->size) &&
		    iova >= (plist->iova)) {
			mtk_iommu_iova_map_info_dump(s, plist);
			dump_count++;
			if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
				break;
		}
	spin_unlock_irqrestore(&map_list.lock, flags);
	return dump_count;
}

static int __iommu_trace_dump(struct seq_file *s, u64 iova)
{
	u64 start_t, end_t;
	int dump_count = 0;
	int event_id;
	int i = 0;

	if (iommu_globals.dump_enable == 0)
		return 0;

	start_t = sched_clock();
	if (smmu_v3_enable) {
		iommu_dump(s, "smmu trace dump:\n");
		iommu_dump(s, "%-8s %-9s %-11s %-11s %-14s %-12s %-14s %17s %s\n",
			   "action", "smmu_id", "stream_id", "asid", "iova_start",
			   "size", "iova_end", "time", "dev");
	} else {
		iommu_dump(s, "iommu trace dump:\n");
		iommu_dump(s, "%-8s %-4s %-14s %-12s %-14s %17s %s\n",
			   "action", "tab_id", "iova_start", "size", "iova_end",
			   "time", "dev");
	}
	for (i = 0; i < IOMMU_EVENT_COUNT_MAX; i++) {
		unsigned long start_iova = 0;
		unsigned long end_iova = 0;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;
		event_id = iommu_globals.record[i].event_id;
		if (event_id < 0 || event_id >= IOMMU_EVENT_MAX)
			continue;

		if (event_id <= IOMMU_UNSYNC && iommu_globals.record[i].data1 > 0) {
			end_iova = iommu_globals.record[i].data1 +
				   iommu_globals.record[i].data2 - 1;
		}

		if (iova > 0 && event_id <= IOMMU_UNSYNC) {
			start_iova = iommu_globals.record[i].data1;
			if (!(iova <= end_iova && iova >= start_iova))
				continue;
		}

		if (smmu_v3_enable) {
			iommu_dump(s,
				   "%-8s 0x%-7x 0x%-9x 0x%-9x 0x%-12lx 0x%-10lx 0x%-12lx %10llu.%06u %s\n",
				   event_mgr[event_id].name,
				   smmu_tab_id_to_smmu_id(iommu_globals.record[i].data3),
				   (iommu_globals.record[i].dev != NULL ?
				   get_smmu_stream_id(iommu_globals.record[i].dev) : -1),
				   smmu_tab_id_to_asid(iommu_globals.record[i].data3),
				   iommu_globals.record[i].data1,
				   iommu_globals.record[i].data2,
				   end_iova,
				   iommu_globals.record[i].time_high,
				   iommu_globals.record[i].time_low,
				   (iommu_globals.record[i].dev != NULL ?
				   dev_name(iommu_globals.record[i].dev) : ""));
		} else {
			iommu_dump(s, "%-8s %-6lu 0x%-12lx 0x%-10lx 0x%-12lx %10llu.%06u %s\n",
				   event_mgr[event_id].name,
				   iommu_globals.record[i].data3,
				   iommu_globals.record[i].data1,
				   iommu_globals.record[i].data2,
				   end_iova,
				   iommu_globals.record[i].time_high,
				   iommu_globals.record[i].time_low,
				   (iommu_globals.record[i].dev != NULL ?
				   dev_name(iommu_globals.record[i].dev) : ""));
		}
		dump_count++;
		if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
			break;
	}
	end_t = sched_clock();

	iommu_dump(s, "trace dump start:%llu, end:%llu, cost:%llu\n",
		   start_t, end_t, (end_t - start_t));
	return dump_count;
}

static int mtk_iommu_trace_dump(struct seq_file *s)
{
	return __iommu_trace_dump(s, 0);
}

int mtk_iova_trace_dump(u64 iova)
{
	return __iommu_trace_dump(NULL, iova);
}
EXPORT_SYMBOL_GPL(mtk_iova_trace_dump);

void mtk_iommu_debug_reset(void)
{
	iommu_globals.enable = 1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_debug_reset);

static inline const char *get_power_status_str(int status)
{
	return (status == 0 ? "Power On" : "Power Off");
}

/**
 * Get mtk_iommu_port list index.
 * @tf_id: Hardware reported AXI id when translation fault
 * @type: mtk_iommu_type or mtk_smmu_type
 * @id: iommu_id for iommu, smmu common id for smmu
 */
static int mtk_iommu_get_tf_port_idx(int tf_id, u32 type, int id)
{
	int i;
	u32 vld_id, port_nr;
	const struct mtk_iommu_port *port_list;
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	u32 smmu_type = type;

	/* Only support MM_SMMU and APU_SMMU */
	if (smmu_v3_enable && smmu_type > APU_SMMU) {
		pr_info("%s fail, invalid type %d\n", __func__, smmu_type);
		return m4u_data->plat_data->port_nr[MM_SMMU];
	} else if (!smmu_v3_enable && type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return m4u_data->plat_data->port_nr[MM_IOMMU];
	}

	if (m4u_data->plat_data->get_valid_tf_id) {
		vld_id = m4u_data->plat_data->get_valid_tf_id(tf_id, type, id);
	} else {
		if (type == APU_IOMMU)
			vld_id = F_APU_MMU_INT_TF_MSK(tf_id);
		else
			vld_id = tf_id & F_MMU_INT_TF_MSK;
	}

	pr_info("get vld tf_id:0x%x\n", vld_id);
	port_nr =  m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];

	/* check (larb | port) for smi_larb or apu_bus */
	for (i = 0; i < port_nr; i++) {
		if (m4u_data->plat_data->tf_id_is_match) {
			if (m4u_data->plat_data->tf_id_is_match(tf_id, type, id, port_list[i]))
				return i;
		} else {
			if (port_list[i].port_type == NORMAL &&
			    port_list[i].tf_id == vld_id &&
			    port_list[i].id == id)
				return i;
		}
	}

	/* check larb for smi_common */
	if (type == MM_IOMMU && m4u_data->plat_data->mm_tf_ccu_support) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == CCU_FAKE &&
			    (port_list[i].tf_id & F_MMU_INT_TF_CCU_MSK) ==
			    (vld_id & F_MMU_INT_TF_CCU_MSK) &&
			    port_list[i].id == id)
				return i;
		}
	}

	/* check gce/video_uP */
	mm_tf_is_gce_videoup = m4u_data->plat_data->mm_tf_is_gce_videoup;
	if (type == MM_IOMMU && mm_tf_is_gce_videoup) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == GCE_VIDEOUP_FAKE &&
			    mm_tf_is_gce_videoup(port_list[i].tf_id, tf_id) &&
			    port_list[i].id == id)
				return i;
		}
	}

	return port_nr;
}

static int mtk_iommu_port_idx(int id, enum mtk_iommu_type type, int *idx_list)
{
	int  i, larb_id = -1;
	u32 port_nr;
	const struct mtk_iommu_port *port_list;

	if (type < MM_IOMMU || type >= TYPE_NUM || idx_list == NULL) {
		pr_info("%s, invalid parameter, type=%d\n", __func__, type);
		return -1;
	}

	/* some larb port connected to both MDP and DISP SMMU will use 2nd idx */
	idx_list[0] = -1;
	idx_list[1] = -1;
	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	for (i = 0; i < port_nr; i++) {
		if ((port_list[i].larb_id == MTK_M4U_TO_LARB(id)) &&
		     (port_list[i].port_id == MTK_M4U_TO_PORT(id))) {
			if (idx_list[0] == -1) {
				idx_list[0] = i;
				larb_id = port_list[i].larb_id;
			} else {
				idx_list[1]  = i;
				break;
			}
		}

		if (larb_id != -1 && port_list[i].larb_id > larb_id)
			break;
	}

	return idx_list[0] >= 0 ? 0 : -1;
}

static void report_custom_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, u32 type, int id)
{
	const struct mtk_iommu_port *port_list;
	bool support_tf_fn = false;
	u32 smmu_type = type;
	u32 port_nr;
	int idx;

	/* Only support MM_SMMU and APU_SMMU */
	if (smmu_v3_enable && smmu_type > APU_SMMU) {
		pr_info("%s fail, invalid type %d\n", __func__, smmu_type);
		return;
	} else if (!smmu_v3_enable && type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return;
	}

	pr_info("error, tf report start fault_id:0x%x\n", fault_id);
	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(fault_id, type, id);
	if (idx >= port_nr) {
		pr_warn("fail,iova:0x%llx, port:0x%x\n",
			fault_iova, fault_id);
		return;
	}

	/* Only MM_IOMMU support fault callback */
	support_tf_fn = (smmu_v3_enable ? (smmu_type == MM_SMMU) : (type == MM_IOMMU));
	if (support_tf_fn) {
		pr_info("error, tf report larb-port:(%u--%u), idx:%d\n",
			port_list[idx].larb_id,
			port_list[idx].port_id, idx);

		if (port_list[idx].enable_tf &&
			m4u_data->m4u_cb[idx].fault_fn)
			m4u_data->m4u_cb[idx].fault_fn(m4u_data->m4u_cb[idx].port,
			fault_iova, m4u_data->m4u_cb[idx].fault_data);
	}

	m4u_aee_print(mmu_translation_log_format,
		(smmu_v3_enable ? "SMMU" : "M4U"),
		port_list[idx].name,
		port_list[idx].name, fault_iova,
		fault_pa);
}

void report_custom_iommu_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, enum mtk_iommu_type type,
	int id)
{
	report_custom_fault(fault_iova, fault_pa, fault_id, type, id);
}
EXPORT_SYMBOL_GPL(report_custom_iommu_fault);

void report_iommu_mau_fault(
	u32 assert_id, u32 falut_id, char *port_name,
	u32 assert_addr, u32 assert_b32)
{
	m4u_aee_print(mau_assert_log_format,
		      assert_id, falut_id, port_name, assert_addr, assert_b32);
}
EXPORT_SYMBOL_GPL(report_iommu_mau_fault);

int mtk_iommu_register_fault_callback(int port,
	mtk_iommu_fault_callback_t fn,
	void *cb_data, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int i, idx, idx_list[] = {-1, -1};

	if (mtk_iommu_port_idx(port, type, idx_list)) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(idx_list); i++) {
		idx = idx_list[i];
		if (idx >= 0) {
			if (is_vpu)
				idx += m4u_data->plat_data->port_nr[type];

			m4u_data->m4u_cb[idx].port = port;
			m4u_data->m4u_cb[idx].fault_fn = fn;
			m4u_data->m4u_cb[idx].fault_data = cb_data;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_register_fault_callback);

int mtk_iommu_unregister_fault_callback(int port, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int i, idx, idx_list[] = {-1, -1};

	if (mtk_iommu_port_idx(port, type, idx_list)) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(idx_list); i++) {
		idx = idx_list[i];
		if (idx > 0) {
			if (is_vpu)
				idx += m4u_data->plat_data->port_nr[type];

			m4u_data->m4u_cb[idx].port = -1;
			m4u_data->m4u_cb[idx].fault_fn = NULL;
			m4u_data->m4u_cb[idx].fault_data = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_unregister_fault_callback);

char *mtk_iommu_get_port_name(enum mtk_iommu_type type, int id, int tf_id)
{
	const struct mtk_iommu_port *port_list;
	u32 port_nr;
	int idx;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_notice("%s fail, invalid type %d\n", __func__, type);
		return "m4u_port_unknown";
	}

	if (type == PERI_IOMMU)
		return peri_tf_analyse(id, tf_id);

	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(tf_id, type, id);
	if (idx >= port_nr) {
		pr_notice("%s err, iommu(%d,%d) tf_id:0x%x\n",
			  __func__, type, id, tf_id);
		return "m4u_port_unknown";
	}

	return port_list[idx].name;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_port_name);

const struct mau_config_info *mtk_iommu_get_mau_config(
	enum mtk_iommu_type type, int id,
	unsigned int slave, unsigned int mau)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	const struct mau_config_info *mau_config;
	int i;

	for (i = 0; i < m4u_data->plat_data->mau_config_nr; i++) {
		mau_config = &m4u_data->plat_data->mau_config[i];
		if (mau_config->iommu_type == type &&
		    mau_config->iommu_id == id &&
		    mau_config->slave == slave &&
		    mau_config->mau == mau)
			return mau_config;
	}
#endif

	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_mau_config);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
static const struct mtk_smmu_ops *smmu_ops;

int mtk_smmu_set_debug_ops(const struct mtk_smmu_ops *ops)
{
	if (smmu_ops == NULL)
		smmu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_set_debug_ops);

static int mtk_smmu_power_get(u32 smmu_type)
{
	return mtk_smmu_rpm_get(smmu_type);
}

static int mtk_smmu_power_put(u32 smmu_type)
{
	return mtk_smmu_rpm_put(smmu_type);
}

static int get_smmu_common_id(u32 smmu_type, u32 tbu_id)
{
	int id = -1;

	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("%s fail, invalid smmu_type %d\n", __func__, smmu_type);
		return -1;
	}

	if (tbu_id >= SMMU_TBU_CNT(smmu_type)) {
		pr_info("%s fail, invalid tbu_id:%u\n", __func__, tbu_id);
		return -1;
	}

	if (m4u_data->plat_data->smmu_common_id)
		id = m4u_data->plat_data->smmu_common_id(smmu_type, tbu_id);

	pr_debug("%s smmu_type:%u, tbu_id:%u, id:%d\n",
		 __func__, smmu_type, tbu_id, id);

	return id;
}

void report_custom_smmu_fault(u64 fault_iova, u64 fault_pa,
			      u32 fault_id, u32 smmu_id)
{
	u32 tbu_id = SMMUWP_TF_TBU_VAL(fault_id);
	char *port_name = NULL;
	int id;

	id = get_smmu_common_id(smmu_id, tbu_id);
	if (id < 0)
		return;

	if (smmu_id == SOC_SMMU) {
		if (m4u_data->plat_data->smmu_port_name)
			port_name = m4u_data->plat_data->smmu_port_name(SOC_SMMU, id, fault_id);

		if (port_name != NULL)
			m4u_aee_print(mmu_translation_log_format, "SMMU", port_name,
				      port_name, fault_iova, fault_pa);
		return;
	} else if (smmu_id == GPU_SMMU) {
		m4u_aee_print(mmu_translation_log_format, "SMMU", "GPU",
			      "GPU", fault_iova, fault_pa);
		return;
	}

	report_custom_fault(fault_iova, fault_pa, fault_id, smmu_id, id);
}
EXPORT_SYMBOL_GPL(report_custom_smmu_fault);

static void dump_wrapper_register(struct seq_file *s,
				  struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	unsigned int smmuwp_reg_nr, i;

	iommu_dump(s, "wp reg for smmu:%d, base:0x%llx, wp_base:0x%llx\n",
		   data->plat_data->smmu_type,
		   (unsigned long long) smmu->base,
		   (unsigned long long) smmu->wp_base);

	smmuwp_reg_nr = ARRAY_SIZE(smmuwp_regs);

	/* SOC has one less TBU than the others */
	if (data->plat_data->smmu_type == SOC_SMMU)
		smmuwp_reg_nr -= SMMU_TBU_REG_NUM;

	for (i = 0; i < smmuwp_reg_nr; i++) {
		if (i + 4 < smmuwp_reg_nr) {
			iommu_dump(s,
				   "%-11s:0x%03x=0x%-8x %-11s:0x%03x=0x%-8x %-11s:0x%03x=0x%-8x %-11s:0x%03x=0x%-8x %-11s:0x%03x=0x%x\n",
				   smmuwp_regs[i + 0].name, smmuwp_regs[i + 0].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 0].offset),
				   smmuwp_regs[i + 1].name, smmuwp_regs[i + 1].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 1].offset),
				   smmuwp_regs[i + 2].name, smmuwp_regs[i + 2].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 2].offset),
				   smmuwp_regs[i + 3].name, smmuwp_regs[i + 3].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 3].offset),
				   smmuwp_regs[i + 4].name, smmuwp_regs[i + 4].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 4].offset));
			i = i + 4;
		} else {
			iommu_dump(s, "%-11s:0x%03x=0x%x\n",
				   smmuwp_regs[i].name, smmuwp_regs[i].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i].offset));
		}
	}
}

void mtk_smmu_wpreg_dump(struct seq_file *s, u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			dump_wrapper_register(s, &data->smmu);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_wpreg_dump);

//=====================================================
// SMMU private data for Dump Page Table start
//=====================================================

/* IOPTE accessors */
#define iopte_deref(pte, d) __va(iopte_to_paddr(pte, d))

static phys_addr_t iopte_to_paddr(arm_lpae_iopte pte,
				  struct arm_lpae_io_pgtable *data)
{
	u64 paddr = pte & ARM_LPAE_PTE_ADDR_MASK;

	if (ARM_LPAE_GRANULE(data) < SZ_64K)
		return paddr;

	/* Rotate the packed high-order bits back to the top */
	return (paddr | (paddr << (48 - 12))) & (ARM_LPAE_PTE_ADDR_MASK << 4);
}

static u64 arm_lpae_iova_to_iopte(struct io_pgtable_ops *ops, unsigned long iova)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
	arm_lpae_iopte pte, *ptep = data->pgd;
	int lvl = data->start_level;

	do {
		/* Valid IOPTE pointer? */
		if (!ptep)
			return 0;

		/* Grab the IOPTE we're interested in */
		ptep += ARM_LPAE_LVL_IDX(iova, lvl, data);
		pte = READ_ONCE(*ptep);

		/* Valid entry? */
		if (!pte)
			return 0;

		/* Leaf entry? */
		if (iopte_leaf(pte, lvl, data->iop.fmt))
			goto found_translation;

		/* Take it to the next level */
		ptep = iopte_deref(pte, data);
	} while (++lvl < ARM_LPAE_MAX_LEVELS);

	/* Ran out of page tables to walk */
	return 0;

found_translation:
	pr_info("%s, iova:0x%lx, pte:0x%llx, lvl:%d, iopte_type:%llu, fmt:%u\n",
		__func__, iova, pte, lvl, iopte_type(pte), data->iop.fmt);
	return pte;
}

static void arm_lpae_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;

	iommu_dump(s, "SMMU OPS values:\n");
	iommu_dump(s,
		   "ops cfg: quirks 0x%lx, pgsize_bitmap 0x%lx, ias %u-bit, oas %u-bit, coherent_walk:%d\n",
		   cfg->quirks, cfg->pgsize_bitmap, cfg->ias, cfg->oas, cfg->coherent_walk);
	iommu_dump(s,
		   "ops data: %d levels, 0x%zx pgd_size, %u pg_shift, %u bits_per_level, pgd @ %p\n",
		   ARM_LPAE_MAX_LEVELS - data->start_level, ARM_LPAE_PGD_SIZE(data),
		   ilog2(ARM_LPAE_GRANULE(data)), data->bits_per_level, data->pgd);
}

static void dump_pgtable_ops(struct seq_file *s, struct arm_smmu_master *master)
{
	if (!master || !master->domain || !master->domain->pgtbl_ops) {
		iommu_dump(s, "Not do arm_smmu_domain_finalise\n");
		return;
	}

	arm_lpae_ops_dump(s, master->domain->pgtbl_ops);
}

static inline bool is_valid_ste(__le64 *ste)
{
	return (ste && (le64_to_cpu(ste[0]) & STRTAB_STE_0_V));
}

static inline bool is_valid_cd(__le64 *cd)
{
	return (cd && (le64_to_cpu(cd[0]) & CTXDESC_CD_0_V));
}

static void ste_dump(struct seq_file *s, u32 sid, __le64 *ste)
{
	int i;

	if (ste == NULL)
		return;

	if (!is_valid_ste(ste)) {
		iommu_dump(s, "Failed to valid ste(sid:%u)\n", sid);
		return;
	}

	iommu_dump(s, "SMMU STE values:\n");
	for (i = 0; i < STRTAB_STE_DWORDS; i++) {
		if (i + 3 < STRTAB_STE_DWORDS) {
			iommu_dump(s, "u64[%d~%d]:0x%016llx|0x%016llx|0x%016llx|0x%016llx\n",
				   i, i + 3, ste[i + 0], ste[i + 1], ste[i + 2], ste[i + 3]);
			i = i + 3;
		} else {
			iommu_dump(s, "u64[%d]:0x%016llx\n", i, ste[i]);
		}
	}
}

static void cd_dump(struct seq_file *s, u32 ssid, __le64 *cd)
{
	int i;

	if (cd == NULL)
		return;

	if (!is_valid_cd(cd)) {
		iommu_dump(s, "Failed to valid cd(ssid:%u)\n", ssid);
		return;
	}

	iommu_dump(s, "SMMU CD values:\n");
	for (i = 0; i < CTXDESC_CD_DWORDS; i++) {
		if (i + 3 < CTXDESC_CD_DWORDS) {
			iommu_dump(s, "u64[%d~%d]:0x%016llx|0x%016llx|0x%016llx|0x%016llx\n",
				   i, i + 3, cd[i + 0], cd[i + 1], cd[i + 2], cd[i + 3]);
			i = i + 3;
		} else {
			iommu_dump(s, "u64[%d]:0x%016llx\n", i, cd[i]);
		}
	}
}

static void dump_ste_cd_info(struct seq_file *s,
			     struct arm_smmu_master *master)
{
	struct arm_smmu_domain *domain;
	struct arm_smmu_device *smmu;
	__le64 *steptr = NULL, *cdptr = NULL;
	u64 asid = 0, ttbr = 0;
	u32 sid, ssid;

	if (!master || !master->streams) {
		pr_info("%s, ERROR", __func__);
		return;
	}

	/* currently only support one master one sid */
	sid = master->streams[0].id;
	ssid = 0;
	smmu = master->smmu;
	domain = master->domain;

	if (smmu_ops && smmu_ops->get_step_ptr)
		steptr = smmu_ops->get_step_ptr(smmu, sid);

	if (smmu_ops && smmu_ops->get_cd_ptr)
		cdptr = smmu_ops->get_cd_ptr(domain, ssid);

	if (is_valid_cd(cdptr)) {
		asid = FIELD_GET(CTXDESC_CD_0_ASID, le64_to_cpu(cdptr[0]));
		ttbr = FIELD_GET(CTXDESC_CD_1_TTB0_MASK,  le64_to_cpu(cdptr[1]));
	}

	iommu_dump(s, "%s strtab base:0x%llx, cfg:0x%x\n", __func__,
		   smmu->strtab_cfg.strtab_base, smmu->strtab_cfg.strtab_base_cfg);
	iommu_dump(s, "%s sid=0x%x asid=0x%llx ttbr=0x%llx dev(%s) [cd:%d ste:%d]\n",
		   __func__, sid, asid, ttbr, dev_name(master->dev),
		   (cdptr == NULL), (steptr == NULL));

	ste_dump(s, sid, steptr);
	cd_dump(s, ssid, cdptr);
}

static inline void pt_info_dump(struct seq_file *s,
				struct arm_lpae_io_pgtable *data,
				arm_lpae_iopte *ptep,
				arm_lpae_iopte pte_s,
				arm_lpae_iopte pte_e,
				u64 iova_s,
				u64 iova_e,
				u64 pgsize,
				u64 pgcount)
{
	iommu_dump(s,
		   "ptep:%pa pte:0x%llx ~ 0x%llx iova:0x%llx ~ 0x%llx -> pa:0x%llx ~ 0x%llx pgsize:0x%llx count:%llu\n",
		   &ptep, pte_s, pte_e, iova_s, (iova_e + pgsize -1),
		   iopte_to_paddr(pte_s, data),
		   (iopte_to_paddr(pte_e, data) + pgsize -1),
		   pgsize, pgcount);
}

static void __ptdump(struct seq_file *s, arm_lpae_iopte *ptep, int lvl, u64 va,
		     struct arm_lpae_io_pgtable *data)
{
	arm_lpae_iopte pte, *ptep_next;
	u64 i, entry_num, pgsize, pgcount = 0, tmp_va = 0;
	arm_lpae_iopte pte_pre = 0;
	arm_lpae_iopte pte_s = 0;
	arm_lpae_iopte pte_e = 0;
	bool need_ptdump = false;
	bool need_continue = false;
	u64 iova_s = 0;
	u64 iova_e = 0;

	entry_num = 1 << (data->bits_per_level + ARM_LPAE_PGD_IDX(lvl, data));
	pgsize = ARM_LPAE_BLOCK_SIZE(lvl, data);

	iommu_dump(s, "ptep:%pa lvl:%d va:0x%llx pgsize:0x%llx entry_num:%llu\n",
		   &ptep, lvl, va, pgsize, entry_num);

	for (i = 0; i < entry_num; i++) {
		pte = READ_ONCE(*(ptep + i));
		if (!pte)
			goto ptdump_reset_continue;

		tmp_va = va | (i << ARM_LPAE_LVL_SHIFT(lvl, data));

		if (iopte_leaf(pte, lvl, data->iop.fmt)) {
#ifdef SMMU_PTDUMP_RAW
			iommu_dump(s, "ptep:%pa pte_raw:0x%llx iova:0x%llx -> pa:0x%llx pgsize:0x%llx\n",
				   &ptep, pte, tmp_va, iopte_to_paddr(pte, data), pgsize);
#endif
			if (pte_s == 0) {
				pte_s = pte;
				pte_e = pte;
				iova_s = tmp_va;
				iova_e = tmp_va;
			}

			if (pte_pre == 0 || pte - pte_pre == pgsize) {
				need_ptdump = true;
				pte_pre = pte;
				pte_e = pte;
				iova_e = tmp_va;
				pgcount++;
			} else {
				pt_info_dump(s, data, ptep, pte_s, pte_e, iova_s, iova_e,
					     pgsize, pgcount);
				need_ptdump = true;
				pte_pre = pte;
				pte_s = pte;
				pte_e = pte;
				iova_s = tmp_va;
				iova_e = tmp_va;
				pgcount = 1;
			}

			if ((i + 1) == entry_num)
				goto ptdump_reset_continue;

			continue;
		}

		goto ptdump_reset;

ptdump_reset_continue:
		need_continue = true;

ptdump_reset:
		if (need_ptdump) {
			pt_info_dump(s, data, ptep, pte_s, pte_e, iova_s, iova_e,
				     pgsize, pgcount);
			need_ptdump = false;
			pte_pre = 0;
			pte_s = 0;
			pte_e = 0;
			iova_s = 0;
			iova_e = 0;
			pgcount = 0;
		}

		if(need_continue) {
			need_continue = false;
			continue;
		}

		ptep_next = iopte_deref(pte, data);
		__ptdump(s, ptep_next, lvl + 1, tmp_va, data);
	}
}

static void ptdump(struct seq_file *s,
		   struct arm_smmu_domain *domain,
		   void *pgd, int stage, u32 ssid)
{
	struct arm_lpae_io_pgtable *data, data_sva;
	int levels, va_bits, bits_per_level;
	struct io_pgtable_ops *ops;
	arm_lpae_iopte *ptep = pgd;

	iommu_dump(s, "SMMU dump page table for stage %d, ssid 0x%x:\n",
		   stage, ssid);

	if (stage == 1 && !ssid) {
		ops = domain->pgtbl_ops;
		data = io_pgtable_ops_to_data(ops);
	} else {
		va_bits = VA_BITS - PAGE_SHIFT;
		bits_per_level = PAGE_SHIFT - ilog2(sizeof(arm_lpae_iopte));
		levels = DIV_ROUND_UP(va_bits, bits_per_level);

		memset(&data_sva, 0, sizeof(data_sva));
		data_sva.start_level = ARM_LPAE_MAX_LEVELS - levels;
		data_sva.pgd_bits = va_bits - (bits_per_level * (levels - 1));
		data_sva.bits_per_level = bits_per_level;
		data_sva.pgd = pgd;

		data = &data_sva;
	}

	__ptdump(s, ptep, data->start_level, 0, data);
}

static void dump_io_pgtable_s1(struct seq_file *s,
			       struct arm_smmu_domain *domain,
			       u32 sid, u32 ssid, __le64 *cd)
{
	void *pgd;
	u64 ttbr;

	if (!is_valid_cd(cd)) {
		iommu_dump(s, "Failed to find valid cd(sid:%u, ssid:%u):%d\n",
			   sid, ssid, (cd == NULL));
		return;
	}

	/* CD0 and other CDx are all using ttbr0 */
	ttbr = le64_to_cpu(cd[1]) & CTXDESC_CD_1_TTB0_MASK;

	if (!ttbr) {
		iommu_dump(s, "Stage 1 TTBR is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	pgd = phys_to_virt(ttbr);
	ptdump(s, domain, pgd, 1, ssid);
}

static void dump_io_pgtable_s2(struct seq_file *s,
			       struct arm_smmu_domain *domain,
			       u32 sid, u32 ssid, __le64 *ste)
{
	void *pgd;
	u64 vttbr;

	if (!is_valid_ste(ste)) {
		iommu_dump(s, "Failed to valid ste(sid:%u):%d\n", sid, (ste == NULL));
		return;
	}

	if (!(le64_to_cpu(ste[0]) & (1UL << 2))) {
		iommu_dump(s, "Stage 2 translation is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	vttbr = le64_to_cpu(ste[3]) & STRTAB_STE_3_S2TTB_MASK;

	if (!vttbr) {
		iommu_dump(s, "Stage 2 TTBR is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	pgd = phys_to_virt(vttbr);
	ptdump(s, domain, pgd, 2, ssid);
}

static void dump_io_pgtable(struct seq_file *s, struct arm_smmu_master *master)
{
	struct arm_smmu_domain *domain;
	struct arm_smmu_device *smmu;
	__le64 *steptr = NULL, *cdptr = NULL;
	u32 sid, ssid;

	if (!master || !master->streams) {
		pr_info("%s, ERROR", __func__);
		return;
	}

	/* currently only support one master one sid */
	sid = master->streams[0].id;
	ssid = 0;
	smmu = master->smmu;
	domain = master->domain;

	iommu_dump(s, "SMMU dump page table for sid:0x%x, ssid:0x%x:\n",
		   sid, ssid);

	if (smmu_ops && smmu_ops->get_cd_ptr)
		cdptr = smmu_ops->get_cd_ptr(domain, ssid);

	dump_io_pgtable_s1(s, domain, sid, ssid, cdptr);

	if (smmu_ops && smmu_ops->get_step_ptr)
		steptr = smmu_ops->get_step_ptr(smmu, sid);

	dump_io_pgtable_s2(s, domain, sid, ssid, steptr);
}

static void dump_ste_info_list(struct seq_file *s,
			       struct arm_smmu_device *smmu)
{
	struct rb_node *n;
	struct arm_smmu_stream *stream;

	if (!smmu) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL)
			dump_ste_cd_info(s, stream->master);
	}
}

void mtk_smmu_ste_cd_dump(struct seq_file *s, u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			dump_ste_info_list(s, &data->smmu);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_ste_cd_dump);

void mtk_smmu_ste_cd_info_dump(struct seq_file *s, u32 smmu_type, u32 sid)
{
	struct arm_smmu_device *smmu = NULL;
	struct arm_smmu_stream *stream;
	struct mtk_smmu_data *data;
	struct rb_node *n;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			smmu = &data->smmu;
	}

	if (smmu == NULL) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL &&
		    stream->master->streams != NULL)
			/* currently only support one master one sid */
			if (sid == stream->master->streams[0].id) {
				dump_ste_cd_info(s, stream->master);
				return;
			}
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_ste_cd_info_dump);

static void smmu_pgtable_dump(struct seq_file *s, struct arm_smmu_device *smmu, bool dump_rawdata)
{
	struct rb_node *n;
	struct arm_smmu_stream *stream;
	struct mtk_smmu_data *data;

	if (!smmu) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	data = to_mtk_smmu_data(smmu);
	iommu_dump(s, "pgtable dump for smmu_%d:\n", data->plat_data->smmu_type);

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL) {
			dump_pgtable_ops(s, stream->master);
			dump_ste_cd_info(s, stream->master);

			if (dump_rawdata)
				dump_io_pgtable(s, stream->master);
		}
	}
}

void mtk_smmu_pgtable_dump(struct seq_file *s, u32 smmu_type, bool dump_rawdata)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			smmu_pgtable_dump(s, &data->smmu, dump_rawdata);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_pgtable_dump);

void mtk_smmu_pgtable_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops)
{
	arm_lpae_ops_dump(s, ops);
}
EXPORT_SYMBOL_GPL(mtk_smmu_pgtable_ops_dump);

u64 mtk_smmu_iova_to_iopte(struct io_pgtable_ops *ops, u64 iova)
{
	return arm_lpae_iova_to_iopte(ops, iova);
}
EXPORT_SYMBOL_GPL(mtk_smmu_iova_to_iopte);

int mtk_smmu_latest_trace_dump(struct seq_file *s, u32 smmu_type)
{
	int dump_count = 0, trace_count = 0;
	int event_id;
	int i = 0;
	unsigned int start_idx;

	if (!smmu_v3_enable || smmu_type > APU_SMMU ||
	    iommu_globals.dump_enable == 0 || iommu_globals.write_pointer == 0)
		return 0;

	iommu_dump(s, "%-8s %-9s %-11s %-11s %-14s %-12s %-14s %17s %s\n",
		   "action", "smmu_id", "stream_id", "asid", "iova_start",
		   "size", "iova_end", "time", "dev");

	start_idx = (atomic_read((atomic_t *)
		    &(iommu_globals.write_pointer)) - 1)
		    % IOMMU_EVENT_COUNT_MAX;

	i = start_idx;
	while (dump_count < IOVA_LATEST_DUMP_MAX && trace_count++ < IOVA_LATEST_TRACE_MAX) {
		unsigned long end_iova = 0;

		if (i < 0)
			i = IOMMU_EVENT_COUNT_MAX  - 1;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;

		event_id = iommu_globals.record[i].event_id;
		if (event_id < 0 || event_id > IOMMU_FREE ||
		    smmu_tab_id_to_smmu_id(
		    iommu_globals.record[i].data3) != smmu_type) {
			i--;
			continue;
		}

		end_iova = iommu_globals.record[i].data1 +
			   iommu_globals.record[i].data2 - 1;
		iommu_dump(s,
			   "%-8s 0x%-7x 0x%-9x 0x%-9x 0x%-12lx 0x%-10lx 0x%-12lx %10llu.%06u %s\n",
			   event_mgr[event_id].name,
			   smmu_tab_id_to_smmu_id(iommu_globals.record[i].data3),
			   (iommu_globals.record[i].dev != NULL ?
			   get_smmu_stream_id(iommu_globals.record[i].dev) : -1),
			   smmu_tab_id_to_asid(iommu_globals.record[i].data3),
			   iommu_globals.record[i].data1,
			   iommu_globals.record[i].data2,
			   end_iova,
			   iommu_globals.record[i].time_high,
			   iommu_globals.record[i].time_low,
			   (iommu_globals.record[i].dev != NULL ?
			   dev_name(iommu_globals.record[i].dev) : ""));
		dump_count++;
		i--;
	}

	return dump_count;
}
EXPORT_SYMBOL_GPL(mtk_smmu_latest_trace_dump);
#else /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
static inline int mtk_smmu_power_get(u32 smmu_type)
{
	return -1;
}

static inline int mtk_smmu_power_put(u32 smmu_type)
{
	return -1;
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */

/* peri_iommu */
static struct peri_iommu_data mt6983_peri_iommu_data[PERI_IOMMU_NUM] = {
	[PERI_IOMMU_M4] = {
		.id = PERI_IOMMU_M4,
		.bus_id = 4,
	},
	[PERI_IOMMU_M6] = {
		.id = PERI_IOMMU_M6,
		.bus_id = 6,
	},
	[PERI_IOMMU_M7] = {
		.id = PERI_IOMMU_M7,
		.bus_id = 7,
	},
};

static char *mt6983_peri_m7_id(u32 id)
{
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id4_2 = FIELD_GET(GENMASK(4, 2), id);

	if (id1_0 == 0)
		return "MCU_AP_M";
	else if (id1_0 == 1)
		return "DEBUG_TRACE_LOG";
	else if (id1_0 == 2)
		return "PERI2INFRA1_M";

	switch (id4_2) {
	case 0:
		return "CQ_DMA";
	case 1:
		return "DEBUGTOP";
	case 2:
		return "GPU_EB";
	case 3:
		return "CPUM_M";
	case 4:
		return "DXCC_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_m6_id(u32 id)
{
	return "PERI2INFRA0_M";
}

static char *mt6983_peri_m4_id(u32 id)
{
	u32 id0 = id & 0x1;
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id3_2 = FIELD_GET(GENMASK(3, 2), id);

	if (id0 == 0)
		return "DFD_M";
	else if (id1_0 == 1)
		return "DPMAIF_M";

	switch (id3_2) {
	case 0:
		return "ADSPSYS_M0_M";
	case 1:
		return "VLPSYS_M";
	case 2:
		return "CONN_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_tf(enum peri_iommu id, u32 fault_id)
{
	switch (id) {
	case PERI_IOMMU_M4:
		return mt6983_peri_m4_id(fault_id);
	case PERI_IOMMU_M6:
		return mt6983_peri_m6_id(fault_id);
	case PERI_IOMMU_M7:
		return mt6983_peri_m7_id(fault_id);
	default:
		return "UNKNOWN";
	}
}

enum peri_iommu get_peri_iommu_id(u32 bus_id)
{
	int i;

	for (i = PERI_IOMMU_M4; i < PERI_IOMMU_NUM; i++) {
		if (bus_id == m4u_data->plat_data->peri_data[i].bus_id)
			return i;
	}

	return PERI_IOMMU_NUM;
};
EXPORT_SYMBOL_GPL(get_peri_iommu_id);

char *peri_tf_analyse(enum peri_iommu iommu_id, u32 fault_id)
{
	if (m4u_data->plat_data->peri_tf_analyse)
		return m4u_data->plat_data->peri_tf_analyse(iommu_id, fault_id);

	pr_info("%s is not support\n", __func__);
	return NULL;
}
EXPORT_SYMBOL_GPL(peri_tf_analyse);

static int mtk_iommu_debug_help(struct seq_file *s)
{
	iommu_dump(s, "iommu debug file:\n");
	iommu_dump(s, "help: description debug file and command\n");
	iommu_dump(s, "debug: iommu main debug file, receive debug command\n");
	iommu_dump(s, "iommu_dump: iova trace dump file\n");
	iommu_dump(s, "iova_alloc: iova alloc list dump file\n");
	iommu_dump(s, "iova_map: iova map list dump file\n");

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu_pgtable: smmu page table dump file\n");
		iommu_dump(s, "smmu_wp: smmu wrapper register dump file\n");
	}

	iommu_dump(s, "\niommu debug command:\n");
	iommu_dump(s, "echo 1 > /proc/iommu_debug/debug: iommu debug help\n");
	iommu_dump(s, "echo 2 > /proc/iommu_debug/debug: mm translation fault test\n");
	iommu_dump(s, "echo 3 > /proc/iommu_debug/debug: apu translation fault test\n");
	iommu_dump(s, "echo 4 > /proc/iommu_debug/debug: peri translation fault test\n");
	iommu_dump(s, "echo 5 > /proc/iommu_debug/debug: dump iova trace\n");
	iommu_dump(s, "echo 6 > /proc/iommu_debug/debug: dump iova alloc list\n");
	iommu_dump(s, "echo 7 > /proc/iommu_debug/debug: enable/disable trace log\n");
	iommu_dump(s, "echo 8 > /proc/iommu_debug/debug: enable/disable trace dump\n");
	iommu_dump(s, "echo 9 > /proc/iommu_debug/debug: enable/disable debug config\n");

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
static inline u32 get_debug_config(u64 val, u32 config_bit)
{
	return (val & F_BIT_SET(config_bit)) > 0 ? 1 : 0;
}

/* Notice: Please also update help info if debug command changes */
static int m4u_debug_set(void *data, u64 input)
{
	u32 index = FIELD_GET(GENMASK_ULL(4, 0), input);
	u64 val = FIELD_GET(GENMASK_ULL(28, 5), input);
	u64 tag = FIELD_GET(GENMASK_ULL(31, 29), input);
	void *file = NULL;
	int ret = 0;
	u32 i = 0;

	if (tag == (u64)IOMMU_DUMP_TAG_SKIP)
		file = IOMMU_DUMP_TAG_SKIP;

	pr_info("%s input:0x%llx, index:%u, val:%llu, data:%d\n",
		__func__, input, index, val, (data != NULL));

	switch (index) {
	case 1:	/* show help info */
		ret = mtk_iommu_debug_help(file);
		break;
	case 2: /* mm translation fault test */
		report_custom_iommu_fault(0, 0, 0x500000f, MM_IOMMU, 0);
		break;
	case 3: /* apu translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, APU_IOMMU, 0);
		break;
	case 4: /* peri translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, PERI_IOMMU, 0);
		break;
	case 5:	/* dump iova trace */
		ret = mtk_iommu_trace_dump(file);
		break;
	case 6:	/* dump iova alloc list */
		mtk_iommu_iova_alloc_dump_top(file, NULL);
		ret = mtk_iommu_iova_alloc_dump(file, NULL);
		break;
	case 7:	/* enable/disable trace log */
		for (i = 0; i < IOMMU_EVENT_MAX; i++) {
			event_mgr[i].dump_log = get_debug_config(val, i);
			pr_info("%s event[%s].dump_log:%d\n", __func__,
				event_mgr[i].name, event_mgr[i].dump_log);
		}
		break;
	case 8:	/* enable/disable trace dump */
		for (i = 0; i < IOMMU_EVENT_MAX; i++) {
			event_mgr[i].dump_trace = get_debug_config(val, i);
			pr_info("%s event[%s].dump_trace:%d\n", __func__,
				event_mgr[i].name, event_mgr[i].dump_trace);
		}
		break;
	case 9:	/* enable/disable debug switch config */
		iommu_globals.iova_evt_enable = get_debug_config(val, 0);
		iommu_globals.iova_alloc_list = get_debug_config(val, 1);
		iommu_globals.iova_map_list = get_debug_config(val, 2);
		iommu_globals.iova_warn_aee = get_debug_config(val, 3);
		iommu_globals.iova_stack_trace = get_debug_config(val, 4);
		iommu_globals.iova_alloc_rbtree = get_debug_config(val, 5);

		pr_info("%s evt:%d, alloc:%d, map:%d, warn_aee:%d, stack_trace:%d, rbtree:%d\n",
			__func__,
			iommu_globals.iova_evt_enable,
			iommu_globals.iova_alloc_list,
			iommu_globals.iova_map_list,
			iommu_globals.iova_warn_aee,
			iommu_globals.iova_stack_trace,
			iommu_globals.iova_alloc_rbtree);
		break;
	default:
		pr_info("%s not support index=%u\n", __func__, index);
		break;
	}

	pr_info("%s ret:%d, input:0x%llx, index:%u, val:%llu\n",
		__func__, ret, input, index, val);

	if (tag == (u64)IOMMU_DUMP_TAG_SKIP)
		return ret;
	else
		return 0;
}
#else
static int m4u_debug_set(void *data, u64 input)
{
	return 0;
}
#endif

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_PROC_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_FOPS_RO(name)				\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			pde_data(inode));			\
	}							\
	static const struct proc_ops name = {			\
		.proc_open		= name ## _proc_open,	\
		.proc_read		= seq_read,		\
		.proc_lseek		= seq_lseek,		\
		.proc_release	= single_release,		\
	}

static int mtk_iommu_help_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_debug_help(s);
	return 0;
}

static int mtk_iommu_dump_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_trace_dump(s);
	mtk_iommu_iova_alloc_dump(s, NULL);
	mtk_iommu_iova_alloc_dump_top(s, NULL);

	if (smmu_v3_enable) {
		int i, ret;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			/* skip GPU SMMU */
			if (i == GPU_SMMU)
				continue;

			ret = mtk_smmu_power_get(i);
			iommu_dump(s, "smmu_%d: %s\n", i, get_power_status_str(ret));
			if (ret)
				continue;

			mtk_smmu_wpreg_dump(s, i);
			mtk_smmu_power_put(i);

			/* no need dump all page table raw data */
			mtk_smmu_pgtable_dump(s, i, false);
		}
	}
	return 0;
}

static int mtk_iommu_iova_alloc_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_iova_alloc_dump_top(s, NULL);
	mtk_iommu_iova_alloc_dump(s, NULL);
	return 0;
}

static int mtk_iommu_iova_map_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		mtk_iommu_iova_map_dump(s, 0, MM_SMMU);
		mtk_iommu_iova_map_dump(s, 0, APU_SMMU);
	} else {
		mtk_iommu_iova_map_dump(s, 0, MM_TABLE);
		mtk_iommu_iova_map_dump(s, 0, APU_TABLE);
	}
	return 0;
}

static int mtk_smmu_wp_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		int i, ret;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			ret = mtk_smmu_power_get(i);
			iommu_dump(s, "smmu_%d: %s\n", i, get_power_status_str(ret));
			if (ret)
				continue;

			mtk_smmu_wpreg_dump(s, i);
			mtk_smmu_power_put(i);
		}
	}
	return 0;
}

static int mtk_smmu_pgtable_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		int i;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++)
			mtk_smmu_pgtable_dump(s, i, true);
	}
	return 0;
}

/* adb shell cat /proc/iommu_debug/xxx */
DEFINE_PROC_FOPS_RO(mtk_iommu_help_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_dump_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_alloc_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_map_fops);
DEFINE_PROC_FOPS_RO(mtk_smmu_wp_fops);
DEFINE_PROC_FOPS_RO(mtk_smmu_pgtable_fops);

static void mtk_iommu_trace_init(struct mtk_m4u_data *data)
{
	int total_size = IOMMU_EVENT_COUNT_MAX * sizeof(struct iommu_event_t);

	strncpy(event_mgr[IOMMU_ALLOC].name, "alloc", 10);
	strncpy(event_mgr[IOMMU_FREE].name, "free", 10);
	strncpy(event_mgr[IOMMU_MAP].name, "map", 10);
	strncpy(event_mgr[IOMMU_UNMAP].name, "unmap", 10);
	strncpy(event_mgr[IOMMU_SYNC].name, "sync", 10);
	strncpy(event_mgr[IOMMU_UNSYNC].name, "unsync", 10);
	strncpy(event_mgr[IOMMU_SUSPEND].name, "suspend", 10);
	strncpy(event_mgr[IOMMU_RESUME].name, "resume", 10);
	strncpy(event_mgr[IOMMU_POWER_ON].name, "pwr_on", 10);
	strncpy(event_mgr[IOMMU_POWER_OFF].name, "pwr_off", 10);
	event_mgr[IOMMU_ALLOC].dump_trace = 1;
	event_mgr[IOMMU_FREE].dump_trace = 1;
	event_mgr[IOMMU_SYNC].dump_trace = 1;
	event_mgr[IOMMU_UNSYNC].dump_trace = 1;
	event_mgr[IOMMU_SUSPEND].dump_trace = 1;
	event_mgr[IOMMU_RESUME].dump_trace = 1;
	event_mgr[IOMMU_POWER_ON].dump_trace = 1;
	event_mgr[IOMMU_POWER_OFF].dump_trace = 1;

	event_mgr[IOMMU_SUSPEND].dump_log = 1;
	event_mgr[IOMMU_RESUME].dump_log = 1;
	event_mgr[IOMMU_POWER_ON].dump_log = 0;
	event_mgr[IOMMU_POWER_OFF].dump_log = 0;

	iommu_globals.record = vmalloc(total_size);
	if (!iommu_globals.record) {
		iommu_globals.enable = 0;
		return;
	}

	memset(iommu_globals.record, 0, total_size);
	iommu_globals.enable = 1;
	iommu_globals.dump_enable = 1;
	iommu_globals.write_pointer = 0;
	iommu_globals.iova_evt_enable = 1;
	iommu_globals.iova_warn_aee = 0;
	iommu_globals.iova_stack_trace = 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	iommu_globals.iova_alloc_list = 1;
	iommu_globals.iova_alloc_rbtree = 1;
	iommu_globals.iova_map_list = 0;
#else
	iommu_globals.iova_alloc_list = 0;
	iommu_globals.iova_alloc_rbtree = 0;
	iommu_globals.iova_map_list = 0;
#endif

	spin_lock_init(&iommu_globals.lock);
}

static void mtk_iommu_trace_rec_write(int event,
	unsigned long data1, unsigned long data2,
	unsigned long data3, struct device *dev)
{
	unsigned int index;
	struct iommu_event_t *p_event = NULL;
	unsigned long flags;

	if (iommu_globals.enable == 0)
		return;
	if ((event >= IOMMU_EVENT_MAX) ||
	    (event < 0))
		return;

	if (event_mgr[event].dump_log) {
		if (smmu_v3_enable)
			pr_info("[trace] %5s |0x%-9lx |%9lx |0x%x |0x%-4x |%s\n",
				event_mgr[event].name,
				data1,
				data2,
				smmu_tab_id_to_smmu_id(data3),
				smmu_tab_id_to_asid(data3),
				(dev != NULL ? dev_name(dev) : ""));
		else
			pr_info("[trace] %5s |0x%-9lx |%9lx |0x%lx |%s\n",
				event_mgr[event].name,
				data1, data2, data3,
				(dev != NULL ? dev_name(dev) : ""));
	}

	if (event_mgr[event].dump_trace == 0)
		return;

	index = (atomic_inc_return((atomic_t *)
			&(iommu_globals.write_pointer)) - 1)
	    % IOMMU_EVENT_COUNT_MAX;

	spin_lock_irqsave(&iommu_globals.lock, flags);

	p_event = (struct iommu_event_t *)
		&(iommu_globals.record[index]);
	mtk_iommu_system_time(&(p_event->time_high), &(p_event->time_low));
	p_event->event_id = event;
	p_event->data1 = data1;
	p_event->data2 = data2;
	p_event->data3 = data3;
	p_event->dev = dev;

	spin_unlock_irqrestore(&iommu_globals.lock, flags);
}

static void mtk_iommu_iova_trace(int event,
	dma_addr_t iova, size_t size,
	u64 tab_id, struct device *dev)
{
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
	u32 id = (iova >> 32);

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	mtk_iommu_trace_rec_write(event, (unsigned long) iova, size, tab_id, dev);
#endif
}

void mtk_iommu_tlb_sync_trace(u64 iova, size_t size, int iommu_ids)
{
	mtk_iommu_trace_rec_write(IOMMU_SYNC, (unsigned long) iova, size,
				  (unsigned long) iommu_ids, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iommu_tlb_sync_trace);

void mtk_iommu_pm_trace(int event, int iommu_id, int pd_sta,
	unsigned long flags, struct device *dev)
{
	mtk_iommu_trace_rec_write(event, (unsigned long) pd_sta, flags,
				  (unsigned long) iommu_id, dev);
}
EXPORT_SYMBOL_GPL(mtk_iommu_pm_trace);

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
static int mtk_iommu_mrdump_show(struct seq_file *s, void *unused)
{
	mtk_iommu_trace_dump(s);
	mtk_iommu_iova_alloc_dump(s, NULL);
	return 0;
}

static void mtk_iommu_mrdump(const char *fmt, ...)
{
	unsigned long len;
	va_list ap;

	if (!iommu_mrdump_buffer)
		return;

	if ((iommu_mrdump_size + MAX_STRING_SIZE) >= (unsigned long)MAX_IOMMU_MRDUMP_SIZE)
		return;

	va_start(ap, fmt);
	len = vscnprintf(&iommu_mrdump_buffer[iommu_mrdump_size],
			 MAX_STRING_SIZE, fmt, ap);
	va_end(ap);
	iommu_mrdump_size += len;
}

void get_iommu_mrdump_buffer(unsigned long *vaddr, unsigned long *size)
{
	if (!iommu_mrdump_buffer) {
		pr_info("%s iommu_mrdump_buffer is NULL\n", __func__);
		return;
	}

	iommu_mrdump_size = 0;
	memset(iommu_mrdump_buffer, 0, MAX_IOMMU_MRDUMP_SIZE);

	mtk_iommu_mrdump_show(IOMMU_DUMP_TAG_MRDUMP, NULL);
	*vaddr = (unsigned long)iommu_mrdump_buffer;
	*size = iommu_mrdump_size;
}
#endif

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu_debug", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_err("failed to create debug dir\n");

	debug_file = proc_create_data("debug",
		S_IFREG | 0640, data->debug_root, &m4u_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to create debug file\n");

	debug_file = proc_create_data("help",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_help_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create help file\n");

	debug_file = proc_create_data("iommu_dump",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_dump_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iommu_dump file\n");

	debug_file = proc_create_data("iova_alloc",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_iova_alloc_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_alloc file\n");

	debug_file = proc_create_data("iova_map",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_iova_map_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_map file\n");

	if (smmu_v3_enable) {
		debug_file = proc_create_data("smmu_wp",
			S_IFREG | 0640, data->debug_root, &mtk_smmu_wp_fops, NULL);
		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to proc_create smmu_wp file\n");

		debug_file = proc_create_data("smmu_pgtable",
			S_IFREG | 0640, data->debug_root, &mtk_smmu_pgtable_fops, NULL);
		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to proc_create smmu_pgtable file\n");
	}

	mtk_iommu_trace_init(data);

	spin_lock_init(&iova_list.lock);
	INIT_LIST_HEAD(&iova_list.head);

	spin_lock_init(&map_list.lock);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE0]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE1]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE2]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE3]);

	spin_lock_init(&count_list.lock);
	INIT_LIST_HEAD(&count_list.head);

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	iommu_mrdump_buffer = kzalloc(MAX_IOMMU_MRDUMP_SIZE, GFP_KERNEL);
	if (iommu_mrdump_buffer) {
		iommu_mrdump_proc = mtk_iommu_mrdump;
		mrdump_set_extra_dump(AEE_EXTRA_FILE_IOMMU, get_iommu_mrdump_buffer);
	}
#endif

	return 0;
}

static int iova_size_cmp(void *priv, const struct list_head *a,
	const struct list_head *b)
{
	struct iova_count_info *ia, *ib;

	ia = list_entry(a, struct iova_count_info, list_node);
	ib = list_entry(b, struct iova_count_info, list_node);

	if (ia->size < ib->size)
		return 1;
	if (ia->size > ib->size)
		return -1;

	return 0;
}

static void mtk_iommu_clear_iova_size(void)
{
	struct iova_count_info *plist;
	struct iova_count_info *tmp_plist;

	list_for_each_entry_safe(plist, tmp_plist, &count_list.head, list_node) {
		list_del(&plist->list_node);
		kfree(plist);
	}
}

static void mtk_iommu_count_iova_size(
	struct device *dev, dma_addr_t iova, size_t size)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_count_info *plist = NULL;
	struct iova_count_info *n = NULL;
	struct iova_count_info *new_info;
	u64 tab_id;
	u32 dom_id;

	fwspec = dev_iommu_fwspec_get(dev);
	if (fwspec == NULL) {
		pr_notice("%s fail! dev:%s, fwspec is NULL\n",
			  __func__, dev_name(dev));
		return;
	}

	/* Add to iova_count_info if exist */
	spin_lock(&count_list.lock);
	list_for_each_entry_safe(plist, n, &count_list.head, list_node) {
		if (plist->dev == dev) {
			plist->count++;
			plist->size += (unsigned long) (size / 1024);
			spin_unlock(&count_list.lock);
			return;
		}
	}

	/* Create new iova_count_info if no exist */
	new_info = kzalloc(sizeof(*new_info), GFP_ATOMIC);
	if (!new_info) {
		spin_unlock(&count_list.lock);
		pr_notice("%s, alloc iova_count_info fail! dev:%s\n",
			  __func__, dev_name(dev));
		return;
	}

	if (smmu_v3_enable) {
		tab_id = get_smmu_tab_id(dev);
		dom_id = 0;
	} else {
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	new_info->tab_id = tab_id;
	new_info->dom_id = dom_id;
	new_info->dev = dev;
	new_info->size = (unsigned long) (size / 1024);
	new_info->count = 1;
	list_add_tail(&new_info->list_node, &count_list.head);
	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_dump_top(
	struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	struct iova_count_info *p_count_list = NULL;
	struct iova_count_info *n_count = NULL;
	int total_cnt = 0, dom_count = 0, i = 0;
	u64 size = 0, total_size = 0, dom_size = 0;
	int smmu_id = -1, stream_id = -1;
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (iommu_globals.iova_evt_enable == 0)
		return;

	if (iommu_globals.iova_alloc_list == 0) {
		iommu_dump(s, "iova alloc count:%llu\n", iova_list.count);
		return;
	}

	/* check fwspec by device */
	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_notice("%s fail! dev:%s, fwspec is NULL\n",
				  __func__, dev_name(dev));
			return;
		}
		if (smmu_v3_enable) {
			smmu_id = get_smmu_id(dev);
			stream_id = get_smmu_stream_id(dev);
			tab_id = get_smmu_tab_id(dev);
			dom_id = 0;
		} else {
			tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
			dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		}
	}

	/* count iova size by device */
	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node) {
		size = (unsigned long) (plist->size / 1024);
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			mtk_iommu_count_iova_size(plist->dev, plist->iova, plist->size);
			dom_size += size;
			dom_count++;
		}
		total_size += size;
		total_cnt++;
	}
	spin_unlock(&iova_list.lock);

	spin_lock(&count_list.lock);
	/* sort count iova size by device */
	list_sort(NULL, &count_list.head, iova_size_cmp);

	/* dump top max user */
	if (smmu_v3_enable) {
		iommu_dump(s,
			   "smmu iova alloc total:(%d/%lluKB), dom:(%d/%lluKB,%d,%d,%d) top %d user:\n",
			   total_cnt, total_size, dom_count, dom_size,
			   smmu_id, stream_id, dom_id, IOVA_DUMP_TOP_MAX);
		iommu_dump(s, "%-8s %-10s %-10s %-7s %-10s %-16s %s\n",
			   "smmu_id", "stream_id", "asid", "dom_id", "count",
			   "size(KB)", "dev");
	} else {
		iommu_dump(s,
			   "iommu iova alloc total:(%d/%lluKB), dom:(%d/%lluKB,%llu,%d) top %d user:\n",
			   total_cnt, total_size, dom_count, dom_size,
			   tab_id, dom_id, IOVA_DUMP_TOP_MAX);
		iommu_dump(s, "%6s %6s %8s %10s %3s\n",
			   "tab_id", "dom_id", "count", "size(KB)", "dev");
	}

	list_for_each_entry_safe(p_count_list, n_count, &count_list.head, list_node) {
		if (smmu_v3_enable) {
			iommu_dump(s, "%-8u 0x%-8x 0x%-8x %-7u %-10u %-16llu %s\n",
				   smmu_tab_id_to_smmu_id(p_count_list->tab_id),
				   get_smmu_stream_id(p_count_list->dev),
				   smmu_tab_id_to_asid(p_count_list->tab_id),
				   p_count_list->dom_id,
				   p_count_list->count,
				   p_count_list->size,
				   dev_name(p_count_list->dev));
		} else {
			iommu_dump(s, "%6llu %6u %8u %10llu %s\n",
				   p_count_list->tab_id,
				   p_count_list->dom_id,
				   p_count_list->count,
				   p_count_list->size,
				   dev_name(p_count_list->dev));
		}
		i++;
		if (i >= IOVA_DUMP_TOP_MAX)
			break;
	}

	/* clear count iova size */
	mtk_iommu_clear_iova_size();

	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_title_dump(struct seq_file *s)
{
	if (smmu_v3_enable)
		iommu_dump(s, "%-9s %-10s %-10s %-18s %-14s %17s %-25s %-9s %s\n",
			   "smmu_id", "stream_id", "asid", "iova", "size",
			   "time", "dev", "in_rbtree", iommu_globals.iova_stack_trace == 1 ?
			   "alloc_trace" : "");
	else
		iommu_dump(s, "%6s %6s %-18s %-10s %17s %-25s %s\n",
			   "tab_id", "dom_id", "iova", "size", "time", "dev", "in_rbtree");
}

static void mtk_iommu_iova_alloc_info_dump(struct seq_file *s, struct iova_info *plist)
{
	if (smmu_v3_enable)
		iommu_dump(s, "%-9u 0x%-8x 0x%-8x %-18pa 0x%-12zx %10llu.%06u %-25s %-9d %s\n",
			   smmu_tab_id_to_smmu_id(plist->tab_id),
			   get_smmu_stream_id(plist->dev),
			   smmu_tab_id_to_asid(plist->tab_id),
			   &plist->iova,
			   plist->size,
			   plist->time_high,
			   plist->time_low,
			   dev_name(plist->dev),
			   plist->in_rbtree,
			   plist->trace_info ?
			   plist->trace_info : "");
	else
		iommu_dump(s, "%6llu %6u %-18pa 0x%-8zx %10llu.%06u %-25s %d\n",
			   plist->tab_id,
			   plist->dom_id,
			   &plist->iova,
			   plist->size,
			   plist->time_high,
			   plist->time_low,
			   dev_name(plist->dev),
			   plist->in_rbtree);
}

static int mtk_iommu_iova_alloc_dump_rbtree(struct seq_file *s, struct device *dev,
					     u64 tab_id, u32 dom_id)
{
	struct rb_root *root = &iova_list.iova_rb_root;
	struct iova_info *plist = NULL;
	struct rb_node *tmp_rb;
	int dump_count = 0;

	iommu_dump(s, "iova alloc dump in rbtree:\n");
	mtk_iommu_iova_alloc_title_dump(s);
	for (tmp_rb = rb_first(root); tmp_rb; tmp_rb = rb_next(tmp_rb)) {
		plist = rb_entry(tmp_rb, struct iova_info, iova_rb_node);
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			mtk_iommu_iova_alloc_info_dump(s, plist);
			dump_count++;
			if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
				break;
		}
	}
	return dump_count;
}

static int mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	int dump_count = 0;
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (iommu_globals.iova_evt_enable == 0 || iommu_globals.iova_alloc_list == 0)
		return 0;

	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_info("%s fail! dev:%s, fwspec is NULL\n",
				__func__, dev_name(dev));
			return -EINVAL;
		}
		if (smmu_v3_enable) {
			tab_id = get_smmu_tab_id(dev);
			dom_id = 0;
		} else {
			tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
			dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		}
	}

	iommu_dump(s, "iova alloc dump:\n");
	if (iommu_globals.iova_alloc_rbtree == 1)
		iommu_dump(s, "total_cnt:%llu list_only_cnt:%llu rbtree_cnt:%llu\n",
			   iova_list.count, iova_list.list_only_count,
			   (iova_list.count - iova_list.list_only_count));
	mtk_iommu_iova_alloc_title_dump(s);

	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node)
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			mtk_iommu_iova_alloc_info_dump(s, plist);
			dump_count++;
			if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
				break;
		}

	if (iommu_globals.iova_alloc_rbtree == 1)
		mtk_iommu_iova_alloc_dump_rbtree(s, dev, tab_id, dom_id);
	spin_unlock(&iova_list.lock);
	return dump_count;
}

/* For smmu, tab_id is smmu hardware id */
static int mtk_iommu_iova_dump(struct seq_file *s, u64 iova, u64 tab_id)
{
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	int dump_count = 0;

	if (iommu_globals.iova_evt_enable == 0 || iommu_globals.iova_alloc_list == 0)
		return 0;

	if (!iova || tab_id > MTK_M4U_TAB_NR_MAX) {
		pr_info("%s fail, invalid iova:0x%llx tab_id:%llu\n",
			__func__, iova, tab_id);
		return -EINVAL;
	}

	iommu_dump(s, "iova dump:\n");
	mtk_iommu_iova_alloc_title_dump(s);

	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node)
		if (to_smmu_hw_id(plist->tab_id) == tab_id &&
			    iova <= (plist->iova + plist->size) &&
			    iova >= (plist->iova)) {
			mtk_iommu_iova_alloc_info_dump(s, plist);
			dump_count++;
			if (s == NULL && dump_count >= IOVA_DUMP_LOG_MAX)
				break;
		}
	spin_unlock(&iova_list.lock);
	return dump_count;
}

#if IS_ENABLED(CONFIG_STACKTRACE)
static char *generate_trace_info(const unsigned long *entries,
				 unsigned int nr_entries)
{
	char *trace_info, *plus_str;
	char ent_str[SMMU_STACK_LINE_MAX_LEN] = {0};
	size_t len, size = SMMU_STACK_MAX_LEN;
	int i;

	if (!entries || !nr_entries)
		return NULL;

	trace_info = kcalloc(SMMU_STACK_MAX_LEN, sizeof(char),
			     GFP_ATOMIC);
	if (!trace_info)
		return NULL;

	for (i = nr_entries - 1; i >= 0; i--) {
		if (snprintf(ent_str, SMMU_STACK_LINE_MAX_LEN, "%pS",
		    (void *)entries[i]) < 0)
			continue;

		plus_str = strchr(ent_str, '+');

		if (plus_str != NULL)
			len = plus_str - ent_str;
		else
			len = strlen(ent_str);

		if (i != nr_entries - 1) {
			if (len + 4 < len || size <= len + 4)
				break;

			strncat(trace_info, " -> ", 4);
			size -= 4;
		} else if (size <= len) {
			break;
		}

		strncat(trace_info, ent_str, len);
		size -= len;
	}

	return trace_info;
}

void mtk_iova_add_trace_info(struct iova_info *iova_buf)
{
	unsigned long stack_entries[SMMU_STACK_DEPTH];
	unsigned int nr_entries;

	if (!smmu_v3_enable || !iova_buf)
		return;

	nr_entries = stack_trace_save(stack_entries, ARRAY_SIZE(stack_entries),
				      SMMU_STACK_SKIPNR);
	iova_buf->trace_info = generate_trace_info(stack_entries, nr_entries);
}

static void mtk_iova_del_trace_info(struct iova_info *iova_buf)
{
	if (smmu_v3_enable && iova_buf && iova_buf->trace_info) {
		kfree(iova_buf->trace_info);
		iova_buf->trace_info = NULL;
	}
}
#endif

/*
 * For ktf test case alloc_iova_in_rbtree
 */
int __maybe_unused test_check_iova_count_in_rbtree(void)
{
	struct rb_root *root = &iova_list.iova_rb_root;
	struct rb_node *tmp_rb;
	u64 rbtree_count = 0;
	int ret = -1;

	spin_lock(&iova_list.lock);
	for (tmp_rb = rb_first(root); tmp_rb; tmp_rb = rb_next(tmp_rb))
		rbtree_count++;

	if (rbtree_count + iova_list.list_only_count == iova_list.count)
		ret = 0;
	else
		pr_info("%s iova_count:%llu, list_only_count:%llu rbtree_count:%llu\n",
			__func__, iova_list.count, iova_list.list_only_count, rbtree_count);
	spin_unlock(&iova_list.lock);
	return ret;
}

static int mtk_iova_rbtree_compare(struct iova_domain *iovad,
				   dma_addr_t iova, struct iova_info *entry_p)
{
	if (iovad < entry_p->iovad)
		return -1;
	if (iovad > entry_p->iovad)
		return 1;
	if (iova < entry_p->iova)
		return -1;
	if (iova > entry_p->iova)
		return 1;
	return 0;
}

static struct iova_info *mtk_iova_rbtree_find_or_add(struct iova_domain *iovad,
						     dma_addr_t iova,
						     struct rb_node *node)
{
	struct rb_root *root = &iova_list.iova_rb_root;
	struct iova_info *entry_p = NULL;
	struct rb_node **link = NULL, *parent = NULL;
	int ret = 0;

	link = &root->rb_node;
	while (*link) {
		parent = *link;
		entry_p = rb_entry(parent, struct iova_info, iova_rb_node);
		ret = mtk_iova_rbtree_compare(iovad, iova, entry_p);
		if (ret > 0)
			link = &parent->rb_right;
		else if (ret < 0)
			link = &parent->rb_left;
		else
			return entry_p;
	}

	if (node == NULL)
		return NULL;

	rb_link_node(node, parent, link);
	rb_insert_color(node, root);

	return NULL;
}

static struct iova_info *mtk_iova_rbtree_find(struct iova_domain *iovad,
					      dma_addr_t iova)
{
	return mtk_iova_rbtree_find_or_add(iovad, iova, NULL);
}

static void mtk_iova_rbtree_add(struct iova_domain *iovad,
				dma_addr_t iova,
				struct rb_node *node)
{
	mtk_iova_rbtree_find_or_add(iovad, iova, node);
}

static struct iova_info *mtk_iova_rbtree_del(struct iova_domain *iovad,
					     dma_addr_t iova)
{
	struct iova_info *entry = NULL;

	if (rb_first(&iova_list.iova_rb_root) != NULL) {
		entry = mtk_iova_rbtree_find(iovad, iova);
		if (entry)
			rb_erase(&entry->iova_rb_node, &iova_list.iova_rb_root);
	}
	return entry;
}

static void mtk_iova_add(struct iova_info *entry_n)
{
	list_add(&entry_n->list_node, &iova_list.head);

	if (iommu_globals.iova_alloc_rbtree == 1) {
		if (iova_list.list_only_count == IOVA_ENABLE_RBTREE_COUNT) {
			mtk_iova_rbtree_add(entry_n->iovad, entry_n->iova, &entry_n->iova_rb_node);
			entry_n->in_rbtree = true;
		} else {
			if (iova_list.list_only_count < ULLONG_MAX)
				iova_list.list_only_count += 1;
			else
				pr_info_ratelimited("%s, list only count overflow\n", __func__);
		}
	}
}

static struct iova_info *mtk_iova_del(struct iova_domain *iovad, dma_addr_t iova)
{
	struct iova_info *plist = NULL, *tmp_plist = NULL;

	if (iommu_globals.iova_alloc_rbtree == 1) {
		plist = mtk_iova_rbtree_del(iovad, iova);
		if (plist) {
			list_del(&plist->list_node);
			return plist;
		}
	}

	list_for_each_entry_safe(plist, tmp_plist, &iova_list.head, list_node) {
		if (plist->in_rbtree == false &&
		    plist->iova == iova &&
		    plist->iovad == iovad) {
			list_del(&plist->list_node);
			if (iova_list.list_only_count > 0)
				iova_list.list_only_count -= 1;
			else
				pr_info_ratelimited("%s, list only count underflow\n", __func__);

			return plist;
		}
	}

	return NULL;
}

static void mtk_iova_dbg_alloc(struct device *dev,
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	static DEFINE_RATELIMIT_STATE(dump_rs, IOVA_DUMP_RS_INTERVAL,
				      IOVA_DUMP_RS_BURST);
	struct iova_info *iova_buf;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (iommu_globals.iova_evt_enable == 0)
		return;

	if (!fwspec) {
		pr_info("%s fail, dev(%s) is not iommu-dev\n",
			__func__, dev_name(dev));
		return;
	}

	if (smmu_v3_enable) {
		tab_id = get_smmu_tab_id(dev);
		dom_id = 0;
	} else {
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	if (!iova) {
		pr_info("%s fail! dev:%s, size:0x%zx, tab_id:0x%llx, dom_id:%d, count:%llu\n",
			__func__, dev_name(dev), size, tab_id, dom_id, iova_list.count);

		if (!__ratelimit(&dump_rs))
			return;

		if (dom_id > 0 || smmu_v3_enable)
			mtk_iommu_iova_alloc_dump(NULL, dev);

		mtk_iommu_iova_alloc_dump_top(NULL, dev);

		if (iommu_globals.iova_warn_aee == 1) {
			m4u_aee_print(iova_warn_log_format,
				      dev_name(dev), dev_name(dev),
				      tab_id, dom_id, iova_list.count);
		}
		return;
	}

	if (iommu_globals.iova_alloc_list == 0) {
		spin_lock(&iova_list.lock);
		mtk_iova_count_inc();
		spin_unlock(&iova_list.lock);
		goto iova_trace;
	}

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->dom_id = dom_id;
	iova_buf->dev = dev;
	iova_buf->iovad = iovad;
	iova_buf->iova = iova;
	iova_buf->size = size;
#if IS_ENABLED(CONFIG_STACKTRACE)
	if (iommu_globals.iova_stack_trace == 1)
		mtk_iova_add_trace_info(iova_buf);
#endif
	spin_lock(&iova_list.lock);
	mtk_iova_count_inc();
	mtk_iova_add(iova_buf);
	spin_unlock(&iova_list.lock);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_ALLOC, iova, size, tab_id, dev);
	mtk_iova_count_check(dev, iova, size);
}

static void mtk_iova_dbg_free(
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	u64 start_t, end_t;
	struct iova_info *plist;
	struct device *dev = NULL;
	u64 tab_id = 0;

	if (iommu_globals.iova_evt_enable == 0)
		return;

	if (iommu_globals.iova_alloc_list == 0) {
		spin_lock(&iova_list.lock);
		mtk_iova_count_dec();
		spin_unlock(&iova_list.lock);
		goto iova_trace;
	}

	spin_lock(&iova_list.lock);
	start_t = sched_clock();
	plist = mtk_iova_del(iovad, iova);
	if (plist) {
		tab_id = plist->tab_id;
		dev = plist->dev;
#if IS_ENABLED(CONFIG_STACKTRACE)
		if (iommu_globals.iova_stack_trace == 1)
			mtk_iova_del_trace_info(plist);
#endif
		kfree(plist);
		mtk_iova_count_dec();
	}
	end_t = sched_clock();
	spin_unlock(&iova_list.lock);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info_ratelimited("%s, dev:%s, find iova:[0x%llx 0x%llx 0x%zx] %llu time:%llu\n",
				    __func__, (dev ? dev_name(dev) : "NULL"),
				    tab_id, (unsigned long long)iova, size,
				    iova_list.count, (end_t - start_t));

	if (dev == NULL)
		pr_info("%s warnning, iova:[0x%llx 0x%zx] not find in %llu\n",
			__func__, (unsigned long long)iova, size, iova_list.count);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_FREE, iova, size, tab_id, dev);
}

/* all code inside alloc_iova_hook can't be scheduled! */
static void alloc_iova_hook(void *data,
	struct device *dev, struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_alloc(dev, iovad, iova, size);
}

/* all code inside free_iova_hook can't be scheduled! */
static void free_iova_hook(void *data,
	struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_free(iovad, iova, size);
}

static unsigned long limit_align_shift(struct iova_domain *iovad, unsigned long shift)
{
	unsigned long max_align_shift;

	max_align_shift = iommu_max_align_shift + PAGE_SHIFT - iova_shift(iovad);
	return min_t(unsigned long, max_align_shift, shift);
}

static void limit_align_hook(void __always_unused *data, struct iova_domain *iovad,
			     unsigned long size, unsigned long *shift)
{
	*shift = limit_align_shift(iovad, *shift);
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 total_port;
	int ret = 0;

	smmu_v3_enable = smmu_v3_enabled();
	pr_info("%s start, smmu_v3_enable:%d\n", __func__, smmu_v3_enable);

	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	total_port = m4u_data->plat_data->port_nr[MM_IOMMU] +
		     m4u_data->plat_data->port_nr[APU_IOMMU] +
		     m4u_data->plat_data->port_nr[PERI_IOMMU];
	m4u_data->m4u_cb = devm_kzalloc(dev, total_port *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);

	ret = register_trace_android_vh_iommu_iovad_alloc_iova(alloc_iova_hook,
							       "mtk_m4u_dbg_probe");
	pr_debug("add alloc iova hook %s\n", (ret ? "fail" : "pass"));
	ret = register_trace_android_vh_iommu_iovad_free_iova(free_iova_hook,
							      "mtk_m4u_dbg_probe");
	pr_debug("add free iova hook %s\n", (ret ? "fail" : "pass"));

	ret = register_trace_android_rvh_iommu_limit_align_shift(limit_align_hook,
								 "mtk_m4u_dbg_probe");
	pr_debug("add limit align shift hook %s\n", (ret ? "fail" : "pass"));

	return 0;
}

static const struct mau_config_info mau_config_default[] = {
	/* Monitor each IOMMU input IOVA<4K and output PA=0 */
	MAU_CONFIG_INIT(0, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(0, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
};

static int mt6855_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6879_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 9), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6886_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6897_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6983_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 10), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);

}

static int mt6985_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int default_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static u32 default_get_valid_tf_id(int tf_id, u32 type, int id)
{
	u32 vld_id = 0;

	if (type == APU_SMMU)
		vld_id = FIELD_GET(GENMASK(12, 8), tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	return vld_id;
}

static u32 default_iommu_get_valid_tf_id(int tf_id, u32 type, int id)
{
	u32 vld_id = 0;

	if (type == APU_IOMMU)
		vld_id = FIELD_GET(GENMASK(11, 8), tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	return vld_id;
}

static bool mt6989_tf_id_is_match(int tf_id, u32 type, int id,
				  struct mtk_iommu_port port)
{
	int vld_id = -1;

	if (port.id != id)
		return false;

	if (port.port_type == SPECIAL)
		vld_id = tf_id & F_MMU_INT_TF_SPEC_MSK(port.port_start);
	else if (port.port_type == NORMAL)
		vld_id = default_get_valid_tf_id(tf_id, type, id);

	if (port.tf_id == vld_id)
		return true;

	return false;
}

static const u32 default_smmu_common_ids[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX] = {
	[MM_SMMU] = {
		MM_SMMU_MDP,
		MM_SMMU_MDP,
		MM_SMMU_DISP,
		MM_SMMU_DISP,
	},
	[APU_SMMU] = {
		APU_SMMU_M0,
		APU_SMMU_M0,
		APU_SMMU_M0,
		APU_SMMU_M0,
	},
	[SOC_SMMU] = {
		SOC_SMMU_M4,
		SOC_SMMU_M6,
		SOC_SMMU_M7,
	},
};

static int default_smmu_common_id(u32 smmu_type, u32 tbu_id)
{
	if (smmu_type >= SMMU_TYPE_NUM || tbu_id >= SMMU_TBU_CNT(smmu_type))
		return -1;

	return default_smmu_common_ids[smmu_type][tbu_id];
}

static char *mt6989_smmu_soc_port_name(u32 type, int id, int tf_id)
{
	if (type != SOC_SMMU) {
		pr_info("%s is not support type:%u\n", __func__, type);
		return NULL;
	}

	switch (id) {
	case SOC_SMMU_M4:
		return mt6989_soc_m4_port_name(tf_id);
	case SOC_SMMU_M6:
		return mt6989_soc_m6_port_name(tf_id);
	case SOC_SMMU_M7:
		return mt6989_soc_m7_port_name(tf_id);
	default:
		return "SOC_UNKNOWN";
	}
}

static char *mt6991_smmu_soc_port_name(u32 type, int id, int tf_id)
{
	if (type != SOC_SMMU) {
		pr_info("%s is not support type:%u\n", __func__, type);
		return NULL;
	}

	switch (id) {
	case SOC_SMMU_M4:
		return mt6991_soc_m4_port_name(tf_id);
	case SOC_SMMU_M6:
		return mt6991_soc_m6_port_name(tf_id);
	case SOC_SMMU_M7:
		return mt6991_soc_m7_port_name(tf_id);
	default:
		return "SOC_UNKNOWN";
	}
}

static const struct mtk_m4u_plat_data mt6761_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6761,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6761),
};

static const struct mtk_m4u_plat_data mt6765_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6765,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6765),
};

static const struct mtk_m4u_plat_data mt6768_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6768,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6768),
};

static const struct mtk_m4u_plat_data mt6781_data = {
	.port_list[MM_IOMMU] = mm_port_mt6781,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6781),
	.port_list[APU_IOMMU] = apu_port_mt6781,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6781),
	.mm_tf_ccu_support   = 1,
};

static const struct mtk_m4u_plat_data mt6833_data = {
	.port_list[MM_IOMMU] = mm_port_mt6833,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6833),
	.mm_tf_ccu_support   = 1,
};

static const struct mtk_m4u_plat_data mt6853_data = {
	.port_list[MM_IOMMU] = mm_port_mt6853,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6853),
	.port_list[APU_IOMMU] = apu_port_mt6853,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6853),
	.mm_tf_ccu_support = 1,
};

static const struct mtk_m4u_plat_data mt6855_data = {
	.port_list[MM_IOMMU] = mm_port_mt6855,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6855),
	.mm_tf_is_gce_videoup = mt6855_tf_is_gce_videoup,
	.mm_tf_ccu_support = 0,
};

static const struct mtk_m4u_plat_data mt6877_data = {
	.port_list[MM_IOMMU] = iommu_port_mt6877,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(iommu_port_mt6877),
	.port_list[APU_IOMMU] = apu_port_mt6877,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6877),
};

static const struct mtk_m4u_plat_data mt6983_data = {
	.port_list[MM_IOMMU] = mm_port_mt6983,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6983),
	.port_list[APU_IOMMU] = apu_port_mt6983,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6983),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6879_data = {
	.port_list[MM_IOMMU] = mm_port_mt6879,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6879),
	.port_list[APU_IOMMU] = apu_port_mt6879,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6879),
	.port_list[PERI_IOMMU] = peri_port_mt6879,
	.port_nr[PERI_IOMMU]   = ARRAY_SIZE(peri_port_mt6879),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6879_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6886_data = {
	.port_list[MM_IOMMU] = mm_port_mt6886,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6886),
	.port_list[APU_IOMMU] = apu_port_mt6886,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6886),
	.mm_tf_is_gce_videoup = mt6886_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6893_data = {
	.port_list[MM_IOMMU] = mm_port_mt6893,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6893),
	.port_list[APU_IOMMU] = apu_port_mt6893,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6893),
	.mm_tf_ccu_support = 1,
};

static const struct mtk_m4u_plat_data mt6895_data = {
	.port_list[MM_IOMMU] = mm_port_mt6895,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6895),
	.port_list[APU_IOMMU] = apu_port_mt6895,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6895),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6897_data = {
	.port_list[MM_IOMMU] = mm_port_mt6897,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6897),
	.port_list[APU_IOMMU] = apu_port_mt6897,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6897),
	.mm_tf_is_gce_videoup = mt6897_tf_is_gce_videoup,
	.get_valid_tf_id = default_iommu_get_valid_tf_id,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6899_data = {
	.port_list[MM_IOMMU] = mm_port_mt6899,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6899),
	.port_list[APU_IOMMU] = apu_port_mt6899,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6899),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = default_tf_is_gce_videoup,
	.get_valid_tf_id = default_iommu_get_valid_tf_id,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6985_data = {
	.port_list[MM_IOMMU] = mm_port_mt6985,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6985),
	.port_list[APU_IOMMU] = apu_port_mt6985,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6985),
	.mm_tf_is_gce_videoup = mt6985_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6989_smmu_data = {
	.port_list[MM_SMMU] = mm_port_mt6989,
	.port_nr[MM_SMMU]   = ARRAY_SIZE(mm_port_mt6989),
	.port_list[APU_SMMU] = apu_port_mt6989,
	.port_nr[APU_SMMU]   = ARRAY_SIZE(apu_port_mt6989),
	.get_valid_tf_id = default_get_valid_tf_id,
	.tf_id_is_match = mt6989_tf_id_is_match,
	.mm_tf_is_gce_videoup = default_tf_is_gce_videoup,
	.smmu_common_id = default_smmu_common_id,
	.smmu_port_name = mt6989_smmu_soc_port_name,
};

static const struct mtk_m4u_plat_data mt6991_smmu_data = {
	.port_list[MM_SMMU] = mm_port_mt6991,
	.port_nr[MM_SMMU]   = ARRAY_SIZE(mm_port_mt6991),
	.port_list[APU_SMMU] = apu_port_mt6991,
	.port_nr[APU_SMMU]   = ARRAY_SIZE(apu_port_mt6991),
	.mm_tf_ccu_support = 1,
	.get_valid_tf_id = default_get_valid_tf_id,
	.mm_tf_is_gce_videoup = default_tf_is_gce_videoup,
	.smmu_common_id = default_smmu_common_id,
	.smmu_port_name = mt6991_smmu_soc_port_name,
};

static const struct of_device_id mtk_m4u_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6761-iommu-debug", .data = &mt6761_data},
	{ .compatible = "mediatek,mt6765-iommu-debug", .data = &mt6765_data},
	{ .compatible = "mediatek,mt6768-iommu-debug", .data = &mt6768_data},
	{ .compatible = "mediatek,mt6781-iommu-debug", .data = &mt6781_data},
	{ .compatible = "mediatek,mt6833-iommu-debug", .data = &mt6833_data},
	{ .compatible = "mediatek,mt6853-iommu-debug", .data = &mt6853_data},
	{ .compatible = "mediatek,mt6855-iommu-debug", .data = &mt6855_data},
	{ .compatible = "mediatek,mt6877-iommu-debug", .data = &mt6877_data},
	{ .compatible = "mediatek,mt6879-iommu-debug", .data = &mt6879_data},
	{ .compatible = "mediatek,mt6886-iommu-debug", .data = &mt6886_data},
	{ .compatible = "mediatek,mt6893-iommu-debug", .data = &mt6893_data},
	{ .compatible = "mediatek,mt6895-iommu-debug", .data = &mt6895_data},
	{ .compatible = "mediatek,mt6897-iommu-debug", .data = &mt6897_data},
	{ .compatible = "mediatek,mt6899-iommu-debug", .data = &mt6899_data},
	{ .compatible = "mediatek,mt6983-iommu-debug", .data = &mt6983_data},
	{ .compatible = "mediatek,mt6985-iommu-debug", .data = &mt6985_data},
	{ .compatible = "mediatek,mt6989-smmu-debug", .data = &mt6989_smmu_data},
	{ .compatible = "mediatek,mt6991-smmu-debug", .data = &mt6991_smmu_data},
	{},
};

static struct platform_driver mtk_m4u_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "mtk-m4u-debug",
		.of_match_table = of_match_ptr(mtk_m4u_dbg_of_ids),
	}
};

#if IS_BUILTIN(CONFIG_MTK_IOMMU_MISC_DBG)
static int __init mtk_m4u_dbg_init(void)
{
	return platform_driver_register(&mtk_m4u_dbg_drv);
}
fs_initcall(mtk_m4u_dbg_init);
#else
module_platform_driver(mtk_m4u_dbg_drv);
#endif
MODULE_LICENSE("GPL v2");
