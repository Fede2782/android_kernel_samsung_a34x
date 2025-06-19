/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IOMMU API for ARM architected SMMUv3 implementations.
 *
 * Copyright (C) 2015 ARM Limited
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _ARM_SMMU_V3_H
#define _ARM_SMMU_V3_H

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
#include <linux/io-pgtable.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/sizes.h>

#include "arm-smmu-v3-regs.h"

/* ARM_SMMU_IDR3 */
#define IDR3_MPAM			(1 << 7)

/* SMMU MPAM */
#define ARM_SMMU_MPAMIDR		0x130
#define SMMU_MPAMIDR_PARTID_MAX		GENMASK(15, 0)
#define SMMU_MPAMIDR_PMG_MAX		GENMASK(23, 16)

/* SMMU GMPAM */
#define ARM_SMMU_GMPAM			0x138
#define SMMU_GMPAM_SO_PARTID		GENMASK(15, 0)
#define SMMU_GMPAM_SO_PMG		GENMASK(23, 16)
#define SMMU_GMPAM_UPADTE		(1 << 31)

#define SMMUWP_REG_SZ			0x800
#define SMMUWP_OFFSET			0x1ff000

/* Stream table */
#define STRTAB_STE_1_TCU_PF		GENMASK_ULL(57, 56)
#define STRTAB_STE_1_TCU_PF_DIS		0UL
#define STRTAB_STE_1_TCU_PF_RSV		1UL
#define STRTAB_STE_1_TCU_PF_FP		2UL
#define STRTAB_STE_1_TCU_PF_BP		3UL

#define STRTAB_STE_4_PARTID		GENMASK_ULL(31, 16)

#define STRTAB_STE_5_PMG		GENMASK_ULL(7, 0)

#define Q_IDX(llq, p)			((p) & ((1 << (llq)->max_n_shift) - 1))
#define Q_WRP(llq, p)			((p) & (1 << (llq)->max_n_shift))
#define Q_OVERFLOW_FLAG			(1U << 31)
#define Q_OVF(p)			((p) & Q_OVERFLOW_FLAG)
#define Q_ENT(q, p)			((q)->base +			\
					 Q_IDX(&((q)->llq), p) *	\
					 (q)->ent_dwords)

/* Ensure DMA allocations are naturally aligned */
#ifdef CONFIG_CMA_ALIGNMENT
#define Q_MAX_SZ_SHIFT			(PAGE_SHIFT + CONFIG_CMA_ALIGNMENT)
#else
#define Q_MAX_SZ_SHIFT			(PAGE_SHIFT + MAX_ORDER)
#endif

#define CMDQ_PROD_OWNED_FLAG		Q_OVERFLOW_FLAG

/*
 * This is used to size the command queue and therefore must be at least
 * BITS_PER_LONG so that the valid_map works correctly (it relies on the
 * total number of queue entries being a multiple of BITS_PER_LONG).
 */
#define CMDQ_BATCH_ENTRIES		BITS_PER_LONG

/*
 * When the SMMU only supports linear context descriptor tables, pick a
 * reasonable size limit (64kB).
 */
#define CTXDESC_LINEAR_CDMAX		ilog2(SZ_64K / (CTXDESC_CD_DWORDS << 3))

/* High-level queue structures */
#define ARM_SMMU_POLL_TIMEOUT_US	1000000 /* 1s! */
#define ARM_SMMU_POLL_SPIN_COUNT	10

#define MSI_IOVA_BASE			0x8000000
#define MSI_IOVA_LENGTH			0x100000

/* MTK iommu device features */
#define MTK_IOMMU_DEV_FEAT_BASE			20
/**
 * @IOMMU_DEV_FEAT_BYPASS_S1: Bypass smmu stage 1 by StreamID
 *
 */
#define IOMMU_DEV_FEAT_BYPASS_S1		(MTK_IOMMU_DEV_FEAT_BASE + 0)
#define MASTER_FEATURE_COUNT_EXTENDED		(MTK_IOMMU_DEV_FEAT_BASE + 1)

