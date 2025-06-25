// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <audio_task_manager.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>

#include <adsp_helper.h>
#include <audio_ipi_platform.h>
#include <audio_ipi_platform_auto.h>
#include <audio_messenger_ipi.h>

#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "mtk-dsp-platform-driver.h"
#include "mtk-base-afe.h"

#include <mt-plat/mtk_irq_mon.h>
#include <linux/tracepoint.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/namei.h>
#include "mtk-dsp-platform-driver-auto.h"

static DEFINE_SPINLOCK(dsp_ringbuf_lock);

static struct mtk_dsp_evt_cb dsp_evt_cb;

static uint64_t irq_cnt[TASK_SCENE_SIZE] = {0};

virt_ipi_cb_func virt_ipi_cb;
virt_adsp_reg_feature_cb_func virt_adsp_reg_feature_cb;
virt_adsp_query_status_cb_func virt_adsp_query_status_cb;
virt_adsp_dump_cb_func virt_adsp_dump_cb;

static int is_guest_dsp_task[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]        = 0,
	[AUDIO_TASK_PRIMARY_ID]     = 1,
	[AUDIO_TASK_OFFLOAD_ID]     = 0,
	[AUDIO_TASK_DEEPBUFFER_ID]  = 1,
	[AUDIO_TASK_PLAYBACK_ID]    = 1,
	[AUDIO_TASK_MUSIC_ID]       = 1,
	[AUDIO_TASK_CAPTURE_RAW_ID] = 1,
	[AUDIO_TASK_CAPTURE_UL1_ID] = 1,
	[AUDIO_TASK_A2DP_ID]        = 1,
	[AUDIO_TASK_BLEDL_ID]       = 0,
	[AUDIO_TASK_BLEUL_ID]       = 0,
	[AUDIO_TASK_BTDL_ID]        = 0,
	[AUDIO_TASK_BTUL_ID]        = 0,
	[AUDIO_TASK_DATAPROVIDER_ID] = 1,
	[AUDIO_TASK_CALL_FINAL_ID]  = 0,
	[AUDIO_TASK_FAST_ID]        = 1,
	[AUDIO_TASK_KTV_ID]         = 0,
	[AUDIO_TASK_FM_ADSP_ID]     = 1,
	[AUDIO_TASK_UL_PROCESS_ID]  = 1,
	[AUDIO_TASK_ECHO_REF_ID]    = 1,
	[AUDIO_TASK_ECHO_REF_DL_ID] = 1,
	[AUDIO_TASK_USBDL_ID]       = 0,
	[AUDIO_TASK_USBUL_ID]       = 0,
	[AUDIO_TASK_MDDL_ID]        = 0,
	[AUDIO_TASK_MDUL_ID]        = 0,
	[AUDIO_TASK_SPATIALIZER_ID] = 0,
	[AUDIO_TASK_CALLDL_ID]      = 0,
	[AUDIO_TASK_CALLUL_ID]      = 0,
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]  = 1,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = 1,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]    = 1,
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]    = 1,
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]    = 1,
#endif
	[AUDIO_TASK_PLAYBACK0_ID]   = 1,
	[AUDIO_TASK_PLAYBACK1_ID]   = 1,
	[AUDIO_TASK_PLAYBACK2_ID]   = 1,
	[AUDIO_TASK_PLAYBACK3_ID]   = 1,
	[AUDIO_TASK_PLAYBACK4_ID]   = 1,
	[AUDIO_TASK_PLAYBACK5_ID]   = 1,
	[AUDIO_TASK_PLAYBACK6_ID]   = 1,
	[AUDIO_TASK_PLAYBACK7_ID]   = 1,
	[AUDIO_TASK_PLAYBACK8_ID]   = 1,
	[AUDIO_TASK_PLAYBACK9_ID]   = 1,
	[AUDIO_TASK_PLAYBACK10_ID]  = 1,
	[AUDIO_TASK_PLAYBACK11_ID]  = 1,
	[AUDIO_TASK_PLAYBACK12_ID]  = 1,
	[AUDIO_TASK_PLAYBACK13_ID]  = 1,
	[AUDIO_TASK_PLAYBACK14_ID]  = 1,
	[AUDIO_TASK_PLAYBACK15_ID]  = 1,
	[AUDIO_TASK_CAPTURE_MCH_ID]  = 1,
