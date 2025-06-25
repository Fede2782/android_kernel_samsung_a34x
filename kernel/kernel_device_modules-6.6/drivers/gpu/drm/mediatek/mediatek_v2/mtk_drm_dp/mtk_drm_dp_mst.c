// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_modes.h>
#include <video/videomode.h>

#include "mtk_drm_dp_mst.h"
#include "mtk_drm_dp_mst_drv.h"
#include "mtk_drm_dp.h"
#include "mtk_drm_dp_reg.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
#define DPTX_MST_HDCP_ENABLE	0
#define DPTX_MST_FEC_ENABLE	0
#define DPTX_DPCD_TRANS_BYTES_MAX	16
#define DPTX_SUPPORT_MAX_LINKRATE	DP_LINK_RATE_HBR3
#define DPTX_SUPPORT_MAX_LANECOUNT	DP_2LANE

/**
 * mtk_dp_mst_hal_enc_enable() - enable encoder MST mode
 * @enable: set true if want to enable MST mode
 */
static void mtk_dp_mst_hal_enc_enable(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				      const u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSET(encoder_id);

	/* encoder to MST Tx must set 4lane, set 2 */
	if (enable)
		WRITE_BYTE_MASK(mtk_dp, (REG_3000_DP_ENCODER0_P0 + reg_offset_enc),
				2 << LANE_NUM_DP_ENCODER0_P0_FLDMASK_POS,
			    LANE_NUM_DP_ENCODER0_P0_FLDMASK);

	WRITE_BYTE_MASK(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
			((enable > 0) ? 0x01 : 0x00) << DP_MST_EN_DP_ENCODER1_P0_FLDMASK_POS,
		DP_MST_EN_DP_ENCODER1_P0_FLDMASK);

	/* mixer_sdp & enhanced_frame should be disable in MST mode*/
	WRITE_4BYTE_MASK(mtk_dp, (REG_3030_DP_ENCODER0_P0 + reg_offset_enc),
			 ((enable == 0) ? 0x01 : 0x00) << MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK_POS,
		MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK);
	WRITE_BYTE_MASK(mtk_dp, (REG_3000_DP_ENCODER0_P0 + reg_offset_enc),
			((enable == 0) ? 0x01 : 0x00)
			<< ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK_POS,
		ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dp_mst_hal_mst_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	WRITE_BYTE_MASK(mtk_dp, REG_3808_DP_MST_DPTX,
			((enable > 0) ? 0x01 : 0x00) << MST_EN_TX_DP_MST_DPTX_FLDMASK_POS,
		MST_EN_TX_DP_MST_DPTX_FLDMASK);

	/* disable HDCP ECF bypass */
	WRITE_BYTE_MASK(mtk_dp, REG_3888_DP_MST_DPTX,
			((enable == 0) ? 0x01 : 0x00),
		HDCP_ECF_BYPASS_DP_MST_DPTX_FLDMASK);

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	WRITE_BYTE_MASK(mtk_dp, REG_3980_DP_MST_DPTX,
			((enable > 0) ? 0x01 : 0x00)
			<< ENCRYPTION_EN_MST_TX_DP_MST_DPTX_FLDMASK_POS,
		ENCRYPTION_EN_MST_TX_DP_MST_DPTX_FLDMASK); // Enable HDCP encryption
#endif
}

static void mtk_dp_mst_hal_trans_enable(struct mtk_dp *mtk_dp, const bool enable)
{
	u32 port_mux = enable ? 0x04 : 0x00;
	u32 value = 0x0;

	if (mtk_dp->training_state == DP_TRAINING_STATE_NORMAL && enable != 0x0)
		value = 0x1;

	/* enable need wait training lock otherwise TPS4 training fail */
	WRITE_4BYTE_MASK(mtk_dp, REG_3480_DP_TRANS_P0,
			 value << MST_EN_DP_TRANS_P0_FLDMASK_POS,
						MST_EN_DP_TRANS_P0_FLDMASK);

	/* PRE_MISC_PORT_MUX: 4 = MST mode */
	WRITE_4BYTE_MASK(mtk_dp, REG_3400_DP_TRANS_P0,
			 (0 << PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(1 << PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(2 << PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(3 << PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(port_mux << PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK_POS),
					PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK);
}

/** reduce drv & hal layer mst enable function
 * mtk_dp_mst_hal_tx_enable() - set lane count & enable encoder + transmitter
 * @enable: set true if want to enable MST function
 */
void mtk_dp_mst_hal_tx_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	enum dp_encoder_id encoder_id;

	DP_MSG("MST hal tx enable %d\n", enable);
	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_mst_hal_enc_enable(mtk_dp, encoder_id, enable);

	mtk_dp_mst_hal_mst_enable(mtk_dp, enable);
	mtk_dp_mst_hal_trans_enable(mtk_dp, enable);
}

/**
 * mtk_dp_mst_hal_tx_init() - initialize MST HW function
 */
void mtk_dp_mst_hal_tx_init(struct mtk_dp *mtk_dp)
{
	u32 reg_offset_enc;
	enum dp_encoder_id encoder_id = DP_ENCODER_ID_MAX;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSET(encoder_id);

		WRITE_BYTE_MASK(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
				(1 << MST_RESET_SW_DP_ENCODER1_P0_FLDMASK_POS) |
						(1 << MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK_POS),
						(MST_RESET_SW_DP_ENCODER1_P0_FLDMASK) |
						(MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK));
		usleep_range(20, 21);

		WRITE_BYTE_MASK(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc), 0x0,
				(MST_RESET_SW_DP_ENCODER1_P0_FLDMASK) |
						(MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK));
	}

	// MST_SW_RESET:
	// [0] rx_data_shift,	[1] rx_lane_cnt_adjust,	[2] rx_vcp_allocate,
	// [3] rx_vcp__table,	[4] fifo,		[5] tx_merge,
	// [6]tx_payload_mux,	[7] tx_rate_governer,	[8] tx_vc_payload_id_sync,
	// [9] tx_vc_payload_update,			[15] reset all
	WRITE_2BYTE_MASK(mtk_dp, REG_3800_DP_MST_DPTX, BIT(15), BIT(15));
	WRITE_2BYTE_MASK(mtk_dp, REG_3894_DP_MST_DPTX,
			 (1 << RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK_POS) |
		(1 << RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK_POS),
		RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
		RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
	mdelay(1);
	WRITE_2BYTE_MASK(mtk_dp, REG_3800_DP_MST_DPTX, 0x0, BIT(15));
	WRITE_2BYTE_MASK(mtk_dp, REG_3894_DP_MST_DPTX, 0x0,
			 RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
		RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
	/* reg_mst_source_sel
	 * 3'd0 : MST original
	 * 3'd1 : encode 0
	 * 3'd2 : encode 1
	 * 3'd3 : encode 2
	 * 3'd4 : encode 3
	 */
	WRITE_2BYTE(mtk_dp, REG_3898_DP_MST_DPTX, 0x4321);

	if (encoder_id >= 4)
		DP_ERR("Should add reg_mst_source_sel configurations\n");

	/* early enable mst when we know the primary branch support MST*/
	mtk_dp_mst_hal_mst_enable(mtk_dp, true);

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSET(encoder_id);
		//[CH]: workaround for audio pkt lost from audio engine(I2S)
		WRITE_2BYTE_MASK(mtk_dp, REG_330C_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x09 << SDP_MST_INSERT_CNT_DP_ENCODER1_P0_FLDMASK_POS) |
				(0x01 << MST_MTP_CNT_VIDEO_MASK_DP_ENCODER1_P0_FLDMASK_POS),
				SDP_MST_INSERT_CNT_DP_ENCODER1_P0_FLDMASK |
				MST_MTP_CNT_VIDEO_MASK_DP_ENCODER1_P0_FLDMASK);
	}
	// top gp bank wait for coda TBD
	//WRITE_BYTE_MASK(mtk_dp, REG_01B0_DP_TX_TOP_GP, 0x01, RTX_CLK_SEL_P0);
	//WRITE_BYTE_MASK(mtk_dp, REG_0030_DP_TX_TOP_GP, 0x07, TX_PIX_CLK_SEL_0);
	//WRITE_BYTE_MASK(mtk_dp, REG_0054_DP_TX_TOP_GP, 0x07, TX_MAINLINK_CLK_SEL_0);
}

void mtk_dp_mst_hal_mst_config(struct mtk_dp *mtk_dp)
{
	/* This function is executed after link training done */
	enum dp_lane_count lane_count = mtk_dp->training_info.link_lane_count;

	if (mtk_dp->training_state != DP_TRAINING_STATE_NORMAL) {
		DP_ERR("Un-expected training state %d while setting mst lane count\n",
		       mtk_dp->training_state);
	}

	/* 0: 1lane,  1: 2lane,  2: 4lane */
	WRITE_BYTE_MASK(mtk_dp, REG_3808_DP_MST_DPTX,
			(lane_count >> 1) << LANE_NUM_TX_DP_MST_DPTX_FLDMASK_POS,
				LANE_NUM_TX_DP_MST_DPTX_FLDMASK);
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	WRITE_4BYTE_MASK(mtk_dp, REG_3884_DP_MST_DPTX, 0x0,
			 HDCP_ECF_FIFO_OV_DP_MST_DPTX_FLDMASK); // Disable HDCP bypass
#endif
}

/**
 * mtk_dp_mst_hal_set_mtp_size() - set slots number according to lane count
 * @num_slots: target slots number
 */
void mtk_dp_mst_hal_set_mtp_size(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				 const u8 num_slots)
{
	u16 num_slot_enc;
	u32 reg_offset_enc = DP_REG_OFFSET(encoder_id);

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_4LANE:
		num_slot_enc = num_slots;
		break;
	case DP_2LANE:
		num_slot_enc = num_slots >> 1;
		break;
	case DP_1LANE:
		num_slot_enc = num_slots >> 2;
		break;
	default:
		num_slot_enc = num_slots;
		DP_ERR("Unknown lane count %d\n", mtk_dp->training_info.link_lane_count);
		break;
	}

	/* update slots number */
	WRITE_2BYTE_MASK(mtk_dp, (REG_3310_DP_ENCODER1_P0 + reg_offset_enc),
			 num_slot_enc << MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK_POS,
				MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK);
}

/**
 * mtk_dp_mst_hal_set_timeslot() - set vcp id of each slots
 * @start_slot: start of slot, for example 5
 * @end_slot: end of slot, for example 20
 * @vcpi: vc payload id, for example 1
 *
 * Means from MTP slots 5 ~ 20 will set vcpi = 1
 * Even slots use LSB, Odd slots use MSB
 */
void mtk_dp_mst_hal_set_timeslot(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
				 const u16 start_slot, const u16 end_slot, const u32 vcpi)
{
	u32 slot, addr;
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	u32 hdcp_bitmap_upper = 0x0;
	u32 hdcp_bitmap_lower = 0x0;
#endif

	if (vcpi > DP_STREAM_MAX)
		DP_ERR("Un-expected vcpi%d", vcpi);

	/* update corresponding vcpi */
	/*0x38A8*/
	/*     slot0           slot1  */
	/*bit0___bit6 bit8___bit14*/
	/*0x38AC*/
	/*     slot2           slot3  */
	/*bit0___bit6 bit8___bit14*/
	for (slot = start_slot; slot < end_slot; slot++) {
		addr = REG_38A8_DP_MST_DPTX + ((slot >> 1) << 2);
		if (slot % 2)	/* odd slots */
			WRITE_2BYTE_MASK(mtk_dp, addr,
					 vcpi << VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK_POS,
					VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK);
		else		/* even slots */
			WRITE_2BYTE_MASK(mtk_dp, addr,
					 vcpi << VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK_POS,
					VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK);
	}

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	if (start_slot < 32) {
		hdcp_bitmap_lower = ((0xFFFFFFFF >> start_slot) << start_slot);
		hdcp_bitmap_upper = 0xFFFFFFFF;
	} else {
		hdcp_bitmap_lower = 0x0;
		hdcp_bitmap_upper = ((0xFFFFFFFF >> start_slot) << start_slot);
	}

	if (end_slot < 32) {
		hdcp_bitmap_lower = ((hdcp_bitmap_lower << (31 - end_slot) >> (31 - end_slot)));
		hdcp_bitmap_upper = 0x0;
	} else {
		hdcp_bitmap_upper = ((hdcp_bitmap_upper << (31 - end_slot) >> (31 - end_slot)));
	}

	WRITE_2BYTE_MASK(mtk_dp, REG_3984_DP_MST_DPTX, hdcp_bitmap_lower,
			 hdcp_bitmap_lower & HDCP_TIMESLOT_MST_TX_0_DP_MST_DPTX_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3988_DP_MST_DPTX, hdcp_bitmap_lower >> 16,
			 (hdcp_bitmap_lower >> 16) & HDCP_TIMESLOT_MST_TX_1_DP_MST_DPTX_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_398C_DP_MST_DPTX, hdcp_bitmap_upper,
			 hdcp_bitmap_upper & HDCP_TIMESLOT_MST_TX_2_DP_MST_DPTX_FLDMASK);
	WRITE_2BYTE_MASK(mtk_dp, REG_3990_DP_MST_DPTX, hdcp_bitmap_upper >> 16,
			 (hdcp_bitmap_upper >> 16) & HDCP_TIMESLOT_MST_TX_3_DP_MST_DPTX_FLDMASK);

	/*reg_trig_hdcp_timeslot, WO*/
	WRITE_BYTE_MASK(mtk_dp, REG_3984_DP_MST_DPTX, 0, BIT(0));
	WRITE_2BYTE_MASK(mtk_dp, REG_3980_DP_MST_DPTX,
			 1 << TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK_POS,
					TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK);
#endif
}

void mtk_dp_mst_hal_set_id_buf(struct mtk_dp *mtk_dp, const enum dp_encoder_id encoder_id,
			       const u32 vcpi)
{
	/* overwrite ID buffer define */
	if (encoder_id % 2)	/* encoder 1/3/... */
		WRITE_2BYTE_MASK(mtk_dp, (REG_3854_DP_MST_DPTX + ((encoder_id >> 1) << 2)),
				 vcpi << ID_BUF_2_OV_DP_MST_DPTX_FLDMASK_POS,
					ID_BUF_2_OV_DP_MST_DPTX_FLDMASK);
	else			/* encoder 0/2... */
		WRITE_2BYTE_MASK(mtk_dp, (REG_3854_DP_MST_DPTX + ((encoder_id >> 1) << 2)),
				 vcpi << ID_BUF_1_OV_DP_MST_DPTX_FLDMASK_POS,
					ID_BUF_1_OV_DP_MST_DPTX_FLDMASK);
}

static void mtk_dp_mst_hal_clear_id_buf(struct mtk_dp *mtk_dp)
{
	WRITE_2BYTE(mtk_dp, REG_3854_DP_MST_DPTX, 0x0);
	WRITE_2BYTE(mtk_dp, REG_3858_DP_MST_DPTX, 0x0);
	WRITE_2BYTE(mtk_dp, REG_385C_DP_MST_DPTX, 0x0);
	WRITE_2BYTE(mtk_dp, REG_3860_DP_MST_DPTX, 0x0);
}

/**
 * mtk_dp_mst_hal_reset_payload() - reset vcpi = 0 for each slots
 */
void mtk_dp_mst_hal_reset_payload(struct mtk_dp *mtk_dp)
{
	u32 encoder_id, i;
	u32 reg_offset_enc;

	mtk_dp_mst_hal_clear_id_buf(mtk_dp);

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSET(encoder_id);

		/* update slots number */
		WRITE_2BYTE_MASK(mtk_dp, (REG_3310_DP_ENCODER1_P0 + reg_offset_enc),
				 0x00 << MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK_POS,
					MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK);
	}

	/* update corresponding vcpi */
	for (i = 0; i < 32; i++)
		WRITE_2BYTE_MASK(mtk_dp, (REG_38A8_DP_MST_DPTX + (i << 2)), 0x00,
				 VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK |
						VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK);

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	for (i = 0; i < 4; i++)
		WRITE_2BYTE_MASK(mtk_dp, REG_3984_DP_MST_DPTX  + (i << 2),
				 0x0, HDCP_TIMESLOT_MST_TX_0_DP_MST_DPTX_FLDMASK);
#endif
}

