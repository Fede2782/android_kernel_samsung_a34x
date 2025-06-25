// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
// Author: Chipeng Chang <chipeng.chang@mediatek.com>

#include <sound/soc.h>
#include <linux/device.h>
#include <linux/notifier.h>

/* ipi message related*/
#include "audio_ipi_dma.h"
#include "audio_ipi_platform.h"
#include "audio_messenger_ipi.h"
#include "audio_task_manager.h"
#include "audio_task.h"
#include "mtk-base-afe.h"
#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "mtk-afe-external.h"

#include <adsp_helper.h>
#include <linux/sched/clock.h>

//#define DEBUG_VERBOSE

#include "mtk-sp-spk-amp.h"
#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
#include "slbc_ops.h"
#endif

static int adsp_standby_flag;
static struct wait_queue_head waitq;

#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
static int slc_counter;
#endif

/* don't use this directly if not necessary */
static struct mtk_base_dsp *local_base_dsp;
static struct mtk_base_afe *local_dsp_afe;

static char *dsp_task_name[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]         = "voip",
	[AUDIO_TASK_PRIMARY_ID]      = "primary",
	[AUDIO_TASK_OFFLOAD_ID]      = "offload",
	[AUDIO_TASK_DEEPBUFFER_ID]   = "deep",
	[AUDIO_TASK_PLAYBACK_ID]     = "playback",
	[AUDIO_TASK_MUSIC_ID]        = "music",
	[AUDIO_TASK_CAPTURE_RAW_ID]  = "captureraw",
	[AUDIO_TASK_CAPTURE_UL1_ID]  = "capture",
	[AUDIO_TASK_A2DP_ID]         = "a2dp",
	[AUDIO_TASK_BLEDL_ID]        = "bledl",
	[AUDIO_TASK_BLEUL_ID]        = "bleul",
	[AUDIO_TASK_BTDL_ID]         = "btdl",
	[AUDIO_TASK_BTUL_ID]         = "btul",
	[AUDIO_TASK_DATAPROVIDER_ID] = "dataprovider",
	[AUDIO_TASK_CALL_FINAL_ID]   = "call_final",
	[AUDIO_TASK_FAST_ID]         = "fast",
	[AUDIO_TASK_KTV_ID]          = "ktv",
	[AUDIO_TASK_FM_ADSP_ID]      = "fm",
	[AUDIO_TASK_UL_PROCESS_ID]   = "ulproc",
	[AUDIO_TASK_ECHO_REF_ID]     = "echoref",
	[AUDIO_TASK_ECHO_REF_DL_ID]  = "echodl",
	[AUDIO_TASK_USBDL_ID]        = "usbdl",
	[AUDIO_TASK_USBUL_ID]        = "usbul",
	[AUDIO_TASK_MDDL_ID]         = "mddl",
	[AUDIO_TASK_MDUL_ID]         = "mdul",
	[AUDIO_TASK_SPATIALIZER_ID]  = "spatializer",
	[AUDIO_TASK_DYNAMIC_ID]      = "dynamic",
	[AUDIO_TASK_CALLDL_ID]       = "calldl",
	[AUDIO_TASK_CALLUL_ID]       = "callul",
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]    = "hfp_client_rx",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = "anc",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]    = "extstream1",
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]    = "extstream2",
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]    = "subpb",
#endif
	[AUDIO_TASK_PLAYBACK0_ID]    = "pb0",
	[AUDIO_TASK_PLAYBACK1_ID]    = "pb1",
	[AUDIO_TASK_PLAYBACK2_ID]    = "pb2",
	[AUDIO_TASK_PLAYBACK3_ID]    = "pb3",
	[AUDIO_TASK_PLAYBACK4_ID]    = "pb4",
	[AUDIO_TASK_PLAYBACK5_ID]    = "pb5",
	[AUDIO_TASK_PLAYBACK6_ID]    = "pb6",
	[AUDIO_TASK_PLAYBACK7_ID]    = "pb7",
	[AUDIO_TASK_PLAYBACK8_ID]    = "pb8",
	[AUDIO_TASK_PLAYBACK9_ID]    = "pb9",
	[AUDIO_TASK_PLAYBACK10_ID]   = "pb10",
	[AUDIO_TASK_PLAYBACK11_ID]   = "pb11",
	[AUDIO_TASK_PLAYBACK12_ID]   = "pb12",
	[AUDIO_TASK_PLAYBACK13_ID]   = "pb13",
	[AUDIO_TASK_PLAYBACK14_ID]   = "pb14",
	[AUDIO_TASK_PLAYBACK15_ID]   = "pb15",
	[AUDIO_TASK_CAPTURE_MCH_ID]  = "capmch",
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID]    = "hfp_client_tx",
#endif
#endif
};

