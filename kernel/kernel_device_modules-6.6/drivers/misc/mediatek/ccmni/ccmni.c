// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Cross Chip Modem Network Interface
 *
 * Author:
 * -------
 *   Anny.Hu(mtk80401)
 *
 ****************************************************************************/
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/of.h>
#include <linux/tcp.h>
#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/sch_generic.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/preempt.h>
#include <linux/stacktrace.h>
#include <linux/sysctl.h>
#include <linux/sched/clock.h>
#include "ccmni.h"
#include "ccci_debug.h"
#include "ccci_core.h"
#include "rps_perf.h"
#if defined(CCMNI_MET_DEBUG)
#include <mt-plat/met_drv.h>
#endif


struct ccmni_ctl_block *ccmni_ctl_blk;
EXPORT_SYMBOL(ccmni_ctl_blk);

struct ccmni_ccci_cfg eccci_ccmni_cfg = {
	.ccmni_ver = CCMNI_DRV_V0,
#ifdef CONFIG_MTK_SRIL_SUPPORT
	.name = "rmnet",
#else
	.name = "ccmni",
#endif
	.md_ability = MODEM_CAP_DATA_ACK_DVD | MODEM_CAP_CCMNI_MQ,
	.napi_poll_weigh = NAPI_POLL_WEIGHT,
};

static struct ccmni_ch ccmni_ch_arr[] = {
	{CCCI_CCMNI1_RX, 0xFF, CCCI_CCMNI1_TX, 0xFF, CCCI_CCMNI1_DL_ACK},
	{CCCI_CCMNI2_RX, 0xFF, CCCI_CCMNI2_TX, 0xFF, CCCI_CCMNI2_DL_ACK},
	{CCCI_CCMNI3_RX, 0xFF, CCCI_CCMNI3_TX, 0xFF, CCCI_CCMNI3_TX},
	{CCCI_CCMNI4_RX, 0xFF, CCCI_CCMNI4_TX, 0xFF, CCCI_CCMNI4_TX},
	{CCCI_CCMNI5_RX, 0xFF, CCCI_CCMNI5_TX, 0xFF, CCCI_CCMNI5_TX},
	{CCCI_CCMNI6_RX, 0xFF, CCCI_CCMNI6_TX, 0xFF, CCCI_CCMNI6_TX},
	{CCCI_CCMNI7_RX, 0xFF, CCCI_CCMNI7_TX, 0xFF, CCCI_CCMNI7_TX},
	{CCCI_CCMNI8_RX, 0xFF, CCCI_CCMNI8_TX, 0xFF, CCCI_CCMNI8_TX},
	{CCCI_CCMNI9_RX, 0xFF, CCCI_CCMNI9_TX, 0xFF, CCCI_CCMNI9_TX},
	{CCCI_CCMNI10_RX, 0xFF, CCCI_CCMNI10_TX, 0xFF, CCCI_CCMNI10_TX},
	{CCCI_CCMNI11_RX, 0xFF, CCCI_CCMNI11_TX, 0xFF, CCCI_CCMNI11_TX},
	{CCCI_CCMNI12_RX, 0xFF, CCCI_CCMNI12_TX, 0xFF, CCCI_CCMNI12_TX},
	{CCCI_CCMNI13_RX, 0xFF, CCCI_CCMNI13_TX, 0xFF, CCCI_CCMNI13_TX},
	{CCCI_CCMNI14_RX, 0xFF, CCCI_CCMNI14_TX, 0xFF, CCCI_CCMNI14_TX},
	{CCCI_CCMNI15_RX, 0xFF, CCCI_CCMNI15_TX, 0xFF, CCCI_CCMNI15_TX},
	{CCCI_CCMNI16_RX, 0xFF, CCCI_CCMNI16_TX, 0xFF, CCCI_CCMNI16_TX},
	{CCCI_CCMNI17_RX, 0xFF, CCCI_CCMNI17_TX, 0xFF, CCCI_CCMNI17_TX},
	{CCCI_CCMNI18_RX, 0xFF, CCCI_CCMNI18_TX, 0xFF, CCCI_CCMNI18_TX},
	{CCCI_CCMNI19_RX, 0xFF, CCCI_CCMNI19_TX, 0xFF, CCCI_CCMNI19_TX},
	{CCCI_CCMNI20_RX, 0xFF, CCCI_CCMNI20_TX, 0xFF, CCCI_CCMNI20_TX},
	{CCCI_CCMNI21_RX, 0xFF, CCCI_CCMNI21_TX, 0xFF, CCCI_CCMNI21_TX},
#if defined(MTK_TC10_FEATURE_CCMNI_PORT) || defined(CONFIG_MTK_SRIL_SUPPORT)
	{CCCI_CCMNI22_RX, 0xFF, CCCI_CCMNI22_TX, 0xFF, CCCI_CCMNI22_TX},
	{CCCI_CCMNI23_RX, 0xFF, CCCI_CCMNI23_TX, 0xFF, CCCI_CCMNI23_TX},
	{CCCI_CCMNI24_RX, 0xFF, CCCI_CCMNI24_TX, 0xFF, CCCI_CCMNI24_TX},
	{CCCI_CCMNI25_RX, 0xFF, CCCI_CCMNI25_TX, 0xFF, CCCI_CCMNI25_TX},
	{CCCI_CCMNI26_RX, 0xFF, CCCI_CCMNI26_TX, 0xFF, CCCI_CCMNI26_TX},
	{CCCI_CCMNI27_RX, 0xFF, CCCI_CCMNI27_TX, 0xFF, CCCI_CCMNI27_TX},
	{CCCI_CCMNI28_RX, 0xFF, CCCI_CCMNI28_TX, 0xFF, CCCI_CCMNI28_TX},
	{CCCI_CCMNI29_RX, 0xFF, CCCI_CCMNI29_TX, 0xFF, CCCI_CCMNI29_TX},
	{CCCI_CCMNI30_RX, 0xFF, CCCI_CCMNI30_TX, 0xFF, CCCI_CCMNI30_TX},
#endif
};

/* Time in ns. This number must be less than 500ms. */
#ifdef ENABLE_WQ_GRO
long gro_flush_timer __read_mostly = 2000000L;
#else
long gro_flush_timer;
#endif
static unsigned long g_init_rps_value;
/*VIP_MARK is defined as highest priority */
#define APP_VIP_MARK		0x80000000
#define APP_VIP_MARK2		0x40000000

#define DEV_OPEN                1
#define DEV_CLOSE               0
#define MAX_MTU                 3000
static unsigned long timeout_flush_num, clear_flush_num;
static u64 g_cur_dl_speed = 0xFFFFFFFFFFFFFFFF;
static u32 g_tcp_is_need_gro = 1;
/*
 * Register the sysctl to set tcp_pacing_shift.
 */
static int sysctl_tcp_pacing_shift;
static struct ctl_table tcp_pacing_table[] = {
	{
		.procname	= "tcp_pacing_shift",
		.data		= &sysctl_tcp_pacing_shift,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
	},
	{}
};

static int ccmni_rx_callback(unsigned int ccmni_idx, struct sk_buff *skb,
		void *priv_data);
