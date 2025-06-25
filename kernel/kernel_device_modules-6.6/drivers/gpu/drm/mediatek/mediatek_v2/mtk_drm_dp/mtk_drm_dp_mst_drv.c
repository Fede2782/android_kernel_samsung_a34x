// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "mtk_drm_dp_mst_drv.h"

#include "mtk_drm_dp.h"
#include "mtk_drm_dp_mst.h"
#include "mtk_drm_dp_reg.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
#define DPTX_DRM_UNUSED_FUNC 0

#define DPTX_MST_DEBUG 1
#define DPTX_MST_I2C_READ_ENABLE 1
#define DPTX_MST_POWER_UP_PHY_ENABLE 1
#define DPTX_PRINT_LEVEL 1

#define save_mstb_topology_ref(mstb, type)
#define save_port_topology_ref(port, type)

#define DBG_PREFIX "[dp_mst]"

#define DP_STR(x) \
[DP_##x] = #x

struct mtk_dp *mst_mtk_dp;
struct mtk_drm_dp mtk_drm_dp_real;
struct mtk_drm_dp *mtk_drm_dp = &mtk_drm_dp_real;

static const char *mtk_drm_dp_mst_req_type_str(u8 req_type)
{
	static const char * const req_type_str[] = {
		DP_STR(GET_MSG_TRANSACTION_VERSION),
		DP_STR(LINK_ADDRESS),
		DP_STR(CONNECTION_STATUS_NOTIFY),
		DP_STR(ENUM_PATH_RESOURCES),
		DP_STR(ALLOCATE_PAYLOAD),
		DP_STR(QUERY_PAYLOAD),
		DP_STR(RESOURCE_STATUS_NOTIFY),
		DP_STR(CLEAR_PAYLOAD_ID_TABLE),
		DP_STR(REMOTE_DPCD_READ),
		DP_STR(REMOTE_DPCD_WRITE),
		DP_STR(REMOTE_I2C_READ),
		DP_STR(REMOTE_I2C_WRITE),
		DP_STR(POWER_UP_PHY),
		DP_STR(POWER_DOWN_PHY),
		DP_STR(SINK_EVENT_NOTIFY),
		DP_STR(QUERY_STREAM_ENC_STATUS),
	};

	if (req_type >= ARRAY_SIZE(req_type_str) ||
	    !req_type_str[req_type])
		return "unknown";

	return req_type_str[req_type];
}

#undef DP_STR
#define DP_STR(x)  \
[DP_NAK_##x] = #x

static const char *mtk_drm_dp_mst_nak_reason_str(u8 nak_reason)
{
	static const char * const nak_reason_str[] = {
		DP_STR(WRITE_FAILURE),
		DP_STR(INVALID_READ),
		DP_STR(CRC_FAILURE),
		DP_STR(BAD_PARAM),
		DP_STR(DEFER),
		DP_STR(LINK_FAILURE),
		DP_STR(NO_RESOURCES),
		DP_STR(DPCD_FAIL),
		DP_STR(I2C_NAK),
		DP_STR(ALLOCATE_FAIL),
	};

	if (nak_reason >= ARRAY_SIZE(nak_reason_str) ||
	    !nak_reason_str[nak_reason])
		return "unknown";

	return nak_reason_str[nak_reason];
}

#undef DP_STR
#define DP_STR(x) \
[DRM_DP_SIDEBAND_TX_##x] = #x

static const char *mtk_drm_dp_mst_sideband_tx_state_str(int state)
{
	static const char * const sideband_reason_str[] = {
		DP_STR(QUEUED),
		DP_STR(START_SEND),
		DP_STR(SENT),
		DP_STR(RX),
		DP_STR(TIMEOUT),
	};

	if (state >= 5 ||
	    !sideband_reason_str[state])
		return "unknown";

	return sideband_reason_str[state];
}

static int
mtk_drm_dp_mst_rad_to_str(const u8 rad[8], u8 lct, char *out, size_t len)
{
	int i;
	u8 unpacked_rad[16];

	for (i = 0; i < lct; i++) {
		if (i % 2)
			unpacked_rad[i] = rad[i / 2] >> 4;
		else
			unpacked_rad[i] = rad[i / 2] & GENMASK(4, 0);
	}

	return snprintf(out, len, "%*phC", lct, unpacked_rad);
}

/* sideband msg handling */
static u8 mtk_drm_dp_msg_header_crc4(const u8 *data, size_t num_nibbles)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = num_nibbles * 4;
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

static u8 mtk_drm_dp_msg_data_crc4(const u8 *data, u8 number_of_bytes)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = number_of_bytes * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xd5;
	}

	number_of_bits = 8;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x100) != 0)
			remainder ^= 0xd5;
	}

	return remainder & 0xff;
}

static inline u8 mtk_drm_dp_calc_sb_hdr_size(struct mtk_drm_dp_sideband_msg_hdr *hdr)
{
	u8 size = 3;

	size += (hdr->lct / 2);
	return size;
}

static void mtk_drm_dp_encode_sideband_msg_hdr(struct mtk_drm_dp_sideband_msg_hdr *hdr,
					       u8 *buf, int *len)
{
	int idx = 0;
	int i;
	u8 crc4;

	buf[idx++] = ((hdr->lct & 0xf) << 4) | (hdr->lcr & 0xf);
	for (i = 0; i < (hdr->lct / 2); i++)
		buf[idx++] = hdr->rad[i];
	buf[idx++] = (hdr->broadcast << 7) | (hdr->path_msg << 6) |
		(hdr->msg_len & 0x3f);
	buf[idx++] = (hdr->somt << 7) | (hdr->eomt << 6) | (hdr->seqno << 4);

	crc4 = mtk_drm_dp_msg_header_crc4(buf, (idx * 2) - 1);
	buf[idx - 1] |= (crc4 & 0xf);

	*len = idx;
}

static bool mtk_drm_dp_decode_sideband_msg_hdr(const struct mtk_drm_dp_mst_topology_mgr *mgr,
					       struct mtk_drm_dp_sideband_msg_hdr *hdr,
					   u8 *buf, int buflen, u8 *hdrlen)
{
	u8 crc4;
	u8 len;
	int i;
	u8 idx;

	if (buf[0] == 0)
		return false;
	len = 3;
	len += ((buf[0] & 0xf0) >> 4) / 2;
	if (len > buflen)
		return false;
	crc4 = mtk_drm_dp_msg_header_crc4(buf, (len * 2) - 1);

	if ((crc4 & 0xf) != (buf[len - 1] & 0xf)) {
		DP_MSG("crc4 mismatch 0x%x 0x%x\n", crc4, buf[len - 1]);
		return false;
	}

	hdr->lct = (buf[0] & 0xf0) >> 4;
	hdr->lcr = (buf[0] & 0xf);
	idx = 1;
	for (i = 0; i < (hdr->lct / 2); i++)
		hdr->rad[i] = buf[idx++];
	hdr->broadcast = (buf[idx] >> 7) & 0x1;
	hdr->path_msg = (buf[idx] >> 6) & 0x1;
	hdr->msg_len = buf[idx] & 0x3f;
	idx++;
	hdr->somt = (buf[idx] >> 7) & 0x1;
	hdr->eomt = (buf[idx] >> 6) & 0x1;
	hdr->seqno = (buf[idx] >> 4) & 0x1;
	idx++;
	*hdrlen = idx;
	return true;
}

void mtk_drm_dp_encode_sideband_req(const struct mtk_drm_dp_sideband_msg_req_body *req,
				    struct mtk_drm_dp_sideband_msg_tx *raw)
{
	int idx = 0;
	int i;
	u8 *buf = raw->msg;

	buf[idx++] = req->req_type & 0x7f;
	mtk_drm_dp->current_request_type = req->req_type;

	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		buf[idx] = (req->u.port_num.port_number & 0xf) << 4;
		idx++;
		break;
	case DP_ALLOCATE_PAYLOAD:
		buf[idx] = (req->u.allocate_payload.port_number & 0xf) << 4 |
			(req->u.allocate_payload.number_sdp_streams & 0xf);
		idx++;
		buf[idx] = (req->u.allocate_payload.vcpi & 0x7f);
		idx++;
		buf[idx] = (req->u.allocate_payload.pbn >> 8);
		idx++;
		buf[idx] = (req->u.allocate_payload.pbn & 0xff);
		idx++;
		for (i = 0; i < req->u.allocate_payload.number_sdp_streams / 2; i++) {
			buf[idx] = ((req->u.allocate_payload.sdp_stream_sink[i * 2] & 0xf) << 4) |
				(req->u.allocate_payload.sdp_stream_sink[i * 2 + 1] & 0xf);
			idx++;
		}
		if (req->u.allocate_payload.number_sdp_streams & 1) {
			i = req->u.allocate_payload.number_sdp_streams - 1;
			buf[idx] = (req->u.allocate_payload.sdp_stream_sink[i] & 0xf) << 4;
			idx++;
		}
		break;
	case DP_QUERY_PAYLOAD:
		buf[idx] = (req->u.query_payload.port_number & 0xf) << 4;
		idx++;
		buf[idx] = (req->u.query_payload.vcpi & 0x7f);
		idx++;
		break;
	case DP_REMOTE_DPCD_READ:
		buf[idx] = (req->u.dpcd_read.port_number & 0xf) << 4;
		buf[idx] |= ((req->u.dpcd_read.dpcd_address & 0xf0000) >> 16) & 0xf;
		idx++;
		buf[idx] = (req->u.dpcd_read.dpcd_address & 0xff00) >> 8;
		idx++;
		buf[idx] = (req->u.dpcd_read.dpcd_address & 0xff);
		idx++;
		buf[idx] = (req->u.dpcd_read.num_bytes);
		idx++;
		break;

	case DP_REMOTE_DPCD_WRITE:
		buf[idx] = (req->u.dpcd_write.port_number & 0xf) << 4;
		buf[idx] |= ((req->u.dpcd_write.dpcd_address & 0xf0000) >> 16) & 0xf;
		idx++;
		buf[idx] = (req->u.dpcd_write.dpcd_address & 0xff00) >> 8;
		idx++;
		buf[idx] = (req->u.dpcd_write.dpcd_address & 0xff);
		idx++;
		buf[idx] = (req->u.dpcd_write.num_bytes);
		idx++;
		memcpy(&buf[idx], req->u.dpcd_write.bytes, req->u.dpcd_write.num_bytes);
		idx += req->u.dpcd_write.num_bytes;
		break;
	case DP_REMOTE_I2C_READ:
		buf[idx] = (req->u.i2c_read.port_number & 0xf) << 4;
		buf[idx] |= (req->u.i2c_read.num_transactions & 0x3);
		idx++;
		for (i = 0; i < (req->u.i2c_read.num_transactions & 0x3); i++) {
			buf[idx] = req->u.i2c_read.transactions[i].i2c_dev_id & 0x7f;
			idx++;
			buf[idx] = req->u.i2c_read.transactions[i].num_bytes;
			idx++;
			memcpy(&buf[idx], req->u.i2c_read.transactions[i].bytes,
			       req->u.i2c_read.transactions[i].num_bytes);
			idx += req->u.i2c_read.transactions[i].num_bytes;

			buf[idx] = (req->u.i2c_read.transactions[i].no_stop_bit & 0x1) << 4;
			buf[idx] |= (req->u.i2c_read.transactions[i].i2c_transaction_delay & 0xf);
			idx++;
		}
		buf[idx] = (req->u.i2c_read.read_i2c_device_id) & 0x7f;
		idx++;
		buf[idx] = (req->u.i2c_read.num_bytes_read);
		idx++;
		break;

	case DP_REMOTE_I2C_WRITE:
		buf[idx] = (req->u.i2c_write.port_number & 0xf) << 4;
		idx++;
		buf[idx] = (req->u.i2c_write.write_i2c_device_id) & 0x7f;
		idx++;
		buf[idx] = (req->u.i2c_write.num_bytes);
		idx++;
		memcpy(&buf[idx], req->u.i2c_write.bytes, req->u.i2c_write.num_bytes);
		idx += req->u.i2c_write.num_bytes;
		break;
	case DP_QUERY_STREAM_ENC_STATUS: {
		const struct mtk_drm_dp_query_stream_enc_status *msg;

		msg = &req->u.enc_status;
		buf[idx] = msg->stream_id;
		idx++;
		memcpy(&buf[idx], msg->client_id, sizeof(msg->client_id));
		idx += sizeof(msg->client_id);
		buf[idx] = 0;
		buf[idx] |= (GENMASK(1, 0) & msg->stream_event);
		buf[idx] |= msg->valid_stream_event ? BIT(2) : 0;
		buf[idx] |= (GENMASK(4, 3) & msg->stream_behavior);
		buf[idx] |= msg->valid_stream_behavior ? BIT(5) : 0;
		idx++;
		}
		break;
	}
	raw->cur_len = idx;
}

/* Decode a sideband request we've encoded, mainly used for debugging */
int mtk_drm_dp_decode_sideband_req(const struct mtk_drm_dp_sideband_msg_tx *raw,
				   struct mtk_drm_dp_sideband_msg_req_body *req)
{
	const u8 *buf = raw->msg;
	u8 i, idx = 0;

	req->req_type = buf[idx++] & 0x7f;
	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		req->u.port_num.port_number = (buf[idx] >> 4) & 0xf;
		break;
	case DP_ALLOCATE_PAYLOAD:
		{
			struct mtk_drm_dp_allocate_payload *a =
				&req->u.allocate_payload;

			a->number_sdp_streams = buf[idx] & 0xf;
			a->port_number = (buf[idx] >> 4) & 0xf;

			if (buf[++idx] & 0x80)
				DP_ERR("invalid req !\n");
			a->vcpi = buf[idx] & 0x7f;

			a->pbn = buf[++idx] << 8;
			a->pbn |= buf[++idx];

			idx++;
			for (i = 0; i < a->number_sdp_streams; i++) {
				a->sdp_stream_sink[i] =
					(buf[idx + (i / 2)] >> ((i % 2) ? 0 : 4)) & 0xf;
			}
		}
		break;
	case DP_QUERY_PAYLOAD:
		req->u.query_payload.port_number = (buf[idx] >> 4) & 0xf;
		if (buf[++idx] & 0x80)
			DP_ERR("invalid req !\n");
		req->u.query_payload.vcpi = buf[idx] & 0x7f;
		break;
	case DP_REMOTE_DPCD_READ:
		{
			struct mtk_drm_dp_remote_dpcd_read *r = &req->u.dpcd_read;

			r->port_number = (buf[idx] >> 4) & 0xf;

			r->dpcd_address = (buf[idx] << 16) & 0xf0000;
			r->dpcd_address |= (buf[++idx] << 8) & 0xff00;
			r->dpcd_address |= buf[++idx] & 0xff;

			r->num_bytes = buf[++idx];
		}
		break;
	case DP_REMOTE_DPCD_WRITE:
		{
			struct mtk_drm_dp_remote_dpcd_write *w =
				&req->u.dpcd_write;

			w->port_number = (buf[idx] >> 4) & 0xf;

			w->dpcd_address = (buf[idx] << 16) & 0xf0000;
			w->dpcd_address |= (buf[++idx] << 8) & 0xff00;
			w->dpcd_address |= buf[++idx] & 0xff;

			w->num_bytes = buf[++idx];

			w->bytes = kmalloc(w->num_bytes, GFP_KERNEL);
			DP_MSG("kmalloc for DP_REMOTE_DPCD_WRITE w->bytes at %s with %d\n",
			       w->bytes, w->num_bytes);
			memcpy(w->bytes, &buf[++idx], w->num_bytes);

			if (!w->bytes)
				return -DPTX_STATUS_ERR;
		}
		break;
	case DP_REMOTE_I2C_READ:
		{
			struct mtk_drm_dp_remote_i2c_read *r = &req->u.i2c_read;
			struct mtk_drm_dp_remote_i2c_read_tx *tx;

			r->num_transactions = buf[idx] & 0x3;
			r->port_number = (buf[idx] >> 4) & 0xf;
			for (i = 0; i < r->num_transactions; i++) {
				tx = &r->transactions[i];

				tx->i2c_dev_id = buf[++idx] & 0x7f;
				tx->num_bytes = buf[++idx];
				idx += tx->num_bytes;
				tx->no_stop_bit = (buf[idx] >> 5) & 0x1;
				tx->i2c_transaction_delay = buf[idx] & 0xf;
			}
			r->read_i2c_device_id = buf[++idx] & 0x7f;
			r->num_bytes_read = buf[++idx];
		}
		break;
	case DP_REMOTE_I2C_WRITE:
		{
			struct mtk_drm_dp_remote_i2c_write *w = &req->u.i2c_write;

			w->port_number = (buf[idx] >> 4) & 0xf;
			w->write_i2c_device_id = buf[++idx] & 0x7f;
			w->num_bytes = buf[++idx];
			w->bytes = kmalloc(w->num_bytes, GFP_KERNEL);
			DP_MSG("kmalloc for DP_REMOTE_I2C_WRITE ->bytes at %s with %d\n",
			       w->bytes, w->num_bytes);
			memcpy(w->bytes, &buf[++idx], w->num_bytes);

			if (!w->bytes)
				return -DPTX_STATUS_ERR;
		}
		break;
	case DP_QUERY_STREAM_ENC_STATUS:
		req->u.enc_status.stream_id = buf[idx++];
		for (i = 0; i < sizeof(req->u.enc_status.client_id); i++)
			req->u.enc_status.client_id[i] = buf[idx++];

