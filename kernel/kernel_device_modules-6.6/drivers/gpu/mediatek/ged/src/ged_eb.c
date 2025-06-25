// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#if defined(MTK_GPU_EB_SUPPORT)
#include <ged_notify_sw_vsync.h>
#include <gpueb_ipi.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#endif

#include "ged_eb.h"
#include "ged_base.h"
#include "ged_dvfs.h"
#include "ged_kpi.h"
#include "ged_global.h"
#include "ged_log.h"
#include "ged_tracepoint.h"
#include "ged_dcs.h"

#include <mt-plat/mtk_gpu_utility.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

#if defined(MTK_GPU_EB_SUPPORT)
static void __iomem *mtk_gpueb_dvfs_sysram_base_addr;

struct fdvfs_ipi_rcv_data fdvfs_ipi_rcv_msg;
static int g_fast_dvfs_ipi_channel = -1;

static int g_fdvfs_event_ipi_channel = -1;
uint32_t fdvfs_event_ipi_rcv_msg[4];

int g_is_fastdvfs_enable;
int g_is_fulltrace_enable;

int g_is_stability_enable;
static unsigned int eb_policy_mode;
static unsigned int sysram_size;
#endif

bool need_to_refresh_mode = true;
static struct hrtimer g_HT_fdvfs_debug;
#define GED_FDVFS_TIMER_TIMEOUT 1000000 // 1ms

#define DVFS_trace_counter(name, value) \
	trace_tracing_mark_write(5566, name, value)

static DEFINE_SPINLOCK(counter_info_lock);
static int mfg_is_power_on;
static unsigned int fb_uncomplete_cnt;

struct ged_last_timer {
	atomic_t last_soc_timer_hi;
	atomic_t last_soc_timer_lo;
	atomic_t last_ged_timer_hi;
	atomic_t last_ged_timer_lo;
};

static struct ged_last_timer g_ged_last_timer;

#if defined(MTK_GPU_EB_SUPPORT)
#define FDVFS_IPI_ATTR "ipi_dev:%p, ch:%d, DATA_LEN: %lu, TIMEOUT: %d(ms)"
#define EB_DVFS_FALLBACK 5566
#define EB_DVFS_DUMP_TH 0x800

static struct workqueue_struct *g_psEBWorkQueue;
static struct mutex gsEBLock;

#define MAX_EB_NOTIFY_CNT 120
struct GED_EB_EVENT eb_notify[MAX_EB_NOTIFY_CNT];
int eb_notify_index;

static struct work_struct sg_notify_ged_ready_work;
#define EB_CLOCK 26 //uinit:MHz

static void ged_eb_work_cb(struct work_struct *psWork)
{
	struct GED_EB_EVENT *psEBEvent =
		GED_CONTAINER_OF(psWork, struct GED_EB_EVENT, sWork);

	/* debug info */
	/*
	unsigned long eb_cur_loading;
	unsigned long eb_cur_virtual_freq;
	unsigned long eb_cur_oppidx;
	unsigned int loading_high;
	unsigned int loading_low;
	unsigned int eb_margin;
	unsigned int eb_policy_state;
	*/
	switch (psEBEvent->cmd) {
	case GPUFDVFS_IPI_EVENT_CLK_CHANGE:
		GPUFDVFS_LOGD("%s@%d top clock:%d(KHz)\n",
				__func__, __LINE__, psEBEvent->freq_new);
		/*check top freq is reasonable*/
		if (psEBEvent->freq_new < ged_get_top_freq_by_virt_opp(ged_get_min_oppidx_real()) - (EB_CLOCK*1000*2)){
			trace_tracing_mark_write(5566, "unreasonable_top_freq",psEBEvent->freq_new);
		} else {
			mtk_notify_gpu_freq_change(0, psEBEvent->freq_new);
			if (eb_policy_dts_flag && ged_get_cur_oppidx() < ged_get_min_stack_oppidx()
				&& dcs_get_cur_core_num() != dcs_get_max_core_num()) {
				mutex_lock(&gsPolicyLock);
				ged_kpi_fastdvfs_update_dcs();
				mutex_unlock(&gsPolicyLock);
			}
		}
		break;
	case GPUFDVFS_IPI_EVENT_DEBUG_MODE_ON:
		//eb_cur_loading = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_CUR_LOADING);
		//eb_cur_virtual_freq = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_CUR_VIRTUAL_FREQ);
		//eb_cur_oppidx = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_CUR_OPPIDX);
		//loading_high = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_MARGIN_HIGH);
		//loading_low = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_MARGIN_LOW);
		//eb_margin = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_MARGIN);
		//eb_policy_state = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_CUR_POLICY_STATE);

		//trace_tracing_mark_write(5566, "eb_loading", eb_cur_loading);
		//trace_tracing_mark_write(5566, "eb_virtual_freq", eb_cur_virtual_freq);
		//trace_tracing_mark_write(5566, "eb_cur_oppidx", eb_cur_oppidx);
		//trace_tracing_mark_write(5566, "eb_loading_high", loading_high);
		//trace_tracing_mark_write(5566, "eb_loading_low", loading_low);
		//trace_tracing_mark_write(5566, "eb_margin", eb_margin);
		//trace_tracing_mark_write(5566, "eb_policy_state", eb_policy_state);
		break;
	default:
		GPUFDVFS_LOGI("(%d), cmd: %d wrong!!!\n", __LINE__, psEBEvent->cmd);
		break;
	}
	psEBEvent->bUsed = false;
}