/* MTK impl arm_smmu_device->features */
#define ARM_SMMU_FEAT_IMPL(id)			(31 - (id))
#define ARM_SMMU_FEAT_MPAM			(1 << ARM_SMMU_FEAT_IMPL(0))
#define ARM_SMMU_FEAT_TCU_PF			(1 << ARM_SMMU_FEAT_IMPL(1))
#define ARM_SMMU_FEAT_DIS_EVTQ			(1 << ARM_SMMU_FEAT_IMPL(2))

/* MTK impl share smmu structure memory to hypervisor */
#define HYP_SMMU_CMDQ_SHARE	(0U)
#define HYP_SMMU_STE_SHARE	(1U)

struct arm_smmu_ll_queue {
	union {
		u64			val;
		struct {
			u32		prod;
			u32		cons;
		};
		struct {
			atomic_t	prod;
			atomic_t	cons;
		} atomic;
		u8			__pad[SMP_CACHE_BYTES];
	} ____cacheline_aligned_in_smp;
	u32				max_n_shift;
};

struct arm_smmu_queue {
	struct arm_smmu_ll_queue	llq;
	int				irq; /* Wired interrupt */

	__le64				*base;
	dma_addr_t			base_dma;
	u64				q_base;

	size_t				ent_dwords;

	u32 __iomem			*prod_reg;
	u32 __iomem			*cons_reg;
};

struct arm_smmu_queue_poll {
	ktime_t				timeout;
	unsigned int			delay;
	unsigned int			spin_cnt;
	bool				wfe;
};

struct arm_smmu_cmdq {
	struct arm_smmu_queue		q;
	atomic_long_t			*valid_map;
	atomic_t			owner_prod;
	atomic_t			lock;
};

struct arm_smmu_cmdq_batch {
	u64				cmds[CMDQ_BATCH_ENTRIES * CMDQ_ENT_DWORDS];
	int				num;
};

struct arm_smmu_evtq {
	struct arm_smmu_queue		q;
	struct iopf_queue		*iopf;
	u32				max_stalls;
};

struct arm_smmu_priq {
	struct arm_smmu_queue		q;
};

/* High-level stream table and context descriptor structures */
struct arm_smmu_strtab_l1_desc {
	u8				span;

	__le64				*l2ptr;
	dma_addr_t			l2ptr_dma;
};

struct arm_smmu_ctx_desc {
	u16				asid;
	u64				ttbr;
	u64				tcr;
	u64				mair;

	refcount_t			refs;
	struct mm_struct		*mm;
};

struct arm_smmu_l1_ctx_desc {
	__le64				*l2ptr;
	dma_addr_t			l2ptr_dma;
};

struct arm_smmu_ctx_desc_cfg {
	__le64				*cdtab;
	dma_addr_t			cdtab_dma;
	struct arm_smmu_l1_ctx_desc	*l1_desc;
	unsigned int			num_l1_ents;
};

struct arm_smmu_s1_cfg {
	struct arm_smmu_ctx_desc_cfg	cdcfg;
	struct arm_smmu_ctx_desc	cd;
	u8				s1fmt;
	u8				s1cdmax;
};

struct arm_smmu_s2_cfg {
	u16				vmid;
	u64				vttbr;
	u64				vtcr;
};

struct arm_smmu_strtab_cfg {
	__le64				*strtab;
	dma_addr_t			strtab_dma;
	struct arm_smmu_strtab_l1_desc	*l1_desc;
	unsigned int			num_l1_ents;

	u64				strtab_base;
	u32				strtab_base_cfg;
	u8				split;
};

/* An SMMUv3 instance */
struct arm_smmu_device {
	struct device			*dev;
	void __iomem			*base;
	void __iomem			*page1;
	void __iomem			*wp_base;

	u32				features;

#define ARM_SMMU_OPT_SKIP_PREFETCH	(1 << 0)
#define ARM_SMMU_OPT_PAGE0_REGS_ONLY	(1 << 1)
#define ARM_SMMU_OPT_MSIPOLL		(1 << 2)
#define ARM_SMMU_OPT_CMDQ_FORCE_SYNC	(1 << 3)
	u32				options;

	const struct arm_smmu_impl	*impl;

