// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#define pr_fmt(fmt) "virtio-cmdq: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <linux/virtio_ring.h>
#include <linux/dma-mapping.h>
#include <linux/virtio_config.h>
#include <linux/completion.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include <linux/clk.h>

#include "proto.h"
#include "cmdq-util.h"
#include "virtio_ids.h"

#define CMDQ_THR_BASE			0x100
#define CMDQ_THR_SIZE			0x80

enum {
	VIRTIO_CMDQ_VQ_REQ = 0,
	VIRTIO_CMDQ_VQ_EVT = 1,
	VIRTIO_CMDQ_VQ_MAX = 2,
};

struct virtio_cmdq {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_CMDQ_VQ_MAX];
	spinlock_t lock;
	spinlock_t evt_lock;
	u8			hwid;
};

static struct virtio_cmdq *g_cmdq[2];
struct virtio_cmdq_req_hdr {
	bool is_event;
};

struct virtio_cmdq_event {
	struct virtio_cmdq_req_hdr hdr;
	struct cmdq_task_complete_event evt;
};

struct virtio_cmdq_req {
	struct virtio_cmdq_req_hdr hdr;
	struct cmdq_request req;
	struct cmdq_response res;
	struct completion done;
};

struct cmdq_service {
	struct mbox_controller mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	uint32_t			hwid;
	struct cmdq_thread thread[CMDQ_THR_MAX_COUNT];

	spinlock_t client_pkt_list_lock;
	struct list_head client_pkt_list;

	struct clk		*clock;
	struct clk		*clock_timer;
};

struct client_record {
	uint64_t key;
	struct cmdq_pkt *pkt;
	struct list_head list_entry;
	cmdq_async_flush_cb cb;
	void *data;
	struct completion done;
	int host_result;  // host cmdq exec result.
};

static inline struct virtio_cmdq_event *to_vce(struct virtio_cmdq_req_hdr *hdr)
{
	return (struct virtio_cmdq_event *)hdr;
}

static inline struct virtio_cmdq_req *to_vcr(struct virtio_cmdq_req_hdr *hdr)
{
	return (struct virtio_cmdq_req *)hdr;
}

static struct cmdq_service g_cmdq_service[2];

/*
 * static u32 cmdq_thread_get_id(struct cmdq_client *clt)
 * {
 *	struct cmdq_thread *thread = clt->chan->con_priv;
 *	return thread->idx;
 * }
 */

struct cmdq_util_platform_fp *virtio_cmdq_platform;

void virtio_cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform)
{
	if (!cust_cmdq_platform) {
		cmdq_err("%s cmdq_util_platform_fp is NULL ", __func__);
		return;
	}
	virtio_cmdq_platform = cust_cmdq_platform;
}
EXPORT_SYMBOL(virtio_cmdq_util_set_fp);

static phys_addr_t virtual_cmdq_mbox_get_base_pa(void *chan)
{
	struct cmdq_service *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->base_pa;
}

static u32 virtual_cmdq_util_get_hw_id(struct cmdq_pkt *pkt)
{
	uint32_t hwid;
	struct cmdq_client *client = pkt->cl;

	hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(client->chan));
	if (hwid > 2)
		cmdq_err("%s error: hwid %d is error!\n", __func__, hwid);

	return hwid;
}

void build_from_pkt(struct cmdq_pkt *pkt, struct cmdq_flush_request *req)
{
	struct cmdq_client *client = pkt->cl;
	struct cmdq_pkt_buffer *buf;
	u32 size, cnt = 0;

	list_for_each_entry (buf, &pkt->buf, list_entry) {
		WARN_ON_ONCE(cnt >= MAX_CMDQ_IOV_SIZE);

		if (list_is_last(&buf->list_entry, &pkt->buf))
			size = CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
		else
			size = CMDQ_CMD_BUFFER_SIZE;
		req->iov[cnt].iov_base = buf->va_base;
		req->iov[cnt].iov_len = size;
		req->cmd_buf_paddrs[cnt] = buf->pa_base;
		req->cmd_buf_iovaddrs[cnt] = buf->iova_base;
		cnt++;
	}

	req->key = (uint64_t)pkt;
	req->thread_id = virtio_cmdq_platform->get_thread_id((void *)(client->chan));
	req->avail_buf_size = pkt->avail_buf_size;
	req->cmd_buf_size = pkt->cmd_buf_size;
	req->buf_size = pkt->buf_size;
	req->loop = pkt->loop;
	req->priority = pkt->priority;
	req->hwid = virtual_cmdq_util_get_hw_id(pkt);
	req->iov_len = cnt;
}

