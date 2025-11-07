/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TZ_FFA_H__
#define __TZ_FFA_H__

#include <linux/mm.h>
#include <linux/mm_types.h>

#include <tzdev_smc.h>

#include "core/ffa_shmem.h"

#define FFA_NOT_SUPPORTED	-1
#define FFA_INVALID_PARAMETERS	-2
#define FFA_NO_MEMORY		-3
#define FFA_BUSY		-4
#define FFA_INTERRUPTED		-5
#define FFA_DENIED		-6
#define FFA_RETRY		-7
#define FFA_ABORTED		-8
#define FFA_NO_DATA		-9

/* FF-A interfaces */
#define FFA_ERROR		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x60)
#define FFA_SUCCESS		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x61)
#define FFA_INTERRUPT		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x62)
#define FFA_RX_RELEASE		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_64, SMC_STANDARD_CALL_MASK, 0x65)
#define FFA_RXTX_MAP		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_64, SMC_STANDARD_CALL_MASK, 0x66)
#define FFA_PARTITION_INFO_GET	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x68)
#define FFA_ID_GET		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x69)
#define FFA_YIELD		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x6C)
#define FFA_RUN			CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x6D)
#define FFA_MSG_SEND_DIRECT_REQ	CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x6F)
#define FFA_MEM_SHARE		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x73)
#define FFA_MEM_RECLAIM		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x77)
#define FFA_MEM_FRAG_RX		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x7A)
#define FFA_MEM_FRAG_TX		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x7B)
#define FFA_SPMC_ID_GET		CREATE_SMC_CMD(SMC_TYPE_FAST, SMC_AARCH_32, SMC_STANDARD_CALL_MASK, 0x85)

#define IS_FFA_ERROR(smc_data)	(smc_data.args[0] == FFA_ERROR)
#define FFA_GET_ERROR(smc_data)	(smc_data.args[2])

static inline int ffa_generic_error(int ffa_err)
{
	switch (ffa_err) {
		case FFA_NOT_SUPPORTED:
		       return -EOPNOTSUPP;
		case FFA_INVALID_PARAMETERS:
			return -EINVAL;
		case FFA_NO_MEMORY:
			return -ENOMEM;
		case FFA_BUSY:
			return -EBUSY;
		case FFA_INTERRUPTED:
			return -EINTR;
		case FFA_DENIED:
			return -EACCES;
		case FFA_RETRY:
			return -EAGAIN;
		case FFA_ABORTED:
			return -ECANCELED;
		case FFA_NO_DATA:
			return -ECANCELED;
	}

	return -EINVAL;
}

int tzdev_ffa_init(void);
void tzdev_ffa_fini(void);

int tzdev_ffa_mem_share(unsigned int num_pages, struct page **pages, ffa_handle_t *phandle);
int tzdev_ffa_mem_reclaim(ffa_handle_t handle);

unsigned short tzdev_ffa_id(void);
unsigned short tzdev_ffa_sp_id(void);

#endif /* __TZ_FFA_H__ */
