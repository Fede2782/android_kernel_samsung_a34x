/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Richtek Technology Corp.
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#ifndef __LINUX_UFCS_CLASS_H__
#define __LINUX_UFCS_CLASS_H__

struct ufcs_port;

enum ufcs_hard_reset_type {
	UFCS_HARD_RESET_TO_SRC = 0,
	UFCS_HARD_RESET_TO_CABLE = 1,
};

enum ufcs_baud_rate {
	UFCS_BAUD_RATE_115200 = 0,
	UFCS_BAUD_RATE_57600,
	UFCS_BAUD_RATE_38400,
	UFCS_BAUD_RATE_MAX
};

enum ufcs_transmit_status {
	UFCS_TX_SUCCESS = 0,
	UFCS_TX_FAIL = 1,
};

#define UFCS_MAX_PAYLOAD		61

#define UFCS_HEADER_ROLE_SHIFT		13
#define UFCS_HEADER_ROLE_MASK		GENMASK(2, 0)
#define UFCS_HEADER_ID_SHIFT		9
#define UFCS_HEADER_ID_MASK		GENMASK(3, 0)
#define UFCS_HEADER_REV_SHIFT		3
#define UFCS_HEADER_REV_MASK		GENMASK(5, 0)
#define UFCS_HEADER_TYPE_SHIFT		0
#define UFCS_HEADER_TYPE_MASK		GENMASK(2, 0)

#define MAX_SRC_CAP_CNT			7
#define MAX_EMARK_BYTE_CNT		10

enum ufcs_role {
	UFCS_SRC = 1,
	UFCS_SINK = 2,
	UFCS_EMARK = 3,
};

struct ufcs_ctrl_payload {
	u8 command;
} __packed;

struct ufcs_data_payload {
	u8 command;
	u8 datalen;
	union {
		__be64 payload64[7];
		__be32 payload32;
		__be16 payload16;
		u8 payload[59];
	};
} __packed;

struct ufcs_vdm_payload {
	__be16 vendor;
	u8 datalen;
	u8 payload[58];
} __packed;

struct ufcs_message {
	__be16 header;
	union {
		struct ufcs_ctrl_payload ctrl;
		struct ufcs_data_payload data;
		struct ufcs_vdm_payload vdm;
		u8 payload[UFCS_MAX_PAYLOAD];
	};
} __packed;

enum ufcs_dpm_request {
	UFCS_DPM_NONE = 0,
	UFCS_DPM_POWER_REQUEST = 1,
	UFCS_DPM_VERIFY_REQUEST = 2,
	UFCS_DPM_CABLE_INFO = 3,
	UFCS_DPM_SRC_DEVICE_INFO = 4,
	UFCS_DPM_SRC_INFO = 5,
	UFCS_DPM_ERROR_INFO = 6,
	UFCS_DPM_VDM = 7,
	UFCS_DPM_SRC_CAP = 8,
	UFCS_DPM_EXIT_UFCS_MODE = 9,
};

union ufcs_dpm_input {
	/* Power request input */
	struct {
		u32 req_millivolt;
		u32 req_milliamp;
	};
	/* Verify request input */
	struct {
		u8 random[16];
		u8 *key;
		size_t key_len;
		u8 key_id;
	};
};

struct ufcs_src_cap {
	u32 min_mV;
	u32 max_mV;
	u32 min_mA;
	u32 max_mA;
};

union ufcs_dpm_output {
	/* Power request result */
	bool power_request_ready;
	/* Verify request result */
	struct {
		u8 secure[32];
		u8 random[16];
		bool verify_request_pass;
	};
	/* Cable info result */
	struct {
		u16 cbl_vid1;
		u16 cbl_vid2;
		u16 cbl_mOhm;
		u32 cbl_mVolt;
		u32 cbl_mAmp;
	};
	/* Src device info result */
	struct {
		u16 hw_ver;
		u16 sw_ver;
	};
	/* Src info result */
	struct {
		s16 device_temp;
		s16 conn_temp;
		u16 output_millivolt;
		u16 output_milliamp;
	};
	/* Error info result */
	struct {
		u32 output_ovp:1;
		u32 output_uvp:1;
		u32 output_ocp:1;
		u32 output_scp:1;
		u32 usb_conn_otp:1;
		u32 device_otp:1;
		u32 cc_ovp:1;
		u32 dm_ovp:1;
		u32 dp_ovp:1;
		u32 input_ovp:1;
		u32 input_uvp:1;
		u32 leak_overspec:1;
		u32 input_drop:1;
		u32 crc_abnormal_output_disable:1;
		u32 wdt_overflow:1;
		u32 rsv:17;
	};
	struct {
		struct ufcs_src_cap src_cap[MAX_SRC_CAP_CNT];
		u32 src_cap_cnt;
	};
	/* Raw if needed */
	u32 raw32;
};

