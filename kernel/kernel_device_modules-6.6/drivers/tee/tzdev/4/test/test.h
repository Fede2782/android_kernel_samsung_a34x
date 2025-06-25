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

#ifndef _TZDEV_TEST_H
#define _TZDEV_TEST_H

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int tee_test_init(void);
void tee_test_exit(void);
#endif

#endif /* _TZDEV_TESTS_H */
