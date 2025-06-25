/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */


#ifndef __CCCI_CCMNI_H__
#define __CCCI_CCMNI_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/pm_wakeup.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/timer.h>
#include <linux/if_ether.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include "mt-plat/mtk_ccci_common.h"

/*
 * normal workqueue:   MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * workqueue with GRO: MODEM_CAP_NAPI=0, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=1
 * NAPI without GRO:   MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=0, ENABLE_WQ_GRO=0
 * NAPI with GRO:      MODEM_CAP_NAPI=1, ENABLE_NAPI_GRO=1, ENABLE_WQ_GRO=0
 */
/* #define ENABLE_NAPI_GRO */
#define ENABLE_WQ_GRO

#define  CCMNI_MTU		1500
#define  CCMNI_TX_QUEUE		1000
#define  CCMNI_NETDEV_WDT_TO	(1*HZ)

#define  IPV4_VERSION		0x40
#define  IPV6_VERSION		0x60

/* stop/start tx queue */
#define  SIOCSTXQSTATE		(SIOCDEVPRIVATE + 0)
/* configure ccmni/md remapping */
#define  SIOCCCMNICFG		(SIOCDEVPRIVATE + 1)
/* forward filter for ccmni tx packet */
#define  SIOCFWDFILTER		(SIOCDEVPRIVATE + 2)
/* disable ack first mechanism */
#define  SIOCACKPRIO		(SIOCDEVPRIVATE + 3)
/* push the queued packet to stack */
#define  SIOPUSHPENDING		(SIOCDEVPRIVATE + 4)
/* get HW queue id which is set to MD*/
#define  SIOCGQID		(SIOCDEVPRIVATE + 12)
/* set HW queue to MD*/
#define  SIOCSQID		(SIOCDEVPRIVATE + 13)
/* get networktype value which is set to MD */
#define  SIOCGNETTYPE		(SIOCDEVPRIVATE + 14)
/* set networktype to MD */
#define  SIOCSNETTYPE		(SIOCDEVPRIVATE + 15)

/*
 * This feature increase ccmni num to 30
 */
#define MTK_TC10_FEATURE_CCMNI_PORT
#if defined(MTK_TC10_FEATURE_CCMNI_PORT) || defined(CONFIG_MTK_SRIL_SUPPORT)
#define  CCMNI_INTERFACE_NUM    30
#else
#define  CCMNI_INTERFACE_NUM    21
#endif

#define  DPMAIF_DRIVER          (1<<2)
#define  CLDMA_DRIVER           (1<<0)

#define  CCMNI_TX_PRINT_F	(0x1 << 0)
#define  MDT_TAG_PATTERN	0x46464646
#define  CCMNI_FLT_NUM		32

/* #define CCMNI_MET_DEBUG */
#if defined(CCMNI_MET_DEBUG)
#define MET_LOG_TIMER		20 /*20ms*/
#define CCMNI_RX_MET_ID		0xF0000
#define CCMNI_TX_MET_ID		0xF1000
#endif

struct ccmni_ch {
	int		   rx;
	int		   rx_ack;
	int		   tx;
	int		   tx_ack;
	int		   dl_ack;
};

struct ccmni_ch_hwq {
	unsigned int      hwqno;
/* 0: default, 1: setting HW queue by rpc msg, 2: setting HW queue by ioctl */
	unsigned int      ioctl_or_rpc;
	spinlock_t        *spinlock_channel;
};

struct ccmni_tx_para_info {
	struct sk_buff *skb;
	unsigned int    ccmni_idx;
	unsigned int    network_type;
	unsigned int    hw_qno;
	unsigned int    count_l;
};

enum {
	CCMNI_FLT_ADD    = 1,
	CCMNI_FLT_DEL    = 2,
	CCMNI_FLT_FLUSH  = 3,
};

struct ccmni_fwd_filter {
	u16 ver;           /* ipv4 or ipv6*/
	u8 s_pref;         /* mask number for source ip address */
	u8 d_pref;         /* mask number for dest ip address */
	union {
		struct {
			u32 saddr; /* source ip address */
			u32 daddr; /* dest ip address */
		} ipv4;
		struct {
			u32 saddr[4];
			u32 daddr[4];
		} ipv6;
	};
};