	struct arm_smmu_cmdq		cmdq;
	struct arm_smmu_evtq		evtq;
	struct arm_smmu_priq		priq;

	int				gerr_irq;
	int				combined_irq;

	unsigned long			ias; /* IPA */
	unsigned long			oas; /* PA */
	unsigned long			pgsize_bitmap;

#define ARM_SMMU_MAX_ASIDS		(1 << 16)
	unsigned int			asid_bits;

#define ARM_SMMU_MAX_VMIDS		(1 << 16)
	unsigned int			vmid_bits;
	struct ida			vmid_map;

	unsigned int			ssid_bits;
	unsigned int			sid_bits;

	struct arm_smmu_strtab_cfg	strtab_cfg;

	/* IOMMU core code handle */
	struct iommu_device		iommu;

	struct rb_root			streams;
	struct mutex			streams_mutex;

	struct mutex			init_mutex;
};

struct arm_smmu_stream {
	u32				id;
	struct arm_smmu_master		*master;
	struct rb_node			node;
};

/* SMMU private data for each master */
struct arm_smmu_master {
	struct arm_smmu_device		*smmu;
	struct device			*dev;
	struct arm_smmu_domain		*domain;
	struct list_head		domain_head;
	struct arm_smmu_stream		*streams;
	unsigned int			num_streams;
	bool				ats_enabled;
	bool				stall_enabled;
	bool				sva_enabled;
	bool				iopf_enabled;
	struct list_head		bonds;
	unsigned int			ssid_bits;
	/* Mediatek proprietary */
	DECLARE_BITMAP(features, MASTER_FEATURE_COUNT_EXTENDED);
};

/* SMMU private data for an IOMMU domain */
enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
	ARM_SMMU_DOMAIN_NESTED,
	ARM_SMMU_DOMAIN_BYPASS,
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct mutex			init_mutex; /* Protects smmu pointer */

	struct io_pgtable_ops		*pgtbl_ops;
	bool				stall_enabled;
	atomic_t			nr_ats_masters;

	enum arm_smmu_domain_stage	stage;
	union {
		struct arm_smmu_s1_cfg	s1_cfg;
		struct arm_smmu_s2_cfg	s2_cfg;
	};

	struct iommu_domain		domain;

	struct list_head		devices;
	spinlock_t			devices_lock;

	struct list_head		mmu_notifiers;
};

static inline struct arm_smmu_domain *to_smmu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct arm_smmu_domain, domain);
}

struct arm_smmu_impl {
	struct iommu_group* (*device_group)(struct device *dev);
	bool (*delay_hw_init)(struct arm_smmu_device *smmu);
	int (*smmu_hw_init)(struct arm_smmu_device *smmu);
	int (*smmu_hw_deinit)(struct arm_smmu_device *smmu);
	int (*smmu_hw_sec_init)(struct arm_smmu_device *smmu);
	void (*smmu_device_reset)(struct arm_smmu_device *smmu);
	int (*smmu_power_get)(struct arm_smmu_device *smmu);
	int (*smmu_power_put)(struct arm_smmu_device *smmu);
	int (*smmu_runtime_suspend)(struct device *dev);
	int (*smmu_runtime_resume)(struct device *dev);
	void (*get_resv_regions)(struct device *dev, struct list_head *head);
	int (*smmu_irq_handler)(int irq, void *dev);
	int (*smmu_evt_handler)(int irq, void *dev, u64 *evt);
	int (*report_device_fault)(struct arm_smmu_device *smmu,
				   struct arm_smmu_master *master,
				   u64 *evt,
				   struct iommu_fault_event *fault_evt);
	void (*smmu_setup_features)(struct arm_smmu_master *master,
				    u32 sid, __le64 *dst);
	int (*def_domain_type)(struct device *dev);
	bool (*dev_has_feature)(struct device *dev,
				enum iommu_dev_features feat);
	bool (*dev_feature_enabled)(struct device *dev,
				    enum iommu_dev_features feat);
	bool (*dev_enable_feature)(struct device *dev,
				   enum iommu_dev_features feat);
	bool (*dev_disable_feature)(struct device *dev,
				    enum iommu_dev_features feat);
	int (*map_pages)(struct arm_smmu_domain *smmu_domain, unsigned long iova,
			 phys_addr_t paddr, size_t pgsize, size_t pgcount,
			 int prot, gfp_t gfp, size_t *mapped);
	void (*iotlb_sync_map)(struct iommu_domain *domain,
			       unsigned long iova, size_t size);
	void (*iotlb_sync)(struct iommu_domain *domain,
			   struct iommu_iotlb_gather *gather);
	int (*tlb_flush)(struct arm_smmu_domain *smmu_domain,
			 unsigned long iova, size_t size,
			 int power_status);
	void (*fault_dump)(struct arm_smmu_device *smmu);
	bool (*skip_shutdown)(struct arm_smmu_device *smmu);
	bool (*skip_sync_timeout)(struct arm_smmu_device *smmu);
	struct io_pgtable_ops* (*alloc_io_pgtable_ops)(enum io_pgtable_fmt fmt,
						       struct io_pgtable_cfg *cfg,
						       void *cookie);
	void (*free_io_pgtable_ops)(struct io_pgtable_ops *ops);
	void (*smmu_mem_share)(struct arm_smmu_device *smmu,
			       unsigned int mem_type);
};

