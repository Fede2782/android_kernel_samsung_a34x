// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/regulator/consumer.h>
#include <mtk_low_battery_throttling.h>
#include <mtk_battery_oc_throttling.h>
#include <mtk_peak_power_budget_mbrain.h>
#include <mtk-mmdvfs-debug.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_v6899_power.h"

#include <scp_mbrain_dbg.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SPMI_MTK_PMIF)
#include <spmi-mtk.h>
#define MAX_SPMI_SLVID slvid_cnt
#else
#define MAX_SPMI_SLVID 32
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_REGULATOR_RT6160)
#include <rt6160.h>
#endif


#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

#include <swpm_module_psp.h>

#include "mbraink_sys_res_mbrain_dbg.h"
#include "mbraink_sys_res_mbrain_plat.h"
#include "mbraink_sys_res_plat.h"

unsigned char *g_spm_raw;
unsigned int g_data_size;
int g_spm_l2_sig_tbl_size;
unsigned int g_spm_l2_sig_num;
unsigned char *g_spm_l2_ls_ptr = NULL, *g_spm_l2_sig_tbl_ptr = NULL;

#endif

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#include "mtk_ccci_common.h"

unsigned int g_md_last_has_data_blk_idx;
unsigned int g_md_last_read_blk_idx;
unsigned int g_md_read_count;
#endif

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE)
#include <lpm_dbg_common_v2.h>
#endif

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE)
void lpm_get_suspend_event_info(struct lpm_dbg_lp_info *info);
#endif

static int mbraink_v6899_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo)
{
	int ret = 0;
	int i = 0;
	int32_t core_vol_num = 0, core_ip_num = 0;
	struct ip_stats *core_ip_stats_ptr = NULL;
	struct vol_duration *core_duration_ptr = NULL;
	int32_t core_vol_check = 0, core_ip_check = 0;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr = kcalloc(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	if (core_duration_ptr == NULL) {
		ret = -1;
		goto End;
	}

	core_ip_stats_ptr = kcalloc(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	if (core_ip_stats_ptr == NULL) {
		ret = -1;
		goto End;
	}

	for (i = 0; i < core_ip_num; i++) {
		core_ip_stats_ptr[i].times =
		kzalloc(sizeof(struct ip_times), GFP_KERNEL);
		if (core_ip_stats_ptr[i].times == NULL) {
			pr_notice("times failure\n");
			goto End;
		}
	}

	sync_latest_data();

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num, core_ip_stats_ptr);

	if (core_vol_num > MAX_VCORE_NUM) {
		pr_notice("core vol num over (%d)", MAX_VCORE_NUM);
		core_vol_check = MAX_VCORE_NUM;
	} else
		core_vol_check = core_vol_num;

	if (core_ip_num > MAX_VCORE_IP_NUM) {
		pr_notice("core_ip_num over (%d)", MAX_VCORE_IP_NUM);
		core_ip_check = MAX_VCORE_IP_NUM;
	} else
		core_ip_check = core_ip_num;

	pmbrainkPowerVcoreInfo->totalVCNum = core_vol_check;
	pmbrainkPowerVcoreInfo->totalVCIpNum = core_ip_check;

	for (i = 0; i < core_vol_check; i++) {
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].vol =
			core_duration_ptr[i].vol;
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].duration =
			core_duration_ptr[i].duration;
	}

	for (i = 0; i < core_ip_check; i++) {
		strscpy(pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].ip_name,
			core_ip_stats_ptr[i].ip_name,
			MAX_IP_NAME_LENGTH - 1);
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.active_time =
			core_ip_stats_ptr[i].times->active_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.idle_time =
			core_ip_stats_ptr[i].times->idle_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.off_time =
			core_ip_stats_ptr[i].times->off_time;
	}

End:
	if (core_duration_ptr != NULL)
		kfree(core_duration_ptr);

	if (core_ip_stats_ptr != NULL) {
		for (i = 0; i < core_ip_num; i++) {
			if (core_ip_stats_ptr[i].times != NULL)
				kfree(core_ip_stats_ptr[i].times);
		}
		kfree(core_ip_stats_ptr);
	}

	return ret;
}