struct ccmni_flt_act {
	u32 action;
	struct ccmni_fwd_filter flt;
};

struct ccmni_instance {
	unsigned int       index;
	struct ccmni_ch    *ch;
	struct ccmni_ch_hwq ch_hwq;
	unsigned int       net_type;
	unsigned int	   ack_prio_en;
	atomic_t           usage;
	/* use pointer to keep these items unique,
	 * while switching between CCMNI instances
	 */
	struct timer_list  *timer;
	struct net_device  *dev;
	struct napi_struct *napi;
	unsigned int       rx_seq_num;
	unsigned int       flags[MD_HW_Q_MAX];
	spinlock_t         *spinlock;
	struct ccmni_ctl_block  *ctlb;
	unsigned long      tx_busy_cnt[MD_HW_Q_MAX];
	unsigned int       rx_gro_cnt;
	unsigned int       flt_cnt;
	struct ccmni_fwd_filter flt_tbl[CCMNI_FLT_NUM];
#if defined(CCMNI_MET_DEBUG)
	unsigned long      rx_met_time;
	unsigned long      tx_met_time;
	unsigned long      rx_met_bytes;
	unsigned long      tx_met_bytes;
#endif
	void               *priv_data;

	struct sk_buff_head rx_list;
	atomic_t           is_up; /*for ccmni status*/
	/* For queue packet before ready */
	struct workqueue_struct *worker;
	struct delayed_work pkt_queue_work;
};

struct ccmni_ccci_cfg {
	int                ccmni_ver;   /* CCMNI_DRV_VER */
	/* "ccmni" or "cc2mni" or "ccemni" */
	unsigned char      name[16];
	unsigned int       md_ability;
	unsigned int       napi_poll_weigh;
};

struct ccmni_ctl_block {
	struct ccmni_ccci_cfg   *ccci_cfg;
	struct ccmni_instance   *ccmni_inst[32];
	unsigned int       md_sta;
	struct wakeup_source   *ccmni_wakelock;
	char               wakelock_name[16];
	unsigned long long net_rx_delay[4];
};

struct ccmni_dev_ops {
	/* must-have */
	int  skb_alloc_size;
	int (*recv_skb)(unsigned int ccmni_idx, struct sk_buff *skb);
	void (*md_state_callback)(unsigned int ccmni_idx, enum MD_STATE state);
	void (*dump)(void);
	void (*dump_rx_status)(unsigned long long *status);
	struct ccmni_ch *(*get_ch)(unsigned int ccmni_idx);
	struct ccmni_ch_hwq *(*get_ch_hwq)(unsigned int ccmni_idx);
	int (*is_ack_skb)(struct sk_buff *skb);
	unsigned int (*flush_queue)(unsigned int ccmni_idx);
	int (*stop_queue)(unsigned int ccmni_idx, unsigned int que_idx);
	int (*start_queue)(unsigned int ccmni_idx, unsigned int que_idx);
	int (*send_skb)(struct ccmni_tx_para_info *tx_info);
};

struct md_drt_tag {
	u8  in_netif_id;
	u8  out_netif_id;
	u16 port;
};

struct md_tag_packet {
	u32 guard_pattern;
	struct md_drt_tag info;
};

enum {
	/* for eemcs/eccci */
	CCMNI_DRV_V0   = 0,
	/* for dual_ccci ccmni_v1 */
	CCMNI_DRV_V1   = 1,
	/* for dual_ccci ccmni_v2 */
	CCMNI_DRV_V2   = 2,
};

enum {
	/* ccci send pkt success */
	CCMNI_ERR_TX_OK = 0,
	/* ccci tx packet buffer full and tx fail */
	CCMNI_ERR_TX_BUSY = -1,
	/* modem not ready and tx fail */
	CCMNI_ERR_MD_NO_READY = -2,
	/* modem not ready and tx fail */
	CCMNI_ERR_TX_INVAL = -3,
};

extern struct ccmni_dev_ops ccmni_ops;

#endif /* __CCCI_CCMNI_H__ */
