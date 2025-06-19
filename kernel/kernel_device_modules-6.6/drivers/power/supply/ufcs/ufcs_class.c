// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Richtek Technology Corp.
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Universal Fast Charging Spec port class
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <asm/unaligned.h>
#include <crypto/hash.h>
#include <linux/atomic.h>
#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb.h>

#include "ufcs_class.h"

static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);

#define UFCS_T_WAIT_HAND_SHAKE	120
#define UFCS_T_SENDER_RESPONSE	100
#define UFCS_T_POWER_SUPPLY	550
#define UFCS_T_TX_TIMEOUT	300
#define UFCS_T_RESTART_TRANS	1000
#define UFCS_T_DPM_REACTION	1000
#define UFCS_T_VERIFY_RESPONSE	500
#define UFCS_T_EXIT_UFCS_MODE	200

#define UFCS_REV_1_0	1

#define UFCS_HEADER(role, msgid, spec_rev, type)			\
	(((role & UFCS_HEADER_ROLE_MASK) << UFCS_HEADER_ROLE_SHIFT) |	\
	 ((msgid & UFCS_HEADER_ID_MASK) << UFCS_HEADER_ID_SHIFT) |	\
	 ((spec_rev & UFCS_HEADER_REV_MASK) << UFCS_HEADER_REV_SHIFT) |	\
	 ((type & UFCS_HEADER_TYPE_MASK) << UFCS_HEADER_TYPE_SHIFT))

#define UFCS_HEADER_BE(role, msgid, spec_rev, type) \
	cpu_to_be16(UFCS_HEADER(role, msgid, spec_rev, type))

#define UFCS_ATTACH_EVENT	BIT(0)
#define UFCS_RESET_EVENT	BIT(1)
#define UFCS_HS_SUCCESS_EVENT	BIT(2)
#define UFCS_HS_FAIL_EVENT	BIT(3)
#define UFCS_DPM_EVENT		BIT(4)

#define LOG_BUFFER_ENTRIES	1024
#define LOG_BUFFER_ENTRY_SIZE	128

#define UFCS_CTRL_HEADER_SIZE		1
#define UFCS_DATA_HEADER_SIZE		2
#define UFCS_VDM_HEADER_SIZE		3

enum ufcs_ctrl_cmd {
	UFCS_CTRL_PING = 0,
	UFCS_CTRL_ACK = 1,
	UFCS_CTRL_NCK = 2,
	UFCS_CTRL_ACCEPT = 3,
	UFCS_CTRL_SOFT_RESET = 4,
	UFCS_CTRL_POWER_READY = 5,
	UFCS_CTRL_GET_OUTPUT_CAPABILITIES = 6,
	UFCS_CTRL_GET_SOURCE_INFO = 7,
	UFCS_CTRL_GET_SINK_INFO = 8,
	UFCS_CTRL_GET_CABLE_INFO = 9,
	UFCS_CTRL_GET_DEVICE_INFO = 10,
	UFCS_CTRL_GET_ERROR_INFO = 11,
	UFCS_CTRL_DETECT_CABLE_INFO = 12,
	UFCS_CTRL_START_CABLE_DETECT = 13,
	UFCS_CTRL_END_CABLE_DETECT = 14,
	UFCS_CTRL_EXIT_UFCS_MODE = 15,
	UFCS_CTRL_POWER_SWAP = 16,
};

enum ufcs_data_cmd {
	UFCS_DATA_OUTPUT_CAPABILITIES = 1,
	UFCS_DATA_REQUEST = 2,
	UFCS_DATA_SOURCE_INFO = 3,
	UFCS_DATA_SINK_INFO = 4,
	UFCS_DATA_CABLE_INFO = 5,
	UFCS_DATA_DEVICE_INFO = 6,
	UFCS_DATA_ERROR_INFO = 7,
	UFCS_DATA_CONFIG_WATCHDOG = 8,
	UFCS_DATA_REFUSE = 9,
	UFCS_DATA_VERIFY_REQUEST = 10,
	UFCS_DATA_VERIFY_RESPONSE = 11,
	UFCS_DATA_POWER_CHANGE = 12,
	UFCS_DATA_TEST_REQUEST = 255,
};

enum ufcs_msg_type {
	UFCS_MSG_CTRL = 0,
	UFCS_MSG_DATA = 1,
	UFCS_MSG_VDM = 2,
};

enum ufcs_refuse_reason {
	UFCS_REFUSE_UNRECOGNIZED = 1,
	UFCS_REFUSE_UNSUPPORTED = 2,
	UFCS_REFUSE_BUSY = 3,
	UFCS_REFUSE_REQUEST_OVER_RANGE = 4,
	UFCS_REFUSE_OTHER = 5,
};

enum ufcs_emark_state {
	UFCS_EMARK_NONE = 0,
	UFCS_EMARK_NOT_EXIST = 1,
	UFCS_EMARK_EXIST = 2,
};

#define GENERATE_ENUM(e)	e
#define GENERATE_STRING(s)	#s

enum ufcs_state {
	GENERATE_ENUM(INVALID_STATE),
	GENERATE_ENUM(SNK_DETECT_FAIL),
	GENERATE_ENUM(SNK_ATTACH_WAIT),
	GENERATE_ENUM(SNK_ATTACH_FIRST_PING),
	GENERATE_ENUM(SNK_ATTACHED),
	GENERATE_ENUM(SNK_STARTUP),
	GENERATE_ENUM(SNK_START_CABLE_DETECT),
	GENERATE_ENUM(SNK_GET_CABLE_INFO),
	GENERATE_ENUM(SNK_END_CABLE_DETECT),
	GENERATE_ENUM(SNK_CONFIG_WATCHDOG),
	GENERATE_ENUM(SNK_GET_OUTPUT_CAPABILITIES),
	GENERATE_ENUM(SNK_NEGOTIATE_CAPABILITIES),
	GENERATE_ENUM(SNK_TRANSITION_SINK),
	GENERATE_ENUM(SNK_READY),
	GENERATE_ENUM(SNK_SEND_VERIFY_REQUEST),
	GENERATE_ENUM(SNK_WAIT_VERIFY_RESPONSE),
	GENERATE_ENUM(SNK_GET_DEVICE_INFO),
	GENERATE_ENUM(SNK_GET_SOURCE_INFO),
	GENERATE_ENUM(SNK_GET_ERROR_INFO),
	GENERATE_ENUM(SNK_EXIT_UFCS_MODE),

	GENERATE_ENUM(SNK_DETECT_CABLE_INFO_RECV),
	GENERATE_ENUM(SNK_START_CABLE_DETECT_RECV),
	GENERATE_ENUM(SNK_END_CABLE_DETECT_RECV),
	GENERATE_ENUM(SNK_GET_SINK_INFO_RECV),
	GENERATE_ENUM(SNK_GET_DEVICE_INFO_RECV),
	GENERATE_ENUM(SNK_GET_ERROR_INFO_RECV),
	GENERATE_ENUM(SNK_TEST_REQUEST_RECV),

	GENERATE_ENUM(SOFT_RESET),
	GENERATE_ENUM(SNK_SOFT_RESET),
	GENERATE_ENUM(SOFT_RESET_SEND),

	GENERATE_ENUM(HARD_RESET),
	GENERATE_ENUM(HARD_RESET_SEND),

	GENERATE_ENUM(PORT_RESET),
};

static const char * const ufcs_states[] = {
	GENERATE_STRING(INVALID_STATE),
	GENERATE_STRING(SNK_DETECT_FAIL),
	GENERATE_STRING(SNK_ATTACH_WAIT),
	GENERATE_STRING(SNK_ATTACH_FIRST_PING),
	GENERATE_STRING(SNK_ATTACHED),
	GENERATE_STRING(SNK_STARTUP),
	GENERATE_STRING(SNK_START_CABLE_DETECT),
	GENERATE_STRING(SNK_GET_CABLE_INFO),
	GENERATE_STRING(SNK_END_CABLE_DETECT),
	GENERATE_STRING(SNK_CONFIG_WATCHDOG),
	GENERATE_STRING(SNK_GET_OUTPUT_CAPABILITIES),
	GENERATE_STRING(SNK_NEGOTIATE_CAPABILITIES),
	GENERATE_STRING(SNK_TRANSITION_SINK),
	GENERATE_STRING(SNK_READY),
	GENERATE_STRING(SNK_SEND_VERIFY_REQUEST),
	GENERATE_STRING(SNK_WAIT_VERIFY_RESPONSE),
	GENERATE_STRING(SNK_GET_DEVICE_INFO),
	GENERATE_STRING(SNK_GET_SOURCE_INFO),
	GENERATE_STRING(SNK_GET_ERROR_INFO),
	GENERATE_STRING(SNK_EXIT_UFCS_MODE),

	GENERATE_STRING(SNK_DETECT_CABLE_INFO_RECV),
	GENERATE_STRING(SNK_START_CABLE_DETECT_RECV),
	GENERATE_STRING(SNK_END_CABLE_DETECT_RECV),
	GENERATE_STRING(SNK_GET_SINK_INFO_RECV),
	GENERATE_STRING(SNK_GET_DEVICE_INFO_RECV),
	GENERATE_STRING(SNK_GET_ERROR_INFO_RECV),
	GENERATE_STRING(SNK_TEST_REQUEST_RECV),

	GENERATE_STRING(SOFT_RESET),
	GENERATE_STRING(SNK_SOFT_RESET),
	GENERATE_STRING(SOFT_RESET_SEND),

	GENERATE_STRING(HARD_RESET),
	GENERATE_STRING(HARD_RESET_SEND),

	GENERATE_STRING(PORT_RESET),
};