		req->u.enc_status.stream_event = (buf[idx] & GENMASK(7, 6)) >> 6;
		req->u.enc_status.valid_stream_event = (buf[idx] & BIT(5)) >> 5;
		req->u.enc_status.stream_behavior = (buf[idx] & GENMASK(4, 3)) >> 3;
		req->u.enc_status.valid_stream_behavior = (buf[idx] & BIT(2)) >> 2;
		break;
	}

	return 0;
}

void mtk_drm_dp_dump_sideband_msg_req_body(const struct mtk_drm_dp_sideband_msg_req_body *req,
					   int indent)
{
	int i;

	if (req->req_type == DP_LINK_ADDRESS || req->req_type == DP_CLEAR_PAYLOAD_ID_TABLE) {
		/* No contents to print */
		DP_MSG("type=%s\n", mtk_drm_dp_mst_req_type_str(req->req_type));
		return;
	}

	DP_MSG("type=%s contents:\n", mtk_drm_dp_mst_req_type_str(req->req_type));
	indent++;

	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		DP_MSG("port=%d\n", req->u.port_num.port_number);
		break;
	case DP_ALLOCATE_PAYLOAD:
		DP_MSG("port=%d vcpi=%d pbn=%d sdp_streams=%d %*ph\n",
		       req->u.allocate_payload.port_number,
		  req->u.allocate_payload.vcpi, req->u.allocate_payload.pbn,
		  req->u.allocate_payload.number_sdp_streams,
		  req->u.allocate_payload.number_sdp_streams,
		  req->u.allocate_payload.sdp_stream_sink);
		break;
	case DP_QUERY_PAYLOAD:
		DP_MSG("port=%d vcpi=%d\n",
		       req->u.query_payload.port_number,
		  req->u.query_payload.vcpi);
		break;
	case DP_REMOTE_DPCD_READ:
		DP_MSG("port=%d dpcd_addr=%05x len=%d\n",
		       req->u.dpcd_read.port_number, req->u.dpcd_read.dpcd_address,
		  req->u.dpcd_read.num_bytes);
		break;
	case DP_REMOTE_DPCD_WRITE:
		DP_MSG("port=%d addr=%05x len=%d: %*ph\n",
		       req->u.dpcd_write.port_number,
		  req->u.dpcd_write.dpcd_address,
		  req->u.dpcd_write.num_bytes, req->u.dpcd_write.num_bytes,
		  req->u.dpcd_write.bytes);
		break;
	case DP_REMOTE_I2C_READ:
		DP_MSG("port=%d num_tx=%d id=%d size=%d:\n",
		       req->u.i2c_read.port_number,
		  req->u.i2c_read.num_transactions,
		  req->u.i2c_read.read_i2c_device_id,
		  req->u.i2c_read.num_bytes_read);

		indent++;
		for (i = 0; i < req->u.i2c_read.num_transactions; i++) {
		#if ENABLE_DPTX_DEBUG
			const struct mtk_drm_dp_remote_i2c_read_tx *rtx =
				&req->u.i2c_read.transactions[i];

			DP_DBG("%d: id=%03d size=%03d no_stop_bit=%d tx_delay=%03d: %*ph\n",
			       i, rtx->i2c_dev_id, rtx->num_bytes,
			  rtx->no_stop_bit, rtx->i2c_transaction_delay,
			  rtx->num_bytes, rtx->bytes);
		#endif
		}
		break;
	case DP_REMOTE_I2C_WRITE:
		DP_MSG("port=%d id=%d size=%d: %*ph\n",
		       req->u.i2c_write.port_number,
		  req->u.i2c_write.write_i2c_device_id,
		  req->u.i2c_write.num_bytes, req->u.i2c_write.num_bytes,
		  req->u.i2c_write.bytes);
		break;
	case DP_QUERY_STREAM_ENC_STATUS:
		DP_MSG("stream_id=%u client_id=%*ph stream_event=%x\n",
		       req->u.enc_status.stream_id,
		(int)ARRAY_SIZE(req->u.enc_status.client_id),
		req->u.enc_status.client_id, req->u.enc_status.stream_event);
		DP_MSG("valid_event=%d stream_behavior=%x valid_behavior=%d\n",
		       req->u.enc_status.valid_stream_event,
		req->u.enc_status.stream_behavior,
		req->u.enc_status.valid_stream_behavior);
		break;
	default:
		DP_MSG("???\n");
		break;
	}
}

static inline void
mtk_drm_dp_mst_dump_sideband_msg_tx(const struct mtk_drm_dp_sideband_msg_tx *txmsg)
{
	struct mtk_drm_dp_sideband_msg_req_body req;
	char buf[64];
	int ret;
	int i;

	mtk_drm_dp_mst_rad_to_str(txmsg->dst->rad, txmsg->dst->lct, buf,
				  sizeof(buf));
	DP_MSG("txmsg cur_offset=%x cur_len=%x seqno=%x state=%s path_msg=%d dst=%s\n",
	       txmsg->cur_offset, txmsg->cur_len, txmsg->seqno,
		   mtk_drm_dp_mst_sideband_tx_state_str(txmsg->state),
		   txmsg->path_msg, buf);

	ret = mtk_drm_dp_decode_sideband_req(txmsg, &req);
	if (ret) {
		DP_ERR("<failed to decode sideband req: %d>\n", ret);
		return;
	}
	mtk_drm_dp_dump_sideband_msg_req_body(&req, 1);

	switch (req.req_type) {
	case DP_REMOTE_DPCD_WRITE:
		kfree(req.u.dpcd_write.bytes);
		break;
	case DP_REMOTE_I2C_READ:
		for (i = 0; i < req.u.i2c_read.num_transactions; i++)
			kfree(req.u.i2c_read.transactions[i].bytes);
		break;
	case DP_REMOTE_I2C_WRITE:
		kfree(req.u.i2c_write.bytes);
		break;
	}
}

static void mtk_drm_dp_crc_sideband_chunk_req(u8 *msg, u8 len)
{
	u8 crc4;

	crc4 = mtk_drm_dp_msg_data_crc4(msg, len);
	msg[len] = crc4;
}

static void mtk_drm_dp_encode_sideband_reply(struct mtk_drm_dp_sideband_msg_reply_body *rep,
					     struct mtk_drm_dp_sideband_msg_tx *raw)
{
	int idx = 0;
	u8 *buf = raw->msg;

	buf[idx++] = (rep->reply_type & 0x1) << 7 | (rep->req_type & 0x7f);

	raw->cur_len = idx;
}

static int mtk_drm_dp_sideband_msg_set_header(struct mtk_drm_dp_sideband_msg_rx *msg,
					      struct mtk_drm_dp_sideband_msg_hdr *hdr,
					  u8 hdrlen)
{
	/*
	 * ignore out-of-order messages or messages that are part of a
	 * failed transaction
	 */
	if (!hdr->somt && !msg->have_somt)
		return false;

	/* get length contained in this portion */
	msg->curchunk_idx = 0;
	msg->curchunk_len = hdr->msg_len;
	msg->curchunk_hdrlen = hdrlen;

	/* we have already gotten an somt - don't bother parsing */
	if (hdr->somt && msg->have_somt)
		return false;

	if (hdr->somt) {
		memcpy(&msg->initial_hdr, hdr, sizeof(struct mtk_drm_dp_sideband_msg_hdr));
		msg->have_somt = true;
	}
	if (hdr->eomt)
		msg->have_eomt = true;

	return true;
}

/* this adds a chunk of msg to the builder to get the final msg */
static bool mtk_drm_dp_sideband_append_payload(struct mtk_drm_dp_sideband_msg_rx *msg,
					       u8 *replybuf, u8 replybuflen)
{
	u8 crc4, i;

	memcpy(&msg->chunk[msg->curchunk_idx], replybuf, replybuflen);
	msg->curchunk_idx += replybuflen;

	if (msg->curchunk_idx >= msg->curchunk_len) {
		/* do CRC */
		crc4 = mtk_drm_dp_msg_data_crc4(msg->chunk, msg->curchunk_len - 1);
		if (crc4 != msg->chunk[msg->curchunk_len - 1]) {
			DP_ERR("Error CRC value !\n");
			for (i = 0; i < msg->curchunk_len; i++)
				DP_ERR("%s\n", msg->chunk);
		}
		/* copy chunk into bigger msg */
		memcpy(&msg->msg[msg->curlen], msg->chunk, msg->curchunk_len - 1);
		msg->curlen += msg->curchunk_len - 1;
	}
	return true;
}

