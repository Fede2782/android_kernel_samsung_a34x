// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/delay.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include "mtk-mmdvfs-v3-memory.h"
#include "vcp_status.h"
#include "mtk_vdisp.h"
#include "mtk_vdisp_avs.h"

#define VDISP_IPI_ACK_TIMEOUT_US 1000

#define VDISPDBG(fmt, args...) \
	pr_info("[vdisp] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define VDISPERR(fmt, args...) \
	pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

struct mtk_vdisp_avs_ipi_data {
	u32 func_id;
	u32 val;
};

static atomic_t vcp_is_alive = ATOMIC_INIT(true);

#define vdisp_avs_ipi_send_slot_v1(id, value) \
	mtk_vdisp_avs_ipi_send_v1((struct mtk_vdisp_avs_ipi_data) \
	{ .func_id = id, .val = value})
#define IPI_TIMEOUT_MS	(200U)
int mtk_vdisp_avs_ipi_send_v1(struct mtk_vdisp_avs_ipi_data data)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_ARM64)
	int ack = 0, i = 0;
	struct mtk_ipi_device *ipidev;

	ipidev = vcp_get_ipidev(VDISP_FEATURE_ID);
	if (!ipidev) {
		VDISPDBG("vcp_get_ipidev fail");
		return IPI_DEV_ILLEGAL;
	}
	ack = VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_IPI_ACK_BIT);
	ret = mtk_ipi_send(ipidev, IPI_OUT_VDISP, IPI_SEND_WAIT,
		&data, PIN_OUT_SIZE_VDISP, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE) {
		VDISPDBG("ipi fail: %d", ret);
		return ret;
	}
	while ((VDISP_SHRMEM_BITWISE_VAL & BIT(VDISP_AVS_IPI_ACK_BIT)) == ack) {
		udelay(1);
		i++;
		if (i >= VDISP_IPI_ACK_TIMEOUT_US) {
			VDISPDBG("ack timeout");
			return IPI_COMPL_TIMEOUT;
		}
	}
#endif
	return ret;
}

int vdisp_avs_ipi_send_slot_enable_vcp_v1(enum mtk_vdisp_avs_ipi_func_id func_id, uint32_t val)
{
	int ret = 0;

	ret = mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	if (ret) {
		VDISPDBG("request mmdvfs rst fail");
		return ret;
	}
	if (!atomic_read(&vcp_is_alive)) {
		VDISPDBG("vcp is not alive, do nothing");
		goto release_vcp;
	}
	ret = vdisp_avs_ipi_send_slot_v1(func_id, val);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
	return ret;
}

void mtk_vdisp_avs_vcp_notifier_v1(unsigned long vcp_event, void *data)
{
	switch (vcp_event) {
	case VCP_EVENT_READY:
	case VCP_EVENT_STOP:
		break;
	case VCP_EVENT_SUSPEND:
		atomic_set(&vcp_is_alive, false);
		break;
	case VCP_EVENT_RESUME:
		atomic_set(&vcp_is_alive, true);
		break;
	}
}
EXPORT_SYMBOL(mtk_vdisp_avs_vcp_notifier_v1);

int mtk_vdisp_avs_dbg_opt_v1(const char *opt)
{
	int ret = 0;
	u32 v1 = 0, v2 = 0;

	if (strncmp(opt + 4, "off:", 4) == 0) {
		ret = sscanf(opt, "avs:off:%u,%u\n", &v1, &v2);
		/* opp(v1): max 5 level; step(v2) max 32 level; v1(5) is used to toggle AVS */
		if ((ret != 2) || (v1 > 5 || v2 >= 31)) {
			VDISPDBG("[Warning] avs:off sscanf not match");
			return -EINVAL;
		}
		/*Set opp and step*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_STEP, (v1 << 16) | v2);
		if (ret)
			return ret;
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_EN, 0);
		if (ret)
			return ret;
		ret = mmdvfs_force_step_by_vcp(2, 4 - v1);
	} else if (strncmp(opt + 4, "off", 3) == 0) {
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_EN, 0);
	} else if (strncmp(opt + 4, "on", 2) == 0) {
		/*On avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_EN, 1);
	} else if (strncmp(opt + 4, "dbg:on", 6) == 0) {
		/*On avs debug mode */
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_DBG_MODE, 1);
	} else if (strncmp(opt + 4, "dbg:off", 7) == 0) {
		/*Off avs debug mode*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp_v1(FUNC_IPI_AVS_DBG_MODE, 0);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_vdisp_avs_dbg_opt_v1);
