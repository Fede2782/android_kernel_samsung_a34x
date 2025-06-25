/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DRM_DP_MST_DRV_H__
#define __MTK_DRM_DP_MST_DRV_H__

#include "mtk_drm_dp_common.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DP_MST_SUPPORT)
#define DP_LCT_MAX	(DP_STREAM_MAX * 2)
#define DP_PORT_NUM_MAX	(DP_MST_LOGICAL_PORT_0 + DP_STREAM_MAX)
#define DPTX_DRM_UNUSED_FUNC 0
#define DPTX_EDID_SIZE		0x200
#define DPTX_PHY_HPD_INT_EVNET	HPD_INT_EVNET
#define ENABLE_DPTX_DEBUG 0
#define DP_PAYLOAD_START_SLOT 1

#define DP_DOWNSTREAMPORT_PRESENT		0x005
#define DP_DWN_STRM_PORT_TYPE_MASK		0x06
#define DP_DWN_STRM_PORT_TYPE_DP		(0 << 1)
#define DP_DWN_STRM_PORT_TYPE_TMDS		(2 << 1)
#define DP_DWN_STRM_PORT_TYPE_OTHER		(3 << 1)

#define DP_MAX_DOWNSTREAM_PORTS			0x10

/* DFP Capability Extension */
#define DP_DFP_CAPABILITY_EXTENSION_SUPPORT	0x0a3	 /* 2.0 */

#define DP_PAYLOAD_ALLOCATE_START_TIME_SLO	DPCD_001C1

/* DP 1.2 Sideband message defines */
/* peer device type - DP 1.2a Table 2-92 */
#define DP_PEER_DEVICE_NONE		0x0
#define DP_PEER_DEVICE_SOURCE_OR_SST	0x1
#define DP_PEER_DEVICE_MST_BRANCHING	0x2
#define DP_PEER_DEVICE_SST_SINK		0x3
#define DP_PEER_DEVICE_DP_LEGACY_CONV	0x4

/* DP 1.2 MST sideband request names DP 1.2a Table 2-80 */
#define DP_GET_MSG_TRANSACTION_VERSION	0x00 /* DP 1.3 */
#define DP_LINK_ADDRESS			0x01
#define DP_CONNECTION_STATUS_NOTIFY	0x02
#define DP_ENUM_PATH_RESOURCES		0x10
#define DP_ALLOCATE_PAYLOAD		0x11
#define DP_QUERY_PAYLOAD		0x12
#define DP_RESOURCE_STATUS_NOTIFY	0x13
#define DP_CLEAR_PAYLOAD_ID_TABLE	0x14
#define DP_REMOTE_DPCD_READ		0x20
#define DP_REMOTE_DPCD_WRITE		0x21
#define DP_REMOTE_I2C_READ		0x22
#define DP_REMOTE_I2C_WRITE		0x23
#define DP_POWER_UP_PHY			0x24
#define DP_POWER_DOWN_PHY		0x25
#define DP_SINK_EVENT_NOTIFY		0x30
#define DP_QUERY_STREAM_ENC_STATUS	0x38
#define DP_QUERY_STREAM_ENC_STATUS_STATE_NO_EXIST	0
#define DP_QUERY_STREAM_ENC_STATUS_STATE_INACTIVE	1
#define DP_QUERY_STREAM_ENC_STATUS_STATE_ACTIVE		2

/* DP 1.2 MST sideband reply types */
#define DP_SIDEBAND_REPLY_ACK		0x00
#define DP_SIDEBAND_REPLY_NAK		0x01

/* DP 1.2 MST sideband nak reasons - table 2.84 */
#define DP_NAK_WRITE_FAILURE		0x01
#define DP_NAK_INVALID_READ		0x02
#define DP_NAK_CRC_FAILURE		0x03
#define DP_NAK_BAD_PARAM		0x04
#define DP_NAK_DEFER			0x05
#define DP_NAK_LINK_FAILURE		0x06
#define DP_NAK_NO_RESOURCES		0x07
#define DP_NAK_DPCD_FAIL		0x08
#define DP_NAK_I2C_NAK			0x09
#define DP_NAK_ALLOCATE_FAIL		0x0a

/* DP 1.2 MST PORTs - Section 2.5.1 v1.2a spec */
#define DP_MST_PHYSICAL_PORT_0		0
#define DP_MST_LOGICAL_PORT_0		8
#define DP_RECEIVER_CAP_SIZE		0xf

