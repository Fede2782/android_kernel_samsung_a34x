// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#define DEBUG

#define pr_fmt(fmt) "vhost-mbrain: " fmt

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/vhost.h>
#include <linux/mutex.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/llist.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "vhost.h"

#include "mbraink_hypervisor_virtio.h"

/* Guest driver can echo "mbrainkworld" message to device. */
#define VIRTIO_MBRAINK_F_ECHO_MSG 0

/* Max number of bytes transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others.
 */
#define VHOST_MBRAINK_WEIGHT 0x80000

/* Max number of packets transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others with small
 * pkts.
 */
#define VHOST_MBRAINK_PKT_WEIGHT 256

#define ZX_OK (0)
#define ZX_ERR_NOT_SUPPORTED (-2)

enum {
	VHOST_MBRAINK_FEATURES = (1ULL << VIRTIO_RING_F_INDIRECT_DESC)
			| (1ULL << VIRTIO_RING_F_EVENT_IDX)
			| (1ULL << VIRTIO_MBRAINK_F_ECHO_MSG),
};

enum {
	VIRTIO_MBRAINK_Q_COMMAND = 0,
	VIRTIO_MBRAINK_Q_EVENT = 1,
	VIRTIO_MBRAINK_Q_COUNT = 2,
};

enum {
	VIRTIO_MBRAINK_CMD_ECHO_SYNC = 0,
	VIRTIO_MBRAINK_CMD_ECHO_ASYNC = 1,
	VIRTIO_MBRAINK_CMD_START_EVENT = 2,
};

enum {
	VIRTIO_MBRAINK_EVENT_TYPE_NORMAL = 0,
	VIRTIO_MBRAINK_EVENT_TYPE_CB = 1,
};

/**
 * Request, Response, and Event protocol for virtio-mbraink device.
 */
struct virtio_mbraink_req {
	uint32_t id;
	uint32_t cmd;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};

struct virtio_mbraink_rsp {
	uint32_t rc;
	uint8_t data[MAX_VIRTIO_SEND_BYTE];
};

struct virtio_mbraink_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	union {
		uint8_t data[MAX_VIRTIO_SEND_BYTE];
		struct virtio_mbraink_rsp rsp;
	};
};

struct event_buffer_entry {
	struct virtio_mbraink_event event;
	struct llist_node llnode;
};

struct vhost_mbraink {
	struct vhost_virtqueue vq[VIRTIO_MBRAINK_Q_COUNT];
	struct vhost_dev dev;
	struct vhost_work work;
	spinlock_t evt_lock;
	unsigned int evt_nr;
	struct event_buffer_entry *evt_buf;
	struct llist_head evt_pool;
	struct llist_head evt_queue;
};

struct vhost_mbraink *vmbraink;
struct mutex mbraink_using_mutex;