static void mbraink_v6899_get_power_wakeup_info(
	struct mbraink_power_wakeup_data *wakeup_info_buffer)
{
	struct wakeup_source *ws = NULL;
	ktime_t total_time;
	ktime_t max_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;
	int lockid = 0;
	unsigned short i = 0;
	unsigned short pos = 0;
	unsigned short count = 5000;

	if (wakeup_info_buffer == NULL)
		return;

	lockid = wakeup_sources_read_lock();

	ws = wakeup_sources_walk_start();
	pos = wakeup_info_buffer->next_pos;
	while (ws && (pos > i) && (count > 0)) {
		ws = wakeup_sources_walk_next(ws);
		i++;
		count--;
	}

	for (i = 0; i < MAX_WAKEUP_SOURCE_NUM; i++) {
		if (ws != NULL) {
			wakeup_info_buffer->next_pos++;

			total_time = ws->total_time;
			max_time = ws->max_time;
			prevent_sleep_time = ws->prevent_sleep_time;
			active_count = ws->active_count;
			if (ws->active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, ws->last_time);
				total_time = ktime_add(total_time, active_time);
				if (active_time > max_time)
					max_time = active_time;

				if (ws->autosleep_enabled) {
					prevent_sleep_time = ktime_add(prevent_sleep_time,
						ktime_sub(now, ws->start_prevent_time));
				}
			} else {
				active_time = 0;
			}

			if (ws->name != NULL) {
				if ((strlen(ws->name) > 0) && (strlen(ws->name) < MAX_NAME_SZ)) {
					memcpy(wakeup_info_buffer->drv_data[i].name,
					ws->name, strlen(ws->name));
				}
			}

			wakeup_info_buffer->drv_data[i].active_count = active_count;
			wakeup_info_buffer->drv_data[i].event_count = ws->event_count;
			wakeup_info_buffer->drv_data[i].wakeup_count = ws->wakeup_count;
			wakeup_info_buffer->drv_data[i].expire_count = ws->expire_count;
			wakeup_info_buffer->drv_data[i].active_time = ktime_to_ms(active_time);
			wakeup_info_buffer->drv_data[i].total_time = ktime_to_ms(total_time);
			wakeup_info_buffer->drv_data[i].max_time = ktime_to_ms(max_time);
			wakeup_info_buffer->drv_data[i].last_time = ktime_to_ms(ws->last_time);
			wakeup_info_buffer->drv_data[i].prevent_sleep_time = ktime_to_ms(
				prevent_sleep_time);

			ws = wakeup_sources_walk_next(ws);
		}
	}
	wakeup_sources_read_unlock(lockid);

	if (ws != NULL)
		wakeup_info_buffer->is_has_data = 1;
	else
		wakeup_info_buffer->is_has_data = 0;
}

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
static int mbraink_v6899_power_get_spm_l1_info(long long *out_spm_l1_array, int spm_l1_size)
{
	int ret = -1;
	int i = 0;
	long long value = 0;
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	unsigned char *spm_l1_data = NULL;
	unsigned int data_size = 0;
	int size = 0;

	if (out_spm_l1_array == NULL)
		return -1;

	if (spm_l1_size != SPM_L1_DATA_NUM)
		return -1;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();
	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_last_suspend_res_data) {

		data_size = SPM_L1_SZ;
		spm_l1_data = kmalloc(data_size, GFP_KERNEL);
		if (spm_l1_data != NULL) {
			memset(spm_l1_data, 0, data_size);
			if (sys_res_mbrain_ops->get_last_suspend_res_data(spm_l1_data,
					data_size) == 0) {
				size = sizeof(value);
				for (i = 0; i < SPM_L1_DATA_NUM; i++) {
					if ((i*size + size) <= SPM_L1_SZ)
						memcpy(&value, (const void *)(spm_l1_data
							+ i*size), size);
					out_spm_l1_array[i] = value;
				}
				ret = 0;
			}
		}
	}

	if (spm_l1_data != NULL) {
		kfree(spm_l1_data);
		spm_l1_data = NULL;
	}

	return ret;
}