static struct client_record *find_client_record(uint64_t key, uint32_t hwid)
{
	struct client_record *rec, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	list_for_each_entry (rec, &g_cmdq_service[hwid].client_pkt_list, list_entry) {
		if (rec->key == key)
			found = rec;
	}
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	return found;
}

static struct client_record *remove_client_record(uint64_t key, uint32_t hwid)
{
	struct client_record *rec, *tmp, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
	list_for_each_entry_safe (rec, tmp, &g_cmdq_service[hwid].client_pkt_list,
				  list_entry) {
		if (rec->key == key) {
			found = rec;
			list_del(&found->list_entry);
		}
	}
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	return found;
}

static void add_client_record(struct client_record *rec, uint32_t hwid)
{
	unsigned long flags;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
	list_add_tail(&rec->list_entry, &g_cmdq_service[hwid].client_pkt_list);
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
}

/*
 * static void submit_event_buffer(struct virtio_cmdq_event *vce, uint32_t hwid)
 * {
 *	struct virtqueue *vq = g_cmdq[hwid]->vqs[VIRTIO_CMDQ_VQ_EVT];
 *	unsigned int num_out = 0, num_in = 0;
 *	struct scatterlist evt_sg, *sgs[1];
 *	unsigned long flags;
 *	int ret;
 *
 *	vce->hdr.is_event = true;
 *
 *	spin_lock_irqsave(&g_cmdq[hwid]->lock, flags);
 *
 *	sg_init_one(&evt_sg, &vce->evt, sizeof(vce->evt));
 *	sgs[num_out + num_in++] = &evt_sg;
 *
 *	ret = virtqueue_add_sgs(vq, sgs, num_out, num_in, vce, GFP_ATOMIC);
 *	BUG_ON(ret != 0);
 *
 *	spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);
 * }
 */

static void handle_cmdq_event(struct cmdq_task_complete_event *evt, uint32_t hwid)
{
	struct client_record *rec = find_client_record(evt->key, hwid);
	struct cmdq_cb_data data;

	if (rec == NULL) {
		cmdq_err("%s %d error:no client record.\n", __func__, __LINE__);
		return;
	}
	rec->host_result = evt->result;
	complete(&rec->pkt->cmplt);
	data.err = evt->result;
	data.data = rec->data;
	if (rec->cb)
		rec->cb(data);
}