static void ged_eb_sysram_debug_data_write(void)
{
	int dbg_data[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int dbg_data2[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int dbg_data3[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int dbg_data4[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int dbg_data5[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int dbg_data6[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};

	u32 diff_data[GPUEB_SYSRAM_DVFS_DEBUG_COUNT] = {0};
	int i, dbg_cnt;
	u32 head = 0;
	u32 tail =
		mtk_gpueb_sysram_read(SYSRAM_GPU_EB_GPU_EB_DEBUG_WRITE_POINTER);
	u64 soc_timer = 0;
	u64 soc_timer_eb_hi = 0;
	u64 soc_timer_eb = 0;
	u32 time_diff = 0;
	u32 is_fb = 1;
	int tmp_head;

	if (tail >= GPUEB_SYSRAM_DVFS_DEBUG_COUNT)
		head = tail - GPUEB_SYSRAM_DVFS_DEBUG_COUNT;
	else
		head = GPUEB_SYSRAM_DVFS_DEBUG_BUF_SIZE -
			(GPUEB_SYSRAM_DVFS_DEBUG_COUNT -tail);


	soc_timer = mtk_gpueb_read_soc_timer();
	tmp_head = head;
	for (i = 0; i < GPUEB_SYSRAM_DVFS_DEBUG_COUNT; i++) {
		int cur_read_p = tmp_head * sizeof(u32);

		if (tmp_head != tail) {
			soc_timer_eb_hi = mtk_gpueb_sysram_read(
				SYSRAM_GPU_EB_LOG_DUMP_SOC_TIMER_HI + cur_read_p);
			soc_timer_eb = (u32) mtk_gpueb_sysram_read(
				SYSRAM_GPU_EB_LOG_DUMP_SOC_TIMER_LO + cur_read_p);
			soc_timer_eb |= (((u64)soc_timer_eb_hi) << 32);
			if (soc_timer > soc_timer_eb && soc_timer_eb != 0)
				time_diff = (soc_timer - soc_timer_eb) / 13;
			else
				time_diff = 0;
			diff_data[i] = time_diff;

			tmp_head++;
			if (tmp_head >= GPUEB_SYSRAM_DVFS_DEBUG_BUF_SIZE)
				tmp_head = 0;
		} else {
			diff_data[i] = 0;
		}
	}

	trace_GPU_DVFS__Policy__EBRB_TIME(diff_data);

	for (dbg_cnt = 0; dbg_cnt < EB_MAX_COUNT; dbg_cnt++) {
		tmp_head = head;
		for (i = 0; i < GPUEB_SYSRAM_DVFS_DEBUG_COUNT; i++) {
			int cur_read_p = tmp_head * sizeof(u32);
			if (tmp_head != tail) {
				switch (dbg_cnt) {
				case EB_FREQ:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_TOP_FREQ + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_STACK_FREQ + cur_read_p);
					break;
				case EB_LOADING:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_LOADING1 + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_LOADING2 + cur_read_p);
					break;
				case EB_ASYNC_COUNTER:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_GPU + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_ITER + cur_read_p);
					dbg_data3[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_COMPUTE + cur_read_p);
					dbg_data4[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_L2EXT + cur_read_p);
					dbg_data5[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_IRQ + cur_read_p);
					dbg_data6[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_MCU + cur_read_p);
					break;
				case EB_ASYNC_MCU_INDEX:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX1 + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX2 + cur_read_p);
					break;
				case EB_ASYNC_POLICY:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX2 + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX3 + cur_read_p);
					break;
				case EB_COMMIT_TYPE:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							gpueb_dbg_data[dbg_cnt].sysram_base + cur_read_p);
					if (dbg_data[i] != GED_DVFS_FRAME_BASE_COMMIT)
						is_fb = 0;
					break;
				case EB_DEBUG_COUNT:
				case EB_POWER_STATE:
				case EB_PRESERV:
				case EB_OPP:
				case EB_BOUND:
				case EB_MARGIN:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							gpueb_dbg_data[dbg_cnt].sysram_base + cur_read_p);
					break;
				case EB_AVG_LOADING:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_LOADING1 + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_DELTA_TIME + cur_read_p);
					break;
				case EB_FB_MONITOR:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_FB_MONITOR + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_FB_TARGET + cur_read_p);
					break;
				case EB_GPU_TIME:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_UN_TIME + cur_read_p);
					dbg_data2[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_COM_TIME + cur_read_p);
					dbg_data3[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_UN_TIME_TARGET + cur_read_p);
					dbg_data4[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_COM_TIME_TARGE + cur_read_p);
					dbg_data5[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_GPU_TIME + cur_read_p);
					dbg_data6[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_TIME_TARGET + cur_read_p);
					break;
				case EB_PREOC:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							SYSRAM_GPU_EB_LOG_DUMP_PREOC + cur_read_p);
					break;
				default:
					dbg_data[i] =
						mtk_gpueb_sysram_read(
							gpueb_dbg_data[dbg_cnt].sysram_base + cur_read_p);
					break;
				}

				tmp_head++;
				if (tmp_head >= GPUEB_SYSRAM_DVFS_DEBUG_BUF_SIZE)
					tmp_head = 0;
			} else {
				dbg_data[i] = 0;
			}
		}

		if (is_fb) {
			switch (dbg_cnt) {
			case EB_LOADING:
				trace_GPU_DVFS__EBRB_LOADING(dbg_data, dbg_data2);
				break;
			case EB_COMMIT_TYPE:
				trace_GPU_DVFS__EBRB_COMMON(dbg_data);
				break;
			case EB_POWER_STATE:
				trace_GPU_DVFS__EBRB_POWER_STATE(dbg_data);
				break;
			case EB_FB_MONITOR:
				trace_GPU_DVFS__EBRB_FB_MONITOR(dbg_data, dbg_data2);
				break;
			case EB_DEBUG_COUNT:
				trace_GPU_DVFS__EBRB_DEBUG(dbg_data);
				break;
			case EB_PREOC:
				trace_GPU_DVFS__EBRB_PREOC(dbg_data);
				break;
			default:
				trace_GPU_DVFS__Policy__EB_RINBUFFER(
					gpueb_dbg_data[dbg_cnt].name, dbg_data, diff_data);
				break;
			}
		} else {
			switch (dbg_cnt) {
			case EB_FREQ:
				trace_GPU_DVFS__EBRB_FREQ(dbg_data, dbg_data2);
				break;
			case EB_LOADING:
				trace_GPU_DVFS__EBRB_LOADING(dbg_data, dbg_data2);
				break;
			case EB_AVG_LOADING:
				trace_GPU_DVFS__EBRB_AVG_LOADING(dbg_data, dbg_data2);
				break;
			case EB_COMMIT_TYPE:
				trace_GPU_DVFS__EBRB_COMMON(dbg_data);
				break;
			case EB_POWER_STATE:
				trace_GPU_DVFS__EBRB_POWER_STATE(dbg_data);
				break;
			case EB_PRESERV:
				trace_GPU_DVFS__EBRB_PRESERVE(dbg_data);
				break;
			case EB_OPP:
				trace_GPU_DVFS__EBRB_OPP(dbg_data);
				break;
			case EB_BOUND:
				trace_GPU_DVFS__EBRB_BOUND(dbg_data);
				break;
			case EB_MARGIN:
				trace_GPU_DVFS__EBRB_MARGIN(dbg_data);
				break;
			case EB_FB_MONITOR:
				trace_GPU_DVFS__EBRB_FB_MONITOR(dbg_data, dbg_data2);
			break;
			case EB_GPU_TIME:
				trace_GPU_DVFS__EBRB_GPU_TIME(
					dbg_data, dbg_data2, dbg_data3, dbg_data4, dbg_data5, dbg_data6);
				break;
			case EB_ASYNC_COUNTER:
				trace_GPU_DVFS__EBRB_ASYNC_COUNTER(
					dbg_data, dbg_data2, dbg_data3, dbg_data4, dbg_data5, dbg_data6);
				break;
			case EB_ASYNC_MCU_INDEX:
				trace_GPU_DVFS__EBRB_ASYNC_MCU_INDEX(dbg_data, dbg_data2);
				break;
			case EB_ASYNC_POLICY:
				trace_GPU_DVFS__EBRB_ASYNC_POLICY(dbg_data, dbg_data2);
				break;
			case EB_DEBUG_COUNT:
				trace_GPU_DVFS__EBRB_DEBUG(dbg_data);
				break;
			case EB_PREOC:
				trace_GPU_DVFS__EBRB_PREOC(dbg_data);
				break;
			default:
				trace_GPU_DVFS__Policy__EB_RINBUFFER(
					gpueb_dbg_data[dbg_cnt].name, dbg_data, diff_data);
				break;
			}
		}

		mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GPU_EB_DEBUG_READ_POINTER, tmp_head);
	}

}

/*
 * handle events from EB
 * @param id    : ipi id
 * @param prdata: ipi handler parameter
 * @param data  : ipi data
 * @param len   : length of ipi data
 */
