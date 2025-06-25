// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virtio-snd: Virtio sound device
 * Copyright (C) 2021 OpenSynergy GmbH
 */
#include <sound/pcm_params.h>

#include "virtio_card.h"

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
#include "../soc/mediatek/common/mtk-sram-manager.h"
#include "../soc/mediatek/common/mtk-mmap-ion.h"
#include "../soc/mediatek/mt6991/mt6991-reg.h"
static loff_t wpos;
#endif

/*
 * I/O messages lifetime
 * ---------------------
 *
 * Allocation:
 *   Messages are initially allocated in the ops->hw_params() after the size and
 *   number of periods have been successfully negotiated.
 *
 * Freeing:
 *   Messages can be safely freed after the queue has been successfully flushed
 *   (RELEASE command in the ops->sync_stop()) and the ops->hw_free() has been
 *   called.
 *
 *   When the substream stops, the ops->sync_stop() waits until the device has
 *   completed all pending messages. This wait can be interrupted either by a
 *   signal or due to a timeout. In this case, the device can still access
 *   messages even after calling ops->hw_free(). It can also issue an interrupt,
 *   and the interrupt handler will also try to access message structures.
 *
 *   Therefore, freeing of already allocated messages occurs:
 *
 *   - in ops->hw_params(), if this operator was called several times in a row,
 *     or if ops->hw_free() failed to free messages previously;
 *
 *   - in ops->hw_free(), if the queue has been successfully flushed;
 *
 *   - in dev->release().
 */

/* Map for converting ALSA format to VirtIO format. */
struct virtsnd_a2v_format {
	snd_pcm_format_t alsa_bit;
	unsigned int vio_bit;
};

static const struct virtsnd_a2v_format g_a2v_format_map[] = {
	{ SNDRV_PCM_FORMAT_IMA_ADPCM, VIRTIO_SND_PCM_FMT_IMA_ADPCM },
	{ SNDRV_PCM_FORMAT_MU_LAW, VIRTIO_SND_PCM_FMT_MU_LAW },
	{ SNDRV_PCM_FORMAT_A_LAW, VIRTIO_SND_PCM_FMT_A_LAW },
	{ SNDRV_PCM_FORMAT_S8, VIRTIO_SND_PCM_FMT_S8 },
	{ SNDRV_PCM_FORMAT_U8, VIRTIO_SND_PCM_FMT_U8 },
	{ SNDRV_PCM_FORMAT_S16_LE, VIRTIO_SND_PCM_FMT_S16 },
	{ SNDRV_PCM_FORMAT_U16_LE, VIRTIO_SND_PCM_FMT_U16 },
	{ SNDRV_PCM_FORMAT_S18_3LE, VIRTIO_SND_PCM_FMT_S18_3 },
	{ SNDRV_PCM_FORMAT_U18_3LE, VIRTIO_SND_PCM_FMT_U18_3 },
	{ SNDRV_PCM_FORMAT_S20_3LE, VIRTIO_SND_PCM_FMT_S20_3 },
	{ SNDRV_PCM_FORMAT_U20_3LE, VIRTIO_SND_PCM_FMT_U20_3 },
	{ SNDRV_PCM_FORMAT_S24_3LE, VIRTIO_SND_PCM_FMT_S24_3 },
	{ SNDRV_PCM_FORMAT_U24_3LE, VIRTIO_SND_PCM_FMT_U24_3 },
	{ SNDRV_PCM_FORMAT_S20_LE, VIRTIO_SND_PCM_FMT_S20 },
	{ SNDRV_PCM_FORMAT_U20_LE, VIRTIO_SND_PCM_FMT_U20 },
	{ SNDRV_PCM_FORMAT_S24_LE, VIRTIO_SND_PCM_FMT_S24 },
	{ SNDRV_PCM_FORMAT_U24_LE, VIRTIO_SND_PCM_FMT_U24 },
	{ SNDRV_PCM_FORMAT_S32_LE, VIRTIO_SND_PCM_FMT_S32 },
	{ SNDRV_PCM_FORMAT_U32_LE, VIRTIO_SND_PCM_FMT_U32 },
	{ SNDRV_PCM_FORMAT_FLOAT_LE, VIRTIO_SND_PCM_FMT_FLOAT },
	{ SNDRV_PCM_FORMAT_FLOAT64_LE, VIRTIO_SND_PCM_FMT_FLOAT64 },
	{ SNDRV_PCM_FORMAT_DSD_U8, VIRTIO_SND_PCM_FMT_DSD_U8 },
	{ SNDRV_PCM_FORMAT_DSD_U16_LE, VIRTIO_SND_PCM_FMT_DSD_U16 },
	{ SNDRV_PCM_FORMAT_DSD_U32_LE, VIRTIO_SND_PCM_FMT_DSD_U32 },
	{ SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE,
	  VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME }
};

