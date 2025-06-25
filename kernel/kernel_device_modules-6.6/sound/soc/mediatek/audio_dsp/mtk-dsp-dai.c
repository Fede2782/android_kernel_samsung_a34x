// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "mtk-dsp-common.h"
#include "mtk-base-dsp.h"


#define MTK_I2S_RATES \
	(SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_88200 | \
	 SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define MTK_I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

static int mtk_dai_stub_compress_new(struct snd_soc_pcm_runtime *rtd, int num)
{
#if IS_ENABLED(CONFIG_SND_SOC_COMPRESS)
	snd_soc_new_compress(rtd, num);
#endif
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_dsp_driver_ops = {
	.compress_new = mtk_dai_stub_compress_new,
};

static struct snd_soc_dai_driver mtk_dai_dsp_driver[] = {
	{
		.name = "audio_task_voip_dai",
		.id = AUDIO_TASK_VOIP_ID,
		.playback = {
				.stream_name = "DSP_Playback_Voip",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_primary_dai",
		.id = AUDIO_TASK_PRIMARY_ID,
		.playback = {
				.stream_name = "DSP_Playback_Primary",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_offload_dai",
		.id = AUDIO_TASK_OFFLOAD_ID,
		.playback = {
			.stream_name = "Offload_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_I2S_RATES,
			.formats = MTK_I2S_FORMATS,
		},
		.ops = &mtk_dai_dsp_driver_ops,
	},
	{
		.name = "audio_task_deepbuf_dai",
		.id = AUDIO_TASK_DEEPBUFFER_ID,
		.playback = {
				.stream_name = "DSP_Playback_DeepBuf",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_dynamic_dai",
		.id = AUDIO_TASK_DYNAMIC_ID,
		.playback = {
				.stream_name = "DSP_Playback_Dynamic",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_Playback_dai",
		.id = AUDIO_TASK_PLAYBACK_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback",
				.channels_min = 1,
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
				.channels_max = 16,
#else
				.channels_max = 4,
#endif
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_capture_ul1_dai",
		.id = AUDIO_TASK_CAPTURE_UL1_ID,
		.capture = {
				.stream_name = "DSP_Capture_Ul1",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_a2dp_dai",
		.id = AUDIO_TASK_A2DP_ID,
		.playback = {
				.stream_name = "DSP_Playback_A2DP",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_bledl_dai",
		.id = AUDIO_TASK_BLEDL_ID,
		.playback = {
				.stream_name = "DSP_Playback_BLEDL",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_call_final_dai",
		.id = AUDIO_TASK_CALL_FINAL_ID,
		.playback = {
				.stream_name = "DSP_Call_Final",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_fast_dai",
		.id = AUDIO_TASK_FAST_ID,
		.playback = {
				.stream_name = "DSP_Playback_Fast",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_spatializer_dai",
		.id = AUDIO_TASK_SPATIALIZER_ID,
		.playback = {
				.stream_name = "DSP_Playback_Spatializer",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_ktv_dai",
		.id = AUDIO_TASK_KTV_ID,
		.playback = {
				.stream_name = "DSP_Playback_Ktv",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_capture_raw_dai",
		.id = AUDIO_TASK_CAPTURE_RAW_ID,
		.capture = {
				.stream_name = "DSP_Capture_Raw",
				.channels_min = 1,
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
				.channels_max = 16,
#else
				.channels_max = 6,
#endif
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_fm_adsp_dai",
		.id = AUDIO_TASK_FM_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_Fm_Adsp",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_bleul_dai",
		.id = AUDIO_TASK_BLEUL_ID,
		.capture = {
				.stream_name = "DSP_Capture_BLE",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_btdl_dai",
		.id = AUDIO_TASK_BTDL_ID,
		.playback = {
				.stream_name = "DSP_Playback_BT",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_btul_dai",
		.id = AUDIO_TASK_BTUL_ID,
		.capture = {
				.stream_name = "DSP_Capture_BT",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_ulproc_dai",
		.id = AUDIO_TASK_UL_PROCESS_ID,
		.capture = {
				.stream_name = "DSP_Capture_Process",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_echoref_dai",
		.id = AUDIO_TASK_ECHO_REF_ID,
		.capture = {
				.stream_name = "DSP_Capture_Echoref",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_echodl_dai",
		.id = AUDIO_TASK_ECHO_REF_DL_ID,
		.playback = {
				.stream_name = "DSP_Playback_Echoref",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_usbdl_dai",
		.id = AUDIO_TASK_USBDL_ID,
		.playback = {
				.stream_name = "DSP_Playback_USB",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_usbul_dai",
		.id = AUDIO_TASK_USBUL_ID,
		.capture = {
				.stream_name = "DSP_Capture_USB",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_mddl_dai",
		.id = AUDIO_TASK_MDDL_ID,
		.capture = {
				.stream_name = "DSP_Capture_MDDL",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_mdul_dai",
		.id = AUDIO_TASK_MDUL_ID,
		.playback = {
				.stream_name = "DSP_Playback_MDUL",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_calldl_dai",
		.id = AUDIO_TASK_CALLDL_ID,
		.playback = {
				.stream_name = "DSP_Playback_CALLDL",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_callul_dai",
		.id = AUDIO_TASK_CALLUL_ID,
		.capture = {
				.stream_name = "DSP_Capture_CALLUL",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	{
		.name = "audio_task_hfp_client_rx_dai",
		.id = AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_HFP_CLIENT_RX",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_hfp_client_tx_dai",
		.id = AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_HFP_CLIENT_TX",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	{
		.name = "audio_task_anc_dai",
		.id = AUDIO_TASK_ANC_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_ANC",
				.channels_min = 1,
				.channels_max = 4,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	{
		.name = "audio_task_extstream1_dai",
		.id = AUDIO_TASK_EXTSTREAM1_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_EXTSTREAM1",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_extstream2_dai",
		.id = AUDIO_TASK_EXTSTREAM2_ADSP_ID,
		.playback = {
				.stream_name = "DSP_Playback_EXTSTREAM2",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	{
		.name = "audio_task_Sub_Playback_dai",
		.id = AUDIO_TASK_SUB_PLAYBACK_ID,
		.playback = {
				.stream_name = "DSP_Playback_Sub_Playback",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#endif
	{
		.name = "audio_task_playback0_dai",
		.id = AUDIO_TASK_PLAYBACK0_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback0",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback1_dai",
		.id = AUDIO_TASK_PLAYBACK1_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback1",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback2_dai",
		.id = AUDIO_TASK_PLAYBACK2_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback2",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback3_dai",
		.id = AUDIO_TASK_PLAYBACK3_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback3",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback4_dai",
		.id = AUDIO_TASK_PLAYBACK4_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback4",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback5_dai",
		.id = AUDIO_TASK_PLAYBACK5_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback5",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback6_dai",
		.id = AUDIO_TASK_PLAYBACK6_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback6",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback7_dai",
		.id = AUDIO_TASK_PLAYBACK7_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback7",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback8_dai",
		.id = AUDIO_TASK_PLAYBACK8_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback8",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback9_dai",
		.id = AUDIO_TASK_PLAYBACK9_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback9",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback10_dai",
		.id = AUDIO_TASK_PLAYBACK10_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback10",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback11_dai",
		.id = AUDIO_TASK_PLAYBACK11_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback11",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback12_dai",
		.id = AUDIO_TASK_PLAYBACK12_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback12",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback13_dai",
		.id = AUDIO_TASK_PLAYBACK13_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback13",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback14_dai",
		.id = AUDIO_TASK_PLAYBACK14_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback14",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
	{
		.name = "audio_task_playback15_dai",
		.id = AUDIO_TASK_PLAYBACK15_ID,
		.playback = {
				.stream_name = "DSP_Playback_Playback15",
				.channels_min = 1,
				.channels_max = 16,
				.rates = MTK_I2S_RATES,
				.formats = MTK_I2S_FORMATS,
			},
	},
#endif
};

int dai_dsp_register(struct platform_device *pdev, struct mtk_base_dsp *dsp)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	dsp->dai_drivers = mtk_dai_dsp_driver;
	dsp->num_dai_drivers = ARRAY_SIZE(mtk_dai_dsp_driver);

	return 0;
};