static struct ctl_table_header *sysctl_header;

static int register_tcp_pacing_sysctl(void)
{
	sysctl_header = register_sysctl("net", tcp_pacing_table);
	if (sysctl_header == NULL) {
		pr_info("CCCI:CCMNI:register tcp_pacing failed\n");
		return -1;
	}
	return 0;
}

/*static void unregister_tcp_pacing_sysctl(void)
 *{
 *	if (sysctl_header)
 *		unregister_sysctl_table(sysctl_header);
 *}
 */

void ccmni_set_init_rps(unsigned long rps_value)
{
	g_init_rps_value = rps_value;
}
EXPORT_SYMBOL(ccmni_set_init_rps);

void set_ccmni_rps(unsigned long value)
{
	int i = 0;

	if (ccmni_ctl_blk == NULL) {
		pr_info("%s: invalid ctlb\n", __func__);
		return;
	}

	for (i = 0; i < CCMNI_INTERFACE_NUM; i++) {
		if (ccmni_ctl_blk->ccmni_inst[i])
			set_rps_map(ccmni_ctl_blk->ccmni_inst[i]->dev->_rx, value);
	}
}
EXPORT_SYMBOL(set_ccmni_rps);

void ccmni_set_cur_speed(u64 cur_dl_speed)
{
	g_cur_dl_speed = cur_dl_speed;
}
EXPORT_SYMBOL(ccmni_set_cur_speed);

void ccmni_set_tcp_is_need_gro(u32 tcp_is_need_gro)
{
	g_tcp_is_need_gro = tcp_is_need_gro;
}
EXPORT_SYMBOL(ccmni_set_tcp_is_need_gro);


static inline int is_ack_skb(struct sk_buff *skb)
{
	struct tcphdr *tcph;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	u8 nexthdr;
	__be16 frag_off;
	u32 l4_off, total_len, packet_type;
	int ret = 0;
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN)
		count = sizeof(tag->info);

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		ip6h = (struct ipv6hdr *)skb->data;
		total_len = sizeof(struct ipv6hdr) +
			    ntohs(ip6h->payload_len);

		/* copy md tag into skb->tail and
		 * skb->len > 128B(md recv buf size)
		 */
		/* this case will cause md EE */
		if (total_len <= 128 - sizeof(struct ccci_header) - count) {
			nexthdr = ip6h->nexthdr;
			l4_off = ipv6_skip_exthdr(skb,
				sizeof(struct ipv6hdr),
				&nexthdr, &frag_off);

			if (nexthdr == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + l4_off);

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					((total_len - l4_off) ==
						(tcph->doff << 2)))
					ret = 1;
			}
		}
	} else if (packet_type == IPV4_VERSION) {
		iph = (struct iphdr *)skb->data;
		if (ntohs(iph->tot_len) <=
				128 - sizeof(struct ccci_header) - count) {

			if (iph->protocol == IPPROTO_TCP) {
				tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));

				if (tcph->syn)
					ret = 1;
				else if (!tcph->fin && !tcph->rst &&
					ntohs(iph->tot_len) ==
					(iph->ihl << 2) + (tcph->doff << 2)) {
					ret = 1;
				}
			}
		}
	}

	return ret;
}

#ifdef ENABLE_WQ_GRO
static int is_skb_gro(struct sk_buff *skb)
{
	u32 packet_type;
	u32 protocol = 0xFFFFFFFF;

	packet_type = skb->data[0] & 0xF0;

	if (packet_type == IPV4_VERSION)
		protocol = ip_hdr(skb)->protocol;
	else if (packet_type == IPV6_VERSION)
		protocol = ipv6_hdr(skb)->nexthdr;

	if (protocol == IPPROTO_TCP) {
		return g_tcp_is_need_gro;
	} else if (protocol == IPPROTO_UDP) {
		/* UDP always do GRO */
		if (g_cur_dl_speed > 300000000LL) //>300Mbps
			return 1;
	}

	return 0;
}

static inline unsigned int napi_gro_list_flush(struct ccmni_instance *ccmni)
{
	struct napi_struct *napi;
	unsigned int rx_count;

	napi = ccmni->napi;
	rx_count = napi->rx_count;

	napi_gro_flush(napi, false);
	if (napi->rx_count) {
		netif_receive_skb_list(&napi->rx_list);
		INIT_LIST_HEAD(&napi->rx_list);
		napi->rx_count = 0;
	}

	return rx_count;
}

#endif

static inline int ccmni_forward_rx(struct ccmni_instance *ccmni,
	struct sk_buff *skb)
{
	bool flt_ok = false;
	bool flt_flag = true;
	unsigned int pkt_type;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	struct ccmni_fwd_filter flt_tmp;
	unsigned int i, j;
	u16 mask;
	u32 *addr1, *addr2;

