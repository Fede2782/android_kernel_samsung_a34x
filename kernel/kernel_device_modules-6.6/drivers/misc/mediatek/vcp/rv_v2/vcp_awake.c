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
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "vcp_feature_define.h"
#include "vcp_ipi_pin.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp.h"

/*
 * acquire vcp lock flag, keep vcp awake
 * @param vcp_core_id: vcp core id
 * return  0 :get lock success
 *        -1 :get lock timeout
 */
int vcp_awake_lock(void *_vcp_id)
{
	enum vcp_core_id core_id = (enum vcp_core_id) _vcp_id;
	unsigned long spin_flags;
	int *vcp_awake_count;

	if (core_id >= VCP_CORE_TOTAL) {
		pr_notice("%s: VCP ID >= VCP_CORE_TOTAL\n", __func__);
		return -1;
	}

	vcp_awake_count = (int *)&vcp_awake_counts[core_id];

	if (is_vcp_ready_by_coreid(core_id) == 0) {
		pr_notice("%s: %u not enabled\n", __func__, core_id);
		return -1;
	}

	/* vcp unlock awake */
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);

	/* vcp lock awake success*/
	*vcp_awake_count = *vcp_awake_count + 1;

	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return 0;
}

/*
 * release vcp awake lock flag
 * @param vcp_core_id: vcp core id
 * return  0 :release lock success
 *        -1 :release lock fail
 */
int vcp_awake_unlock(void *_vcp_id)
{
	enum vcp_core_id core_id = (enum vcp_core_id) _vcp_id;
	unsigned long spin_flags;
	int *vcp_awake_count;

	if (core_id >= VCP_CORE_TOTAL) {
		pr_notice("%s: VCP ID >= VCP_CORE_TOTAL\n", __func__);
		return -1;
	}

	vcp_awake_count = (int *)&vcp_awake_counts[core_id];

	if (is_vcp_ready_by_coreid(core_id) == 0) {
		pr_notice("%s: %u not enabled\n", __func__, core_id);
		return -1;
	}

	/* vcp unlock awake */
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	if (*vcp_awake_count > 0)
		*vcp_awake_count = *vcp_awake_count - 1;
	else
		pr_notice("%s:%u awake_count=%d NOT SYNC!\n", __func__,
			core_id, *vcp_awake_count);

	/* spinlock context safe */
	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return 0;
}