static bool mtk_drm_dp_sideband_parse_link_address(const struct mtk_drm_dp_mst_topology_mgr *mgr,
						   struct mtk_drm_dp_sideband_msg_rx *raw,
					       struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;
	int i;

	memcpy(repmsg->u.link_addr.guid, &raw->msg[idx], 16);
	idx += 16;
	repmsg->u.link_addr.nports = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	for (i = 0; i < repmsg->u.link_addr.nports; i++) {
		if (raw->msg[idx] & 0x80)
			repmsg->u.link_addr.ports[i].input_port = 1;

		repmsg->u.link_addr.ports[i].peer_device_type = (raw->msg[idx] >> 4) & 0x7;
		repmsg->u.link_addr.ports[i].port_number = (raw->msg[idx] & 0xf);

		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		repmsg->u.link_addr.ports[i].mcs = (raw->msg[idx] >> 7) & 0x1;
		repmsg->u.link_addr.ports[i].ddps = (raw->msg[idx] >> 6) & 0x1;
		if (repmsg->u.link_addr.ports[i].input_port == 0)
			repmsg->u.link_addr.ports[i].legacy_device_plug_status =
				(raw->msg[idx] >> 5) & 0x1;
		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		if (repmsg->u.link_addr.ports[i].input_port == 0) {
			repmsg->u.link_addr.ports[i].dpcd_revision = (raw->msg[idx]);
			idx++;
			if (idx > raw->curlen)
				goto fail_len;
			memcpy(repmsg->u.link_addr.ports[i].peer_guid, &raw->msg[idx], 16);
			idx += 16;
			if (idx > raw->curlen)
				goto fail_len;
			repmsg->u.link_addr.ports[i].num_sdp_streams = (raw->msg[idx] >> 4) & 0xf;
			repmsg->u.link_addr.ports[i].num_sdp_stream_sinks = (raw->msg[idx] & 0xf);
			idx++;
		}
		if (idx > raw->curlen)
			goto fail_len;
	}

	return true;
fail_len:
	DP_ERR("link address reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_remote_dpcd_read
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_dpcd_read_ack.port_number = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.remote_dpcd_read_ack.num_bytes = raw->msg[idx];
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(repmsg->u.remote_dpcd_read_ack.bytes, &raw->msg[idx],
	       repmsg->u.remote_dpcd_read_ack.num_bytes);
	return true;
fail_len:
	DP_ERR("link address reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_remote_dpcd_write
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_dpcd_write_ack.port_number = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DP_ERR("parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_remote_i2c_read_ack
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_i2c_read_ack.port_number = (raw->msg[idx] & 0xf);
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.remote_i2c_read_ack.num_bytes = raw->msg[idx];
	idx++;
	/* TODO check */
	memcpy(repmsg->u.remote_i2c_read_ack.bytes, &raw->msg[idx],
	       repmsg->u.remote_i2c_read_ack.num_bytes);
	return true;
fail_len:
	DP_ERR("remote i2c reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_enum_path_resources_ack
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.path_resources.port_number = (raw->msg[idx] >> 4) & 0xf;
	repmsg->u.path_resources.fec_capable = raw->msg[idx] & 0x1;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.path_resources.full_payload_bw_number =
		(raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.path_resources.avail_payload_bw_number =
		(raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DP_ERR("enum resource parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_allocate_payload_ack
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.allocate_payload.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.allocate_payload.vcpi = raw->msg[idx];
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.allocate_payload.allocated_pbn = (raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DP_ERR("allocate payload parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_query_payload_ack
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.query_payload.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.query_payload.allocated_pbn = (raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DP_ERR("query payload parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_power_updown_phy_ack
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.port_number.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen) {
		DP_ERR("power up/down phy parse length fail %d %d\n",
		       idx, raw->curlen);
		return false;
	}
	return true;
}

static bool
mtk_drm_dp_sideband_parse_query_stream_enc_status
	(struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_reply_body *repmsg)
{
	struct mtk_drm_dp_query_stream_enc_status_ack_reply *reply;

	reply = &repmsg->u.enc_status;
	reply->stream_id = raw->msg[3];
	reply->reply_signed = raw->msg[2] & BIT(0);
	reply->hdcp_1x_device_present = raw->msg[2] & BIT(4);
	reply->hdcp_2x_device_present = raw->msg[2] & BIT(3);
	reply->query_capable_device_present = raw->msg[2] & BIT(5);
	reply->legacy_device_present = raw->msg[2] & BIT(6);
	reply->unauthorizable_device_present = raw->msg[2] & BIT(7);
	reply->auth_completed = !!(raw->msg[1] & BIT(3));
	reply->encryption_enabled = !!(raw->msg[1] & BIT(4));
	reply->repeater_present = !!(raw->msg[1] & BIT(5));
	reply->state = (raw->msg[1] & GENMASK(7, 6)) >> 6;

	return true;
}

static bool mtk_drm_dp_sideband_parse_reply(const struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_sideband_msg_rx *raw,
					struct mtk_drm_dp_sideband_msg_reply_body *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->reply_type = (raw->msg[0] & 0x80) >> 7;
	msg->req_type = (raw->msg[0] & 0x7f);

	if (msg->reply_type == DP_SIDEBAND_REPLY_NAK) {
		memcpy(msg->u.nak.guid, &raw->msg[1], 16);
		msg->u.nak.reason = raw->msg[17];
		msg->u.nak.nak_data = raw->msg[18];
		return false;
	}
	if (msg->req_type != mtk_drm_dp->current_request_type) {
		DP_ERR("reply %x, req %x type\n", msg->req_type,
		       mtk_drm_dp->current_request_type);
		mtk_drm_dp->end_of_msg_trans_flag = false; //clear the flag
	}

	switch (msg->req_type) {
	case DP_LINK_ADDRESS:
		return mtk_drm_dp_sideband_parse_link_address(mgr, raw, msg);
	case DP_QUERY_PAYLOAD:
		return mtk_drm_dp_sideband_parse_query_payload_ack(raw, msg);
	case DP_REMOTE_DPCD_READ:
		return mtk_drm_dp_sideband_parse_remote_dpcd_read(raw, msg);
	case DP_REMOTE_DPCD_WRITE:
		return mtk_drm_dp_sideband_parse_remote_dpcd_write(raw, msg);
	case DP_REMOTE_I2C_READ:
		return mtk_drm_dp_sideband_parse_remote_i2c_read_ack(raw, msg);
	case DP_REMOTE_I2C_WRITE:
		return true; /* since there's nothing to parse */
	case DP_ENUM_PATH_RESOURCES:
		return mtk_drm_dp_sideband_parse_enum_path_resources_ack(raw, msg);
	case DP_ALLOCATE_PAYLOAD:
		return mtk_drm_dp_sideband_parse_allocate_payload_ack(raw, msg);
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		return mtk_drm_dp_sideband_parse_power_updown_phy_ack(raw, msg);
	case DP_CLEAR_PAYLOAD_ID_TABLE:
		return true; /* since there's nothing to parse */
	case DP_QUERY_STREAM_ENC_STATUS:
		return mtk_drm_dp_sideband_parse_query_stream_enc_status(raw, msg);
	default:
		DP_ERR("Got unknown reply 0x%02x (%s)\n",
		       msg->req_type, mtk_drm_dp_mst_req_type_str(msg->req_type));
		return false;
	}
}

static bool
mtk_drm_dp_sideband_parse_connection_status_notify(const struct mtk_drm_dp_mst_topology_mgr *mgr,
						   struct mtk_drm_dp_sideband_msg_rx *raw,
					       struct mtk_drm_dp_sideband_msg_req_body *msg)
{
	int idx = 1;

	msg->u.conn_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(msg->u.conn_stat.guid, &raw->msg[idx], 16);
	idx += 16;
	if (idx > raw->curlen)
		goto fail_len;

	msg->u.conn_stat.legacy_device_plug_status = (raw->msg[idx] >> 6) & 0x1;
	msg->u.conn_stat.displayport_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
	msg->u.conn_stat.message_capability_status = (raw->msg[idx] >> 4) & 0x1;
	msg->u.conn_stat.input_port = (raw->msg[idx] >> 3) & 0x1;
	msg->u.conn_stat.peer_device_type = (raw->msg[idx] & 0x7);
	idx++;
	return true;
fail_len:
	DP_ERR("connection status reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_resource_status_notify
	(const struct mtk_drm_dp_mst_topology_mgr *mgr,
	struct mtk_drm_dp_sideband_msg_rx *raw,
	struct mtk_drm_dp_sideband_msg_req_body *msg)
{
	int idx = 1;

	msg->u.resource_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(msg->u.resource_stat.guid, &raw->msg[idx], 16);
	idx += 16;
	if (idx > raw->curlen)
		goto fail_len;

	msg->u.resource_stat.available_pbn = (raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx++;
	return true;
fail_len:
	DP_ERR("resource status reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool mtk_drm_dp_sideband_parse_req(const struct mtk_drm_dp_mst_topology_mgr *mgr,
					  struct mtk_drm_dp_sideband_msg_rx *raw,
				      struct mtk_drm_dp_sideband_msg_req_body *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->req_type = (raw->msg[0] & 0x7f);

	switch (msg->req_type) {
	case DP_CONNECTION_STATUS_NOTIFY:
		return mtk_drm_dp_sideband_parse_connection_status_notify(mgr, raw, msg);
	case DP_RESOURCE_STATUS_NOTIFY:
		return mtk_drm_dp_sideband_parse_resource_status_notify(mgr, raw, msg);
	default:
		DP_ERR("Got unknown request 0x%02x (%s)\n",
		       msg->req_type, mtk_drm_dp_mst_req_type_str(msg->req_type));
		return false;
	}
}

static void build_dpcd_write(struct mtk_drm_dp_sideband_msg_tx *msg,
			     u8 port_num, u32 offset, u8 num_bytes, u8 *bytes)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_REMOTE_DPCD_WRITE;
	req.u.dpcd_write.port_number = port_num;
	req.u.dpcd_write.dpcd_address = offset;
	req.u.dpcd_write.num_bytes = num_bytes;
	req.u.dpcd_write.bytes = bytes;
	mtk_drm_dp_encode_sideband_req(&req, msg);
}

static void build_link_address(struct mtk_drm_dp_sideband_msg_tx *msg)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_LINK_ADDRESS;
	mtk_drm_dp_encode_sideband_req(&req, msg);
}

static void build_clear_payload_id_table(struct mtk_drm_dp_sideband_msg_tx *msg)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_CLEAR_PAYLOAD_ID_TABLE;
	mtk_drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static int build_enum_path_resources(struct mtk_drm_dp_sideband_msg_tx *msg,
				     int port_num)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_ENUM_PATH_RESOURCES;
	req.u.port_num.port_number = port_num;
	mtk_drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
	return 0;
}

static void build_allocate_payload(struct mtk_drm_dp_sideband_msg_tx *msg,
				   int port_num,
				   u8 vcpi, u16 pbn,
				   u8 number_sdp_streams,
				   u8 *sdp_stream_sink)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	memset(&req, 0, sizeof(req));
	req.req_type = DP_ALLOCATE_PAYLOAD;
	req.u.allocate_payload.port_number = port_num;
	req.u.allocate_payload.vcpi = vcpi;
	req.u.allocate_payload.pbn = pbn;
	req.u.allocate_payload.number_sdp_streams = number_sdp_streams;
	memcpy(req.u.allocate_payload.sdp_stream_sink, sdp_stream_sink, number_sdp_streams);
	mtk_drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static void build_power_updown_phy(struct mtk_drm_dp_sideband_msg_tx *msg,
				   int port_num, bool power_up)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	if (power_up)
		req.req_type = DP_POWER_UP_PHY;
	else
		req.req_type = DP_POWER_DOWN_PHY;

	req.u.port_num.port_number = port_num;
	mtk_drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static int
build_query_stream_enc_status(struct mtk_drm_dp_sideband_msg_tx *msg, u8 stream_id,
			      u8 *q_id)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_QUERY_STREAM_ENC_STATUS;
	req.u.enc_status.stream_id = stream_id;
	memcpy(req.u.enc_status.client_id, q_id, sizeof(req.u.enc_status.client_id));
	req.u.enc_status.stream_event = 0;
	req.u.enc_status.valid_stream_event = false;
	req.u.enc_status.stream_behavior = 0;
	req.u.enc_status.valid_stream_behavior = false;

	mtk_drm_dp_encode_sideband_req(&req, msg);
	return 0;
}

u8 first_zero_bit(unsigned long *mask, u8 size)
{
	u8 cur;

	for (cur = 0; cur < size; cur++)
		if ((~(*mask)) >> cur & BIT(0))
			break;
	return cur;
}

void set_cur_bit(u8 cur, unsigned long *mask)
{
	(*mask) |= (1 << cur);
}

void clear_cur_bit(u8 cur, unsigned long *mask)
{
	(*mask) &= (~(1 << cur));
}

static int mtk_drm_dp_mst_assign_payload_id(struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_vcpi *vcpi)
{
	int ret, vcpi_ret;
	u8 payload_index;

	ret = first_zero_bit(&mgr->payload_mask, mgr->max_payloads + 1);
	if (ret > mgr->max_payloads) {
		DP_ERR("payload mask %lx, max_payloads %d\n",
		       mgr->payload_mask, mgr->max_payloads);
		DP_ERR("out of payload ids %d\n", ret);
		ret = -DPTX_STATUS_ERR;
		return ret;
	}

	vcpi_ret = first_zero_bit(&mgr->vcpi_mask, mgr->max_payloads + 1);
	if (vcpi_ret > mgr->max_payloads) {
		ret = -DPTX_STATUS_ERR;
		DP_ERR("out of vcpi ids %d\n", ret);
		return ret;
	}

	set_cur_bit(ret, &mgr->payload_mask);
	set_cur_bit(vcpi_ret, &mgr->vcpi_mask);
	vcpi->vcpi = vcpi_ret + 1;
#if (DPTX_MST_DEBUG == 0x1)
	payload_index = ret;
	//record the vcpi pointer in the structure port
	mtk_drm_dp->propose_vcpis[payload_index] = vcpi;

	DP_MSG("Index %d, Assign VPCI %d\n", ret, vcpi->vcpi);
#else
	mgr->proposed_vcpis[ret - 1] = vcpi;
#endif
	return ret;
}

void mtk_drm_dp_mst_put_payload_id(struct mtk_drm_dp_mst_topology_mgr *mgr,
				   int vcpi)
{
#if (DPTX_MST_DEBUG == 0x1)
	u16 payload_index;
#endif

	if (vcpi == 0)
		return;

	DP_DBG("putting payload %d\n", vcpi);
	clear_cur_bit(vcpi - 1, &mgr->vcpi_mask);

#if (DPTX_MST_DEBUG == 0x1)
	for (payload_index = 0; payload_index < mgr->max_payloads; payload_index++) {
		if (mtk_drm_dp->propose_vcpis[payload_index] &&
		    mtk_drm_dp->propose_vcpis[payload_index]->vcpi == vcpi) {
			DP_DBG("putting payload %d payload index %d\n", vcpi, payload_index);
			mtk_drm_dp->propose_vcpis[payload_index] = NULL;

			mtk_drm_dp->payload[payload_index].payload_state = 0;
			mtk_drm_dp->payload[payload_index].start_slot = 0;
			mtk_drm_dp->payload[payload_index].num_slots = 0;
			mtk_drm_dp->payload[payload_index].vcpi = 0;
			clear_cur_bit(payload_index, &mgr->payload_mask);
		}
	}
#else
	int i;

	for (i = 0; i < mgr->max_payloads; i++) {
		if (mgr->proposed_vcpis[i] &&
		    mgr->proposed_vcpis[i]->vcpi == vcpi) {
			mgr->proposed_vcpis[i] = NULL;
			clear_cur_bit(i + 1, &mgr->payload_mask);
		}
	}
#endif
}

static int mtk_drm_dp_mst_wait_tx_reply(struct mtk_drm_dp_mst_branch *mstb,
					struct mtk_drm_dp_sideband_msg_tx *txmsg)
{
#if (DPTX_MST_DEBUG == 0x1)
	u16 counter = 0;

	mtk_drm_dp->end_of_msg_trans_flag = false; //clear the flag
#ifndef DPTX_FPGA
	mdelay(100);
#endif
	while (++counter < 10000) {
		mdelay(1);
		DP_MSG("wait reply %d, phy_status:%d\n",
		       counter, mst_mtk_dp->training_info.phy_status);

		if ((mst_mtk_dp->training_info.phy_status & DPTX_PHY_HPD_INT_EVNET)
				== DPTX_PHY_HPD_INT_EVNET) {
			mst_mtk_dp->training_info.phy_status &= (~DPTX_PHY_HPD_INT_EVNET);
			mtk_dp_hpd_check_sink_event(mst_mtk_dp);
		}

		// Wait, until all the messages have been received
		if (mtk_drm_dp->end_of_msg_trans_flag)
			break;
	}
	mtk_drm_dp->end_of_msg_trans_flag = false; //clear the flag
#else
	int i;
	struct mtk_drm_dp_mst_topology_mgr *mgr = mstb->mgr;

	// force timeout 50ms * 80 = 4 sec
	for (i = 0; i < 80; i++) {
		mdelay(50);
		wake_up_all_work();

		if (!mgr->cbs->poll_hpd_irq)
			break;

		mgr->cbs->poll_hpd_irq(mgr);
	}
#endif

	if (txmsg->state == DRM_DP_SIDEBAND_TX_TIMEOUT) {
		DP_ERR("DRM_DP_SIDEBAND_TX_TIMEOUT!!\n");
		return -DPTX_STATUS_ERR;
	} else if (txmsg->state == DRM_DP_SIDEBAND_TX_RX) {
		return DPTX_STATUS_DONE;
	}

	DP_ERR("timeout msg send %d %d\n", txmsg->state, txmsg->seqno);

	/* remove from q */
	if (txmsg->state == DRM_DP_SIDEBAND_TX_QUEUED ||
	    txmsg->state == DRM_DP_SIDEBAND_TX_START_SEND ||
	    txmsg->state == DRM_DP_SIDEBAND_TX_SENT) {
		DP_ERR("deleted txmsg->next in wait reply\n\n");
		list_del(&txmsg->next);
	}

	mtk_drm_dp_mst_dump_sideband_msg_tx(txmsg);

	mtk_drm_dp_tx_work();
	return -DPTX_STATUS_ERR;
}

static void mtk_drm_dp_free_mst_port(struct kref *kref)
{
	struct mtk_drm_dp_mst_port *port =
		container_of(kref, struct mtk_drm_dp_mst_port, malloc_kref);

	mtk_drm_dp_mst_put_mstb_malloc(port->parent);
	DP_MSG("******free port %d on %d\n", port->port_num, port->parent->lct);
	memset(port, 0, sizeof(struct mtk_drm_dp_mst_port));
	//kfree(port);
}

void mtk_drm_dp_mst_get_port_malloc(struct mtk_drm_dp_mst_port *port)
{
	kref_get(&port->malloc_kref);
	DP_MSG("port malloc (%d) get\n", kref_read(&port->malloc_kref));
}

void mtk_drm_dp_mst_put_port_malloc(struct mtk_drm_dp_mst_port *port)
{
	DP_MSG("port malloc (%d) put\n", kref_read(&port->malloc_kref) - 1);
	kref_put(&port->malloc_kref, mtk_drm_dp_free_mst_port);
}

static struct mtk_drm_dp_mst_branch *mtk_drm_dp_add_mst_branch_device(u8 lct, u8 *rad)
{
	struct mtk_drm_dp_mst_branch *mstb;

	mstb = &mtk_drm_dp->branch_debug_real[lct - 1];
	memset(mstb, 0, sizeof(struct mtk_drm_dp_mst_branch));

	if (!mstb) {
		DP_ERR("malloc mstb fail\n");
		return NULL;
	}

	DP_MSG("add a mstb on LCT %d\n", lct);

	mstb->lct = lct;
	if (lct > 1)
		memcpy(mstb->rad, rad, lct / 2);
	INIT_LIST_HEAD(&mstb->ports);
	kref_init(&mstb->topology_kref);
	kref_init(&mstb->malloc_kref);
	return mstb;
}

static void mtk_drm_dp_free_mst_branch_device(struct kref *kref)
{
	struct mtk_drm_dp_mst_branch *mstb =
		container_of(kref, struct mtk_drm_dp_mst_branch, malloc_kref);

	if (mstb->port_parent)
		mtk_drm_dp_mst_put_port_malloc(mstb->port_parent);

	DP_MSG("******free mstb on %d\n", mstb->lct);

	if (mstb->port_parent) {
		mstb->port_parent->mstb = NULL;
		memset(mstb, 0, sizeof(struct mtk_drm_dp_mst_branch));
	}
	//kfree(mstb);
}

static void mtk_drm_dp_mst_get_mstb_malloc(struct mtk_drm_dp_mst_branch *mstb)
{
	kref_get(&mstb->malloc_kref);
	DP_MSG("mstb malloc (%d) get\n", kref_read(&mstb->malloc_kref));
}

void mtk_drm_dp_mst_put_mstb_malloc(struct mtk_drm_dp_mst_branch *mstb)
{
	DP_MSG("mstb malloc (%d) put\n", kref_read(&mstb->malloc_kref) - 1);
	kref_put(&mstb->malloc_kref, mtk_drm_dp_free_mst_branch_device);
}

static void mtk_drm_dp_destroy_mst_branch_device(struct kref *kref)
{
	struct mtk_drm_dp_mst_branch *mstb =
		container_of(kref, struct mtk_drm_dp_mst_branch, topology_kref);
	struct mtk_drm_dp_mst_topology_mgr *mgr = mstb->mgr;

	//DP_DBG("mstb is about to be free with kref %d, %d\n",
	//      mstb->topology_kref.refcount, kref->refcount);

	INIT_LIST_HEAD(&mstb->destroy_next);

	/*
	 * This can get called under mgr->mutex, so we need to perform the
	 * actual destruction of the mstb in another worker
	 */
	list_add(&mstb->destroy_next, &mgr->destroy_branch_device_list);
	mtk_drm_dp_delayed_destroy_work();
}

static int
mtk_drm_dp_mst_topology_try_get_mstb(struct mtk_drm_dp_mst_branch *mstb)
{
	int ret;

	ret = kref_get_unless_zero(&mstb->topology_kref);

	DP_MSG("mstb (%d) try get\n", kref_read(&mstb->topology_kref));

	return ret;
}

static void mtk_drm_dp_mst_topology_get_mstb(struct mtk_drm_dp_mst_branch *mstb)
{
	if (kref_read(&mstb->topology_kref) == 0)
		DP_ERR("kref count == 0 !\n");
	kref_get(&mstb->topology_kref);
	DP_MSG("mstb (%d) get\n", kref_read(&mstb->topology_kref));
}

void
mtk_drm_dp_mst_topology_put_mstb(struct mtk_drm_dp_mst_branch *mstb)
{
	if (!mstb)
		return;

	DP_MSG("mstb (%d) put\n", kref_read(&mstb->topology_kref) - 1);
	kref_put(&mstb->topology_kref, mtk_drm_dp_destroy_mst_branch_device);
}

static void mtk_drm_dp_destroy_port(struct kref *kref)
{
	struct mtk_drm_dp_mst_port *port =
		container_of(kref, struct mtk_drm_dp_mst_port, topology_kref);
	struct mtk_drm_dp_mst_topology_mgr *mgr = port->mgr;

	//DP_DBG("port is about to be destroy with kref %d, %d\n",
	//     port->topology_kref.refcount, kref->refcount);

	/* There's nothing that needs locking to destroy an input port yet */
	if (port->input) {
		mtk_drm_dp_mst_put_port_malloc(port);
		return;
	}

	kfree(port->cached_edid);

	/*
	 * we can't destroy the connector here, as we might be holding the
	 * mode_config.mutex from an EDID retrieval
	 */
	list_add(&port->next, &mgr->destroy_port_list);
	mtk_drm_dp_delayed_destroy_work();
}

static int
mtk_drm_dp_mst_topology_try_get_port(struct mtk_drm_dp_mst_port *port)
{
	int ret;

	ret = kref_get_unless_zero(&port->topology_kref);
	if (ret)
		DP_MSG("port (%d) try get\n", kref_read(&port->topology_kref));

	return ret;
}

static void mtk_drm_dp_mst_topology_get_port(struct mtk_drm_dp_mst_port *port)
{
	if (kref_read(&port->topology_kref) == 0)
		DP_ERR("Unexpected kref count is 0!\n");

	kref_get(&port->topology_kref);
	DP_MSG("port (%d) get\n", kref_read(&port->topology_kref));
}

static void mtk_drm_dp_mst_topology_put_port(struct mtk_drm_dp_mst_port *port)
{
	DP_MSG("port (%d) put\n", kref_read(&port->topology_kref) - 1);
	kref_put(&port->topology_kref, mtk_drm_dp_destroy_port);
}

static struct mtk_drm_dp_mst_branch *
mtk_drm_dp_mst_topology_get_mstb_validated_locked(struct mtk_drm_dp_mst_branch *mstb,
						  struct mtk_drm_dp_mst_branch *to_find)
{
	struct mtk_drm_dp_mst_port *port;
	struct mtk_drm_dp_mst_branch *rmstb;

	if (to_find == mstb)
		return mstb;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port->mstb) {
			rmstb = mtk_drm_dp_mst_topology_get_mstb_validated_locked
				(port->mstb, to_find);
			if (rmstb)
				return rmstb;
		}
	}
	return NULL;
}

struct mtk_drm_dp_mst_branch *
mtk_drm_dp_mst_topology_get_mstb_validated(struct mtk_drm_dp_mst_topology_mgr *mgr,
					   struct mtk_drm_dp_mst_branch *mstb)
{
	struct mtk_drm_dp_mst_branch *rmstb = NULL;

	if (mgr->mst_primary) {
		rmstb = mtk_drm_dp_mst_topology_get_mstb_validated_locked
			(mgr->mst_primary, mstb);

		if (rmstb && !mtk_drm_dp_mst_topology_try_get_mstb(rmstb))
			rmstb = NULL;
	}

	return rmstb;
}

static struct mtk_drm_dp_mst_port *
mtk_drm_dp_mst_topology_get_port_validated_locked(struct mtk_drm_dp_mst_branch *mstb,
						  struct mtk_drm_dp_mst_port *to_find)
{
	struct mtk_drm_dp_mst_port *port, *mport;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port == to_find)
			return port;

		if (port->mstb) {
			mport = mtk_drm_dp_mst_topology_get_port_validated_locked
				(port->mstb, to_find);
			if (mport)
				return mport;
		}
	}
	return NULL;
}

static struct mtk_drm_dp_mst_port *
mtk_drm_dp_mst_topology_get_port_validated(struct mtk_drm_dp_mst_topology_mgr *mgr,
					   struct mtk_drm_dp_mst_port *port)
{
	struct mtk_drm_dp_mst_port *rport = NULL;

	if (mgr->mst_primary) {
		rport = mtk_drm_dp_mst_topology_get_port_validated_locked
			(mgr->mst_primary, port);

		if (rport && !mtk_drm_dp_mst_topology_try_get_port(rport))
			rport = NULL;
	}

	return rport;
}

static struct mtk_drm_dp_mst_port *mtk_drm_dp_get_port
	(struct mtk_drm_dp_mst_branch *mstb, u8 port_num)
{
	struct mtk_drm_dp_mst_port *port;
	int ret;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port->port_num == port_num) {
			ret = mtk_drm_dp_mst_topology_try_get_port(port);
			return ret ? port : NULL;
		}
	}

	return NULL;
}

static u8 mtk_drm_dp_calculate_rad(struct mtk_drm_dp_mst_port *port, u8 *rad)
{
	int parent_lct = port->parent->lct;
	int shift = 4;
	int idx = (parent_lct - 1) / 2;

	if (parent_lct > 1) {
		memcpy(rad, port->parent->rad, idx + 1);
		shift = (parent_lct % 2) ? 4 : 0;
	} else {
		rad[0] = 0;
	}

	rad[idx] |= port->port_num << shift;
	return parent_lct + 1;
}

bool mtk_drm_dp_mst_is_end_device(u8 pdt, bool mcs)
{
	switch (pdt) {
	case DP_PEER_DEVICE_DP_LEGACY_CONV:
	case DP_PEER_DEVICE_SST_SINK:
		return true;
	case DP_PEER_DEVICE_MST_BRANCHING:
		/* For sst branch device */
		if (!mcs)
			return true;

		return false;
	}
	return true;
}

static int
mtk_drm_dp_port_set_pdt(struct mtk_drm_dp_mst_port *port, u8 new_pdt, bool new_mcs)
{
	//struct mtk_drm_dp_mst_topology_mgr *mgr = port->mgr;
	struct mtk_drm_dp_mst_branch *mstb;
	u8 rad[8], lct;
	int ret = 0;

	if (port->pdt == new_pdt && port->mcs == new_mcs)
		return 0;

	/* Teardown the old pdt, if there is one */
	if (port->pdt != DP_PEER_DEVICE_NONE) {
		if (mtk_drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
			/*
			 * If the new PDT would also have an i2c bus,
			 * don't bother with reregistering it
			 */
			if (new_pdt != DP_PEER_DEVICE_NONE &&
			    mtk_drm_dp_mst_is_end_device(new_pdt, new_mcs)) {
				port->pdt = new_pdt;
				port->mcs = new_mcs;
				return 0;
			}

		} else {
			if (port->mstb) {
				mtk_drm_dp_mst_topology_put_mstb(port->mstb);
				port->mstb = NULL;
			}
		}
	}

	port->pdt = new_pdt;
	port->mcs = new_mcs;

	if (port->pdt != DP_PEER_DEVICE_NONE) {
		if (mtk_drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
			DP_MSG("end of the device !\n");
		} else {
			lct = mtk_drm_dp_calculate_rad(port, rad);
			mstb = mtk_drm_dp_add_mst_branch_device(lct, rad);
			if (!mstb) {
				ret = -DPTX_STATUS_ERR;
				DP_ERR("Failed to create MSTB for port");
				goto out;
			}

			port->mstb = mstb;
			mstb->mgr = port->mgr;
			mstb->port_parent = port;

			if (lct > (DP_STREAM_MAX + 1))
				DP_ERR("Unexpected LCT %d\n", lct);
			/*
			 * Make sure this port's memory allocation stays
			 * around until its child MSTB releases it
			 */
			mtk_drm_dp_mst_get_port_malloc(port);

			/* And make sure we send a link address for this */
			ret = 1;
		}
	}

out:
	if (ret < 0)
		port->pdt = DP_PEER_DEVICE_NONE;
	return ret;
}

ssize_t mtk_drm_dp_mst_dpcd_read(struct mtk_drm_dp_mst_port *port,
				 u32 offset, void *buffer, size_t size)
{
	return mtk_drm_dp_send_dpcd_read(port->mgr, port, offset, size, buffer);
}

ssize_t mtk_drm_dp_mst_dpcd_write(struct mtk_drm_dp_mst_port *port, u32 offset, void *buffer,
				  size_t size)
{
	return mtk_drm_dp_send_dpcd_write(port->mgr, port, offset, size, buffer);
}

static int mtk_drm_dp_check_mstb_guid(struct mtk_drm_dp_mst_branch *mstb, u8 *guid)
{
	int ret = 0;

	memcpy(mstb->guid, guid, 16);

	if (!mtk_drm_dp_validate_guid(mstb->mgr, mstb->guid)) {
		if (mstb->port_parent) {
			ret = mtk_drm_dp_send_dpcd_write(mstb->mgr,
							 mstb->port_parent,
						     DP_GUID, 16, mstb->guid);
		} else {
			if (drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_GUID, mstb->guid, 16))
				ret = 16;
		}
	}

	if (ret < 16)
		return -DPTX_STATUS_ERR;

	return ret == 16 ? 0 : ret;
}

static void build_mst_prop_path(const struct mtk_drm_dp_mst_branch *mstb,
				int pnum,
				char *proppath,
				size_t proppath_size)
{
	int i;
	char temp[8];

	snprintf(proppath, proppath_size, "mst:%d", mstb->mgr->conn_base_id);
	for (i = 0; i < (mstb->lct - 1); i++) {
		int shift = (i % 2) ? 0 : 4;
		int port_num = (mstb->rad[i / 2] >> shift) & 0xf;

		snprintf(temp, sizeof(temp), "-%d", port_num);
		strcat(proppath, temp); //, proppath_size);
	}
	snprintf(temp, sizeof(temp), "-%d", pnum);
	strcat(proppath, temp); //, proppath_size);
}

static void
mtk_drm_dp_mst_port_add_connector(struct mtk_drm_dp_mst_branch *mstb,
				  struct mtk_drm_dp_mst_port *port)
{
	//struct mtk_drm_dp_mst_topology_mgr *mgr = port->mgr;
	char proppath[255];
#if DPTX_MST_I2C_READ_ENABLE
	u16 i = 0;
	u8 *edid_debug;
#endif

	build_mst_prop_path(mstb, port->port_num, proppath, sizeof(proppath));

#if DPTX_MST_I2C_READ_ENABLE
	if (port->pdt != DP_PEER_DEVICE_NONE &&
	    mtk_drm_dp_mst_is_end_device(port->pdt, port->mcs))
		port->cached_edid = mtk_dp_mst_drv_send_remote_i2c_read(mstb, port);

	/*debug edid*/
	edid_debug = mtk_drm_dp_mst_get_edid(mstb->mgr, port);

	if (edid_debug) {
		DP_MSG("EDID:");
		for (i = 0; i < DPTX_EDID_SIZE; i++) {
			if ((i % 0x10) == 0x0)
				DP_MSG("\nidx %x: ", i);
			DP_MSG("%x ", edid_debug[i]);
		}
		DP_MSG("\n");
	}
#endif
}

static void
mtk_drm_dp_mst_topology_unlink_port(struct mtk_drm_dp_mst_topology_mgr *mgr,
				    struct mtk_drm_dp_mst_port *port)
{
	port->parent->num_ports--;
	list_del(&port->next);
	mtk_drm_dp_mst_topology_put_port(port);
}

static struct mtk_drm_dp_mst_port *
mtk_drm_dp_mst_add_port(struct mtk_drm_dp_mst_topology_mgr *mgr,
			struct mtk_drm_dp_mst_branch *mstb, u8 port_number)
{
	struct mtk_drm_dp_mst_port *port = &mtk_drm_dp->port_debug_real[mstb->lct - 1][port_number];

	memset(port, 0, sizeof(struct mtk_drm_dp_mst_port));

	DP_MSG("add a new port for port num %d\n", port_number);
	if (port->vcpi.vcpi) {
		DP_ERR("Unexpected vcpi %d\n", port->vcpi.vcpi);
		port->vcpi.vcpi = 0;
	}

	if (!port) {
		DP_ERR("malloc port fail\n");
		return NULL;
	}

	kref_init(&port->topology_kref);
	kref_init(&port->malloc_kref);
	port->parent = mstb;
	port->port_num = port_number;
	port->mgr = mgr;

	/*
	 * Make sure the memory allocation for our parent branch stays
	 * around until our own memory allocation is released
	 */
	mtk_drm_dp_mst_get_mstb_malloc(mstb);

	return port;
}

static int
mtk_drm_dp_mst_handle_link_address_port(struct mtk_drm_dp_mst_branch *mstb,
					struct mtk_drm_dp_link_addr_reply_port *port_msg)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct mtk_drm_dp_mst_port *port;
	int old_ddps = 0, ret;
	u8 new_pdt = DP_PEER_DEVICE_NONE;
	bool new_mcs = 0;
	bool created = false, send_link_addr = false, changed = false;

	if (!port_msg)
		return -DPTX_STATUS_ERR;

	port = mtk_drm_dp_get_port(mstb, port_msg->port_number);

	if (!port) {
		port = mtk_drm_dp_mst_add_port(mgr, mstb, port_msg->port_number);
		if (!port)
			return -DPTX_STATUS_ERR;
		created = true;
		changed = true;
	} else if (!port->input && port_msg->input_port) {
		/* Since port->connector can't be changed here, we create a
		 * new port if input_port changes from 0 to 1
		 */
		mtk_drm_dp_mst_topology_unlink_port(mgr, port);
		mtk_drm_dp_mst_topology_put_port(port);
		port = mtk_drm_dp_mst_add_port(mgr, mstb, port_msg->port_number);
		if (!port)
			return -DPTX_STATUS_ERR;
		changed = true;
		created = true;
	} else if (port->input && !port_msg->input_port) {
		changed = true;
	} else {
		old_ddps = port->ddps;
		changed = port->ddps != port_msg->ddps ||
			(port->ddps &&
			 (port->ldps != port_msg->legacy_device_plug_status ||
			  port->dpcd_rev != port_msg->dpcd_revision ||
			  port->mcs != port_msg->mcs ||
			  port->pdt != port_msg->peer_device_type ||
			  port->num_sdp_stream_sinks !=
			  port_msg->num_sdp_stream_sinks));
	}

	port->input = port_msg->input_port;
	if (!port->input)
		new_pdt = port_msg->peer_device_type;
	new_mcs = port_msg->mcs;
	port->ddps = port_msg->ddps;
	port->ldps = port_msg->legacy_device_plug_status;
	port->dpcd_rev = port_msg->dpcd_revision;
	port->num_sdp_streams = port_msg->num_sdp_streams;
	port->num_sdp_stream_sinks = port_msg->num_sdp_stream_sinks;

	/* manage mstb port lists with mgr lock - take a reference for this list */
	if (created) {
		mtk_drm_dp_mst_topology_get_port(port);
		list_add(&port->next, &mstb->ports);
		mstb->num_ports++;
	}

#if (DPTX_MST_POWER_UP_PHY_ENABLE)
	if (!port->input && new_pdt != DP_PEER_DEVICE_NONE &&
	    mtk_drm_dp_mst_is_end_device(new_pdt, new_mcs))
		mtk_drm_dp_send_power_updown_phy(mgr, port, TRUE);
#endif

	/*
	 * Reprobe PBN caps on both hotplug, and when re-probing the link
	 * for our parent mstb
	 */
	if (old_ddps != port->ddps || !created) {
		if (port->ddps && !port->input) {
			ret = mtk_drm_dp_send_enum_path_resources(mgr, mstb, port);
			if (ret == 1)
				changed = true;
		} else {
			port->full_pbn = 0;
		}
	}

	ret = mtk_drm_dp_port_set_pdt(port, new_pdt, new_mcs);
	if (ret == 1) {
		send_link_addr = true;
	} else if (ret < 0) {
		DP_ERR("Failed to change PDT on port:%d\n", ret);
		goto fail;
	}

	/*
	 * If this port wasn't just created, then we're reprobing because
	 * we're coming out of suspend. In this case, always resend the link
	 * address if there's an MSTB on this port
	 */
	if (!created && port->pdt == DP_PEER_DEVICE_MST_BRANCHING &&
	    port->mcs)
		send_link_addr = true;

	if (!port->input)
		mtk_drm_dp_mst_port_add_connector(mstb, port);

	if (send_link_addr && port->mstb) {
		ret = mtk_drm_dp_send_link_address(mgr, port->mstb);
		if (ret == 1) /* MSTB below us changed */
			changed = true;
		else if (ret < 0)
			goto fail_put;
	}

	/* put reference to this port */
	mtk_drm_dp_mst_topology_put_port(port);
	return changed;

fail:
	mtk_drm_dp_mst_topology_unlink_port(mgr, port);
fail_put:
	mtk_drm_dp_mst_topology_put_port(port);
	return ret;
}

static void
mtk_drm_dp_mst_handle_conn_stat(struct mtk_drm_dp_mst_branch *mstb,
				struct mtk_drm_dp_connection_status_notify *conn_stat)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct mtk_drm_dp_mst_port *port;
	int old_ddps, ret;
	u8 new_pdt;
	u8 new_mcs;

	bool create_connector = false;

	port = mtk_drm_dp_get_port(mstb, conn_stat->port_number);
	if (!port)
		return;

	if (port->input && !conn_stat->input_port) {
		create_connector = true;
		/* Reprobe link address so we get num_sdp_streams */
		mgr->mst_primary->link_address_sent = false;
	}

	old_ddps = port->ddps;
	port->input = conn_stat->input_port;
	port->ldps = conn_stat->legacy_device_plug_status;
	port->ddps = conn_stat->displayport_device_plug_status;

	new_pdt = port->input ? DP_PEER_DEVICE_NONE : conn_stat->peer_device_type;
	new_mcs = conn_stat->message_capability_status;
	ret = mtk_drm_dp_port_set_pdt(port, new_pdt, new_mcs);
	if (ret < 0)
		DP_ERR("Failed to change PDT for port: %d\n", ret);

	if (create_connector)
		mtk_drm_dp_mst_port_add_connector(mstb, port);

	mtk_drm_dp_mst_topology_put_port(port);

	if (old_ddps != port->ddps) {
		if (port->ddps == 0x0) {
			// plug off event
			mtk_dp_mst_drv_reset(mst_mtk_dp, mgr, FALSE);
		} else {
			// plug on event
			mtk_dp_mst_drv_reset(mst_mtk_dp, mgr, TRUE);
			mdelay(10);
			mtk_drm_dp_mst_link_probe_work();
		}
	}
}

static struct mtk_drm_dp_mst_branch *
mtk_drm_dp_get_mst_branch_device(struct mtk_drm_dp_mst_topology_mgr *mgr,
				 u8 lct, u8 *rad)
{
	struct mtk_drm_dp_mst_branch *mstb;
	struct mtk_drm_dp_mst_port *port;
	int i, ret;

	/* find the port by iterating down */
	mstb = mgr->mst_primary;

	if (!mstb)
		goto out;

	for (i = 0; i < lct - 1; i++) {
		int shift = (i % 2) ? 0 : 4;
		int port_num = (rad[i / 2] >> shift) & 0xf;

		list_for_each_entry(port, &mstb->ports, next) {
			if (port->port_num == port_num) {
				mstb = port->mstb;
				if (!mstb) {
					DP_ERR("failed to lookup MSTB with lct %d, rad %02x\n",
					       lct, rad[0]);

					if (lct > DP_STREAM_MAX)
						DP_ERR("Unexpected LCT %d\n", lct);
				#if (DPTX_MST_DEBUG == 0x1)
					if (lct >= 1 && lct <= 15)
						mstb = &mtk_drm_dp->branch_debug_real[lct - 1];
				#endif
				}

				break;
			}
		}
	}
	ret = mtk_drm_dp_mst_topology_try_get_mstb(mstb);
	if (!ret) {
		mstb = NULL;
		DP_ERR("Get mstb fail on LCT %d\n", lct);
		DP_ERR("Primary branch\n");
	}
out:
	return mstb;
}

static struct mtk_drm_dp_mst_branch *
get_mst_branch_device_by_guid_helper(struct mtk_drm_dp_mst_branch *mstb,
				     const u8 *guid)
{
	struct mtk_drm_dp_mst_branch *found_mstb;
	struct mtk_drm_dp_mst_port *port;

	if (memcmp(mstb->guid, guid, 16) == 0)
		return mstb;

	list_for_each_entry(port, &mstb->ports, next) {
		if (!port->mstb)
			continue;

		found_mstb = get_mst_branch_device_by_guid_helper(port->mstb, guid);

		if (found_mstb)
			return found_mstb;
	}

	return NULL;
}

static struct mtk_drm_dp_mst_branch *
mtk_drm_dp_get_mst_branch_device_by_guid(struct mtk_drm_dp_mst_topology_mgr *mgr,
					 const u8 *guid)
{
	struct mtk_drm_dp_mst_branch *mstb;
	int ret;

	/* find the port by iterating down */
	mstb = get_mst_branch_device_by_guid_helper(mgr->mst_primary, guid);
	if (mstb) {
		ret = mtk_drm_dp_mst_topology_try_get_mstb(mstb);
		if (!ret)
			mstb = NULL;
	}

	return mstb;
}

static int mtk_drm_dp_check_and_send_link_address(struct mtk_drm_dp_mst_topology_mgr *mgr,
						  struct mtk_drm_dp_mst_branch *mstb)
{
	struct mtk_drm_dp_mst_port *port;
	int ret;
	bool changed = false;

	if (!mstb->link_address_sent) {
		ret = mtk_drm_dp_send_link_address(mgr, mstb);
		if (ret == 1)
			changed = true;
		else if (ret < 0)
			return ret;
	}

	list_for_each_entry(port, &mstb->ports, next) {
		struct mtk_drm_dp_mst_branch *mstb_child = NULL;

		if (port->input || !port->ddps)
			continue;

		if (port->mstb)
			mstb_child = mtk_drm_dp_mst_topology_get_mstb_validated(mgr, port->mstb);

		if (mstb_child) {
			ret = mtk_drm_dp_check_and_send_link_address(mgr, mstb_child);
			mtk_drm_dp_mst_topology_put_mstb(mstb_child);
			if (ret == 1)
				changed = true;
			else if (ret < 0)
				return ret;
		}
	}

	return changed;
}

void mtk_drm_dp_mst_link_probe_work(void)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = &mst_mtk_dp->mtk_mgr;
	struct mtk_drm_dp_mst_branch *mstb;
	int ret;
	bool clear_payload_id_table;
	static bool clearing_payload_id_table;

	clear_payload_id_table = !mgr->payload_id_table_cleared;
	mgr->payload_id_table_cleared = true;

	mstb = mgr->mst_primary;
	if (mstb) {
		ret = mtk_drm_dp_mst_topology_try_get_mstb(mstb);
		if (!ret)
			mstb = NULL;
	} else {
		return;
	}

	if (clear_payload_id_table) {
		clearing_payload_id_table = true;
		DP_MSG("Clearing payload ID table\n");
		mtk_drm_dp_send_clear_payload_id_table(mgr, mstb);
		clearing_payload_id_table = false;
	}

	// [CH] to gurantee clear payload id table has been finished
	//      before sending others sideband message
	if (clearing_payload_id_table)
		return;

	ret = mtk_drm_dp_check_and_send_link_address(mgr, mstb);
	mtk_drm_dp_mst_topology_put_mstb(mstb);

	//if (ret > 0)
		//drm_kms_helper_hotplug_event(dev);
}

