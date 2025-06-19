// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "smmu.h"
#include <asm/io.h>
#include "smmu_mgmt.h"
#include <asm/kvm_pkvm_module.h>
#include <asm/barrier.h>
#include "hyp_spinlock.h"

#define REG64(addr) ((volatile uint64_t *)(uintptr_t)(addr))
#define writel_64(v, a) (*REG64(a) = (v))
#define readl_64(a) (*REG64(a))
typedef unsigned int uint32_t;
const struct pkvm_module_ops *pkvm_smmu_ops;
static uint32_t mmio_read32(void *addr)
{
	return (uint32_t)readl(addr);
}

static uint64_t mmio_read64(void *addr)
{
	return (uint64_t)readl_64(addr);
}

static uint32_t mmio_read32_offset(void *addr, uint32_t offset)
{
	return (uint32_t)readl((void *)(addr + offset));
}

static inline void mmio_write32(void *addr, uint32_t data)
{
	writel(data, addr);
}

static inline void mmio_write32_offset(void *addr, uint32_t offset,
				       uint32_t data)
{
	writel(data, (void *)(addr + offset));
}

void construct_cmd_sync(uint64_t *cmd)
{
	cmd[0] = COMPOSE(OP_CMD_SYNC, OP_SHIFT, OP_MASK);
	cmd[0] = cmd[0] | COMPOSE(CSIGNAL_NONE, CSIGNAL_SHIFT, CSIGNAL_MASK);
	cmd[1] = 0;
}

void construct_cmd_tlbi_s12_vmall(uint64_t *cmd, unsigned int vmid)
{
	cmd[0] = COMPOSE(CMDQ_OP_TLBI_S12_VMALL, OP_SHIFT, OP_MASK);
	cmd[0] |= COMPOSE((unsigned long)vmid, CMD_VMID_SHIFT, 0xffff);
	cmd[1] = 0;
}

void write_ste(unsigned long long *st_entry, const unsigned long long *data)
{
	int i;

	/*
	 * Mark the stream table entry as invalid to avoid race condition
	 * STE.V = 0 (bit 0) of first double word
	 */
	st_entry[0] = 0;

	/*
	 * Write to memory from upper double word of Stream Table entry such
	 * that the bottom double word which has the STE.Valid bit is written
	 * last.
	 */
	for (i = STE_SIZE_DW - 1U; i >= 0; i--)
		st_entry[i] = data[i];
	smmu_flush_dcache(st_entry, STE_SIZE);

	/* Ensure written data(STE) is observable to SMMU by performing DSB */
	dsb(sy);
}

void push_entry_to_cmdq(uint64_t *cmdq_entry, const uint64_t *cmd_dword)
{
	if (!cmdq_entry) {
		pkvm_smmu_ops->puts("SMMUv3: error cmdq_entry ptr");
		WARN_ON(1);
		return;
	}
	for (unsigned int i = 0; i < CMD_SIZE_DW; i++)
		cmdq_entry[i] = cmd_dword[i];

	smmu_flush_dcache(cmdq_entry, CMD_SIZE);
	/*
	 * Ensure written data(command) is observable to SMMU by performing DSB
	 */
	dsb(sy);
}

/* replace error cmd with sync cmd */
static void cmdq_error_cmd_handler(smmu_device_t *dev)
{
	struct smmuv3_driver *smmuv3 = dev->smmuv3;
	uint32_t index_mask;
	uint32_t cons_reg;
	void *cmd_target;
	uint64_t cmd_sync[CMD_SIZE_DW];
	uint32_t cons_idx;

	index_mask = ALL_1s(smmuv3->prop.cmdq_entries_log2);
	/* find erroneous cmd's index */
	cons_reg = mmio_read32(smmuv3->cmd_queue.cons_reg_base);
	cons_idx = cons_reg & index_mask;
	cmd_target = (void *)((uint8_t *)smmuv3->cmd_queue.q_base +
			      cons_idx * CMD_SIZE);
	/* replace erroneous cmd with sync cmd */
	construct_cmd_sync(cmd_sync);
	push_entry_to_cmdq(cmd_target, cmd_sync);
}

/*
 * check cmdq has error or not. if cmdq has error situation, fix error and adjust
 * GERRORN reg.
 */