/**
 * mtk_dp_mst_hal_vcp_table_update() - trigger vcpi update
 */
void mtk_dp_mst_hal_vcp_table_update(struct mtk_dp *mtk_dp)
{
	/* update TX ID buffer */
	WRITE_2BYTE_MASK(mtk_dp, REG_3868_DP_MST_DPTX,
			 1 << UPDATE_ID_BUF_TX_TRIG_DP_MST_DPTX_FLDMASK_POS,
				UPDATE_ID_BUF_TX_TRIG_DP_MST_DPTX_FLDMASK);

	/* trigger to update VC payload table */
	WRITE_BYTE_MASK(mtk_dp, REG_3980_DP_MST_DPTX,
			1 << VC_PAYLOAD_TABLE_TX_UPDATE_DP_MST_DPTX_FLDMASK_POS,
				VC_PAYLOAD_TABLE_TX_UPDATE_DP_MST_DPTX_FLDMASK);
}

/**
 * mtk_dp_mst_hal_stream_enable() - mapping stream to encoder and start output
 * @vcpi_mask: vc payload id bitwise mask (1b'1 means used 1b'0 means unused)
 * @max_payloads: maximum number of stream payloads
 *
 * Default mapping encoder N to stream N
 */
void mtk_dp_mst_hal_stream_enable(struct mtk_dp *mtk_dp,
				  const u8 vcpi_mask, const u32 max_payloads)
{
	u32 reg_offset_enc, encoder_id;
	u32 mst_stream_en_mask;
	u32 mst_stream_en_shift;
	u32 stream_count = 0;

	mst_stream_en_shift = max_payloads < 8 ? max_payloads : 8;
	mst_stream_en_mask = (1 << mst_stream_en_shift) - 1;

	for (encoder_id = 0; encoder_id < DP_STREAM_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSET(encoder_id);
		if ((vcpi_mask >> encoder_id) & 0x1) {
			/*reg_dp_mst_en*/
			WRITE_BYTE_MASK(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
					1 << DP_MST_EN_DP_ENCODER1_P0_FLDMASK_POS,
							DP_MST_EN_DP_ENCODER1_P0_FLDMASK);
			stream_count++;
		}
	}

	/* disable remaining encoders */
	for (encoder_id = stream_count; encoder_id < DP_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSET(encoder_id);

		WRITE_BYTE_MASK(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
				0x0, DP_MST_EN_DP_ENCODER1_P0_FLDMASK);
	}

	WRITE_BYTE_MASK(mtk_dp, REG_3930_DP_MST_DPTX,
			((0x1 << stream_count) - 1), mst_stream_en_mask);

	DP_MSG("Configure reg_dp_mst_en 0x%x\n", vcpi_mask);
}

/**
 * mtk_dp_mst_hal_trigger_act() - trigger ACT update event
 */