#endif
};

#ifdef DEBUG_VERBOSE
static unsigned long long get_cntpct(void)
{
	unsigned long long cval;

	asm volatile("mrs %0, cntpct_el0" : "=r" (cval));
	return cval;
}
#endif

void mtk_dsp_register_event_cb(adsp_evt_cb_func cb, void *data)
{
	dsp_evt_cb.cb = cb;
	dsp_evt_cb.data = data;
}
EXPORT_SYMBOL(mtk_dsp_register_event_cb);

void register_virt_ipi_cb(virt_ipi_cb_func cb)
{
	virt_ipi_cb = cb;
	pr_info("[VADSP] %s() cb: %p\n", __func__, cb);
}
EXPORT_SYMBOL(register_virt_ipi_cb);

void register_virt_adsp_reg_feature_cb(virt_adsp_reg_feature_cb_func cb)
{
	virt_adsp_reg_feature_cb = cb;
	pr_info("[VADSP] %s() cb: %p\n", __func__, cb);
}
EXPORT_SYMBOL(register_virt_adsp_reg_feature_cb);

void register_virt_adsp_query_status_cb(virt_adsp_query_status_cb_func cb)
{
	virt_adsp_query_status_cb = cb;
	pr_info("[VADSP] %s() cb: %p\n", __func__, cb);
}
EXPORT_SYMBOL(register_virt_adsp_query_status_cb);

void register_virt_adsp_dump_cb(virt_adsp_dump_cb_func cb)
{
	virt_adsp_dump_cb = cb;
	pr_info("[VADSP] %s() cb: %p\n", __func__, cb);
}
EXPORT_SYMBOL(register_virt_adsp_dump_cb);

void guest_adsp_irq_notify(int core_id, int dsp_scene, int xrun)
{
	if (dsp_evt_cb.cb) {
		struct adsp_evt_cb_data data;

		data.evt_type = ADSP_EVT_IRQ;
		data.irq.core = core_id;
		data.irq.task = dsp_scene;
		data.irq.xrun = xrun;
		data.data = dsp_evt_cb.data;

		if (dsp_scene > 0 && dsp_scene < TASK_SCENE_SIZE)
			irq_cnt[dsp_scene]++;

		dsp_evt_cb.cb(&data);
	}
}

void guest_adsp_task_share_dram_notify(int dsp_scene,
		int type, unsigned long long phy_addr, unsigned long long size)
{
	if (dsp_evt_cb.cb) {
		struct adsp_evt_cb_data data;

		data.evt_type = ADSP_EVT_MEM;
		data.mem.task = dsp_scene;
		data.mem.type = type;
		data.mem.phy_addr = phy_addr;
		data.mem.size = size;
		data.data = dsp_evt_cb.data;

		dsp_evt_cb.cb(&data);
	}
}

void guest_adsp_task_ipi_notify(struct vadsp_dump_buffer_info_t *dump_info)
{
	if (dsp_evt_cb.cb) {
		struct adsp_evt_cb_data data;

		data.evt_type = ADSP_EVT_IPI;
		data.ipi_info.phy_addr = dump_info->phys_addr_base;
		data.ipi_info.buffer_size = dump_info->buffer_size;
		data.ipi_info.write_offset = dump_info->write_offset;
		data.ipi_info.write_size = dump_info->write_size;
		data.ipi_info.phys_rp_addr = dump_info->phys_rp_addr;
		data.data = dsp_evt_cb.data;

		dsp_evt_cb.cb(&data);
	}
}

