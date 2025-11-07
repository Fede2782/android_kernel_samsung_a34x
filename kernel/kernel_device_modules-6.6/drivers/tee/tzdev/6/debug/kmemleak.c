/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "tzdev_internal.h"
#include "core/iw_mem.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/smc_channel.h"
#include "core/subsystem.h"

enum {
	TZDEV_KMEMLEAK_CMD_GET_BUF_SIZE,
};

static struct tzdev_iw_mem *buf;

static int kmemleak_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	int ret;
	unsigned int num_pages;
	struct tzdev_smc_channel *ch;

	(void)cb;
	(void)code;
	(void)unused;

	ch = tzdev_smc_channel_acquire();

	ret = tzdev_smc_kmemleak_cmd(TZDEV_KMEMLEAK_CMD_GET_BUF_SIZE);
	if (ret) {
		if (ret == -ENOSYS) {
			tzdev_smc_channel_release(ch);
			return 0;
		}

		log_error(tzdev, "Failed to get buffer size, error=%d\n", ret);
		tzdev_smc_channel_release(ch);
		return NOTIFY_BAD;
	}

	ret = tzdev_smc_channel_read(ch, &num_pages, sizeof(num_pages));
	if (ret) {
		log_error(tzdev, "Failed to get read num pages error=%d\n", ret);
		tzdev_smc_channel_release(ch);
		return NOTIFY_BAD;
	}

	tzdev_smc_channel_release(ch);

	buf = tzdev_iwio_channel_connect(TZ_IWIO_CONNECT_KMEMLEAK, num_pages * PAGE_SIZE, NULL, NULL);
	if (IS_ERR(buf)) {
		log_error(tzdev, "Failed to connect kmemleak error=%ld\n", PTR_ERR(buf));
		return NOTIFY_BAD;
	}

	return NOTIFY_DONE;
}

static int kmemleak_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	tzdev_iw_mem_destroy(buf);

	return NOTIFY_DONE;
}

static struct notifier_block kmemleak_init_notifier = {
	.notifier_call = kmemleak_init_call,
};

static struct notifier_block kmemleak_fini_notifier = {
	.notifier_call = kmemleak_fini_call,
};

int tz_kmemleak_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &kmemleak_init_notifier);
	if (rc)
		return rc;

	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &kmemleak_fini_notifier);
	if (rc) {
		tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &kmemleak_init_notifier);
		return rc;
	}

	return 0;
}

tzdev_early_initcall(tz_kmemleak_init);