void mtk_dp_mst_hal_trigger_act(struct mtk_dp *mtk_dp)
{
	/* trigger VCPF */
	WRITE_BYTE_MASK(mtk_dp, REG_3894_DP_MST_DPTX,
			(0x1 << RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK_POS) |
			(0x1 << RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK_POS),
			RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
			RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);

	/* write one trigger */
	WRITE_BYTE_MASK(mtk_dp, REG_3980_DP_MST_DPTX,
			0x1 << ACT_TRIGGER_MST_TX_DP_MST_DPTX_FLDMASK_POS,
			ACT_TRIGGER_MST_TX_DP_MST_DPTX_FLDMASK);
	usleep_range(10, 11);

	/* trigger VCPF */
	WRITE_BYTE_MASK(mtk_dp, REG_3894_DP_MST_DPTX, 0,
			RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
			RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	WRITE_BYTE_MASK(mtk_dp, REG_3980_DP_MST_DPTX,
			0x1 << TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK_POS,
			TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK);
#endif
}

/**
 * mtk_dp_mst_drv_init_variable() - initialize all MST relative SW parameters
 */
static void mtk_dp_mst_drv_init_variable(struct mtk_dp *mtk_dp)
{
	enum dp_stream_id stream_id;

	/* enc_id equals to stream_id */
	for (stream_id = DP_STREAM_ID_0; stream_id < DP_STREAM_MAX; stream_id++) {
		mtk_dp->stream_info[stream_id].ideal_timing = SINK_1920_1080;
		mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;
		mtk_dp->stream_info[stream_id].color_depth = DP_COLOR_DEPTH_8BIT;
		mtk_dp->stream_info[stream_id].pg_type = DP_PG_HORIZONTAL_RAMPING + stream_id;
		mtk_dp->stream_info[stream_id].color_format = DP_COLOR_FORMAT_RGB_444;
		mtk_dp->stream_info[stream_id].is_dsc = false;
		mtk_dp->stream_info[stream_id].audio_freq = FS_192K;
		mtk_dp->stream_info[stream_id].audio_ch = 2;
		mtk_dp->stream_info[stream_id].port = NULL;

		mtk_dp->info[stream_id].input_src = DP_SRC_DPINTF;//to check
		mtk_dp->info[stream_id].pattern_gen = false;//to check
		mtk_dp->info[stream_id].pattern_id =
			mtk_dp->stream_info[stream_id].ideal_timing;
		mtk_dp->info[stream_id].format = DP_COLOR_FORMAT_RGB_444;
		mtk_dp->info[stream_id].depth =
			mtk_dp->stream_info[stream_id].color_depth;
		mtk_dp->info[stream_id].dp_output_timing.frame_rate = 60;
	}

	mtk_dp->stream_info[DP_STREAM_ID_0].pg_type = DP_PG_VERTICAL_COLOR_BAR;
#if (DP_ENCODER_NUM >= 2)
	mtk_dp->stream_info[DP_STREAM_ID_1].pg_type = DP_PG_HORIZONTAL_COLOR_BAR;
#endif
	mtk_drm_dp_mst_init();
}

static void mtk_dp_mst_drv_fec_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	DP_MSG("Set FEC %s!\r\n", enable ? "ON" : "OFF");

	mtk_dp_fec_enable(mtk_dp, enable);

	if (!enable)
		mtk_dp->is_mst_fec_en = false;
}

/**
 * mtk_dp_mst_drv_stream_enable() - start to output video & audio streams
 */
static void mtk_dp_mst_drv_stream_enable(struct mtk_dp *mtk_dp,
					 unsigned long vcpi_mask, int max_payloads)
{
	enum dp_stream_id stream_id;
	enum dp_encoder_id encoder_id;
	u8 channel;
	enum audio_fs fs;
	enum audio_len len = WL_24BIT;
	u8 div = 0x4;

	mtk_dp_mst_hal_stream_enable(mtk_dp, vcpi_mask, max_payloads);

	for (stream_id = DP_STREAM_ID_0; stream_id < DP_STREAM_MAX; stream_id++) {
		encoder_id = (enum dp_encoder_id)stream_id;
		channel = mtk_dp->stream_info[stream_id].audio_ch;
		fs = mtk_dp->stream_info[stream_id].audio_freq;

		if (mtk_dp->stream_info[stream_id].final_timing < SINK_MAX) {
			DP_MSG("Encoder %d, PG type %d, Color Depth %d, pattern_id %d\n",
			       encoder_id,
				 mtk_dp->stream_info[stream_id].pg_type,
				 mtk_dp->stream_info[stream_id].color_depth,
				 mtk_dp->stream_info[stream_id].final_timing);

			mtk_dp->info[encoder_id].resolution =
				mtk_dp->stream_info[stream_id].final_timing;
			mtk_dp->info[encoder_id].depth =
				mtk_dp->stream_info[stream_id].color_depth;
			mtk_dp->info[encoder_id].format =
				mtk_dp->stream_info[stream_id].color_format;
			mtk_dp->info[encoder_id].pattern_id =
				mtk_dp->stream_info[stream_id].final_timing;

			mtk_dp_video_enable(mtk_dp, encoder_id, true);

			if (mtk_dp->audio_enable) {
				DP_MSG("audio %d ch %d, Fs %d, len %d, div %d\n",
				       encoder_id, channel, fs, len, div);
				if (mtk_dp->info[encoder_id].input_src == DP_SRC_PG) {
					mtk_dp_audio_pg_enable(mtk_dp, encoder_id,
							       channel, fs, true);
					mtk_dp_audio_ch_status_set(mtk_dp, encoder_id,
								   channel, fs, len);
					mtk_dp_audio_mdiv_set(mtk_dp, encoder_id, div);
					mtk_dp_audio_sdp_setting(mtk_dp, encoder_id, channel);
				} else {
					mtk_dp_audio_pg_enable(mtk_dp, encoder_id,
							       channel, fs, false);
					mtk_dp_i2s_audio_config(mtk_dp, encoder_id);
				}
			} else {
				DP_MSG("Audio disable, due to audio_enable is False\n");
			}
		}
	}
}

/**
 * mtk_dp_mst_drv_update_vcp_table() - update vcp table for all devices in the topology
 */
static void mtk_dp_mst_drv_update_vcp_table(struct mtk_dp *mtk_dp,
					    struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	enum dp_encoder_id encoder_id = 0;
	u16 start_slot, end_slot;
	u16 mst_stream_en_mask;
	u16 mst_stream_en_shift;
	//u32 vcpi_mask_local_temp;
	u32 vcpi_mask_local;
	int payload_idx;
#if DP_DRM_COMMON //to check
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_dp_mst_topology_state *state;
#else
	struct mtk_drm_dp_payload *payload;
#endif

	mst_stream_en_shift = mgr->max_payloads < 8  ? mgr->max_payloads : 8;
	mst_stream_en_mask = (1 << mst_stream_en_shift) - 1;
#if DP_DRM_COMMON //to check
	state = to_drm_dp_mst_topology_state(mtk_dp->mgr.base.state);
	vcpi_mask_local = state->payload_mask & mst_stream_en_mask;
#else
	vcpi_mask_local = mgr->vcpi_mask & mst_stream_en_mask;
#endif
	//mtk_dp->enc_id = DPTX_ENC_ID_0;

	mtk_dp_mst_hal_reset_payload(mtk_dp);

	if (vcpi_mask_local == 0) {
		DP_MSG("No payload id was assigned, skip payload configuration\n");
		return;
	}

	//for (payload_idx = (DP_STREAM_MAX - 1); payload_idx >= 0; payload_idx--) {
	for (payload_idx = 0; payload_idx < DP_STREAM_MAX; payload_idx++) {
		if (((vcpi_mask_local >> payload_idx) & 0x1) == 0x0)
			continue;
#if DP_DRM_COMMON //to check
		payload = drm_atomic_get_mst_payload_state(state,
							   mtk_dp->mtk_connector[encoder_id]->port);

		start_slot = payload->vc_start_slot;
		end_slot = start_slot + payload->time_slots;

		mtk_dp_mst_hal_set_mtp_size(mtk_dp, encoder_id, payload->time_slots);

		//vcpi_mask_local_temp = vcpi_mask_local >> 1;

		DP_MSG("Start allocate VCPI %d, start slot %d, end slot %d\n",
		       payload->vcpi, start_slot, end_slot - 1);
		/* reg_vc_payload_timeslot */
		if (payload->vcpi < 1 || payload->vcpi > DP_STREAM_MAX) {
			DP_ERR("Invalid VCPI, vcpi %d\n", payload->vcpi);
		} else if ((start_slot > 64) || (end_slot > 64)) {
			DP_ERR("Invalid slot region, start_slot %d, end_slot %d\n",
			       start_slot, end_slot);
		} else {
			mtk_dp_mst_hal_set_timeslot(mtk_dp, encoder_id,
						    start_slot, end_slot, payload->vcpi);
			mtk_dp_mst_hal_set_id_buf(mtk_dp, encoder_id, payload->vcpi);
		}
#else
		payload = mtk_drm_dp_mst_get_payload(payload_idx);

		start_slot = payload->start_slot;
		end_slot = start_slot + payload->num_slots;

		mtk_dp_mst_hal_set_mtp_size(mtk_dp, encoder_id, payload->num_slots);

		//vcpi_mask_local_temp = vcpi_mask_local >> 1;

		DP_MSG("Start allocate VCPI %d, start slot %d, end slot %d\n",
		       payload->vcpi, start_slot, end_slot - 1);
		/* reg_vc_payload_timeslot */
		if (payload->vcpi < 1 || payload->vcpi > DP_STREAM_MAX) {
			DP_ERR("Invalid VCPI, vcpi %d\n", payload->vcpi);
		} else if ((start_slot > 64) || (end_slot > 64)) {
			DP_ERR("Invalid slot region, start_slot %d, end_slot %d\n",
			       start_slot, end_slot);
		} else {
			mtk_dp_mst_hal_set_timeslot(mtk_dp, encoder_id
						, start_slot, end_slot, payload->vcpi);
			mtk_dp_mst_hal_set_id_buf(mtk_dp, encoder_id, payload->vcpi);
		}
#endif
		encoder_id++;
	}

	mtk_dp_mst_hal_vcp_table_update(mtk_dp);
}