/* Map for converting ALSA frame rate to VirtIO frame rate. */
struct virtsnd_a2v_rate {
	unsigned int rate;
	unsigned int vio_bit;
};

static const struct virtsnd_a2v_rate g_a2v_rate_map[] = {
	{ 5512, VIRTIO_SND_PCM_RATE_5512 },
	{ 8000, VIRTIO_SND_PCM_RATE_8000 },
	{ 11025, VIRTIO_SND_PCM_RATE_11025 },
	{ 16000, VIRTIO_SND_PCM_RATE_16000 },
	{ 22050, VIRTIO_SND_PCM_RATE_22050 },
	{ 32000, VIRTIO_SND_PCM_RATE_32000 },
	{ 44100, VIRTIO_SND_PCM_RATE_44100 },
	{ 48000, VIRTIO_SND_PCM_RATE_48000 },
	{ 64000, VIRTIO_SND_PCM_RATE_64000 },
	{ 88200, VIRTIO_SND_PCM_RATE_88200 },
	{ 96000, VIRTIO_SND_PCM_RATE_96000 },
	{ 176400, VIRTIO_SND_PCM_RATE_176400 },
	{ 192000, VIRTIO_SND_PCM_RATE_192000 }
};

static int virtsnd_pcm_sync_stop(struct snd_pcm_substream *substream);

/**
 * virtsnd_pcm_open() - Open the PCM substream.
 * @substream: Kernel ALSA substream.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_open(struct snd_pcm_substream *substream)
{
	struct virtio_pcm *vpcm = snd_pcm_substream_chip(substream);
	struct virtio_pcm_stream *vs = &vpcm->streams[substream->stream];
	struct virtio_pcm_substream *vss = vs->substreams[substream->number];

	substream->runtime->hw = vss->hw;
	substream->private_data = vss;

	snd_pcm_hw_constraint_integer(substream->runtime,
				      SNDRV_PCM_HW_PARAM_PERIODS);

	vss->stopped = !!virtsnd_pcm_msg_pending_num(vss);
	vss->suspended = false;

	wpos = 0;

	/*
	 * If the substream has already been used, then the I/O queue may be in
	 * an invalid state. Just in case, we do a check and try to return the
	 * queue to its original state, if necessary.
	 */
	return virtsnd_pcm_sync_stop(substream);
}

/**
 * virtsnd_pcm_close() - Close the PCM substream.
 * @substream: Kernel ALSA substream.
 *
 * Context: Process context.
 * Return: 0.
 */
static int virtsnd_pcm_close(struct snd_pcm_substream *substream)
{
	wpos = 0;
	return 0;
}