static int fast_dvfs_eb_event_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	struct GED_EB_EVENT *psEBEvent =
		&(eb_notify[((eb_notify_index++) % MAX_EB_NOTIFY_CNT)]);

	unsigned int cmd = 0;
	struct fastdvfs_event_data *event_data = NULL;

	if (eb_notify_index >= MAX_EB_NOTIFY_CNT)
		eb_notify_index = 0;

	if (data != NULL && psEBEvent && psEBEvent->bUsed == false) {
		event_data = (struct fastdvfs_event_data *) data;
		cmd = event_data->cmd;
		if (cmd == GPUFDVFS_IPI_EVENT_DEBUG_DATA) {
			if (event_data->u.set_para.arg[0] == EB_DVFS_FALLBACK) {
				ged_eb_dvfs_trace_dump();
				ged_set_backup_timer_timeout(ged_get_fallback_time());
				ged_cancel_backup_timer();
			}
			if (event_data->u.set_para.arg[1] == EB_DVFS_FALLBACK &&
				sysram_size >= EB_DVFS_DUMP_TH) {
				ged_eb_sysram_debug_data_write();
			}
			return 0;
		}

		psEBEvent->freq_new = ((struct fastdvfs_event_data *)data)->u.set_para.arg[0];

		/* irq cmd type (from gpueb) */
		psEBEvent->cmd = ((struct fastdvfs_event_data *)data)->cmd;

		/* get rate from EB */
		psEBEvent->bUsed = true;

		INIT_WORK(&psEBEvent->sWork, ged_eb_work_cb);
		queue_work(g_psEBWorkQueue, &psEBEvent->sWork);
	}

	return 0;
}

int ged_to_fdvfs_command(unsigned int cmd, struct fdvfs_ipi_data *ipi_data)
{
	int ret = 0;
	ktime_t cmd_start, cmd_now, cmd_duration;

	if (ipi_data != NULL &&
		g_fast_dvfs_ipi_channel >= 0 && g_fdvfs_event_ipi_channel >= 0) {
		ipi_data->cmd = cmd;
	} else {
		GPUFDVFS_LOGI("(%d), Can't send cmd(%d) ipi_data:%p, ch:(%d)(%d)\n",
			__LINE__, cmd, ipi_data,
			g_fast_dvfs_ipi_channel, g_fdvfs_event_ipi_channel);
		return -ENOENT;
	}

	GPUFDVFS_LOGD("(%d), send cmd: %d, msg[0]: %d\n",
		__LINE__,
		cmd,
		ipi_data->u.set_para.arg[0]);

	cmd_start = ktime_get();
	ipi_data->u.set_para.arg[3] = (cmd_start & 0xFFFFFFFF00000000) >> 32;
	ipi_data->u.set_para.arg[4] = (u32)(cmd_start & 0x00000000FFFFFFFF);


	switch (cmd) {
	// Set +
	case GPUFDVFS_IPI_SET_FRAME_DONE:
	case GPUFDVFS_IPI_SET_NEW_FREQ:
	case GPUFDVFS_IPI_SET_FRAME_BASE_DVFS:
	case GPUFDVFS_IPI_SET_TARGET_FRAME_TIME:
	case GPUFDVFS_IPI_SET_FEEDBACK_INFO:
	case GPUFDVFS_IPI_SET_MODE:
	case GPUFDVFS_IPI_SET_GED_READY:
	case GPUFDVFS_IPI_SET_DVFS_STRESS_TEST:
	case GPUFDVFS_IPI_SET_POWER_STATE:
	case GPUFDVFS_IPI_SET_DVFS_REINIT:
		ret = mtk_ipi_send_compl_to_gpueb(
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("(%d), cmd: %u, ret: %d, data: %p,"FDVFS_IPI_ATTR"\n",
				__LINE__, cmd, ret, ipi_data,
				get_gpueb_ipidev(),
				g_fast_dvfs_ipi_channel,
				FDVFS_IPI_DATA_LEN, FASTDVFS_IPI_TIMEOUT);
		} else {
			ret = fdvfs_ipi_rcv_msg.u.set_para.arg[0];
		}
	break;
	// Set -

	// Get +
	case GPUFDVFS_IPI_GET_BOUND:
	case GPUFDVFS_IPI_GET_MARGIM:
	case GPUFDVFS_IPI_GET_MODE:
	case GPUFDVFS_IPI_GET_FB_TUNE_PARAM:
	case GPUFDVFS_IPI_GET_LB_TUNE_PARAM:
		ret = mtk_ipi_send_compl_to_gpueb(
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		ipi_data->u.set_para.arg[0] = fdvfs_ipi_rcv_msg.u.set_para.arg[0];
		ipi_data->u.set_para.arg[1] = fdvfs_ipi_rcv_msg.u.set_para.arg[1];
		ipi_data->u.set_para.arg[2] = fdvfs_ipi_rcv_msg.u.set_para.arg[2];
		ipi_data->u.set_para.arg[3] = fdvfs_ipi_rcv_msg.u.set_para.arg[3];
		ipi_data->u.set_para.arg[4] = fdvfs_ipi_rcv_msg.u.set_para.arg[4];

	break;
	// Get

	case GPUFDVFS_IPI_PMU_START:
		ret = mtk_ipi_send_compl_to_gpueb(
			g_fast_dvfs_ipi_channel,
			IPI_SEND_POLLING, ipi_data,
			FDVFS_IPI_DATA_LEN,
			FASTDVFS_IPI_TIMEOUT);

		if (ret != 0) {
			GPUFDVFS_LOGI("(%d), cmd: %d, mtk_ipi_send_compl, ret: %d\n",
				__LINE__, cmd, ret);
		}
	break;

	default:
		GPUFDVFS_LOGI("(%d), cmd: %d wrong!!!\n",
			__LINE__, cmd);
	break;
	}

	cmd_now = ktime_get();
	cmd_duration = ktime_sub(cmd_now,
		(((u64)((u64)(fdvfs_ipi_rcv_msg.u.set_para.arg[3]) << 32) & 0xFFFFFFFF00000000) +
		fdvfs_ipi_rcv_msg.u.set_para.arg[4]));

	GPUFDVFS_LOGD("(%d), cmd: %d, ack cmd: %d, msg[0]: %d. IPI duration: %llu ns(%llu ns)\n",
		__LINE__,
		cmd,
		fdvfs_ipi_rcv_msg.cmd,
		fdvfs_ipi_rcv_msg.u.set_para.arg[0],
		ktime_to_ns(cmd_duration), ktime_to_ns(ktime_sub(cmd_now, cmd_start)));

	return ret;
}

void mtk_gpueb_dvfs_commit(unsigned long ulNewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	int ret = 0;

	static unsigned long ulPreFreqID = -1;

	if (ulNewFreqID != ulPreFreqID) {
#ifdef FDVFS_REDUCE_IPI
/*
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_PLATFORM_FREQ_IDX,
			ulNewFreqID);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_TYPE,
			eCommitType);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_VIRTUAL_FREQ,
			0);
*/
#else
		struct fdvfs_ipi_data ipi_data;

		ipi_data.u.set_para.arg[0] = ulNewFreqID;
		ipi_data.u.set_para.arg[1] = eCommitType;
		ipi_data.u.set_para.arg[2] = 0;
		ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);
#endif
		if (pbCommited) {
			if (ret == 0)
				*pbCommited = true;
			else
				*pbCommited = false;
		}
	} else {
		if (pbCommited)
			*pbCommited = true;
	}

	ulPreFreqID = ulNewFreqID;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_commit);