static void guest_adsp_ul_handler(struct mtk_base_dsp *dsp,
			int id, int core_id, int xrun)
{
	struct mtk_base_dsp_mem *dsp_mem;
	void *ipi_audio_buf;
	unsigned long flags;
	int dsp_scene = get_dspscene_by_dspdaiid(id);

	if (id < 0 || id >= AUDIO_TASK_DAI_NUM)
		return;

	dsp_mem = &dsp->dsp_mem[id];

	if (!dsp->dsp_mem[id].substream) {
		pr_info("%s substream NULL\n", __func__);
		return;
	}


	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* upadte for write index*/
	ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	dsp_mem->adsp_buf.aud_buffer.buf_bridge.pWrite =
		(dsp_mem->adsp_work_buf.aud_buffer.buf_bridge.pWrite);
#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_work_buf.aud_buffer.buf_bridge);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	sync_ringbuf_writeidx(&dsp_mem->ring_buf,
			      &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif

	/* notify subsream */
	// snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
	return guest_adsp_irq_notify(core_id, dsp_scene, xrun);
DSP_IRQ_HANDLER_ERR:
	return;
}

static void guest_adsp_dl_consume_handler(struct mtk_base_dsp *dsp,
			int id, int core_id, int xrun, int reset)
{
	unsigned long flags;
	void *ipi_audio_buf;
	int dsp_scene = get_dspscene_by_dspdaiid(id);

	struct mtk_base_dsp_mem *dsp_mem;

	if (id < 0 || id >= AUDIO_TASK_DAI_NUM)
		return;

	dsp_mem = &dsp->dsp_mem[id];

	if (!dsp->dsp_mem[id].substream) {
		pr_info_ratelimited("%s substream NULL id[%d]\n", __func__, id);
		return;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info_ratelimited("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		return;
	}

	/* adsp reset message */
	if (reset) {
		pr_info("%s adsp resert id = %d\n", __func__, id);
		RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);
		/* notify subsream */
		// return snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
		return guest_adsp_irq_notify(core_id, dsp_scene, xrun);
	}

	/* adsp reset message */
	if (xrun) {
		pr_info("%s adsp underflowed id = %d\n", __func__, id);
		dsp->dsp_mem[id].underflowed = true;
		/* notify subsream */
		// return snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
		return guest_adsp_irq_notify(core_id, dsp_scene, xrun);
	}

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	/* upadte for write index*/
	ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge.pRead =
	    dsp->dsp_mem[id].adsp_work_buf.aud_buffer.buf_bridge.pRead;

#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume before sync", &dsp->dsp_mem[id].ring_buf);
#endif

	sync_ringbuf_readidx(
		&dsp->dsp_mem[id].ring_buf,
		&dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge);

	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE_IRQ
	pr_info("%s id = %d\n", __func__, id);
	dump_rbuf_s("dl_consume", &dsp->dsp_mem[id].ring_buf);
#endif
	/* notify subsream */
	//snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
	guest_adsp_irq_notify(core_id, dsp_scene, xrun);
}

int is_guest_audio_task(int task_id)
{
	if (task_id >= 0 && task_id < AUDIO_TASK_DAI_NUM)
		return is_guest_dsp_task[task_id];
	return 0;
}

int is_guest_ul_audio_task(int task_id)
{
	if (task_id == TASK_SCENE_CAPTURE_UL1
		|| task_id == TASK_SCENE_CAPTURE_RAW
		|| task_id == TASK_SCENE_UL_PROCESS)
		return 1;
	return 0;
}


void guest_adsp_irq_handler(struct mtk_base_dsp *dsp,
		     int core_id, int id, int xrun)
{
	int dsp_scene = get_dspscene_by_dspdaiid(id);
	int is_ul_task = is_guest_ul_audio_task(dsp_scene);

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return;
	}

	if (!is_audio_task_dsp_ready(dsp_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return;
	}

	if (is_ul_task)
		guest_adsp_ul_handler(dsp, id, core_id, xrun);
	else
		guest_adsp_dl_consume_handler(dsp, id, core_id, xrun, 0);
}