static int dsp_task_scence[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]        = TASK_SCENE_VOIP,
	[AUDIO_TASK_PRIMARY_ID]     = TASK_SCENE_PRIMARY,
	[AUDIO_TASK_OFFLOAD_ID]     = TASK_SCENE_PLAYBACK_MP3,
	[AUDIO_TASK_DEEPBUFFER_ID]  = TASK_SCENE_DEEPBUFFER,
	[AUDIO_TASK_PLAYBACK_ID]    = TASK_SCENE_AUDPLAYBACK,
	[AUDIO_TASK_MUSIC_ID]       = TASK_SCENE_MUSIC,
	[AUDIO_TASK_CAPTURE_RAW_ID] = TASK_SCENE_CAPTURE_RAW,
	[AUDIO_TASK_CAPTURE_UL1_ID] = TASK_SCENE_CAPTURE_UL1,
	[AUDIO_TASK_A2DP_ID]        = TASK_SCENE_A2DP,
	[AUDIO_TASK_BLEDL_ID]       = TASK_SCENE_BLEDL,
	[AUDIO_TASK_BLEUL_ID]       = TASK_SCENE_BLEUL,
	[AUDIO_TASK_BTDL_ID]        = TASK_SCENE_BTDL,
	[AUDIO_TASK_BTUL_ID]        = TASK_SCENE_BTUL,
	[AUDIO_TASK_DATAPROVIDER_ID] = TASK_SCENE_DATAPROVIDER,
	[AUDIO_TASK_CALL_FINAL_ID]  = TASK_SCENE_CALL_FINAL,
	[AUDIO_TASK_FAST_ID]        = TASK_SCENE_FAST,
	[AUDIO_TASK_KTV_ID]         = TASK_SCENE_KTV,
	[AUDIO_TASK_FM_ADSP_ID]     = TASK_SCENE_FM_ADSP,
	[AUDIO_TASK_UL_PROCESS_ID]  = TASK_SCENE_UL_PROCESS,
	[AUDIO_TASK_ECHO_REF_ID]    = TASK_SCENE_ECHO_REF_UL,
	[AUDIO_TASK_ECHO_REF_DL_ID] = TASK_SCENE_ECHO_REF_DL,
	[AUDIO_TASK_USBDL_ID]       = TASK_SCENE_USB_DL,
	[AUDIO_TASK_USBUL_ID]       = TASK_SCENE_USB_UL,
	[AUDIO_TASK_MDDL_ID]        = TASK_SCENE_MD_DL,
	[AUDIO_TASK_MDUL_ID]        = TASK_SCENE_MD_UL,
	[AUDIO_TASK_SPATIALIZER_ID] = TASK_SCENE_SPATIALIZER,
	[AUDIO_TASK_DYNAMIC_ID]     = TASK_SCENE_DYNAMIC,
	[AUDIO_TASK_CALLDL_ID]      = TASK_SCENE_PHONE_CALL_SUB,
	[AUDIO_TASK_CALLUL_ID]      = TASK_SCENE_PHONE_CALL,
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]  = TASK_SCENE_HFP_CLIENT_RX,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = TASK_SCENE_ANC,
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]    = TASK_SCENE_EXTSTREAM1,
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]    = TASK_SCENE_EXTSTREAM2,
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]    = TASK_SCENE_SUB_PLAYBACK,
#endif
	[AUDIO_TASK_PLAYBACK0_ID]   = TASK_SCENE_PLAYBACK0,
	[AUDIO_TASK_PLAYBACK1_ID]   = TASK_SCENE_PLAYBACK1,
	[AUDIO_TASK_PLAYBACK2_ID]   = TASK_SCENE_PLAYBACK2,
	[AUDIO_TASK_PLAYBACK3_ID]   = TASK_SCENE_PLAYBACK3,
	[AUDIO_TASK_PLAYBACK4_ID]   = TASK_SCENE_PLAYBACK4,
	[AUDIO_TASK_PLAYBACK5_ID]   = TASK_SCENE_PLAYBACK5,
	[AUDIO_TASK_PLAYBACK6_ID]   = TASK_SCENE_PLAYBACK6,
	[AUDIO_TASK_PLAYBACK7_ID]   = TASK_SCENE_PLAYBACK7,
	[AUDIO_TASK_PLAYBACK8_ID]   = TASK_SCENE_PLAYBACK8,
	[AUDIO_TASK_PLAYBACK9_ID]   = TASK_SCENE_PLAYBACK9,
	[AUDIO_TASK_PLAYBACK10_ID]  = TASK_SCENE_PLAYBACK10,
	[AUDIO_TASK_PLAYBACK11_ID]  = TASK_SCENE_PLAYBACK11,
	[AUDIO_TASK_PLAYBACK12_ID]  = TASK_SCENE_PLAYBACK12,
	[AUDIO_TASK_PLAYBACK13_ID]  = TASK_SCENE_PLAYBACK13,
	[AUDIO_TASK_PLAYBACK14_ID]  = TASK_SCENE_PLAYBACK14,
	[AUDIO_TASK_PLAYBACK15_ID]  = TASK_SCENE_PLAYBACK15,
	[AUDIO_TASK_CAPTURE_MCH_ID]  = TASK_SCENE_CAPTURE_MCH,
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID]  = TASK_SCENE_HFP_CLIENT_TX,
#endif
#endif
};