void mtk_gpueb_dvfs_dcs_commit(unsigned int platform_freq_idx,
		GED_DVFS_COMMIT_TYPE eCommitType,
		unsigned int virtual_freq_in_MHz)
{
	static unsigned int pre_platform_freq_idx = -1;
	static unsigned int pre_virtual_freq_in_MHz = -1;

	if (platform_freq_idx != pre_platform_freq_idx ||
		virtual_freq_in_MHz != pre_virtual_freq_in_MHz) {
#ifdef FDVFS_REDUCE_IPI
/*
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_PLATFORM_FREQ_IDX,
			platform_freq_idx);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_TYPE,
			eCommitType);
		mtk_gpueb_sysram_write(SYSRAM_GPU_COMMIT_VIRTUAL_FREQ,
			virtual_freq_in_MHz);
*/
#else
		struct fdvfs_ipi_data ipi_data;

		ipi_data.u.set_para.arg[0] = platform_freq_idx;
		ipi_data.u.set_para.arg[1] = eCommitType;
		ipi_data.u.set_para.arg[2] = virtual_freq_in_MHz;
		ipi_data.u.set_para.arg[3] = 0xFFFFFFFF;

		ged_to_fdvfs_command(GPUFDVFS_IPI_SET_NEW_FREQ, &ipi_data);
#endif
	}

	pre_platform_freq_idx = platform_freq_idx;
	pre_virtual_freq_in_MHz = virtual_freq_in_MHz;

}
EXPORT_SYMBOL(mtk_gpueb_dvfs_dcs_commit);

int mtk_gpueb_dvfs_set_frame_done(void)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAME_DONE, &ipi_data);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_done);

/*
unsigned int mtk_gpueb_dvfs_set_feedback_info(int frag_done_interval_in_ns,
	struct GpuUtilization_Ex util_ex, unsigned int curr_fps)
{
	int ret = 0;
	unsigned int utils = 0;

#ifdef FDVFS_REDUCE_IPI
	utils = ((util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24));

	mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_GPU_UTILS, utils);

	if (frag_done_interval_in_ns > 0)
		mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_GPU_TIME,
			frag_done_interval_in_ns);

	if (curr_fps > 0)
		mtk_gpueb_sysram_write(SYSRAM_GPU_FEEDBACK_INFO_CURR_FPS,
			curr_fps);
	ret = mtk_gpueb_sysram_read(SYSRAM_GPU_TA_3D_COEF);
#else
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = (unsigned int)frag_done_interval_in_ns;
	ipi_data.u.set_para.arg[1] =
		(util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24);

	ipi_data.u.set_para.arg[2] = (unsigned int)curr_fps;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FEEDBACK_INFO,
		&ipi_data);
#endif

	return (ret > 0) ? ret:0xFFFFFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_feedback_info);
*/

unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;
	static unsigned int is_fb_dvfs_enabled;

	if (enable != is_fb_dvfs_enabled) {
		ipi_data.u.set_para.arg[0] = (enable && g_is_fastdvfs_enable);
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_FRAME_BASE_DVFS, &ipi_data);
	}

	is_fb_dvfs_enabled = enable;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_base_dvfs);

int mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time,
	unsigned int target_margin)
{
	int ret = 0;
	static unsigned int pre_target_frame_time;
	static unsigned int pre_target_margin;

	if (g_fastdvfs_margin)
		target_margin = 999;

	if (target_frame_time != pre_target_frame_time ||
			target_margin != pre_target_margin) {
#ifdef FDVFS_REDUCE_IPI
/*
		mtk_gpueb_sysram_write(SYSRAM_GPU_SET_TARGET_FRAME_TIME,
			target_frame_time);
		mtk_gpueb_sysram_write(SYSRAM_GPU_SET_TARGET_MARGIN,
			target_margin);
*/
#else
		struct fdvfs_ipi_data ipi_data;

		ipi_data.u.set_para.arg[0] = target_frame_time;
		ipi_data.u.set_para.arg[1] = target_margin;

		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_TARGET_FRAME_TIME, &ipi_data);
#endif
	}

	pre_target_frame_time = target_frame_time;
	pre_target_margin = target_margin;

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_taget_frame_time);

unsigned int mtk_gpueb_set_fallback_mode(int fallback_status)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	if (g_is_fastdvfs_enable != fallback_status) {
		ipi_data.u.set_para.arg[0] = fallback_status;
		ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_MODE, &ipi_data);
		g_is_fastdvfs_enable = fallback_status;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_set_fallback_mode);

unsigned int mtk_gpueb_set_stability_mode(int stability_status)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = stability_status;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_DVFS_STRESS_TEST, &ipi_data);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_set_stability_mode);

void mtk_gpueb_dvfs_get_desire_freq(unsigned long *ui32NewFreqID)
{
	if (ui32NewFreqID == NULL) {
		GED_LOGE("%s: ui32NewFreqID is NULL", __func__);
		return;
	}
	/* read desire oppidx from sysram */
	*ui32NewFreqID = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_DESIRE_FREQ_ID);
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_desire_freq);

void mtk_gpueb_dvfs_get_desire_freq_dual(unsigned long *stackNewFreqID,
	unsigned long *topNewFreqID)
{
	if (stackNewFreqID == NULL || topNewFreqID == NULL) {
		GED_LOGE("%s: Stack or Top NewFreqID is NULL", __func__);
		return;
	}
	/* read desire oppidx from sysram */
	*stackNewFreqID = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_DESIRE_FREQ_ID);
	*topNewFreqID = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_DESIRE_FREQ_ID);
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_desire_freq_dual);

unsigned int mtk_gpueb_dvfs_set_mode(unsigned int action)
{
	int ret = 0;
	eb_policy_mode = action;

	/* mpdify fallback interval timer */
	if (eb_policy_mode)
		g_fallback_mode = ALIGN_FAST_DVFS;
	else
		g_fallback_mode = ALIGN_INTERVAL;

	/* modify loading base interval timer */
	if (eb_policy_mode)
		ged_kpi_set_loading_mode(LB_TIMEOUT_TYPE_REDUCE_MIPS,
					g_loading_stride_size);
	else
		ged_kpi_set_loading_mode(0,
					g_loading_stride_size);

	ret = mtk_gpueb_set_stability_mode(eb_policy_mode);

	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_mode);


void mtk_gpueb_dvfs_get_mode(struct fdvfs_ipi_data *ipi_data)
{
	int ret = 0;
	unsigned int cmd = ipi_data->cmd;

	ret = ged_to_fdvfs_command(cmd, ipi_data);

	if (ret)
		GED_LOGD("%s err:%d\n", __func__, ret);
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_mode);

int mtk_gpueb_power_modle_cmd(unsigned int enable)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = enable;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_PMU_START, &ipi_data);
	return ret;
}
EXPORT_SYMBOL(mtk_gpueb_power_modle_cmd);

int mtk_set_ged_ready(int ged_ready_flag)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = ged_ready_flag;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_GED_READY, &ipi_data);
	return ret;
}

void mtk_gpueb_set_power_state(enum ged_gpu_power_state power_state)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = power_state;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_POWER_STATE, &ipi_data);

	if (ret)
		GED_LOGD("%s err:%d\n", __func__, ret);
}
EXPORT_SYMBOL(mtk_gpueb_set_power_state);

unsigned int is_fdvfs_enable(void)
{
	return eb_policy_mode;
}


unsigned int mtk_gpueb_dvfs_get_cur_freq(void)
{
	int ret = 0;
/*	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_GET_CURR_FREQ, &ipi_data);
*/

	return (ret > 0) ? ret:0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_cur_freq);