struct arm_smmu_device *arm_smmu_v3_impl_init(struct arm_smmu_device *smmu);
struct arm_smmu_device *mtk_smmu_v3_impl_init(struct arm_smmu_device *smmu);

extern struct xarray arm_smmu_asid_xa;
extern struct mutex arm_smmu_asid_lock;
extern struct arm_smmu_ctx_desc quiet_cd;

int arm_smmu_write_reg_sync(struct arm_smmu_device *smmu, u32 val,
			    unsigned int reg_off, unsigned int ack_off);
int arm_smmu_update_gbpa(struct arm_smmu_device *smmu, u32 set, u32 clr);
int arm_smmu_device_disable(struct arm_smmu_device *smmu);
bool arm_smmu_capable(struct device *dev, enum iommu_cap cap);
struct iommu_group *arm_smmu_device_group(struct device *dev);
int arm_smmu_of_xlate(struct device *dev, struct of_phandle_args *args);

struct platform_device;
int arm_smmu_fw_probe(struct platform_device *pdev,
		      struct arm_smmu_device *smmu, bool *bypass);
int arm_smmu_device_hw_probe(struct arm_smmu_device *smmu);
int arm_smmu_init_one_queue(struct arm_smmu_device *smmu,
			    struct arm_smmu_queue *q,
			    void __iomem *page,
			    unsigned long prod_off,
			    unsigned long cons_off,
			    size_t dwords, const char *name);
int arm_smmu_init_strtab(struct arm_smmu_device *smmu);
void arm_smmu_write_strtab_l1_desc(__le64 *dst,
				   struct arm_smmu_strtab_l1_desc *desc);

void arm_smmu_probe_irq(struct platform_device *pdev,
			struct arm_smmu_device *smmu);
void arm_smmu_setup_unique_irqs(struct arm_smmu_device *smmu,
				irqreturn_t evtqirq(int irq, void *dev),
				irqreturn_t gerrorirq(int irq, void *dev),
				irqreturn_t priirq(int irq, void *dev));

int arm_smmu_register_iommu(struct arm_smmu_device *smmu,
			    struct iommu_ops *ops, phys_addr_t ioaddr);
void arm_smmu_unregister_iommu(struct arm_smmu_device *smmu);

int arm_smmu_write_ctx_desc(struct arm_smmu_domain *smmu_domain, int ssid,
			    struct arm_smmu_ctx_desc *cd);
void arm_smmu_tlb_inv_asid(struct arm_smmu_device *smmu, u16 asid);
void arm_smmu_tlb_inv_range_asid(unsigned long iova, size_t size, int asid,
				 size_t granule, bool leaf,
				 struct arm_smmu_domain *smmu_domain);
bool arm_smmu_free_asid(struct arm_smmu_ctx_desc *cd);
int arm_smmu_atc_inv_domain(struct arm_smmu_domain *smmu_domain, int ssid,
			    unsigned long iova, size_t size);