struct ufcs_dev {
	int (*init)(struct ufcs_dev *dev);
	int (*enable)(struct ufcs_dev *dev, bool enable);
	int (*config_baud_rate)(struct ufcs_dev *dev, enum ufcs_baud_rate rate);
	int (*transmit)(struct ufcs_dev *dev, const struct ufcs_message *msg, u8 msglen);
	int (*send_hard_reset)(struct ufcs_dev *dev, enum ufcs_hard_reset_type type);
	int (*config_tx_hiz)(struct ufcs_dev *dev, bool enable);
};

extern struct class *ufcs_class;

enum ufcs_notify {
	UFCS_NOTIFY_ATTACH_NONE = 0,
	UFCS_NOTIFY_ATTACH_FAIL,
	UFCS_NOTIFY_ATTACH_PASS,
};

#if IS_ENABLED(CONFIG_UFCS_CLASS)
extern void ufcs_tx_complete(struct ufcs_port *port, enum ufcs_transmit_status status);
extern void ufcs_rx_receive(struct ufcs_port *port, const struct ufcs_message *msg);
extern void ufcs_attach_change(struct ufcs_port *port, bool dcp_attached);
extern void ufcs_hard_reset(struct ufcs_port *port);
extern void ufcs_hand_shake_state(struct ufcs_port *port, bool success);
/* User DPM request API */
extern struct ufcs_port *ufcs_port_get_by_name(const char *name);
extern void ufcs_port_put(struct ufcs_port *port);
extern int register_ufcs_dev_notifier(struct ufcs_port *port, struct notifier_block *nb);
extern int unregister_ufcs_dev_notifier(struct ufcs_port *port, struct notifier_block *nb);
extern int ufcs_port_dpm_reaction(struct ufcs_port *port, enum ufcs_dpm_request request,
			   union ufcs_dpm_input *input, union ufcs_dpm_output *output);
extern struct ufcs_port *ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs);
extern void ufcs_unregister_port(struct ufcs_port *port);
extern struct ufcs_port *devm_ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs);
#else
static inline void ufcs_tx_complete(struct ufcs_port *port, enum ufcs_transmit_status status) {}
static inline void ufcs_rx_receive(struct ufcs_port *port, const struct ufcs_message *msg) {}
static inline void ufcs_attach_change(struct ufcs_port *port, bool dcp_attached) {}
static inline void ufcs_hard_reset(struct ufcs_port *port) {}
static inline void ufcs_hand_shake_state(struct ufcs_port *port, bool success) {}
/* User DPM request API */
static inline void ufcs_port_put(struct ufcs_port *port) {}
static inline struct ufcs_port *ufcs_port_get_by_name(const char *name) { return ERR_PTR(-EINVAL); }
static inline int register_ufcs_dev_notifier(struct ufcs_port *port,
	struct notifier_block *nb) { return -EOPNOTSUPP; }
static inline int unregister_ufcs_dev_notifier(struct ufcs_port *port,
	struct notifier_block *nb) { return -EOPNOTSUPP; }
static inline int ufcs_port_dpm_reaction(struct ufcs_port *port, enum ufcs_dpm_request request,
	union ufcs_dpm_input *input, union ufcs_dpm_output *output) { return -EOPNOTSUPP; };
static inline  struct ufcs_port *ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs)
{
	return ERR_PTR(-EINVAL);
}
static inline void ufcs_unregister_port(struct ufcs_port *port) {}
static inline struct ufcs_port *devm_ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs)
{
	return ERR_PTR(-EINVAL);
}
#endif

#if IS_ENABLED (CONFIG_OF) && IS_ENABLED(CONFIG_UFCS_CLASS)
extern struct ufcs_port *ufcs_port_get_by_phandle(struct device_node *np, const char *property);
extern struct ufcs_port *devm_ufcs_port_get_by_phandle(struct device *dev, const char *property);
#else /* !IS_ENABLED (CONFIG_OF) || !IS_ENABLED(CONFIG_UFCS_CLASS) */
static inline struct ufcs_port *ufcs_port_get_by_phandle(struct device_node *np,
	const char *property) { return ERR_PTR(-EINVAL); }
static inline struct ufcs_port *devm_ufcs_port_get_by_phandle(struct device *dev,
	const char *property) { return ERR_PTR(-EINVAL); }
#endif

#endif /* __LINUX_UFCS_CLASS_H__ */

