// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ktime.h>
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

static void __iomem *g_aging_base;
static struct clk *g_mmdvfs_clk;
const struct mtk_vdisp_avs_data *g_vdisp_avs_data;
struct mtk_vdisp_avs_ipi_data {
	u32 func_id;
	u32 val;
};
static bool fast_en;
static bool vcp_is_alive = true;
static bool aging_force_disable;

#define vdisp_avs_ipi_send_slot(id, value) \
	mtk_vdisp_avs_ipi_send((struct mtk_vdisp_avs_ipi_data) \
	{ .func_id = id, .val = value})
#define IPI_TIMEOUT_MS	(200U)
int mtk_vdisp_avs_ipi_send(struct mtk_vdisp_avs_ipi_data data)
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

bool wait_for_aging_ack_timeout(int flag)
{
	u32 i = 0;

	while (((VDISP_SHRMEM_BITWISE_VAL >> VDISP_AVS_AGING_ACK_BIT) & 1UL) != flag) {
		udelay(1);
		i++;
		if (i >= VDISP_IPI_ACK_TIMEOUT_US) {
			VDISPDBG("ack timeout");
			vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 0);
			return true;
		}
	}
	return false;
}

int vdisp_avs_ipi_send_slot_enable_vcp(enum mtk_vdisp_avs_ipi_func_id func_id, uint32_t val)
{
	int ret = 0;

	ret = mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_RST);
	if (ret) {
		VDISPDBG("request mmdvfs rst fail");
		return ret;
	}
	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, do nothing");
		goto release_vcp;
	}
	ret = vdisp_avs_ipi_send_slot(func_id, val);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_RST);
	return ret;
}

void mtk_vdisp_avs_vcp_notifier(unsigned long vcp_event, void *data)
{
	switch (vcp_event) {
	case VCP_EVENT_READY:
	case VCP_EVENT_STOP:
		break;
	case VCP_EVENT_SUSPEND:
		vcp_is_alive = false;
		break;
	case VCP_EVENT_RESUME:
		vcp_is_alive = true;
		break;
	}
}
EXPORT_SYMBOL(mtk_vdisp_avs_vcp_notifier);


void query_curr_ro(const struct mtk_vdisp_avs_data *vdisp_avs_data)
{
	u32 ro_fresh_curr = 0, ro_aging_curr = 0;

	/* read vdisp avs RO fresh current */
	// 1. Enable Power on
	writel(0x0000001D, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000005F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000004F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_test);
	writel(0x00000100, g_aging_base + vdisp_avs_data->aging_reg_test);
	// 3. enable RO
	writel(vdisp_avs_data->aging_reg_ro_en0_fresh_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en0);
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_ro_en1);
	writel(vdisp_avs_data->aging_reg_ro_en2_fresh_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en2);
	// 4. RO select
	writel(vdisp_avs_data->aging_ro_sel_0_fresh_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_0);
	writel(vdisp_avs_data->aging_ro_sel_1_fresh_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO fresh current Value
	ro_fresh_curr = readl(g_aging_base + vdisp_avs_data->aging_ro_fresh) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_FRESH_CURR=%x", ro_fresh_curr);
	// 8. Power off
	writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000007F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000003F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001E, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	writel(0x0000001C, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);

	/* read vdisp avs RO aging current */
	// // 1. Enable Power on
	// writel(0x0000001D, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000001F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000005F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000004F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// writel(0x0000006F, g_aging_base + vdisp_avs_data->aging_reg_pwr_ctrl);
	// 2. let rst_b =1’b0 (normal mode0 to  rst_b = 1’b1
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_test);
	writel(0x00000100, g_aging_base + vdisp_avs_data->aging_reg_test);
	// 3. enable RO
	writel(vdisp_avs_data->aging_reg_ro_en0_aging_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en0);
	writel(0x00000000, g_aging_base + vdisp_avs_data->aging_reg_ro_en1);
	writel(vdisp_avs_data->aging_reg_ro_en2_aging_val,
		g_aging_base + vdisp_avs_data->aging_reg_ro_en2);
	// 4. RO select
	writel(vdisp_avs_data->aging_ro_sel_0_aging_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_0);
	writel(vdisp_avs_data->aging_ro_sel_1_aging_val,
		g_aging_base + vdisp_avs_data->aging_ro_sel_1);
	// 5. Aptv_timer + cnt restart & clr
	writel(0x00022A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	udelay(20);
	writel(0x00002A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 6. Aptv_timer + cnt start & clr
	writel(0x00012A00, g_aging_base + vdisp_avs_data->aging_win_cyc);
	// 7. Count over
	udelay(460);
	// Get RO aging current Value
	ro_aging_curr = readl(g_aging_base + vdisp_avs_data->aging_ro_aging) & 0xFFFF;
	// VDISPDBG("VDISP_AVS_RO_AGING_CURR=%x", ro_aging_curr);

	/* update to share memory and send ipi to update */
	vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_UPDATE, (ro_fresh_curr << 16) | ro_aging_curr);
}