/**
 * mtk_dp_mst_drv_update_payload() - update payload by DRM API
 */
static void mtk_dp_mst_drv_update_payload(struct mtk_dp *mtk_dp,
					  struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	int status;
	int counter = 5;

	DP_MSG("===DPCD_001C0===\n\n");
	mtk_drm_dp_update_payload_part1(mgr, DP_PAYLOAD_START_SLOT);

	/* Base on vcpi to update payload, should assign payload ID first */
	mtk_dp_mst_drv_update_vcp_table(mtk_dp, mgr);

	do {
		mtk_dp_mst_hal_trigger_act(mtk_dp);
		DP_MSG("===trigger ACT===\n\n");
		status = mtk_drm_dp_check_act_status(mgr);
		DP_MSG("DPCD_0020C status %x\n", status);
		if (counter-- <= 0)
			break;
	} while (!((status & DP_PAYLOAD_ACT_HANDLED) && (status > 0)));

	DP_MSG("update_payload_part2\n");
	mtk_drm_dp_update_payload_part2(mgr);
}

void mtk_dp_mst_drv_timing_getting(struct mtk_dp *mtk_dp, enum dp_stream_id stream_id,
				   struct dp_timing_parameter *p_DPTX_TBL)
{
	struct videomode vm = {0};

	vm.hactive = mtk_dp->mode[stream_id].hdisplay;
	vm.hfront_porch = mtk_dp->mode[stream_id].hsync_start - mtk_dp->mode[stream_id].hdisplay;
	vm.hsync_len = mtk_dp->mode[stream_id].hsync_end - mtk_dp->mode[stream_id].hsync_start;
	vm.hback_porch = mtk_dp->mode[stream_id].htotal - mtk_dp->mode[stream_id].hsync_end;
	vm.vactive = mtk_dp->mode[stream_id].vdisplay;
	vm.vfront_porch = mtk_dp->mode[stream_id].vsync_start - mtk_dp->mode[stream_id].vdisplay;
	vm.vsync_len = mtk_dp->mode[stream_id].vsync_end - mtk_dp->mode[stream_id].vsync_start;
	vm.vback_porch = mtk_dp->mode[stream_id].vtotal - mtk_dp->mode[stream_id].vsync_end;
	vm.pixelclock = mtk_dp->mode[stream_id].clock * 1000;

	p_DPTX_TBL->frame_rate = mtk_dp->mode[stream_id].clock * 1000 /
		mtk_dp->mode[stream_id].htotal / mtk_dp->mode[stream_id].vtotal;
	p_DPTX_TBL->htt = mtk_dp->mode[stream_id].htotal;
	p_DPTX_TBL->hbp = vm.hback_porch;
	p_DPTX_TBL->hsw = vm.hsync_len;
	p_DPTX_TBL->hsp = 1; /* todo */
	p_DPTX_TBL->hfp = vm.hfront_porch;
	p_DPTX_TBL->hde = vm.hactive;
	p_DPTX_TBL->vtt = mtk_dp->mode[stream_id].vtotal;
	p_DPTX_TBL->vbp = vm.vback_porch;
	p_DPTX_TBL->vsw = vm.vsync_len;
	p_DPTX_TBL->vsp = 1; /* todo */
	p_DPTX_TBL->vfp = vm.vfront_porch;
	p_DPTX_TBL->vde = vm.vactive;

	/* interlace not support */
	p_DPTX_TBL->video_ip_mode = DP_VIDEO_PROGRESSIVE;
}

union dp_pps pps_user[DP_ENCODER_ID_MAX]; //to check

/**
 * mtk_dp_mst_drv_choose_timing() - select output video timing by mtk policy
 * @available_pbn: current available pbns of the link bandwidth
 * @resolution: config expected video resolution
 * @color_depth: config expected video color depth
 * @is_dsc: is this video need DSC function or not
 */
static int mtk_dp_mst_drv_choose_timing(struct mtk_dp *mtk_dp,
					enum dp_stream_id  stream_id, int avai_pbn,
					  u8 res, u8 color_depth, u8 is_dsc)
{
	u32 pixel_clock;
	u32 allocate_pbn;
	int pattern_id;
	u32 htt, vtt;
	u8  bpp =  mtk_dp_color_get_bpp(mtk_dp->stream_info[stream_id].color_format, color_depth);

	if (avai_pbn < 0) {
		DP_ERR("Available PBN is invalid, available PBN %d\n", avai_pbn);
		return -1;
	}

	/* search timing to meet current available PBN */
	for (pattern_id = res; pattern_id >= 0; pattern_id--) {
		//mtk_dp->info[stream_id].resolution = pattern_id;
		//mtk_dp_timing_config();

		mtk_dp_mst_drv_timing_getting(mtk_dp, stream_id,
					      &mtk_dp->info[stream_id].dp_output_timing);
		if (mtk_dp->dsc_enable) {
			//Calculate pixel clock for Compressed timing
			//According formula of eDP simulation
			//Compressed HDE = CEIL(MSA_HDE * bpp/12/8)*4
			u8 dsc_bpp = ((pps_user[stream_id].pps_raw[4] & 0x3) << 4) |
						(pps_user[stream_id].pps_raw[5]  >> 4); //1/16;
			u16 MSAHDE = mtk_dp->info[stream_id].dp_output_timing.hde;
			u16 MSAHBP = mtk_dp->info[stream_id].dp_output_timing.hbp;
			u16 MSAHFP = mtk_dp->info[stream_id].dp_output_timing.hfp;
			u16 MSAHSW = mtk_dp->info[stream_id].dp_output_timing.hsw;
			u16 DSCHDE = ((MSAHDE * dsc_bpp + (12 * 8 - 1)) / (12 * 8)) * 4;

			htt = DSCHDE + MSAHBP + MSAHFP + MSAHSW;
		} else {
			htt = mtk_dp->info[stream_id].dp_output_timing.htt;
		}

		vtt = mtk_dp->info[stream_id].dp_output_timing.vtt;
		pixel_clock = htt * vtt * mtk_dp->info[stream_id].dp_output_timing.frame_rate;

		/* unit: kBps, 640_480x30 fps = 12.6MBps, 7680_4320x120 fps = 4,226.8 MBps */
		pixel_clock = (pixel_clock + (1000 - 1)) / 1000;
#if DP_DRM_COMMON //to check
		allocate_pbn = drm_dp_calc_pbn_mode(pixel_clock, bpp);
#else
		allocate_pbn = mtk_drm_dp_calc_pbn_mode(pixel_clock, bpp, is_dsc);
#endif
		DP_MSG("res %d, htt %d, vtt %d, frame_rate %d, bpp %d, pixel_clock %d\n",
		       pattern_id, htt, vtt, mtk_dp->info[stream_id].dp_output_timing.frame_rate,
			bpp, pixel_clock);

		if (allocate_pbn < (u32)avai_pbn) {
			mtk_dp->stream_info[stream_id].final_timing = pattern_id;
			DP_MSG("require PBN %d, available PBN %d\n",
			       allocate_pbn, (avai_pbn - allocate_pbn));

			return allocate_pbn;
		}
	}

	mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;

	return -1;
}

static u8 mtk_dp_mst_drv_find_vcpi_slots(struct mtk_dp *mtk_dp,
					 struct mtk_drm_dp_mst_topology_mgr *mgr,
					   const int pbn_allocating)
{
#if DP_DRM_COMMON
	int slots = 60; //to check
#else
	int slots = mtk_drm_dp_find_vcpi_slots(mgr, pbn_allocating);
#endif

	DP_DBG("Slots before fine-tune %d\n", slots);

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_1LANE:
		slots += (4 - (slots % 4));
		break;
	case DP_2LANE:
		slots += (2 - (slots % 2));
		break;
	case DP_4LANE:
		slots++;
		break;
	}

	//mdr_dptx_mst_GetReqSlotForAudioSymbol(dpTx_ID, dpOutStreamID, &slots);

	if (slots < 1 || slots > 63) {
		DP_ERR("Un-expected slots %d\n", slots);
		return 0;
	}

	DP_DBG("Slots after fine-tune %d\n", slots);
	return (u8)slots;
}

/**
 * mtk_dp_mst_drv_allocate_vcpi() - allocate vcp id to all devices in the topology
 *
 * According to the output video timing for each device, calculate its pbn and vcpi
 */