bool mtk_drm_dp_validate_guid(struct mtk_drm_dp_mst_topology_mgr *mgr, u8 *guid)
{
	u32 salt[2];

	if (memchr_inv(guid, 0, 16))
		return true;

	get_random_bytes(salt, sizeof(salt));

	memcpy(&guid[0], &salt[0], sizeof(u32));
	memcpy(&guid[4], &salt[1], sizeof(u32));
	memcpy(&guid[8], &salt[0], sizeof(u32));
	memcpy(&guid[12], &salt[1], sizeof(u32));

	return false;
}

static void build_dpcd_read(struct mtk_drm_dp_sideband_msg_tx *msg,
			    u8 port_num, u32 offset, u8 num_bytes)
{
	struct mtk_drm_dp_sideband_msg_req_body req;

	req.req_type = DP_REMOTE_DPCD_READ;
	req.u.dpcd_read.port_number = port_num;
	req.u.dpcd_read.dpcd_address = offset;
	req.u.dpcd_read.num_bytes = num_bytes;
	mtk_drm_dp_encode_sideband_req(&req, msg);
}

static int mtk_drm_dp_send_sideband_msg(struct mtk_drm_dp_mst_topology_mgr *mgr,
					bool up, u8 *msg, int len)
{
	int ret;
	int regbase = up ? DP_SIDEBAND_MSG_UP_REP_BASE : DP_SIDEBAND_MSG_DOWN_REQ_BASE;
	int tosend, total, offset;
	int retries = 0;

retry:
	total = len;
	offset = 0;
	do {
		tosend = mgr->max_dpcd_transaction_bytes < 16 ?
				(mgr->max_dpcd_transaction_bytes < total ?
					mgr->max_dpcd_transaction_bytes : total) :
				(total > 16 ? 16 : total);

		ret = drm_dp_dpcd_write(&mst_mtk_dp->aux, regbase + offset, &msg[offset], tosend);

		if (!ret) {
			if (ret == -DPTX_STATUS_ERR && retries < 5) {
				retries++;
				goto retry;
			}
			DP_ERR("failed to dpcd write %d %d\n", tosend, ret);
			return -DPTX_STATUS_ERR;
		}
		offset += tosend;
		total -= tosend;
	} while (total > 0);
	return 0;
}