#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
static int adsp_gid = -1;
static struct slbc_gid_data slbc_gid_adsp_data = {
	.sign = 0x0,
	.buffer_fd = 0,
	.producer = 0,
	.consumer = 0,
	.height = 0,    //unit: pixel
	.width = 0,     //unit: pixel
	.dma_size = 0,  //unit: MB
	.bw = 0,       //unit: MBps
	.fps = 0,       //unit: frames per sec
};
#endif

int audio_set_dsp_afe(struct mtk_base_afe *afe)
{
	local_dsp_afe = afe;
	return 0;
}
EXPORT_SYMBOL_GPL(audio_set_dsp_afe);

struct mtk_base_afe *get_afe_base(void)
{
	if (local_dsp_afe == NULL)
		pr_info("%s local_base_dsp == NULL", __func__);

	return local_dsp_afe;
}

int set_dsp_base(struct mtk_base_dsp *pdsp)
{
	if (pdsp == NULL)
		pr_info("%s pdsp == NULL", __func__);

	local_base_dsp = pdsp;
	return 0;
}

void *get_dsp_base(void)
{
	if (local_base_dsp == NULL)
		pr_warn("%s local_base_dsp == NULL", __func__);
	return local_base_dsp;
}
EXPORT_SYMBOL(get_dsp_base);

#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
void set_slc_counter(int cnt)
{
	if (cnt)
		slc_counter++;
	else
		slc_counter--;
}
EXPORT_SYMBOL(set_slc_counter);

int get_slc_counter(void)
{
	pr_debug("%s slc_counter = %d", __func__, slc_counter);
	return slc_counter;
}
EXPORT_SYMBOL(get_slc_counter);