	if (ccmni->flt_cnt) {
		for (i = 0; i < CCMNI_FLT_NUM; i++) {
			flt_tmp = ccmni->flt_tbl[i];
			pkt_type = skb->data[0] & 0xF0;
			if (!flt_tmp.ver || (flt_tmp.ver != pkt_type))
				continue;

			if (pkt_type == IPV4_VERSION) {
				iph = (struct iphdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = &iph->saddr;
				addr2 = &flt_tmp.ipv4.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = &iph->daddr;
					addr2 = &flt_tmp.ipv4.daddr;
				}
			} else if (pkt_type == IPV6_VERSION) {
				ip6h = (struct ipv6hdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = ip6h->saddr.s6_addr32;
				addr2 = flt_tmp.ipv6.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask == 0) {
						mask = flt_tmp.d_pref;
						addr1 = ip6h->daddr.s6_addr32;
						addr2 = flt_tmp.ipv6.daddr;
						continue;
					}
					if (mask <= 32 &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 64 &&
						(addr1[0] != addr2[0] ||
						addr1[1] >> (64 - mask) !=
						addr2[1] >> (64 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 96 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] >> (96 - mask) !=
						addr2[2] >> (96 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 128 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] != addr2[2] ||
						addr1[3] >> (128 - mask) !=
						addr2[3] >> (128 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = ip6h->daddr.s6_addr32;
					addr2 = flt_tmp.ipv6.daddr;
				}
			}
			if (flt_flag) {
				flt_ok = true;
				break;
			}
		}

		if (flt_ok) {
			skb->ip_summed = CHECKSUM_NONE;
			skb_set_mac_header(skb, -ETH_HLEN);
			netif_rx(skb);
			return NETDEV_TX_OK;
		}
	}

	return -1;
}


/********************netdev register function********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
	struct net_device *sb_dev/*, select_queue_fallback_t fallback */)
{
	u16 ret = MD_HW_NORMAL_Q;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (unlikely(ccmni_ctl_blk == NULL)) {
		netdev_info(dev, "invalid ccmni_ctl_blk");
		return ret;
	}
	if ((ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_DATA_ACK_DVD) &&
		!ccmni->ch_hwq.ioctl_or_rpc) {

		if (skb->mark & APP_VIP_MARK) {
			ret = MD_HW_HIGH_Q; /* highest priority */
		} else if (skb->mark & APP_VIP_MARK2) {
			ret = MD_HW_MEDIUM_Q;
		} else if (ccmni->ack_prio_en) {
			if (is_ack_skb(skb))
				ret = MD_HW_HIGH_Q;
			else
				ret = MD_HW_NORMAL_Q;
		}
	} else if (ccmni->ch_hwq.ioctl_or_rpc)
		ret = ccmni->ch_hwq.hwqno;
	return ret;
}

static int recv_from_rx_list(struct sk_buff_head *rx_list, unsigned int ccmni_idx)
{
	unsigned long flags;
	struct sk_buff *skb = NULL;

	spin_lock_irqsave(&rx_list->lock, flags);
	skb = __skb_dequeue(rx_list);
	if (skb == NULL) {
		spin_unlock_irqrestore(&rx_list->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&rx_list->lock, flags);

	return ccmni_rx_callback(ccmni_idx, skb, NULL);
}

static void ccmni_data_handle_list(int status, unsigned int ccmni_idx)
{
	unsigned long flags;
	struct sk_buff *skb = NULL;
	struct ccmni_instance *ccmni = NULL;
	int ret = 0;

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];

	if (status) {
		atomic_set(&ccmni->is_up, 1);
		while (!skb_queue_empty(&ccmni->rx_list))
			ret += recv_from_rx_list(&ccmni->rx_list, ccmni_idx);

		if (ret) {
			spin_lock_bh(ccmni->spinlock);
			ret = napi_gro_list_flush(ccmni);
			spin_unlock_bh(ccmni->spinlock);
		}
	} else {
		atomic_set(&ccmni->is_up, 0);
		spin_lock_irqsave(&ccmni->rx_list.lock, flags);
		while ((skb = __skb_dequeue(&ccmni->rx_list))
			!= NULL)
			dev_kfree_skb_any(skb);
		spin_unlock_irqrestore(&ccmni->rx_list.lock, flags);
	}
}

static int ccmni_queue_recv_skb(unsigned int ccmni_idx, struct sk_buff *skb)
{
	unsigned long flags;
	int ret = 0;
	struct ccmni_instance *ccmni = NULL;

	if (unlikely(ccmni_idx >= CCMNI_INTERFACE_NUM || ccmni_ctl_blk == NULL)) {
		pr_info("invalid CCMNI%d  or ccmni_ctl_blk\n", ccmni_idx);
		dev_kfree_skb_any(skb);
		return ret;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	if (ccmni == NULL) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	if (atomic_read(&ccmni->is_up)) {
		while (!skb_queue_empty(&ccmni->rx_list))
			ret += recv_from_rx_list(&ccmni->rx_list, ccmni_idx);

		if (ret) {
			spin_lock_bh(ccmni->spinlock);
			ret = napi_gro_list_flush(ccmni);
			spin_unlock_bh(ccmni->spinlock);
		}
		/*The packet may be out of order when ccmni is up at the*/
		/* same time, it will be correctly handled by TCP stack.*/
		ret += ccmni_rx_callback(ccmni_idx, skb, NULL);
	} else {

		spin_lock_irqsave(&ccmni->rx_list.lock, flags);
		__skb_queue_tail(&ccmni->rx_list, skb);
		spin_unlock_irqrestore(&ccmni->rx_list.lock, flags);
	}

	return ret;
}

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (unlikely(ccmni->index >= CCMNI_INTERFACE_NUM || ccmni_ctl_blk == NULL)) {
		pr_info("invalid CCMNI%d or ccmni_ctl_blk\n", ccmni->index);
		return -1;
	}

	netif_carrier_on(dev);

	netif_tx_start_all_queues(dev);

	if (unlikely(ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_NAPI)) {
		napi_enable(ccmni->napi);
		napi_schedule(ccmni->napi);
	}

	atomic_inc(&ccmni->usage);
	ccmni_tmp = ccmni_ctl_blk->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}
	if (g_init_rps_value)
		set_rps_map(dev->_rx, g_init_rps_value);
	else
		set_rps_map(dev->_rx, 0x0F);
	queue_delayed_work(ccmni->worker,
				&ccmni->pkt_queue_work,
				msecs_to_jiffies(500));

	pr_info(
		"%s_Open:cnt=(%d,%d), md_ab=0x%X, gro=(%llx, %ld), flt_cnt=%d, rps:%lx\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage),
		ccmni_ctl_blk->ccci_cfg->md_ability,
		dev->features, gro_flush_timer, ccmni->flt_cnt, g_init_rps_value? g_init_rps_value: 0x0F);

	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (unlikely(ccmni->index >= CCMNI_INTERFACE_NUM || ccmni_ctl_blk == NULL)) {
		pr_info("invalid CCMNI%d or ccmni_ctl_blk\n", ccmni->index);
		return -1;
	}

	cancel_delayed_work(&ccmni->pkt_queue_work);
	flush_delayed_work(&ccmni->pkt_queue_work);

	atomic_dec(&ccmni->usage);
	ccmni->net_type = 0;
	spin_lock(ccmni->ch_hwq.spinlock_channel);
	ccmni->ch_hwq.ioctl_or_rpc = 0;
	ccmni->ch_hwq.hwqno = MD_HW_NORMAL_Q;
	spin_unlock(ccmni->ch_hwq.spinlock_channel);
	ccmni_tmp = ccmni_ctl_blk->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}

	netif_tx_disable(dev);

	if (unlikely(ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_NAPI))
		napi_disable(ccmni->napi);

	ccmni_data_handle_list(DEV_CLOSE, ccmni->index);
	netdev_info(dev, "%s_Close:cnt=(%d, %d)\n",
		    dev->name, atomic_read(&ccmni->usage),
		    atomic_read(&ccmni_tmp->usage));

	return 0;
}

