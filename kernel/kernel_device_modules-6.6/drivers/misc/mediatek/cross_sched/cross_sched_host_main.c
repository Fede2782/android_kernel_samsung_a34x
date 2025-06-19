// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#define DEBUG

#define pr_fmt(fmt) "vhost-vm: " fmt
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#include <uapi/linux/sched/types.h>
#include <linux/sched/types.h>
#endif

#include "vhost.h"

/* Guest driver can echo "vmworld" message to device. */
#define VIRTIO_VM_F_ECHO_MSG 0

/* Max number of bytes transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others.
 */
#define VHOST_VM_WEIGHT 0x80000

/* Max number of packets transferred before requeueing the job.
 * Using this limit prevents one virtqueue from starving others with small
 * pkts.
 */
#define VHOST_VM_PKT_WEIGHT 256

#define ZX_OK (0)
#define ZX_ERR_NOT_SUPPORTED (-2)

enum {
	VHOST_VM_FEATURES = (1ULL << VIRTIO_RING_F_INDIRECT_DESC)
			| (1ULL << VIRTIO_RING_F_EVENT_IDX)
			| (1ULL << VIRTIO_VM_F_ECHO_MSG),
};

enum {
	VIRTIO_VM_Q_COMMAND = 0,
	VIRTIO_VM_Q_EVENT = 1,
	VIRTIO_VM_Q_COUNT = 2,
};

enum {
	VIRTIO_VM_CMD_SCHED_SYNC = 0,
	VIRTIO_VM_CMD_ECHO_ASYNC = 1,
	VIRTIO_VM_CMD_START_EVENT = 2,
};

enum {
	VIRTIO_VM_EVENT_TYPE_NORMAL = 0,
	VIRTIO_VM_EVENT_TYPE_CB = 1,
};

/**
 * Request, Response, and Event protocol for virtio-vm device.
 */
struct vm_cpu {
	bool				vcpu_offline;
	unsigned long		util;
};

struct virtio_vm_req {
	uint32_t id;
	uint32_t cmd;
	unsigned long util_sum;
	struct vm_cpu cpu_info[8];
	bool need_update;
};
struct virtio_vm_rsp {
	uint32_t rc;
	int offline_mask;
};
struct virtio_vm_event {
	uint32_t id;
	uint32_t type;
	uint32_t cmd_id;
	union {
		uint8_t data[256];
		struct virtio_vm_rsp rsp;
	};
};

struct event_buffer_entry {
	struct virtio_vm_event event;
	struct llist_node llnode;
};

struct vhost_vm {
	struct vhost_virtqueue vq[VIRTIO_VM_Q_COUNT];
	struct vhost_dev dev;
	struct vhost_work work;
	spinlock_t evt_lock;
	unsigned int evt_nr;
	struct event_buffer_entry *evt_buf;
	struct llist_head evt_pool;
	struct llist_head evt_queue;
};


#define VM_SUM_UTIL_RATE(num)	((num)*60/100)

#define VM_CPU_NUM			8

static int vcpu_host_offline_mask;
static int vcpu_client_offline_mask;
static int index_host_org;
static bool do_update;
static unsigned long vm_cpu_cap_watermark[5];
static unsigned long host_cpu_util_sum;
struct vm_cpu host_cpu[VM_CPU_NUM];
struct vm_cpu client_cpu[VM_CPU_NUM];

struct task_struct *corss_domain_thread;

/*init watermark for 5 levels:
 * level 0: one little core
 * level 1: one big core
 * level 2: two little cores
 * level 3: one little core and one big core
 * level 4: two little cores and one big core
 */
static void vm_init_cpu_capacity(void)
{
	unsigned long capacity_l = 0;
	unsigned long capacity_b = 0;

	capacity_l = READ_ONCE(cpu_rq(0)->cpu_capacity_orig);
	capacity_b = READ_ONCE(cpu_rq(4)->cpu_capacity_orig);


	vm_cpu_cap_watermark[0] = capacity_l; //one little core
	vm_cpu_cap_watermark[1] = capacity_b; //one big core
	vm_cpu_cap_watermark[2] = capacity_l * 2; //two little cores
	vm_cpu_cap_watermark[3] = capacity_l + capacity_b; //one little core and one big core
	vm_cpu_cap_watermark[4] = (capacity_l * 2) + capacity_b; //two little cores and one big core
}

