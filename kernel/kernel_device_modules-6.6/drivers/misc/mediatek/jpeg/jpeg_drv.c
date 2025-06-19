// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/smi.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <mtk_heap.h>

#include "jpeg_drv.h"
#include "jpeg_drv_reg.h"
#include "mtk-interconnect.h"
#include "mtk-smmu-v3.h"

#define JPEG_DEVNAME "mtk_jpeg"

#define JPEG_DEC_PROCESS 0x1

static struct JpegDeviceStruct gJpegqDev;
static atomic_t nodeCount;

static const struct of_device_id jdec_hybrid_of_ids[] = {
	{.compatible = "mediatek,jpgdec",},
	{.compatible = "mediatek,jpgdec_c1",},
	{}
};

/* hybrid decoder */
static wait_queue_head_t hybrid_dec_wait_queue[HW_CORE_NUMBER];
static DEFINE_MUTEX(jpeg_hybrid_dec_lock);

static bool dec_hwlocked[HW_CORE_NUMBER] = {false, false, false};
static bool dec_hw_enable[HW_CORE_NUMBER] = {false, false, false};
static unsigned int _jpeg_hybrid_dec_int_status[HW_CORE_NUMBER];
static struct dmabuf_info bufInfo[HW_CORE_NUMBER];

int jpg_dbg_level;
module_param(jpg_dbg_level, int, 0644);

int jpg_core_binding = -1;
module_param(jpg_core_binding, int, 0644);

static void jpeg_drv_hybrid_dec_dump_register_setting(int id)
{
	unsigned int regs[8];

	JPEG_LOG(0, "start dump id: %d", id);
	for (int i = 0x90; i < 0x370; i += 32) {
		for (int j = 0; j < 8; j++)
			regs[j] = IMG_REG_READ(JPEG_HYBRID_DEC_BASE(id) + i + j * 4);

		JPEG_LOG(0, "0x%03x: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			 i, regs[0], regs[1], regs[2], regs[3],
			 regs[4], regs[5], regs[6], regs[7]);
	}
	for (int i = 0; i < 6; i++)
		regs[i] = IMG_REG_READ(JPEG_HYBRID_DEC_BASE(id) + 0x370 + i * 4);
	JPEG_LOG(0, "0x370: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
		 regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);
}

static int _jpeg_isr_hybrid_dec_lisr(int id)
{
	unsigned int tmp = 0;

	if (dec_hwlocked[id] && dec_hw_enable[id]) {
		tmp = IMG_REG_READ(REG_JPGDEC_HYBRID_274(id));
		if (tmp) {
			_jpeg_hybrid_dec_int_status[id] = tmp;
			IMG_REG_WRITE(tmp, REG_JPGDEC_HYBRID_274(id));
			JPEG_LOG(1, "return 0");
			return 0;
		}
	}
	JPEG_LOG(1, "return -1");
	return -1;
}

static int jpeg_isr_hybrid_dec_lisr(int id)
{
	int ret = 0;

	mutex_lock(&jpeg_hybrid_dec_lock);
	ret = _jpeg_isr_hybrid_dec_lisr(id);
	mutex_unlock(&jpeg_hybrid_dec_lock);

	return ret;
}

static inline void jpeg_reg_write_mask(long addr, uint32_t mask, uint32_t val)
{
	uint32_t reg_tmp;

	reg_tmp = IMG_REG_READ(addr);
	reg_tmp = (reg_tmp & ~mask) | (val & mask);
	IMG_REG_WRITE(reg_tmp, addr);
}

static void jpeg_axdomain_set(int id)
{
	int node_id = id / 2;

	if (!gJpegqDev.smiLarbBaseVA[node_id])
		return;

	if (id == 0) {
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 17),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 17),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 18),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 18),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 30),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 30),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
	} else if (id == 1) {
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 27),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 27),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 28),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 28),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 29),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 29),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
	}  else if (id == 2) {
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 17),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 17),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 18),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 18),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F00(gJpegqDev.smiLarbBaseVA[node_id], 30),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
		jpeg_reg_write_mask(REG_JPGDEC_LARB_F80(gJpegqDev.smiLarbBaseVA[node_id], 30),
				    0x1f0, gJpegqDev.axdomain[node_id] << 4);
	}
}