static unsigned int ccmni_flush_dev_queue(unsigned int ccmni_idx)
{
	struct ccmni_instance *ccmni = NULL;
	unsigned int ret;

	if (unlikely(ccmni_idx >= CCMNI_INTERFACE_NUM || ccmni_ctl_blk == NULL)) {
		pr_info("flush_dev_queue : invalid ccmni index = %d or ccmni_ctl_blk null\n",
		ccmni_idx);
		return 0;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	if (ccmni == NULL) {
		pr_info("flush dev queue : invalid ccmni null\n");
		return 0;
	}

	spin_lock_bh(ccmni->spinlock);
	ccmni->rx_gro_cnt++;
	ret = napi_gro_list_flush(ccmni);
	spin_unlock_bh(ccmni->spinlock);

	return ret;
}

static int ccmni_stop_dev_queue(unsigned int ccmni_idx, unsigned int que_idx)
{
	struct ccmni_instance *ccmni = NULL;

	if (unlikely(ccmni_ctl_blk == NULL)) {
		pr_info("stop_dev_queue : invalid ccmni_ctl_blk null\n");
		return -1;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	if (ccmni == NULL) {
		pr_info("stop dev queue : invalid ccmni null\n");
		return -1;
	}

	if (likely(atomic_read(&ccmni->usage) > 0)) {
		if (likely(ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_CCMNI_MQ))
			netif_tx_stop_queue(netdev_get_tx_queue(ccmni->dev, que_idx));
		else
			netif_stop_queue(ccmni->dev);
		return 0;
	}

	return -2;
}

static int ccmni_start_dev_queue(unsigned int ccmni_idx, unsigned int que_idx)
{
	struct ccmni_instance *ccmni = NULL;

	if (unlikely(ccmni_ctl_blk == NULL)) {
		pr_info("start_dev_queue : invalid ccmni index = %d or ccmni_ctl_blk null\n",
			   ccmni_idx);
		return -1;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	if (ccmni == NULL) {
		pr_info("start dev queue : invalid ccmni null\n");
		return -1;
	}
	if (likely(atomic_read(&ccmni->usage) > 0 && netif_running(ccmni->dev))) {
		if (likely(ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_CCMNI_MQ)) {
			struct netdev_queue *net_queue = netdev_get_tx_queue(ccmni->dev, que_idx);

			if (netif_tx_queue_stopped(net_queue)) {
				netif_tx_wake_queue(net_queue);
				return 0;
			}

		} else {
			if (netif_queue_stopped(ccmni->dev)) {
				netif_wake_queue(ccmni->dev);
				return 0;
			}
		}

		return -3;
	}

	return -2;
}

static netdev_tx_t ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int skb_len = skb->len;
	struct ccmni_instance *ccmni = (struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_tx_para_info tx_para_info;

#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	/* dev->mtu is changed  if dev->mtu is changed by upper layer */
	if (unlikely(skb->len > dev->mtu)) {
		netdev_err(dev,
			   "CCMNI%d write fail: len(0x%x)>MTU(0x%x, 0x%x)\n",
			   ccmni->index, skb->len, CCMNI_MTU, dev->mtu);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (ccmni_forward_rx(ccmni, skb) == NETDEV_TX_OK)
		return NETDEV_TX_OK;

	if (sysctl_header)
		sk_pacing_shift_update(skb->sk, sysctl_tcp_pacing_shift);

	tx_para_info.skb = skb;
	tx_para_info.ccmni_idx = ccmni->index;
	tx_para_info.hw_qno = skb->queue_mapping;
	tx_para_info.count_l = (skb->mark & APP_VIP_MARK) ? 0x1000 : 0;
	tx_para_info.network_type = ccmni->net_type;
	if (ccmni_ops.send_skb) {
		if (ccmni_ops.send_skb(&tx_para_info))
			goto tx_busy;
		else
			goto tx_is_ok;
	}

	dev->stats.tx_dropped++;
	ccmni->tx_busy_cnt[tx_para_info.hw_qno] = 0;
	netdev_info(dev, "[TX] error: hw_qno or send_skb is NULL. free skb.\n");
	//send_skb is NULL, free this skb
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;


tx_is_ok:
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;


	if (ccmni->tx_busy_cnt[tx_para_info.hw_qno] > 0) {
		if (ccmni->tx_busy_cnt[tx_para_info.hw_qno] > 10) {
			netdev_dbg(dev,
				"[TX] CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry %ld times done\n",
				ccmni->index, dev->stats.tx_packets,
				tx_para_info.hw_qno, ccmni->tx_busy_cnt[tx_para_info.hw_qno]);

		}

		ccmni->tx_busy_cnt[tx_para_info.hw_qno] = 0;

	}

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->tx_met_time == 0) {
		ccmni->tx_met_time = jiffies;
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->tx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_tx_bytes", dev->name);
		tag_id = CCMNI_TX_MET_ID + ccmni->index;
		met_tag_oneshot(tag_id, tag_name,
		(dev->stats.tx_bytes - ccmni->tx_met_bytes));
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
		ccmni->tx_met_time = jiffies;
	}
#endif

	return NETDEV_TX_OK;

tx_busy:

	ccmni->tx_busy_cnt[tx_para_info.hw_qno]++;
	if (ccmni->tx_busy_cnt[tx_para_info.hw_qno] % 100 == 0)
		netdev_dbg(dev,
			"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry_times=%ld\n",
			ccmni->index, dev->stats.tx_packets,
			tx_para_info.hw_qno, ccmni->tx_busy_cnt[tx_para_info.hw_qno]);

	return NETDEV_TX_BUSY;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (new_mtu > MAX_MTU)
		return -EINVAL;

	dev->mtu = new_mtu;
	netdev_dbg(dev,
		"CCMNI%d change mtu_siz=%d\n", ccmni->index, new_mtu);
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	netdev_dbg(dev,
		   "ccmni%d_tx_timeout: usage_cnt=%d, timeout=%ds\n",
		   ccmni->index, atomic_read(&ccmni->usage),
		   (ccmni->dev->watchdog_timeo/HZ));

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static int ccmni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int usage_cnt;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk;
	unsigned int timeout = 0;
	struct ccmni_fwd_filter flt_tmp;
	struct ccmni_flt_act flt_act;
	unsigned int i;
	unsigned int cmp_len;

	if (ccmni->index >= CCMNI_INTERFACE_NUM) {
		netdev_err(dev, "%s : invalid ccmni index = %d\n",
			   __func__, ccmni->index);
		return -EINVAL;
	}

	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:   start/stop
		 * ifru_ivalue[7~4]:   reserve
		 * ifru_ivalue[15~8]:  user id, bit8=rild, bit9=thermal
		 * ifru_ivalue[31~16]: watchdog timeout value
		 */
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			/*ignore txq stop/resume if dev is not running */
			if (atomic_read(&ccmni->usage) > 0 &&
				netif_running(dev)) {
				atomic_dec(&ccmni->usage);

				netif_tx_disable(dev);
				/* stop queue won't stop Tx
				 * watchdog (ndo_tx_timeout)
				 */
				timeout = (ifr->ifr_ifru.ifru_ivalue &
					0xFFFF0000) >> 16;
				if (timeout == 0)
					dev->watchdog_timeo = 60 * HZ;
				else
					dev->watchdog_timeo = timeout*HZ;

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0 &&
					netif_running(dev)) {
				netif_tx_wake_all_queues(dev);
				dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
				atomic_inc(&ccmni->usage);

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		}
		if (likely(ccmni_tmp != NULL)) {
			netdev_dbg(dev,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=(%d, %d)\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage));
		} else {
			netdev_dbg(dev,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=%d\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage));
		}
		break;

	case SIOCFWDFILTER:
		if (copy_from_user(&flt_act, ifr->ifr_ifru.ifru_data,
				sizeof(struct ccmni_flt_act))) {
			netdev_info(dev,
				"SIOCFWDFILTER: %s copy data from user fail\n",
				dev->name);
			return -EFAULT;
		}

		flt_tmp = flt_act.flt;
		if (flt_tmp.ver != 0x40 && flt_tmp.ver != 0x60) {
			netdev_info(dev,
				"SIOCFWDFILTER[%d]: %s invalid flt(%x, %x, %x, %x, %x)(%d)\n",
				flt_act.action, dev->name,
				flt_tmp.ver, flt_tmp.s_pref,
				flt_tmp.d_pref, flt_tmp.ipv4.saddr,
				flt_tmp.ipv4.daddr, ccmni->flt_cnt);
			return -EINVAL;
		}

		if (flt_act.action == CCMNI_FLT_ADD) { /* add new filter */
			if (ccmni->flt_cnt >= CCMNI_FLT_NUM) {
				netdev_info(dev,
					"SIOCFWDFILTER[ADD]: %s flt table full\n",
					dev->name);
				return -ENOMEM;
			}
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					break;
			}
			if (i < CCMNI_FLT_NUM) {
				memcpy(&ccmni->flt_tbl[i], &flt_tmp,
					sizeof(struct ccmni_fwd_filter));
				ccmni->flt_cnt++;
			}
			netdev_info(dev,
				"SIOCFWDFILTER[ADD]: %s add flt%d(%x, %x, %x, %x, %x)(%d)\n",
				dev->name, i, flt_tmp.ver,
				flt_tmp.s_pref, flt_tmp.d_pref,
				flt_tmp.ipv4.saddr, flt_tmp.ipv4.daddr,
				ccmni->flt_cnt);
		} else if (flt_act.action == CCMNI_FLT_DEL) {
			if (flt_tmp.ver == IPV4_VERSION)
				cmp_len = offsetof(struct ccmni_fwd_filter,
							ipv4.daddr) + 4;
			else
				cmp_len = sizeof(struct ccmni_fwd_filter);
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					continue;
				if (!memcmp(&ccmni->flt_tbl[i],
						&flt_tmp, cmp_len)) {
					netdev_info(dev,
						"SIOCFWDFILTER[DEL]: %s del flt%d(%x, %x, %x, %x, %x)(%d)\n",
						dev->name, i, flt_tmp.ver,
						flt_tmp.s_pref, flt_tmp.d_pref,
						flt_tmp.ipv4.saddr,
						flt_tmp.ipv4.daddr,
						ccmni->flt_cnt);
					memset(
						&ccmni->flt_tbl[i],
						0,
						sizeof(struct ccmni_fwd_filter)
					);
					ccmni->flt_cnt--;
					break;
				}
			}
			if (i >= CCMNI_FLT_NUM) {
				netdev_info(dev,
					"SIOCFWDFILTER[DEL]: %s no match flt(%x, %x, %x, %x, %x)(%d)\n",
					dev->name, flt_tmp.ver,
					flt_tmp.s_pref, flt_tmp.d_pref,
					flt_tmp.ipv4.saddr,
					flt_tmp.ipv4.daddr,
					ccmni->flt_cnt);
				return -ENXIO;
			}
		} else if (flt_act.action == CCMNI_FLT_FLUSH) {
			ccmni->flt_cnt = 0;
			memset(ccmni->flt_tbl, 0,
			CCMNI_FLT_NUM*sizeof(struct ccmni_fwd_filter));
			netdev_info(dev,
				"SIOCFWDFILTER[FLUSH]: %s flush filter\n",
				dev->name);
		}
		break;

	case SIOCACKPRIO:
		/* ifru_ivalue[3~0]: enable/disable ack prio feature  */
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			for (i = 0; i < CCMNI_INTERFACE_NUM; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 0;
			}
		} else {
			for (i = 0; i < CCMNI_INTERFACE_NUM; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 1;
			}
		}
		netdev_info(dev,
			"SIOCACKPRIO: ack_prio_en=%d, ccmni0_ack_en=%d\n",
			ifr->ifr_ifru.ifru_ivalue,
			ccmni_tmp->ack_prio_en);
		break;

	case SIOPUSHPENDING:
		netdev_info(dev, "%s SIOPUSHPENDING called\n", ccmni->dev->name);
		cancel_delayed_work(&ccmni->pkt_queue_work);
		flush_delayed_work(&ccmni->pkt_queue_work);
		ccmni_data_handle_list(DEV_OPEN, ccmni->index);
		break;

	case SIOCSNETTYPE:
		/*HW only has 3bits*/
		if(ifr->ifr_ifru.ifru_ivalue > 7 || ifr->ifr_ifru.ifru_ivalue < 0) {
			netdev_info(dev, "SIOCSNETTYPE set the wrong value:%u\n", ifr->ifr_ifru.ifru_ivalue);
			return -EINVAL;
		}
		ccmni->net_type = ifr->ifr_ifru.ifru_ivalue;
		netdev_info(dev,"nettype value:%d", ccmni->net_type);
		break;

	case SIOCGNETTYPE:
		if (strncmp(ifr->ifr_name, ccmni->dev->name, sizeof(ifr->ifr_name))== 0)
			ifr->ifr_ifru.ifru_ivalue = ccmni->net_type;
		else {
			netdev_info(dev, "SIOCGNETTYPE doesnot match %s\n", ccmni->dev->name);
			return -EINVAL;
		}
		break;

	case SIOCSQID:
		/*HW only has 3bits*/
		if(ifr->ifr_ifru.ifru_ivalue >= MD_HW_Q_MAX || ifr->ifr_ifru.ifru_ivalue < 0) {
			netdev_info(dev, "SIOCSQID set the wrong value:%u\n", ifr->ifr_ifru.ifru_ivalue);
			return -EINVAL;
		}
		spin_lock(ccmni->ch_hwq.spinlock_channel);
		ccmni->ch_hwq.hwqno = ifr->ifr_ifru.ifru_ivalue;
		ccmni->ch_hwq.ioctl_or_rpc = 2;
		spin_unlock(ccmni->ch_hwq.spinlock_channel);
		netdev_info(dev,"qid value:%d, ifru_ivalue:%d", ccmni->ch_hwq.hwqno, ifr->ifr_ifru.ifru_ivalue);
		break;

	case SIOCGQID:
		if (strncmp(ifr->ifr_name, ccmni->dev->name, sizeof(ifr->ifr_name))== 0)
			ifr->ifr_ifru.ifru_ivalue = ccmni->ch_hwq.hwqno;
		else {
			netdev_info(dev, "SIOCGQID doesnot match\n");
			return -EINVAL;
		}
		break;

	default:
		netdev_dbg(dev,
			" unknown ioctl cmd=%x\n", cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int ccmni_private_ioctl(struct net_device *dev, struct ifreq *ifr,
								void __user *data, int cmd)
{
	return ccmni_ioctl(dev, ifr, cmd);
}

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open		= ccmni_open,
	.ndo_stop		= ccmni_close,
	.ndo_start_xmit	= ccmni_start_xmit,
	.ndo_tx_timeout	= ccmni_tx_timeout,
	.ndo_do_ioctl   = ccmni_ioctl,
	.ndo_siocdevprivate = ccmni_private_ioctl,
	.ndo_change_mtu = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
};