void arm_smmu_sync_ste_for_sid(struct arm_smmu_device *smmu, u32 sid);
int arm_smmu_cmdq_issue_cmd(struct arm_smmu_device *smmu,
			    struct arm_smmu_cmdq_ent *ent);
void arm_smmu_cmdq_batch_add(struct arm_smmu_device *smmu,
			     struct arm_smmu_cmdq_batch *cmds,
			     struct arm_smmu_cmdq_ent *cmd);
int arm_smmu_cmdq_batch_submit(struct arm_smmu_device *smmu,
			       struct arm_smmu_cmdq_batch *cmds);
int arm_smmu_init_sid_strtab(struct arm_smmu_device *smmu, u32 sid);
struct arm_smmu_master *arm_smmu_find_master(struct arm_smmu_device *smmu,
					     u32 sid);

#ifdef CONFIG_ARM_SMMU_V3_SVA
bool arm_smmu_sva_supported(struct arm_smmu_device *smmu);
bool arm_smmu_master_sva_supported(struct arm_smmu_master *master);
bool arm_smmu_master_sva_enabled(struct arm_smmu_master *master);
int arm_smmu_master_enable_sva(struct arm_smmu_master *master);
int arm_smmu_master_disable_sva(struct arm_smmu_master *master);
bool arm_smmu_master_iopf_supported(struct arm_smmu_master *master);
void arm_smmu_sva_notifier_synchronize(void);
struct iommu_domain *arm_smmu_sva_domain_alloc(void);
void arm_smmu_sva_remove_dev_pasid(struct iommu_domain *domain,
				   struct device *dev, ioasid_t id);
#else /* CONFIG_ARM_SMMU_V3_SVA */
static inline bool arm_smmu_sva_supported(struct arm_smmu_device *smmu)
{
	return false;
}

static inline bool arm_smmu_master_sva_supported(struct arm_smmu_master *master)
{
	return false;
}

static inline bool arm_smmu_master_sva_enabled(struct arm_smmu_master *master)
{
	return false;
}

static inline int arm_smmu_master_enable_sva(struct arm_smmu_master *master)
{
	return -ENODEV;
}

static inline int arm_smmu_master_disable_sva(struct arm_smmu_master *master)
{
	return -ENODEV;
}

static inline bool arm_smmu_master_iopf_supported(struct arm_smmu_master *master)
{
	return false;
}

static inline void arm_smmu_sva_notifier_synchronize(void) {}

static inline struct iommu_domain *arm_smmu_sva_domain_alloc(void)
{
	return NULL;
}

static inline void arm_smmu_sva_remove_dev_pasid(struct iommu_domain *domain,
						 struct device *dev,
						 ioasid_t id)
{
}
#endif /* CONFIG_ARM_SMMU_V3_SVA */

/* Queue functions shared with common and kernel drivers */
static bool __maybe_unused queue_has_space(struct arm_smmu_ll_queue *q, u32 n)
{
	u32 space, prod, cons;

	prod = Q_IDX(q, q->prod);
	cons = Q_IDX(q, q->cons);

	if (Q_WRP(q, q->prod) == Q_WRP(q, q->cons))
		space = (1 << q->max_n_shift) - (prod - cons);
	else
		space = cons - prod;

	return space >= n;
}

static bool __maybe_unused queue_full(struct arm_smmu_ll_queue *q)
{
	return Q_IDX(q, q->prod) == Q_IDX(q, q->cons) &&
	       Q_WRP(q, q->prod) != Q_WRP(q, q->cons);
}

static bool __maybe_unused queue_empty(struct arm_smmu_ll_queue *q)
{
	return Q_IDX(q, q->prod) == Q_IDX(q, q->cons) &&
	       Q_WRP(q, q->prod) == Q_WRP(q, q->cons);
}

static bool __maybe_unused queue_consumed(struct arm_smmu_ll_queue *q, u32 prod)
{
	return ((Q_WRP(q, q->cons) == Q_WRP(q, prod)) &&
		(Q_IDX(q, q->cons) > Q_IDX(q, prod))) ||
	       ((Q_WRP(q, q->cons) != Q_WRP(q, prod)) &&
		(Q_IDX(q, q->cons) <= Q_IDX(q, prod)));
}