static int mbraink_v6899_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	uint32_t thr[4];
	unsigned int wpos = 0, wsize = 0;
	unsigned int get_sig = 0;
	bool bfree = false;
	int res_size = (int)sizeof(struct mbraink_sys_res_sig_info);

	if (spm_l2_info == NULL)
		return 0;

	pr_notice("mbraink spm get_sig(%d)\n", get_sig);

	sys_res_mbrain_ops = get_mbraink_dbg_ops();
	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_over_threshold_num &&
		sys_res_mbrain_ops->get_over_threshold_data) {

		thr[0] = spm_l2_info->value[0];
		thr[1] = spm_l2_info->value[1];
		thr[2] = spm_l2_info->value[2];
		thr[3] = spm_l2_info->value[3];

		memcpy(&get_sig, spm_l2_info->spm_data + 28, sizeof(get_sig));
		if (get_sig == 0) {
			if (g_spm_l2_ls_ptr != NULL) {
				kfree(g_spm_l2_ls_ptr);
				g_spm_l2_ls_ptr = NULL;
			}

			if (g_spm_l2_sig_tbl_ptr != NULL) {
				kfree(g_spm_l2_sig_tbl_ptr);
				g_spm_l2_sig_tbl_ptr = NULL;
			}

			g_spm_l2_ls_ptr = kmalloc(SPM_L2_LS_SZ, GFP_KERNEL);
			if (g_spm_l2_ls_ptr == NULL) {
				bfree = true;
				goto End;
			}

			if (sys_res_mbrain_ops->get_over_threshold_num(g_spm_l2_ls_ptr,
				SPM_L2_LS_SZ, thr, 4) == 0) {
				memcpy(&g_spm_l2_sig_num, g_spm_l2_ls_ptr + 24, sizeof(g_spm_l2_sig_num));

				if (g_spm_l2_sig_num > SPM_TOTAL_MAX_SIG_NUM) {
					bfree = true;
					goto End;
				}

				g_spm_l2_sig_tbl_size = g_spm_l2_sig_num*res_size;
				g_spm_l2_sig_tbl_ptr = kmalloc(g_spm_l2_sig_tbl_size, GFP_KERNEL);
				if (g_spm_l2_sig_tbl_ptr == NULL) {
					bfree = true;
					goto End;
				}

				if (sys_res_mbrain_ops->get_over_threshold_data(g_spm_l2_sig_tbl_ptr,
					g_spm_l2_sig_tbl_size) != 0) {
					bfree = true;
					goto End;
				}
			}
		}

		if (g_spm_l2_ls_ptr != NULL && g_spm_l2_sig_tbl_ptr != NULL) {
			memcpy(spm_l2_info->spm_data, g_spm_l2_ls_ptr, SPM_L2_LS_SZ);
			wpos += SPM_L2_LS_SZ;

			if ((wpos + (g_spm_l2_sig_num - get_sig)*res_size) <= SPM_L2_SZ) {
				wsize = (g_spm_l2_sig_num - get_sig)*res_size;
				memcpy(spm_l2_info->spm_data + wpos,
					g_spm_l2_sig_tbl_ptr + get_sig*res_size, wsize);
			} else {
				wsize = (sizeof(spm_l2_info->spm_data) - SPM_L2_LS_SZ)
					/res_size*res_size;
				memcpy(spm_l2_info->spm_data + SPM_L2_LS_SZ, g_spm_l2_sig_tbl_ptr
					+ get_sig*res_size, wsize);
			}
			get_sig += wsize/res_size;
			memcpy(spm_l2_info->spm_data + 28, &get_sig, sizeof(get_sig));
		}
	}
End:

	if (get_sig == g_spm_l2_sig_num || bfree) {
		if (g_spm_l2_ls_ptr != NULL) {
			kfree(g_spm_l2_ls_ptr);
			g_spm_l2_ls_ptr = NULL;
		}

		if (g_spm_l2_sig_tbl_ptr != NULL) {
			kfree(g_spm_l2_sig_tbl_ptr);
			g_spm_l2_sig_tbl_ptr = NULL;
		}
	}

	return 0;
}