static int ccmni_napi_poll(struct napi_struct *napi, int budget)
{
#ifdef ENABLE_WQ_GRO
	return 0;
#else
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(napi->dev);

	del_timer(ccmni->timer);
	return 0;
#endif
}

//static void ccmni_napi_poll_timeout(unsigned long data)
static void ccmni_napi_poll_timeout(struct timer_list *t)
{
	//struct ccmni_instance *ccmni = (struct ccmni_instance *)data;
	//struct ccmni_instance *ccmni = from_timer(ccmni, t, timer);

	//netdev_dbg(dev,
	//	"CCMNI%d lost NAPI polling\n", ccmni->index);
}

static void get_queued_pkts(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ccmni_instance *ccmni =
		container_of(dwork, struct ccmni_instance, pkt_queue_work);

	if (unlikely(ccmni->index >= CCMNI_INTERFACE_NUM)) {
		pr_info("invalid CCMNI%d when getting queued pkts\n", ccmni->index);
		return;
	}
	ccmni_data_handle_list(DEV_OPEN, ccmni->index);
}

/********************ccmni driver register  ccci function********************/
static inline int ccmni_inst_init(struct ccmni_instance *ccmni, struct net_device *dev)
{
	ccmni->ch = &ccmni_ch_arr[ccmni->index];
	ccmni->dev = dev;
	ccmni->ctlb = ccmni_ctl_blk;

	ccmni->timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (ccmni->timer == NULL) {
		netdev_err(dev, "%s kzalloc ccmni->timer fail\n", __func__);
		return -1;
	}
	ccmni->spinlock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (ccmni->spinlock == NULL) {
		netdev_err(dev, "%s kzalloc ccmni->spinlock fail\n", __func__);
		return -1;
	}
	ccmni->ch_hwq.spinlock_channel = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (ccmni->ch_hwq.spinlock_channel == NULL)
		return -1;

	ccmni->napi = kzalloc(sizeof(struct napi_struct), GFP_KERNEL);
	if (ccmni->napi == NULL)
		return -1;

	ccmni->ack_prio_en = 1;

	/* register napi device */
	if (dev && (ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_NAPI)) {
		//init_timer(ccmni->timer);
		//ccmni->timer->function = ccmni_napi_poll_timeout;
		//ccmni->timer->data = (unsigned long)ccmni;
		timer_setup(ccmni->timer, ccmni_napi_poll_timeout, 0);
		netif_napi_add_weight(dev, ccmni->napi, ccmni_napi_poll,
			ccmni_ctl_blk->ccci_cfg->napi_poll_weigh);
	}
#ifdef ENABLE_WQ_GRO
	if (dev)
		netif_napi_add_weight(dev, ccmni->napi, ccmni_napi_poll,
			ccmni_ctl_blk->ccci_cfg->napi_poll_weigh);
#endif

	atomic_set(&ccmni->usage, 0);
	spin_lock_init(ccmni->spinlock);
	spin_lock_init(ccmni->ch_hwq.spinlock_channel);
	spin_lock(ccmni->ch_hwq.spinlock_channel);
	ccmni->ch_hwq.ioctl_or_rpc = 0;
	ccmni->ch_hwq.hwqno = MD_HW_NORMAL_Q;
	spin_unlock(ccmni->ch_hwq.spinlock_channel);
	ccmni->worker = alloc_workqueue("ccmni%d_rx_q_worker",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1, ccmni->index);
	if (!ccmni->worker) {
		netdev_info(dev, "%s alloc queue worker fail\n",
			__func__);
		return -1;
	}
	INIT_DELAYED_WORK(&ccmni->pkt_queue_work, get_queued_pkts);

	return 0;
}