unsigned int mtk_gpueb_dvfs_get_frame_loading(void)
{
	int ret = 0;
/*	struct fdvfs_ipi_data ipi_data;

	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_GET_FRAME_LOADING, &ipi_data);
*/

	return (ret > 0) ? ret:0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_frame_loading);

static void mtk_gpueb_dvfs_reinit(unsigned int type)
{
	int ret = 0;
	struct fdvfs_ipi_data ipi_data;

	ipi_data.u.set_para.arg[0] = type;
	ret = ged_to_fdvfs_command(GPUFDVFS_IPI_SET_DVFS_REINIT, &ipi_data);

	if (ret)
		GED_LOGD("%s err:%d\n", __func__, ret);
}

int mtk_gpueb_sysram_batch_read(int max_read_count,
	char *batch_string, int batch_str_size)
{
	if (g_is_fulltrace_enable == 0) {
		int index_freq = 0;
		int curr_str_len = 0;
		int read_freq = 0;
		int avg_freq;
		int value_cnt = 0;
		int frequency1, frequency2;


		if (!mtk_gpueb_dvfs_sysram_base_addr
			|| max_read_count <= 0
			|| batch_string == NULL)
			return 0;

		curr_str_len = 0;
		avg_freq = 0;
		for (index_freq = 0 ; index_freq < max_read_count &&
			curr_str_len <= batch_str_size; index_freq++) {
			//read_freq = (__raw_readl(mtk_gpueb_dvfs_sysram_base_addr +
				//SYSRAM_GPU_CURR_FREQ + (index_freq<<2)));

			frequency1 = ((read_freq>>16)&0xffff);
			if (frequency1 > 0) {
				if (frequency1 <= ged_get_max_freq_in_opp()) {
					value_cnt++;
					avg_freq += frequency1;
					curr_str_len += scnprintf(batch_string + curr_str_len,
						batch_str_size, "|%d", frequency1);
				}

				frequency2 = ((read_freq)&0x0000ffff);
				if (frequency2 > 0) {
					if (frequency2 <= ged_get_max_freq_in_opp()) {
						value_cnt++;
						avg_freq += frequency2;
						curr_str_len +=
							scnprintf(batch_string + curr_str_len,
							batch_str_size, "|%d", frequency2);
					}
				} else {
					// Reset the rest data
					for (; index_freq < max_read_count; index_freq++)
						/* Reset the rest unread data */
						//__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
							//SYSRAM_GPU_CURR_FREQ + (index_freq<<2));
					break;
				}
			} else {
				GPUFDVFS_LOGD("batch_string: %s, index_freq: %d\n",
					batch_string, index_freq);

				// Reset the rest data
				for (; index_freq < max_read_count; index_freq++)
					/* Reset the rest unread data */
					//__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
						//SYSRAM_GPU_CURR_FREQ + (index_freq<<2));

				break;
			}

			GPUFDVFS_LOGD("read_freq: 0x%x, frequency1: %d, frequency2: %d\n",
				read_freq, frequency1, frequency2);

			/* Reset the read data */
			//__raw_writel(0, mtk_gpueb_dvfs_sysram_base_addr +
			//SYSRAM_GPU_CURR_FREQ + (index_freq<<2));
		}

		if (value_cnt > 0)
			avg_freq /= value_cnt;
		else {
			avg_freq = ged_get_cur_freq()/1000;
			curr_str_len += scnprintf(batch_string + curr_str_len,
					batch_str_size, "|%d", avg_freq);
		}

		return avg_freq;

	} else {
		GPUFDVFS_LOGD("%s(). cur_freq: %d", __func__, (ged_get_cur_freq() / 1000));
		return (ged_get_cur_freq() / 1000);
	}

	return -1;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_batch_read);

int mtk_gpueb_sysram_read(int offset)
{
	if (!mtk_gpueb_dvfs_sysram_base_addr)
		return -1;

	if ((offset % 4) != 0)
		return -1;

	return (int)(__raw_readl(mtk_gpueb_dvfs_sysram_base_addr + offset));
}
EXPORT_SYMBOL(mtk_gpueb_sysram_read);

int mtk_gpueb_sysram_write(int offset, int val)
{
	if (!mtk_gpueb_dvfs_sysram_base_addr)
		return -EADDRNOTAVAIL;

	if ((offset % 4) != 0)
		return -EINVAL;

	__raw_writel(val, mtk_gpueb_dvfs_sysram_base_addr + offset);
	mb(); /* make sure register access in order */

	if (val != mtk_gpueb_sysram_read(offset))
		GPUFDVFS_LOGD("%s(). failed to update sysram. value : %0x, val: %d/%d",
		__func__, offset, val,
		mtk_gpueb_sysram_read(offset));

	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_write);

u64 mtk_gpueb_read_soc_timer(void)
{
	u64 soc_timer = (u32)atomic_read(&g_ged_last_timer.last_soc_timer_lo);
	u32 soc_timer_hi = (u32)atomic_read(&g_ged_last_timer.last_soc_timer_hi);
	u64 ged_timer = (u32)atomic_read(&g_ged_last_timer.last_ged_timer_lo);
	u32 ged_timer_hi = (u32)atomic_read(&g_ged_last_timer.last_ged_timer_hi);

	soc_timer |= (((u64)soc_timer_hi) << 32);
	ged_timer |= (((u64)ged_timer_hi) << 32);

	//mtk_get_system_timer(&soc_timer);
	if (soc_timer != 0 && ged_timer != 0) {
		// use cpu time to calculate soc timer if gpu off
		u64 cur_ged_timer = ged_get_time();
		u64 timer_dur = 0;

		if (cur_ged_timer > ged_timer) {
			timer_dur = cur_ged_timer - ged_timer;
			timer_dur = (timer_dur * 13) / 1000;
		} else {
			timer_dur = 0;
		}

		soc_timer = soc_timer + timer_dur;
		//trace_tracing_mark_write(5566, "soc_timer_gpu_off", soc_timer);
	}

	return soc_timer;
}

void mtk_gpueb_record_soc_timer(u64 soc_timer)
{
	if (soc_timer != 0 && is_fdvfs_enable()) {
		u64 ged_timer = ged_get_time();
		u32 ged_timer_hi = (u32)(ged_timer >> 32);
		u32 ged_timer_lo = (u32)(ged_timer & 0xFFFFFFFF);
		u32 soc_timer_hi = (u32)(soc_timer >> 32);
		u32 soc_timer_lo = (u32)(soc_timer & 0xFFFFFFFF);

		atomic_set(&g_ged_last_timer.last_soc_timer_hi, soc_timer_hi);
		atomic_set(&g_ged_last_timer.last_soc_timer_lo, soc_timer_lo);
		atomic_set(&g_ged_last_timer.last_ged_timer_hi, ged_timer_hi);
		atomic_set(&g_ged_last_timer.last_ged_timer_lo, ged_timer_lo);
		//trace_tracing_mark_write(5566, "soc_timer_update", soc_timer);
	}
}

