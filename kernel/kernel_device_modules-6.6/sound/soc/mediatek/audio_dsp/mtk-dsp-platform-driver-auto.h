/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-dsp-platform-auto.h --  Mediatek ADSP platform
 *
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Feilong <Feilong.wei@mediatek.com>
 */


#ifndef _MTK_DSP_PLATFORM_DRIVER_AUTO_H_
#define _MTK_DSP_PLATFORM_DRIVER_AUTO_H_

#include <sound/pcm.h>
#include <linux/fs.h>

enum {
	ADSP_EVT_IRQ,
	ADSP_EVT_MEM,
	ADSP_EVT_IPI,
};

enum {
	MEM_TYPE_RING,
	MEM_TYPE_A2D,
	MEM_TYPE_D2A,
	MEM_TYPE_RSV,
};

struct adsp_irq_data {
	uint64_t task;
	int32_t core;
	int32_t xrun;
};

struct mem_data {
	uint64_t task;
	uint64_t phy_addr;
	uint64_t size;
	int32_t type;
};

struct adsp_ipi_info {
	uint64_t phy_addr;
	uint64_t buffer_size;
	uint64_t write_offset;
	uint64_t write_size;
	uint64_t phys_rp_addr;
};


struct adsp_evt_cb_data {
	int32_t evt_type;
	union {
		struct adsp_irq_data irq;
		struct mem_data mem;
		struct adsp_ipi_info ipi_info;
	};
	void *data;
};

typedef void (*adsp_evt_cb_func)(struct adsp_evt_cb_data *data);
struct mtk_dsp_evt_cb {
	adsp_evt_cb_func cb;
	void *data;
};

enum {
	TYPE_PCM_ID,
	TYPE_TASK_ID
};

enum rec_time_pos {
	REC_TIME_POS_CPY,
	REC_TIME_POS_PTR,
	REC_TIME_POS_IRQ,
	REC_TIME_POS_NUM,
};

struct audio_ipi_info {
	uint64_t type;
	uint64_t value;
};

struct audio_dsp_reg_feature {
	uint16_t reg_flag;
	uint16_t feature_id;
};

struct audio_dsp_query_status {
	uint16_t ready_flag;
	uint16_t core_id;
};

struct audio_dump_buffer_info {
	uint64_t pa;
	uint64_t bytes;
	uint64_t pra;
};

typedef int (*virt_ipi_cb_func)(struct audio_ipi_info *data);
typedef int (*virt_adsp_reg_feature_cb_func)(struct audio_dsp_reg_feature *data);
typedef int (*virt_adsp_query_status_cb_func)(struct audio_dsp_query_status *data);
typedef int (*virt_adsp_dump_cb_func)(struct audio_dump_buffer_info *data);

extern virt_ipi_cb_func virt_ipi_cb;
extern virt_adsp_reg_feature_cb_func virt_adsp_reg_feature_cb;
extern virt_adsp_query_status_cb_func virt_adsp_query_status_cb;
extern virt_adsp_dump_cb_func virt_adsp_dump_cb;

void mtk_dsp_register_event_cb(adsp_evt_cb_func cb, void *data);
void register_virt_ipi_cb(virt_ipi_cb_func cb);
void register_virt_adsp_reg_feature_cb(virt_adsp_reg_feature_cb_func cb);
void register_virt_adsp_query_status_cb(virt_adsp_query_status_cb_func cb);
void register_virt_adsp_dump_cb(virt_adsp_dump_cb_func cb);
snd_pcm_uframes_t guest_get_pcm_pointer(int dsp_scene, int *xrun);
int32_t guest_pcm_copy_dl(int dsp_scene, uint64_t phy_addr, uint64_t copy_size);
uint64_t get_irq_cnt(int task_scene);
int32_t guest_pcm_copy_ul(int dsp_scene, uint64_t phy_addr, uint64_t copy_size);
void guest_adsp_irq_notify(int core_id, int dsp_scene, int xrun);
void guest_adsp_task_share_dram_notify(int dsp_scene,
		int type, unsigned long long phy_addr, unsigned long long size);
void guest_adsp_irq_handler(struct mtk_base_dsp *dsp,
		     int core_id, int id, int xrun);

extern int is_guest_audio_task(int task_id);

extern int is_guest_ul_audio_task(int task_id);
extern int vadsp_probe(struct snd_soc_component *component);

#endif