static int vhost_send_msg_to_client(const void *data_buf)
{
	struct event_buffer_entry *evt;
	struct llist_node *node;
	ssize_t length = 0;

	if (vmbraink == NULL) {
		pr_info("%s ERROR: vmbraink is not initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock(&vmbraink->evt_lock);
	node = llist_del_first(&vmbraink->evt_pool);
	if (node == NULL) {
		WARN_ON_ONCE(node == NULL);
		goto send_err;
	}
	if (!llist_add(node, &vmbraink->evt_queue)) {
		pr_info("%s ERROR: llist add fail!\n", __func__);
		goto send_err;
	}
	spin_unlock(&vmbraink->evt_lock);

	evt = container_of(node, typeof(*evt), llnode);
	if (evt == NULL) {
		WARN_ON_ONCE(evt == NULL);
		return -EINVAL;
	}
	evt->event.type = VIRTIO_MBRAINK_EVENT_TYPE_NORMAL;
	length = strscpy(evt->event.data, data_buf, sizeof(evt->event.data));
	if (length <= 0 || length > sizeof(evt->event.data)) {
		pr_info("%s ERROR: strscpy failed\n", __func__);
		return -EINVAL;
	}

	return vhost_vq_work_queue(&vmbraink->vq[VIRTIO_MBRAINK_Q_EVENT], &vmbraink->work);

send_err:
	spin_unlock(&vmbraink->evt_lock);
	return -EINVAL;
}

/* Host kick us for I/O completion */
static void vhost_mbraink_handle_host_kick(struct vhost_work *work)
{
	struct vhost_virtqueue *vq;
	struct event_buffer_entry *evt;
	struct llist_node *llnode;
	struct llist_head reclaim_head;
	struct vhost_mbraink *mbraink;
	bool added;
	unsigned long flags;
	size_t copy_len = sizeof(struct virtio_mbraink_event);
	int head, in, out;

	init_llist_head(&reclaim_head);
	mbraink = container_of(work, struct vhost_mbraink, work);
	vq = &mbraink->vq[VIRTIO_MBRAINK_Q_EVENT];

	spin_lock_irqsave(&mbraink->evt_lock, flags);
	while ((llnode = llist_del_first(&mbraink->evt_queue)) != NULL)
		llist_add(llnode, &reclaim_head);

	spin_unlock_irqrestore(&mbraink->evt_lock, flags);

	added = false;
	llnode = reclaim_head.first;
	vhost_disable_notify(&mbraink->dev, vq);
	while (llnode) {
		struct iov_iter iter;

		evt = llist_entry(llnode, struct event_buffer_entry, llnode);
		llnode = llist_next(llnode);
		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov), &out,
					 &in, NULL, NULL);

		if (unlikely(head < 0)) {
			vq_err(vq, "failed to get vring desc: %d\n", head);
			continue;
		}

		if (unlikely(head == vq->num)) {
			if (unlikely(vhost_enable_notify(&mbraink->dev, vq))) {
				vhost_disable_notify(&mbraink->dev, vq);
				continue;
			}
			continue;
		}

		iov_iter_init(&iter, ITER_DEST, &vq->iov[0], 1, copy_len);
		if (copy_to_iter(&evt->event, copy_len, &iter) != copy_len) {
			vq_err(vq, "Failed to write event\n");
			continue;
		}

		vhost_add_used(vq, head, copy_len);
		added = true;
	}

	if (likely(added))
		vhost_signal(&mbraink->dev, &mbraink->vq[VIRTIO_MBRAINK_Q_EVENT]);

	spin_lock_irqsave(&mbraink->evt_lock, flags);
	while ((llnode = llist_del_first(&reclaim_head)) != NULL)
		llist_add(llnode, &mbraink->evt_pool);

	spin_unlock_irqrestore(&mbraink->evt_lock, flags);
}

static void handle_mbraink_request(struct vhost_mbraink *mbraink,
		struct vhost_virtqueue *vq, struct virtio_mbraink_req *req,
		struct virtio_mbraink_rsp *rsp)
{
	switch (req->cmd) {
	case VIRTIO_MBRAINK_CMD_ECHO_SYNC: {
		pr_info("SYNC[BE]MBraink device received command(id:%d cmd:%d data:%s length:%d)\n",
				req->id, req->cmd, req->data, strlen(req->data));
		mbraink_netlink_send_msg(req->data);

		strscpy((char *) rsp->data, "OK", sizeof(rsp->data));
		rsp->rc = ZX_OK;
		break;
	}
	default: {
		pr_info("%s : unsupported command type: %d\n", __func__, req->cmd);
		break;
	}
	}
}

static void vhost_mbraink_handle_guest_cmd_kick(struct vhost_work *work)
{
	struct virtio_mbraink_req req;
	struct virtio_mbraink_rsp resp;
	struct vhost_virtqueue *vq;
	struct vhost_mbraink *mbraink;
	int head;
	bool added = false;

	vq = container_of(work, struct vhost_virtqueue, poll.work);
	mbraink = container_of(vq->dev, struct vhost_mbraink, dev);

	vhost_disable_notify(&mbraink->dev, vq);
	for (;;) {
		int in, out, ret, used = 0;
		size_t out_size, in_size;
		struct iovec *out_iov, *in_iov;
		struct iov_iter out_iter, in_iter;

		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov), &out,
					 &in, NULL, NULL);
		if (unlikely(head < 0)) {
			vq_err(vq, "failed to get vring desc: %d\n", head);
			break;
		}

		if (unlikely(head == vq->num)) {
			if (unlikely(vhost_enable_notify(&mbraink->dev, vq))) {
				vhost_disable_notify(&mbraink->dev, vq);
				continue;
			}
			break;
		}

		out_iov = vq->iov;
		out_size = iov_length(out_iov, out);
		iov_iter_init(&out_iter, ITER_SOURCE, out_iov, out, out_size);
		ret = copy_from_iter(&req, sizeof(req), &out_iter);
		if (ret != sizeof(req)) {
			vq_err(vq, "Failed to copy request, ret=%d\n", ret);
			vhost_discard_vq_desc(vq, 1);
			break;
		}

		handle_mbraink_request(mbraink, vq, &req, &resp);

		if (in > 0) {
			in_iov = &vq->iov[out];
			in_size = iov_length(in_iov, in);

			iov_iter_init(&in_iter, ITER_DEST, in_iov, in, in_size);
			ret = copy_to_iter(&resp, sizeof(resp), &in_iter);
			if (ret != sizeof(resp)) {
				vq_err(vq, "Failed to copy result, ret=%d\n", ret);
				vhost_discard_vq_desc(vq, 1);
				break;
			}

			used += ret;
		}

		vhost_add_used(vq, head, used);
		added = true;
	}

	if (added)
		vhost_signal(&mbraink->dev, vq);

}