static int jpeg_drv_hybrid_dec_start(unsigned int data[],
				     unsigned int id,
				     int *index_buf_fd)
{
	u64 ibuf_iova, obuf_iova;
	int ret;
	void *ptr;
	struct iosys_map map;
	unsigned int node_id;

	JPEG_LOG(1, "+ id:%d", id);
	ret = 0;
	ibuf_iova = 0;
	obuf_iova = 0;
	node_id = id / 2;

	mutex_lock(&jpeg_hybrid_dec_lock);
	bufInfo[id].o_dbuf = jpg_dmabuf_alloc(data[20], 128, 0);
	bufInfo[id].o_attach = NULL;
	bufInfo[id].o_sgt = NULL;
	if (bufInfo[id].o_dbuf)
		mtk_dma_buf_set_name(bufInfo[id].o_dbuf, "jpg_dec_o_buf");

	bufInfo[id].i_dbuf = jpg_dmabuf_get(data[7]);
	bufInfo[id].i_attach = NULL;
	bufInfo[id].i_sgt = NULL;
	if (bufInfo[id].i_dbuf)
		mtk_dma_buf_set_name(bufInfo[id].i_dbuf, "jpg_dec_i_buf");

	if (!bufInfo[id].o_dbuf) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "o_dbuf alloc failed");
		return -1;
	}

	if (!bufInfo[id].i_dbuf) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "i_dbuf null error");
		return -1;
	}

	ret = jpg_dmabuf_get_iova(bufInfo[id].o_dbuf, &obuf_iova, gJpegqDev.smmu_dev[node_id],
	&bufInfo[id].o_attach, &bufInfo[id].o_sgt);
	JPEG_LOG(1, "obuf_iova:0x%llx lsb:0x%lx msb:0x%lx", obuf_iova,
		(unsigned long)(unsigned char *)obuf_iova,
		(unsigned long)(unsigned char *)(obuf_iova>>32));

	if (ret != 0) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "get obuf_iova fail");
		return ret;
	}

	ret = jpg_dmabuf_vmap(bufInfo[id].o_dbuf, &map);
	ptr = map.vaddr;
	if (ret == 0 && ptr != NULL && data[20] > 0)
		memset(ptr, 0, data[20]);
	jpg_dmabuf_vunmap(bufInfo[id].o_dbuf, &map);

	ret = jpg_dmabuf_get_iova(bufInfo[id].i_dbuf, &ibuf_iova, gJpegqDev.smmu_dev[node_id],
	&bufInfo[id].i_attach, &bufInfo[id].i_sgt);
	JPEG_LOG(1, "ibuf_iova 0x%llx lsb:0x%lx msb:0x%lx", ibuf_iova,
		(unsigned long)(unsigned char *)ibuf_iova,
		(unsigned long)(unsigned char *)(ibuf_iova>>32));

	if (ret != 0) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "get iova fail i:0x%llx o:0x%llx", ibuf_iova, obuf_iova);
		return ret;
	}

	if (!dec_hwlocked[id] || !dec_hw_enable[id]) {
		mutex_unlock(&jpeg_hybrid_dec_lock);
		JPEG_LOG(0, "hw %d invalid(isLocked: %d, isEnabled: %d), start fail",
			 id, dec_hwlocked[id], dec_hw_enable[id]);
		return -1;
	}

	*index_buf_fd = jpg_dmabuf_fd(bufInfo[id].o_dbuf);
	// get obuf for adding reference count, avoid early release in userspace.
	bufInfo[id].o_dbuf = jpg_dmabuf_get(*index_buf_fd);

	jpeg_axdomain_set(id);

	IMG_REG_WRITE(data[0], REG_JPGDEC_HYBRID_090(id));
	IMG_REG_WRITE(data[1], REG_JPGDEC_HYBRID_090(id));
	IMG_REG_WRITE(data[2], REG_JPGDEC_HYBRID_0FC(id));
	IMG_REG_WRITE(data[3], REG_JPGDEC_HYBRID_14C(id));
	IMG_REG_WRITE(data[4], REG_JPGDEC_HYBRID_150(id));
	IMG_REG_WRITE(data[5], REG_JPGDEC_HYBRID_154(id));
	IMG_REG_WRITE(data[6], REG_JPGDEC_HYBRID_17C(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)ibuf_iova, REG_JPGDEC_HYBRID_200(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)(ibuf_iova>>32), REG_JPGDEC_HYBRID_378(id));
	IMG_REG_WRITE(data[8], REG_JPGDEC_HYBRID_20C(id));
	IMG_REG_WRITE(data[9], REG_JPGDEC_HYBRID_210(id));
	IMG_REG_WRITE(data[10], REG_JPGDEC_HYBRID_224(id));
	IMG_REG_WRITE(data[11], REG_JPGDEC_HYBRID_23C(id));
	IMG_REG_WRITE(data[12], REG_JPGDEC_HYBRID_24C(id));
	IMG_REG_WRITE(data[13], REG_JPGDEC_HYBRID_270(id));
	IMG_REG_WRITE(data[14], REG_JPGDEC_HYBRID_31C(id));
	IMG_REG_WRITE(data[15], REG_JPGDEC_HYBRID_330(id));
	IMG_REG_WRITE(data[16], REG_JPGDEC_HYBRID_334(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)obuf_iova, REG_JPGDEC_HYBRID_338(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)(obuf_iova>>32), REG_JPGDEC_HYBRID_384(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)obuf_iova, REG_JPGDEC_HYBRID_36C(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)obuf_iova + data[20],
	REG_JPGDEC_HYBRID_370(id));
	IMG_REG_WRITE((unsigned long)(unsigned char *)obuf_iova + data[20]*2,
	REG_JPGDEC_HYBRID_374(id));
	IMG_REG_WRITE(data[17], REG_JPGDEC_HYBRID_33C(id));
	IMG_REG_WRITE(data[18], REG_JPGDEC_HYBRID_344(id));
	IMG_REG_WRITE(data[19], REG_JPGDEC_HYBRID_240(id));

	mutex_unlock(&jpeg_hybrid_dec_lock);

	JPEG_LOG(1, "-");
	return ret;
}

static void jpeg_drv_hybrid_dec_get_p_n_s(
								unsigned int id,
								int *progress_n_status)
{
	int progress, status;

	progress = IMG_REG_READ(REG_JPGDEC_HYBRID_340(id)) - 1;
	status = IMG_REG_READ(REG_JPGDEC_HYBRID_348(id));
	*progress_n_status = progress << 4 | status;
	JPEG_LOG(1, "progress_n_status %d", *progress_n_status);
}

static irqreturn_t jpeg_drv_hybrid_dec_isr(int irq, void *dev_id)
{
	int ret = 0;
	int i;

	JPEG_LOG(1, "JPEG Hybrid Decoder Interrupt %d", irq);
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		if (irq == gJpegqDev.hybriddecIrqId[i]) {
			if (!dec_hwlocked[i] || !dec_hw_enable[i]) {
				JPEG_LOG(0,
					"JPEG isr from invalid HW %d(isLocked: %d, isEnabled: %d)",
					i, dec_hwlocked[i], dec_hw_enable[i]);
				return IRQ_HANDLED;
			}
			ret = _jpeg_isr_hybrid_dec_lisr(i);
			if (ret == 0)
				wake_up_interruptible(
				&(hybrid_dec_wait_queue[i]));
			JPEG_LOG(1, "JPEG Hybrid Dec clear Interrupt %d ret %d"
					, irq, ret);
			break;
		}
	}

	return IRQ_HANDLED;
}