int request_slc(int id)
{
	const char *task_name = get_str_by_dsp_dai_id(id);
	int ret_slc = 0;

	slbc_gid_adsp_data.sign = get_dsp_task_attr(id, ADSP_TASK_ATTR_ADSP_SLC_SIGN);
	slbc_gid_adsp_data.dma_size = get_dsp_task_attr(id, ADSP_TASK_ATTR_ADSP_SLC_DMASIZE);
	slbc_gid_adsp_data.bw = get_dsp_task_attr(id, ADSP_TASK_ATTR_ADSP_SLC_BW);

	ret_slc = slbc_gid_request(ID_ADSP, &adsp_gid, &slbc_gid_adsp_data);
	if (ret_slc) {
		pr_warn("%s(), %s SLC request failed = %d\n",
			__func__, task_name, ret_slc);
		return -1;
	}
	ret_slc = slbc_validate(ID_ADSP, adsp_gid);
	if (ret_slc) {
		pr_warn("%s(), %s SLC validate failed = %d\n",
			__func__, task_name, ret_slc);
		return -1;
	}
	pr_info("%s() %s SLC allocate GOOD, adsp_gid = %d, dma_size = %d, bw = %d\n",
		__func__, task_name, adsp_gid, slbc_gid_adsp_data.dma_size, slbc_gid_adsp_data.bw);
	return 0;
}
EXPORT_SYMBOL(request_slc);

int release_slc(int id)
{
	const char *task_name = get_str_by_dsp_dai_id(id);
	int ret_slc = 0;

	ret_slc = slbc_invalidate(ID_ADSP, adsp_gid);
	if (ret_slc) {
		pr_warn("%s(), %s SLC invalidate failed\n", __func__, task_name);
		return -1;
	}
	ret_slc = slbc_gid_release(ID_ADSP, adsp_gid);
	if (ret_slc) {
		pr_warn("%s(), %s SLC release failed\n", __func__, task_name);
		return -1;
	}
	pr_info("%s() %s SLC release GOOD\n", __func__, task_name);
	return 0;
}
EXPORT_SYMBOL(release_slc);
#endif

static void *ipi_recv_private;
void *get_ipi_recv_private(void)
{
	return ipi_recv_private;
}
EXPORT_SYMBOL(get_ipi_recv_private);

void set_ipi_recv_private(void *priv)
{
	pr_debug("%s\n", __func__);

	if (priv != NULL)
		ipi_recv_private = priv;
	else
		pr_debug("%s ipi_recv_private has been set\n", __func__);
}

int copy_ipi_payload(void *dst, void *src, unsigned int size)
{
	if (size > MAX_PAYLOAD_SIZE)
		return -1;

	memcpy(dst, src, size);
	return 0;
}

/*
 * common function for IPI message
 */
int mtk_scp_ipi_send(int task_scene, int data_type, int ack_type,
		     uint16_t msg_id, uint32_t param1, uint32_t param2,
		     char *payload)
{
	struct ipi_msg_t ipi_msg;
	int send_result = 0;

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));

	if (!is_audio_task_dsp_ready(task_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return -1;
	}

	if (get_task_attr(get_dspdaiid_by_dspscene(task_scene),
			  ADSP_TASK_ATTR_DEFAULT) <= 0) {
		pr_info("%s() task_scene[%d] not enable\n",
			__func__, task_scene);
		return -1;
	}

	send_result = audio_send_ipi_msg(
		&ipi_msg, task_scene,
		AUDIO_IPI_LAYER_TO_DSP, data_type,
		ack_type, msg_id, param1, param2,
		(char *)payload);
	if (send_result)
		pr_info("%s(),scp_ipi send fail\n", __func__);

	return send_result;
}
EXPORT_SYMBOL(mtk_scp_ipi_send);

/* dsp scene ==> od mapping */
int get_dspscene_by_dspdaiid(int id)
{
	if (id < 0 || id >= AUDIO_TASK_DAI_NUM) {
		pr_info("%s(), id err: %d\n", __func__, id);
		return -1;
	}

	return dsp_task_scence[id];
}
EXPORT_SYMBOL(get_dspscene_by_dspdaiid);