static void __maybe_unused queue_sync_cons_out(struct arm_smmu_queue *q)
{
	/*
	 * Ensure that all CPU accesses (reads and writes) to the queue
	 * are complete before we update the cons pointer.
	 * __iomb() is only used in arm64-specific.
	 */
#if IS_ENABLED(CONFIG_ARM64)
	__iomb();
#else
	dma_mb();
#endif
	writel_relaxed(q->llq.cons, q->cons_reg);
}

static void __maybe_unused queue_sync_cons_ovf(struct arm_smmu_queue *q)
{
	struct arm_smmu_ll_queue *llq = &q->llq;

	if (likely(Q_OVF(llq->prod) == Q_OVF(llq->cons)))
		return;

	llq->cons = Q_OVF(llq->prod) | Q_WRP(llq, llq->cons) |
		    Q_IDX(llq, llq->cons);
	queue_sync_cons_out(q);
}

static void __maybe_unused queue_inc_cons(struct arm_smmu_ll_queue *q)
{
	u32 cons = (Q_WRP(q, q->cons) | Q_IDX(q, q->cons)) + 1;
	q->cons = Q_OVF(q->cons) | Q_WRP(q, cons) | Q_IDX(q, cons);
}

static int __maybe_unused queue_sync_prod_in(struct arm_smmu_queue *q)
{
	u32 prod;
	int ret = 0;

	/*
	 * We can't use the _relaxed() variant here, as we must prevent
	 * speculative reads of the queue before we have determined that
	 * prod has indeed moved.
	 */
	prod = readl(q->prod_reg);

	if (Q_OVF(prod) != Q_OVF(q->llq.prod))
		ret = -EOVERFLOW;

	q->llq.prod = prod;
	return ret;
}

static u32 __maybe_unused queue_inc_prod_n(struct arm_smmu_ll_queue *q, int n)
{
	u32 prod = (Q_WRP(q, q->prod) | Q_IDX(q, q->prod)) + n;
	return Q_OVF(q->prod) | Q_WRP(q, prod) | Q_IDX(q, prod);
}

static void __maybe_unused queue_poll_init(struct arm_smmu_device *smmu,
					   struct arm_smmu_queue_poll *qp)
{
	qp->delay = 1;
	qp->spin_cnt = 0;
	qp->wfe = !!(smmu->features & ARM_SMMU_FEAT_SEV);
	qp->timeout = ktime_add_us(ktime_get(), ARM_SMMU_POLL_TIMEOUT_US);
}

static int __maybe_unused queue_poll(struct arm_smmu_queue_poll *qp)
{
	if (ktime_compare(ktime_get(), qp->timeout) > 0)
		return -ETIMEDOUT;

	if (qp->wfe) {
		wfe();
	} else if (++qp->spin_cnt < ARM_SMMU_POLL_SPIN_COUNT) {
		cpu_relax();
	} else {
		udelay(qp->delay);
		qp->delay *= 2;
		qp->spin_cnt = 0;
	}

	return 0;
}

static void __maybe_unused queue_write(__le64 *dst, u64 *src, size_t n_dwords)
{
	int i;

	for (i = 0; i < n_dwords; ++i)
		*dst++ = cpu_to_le64(*src++);
}

static void __maybe_unused queue_read(u64 *dst, __le64 *src, size_t n_dwords)
{
	int i;

	for (i = 0; i < n_dwords; ++i)
		*dst++ = le64_to_cpu(*src++);
}

static int __maybe_unused queue_remove_raw(struct arm_smmu_queue *q, u64 *ent)
{
	if (queue_empty(&q->llq))
		return -EAGAIN;

	queue_read(ent, Q_ENT(q, q->llq.cons), q->ent_dwords);
	queue_inc_cons(&q->llq);
	queue_sync_cons_out(q);
	return 0;
}

enum arm_smmu_msi_index {
	EVTQ_MSI_INDEX,
	GERROR_MSI_INDEX,
	PRIQ_MSI_INDEX,
	ARM_SMMU_MAX_MSIS,
};

#endif /* _ARM_SMMU_V3_H */