void jpeg_drv_hybrid_dec_prepare_dvfs(unsigned int id)
{
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	ret = dev_pm_opp_of_add_table(gJpegqDev.pDev[id]);
	if (ret < 0) {
		JPEG_LOG(0, "Failed to get opp table (%d)", ret);
		return;
	}

	gJpegqDev.jpeg_reg[id] = devm_regulator_get_optional(gJpegqDev.pDev[id],
						"mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(gJpegqDev.jpeg_reg[id])) {
		JPEG_LOG(0, "Failed to get regulator");
		gJpegqDev.jpeg_reg[id] = NULL;
		gJpegqDev.jpeg_dvfs[id] = devm_clk_get(gJpegqDev.pDev[id], "mmdvfs_clk");
		if (IS_ERR_OR_NULL(gJpegqDev.jpeg_dvfs[id])) {
			JPEG_LOG(0, "Failed to get mmdvfs clk");
			gJpegqDev.jpeg_dvfs[id] = NULL;
			return;
		}
	}

	gJpegqDev.jpeg_freq_cnt[id] = dev_pm_opp_get_opp_count(gJpegqDev.pDev[id]);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id], &freq))) {
		gJpegqDev.jpeg_freqs[id][i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

void jpeg_drv_hybrid_dec_unprepare_dvfs(void)
{
}

void jpeg_drv_hybrid_dec_start_dvfs(unsigned int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (gJpegqDev.jpeg_reg[id]) {
		JPEG_LOG(1, "request freq %lu",
				gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		opp = dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id],
		&gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(gJpegqDev.jpeg_reg[id], volt, INT_MAX);
		if (ret) {
			JPEG_LOG(0, "Failed to set regulator voltage %d",
			volt);
		}
	} else if (gJpegqDev.jpeg_dvfs[id]) {
		JPEG_LOG(1, "request freq %lu",
				gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		if (mmdvfs_get_version())
			mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_JPEGDEC);
		ret = clk_set_rate(gJpegqDev.jpeg_dvfs[id],
			gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		if (ret) {
			JPEG_LOG(0, "Failed to set freq %lu",
			gJpegqDev.jpeg_freqs[id][gJpegqDev.jpeg_freq_cnt[id]-1]);
		}
		if (mmdvfs_get_version())
			mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_JPEGDEC);
	}

}

void jpeg_drv_hybrid_dec_end_dvfs(unsigned int id)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;

	if (gJpegqDev.jpeg_reg[id]) {
		JPEG_LOG(1, "request freq %lu", gJpegqDev.jpeg_freqs[id][0]);
		opp = dev_pm_opp_find_freq_ceil(gJpegqDev.pDev[id],
					&gJpegqDev.jpeg_freqs[id][0]);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(gJpegqDev.jpeg_reg[id], volt, INT_MAX);
		if (ret) {
			JPEG_LOG(0, "Failed to set regulator voltage %d",
			volt);
		}
	} else if (gJpegqDev.jpeg_dvfs[id]) {
		JPEG_LOG(1, "request freq 0");
		ret = clk_set_rate(gJpegqDev.jpeg_dvfs[id], 0);
		if (ret)
			JPEG_LOG(0, "Failed to set freq 0");
	}
}

static void jpeg_drv_prepare_bw_request(unsigned int node_id)
{
	JPEG_LOG(1, "bw requset prepare");

	if (node_id == 0) {
		gJpegqDev.jpeg_path_wdma[0] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							     "path_jpegdec0_wdma");
		gJpegqDev.jpeg_path_bsdma[0] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							      "path_jpegdec0_bsdma");
		gJpegqDev.jpeg_path_huff_offset[0] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
								    "path_jpegdec0_huff_offset");
		gJpegqDev.jpeg_path_wdma[1] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							     "path_jpegdec1_wdma");
		gJpegqDev.jpeg_path_bsdma[1] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							      "path_jpegdec1_bsdma");
		gJpegqDev.jpeg_path_huff_offset[1] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
								    "path_jpegdec1_huff_offset");
	} else {
		gJpegqDev.jpeg_path_wdma[2] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							     "path_jpegdec2_wdma");
		gJpegqDev.jpeg_path_bsdma[2] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
							      "path_jpegdec2_bsdma");
		gJpegqDev.jpeg_path_huff_offset[2] = of_mtk_icc_get(gJpegqDev.pDev[node_id],
								    "path_jpegdec2_huff_offset");
	}
}

static void jpeg_drv_update_bw_request(unsigned int id)
{
	JPEG_LOG(1, "update bw request id: %d", id);

	mtk_icc_set_bw(gJpegqDev.jpeg_path_wdma[id], MBps_to_icc(960), 0);
	mtk_icc_set_bw(gJpegqDev.jpeg_path_bsdma[id], MBps_to_icc(576), 0);
	mtk_icc_set_bw(gJpegqDev.jpeg_path_huff_offset[id], MBps_to_icc(40), 0);
}

static void jpeg_drv_end_bw_request(unsigned int id)
{
	JPEG_LOG(1, "end bw request id: %d", id);

	mtk_icc_set_bw(gJpegqDev.jpeg_path_wdma[id], 0, 0);
	mtk_icc_set_bw(gJpegqDev.jpeg_path_bsdma[id], 0, 0);
	mtk_icc_set_bw(gJpegqDev.jpeg_path_huff_offset[id], 0, 0);
}

void jpeg_drv_hybrid_dec_power_on(int id)
{
	int ret;
	unsigned int ven_res;
	int cnt = 0;

	if (!dec_hwlocked[(id+1)%HW_CORE_NUMBER] && !dec_hwlocked[(id+2)%HW_CORE_NUMBER]) {
		if (gJpegqDev.jpegLarb[0]) {
			JPEG_LOG(1, "power on larb7");
			ret = pm_runtime_resume_and_get(gJpegqDev.jpegLarb[0]);
			if (ret)
				JPEG_LOG(0, "pm_runtime_resume_and_get failed %d",
					ret);
		}
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgDec);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC failed %d",
					ret);
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgDec_c1);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC_C1 failed %d",
					ret);
		if (gJpegqDev.ven0BaseVA) {
			IMG_REG_WRITE((BIT(0)), (gJpegqDev.ven0BaseVA + 0x10));
			ven_res = IMG_REG_READ((gJpegqDev.ven0BaseVA + 0x15C));
			while (!(ven_res & BIT(31))) {
				cnt++;
				if (cnt > 10000) {
					JPEG_LOG(0, "[Error] venc ddrsrc ack timeout 0x%08x",
						    ven_res);
					break;
				}
				udelay(10);
				ven_res = IMG_REG_READ((gJpegqDev.ven0BaseVA + 0x15C));
			}
		}
	}

	if (id == 2) {
		if (gJpegqDev.jpegLarb[1]) {
			JPEG_LOG(1, "power on larb8");
			ret = pm_runtime_resume_and_get(gJpegqDev.jpegLarb[1]);
			if (ret)
				JPEG_LOG(0, "pm_runtime_resume_and_get failed %d",
					ret);
		}
		ret = clk_prepare_enable(gJpegqDev.jpegClk.clk_venc_jpgDec_c2);
		if (ret)
			JPEG_LOG(0, "clk enable MT_CG_VENC_JPGDEC_C2 failed %d",
					ret);
		jpeg_drv_hybrid_dec_start_dvfs(1);
	} else
		jpeg_drv_hybrid_dec_start_dvfs(0);

	jpeg_drv_update_bw_request(id);

	JPEG_LOG(1, "JPEG Hybrid Decoder Power On %d", id);
}

