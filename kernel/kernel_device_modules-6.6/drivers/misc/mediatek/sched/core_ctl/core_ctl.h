/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _CORE_CTL_H
#define _CORE_CTL_H

struct _CORE_CTL_PACKAGE {
	union {
		__u32 cid;
		__u32 cpu;
	};
	union {
		__u32 min;
		__u32 is_pause;
		__u32 throttle_ms;
		__u32 not_preferred_cpus;
		__u32 boost;
		__u32 thres;
		__u32 enable_policy;
	};
	__u32 max;
};

struct core_ctl_notif_data {
	unsigned int cpu;
	unsigned int is_pause;
	unsigned int paused_mask;
	unsigned int online_mask;
};

#define CORE_CTL_FORCE_RESUME_CPU               _IOW('g', 1,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_FORCE_PAUSE_CPU                _IOW('g', 2,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_OFFLINE_THROTTLE_MS        _IOW('g', 3,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_LIMIT_CPUS                 _IOW('g', 4,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_NOT_PREFERRED              _IOW('g', 5, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_BOOST                      _IOW('g', 6, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_UP_THRES                   _IOW('g', 7, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_ENABLE_POLICY                  _IOW('g', 8, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_CPU_BUSY_THRES             _IOW('g', 9, struct _CORE_CTL_PACKAGE)

extern void core_ctl_notifier_register(struct notifier_block *n);
extern void core_ctl_notifier_unregister(struct notifier_block *n);
extern int pd_freq2opp(int cpu, int opp, int quant, int wl);

#endif /* _CORE_CTL_H */