/* Called from virtio device, in IRQ context */
void cmdq_request_done(struct virtqueue *vq)
{
	struct virtio_cmdq *cmdq = vq->vdev->priv;
	struct virtio_cmdq_req_hdr *hdr;
	unsigned long flags;
	unsigned int len;
	bool notify = false;

	spin_lock_irqsave(&cmdq->lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_cmdq_req *vcr = to_vcr(hdr);

			complete(&vcr->done);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&cmdq->lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

static int virtio_cmdq_queue_evt_buffer(struct virtqueue *vq,
					struct virtio_cmdq_event *evt)
{
	int ret;
	struct scatterlist sg;

	sg_init_one(&sg, &evt->evt, sizeof(evt->evt));

	ret = virtqueue_add_inbuf(vq, &sg, 1, evt, GFP_ATOMIC);

	if (ret)
		return ret;

	virtqueue_kick(vq);

	return 0;
}

struct virtqueue *cmd_evtq;
void cmdq_evt_done(struct virtqueue *vq)
{
	struct virtio_cmdq *cmdq = vq->vdev->priv;
	struct virtio_cmdq_req_hdr *hdr;
	unsigned long flags;
	unsigned int len;
	bool notify = false;

	cmd_evtq = vq;
	spin_lock_irqsave(&cmdq->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_cmdq_event *vce = to_vce(hdr);

			handle_cmdq_event(&vce->evt, cmdq->hwid);
			virtio_cmdq_queue_evt_buffer(vq, vce);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;

	spin_unlock_irqrestore(&cmdq->evt_lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

int fill_cmdq_evt(struct virtio_cmdq *cmdq)
{
	int i;
	struct virtio_cmdq_event *evts;
	struct virtqueue *vq = cmdq->vqs[VIRTIO_CMDQ_VQ_EVT];
	size_t nr_evts = vq->num_free;
	int ret;

	evts = kmalloc_array(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	for (i = 0; i < nr_evts; i++) {
		ret = virtio_cmdq_queue_evt_buffer(vq, &evts[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int submit_cmdq_request(struct virtqueue *vq,
			       struct virtio_cmdq_req *vcr)
{
	struct scatterlist req_sg, res_sg, buf_sg[MAX_CMDQ_IOV_SIZE],
		*sgs[MAX_CMDQ_IOV_SIZE + 2];
	unsigned int num_out = 0, num_in = 0;

	sg_init_one(&req_sg, &vcr->req, sizeof(vcr->req));
	sgs[num_out++] = &req_sg;

	if (vcr->req.type == CMDQ_OPS_FLUSH) {
		struct cmdq_flush_request *flush_req = &vcr->req.flush_req;
		int i;

		for (i = 0; i < flush_req->iov_len; i++) {
			struct iovec *iov = &flush_req->iov[i];

			sg_init_one(&buf_sg[i],
				    phys_to_virt(flush_req->cmd_buf_paddrs[i]),
				    iov->iov_len);
			sgs[num_out++] = &buf_sg[i];
		}
	}

	sg_init_one(&res_sg, &vcr->res, sizeof(vcr->res));
	sgs[num_out + num_in++] = &res_sg;

	return virtqueue_add_sgs(vq, sgs, num_out, num_in, vcr, GFP_ATOMIC);
}

static int submit_req_and_wait_result(struct virtio_cmdq_req *vcr, uint32_t hwid)
{
	unsigned long flags;
	bool notify = false;
	int err;
	struct virtqueue *vq = g_cmdq[hwid]->vqs[VIRTIO_CMDQ_VQ_REQ];

	WARN_ON_ONCE(g_cmdq[hwid] == NULL || vcr == NULL);

	spin_lock_irqsave(&g_cmdq[hwid]->lock, flags);
	err = submit_cmdq_request(vq, vcr);
	if (err) {
		spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);
		err = -ENOMEM;
		goto out;
	}

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);

	if (notify)
		virtqueue_notify(vq);

	wait_for_completion(&vcr->done);
	err = vcr->res.ret;
out:
	return err;
}

s32 virtio_cmdq_pkt_flush_async(struct cmdq_pkt *pkt, cmdq_async_flush_cb cb,
				void *data)
{
	struct virtio_cmdq_req *vcr;
	struct client_record *rec;
	int ret;
	uint32_t hwid;

	vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	WARN_ON_ONCE(vcr == NULL);

	ret = virtio_cmdq_platform->check_pkt_finalize((void *)pkt);
	if (ret < 0)
		return ret;

	hwid = virtual_cmdq_util_get_hw_id(pkt);
	rec = find_client_record((uint64_t)pkt, hwid);
	if (!rec) {
		rec = kzalloc(sizeof(*rec), GFP_KERNEL);
		if (!rec)
			return -ENOMEM;

		rec->key = (uint64_t)pkt;
		rec->pkt = pkt;
		add_client_record(rec, hwid);
	}

	rec->cb = cb;
	rec->data = data;
	rec->host_result = 0;

	vcr->req.type = CMDQ_OPS_FLUSH;
	build_from_pkt(pkt, &vcr->req.flush_req);
	init_completion(&vcr->done);

	ret = submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);

	return ret;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_flush_async);

int virtio_cmdq_pkt_wait_complete(struct cmdq_pkt *pkt)
{
	struct client_record *rec;
	uint32_t hwid;

	hwid = virtual_cmdq_util_get_hw_id(pkt);
	rec = find_client_record((uint64_t)pkt, hwid);
	WARN_ON_ONCE(rec == NULL);

	wait_for_completion(&pkt->cmplt);

	return rec->host_result;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_wait_complete);

void virtio_cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	struct client_record *rec;
	struct virtio_cmdq_req *vcr = kmalloc(sizeof(*vcr), GFP_KERNEL);
	uint32_t hwid = virtual_cmdq_util_get_hw_id(pkt);

	WARN_ON_ONCE(vcr == NULL);
	vcr->req.destroy_req.key = (uint64_t)pkt;
	vcr->req.destroy_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_DESTROY;
	init_completion(&vcr->done);
	hwid = virtual_cmdq_util_get_hw_id(pkt);

	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);

	rec = remove_client_record((uint64_t)pkt, hwid);
	WARN_ON_ONCE(rec == NULL);
	kfree(rec);
}
EXPORT_SYMBOL(virtio_cmdq_pkt_destroy);

void virtio_cmdq_mbox_channel_stop(struct mbox_chan *chan)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct cmdq_thread *thread = chan->con_priv;
	uint32_t hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(chan));

	WARN_ON_ONCE(vcr == NULL);
	vcr->req.chan_stop_req.thread_id = thread->idx;
	vcr->req.chan_stop_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_CHAN_STOP;
	init_completion(&vcr->done);
	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);
}
EXPORT_SYMBOL(virtio_cmdq_mbox_channel_stop);

void virtio_cmdq_mbox_enable(void *chan)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	uint32_t hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(chan));

	WARN_ON_ONCE(vcr == NULL);
	vcr->req.mbox_enable_req.thread_id = thread->idx;
	vcr->req.mbox_enable_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_MBOX_ENABLE;
	init_completion(&vcr->done);

	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);
}
EXPORT_SYMBOL(virtio_cmdq_mbox_enable);

