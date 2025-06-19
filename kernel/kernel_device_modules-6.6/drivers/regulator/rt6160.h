/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Jeff Chang <jeff_chang@richtek.com>
 */

#ifndef __RT6160_H
#define __RT6160_H

struct rt6160_error {
	int ot;
	int uv;
	int oc;
};

int rt6160_get_chip_num(void);
void rt6160_get_error_cnt(int id, struct rt6160_error *error);

#endif /*__RT6160_H */