static void ged_eb_dvfs_udpate_gpu_time(void)
{
	struct ged_risky_bq_info info_kpi;
	struct ged_sysram_info info_sysram;
	GED_ERROR ret_bq_state;
	int t_gpu_complete_eb = 0;
	int t_gpu_target_eb_complete = 0;
	unsigned int gpu_completed_counteb = 0;

	// get tgpu uncompleted time in loading-based
	ged_kpi_update_sysram_uncompleted_tgpu(&info_sysram);
	info_sysram.last_tgpu_uncompleted /= 1000;
	info_sysram.last_tgpu_uncompleted_target /= 1000;

	// get frame info in loading-based
	ret_bq_state = ged_kpi_eb_dvfs_get_time(&info_kpi);

	// update frame info in loading-based
	if (ret_bq_state == GED_OK) {
		t_gpu_complete_eb = (int) info_kpi.completed_bq.t_gpu;
		t_gpu_target_eb_complete = info_kpi.completed_bq.t_gpu_target;
		gpu_completed_counteb = info_kpi.total_gpu_completed_count;
	} else {
		//no complete time when bq_state is not ready
		t_gpu_complete_eb = 0;
		t_gpu_target_eb_complete = 0;
		gpu_completed_counteb = 0;
	}
	/* update info in sysram */
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_BQ_STATE, ret_bq_state);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_COMPLETE_TIME, t_gpu_complete_eb);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_COMPLETE_TARGET_TIME, t_gpu_target_eb_complete);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_COMPLETE_COUNT, gpu_completed_counteb);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TIME, info_sysram.last_tgpu_uncompleted);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TARGET_TIME, info_sysram.last_tgpu_uncompleted_target);
#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	if (ged_get_policy_state() == POLICY_STATE_LB) {
		if (t_gpu_target_eb_complete > 0 && info_sysram.last_tgpu_uncompleted_target > 0)
			ged_get_gpu_frame_time(umin(t_gpu_target_eb_complete,
				info_sysram.last_tgpu_uncompleted_target) * 1000);
		else
			ged_get_gpu_frame_time(umax(t_gpu_target_eb_complete,
				info_sysram.last_tgpu_uncompleted_target) * 1000);
	}
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */
	if (info_sysram.uncompleted_count > 0) {
		mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI,
				(u32)(info_sysram.last_uncomplete_soc_timer >> 32));
		mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO,
				(u32)(info_sysram.last_uncomplete_soc_timer & 0xFFFFFFFF));
		trace_GPU_DVFS__Policy__Common__SOC_Timer_LB(SOC_DONE,
			(u32)(info_sysram.last_uncomplete_soc_timer >> 32),
			(u32)(info_sysram.last_uncomplete_soc_timer & 0xFFFFFFFF));

	} else {
		mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI, 0);
		mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO, 0);
		trace_GPU_DVFS__Policy__Common__SOC_Timer_LB(SOC_RESET, 0, 0);
	}
	mtk_gpueb_sysram_write(SYSRAM_GPU_EB_UNCOMPLETE_COUNT, info_sysram.uncompleted_count);

}

static void ged_eb_dvfs_udpate_gpu_uncomplete_time(void)
{
	struct ged_sysram_info info_sysram;

	// get tgpu uncompleted time in loading-based
	ged_kpi_update_sysram_uncompleted_tgpu(&info_sysram);


	info_sysram.last_tgpu_uncompleted /= 1000;
	info_sysram.last_tgpu_uncompleted_target /= 1000;

	/* update info in sysram */
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TIME, info_sysram.last_tgpu_uncompleted);
	mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TARGET_TIME, info_sysram.last_tgpu_uncompleted_target);
	mtk_gpueb_sysram_write(SYSRAM_GPU_EB_UNCOMPLETE_COUNT, info_sysram.uncompleted_count);

}

int ged_eb_dvfs_task(enum ged_eb_dvfs_task_index index, int value)
{
	u64 soc_timer = 0;
	int tmp = 0;

	if (is_fdvfs_enable()) {
		switch (index) {
		case EB_UPDATE_UNCOMPLETE_COUNT:
			if (ged_get_policy_state() == POLICY_STATE_FB)
				mtk_gpueb_sysram_write(SYSRAM_GPU_EB_UNCOMPLETE_COUNT, value);
			break;
		case EB_UPDATE_POLICY_STATE:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_DESIRE_POLICY_STATE, value);
			break;
		case EB_COMMIT_DCS:
			/* In GPU Job Boundary, ged will check & commit gpueb desire oppidx */
			if (ged_get_policy_state() != POLICY_STATE_FB)
				ged_kpi_fastdvfs_update_dcs();
			break;
		case EB_UPDATE_GPU_TIME_INFO:
			ged_eb_dvfs_udpate_gpu_time();
			break;
		case EB_UPDATE_UNCOMPLETE_GPU_TIME:
			if (ged_get_policy_state() == POLICY_STATE_LB)
				ged_eb_dvfs_udpate_gpu_uncomplete_time();
			break;
		case EB_UPDATE_FB_TARGET_TIME:
			mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_HD, value);
			update_fb_timer_set_count();
			soc_timer = mtk_gpueb_read_soc_timer();
			fb_uncomplete_cnt++;
			mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_SOC_TIMER_HI,
					(u32)(soc_timer >> 32));
			mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_SOC_TIMER_LO,
					(u32)(soc_timer & 0xFFFFFFFF));
			trace_GPU_DVFS__Policy__Common__SOC_Timer_FB(SOC_FB,
				(u32)(soc_timer >> 32),
				(u32)(soc_timer & 0xFFFFFFFF));
			break;
		case EB_UPDATE_FB_TARGET_TIME_DONE:
			mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_HD, value);
			update_fb_timer_set_count();
			if (fb_uncomplete_cnt == 1) {
				mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_SOC_TIMER_HI,0);
				mtk_gpueb_sysram_write(SYSRAM_GPU_FB_TARGET_SOC_TIMER_LO,0);
				trace_GPU_DVFS__Policy__Common__SOC_Timer_FB(SOC_RESET, 0, 0);
			}
			fb_uncomplete_cnt = 0;
			break;
		case EB_SET_FTRACE:
			if (value == 1) {
				mtk_set_fastdvfs_mode(POLICY_MODE | POLICY_DEBUG_FTRACE);
				ged_set_backup_timer_timeout(ged_get_fallback_time());
				ged_cancel_backup_timer();
			} else if (value == 0) {
				mtk_set_fastdvfs_mode(POLICY_MODE);
			}
			break;
		case EB_COMMIT_LAST_KERNEL_OPP:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GED_KERNEL_COMMIT_OPP, value);
			soc_timer = mtk_gpueb_read_soc_timer();
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_HI,
							(u32)(soc_timer >> 32));
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_LO,
							(u32)(soc_timer & 0xFFFFFFFF));
			break;
		case EB_UPDATE_PRESERVE:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GED_PRESERVE, value);
		break;
		case EB_DCS_ENABLE:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_DCS_ENABLE, value);
		break;
		case EB_DCS_CORE_NUM:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_DCS_CORE_NUM, value);
		break;
		case EB_ASYNC_RATIO_ENABLE:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_ASYNC_RATIO_ENABLE, value);
		break;
		case EB_ASYNC_PARAM:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_ASYNC_PARAM, value);
		break;
		case EB_UPDATE_API_BOOST:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_API_BOOST, value);
		break;
		case EB_REINIT:
			mtk_gpueb_dvfs_reinit(value);
		break;
		case EB_UPDATE_SMALL_FRAME:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_SMALL_FRAME, value);
		break;
		case EB_UPDATE_STABLE_LB:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_STABLE_LB, value);
		break;
		case EB_UPDATE_DESIRE_FREQ_ID:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_DESIRE_FREQ_ID, value);
		break;
		case EB_UPDATE_LAST_COMMIT_IDX:
			mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, value);
		break;
		case EB_UPDATE_LAST_COMMIT_TOP_IDX:
			mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_TOP_IDX, value);
		break;
		case EB_DBG_CMD:
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_FALLBACK_INTERVAL,
				g_fallback_mode * 100 + g_fallback_time);
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_FALLBACK_WIN_SIZE,
				g_fallback_window_size);
			mtk_get_loading_base_dvfs_step(&tmp);
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_LB_DVFS_STEP, tmp);
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_LOADING_STRIDE_SIZE,
				g_loading_target_mode * 100 + g_loading_stride_size);
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_LOADING_WIN_SIZE,
				ged_dvfs_get_lb_win_size_cmd());
			// g_tb_dvfs_margin_value [0:7]
			mtk_get_timer_base_dvfs_margin(&tmp);
			// g_tb_dvfs_margin_mode [8:16]
			tmp |= ged_dvfs_get_tb_dvfs_margin_mode();
			// g_tb_dvfs_margin_value_min_cmd [16:23]
			tmp |= ((ged_dvfs_get_margin_value_min_cmd() & 0xFF) << 16);
			// g_tb_dvfs_margin_step [24:27]
			tmp |= ((ged_dvfs_get_margin_step() & 0xF) << 24);
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_CMD_TB_DVFS_MARGIN, tmp);
		break;
		default:
			GPUFDVFS_LOGI("(%d), no cmd: %d\n", __LINE__, index);
			break;
		}
	}
	return GED_OK;
}