void jpeg_drv_hybrid_dec_power_off(int id)
{
	jpeg_drv_end_bw_request(id);

	if (id == 2) {
		jpeg_drv_hybrid_dec_end_dvfs(1);
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgDec_c2);
		if (gJpegqDev.jpegLarb[1]) {
			pm_runtime_put_sync(gJpegqDev.jpegLarb[1]);
			JPEG_LOG(1, "Larb 8 Power Off (trigger id: %d)", id);
		}
	} else
		jpeg_drv_hybrid_dec_end_dvfs(0);

	if (!dec_hwlocked[(id+1)%HW_CORE_NUMBER] && !dec_hwlocked[(id+2)%3]) {
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgDec_c1);
		clk_disable_unprepare(gJpegqDev.jpegClk.clk_venc_jpgDec);
		if (gJpegqDev.jpegLarb[0]) {
			pm_runtime_put_sync(gJpegqDev.jpegLarb[0]);
			JPEG_LOG(1, "Larb 7 Power Off (trigger id: %d)", id);
		}
	}
}

static int jpeg_drv_hybrid_dec_lock(int *hwid)
{
	int retValue = 0;
	int id = 0;

	mutex_lock(&jpeg_hybrid_dec_lock);
	if (gJpegqDev.is_suspending || gJpegqDev.is_shutdowning) {
		JPEG_LOG(0, "jpeg dec is suspending or shutdowning, %d, %d",
			    gJpegqDev.is_suspending,
			    gJpegqDev.is_shutdowning);
		*hwid = -1;
		mutex_unlock(&jpeg_hybrid_dec_lock);
		return -EBUSY;
	}

	for (id = 0; id < HW_CORE_NUMBER; id++) {
		if (jpg_core_binding != -1 && jpg_core_binding != id) {
			JPEG_LOG(0, "jpeg skip HW core %d", id);
			continue;
		}

		if (dec_hwlocked[id] || !dec_hw_enable[id]) {
			JPEG_LOG(1,
				"jpeg dec HW core %d isn't available(isLocked: %d, isEnabled: %d)",
				 id, dec_hwlocked[id], dec_hw_enable[id]);
			continue;
		}

		*hwid = id;
		JPEG_LOG(1, "jpeg dec get %d HW core", id);
		_jpeg_hybrid_dec_int_status[id] = 0;
		jpeg_drv_hybrid_dec_power_on(id);
		dec_hwlocked[id] = true;
		enable_irq(gJpegqDev.hybriddecIrqId[id]);
		break;
	}

	mutex_unlock(&jpeg_hybrid_dec_lock);
	if (id == HW_CORE_NUMBER) {
		JPEG_LOG(1, "jpeg dec HW core all busy");
		*hwid = -1;
		retValue = -EBUSY;
	}

	return retValue;
}

static void _jpeg_drv_hybrid_dec_unlock(unsigned int hwid)
{
	disable_irq(gJpegqDev.hybriddecIrqId[hwid]);
	dec_hwlocked[hwid] = false;
	jpeg_drv_hybrid_dec_power_off(hwid);
	JPEG_LOG(1, "jpeg dec HW core %d is unlocked", hwid);
	jpg_dmabuf_free_iova(bufInfo[hwid].i_dbuf,
		bufInfo[hwid].i_attach,
		bufInfo[hwid].i_sgt);
	jpg_dmabuf_free_iova(bufInfo[hwid].o_dbuf,
		bufInfo[hwid].o_attach,
		bufInfo[hwid].o_sgt);
	jpg_dmabuf_put(bufInfo[hwid].i_dbuf);
	jpg_dmabuf_put(bufInfo[hwid].o_dbuf);
	bufInfo[hwid].i_dbuf = NULL;
	bufInfo[hwid].o_dbuf = NULL;
}

static void jpeg_drv_hybrid_dec_unlock(unsigned int hwid)
{
	mutex_lock(&jpeg_hybrid_dec_lock);
	if (!dec_hwlocked[hwid] || !dec_hw_enable[hwid]) {
		JPEG_LOG(0, "try to unlock a invalid core %d(isLocked: %d, isEnabled: %d)",
			 hwid, dec_hwlocked[hwid], dec_hw_enable[hwid]);
	} else {
		_jpeg_drv_hybrid_dec_unlock(hwid);
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);
}

static int jpeg_drv_hybrid_dec_suspend_notifier(
					struct notifier_block *nb,
					unsigned long action, void *data)
{
	int i;
	int wait_cnt = 0;

	JPEG_LOG(0, "action:%ld", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&jpeg_hybrid_dec_lock);
		gJpegqDev.is_suspending = 1;
		for (i = 0 ; i < HW_CORE_NUMBER; i++) {
			JPEG_LOG(1, "jpeg dec sn wait core %d", i);
			while (dec_hwlocked[i] && dec_hw_enable[i]) {
				JPEG_LOG(1, "jpeg dec sn core %d locked. wait...", i);
				usleep_range(10000, 20000);
				wait_cnt++;
				if (wait_cnt > 5) {
					JPEG_LOG(0, "jpeg dec sn unlock core %d", i);
					_jpeg_drv_hybrid_dec_unlock(i);
					break;
				}
			}
		}
		mutex_unlock(&jpeg_hybrid_dec_lock);

		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		gJpegqDev.is_suspending = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static int jpeg_drv_hybrid_dec_suspend(void)
{
	int i;

	JPEG_LOG(0, "+");
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		JPEG_LOG(1, "jpeg dec suspend core %d", i);
		if (dec_hwlocked[i] && dec_hw_enable[i]) {
			JPEG_LOG(0, "suspend unlock core %d\n", i);
			jpeg_drv_hybrid_dec_unlock(i);
		}
	}
	return 0;
}

static int jpeg_drv_hybrid_dec_get_status(int hwid)
{
	int p_n_s;

	p_n_s = -1;
	mutex_lock(&jpeg_hybrid_dec_lock);
	if (!dec_hwlocked[hwid] || !dec_hw_enable[hwid]) {
		JPEG_LOG(1, "hw %d invalid(isLocked: %d, isEnabled: %d), return -1 status",
			 hwid, dec_hwlocked[hwid], dec_hw_enable[hwid]);
	} else {
		JPEG_LOG(1, "get p_n_s @ hw %d", hwid);
		jpeg_drv_hybrid_dec_get_p_n_s(hwid, &p_n_s);
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);

	return p_n_s;
}