struct ufcs_port {
	struct device dev;

	struct mutex lock;
	struct kthread_worker *wq;

	struct ufcs_dev *ufcs;

	/* User notify interface */
	struct srcu_notifier_head evt_nh;

	enum ufcs_state enter_state;
	enum ufcs_state prev_state;
	enum ufcs_state state;
	enum ufcs_state delayed_state;
	enum ufcs_state test_state;
	ktime_t delayed_runtime;
	unsigned long delayed_ms;

	spinlock_t ufcs_event_lock;
	u32 ufcs_events;
	bool dcp_attached;

	struct kthread_work event_work;
	struct hrtimer state_machine_timer;
	struct kthread_work state_machine;
	bool state_machine_running;

	struct completion tx_complete;
	enum ufcs_transmit_status tx_status;

	unsigned int message_id;
	unsigned int rx_msgid;
	bool ignore_hard_reset;
	bool attached;
	bool registered;
	atomic_t use_cnt;

	u64 src_caps[MAX_SRC_CAP_CNT];
	unsigned int src_cap_cnt;
	u8 emark_info[MAX_EMARK_BYTE_CNT];
	enum ufcs_emark_state emark_state;
	u32 req_current_limit;
	u32 req_supply_voltage;
	u32 _req_supply_voltage;
	u32 _req_current_limit;

	struct mutex dpm_lock;
	struct completion dpm_completion;
	int dpm_error;
	enum ufcs_dpm_request dpm_request;
	union ufcs_dpm_input input;
	union ufcs_dpm_output output;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	struct mutex logbuffer_lock;	/* log buffer access lock */
	unsigned int logbuffer_head;
	unsigned int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];
#endif
};

struct ufcs_rx_event {
	struct kthread_work work;
	struct ufcs_port *port;
	struct ufcs_message msg;
};

struct class *ufcs_class;
EXPORT_SYMBOL_GPL(ufcs_class);

/* Pre-declaration functions */
static int ufcs_send_ctrl_message(struct ufcs_port *port, enum ufcs_role to, u8 cmd);
static int ufcs_send_refuse(struct ufcs_port *port, u32 type, u32 cmd, u32 reason);
static inline void ufcs_put_dpm_reaction_result(struct ufcs_port *port, int error);
static void ufcs_put_refuse_dpm_reaction_result(struct ufcs_port *port, u8 reason);

static inline unsigned int ufcs_header_type(u16 header)
{
	return (header >> UFCS_HEADER_TYPE_SHIFT) & UFCS_HEADER_TYPE_MASK;
}

static inline unsigned int ufcs_header_type_be(__be16 header)
{
	return ufcs_header_type(be16_to_cpu(header));
}

static inline unsigned int ufcs_header_msgid(u16 header)
{
	return (header >> UFCS_HEADER_ID_SHIFT) & UFCS_HEADER_ID_MASK;
}

static inline unsigned int ufcs_header_msgid_be(__be16 header)
{
	return ufcs_header_msgid(be16_to_cpu(header));
}

static inline unsigned int src_cap_get_step_milliamp(u64 src_cap)
{
	return FIELD_GET(GENMASK(59, 57), src_cap) * 10 + 10;
}

static inline unsigned int src_cap_get_step_millivolt(u64 src_cap)
{
	return FIELD_GET(BIT(56), src_cap) ? 20 : 10;
}

static inline unsigned int src_cap_get_max_millivolt(u64 src_cap)
{
	return FIELD_GET(GENMASK(55, 40), src_cap) * 10;
}

static inline unsigned int src_cap_get_min_millivolt(u64 src_cap)
{
	return FIELD_GET(GENMASK(39, 24), src_cap) * 10;
}

static inline unsigned int src_cap_get_max_milliamp(u64 src_cap)
{
	return FIELD_GET(GENMASK(23, 8), src_cap) * 10;
}

static inline unsigned int src_cap_get_min_milliamp(u64 src_cap)
{
	return FIELD_GET(GENMASK(7, 0), src_cap) * 10;
}

static inline bool src_cap_is_fixed_current(u64 src_cap)
{
	return FIELD_GET(GENMASK(59, 57), src_cap) == 7;
}

static inline u64 put_cap_request_msg(u32 idx, u32 req_volt, u32 req_curr, u32 step_mV, u32 step_mA)
{
	return FIELD_PREP(GENMASK(31, 16), req_volt / step_mV * step_mV / 10) |
	       FIELD_PREP(GENMASK(15, 0), req_curr / step_mA * step_mA / 10) |
	       FIELD_PREP(GENMASK(63, 60), idx + 1);
}

static inline u32 put_refuse_msg(u32 rxmsg_id, u32 type, u32 cmd, u32 reason)
{
	return FIELD_PREP(GENMASK(27, 24), rxmsg_id) | FIELD_PREP(GENMASK(18, 16), type) |
	       FIELD_PREP(GENMASK(15, 8), cmd) | FIELD_PREP(GENMASK(7, 0), reason);
}

#ifdef CONFIG_DEBUG_FS