void virtio_cmdq_mbox_enable_clk(void *chan)
{
	struct cmdq_service *cmdq = container_of(
		((struct mbox_chan *)chan)->mbox, typeof(*cmdq), mbox);
	int err;

	WARN_ON(clk_prepare(cmdq->clock) < 0);

	err = clk_enable(cmdq->clock);
	if (err < 0) {
		cmdq_err("failed to enable cmq clk.\n");
		return;
	}

	err = clk_enable(cmdq->clock_timer);
	if (err < 0)
		cmdq_err("failed to enable cmq  clk.\n");
}
EXPORT_SYMBOL(virtio_cmdq_mbox_enable_clk);

static int cmdq_mbox_send_data(struct mbox_chan *chan, void *data)
{
	WARN_ON_ONCE();
	return 0;
}

static int cmdq_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = true;
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = false;
}

static bool cmdq_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static void cmdq_config_dma_mask(struct device *dev)
{
	u32 dma_mask_bit = 0;
	s32 ret;

	ret = of_property_read_u32(dev->of_node, "dma-mask-bit",
		&dma_mask_bit);
	/* if not assign from dts, give default 32bit for legacy chip */
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = 32;
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(dma_mask_bit));
	if (ret)
		cmdq_err("virtio mbox set dma mask bit:%u result:%d\n",
			dma_mask_bit, ret);
	else
		cmdq_msg("virtio mbox set dma mask bit:%u result:%d\n",
			dma_mask_bit, ret);
}

static const struct mbox_chan_ops cmdq_mbox_chan_ops = {
	.send_data = cmdq_mbox_send_data,
	.startup = cmdq_mbox_startup,
	.shutdown = cmdq_mbox_shutdown,
	.last_tx_done = cmdq_mbox_last_tx_done,
};

static struct mbox_chan *cmdq_xlate(struct mbox_controller *mbox,
				    const struct of_phandle_args *sp)
{
	int ind = sp->args[0];
	struct cmdq_thread *thread;

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	thread = mbox->chans[ind].con_priv;
	thread->timeout_ms =
		sp->args[1] != 0 ? sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
}