static unsigned int jpeg_get_node_index(const char *name)
{
	if (strncmp(name, "jpgdec0", strlen("jpgdec0")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return 0;
	} else if (strncmp(name, "jpgdec1", strlen("jpgdec1")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return 1;
	}
	JPEG_LOG(0, "name not found %s", name);
	return 0;
}

static void jpeg_check_dec_done(int hwid)
{
	long timeout_jiff;

	timeout_jiff = msecs_to_jiffies(3000);
	JPEG_LOG(1, "JPEG Hybrid Decoder Wait Resume Time: %ld",
			timeout_jiff);

	if (jpeg_isr_hybrid_dec_lisr(hwid) < 0) {
		long ret = 0;
		int waitfailcnt = 0;

		do {
			ret = wait_event_interruptible_timeout(
				hybrid_dec_wait_queue[hwid],
				_jpeg_hybrid_dec_int_status[hwid],
				timeout_jiff);
			if (ret == 0) {
				JPEG_LOG(0, "JPEG Hybrid Dec Wait timeout!");
				mutex_lock(&jpeg_hybrid_dec_lock);
				if (dec_hwlocked[hwid]) {
					jpeg_drv_hybrid_dec_dump_register_setting(hwid);

					/* trigger smi dump to get more info. */
					mtk_smi_dbg_hang_detect("JPEG DEC");
				} else {
					JPEG_LOG(0, "wait dec_hwlocked hw: %d", hwid);
				}
				mutex_unlock(&jpeg_hybrid_dec_lock);
			}
			if (ret < 0) {
				waitfailcnt++;
				JPEG_LOG(0,
				"JPEG Hybrid Dec Wait Error(%ld) %d",
				ret, waitfailcnt);
				usleep_range(10000, 20000);
			}
		} while (ret < 0 && waitfailcnt < 500);
	} else
		JPEG_LOG(1, "JPEG Hybrid Dec IRQ Wait Already Done!");
	_jpeg_hybrid_dec_int_status[hwid] = 0;
}

static int jpeg_hybrid_dec_ioctl(unsigned int cmd, unsigned long arg,
			struct file *file)
{
	struct JpegPrivData *priv_data;
	int hwid;
	int index_buf_fd;
	int progress_n_status;

	struct JPEG_DEC_DRV_HYBRID_TASK taskParams;
	struct JPEG_DEC_DRV_HYBRID_P_N_S pnsParmas;

	priv_data = (struct JpegPrivData *)file->private_data;

	if (priv_data == NULL) {
		JPEG_LOG
		(0, "Private data is null");
		return -EFAULT;
	}
	switch (cmd) {
	case JPEG_DEC_IOCTL_HYBRID_START:
		JPEG_LOG(1, "JPEG DEC IOCTL HYBRID START");
		if (copy_from_user(
			&taskParams, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_TASK))) {
			JPEG_LOG(0, "Copy from user error");
			return -EFAULT;
		}

		// JPEG oal magic number
		mutex_lock(&priv_data->state_lock);
		if (taskParams.timeout != 3000 || priv_data->state != JPEG_DEC_OPEN) {
			JPEG_LOG(0, "JPEG check error: oal magic number %ld, state: %d",
				 taskParams.timeout, priv_data->state);
			mutex_unlock(&priv_data->state_lock);
			return -EFAULT;
		}

		if (jpeg_drv_hybrid_dec_lock(&priv_data->hw_id) != 0) {
			JPEG_LOG(1, "jpeg_drv_hybrid_dec_lock failed (hw busy)");
			mutex_unlock(&priv_data->state_lock);
			return -EBUSY;
		}

		priv_data->state = JPEG_DEC_POWER_ON;

		if (jpeg_drv_hybrid_dec_start(taskParams.data, priv_data->hw_id, &index_buf_fd) != 0) {
			JPEG_LOG(0, "jpeg_drv_dec_hybrid_start failed");
			jpeg_drv_hybrid_dec_unlock(priv_data->hw_id);
			priv_data->state = JPEG_DEC_POWER_OFF;
			mutex_unlock(&priv_data->state_lock);
			return -EFAULT;
		}

		priv_data->state = JPEG_DEC_DECODE;
		mutex_unlock(&priv_data->state_lock);

		JPEG_LOG(1, "jpeg_drv_hybrid_dec_start success %u index buf fd:%d",
			priv_data->hw_id, index_buf_fd);
		if (copy_to_user(
			taskParams.hwid, &priv_data->hw_id, sizeof(int))) {
			JPEG_LOG(0, "Copy to user error");
			return -EFAULT;
		}
		if (copy_to_user(
			taskParams.index_buf_fd, &index_buf_fd, sizeof(int))) {
			JPEG_LOG(0, "Copy to user error");
			return -EFAULT;
		}
		break;

	case JPEG_DEC_IOCTL_HYBRID_WAIT:
		JPEG_LOG(1, "JPEG DEC IOCTL HYBRID WAIT");
		mutex_lock(&priv_data->state_lock);
		if (priv_data->state != JPEG_DEC_DECODE) {
			JPEG_LOG(0,
				 "Permission Denied! This process cannot access decoder(%d)",
				 priv_data->state);
			mutex_unlock(&priv_data->state_lock);
			return -EFAULT;
		}

		priv_data->state = JPEG_DEC_WAITING;
		mutex_unlock(&priv_data->state_lock);

		if (copy_from_user(
			&pnsParmas, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_P_N_S))) {
			JPEG_LOG(0, "Copy from user error");
			return -EFAULT;
		}

		hwid = pnsParmas.hwid;
		if (hwid < 0 || hwid >= HW_CORE_NUMBER || !dec_hw_enable[hwid] ||
		    hwid != priv_data->hw_id) {
			JPEG_LOG(0, "get hybrid dec id(%d) failed", hwid);
			return -EFAULT;
		}

	#ifdef FPGA_VERSION
		JPEG_LOG(1, "Polling JPEG Hybrid Dec Status hwid: %d",
				hwid);

		do {
			_jpeg_hybrid_dec_int_status[hwid] =
			IMG_REG_READ(REG_JPGDEC_HYBRID_INT_STATUS(hwid));
			JPEG_LOG(1, "Hybrid Polling status %d",
			_jpeg_hybrid_dec_int_status[hwid]);
		} while (_jpeg_hybrid_dec_int_status[hwid] == 0);

	#else
		jpeg_check_dec_done(hwid);
	#endif
		/* check error flow */
		priv_data->state = JPEG_DEC_DECODE_DONE;

		progress_n_status = jpeg_drv_hybrid_dec_get_status(hwid);
		JPEG_LOG(1, "jpeg_drv_hybrid_dec_get_status %d", progress_n_status);

		if (copy_to_user(
			pnsParmas.progress_n_status, &progress_n_status, sizeof(int))) {
			JPEG_LOG(0, "Copy to user error");
			return -EFAULT;
		}

		mutex_lock(&jpeg_hybrid_dec_lock);
		if (dec_hwlocked[hwid]) {
			IMG_REG_WRITE(0x0, REG_JPGDEC_HYBRID_090(hwid));
			IMG_REG_WRITE(0x00000010, REG_JPGDEC_HYBRID_090(hwid));
		}
		mutex_unlock(&jpeg_hybrid_dec_lock);

		jpeg_drv_hybrid_dec_unlock(hwid);
		priv_data->state = JPEG_DEC_POWER_OFF;
		break;

	case JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		JPEG_LOG(1, "JPEG DEC IOCTL HYBRID GET PROGRESS N STATUS");

		if (!JPEG_HAS_TRIGGER_DEC(priv_data->state)) {
			JPEG_LOG(0,
				 "Permission Denied! This process cannot access decoder(%d)",
				 priv_data->state);
			return -EFAULT;
		}

		if (copy_from_user(
			&pnsParmas, (void *)arg,
			sizeof(struct JPEG_DEC_DRV_HYBRID_P_N_S))) {
			JPEG_LOG(0, "JPEG Decoder : Copy from user error");
			return -EFAULT;
		}

		hwid = pnsParmas.hwid;
		if (hwid < 0 || hwid >= HW_CORE_NUMBER || hwid != priv_data->hw_id) {
			JPEG_LOG(0, "get P_N_S hwid invalid");
			return -EFAULT;
		}
		progress_n_status = jpeg_drv_hybrid_dec_get_status(hwid);

		if (copy_to_user(
			pnsParmas.progress_n_status, &progress_n_status, sizeof(int))) {
			JPEG_LOG(0, "JPEG Decoder: Copy to user error");
			return -EFAULT;
		}
		break;
	default:
		JPEG_LOG(0, "JPEG DEC IOCTL NO THIS COMMAND");
		break;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static int compat_get_jpeg_hybrid_task_data(
		 unsigned long arg,
		 struct JPEG_DEC_DRV_HYBRID_TASK *data)
{
	long ret = -1;
	int i;
	struct compat_JPEG_DEC_DRV_HYBRID_TASK data32;

	ret = (long)copy_from_user(&data32, compat_ptr(arg),
		(unsigned long)sizeof(struct compat_JPEG_DEC_DRV_HYBRID_TASK));
	if (ret != 0L) {
		JPEG_LOG(0, "Copy data from user failed!\n");
		return -EINVAL;
	}

	data->timeout = data32.timeout;
	data->hwid  = compat_ptr(data32.hwid);
	data->index_buf_fd   = compat_ptr(data32.index_buf_fd);

	for (i = 0; i < 21; i++)
		data->data[i] = data32.data[i];

	return 0;
}

static int compat_get_jpeg_hybrid_pns_data(
		 unsigned long arg,
		 struct JPEG_DEC_DRV_HYBRID_P_N_S *data)
{
	long ret = -1;
	struct compat_JPEG_DEC_DRV_HYBRID_P_N_S data32;

	ret = (long)copy_from_user(&data32, compat_ptr(arg),
		(unsigned long)sizeof(struct compat_JPEG_DEC_DRV_HYBRID_P_N_S));
	if (ret != 0L) {
		JPEG_LOG(0, "Copy data from user failed!\n");
		return -EINVAL;
	}

	data->hwid  = data32.hwid;
	data->progress_n_status   = compat_ptr(data32.progress_n_status);

	return 0;
}

static long compat_jpeg_ioctl(
		 struct file *filp,
		 unsigned int cmd,
		 unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_START:
		{
			struct JPEG_DEC_DRV_HYBRID_TASK data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_START");
			err = compat_get_jpeg_hybrid_task_data(arg, &data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_HYBRID_START,
					(unsigned long)&data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT:
		{
			struct JPEG_DEC_DRV_HYBRID_P_N_S data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_WAIT");
			err = compat_get_jpeg_hybrid_pns_data(arg, &data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(
					filp, JPEG_DEC_IOCTL_HYBRID_WAIT,
					(unsigned long)&data);
			return ret ? ret : err;
		}
	case COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		{
			struct JPEG_DEC_DRV_HYBRID_P_N_S data;
			int err;

			JPEG_LOG(1, "COMPAT_JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS");
			err = compat_get_jpeg_hybrid_pns_data(arg, &data);
			if (err)
				return err;
			ret =
			filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS,
					(unsigned long)&data);
			return ret ? ret : err;
		}
	default:
		return -ENOIOCTLCMD;
	}
}
#endif

static long jpeg_unlocked_ioctl(
	struct file *file,
	 unsigned int cmd,
	 unsigned long arg)
{
	switch (cmd) {
	case JPEG_DEC_IOCTL_HYBRID_START:
	case JPEG_DEC_IOCTL_HYBRID_WAIT:
	case JPEG_DEC_IOCTL_HYBRID_GET_PROGRESS_STATUS:
		return jpeg_hybrid_dec_ioctl(cmd, arg, file);
	default:
		break;
	}
	return -EINVAL;
}

static int jpeg_open(struct inode *inode, struct file *file)
{
	struct JpegPrivData *priv_data;

	JPEG_LOG(1, "jpeg_open %p", file);

	/* Allocate and initialize private data */
	file->private_data = kmalloc(sizeof(struct JpegPrivData), GFP_ATOMIC);

	if (file->private_data == NULL) {
		JPEG_LOG(0, "Not enough entry for JPEG open operation");
		return -ENOMEM;
	}

	priv_data = (struct JpegPrivData *)file->private_data;
	priv_data->state = JPEG_DEC_OPEN;
	mutex_init(&priv_data->state_lock);
	priv_data->hw_id = -1;

	return 0;
}

static ssize_t jpeg_read(
	struct file *file,
	 char __user *data,
	 size_t len, loff_t *ppos)
{
	JPEG_LOG(1, "jpeg driver read");
	return 0;
}

static int jpeg_release(struct inode *inode, struct file *file)
{
	struct JpegPrivData *priv_dat;
	int hwid;

	JPEG_LOG(1, "jpeg_release %p", file);

	priv_dat = (struct JpegPrivData *)file->private_data;
	if (priv_dat == NULL) {
		JPEG_LOG(0, "Private data is null");
		return 0;
	}

	hwid = priv_dat->hw_id;
	JPEG_LOG(1, "hw_id: %d, pStatus: %d", hwid, priv_dat->state);
	if (hwid != -1 && priv_dat->state != JPEG_DEC_POWER_OFF) {
		JPEG_LOG(1, "dec_hwlocked[%d]: %d, dec_hw_enable[%d]: %d",
			    hwid, dec_hwlocked[hwid], hwid, dec_hw_enable[hwid]);
		if (dec_hwlocked[hwid] && dec_hw_enable[hwid]) {
			JPEG_LOG(0, "try to close jpegdec %d, state: %d", hwid, priv_dat->state);
			jpeg_check_dec_done(hwid);

			/* reset */
			mutex_lock(&jpeg_hybrid_dec_lock);
			if (dec_hwlocked[hwid] && dec_hw_enable[hwid]) {
				IMG_REG_WRITE(0x0, REG_JPGDEC_HYBRID_090(hwid));
				IMG_REG_WRITE(0x00000010, REG_JPGDEC_HYBRID_090(hwid));
			}
			mutex_unlock(&jpeg_hybrid_dec_lock);

			jpeg_drv_hybrid_dec_unlock(hwid);
			priv_dat->state = JPEG_DEC_POWER_OFF;
		}
	}

	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

/* Kernel interface */
static const struct proc_ops jpeg_fops = {
	.proc_ioctl = jpeg_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = compat_jpeg_ioctl,
#endif
	.proc_open = jpeg_open,
	.proc_release = jpeg_release,
	.proc_read = jpeg_read,
	.proc_lseek = no_llseek,
};

const long jpeg_dev_get_hybrid_decoder_base_VA(int id)
{
	return gJpegqDev.hybriddecRegBaseVA[id];
}

static int jpeg_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL, *larbnode = NULL;
	struct platform_device *larbdev;
	int i, node_index, ret;
	unsigned int ven0_base_idx, larb_base_idx;

	JPEG_LOG(0, "JPEG Probe");
	atomic_inc(&nodeCount);

	node_index = jpeg_get_node_index(pdev->dev.of_node->name);

	if (atomic_read(&nodeCount) == 1)
		memset(&gJpegqDev, 0x0, sizeof(struct JpegDeviceStruct));
	gJpegqDev.pDev[node_index] = &pdev->dev;
	gJpegqDev.smmu_dev[node_index] = mtk_smmu_get_shared_device(&pdev->dev);

	node = pdev->dev.of_node;

	if (node_index == 0) {
		for (i = 0; i < HW_CORE_NUMBER - 1; i++) {
			bufInfo[i].o_dbuf = NULL;
			bufInfo[i].o_attach = NULL;
			bufInfo[i].o_sgt = NULL;

			bufInfo[i].i_dbuf = NULL;
			bufInfo[i].i_attach = NULL;
			bufInfo[i].i_sgt = NULL;

			gJpegqDev.hybriddecRegBaseVA[i] =
				(unsigned long)of_iomap(node, i);

			gJpegqDev.hybriddecIrqId[i] = irq_of_parse_and_map(node, i);
			JPEG_LOG(0, "Jpeg Hybrid Dec Probe %d base va 0x%lx irqid %d",
				i, gJpegqDev.hybriddecRegBaseVA[i],
				gJpegqDev.hybriddecIrqId[i]);

			JPEG_LOG(0, "Request irq %d", gJpegqDev.hybriddecIrqId[i]);
			init_waitqueue_head(&(hybrid_dec_wait_queue[i]));
			if (request_irq(gJpegqDev.hybriddecIrqId[i],
				jpeg_drv_hybrid_dec_isr, IRQF_TRIGGER_HIGH,
				"jpeg_dec_driver", NULL))
				JPEG_LOG(0, "JPEG Hybrid DEC requestirq %d failed", i);
			disable_irq(gJpegqDev.hybriddecIrqId[i]);
		}
		gJpegqDev.jpegClk.clk_venc_jpgDec =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgDec))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC clk error!");

		gJpegqDev.jpegClk.clk_venc_jpgDec_c1 =
			of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC_C1");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgDec_c1))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC_C1 clk error!");

		ret = of_property_read_u32(pdev->dev.of_node,
					   "jpeg-set-resource",
					   &ven0_base_idx);
		if (ret == 0) {
			gJpegqDev.ven0BaseVA = (unsigned long)of_iomap(node, ven0_base_idx);
			JPEG_LOG(0, "jpeg enable ven0 resource set, base: 0x%lx",
				    gJpegqDev.ven0BaseVA);
		} else {
			gJpegqDev.ven0BaseVA = 0;
			JPEG_LOG(0, "jpeg disable ven0 resource set, %d", ret);
		}

		ret = of_property_read_u32(pdev->dev.of_node,
					   "jpeg-set-axdomain-base-idx",
					   &larb_base_idx);
		if (ret == 0)
			gJpegqDev.smiLarbBaseVA[0] = (unsigned long)of_iomap(node, larb_base_idx);
		else
			gJpegqDev.smiLarbBaseVA[0] = 0;
		JPEG_LOG(0, "larb base: 0x%lx", gJpegqDev.smiLarbBaseVA[0]);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "jpeg-set-axdomain",
					   &gJpegqDev.axdomain[0]);
		if (ret == 0)
			JPEG_LOG(0, "axdomain: 0x%x", gJpegqDev.axdomain[0]);
	} else {
		i = HW_CORE_NUMBER - 1;

		bufInfo[i].o_dbuf = NULL;
		bufInfo[i].o_attach = NULL;
		bufInfo[i].o_sgt = NULL;

		bufInfo[i].i_dbuf = NULL;
		bufInfo[i].i_attach = NULL;
		bufInfo[i].i_sgt = NULL;

		gJpegqDev.hybriddecRegBaseVA[i] =
		(unsigned long)of_iomap(node, 0);

		gJpegqDev.hybriddecIrqId[i] =
		irq_of_parse_and_map(node, 0);
		JPEG_LOG(0, "Jpeg Hybrid Dec Probe %d base va 0x%lx irqid %d",
		i,
		gJpegqDev.hybriddecRegBaseVA[i],
		gJpegqDev.hybriddecIrqId[i]);

		JPEG_LOG(0, "Request irq %d", gJpegqDev.hybriddecIrqId[i]);
		init_waitqueue_head(&(hybrid_dec_wait_queue[i]));
		if (request_irq(gJpegqDev.hybriddecIrqId[i],
		    jpeg_drv_hybrid_dec_isr, IRQF_TRIGGER_HIGH,
		    "jpeg_dec_driver", NULL))
			JPEG_LOG(0, "JPEG Hybrid DEC requestirq %d failed", i);
		disable_irq(gJpegqDev.hybriddecIrqId[i]);
		gJpegqDev.jpegClk.clk_venc_jpgDec_c2 =
		of_clk_get_by_name(node, "MT_CG_VENC_JPGDEC_C2");
		if (IS_ERR(gJpegqDev.jpegClk.clk_venc_jpgDec_c2))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC_C2 clk error!");

		ret = of_property_read_u32(pdev->dev.of_node,
					   "jpeg-set-axdomain-base-idx",
					   &larb_base_idx);
		if (ret == 0)
			gJpegqDev.smiLarbBaseVA[1] = (unsigned long)of_iomap(node, larb_base_idx);
		else
			gJpegqDev.smiLarbBaseVA[1] = 0;
		JPEG_LOG(0, "larb base: 0x%lx", gJpegqDev.smiLarbBaseVA[1]);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "jpeg-set-axdomain",
					   &gJpegqDev.axdomain[1]);
		if (ret == 0)
			JPEG_LOG(0, "axdomain: 0x%x", gJpegqDev.axdomain[1]);
	}

	larbnode = of_parse_phandle(node, "mediatek,larbs", 0);
	if (!larbnode) {
		JPEG_LOG(0, "fail to get larbnode %d", node_index);
	} else {
		larbdev = of_find_device_by_node(larbnode);
		if (WARN_ON(!larbdev)) {
			of_node_put(larbnode);
			JPEG_LOG(0, "fail to get larbdev %d", node_index);
			return -1;
		}
		gJpegqDev.jpegLarb[node_index] = &larbdev->dev;
		JPEG_LOG(0, "get larb from node %d", node_index);

		if (!device_link_add(gJpegqDev.pDev[node_index], gJpegqDev.jpegLarb[node_index],
			DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS)) {
			JPEG_LOG(0, "larb device link fail");
			return -1;
		}
	}

	if (!smmu_v3_enabled()) {
		JPEG_LOG(0, "iommu arch");
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
		if (ret) {
			JPEG_LOG(0, "64-bit DMA enable failed");
			return ret;
		}
		if (!pdev->dev.dma_parms) {
			pdev->dev.dma_parms =
			devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
		}
		if (pdev->dev.dma_parms) {
			ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(34));
			if (ret)
				JPEG_LOG(0, "Failed to set DMA segment size\n");
		}
	}
	pm_runtime_enable(&pdev->dev);

	jpeg_drv_hybrid_dec_prepare_dvfs(node_index);
	jpeg_drv_prepare_bw_request(node_index);

	if (atomic_read(&nodeCount) == 1) {
		gJpegqDev.pm_notifier.notifier_call = jpeg_drv_hybrid_dec_suspend_notifier;
		register_pm_notifier(&gJpegqDev.pm_notifier);
		gJpegqDev.is_suspending = 0;
		gJpegqDev.is_shutdowning = 0;
		memset(_jpeg_hybrid_dec_int_status, 0, HW_CORE_NUMBER);
		proc_create("mtk_jpeg", 0x644, NULL, &jpeg_fops);

		dec_hw_enable[0] = true;
		dec_hw_enable[1] = true;
	} else {
		dec_hw_enable[2] = true;
	}

	JPEG_LOG(0, "JPEG Probe Done");

	return 0;
}