static void free_ccmni_alloc(struct net_device *dev)
{
	struct ccmni_instance *ccmni = netdev_priv(dev);

	if (ccmni) {
		if (ccmni->napi)
			netif_napi_del(ccmni->napi);
		kfree(ccmni->napi);

		kfree(ccmni->timer);
		kfree(ccmni->spinlock);
		kfree(ccmni->ch_hwq.spinlock_channel);
		if (ccmni->worker)
			destroy_workqueue(ccmni->worker);
		netdev_info(dev, "%s cleanup", __func__);
	}
}

static inline void ccmni_dev_init(struct net_device *dev)
{
	u8 addr[ETH_ALEN];

	dev->header_ops = NULL; /* No Header */
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
	/* ccmni is a pure IP device */
	dev->flags = IFF_NOARP &
			(~IFF_BROADCAST & ~IFF_MULTICAST);
	/* not support VLAN */
	dev->features = NETIF_F_VLAN_CHALLENGED;
	dev->features |= NETIF_F_GRO_FRAGLIST;
	if (ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_HWTXCSUM) {
		netdev_info(dev, "checksum_dbg %s MODEM_CAP_HWTXCSUM", __func__);
		dev->features |= NETIF_F_HW_CSUM;
	}
	if (ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_SGIO) {
		dev->features |= NETIF_F_SG;
		dev->hw_features |= NETIF_F_SG;
	}
	if (ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_NAPI) {
#ifdef ENABLE_NAPI_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		dev->features |= NETIF_F_GRO;
		dev->hw_features |= NETIF_F_GRO;
#endif
	}
	/* check gro_list_prepare
	 * when skb hasn't ethernet header,
	 * GRO needs hard_header_len == 0.
	 */
	dev->hard_header_len = 0;
	dev->addr_len = 0;        /* hasn't ethernet header */
	dev->priv_destructor = free_ccmni_alloc;
	dev->netdev_ops = &ccmni_netdev_ops;
	memset(addr, 0, sizeof(addr));
	eth_random_addr(addr);
	__dev_addr_set(dev, addr, ETH_ALEN);
}

#ifdef CCCI_CCMNI_MODULE
const struct header_ops ccmni_eth_header_ops ____cacheline_aligned = {
	.create		= eth_header,
	.parse		= eth_header_parse,
	.cache		= eth_header_cache,
	.cache_update	= eth_header_cache_update,
};
#endif

static u32 ccmni_get_capability_from_dts_node(unsigned int hw_type)
{
	u32 capability = 0;
	struct device_node *node = NULL;
	int ret;

	if (hw_type & DPMAIF_DRIVER) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,dpmaif");
		if (node) {
			ret = of_property_read_u32(node, "mediatek,dpmaif-cap", &capability);
			if (ret < 0) {
				pr_info("cannot find dpmaif-cap in dts node\n");
				return -1;
			}
			return capability;
		}
	}
	if (hw_type & CLDMA_DRIVER) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,ccci_cldma");
		if (node) {
			ret = of_property_read_u32(node, "mediatek,cldma-capability", &capability);
			if (ret < 0) {
				pr_info("cannot find cldma-cap in dts node\n");
				return -1;
			}
			return capability;
		}
	}
	return -1;
}

static u32 ccmni_get_capability_from_dts(void)
{
	struct device_node *node = NULL;
	unsigned int hw_type = 0;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (node) {
		ret = of_property_read_u32(node, "mediatek,mdhif-type", &hw_type);
		if (ret < 0) {
			pr_info("cannot find mdhif-type in dts file\n");
			return -1;
		}
		return ccmni_get_capability_from_dts_node(hw_type);
	}
	pr_info("cannot find mddriver\n");
	return -1;
}