static int mbraink_v6899_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	bool bfree = false;
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	int bufIdx = 0;
	int size = 0;

	if (spm_buffer == NULL) {
		bfree = true;
		goto End;
	}

	if (spm_buffer->type == 1) {
		sys_res_mbrain_ops = get_mbraink_dbg_ops();

		if (sys_res_mbrain_ops && sys_res_mbrain_ops->get_length) {
			g_data_size = sys_res_mbrain_ops->get_length();
			g_data_size += MAX_POWER_HD_SZ;
			pr_notice("g_data_size(%d)\n", g_data_size);
		}

		if (g_data_size == 0) {
			bfree = true;
			goto End;
		}

		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}

		if (g_data_size <= SPM_TOTAL_MAX_SIG_NUM*sizeof(struct mbraink_sys_res_sig_info)
			*MBRAINK_SCENE_RELEASE_NUM) {
			g_spm_raw = vmalloc(g_data_size);
			if (g_spm_raw != NULL) {
				memset(g_spm_raw, 0, g_data_size);

				if (sys_res_mbrain_ops && sys_res_mbrain_ops->get_data &&
					sys_res_mbrain_ops->get_data(g_spm_raw, g_data_size) != 0) {
					bfree = true;
					goto End;
				}
			}
		}
	}

	if (g_spm_raw != NULL) {
		if (spm_buffer->type == 1) {
			size = sizeof(struct mbraink_sys_res_mbrain_header);
			bufIdx = 0;
			if ((bufIdx + size) <= g_data_size) {
				memcpy(spm_buffer->spm_data + bufIdx, g_spm_raw, size);
				bufIdx += size;
			}

			size = sizeof(struct mbraink_sys_res_scene_info)*MBRAINK_SCENE_RELEASE_NUM;
			if ((bufIdx + size) <= g_data_size) {
				memcpy(spm_buffer->spm_data + bufIdx, g_spm_raw + bufIdx, size);
				bufIdx += size;
			}
		} else {
			if (((spm_buffer->pos + spm_buffer->size) > (g_data_size)) ||
			spm_buffer->size > sizeof(spm_buffer->spm_data)) {
				bfree = true;
				goto End;
			}
			memcpy(spm_buffer->spm_data, g_spm_raw + spm_buffer->pos, spm_buffer->size);

			if (spm_buffer->type == 0) {
				bfree = true;
				goto End;
			}
		}
	}

End:
	if (bfree == true) {
		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}
		g_data_size = 0;
	}

	return 0;
}
#else

static int mbraink_v6899_power_get_spm_l1_info(long long *out_spm_l1_array, int spm_l1_size)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

static int mbraink_v6899_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

static int mbraink_v6899_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	pr_info("[Mbraink][SPM][%s] SWPM not support\n", __func__);
}

#endif

