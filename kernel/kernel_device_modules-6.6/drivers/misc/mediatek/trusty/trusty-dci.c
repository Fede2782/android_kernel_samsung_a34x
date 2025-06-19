// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk_ise_lpm.h>

/* TEE usage */
#include <mobicore_driver_api.h>

/* Trusty DCI of iSE */
#include "trusty-dci.h"

static struct mc_uuid_t ise_tdrv_gp_uuid = ISE_TDRV_UUID;
static struct mc_session_handle ise_tdrv_gp_session = {0};
static u32 ise_tdrv_gp_devid = MC_DEVICE_ID_DEFAULT;
static struct dciMessage_t *ise_tdrv_gp_dci;

static struct task_struct *open_th;
static struct task_struct *ise_tdrv_gp_Dci_th;

static enum mc_result ise_tdrv_gp_execute(u32 cmdId)
{
	switch (cmdId) {

	case DCI_ISE_POWER_ON:
		if (mtk_ise_awake_lock(ISE_TEE))
			pr_info("%s: ise power on failed\n", __func__);
		break;
	case DCI_ISE_POWER_OFF:
		if (mtk_ise_awake_unlock(ISE_TEE))
			pr_info("%s: ise power off failed\n", __func__);
		break;
	default:
		pr_info("%s: receive an unknown command id(%d).\n",
			__func__, cmdId);
		break;

	}

	return MC_DRV_OK;
}

static int ise_tdrv_gp_listenDci(void *arg)
{
	enum mc_result mc_ret;
	u32 cmdId = 0;

	pr_info("%s: DCI listener.\n", __func__);

	for (;;) {

		pr_info("%s: Waiting for notification\n", __func__);

		/* Wait for notification from SWd */
		mc_ret = mc_wait_notification(&ise_tdrv_gp_session,
						MC_INFINITE_TIMEOUT);
		if (mc_ret != MC_DRV_OK) {
			pr_debug("%s: mcWaitNotification failed, mc_ret=%d\n",
				__func__, mc_ret);
			break;
		}

		if (ise_tdrv_gp_dci->command.len == CMD_LEN_MAGIC_NUM) {
			cmdId = ise_tdrv_gp_dci->command.header.commandId;
			mc_ret = ise_tdrv_gp_execute(cmdId);
		}

		pr_info("%s: wait notification done!! cmdId = %x\n",
			__func__, cmdId);

		/* Notify the STH*/
		mc_ret = mc_notify(&ise_tdrv_gp_session);
		if (mc_ret != MC_DRV_OK) {
			pr_debug("%s: mcNotify returned: %d\n", __func__, mc_ret);
			break;
		}
	}

	return 0;
}

static enum mc_result trusty_dci_gp_open_session(void)
{
	int cnt = 0;
	enum mc_result mc_ret = MC_DRV_ERR_UNKNOWN;

	do {
		msleep(2000);

		/* open device */
		mc_ret = mc_open_device(ise_tdrv_gp_devid);
		if (mc_ret == MC_DRV_ERR_NOT_IMPLEMENTED) {
			pr_info("%s, mc_open_device not support\n", __func__);
			break;
		} else	if (mc_ret != MC_DRV_OK) {
			pr_notice("%s, mc_open_device failed: %d\n", __func__, mc_ret);
			cnt++;
			continue;
		}

		pr_info("%s, mc_open_device success.\n", __func__);


		/* allocating WSM for DCI */
		mc_ret = mc_malloc_wsm(ise_tdrv_gp_devid, 0,
					(u32)sizeof(struct dciMessage_t),
					(uint8_t **)&ise_tdrv_gp_dci, 0);
		if (mc_ret != MC_DRV_OK) {
			pr_debug("%s, mc_malloc_wsm failed: %d\n",
				__func__, mc_ret);
			mc_ret = mc_close_device(ise_tdrv_gp_devid);
			cnt++;
			continue;
		}

		pr_info("%s, mc_malloc_wsm success.\n", __func__);
		pr_info("%s, uuid[0]=%d, uuid[1]=%d, uuid[2]=%d, uuid[3]=%d\n",
			__func__,
			ise_tdrv_gp_uuid.value[0],
			ise_tdrv_gp_uuid.value[1],
			ise_tdrv_gp_uuid.value[2],
			ise_tdrv_gp_uuid.value[3]
			);

		ise_tdrv_gp_session.device_id = ise_tdrv_gp_devid;

		/* open session */
		mc_ret = mc_open_session(&ise_tdrv_gp_session,
					 &ise_tdrv_gp_uuid,
					 (uint8_t *) ise_tdrv_gp_dci,
					 (u32)sizeof(struct dciMessage_t));

		if (mc_ret != MC_DRV_OK) {
			pr_debug("%s, mc_open_session failed, result(%d), cnt(%d)\n",
				__func__, mc_ret, cnt);

			mc_ret = mc_free_wsm(ise_tdrv_gp_devid, (uint8_t *)ise_tdrv_gp_dci);
			pr_debug("%s, free wsm result (%d)\n",
				__func__, mc_ret);

			mc_ret = mc_close_device(ise_tdrv_gp_devid);
			pr_debug("%s, try free wsm and close device\n",
				__func__);
			cnt++;
			continue;
		}
		pr_info("%s, mc_open_session success. (0x%x)\n",
			__func__, ise_tdrv_gp_session.session_id);

		/* create a thread for listening DCI signals */
		ise_tdrv_gp_Dci_th = kthread_run(ise_tdrv_gp_listenDci,
						NULL, "ise_tdrv_gp_Dci");
		if (IS_ERR(ise_tdrv_gp_Dci_th))
			pr_info("%s, init kthread_run failed!\n", __func__);
		else
			break;

	} while (cnt < LOOP_MAX);

	if (cnt >= LOOP_MAX)
		pr_info("%s, open session failed!!! (loop:%d)\n", __func__, cnt);

	pr_info("%s end, mc_ret(0x%x), loop(%d)\n", __func__, mc_ret, cnt);

	return mc_ret;
}

static int trusty_dci_thread(void *context)
{
	enum mc_result ret;

	ret = trusty_dci_gp_open_session();
	pr_info( "%s trusty_dci_gp_open_session, ret = %x\n", __func__, ret);

	return 0;
}

int trusty_dci_init(void)
{
	struct device_node *mobicore_node;
	int ret = 0;
	u32 real_drv;

	mobicore_node = of_find_compatible_node(NULL, NULL, "trustonic,mobicore");
	if (!mobicore_node) {
		pr_notice("find trustonic,mobicore fail\n");
		return 0;
	}

	ret = of_property_read_u32(mobicore_node, "trustonic,real-drv", &real_drv);
	if (ret || !real_drv) {
		pr_info("MobiCore dummy driver, ret=%d, real_drv=%d", ret, real_drv);
		return 0;
	}


	pr_info("%s trusty_dci_thread start\n", __func__);
	open_th = kthread_run(trusty_dci_thread, NULL, "trusty_dci_open");
	if (IS_ERR(open_th))
		pr_notice("%s, init kthread_run failed!\n", __func__);

	return 0;
}