/**
 * virtsnd_pcm_dev_set_params() - Set the parameters of the PCM substream on
 *                                the device side.
 * @vss: VirtIO PCM substream.
 * @buffer_bytes: Size of the hardware buffer.
 * @period_bytes: Size of the hardware period.
 * @channels: Selected number of channels.
 * @format: Selected sample format (SNDRV_PCM_FORMAT_XXX).
 * @rate: Selected frame rate.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_dev_set_params(struct virtio_pcm_substream *vss,
				      unsigned int buffer_bytes,
				      unsigned int period_bytes,
				      unsigned int channels,
				      snd_pcm_format_t format,
				      unsigned int rate)
{
	struct virtio_snd_msg *msg;
	struct virtio_snd_pcm_set_params *request;
	unsigned int i;
	int vformat = -1;
	int vrate = -1;

	for (i = 0; i < ARRAY_SIZE(g_a2v_format_map); ++i)
		if (g_a2v_format_map[i].alsa_bit == format) {
			vformat = g_a2v_format_map[i].vio_bit;

			break;
		}

	for (i = 0; i < ARRAY_SIZE(g_a2v_rate_map); ++i)
		if (g_a2v_rate_map[i].rate == rate) {
			vrate = g_a2v_rate_map[i].vio_bit;

			break;
		}

	if (vformat == -1 || vrate == -1)
		return -EINVAL;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_SET_PARAMS,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	request = virtsnd_ctl_msg_request(msg);
	request->buffer_bytes = cpu_to_le32(buffer_bytes);
	request->period_bytes = cpu_to_le32(period_bytes);
	request->channels = channels;
	request->format = vformat;
	request->rate = vrate;

	if (vss->features & (1U << VIRTIO_SND_PCM_F_MSG_POLLING))
		request->features |=
			cpu_to_le32(1U << VIRTIO_SND_PCM_F_MSG_POLLING);

	if (vss->features & (1U << VIRTIO_SND_PCM_F_EVT_XRUNS))
		request->features |=
			cpu_to_le32(1U << VIRTIO_SND_PCM_F_EVT_XRUNS);

	return virtsnd_ctl_msg_send_sync(vss->snd, msg);
}
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
int virtsnd_mtk_memif_set_addr(struct virtio_snd *snd, int pcm_id,
		       unsigned char *dma_area,
		       dma_addr_t dma_addr,
		       size_t dma_bytes);
static void virtsnd_sram_control_work(struct work_struct *work)
{
	struct virtio_snd *snd =
		container_of(work, struct virtio_snd, sram_reg_control);

	int rc = 0;
	struct virtio_snd_ctl_info *kinfo;
	unsigned int type;
	unsigned int count;
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	struct virtio_snd_ctl_value *kvalue;
	size_t request_size = sizeof(*hdr) + sizeof(*kvalue);
	size_t response_size = sizeof(struct virtio_snd_hdr);

	rc = virtsnd_info_find_by_name(snd, "sram_mode",
			&kinfo);
	if (rc < 0){
		dev_info(&snd->vdev->dev, "get sram_mode info failed: %d\n", rc);
		snd->result = rc;
		return;
	}

	type = le32_to_cpu(kinfo->type);
	count = le32_to_cpu(kinfo->count);

	msg = virtsnd_ctl_msg_alloc(request_size, response_size, GFP_KERNEL);
	if (!msg){
		snd->result = -ENOMEM;
		return;
	}

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_WRITE);
	hdr->control_id = cpu_to_le32(kinfo->index);

	kvalue = (void *)((u8 *)hdr + sizeof(*hdr));
	kvalue->value.bytes[0] = (u8)snd->sram_mode;

	rc = virtsnd_ctl_msg_send_sync(snd, msg);

	snd->result = rc;
}

#endif
/**
 * virtsnd_pcm_hw_params() - Set the parameters of the PCM substream.
 * @substream: Kernel ALSA substream.
 * @hw_params: Hardware parameters.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_device *vdev = vss->snd->vdev;
	int rc;
	int i;
	unsigned int params_buffer_size;

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)

	int pcm_id;
	int use_dram_only = 0;
	struct snd_dma_buffer *dma_buf;

	pcm_id = substream->pcm->device;
	dma_buf = &substream->dma_buffer;
#endif
	params_buffer_size = 0;
	if (virtsnd_pcm_msg_pending_num(vss)) {
		dev_info(&vdev->dev, "SID %u: invalid I/O queue state\n",
			vss->sid);
		return -EBADFD;
	}

	rc = virtsnd_pcm_dev_set_params(vss, params_buffer_bytes(hw_params),
					params_period_bytes(hw_params),
					params_channels(hw_params),
					params_format(hw_params),
					params_rate(hw_params));
	if (rc)
		return rc;

	/*
	 * Free previously allocated messages if ops->hw_params() is called
	 * several times in a row, or if ops->hw_free() failed to free messages.
	 */
	virtsnd_pcm_msg_free(vss);

	for(i = 0; i < params_periods(hw_params); i ++)
		params_buffer_size +=  params_period_bytes(hw_params);

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	rc = virtsnd_kctl_get_use_dram_only(vss->snd, pcm_id);
	if (rc < 0)
		return rc;

	use_dram_only = rc;

	/*
	 * hw_params may be called several time,
	 * free sram of this substream first
	 */
	virtsnd_kctl_unreg_shm(vss->snd, pcm_id, 1);
	mtk_audio_sram_free(vss->snd->sram, substream);

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
    // TODO remove this debug code.
	use_dram_only = 1;
	INIT_WORK(&vss->snd->sram_reg_control, virtsnd_sram_control_work);
	if (use_dram_only == 0 &&
	    mtk_audio_sram_allocate(vss->snd->sram,
				    &substream->runtime->dma_addr,
				    &substream->runtime->dma_area,
				    substream->runtime->dma_bytes,
				    substream,
				    params_format(hw_params), false, false) == 0) {

		// virtsnd_mtk_memif_set_addr(vss->snd, pcm_id,
		//						substream->runtime->dma_addr,
		//						substream->runtime->dma_addr,
		//						substream->runtime->dma_bytes);
		rc = virtsnd_kctl_reg_shm(vss->snd, pcm_id,
				substream->runtime->dma_addr, substream->runtime->dma_bytes, 1);
		if (rc)
			return rc;

	} else {
		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->private_data = NULL;

		rc = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));

		if (rc < 0)
			return rc;

		// virtsnd_mtk_memif_set_addr(vss->snd, pcm_id,
		//						substream->runtime->dma_addr,
		//						substream->runtime->dma_addr,
		//						substream->runtime->dma_bytes);
		rc = virtsnd_kctl_reg_shm(vss->snd, pcm_id,
				substream->runtime->dma_addr, substream->runtime->dma_bytes, 0);
	}