static int jpeg_remove(struct platform_device *pdev)
{
	int i, node_index;

	JPEG_LOG(0, "JPEG Codec remove");
	atomic_dec(&nodeCount);

	node_index = jpeg_get_node_index(pdev->dev.of_node->name);

	if (node_index == 0) {
		for (i = 0; i < HW_CORE_NUMBER - 1; i++) {
			free_irq(gJpegqDev.hybriddecIrqId[i], NULL);
			dec_hw_enable[i] = false;
		}
	} else {
		i = HW_CORE_NUMBER - 1;
		free_irq(gJpegqDev.hybriddecIrqId[i], NULL);
		dec_hw_enable[i] = false;
	}
	jpeg_drv_hybrid_dec_unprepare_dvfs();

	return 0;
}

static void jpeg_shutdown(struct platform_device *pdev)
{
	int i;
	int wait_cnt = 0;
	JPEG_LOG(0, "JPEG Codec shutdown");

	mutex_lock(&jpeg_hybrid_dec_lock);
	gJpegqDev.is_shutdowning = true;
	for (i = 0 ; i < HW_CORE_NUMBER; i++) {
		if (dec_hw_enable[i]) {
			while (dec_hwlocked[i]) {
				JPEG_LOG(1, "jpeg dec sn core %d locked. wait...", i);
				usleep_range(10000, 20000);
				wait_cnt++;
				if (wait_cnt > 5) {
					JPEG_LOG(0, "jpeg dec sn unlock core %d", i);
					_jpeg_drv_hybrid_dec_unlock(i);
					break;
				}
			}
			dec_hw_enable[i] = false;
			JPEG_LOG(0, "jpeg dec shutdown core %d", i);
		}
	}
	mutex_unlock(&jpeg_hybrid_dec_lock);
}