snd_pcm_uframes_t guest_get_pcm_pointer(int dsp_scene, int *xrun)
{
	int id = 0;
	struct mtk_base_dsp *dsp = get_dsp_base();
	snd_pcm_uframes_t ptr = 0;

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return 0;
	}

	id = get_dspdaiid_by_dspscene(dsp_scene);
	if (id < 0)
		return 0;

	if (!is_audio_task_dsp_ready(dsp_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return 0;
	}

	if (!dsp->dsp_mem[id].substream) {
		pr_info_ratelimited("%s substream NULL id[%d]\n", __func__, id);
		return 0;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info_ratelimited("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		return 0;
	}
	struct snd_pcm_substream *substream = dsp->dsp_mem[id].substream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);

	ptr = mtk_dsphw_pcm_pointer(component, dsp->dsp_mem[id].substream);

	if (ptr == SNDRV_PCM_POS_XRUN) {
		int ret = ptr;

		*xrun = 1;
		pr_info("%s(), ptr:%lu, %d, int:%d 222\n", __func__, ptr, (int)ptr, ret);
	}
	return ptr;
}
EXPORT_SYMBOL(guest_get_pcm_pointer);

int32_t guest_pcm_copy_dl(int dsp_scene, uint64_t phy_addr, uint64_t copy_size)
{
	int id = 0;
	struct mtk_base_dsp *dsp = get_dsp_base();
	struct mtk_base_dsp_mem *dsp_mem = NULL;
	int ret = 0, availsize = 0;
	unsigned long flags = 0;
	int ack_type;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct RingBuf *ringbuf;
	struct ringbuf_bridge *buf_bridge;
	const char *buf = phys_to_virt(phy_addr);
#ifdef DEBUG_VERBOSE
	unsigned long long cntpct_b, cntpct_e;
#endif

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return 0;
	}

	id = get_dspdaiid_by_dspscene(dsp_scene);

	if (id < 0)
		return 0;

	if (!is_audio_task_dsp_ready(dsp_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return 0;
	}
	dsp_mem = &dsp->dsp_mem[id];

	ringbuf = &(dsp_mem->ring_buf);
	buf_bridge =
		&(dsp_mem->adsp_buf.aud_buffer.buf_bridge);


#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	availsize = RingBuf_getFreeSpace(ringbuf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	if (availsize >= copy_size) {
		RingBuf_copyFromLinear(ringbuf, buf, copy_size);
		RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);
	} else {
		pr_info("%s, id = %d, fail copy_size = %llu availsize = %d\n",
			__func__, id, copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* send audio_hw_buffer to SCP side*/
	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
	       sizeof(struct audio_hw_buffer));

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dsp_mem->adsp_buf.counter++;

	//trace_mtk_dsp_pcm_copy_dl(id, copy_size, availsize);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	if (dsp->dsp_mem[id].substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
	else
		ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
#ifdef DEBUG_VERBOSE
	cntpct_b = get_cntpct();
#endif
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			ack_type, AUDIO_DSP_TASK_DLCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);
#ifdef DEBUG_VERBOSE
	cntpct_e = get_cntpct();
	/*
	 * trace_printk("[VADSP]%s(), taskid=%d, phy_addr:0x%llx,
	 * copy_size:%llu ipi cntpct_b:%llu, cntpct_diff:%llu\n",
	 * __func__, task_id, phy_addr, copy_size, cntpct_b,
	 * cntpct_e - cntpct_b);
	 */
#endif
	return ret;
}
EXPORT_SYMBOL(guest_pcm_copy_dl);