static int mtk_dp_mst_drv_allocate_vcpi(struct mtk_dp *mtk_dp,
					struct mtk_drm_dp_mst_topology_mgr *mgr,
	struct mtk_drm_dp_mst_branch *mstb,
	enum dp_stream_id *p_stream_id,
	const u8 is_enable,
	int avail_pbn)
{
	struct mtk_drm_dp_mst_port *port;
	int allocate_pbn;
	u32 slots;
	int ret = 1;
	int ret_tmp = 0;
	enum dp_stream_id  stream_id = *p_stream_id;

	list_for_each_entry(port, &mstb->ports, next) {
		struct mtk_drm_dp_mst_branch *mstb_child = NULL;

		if (port->input || !port->ddps)
			continue;
#if DP_DRM_COMMON
		if (is_enable) { //to check
#else
		if (is_enable && mtk_drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
#endif
			if (port->port_num >= DP_PORT_NUM_MAX) {
				continue;
				DP_MSG("skip port_num %d!\n", port->port_num);
			}
			if (stream_id >= DP_STREAM_MAX) {
				DP_MSG("Return! All streams have been allocated !\n");
				return 0;
			}
			DP_MSG("allocate vcpi on LCT %d with port_num %d\n",
			       mstb->lct, port->port_num);
			allocate_pbn = mtk_dp_mst_drv_choose_timing(mtk_dp, stream_id,
								    avail_pbn,
						mtk_dp->stream_info[stream_id].ideal_timing,
						mtk_dp->stream_info[stream_id].color_depth,
						mtk_dp->stream_info[stream_id].is_dsc);

			if (allocate_pbn < 0)
				continue;

			slots = mtk_dp_mst_drv_find_vcpi_slots(mtk_dp, mgr, allocate_pbn);

#if DP_DRM_COMMON
//to check
#else
			allocate_pbn = mgr->pbn_div * slots;
#endif
			avail_pbn -= allocate_pbn;

			DP_DBG("Slots %d, PBN %d, pbn_div %d\n",
			       slots, allocate_pbn, mgr->pbn_div);
#if DP_DRM_COMMON
			//to check
#else
			ret_tmp = mtk_drm_dp_mst_allocate_vcpi(mgr, port, allocate_pbn, slots);
#endif
			mtk_dp->stream_info[stream_id].port = port;

			(*p_stream_id)++;
			stream_id = *p_stream_id;

			if (ret_tmp > 0)
				ret = ret_tmp;
		}

#if DP_DRM_COMMON
		//to check
#else
		if (port->mstb)
			mstb_child = mtk_drm_dp_mst_topology_get_mstb_validated(mgr, port->mstb);
#endif
		if (mstb_child) {
			ret_tmp = mtk_dp_mst_drv_allocate_vcpi(mtk_dp, mgr, mstb_child,
							       p_stream_id, is_enable, avail_pbn);
			stream_id = *p_stream_id;
			if (ret_tmp < 0)
				ret = ret_tmp;
#if DP_DRM_COMMON
		//to check
#else
			mtk_drm_dp_mst_topology_put_mstb(mstb_child);
#endif
		}
	}
	return ret;
}

/**
 * mtk_dp_mst_drv_allocate_stream() - allocate vcpi and streams
 */
static void mtk_dp_mst_drv_allocate_stream(struct mtk_dp *mtk_dp,
					   struct mtk_drm_dp_mst_topology_mgr *mgr,
					     bool is_enable)
{
	struct mtk_drm_dp_mst_branch *mstb = mgr->mst_primary;
	enum dp_stream_id stream_id = 0;

	DP_MSG("===allocate vcpi===\n\n");
	mtk_dp_mst_drv_allocate_vcpi(mtk_dp, mgr, mstb, &stream_id, is_enable,
				     (mgr->pbn_div * 63));

	DP_MSG("===stream Enable===\n\n");
	mtk_dp_mst_drv_stream_enable(mtk_dp, mgr->vcpi_mask, mgr->max_payloads);
}

/**
 * mtk_dp_mst_drv_clear_vcpi() - clear vcpi table for devices in the topology
 */
static void mtk_dp_mst_drv_clear_vcpi(struct mtk_dp *mtk_dp,
				      struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	enum dp_stream_id stream_id;
	u8 temp_value[0x3] = {0x0, 0x0, 0x3F};
	u8 ret = 0;

	for (stream_id = DP_STREAM_ID_0; stream_id < DP_STREAM_MAX; stream_id++) {
		DP_DBG("Stream %d\n", (int)stream_id);
#if DP_DRM_COMMON
		//to check
#else
		if (mtk_dp->stream_info[stream_id].port  != 0)
			mtk_drm_dp_mst_deallocate_vcpi(mgr, mtk_dp->stream_info[stream_id].port);
#endif
		mtk_dp->stream_info[stream_id].port = NULL;
		mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;
	}
	mgr->payload_id_table_cleared = false;

	/* clear DPCD_001C0 ~ DPCD_001C2 */
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DPCD_001C0, temp_value, 0x3);
	DP_MSG("Clear DPCD_001C0 ~ DPCD_001C2, result %d\n", ret);
}

#if ENABLE_SERDES_MST
struct mtk_drm_dp_payload payload[DP_STREAM_MAX];
static void mtk_dp_mst_update_vcp_table(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id = 0;
	u16 start_slot, end_slot;
	int payload_idx;

	mtk_dp_mst_hal_reset_payload(mtk_dp);

	for (payload_idx = 0; payload_idx < DP_STREAM_MAX; payload_idx++) {
		start_slot = payload[payload_idx].start_slot;
		end_slot = start_slot + payload[payload_idx].num_slots;

		mtk_dp_mst_hal_set_mtp_size(mtk_dp, encoder_id, payload[payload_idx].num_slots);

		DP_MSG("Start allocate VCPI %d, start slot %d, end slot %d\n",
		       payload[payload_idx].vcpi, start_slot, end_slot - 1);
		/* reg_vc_payload_timeslot */
		if (payload[payload_idx].vcpi < 1 || payload[payload_idx].vcpi > DP_STREAM_MAX) {
			DP_ERR("Invalid VCPI, vcpi %d\n", payload[payload_idx].vcpi);
		} else if ((start_slot > 64) || (end_slot > 64)) {
			DP_ERR("Invalid slot region, start_slot %d, end_slot %d\n",
			       start_slot, end_slot);
		} else {
			mtk_dp_mst_hal_set_timeslot(mtk_dp, encoder_id
						, start_slot, end_slot, payload[payload_idx].vcpi);
			mtk_dp_mst_hal_set_id_buf(mtk_dp, encoder_id, payload[payload_idx].vcpi);
		}
		encoder_id++;
	}

	mtk_dp_mst_hal_vcp_table_update(mtk_dp);
}