static int set_hdr_from_dst_qlock(struct mtk_drm_dp_sideband_msg_hdr *hdr,
				  struct mtk_drm_dp_sideband_msg_tx *txmsg)
{
	struct mtk_drm_dp_mst_branch *mstb = txmsg->dst;
	u8 req_type;

	req_type = txmsg->msg[0] & 0x7f;
	if (req_type == DP_CONNECTION_STATUS_NOTIFY ||
	    req_type == DP_RESOURCE_STATUS_NOTIFY ||
		req_type == DP_CLEAR_PAYLOAD_ID_TABLE)
		hdr->broadcast = 1;
	else
		hdr->broadcast = 0;
	hdr->path_msg = txmsg->path_msg;
	if (hdr->broadcast) {
		hdr->lct = 1;
		hdr->lcr = 6;
	} else {
		hdr->lct = mstb->lct;
		hdr->lcr = mstb->lct - 1;
	}

	memcpy(hdr->rad, mstb->rad, hdr->lct / 2);

	return 0;
}

/*
 * process a single block of the next message in the sideband queue
 */
static int process_single_tx_qlock(struct mtk_drm_dp_mst_topology_mgr *mgr,
				   struct mtk_drm_dp_sideband_msg_tx *txmsg, bool up)
{
	u8 chunk[48];
	struct mtk_drm_dp_sideband_msg_hdr hdr;
	int len, space, idx, tosend;
	int ret;

	if (txmsg->state == DRM_DP_SIDEBAND_TX_SENT)
		return 0;

	memset(&hdr, 0, sizeof(struct mtk_drm_dp_sideband_msg_hdr));

	if (txmsg->state == DRM_DP_SIDEBAND_TX_QUEUED)
		txmsg->state = DRM_DP_SIDEBAND_TX_START_SEND;

	/* make hdr from dst mst */
	ret = set_hdr_from_dst_qlock(&hdr, txmsg);
	if (ret < 0)
		return ret;

	/* amount left to send in this message */
	len = txmsg->cur_len - txmsg->cur_offset;

	/* 48 - sideband msg size - 1 byte for data CRC, x header bytes */
	space = 48 - 1 - mtk_drm_dp_calc_sb_hdr_size(&hdr);

	tosend = len < space ? len : space;
	if (len == txmsg->cur_len)
		hdr.somt = 1;
	if (space >= len)
		hdr.eomt = 1;

	hdr.msg_len = tosend + 1;
	mtk_drm_dp_encode_sideband_msg_hdr(&hdr, chunk, &idx);
	memcpy(&chunk[idx], &txmsg->msg[txmsg->cur_offset], tosend);
	/* add crc at end */
	mtk_drm_dp_crc_sideband_chunk_req(&chunk[idx], tosend);
	idx += tosend + 1;

	ret = mtk_drm_dp_send_sideband_msg(mgr, up, chunk, idx);
	if (ret) {
		DP_ERR("sideband msg failed to send\n");
		mtk_drm_dp_mst_dump_sideband_msg_tx(txmsg);
		return ret;
	}

	txmsg->cur_offset += tosend;
	if (txmsg->cur_offset == txmsg->cur_len) {
		txmsg->state = DRM_DP_SIDEBAND_TX_SENT;
		return 1;
	}
	return 0;
}

static void process_single_down_tx_qlock(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	int ret;

	/* construct a chunk from the first msg in the tx_msg queue */
	if (list_empty(&mgr->tx_msg_downq)) {
		DP_ERR("tx_msg_downq is empty\n");
		return;
	}

	txmsg = list_first_entry(&mgr->tx_msg_downq,
				 struct mtk_drm_dp_sideband_msg_tx, next);
	ret = process_single_tx_qlock(mgr, txmsg, false);
	if (ret < 0) {
		DP_ERR("failed to send msg in q %d\n", ret);
		list_del(&txmsg->next);
		txmsg->state = DRM_DP_SIDEBAND_TX_TIMEOUT;
		wake_up_all_work();
	}
}

static void mtk_drm_dp_queue_down_tx(struct mtk_drm_dp_mst_topology_mgr *mgr,
				     struct mtk_drm_dp_sideband_msg_tx *txmsg)
{
	//please keep this log
	DP_MSG("tx_msg_downq\n");

	list_add_tail(&txmsg->next, &mgr->tx_msg_downq);

	mtk_drm_dp_mst_dump_sideband_msg_tx(txmsg);

	if (list_is_singular(&mgr->tx_msg_downq))
		process_single_down_tx_qlock(mgr);
}

static void
mtk_drm_dp_dump_link_address(const struct mtk_drm_dp_mst_topology_mgr *mgr,
			     struct mtk_drm_dp_link_address_ack_reply *reply)
{
#if DPTX_PRINT_LEVEL <= 1
	struct mtk_drm_dp_link_addr_reply_port *port_reply;
	int i;

	for (i = 0; i < reply->nports; i++) {
		port_reply = &reply->ports[i];
		DP_MSG("port %d: input %d, pdt: %d, pn: %d, dpcd_rev: %02x\n",
		       i,
			 port_reply->input_port,
			 port_reply->peer_device_type,
			 port_reply->port_number,
			 port_reply->dpcd_revision);
		DP_MSG("mcs: %d, ddps: %d, ldps %d, sdp %d/%d\n",
		       port_reply->mcs,
			 port_reply->ddps,
			 port_reply->legacy_device_plug_status,
			 port_reply->num_sdp_streams,
			 port_reply->num_sdp_stream_sinks);
	}
#endif
}

int mtk_drm_dp_send_link_address(struct mtk_drm_dp_mst_topology_mgr *mgr,
				 struct mtk_drm_dp_mst_branch *mstb)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	struct mtk_drm_dp_link_address_ack_reply *reply;
	//struct mtk_drm_dp_mst_port *port, *tmp;
	int i, ret, port_mask = 0;
	bool changed = false;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_link_address at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg)
		return -DPTX_STATUS_ERR;

	txmsg->dst = mstb;
	build_link_address(txmsg);

	mstb->link_address_sent = true;
	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mst_mtk_dp->training_info.phy_status &= (~DPTX_PHY_HPD_INT_EVNET);
	/* FIXME: Actually do some real error handling here */
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret <= 0) {
		DP_ERR("Sending link address failed with %d\n", ret);
		goto out;
	}
	if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		DP_ERR("link address NAK received\n");
		ret = -DPTX_STATUS_ERR;
		goto out;
	}

	reply = &txmsg->reply.u.link_addr;
	if (!reply->nports)
		DP_ERR("link address reply: %d\n", reply->nports);
	mtk_drm_dp_dump_link_address(mgr, reply);

	ret = mtk_drm_dp_check_mstb_guid(mstb, reply->guid);

	if (ret < 0) {
		char buf[64];

		mtk_drm_dp_mst_rad_to_str(mstb->rad, mstb->lct, buf, sizeof(buf));
		DP_ERR("GUID check on %s failed: %d\n", buf, ret);
#if (DPTX_MST_DEBUG == 0x0)
		goto out;
#endif
	}

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);

	for (i = reply->nports - 1; i >= 0; i--) {
		port_mask |= (1 << reply->ports[i].port_number);
		ret = mtk_drm_dp_mst_handle_link_address_port(mstb, &reply->ports[i]);
		if (ret == 1)
			changed = true;
		else if (ret < 0)
			goto out;
	}

out:
	kfree(txmsg);
	return ret < 0 ? ret : changed;
}

void
mtk_drm_dp_send_clear_payload_id_table(struct mtk_drm_dp_mst_topology_mgr *mgr,
				       struct mtk_drm_dp_mst_branch *mstb)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	int ret;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_clear_payload_id_table at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg)
		return;

	txmsg->dst = mstb;
	build_clear_payload_id_table(txmsg);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	mtk_dp_mst_drv_sideband_msg_rdy_clear(mst_mtk_dp);
	if (ret > 0 && txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
		DP_ERR("clear payload table id nak received\n");

	kfree(txmsg);
}