#else
	rc = snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_size);

	if (rc < 0){
		dev_info(&vdev->dev, "SID %u: snd pcm allloc vmalloc failed %d\n",
			vss->sid, rc);
		return rc;
	}
#endif


	return virtsnd_pcm_msg_alloc(vss, params_periods(hw_params),
				     params_period_bytes(hw_params));
}

/**
 * virtsnd_pcm_hw_free() - Reset the parameters of the PCM substream.
 * @substream: Kernel ALSA substream.
 *
 * Context: Process context.
 * Return: 0
 */
static int virtsnd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd_msg *msg;
	int pcm_id = substream->pcm->device;

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	virtsnd_kctl_unreg_shm(vss->snd, pcm_id, 1);
	virtsnd_kctl_unreg_shm(vss->snd, pcm_id, 0);

	if (vss->snd->sram)
		mtk_audio_sram_free(vss->snd->sram, vss->substream);
	else
		snd_pcm_lib_free_pages(vss->substream);

#endif

	/* If the queue is flushed, we can safely free the messages here. */
	if (!virtsnd_pcm_msg_pending_num(vss))
		virtsnd_pcm_msg_free(vss);

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_FREE,
					GFP_KERNEL);
	if (!msg)
		return 0;

	return virtsnd_ctl_msg_send_sync(vss->snd, msg);
#endif
	return 0;
}

/**
 * virtsnd_pcm_prepare() - Prepare the PCM substream.
 * @substream: Kernel ALSA substream.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_device *vdev = vss->snd->vdev;
	struct virtio_snd_msg *msg;

	if (!vss->suspended) {
		if (virtsnd_pcm_msg_pending_num(vss)) {
			dev_info(&vdev->dev, "SID %u: invalid I/O queue state\n",
				vss->sid);
			return -EBADFD;
		}

		vss->buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		vss->hw_ptr = 0;
		vss->msg_last_enqueued = -1;
	} else {
		struct snd_pcm_runtime *runtime = substream->runtime;
		unsigned int buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		unsigned int period_bytes = snd_pcm_lib_period_bytes(substream);
		int rc;

		rc = virtsnd_pcm_dev_set_params(vss, buffer_bytes, period_bytes,
						runtime->channels,
						runtime->format, runtime->rate);
		if (rc)
			return rc;
	}

	vss->xfer_xrun = false;
	vss->suspended = false;
	vss->msg_count = 0;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_PREPARE,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	return virtsnd_ctl_msg_send_sync(vss->snd, msg);
}

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)

int virtsnd_mtk_vpcm_set_enable(unsigned int start, const struct mtk_base_memif_data *data)
{
	if (data->enable_shift < 0) {
		// dev_info(afe->dev, "%s(), error, id %d, enable_shift < 0\n",
		//	 __func__, id);
		return 0;
	}
	return virtsnd_mtk_reg_update_bits(start + data->enable_reg,
				      1 << data->enable_shift,
				      1 << data->enable_shift);
}

int virtsnd_mtk_vpcm_set_disable(unsigned int start, const struct mtk_base_memif_data *data)
{
	if (data->enable_shift < 0) {
		// dev_info(afe->dev, "%s(), error, id %d, enable_shift < 0\n",
		//	 __func__, id);
		return 0;
	}
	return virtsnd_mtk_reg_update_bits(start + data->enable_reg,
				      1 << data->enable_shift,
				      0);
}

int virtsnd_mtk_memif_set_addr(struct virtio_snd *snd, int pcm_id,
		       unsigned char *dma_area,
		       dma_addr_t dma_addr,
		       size_t dma_bytes)
{
	struct virtio_pcm *vpcm = virtsnd_pcm_find(snd, pcm_id);
	const struct mtk_base_memif_data *memif_data = vpcm->data;
	unsigned int start = snd->res.start;
	int msb_at_bit33 = upper_32_bits(dma_addr) ? 1 : 0;
	unsigned int phys_buf_addr = lower_32_bits(dma_addr);
	unsigned int phys_buf_addr_upper_32 = upper_32_bits(dma_addr);

	/* start */
	virtsnd_mtk_reg_write(start + memif_data->reg_ofs_base,
			 phys_buf_addr);
	/* end */
	if (memif_data->reg_ofs_end)
		virtsnd_mtk_reg_write(start + memif_data->reg_ofs_end,
				 phys_buf_addr + dma_bytes - 1);
	else
		virtsnd_mtk_reg_write(start + memif_data->reg_ofs_base +
				 8,
				 phys_buf_addr + dma_bytes - 1);

	/* set start, end, upper 32 bits */
	if (memif_data->reg_ofs_base_msb) {
		virtsnd_mtk_reg_write(start + memif_data->reg_ofs_base_msb,
				 phys_buf_addr_upper_32);
		virtsnd_mtk_reg_write(start + memif_data->reg_ofs_end_msb,
				 phys_buf_addr_upper_32);
	}

	/* set MSB to 33-bit */
	if (memif_data->msb_reg >= 0)
		virtsnd_mtk_reg_update_bits(start + memif_data->msb_reg,
				1 << memif_data->msb_shift,
				msb_at_bit33 << memif_data->msb_shift);

	return 0;
}