static bool ufcs_log_full(struct ufcs_port *port)
{
	return port->logbuffer_tail == (port->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;
}

__printf(2, 0)
static void _ufcs_log(struct ufcs_port *port, const char *fmt, va_list args)
{
	char tmpbuffer[LOG_BUFFER_ENTRY_SIZE] = { 0 };
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	mutex_lock(&port->logbuffer_lock);
	if (!port->logbuffer[port->logbuffer_head]) {
		port->logbuffer[port->logbuffer_head] = kzalloc(LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!port->logbuffer[port->logbuffer_head]) {
			mutex_unlock(&port->logbuffer_lock);
			return;
		}
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	if (!dbg_log_en && ufcs_log_full(port)) {
		port->logbuffer_head = max(port->logbuffer_head - 1, 0);
		strscpy(tmpbuffer, "overflow", LOG_BUFFER_ENTRY_SIZE);
	}

	if (port->logbuffer_head >= LOG_BUFFER_ENTRIES) {
		dev_info(&port->dev, "Bad log buffer index %d\n", port->logbuffer_head);
		goto abort;
	}

	if (!port->logbuffer[port->logbuffer_head]) {
		dev_info(&port->dev, "Log buffer index %d is NULL\n", port->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(port->logbuffer[port->logbuffer_head],
		  LOG_BUFFER_ENTRY_SIZE, "[UFCS][%5lu.%06lu] %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	if (dbg_log_en)
		pr_notice("%s\n", port->logbuffer[port->logbuffer_head]);

	port->logbuffer_head = (port->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&port->logbuffer_lock);
}

__printf(2, 3)
static void ufcs_log(struct ufcs_port *port, const char *fmt, ...)
{
	va_list args;

	/* Do not log while disconnected and unattached */
	if (port->state == PORT_RESET)
		return;

	va_start(args, fmt);
	_ufcs_log(port, fmt, args);
	va_end(args);
}

__printf(2, 3)
static void ufcs_log_force(struct ufcs_port *port, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_ufcs_log(port, fmt, args);
	va_end(args);
}

static void ufcs_log_source_caps(struct ufcs_port *port)
{
	int i;

	for (i = 0; i < port->src_cap_cnt; i++) {
		u64 src_cap = port->src_caps[i];
		char msg[64];

		scnprintf(msg, sizeof(msg), "%u-%u mV, %u-%u mA, step %u mV %u mA",
			  src_cap_get_min_millivolt(src_cap), src_cap_get_max_millivolt(src_cap),
			  src_cap_get_min_milliamp(src_cap), src_cap_get_max_milliamp(src_cap),
			  src_cap_get_step_millivolt(src_cap), src_cap_get_step_milliamp(src_cap));

		ufcs_log(port, " PDO %d: %s", i, msg);
	}
}

static int ufcs_debug_show(struct seq_file *s, void *v)
{
	struct ufcs_port *port = (struct ufcs_port *)s->private;
	unsigned int tail;

	mutex_lock(&port->logbuffer_lock);

	tail = port->logbuffer_tail;
	while (tail != port->logbuffer_head && tail < LOG_BUFFER_ENTRY_SIZE) {
		seq_printf(s, "%s\n", port->logbuffer[tail]);
		tail = (tail + 1) % LOG_BUFFER_ENTRIES;
	}

	if (!seq_has_overflowed(s))
		port->logbuffer_tail = tail;

	mutex_unlock(&port->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(ufcs_debug);

static void ufcs_debugfs_init(struct ufcs_port *port)
{
	char name[NAME_MAX];

	mutex_init(&port->logbuffer_lock);
	snprintf(name, NAME_MAX, "ufcs-%s", dev_name(&port->dev));
	port->dentry = debugfs_create_dir(name, usb_debug_root);
	debugfs_create_file("log", S_IFREG | 0444, port->dentry, port,
			    &ufcs_debug_fops);
}

static void ufcs_debugfs_exit(struct ufcs_port *port)
{
	int i;

	mutex_lock(&port->logbuffer_lock);
	for (i = 0; i < LOG_BUFFER_ENTRIES; i++) {
		kfree(port->logbuffer[i]);
		port->logbuffer[i] = NULL;
	}
	mutex_unlock(&port->logbuffer_lock);

	debugfs_remove(port->dentry);
}

#else

__printf(2, 3)
static void ufcs_log(const struct ufcs_port *port, const char *fmt, ...) { }
__printf(2, 3)
static void ufcs_log_force(struct ufcs_port *port, const char *fmt, ...) { }
static void ufcs_log_source_caps(struct ufcs_port *port) { }
static void ufcs_debugfs_init(const struct ufcs_port *port) { }
static void ufcs_debugfs_exit(const struct ufcs_port *port) { }

#endif

static void mod_ufcs_delayed_work(struct ufcs_port *port, unsigned int delay_ms)
{
	if (delay_ms) {
		hrtimer_start(&port->state_machine_timer, ms_to_ktime(delay_ms), HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&port->state_machine_timer);
		kthread_queue_work(port->wq, &port->state_machine);
	}
}

static void ufcs_set_state(struct ufcs_port *port, enum ufcs_state state, unsigned int delay_ms)
{
	if (delay_ms) {
		ufcs_log(port, "pending state change %s -> %s @ %u ms",
			 ufcs_states[port->state], ufcs_states[state], delay_ms);
		port->delayed_state = state;
		mod_ufcs_delayed_work(port, delay_ms);
		port->delayed_runtime = ktime_add(ktime_get(), ms_to_ktime(delay_ms));
		port->delayed_ms = delay_ms;
	} else {
		ufcs_log(port, "state change %s -> %s",
			 ufcs_states[port->state], ufcs_states[state]);
		port->delayed_state = INVALID_STATE;
		port->prev_state = port->state;
		port->state = state;

		if (!port->state_machine_running)
			mod_ufcs_delayed_work(port, 0);
	}
}

static void ufcs_set_state_cond(struct ufcs_port *port, enum ufcs_state state,
				unsigned int delay_ms)
{
	if (port->enter_state == port->state)
		ufcs_set_state(port, state, delay_ms);
	else
		ufcs_log(port,
			 "skipped %sstate change %s -> %s [%u ms], context state %s",
			 delay_ms ? "delayed " : "",
			 ufcs_states[port->state], ufcs_states[state],
			 delay_ms, ufcs_states[port->enter_state]);
}

void ufcs_tx_complete(struct ufcs_port *port, enum ufcs_transmit_status status)
{
	ufcs_log(port, "UFCS TX complete, status: %u", status);
	port->tx_status = status;
	complete(&port->tx_complete);
}
EXPORT_SYMBOL_GPL(ufcs_tx_complete);

static void ufcs_handle_ctrl_event(struct ufcs_port *port, const struct ufcs_ctrl_payload *ctrl)
{
	enum ufcs_state next_state = SOFT_RESET_SEND;
	int ret;

	ufcs_log(port, "UFCS handle ctrl command: %d", ctrl->command);

	switch (ctrl->command) {
	case UFCS_CTRL_PING:
		return;
	case UFCS_CTRL_ACCEPT:
		if (port->state == SNK_START_CABLE_DETECT) {
			next_state = SNK_GET_CABLE_INFO;
			port->ignore_hard_reset = true;
		} else if (port->state == SNK_CONFIG_WATCHDOG) {
			if (port->state == port->test_state) {
				port->test_state = INVALID_STATE;

				next_state = SNK_READY;
			} else
				next_state = SNK_GET_OUTPUT_CAPABILITIES;
		} else if (port->state == SNK_NEGOTIATE_CAPABILITIES)
			next_state = SNK_TRANSITION_SINK;
		else if (port->state == SNK_SEND_VERIFY_REQUEST)
			next_state = SNK_WAIT_VERIFY_RESPONSE;
		break;
	case UFCS_CTRL_SOFT_RESET:
		next_state = SOFT_RESET;
		break;
	case UFCS_CTRL_POWER_READY:
		if (port->state == SNK_TRANSITION_SINK) {
			if (port->test_state == SNK_NEGOTIATE_CAPABILITIES)
				port->test_state = INVALID_STATE;

			next_state = SNK_READY;
		}

		if (port->state != SOFT_RESET_SEND && port->dpm_request == UFCS_DPM_POWER_REQUEST) {
			port->_req_supply_voltage = port->req_supply_voltage;
			port->_req_current_limit = port->req_current_limit;
			port->output.power_request_ready = true;
			ufcs_put_dpm_reaction_result(port, 0);
		}
		break;
	case UFCS_CTRL_GET_SINK_INFO:
		if (port->state != SNK_READY) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_BUSY);
			return;
		}

		next_state = SNK_GET_SINK_INFO_RECV;
		break;
	case UFCS_CTRL_GET_DEVICE_INFO:
		if (port->state != SNK_READY) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_BUSY);
			return;
		}

		next_state = SNK_GET_DEVICE_INFO_RECV;
		break;
	case UFCS_CTRL_GET_ERROR_INFO:
		if (port->state != SNK_READY) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_BUSY);
			return;
		}

		next_state = SNK_GET_ERROR_INFO_RECV;
		break;
	case UFCS_CTRL_DETECT_CABLE_INFO:
		if (port->state != SNK_READY) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_BUSY);
			return;
		} else if (port->emark_state != UFCS_EMARK_EXIST) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_OTHER);
			return;
		}

		ret = ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_ACCEPT);
		if (!ret)
			next_state = SNK_DETECT_CABLE_INFO_RECV;
		break;
	case UFCS_CTRL_START_CABLE_DETECT:
		if (port->state != SNK_READY) {
			ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_BUSY);
			return;
		}

		ret = ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_ACCEPT);
		if (!ret)
			next_state = SNK_START_CABLE_DETECT_RECV;
		break;
	case UFCS_CTRL_END_CABLE_DETECT:
		if (port->state == SNK_START_CABLE_DETECT_RECV)
			next_state = SNK_END_CABLE_DETECT_RECV;
		break;
	case UFCS_CTRL_EXIT_UFCS_MODE:
		next_state = PORT_RESET;
		break;
	case UFCS_CTRL_POWER_SWAP:
		ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command,
			port->state == SNK_READY ? UFCS_REFUSE_UNSUPPORTED : UFCS_REFUSE_BUSY);
		return;
	default:
		ufcs_send_refuse(port, UFCS_MSG_CTRL, ctrl->command, UFCS_REFUSE_UNRECOGNIZED);
		return;
	}

	ufcs_set_state(port, next_state, 0);
}