/*vm_get_host_cpu_util: update host util
 * host_cpu_util_sum: host all cpus util sum
 * host_cpu[cpu]: each cpu util
 */
static void vm_get_host_cpu_util(void)
{
	int cpu;
	unsigned long util_org = 0;

	host_cpu_util_sum = 0;
	for_each_possible_cpu(cpu) {
		if (cpu >= 0 && cpu < VM_CPU_NUM) {
			if (!cpu_online(cpu) || !cpu_active(cpu)) {
				host_cpu[cpu].util = 0;
			} else {
				util_org = READ_ONCE(cpu_rq(cpu)->cfs.avg.util_avg);
				if (sched_feat(UTIL_EST))
					util_org = max_t(unsigned long, util_org,
						READ_ONCE(cpu_rq(cpu)->cfs.avg.util_est));
				host_cpu[cpu].util = min(util_org, READ_ONCE(cpu_rq(cpu)->cpu_capacity_orig));
			}
			host_cpu_util_sum += host_cpu[cpu].util;
		}
	}
}

extern int core_ctl_force_pause_cpu(unsigned int cpu, bool is_pause);

/*update_offline_host_vm_cpu: update paused cpu
 * offline_mask: cpu paused mask
 */
static void update_offline_host_vm_cpu(int offline_mask)
{
	int cpu;
	int result = -1;

	for_each_possible_cpu(cpu) {
		if (cpu >= (VM_CPU_NUM - 1)) {
			//skip last one cpu
			continue;
		}

		if (offline_mask & (1 << cpu)) {
			if (!host_cpu[cpu].vcpu_offline) {
				result = core_ctl_force_pause_cpu(cpu, true);
				if (!result)
					host_cpu[cpu].vcpu_offline = true;
			}
		} else {
			if (host_cpu[cpu].vcpu_offline) {
				result = core_ctl_force_pause_cpu(cpu, false);
				if (!result)
					host_cpu[cpu].vcpu_offline = false;
			}
		}
	}
}

/*find_case_index: find case index from watermark
 * util_value: host sum util
 * watermark: init from vm_init_cpu_capacity()
 * size: watermark size, now size = 5
 */
static int find_case_index(unsigned long util_value, unsigned long *watermark, int size)
{
	if (util_value < watermark[0])
		return 0;

	if (util_value == watermark[0])
		return 1;

	for (int i = 1; i < size; i++) {
		if (util_value < watermark[i])
			return i;
	}

	return size;
}

/**
 * select_offline_vm_cpu() - Updates the vm cpu mask.
 * @cpu_util_sum: input total cpu util to choose case index
 * @cpus: input cpus to check cpu offline status
 * @cpus_r: input cpus_r to get cpu util
 * @cpu_offline_mask: update vm cpu mask
 */
static void select_offline_vm_cpu(unsigned long cpu_util_sum,
			struct vm_cpu *cpus, struct vm_cpu *cpus_r, int *cpu_offline_mask)
{
	int case_index = 0;
	int little_cpu_min = 0;
	int little_cpu_min_s = 0;
	int firstMin = INT_MAX;
	int secondMin = INT_MAX;
	int big_cpu_min = 4;
	unsigned long cpu_util_sum_rate = 0;
	int i = 0;

	cpu_util_sum_rate = (unsigned long)VM_SUM_UTIL_RATE(cpu_util_sum); //rate = 60%

	int size = ARRAY_SIZE(vm_cpu_cap_watermark);

	case_index = find_case_index(cpu_util_sum_rate, vm_cpu_cap_watermark, size);
	pr_info("test case_index = %d\n", case_index);

	if (case_index == 0) {
		*cpu_offline_mask = 0;
		return;
	}