#endif

/**
 * virtsnd_pcm_trigger() - Process command for the PCM substream.
 * @substream: Kernel ALSA substream.
 * @command: Substream command (SNDRV_PCM_TRIGGER_XXX).
 *
 * Context: Any context. Takes and releases the VirtIO substream spinlock.
 *          May take and release the tx/rx queue spinlock.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_trigger(struct snd_pcm_substream *substream, int command)
{
#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	struct virtio_snd_queue *queue;
	struct virtio_snd_msg *msg;
	unsigned long flags;
	int rc = 0;

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		queue = virtsnd_pcm_queue(vss);

		spin_lock_irqsave(&queue->lock, flags);
		spin_lock(&vss->lock);
		rc = virtsnd_pcm_msg_send(vss);
		if (!rc)
			vss->xfer_enabled = true;
		spin_unlock(&vss->lock);
		spin_unlock_irqrestore(&queue->lock, flags);
		if (rc)
			return rc;

		msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_START,
						GFP_KERNEL);
		if (!msg) {
			spin_lock_irqsave(&vss->lock, flags);
			vss->xfer_enabled = false;
			spin_unlock_irqrestore(&vss->lock, flags);

			return -ENOMEM;
		}

		return virtsnd_ctl_msg_send_sync(snd, msg);
	case SNDRV_PCM_TRIGGER_SUSPEND:
		vss->suspended = true;
	case SNDRV_PCM_TRIGGER_STOP:
		vss->stopped = true;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&vss->lock, flags);
		vss->xfer_enabled = false;
		spin_unlock_irqrestore(&vss->lock, flags);

		msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_STOP,
						GFP_KERNEL);
		if (!msg)
			return -ENOMEM;

		return virtsnd_ctl_msg_send_sync(snd, msg);
	default:
		return -EINVAL;
	}
#else

	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	int id = vss->nid;
	struct virtio_pcm *vpcm = virtsnd_pcm_find(snd, vss->nid);
	const struct mtk_base_memif_data *memif_data = vpcm->data;
	struct mtk_base_afe_irq *irqs = &snd->irqs[vpcm->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	unsigned int start = snd->res.start;
	unsigned int rate = runtime->rate;
	int fs;
	int ret;
	//unsigned long flags;
	//unsigned int val;
	dev_info(&snd->vdev->dev, "%s(), command : %d\n",
				__func__, command);
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:

		// spin_lock_irqsave(&vss->lock, flags);
		ret = virtsnd_mtk_vpcm_set_enable(start, memif_data);
		if (ret) {
			dev_info(&snd->vdev->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			// spin_unlock_irqrestore(&vss->lock, flags);
			return ret;
		}
		/* set irq counter */
		virtsnd_mtk_reg_update_bits(start + irq_data->irq_cnt_reg,
				       irq_data->irq_cnt_maskbit
				       << irq_data->irq_cnt_shift,
				       counter << irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = snd->irq_fs(substream, rate);

		if (fs < 0)
			return -EINVAL;

		vss->xfer_enabled = true;

		if (irq_data->irq_fs_reg >= 0) {
			virtsnd_mtk_reg_update_bits(start + irq_data->irq_fs_reg,
						irq_data->irq_fs_maskbit
						<< irq_data->irq_fs_shift,
						fs << irq_data->irq_fs_shift);
		}

		/* enable interrupt */
		virtsnd_mtk_reg_update_bits(start + irq_data->irq_en_reg,
				       1 << irq_data->irq_en_shift,
				       1 << irq_data->irq_en_shift);
		// spin_unlock_irqrestore(&vss->lock, flags);
				/* enable tdm */
	// pr_info("shengjp virtsnd_pcm_trigger --wirte tdm con1\n");
		virtsnd_mtk_reg_update_bits(start + AFE_TDM_CON1,
				   TDM_EN_MASK_SFT, 0x1 << TDM_EN_SFT);
		dev_info(&snd->vdev->dev, "%s(), tdm con1 0x%8x = 0x%08x\n",
				__func__, start + AFE_TDM_CON1, 0x1 << TDM_EN_SFT);
	// virtsnd_read_reg(start + AFE_TDM_CON1,&val);
		// pr_info("shengjp virtsnd_pcm_trigger --wirte tdm con1=0x%x\n", val);
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		// spin_lock_irqsave(&vss->lock, flags);
		ret = virtsnd_mtk_vpcm_set_disable(start, memif_data);

		vss->xfer_enabled = false;

		if (ret) {
			dev_info(&snd->vdev->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
		}

		/* disable interrupt */
		virtsnd_mtk_reg_update_bits(start + irq_data->irq_en_reg,
				       1 << irq_data->irq_en_shift,
				       0 << irq_data->irq_en_shift);

		/* and clear pending IRQ */
		virtsnd_mtk_reg_write(start + irq_data->irq_clr_reg,
				 1 << irq_data->irq_clr_shift);
		// spin_unlock_irqrestore(&vss->lock, flags);
				/* disable tdm */
		virtsnd_mtk_reg_update_bits(start + AFE_TDM_CON1,
				   TDM_EN_MASK_SFT, 0);
		return ret;
	default:
		return -EINVAL;
	}

#endif
}