static void ufcs_handle_data_event(struct ufcs_port *port, const struct ufcs_data_payload *data)
{
	enum ufcs_state next_state = SOFT_RESET_SEND;
	u64 tmp64;
	u16 tmp16;
	int i;

	ufcs_log(port, "UFCS handle data command: %d", data->command);

	switch (data->command) {
	case UFCS_DATA_OUTPUT_CAPABILITIES:
		if (data->datalen < 8 || data->datalen > 56 || data->datalen % 8 != 0) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		/* Copy caps into port data */
		port->src_cap_cnt = min_t(u32, data->datalen / sizeof(__be64), MAX_SRC_CAP_CNT);
		for (i = 0; i < port->src_cap_cnt; i++)
			port->src_caps[i] = be64_to_cpu(data->payload64[i]);

		ufcs_log_source_caps(port);

		if (port->state == SNK_GET_OUTPUT_CAPABILITIES) {
			if (port->state == port->test_state)
				port->test_state = INVALID_STATE;

			next_state = SNK_READY;
		}

		break;
	case UFCS_DATA_CABLE_INFO:
		if (data->datalen != MAX_EMARK_BYTE_CNT) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		/* Copy emark info into port data */
		memcpy(port->emark_info, data->payload, MAX_EMARK_BYTE_CNT);
		port->emark_state = UFCS_EMARK_EXIST;

		ufcs_log(port, "CABLE: vid1 %04x, vid2 %04x, %umOhm, max %umV, %umA",
			 get_unaligned_be16(port->emark_info),
			 get_unaligned_be16(port->emark_info + 2),
			 get_unaligned_be16(port->emark_info + 4),
			 (u32)get_unaligned_be16(port->emark_info + 6) * 10,
			 (u32)get_unaligned_be16(port->emark_info + 8) * 10);

		if (port->state == SNK_GET_CABLE_INFO) {
			if (port->state == port->test_state) {
				port->test_state = INVALID_STATE;
				next_state = SNK_READY;
			} else
				next_state = SNK_END_CABLE_DETECT;
		}

		if (next_state != SOFT_RESET_SEND &&
		    port->dpm_request == UFCS_DPM_CABLE_INFO) {
			port->output.cbl_vid1 = get_unaligned_be16(port->emark_info);
			port->output.cbl_vid2 = get_unaligned_be16(port->emark_info + 2);
			port->output.cbl_mOhm = get_unaligned_be16(port->emark_info + 4);
			port->output.cbl_mVolt = (u32)get_unaligned_be16(port->emark_info + 6) * 10;
			port->output.cbl_mAmp = (u32)get_unaligned_be16(port->emark_info + 8) * 10;
		}
		break;
	case UFCS_DATA_DEVICE_INFO:
		if (data->datalen != sizeof(__be64)) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		tmp64 = be64_to_cpu(data->payload64[0]);

		ufcs_log(port, "SRC Ver 0x%llx", tmp64);

		if (port->state == SNK_GET_DEVICE_INFO)
			next_state = SNK_READY;

		if (next_state != SOFT_RESET_SEND &&
		    port->dpm_request == UFCS_DPM_SRC_DEVICE_INFO) {
			port->output.hw_ver = FIELD_GET(GENMASK(31, 16), tmp64);
			port->output.sw_ver = FIELD_GET(GENMASK(15, 0), tmp64);
			ufcs_put_dpm_reaction_result(port, 0);
		}
		break;
	case UFCS_DATA_SOURCE_INFO:
		if (data->datalen != sizeof(__be64)) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		tmp64 = be64_to_cpu(data->payload64[0]);

		if (port->state == SNK_GET_SOURCE_INFO) {
			if (port->state == port->test_state)
				port->test_state = INVALID_STATE;

			next_state = SNK_READY;
		}

		if (next_state != SOFT_RESET_SEND && port->dpm_request == UFCS_DPM_SRC_INFO) {
			port->output.device_temp = FIELD_GET(GENMASK(47, 40), tmp64) - 50;
			port->output.conn_temp = FIELD_GET(GENMASK(39, 32), tmp64) - 50;
			port->output.output_millivolt = FIELD_GET(GENMASK(31, 16), tmp64) * 10;
			port->output.output_milliamp = FIELD_GET(GENMASK(15, 0), tmp64) * 10;
			ufcs_put_dpm_reaction_result(port, 0);
		}
		break;
	case UFCS_DATA_ERROR_INFO:
		if (data->datalen != sizeof(__be32)) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		if (port->state == SNK_GET_ERROR_INFO)
			next_state = SNK_READY;

		if (next_state != SOFT_RESET_SEND && port->dpm_request == UFCS_DPM_ERROR_INFO) {
			port->output.raw32 = be32_to_cpu(data->payload32);
			ufcs_put_dpm_reaction_result(port, 0);
		}
		break;
	case UFCS_DATA_REFUSE:
		if (data->datalen != sizeof(__be32)) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		if (port->state == SNK_GET_OUTPUT_CAPABILITIES ||
		    port->state == SNK_NEGOTIATE_CAPABILITIES ||
		    port->state == SNK_SEND_VERIFY_REQUEST ||
		    port->state == SNK_GET_DEVICE_INFO ||
		    port->state == SNK_GET_SOURCE_INFO ||
		    port->state == SNK_GET_ERROR_INFO) {
			if (port->state == port->test_state)
				port->test_state = INVALID_STATE;

			next_state = SNK_READY;
		} else if (port->state == SNK_START_CABLE_DETECT) {
			ufcs_log(port, "Not expected refuse for cable detect flow");
			next_state = SNK_CONFIG_WATCHDOG;
		}

		if (port->state != SOFT_RESET_SEND && port->dpm_request != UFCS_DPM_NONE)
			ufcs_put_refuse_dpm_reaction_result(port, data->payload[3]);
		break;
	case UFCS_DATA_VERIFY_REQUEST:
		if (data->datalen != 16) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
			port->state == SNK_READY ? UFCS_REFUSE_UNSUPPORTED : UFCS_REFUSE_BUSY);
		return;
	case UFCS_DATA_VERIFY_RESPONSE:
		if (data->datalen != 48) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		if (port->state == SNK_WAIT_VERIFY_RESPONSE)
			next_state = SNK_READY;

		if (port->state != SOFT_RESET_SEND && port->dpm_request != UFCS_DPM_NONE) {
			memcpy(port->output.secure, data->payload, 32);
			memcpy(port->output.random, data->payload + 32, 16);
			ufcs_put_dpm_reaction_result(port, 0);
		}
		break;
	case UFCS_DATA_POWER_CHANGE:
		if (data->datalen < 3 || data->datalen > 21 || data->datalen % 3 != 0) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		if (port->state == SNK_READY)
			next_state = SNK_GET_OUTPUT_CAPABILITIES;
		break;
	case UFCS_DATA_TEST_REQUEST:
		if (data->datalen != 2) {
			ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
					 UFCS_REFUSE_UNRECOGNIZED);
			return;
		}

		tmp16 = be16_to_cpu(data->payload16);
		if (tmp16 & BIT(15)) {
			if (port->test_state != INVALID_STATE) {
				ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
						 UFCS_REFUSE_BUSY);
				return;
			}

			if ((tmp16 & GENMASK(13, 0)) == GENMASK(13, 0))
				return;

			switch (FIELD_GET(GENMASK(10, 8), tmp16)) {
			case UFCS_MSG_CTRL:
				switch (FIELD_GET(GENMASK(7, 0), tmp16)) {
				case UFCS_CTRL_GET_OUTPUT_CAPABILITIES:
					port->test_state = SNK_GET_OUTPUT_CAPABILITIES;
					break;
				case UFCS_CTRL_GET_SOURCE_INFO:
					port->test_state = SNK_GET_SOURCE_INFO;
					break;
				case UFCS_CTRL_GET_CABLE_INFO:
					port->test_state = SNK_GET_CABLE_INFO;
					break;
				default:
					ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
							 UFCS_REFUSE_UNSUPPORTED);
					return;
				}
				break;
			case UFCS_MSG_DATA:
				switch (FIELD_GET(GENMASK(7, 0), tmp16)) {
				case UFCS_DATA_REQUEST:
					port->test_state = SNK_NEGOTIATE_CAPABILITIES;
					break;
				case UFCS_DATA_CONFIG_WATCHDOG:
					port->test_state = SNK_CONFIG_WATCHDOG;
					break;
				default:
					ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
							 UFCS_REFUSE_UNSUPPORTED);
					return;
				}
				break;
			default:
				ufcs_send_refuse(port, UFCS_MSG_DATA, data->command,
						 UFCS_REFUSE_UNSUPPORTED);
				return;
			}

			next_state = SNK_TEST_REQUEST_RECV;

		} else {
			port->test_state = INVALID_STATE;
			next_state = SNK_READY;
		}

		break;
	default:
		ufcs_send_refuse(port, UFCS_MSG_DATA, data->command, UFCS_REFUSE_UNRECOGNIZED);
		return;
	}

	ufcs_set_state(port, next_state, 0);
}

static void ufcs_handle_vdm_event(struct ufcs_port *port, const struct ufcs_vdm_payload *vdm)
{
	/* ToDo: handle vdm receive and transfer */
}

static void ufcs_rx_event_handler(struct kthread_work *work)
{
	struct ufcs_rx_event *event = container_of(work, struct ufcs_rx_event, work);
	struct ufcs_message *msg = &event->msg;
	struct ufcs_port *port = event->port;

	mutex_lock(&port->lock);

	ufcs_log(port, "UFCS RX header: %#x [%d]", be16_to_cpu(msg->header), port->attached);

	if (port->attached) {
		enum ufcs_msg_type type = ufcs_header_type_be(msg->header);
		unsigned int msgid = ufcs_header_msgid_be(msg->header);

		port->rx_msgid = msgid;

		switch (type) {
		case UFCS_MSG_CTRL:
			ufcs_handle_ctrl_event(port, &msg->ctrl);
			break;
		case UFCS_MSG_DATA:
			ufcs_handle_data_event(port, &msg->data);
			break;
		case UFCS_MSG_VDM:
			ufcs_handle_vdm_event(port, &msg->vdm);
			break;
		default:
			/* Invalid msg type, send refuse command with error  0x01 */
			break;
		}
	}

	mutex_unlock(&port->lock);
	kfree(event);
}

void ufcs_rx_receive(struct ufcs_port *port, const struct ufcs_message *msg)
{
	struct ufcs_rx_event *event;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return;

	kthread_init_work(&event->work, ufcs_rx_event_handler);
	event->port = port;
	memcpy(&event->msg, msg, sizeof(*msg));
	kthread_queue_work(port->wq, &event->work);
}
EXPORT_SYMBOL_GPL(ufcs_rx_receive);

static void _ufcs_hard_reset_recv(struct ufcs_port *port)
{
	if (port->ignore_hard_reset)
		return;

	if (port->state == HARD_RESET_SEND || port->state == PORT_RESET)
		return;

	/*
	 * Some adapter recv EXIT_UFCS_MODE command, hard_reset will be got
	 * in advance
	 */
	if (port->state == SNK_EXIT_UFCS_MODE) {
		ufcs_set_state(port, PORT_RESET, 0);
		return;
	}

	ufcs_set_state(port, HARD_RESET, 0);
}

static void _ufcs_attach_change(struct ufcs_port *port, bool attached)
{
	if (attached) {
		if (port->state == PORT_RESET)
			ufcs_set_state(port, SNK_ATTACH_WAIT, 0);
		/*
		 * else **repeated attached???**
		 */
	} else {
		if (port->state == PORT_RESET)
			return;

		ufcs_set_state(port, PORT_RESET, 0);
	}
}

static void _ufcs_hand_shake_state(struct ufcs_port *port, bool success)
{
	enum ufcs_state next_state = success ? SNK_ATTACH_FIRST_PING : SNK_DETECT_FAIL;

	if (port->state != SNK_ATTACH_WAIT)
		return;

	ufcs_set_state(port, next_state, 0);
}

static inline void ufcs_put_dpm_reaction_result(struct ufcs_port *port, int error)
{
	port->dpm_request = UFCS_DPM_NONE;
	port->dpm_error = error;
	complete(&port->dpm_completion);
}

static void ufcs_put_refuse_dpm_reaction_result(struct ufcs_port *port, u8 reason)
{
	int ret;

	switch (reason) {
		break;
	case UFCS_REFUSE_UNSUPPORTED:
		ret = -EOPNOTSUPP;
		break;
	case UFCS_REFUSE_BUSY:
		ret = -EBUSY;
		break;
	case UFCS_REFUSE_REQUEST_OVER_RANGE:
		ret = -ERANGE;
		break;
	case UFCS_REFUSE_UNRECOGNIZED:
	case UFCS_REFUSE_OTHER:
	default:
		ret = -EINVAL;
		break;
	}

	ufcs_put_dpm_reaction_result(port, ret);
}