static void vhost_mbraink_handle_guest_evt_kick(struct vhost_work *work)
{
}

static int vhost_mbraink_open(struct inode *inode, struct file *file)
{
	struct vhost_virtqueue **vqs;
	int ret = 0;

	mutex_lock(&mbraink_using_mutex);

	vmbraink = kvzalloc(sizeof(*vmbraink), GFP_KERNEL);
	if (!vmbraink) {
		ret = -ENOMEM;
		goto out;
	}

	vqs = kcalloc(VIRTIO_MBRAINK_Q_COUNT, sizeof(*vqs), GFP_KERNEL);
	if (!vqs) {
		ret = -ENOMEM;
		goto out_mbraink;
	}

	spin_lock_init(&vmbraink->evt_lock);
	init_llist_head(&vmbraink->evt_pool);
	init_llist_head(&vmbraink->evt_queue);

	vmbraink->vq[VIRTIO_MBRAINK_Q_COMMAND].handle_kick = vhost_mbraink_handle_guest_cmd_kick;
	vmbraink->vq[VIRTIO_MBRAINK_Q_EVENT].handle_kick = vhost_mbraink_handle_guest_evt_kick;

	vqs[VIRTIO_MBRAINK_Q_COMMAND] = &vmbraink->vq[VIRTIO_MBRAINK_Q_COMMAND];
	vqs[VIRTIO_MBRAINK_Q_EVENT] = &vmbraink->vq[VIRTIO_MBRAINK_Q_EVENT];

	vhost_work_init(&vmbraink->work, vhost_mbraink_handle_host_kick);

	vhost_dev_init(&vmbraink->dev, vqs, VIRTIO_MBRAINK_Q_COUNT, UIO_MAXIOV,
			VHOST_MBRAINK_PKT_WEIGHT, VHOST_MBRAINK_WEIGHT, true, NULL);
	file->private_data = vmbraink;

	mutex_unlock(&mbraink_using_mutex);

	return ret;
out_mbraink:
	kvfree(vmbraink);
out:
	mutex_unlock(&mbraink_using_mutex);
	return ret;
}

static int vhost_mbraink_release(struct inode *inode, struct file *f)
{
	struct vhost_mbraink *mbraink = f->private_data;

	mutex_lock(&mbraink_using_mutex);

	vhost_dev_stop(&mbraink->dev);
	vhost_dev_cleanup(&mbraink->dev);
	kfree(mbraink->dev.vqs);
	kfree(mbraink->evt_buf);
	kvfree(mbraink);
	vmbraink = NULL;

	mutex_unlock(&mbraink_using_mutex);

	return 0;
}

static int vhost_mbraink_set_features(struct vhost_mbraink *mbraink, u64 features)
{
	struct vhost_virtqueue *vq;
	int i;

	mutex_lock(&mbraink->dev.mutex);
	for (i = 0; i < VIRTIO_MBRAINK_Q_COUNT; i++) {
		vq = &mbraink->vq[i];
		mutex_lock(&vq->mutex);
		vq->acked_features = features;
		mutex_unlock(&vq->mutex);
	}
	mutex_unlock(&mbraink->dev.mutex);

	return 0;
}

static long vhost_mbraink_reset_owner(struct vhost_mbraink *mbraink)
{

	struct vhost_iotlb *umem;
	int err;

	mutex_lock(&mbraink->dev.mutex);
	err = vhost_dev_check_owner(&mbraink->dev);
	if (err)
		goto done;

	umem = vhost_dev_reset_owner_prepare();
	if (!umem) {
		err = -ENOMEM;
		goto done;
	}

	vhost_dev_reset_owner(&mbraink->dev, umem);

done:
	mutex_unlock(&mbraink->dev.mutex);
	return err;
}