static int ccmni_init(void)
{
	unsigned int i = 0;
	int ret = 0, j = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	unsigned long long time_total = sched_clock();
	u32 capability;

	capability = ccmni_get_capability_from_dts();
	if (capability == -1)
		return -1;
	if (unlikely(capability & MODEM_CAP_CCMNI_DISABLE)) {
		netdev_info(dev, "no need init ccmni: md_ability=0x%x\n",
			capability);
		return 0;
	}

	ctlb = kzalloc(sizeof(struct ccmni_ctl_block), GFP_KERNEL);

	if (unlikely(ctlb == NULL)) {
		netdev_err(dev, "alloc ccmni ctl struct fail\n");
		return -ENOMEM;
	}

	ctlb->ccci_cfg = &eccci_ccmni_cfg;
	ctlb->ccci_cfg->md_ability |= capability;

#if defined(CCMNI_MET_DEBUG)
	if (met_tag_init() != 0)
		netdev_info(dev, "%s:met tag init fail\n", __func__);
#endif

	ccmni_ctl_blk = ctlb;
	for (i = 0; i < CCMNI_INTERFACE_NUM; i++) {
		/* allocate netdev */
		if (ctlb->ccci_cfg->md_ability & MODEM_CAP_CCMNI_MQ)
			/* alloc multiple tx queue, 2 txq and 1 rxq */
			dev =
			alloc_etherdev_mqs(
					sizeof(struct ccmni_instance),
					MD_HW_Q_MAX,
					1);
		else
			dev =
			alloc_etherdev(sizeof(struct ccmni_instance));
		if (unlikely(dev == NULL)) {
			netdev_dbg(dev, "alloc netdev fail\n");
			ret = -ENOMEM;
			goto alloc_netdev_fail;
		}

		/* init net device */
		ccmni_dev_init(dev);

		/* The purpose of changing the ccmni device type to ARPHRD_NONE
		 * is used to support automatically adding an ipv6 mroute and
		 * support for clat eBPF and tethering eBPF offload
		 */
		dev->type = ARPHRD_NONE;
		dev->max_mtu = MAX_MTU;
		scnprintf(dev->name, sizeof(dev->name), "%s%d", ctlb->ccci_cfg->name, i);

		/* init private structure of netdev */
		ccmni = netdev_priv(dev);
		ccmni->index = i;
		ret = ccmni_inst_init(ccmni, dev);
		if (ret) {
			netdev_info(dev,
				"initial ccmni instance fail\n");
			goto alloc_netdev_fail;
		}
		ctlb->ccmni_inst[i] = ccmni;

		/* register net device */
		ret = register_netdev(dev);
		if (ret)
			goto alloc_netdev_fail;
		skb_queue_head_init(&ccmni->rx_list);
	}

	scnprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name),
			"ccmni_md1");
	ctlb->ccmni_wakelock = wakeup_source_register(NULL,
		ctlb->wakelock_name);
	if (!ctlb->ccmni_wakelock) {
		netdev_info(dev, "%s %d: init wakeup source fail!",
			__func__, __LINE__);
		return -1;
	}

	if (register_tcp_pacing_sysctl() == -1)
		return 0;
	sysctl_tcp_pacing_shift = 6;

	time_total = sched_clock() - time_total;
	pr_info("%s cost: %llu\n", __func__, time_total);
	return 0;

alloc_netdev_fail:
	if (dev) {
		free_ccmni_alloc(dev);
		free_netdev(dev);
		ctlb->ccmni_inst[i] = NULL;
	}

	for (j = i - 1; j >= 0; j--) {
		ccmni = ctlb->ccmni_inst[j];
		if (!ccmni)
			continue;
		unregister_netdev(ccmni->dev);
		/* free_netdev(ccmni->dev); */
		ctlb->ccmni_inst[j] = NULL;
	}

	kfree(ctlb);

	ccmni_ctl_blk = NULL;
	return ret;
}

static void ccmni_exit(void)
{
/* Todo:
 * When ccmnix is unregistering, driver still received packets, ccmni may be null,
 * but flush\start\stop_dev_queue will still call ccmni->xx by dpmaif driver, even
 * ccmnix is checked not null, but ccmnix canbe free simultaneously.
 * The best solution is to use lock, but using lock will suffer performance issue,
 * So before better solution is thought out, ccmnix will never be unregistered.
 *
 *
 *	unsigned int i = 0;
 *	struct ccmni_ctl_block *ctlb = NULL;
 *	struct ccmni_instance *ccmni = NULL;

 *	unregister_tcp_pacing_sysctl();

 *	ctlb = ccmni_ctl_blk;
 *	if (ctlb) {

 *		for (i = 0; i < CCMNI_INTERFACE_NUM; i++) {
 *			ccmni = ctlb->ccmni_inst[i];
 *			if (!ccmni)
 *				continue;

 *			unregister_netdev(ccmni->dev);
 *			ctlb->ccmni_inst[i] = NULL;
 *		}
 *		kfree(ctlb);
 *		ccmni_ctl_blk = NULL;
 *	}
 */
}

static int ccmni_rx_callback(unsigned int ccmni_idx, struct sk_buff *skb,
		void *priv_data)
{
	/* struct ccci_header *ccci_h = (struct ccci_header*)skb->data; */
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	int pkt_type, skb_len;
#if defined(CCCI_SKB_TRACE)
	struct iphdr *iph;
#endif
#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	dev = ccmni->dev;

	pkt_type = skb->data[0] & 0xF0;

	skb_reset_transport_header(skb);
	skb_reset_network_header(skb);
	skb_set_mac_header(skb, 0);
	skb_reset_mac_len(skb);

	skb->dev = dev;
	if (pkt_type == 0x60)
		skb->protocol  = htons(ETH_P_IPV6);
	else
		skb->protocol  = htons(ETH_P_IP);

	//skb->ip_summed = CHECKSUM_NONE;
	skb_len = skb->len;

#if defined(NETDEV_TRACE) && defined(NETDEV_DL_TRACE)
	skb->dbg_flag = 0x1;
#endif

#if defined(CCCI_SKB_TRACE)
	iph = (struct iphdr *)skb->data;
	ctlb->net_rx_delay[2] = iph->id;
	ctlb->net_rx_delay[0] = dev->stats.rx_bytes + skb_len;
	ctlb->net_rx_delay[1] = dev->stats.tx_bytes;
#endif


	if (likely(ccmni_ctl_blk->ccci_cfg->md_ability & MODEM_CAP_NAPI)) {
#ifdef ENABLE_NAPI_GRO
		napi_gro_receive(ccmni->napi, skb);
#else
		netif_receive_skb(skb);
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		if (is_skb_gro(skb)) {
			spin_lock_bh(ccmni->spinlock);
			napi_gro_receive(ccmni->napi, skb);
			spin_unlock_bh(ccmni->spinlock);
		} else {
			netif_rx(skb);
		}
#else
		netif_rx(skb);
#endif
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->rx_met_time == 0) {
		ccmni->rx_met_time = jiffies;
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->rx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		scnprintf(tag_name, 32, "%s_rx_bytes", dev->name);
		tag_id = CCMNI_RX_MET_ID + ccmni_idx;
		met_tag_oneshot(tag_id, tag_name,
			(dev->stats.rx_bytes - ccmni->rx_met_bytes));
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
		ccmni->rx_met_time = jiffies;
	}
#endif

	return 1;
}

