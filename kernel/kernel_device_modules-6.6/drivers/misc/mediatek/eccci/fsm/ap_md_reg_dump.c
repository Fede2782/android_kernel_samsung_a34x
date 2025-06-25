// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include "ap_md_reg_dump.h"

#define TAG "mcd"

size_t mdreg_write32(size_t reg_id, size_t value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_DBGSYS_REG_DUMP, reg_id,
		value, 0, 0, 0, 0, &res);

	return res.a0;
}

static int md_dump_mem_once(const void *buff_src, unsigned long dump_len)
{
	int ret;
	unsigned long i;
	unsigned long dump_limit;
	unsigned int tmp_idx;
	char temp_buf[132] = { 0 };

	if (!buff_src || !dump_len) {
		CCCI_ERROR_LOG(0, TAG, "%s: buff_src = NULL or dump_len = 0x%lx\n",
			__func__, dump_len);
		return -1;
	}

	for (i = 0; i < dump_len;) {
		dump_limit = ((dump_len - i) < 128) ? (dump_len - i) : 128;
		memcpy_fromio(temp_buf, (buff_src + i), dump_limit);
		temp_buf[dump_limit] = 0;
		tmp_idx = 0;

		while (unlikely(strlen(temp_buf + tmp_idx) == 0) && (tmp_idx < dump_limit)) {
			CCCI_ERROR_LOG(0, TAG,
				"%s: strlen return 0, dump_limit:0x%lx, i:0x%lx, temp_buf[%d]:%d\n",
				__func__, dump_limit, i, tmp_idx, temp_buf[tmp_idx]);
			i++;
			tmp_idx++;
		}

		if (tmp_idx < dump_limit)
			ret = ccci_dump_write(CCCI_DUMP_MEM_DUMP, 0, "%s",
				(temp_buf + tmp_idx));
		else {
			CCCI_ERROR_LOG(0, TAG,
				"%s: dump_limit:0x%lx, i:0x%lx, temp_buf[0->%d] are all 0\n",
				__func__, dump_limit, i, tmp_idx);
			continue;
		}

		if (ret < 0) {
			CCCI_ERROR_LOG(0, TAG,
				"%s: ccci_dump_write fail %d, 0x%lx, %zu, 0x%lx\n",
				__func__, ret, dump_limit, strlen(temp_buf + tmp_idx), i);
			return -2;
		} else if (!ret) {
			CCCI_ERROR_LOG(0, TAG,
				"%s: ccci_dump_write return 0: %d, 0x%lx, %zu, 0x%lx\n",
				__func__, ret, dump_limit, strlen(temp_buf + tmp_idx), i);
			i++;
		}
		i += ret;
	}

	return i;
}

void md_dump_reg(struct ccci_modem *md)
{
	struct arm_smccc_res res;
	unsigned long long buf_addr, buf_size, total_len = 0;
	void *buff_src;
	int loop_max_cnt = 0, ret;

	/* 1. start to dump, send start command */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_DEBUG_DUMP,
		MD_REG_DUMP_START, MD_REG_DUMP_MAGIC, 0, 0, 0, 0, &res);
	buf_addr = res.a1;
	buf_size = res.a2;
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s][MD_REG_DUMP_START] flag_1=0x%lx, flag_2=0x%lx, flag_3=0x%lx, flag_4=0x%lx\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
	/* go kernel debug red dump,fix me,we need make it more compatible later */
	if ((res.a0 & 0xffff0000) != 0) {
		CCCI_NORMAL_LOG(-1, TAG, "[%s] go kernel md reg dump, md_gen: %u\n",
			__func__, md_cd_plat_val_ptr.md_gen);
		/* FIX ME
		* can't call ioremap in interrupt,
		* move ioremap to md_cd_start flow in the future.
		*/
		if (in_interrupt()) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"In interrupt, skip internal_md_dump_debug_register\n");
			return;
		}

		/* lk+atf legacy chip dump md reg in kernel */
		if(md->hw_info->plat_ptr->md_dump_reg)
			md->hw_info->plat_ptr->md_dump_reg(md);
		return;
	}

	if (buf_addr <= 0 || buf_size <= 0)
		return;

	/* get read buffer, remap */
	buff_src = md->ioremap_buff_src;
	if (buff_src == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"Dump MD failed to ioremap 0x%llx bytes from 0x%llx\n",
			buf_size, buf_addr);
		return;
	}
	CCCI_MEM_LOG_TAG(0, TAG, "MD register dump start, 0x%llx\n",
		(unsigned long long)buff_src);
	/* 2. get dump data, send dump_stage command to get */
	do {
		loop_max_cnt++;
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_DEBUG_DUMP,
			MD_REG_DUMP_STAGE, 0, 0, 0, 0, 0, &res);
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s][MD_REG_DUMP_STAGE] flag_0=0x%lx, flag_1=0x%lx, flag_2=0x%lx, flag_3=0x%lx\n",
			__func__, res.a0, res.a1, res.a2, res.a3);
		switch (res.a2) {
		case DUMP_FINISHED: /* go through */
		case DUMP_UNFINISHED:
			if (!res.a1)
				goto DUMP_END;
			ret = md_dump_mem_once(buff_src, res.a1);
			if (ret < 0)
				goto DUMP_END;
			total_len += ret;
			break;
		case DUMP_DELAY_us:
			/* delay a3 */
			if (res.a3 > 0 && res.a3 < 100)
				udelay(res.a3);
			else
				CCCI_MEM_LOG_TAG(0, TAG, "invalid delay data: %ld us\n", res.a3);
			if (!res.a1)
				break;
			ret = md_dump_mem_once(buff_src, res.a1);
			if (ret < 0)
				goto DUMP_END;
			total_len += ret;
			break;
		default:
			break;
		}
	} while (res.a2);

DUMP_END:
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s]MD register dump end 0x%x, 0x%llx, 0x%llx\n",
		__func__, loop_max_cnt, buf_size, total_len);
	CCCI_MEM_LOG_TAG(-1, TAG,
		"[%s]MD register dump end 0x%x, 0x%llx, 0x%llx\n",
		__func__, loop_max_cnt, buf_size, total_len);
}

/* For using the lagacy chip with lk, select its corresponding
 * MD dump register function based on the ap_platform here.
 */
void legacy_md_dump_register(struct ccci_modem *md)
{
	unsigned int ap_platform;

	ap_platform = ccci_get_ap_plat();
	CCCI_MEM_LOG_TAG(0, TAG, "[%s] selecting MD dump register for ap-platform %u\n",
			__func__, ap_platform);
	switch(ap_platform) {
	case 6761:
		md_dump_register_for_mt6761(md);
		break;
	case 6765:
		md_dump_register_for_mt6765(md);
		break;
	case 6768:
		md_dump_register_for_mt6768(md);
		break;
	case 6781:
		md_dump_register_for_mt6781(md);
		break;
	case 6833:
	case 6877:
		md_dump_register_for_mt6877(md);
		break;
	case 6853:
	case 6885:
	case 6893:
		md_dump_register_for_mt6885(md);
		break;
	default:
		CCCI_NORMAL_LOG(0, TAG,
			"[%s]skip MD register dump for platform %u due to no support yet\n",
			__func__, ap_platform);
		CCCI_MEM_LOG_TAG(0, TAG,
			"[%s]skip MD register dump for platform %u due to no support yet\n",
			__func__, ap_platform);
	}
}