static void _ufcs_dpm_event_handler(struct ufcs_port *port)
{
	union ufcs_dpm_input *input = &port->input;
	int i;

	ufcs_log(port, "UFCS dpm_action req = %d", port->dpm_request);

	if (port->state != SNK_READY) {
		ufcs_put_dpm_reaction_result(port, -EBUSY);
		return;
	}

	switch (port->dpm_request) {
	case UFCS_DPM_POWER_REQUEST:
		port->req_supply_voltage = input->req_millivolt;
		port->req_current_limit = input->req_milliamp;
		ufcs_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
		break;
	case UFCS_DPM_VERIFY_REQUEST:
		ufcs_set_state(port, SNK_SEND_VERIFY_REQUEST, 0);
		break;
	case UFCS_DPM_CABLE_INFO:
		if (port->emark_state == UFCS_EMARK_EXIST) {
			port->output.cbl_vid1 = get_unaligned_be16(port->emark_info);
			port->output.cbl_vid2 = get_unaligned_be16(port->emark_info + 2);
			port->output.cbl_mOhm = get_unaligned_be16(port->emark_info + 4);
			port->output.cbl_mVolt = (u32)get_unaligned_be16(port->emark_info + 6) * 10;
			port->output.cbl_mAmp = (u32)get_unaligned_be16(port->emark_info + 8) * 10;
			ufcs_put_dpm_reaction_result(port, 0);
		} else if (port->emark_state == UFCS_EMARK_NOT_EXIST)
			ufcs_put_dpm_reaction_result(port, -ENODEV);
		else
			ufcs_set_state(port, SNK_START_CABLE_DETECT, 0);
		break;
	case UFCS_DPM_SRC_DEVICE_INFO:
		ufcs_set_state(port, SNK_GET_DEVICE_INFO, 0);
		break;
	case UFCS_DPM_SRC_INFO:
		ufcs_set_state(port, SNK_GET_SOURCE_INFO, 0);
		break;
	case UFCS_DPM_ERROR_INFO:
		ufcs_set_state(port, SNK_GET_ERROR_INFO, 0);
		break;
	case UFCS_DPM_SRC_CAP:
		port->output.src_cap_cnt = port->src_cap_cnt;

		for (i = 0; i < port->src_cap_cnt; i++) {
			u64 src_cap = port->src_caps[i];

			port->output.src_cap[i].min_mV = src_cap_get_min_millivolt(src_cap);
			port->output.src_cap[i].max_mV = src_cap_get_max_millivolt(src_cap);
			port->output.src_cap[i].min_mA = src_cap_get_min_milliamp(src_cap);
			port->output.src_cap[i].max_mA = src_cap_get_max_milliamp(src_cap);
		}

		ufcs_put_dpm_reaction_result(port, 0);
		break;
	case UFCS_DPM_EXIT_UFCS_MODE:
		ufcs_set_state(port, SNK_EXIT_UFCS_MODE, 0);
		break;
	case UFCS_DPM_VDM:
	default:
		break;
	}
}

static void ufcs_event_handler(struct kthread_work *work)
{
	struct ufcs_port *port = container_of(work, struct ufcs_port, event_work);
	u32 events;
	bool attached;

	mutex_lock(&port->lock);
	spin_lock(&port->ufcs_event_lock);
	while (port->ufcs_events) {
		events = port->ufcs_events;
		port->ufcs_events = 0;
		attached = port->dcp_attached;
		spin_unlock(&port->ufcs_event_lock);

		if (events & UFCS_RESET_EVENT)
			_ufcs_hard_reset_recv(port);

		if (events & UFCS_ATTACH_EVENT)
			_ufcs_attach_change(port, attached);

		if (events & UFCS_HS_SUCCESS_EVENT)
			_ufcs_hand_shake_state(port, true);

		if (events & UFCS_HS_FAIL_EVENT)
			_ufcs_hand_shake_state(port, false);

		if (events & UFCS_DPM_EVENT)
			_ufcs_dpm_event_handler(port);

		spin_lock(&port->ufcs_event_lock);
	}
	spin_unlock(&port->ufcs_event_lock);
	mutex_unlock(&port->lock);
}

