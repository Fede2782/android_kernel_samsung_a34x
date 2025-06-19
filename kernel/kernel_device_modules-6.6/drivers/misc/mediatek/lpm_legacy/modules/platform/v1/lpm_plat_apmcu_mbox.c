// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_helper.h>
#endif

#include <lpm_plat_apmcu_mbox.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#include <sspm_reservedmem.h>
#endif

#include <lpm_module.h>

struct lpm_apmcu_mbox {
	unsigned int ap_ready;
	unsigned int reserved1;
	unsigned int reserved2;
	unsigned int reserved3;
	unsigned int pwr_ctrl_en;
	unsigned int l3_cache_mode;
	unsigned int buck_mode;
	unsigned int armpll_mode;
	unsigned int task_sta;
	unsigned int reserved9;
	unsigned int reserved10;
	unsigned int reserved11;
	unsigned int wakeup_cpu;
};

struct lpm_apmcu_ipi_data {
	unsigned int ipi_id	: 24;
	unsigned int magic	: 8;
	unsigned int type;
	unsigned int reserved[6];
};

struct lpm_apmcu_ipi_reply {
	unsigned int value;
};

struct mbox_ops {
	void (*write)(int id, int *buf, unsigned int len);
	void (*read)(int id, int *buf, unsigned int len);
};

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static struct lpm_apmcu_ipi_reply lpm_apmcu_mbox_ipi_reply;
#endif
static struct lpm_apmcu_mbox *lpm_apmcu_mbox_data;


#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#define APMCU_SSPM_MBOX_SHARE_SRAM(_wr, _target, _buf) ({\
	if (_wr)\
		lpm_apmcu_mbox_data->_target = _buf;\
	else\
		_buf = lpm_apmcu_mbox_data->_target; })

static void __apmcu_sspm_mailbox(int IsWrite, int id,
					int *buf, unsigned int len)
{
	if (!lpm_apmcu_mbox_data || !buf || (len == 0))
		return;

	switch (id) {
	case APMCU_MCUPM_MBOX_AP_READY:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, ap_ready, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_1:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved1, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_2:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved2, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_3:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved3, *buf);
		break;
	case APMCU_MCUPM_MBOX_PWR_CTRL_EN:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, pwr_ctrl_en, *buf);
		break;
	case APMCU_MCUPM_MBOX_L3_CACHE_MODE:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, l3_cache_mode, *buf);
		break;
	case APMCU_MCUPM_MBOX_BUCK_MODE:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, buck_mode, *buf);
		break;
	case APMCU_MCUPM_MBOX_ARMPLL_MODE:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, armpll_mode, *buf);
		break;
	case APMCU_MCUPM_MBOX_TASK_STA:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, task_sta, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_9:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved9, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_10:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved10, *buf);
		break;
	case APMCU_MCUPM_MBOX_RESERVED_11:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, reserved11, *buf);
		break;
	case APMCU_MCUPM_MBOX_WAKEUP_CPU:
		APMCU_SSPM_MBOX_SHARE_SRAM(IsWrite, wakeup_cpu, *buf);
		break;
	}
}
#endif

static void apmcu_sspm_mailbox_write(int id, int *buf, unsigned int len)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	__apmcu_sspm_mailbox(1, id, buf, len);
#endif
}

static void apmcu_sspm_mailbox_read(int id, int *buf, unsigned int len)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	__apmcu_sspm_mailbox(0, id, buf, len);
#endif
}

static struct mbox_ops mbox[NF_MBOX] = {
	[MBOX_SSPM] = {
		.write = apmcu_sspm_mailbox_write,
		.read = apmcu_sspm_mailbox_read
	}
};

static void mtk_lp_apmcu_pwr_ctrl_setting(int dev)
{
	mbox[MBOX_SSPM].write(APMCU_MCUPM_MBOX_PWR_CTRL_EN, &dev, 1);
}

void mtk_set_lp_apmcu_pll_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_ARMPLL_MODE)
		mbox[MBOX_SSPM].write(APMCU_MCUPM_MBOX_ARMPLL_MODE, &mode, 1);
}
EXPORT_SYMBOL(mtk_set_lp_apmcu_pll_mode);

void mtk_set_lp_apmcu_buck_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_BUCK_MODE)
		mbox[MBOX_SSPM].write(APMCU_MCUPM_MBOX_BUCK_MODE, &mode, 1);
}
EXPORT_SYMBOL(mtk_set_lp_apmcu_buck_mode);

bool mtk_lp_apmcu_is_ready(void)
{
	int sta = MCUPM_TASK_INIT_FINISH;

	mbox[MBOX_SSPM].read(APMCU_MCUPM_MBOX_TASK_STA, &sta, 1);

	return sta == MCUPM_TASK_WAIT || sta == MCUPM_TASK_INIT_FINISH;
}

void mtk_wait_mbox_init_done(void)
{
	int sta = MCUPM_TASK_UNINIT;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ret = 0;
	struct lpm_apmcu_ipi_data d_lpm_apmcu_ipi = {
					.ipi_id = APMCU_PM_IPI_UID_MCDI,
					.magic = MCDI_IPI_MAGIC_NUM,
					.type = MCDI_IPI_SHARE_SRAM_INFO_GET};
#endif
	while (1) {

		if (!lpm_apmcu_mbox_data) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
			if (!is_sspm_ready()) {
				pr_info("[name:mtk_lpm] - sspm mbox not ready !\n");
				continue;
			}

			ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_SPM_SUSPEND,
					IPI_SEND_POLLING, &d_lpm_apmcu_ipi,
					sizeof(d_lpm_apmcu_ipi) / SSPM_MBOX_SLOT_SIZE, 2000);
			if (ret)
				continue;

			lpm_apmcu_mbox_data = (struct lpm_apmcu_mbox *)
					sspm_sbuf_get(lpm_apmcu_mbox_ipi_reply.value);

			if (lpm_apmcu_mbox_data) {
				lpm_smc_cpu_pm(MBOX_INFO, MT_LPM_SMC_ACT_SET,
						   lpm_apmcu_mbox_ipi_reply.value, 0);
			}
#endif
		} else
			mbox[MBOX_SSPM].read(APMCU_MCUPM_MBOX_TASK_STA, &sta, 1);

		if (sta == MCUPM_TASK_INIT)
			break;

		msleep(1000);
	}

	mtk_set_lp_apmcu_pll_mode(MCUPM_ARMPLL_OFF);
	mtk_set_lp_apmcu_buck_mode(MCUPM_BUCK_OFF_MODE);

	mtk_lp_apmcu_pwr_ctrl_setting(
			 MCUPM_MCUSYS_CTRL |
			 MCUPM_CM_CTRL |

			 MCUPM_BUCK_CTRL |
			 MCUPM_ARMPLL_CTRL);
}

void mtk_notify_subsys_ap_ready(void)
{
	int ready = 1;

	mbox[MBOX_SSPM].write(APMCU_MCUPM_MBOX_AP_READY, &ready, 1);
}

int mtk_apmcu_mbox_init(void)
{
	unsigned int ret = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	/* for AP to SSPM */
	if (is_sspm_ready())
		ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_SPM_SUSPEND, NULL, NULL,
			      (void *) &lpm_apmcu_mbox_ipi_reply);
	if (ret)
		pr_info("IPIS_C_SPM_SUSPEND ipi_register fail, ret %d\n", ret);
#endif
	return ret;
}