static void smmuv3_show_cmdq_err(smmu_device_t *dev)
{
	uint32_t cons_reg;
	uint32_t gerror_reg;
	uint32_t gerror_n_reg;
	uint32_t offset;
	uint32_t offset_n;
	struct smmuv3_driver *smmuv3 = dev->smmuv3;

	offset = GERROR;
	offset_n = GERRORN;
	/* Check if global error conditions exist */
	gerror_reg = mmio_read32_offset(smmuv3->base_addr, offset);
	gerror_n_reg = mmio_read32_offset(smmuv3->base_addr, offset_n);
	/* check if the bits differ between (S)_GERROR and (S)_GERRORN */
	if ((gerror_reg & SFM_ERR_MASK) != (gerror_n_reg & SFM_ERR_MASK))
		pkvm_smmu_ops->puts("Entered service failure mode");

	if ((gerror_reg & CMDQ_ERR_MASK) == (gerror_n_reg & CMDQ_ERR_MASK))
		return;

	cons_reg = mmio_read32(smmuv3->cmd_queue.cons_reg_base);

	switch (EXTRACT(cons_reg, CMDQ_ERRORCODE_SHIFT, CMDQ_ERRORCODE_MASK)) {
	case CERROR_NONE:
		break;
	case CERROR_ILL:
		cmdq_error_cmd_handler(dev);
		pkvm_smmu_ops->puts("CMDQ encountered error: CERROR_ILL");
		break;
	case CERROR_ABT:
		pkvm_smmu_ops->puts("CMDQ encountered error: CERROR_ABT");
		break;
	case CERROR_ATC_INV_SYNC:
		pkvm_smmu_ops->puts(
			"CMDQ encountered error: CERROR_ATC_INV_SYNC");
		break;
	default:
		pkvm_smmu_ops->puts("CMDQ encountered error: UNKNOWN");
		break;
	}
	gerror_n_reg = gerror_n_reg ^ CMDQ_ERR_MASK;
	mmio_write32_offset(smmuv3->base_addr, offset_n, gerror_n_reg);
}

static inline uint32_t find_offset_next_wr_idx(smmu_device_t *dev,
					       uint32_t current_idx,
					       uint32_t prod_wrap)
{
	uint32_t next_idx;
	uint32_t max_idx;
	uint32_t wrap_bit_set;
	struct smmuv3_driver *smmuv3 = dev->smmuv3;

	max_idx = (1 << smmuv3->prop.cmdq_entries_log2) - 1;

	if (current_idx > max_idx)
		pkvm_smmu_ops->puts("Prod idx overflow");

	if (current_idx < max_idx) {
		next_idx = current_idx + 1;
		return next_idx | prod_wrap;
	}

	/*
	 * If current write index is already at the end, we need to wrap
	 * it around i.e, start from 0 and toggle wrap bit
	 */
	next_idx = 0;
	wrap_bit_set = 1 << smmuv3->prop.cmdq_entries_log2;

	if (prod_wrap == 0)
		return next_idx | wrap_bit_set;

	return next_idx;
}

static void update_cmdq_prod(smmu_device_t *dev, uint32_t expect_idx)
{
	uint32_t prod_index_val;
	struct smmuv3_driver *smmuv3 = dev->smmuv3;

	mmio_write32(smmuv3->cmd_queue.prod_reg_base, expect_idx);
	prod_index_val = mmio_read32(smmuv3->cmd_queue.prod_reg_base);
	/* check prod reg value is correct */
	if (prod_index_val != expect_idx) {
		pkvm_smmu_ops->puts("expect index, HW prod index no match");
		pkvm_smmu_ops->puts("expect index :");
		pkvm_smmu_ops->putx64(expect_idx);
		pkvm_smmu_ops->puts("HW prod index :");
		pkvm_smmu_ops->putx64(prod_index_val);
	}
}