void ufcs_attach_change(struct ufcs_port *port, bool dcp_attached)
{
	ufcs_log_force(port, "DCP %s", dcp_attached ? "Attach" : "Detach");

	spin_lock(&port->ufcs_event_lock);
	port->ufcs_events |= UFCS_ATTACH_EVENT;
	port->dcp_attached = dcp_attached;
	spin_unlock(&port->ufcs_event_lock);
	kthread_queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(ufcs_attach_change);

void ufcs_hard_reset(struct ufcs_port *port)
{
	ufcs_log_force(port, "HardReset recv");

	spin_lock(&port->ufcs_event_lock);
	port->ufcs_events |= UFCS_RESET_EVENT;
	spin_unlock(&port->ufcs_event_lock);
	kthread_queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(ufcs_hard_reset);

void ufcs_hand_shake_state(struct ufcs_port *port, bool success)
{
	ufcs_log_force(port, "HandShake %s", success ? "Okay" : "Fail");

	spin_lock(&port->ufcs_event_lock);
	port->ufcs_events |= (success ? UFCS_HS_SUCCESS_EVENT : UFCS_HS_FAIL_EVENT);
	spin_unlock(&port->ufcs_event_lock);
	kthread_queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(ufcs_hand_shake_state);

static int ufcs_output_verify_result(union ufcs_dpm_input *input, union ufcs_dpm_output *output)
{
	struct shash_desc *desc;
	struct crypto_shash *tfm;
	bool result = false;
	u8 digest[32];
	int ret;

	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto out_verify;
	}

	desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
	if (!desc) {
		crypto_free_shash(tfm);
		ret = -ENOMEM;
		goto out_verify;
	}

	desc->tfm = tfm;
	ret = crypto_shash_init(desc);
	if (ret)
		goto out_shash;

	/* Patern = randomA + key + randomB */
	ret = crypto_shash_update(desc, input->random, 16);
	ret |= crypto_shash_update(desc, input->key, input->key_len);
	ret |= crypto_shash_update(desc, output->random, 16);
	if (ret)
		goto out_shash;

	ret = crypto_shash_final(desc, digest);
	if (ret)
		goto out_shash;

	/* Check S = S' */
	if (!memcmp(digest, output->secure, 32))
		result = true;

out_shash:
	crypto_free_shash(tfm);
	kfree(desc);
out_verify:
	output->verify_request_pass = result;
	return ret;
}

int ufcs_port_dpm_reaction(struct ufcs_port *port, enum ufcs_dpm_request request,
			   union ufcs_dpm_input *input, union ufcs_dpm_output *output)
{
	unsigned long timeout;
	bool input_ignored = true, output_ignored = true;
	int ret;

	/* Validate input/output parameter by request */
	switch (request) {
	case UFCS_DPM_POWER_REQUEST:
		if (!input || !input->req_millivolt || !input->req_milliamp || !output)
			return -EINVAL;

		input_ignored = output_ignored = false;
		break;
	case UFCS_DPM_VERIFY_REQUEST:
		if (!input || !input->key || !input->key_len || !output)
			return -EINVAL;

		input_ignored = false;
		break;
	case UFCS_DPM_CABLE_INFO:
	case UFCS_DPM_SRC_DEVICE_INFO:
	case UFCS_DPM_SRC_INFO:
	case UFCS_DPM_ERROR_INFO:
	case UFCS_DPM_SRC_CAP:
		if (!output)
			return -EINVAL;

		output_ignored = false;
		fallthrough;
	case UFCS_DPM_EXIT_UFCS_MODE:
		break;
	case UFCS_DPM_VDM:
	default:
		return -EINVAL;
	}

	mutex_lock(&port->dpm_lock);

	port->dpm_request = request;
	if (!input_ignored)
		memcpy(&port->input, input, sizeof(*input));
	if (!output_ignored)
		memset(&port->output, 0, sizeof(*output));
	reinit_completion(&port->dpm_completion);

	spin_lock(&port->ufcs_event_lock);
	port->ufcs_events |= UFCS_DPM_EVENT;
	spin_unlock(&port->ufcs_event_lock);
	kthread_queue_work(port->wq, &port->event_work);

	dev_info(&port->dev, "%s: start to wait req:%d\n", __func__, request);
	timeout = wait_for_completion_timeout(&port->dpm_completion,
					      msecs_to_jiffies(UFCS_T_DPM_REACTION));
	dev_info(&port->dev, "%s: end to wait req:%d, timeout:%lu\n",
		 __func__, request, timeout);
	if (!timeout) {
		ret = -ETIMEDOUT;
		goto out_dpm_reaction;
	}

	ret = port->dpm_error;
	if (ret)
		goto out_dpm_reaction;

	if (request == UFCS_DPM_VERIFY_REQUEST) {
		ret = ufcs_output_verify_result(&port->input, &port->output);
		if (ret)
			goto out_dpm_reaction;
	}

	if (!output_ignored)
		memcpy(output, &port->output, sizeof(*output));

out_dpm_reaction:
	mutex_unlock(&port->dpm_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(ufcs_port_dpm_reaction);

static int ufcs_port_transmit(struct ufcs_port *port, const struct ufcs_message *msg,
			      u8 payload_len)
{
	unsigned long timeout;
	int ret;
	u32 type;

	ufcs_log(port, "UFCS TX header: %#x", be16_to_cpu(msg->header));
	type = ufcs_header_type_be(msg->header);
	if (type == UFCS_MSG_CTRL)
		ufcs_log(port, "UFCS TX ctrl msg: %x", msg->ctrl.command);
	else if (type == UFCS_MSG_DATA)
		ufcs_log(port, "UFCS TX data msg cmd: %x, lens: %d", msg->data.command, msg->data.datalen);

	reinit_completion(&port->tx_complete);
	/* total message len = 2 (header) + payload_len */
	ret = port->ufcs->transmit(port->ufcs, msg, payload_len + 2);
	if (ret)
		return ret;

	mutex_unlock(&port->lock);
	timeout = wait_for_completion_timeout(&port->tx_complete,
					      msecs_to_jiffies(UFCS_T_TX_TIMEOUT));
	mutex_lock(&port->lock);
	if (!timeout)
		return -ETIMEDOUT;

	if (port->tx_status == UFCS_TX_FAIL)
		return -EAGAIN;

	port->message_id = (port->message_id + 1) & UFCS_HEADER_ID_MASK;
	return 0;
}

static int ufcs_port_send_hard_reset(struct ufcs_port *port, enum ufcs_hard_reset_type type)
{
	unsigned long timeout;
	int ret;

	ufcs_log(port, "UFCS hardreset type: %u", type);

	reinit_completion(&port->tx_complete);
	ret = port->ufcs->send_hard_reset(port->ufcs, type);
	if (ret)
		return ret;

	mutex_unlock(&port->lock);
	timeout = wait_for_completion_timeout(&port->tx_complete, msecs_to_jiffies(1000));
	mutex_lock(&port->lock);
	if (!timeout)
		return -ETIMEDOUT;

	if (port->tx_status == UFCS_TX_FAIL)
		return -EAGAIN;

	return 0;
}

static int ufcs_port_enable(struct ufcs_port *port, bool enable)
{
	return port->ufcs->enable(port->ufcs, enable);
}

static int ufcs_port_config_baud_rate(struct ufcs_port *port, enum ufcs_baud_rate rate)
{
	return port->ufcs->config_baud_rate(port->ufcs, rate);
}

static int ufcs_port_config_tx_hiz(struct ufcs_port *port, bool enable)
{
	return port->ufcs->config_tx_hiz(port->ufcs, enable);
}

static int ufcs_send_ctrl_message(struct ufcs_port *port, enum ufcs_role to, u8 cmd)
{
	struct ufcs_message msg;
	struct ufcs_ctrl_payload *ctrl = &msg.ctrl;

	memset(&msg, 0, sizeof(msg));
	msg.header = UFCS_HEADER_BE(to, port->message_id, UFCS_REV_1_0, UFCS_MSG_CTRL);

	ctrl->command = cmd;

	return ufcs_port_transmit(port, &msg, UFCS_CTRL_HEADER_SIZE);
}

static int ufcs_send_data_message(struct ufcs_port *port, enum ufcs_role to, u8 cmd, u8 len,
				  void *payload)
{
	struct ufcs_message msg;
	struct ufcs_data_payload *data = &msg.data;

	memset(&msg, 0, sizeof(msg));
	msg.header = UFCS_HEADER_BE(to, port->message_id, UFCS_REV_1_0, UFCS_MSG_DATA);

	data->command = cmd;
	data->datalen = len;
	memcpy(data->payload, payload, len);

	return ufcs_port_transmit(port, &msg, UFCS_DATA_HEADER_SIZE + len);
}

static int __maybe_unused ufcs_send_vdm_message(struct ufcs_port *port, enum ufcs_role to,
						u16 vendor, u8 len, void *payload)
{
	struct ufcs_message msg;
	struct ufcs_vdm_payload *vdm = &msg.vdm;

	memset(&msg, 0, sizeof(msg));
	msg.header = UFCS_HEADER_BE(to, port->message_id, UFCS_REV_1_0, UFCS_MSG_VDM);

	vdm->vendor = cpu_to_be16(vendor);
	vdm->datalen = len;
	memcpy(vdm->payload, payload, len);

	return ufcs_port_transmit(port, &msg, UFCS_VDM_HEADER_SIZE + len);
}

static void ufcs_reset_port(struct ufcs_port *port)
{
	/* Default to 5000mV 2000mA */
	port->req_supply_voltage = port->_req_supply_voltage = 5000;
	port->req_current_limit = port->_req_current_limit = 2000;
	memset64(port->src_caps, 0, MAX_SRC_CAP_CNT);
	port->src_cap_cnt = 0;
	memset(port->emark_info, 0, MAX_EMARK_BYTE_CNT);
	port->emark_state = UFCS_EMARK_NONE;
	port->message_id = 0;
	port->rx_msgid = -1;
	port->ignore_hard_reset = false;
	port->test_state = INVALID_STATE;
	port->attached = false;
}

static void ufcs_check_first_ping(struct ufcs_port *port)
{
	enum ufcs_state next_state = SNK_DETECT_FAIL;
	enum ufcs_baud_rate cfg_rate = UFCS_BAUD_RATE_115200;
	int i, ret;

	for (i = 0; i < UFCS_BAUD_RATE_MAX; i++) {
		ufcs_port_config_baud_rate(port, cfg_rate + i);

		ret = ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_PING);
		if (!ret) {
			next_state = SNK_ATTACHED;
			break;
		}
	}

	ufcs_set_state(port, next_state, 0);
}

static int ufcs_config_watchdog(struct ufcs_port *port)
{
	__be16 milliseconds;

	/* Config watchdog to 0 */
	milliseconds = cpu_to_be16(0);
	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_CONFIG_WATCHDOG,
				      sizeof(milliseconds), &milliseconds);
}

static int ufcs_send_capability_request(struct ufcs_port *port)
{
	u32 req_volt, req_curr;
	u64 req_msg;
	__be64 req_packet;
	bool found = false;
	int i;

	req_volt = port->req_supply_voltage;
	req_curr = port->req_current_limit;

	for (i = 0; i < port->src_cap_cnt; i++) {
		u64 src_cap = port->src_caps[i];
		u32 min_volt, max_volt, min_curr, max_curr;
		u32 step_mV, step_mA;
		bool fixed_current;

		fixed_current = src_cap_is_fixed_current(src_cap);
		min_volt = src_cap_get_min_millivolt(src_cap);
		max_volt = src_cap_get_max_millivolt(src_cap);
		min_curr = src_cap_get_min_milliamp(src_cap);
		max_curr = src_cap_get_max_milliamp(src_cap);
		step_mV = src_cap_get_step_millivolt(src_cap);
		step_mA = src_cap_get_step_milliamp(src_cap);

		if ((min_volt <= req_volt && req_volt <= max_volt) &&
		    (min_curr <= req_curr && req_curr <= max_curr)) {
			if (fixed_current) {
				req_curr = max_curr;
				step_mA = 10;
			}

			found = true;
			req_msg = put_cap_request_msg(i, req_volt, req_curr, step_mV, step_mA);
			ufcs_log(port, "found cap:%d", i);
			break;
		}
	}

	if (!found)
		return -EINVAL;

	req_packet = cpu_to_be64(req_msg);
	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_REQUEST, sizeof(req_packet),
				      &req_packet);
}

static int ufcs_send_refuse(struct ufcs_port *port, u32 type, u32 cmd, u32 reason)
{
	u32 refuse_msg = put_refuse_msg(port->rx_msgid, type, cmd, reason);
	__be32 refuse_packet;

	refuse_packet = cpu_to_be32(refuse_msg);
	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_REFUSE, sizeof(refuse_packet),
				      &refuse_packet);
}

static int ufcs_send_cable_info(struct ufcs_port *port)
{
	u8 cbl_packet[MAX_EMARK_BYTE_CNT];

	memcpy(cbl_packet, port->emark_info, MAX_EMARK_BYTE_CNT);
	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_CABLE_INFO, MAX_EMARK_BYTE_CNT,
				      cbl_packet);
}

static int ufcs_send_sink_info(struct ufcs_port *port)
{
	__be64 state_packet;
	u64 state;

	/* By default, dev_temp 25'c, conn_temp 25'c, report the requested voltage/current */
	state = FIELD_PREP(GENMASK(47, 40), 75) | FIELD_PREP(GENMASK(39, 32), 75) |
		FIELD_PREP(GENMASK(31, 16), port->req_supply_voltage / 10) |
		FIELD_PREP(GENMASK(15, 0), port->req_current_limit / 10);

	state_packet = cpu_to_be64(state);
	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_SINK_INFO, sizeof(state_packet),
				      &state_packet);
}

static int ufcs_send_device_info(struct ufcs_port *port)
{
	/* FW Major ver: 1, minor ver: 0 */
	__be64 devi_packet = cpu_to_be64(0x100);

	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_DEVICE_INFO, sizeof(devi_packet),
				      &devi_packet);
}

static int ufcs_send_error_info(struct ufcs_port *port)
{
	__be32 erri_packet = cpu_to_be32(0);

	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_ERROR_INFO, sizeof(erri_packet),
				      &erri_packet);
}

static int ufcs_send_verify_request(struct ufcs_port *port)
{
	u8 vrf_packet[17] = {0};

	vrf_packet[0] = port->input.key_id;
	memcpy(vrf_packet + 1, port->input.random, 16);

	return ufcs_send_data_message(port, UFCS_SRC, UFCS_DATA_VERIFY_REQUEST, sizeof(vrf_packet),
				      vrf_packet);
}