static int mbraink_v6899_power_get_scp_info(struct mbraink_power_scp_info *scp_info)
{
	struct scp_res_mbrain_dbg_ops *scp_res_mbrain_ops = NULL;
	unsigned int data_size = 0;
	unsigned char *ptr = NULL;

	if (scp_info == NULL)
		return -1;

	scp_res_mbrain_ops = get_scp_mbrain_dbg_ops();

	if (scp_res_mbrain_ops &&
		scp_res_mbrain_ops->get_length &&
		scp_res_mbrain_ops->get_data) {

		data_size = scp_res_mbrain_ops->get_length();

		if (data_size > 0 && data_size <= sizeof(scp_info->scp_data)) {
			ptr = kmalloc(data_size, GFP_KERNEL);
			if (ptr == NULL)
				goto End;

			scp_res_mbrain_ops->get_data(ptr, data_size);
			if (data_size <= sizeof(scp_info->scp_data))
				memcpy(scp_info->scp_data, ptr, data_size);

		} else {
			goto End;
		}
	}

End:
	if (ptr != NULL) {
		kfree(ptr);
		ptr = NULL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static int mbraink_v6899_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	int shm_size = 0;
	void __iomem *shm_addr = NULL;
	unsigned char *base_addr = NULL;
	unsigned char *read_addr = NULL;
	int i = 0;
	unsigned int mem_status = 0;
	unsigned int read_blk_idx = 0;
	unsigned int offset = 0;
	bool ret = true;

	if (modem_buffer == NULL)
		return 0;

	shm_addr = get_smem_start_addr(SMEM_USER_32K_LOW_POWER, &shm_size);
	if (shm_addr == NULL) {
		pr_notice("get_smem_start_addr addr is null\n");
		return 0;
	}

	if (shm_size == 0 || MD_MAX_SZ > shm_size) {
		pr_notice("get_smem_start_addr size(%d) is incorrect\n", shm_size);
		return 0;
	}

	base_addr = (unsigned char *)shm_addr;

	if (modem_buffer->type == 0) {
		read_addr = base_addr;
		memcpy(modem_buffer->data1, read_addr, MD_HD_SZ);
		read_addr = base_addr + MD_HD_SZ;
		memcpy(modem_buffer->data2, read_addr, MD_MDHD_SZ);

		if (modem_buffer->data1[0] != 1 ||  modem_buffer->data1[2] != 8) {
			modem_buffer->is_has_data = 0;
			modem_buffer->count = 0;
			return 0;
		}

		g_md_read_count = 0;
		g_md_last_read_blk_idx = g_md_last_has_data_blk_idx;

		pr_notice("g_md_last_read_blk_idx(%d)", g_md_last_read_blk_idx);
	}

	read_blk_idx = g_md_last_read_blk_idx;
	i = 0;
	ret = true;
	while ((g_md_read_count < MD_BLK_MAX_NUM) && (i < MD_SECBLK_NUM)) {
		offset = MD_HD_SZ + MD_MDHD_SZ + read_blk_idx*MD_BLK_SZ;
		if (offset > shm_size) {
			ret = false;
			break;
		}
		read_addr = base_addr + offset;
		memcpy(&mem_status, read_addr, sizeof(mem_status));

		read_blk_idx = (read_blk_idx + 1) % MD_BLK_MAX_NUM;
		g_md_read_count++;

		if (mem_status == MD_STATUS_W_DONE) {
			offset = i*MD_BLK_SZ;
			if ((offset + MD_BLK_SZ) > sizeof(modem_buffer->data3)) {
				ret = false;
				break;
			}

			memcpy(modem_buffer->data3 + offset, read_addr, MD_BLK_SZ);
			//reset mem_status mem_count after read data
			mem_status = MD_STATUS_R_DONE;
			memcpy(read_addr, &mem_status, sizeof(mem_status));
			memset(read_addr + sizeof(mem_status), 0, 4); //mem_count
			i++;
			g_md_last_has_data_blk_idx = read_blk_idx;
		}
	}

	if ((g_md_read_count < MD_BLK_MAX_NUM) && (ret == true))
		modem_buffer->is_has_data = 1;
	else
		modem_buffer->is_has_data = 0;

	g_md_last_read_blk_idx = read_blk_idx;
	modem_buffer->count = i;

	return 0;
}

#else

static int mbraink_v6899_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	pr_notice("not support eccci modem interface\n");
	return 0;
}

#endif

static void mbraink_v6899_power_get_voting_info(
	struct mbraink_voting_struct_data *mbraink_vcorefs_src)
{
	unsigned int *mbraink_voting_ret = NULL;
	int idx = 0;
	int voting_num = 0;

	mbraink_voting_ret = vcorefs_get_src_req();
	voting_num = vcorefs_get_src_req_num();

	if (voting_num > MAX_STRUCT_SZ)
		voting_num = MAX_STRUCT_SZ;

	memset(mbraink_vcorefs_src, 0, sizeof(struct mbraink_voting_struct_data));

	if (mbraink_voting_ret) {
		mbraink_vcorefs_src->voting_num = voting_num;
		for (idx = 0; idx < voting_num; idx++) {
			mbraink_vcorefs_src->mbraink_voting_data[idx] =
				mbraink_voting_ret[idx];
		}
	} else {
		mbraink_vcorefs_src->voting_num = -1;
		pr_info("vcore voting system is not support on kernel space !\n");
	}
}

static int mbraink_v6899_power_get_spmi_info(
	struct mbraink_spmi_struct_data *mbraink_spmi_data)
{
	unsigned int Buf[MAX_SPMI_SLVID] = {0};
	int ret = 0;
	int num = 0;

	if (mbraink_spmi_data == NULL) {
		pr_info("mbraink_spmi_data is null\n");
		return -1;
	}

	get_spmi_slvid_nack_cnt(Buf);
	num = (MAX_PMIC_SPMI_SZ > MAX_SPMI_SLVID) ? MAX_SPMI_SLVID : MAX_PMIC_SPMI_SZ;
	memcpy(mbraink_spmi_data->spmi, Buf, sizeof(unsigned int)*num);
	mbraink_spmi_data->spmi_count = num;

	return ret;
}

static int mbraink_v6899_power_get_uvlo_info(
	struct mbraink_uvlo_struct_data *mbraink_uvlo_data)
{
	int num = 0;
	int i = 0;
	int ret = 0;
	struct rt6160_error rt6160_err;