int
mtk_drm_dp_send_enum_path_resources(struct mtk_drm_dp_mst_topology_mgr *mgr,
				    struct mtk_drm_dp_mst_branch *mstb,
				struct mtk_drm_dp_mst_port *port)
{
	struct mtk_drm_dp_enum_path_resources_ack_reply *path_res;
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	int ret;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_enum_path_resources at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg)
		return -DPTX_STATUS_ERR;

	txmsg->dst = mstb;
	build_enum_path_resources(txmsg, port->port_num);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	mtk_dp_mst_drv_sideband_msg_rdy_clear(mst_mtk_dp);

	if (ret > 0) {
		ret = 0;
		path_res = &txmsg->reply.u.path_resources;

		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
			DP_ERR("enum path resources nak received\n");
		} else {
			if (port->port_num != path_res->port_number)
				DP_ERR("got incorrect port in response\n");

			DP_MSG("enum path resources %d: %d %d\n",
			       path_res->port_number,
				 path_res->full_payload_bw_number,
				 path_res->avail_payload_bw_number);

			/*
			 * If something changed, make sure we send a
			 * hotplug
			 */
			if (port->full_pbn != path_res->full_payload_bw_number ||
			    port->fec_capable != path_res->fec_capable)
				ret = 1;

			port->full_pbn = path_res->full_payload_bw_number;
			port->fec_capable = path_res->fec_capable;
		}
	}

	kfree(txmsg);
	return ret;
}

static struct mtk_drm_dp_mst_port *mtk_drm_dp_get_last_connected_port_to_mstb
	(struct mtk_drm_dp_mst_branch *mstb)
{
	if (!mstb->port_parent)
		return NULL;

	if (mstb->port_parent->mstb != mstb)
		return mstb->port_parent;

	return mtk_drm_dp_get_last_connected_port_to_mstb(mstb->port_parent->parent);
}

static struct mtk_drm_dp_mst_branch *
mtk_drm_dp_get_last_connected_port_and_mstb(struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_mst_branch *mstb,
					int *port_num)
{
	struct mtk_drm_dp_mst_branch *rmstb = NULL;
	struct mtk_drm_dp_mst_port *found_port;

	if (!mgr->mst_primary)
		goto out;

	do {
		found_port = mtk_drm_dp_get_last_connected_port_to_mstb(mstb);
		if (!found_port)
			break;

		if (mtk_drm_dp_mst_topology_try_get_mstb(found_port->parent)) {
			rmstb = found_port->parent;
			*port_num = found_port->port_num;
		} else {
			/* Search again, starting from this parent */
			mstb = found_port->parent;
		}
	} while (!rmstb);
out:
	return rmstb;
}

static int mtk_drm_dp_payload_send_msg(struct mtk_drm_dp_mst_topology_mgr *mgr,
				       struct mtk_drm_dp_mst_port *port,
				   int id,
				   int pbn)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	struct mtk_drm_dp_mst_branch *mstb;
	int ret, port_num;
	u8 sinks[DRM_DP_MAX_SDP_STREAMS];
	int i;

	port_num = port->port_num;
	mstb = mtk_drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb) {
		mstb = mtk_drm_dp_get_last_connected_port_and_mstb(mgr,
								   port->parent,
							       &port_num);

		if (!mstb)
			return -DPTX_STATUS_ERR;
	}

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for payload_send_msg at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg) {
		ret = -DPTX_STATUS_ERR;
		goto fail_put;
	}

	for (i = 0; i < port->num_sdp_streams; i++)
		sinks[i] = i;

	txmsg->dst = mstb;
	build_allocate_payload(txmsg, port_num,
			       id,
			       pbn, port->num_sdp_streams, sinks);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	/*
	 * FIXME: there is a small chance that between getting the last
	 * connected mstb and sending the payload message, the last connected
	 * mstb could also be removed from the topology. In the future, this
	 * needs to be fixed by restarting the
	 * mtk_drm_dp_get_last_connected_port_and_mstb() search in the event of a
	 * timeout if the topology is still connected to the system.
	 */
	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -DPTX_STATUS_ERR;
		else
			ret = 0;
	}
	kfree(txmsg);
fail_put:
	mtk_drm_dp_mst_topology_put_mstb(mstb);
	return ret;
}

int mtk_drm_dp_send_power_updown_phy(struct mtk_drm_dp_mst_topology_mgr *mgr,
				     struct mtk_drm_dp_mst_port *port, bool power_up)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	int ret;

	port = mtk_drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return -DPTX_STATUS_ERR;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_power_updown_phy at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg) {
		DP_ERR("Get txmsg fail\n");
		mtk_drm_dp_mst_topology_put_port(port);
		return -DPTX_STATUS_ERR;
	}

	txmsg->dst = port->parent;
	build_power_updown_phy(txmsg, port->port_num, power_up);
	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(port->parent, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -DPTX_STATUS_ERR;
		else
			ret = 0;
	}
	kfree(txmsg);
	mtk_drm_dp_mst_topology_put_port(port);

	return ret;
}

//EXPORT_SYMBOL(mtk_drm_dp_send_power_updown_phy);

int mtk_drm_dp_send_query_stream_enc_status(struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_mst_port *port,
		struct mtk_drm_dp_query_stream_enc_status_ack_reply *status)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	u32 nonce[7];
	int ret;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_query_stream_enc_status at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));
	if (!txmsg)
		return -DPTX_STATUS_ERR;

	port = mtk_drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port) {
		ret = -DPTX_STATUS_ERR;
		goto out_get_port;
	}

	//get_random_bytes(nonce, sizeof(nonce));
	// change to CTP API
	get_random_bytes(nonce, sizeof(nonce));

	/*
	 * "Source device targets the QUERY_STREAM_ENCRYPTION_STATUS message
	 *  transaction at the MST Branch device directly connected to the
	 *  Source"
	 */
	txmsg->dst = mgr->mst_primary;

	build_query_stream_enc_status(txmsg, port->vcpi.vcpi, (u8 *)nonce);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mgr->mst_primary, txmsg);
	if (ret < 0) {
		goto out;
	} else if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		DP_ERR("query encryption status nak received\n");
		ret = -DPTX_STATUS_ERR;
		goto out;
	}

	ret = 0;
	memcpy(status, &txmsg->reply.u.enc_status, sizeof(*status));

out:
	mtk_drm_dp_mst_topology_put_port(port);
out_get_port:
	kfree(txmsg);
	return ret;
}

//EXPORT_SYMBOL(mtk_drm_dp_send_query_stream_enc_status);

static int mtk_drm_dp_create_payload_step1(struct mtk_drm_dp_mst_topology_mgr *mgr,
					   int id,
				       struct mtk_drm_dp_payload *payload)
{
	int ret;

	ret = mtk_drm_dp_dpcd_write_payload(mgr, id, payload);
	if (ret < 0) {
		payload->payload_state = 0;
		return ret;
	}
	payload->payload_state = DP_PAYLOAD_LOCAL;
	return 0;
}

static int mtk_drm_dp_create_payload_step2(struct mtk_drm_dp_mst_topology_mgr *mgr,
					   struct mtk_drm_dp_mst_port *port,
				       int id,
				       struct mtk_drm_dp_payload *payload)
{
	int ret;

	ret = mtk_drm_dp_payload_send_msg(mgr, port, id, port->vcpi.pbn);

	if (ret < 0)
		return ret;
	payload->payload_state = DP_PAYLOAD_REMOTE;
	return ret;
}

static int mtk_drm_dp_destroy_payload_step1(struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_mst_port *port,
					int id,
					struct mtk_drm_dp_payload *payload)
{
	/* it's okay for these to fail */
	if (port)
		mtk_drm_dp_payload_send_msg(mgr, port, id, 0);

	mtk_drm_dp_dpcd_write_payload(mgr, id, payload);
	payload->payload_state = DP_PAYLOAD_DELETE_LOCAL;
	return 0;
}

static int mtk_drm_dp_destroy_payload_step2(struct mtk_drm_dp_payload *payload)
{
	payload->payload_state = 0;
	return 0;
}

int mtk_drm_dp_update_payload_part1(struct mtk_drm_dp_mst_topology_mgr *mgr, int start_slot)
{
	struct mtk_drm_dp_payload req_payload;
	struct mtk_drm_dp_mst_port *port;
	int i;
	int cur_slots = start_slot;
	bool skip;

#if (DPTX_MST_DEBUG == 0x1)
	for (i = 0; i < mgr->max_payloads; i++) {
		struct mtk_drm_dp_vcpi *vcpi = mtk_drm_dp->propose_vcpis[i];
		struct mtk_drm_dp_payload *payload = &mtk_drm_dp->payload[i];
#else
	for (i = 0; i < mgr->max_payloads; i++) {
		struct mtk_drm_dp_vcpi *vcpi = mgr->proposed_vcpis[i];
		struct mtk_drm_dp_payload *payload = &mgr->payloads[i];
#endif
		bool put_port = false;

		/* solve the current payloads - compare to the hw ones - update the hw view */
		req_payload.start_slot = cur_slots;

		if (vcpi) {
			port = container_of(vcpi, struct mtk_drm_dp_mst_port, vcpi);

			skip = !mtk_drm_dp_mst_port_downstream_of_branch(port, mgr->mst_primary);

			if (skip) {
				DP_MSG("Virtual channel %d is not in current topology\n", i);
				continue;
			}
			/* Validated ports don't matter if we're releasing
			 * VCPI
			 */
			if (vcpi->num_slots) {
				port = mtk_drm_dp_mst_topology_get_port_validated
					(mgr, port);
				if (!port) {
					if (vcpi->num_slots == payload->num_slots) {
						cur_slots += vcpi->num_slots;
						payload->start_slot = req_payload.start_slot;
						payload->num_slots = 0;

						mtk_drm_dp_mst_put_payload_id(mgr, vcpi->vcpi);
						continue;
					} else {
						DP_ERR("Fail:set payload to invalid sink\n");
						return -DPTX_STATUS_ERR;
					}
				}
				put_port = true;
			}
			req_payload.num_slots = vcpi->num_slots;
			req_payload.vcpi = vcpi->vcpi;
		} else {
			port = NULL;
			req_payload.num_slots = 0;
		}

		payload->start_slot = req_payload.start_slot;
		/* work out what is required to happen with this payload */
		if (payload->num_slots != req_payload.num_slots) {
			/* need to push an update for this payload */
			if (req_payload.num_slots) {
				mtk_drm_dp_create_payload_step1(mgr, vcpi->vcpi,
								&req_payload);
				payload->num_slots = req_payload.num_slots;
				payload->vcpi = req_payload.vcpi;

			} else if (payload->num_slots) {
				payload->num_slots = 0;
				mtk_drm_dp_destroy_payload_step1(mgr, port,
								 payload->vcpi,
							     payload);
				req_payload.payload_state =
					payload->payload_state;
				payload->start_slot = 0;
			}
			payload->payload_state = req_payload.payload_state;
		}
		cur_slots += req_payload.num_slots;

		if (put_port)
			mtk_drm_dp_mst_topology_put_port(port);
	}

	return 0;
}

int mtk_drm_dp_update_payload_part2(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	struct mtk_drm_dp_mst_port *port;
	u8 payload_index;
	int ret = 0;
	u8 skip;

	for (payload_index = 0; payload_index < mgr->max_payloads; payload_index++) {
		if (!mtk_drm_dp->propose_vcpis[payload_index])
			continue;
		if ((mgr->vcpi_mask & (1 << payload_index)) == 0x0)
			continue;

		port = container_of(mtk_drm_dp->propose_vcpis[payload_index],
				    struct mtk_drm_dp_mst_port, vcpi);
		skip = !mtk_drm_dp_mst_port_downstream_of_branch(port, mgr->mst_primary);

		if (skip)
			continue;

		DP_DBG("payload %d state%d\n", payload_index,
		       mtk_drm_dp->payload[payload_index].payload_state);
		if (mtk_drm_dp->payload[payload_index].payload_state == DP_PAYLOAD_LOCAL) {
			ret = mtk_drm_dp_create_payload_step2
				(mgr, port, mtk_drm_dp->propose_vcpis[payload_index]->vcpi,
					&mtk_drm_dp->payload[payload_index]);
		} else if (mtk_drm_dp->payload[payload_index].payload_state ==
								DP_PAYLOAD_DELETE_LOCAL) {
			ret = mtk_drm_dp_destroy_payload_step2(&mtk_drm_dp->payload[payload_index]);
		}

		if (ret)
			return ret;
	}

	return 0;
}

int mtk_drm_dp_send_dpcd_read(struct mtk_drm_dp_mst_topology_mgr *mgr,
			      struct mtk_drm_dp_mst_port *port,
				 int offset, int size, u8 *bytes)
{
	int ret = 0;
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	struct mtk_drm_dp_mst_branch *mstb;

	mstb = mtk_drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb)
		return -DPTX_STATUS_ERR;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_dpcd_read at with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg) {
		ret = -DPTX_STATUS_ERR;
		goto fail_put;
	}

	txmsg->dst = port->parent;
	build_dpcd_read(txmsg, port->port_num, offset, size);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret < 0)
		goto fail_free;

	/* DPCD read should never be NACKed */
	if (txmsg->reply.reply_type == 1) {
		DP_ERR("mstb port %d: DPCD read on addr 0x%x for %d bytes NAKed\n",
		       port->port_num, offset, size);
		ret = -DPTX_STATUS_ERR;
		goto fail_free;
	}

	if (txmsg->reply.u.remote_dpcd_read_ack.num_bytes != size) {
		ret = -DPTX_STATUS_ERR;
		goto fail_free;
	}

	ret = (txmsg->reply.u.remote_dpcd_read_ack.num_bytes < size) ?
		txmsg->reply.u.remote_dpcd_read_ack.num_bytes : size;

	memcpy(bytes, txmsg->reply.u.remote_dpcd_read_ack.bytes, ret);

fail_free:
	kfree(txmsg);
fail_put:
	mtk_drm_dp_mst_topology_put_mstb(mstb);

	return ret;
}

int mtk_drm_dp_send_dpcd_write(struct mtk_drm_dp_mst_topology_mgr *mgr,
			       struct mtk_drm_dp_mst_port *port,
				  int offset, int size, u8 *bytes)
{
	int ret;
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	struct mtk_drm_dp_mst_branch *mstb;

	mstb = mtk_drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb)
		return -DPTX_STATUS_ERR;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_dpcd_write with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg) {
		ret = -DPTX_STATUS_ERR;
		goto fail_put;
	}

	txmsg->dst = mstb;
	build_dpcd_write(txmsg, port->port_num, offset, size, bytes);

	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -DPTX_STATUS_ERR;
		else
			ret = size;
	}

	kfree(txmsg);
fail_put:
	mtk_drm_dp_mst_topology_put_mstb(mstb);
	return ret;
}

static int mtk_drm_dp_encode_up_ack_reply(struct mtk_drm_dp_sideband_msg_tx *msg, u8 req_type)
{
	struct mtk_drm_dp_sideband_msg_reply_body reply;

	reply.reply_type = DP_SIDEBAND_REPLY_ACK;
	reply.req_type = req_type;
	mtk_drm_dp_encode_sideband_reply(&reply, msg);
	return 0;
}

static int mtk_drm_dp_send_up_ack_reply(struct mtk_drm_dp_mst_topology_mgr *mgr,
					struct mtk_drm_dp_mst_branch *mstb,
				    int req_type, bool broadcast)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);
	DP_MSG("malloc for send_up_ack_reply with %lu\n",
	       sizeof(struct mtk_drm_dp_sideband_msg_tx));
	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg)
		return -DPTX_STATUS_ERR;

	txmsg->dst = mstb;
	mtk_drm_dp_encode_up_ack_reply(txmsg, req_type);

	/* construct a chunk from the first msg in the tx_msg queue */
	process_single_tx_qlock(mgr, txmsg, true);

	kfree(txmsg);
	return 0;
}

int mtk_drm_dp_get_vc_payload_bw(const struct mtk_drm_dp_mst_topology_mgr *mgr,
				 int link_rate, int link_lane_count)
{
	if (link_rate == 0 || link_lane_count == 0)
		DP_ERR("invalid link rate/lane count: (%d / %d)\n",
		       link_rate, link_lane_count);

	/* See DP v2.0 2.6.4.2, VCPayload_Bandwidth_for_OneTimeSlotPer_MTP_Allocation */
	return link_rate * link_lane_count / 54000;
}

bool mtk_drm_dp_read_mst_cap(const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	u8 mstm_cap;

	if (dpcd[DPCD_00000] < 0x12)
		return false;

	if (drm_dp_dpcd_read(&mst_mtk_dp->aux, DP_MSTM_CAP, &mstm_cap, 1) != 1)
		return false;

	return mstm_cap & DP_MST_CAP;
}