static int fastdvfs_proc_show(struct seq_file *m, void *v)
{
/*	char show_string[256];
	unsigned int ui32FastDVFSMode = 0;

	if (g_is_fulltrace_enable == 1) {
		seq_puts(m, "\n#### Frequency ####\n");

		scnprintf(show_string, 256, "Current gpu freq       : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_CURR_FREQ));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Predicted gpu freq     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_FREQ));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Workload ####\n");
		scnprintf(show_string, 256, "Predicted workload     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_WORKLOAD));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Left workload          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_WL));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Finish workload        : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_FINISHED_WORKLOAD));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Under Hint WL          : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_WL));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Time Budget ####\n");
		scnprintf(show_string, 256, "Target time            : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_TIME));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "Left time              : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_TIME));
		seq_puts(m, show_string);

		seq_puts(m, "\n#### Interval ####\n");
		scnprintf(show_string, 256, "Kernel Frame Done Interval     : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL));
		seq_puts(m, show_string);

		scnprintf(show_string, 256, "EB Frame Done Interval         : %d\n",
			mtk_gpueb_sysram_read(SYSRAM_GPU_EB_FRAME_DONE_INTERVAL));
		seq_puts(m, show_string);
		seq_puts(m, "\n\n\n");

	}
*/

	return 0;
}

static ssize_t
fastdvfs_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *f_pos)
{
	char desc[32];
	int len = 0;
	//int gpu_freq = 0;
	int action = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	// bit 0 : enable/disable FastDVFS
	// bit 1 : Full systrace
	if (!kstrtoint(desc, 10, &action)) {
		g_is_fastdvfs_enable =
			((action & (0x1 << ACTION_MAP_FASTDVFS)))>>ACTION_MAP_FASTDVFS;
		g_is_fulltrace_enable =
			((action & (0x1 << ACTION_MAP_FULLTRACE)))>>ACTION_MAP_FULLTRACE;

		mtk_gpueb_dvfs_set_mode(action);
	}

	if (g_is_fulltrace_enable == 1) {
		if (hrtimer_try_to_cancel(&g_HT_fdvfs_debug)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_fdvfs_debug);
			hrtimer_start(&g_HT_fdvfs_debug,
				ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		} else {
			/*
			 * Timer is not existed
			 */
			hrtimer_start(&g_HT_fdvfs_debug,
				ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		}
	} else {
		if (hrtimer_try_to_cancel(&g_HT_fdvfs_debug)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_fdvfs_debug);
		}
	}

	return count;
}

static int fastdvfs_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, fastdvfs_proc_show, NULL);
}

static const struct proc_ops fastdvfs_proc_fops = {
	.proc_open = fastdvfs_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = fastdvfs_proc_write,
};

void ged_fastdvfs_systrace(void)
{
/*
	DVFS_trace_counter("(FDVFS)Curr GPU freq",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_CURR_FREQ));
	DVFS_trace_counter("(FDVFS)Pred WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_WORKLOAD));
	DVFS_trace_counter("(FDVFS)Finish WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FINISHED_WORKLOAD));
	DVFS_trace_counter("(FDVFS)EB_FRAME_INTL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_EB_FRAME_DONE_INTERVAL));
	DVFS_trace_counter("(FDVFS)GED_FRAME_INTL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL));
	DVFS_trace_counter("(FDVFS)TARGET TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_TIME));
	DVFS_trace_counter("(FDVFS)3D loading",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FRAGMENT_LOADING));
	DVFS_trace_counter("(FDVFS)Frame End Hint Cnt",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_FRAME_END_HINT_CNT));
	DVFS_trace_counter("(FDVFS)Pred GPU freq",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_PRED_FREQ));
	DVFS_trace_counter("(FDVFS)FRAME BOUNDARY",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_TARGET_FRAME_BOUNDARY));
	DVFS_trace_counter("(FDVFS)LEFT WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_WL));
	DVFS_trace_counter("(FDVFS)ELAPSED TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_ELAPSED_TIME));
	DVFS_trace_counter("(FDVFS)UNDER HINT WL",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_WL));
	DVFS_trace_counter("(FDVFS)LEFT TIME",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_LEFT_TIME));
	DVFS_trace_counter("(FDVFS)UNDER HINT CNT",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_UNDER_HINT_CNT));
	DVFS_trace_counter("(FDVFS)JS0 DELTA",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_JS0_DELTA));
	DVFS_trace_counter("(FDVFS)COMMIT PROFILE",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_COMMIT_PROFILE));
	DVFS_trace_counter("(FDVFS)DCS",
		(long long)mtk_gpueb_sysram_read(SYSRAM_GPU_DCS));
*/
}

enum hrtimer_restart ged_fdvfs_debug_cb(struct hrtimer *timer)
{
	if (g_is_fulltrace_enable == 1) {
		ged_fastdvfs_systrace();
		hrtimer_start(&g_HT_fdvfs_debug,
			ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
	}

	return HRTIMER_NORESTART;
}

static void __iomem *_gpu_fastdvfs_of_ioremap(const char *node_name)
{
	struct device_node *node = NULL;
	void __iomem *mapped_addr = NULL;
	struct resource res;
	int rc = 0;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node) {
		mapped_addr = of_iomap(node, 0);
		GPUFDVFS_LOGI("@%d mapped_addr: %p\n", __LINE__, mapped_addr);
		rc = of_address_to_resource(node, 0, &res);
		if (rc)
			GPUFDVFS_LOGI("Cannot get physical memory addr");
		sysram_size = resource_size(&res);

		of_node_put(node);
	} else
		GPUFDVFS_LOGE("#@# %s:(%s::%d) Cannot find [%s] of_node\n", "FDVFS",
			__FILE__, __LINE__, node_name);

	return mapped_addr;
}

