/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _BLOCKTAG_FUSE_TRACE_H
#define _BLOCKTAG_FUSE_TRACE_H

#include <linux/types.h>
#include <linux/fuse.h>
#include "blocktag-internal.h"

#define MAX_FUSE_REQ_HIST_CNT (512)

struct btag_fuse_entry {
	unsigned long flags;
	u64 time;
	u64 unique;
	u64 nodeid;
	u32 uid;
	u32 gid;
	u32 pid;
	u32 opcode;
	u32 filter;
	u32 event;
	u8 cpu;
};

struct btag_fuse_req_hist {
	struct btag_fuse_entry req_hist[MAX_FUSE_REQ_HIST_CNT];
	u32 next;
	spinlock_t lock;
	u8 enable;
};

struct btag_fuse_req_stat {
	u64 count;
	u64 prefilter;
	u64 postfilter;
};

/*
 * periodic fuse request count statistiics
 * @lock:         protect whole contents in this structure
 * @accumulator:  fuse request count accumulator for the current ongoing window
 * @last_cnt:     the fuse request count of the last completed window
 * @distribution: the distribution function of fuse request count
 *                index 0 for 0 count
 *                index 1..32 for [2^(index - 1), 2^index)
 */
struct fuse_periodic_stat {
	u64 accumulator;
	u64 last_cnt;
	u64 max_cnt;
	u64 distribution[33];
	spinlock_t lock;
};

enum btag_fuse_event {
	BTAG_FUSE_REQ,
	NR_BTAG_FUSE_EVENT,
};

u64 btag_fuse_pstat_get_last(void);
u64 btag_fuse_pstat_get_max(void);
u64 btag_fuse_pstat_get_distribution(u32 idx);

void mtk_btag_fuse_req_hist_show(char **buff, unsigned long *size,
				 struct seq_file *seq);

void mtk_btag_fuse_init(struct proc_dir_entry *btag_root);
void mtk_btag_fuse_exit(void);

void mtk_btag_eara_get_fuse_data(struct eara_iostat *data);
void mtk_btag_fuse_get_req_cnt(int *total_cnt, int *unlink_cnt);
void mtk_btag_fuse_clear_req_cnt(void);

#endif /* _BLOCKTAG_FUSE_TRACE_H */