int get_dspdaiid_by_dspscene(int dspscene)
{
	int id;
	int ret = -1;

	if (dspscene < 0) {
		pr_info("%s() dspscene err: %d\n", __func__, dspscene);
		return -1;
	}

	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		if (dsp_task_scence[id] == dspscene) {
			ret = id;
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(get_dspdaiid_by_dspscene);

/* todo:: refine for check mechanism.*/
int get_audio_memery_type(struct snd_pcm_substream *substream)
{
	if (substream->runtime->dma_addr < 0x20000000)
		return MEMORY_AUDIO_SRAM;
	else
		return MEMORY_AUDIO_DRAM;
}

int afe_get_pcmdir(int dir, struct audio_hw_buffer buf)
{
	int ret = -1, i = 0, memif = 0;

	if (dir == SNDRV_PCM_STREAM_CAPTURE)
		ret = AUDIO_DSP_TASK_PCM_HWPARAM_UL;
	else if (dir == SNDRV_PCM_STREAM_PLAYBACK)
		ret = AUDIO_DSP_TASK_PCM_HWPARAM_DL;

	if (buf.hw_buffer == BUFFER_TYPE_SHARE_MEM)
		return ret;

	/* check if it is hardware buffer
	 * if yes ==> check if it is ref buffer.
	 */
	memif = buf.audio_memiftype;
	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		if (get_afememref_by_afe_taskid(i) == memif &&
		   buf.hw_buffer == BUFFER_TYPE_HW_MEM &&
		   get_task_attr(i, ADSP_TASK_ATTR_REF_RUNTIME) == 1) {
			ret = AUDIO_DSP_TASK_PCM_HWPARAM_REF;
			break;
		}
	}

	return ret;
}

int get_dsp_task_attr(int dsp_id, int task_attr)
{
	return get_task_attr(dsp_id, task_attr);
}
EXPORT_SYMBOL_GPL(get_dsp_task_attr);

int get_dsp_task_id_from_str(const char *task_name)
{
	int ret = -1;
	int id;

	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		if (strstr(task_name, dsp_task_name[id]))
			ret = id;
	}

	if (ret < 0)
		pr_info("%s(), %s has no task id, ret %d",
			__func__, task_name, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(get_dsp_task_id_from_str);

const char *get_str_by_dsp_dai_id(const int task_id)
{
	if (task_id < 0 || task_id >= AUDIO_TASK_DAI_NUM) {
		pr_info("%s(), task_id err: %d\n", __func__, task_id);
		return NULL;
	}

	return dsp_task_name[task_id];
}
EXPORT_SYMBOL_GPL(get_str_by_dsp_dai_id);

static int set_aud_buf_attr(struct audio_hw_buffer *audio_hwbuf,
			    struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    int irq_usage,
			    struct snd_soc_dai *dai)
{
	int ret = 0;

	ret = set_afe_audio_pcmbuf(audio_hwbuf, substream);
	if (ret < 0) {
		pr_info("set_afe_audio_pcmbuf fail\n");
		return -1;
	}

	ret = set_audiobuffer_hw(audio_hwbuf, BUFFER_TYPE_HW_MEM);
	if (ret < 0) {
		pr_info("set_audiobuffer_hw fail\n");
		return -1;
	}

	ret = set_audiobuffer_audio_irq_num(audio_hwbuf, irq_usage);
	if (ret < 0) {
		pr_info("set_audiobuffer_audio_irq_num fail\n");
		return -1;
	}

	ret = set_audiobuffer_audio_memiftype(audio_hwbuf, dai->id);
	if (ret < 0) {
		pr_info("set_audiobuffer_audio_memiftype fail\n");
		return -1;
	}

	ret = set_audiobuffer_memorytype(
		audio_hwbuf,
		get_audio_memery_type(substream));
	if (ret < 0) {
		pr_info("set_audiobuffer_memorytype fail\n");
		return -1;
	}

	ret = set_audiobuffer_attribute(
		audio_hwbuf,
		substream, params,
		afe_get_pcmdir(substream->stream, *audio_hwbuf));
	if (ret < 0) {
		pr_info("set_audiobuffer_attribute fail\n");
		return -1;
	}

	ret = set_audiobuffer_threshold(audio_hwbuf, substream);
	if (ret < 0) {
		pr_info("set_audiobuffer_threshold fail\n");
		return -1;
	}
#ifdef DEBUG_VERBOSE
	pr_info("%s() memiftype: %d ch: %u fmt: %u rate: %u dir: %d, start_thres: %u stop_thres: %u period_size: %d period_cnt: %d\n",
		__func__,
		audio_hwbuf->audio_memiftype,
		audio_hwbuf->aud_buffer.buffer_attr.channel,
		audio_hwbuf->aud_buffer.buffer_attr.format,
		audio_hwbuf->aud_buffer.buffer_attr.rate,
		audio_hwbuf->aud_buffer.buffer_attr.direction,
		audio_hwbuf->aud_buffer.start_threshold,
		audio_hwbuf->aud_buffer.stop_threshold,
		audio_hwbuf->aud_buffer.period_size,
		audio_hwbuf->aud_buffer.period_count);
#endif
	return 0;
}

/* function warp playback buffer information send to dsp */
int afe_pcm_ipi_to_dsp(int command, struct snd_pcm_substream *substream,
		       struct snd_pcm_hw_params *params,
		       struct snd_soc_dai *dai,
		       struct mtk_base_afe *afe)
{
	int ret = 0;
	struct mtk_base_dsp *dsp = (struct mtk_base_dsp *)local_base_dsp;
	void *ipi_audio_buf; /* dsp <-> audio data struct*/
	struct mtk_base_dsp_mem *dsp_memif;
	struct mtk_base_afe_memif *memif = &afe->memif[dai->id];
	int task_id = get_taskid_by_afe_daiid(dai->id);
#ifdef DEBUG_VERBOSE
	const char *task_name;
#endif

	if (task_id < 0 || task_id >= AUDIO_TASK_DAI_NUM)
		return -1;

	if (get_task_attr(task_id, ADSP_TASK_ATTR_RUNTIME) <= 0 ||
	    get_task_attr(task_id, ADSP_TASK_ATTR_DEFAULT) <= 0)
		return -1;

#ifdef DEBUG_VERBOSE
	task_name = get_str_by_dsp_dai_id(task_id);
	pr_info("%s(), %s send cmd 0x%x\n", __func__, task_name, command);
#endif
	dsp_memif = (struct mtk_base_dsp_mem *)&dsp->dsp_mem[task_id];

	/* send msg by task by unsing common function */
	switch (command) {
	case AUDIO_DSP_TASK_PCM_HWPARAM:
	case AUDIO_DSP_TASK_PCM_PREPARE:
		set_aud_buf_attr(&dsp_memif->audio_afepcm_buf,
				 substream,
				 params,
				 memif->irq_usage,
				 dai);

		/* send audio_afepcm_buf to SCP side */
		ipi_audio_buf =
			(void *)dsp_memif->msg_atod_share_buf.va_addr;
		memcpy((void *)ipi_audio_buf,
		       (void *)&dsp_memif->audio_afepcm_buf,
		       sizeof(struct audio_hw_buffer));

#ifdef DEBUG_VERBOSE
		dump_audio_hwbuffer(ipi_audio_buf);
#endif

		/* send to task with hw_param information, buffer and pcm attribute
		 * or send to task with prepare status
		 */
		ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(task_id),
				       AUDIO_IPI_PAYLOAD,
				       AUDIO_IPI_MSG_NEED_ACK,
				       command,
				       sizeof(dsp_memif->msg_atod_share_buf.phy_addr),
				       0,
				       (char *)
				       &dsp_memif->msg_atod_share_buf.phy_addr);
		break;
	case AUDIO_DSP_TASK_PCM_HWFREE:
		set_aud_buf_attr(&dsp_memif->audio_afepcm_buf,
				 substream,
				 params,
				 memif->irq_usage,
				 dai);
		/* send to task with prepare status */
		ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(task_id),
				       AUDIO_IPI_MSG_ONLY,
				       AUDIO_IPI_MSG_NEED_ACK,
				       AUDIO_DSP_TASK_PCM_HWFREE,
				       afe_get_pcmdir(substream->stream,
				       dsp_memif->audio_afepcm_buf),
				       0,
				       NULL);
		break;
	default:
		pr_warn("%s error command: %d\n", __func__, command);
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(afe_pcm_ipi_to_dsp);

void mtk_dsp_pcm_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	struct mtk_base_dsp *dsp = get_ipi_recv_private();

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	if (!is_audio_task_dsp_ready(ipi_msg->task_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return;
	}

	if (dsp->dsp_ipi_ops.ipi_handler)
		dsp->dsp_ipi_ops.ipi_handler(dsp, ipi_msg);
}

int mtk_dsp_register_feature(int id)
{
	return adsp_register_feature(id);
}

int mtk_dsp_deregister_feature(int id)
{
	return adsp_deregister_feature(id);
}

int wait_dsp_ready(void)
{
	/* should not call this in atomic or interrupt level */
	if (!wait_event_timeout(waitq, adsp_standby_flag == 0 && is_adsp_ready(ADSP_A_ID),
				msecs_to_jiffies(200)))
		return -EBUSY;

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
static int mtk_audio_dsp_event_receive(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	switch (event) {
	case ADSP_EVENT_STOP:
		adsp_standby_flag = 1;
		break;
	case ADSP_EVENT_READY:
		mtk_reinit_adsp();
		adsp_standby_flag = 0;
		wake_up(&waitq);
		break;
	default:
		pr_info("event %lu err", event);
	}
	return 0;
}

static struct notifier_block mtk_audio_dsp_notifier = {
	.notifier_call = mtk_audio_dsp_event_receive,
	.priority = AUDIO_PLAYBACK_FEATURE_PRI,
};
#endif

int mtk_audio_register_notify(void)
{
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	adsp_register_notify(&mtk_audio_dsp_notifier);
#endif
	init_waitqueue_head(&waitq);
	return 0;
}

int mtk_get_ipi_buf_scene_adsp(void)
{
	int task_scene = -1;

	if (adsp_standby_flag)
		return task_scene;

	if (get_task_attr(AUDIO_TASK_CALL_FINAL_ID,
			  ADSP_TASK_ATTR_RUNTIME) > 0)
		task_scene = TASK_SCENE_CALL_FINAL;
	else if (get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			       ADSP_TASK_ATTR_RUNTIME) > 0)
		task_scene = TASK_SCENE_AUDPLAYBACK;
	else {
		pr_info("%s(), callfinal and playback are not enable\n",
			__func__);
	}

	return task_scene;
}
EXPORT_SYMBOL(mtk_get_ipi_buf_scene_adsp);

void hook_has_video_cb(has_video_cb_t cb)
{
	if (cb && local_base_dsp)
		local_base_dsp->offload_cb.query_has_video = cb;
}
EXPORT_SYMBOL(hook_has_video_cb);

void hook_vp_sync_cb(offlad_vp_event cb)
{
	if (cb && local_base_dsp)
		local_base_dsp->offload_cb.receive_vp_sync = cb;
}
EXPORT_SYMBOL(hook_vp_sync_cb);

int notify_vp_audio_event(struct notifier_block *nb,
				unsigned long event, void *v)
{

	int status = NOTIFY_DONE, result = -1;
	bool has_video = false;
	bool latency_support;
	int is_offload_active = -1;
	struct mtk_base_dsp *dsp = local_base_dsp;

	if (!dsp)
		return NOTIFY_DONE;

	latency_support = adsp_task_get_latency_support();
	if (latency_support == false)
		return status;
	is_offload_active = get_feature_register(OFFLOAD_FEATURE_ID);
	if (is_offload_active != 1)
		return status;

	if (event == NOTIFIER_VP_AUDIO_TRIGGER) {
		if (dsp->offload_cb.query_has_video)
			has_video = dsp->offload_cb.query_has_video();
		/* notify offload with vp sync */
		if (has_video && dsp->offload_cb.receive_vp_sync)
			dsp->offload_cb.receive_vp_sync();
	} else if (event == NOTIFIER_VP_AUDIO_TIMER) {
		if (dsp->offload_cb.query_has_video)
			has_video = dsp->offload_cb.query_has_video();
		if (has_video) {
			result = adsp_send_vp_irq(true);
			if (result)
				pr_info_ratelimited("adsp_send_vp_irq result = %d\n", result);
		}
	}
	return status;
}

static struct notifier_block adsp_vp_notifier = {
	.notifier_call = notify_vp_audio_event,
};

int register_vp_notifier(void)
{
	return register_vp_audio_notifier(&adsp_vp_notifier);
}