static void gpu_power_change_notify_fdvfs(int power_on)
{
	/* Do something if necessary */
	spin_lock(&counter_info_lock);

	GPUFDVFS_LOGD("%s() power on: %d\n", __func__, power_on);
	mfg_is_power_on = power_on;

	spin_unlock(&counter_info_lock);
}

static void mtk_set_ged_ready_handler(struct work_struct *work)
{
	static int retry_count;
	int ret = 0;

	do {
		retry_count += 1;
		ret = mtk_set_ged_ready(1);
		GPUFDVFS_LOGI("(attempt %d) mtk_set_ged_ready return %d", retry_count,
			ret);
	} while (ret != 0);
}

void fdvfs_init(void)
{
	g_is_fastdvfs_enable = 1;
	g_is_fulltrace_enable = 0;

	g_fast_dvfs_ipi_channel = -1;
	g_fdvfs_event_ipi_channel = -1;

	/* init ipi channel */
	if (g_fast_dvfs_ipi_channel < 0) {
		g_fast_dvfs_ipi_channel =
			gpueb_get_send_PIN_ID_by_name("IPI_ID_FAST_DVFS");
		if (unlikely(g_fast_dvfs_ipi_channel <= 0)) {
			GPUFDVFS_LOGE("fail to get fast dvfs IPI channel id (ENOENT)");
			g_is_fastdvfs_enable = -1;

			return;
		}

		mtk_ipi_register(get_gpueb_ipidev(), g_fast_dvfs_ipi_channel,
			NULL, NULL, (void *)&fdvfs_ipi_rcv_msg);
	}

	if (g_fdvfs_event_ipi_channel < 0) {
		g_fdvfs_event_ipi_channel =
			gpueb_get_recv_PIN_ID_by_name("IPI_ID_FAST_DVFS_EVENT");
		if (unlikely(g_fdvfs_event_ipi_channel < 0)) {
			GPUFDVFS_LOGE("fail to get FDVFS EVENT IPI channel id (ENOENT)");

			return;
		}

		mtk_ipi_register(get_gpueb_ipidev(), g_fdvfs_event_ipi_channel,
				(void *)fast_dvfs_eb_event_handler, NULL, &fdvfs_event_ipi_rcv_msg);

		g_psEBWorkQueue =
			alloc_ordered_workqueue("ged_eb",
				WQ_FREEZABLE | WQ_MEM_RECLAIM);
	}

	GPUFDVFS_LOGI("succeed to register channel: (%d)(%d), ipi_size: %lu\n",
		g_fast_dvfs_ipi_channel,
		g_fdvfs_event_ipi_channel,
		FDVFS_IPI_DATA_LEN);


	/* init sysram for debug */
	mtk_gpueb_dvfs_sysram_base_addr =
		_gpu_fastdvfs_of_ioremap("mediatek,gpu_fdvfs");

	if (mtk_gpueb_dvfs_sysram_base_addr == NULL) {
		GPUFDVFS_LOGE("can't find fdvfs sysram");
		return;
	}

	mtk_register_gpu_power_change("fdvfs", gpu_power_change_notify_fdvfs);

	hrtimer_init(&g_HT_fdvfs_debug, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_fdvfs_debug.function = ged_fdvfs_debug_cb;

	if (g_is_fulltrace_enable == 1)
		hrtimer_start(&g_HT_fdvfs_debug,
			ns_to_ktime(GED_FDVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);

	mutex_init(&gsEBLock);
}

void fdvfs_exit(void)
{
	destroy_workqueue(g_psEBWorkQueue);
	mtk_unregister_gpu_power_change("fdvfs");
	mutex_destroy(&gsEBLock);
}

int fastdvfs_proc_init(void)
{
	proc_create("fastdvfs_proc", 0660, NULL, &fastdvfs_proc_fops);

	return 0;
}

void fastdvfs_proc_exit(void)
{
	remove_proc_entry("fastdvfs_proc", NULL);
}

void ged_notify_eb_ged_ready(void)
{
	// send ready message to GPUEB so top clock can now be handled
	if (g_ged_gpu_freq_notify_support) {
		INIT_WORK(&sg_notify_ged_ready_work, mtk_set_ged_ready_handler);
		schedule_work(&sg_notify_ged_ready_work);
	}
}
#else
void mtk_gpueb_dvfs_commit(unsigned long ulNewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_commit);

void mtk_gpueb_dvfs_dcs_commit(unsigned int platform_freq_idx,
		GED_DVFS_COMMIT_TYPE eCommitType,
		unsigned int virtual_freq_in_MHz)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_dcs_commit);

int mtk_gpueb_dvfs_set_frame_done(void)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_done);

unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_frame_base_dvfs);

int mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time,
	unsigned int target_margin)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_taget_frame_time);

unsigned int mtk_gpueb_set_fallback_mode(int fallback_status)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_set_fallback_mode);

unsigned int mtk_gpueb_set_stability_mode(int stability_status)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_set_stability_mode);

void mtk_gpueb_dvfs_get_desire_freq(unsigned long *ui32NewFreqID)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_desire_freq);

void mtk_gpueb_dvfs_get_desire_freq_dual(unsigned long *stackNewFreqID,
	unsigned long *topNewFreqID)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_desire_freq_dual);

unsigned int mtk_gpueb_dvfs_set_mode(unsigned int action)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_set_mode);

void mtk_gpueb_dvfs_get_mode(struct fdvfs_ipi_data *ipi_data)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_mode);

int mtk_gpueb_power_modle_cmd(unsigned int enable)
{
	//Do nothing
	return 0;
}
EXPORT_SYMBOL(mtk_gpueb_power_modle_cmd);

int mtk_set_ged_ready(int ged_ready_flag)
{
	//Do nothing
	return 0;
}

void mtk_gpueb_set_power_state(enum ged_gpu_power_state power_state)
{
	//Do nothing
}
EXPORT_SYMBOL(mtk_gpueb_set_power_state);

unsigned int is_fdvfs_enable(void)
{
	//Do nothing
	return 0;
}

unsigned int mtk_gpueb_dvfs_get_cur_freq(void)
{
	//Do nothing
	return 0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_cur_freq);

unsigned int mtk_gpueb_dvfs_get_frame_loading(void)
{
	//Do nothing
	return 0xFFFF;
}
EXPORT_SYMBOL(mtk_gpueb_dvfs_get_frame_loading);

int mtk_gpueb_sysram_batch_read(int max_read_count,
	char *batch_string, int batch_str_size)
{
	//Do nothing
	return -1;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_batch_read);

int mtk_gpueb_sysram_read(int offset)
{
	//Do nothing
	return -1;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_read);

int mtk_gpueb_sysram_write(int offset, int val)
{
	//Do nothing
	return -1;
}
EXPORT_SYMBOL(mtk_gpueb_sysram_write);

u64 mtk_gpueb_read_soc_timer(void)
{
	//Do nothing
	return 0;
}

void mtk_gpueb_record_soc_timer(u64 soc_timer)
{
	//Do nothing
}

int ged_eb_dvfs_task(enum ged_eb_dvfs_task_index index, int value)
{
	//Do nothing
	return GED_ERROR_FAIL;
}

void fdvfs_init(void)
{
	//Do nothing
}

void fdvfs_exit(void)
{
	//Do nothing
}

int fastdvfs_proc_init(void)
{
	//Do nothing
	return 0;
}

void fastdvfs_proc_exit(void)
{
	//Do nothing
}

void ged_notify_eb_ged_ready(void)
{
	//Do nothing
}
#endif