int mtk_drm_dp_mst_topology_mgr_set_mst(struct mtk_drm_dp_mst_topology_mgr *mgr, bool mst_state)
{
	int ret = 0;
	u8 buf = 0;
	struct mtk_drm_dp_mst_branch *mstb = NULL;

	if (!mgr) {
		ret = drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_MSTM_CTRL, &buf, 1);
		DP_MSG("Clear DP_MSTM_CTRL, result %d\n", ret);
	}

	if (mst_state == mgr->mst_state)
		goto out_fail;
	mgr->mst_state = mst_state;
	/* set the device into MST mode */
	if (mst_state) {
		struct mtk_drm_dp_payload reset_pay;
		int lane_count;
		int link_rate;

		/* get dpcd info */
		ret = drm_dp_dpcd_read(&mst_mtk_dp->aux,
				       DPCD_00000, mgr->dpcd, DP_RECEIVER_CAP_SIZE);
		if (ret != DP_RECEIVER_CAP_SIZE) {
			DP_ERR("failed to read DPCD, ret %d\n", ret);
			goto out_fail;
		}

		mgr->max_lane_count = (mgr->dpcd[2] & 0xf) < mgr->max_lane_count ?
				(mgr->dpcd[2] & 0xf) : mgr->max_lane_count;

		mgr->max_link_rate = (mgr->dpcd[1] * 27000) < mgr->max_link_rate ?
				(mgr->dpcd[1] * 27000) : mgr->max_link_rate;

		lane_count = (mst_mtk_dp->training_info.link_lane_count & 0xf)
			< mgr->max_lane_count ?
			(mst_mtk_dp->training_info.link_lane_count & 0xf) : mgr->max_lane_count;

		link_rate = (mst_mtk_dp->training_info.link_rate * 27000) < mgr->max_link_rate ?
				(mst_mtk_dp->training_info.link_rate * 27000) : mgr->max_link_rate;

		mgr->pbn_div = mtk_drm_dp_get_vc_payload_bw(mgr, link_rate, lane_count);

		if (mgr->pbn_div == 0) {
			ret = -DPTX_STATUS_ERR;
			goto out_fail;
		}

		/* add initial branch device at LCT 1 */
		mstb = mtk_drm_dp_add_mst_branch_device(1, NULL);
		if (!mstb) {
			ret = -DPTX_STATUS_ERR;
			goto out_fail;
		}
		mstb->mgr = mgr;

		/* give this the main reference */
		mgr->mst_primary = mstb;
		mtk_drm_dp_mst_topology_get_mstb(mgr->mst_primary);

		buf = DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC;
		drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_MSTM_CTRL, &buf, 1);
		reset_pay.start_slot = 0;
		reset_pay.num_slots = 0x3f;
		mtk_drm_dp_dpcd_write_payload(mgr, 0, &reset_pay);

		mtk_drm_dp_mst_link_probe_work();
	#if DPTX_MST_DEBUG
		return ret;
	#endif
		ret = 0;
	} else {
		/* disable MST on the device */
		mstb = mgr->mst_primary;
		mgr->mst_primary = NULL;
		DP_MSG("disable MST\n");
		/* this can fail if the device is gone */
		buf = 0;
		drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_MSTM_CTRL, &buf, 1);

		ret = 0;
	#if (DPTX_MST_DEBUG == 0x1)
		memset(&mtk_drm_dp->payload[0], 0,
		       DP_PAYLOAD_MAX * sizeof(mtk_drm_dp->payload[0]));
		memset(&mtk_drm_dp->mstb[0], 0,
		       sizeof(mtk_drm_dp->mstb));
	#else
		memset(mgr->payloads, 0,
		       mgr->max_payloads * sizeof(mgr->payloads[0]));
		memset(mgr->proposed_vcpis, 0,
		       mgr->max_payloads * sizeof(mgr->proposed_vcpis[0]));
	#endif
		mgr->payload_mask = 0;
		set_cur_bit(0, &mgr->payload_mask);
		mgr->vcpi_mask = 0;
		mgr->payload_id_table_cleared = false;
	}

out_fail:
	if (mstb)
		mtk_drm_dp_mst_topology_put_mstb(mstb);

	return ret;
}

int mtk_drm_dp_mst_topology_mgr_resume(struct mtk_drm_dp_mst_topology_mgr *mgr, bool sync)
{
	int ret;
	u8 guid[16], buf;

	if (!mgr->mst_primary)
		goto out_fail;

	ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, DPCD_00000, mgr->dpcd, DP_RECEIVER_CAP_SIZE);
	if (ret != DP_RECEIVER_CAP_SIZE) {
		DP_ERR("dpcd read failed - undocked during suspend?\n");
		goto out_fail;
	}

	buf = DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC;
	drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_MSTM_CTRL, &buf, 1);

	/* Some hubs forget their guids after they resume */
	ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, DP_GUID, guid, 16);
	if (ret != 16) {
		DP_ERR("dpcd read failed - undocked during suspend?\n");
		goto out_fail;
	}

	ret = mtk_drm_dp_check_mstb_guid(mgr->mst_primary, guid);
	if (ret) {
		DP_ERR("check mstb failed - undocked during suspend?\n");
		goto out_fail;
	}

	mtk_drm_dp_mst_link_probe_work();

	return 0;

out_fail:
	return -1;
}

static bool
mtk_drm_dp_get_one_sb_msg(struct mtk_drm_dp_mst_topology_mgr *mgr, bool up,
			  struct mtk_drm_dp_mst_branch **mstb)
{
	int len, ret;
	int replylen, curreply;
	u8 replyblock[32];
	u8 hdrlen, i;
	struct mtk_drm_dp_sideband_msg_hdr hdr;
	struct mtk_drm_dp_sideband_msg_rx *msg;
	int basereg = up ? DP_SIDEBAND_MSG_UP_REQ_BASE : DP_SIDEBAND_MSG_DOWN_REP_BASE;

	msg = up ? &mgr->up_req_recv : &mgr->down_rep_recv;

	if (!up)
		*mstb = NULL;

	len = mgr->max_dpcd_transaction_bytes < 16 ? mgr->max_dpcd_transaction_bytes : 16;
	ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, basereg, replyblock, len);
	if (ret != len) {
		DP_ERR("failed to read DPCD down rep %d %d\n", len, ret);
		return false;
	}

	ret = mtk_drm_dp_decode_sideband_msg_hdr(mgr, &hdr, replyblock, len, &hdrlen);
	if (hdr.eomt) {
		mtk_drm_dp->end_of_msg_trans_flag = true;
		DP_MSG("The sideband has finished!\n");
	} else {
		DP_MSG("The sideband hasn't finished!\n");
	}

	if (!ret) {
		for (i = 0; i < len; i++)
			DP_ERR("%s\n", replyblock);
		DP_ERR("ERROR: failed header\n");
		return false;
	}

	if (!up) {
		/* Caller is responsible for giving back this reference */
		*mstb = mtk_drm_dp_get_mst_branch_device(mgr, hdr.lct, hdr.rad);

		if (!*mstb) {
			DP_ERR("Got MST reply from unknown device %d\n", hdr.lct);
			return false;
		}
	}

	if (!mtk_drm_dp_sideband_msg_set_header(msg, &hdr, hdrlen)) {
		DP_ERR("sideband msg set header failed %d\n", replyblock[0]);
		return false;
	}

	replylen = msg->curchunk_len < (len - hdrlen) ? msg->curchunk_len : (len - hdrlen);
	ret = mtk_drm_dp_sideband_append_payload(msg, replyblock + hdrlen, replylen);
	if (!ret) {
		DP_ERR("sideband msg build failed %d\n", replyblock[0]);
		return false;
	}

	replylen = msg->curchunk_len + msg->curchunk_hdrlen - len;
	curreply = len;
	while (replylen > 0) {
		len = mgr->max_dpcd_transaction_bytes < 16 ?
			(mgr->max_dpcd_transaction_bytes < replylen ?
				mgr->max_dpcd_transaction_bytes : replylen) :
			(replylen < 16 ? replylen : 16);

		ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, basereg + curreply, replyblock, len);
		if (ret != len) {
			DP_ERR("failed to read a chunk (len %d, ret %d)\n",
			       len, ret);
			return false;
		}

		ret = mtk_drm_dp_sideband_append_payload(msg, replyblock, len);
		if (!ret) {
			DP_ERR("failed to build sideband msg\n");
			return false;
		}

		curreply += len;
		replylen -= len;
	}
	return true;
}

static int mtk_drm_dp_mst_handle_down_rep(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	struct mtk_drm_dp_sideband_msg_tx *txmsg;
	struct mtk_drm_dp_mst_branch *mstb = NULL;
	struct mtk_drm_dp_sideband_msg_rx *msg = &mgr->down_rep_recv;

	if (!mtk_drm_dp_get_one_sb_msg(mgr, false, &mstb))
		goto out;

	/* Multi-packet message transmission, don't clear the reply */
	if (!msg->have_eomt)
		goto out;

	/* find the message */
	txmsg = list_first_entry_or_null(&mgr->tx_msg_downq,
					 struct mtk_drm_dp_sideband_msg_tx, next);

	/* Were we actually expecting a response, and from this mstb? */
	if (!txmsg || txmsg->dst != mstb) {
	#if ENABLE_DPTX_DEBUG
		struct mtk_drm_dp_sideband_msg_hdr *hdr;

		hdr = &msg->initial_hdr;
		DP_ERR("Got MST reply with no msg %d %d %02x %02x\n",
		       hdr->seqno, hdr->lct, hdr->rad[0], msg->msg[0]);
	#endif

		if (!txmsg)
			DP_ERR("txmsg\n");
		else
			DP_ERR("mstb, txmsg->dst\n");
	}

	mtk_drm_dp_sideband_parse_reply(mgr, msg, &txmsg->reply);

	if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		DP_ERR("Got NAK reply: req 0x%02x (%s), reason 0x%02x (%s), nak data 0x%02x\n",
		       txmsg->reply.req_type,
			 mtk_drm_dp_mst_req_type_str(txmsg->reply.req_type),
			 txmsg->reply.u.nak.reason,
			 mtk_drm_dp_mst_nak_reason_str(txmsg->reply.u.nak.reason),
			 txmsg->reply.u.nak.nak_data);
	}

	memset(msg, 0, sizeof(struct mtk_drm_dp_sideband_msg_rx));
	mtk_drm_dp_mst_topology_put_mstb(mstb);

	txmsg->state = DRM_DP_SIDEBAND_TX_RX;
	list_del(&txmsg->next);

	wake_up_all_work();

	return 0;

out:
	if (mstb)
		mtk_drm_dp_mst_topology_put_mstb(mstb);

	return 0;
}

static inline bool
mtk_drm_dp_mst_process_up_req(struct mtk_drm_dp_mst_topology_mgr *mgr,
			      struct mtk_drm_dp_pending_up_req *up_req)
{
	struct mtk_drm_dp_mst_branch *mstb = NULL;
	struct mtk_drm_dp_sideband_msg_req_body *msg = &up_req->msg;
	struct mtk_drm_dp_sideband_msg_hdr *hdr = &up_req->hdr;
	bool hotplug = false;

	if (hdr->broadcast) {
		const u8 *guid = NULL;

		if (msg->req_type == DP_CONNECTION_STATUS_NOTIFY)
			guid = msg->u.conn_stat.guid;
		else if (msg->req_type == DP_RESOURCE_STATUS_NOTIFY)
			guid = msg->u.resource_stat.guid;

		if (guid)
			mstb = mtk_drm_dp_get_mst_branch_device_by_guid(mgr, guid);
	} else {
		mstb = mtk_drm_dp_get_mst_branch_device(mgr, hdr->lct, hdr->rad);
	}

	if (!mstb) {
		DP_ERR("Got MST reply from unknown device %d\n", hdr->lct);
		if (hdr->lct == 1)
			mstb = mgr->mst_primary;
		else if (hdr->lct >= 2)
			mstb = &mtk_drm_dp->branch_debug_real[hdr->lct - 1];
		DP_ERR("Workaround allocate mst branch\n");
	}

	/* TODO: Add missing handler for DP_RESOURCE_STATUS_NOTIFY events */
	if (msg->req_type == DP_CONNECTION_STATUS_NOTIFY) {
		mtk_drm_dp_mst_handle_conn_stat(mstb, &msg->u.conn_stat);
		hotplug = true;
	}

	mtk_drm_dp_mst_topology_put_mstb(mstb);
	return hotplug;
}

static void mtk_drm_dp_mst_up_req_work(void)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = &mst_mtk_dp->mtk_mgr;
	struct mtk_drm_dp_pending_up_req *up_req;
	bool send_hotplug = false;

	while (true) {
		up_req = list_first_entry_or_null(&mgr->up_req_list,
						  struct mtk_drm_dp_pending_up_req,
						  next);
		if (up_req)
			list_del(&up_req->next);

		if (!up_req)
			break;

		send_hotplug |= mtk_drm_dp_mst_process_up_req(mgr, up_req);
		kfree(up_req);
	}

	//if (send_hotplug)
		//drm_kms_helper_hotplug_event(mgr->dev);
}

static int mtk_drm_dp_mst_handle_up_req(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	struct mtk_drm_dp_pending_up_req *up_req;

	DP_MSG("Handle Up Req\n");

	if (!mtk_drm_dp_get_one_sb_msg(mgr, true, NULL))
		goto out;

	if (!mgr->up_req_recv.have_eomt)
		return 0;

	up_req = kmalloc(sizeof(*up_req), GFP_KERNEL);
	DP_MSG("malloc for up_req with %lu\n",
	       sizeof(struct mtk_drm_dp_pending_up_req));
	memset(up_req, 0, sizeof(struct mtk_drm_dp_pending_up_req));
	if (!up_req)
		return -DPTX_STATUS_ERR;

	INIT_LIST_HEAD(&up_req->next);
	mtk_drm_dp->end_of_msg_trans_flag = false;//clear the flag

	mtk_drm_dp_sideband_parse_req(mgr, &mgr->up_req_recv, &up_req->msg);

	/*Clear RDY bit after got the sideband MSG*/
	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);

	if (up_req->msg.req_type != DP_CONNECTION_STATUS_NOTIFY &&
	    up_req->msg.req_type != DP_RESOURCE_STATUS_NOTIFY) {
		DP_ERR("Received unknown up req type, ignoring: %x\n",
		       up_req->msg.req_type);
		kfree(up_req);
		goto out;
	}

	mtk_drm_dp_send_up_ack_reply(mgr, mgr->mst_primary, up_req->msg.req_type,
				     false);

#if ENABLE_DPTX_DEBUG
	if (up_req->msg.req_type == DP_CONNECTION_STATUS_NOTIFY) {
		const struct mtk_drm_dp_connection_status_notify *conn_stat =
			&up_req->msg.u.conn_stat;

		DP_MSG("Got CSN: pn: %d ldps:%d ddps: %d mcs: %d ip: %d pdt: %d\n",
		       conn_stat->port_number,
			 conn_stat->legacy_device_plug_status,
			 conn_stat->displayport_device_plug_status,
			 conn_stat->message_capability_status,
			 conn_stat->input_port,
			 conn_stat->peer_device_type);
	} else if (up_req->msg.req_type == DP_RESOURCE_STATUS_NOTIFY) {
		const struct mtk_drm_dp_resource_status_notify *res_stat =
			&up_req->msg.u.resource_stat;

		DP_MSG("Got RSN: pn: %d avail_pbn %d\n",
		       res_stat->port_number,
			 res_stat->available_pbn);
	}
#endif
	up_req->hdr = mgr->up_req_recv.initial_hdr;
	list_add_tail(&up_req->next, &mgr->up_req_list);
	mtk_drm_dp_mst_up_req_work();

out:
	memset(&mgr->up_req_recv, 0, sizeof(struct mtk_drm_dp_sideband_msg_rx));
	return 0;
}

int mtk_drm_dp_mst_hpd_irq(struct mtk_drm_dp_mst_topology_mgr *mgr, u8 *esi, bool *handled)
{
	int ret = 0;
	int sc;

	*handled = false;
	sc = DP_GET_SINK_COUNT(esi[0]);

	if (sc != mgr->sink_count) {
		mgr->sink_count = sc;
		*handled = true;
	}

	if (esi[1] & DP_DOWN_REP_MSG_RDY) {
		ret = mtk_drm_dp_mst_handle_down_rep(mgr);
		*handled = true;
	}

	if (esi[1] & DP_UP_REQ_MSG_RDY) {
		ret |= mtk_drm_dp_mst_handle_up_req(mgr);
		*handled = true;
	}

	mtk_drm_dp_tx_work();
	return ret;
}