	if (mbraink_uvlo_data == NULL) {
		pr_info("mbraink_uvlo_data is null\n");
		return -1;
	}

	num = rt6160_get_chip_num();
	num = (num > MAX_PMIC_UVLO_SZ) ? MAX_PMIC_UVLO_SZ : num;

	mbraink_uvlo_data->uvlo_count = num;
	for (i = 0; i < num; i++) {
		rt6160_get_error_cnt(i, &rt6160_err);
		mbraink_uvlo_data->uvlo_err_data[i].ot = rt6160_err.ot;
		mbraink_uvlo_data->uvlo_err_data[i].uv = rt6160_err.uv;
		mbraink_uvlo_data->uvlo_err_data[i].oc = rt6160_err.oc;
	}

	return ret;
}

static int mbraink_v6899_power_get_pmic_voltage_info(
	struct mbraink_pmic_voltage_info *pmicVoltageInfo)
{
	unsigned int vcore = 0;
	unsigned int vsram_core = 0;
	int ret = 0;
	int err = 0;
	struct regulator *reg_vcore;
	struct regulator *reg_vsram_core;

	reg_vcore = regulator_get(NULL, "mt6363_vbuck2");
	if (IS_ERR(reg_vcore)) {
		err = PTR_ERR(reg_vcore);
		pr_notice("Failed to get 'mt6363_vbuck2' regulator: %d\n", err);
	} else {
		ret = regulator_get_voltage(reg_vcore);
		if (ret > 0) {
			vcore = ret;
			pr_info("get mt6363_vbuck2, vcore: %d", vcore);
		} else {
			pr_info("failed to get mt6363_vbuck2, vcore: %d", vcore);
		}
		regulator_put(reg_vcore);
	}

	reg_vsram_core = regulator_get(NULL, "mt6363_vbuck4");
	if (IS_ERR(reg_vsram_core)) {
		err = PTR_ERR(reg_vsram_core);
		pr_notice("Failed to get 'mt6363_vbuck4' regulator: %d\n", err);
	} else {
		ret = regulator_get_voltage(reg_vsram_core);
		if (ret > 0) {
			vsram_core = ret;
			pr_info("get mt6363_vbuck4, vsram_core: %d", vsram_core);
		} else {
			pr_info("failed to get mt6363_vbuck4, vsram_core: %d", vsram_core);
		}
		regulator_put(reg_vsram_core);
	}

	pmicVoltageInfo->vcore = vcore;
	pmicVoltageInfo->vsram_core = vsram_core;

	return 0;
}

static int mbraink_v6899_power_sys_res_init(void)
{
	mbraink_sys_res_plat_init();
	mbraink_sys_res_mbrain_plat_init();

	return 0;
}

static int mbraink_v6899_power_sys_res_deinit(void)
{
	mbraink_sys_res_plat_deinit();
	mbraink_sys_res_mbrain_plat_deinit();

	return 0;
}

static void mbraink_v6899_power_suspend_prepare(void)
{
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();

	if (sys_res_mbrain_ops && sys_res_mbrain_ops->update) {
		if (sys_res_mbrain_ops->update() != 0)
			pr_info("suspend_prepare mbraink update sys res fail");
	}
}

static void mbraink_v6899_power_post_suspend(void)
{
	struct mbraink_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;

	sys_res_mbrain_ops = get_mbraink_dbg_ops();

	if (sys_res_mbrain_ops && sys_res_mbrain_ops->update) {
		if (sys_res_mbrain_ops->update() != 0)
			pr_info("post_suspend mbraink update sys res fail");
	}
}

static int mbraink_v6899_power_get_mmdfvs_info(struct mbraink_mmdvfs_info *mmdvfsInfo)
{
	unsigned int mmdvfs_data_size = 0;
	int ret = 0;
	struct mmdvfs_res_mbrain_debug_ops *res_mbrain_debug_ops = NULL;

	if (mmdvfsInfo == NULL)
		return -1;

	res_mbrain_debug_ops = get_mmdvfs_mbrain_dbg_ops();

	if (res_mbrain_debug_ops &&
		res_mbrain_debug_ops->get_length &&
		res_mbrain_debug_ops->get_data) {

		mmdvfs_data_size = res_mbrain_debug_ops->get_length();
		if (mmdvfs_data_size <= MMDVFS_TOTAL_SZ) {
			mmdvfsInfo->size = mmdvfs_data_size;
			ret = res_mbrain_debug_ops->get_data(mmdvfsInfo->mmdvfs_data, mmdvfsInfo->size);
			pr_info("mmdvfs opp, size: %d", mmdvfsInfo->size);
		} else {
			pr_info("incorrect size: %d for mmdvfs opp", mmdvfsInfo->size);
			ret = -1;
		}
	} else {
		pr_info("failed to get mmdvfs opp");
		ret = -1;
	}

	return ret;
}