	*cpu_offline_mask = 0;
	switch (case_index) {
	case 1: //one little core
		for (i = 0; i < 4; i++) {
			if (cpus[i].vcpu_offline)
				continue;
			if(cpus_r[i].util < firstMin) {
				firstMin = cpus_r[i].util;
				little_cpu_min = i;
			}
		}
		*cpu_offline_mask |= (1 << little_cpu_min);
		pr_info("test case_index[%d]: little_cpu_min=%d cpu_offline_mask = %d\n",
				case_index, little_cpu_min, *cpu_offline_mask);
		break;

	case 2: //one big core
		for (i = 4; i < 7; i++) {
			if (cpus[i].vcpu_offline)
				continue;
			if(cpus_r[i].util < firstMin) {
				firstMin = cpus_r[i].util;
				big_cpu_min = i;
			}
		}
		*cpu_offline_mask |= (1 << big_cpu_min);
		pr_info("test case_index[%d]: big_cpu_min=%d cpu_offline_mask = %d\n",
			case_index, big_cpu_min, *cpu_offline_mask);
		break;

	case 3: //two little cores
		for (i = 0; i < 4; i++) {
			if (cpus[i].vcpu_offline)
				continue;

			if (cpus_r[i].util < firstMin) {
				secondMin = firstMin;
				firstMin = cpus_r[i].util;
				little_cpu_min_s = little_cpu_min;
				little_cpu_min = i;
			} else {
				if (cpus_r[i].util < secondMin && cpus_r[i].util != firstMin) {
					secondMin = cpus_r[i].util;
					little_cpu_min_s = i;
				}
			}
		}
		*cpu_offline_mask |= (1 << little_cpu_min);
		*cpu_offline_mask |= (1 << little_cpu_min_s);
		pr_info("test case_index[%d]: little_cpu_min=%d little_cpu_min_s=%d cpu_offline_mask = %d\n",
				case_index, little_cpu_min, little_cpu_min_s, *cpu_offline_mask);
		break;

	case 4: //one little core and one big core
		for (i = 0; i < 4; i++) {
			if (cpus[i].vcpu_offline)
				continue;
			if(cpus_r[i].util < firstMin) {
				firstMin = cpus_r[i].util;
				little_cpu_min = i;
			}
		}
		*cpu_offline_mask |= (1 << little_cpu_min);

		firstMin = INT_MAX;
		for (i = 4; i < 7; i++) {
			if (cpus[i].vcpu_offline)
				continue;

			if(cpus_r[i].util < firstMin) {
				firstMin = cpus_r[i].util;
				big_cpu_min = i;
			}
		}
		*cpu_offline_mask |= (1 << big_cpu_min);
		pr_info("hpj test case_index[%d]: little_cpu_min=%d big_cpu_min=%d cpu_offline_mask = %d\n",
				case_index, little_cpu_min, big_cpu_min, *cpu_offline_mask);
		break;

	case 5: //two little cores and one big core
		for (i = 0; i < 4; i++) {
			if (cpus[i].vcpu_offline)
				continue;

			if (cpus_r[i].util < firstMin) {
				secondMin = firstMin;
				firstMin = cpus_r[i].util;
				little_cpu_min_s = little_cpu_min;
				little_cpu_min = i;
			} else {
				if (cpus_r[i].util < secondMin && cpus_r[i].util != firstMin) {
					secondMin = cpus_r[i].util;
					little_cpu_min_s = i;
				}
			}
		}
		*cpu_offline_mask |= (1 << little_cpu_min);
		*cpu_offline_mask |= (1 << little_cpu_min_s);

		firstMin = INT_MAX;
		for (i = 4; i < 7; i++) {
			if (cpus[i].vcpu_offline)
				continue;

			if(cpus_r[i].util < firstMin) {
				firstMin = cpus_r[i].util;
				big_cpu_min = i;
			}
		}
		*cpu_offline_mask |= (1 << big_cpu_min);
		pr_info("hpj test case_index[%d]: little_cpu_min=%d little_cpu_min_s=%d big_cpu_min=%d cpu_offline_mask = %d\n",
				case_index, little_cpu_min, little_cpu_min_s, big_cpu_min, *cpu_offline_mask);
		break;

	default:
		pr_info("no defined case!\n");
	}
}

#define VIRTIO_VM_EVENT_DATA_SIZE 256
/*vhost_trigger_sched_sync: trigger sched sync to client*/
static void vhost_trigger_sched_sync(struct vhost_virtqueue *vq, const char *data)
{
	struct vhost_vm *vm;
	struct event_buffer_entry *evt;
	struct llist_node *node;

	vm = container_of(vq->dev, struct vhost_vm, dev);

	spin_lock(&vm->evt_lock);
	node = llist_del_first(&vm->evt_pool);
	BUG_ON(node == NULL);
	llist_add(node, &vm->evt_queue);
	spin_unlock(&vm->evt_lock);

	evt = container_of(node, typeof(*evt), llnode);
	evt->event.type = VIRTIO_VM_EVENT_TYPE_NORMAL;
	strscpy(evt->event.data, data, VIRTIO_VM_EVENT_DATA_SIZE - 1);
	evt->event.data[VIRTIO_VM_EVENT_DATA_SIZE - 1] = '\0';

	vhost_vq_work_queue(vq, &vm->work);
}