/**
 * virtsnd_pcm_sync_stop() - Synchronous PCM substream stop.
 * @substream: Kernel ALSA substream.
 *
 * The function can be called both from the upper level or from the driver
 * itself.
 *
 * Context: Process context. Takes and releases the VirtIO substream spinlock.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_pcm_sync_stop(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	struct virtio_snd_msg *msg;
	unsigned int js = msecs_to_jiffies(virtsnd_msg_timeout_ms);
	int rc;
#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	cancel_work_sync(&vss->elapsed_period);
#endif
	if (!vss->stopped)
		return 0;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_RELEASE,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	rc = virtsnd_ctl_msg_send_sync(snd, msg);
	if (rc)
		return rc;

	/*
	 * The spec states that upon receipt of the RELEASE command "the device
	 * MUST complete all pending I/O messages for the specified stream ID".
	 * Thus, we consider the absence of I/O messages in the queue as an
	 * indication that the substream has been released.
	 */
	rc = wait_event_interruptible_timeout(vss->msg_empty,
					      !virtsnd_pcm_msg_pending_num(vss),
					      js);
	if (rc <= 0) {
		dev_info(&snd->vdev->dev, "SID %u: failed to flush I/O queue\n",
			 vss->sid);

		return !rc ? -ETIMEDOUT : rc;
	}

	vss->stopped = false;

	return 0;
}
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
unsigned int virtsnd_word_size_align(unsigned int in_size)
{
	unsigned int align_size;

	/* sram is device memory,need word size align,
	 * 8 byte for 64 bit platform
	 * [3:0] = 4'h0 for the convenience of the hardware implementation
	 */
	align_size = in_size & 0xFFFFFFF0;
	return align_size;
}

#endif