static void run_state_machine(struct ufcs_port *port)
{
	int ret;

	port->enter_state = port->state;
	switch (port->state) {
	case SNK_DETECT_FAIL:
		ufcs_port_enable(port, false);
		/* Notify UFCS detect fail */
		srcu_notifier_call_chain(&port->evt_nh, UFCS_NOTIFY_ATTACH_FAIL, NULL);
		ufcs_set_state(port, PORT_RESET, 0);
		break;
	case SNK_ATTACH_WAIT:
		/* Enable UFCS protocol & UFCS handshake */
		ufcs_port_enable(port, true);
		ufcs_set_state_cond(port, SNK_DETECT_FAIL, UFCS_T_WAIT_HAND_SHAKE);
		break;
	case SNK_ATTACH_FIRST_PING:
		ufcs_check_first_ping(port);
		break;
	case SNK_ATTACHED:
		port->attached = true;
		/* Notify UFCS detect OK */
		srcu_notifier_call_chain(&port->evt_nh, UFCS_NOTIFY_ATTACH_PASS, NULL);
		ufcs_set_state(port, SNK_STARTUP, 0);
		break;
	case SNK_STARTUP:
		ufcs_set_state(port, SNK_CONFIG_WATCHDOG, 0);
		break;
	case SNK_CONFIG_WATCHDOG:
		ufcs_config_watchdog(port);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_GET_OUTPUT_CAPABILITIES:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_GET_OUTPUT_CAPABILITIES);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_NEGOTIATE_CAPABILITIES:
		ufcs_send_capability_request(port);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_TRANSITION_SINK:
		ufcs_set_state_cond(port, HARD_RESET_SEND, UFCS_T_POWER_SUPPLY);
		break;
	case SNK_READY:
		break;
	case SNK_START_CABLE_DETECT:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_START_CABLE_DETECT);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_GET_CABLE_INFO:
		ufcs_send_ctrl_message(port, UFCS_EMARK, UFCS_CTRL_GET_CABLE_INFO);
		port->emark_state = UFCS_EMARK_NOT_EXIST;
		ufcs_set_state_cond(port, SNK_END_CABLE_DETECT, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_END_CABLE_DETECT:
		ufcs_port_send_hard_reset(port, UFCS_HARD_RESET_TO_CABLE);
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_END_CABLE_DETECT);
		port->ignore_hard_reset = false;
		ufcs_set_state(port, SNK_READY, 0);

		if (port->dpm_request == UFCS_DPM_CABLE_INFO) {
			if (port->emark_state == UFCS_EMARK_EXIST)
				ufcs_put_dpm_reaction_result(port, 0);
			else
				ufcs_put_dpm_reaction_result(port, -ENODEV);
		}
		break;
	case SNK_SEND_VERIFY_REQUEST:
		ufcs_send_verify_request(port);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_WAIT_VERIFY_RESPONSE:
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_VERIFY_RESPONSE);
		break;
	case SNK_GET_DEVICE_INFO:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_GET_DEVICE_INFO);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_GET_SOURCE_INFO:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_GET_SOURCE_INFO);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_GET_ERROR_INFO:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_GET_ERROR_INFO);
		ufcs_set_state_cond(port, SOFT_RESET_SEND, UFCS_T_SENDER_RESPONSE);
		break;
	case SNK_EXIT_UFCS_MODE:
		ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_EXIT_UFCS_MODE);
		ufcs_set_state_cond(port, PORT_RESET, UFCS_T_EXIT_UFCS_MODE);
		break;
	case SNK_DETECT_CABLE_INFO_RECV:
		/* Directly send cable info that we got */
		ufcs_send_cable_info(port);

		ufcs_set_state(port, SNK_READY, 0);
		break;
	case SNK_START_CABLE_DETECT_RECV:
		/* Configure Tx HiZ enable */
		ufcs_port_config_tx_hiz(port, true);

		ufcs_set_state_cond(port, SNK_END_CABLE_DETECT_RECV, UFCS_T_RESTART_TRANS);
		break;
	case SNK_END_CABLE_DETECT_RECV:
		/* Configure Tx HiZ disable */
		ufcs_port_config_tx_hiz(port, false);

		ufcs_set_state(port, SNK_READY, 0);
		break;
	case SNK_GET_SINK_INFO_RECV:
		/* Directly send sink info */
		ret = ufcs_send_sink_info(port);

		ufcs_set_state(port, ret ? SOFT_RESET_SEND : SNK_READY, 0);
		break;
	case SNK_GET_DEVICE_INFO_RECV:
		/* Send device FW version */
		ret = ufcs_send_device_info(port);

		ufcs_set_state(port, ret ? SOFT_RESET_SEND : SNK_READY, 0);
		break;
	case SNK_GET_ERROR_INFO_RECV:
		/* Send device error info */
		ret = ufcs_send_error_info(port);

		ufcs_set_state(port, ret ? SOFT_RESET_SEND : SNK_READY, 0);
		break;
	case SNK_TEST_REQUEST_RECV:
		ufcs_set_state(port, port->test_state, 0);
		break;
	case SOFT_RESET:
		/* Receive soft reset from port partner */
		ufcs_set_state(port, SNK_SOFT_RESET, 0);
		break;
	case SNK_SOFT_RESET:
		/* Do soft reset actions */
		ufcs_port_config_tx_hiz(port, false);
		port->req_supply_voltage = port->_req_supply_voltage;
		port->req_current_limit = port->_req_current_limit;
		port->message_id = 0;
		port->rx_msgid = -1;
		port->ignore_hard_reset = false;
		port->test_state = INVALID_STATE;

		if (port->dpm_request != UFCS_DPM_NONE)
			ufcs_put_dpm_reaction_result(port, -EINVAL);

		ufcs_set_state(port, SNK_STARTUP, 0);
		break;
	case SOFT_RESET_SEND:
		ret = ufcs_send_ctrl_message(port, UFCS_SRC, UFCS_CTRL_SOFT_RESET);

		ufcs_set_state(port, ret ? HARD_RESET_SEND : SNK_SOFT_RESET, 0);
		break;
	case HARD_RESET:
		ufcs_set_state(port, PORT_RESET, 0);
		break;
	case HARD_RESET_SEND:
		ufcs_port_send_hard_reset(port, UFCS_HARD_RESET_TO_SRC);
		ufcs_set_state(port, PORT_RESET, 0);
		break;
	case PORT_RESET:
		ufcs_reset_port(port);
		ufcs_port_enable(port, false);

		srcu_notifier_call_chain(&port->evt_nh, UFCS_NOTIFY_ATTACH_NONE, NULL);

		switch (port->dpm_request) {
		case UFCS_DPM_EXIT_UFCS_MODE:
			ufcs_put_dpm_reaction_result(port, 0);
			break;
		case UFCS_DPM_NONE:
			break;
		default:
			ufcs_put_dpm_reaction_result(port, -EINVAL);
			break;
		}

		break;
	default:
		WARN(1, "Unexpected port state %d\n", port->state);
		break;
	}
}

static void ufcs_state_machine_work(struct kthread_work *work)
{
	struct ufcs_port *port = container_of(work, struct ufcs_port, state_machine);
	enum ufcs_state prev_state;

	mutex_lock(&port->lock);
	port->state_machine_running = true;

	/* If we were queued due to a delayed state change, update it now */
	if (port->delayed_state != INVALID_STATE) {
		ufcs_log(port, "state change %s -> %s [delayed %ld ms]", ufcs_states[port->state],
			 ufcs_states[port->delayed_state], port->delayed_ms);

		port->prev_state = port->state;
		port->state = port->delayed_state;
		port->delayed_state = INVALID_STATE;
	}

	do {
		prev_state = port->state;
		run_state_machine(port);
	} while (port->state != prev_state && port->delayed_state == INVALID_STATE);

	port->state_machine_running = false;
	mutex_unlock(&port->lock);
}

static enum hrtimer_restart state_machine_timer_handler(struct hrtimer *timer)
{
	struct ufcs_port *port = container_of(timer, struct ufcs_port, state_machine_timer);

	if (port->registered)
		kthread_queue_work(port->wq, &port->state_machine);

	return HRTIMER_NORESTART;
}

static void ufcs_port_init(struct ufcs_port *port)
{
	port->ufcs->init(port->ufcs);

	ufcs_set_state(port, PORT_RESET, 0);
}

int register_ufcs_dev_notifier(struct ufcs_port *port, struct notifier_block *nb)
{
	if (!port)
		return -ENODEV;

	return srcu_notifier_chain_register(&port->evt_nh, nb);
}
EXPORT_SYMBOL(register_ufcs_dev_notifier);

int unregister_ufcs_dev_notifier(struct ufcs_port *port, struct notifier_block *nb)
{
	if (!port)
		return -ENODEV;

	return srcu_notifier_chain_unregister(&port->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_ufcs_dev_notifier);

struct ufcs_port *ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs)
{
	static atomic_t ufcs_no = ATOMIC_INIT(-1);
	struct ufcs_port *port;
	int err;

	if (!dev || !ufcs || !ufcs->init || !ufcs->enable || !ufcs->config_baud_rate ||
	    !ufcs->transmit || !ufcs->send_hard_reset)
		return ERR_PTR(-EINVAL);

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return ERR_PTR(-ENOMEM);

	device_initialize(&port->dev);

	port->dev.class = ufcs_class;
	port->dev.parent = dev;
	device_set_node(&port->dev, dev_fwnode(dev));
	dev_set_name(&port->dev, "port.%lu", (unsigned long)atomic_inc_return(&ufcs_no));
	dev_set_drvdata(&port->dev, port);

	port->ufcs = ufcs;

	port->wq = kthread_create_worker(0, "%s", dev_name(dev));
	if (IS_ERR(port->wq))
		return ERR_CAST(port->wq);
	sched_set_fifo(port->wq->task);