u8 *mtk_drm_dp_mst_get_edid(struct mtk_drm_dp_mst_topology_mgr *mgr,
			    struct mtk_drm_dp_mst_port *port)
{
	u8 *edid = NULL;

	/* we need to search for the port in the mgr in case it's gone */
	port = mtk_drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return NULL;

	if (port->cached_edid) {
		edid = kmalloc(DPTX_EDID_SIZE, GFP_KERNEL);
		memcpy(edid, port->cached_edid, DPTX_EDID_SIZE);
		DP_MSG("malloc for up_reqwith %d\n", DPTX_EDID_SIZE);
	} else {
		edid = mtk_dptx_drv_get_edid();
	}
	port->has_audio = true; //drm_detect_monitor_audio(edid);
	mtk_drm_dp_mst_topology_put_port(port);
	return edid;
}

int mtk_drm_dp_find_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
			       int pbn)
{
	int num_slots;

	num_slots = (pbn / mgr->pbn_div) + 1; //DIV_ROUND_UP(pbn, mgr->pbn_div);

	/* max. time slots - one slot for MTP header */
	if (num_slots > 63)
		return -DPTX_STATUS_ERR;
	return num_slots;
}

static int mtk_drm_dp_init_vcpi(struct mtk_drm_dp_mst_topology_mgr *mgr,
				struct mtk_drm_dp_vcpi *vcpi, int pbn, int slots)
{
	int ret;

	vcpi->pbn = pbn;
	vcpi->aligned_pbn = slots * mgr->pbn_div;
	vcpi->num_slots = slots;

	ret = mtk_drm_dp_mst_assign_payload_id(mgr, vcpi);
	if (ret < 0)
		return ret;
	return 0;
}

bool mtk_drm_dp_mst_allocate_vcpi(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  struct mtk_drm_dp_mst_port *port, int pbn, int slots)
{
	int ret;

	if (slots < 0)
		return false;

	port = mtk_drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return false;

	if (port->vcpi.vcpi > 0) {
		DP_MSG("payload: vcpi %d already allocated for pbn %d - requested pbn %d\n",
		       port->vcpi.vcpi, port->vcpi.pbn, pbn);
		if (pbn == port->vcpi.pbn) {
			mtk_drm_dp_mst_topology_put_port(port);
			return true;
		}
	}

	ret = mtk_drm_dp_init_vcpi(mgr, &port->vcpi, pbn, slots);
	if (ret) {
		DP_ERR("failed to init vcpi slots=%d ret=%d\n",
		       ((pbn / mgr->pbn_div) + 1), ret);
			 //DIV_ROUND_UP(pbn, mgr->pbn_div), ret);
		mtk_drm_dp_mst_topology_put_port(port);
		goto out;
	}
	DP_MSG("initing vcpi %d for pbn=%d slots=%d\n",
	       port->vcpi.vcpi, pbn, port->vcpi.num_slots);

	/* Keep port allocated until its payload has been removed */
	mtk_drm_dp_mst_get_port_malloc(port);
	mtk_drm_dp_mst_topology_put_port(port);
	return true;
out:
	return false;
}

int mtk_drm_dp_mst_get_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  struct mtk_drm_dp_mst_port *port)
{
	int slots = 0;

	port = mtk_drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return slots;

	slots = port->vcpi.num_slots;
	mtk_drm_dp_mst_topology_put_port(port);
	return slots;
}

void mtk_drm_dp_mst_reset_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
				     struct mtk_drm_dp_mst_port *port)
{
	port->vcpi.num_slots = 0;
}

void mtk_drm_dp_mst_deallocate_vcpi(struct mtk_drm_dp_mst_topology_mgr *mgr,
				    struct mtk_drm_dp_mst_port *port)
{
	bool skip;

	if (!port->vcpi.vcpi)
		return;

	skip = !mtk_drm_dp_mst_port_downstream_of_branch(port, mgr->mst_primary);

	if (skip)
		return;

	mtk_drm_dp_mst_put_payload_id(mgr, port->vcpi.vcpi);
	port->vcpi.num_slots = 0;
	port->vcpi.pbn = 0;
	port->vcpi.aligned_pbn = 0;
	port->vcpi.vcpi = 0;
	mtk_drm_dp_mst_put_port_malloc(port);
}

int mtk_drm_dp_dpcd_write_payload(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  int id, struct mtk_drm_dp_payload *payload)
{
	u8 payload_alloc[3], status;
	int ret;
	int retries = 0;

	status = DP_PAYLOAD_TABLE_UPDATED;
	drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status, 1);

	payload_alloc[0] = id;
	payload_alloc[1] = payload->start_slot;
	payload_alloc[2] = payload->num_slots;

	ret = drm_dp_dpcd_write(&mst_mtk_dp->aux, DP_PAYLOAD_ALLOCATE_SET, payload_alloc, 3);
	if (!ret)
		DP_ERR("failed to write payload allocation %d\n", ret);

retry:
	ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status, 1);
	if (ret == 0) {
		DP_ERR("failed to read payload table status %d\n", ret);
		goto fail;
	}

	if (!(status & DP_PAYLOAD_TABLE_UPDATED)) {
		retries++;
		if (retries < 20) {
			mdelay(20);
			goto retry;
		}
		DP_ERR("status not set after read payload table status %d\n",
		       status);
		ret = -DPTX_STATUS_ERR;
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int do_get_act_status(void)
{
	int ret;
	u8 status;

	ret = drm_dp_dpcd_read(&mst_mtk_dp->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status, 1);
	if (ret == 0)
		return ret;

	return status;
}

int mtk_drm_dp_check_act_status(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	const int timeout_us = 3000000;
	int status, i;

	for (i = 0; i < timeout_us; i += 200) {
		usleep_range(200, 201);
		status = do_get_act_status();

		if (status & DP_PAYLOAD_ACT_HANDLED || status < 0)
			break;
	}

	if (i > timeout_us && status >= 0) {
		DP_ERR("Failed to get ACT after %dms, last status: %02x\n",
		       timeout_us, status);
		return -DPTX_STATUS_ERR;
	} else if (status < 0) {
		/* Failure here isn't unexpected - the hub may have just been unplugged */
		DP_ERR("Failed to read payload table status: %d\n", status);
		return status;
	}

	return status;
}

int mtk_drm_dp_calc_pbn_mode(int clock, int bpp, bool dsc)
{
	u32 pbn;

	if (dsc)
		pbn = (clock / 8) * (bpp / 16);
	else
		pbn = (clock / 8) * bpp;

	if (mst_mtk_dp->is_mst_fec_en) {
		pbn = (pbn * 1030);
		DP_DBG("Add %u\n", pbn);
	} else {
		pbn = (pbn * 1006);
	}

	pbn = (pbn / 54) * 64;
	pbn = (pbn + (1000000 - 1)) / 1000000;

	return pbn + 1; // roundup
}

void wake_up_all_work(void)
{
	mtk_drm_dp_tx_work();
	mtk_drm_dp_delayed_destroy_work();
	mtk_drm_dp_mst_link_probe_work();
	mtk_drm_dp_mst_up_req_work();
}

void mtk_drm_dp_tx_work(void)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = &mst_mtk_dp->mtk_mgr;

	if (!list_empty(&mgr->tx_msg_downq))
		process_single_down_tx_qlock(mgr);
}

void
mtk_drm_dp_delayed_destroy_port(struct mtk_drm_dp_mst_port *port)
{
	mtk_drm_dp_port_set_pdt(port, DP_PEER_DEVICE_NONE, port->mcs);
	mtk_drm_dp_mst_put_port_malloc(port);
}

void
mtk_drm_dp_delayed_destroy_mstb(struct mtk_drm_dp_mst_branch *mstb)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct mtk_drm_dp_mst_port *port, *port_tmp;
	struct mtk_drm_dp_sideband_msg_tx *txmsg, *txmsg_tmp;
	bool wake_tx = false;

	list_for_each_entry_safe(port, port_tmp, &mstb->ports, next) {
		list_del(&port->next);
		mtk_drm_dp_mst_topology_put_port(port);
	}

	/* drop any tx slot msg */
	list_for_each_entry_safe(txmsg, txmsg_tmp, &mgr->tx_msg_downq, next) {
		if (txmsg->dst != mstb)
			continue;

		txmsg->state = DRM_DP_SIDEBAND_TX_TIMEOUT;
		list_del(&txmsg->next);
		wake_tx = true;
	}

	if (wake_tx)
		wake_up_all_work();

	mtk_drm_dp_mst_put_mstb_malloc(mstb);
}

void mtk_drm_dp_delayed_destroy_work(void)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = &mst_mtk_dp->mtk_mgr;
	bool go_again; //, send_hotplug = false;

	do {
		go_again = false;

		for (;;) {
			struct mtk_drm_dp_mst_branch *mstb;

			mstb = list_first_entry_or_null(&mgr->destroy_branch_device_list,
							struct mtk_drm_dp_mst_branch,
							destroy_next);
			if (mstb)
				list_del(&mstb->destroy_next);

			if (!mstb)
				break;

			mtk_drm_dp_delayed_destroy_mstb(mstb);
			go_again = true;
		}

		for (;;) {
			struct mtk_drm_dp_mst_port *port;

			port = list_first_entry_or_null(&mgr->destroy_port_list,
							struct mtk_drm_dp_mst_port,
							next);
			if (port)
				list_del(&port->next);

			if (!port)
				break;

			mtk_drm_dp_delayed_destroy_port(port);
			//send_hotplug = true;
			go_again = true;
		}
	} while (go_again);

	//if (send_hotplug)
		//1 TBD: drm_kms_helper_hotplug_event(mgr->dev);
}

bool
mtk_drm_dp_mst_port_downstream_of_branch(struct mtk_drm_dp_mst_port *port,
					 struct mtk_drm_dp_mst_branch *branch)
{
	while (port->parent) {
		if (port->parent == branch)
			return true;

		if (port->parent->port_parent)
			port = port->parent->port_parent;
		else
			break;
	}
	return false;
}

int mtk_drm_dp_mst_topology_mgr_init(struct mtk_dp *mtk_dp, struct mtk_drm_dp_mst_topology_mgr *mgr,
				     int max_dpcd_transaction_bytes, int max_payloads,
				 int max_lane_count, int max_link_rate,
				 int conn_base_id)
{
	struct mtk_drm_dp_mst_topology_state *mst_state;

	mst_mtk_dp = mtk_dp;

	INIT_LIST_HEAD(&mgr->tx_msg_downq);
	INIT_LIST_HEAD(&mgr->destroy_port_list);
	INIT_LIST_HEAD(&mgr->destroy_branch_device_list);
	INIT_LIST_HEAD(&mgr->up_req_list);

	mgr->max_dpcd_transaction_bytes = max_dpcd_transaction_bytes;
	mgr->max_payloads = max_payloads;
	mgr->max_lane_count = max_lane_count;
	mgr->max_link_rate = max_link_rate;
	mgr->conn_base_id = conn_base_id;

	if (max_payloads + 1 > (int)sizeof(mgr->payload_mask) * 8 ||
	    max_payloads + 1 > (int)sizeof(mgr->vcpi_mask) * 8)
		return -DPTX_STATUS_ERR;
#if (DPTX_MST_DEBUG == 0x1)
	memset(&mtk_drm_dp->payload[0], 0, sizeof(struct mtk_drm_dp_payload));
#else
	mgr->payloads = kmalloc(sizeof(*mgr->payloads), GFP_KERNEL);
	if (!mgr->payloads)
		return -DPTX_STATUS_ERR;
	mgr->proposed_vcpis = kmalloc(sizeof(struct mtk_drm_dp_vcpi *), GFP_KERNEL);
	if (!mgr->proposed_vcpis)
		return -DPTX_STATUS_ERR;
#endif

	mgr->payload_mask = 0;

	mst_state = kmalloc(sizeof(*mst_state), GFP_KERNEL);
		DP_MSG("malloc for mst_state with %lu\n",
		       sizeof(struct mtk_drm_dp_mst_topology_state));

	memset(mst_state, 0, sizeof(struct mtk_drm_dp_mst_topology_state));
	if (!mst_state)
		return -DPTX_STATUS_ERR;

	mst_state->total_avail_slots = 63;
	mst_state->start_slot = DP_PAYLOAD_START_SLOT;
	mst_state->mgr = mgr;
	INIT_LIST_HEAD(&mst_state->vcpis);

	return 0;
}

void mtk_drm_dp_mst_topology_mgr_destroy(struct mtk_drm_dp_mst_topology_mgr *mgr)
{
	mtk_drm_dp_mst_topology_mgr_set_mst(mgr, false);

#if (DPTX_MST_DEBUG == 0x1)
#else
	if (mgr) {
		kfree(mgr->payloads);
		mgr->payloads = NULL;
		kfree(mgr->proposed_vcpis);
		mgr->proposed_vcpis = NULL;
	}
#endif
}

u8 *mtk_dptx_drv_get_edid(void)
{
	u8 retry = 5;
	u8 *edid = NULL;
	struct drm_connector *connector;

	if (mst_mtk_dp->conn)
		connector = mst_mtk_dp->conn;
	else if (mst_mtk_dp->mtk_connector[0])
		connector = &mst_mtk_dp->mtk_connector[0]->connector;

	while (retry--) {
		if (mtk_dp_handle_edid(mst_mtk_dp, connector)) //to check
			break;
	}

	if (mst_mtk_dp->edid) {
		edid = kmalloc(DPTX_EDID_SIZE, GFP_KERNEL);
		memcpy(edid, mst_mtk_dp->edid, DPTX_EDID_SIZE);
		DP_MSG("malloc for get edid with %d\n", DPTX_EDID_SIZE);
	}

	return edid;
}

int mtk_drm_dp_mst_i2c_read(struct mtk_drm_dp_mst_branch *mstb,
			    struct mtk_drm_dp_mst_port *port,
			       struct i2c_msg *msgs, int num)
{
	struct mtk_drm_dp_mst_topology_mgr *mgr = port->mgr;
	struct mtk_drm_dp_sideband_msg_req_body msg;
	struct mtk_drm_dp_sideband_msg_tx *txmsg = NULL;
	int i, ret;

	memset(&msg, 0, sizeof(msg));
	msg.req_type = DP_REMOTE_I2C_READ;
	msg.u.i2c_read.num_transactions = num - 1;
	msg.u.i2c_read.port_number = port->port_num;
	for (i = 0; i < num - 1; i++) {
		msg.u.i2c_read.transactions[i].i2c_dev_id = msgs[i].addr;
		msg.u.i2c_read.transactions[i].num_bytes = msgs[i].len;
		msg.u.i2c_read.transactions[i].bytes = msgs[i].buf;
		msg.u.i2c_read.transactions[i].no_stop_bit = !(msgs[i].flags & I2C_M_STOP);
	}
	msg.u.i2c_read.read_i2c_device_id = msgs[num - 1].addr;
	msg.u.i2c_read.num_bytes_read = msgs[num - 1].len;

	txmsg = kmalloc(sizeof(*txmsg), GFP_KERNEL);

	memset(txmsg, 0, sizeof(struct mtk_drm_dp_sideband_msg_tx));

	if (!txmsg) {
		ret = -DPTX_STATUS_ERR;
		goto out;
	}

	txmsg->dst = mstb;
	mtk_drm_dp_encode_sideband_req(&msg, txmsg);
	mtk_drm_dp_queue_down_tx(mgr, txmsg);

	mtk_dp_mst_drv_sideband_msg_irq_clear(mst_mtk_dp);
	ret = mtk_drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
			ret = -DPTX_STATUS_ERR;
			goto out;
		}
		if (txmsg->reply.u.remote_i2c_read_ack.num_bytes != msgs[num - 1].len) {
			ret = -DPTX_STATUS_ERR;
			goto out;
		}
		memcpy(msgs[num - 1].buf, txmsg->reply.u.remote_i2c_read_ack.bytes,
		       msgs[num - 1].len);
		ret = num;
	}
out:
	kfree(txmsg);
	return ret;
}

void mtk_drm_dp_mst_init(void)
{
	memset(mtk_drm_dp->port_debug_real, 0x0,
	       DP_LCT_MAX  * DP_PORT_NUM_MAX * sizeof(struct mtk_drm_dp_mst_port));
	memset(mtk_drm_dp->branch_debug_real, 0x0,
	       DP_LCT_MAX * sizeof(struct mtk_drm_dp_mst_branch));
}

struct mtk_drm_dp_payload *mtk_drm_dp_mst_get_payload(int idx)
{
	return &mtk_drm_dp->payload[idx];
}
#endif