/**
 * virtsnd_pcm_pointer() - Get the current hardware position for the PCM
 *                         substream.
 * @substream: Kernel ALSA substream.
 *
 * Context: Any context. Takes and releases the VirtIO substream spinlock.
 * Return: Hardware position in frames inside [0 ... buffer_size) range.
 */
static snd_pcm_uframes_t
virtsnd_pcm_pointer(struct snd_pcm_substream *substream)
{
	unsigned long flags;
#if !defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	snd_pcm_uframes_t hw_ptr = SNDRV_PCM_POS_XRUN;
	//unsigned long flags;

	spin_lock_irqsave(&vss->lock, flags);
	if (!vss->xfer_xrun)
		hw_ptr = bytes_to_frames(substream->runtime, vss->hw_ptr);
	spin_unlock_irqrestore(&vss->lock, flags);

	return hw_ptr;
#else
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	struct virtio_pcm *vpcm = virtsnd_pcm_find(snd, vss->nid);
	const struct mtk_base_memif_data *memif_data = vpcm->data;
	int reg_ofs_base = snd->res.start + memif_data->reg_ofs_base;
	int reg_ofs_cur = snd->res.start + memif_data->reg_ofs_cur;
	unsigned int hw_ptr = 0, hw_base = 0;
	int ret;//, pcm_ptr_bytes;
	snd_pcm_uframes_t hw_cur_ptr = SNDRV_PCM_POS_XRUN;

	spin_lock_irqsave(&vss->lock, flags);
	ret = virtsnd_read_reg(reg_ofs_cur, &hw_ptr);
	if (ret || hw_ptr == 0) {
		dev_info(&snd->vdev->dev, "%s %d ret : %d, hw_ptr : %d err\n", __func__, __LINE__, ret, hw_ptr);
		// pcm_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	ret = virtsnd_read_reg(reg_ofs_base, &hw_base);
	if (ret || hw_base == 0) {
		dev_info(&snd->vdev->dev, "%s %d ret : %d, hw_ptr : %d err\n", __func__, __LINE__, ret, hw_ptr);
		// pcm_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}
	vss->hw_ptr = hw_ptr - hw_base;

POINTER_RETURN_FRAMES:

	vss->hw_ptr = virtsnd_word_size_align(vss->hw_ptr);

	// spin_lock_irqsave(&vss->lock, flags);
	if (!vss->xfer_xrun)
		hw_cur_ptr = bytes_to_frames(substream->runtime, vss->hw_ptr);

	spin_unlock_irqrestore(&vss->lock, flags);
	return hw_cur_ptr;
#endif
}
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)

static void *virtsnd_get_dma_ptr(struct snd_pcm_runtime *runtime,
			   int channel, unsigned long pos)
{
	return runtime->dma_area + pos +
		channel * (runtime->dma_bytes / runtime->channels);
}

static int virtsnd_pcm_copy_user(struct snd_pcm_substream *substream, int channel,
			 unsigned long pos, void __user *buf,
			 unsigned long bytes)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (vss->direction == VIRTIO_SND_D_OUTPUT) {
		// memset_io((void __user *)buf, 5, bytes);
		if (copy_from_user(virtsnd_get_dma_ptr(runtime, channel, pos), (void __user *)buf, bytes))
			return -EFAULT;
	} else
		if (copy_to_user((void __user *)buf,
				virtsnd_get_dma_ptr(runtime, channel, pos),
				bytes))
			return -EFAULT;

	return 0;
}
static int virtsnd_pcm_copy_kernel(struct snd_pcm_substream *substream, int channel,
			   unsigned long pos, void *buf, unsigned long bytes)
{
	//struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	memcpy(virtsnd_get_dma_ptr(runtime, channel, pos), buf, bytes);
	return 0;
}
#endif

/* PCM substream operators map. */
const struct snd_pcm_ops virtsnd_pcm_ops = {
	.open = virtsnd_pcm_open,
	.close = virtsnd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = virtsnd_pcm_hw_params,
	.hw_free = virtsnd_pcm_hw_free,
	.prepare = virtsnd_pcm_prepare,
	.trigger = virtsnd_pcm_trigger,
	.sync_stop = virtsnd_pcm_sync_stop,
	.pointer = virtsnd_pcm_pointer,
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
	.copy_kernel = virtsnd_pcm_copy_kernel,
	.copy_user = virtsnd_pcm_copy_user,
#endif
};