	mutex_init(&port->lock);
	kthread_init_work(&port->state_machine, ufcs_state_machine_work);
	kthread_init_work(&port->event_work, ufcs_event_handler);
	hrtimer_init(&port->state_machine_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	port->state_machine_timer.function = state_machine_timer_handler;

	spin_lock_init(&port->ufcs_event_lock);
	init_completion(&port->tx_complete);

	srcu_init_notifier_head(&port->evt_nh);

	mutex_init(&port->dpm_lock);
	init_completion(&port->dpm_completion);

	err = device_add(&port->dev);
	if (err)
		return ERR_PTR(err);

	ufcs_debugfs_init(port);
	atomic_inc(&port->use_cnt);
	port->registered = true;

	mutex_lock(&port->lock);
	ufcs_port_init(port);
	mutex_unlock(&port->lock);

	return port;
}
EXPORT_SYMBOL_GPL(ufcs_register_port);

void ufcs_unregister_port(struct ufcs_port *port)
{
	WARN_ON(atomic_dec_return(&port->use_cnt));
	port->registered = false;

	kthread_destroy_worker(port->wq);

	mutex_destroy(&port->lock);
	hrtimer_cancel(&port->state_machine_timer);

	ufcs_debugfs_exit(port);
	device_unregister(&port->dev);
}
EXPORT_SYMBOL_GPL(ufcs_unregister_port);

static void devm_ufcs_port_release(struct device *dev, void *res)
{
	ufcs_unregister_port(*(struct ufcs_port **)res);
}

struct ufcs_port *devm_ufcs_register_port(struct device *dev, struct ufcs_dev *ufcs)
{
	struct ufcs_port **ptr, *port;

	port = ufcs_register_port(dev, ufcs);
	if (IS_ERR(port))
		return port;

	ptr = devres_alloc(devm_ufcs_port_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr) {
		ufcs_unregister_port(port);
		return ERR_PTR(-ENOMEM);
	}

	*ptr = port;
	devres_add(dev, ptr);

	return port;
}
EXPORT_SYMBOL_GPL(devm_ufcs_register_port);

static int ufcs_port_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct ufcs_port *port = dev_get_drvdata(dev);

	return strcmp(dev_name(&port->dev), name) == 0;
}

struct ufcs_port *ufcs_port_get_by_name(const char *name)
{
	struct ufcs_port *port = NULL;
	struct device *dev = class_find_device(ufcs_class, NULL, name,
					       ufcs_port_match_device_by_name);

	if (dev) {
		port = dev_get_drvdata(dev);
		atomic_inc(&port->use_cnt);
	}

	return port;

}
EXPORT_SYMBOL(ufcs_port_get_by_name);

void ufcs_port_put(struct ufcs_port *port)
{
	might_sleep();

	atomic_dec(&port->use_cnt);
	put_device(&port->dev);
}
EXPORT_SYMBOL(ufcs_port_put);

#ifdef CONFIG_OF
static int ufcs_port_match_device_node(struct device *dev, const void *data)
{
	return dev->of_node && dev->of_node == (struct device_node *)data;
}

struct ufcs_port *ufcs_port_get_by_phandle(struct device_node *np, const char *property)
{
	struct device_node *port_np;
	struct ufcs_port *port = NULL;
	struct device *dev;

	port_np = of_parse_phandle(np, property, 0);
	if (!port_np)
		return ERR_PTR(-ENODEV);

	dev = class_find_device(ufcs_class, NULL, port_np, ufcs_port_match_device_node);

	of_node_put(port_np);

	if (dev) {
		port = dev_get_drvdata(dev);
		atomic_inc(&port->use_cnt);
	}

	return port;
}
EXPORT_SYMBOL(ufcs_port_get_by_phandle);

static void devm_ufcs_port_put(struct device *dev, void *res)
{
	struct ufcs_port **port = res;

	ufcs_port_put(*port);
}

struct ufcs_port *devm_ufcs_port_get_by_phandle(struct device *dev, const char *property)
{
	struct ufcs_port **ptr, *port;

	if (!dev->of_node)
		return ERR_PTR(-ENODEV);

	ptr = devres_alloc(devm_ufcs_port_put, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	port = ufcs_port_get_by_phandle(dev->of_node, property);
	if (IS_ERR_OR_NULL(port)) {
		devres_free(ptr);
	} else {
		*ptr = port;
		devres_add(dev, ptr);
	}

	return port;
}
EXPORT_SYMBOL(devm_ufcs_port_get_by_phandle);
#endif /* CONFIG_OF */

static ssize_t attach_store(struct device *dev, struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct ufcs_port *port = dev_get_drvdata(dev);
	bool attached;
	int ret;

	ret = kstrtobool(buf, &attached);
	if (ret)
		return ret;

	ufcs_attach_change(port, attached);

	return count;
}
static DEVICE_ATTR_WO(attach);

static ssize_t ufcs_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"1:UFCS_DPM_POWER_REQUEST",
			"2:UFCS_DPM_VERIFY_REQUEST",
			"3:UFCS_DPM_CABLE_INFO",
			"4:UFCS_DPM_SRC_DEVICE_INFO",
			"5:UFCS_DPM_SRC_INFO",
			"6:UFCS_DPM_ERROR_INFO",
			"7:UFCS_DPM_VDM",
			"8:UFCS_DPM_SRC_CAP",
			"9:UFCS_DPM_EXIT_UFCS_MODE");
}

static ssize_t ufcs_test_store(struct device *dev, struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct ufcs_port *port = dev_get_drvdata(dev);
	int dpm_req;
	int i, ret, mv, ma;
	union ufcs_dpm_input input;
	union ufcs_dpm_output output;
	char *temp_buf = (char *)buf;
	char *token = strsep(&temp_buf, " ");

	if (!token)
		return -EINVAL;
	ret = kstrtouint(token, 10, &dpm_req);
	if (ret)
		return ret;

	ufcs_log(port, "DPM Request Test: %d", dpm_req);

	switch (dpm_req) {
	case UFCS_DPM_POWER_REQUEST:
		token = strsep(&temp_buf, " ");
		if (token)
			ret = kstrtouint(token, 10, &mv);
		token = strsep(&temp_buf, " ");
		if (token)
			ret |= kstrtouint(token, 10, &ma);
		if (ret) {
			ufcs_log(port, "failed to get mv,ma");
			return ret;
		}
		ufcs_log(port, "power request %d mV %d mA", mv, ma);
		input.req_millivolt = mv;
		input.req_milliamp = ma;
		break;
	case UFCS_DPM_VERIFY_REQUEST:
	case UFCS_DPM_CABLE_INFO:
	case UFCS_DPM_SRC_DEVICE_INFO:
	case UFCS_DPM_SRC_INFO:
	case UFCS_DPM_ERROR_INFO:
	case UFCS_DPM_SRC_CAP:
	case UFCS_DPM_EXIT_UFCS_MODE:
		break;
	case UFCS_DPM_VDM:
		ufcs_log(port, "vdm is todo item");
		break;
	default:
		ufcs_log(port, "unknown dpm req:%d", dpm_req);
		return -EINVAL;
	}

	ret = ufcs_port_dpm_reaction(port, dpm_req, &input, &output);
	ufcs_log(port, "DPM Request Test: %d, ret=%d", dpm_req, ret);
	if (ret < 0)
		return ret;

	switch (dpm_req) {
	case UFCS_DPM_POWER_REQUEST:
		ufcs_log(port, "power request ready:%d", output.power_request_ready);
		break;
	case UFCS_DPM_VERIFY_REQUEST:
		ufcs_log(port, "verify pass:%d", output.verify_request_pass);
		break;
	case UFCS_DPM_CABLE_INFO:
		ufcs_log(port, "[cable info] vid1 0x%04x vid2 0x%04x %d mOhm %d mV %d mA",
			 output.cbl_vid1, output.cbl_vid2, output.cbl_mOhm, output.cbl_mVolt,
			 output.cbl_mAmp);
		break;
	case UFCS_DPM_SRC_DEVICE_INFO:
		ufcs_log(port, "[device info] hw_ver:0x%02x sw_ver:0x%02x",
			 output.hw_ver, output.sw_ver);
		break;
	case UFCS_DPM_SRC_INFO:
		ufcs_log(port, "[src info] dev temp:%d, conn temp:%d, %d mV %d mA",
			 output.device_temp, output.conn_temp, output.output_millivolt,
			 output.output_milliamp);
		break;
	case UFCS_DPM_ERROR_INFO:
		ufcs_log(port, "error info:0x%04x", output.raw32);
		break;
	case UFCS_DPM_VDM:
		ufcs_log(port, "vdm is todo item again");
		break;
	case UFCS_DPM_SRC_CAP:
		for (i = 0; i < output.src_cap_cnt; i++)
			ufcs_log(port, "src_cap[%d] %d ~ %d mV, %d ~ %d mA", i,
				 output.src_cap[i].min_mV, output.src_cap[i].max_mV,
				 output.src_cap[i].min_mA, output.src_cap[i].max_mA);
		break;
	case UFCS_DPM_EXIT_UFCS_MODE:
		ufcs_log_force(port, "exit ufcs mode");
		break;
	default:
		ufcs_log(port, "unknown dpm req:%d", dpm_req);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(ufcs_test);

static struct attribute *ufcs_attrs[] = {
	&dev_attr_attach.attr,
	&dev_attr_ufcs_test.attr,
	NULL
};
ATTRIBUTE_GROUPS(ufcs);

static int __init ufcs_class_init(void)
{
	ufcs_class = class_create("ufcs");
	if (IS_ERR(ufcs_class))
		return PTR_ERR(ufcs_class);

	ufcs_class->dev_groups = ufcs_groups;
	return 0;
}

static void __exit ufcs_class_exit(void)
{
	class_destroy(ufcs_class);
}

subsys_initcall(ufcs_class_init);
module_exit(ufcs_class_exit);

MODULE_DESCRIPTION("Universal Fast Charging Spec port class");
MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_LICENSE("GPL");