// i2c define
#define I2C_M_RD		0x0001	/* guaranteed to be 0x0001! */
#define I2C_M_TEN		0x0010	/* use only if I2C_FUNC_10BIT_ADDR */
#define I2C_M_DMA_SAFE		0x0200	/* use only in kernel space */
#define I2C_M_RECV_LEN		0x0400	/* use only if I2C_FUNC_SMBUS_READ_BLOCK_DATA */
#define I2C_M_NO_RD_ACK		0x0800	/* use only if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* use only if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR	0x2000	/* use only if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* use only if I2C_FUNC_NOSTART */
#define I2C_M_STOP		0x8000	/* use only if I2C_FUNC_PROTOCOL_MANGLING */

enum MTK_DPTX_STATUS {
	DPTX_STATUS_OK = 0,
	DPTX_STATUS_ERR = 1,
	DPTX_STATUS_DONE = 2,
};

struct mtk_drm_dp {
	struct mtk_drm_dp_vcpi *propose_vcpis[DP_PAYLOAD_MAX];
	struct mtk_drm_dp_payload payload[DP_PAYLOAD_MAX];
	struct mtk_drm_dp_mst_branch *mstb[DP_LCT_MAX];
	u8 end_of_msg_trans_flag;
	u8 current_request_type;
	struct mtk_drm_dp_mst_port port_debug_real[DP_LCT_MAX][DP_PORT_NUM_MAX];
	struct mtk_drm_dp_mst_branch branch_debug_real[DP_LCT_MAX];
};

void mtk_drm_dp_encode_sideband_req(const struct mtk_drm_dp_sideband_msg_req_body *req,
				    struct mtk_drm_dp_sideband_msg_tx *raw);
void mtk_drm_dp_mst_put_mstb_malloc(struct mtk_drm_dp_mst_branch *mstb);
void
mtk_drm_dp_mst_topology_put_mstb(struct mtk_drm_dp_mst_branch *mstb);

struct mtk_drm_dp_mst_branch *
mtk_drm_dp_mst_topology_get_mstb_validated(struct mtk_drm_dp_mst_topology_mgr *mgr,
					   struct mtk_drm_dp_mst_branch *mstb);

bool mtk_drm_dp_mst_is_end_device(u8 pdt, bool mcs);

ssize_t mtk_drm_dp_mst_dpcd_read(struct mtk_drm_dp_mst_port *port,
				 u32 offset, void *buffer, size_t size);

ssize_t mtk_drm_dp_mst_dpcd_write(struct mtk_drm_dp_mst_port *port,
				  u32 offset, void *buffer, size_t size);

void mtk_drm_dp_mst_link_probe_work(void);

bool mtk_drm_dp_validate_guid(struct mtk_drm_dp_mst_topology_mgr *mgr, u8 *guid);

int mtk_drm_dp_send_link_address(struct mtk_drm_dp_mst_topology_mgr *mgr,
				 struct mtk_drm_dp_mst_branch *mstb);

void
mtk_drm_dp_send_clear_payload_id_table(struct mtk_drm_dp_mst_topology_mgr *mgr,
				       struct mtk_drm_dp_mst_branch *mstb);

int
mtk_drm_dp_send_enum_path_resources(struct mtk_drm_dp_mst_topology_mgr *mgr,
				    struct mtk_drm_dp_mst_branch *mstb,
				struct mtk_drm_dp_mst_port *port);

int mtk_drm_dp_send_power_updown_phy(struct mtk_drm_dp_mst_topology_mgr *mgr,
				     struct mtk_drm_dp_mst_port *port, bool power_up);

int mtk_drm_dp_send_query_stream_enc_status(struct mtk_drm_dp_mst_topology_mgr *mgr,
					    struct mtk_drm_dp_mst_port *port,
		struct mtk_drm_dp_query_stream_enc_status_ack_reply *status);

int mtk_drm_dp_update_payload_part1(struct mtk_drm_dp_mst_topology_mgr *mgr, int start_slot);

int mtk_drm_dp_update_payload_part2(struct mtk_drm_dp_mst_topology_mgr *mgr);

int mtk_drm_dp_send_dpcd_read(struct mtk_drm_dp_mst_topology_mgr *mgr,
			      struct mtk_drm_dp_mst_port *port,
				 int offset, int size, u8 *bytes);