void mtk_vdisp_avs_query_aging_val(struct device *dev)
{
	int ret;
	static ktime_t last_time;
	ktime_t cur_time = ktime_get();
	ktime_t elapse_time = cur_time - last_time;
	ktime_t t_ag;

	t_ag = fast_en ? 10*1e9 : 24*60*60*1e9; // 1day: 24*60*60*1e9

	if (aging_force_disable)
		return;

	if ((elapse_time < t_ag) && last_time)
		return;
	last_time = cur_time;

	if (!g_aging_base) {
		VDISPDBG("g_aging_base uninitialized, skip");
		return;
	}

	if (!g_mmdvfs_clk) {
		VDISPDBG("g_mmdvfs_clk uninitialized, skip");
		return;
	}

	if (!g_vdisp_avs_data) {
		VDISPDBG("vdisp_avs_data uninitialized, skip");
		return;
	}

	/* trigger VDISP OPP4 750mV */
	if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP)) {
		VDISPDBG("request mmdvfs disp fail");
		return;
	}
	if (!vcp_is_alive) {
		VDISPDBG("vcp is not alive, skip");
		goto release_vcp;
	}

	if ((VDISP_SHRMEM_BITWISE_VAL &
		(BIT(VDISP_AVS_AGING_ENABLE_BIT) | BIT(VDISP_AVS_ENABLE_BIT))) !=
		(BIT(VDISP_AVS_AGING_ENABLE_BIT) | BIT(VDISP_AVS_ENABLE_BIT))) {
		VDISPDBG("aging disabled, skip");
		goto release_vcp;
	}

	if ((vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 1) != IPI_ACTION_DONE) ||
		wait_for_aging_ack_timeout(1))
		goto release_vcp;

	ret = clk_set_rate(g_mmdvfs_clk, 728000000);
	if (ret) {
		VDISPDBG("request vdisp opp4 fail: %d", ret);
		if (vdisp_avs_ipi_send_slot(FUNC_IPI_AGING_ACK, 0) != IPI_ACTION_DONE)
			VDISPDBG("restore aging ack fail");
		goto release_vcp;
	}

	/* wait for VDISP_AVS_AGE_ACK to be set */
	if (wait_for_aging_ack_timeout(0))
		goto release_mmdvfs_clk;

	/* query current aging sensor value */
	query_curr_ro(g_vdisp_avs_data);

	/* release VDISP opp4 */
release_mmdvfs_clk:
	ret = clk_set_rate(g_mmdvfs_clk, 0);
	if (ret)
		VDISPDBG("release vdisp opp request fail: %d", ret);

release_vcp:
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);
}

int mtk_vdisp_avs_dbg_opt(const char *opt)
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
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_STEP, (v1 << 16) | v2);
		if (ret)
			return ret;
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 0);
		if (ret)
			return ret;
		ret = mmdvfs_force_step_by_vcp(2, 4 - v1);
	} else if (strncmp(opt + 4, "off", 3) == 0) {
		/*Off avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 0);
	} else if (strncmp(opt + 4, "t_ag:", 5) == 0) {
		ret = sscanf(opt, "avs:t_ag:%u\n", &v1);
		if (ret != 1) {
			VDISPDBG("[Warning] avs:t_ag sscanf not match");
			return -EINVAL;
		}
		fast_en = (v1 != 0);
		ret = 0;
	} else if (strncmp(opt + 4, "on", 2) == 0) {
		/*On avs*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_EN, 1);
	} else if (strncmp(opt + 4, "dbg:on", 6) == 0) {
		/*On avs debug mode */
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_DBG_MODE, 1);
	} else if (strncmp(opt + 4, "dbg:off", 7) == 0) {
		/*Off avs debug mode*/
		ret = vdisp_avs_ipi_send_slot_enable_vcp(FUNC_IPI_AVS_DBG_MODE, 0);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_vdisp_avs_dbg_opt);

int mtk_vdisp_avs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct clk *clk;
	const struct mtk_vdisp_data *vdisp_data = of_device_get_match_data(dev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "aging_base");
	if (res) {
		g_aging_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!g_aging_base) {
			VDISPERR("fail to ioremap aging_base: 0x%pa", &res->start);
			return -EINVAL;
		}
	}

	clk = devm_clk_get(dev, "mmdvfs_clk");
	if (!IS_ERR(clk))
		g_mmdvfs_clk = clk;

	g_vdisp_avs_data = vdisp_data->avs;

	return 0;
}