int virtio_cmdq_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int error;
	int i;
	static uint32_t hwid;
	struct cmdq_service *cmdq = NULL;

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, CMDQ_THR_MAX_COUNT,
					sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans) {
		error = -ENOMEM;
		goto err_out;
	}

	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_xlate;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		cmdq_err("failed to get resource");
		return -EINVAL;
	}
	cmdq->base = devm_ioremap_resource(dev, res);
	cmdq->base_pa = res->start;
	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;
	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		cmdq->thread[i].idx = i;
		cmdq->thread[i].is_virtio = true;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
	}

	cmdq->clock = devm_clk_get(dev, "gce");
	if (IS_ERR(cmdq->clock)) {
		cmdq_err("failed to get gce clk");
		cmdq->clock = NULL;
	}

	cmdq->clock_timer = devm_clk_get(dev, "gce-timer");
	if (IS_ERR(cmdq->clock_timer)) {
		cmdq_err("failed to get gce timer clk");
		cmdq->clock_timer = NULL;
	}

	error = mbox_controller_register(&cmdq->mbox);
	if (error < 0)
		goto err_free_chan;
	cmdq_config_dma_mask(dev);

	cmdq_msg("virtual cmdq driver info name: %s, hwid: %d base_pa: %lld",
		pdev->name, hwid, cmdq->base_pa);
	g_cmdq_service[hwid] = *cmdq;
	spin_lock_init(&g_cmdq_service[hwid].client_pkt_list_lock);
	INIT_LIST_HEAD(&g_cmdq_service[hwid].client_pkt_list);
	cmdq->hwid = hwid++;
	return 0;

err_free_chan:
	kfree(cmdq->mbox.chans);
err_out:
	return error;
}

static const struct of_device_id dt_match[] = {
	{
		.compatible = "grt,virtio-cmdq",
	},
	{ /* sentinel */ },
};

static struct platform_driver virtio_cmdq_mbox_driver = {
	.probe = virtio_cmdq_mbox_probe,
	.driver = {
		.name   = "virtio_cmdq",
		.of_match_table = dt_match,
	},
};

static int virtio_cmdq_probe(struct virtio_device *vdev)
{
	vq_callback_t *vq_cbs[VIRTIO_CMDQ_VQ_MAX] = { cmdq_request_done,
						      cmdq_evt_done };
	const char *names[VIRTIO_CMDQ_VQ_MAX] = { "request", "event" };
	struct virtio_cmdq *cmdq;
	int err = -EINVAL;
	static u8 hwid;

	cmdq = kzalloc(sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	cmdq->vdev = vdev;
	spin_lock_init(&cmdq->lock);
	spin_lock_init(&cmdq->evt_lock);

	err = virtio_find_vqs(vdev, VIRTIO_CMDQ_VQ_MAX, cmdq->vqs, vq_cbs, names,
			      NULL);
	if (err)
		goto err;

	virtio_device_ready(vdev);

	mdelay(300);
	err = fill_cmdq_evt(cmdq);
	if (err)
		goto err;

	vdev->priv = cmdq;
	g_cmdq[hwid] = cmdq;
	cmdq->hwid = hwid++;
	dev_info(&vdev->dev, "virtio cmdq initialized hwid %d\n", hwid);
	return 0;
err:
	dev_info(&vdev->dev, "virtio cmdq probe failed:%d\n", err);

	if (cmdq->vdev)
		vdev->config->del_vqs(cmdq->vdev);
	kfree(cmdq);
	return err;
}

static void virtio_cmdq_remove(struct virtio_device *vdev)
{
	struct virtio_cmdq *cmdq = vdev->priv;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(cmdq->vdev);
	vdev->config->del_vqs(cmdq->vdev);
	kfree(cmdq);
	g_cmdq[0] = NULL;
	g_cmdq[1] = NULL;
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_CMDQ, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {};

static struct virtio_driver virtio_cmdq_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_cmdq_probe,
	.remove = virtio_cmdq_remove,
};

static int virtio_cmdq_init(void)
{
	int ret;

	ret = platform_driver_register(&virtio_cmdq_mbox_driver);
	WARN_ON_ONCE(ret != 0);

	return register_virtio_driver(&virtio_cmdq_driver);
}

subsys_initcall(virtio_cmdq_init);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("virtio cmdq driver");
MODULE_LICENSE("GPL");