static int mtk_dp_mst_dpcd_write_payload(struct mtk_dp *mtk_dp,
					 int id, u8 start_slot, u8 num_slots)
{
	u8 payload_alloc[3], status;
	int ret;
	int retries = 0;

	drm_dp_dpcd_writeb(&mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS,
			   DP_PAYLOAD_TABLE_UPDATED);

	payload_alloc[0] = id;
	payload_alloc[1] = start_slot;
	payload_alloc[2] = num_slots;

	ret = drm_dp_dpcd_write(&mtk_dp->aux, DP_PAYLOAD_ALLOCATE_SET, payload_alloc, 3);
	if (ret != 3) {
		DP_MSG("failed to write payload allocation %d\n", ret);
		goto fail;
	}

retry:
	ret = drm_dp_dpcd_readb(&mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status);
	if (ret < 0) {
		DP_MSG("failed to read payload table status %d\n", ret);
		goto fail;
	}

	if (!(status & DP_PAYLOAD_TABLE_UPDATED)) {
		retries++;
		if (retries < 20) {
			usleep_range(10000, 20000);
			goto retry;
		}
		DP_MSG("status not set after read payload table status %d\n",
		       status);
		ret = -EINVAL;
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int mtk_dp_mst_do_get_act_status(struct mtk_dp *mtk_dp)
{
	int ret;
	u8 status;

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status, 1);
	if (ret == 0)
		return ret;

	return status;
}

static int mtk_dp_mst_check_act_status(struct mtk_dp *mtk_dp)
{
	const int timeout_ms = 3000;
	int status, i;

	for (i = 0; i < timeout_ms; i += 200) {
		msleep(200);
		status = mtk_dp_mst_do_get_act_status(mtk_dp);

		if (status & DP_PAYLOAD_ACT_HANDLED || status < 0)
			break;
	}

	if (i > timeout_ms && status >= 0) {
		DP_ERR("Failed to get ACT after %dms, last status: %02x\n",
		       timeout_ms, status);
		return -1;
	} else if (status < 0) {
		/* Failure here isn't unexpected - the hub may have just been unplugged */
		DP_ERR("Failed to read payload table status: %d\n", status);
		return status;
	}

	return status;
}

static void mtk_dp_mst_update_payload(struct mtk_dp *mtk_dp)
{
	int status = 0;
	int counter = 5;
	int payload_idx;
	enum dp_stream_id stream_id;
	enum dp_encoder_id encoder_id;

	DP_MSG("===DPCD_001C0===\n\n");

	for (payload_idx = 0; payload_idx < DP_STREAM_MAX; payload_idx++)
		mtk_dp_mst_dpcd_write_payload
		(mtk_dp,
		payload_idx + 1,
		payload[payload_idx].start_slot,
		payload[payload_idx].num_slots);

	mtk_dp_mst_update_vcp_table(mtk_dp);

	do {
		mtk_dp_mst_hal_trigger_act(mtk_dp);
		DP_MSG("===trigger ACT===\n\n");

		status = mtk_dp_mst_check_act_status(mtk_dp);

		DP_MSG("DPCD_0020C status %x\n", status);
		if (counter-- <= 0)
			break;
	} while (!((status & DP_PAYLOAD_ACT_HANDLED) && (status > 0)));

	for (stream_id = DP_STREAM_ID_0; stream_id < DP_STREAM_MAX; stream_id++) {
		encoder_id = (enum dp_encoder_id)stream_id;
		mtk_dp_video_mute(mtk_dp, encoder_id, false);
		if (mtk_dp->audio_enable)
			mtk_dp_audio_mute(mtk_dp, encoder_id, false);
		else
			mtk_dp_audio_mute(mtk_dp, encoder_id, true);
	}
}

int mtk_dp_mst_calc_pbn_mode(struct mtk_dp *mtk_dp, int clock, int bpp, bool dsc)
{
	u32 pbn;

	if (dsc)
		pbn = (clock / 8) * (bpp / 16);
	else
		pbn = (clock / 8) * bpp;

	if (mtk_dp->is_mst_fec_en) {
		pbn = (pbn * 1030);
		DP_DBG("Add %u\n", pbn);
	} else {
		pbn = (pbn * 1006);
	}

	pbn = (pbn / 54) * 64;
	pbn = (pbn + (1000000 - 1)) / 1000000;

	return pbn + 1; // roundup
}

union dp_pps pps_user[DP_ENCODER_ID_MAX]; //to check
static int mtk_dp_mst_choose_timing(struct mtk_dp *mtk_dp,
				    enum dp_stream_id stream_id,
					  u8 res, u8 color_depth, u8 is_dsc)
{
	u32 pixel_clock;
	u32 allocate_pbn;
	u32 htt, vtt;
	u8  bpp =  mtk_dp_color_get_bpp(mtk_dp->stream_info[stream_id].color_format, color_depth);

	mtk_dp_mst_drv_timing_getting(mtk_dp, stream_id,
				      &mtk_dp->info[stream_id].dp_output_timing);
	if (mtk_dp->dsc_enable) {
		//Calculate pixel clock for Compressed timing
		//According formula of eDP simulation
		//Compressed HDE = CEIL(MSA_HDE * bpp/12/8)*4
		u8 dsc_bpp = ((pps_user[stream_id].pps_raw[4] & 0x3) << 4) |
					(pps_user[stream_id].pps_raw[5]  >> 4); //1/16;
		u16 MSAHDE = mtk_dp->info[stream_id].dp_output_timing.hde;
		u16 MSAHBP = mtk_dp->info[stream_id].dp_output_timing.hbp;
		u16 MSAHFP = mtk_dp->info[stream_id].dp_output_timing.hfp;
		u16 MSAHSW = mtk_dp->info[stream_id].dp_output_timing.hsw;
		u16 DSCHDE = ((MSAHDE * dsc_bpp + (12 * 8 - 1)) / (12 * 8)) * 4;

		htt = DSCHDE + MSAHBP + MSAHFP + MSAHSW;
	} else {
		htt = mtk_dp->info[stream_id].dp_output_timing.htt;
	}

	vtt = mtk_dp->info[stream_id].dp_output_timing.vtt;
	pixel_clock = htt * vtt * mtk_dp->info[stream_id].dp_output_timing.frame_rate;

	/* unit: kBps, 640_480x30 fps = 12.6MBps, 7680_4320x120 fps = 4,226.8 MBps */
	pixel_clock = (pixel_clock + (1000 - 1)) / 1000;

	allocate_pbn = mtk_dp_mst_calc_pbn_mode(mtk_dp, pixel_clock, bpp, is_dsc);

	DP_MSG("htt %d, vtt %d, frame_rate %d, bpp %d, pixel_clock %d\n",
	       htt, vtt, mtk_dp->info[stream_id].dp_output_timing.frame_rate,
		bpp, pixel_clock);

	mtk_dp->stream_info[stream_id].final_timing = SINK_1920_1080;
	DP_MSG("require PBN %d\n", allocate_pbn);

	return allocate_pbn;
}

static u8 mtk_dp_mst_find_vcpi_slots(struct mtk_dp *mtk_dp, int pbn_div, const int pbn_allocating)
{
	int slots = 0;

	slots = (pbn_allocating / pbn_div) + 1; //DIV_ROUND_UP(pbn, mgr->pbn_div);

	/* max. time slots - one slot for MTP header */
	if (slots > 63) {
		DP_MSG("error, slots:%d > 63\n", slots);
		return 0;
	}

	DP_MSG("Slots before fine-tune %d\n", slots);

	switch (mtk_dp->training_info.link_lane_count) {
	case DP_1LANE:
		slots += (4 - (slots % 4));
		break;
	case DP_2LANE:
		slots += (2 - (slots % 2));
		break;
	case DP_4LANE:
		slots++;
		break;
	}

	//mdr_dptx_mst_GetReqSlotForAudioSymbol(dpTx_ID, dpOutStreamID, &slots);

	if (slots < 1 || slots > 63) {
		DP_ERR("Un-expected slots %d\n", slots);
		return 0;
	}

	DP_MSG("Slots after fine-tune %d\n", slots);
	return (u8)slots;
}

static int mtk_dp_mst_get_vc_payload_bw(int link_rate, int link_lane_count)
{
	if (link_rate == 0 || link_lane_count == 0)
		DP_ERR("invalid link rate/lane count: (%d / %d)\n",
		       link_rate, link_lane_count);

	/* See DP v2.0 2.6.4.2, VCPayload_Bandwidth_for_OneTimeSlotPer_MTP_Allocation */
	return link_rate * link_lane_count / 54000;
}

static int mtk_dp_mst_allocate_vcpi(struct mtk_dp *mtk_dp, const u8 is_enable)
{
	int allocate_pbn;
	u32 slots;
	int payload_idx;
	int lane_count;
	int link_rate;
	int pbn_div;

	lane_count = mtk_dp->training_info.link_lane_count & 0xf;
	link_rate = mtk_dp->training_info.link_rate * 27000;

	pbn_div = mtk_dp_mst_get_vc_payload_bw(link_rate, lane_count);

	for (payload_idx = 0; payload_idx < DP_STREAM_MAX; payload_idx++) {
		if (is_enable) {
			if (payload_idx >= DP_STREAM_MAX) {
				DP_MSG("Return! All streams have been allocated !\n");
				return 0;
			}

			allocate_pbn = mtk_dp_mst_choose_timing
				(mtk_dp,
				payload_idx,
				mtk_dp->stream_info[payload_idx].ideal_timing,
				mtk_dp->stream_info[payload_idx].color_depth,
				mtk_dp->stream_info[payload_idx].is_dsc);

			if (allocate_pbn < 0) {
				DP_MSG("Return! allocate_pbn fail!\n");
				return 0;
			}

			slots = mtk_dp_mst_find_vcpi_slots(mtk_dp, pbn_div, allocate_pbn);

			allocate_pbn = pbn_div * slots;

			DP_DBG("Slots %d, PBN %d, pbn_div %d\n",
			       slots, allocate_pbn, pbn_div);

			if (payload_idx == 0) {
				payload[payload_idx].start_slot = 1;
				payload[payload_idx].vcpi = 1;
			} else if (payload_idx == 1) {
				payload[payload_idx].start_slot = 32;
				payload[payload_idx].vcpi = 2;
			}

			payload[payload_idx].num_slots = slots;
		}
	}
	return 0;
}

static void mtk_dp_mst_allocate_stream(struct mtk_dp *mtk_dp, bool is_enable)
{
	DP_MSG("===allocate vcpi===\n\n");
	if (mtk_dp->training_info.sink_count > DP_STREAM_MAX)
		mtk_dp->training_info.sink_count = DP_STREAM_MAX;

	mtk_dp_mst_allocate_vcpi(mtk_dp, is_enable);

	DP_MSG("===stream Enable===\n\n");
	mtk_dp_mst_drv_stream_enable(mtk_dp, 0x3, DP_STREAM_MAX);
}

void mtk_dp_mst_payload_handler(struct mtk_dp *mtk_dp)
{
	if (!mtk_dp->mst_enable) {
		DP_ERR("connected device does not support MST, return !\n");
		return;
	}

	if (!mtk_dp->is_mst_start) {
		DP_ERR("unexpected code flow\n");
		return;
	}

	/* allocate vcpi */
	mtk_dp_mst_allocate_stream(mtk_dp, true);

	/* update payload, after update vcpi */
	mtk_dp_mst_update_payload(mtk_dp);
}
#endif

u8 *mtk_dp_mst_drv_send_remote_i2c_read
	(struct mtk_drm_dp_mst_branch *mstb,
								struct mtk_drm_dp_mst_port *port)
{
	#define msg_num 2
	struct i2c_msg msgs[3];
	int ret = 1;
	u8 block_cnt = 0;
	u8 i2c_data_to_write_buff[1] = {0};
	u8 i2c_data_to_write_buff_segment[1] = {0x0};
	u8 block_index = 0;
	u8 segment_index = 0;
	u8 edid_temp[128] = {0x0};
	u8 *edid = kmalloc(DPTX_EDID_SIZE, GFP_KERNEL);

	if (!edid)
		DP_ERR("kmalloc EDID fail\n");
	else
		memset(edid, 0, DPTX_EDID_SIZE);

	msgs[0].addr = 0x30; //Write_I2C_Device_Identifier
	msgs[0].buf = i2c_data_to_write_buff_segment; //I2C_Data_To_Write
	msgs[0].flags = I2C_M_STOP; // No_Stop_Bit
	msgs[0].len = 1; //Number_Of_Bytes_To_Write

	msgs[1].addr = 0x50; //Write_I2C_Device_Identifier
	msgs[1].buf = i2c_data_to_write_buff; //I2C_Data_To_Write
	msgs[1].flags = I2C_M_STOP; // No_Stop_Bit
	msgs[1].len = 1; //Number_Of_Bytes_To_Write

	msgs[2].addr = 0x50; //Write_I2C_Device_Identifier
	msgs[2].buf = edid_temp;
	msgs[2].flags = 0; // Don't care
	msgs[2].len = 128; //Number_Of_Bytes_To_Read

	//if (port->port_num >= 8) {
	if (port->pdt == DP_PEER_DEVICE_SST_SINK) {
		do {
			DP_MSG("I2C read, lct %d, port_num %d\n", mstb->lct, port->port_num);

			if (segment_index <= 1) {
				ret = mtk_drm_dp_mst_i2c_read(mstb, port,
							      &msgs[1 - segment_index],
							      2 + segment_index);
			} else {
				DP_ERR("Un-supported EDID size %x\n", block_cnt * 128);
				break;
			}

			if (ret < 0)
				DP_ERR("Remote I2C read Error, lct %s, port_num %d\n",
				       &mstb->lct, port->port_num);

			memcpy(&edid[block_index * 128], msgs[2].buf, sizeof(u8) * 128);

			if (block_index == 0)
				block_cnt = msgs[2].buf[0x7E];

			block_index++;
			segment_index = (block_index >> 1);

			i2c_data_to_write_buff[0] = 128 * (block_index % 2);

			if (segment_index >= 1)
				i2c_data_to_write_buff_segment[0] = segment_index;
		} while (block_cnt-- != 0);
	}
#if ENABLE_DPTX_DEBUG
	{
		u16 i = 0;

		printf("EDID total msg:");
		for (i = 0; i < DPTX_EDID_SIZE; i++) {
			if ((i % 0x10) == 0x0)
				printf("\nidx %x: ", i);
			printf("%x ", edid[i]);
		}
		printf("\n");
	}
#endif
	#undef msg_num
	return edid;
}

#if DPTX_MST_FEC_ENABLE
static int mtk_dp_mst_drv_send_remote_dpcd_read_fec_en
(struct mtk_drm_dp_mst_topology_mgr *mgr,
						struct mtk_drm_dp_mst_branch *mstb, u16 *p_bytes)
{
	struct mtk_drm_dp_mst_port *port;
	int ret = 0;
	int ret_tmp = 0;
	u16 bytes_tmp = 0;

	port = list_first_entry(&mstb->ports, typeof(*port), next);

	list_for_each_entry(port, &mstb->ports, next) {
		struct mtk_drm_dp_mst_branch *mstb_child = NULL;

		if (port->input || !port->ddps)
			continue;

		DP_DBG("To send remote DPCD read, lct %d, port %x, port_num %d\n",
		       mstb->lct, port, port->port_num);
		ret_tmp = mtk_drm_dp_mst_dpcd_read(&port->aux, DPCD_00090, &bytes_tmp, 0x1);
		if (ret_tmp < 0) {
			DP_ERR("Remote DPCD read fail %d, lct %d, port_num %d\n",
			       ret_tmp, mstb->lct, port->port_num);
			return ret_tmp;
		}

		bytes_tmp &= BIT(0);
		*p_bytes &= bytes_tmp;

		DP_DBG("FEC supported %d, lct %d, port %x, port_num %d\n", bytes_tmp, mstb->lct,
		       port, port->port_num);
		if (*p_bytes == BIT(0)) {
#if DP_DRM_COMMON
			//to check
#else
			if (port->mstb)
				mstb_child = mtk_drm_dp_mst_topology_get_mstb_validated
				(mgr, port->mstb);
#endif
			if (mstb_child) {
				ret_tmp = mtk_dp_mst_drv_send_remote_dpcd_read_fec_en
					(mgr, mstb_child, p_bytes);
				if (ret_tmp < 0)
					ret = ret_tmp;
#if DP_DRM_COMMON
				//to check
#else
				mtk_drm_dp_mst_topology_put_mstb(mstb_child);
#endif
			}
		} else {
			DP_MSG("FEC un-supported, lct %d, port_num %d\n",
			       mstb->lct, port->port_num);
		}
	}

	return ret;
}
#endif

/**
 * mtk_dp_mst_drv_payload_handler() - start to output MST streams
 */
void mtk_dp_mst_drv_payload_handler(struct mtk_dp *mtk_dp,
				    struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	if (!mtk_dp->mst_enable) {
		DP_ERR("connected device does not support MST, return !\n");
		return;
	}

	if (!mtk_dp->is_mst_start) {
		DP_ERR("unexpected code flow\n");
		return;
	}

	/* allocate vcpi */
	mtk_dp_mst_drv_allocate_stream(mtk_dp, mgr, true);

	/* update payload, after update vcpi */
	mtk_dp_mst_drv_update_payload(mtk_dp, mgr);
}

static void mtk_dp_mst_drv_fec_handler(struct mtk_dp *mtk_dp)
{
#if DPTX_MST_FEC_ENABLE
	struct mtk_drm_dp_mst_topology_mgr *mgr = &mtk_dp->mtk_mgr;
	struct mtk_drm_dp_mst_branch *mstb = mgr->mst_primary;
	u16 dpcd_fec_support = 0x0;

	mtk_dp_mst_drv_send_remote_dpcd_read_fec_en(mgr,
						    mstb, &dpcd_fec_support);

	mtk_dp->is_mst_fec_en = dpcd_fec_support && mtk_dp->has_fec;
	DP_MSG("Support FEC %d\n", mtk_dp->is_mst_fec_en);

	// should call mhal_DPTx_FECInitialSetting() before this function
	mtk_dp_mst_drv_fec_enable(mtk_dp, mtk_dp->is_mst_fec_en); //DPCD_00090 bit0
#endif
}

const struct drm_dp_mst_topology_cbs mtk_mst_topology_cbs = {
	.add_connector = mtk_dp_add_connector,
};

/**
 * mtk_dp_mst_drv_init() - initialize DRM mgr structure, and DPTx HW & SW
 */
void mtk_dp_mst_drv_init(struct mtk_dp *mtk_dp)
{
	/* executed when we know the primary Branch supports MST */
	int ret = 0;
	int max_payloads = DP_STREAM_MAX;
	int conn_base_id = 0;

	/* dptx global variable init */
	mtk_dp_mst_drv_init_variable(mtk_dp);

	mtk_dp->mgr.cbs = &mtk_mst_topology_cbs;

	/* init topology mgr */
#if DP_DRM_COMMON //to check
	 ret = drm_dp_mst_topology_mgr_init(&mtk_dp->mgr, mtk_dp->drm_dev, &mtk_dp->aux,
					    DPTX_DPCD_TRANS_BYTES_MAX, max_payloads, conn_base_id);
#else
	ret = mtk_drm_dp_mst_topology_mgr_init(mtk_dp, &mtk_dp->mtk_mgr,
					       DPTX_DPCD_TRANS_BYTES_MAX, max_payloads,
					       DPTX_SUPPORT_MAX_LANECOUNT,
					       DPTX_SUPPORT_MAX_LINKRATE * 27000,
				conn_base_id);
#endif

	if (ret < 0)
		DP_ERR("Topology mgr init fail\n");

	mtk_dp_mst_hal_tx_init(mtk_dp);
}

/**
 * mtk_dp_mst_drv_start() - start to create MST topology
 *
 * Search all device in the MST topology and power-up phy,
 * then remoted read EDIDs and start to output MST video.
 */
void mtk_dp_mst_drv_start(struct mtk_dp *mtk_dp, struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	/* executed when the link training with the primary Branch has been done */
	u8 dpcd[DP_RECEIVER_CAP_SIZE];

	/* check capability */
	dpcd[0] = mtk_dp->training_info.dp_version;
	if (drm_dp_read_mst_cap(&mtk_dp->aux, dpcd) == false) {
		DP_MSG("connected device does not support MST\n");
		return;
	}

	mtk_dp->is_mst_start = true;
	/*set mst and discover the topology*/
	mtk_drm_dp_mst_topology_mgr_set_mst(mgr, true);

	mtk_dp_mst_hal_tx_enable(mtk_dp, true);
	mtk_dp_mst_hal_mst_config(mtk_dp);
}

/**
 * mtk_dp_mst_drv_stop() - source is going to stop mst
 */
void mtk_dp_mst_drv_stop(struct mtk_dp *mtk_dp, struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	mtk_dp->is_mst_start = false;
	DP_MSG("MST stop\n");

	mtk_dp_mst_drv_fec_enable(mtk_dp, false);

	/* de-allocate vcpi */
	mtk_dp_mst_drv_clear_vcpi(mtk_dp, mgr);
	mtk_dp_mst_drv_stream_enable(mtk_dp, mgr->vcpi_mask, mgr->max_payloads);

	/* disable MST output (transmitter/MST TX) */
	mtk_dp_mst_hal_tx_enable(mtk_dp, false);

	/* power down PHY, power down all internal ports */
	//mtk_dp_mst_drv_power_updown_phy(mgr, mgr->mst_primary, false);
#if DP_DRM_COMMON
//to check
#else
	if (mgr)  {
		if (mgr->mst_primary)
			mtk_drm_dp_delayed_destroy_mstb(mgr->mst_primary);
	}
#endif
	/* destroy topology mgr */
	mtk_drm_dp_mst_topology_mgr_destroy(mgr);

	mtk_dp_mst_hal_tx_init(mtk_dp);

	/* DPTx global variable reset */
	mtk_dp_mst_drv_init_variable(mtk_dp);
}

#if ENABLE_SERDES_MST
void mtk_dp_mst_init(struct mtk_dp *mtk_dp)
{
	/* dptx global variable init */
	mtk_dp_mst_drv_init_variable(mtk_dp);

	mtk_dp_mst_hal_tx_init(mtk_dp);
}

void mtk_dp_mst_start(struct mtk_dp *mtk_dp)
{
	mtk_dp->is_mst_start = true;

	mtk_dp_mst_hal_tx_enable(mtk_dp, true);
	mtk_dp_mst_hal_mst_config(mtk_dp);
}

void mtk_dp_mst_stop(struct mtk_dp *mtk_dp)
{
	u8 temp_value[0x3] = {0x0, 0x0, 0x3F};
	u8 ret = 0;

	mtk_dp->is_mst_start = false;
	DP_MSG("MST stop\n");

	mtk_dp_mst_drv_fec_enable(mtk_dp, false);

	/* de-allocate vcpi */
	/* clear DPCD_001C0 ~ DPCD_001C2 */
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DPCD_001C0, temp_value, 0x3);
	DP_MSG("Clear DPCD_001C0 ~ DPCD_001C2, result %d\n", ret);

	mtk_dp_mst_drv_stream_enable(mtk_dp, 0x3, DP_STREAM_MAX);

	/* disable MST output (transmitter/MST TX) */
	mtk_dp_mst_hal_tx_enable(mtk_dp, false);

	mtk_dp_mst_hal_tx_init(mtk_dp);

	/* DPTx global variable reset */
	mtk_dp_mst_drv_init_variable(mtk_dp);
}
#endif