/* PM suspend */
static int jpeg_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int ret;

	ret = jpeg_drv_hybrid_dec_suspend();
	if (ret != 0)
		return ret;
	return 0;
}

/* PM resume */
static int jpeg_resume(struct platform_device *pdev)
{
	return 0;
}

static int jpeg_pm_suspend(struct device *pDevice)
{
	struct platform_device *pdev = to_platform_device(pDevice);

	WARN_ON(pdev == NULL);

	return jpeg_suspend(pdev, PMSG_SUSPEND);
}

static int jpeg_pm_resume(struct device *pDevice)
{
	struct platform_device *pdev = to_platform_device(pDevice);

	WARN_ON(pdev == NULL);

	return jpeg_resume(pdev);
}

static int jpeg_pm_restore_noirq(struct device *pDevice)
{
	return 0;
}

static const struct dev_pm_ops jpeg_pm_ops = {
	.suspend = jpeg_pm_suspend,
	.resume = jpeg_pm_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = jpeg_pm_restore_noirq,
};

static struct platform_driver jpeg_driver = {
	.probe = jpeg_probe,
	.remove = jpeg_remove,
	.shutdown = jpeg_shutdown,
	.suspend = jpeg_suspend,
	.resume = jpeg_resume,
	.driver = {
		.name = JPEG_DEVNAME,
		.pm = &jpeg_pm_ops,
		.of_match_table = jdec_hybrid_of_ids,
	},
};

static int __init jpeg_init(void)
{
	int ret;

	JPEG_LOG(0, "Register the JPEG Codec driver");
	atomic_set(&nodeCount, 0);
	if (platform_driver_register(&jpeg_driver)) {
		JPEG_LOG(0, "failed to register jpeg codec driver");
		ret = -ENODEV;
		return ret;
	}

	return 0;
}

static void __exit jpeg_exit(void)
{
	remove_proc_entry("mtk_jpeg", NULL);
	platform_driver_unregister(&jpeg_driver);
}
module_init(jpeg_init);
module_exit(jpeg_exit);
MODULE_AUTHOR("Jason Hsu <yeong-cherng.hsu@mediatek.com>");
MODULE_DESCRIPTION("JPEG Dec Codec Driver");
MODULE_LICENSE("GPL");