int mtk_drm_dp_send_dpcd_write(struct mtk_drm_dp_mst_topology_mgr *mgr,
			       struct mtk_drm_dp_mst_port *port,
				  int offset, int size, u8 *bytes);

int mtk_drm_dp_get_vc_payload_bw(const struct mtk_drm_dp_mst_topology_mgr *mgr,
				 int link_rate, int link_lane_count);

bool mtk_drm_dp_read_mst_cap(const u8 dpcd[DP_RECEIVER_CAP_SIZE]);

int mtk_drm_dp_mst_topology_mgr_set_mst(struct mtk_drm_dp_mst_topology_mgr *mgr, bool mst_state);

int mtk_drm_dp_mst_topology_mgr_resume(struct mtk_drm_dp_mst_topology_mgr *mgr, bool sync);

int mtk_drm_dp_mst_hpd_irq(struct mtk_drm_dp_mst_topology_mgr *mgr, u8 *esi, bool *handled);

int mtk_drm_dp_mst_detect_port(struct mtk_drm_dp_mst_topology_mgr *mgr,
			       struct mtk_drm_dp_mst_port *port);

u8 *mtk_drm_dp_mst_get_edid(struct mtk_drm_dp_mst_topology_mgr *mgr,
			    struct mtk_drm_dp_mst_port *port);

int mtk_drm_dp_find_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
			       int pbn);

void mtk_drm_dp_mst_update_slots(struct mtk_drm_dp_mst_topology_state *mst_state,
				 u8 link_encoding_cap);

bool mtk_drm_dp_mst_allocate_vcpi(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  struct mtk_drm_dp_mst_port *port, int pbn, int slots);

int mtk_drm_dp_mst_get_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  struct mtk_drm_dp_mst_port *port);

void mtk_drm_dp_mst_reset_vcpi_slots(struct mtk_drm_dp_mst_topology_mgr *mgr,
				     struct mtk_drm_dp_mst_port *port);

void mtk_drm_dp_mst_deallocate_vcpi(struct mtk_drm_dp_mst_topology_mgr *mgr,
				    struct mtk_drm_dp_mst_port *port);

int mtk_drm_dp_dpcd_write_payload(struct mtk_drm_dp_mst_topology_mgr *mgr,
				  int id, struct mtk_drm_dp_payload *payload);

int mtk_drm_dp_check_act_status(struct mtk_drm_dp_mst_topology_mgr *mgr);

int mtk_drm_dp_calc_pbn_mode(int clock, int bpp, bool dsc);

void mtk_drm_dp_mst_dump_topology(struct mtk_drm_dp_mst_topology_mgr *mgr);

void wake_up_all_work(void);

void mtk_drm_dp_tx_work(void);

void
mtk_drm_dp_delayed_destroy_port(struct mtk_drm_dp_mst_port *port);

void
mtk_drm_dp_delayed_destroy_mstb(struct mtk_drm_dp_mst_branch *mstb);

void mtk_drm_dp_delayed_destroy_work(void);

bool
mtk_drm_dp_mst_port_downstream_of_branch(struct mtk_drm_dp_mst_port *port,
					 struct mtk_drm_dp_mst_branch *branch);

int
mtk_drm_dp_mst_atomic_check_port_bw_limit(struct mtk_drm_dp_mst_port *port,
					  struct mtk_drm_dp_mst_topology_state *state);

int mtk_drm_dp_mst_topology_mgr_init(struct mtk_dp *mtk_dp, struct mtk_drm_dp_mst_topology_mgr *mgr,
				     int max_dpcd_transaction_bytes, int max_payloads,
				 int max_lane_count, int max_link_rate,
				 int conn_base_id);

void mtk_drm_dp_mst_topology_mgr_destroy(struct mtk_drm_dp_mst_topology_mgr *mgr);

u8 *mtk_dptx_drv_get_edid(void);

int mtk_drm_dp_mst_i2c_read(struct mtk_drm_dp_mst_branch *mstb,
			    struct mtk_drm_dp_mst_port *port,
			       struct i2c_msg *msgs, int num);

void mtk_drm_dp_mst_init(void);

struct mtk_drm_dp_payload *mtk_drm_dp_mst_get_payload(int idx);
#endif
#endif