static void ccmni_md_state_callback(unsigned int ccmni_idx, enum MD_STATE state)
{
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	int i = 0;
	unsigned long flags;

	if (unlikely(ccmni_ctl_blk == NULL)) {
		pr_err("invalid ccmni ctrl when ccmni%d_md_sta=%d\n",
		       ccmni_idx, state);
		return;
	}
	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	if (ccmni == NULL) {
		pr_info("md state callback : invalid ccmni null\n");
		return;
	}
	dev = ccmni->dev;

	if (atomic_read(&ccmni->usage) > 0)
		netdev_dbg(dev, "md_state_cb: CCMNI%d, md_sta=%d, usage=%d\n",
			   ccmni_idx, state, atomic_read(&ccmni->usage));
	switch (state) {
	case READY:
		for (i = 0; i < MD_HW_Q_MAX; i++)
			ccmni->flags[i] &= ~CCMNI_TX_PRINT_F;
		ccmni->rx_seq_num = 0;
		spin_lock_irqsave(ccmni->spinlock, flags);
		ccmni->rx_gro_cnt = 0;
		spin_unlock_irqrestore(ccmni->spinlock, flags);
		break;

	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		netif_tx_disable(ccmni->dev);
		netif_carrier_off(ccmni->dev);
		break;
	default:
		break;
	}
}

static void ccmni_dump_txrx_status(unsigned int ccmni_idx)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *dev_queue = NULL;
	struct netdev_queue *ack_queue = NULL;
	struct Qdisc *qdisc = NULL;
	struct Qdisc *ack_qdisc = NULL;
	struct rtnl_link_stats64 rtnls;

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	if (unlikely(ccmni_tmp == NULL))
		return;

	if ((ccmni_tmp->dev->stats.rx_packets == 0) &&
			(ccmni_tmp->dev->stats.tx_packets == 0))
		return;

	dev = ccmni_tmp->dev;
	/* ccmni diff from ccmni_tmp for MD IRAT */
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	dev_queue = netdev_get_tx_queue(dev, MD_HW_NORMAL_Q);
	netdev_info(dev, "to:clr(%lu:%lu)\r\n", timeout_flush_num, clear_flush_num);
	dev_get_stats(dev, &rtnls);
	if (ctlb->ccci_cfg->md_ability & MODEM_CAP_CCMNI_MQ) {
		ack_queue = netdev_get_tx_queue(dev, MD_HW_HIGH_Q);
		qdisc = dev_queue->qdisc;
		ack_qdisc = ack_queue->qdisc;
		/* stats.rx_dropped is dropped in ccmni,
		 * dev->rx_dropped is dropped in net device layer
		 */
		/* stats.tx_packets is count by ccmni, bstats.
		 * packets is count by qdisc in net device layer
		 */
		netdev_info(dev,
			"%s(%d,%d), rx=(%ld,%ld,%d), tx=(%ld,%llu,%lld), txq_len=(%d,%d), tx_drop=(%ld,%d,%d), rx_drop=(%ld,%llu), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx,0x%lx)\n",
				  dev->name,
				  atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
			      dev->stats.rx_packets,
				  dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, u64_stats_read(&qdisc->bstats.packets),
				  u64_stats_read(&ack_qdisc->bstats.packets),
			      qdisc->q.qlen, ack_qdisc->q.qlen,
			      dev->stats.tx_dropped, qdisc->qstats.drops,
				  ack_qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  rtnls.rx_dropped,
			      ccmni->tx_busy_cnt[0], ccmni->tx_busy_cnt[1],
			      dev->state, dev->flags, dev_queue->state,
				  ack_queue->state);
	} else
		netdev_info(dev,
			"%s(%d,%d), rx=(%ld,%ld,%d), tx=(%ld,%ld), txq_len=%d, tx_drop=(%ld,%d), rx_drop=(%ld,%llu), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx)\n",
			      dev->name, atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
			      dev->stats.rx_packets, dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, dev->stats.tx_bytes,
			      dev->qdisc->q.qlen, dev->stats.tx_dropped,
				  dev->qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  rtnls.rx_dropped,
				  ccmni->tx_busy_cnt[0],
			      ccmni->tx_busy_cnt[1], dev->state, dev->flags,
				  dev_queue->state);
}

static void ccmni_dump(void)
{
	unsigned int i = 0;

	if (ccmni_ctl_blk == NULL) {
		pr_info("invalid ctlb\n");
		return;
	}
	for (; i < CCMNI_INTERFACE_NUM; i++)
		ccmni_dump_txrx_status(i);
}

static void ccmni_dump_rx_status(unsigned long long *status)
{
	if (ccmni_ctl_blk == NULL) {
		pr_err("%s : invalid ctlb\n", __func__);
		return;
	}
	status[0] = ccmni_ctl_blk->net_rx_delay[0];
	status[1] = ccmni_ctl_blk->net_rx_delay[1];
	status[2] = ccmni_ctl_blk->net_rx_delay[2];
}

static struct ccmni_ch *ccmni_get_ch(unsigned int ccmni_idx)
{
	struct ccmni_instance *ccmni = NULL;

	if (ccmni_idx >= CCMNI_INTERFACE_NUM ) {
		pr_err("%s : invalid ccmni index = %d\n",
			__func__, ccmni_idx);
		return NULL;
	}

	if (ccmni_ctl_blk == NULL) {
		pr_info("%s : invalid ctlb\n", __func__);
		return NULL;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	return ccmni? ccmni->ch : NULL;
}

static struct ccmni_ch_hwq *ccmni_get_ch_hwq(unsigned int ccmni_idx)
{
	struct ccmni_instance *ccmni = NULL;

	if (ccmni_idx >= CCMNI_INTERFACE_NUM) {
		pr_info("invalid ccmni index = %d when getting hw queueno\n",
			ccmni_idx);
		return NULL;
	}

	if (ccmni_ctl_blk == NULL) {
		pr_info("invalid ctlb when getting hw queueno\n");
		return NULL;
	}

	ccmni = ccmni_ctl_blk->ccmni_inst[ccmni_idx];
	return ccmni? &ccmni->ch_hwq : NULL;
}


struct ccmni_dev_ops ccmni_ops = {
	.skb_alloc_size = 1600,
	.recv_skb = &ccmni_queue_recv_skb,
	.md_state_callback = &ccmni_md_state_callback,
	.dump = ccmni_dump,
	.dump_rx_status = ccmni_dump_rx_status,
	.get_ch = ccmni_get_ch,
	.get_ch_hwq = ccmni_get_ch_hwq,
	.is_ack_skb = is_ack_skb,
	.start_queue = &ccmni_start_dev_queue,
	.stop_queue = &ccmni_stop_dev_queue,
	.flush_queue = &ccmni_flush_dev_queue,
};
EXPORT_SYMBOL(ccmni_ops);

module_init(ccmni_init);
module_exit(ccmni_exit);
MODULE_AUTHOR("MTK CCCI");
MODULE_DESCRIPTION("CCCI ccmni driver v0.1");
MODULE_LICENSE("GPL");