int32_t guest_pcm_copy_ul(int dsp_scene, uint64_t phy_addr, uint64_t copy_size)
{
	int id = 0;
	struct mtk_base_dsp *dsp = get_dsp_base();
	struct mtk_base_dsp_mem *dsp_mem = NULL;
	int ret = 0, availsize = 0;
	unsigned long flags = 0;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct RingBuf *ringbuf;
	char *buf = phys_to_virt(phy_addr);
#ifdef DEBUG_VERBOSE
	unsigned long long cntpct_b, cntpct_e;
#endif

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return 0;
	}

	id = get_dspdaiid_by_dspscene(dsp_scene);
	if (id < 0)
		return 0;

	if (!is_audio_task_dsp_ready(dsp_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return 0;
	}
	dsp_mem = &dsp->dsp_mem[id];
	ringbuf = &(dsp_mem->ring_buf);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif
	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
			&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	availsize = RingBuf_getDataCount(ringbuf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	if (availsize < copy_size) {
		pr_info("%s fail copy_size = %llu availsize = %d\n", __func__,
			copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* get audio_buffer from ring buffer */
	RingBuf_copyToLinear(buf, &dsp_mem->ring_buf, copy_size);
	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	sync_bridge_ringbuf_readidx(&dsp_mem->adsp_buf.aud_buffer.buf_bridge,
				    &dsp_mem->ring_buf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);
	dsp_mem->adsp_buf.counter++;

	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
		sizeof(struct audio_hw_buffer));

#ifdef DEBUG_VERBOSE
	cntpct_b = get_cntpct();
#endif
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_ULCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);
#ifdef DEBUG_VERBOSE
	cntpct_e = get_cntpct();
	dump_rbuf_bridge_s("1 mtk_dsp_ul_handler",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsp_ul_handler",
				&dsp_mem->ring_buf);

	/*
	 * trace_printk("[VADSP]%s(), taskid=%d, phy_addr:0x%llx,
	 * copy_size:%llu ipi cntpct_b:%llu, cntpct_diff:%llu\n",
	 * __func__, task_id, phy_addr, copy_size,
	 * cntpct_b, cntpct_e - cntpct_b);
	 */
#endif
	return ret;
}
EXPORT_SYMBOL(guest_pcm_copy_ul);


uint64_t get_irq_cnt(int task_scene)
{
	if (task_scene > 0 && task_scene < TASK_SCENE_SIZE)
		return irq_cnt[task_scene];
	return 0;
}
EXPORT_SYMBOL(get_irq_cnt);

#define SOC_MULTIPLE_EXT(xname, reg, shift, min, max, invert, func_get, \
	func_put, func_info) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = func_info, .get = func_get, .put = func_put, \
	.private_value = SOC_DOUBLE_S_VALUE(reg, shift, shift, min, max, \
	 0 /*xsign_bit*/, invert, 0) }
static int audio_ipi_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	return 0;
}

static int audio_ipi_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audio_ipi_info info;
	int ret = 0;

	info.type = ucontrol->value.integer64.value[0];
	info.value = ucontrol->value.integer64.value[1];
	if (virt_ipi_cb)
		ret = virt_ipi_cb(&info);

	return ret;
}

static int audio_ipi_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = sizeof(struct audio_ipi_info) / sizeof(uint64_t);
	uinfo->value.integer64.min = LLONG_MIN;
	uinfo->value.integer64.max = LLONG_MAX;
	return 0;
}

static int audio_dsp_reg_feature_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = sizeof(struct audio_dsp_reg_feature) / sizeof(uint16_t);
	uinfo->value.integer.min = SHRT_MIN;
	uinfo->value.integer.max = SHRT_MAX;
	return 0;
}

static int audio_dsp_reg_feature_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int audio_dsp_reg_feature_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audio_dsp_reg_feature feature;
	int ret = 0;

	feature.reg_flag = (uint16_t)ucontrol->value.integer.value[0];
	feature.feature_id = (uint16_t)ucontrol->value.integer.value[1];
	pr_info("[VADSP] %s reg_flag:%d, featrue_id:%d\n", __func__,
		feature.reg_flag, feature.feature_id);
	if (virt_adsp_reg_feature_cb)
		ret = virt_adsp_reg_feature_cb(&feature);

	return ret;
}

static int audio_dsp_query_status_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = sizeof(struct audio_dsp_query_status) / sizeof(uint16_t);
	uinfo->value.integer.min = SHRT_MIN;
	uinfo->value.integer.max = SHRT_MAX;
	return 0;
}