int smmuv3_issue_cmd(smmu_device_t *dev, uint64_t *cmd)
{
	uint32_t prod_idx;
	uint32_t cons_idx;
	uint32_t prod_wrap;
	uint32_t cons_wrap;
	uint32_t prod_reg;
	uint32_t cons_reg;
	uint32_t index_mask;
	uint32_t q_max_entries;
	uint32_t q_empty_slots;
	void *cmd_target;
	uint32_t next_wr_idx;
	uint32_t current_wr_idx;
	bool cmd_issue_done;
	struct smmuv3_driver *smmuv3;
	uint64_t cmdq_base_reg_value, cmdq_base_address;

	cmd_issue_done = 0;
	smmuv3 = dev->smmuv3;
	q_max_entries = 1 << smmuv3->prop.cmdq_entries_log2;
	index_mask = ALL_1s(smmuv3->prop.cmdq_entries_log2);

	/* use lock to make sure that cmd issue is correct */
	hyp_spin_lock(&smmuv3->cmd_queue.cmdq_issue_lock);
	prod_reg = mmio_read32(smmuv3->cmd_queue.prod_reg_base);
	prod_wrap =
		EXTRACT(prod_reg, smmuv3->prop.cmdq_entries_log2, WRAP_MASK);
	prod_idx = prod_reg & index_mask;
	cons_reg = mmio_read32(smmuv3->cmd_queue.cons_reg_base);
	cons_wrap =
		EXTRACT(cons_reg, smmuv3->prop.cmdq_entries_log2, WRAP_MASK);
	cons_idx = cons_reg & index_mask;
	smmuv3_show_cmdq_err(dev);

	if (prod_wrap == cons_wrap)
		q_empty_slots = q_max_entries - (prod_idx - cons_idx);
	else
		q_empty_slots = cons_idx - prod_idx;

	if (q_empty_slots == 0) {
		cmd_issue_done = 0;
		goto out;
	}
	/* check cmdq base reg, before issue cmd */
	cmdq_base_reg_value = mmio_read64(smmuv3->base_addr + CMDQ_BASE);
	cmdq_base_address = (uint64_t)smmu_get_cmdq_buf_pa(dev->smmu_id);

	if ((cmdq_base_reg_value & SMMU_CMDQ_BASE_ADDR_MASK) !=
	    cmdq_base_address) {
		pkvm_smmu_ops->puts(
			"correct cmdq base address is, but cmdq base reg does't match");
		pkvm_smmu_ops->puts("correct cmdq base address is");
		pkvm_smmu_ops->putx64(cmdq_base_address);
		pkvm_smmu_ops->puts("cmdq base reg is");
		pkvm_smmu_ops->putx64(cmdq_base_reg_value);
	}
	current_wr_idx = prod_idx;
	cmd_target = (void *)((uint8_t *)smmuv3->cmd_queue.q_base +
			      current_wr_idx * CMD_SIZE);
	push_entry_to_cmdq(cmd_target, cmd);
	next_wr_idx = find_offset_next_wr_idx(
		dev, current_wr_idx,
		(prod_wrap << smmuv3->prop.cmdq_entries_log2));

	/*
	 * Host(PE) updates the register indicating the next empty space in queue
	 */
	update_cmdq_prod(dev, next_wr_idx);
	cmd_issue_done = 1;
out:
	hyp_spin_unlock(&smmuv3->cmd_queue.cmdq_issue_lock);
	return cmd_issue_done ? true : false;
}

/* make sure that all cmds in the cmdq have been executed */
int smmuv3_rd_meets_wr_idx(smmu_device_t *dev)
{
	volatile unsigned int attempts;
	uint32_t prod_reg;
	uint32_t cons_reg;
	uint32_t prod_idx;
	uint32_t cons_idx;
	uint32_t prod_wrap;
	uint32_t cons_wrap;
	uint32_t index_mask;
	struct smmuv3_driver *smmuv3 = dev->smmuv3;

	index_mask = ALL_1s(smmuv3->prop.cmdq_entries_log2);
	prod_reg = mmio_read32(smmuv3->cmd_queue.prod_reg_base);
	prod_wrap =
		EXTRACT(prod_reg, smmuv3->prop.cmdq_entries_log2, WRAP_MASK);
	prod_idx = prod_reg & index_mask;
	cons_reg = mmio_read32(smmuv3->cmd_queue.cons_reg_base);
	cons_wrap =
		EXTRACT(cons_reg, smmuv3->prop.cmdq_entries_log2, WRAP_MASK);
	cons_idx = cons_reg & index_mask;
	attempts = 0;

	while (attempts++ < 100000) {
		if ((cons_wrap == prod_wrap) && (prod_idx == cons_idx))
			return true;
		/*
		 * to avoid infinite polling, check whether cmdq has error or not.
		 * if cmdq has error, fix error first. otherwise, cmdq can not handle
		 * cmd any more, which makes infinite polling.
		 */
		smmuv3_show_cmdq_err(dev);
		cons_reg = mmio_read32(smmuv3->cmd_queue.cons_reg_base);
		cons_wrap = EXTRACT(cons_reg, smmuv3->prop.cmdq_entries_log2,
				    WRAP_MASK);
		cons_idx = cons_reg & index_mask;
	}
	return false;
}