static int vhost_mbraink_setup(struct vhost_mbraink *mbraink)
{
	int i;
	struct event_buffer_entry *evt;

	mbraink->evt_nr = mbraink->vq[VIRTIO_MBRAINK_Q_EVENT].num;

	mbraink->evt_buf = kmalloc_array(mbraink->evt_nr,
		sizeof(struct event_buffer_entry), GFP_KERNEL);
	if (!mbraink->evt_buf)
		return -ENOMEM;

	for (i = 0; i < mbraink->evt_nr; i++) {
		evt = &mbraink->evt_buf[i];
		llist_add(&evt->llnode, &mbraink->evt_pool);
	}

	return 0;
}

static long vhost_mbraink_ioctl(struct file *f, unsigned int ioctl,
			    unsigned long arg)
{
	struct vhost_mbraink *mbraink = f->private_data;
	void __user *argp = (void __user *)arg;
	u64 __user *featurep = argp;
	u64 features;
	int ret;

	switch (ioctl) {
	case VHOST_GET_FEATURES:
		features = VHOST_MBRAINK_FEATURES;
		if (copy_to_user(featurep, &features, sizeof(features)))
			return -EFAULT;
		return 0;
	case VHOST_SET_FEATURES:
		if (copy_from_user(&features, featurep, sizeof(features)))
			return -EFAULT;
		if (features & ~VHOST_MBRAINK_FEATURES)
			return -EOPNOTSUPP;
		return vhost_mbraink_set_features(mbraink, features);
	case VHOST_RESET_OWNER:
		return vhost_mbraink_reset_owner(mbraink);
	default:
		mutex_lock(&mbraink->dev.mutex);
		ret = vhost_dev_ioctl(&mbraink->dev, ioctl, argp);
		if (ret == -ENOIOCTLCMD)
			ret = vhost_vring_ioctl(&mbraink->dev, ioctl, argp);
		if (!ret && ioctl == VHOST_SET_VRING_NUM)
			ret = vhost_mbraink_setup(mbraink);
		mutex_unlock(&mbraink->dev.mutex);
		return ret;
	}
}

static const struct file_operations vhost_mbraink_fops = {
	.owner          = THIS_MODULE,
	.open           = vhost_mbraink_open,
	.release        = vhost_mbraink_release,
	.llseek         = noop_llseek,
	.unlocked_ioctl = vhost_mbraink_ioctl,
};

static struct miscdevice vhost_mbraink_misc = {
	MISC_DYNAMIC_MINOR,
	"vhost-mbrain",
	&vhost_mbraink_fops,
};

int vhost_mbraink_init(void)
{
	mutex_init(&mbraink_using_mutex);
	vmbraink = NULL;
	return misc_register(&vhost_mbraink_misc);
}

void vhost_mbraink_deinit(void)
{
	misc_deregister(&vhost_mbraink_misc);
}

int h2c_send_msg(u32 cmdType, void *cmdData)
{
	int ret = 0;
	int sptr = 0;
	void *data_buf = NULL;

	data_buf = kvzalloc(sizeof(uint8_t) * MAX_VIRTIO_SEND_BYTE, GFP_KERNEL);
	if (!data_buf)
		return -1;

	switch (cmdType) {
	case H2C_CMD_StaticInfo:
		sptr = snprintf(data_buf, sizeof(uint8_t) * MAX_VIRTIO_SEND_BYTE,
			 "CMD:%c", H2C_CMD_StaticInfo);
		break;
	case H2C_CMD_ClientTraceCatch:
		sptr = snprintf(data_buf, sizeof(uint8_t) * MAX_VIRTIO_SEND_BYTE,
			 "CMD:%c", H2C_CMD_ClientTraceCatch);
		break;
	default:
		pr_info("%s: unknown command type %d\n", __func__, cmdType);
		break;
	}

	if (sptr <= 0 || sptr > sizeof(uint8_t) * MAX_VIRTIO_SEND_BYTE) {
		pr_info("%s: invalid send byte size %zu.\n", __func__, sptr);
		ret = -1;
	} else {
		if (mutex_trylock(&mbraink_using_mutex)) {
			ret = (vhost_send_msg_to_client(data_buf) <= 0) ? -1 : 0;
			mutex_unlock(&mbraink_using_mutex);
		} else {
			ret = -1;
			pr_info("%s: mutex lock failed.\n", __func__);
		}
	}

err:
	kvfree(data_buf);
	return ret;
}