/* Host kick us for I/O completion */
static void vhost_vm_handle_host_kick(struct vhost_work *work)
{
	struct vhost_virtqueue *vq;
	struct event_buffer_entry *evt;
	struct llist_node *llnode;
	struct llist_head reclaim_head;
	struct vhost_vm *vm;
	bool added;
	unsigned long flags;
	size_t copy_len = sizeof(struct virtio_vm_event);
	int head, in, out;

	init_llist_head(&reclaim_head);
	vm = container_of(work, struct vhost_vm, work);
	vq = &vm->vq[VIRTIO_VM_Q_EVENT];

	spin_lock_irqsave(&vm->evt_lock, flags);
	while ((llnode = llist_del_first(&vm->evt_queue)) != NULL)
		llist_add(llnode, &reclaim_head);

	spin_unlock_irqrestore(&vm->evt_lock, flags);

	added = false;
	llnode = reclaim_head.first;
	vhost_disable_notify(&vm->dev, vq);
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
			if (unlikely(vhost_enable_notify(&vm->dev, vq))) {
				vhost_disable_notify(&vm->dev, vq);
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
		vhost_signal(&vm->dev, &vm->vq[VIRTIO_VM_Q_EVENT]);

	spin_lock_irqsave(&vm->evt_lock, flags);
	while ((llnode = llist_del_first(&reclaim_head)) != NULL)
		llist_add(llnode, &vm->evt_pool);

	spin_unlock_irqrestore(&vm->evt_lock, flags);
}

struct trigger_arg {
	struct vhost_vm *vm;
	struct virtio_vm_req req;
	int count;
};


static void check_host_index(int *index)
{
	unsigned long cpu_util_sum_rate = 0;
	int case_size = 0;

	vm_get_host_cpu_util();
	cpu_util_sum_rate = (unsigned long)VM_SUM_UTIL_RATE(host_cpu_util_sum);
	case_size = ARRAY_SIZE(vm_cpu_cap_watermark);
	*index = find_case_index(cpu_util_sum_rate, vm_cpu_cap_watermark, case_size);
}

bool sched_host_thread;

/*vm_sched_host_thread: host side sched sync thread
 * vm_sched_client_thread trigger index check every 100ms
 * if host_case_index > index_host_org, trigger vhost_trigger_sched_sync
 */
static int vm_sched_host_thread(void *ptr)
{
	int host_case_index = 0;
	int i = 0;

	struct sched_param param = {
		.sched_priority = 99
	};

	struct trigger_arg *arg = kvzalloc(sizeof(*arg), GFP_KERNEL);

	if (!arg)
		return -ENOMEM;

	arg->vm = ptr;

	if (sched_setscheduler(current, SCHED_FIFO, &param) != 0) {
		kfree(arg);
		return -EFAULT;
	}

	sched_host_thread = true;

	while (!kthread_should_stop()) {
		if (!do_update) {
			check_host_index(&host_case_index);
			if (host_case_index > index_host_org) { //if host side need more cpu, update host side
				pr_info("host_case_index = %d index_host_org = %d\n", host_case_index, index_host_org);
				for (i = 0; i < 8; i++)
					pr_info("vm_get_host_cpu_util,host_cpu[%d].util:%lu\n",i, host_cpu[i].util);
				pr_info("vm_get_host_cpu_util,host_cpu_util_sum:%lu\n",host_cpu_util_sum);
				vhost_trigger_sched_sync(&arg->vm->vq[VIRTIO_VM_Q_EVENT],
					"Start Sync");
			}
		}
		msleep(100);
	}

	kfree(arg);
	return 0;
}

/*handle_vm_request: handle event form client
 * update host and client offline_mask
 * callback vcpu_client_offline_mask to client
 */
static void handle_vm_request(struct vhost_vm *vm,
		struct vhost_virtqueue *vq, struct virtio_vm_req *req,
		struct virtio_vm_rsp *rsp)
{
	int i = 0;
	int case_index = 0;

	switch (req->cmd) {
		case VIRTIO_VM_CMD_SCHED_SYNC: {
			do_update = true;
			check_host_index(&case_index);
			pr_info("case_index=%d,index_host_org:%d\n", case_index, index_host_org);
			if (case_index != index_host_org) {
				select_offline_vm_cpu(host_cpu_util_sum, host_cpu,
									req->cpu_info, &vcpu_client_offline_mask);
				index_host_org = case_index;
				for (i = 0; i < 8; i++)
					pr_info("vm_get_host_cpu_util,host_cpu[%d].util:%lu\n",i, host_cpu[i].util);
				pr_info("vm_get_host_cpu_util,host_cpu_util_sum:%lu\n",host_cpu_util_sum);
				pr_info("send new index: new_index:%d vcpu_client_offline_mask:%d \n",
						case_index, vcpu_client_offline_mask);
			}

			for (i = 0; i < 8; i++)
				pr_info("vm_get_client_cpu_util,client_cpu[%d].util:%lu\n", i, req->cpu_info[i].util);

			pr_info("client_util_sum,client_util_sum:%lu\n",req->util_sum);
			if (req->need_update) {
				select_offline_vm_cpu(req->util_sum, req->cpu_info, host_cpu, &vcpu_host_offline_mask);
				pr_info("test select_offline_vm_cpu,req->util_sum =%lu vcpu_host_offline_mask = %d \n",
						req->util_sum, vcpu_host_offline_mask);
				update_offline_host_vm_cpu(vcpu_host_offline_mask);
			}

			pr_info("[BE]Hello device received command(id:%d cmd:%d)\n",req->id, req->cmd);
			rsp->rc = ZX_OK;
			rsp->offline_mask = vcpu_client_offline_mask;
			if (!sched_host_thread) {
				corss_domain_thread = (struct task_struct *)kthread_run(vm_sched_host_thread,
						vm, "cross_domain_sched");
				if (IS_ERR(vm_sched_host_thread)) {
					pr_info("Error creating vm_sched_host_thread\n");
					return;
				}
			}
			do_update = false;
			break;
		}
	}
}

static void vhost_vm_handle_guest_cmd_kick(struct vhost_work *work)
{
	struct virtio_vm_req req;
	struct virtio_vm_rsp resp;
	struct vhost_virtqueue *vq;
	struct vhost_vm *vm;
	int head;
	bool added = false;

	vq = container_of(work, struct vhost_virtqueue, poll.work);
	vm = container_of(vq->dev, struct vhost_vm, dev);
	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	vhost_disable_notify(&vm->dev, vq);
	for (;;) {
		int in = 0, out = 0, ret = 0, used = 0;
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
			if (unlikely(vhost_enable_notify(&vm->dev, vq))) {
				vhost_disable_notify(&vm->dev, vq);
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

		handle_vm_request(vm, vq, &req, &resp);

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
		vhost_signal(&vm->dev, vq);

}

static void vhost_vm_handle_guest_evt_kick(struct vhost_work *work)
{
}

static int vhost_vm_open(struct inode *inode, struct file *file) //vhost_vm_open
{
	struct vhost_vm *vm;
	struct vhost_virtqueue **vqs;
	int ret = 0;

	vm = kvzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm) {
		ret = -ENOMEM;
		goto out;
	}

	vqs = kcalloc(VIRTIO_VM_Q_COUNT, sizeof(*vqs), GFP_KERNEL);
	if (!vqs) {
		ret = -ENOMEM;
		goto out_vm;
	}

	spin_lock_init(&vm->evt_lock);
	init_llist_head(&vm->evt_pool);
	init_llist_head(&vm->evt_queue);

	vm->vq[VIRTIO_VM_Q_COMMAND].handle_kick = vhost_vm_handle_guest_cmd_kick;
	vm->vq[VIRTIO_VM_Q_EVENT].handle_kick = vhost_vm_handle_guest_evt_kick;

	vqs[VIRTIO_VM_Q_COMMAND] = &vm->vq[VIRTIO_VM_Q_COMMAND];
	vqs[VIRTIO_VM_Q_EVENT] = &vm->vq[VIRTIO_VM_Q_EVENT];

	vhost_work_init(&vm->work, vhost_vm_handle_host_kick);

	vhost_dev_init(&vm->dev, vqs, VIRTIO_VM_Q_COUNT, UIO_MAXIOV,
			VHOST_VM_PKT_WEIGHT, VHOST_VM_WEIGHT, true, NULL);
	file->private_data = vm;

	vm_init_cpu_capacity();//int vm_cpu_cap_watermark

	return ret;
out_vm:
	kvfree(vm);
out:
	return ret;
}

static int vhost_vm_release(struct inode *inode, struct file *f)
{
	struct vhost_vm *vm = f->private_data;

	vhost_dev_stop(&vm->dev);
	vhost_dev_cleanup(&vm->dev);
	kfree(vm->dev.vqs);
	kfree(vm->evt_buf);
	kvfree(vm);

	return 0;
}

static int vhost_vm_set_features(struct vhost_vm *vm, u64 features)
{
	struct vhost_virtqueue *vq;
	int i;

	mutex_lock(&vm->dev.mutex);
	for (i = 0; i < VIRTIO_VM_Q_COUNT; i++) {
		vq = &vm->vq[i];
		mutex_lock(&vq->mutex);
		vq->acked_features = features;
		mutex_unlock(&vq->mutex);
	}
	mutex_unlock(&vm->dev.mutex);

	return 0;
}

static long vhost_vm_reset_owner(struct vhost_vm *vm)
{

	struct vhost_iotlb *umem;
	int err;

	mutex_lock(&vm->dev.mutex);
	err = vhost_dev_check_owner(&vm->dev);
	if (err)
		goto done;

	umem = vhost_dev_reset_owner_prepare();
	if (!umem) {
		err = -ENOMEM;
		goto done;
	}

	vhost_dev_reset_owner(&vm->dev, umem);
done:
	mutex_unlock(&vm->dev.mutex);
	return err;
}

static int vhost_vm_setup(struct vhost_vm *vm)
{
	int i;
	struct event_buffer_entry *evt;

	vm->evt_nr = vm->vq[VIRTIO_VM_Q_EVENT].num;

	vm->evt_buf = kmalloc(
		sizeof(struct event_buffer_entry) * vm->evt_nr, GFP_KERNEL);
	if (!vm->evt_buf)
		return -ENOMEM;

	for (i = 0; i < vm->evt_nr; i++) {
		evt = &vm->evt_buf[i];
		llist_add(&evt->llnode, &vm->evt_pool);
	}

	return 0;
}

static long vhost_vm_ioctl(struct file *f, unsigned int ioctl,
			    unsigned long arg)
{
	struct vhost_vm *vm = f->private_data;
	void __user *argp = (void __user *)arg;
	u64 __user *featurep = argp;
	u64 features;
	int ret;

	switch (ioctl) {
	case VHOST_GET_FEATURES:
		features = VHOST_VM_FEATURES;
		if (copy_to_user(featurep, &features, sizeof(features)))
			return -EFAULT;
		return 0;
	case VHOST_SET_FEATURES:
		if (copy_from_user(&features, featurep, sizeof(features)))
			return -EFAULT;
		if (features & ~VHOST_VM_FEATURES)
			return -EOPNOTSUPP;
		return vhost_vm_set_features(vm, features);
	case VHOST_RESET_OWNER:
		return vhost_vm_reset_owner(vm);
	default:
		mutex_lock(&vm->dev.mutex);
		ret = vhost_dev_ioctl(&vm->dev, ioctl, argp);
		if (ret == -ENOIOCTLCMD)
			ret = vhost_vring_ioctl(&vm->dev, ioctl, argp);
		if (!ret && ioctl == VHOST_SET_VRING_NUM)
			ret = vhost_vm_setup(vm);
		mutex_unlock(&vm->dev.mutex);
		return ret;
	}
}

static const struct file_operations vhost_vm_fops = {
	.owner          = THIS_MODULE,
	.open           = vhost_vm_open,
	.release        = vhost_vm_release,
	.llseek         = noop_llseek,
	.unlocked_ioctl = vhost_vm_ioctl,
};

static struct miscdevice vhost_vm_misc = {
	MISC_DYNAMIC_MINOR,
	"vhost-hello",
	&vhost_vm_fops,
};
module_misc_device(vhost_vm_misc);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Cross Domain Sched");
MODULE_AUTHOR("MediaTek Inc.");