void mtk_dp_mst_drv_video_mute_all(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_video_mute(mtk_dp, encoder_id, true);
}

void mtk_dp_mst_drv_audio_mute_all(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_audio_mute(mtk_dp, encoder_id, true);
}

/**
 * mtk_dp_mst_drv_reset() - reset MST stream for devices in the topology
 * @is_plug: result in the current plug status
 */
void mtk_dp_mst_drv_reset(struct mtk_dp *mtk_dp,
			  struct mtk_drm_dp_mst_topology_mgr *mgr, u8 is_plug)
{
	u8 dpcd[DP_RECEIVER_CAP_SIZE];

	DP_MSG("Reset MST, is_plug %d\r\n", is_plug);
	mtk_dp_mst_drv_video_mute_all(mtk_dp);
	mtk_dp_mst_drv_audio_mute_all(mtk_dp);

	dpcd[0] = mtk_dp->training_info.dp_version;
	if (drm_dp_read_mst_cap(&mtk_dp->aux, dpcd) == false) {
		DP_MSG("connected device not support MST\n");
		mtk_dp->mst_enable = false;
	}

	//if (is_plug) {
#if ENABLE_SERDES_MST
		mtk_dp_mst_stop(mtk_dp);
#else
		mtk_dp_mst_drv_stop(mtk_dp, &mtk_dp->mtk_mgr);
#endif
		mtk_dp_mst_drv_video_mute_all(mtk_dp);
		mtk_dp_mst_drv_audio_mute_all(mtk_dp);
		mtk_dp->state = DP_STATE_INITIAL;
		mtk_dp->mst_enable = 1;
	//} else {
		//mtk_dp_mst_drv_update_payload(mgr);
		//mtk_dp_mst_drv_stream_enable(mgr->vcpi_mask, mgr->max_payloads);
	//}
}