void pt2mbrain_hint_low_battery_volt_throttle(struct lbat_mbrain lbat_mbrain)
{
	int user = lbat_mbrain.user;
	unsigned int thd_volt = lbat_mbrain.thd_volt;
	unsigned int level = lbat_mbrain.level;
	unsigned int soc = lbat_mbrain.soc;
	unsigned int bat_temp = lbat_mbrain.bat_temp;
	unsigned int temp_stage = lbat_mbrain.temp_stage;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d:%d:%d:%d:%d:%d",
			NETLINK_EVENT_LOW_BATTERY_VOLTAGE_THROTTLE,
			timestamp,
			user,
			thd_volt,
			level,
			soc,
			bat_temp,
			temp_stage);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

void pt2mbrain_hint_battery_oc_throttle(struct battery_oc_mbrain bat_oc_mbrain)
{
	unsigned int level = bat_oc_mbrain.level;

	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d",
			NETLINK_EVENT_BATTERY_OVER_CURRENT_THROTTLE,
			timestamp,
			level);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

void pt2mbrain_ppb_notify_func(void)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s",
			NETLINK_EVENT_PPB_NOTIFY);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

static int mbraink_v6899_power_get_power_throttle_hw_info(struct mbraink_power_throttle_hw_data *power_throttle_hw_data)
{
	int ret = 0;
	struct ppb_mbrain_data *res_ppb_mbrain_data = NULL;

	res_ppb_mbrain_data = kzalloc(sizeof(struct ppb_mbrain_data), GFP_KERNEL);
	if (!res_ppb_mbrain_data) {
		ret = -ENOMEM;
		goto End;
	}

	memset(res_ppb_mbrain_data, 0x00, sizeof(struct ppb_mbrain_data));
	ret = get_ppb_mbrain_data(res_ppb_mbrain_data);

	power_throttle_hw_data->kernel_time = res_ppb_mbrain_data->kernel_time;
	power_throttle_hw_data->duration = res_ppb_mbrain_data->duration;
	power_throttle_hw_data->soc = res_ppb_mbrain_data->soc;
	power_throttle_hw_data->temp = res_ppb_mbrain_data->temp;
	power_throttle_hw_data->soc_rdc = res_ppb_mbrain_data->soc_rdc;
	power_throttle_hw_data->soc_rac = res_ppb_mbrain_data->soc_rac;
	power_throttle_hw_data->hpt_bat_budget = res_ppb_mbrain_data->hpt_bat_budget;
	power_throttle_hw_data->hpt_cg_budget = res_ppb_mbrain_data->hpt_cg_budget;
	power_throttle_hw_data->ppb_cg_budget = res_ppb_mbrain_data->ppb_cg_budget;
	power_throttle_hw_data->hpt_cpub_thr_cnt = res_ppb_mbrain_data->hpt_cpub_thr_cnt;
	power_throttle_hw_data->hpt_cpub_thr_time = res_ppb_mbrain_data->hpt_cpub_thr_time;
	power_throttle_hw_data->hpt_cpum_thr_cnt = res_ppb_mbrain_data->hpt_cpum_thr_cnt;
	power_throttle_hw_data->hpt_cpum_thr_time = res_ppb_mbrain_data->hpt_cpum_thr_time;
	power_throttle_hw_data->hpt_gpu_thr_cnt = res_ppb_mbrain_data->hpt_gpu_thr_cnt;
	power_throttle_hw_data->hpt_gpu_thr_time = res_ppb_mbrain_data->hpt_gpu_thr_time;
	power_throttle_hw_data->hpt_cpub_sf = res_ppb_mbrain_data->hpt_cpub_sf;
	power_throttle_hw_data->hpt_cpum_sf = res_ppb_mbrain_data->hpt_cpum_sf;
	power_throttle_hw_data->hpt_gpu_sf = res_ppb_mbrain_data->hpt_gpu_sf;
	power_throttle_hw_data->ppb_combo = res_ppb_mbrain_data->ppb_combo;
	power_throttle_hw_data->ppb_c_combo0 = res_ppb_mbrain_data->ppb_c_combo0;
	power_throttle_hw_data->ppb_g_combo0 = res_ppb_mbrain_data->ppb_g_combo0;
	power_throttle_hw_data->ppb_g_flavor = res_ppb_mbrain_data->ppb_g_flavor;

End:
	if (res_ppb_mbrain_data != NULL)
		kfree(res_ppb_mbrain_data);

	return ret;
}