static int audio_dsp_query_status_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct audio_dsp_query_status status = {0, 0};

	status.core_id = 0;
	if (virt_adsp_query_status_cb) {
		ret = virt_adsp_query_status_cb(&status);
		ucontrol->value.integer.value[0] = status.ready_flag;
	} else
		pr_info("[VADSP] %s() virt_adsp_query_status_cb: %p\n", __func__,
				virt_adsp_query_status_cb);

	status.core_id = 1;
	if (virt_adsp_query_status_cb) {
		ret = virt_adsp_query_status_cb(&status);
		ucontrol->value.integer.value[1] = status.ready_flag;
	}

	return ret;
}

static int audio_dsp_query_memory_size_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = SHRT_MIN;
	uinfo->value.integer.max = SHRT_MAX;
	return 0;
}

static int audio_dsp_query_memory_size_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	uint32_t size;

	size = (uint32_t)adsp_get_reserve_mem_size(ADSP_B_IPI_DMA_MEM_ID);
	if (size == 0) {
		pr_info("[VADSP] %s() get dsp memory size = 0\n", __func__);
		size = 0x100000;
	}
	ucontrol->value.integer.value[0] = size;
	pr_info("[VADSP] %s() get dsp memory size: %d\n", __func__, size);
	return ret;
}

static int audio_dsp_dump_buffer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audio_dump_buffer_info info;
	int ret = 0;

	info.pa = (uint64_t)ucontrol->value.integer64.value[0];
	info.bytes = (uint64_t)ucontrol->value.integer64.value[1];
	info.pra = (uint64_t)ucontrol->value.integer64.value[2];
	pr_info("[VADSP] %s() phys_addr 0x%llx  size: 0x%llx, phys_read_addr: 0x%llx\n",
		__func__, info.pa,
		info.bytes, info.pra);
	if (virt_adsp_dump_cb)
		ret = virt_adsp_dump_cb(&info);
	return ret;
}

static int audio_dsp_dump_buffer_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = sizeof(struct audio_dump_buffer_info) / sizeof(uint64_t);
	uinfo->value.integer64.min = LLONG_MIN;
	uinfo->value.integer64.max = LLONG_MAX;
	return 0;
}

static const struct snd_kcontrol_new virt_adsp_kcontrols[] = {
	SOC_MULTIPLE_EXT("audio ipi", SND_SOC_NOPM, 0, INT_MIN, INT_MAX, 0,
		audio_ipi_get,
		audio_ipi_put,
		audio_ipi_info),
	SOC_MULTIPLE_EXT("audio dsp reg feature", SND_SOC_NOPM, 0, INT_MIN, INT_MAX, 0,
		audio_dsp_reg_feature_get,
		audio_dsp_reg_feature_put,
		audio_dsp_reg_feature_info),
	SOC_MULTIPLE_EXT("audio dsp query status", SND_SOC_NOPM, 0, INT_MIN, INT_MAX, 0,
		audio_dsp_query_status_get,
		NULL,
		audio_dsp_query_status_info),
	SOC_MULTIPLE_EXT("audio dsp get mem size", SND_SOC_NOPM, 0, INT_MIN, INT_MAX, 0,
		audio_dsp_query_memory_size_get,
		NULL,
		audio_dsp_query_memory_size_info),
	SOC_MULTIPLE_EXT("audio dsp set dump buffer", SND_SOC_NOPM, 0, INT_MIN, INT_MAX, 0,
		NULL,
		audio_dsp_dump_buffer_put,
		audio_dsp_dump_buffer_info),
};

int vadsp_probe(struct snd_soc_component *component)
{
	int ret = 0;

	ret = snd_soc_add_component_controls(component,
					     virt_adsp_kcontrols,
					     ARRAY_SIZE(virt_adsp_kcontrols));
	if (ret)
		pr_info("%s add_component err ret = %d\n", __func__, ret);
	vadsp_task_register_callback(guest_adsp_task_ipi_notify);

	return ret;
}


MODULE_DESCRIPTION("Mediatek vdsp platform driver");
MODULE_AUTHOR("feilong wei <feilong.wei@mediatek.com>");
MODULE_LICENSE("GPL");