void mtk_dp_mst_drv_encoder_reset_all(struct mtk_dp *mtk_dp)
{
	enum dp_encoder_id encoder_id;

	for (encoder_id = 0; encoder_id < DP_ENCODER_ID_MAX; encoder_id++)
		mtk_dp_encoder_reset(mtk_dp, encoder_id);
}

/**
 * mtk_dp_mst_drv_handler() - handle MST finite state machine
 *
 * It's a state machine with 6 states.
 * 1st state "DP_STATE_INITIAL": wait and check primary branch support MST
 * 2nd state "DP_STATE_STARTUP": wait for source training done
 * 3rd state "DP_STATE_AUTH": handle hdcp auth. flow if support hdcp
 * 4th state "DP_STATE_PREPARE": prepare before output valid timing
 * 5th state "DP_STATE_NORMAL": already output MST video and here if work normally
 * 6th state "DP_STATE_IDLE": wait here until link status change
 */
u8 mtk_dp_mst_drv_handler(struct mtk_dp *mtk_dp)
{
	u8 ret = DP_RET_NOERR;

	if (!mtk_dp->dp_ready)
		return DP_RET_NOERR;

	if (!mtk_dp->training_info.cable_plug_in)
		return DP_RET_PLUG_OUT;

	if (!mtk_dp->mst_enable)
		return DP_RET_NOERR;

	/* update MST state machine and print log */
	//if (mtk_dp->state != mtk_dp->state_pre)
		//mtk_dp_mst_drv_print_state();

	mtk_dp->state_pre = mtk_dp->state;

	if (mtk_dp->state > DP_STATE_IDLE &&
	    mtk_dp->training_state != DP_TRAINING_STATE_NORMAL) {
		mtk_dp->mst_enable = true;
		DP_ERR("lose lock!!!traininig state %x\n\n", mtk_dp->training_state);
#if ENABLE_SERDES_MST
		mtk_dp_mst_stop(mtk_dp);
#else
		mtk_dp_mst_drv_stop(mtk_dp, &mtk_dp->mtk_mgr);
#endif
		mtk_dp_mst_drv_video_mute_all(mtk_dp);
		mtk_dp_mst_drv_audio_mute_all(mtk_dp);
		mtk_dp->state = DP_STATE_INITIAL;
	}

	switch (mtk_dp->state) {
	case DP_STATE_INITIAL:  // wait primary branch support Mst
		if (mtk_dp->mst_enable) {
			DP_MSG("===MST Enable===\n\n");
			mtk_dp_mst_drv_video_mute_all(mtk_dp);
			mtk_dp_mst_drv_audio_mute_all(mtk_dp);
#if DPTX_MST_HDCP_ENABLE
			mtk_dptx_hdcp13_enable_encrypt(false);
			mtk_dptx_hdcp23_enable_encrypt(false);
#endif
#if ENABLE_SERDES_MST
			mtk_dp_mst_init(mtk_dp);
#else
			mtk_dp_mst_drv_init(mtk_dp);
#endif
			mtk_dp->state = DP_STATE_IDLE;
		}
		break;

	case DP_STATE_IDLE: //wait DP Tx training done
		if (mtk_dp->training_state == DP_TRAINING_STATE_NORMAL) {
			DP_MSG("===MST Start===\n\n");
#if ENABLE_SERDES_MST
			mtk_dp_mst_start(mtk_dp);
#else
			mtk_dp_mst_drv_start(mtk_dp, &mtk_dp->mtk_mgr);
#endif
			mtk_dp->state = DP_STATE_PREPARE;
		}
		break;

	case DP_STATE_PREPARE:
		DP_DBG("===MST Prepare===\n\n");
		if (mtk_dp->video_enable) {
			mtk_dp_mst_drv_fec_handler(mtk_dp);
#if ENABLE_SERDES_MST
			mtk_dp_mst_payload_handler(mtk_dp);
#else
			mtk_dp_mst_drv_payload_handler(mtk_dp, &mtk_dp->mtk_mgr);
#endif
			mtk_dp_mst_drv_encoder_reset_all(mtk_dp);

#if DPTX_MST_HDCP_ENABLE
			mtk_dp->state = DP_STATE_AUTH;

			if (mtk_dp->hdcp_handle.enable) {
				DP_MSG("Start Auth HDCP22!\n");
				mtk_dptx_hdcp23_set_start_auth(true);
			} else if (mtk_dp->hdcp13_info.enable) {
				DP_MSG("[DPTX] HDCP13 auth start!\n");
				mtk_dp->hdcp13_info.main_state = HDCP13_MAIN_STATE_A0;
				mtk_dp->hdcp13_info.sub_state = HDCP13_SUB_STATE_IDLE;
				mtk_dp->hdcp13_info.retry_count = 0;
			}
#else
			mtk_dp->state = DP_STATE_NORMAL;
#endif
		}
		break;

#if DPTX_MST_HDCP_ENABLE
	case DP_STATE_AUTH:

		if (mtk_dp->hdcp13_info.enable) {
			if (mtk_dp->auth_status == AUTH_PASS) {
				DP_MSG("HDCP 13 auth passed!\n");
				mtk_dp->state = DP_STATE_NORMAL;
			} else if (mtk_dp->auth_status == AUTH_FAIL) {
				mtk_dp->state = DP_STATE_IDLE;
				DP_ERR("HDCP 13 auth failed!\n");
				mtk_dptx_hdcp13_enable_encrypt(false);
			}
			mtk_dptx_hdcp13_fsm();
		} else if (mtk_dp->hdcp_handle.enable) {
			if (mtk_dp->auth_status == AUTH_PASS) {
				DP_MSG("HDCP 23 auth passed!\n");
				mtk_dp->state = DP_STATE_NORMAL;
			} else if (mtk_dp->auth_status == AUTH_FAIL) {
				DP_ERR("HDCP 23 auth failed!\n");
				mtk_dp->state = DP_STATE_IDLE;
				mtk_dptx_hdcp23_enable_encrypt(false);
			}
			mtk_dptx_hdcp23_fsm();
		} else {
			DP_MSG("Sink Not support HDCP, skip auth!\n");
			mtk_dp->state = DP_STATE_NORMAL;
		}

		break;
#endif

	case DP_STATE_NORMAL:
		DP_DBG("===MST Normal===\n\n");
		#if DPTX_MST_HDCP_ENABLE
		if (mtk_dp->hdcp13_info.enable)
			mtk_dptx_hdcp13_fsm();
		else if (mtk_dp->hdcp_handle.enable)
			mtk_dptx_hdcp23_fsm();
		#endif

		if (mtk_dp->training_state != DP_TRAINING_STATE_NORMAL) {
			DP_MSG("[MST] DPTX Link Status Change!%d\r\n", mtk_dp->training_state);
#if ENABLE_SERDES_MST
			mtk_dp_mst_stop(mtk_dp);
#else
			mtk_dp_mst_drv_stop(mtk_dp, &mtk_dp->mtk_mgr);
#endif
			mtk_dp_mst_drv_video_mute_all(mtk_dp);
			mtk_dp_mst_drv_audio_mute_all(mtk_dp);
			mtk_dp->state = DP_STATE_INITIAL;
			mtk_dp->mst_enable = true;
		#if DPTX_MST_HDCP_ENABLE
			mtk_dptx_hdcp13_enable_encrypt(false);
			mtk_dptx_hdcp23_enable_encrypt(false);
		#endif
		}
		break;
	default:
		DP_ERR("[MST] Current State invalid, please check !!!\n");
		break;
	}

	return ret;
}

void mtk_dp_mst_drv_sideband_msg_irq_clear(struct mtk_dp *mtk_dp)
{
	mtk_dp->training_info.phy_status &= (~DPTX_PHY_HPD_INT_EVNET);
}

void mtk_dp_mst_drv_sideband_msg_rdy_clear(struct mtk_dp *mtk_dp)
{
	u8 dpcd_buf[2], temp[1];

	/*Clear RDY bit after got the sideband MSG*/
	if (mtk_dp->training_info.sink_ext_cap_en) {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, dpcd_buf, 0x2);
		if (dpcd_buf[0x1] & 0x30) {
			temp[0] = (dpcd_buf[0x1] & 0x30);
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003, temp, 0x1);
		}
	} else {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, dpcd_buf, 0x2);
		if (dpcd_buf[0x1] & 0x30) {
			temp[0] = (dpcd_buf[0x1] & 0x30);
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, temp, 0x1);
		}
	}
}
#endif