static int mbraink_v6899_power_get_lpmstate_info(struct mbraink_lpm_state_data *lpmStateInfo)
{
	int ret = 0;
	struct lpm_dbg_lp_info lpm_info;
	int state_size = 0;
	int i = 0;

	pr_info("(%s)(%d)", __func__, __LINE__);
	if (lpmStateInfo == NULL)
		return -1;

	memset(&lpm_info, 0x00, sizeof(struct lpm_dbg_lp_info));
	lpm_get_suspend_event_info(&lpm_info);

	lpmStateInfo->state_num = NUM_SPM_STAT;

	if (NUM_SPM_STAT > MAX_LPM_STATE_NUM)
		state_size = MAX_LPM_STATE_NUM;
	else
		state_size = NUM_SPM_STAT;

	for (i = 0; i < state_size; i++) {
		lpmStateInfo->lpm_state_info[i].count = lpm_info.record[i].count;
		lpmStateInfo->lpm_state_info[i].duration = lpm_info.record[i].duration;
	}

	return ret;
}

static struct mbraink_power_ops mbraink_v6899_power_ops = {
	.getVotingInfo = mbraink_v6899_power_get_voting_info,
	.getPowerInfo = NULL,
	.getVcoreInfo = mbraink_v6899_power_getVcoreInfo,
	.getWakeupInfo = mbraink_v6899_get_power_wakeup_info,
	.getSpmInfo = mbraink_v6899_power_get_spm_info,
	.getSpmL1Info = mbraink_v6899_power_get_spm_l1_info,
	.getSpmL2Info = mbraink_v6899_power_get_spm_l2_info,
	.getScpInfo = mbraink_v6899_power_get_scp_info,
	.getModemInfo = mbraink_v6899_power_get_modem_info,
	.getSpmiInfo = mbraink_v6899_power_get_spmi_info,
	.getUvloInfo = mbraink_v6899_power_get_uvlo_info,
	.getPmicVoltageInfo = mbraink_v6899_power_get_pmic_voltage_info,
	.suspendprepare = mbraink_v6899_power_suspend_prepare,
	.postsuspend = mbraink_v6899_power_post_suspend,
	.getMmdvfsInfo = mbraink_v6899_power_get_mmdfvs_info,
	.getPowerThrottleHwInfo = mbraink_v6899_power_get_power_throttle_hw_info,
	.getLpmStateInfo = mbraink_v6899_power_get_lpmstate_info,
};

int mbraink_v6899_power_init(void)
{
	int ret = 0;

	ret = register_mbraink_power_ops(&mbraink_v6899_power_ops);
	mbraink_v6899_power_sys_res_init();
	ret = register_low_battery_mbrain_cb(pt2mbrain_hint_low_battery_volt_throttle);
	if (ret != 0) {
		pr_info("register low battery callback failed by: %d", ret);
		return ret;
	}
	ret = register_battery_oc_mbrain_cb(pt2mbrain_hint_battery_oc_throttle);
	if (ret != 0) {
		pr_info("register battery oc callback failed by: %d", ret);
		return ret;
	}
	ret = register_ppb_mbrian_cb(pt2mbrain_ppb_notify_func);
	if (ret != 0) {
		pr_info("register ppb callback failed by: %d", ret);
		return ret;
	}

	return ret;
}

int mbraink_v6899_power_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_power_ops();
	mbraink_v6899_power_sys_res_deinit();
	ret = unregister_ppb_mbrian_cb();
	if (ret != 0) {
		pr_info("ppb unregister callback failed by: %d", ret);
		return ret;
	}
	return ret;
}

