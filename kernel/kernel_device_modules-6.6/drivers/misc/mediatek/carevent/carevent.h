/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __CAREVENT_H__
#define __CAREVENT_H__

#define NETLINK_CAR_EVENT 28 /* Used for car event montior */
#define PIDS_NR 8

enum {
	EVENT_CAR_REVERSE = 1,
	EVENT_CAR_SLEEP = 2,
};

enum {
	CAR_REVERSE_REQ_STATUS = 0x100,
	CAR_REVERSE_SET_RCV,
	CAR_SLEEP_SET_RCV,
	CAR_SLEEP_REQ_STATUS,
	CAR_REVERSE_REMOVE_RCV,
};

/* for EVENT_CAR_REVERSE */
enum {
	CAR_STATUS_REVERSE = 0x100,
	CAR_STATUS_NORMAL,
	CAR_STATUS_UNKNOWN,
};

/* for EVENT_CAR_SLEEP */
enum {
	CAR_STATUS_WAKEUP = 0x0,
	CAR_STATUS_SLEEP = 0x1,
	CAR_SLEEP_STATUS_UNKNOWN = 0x2,
};

struct car_status {
	int response_type;
	int status;
};


#endif /* end of __CAREVENT_H__ */

