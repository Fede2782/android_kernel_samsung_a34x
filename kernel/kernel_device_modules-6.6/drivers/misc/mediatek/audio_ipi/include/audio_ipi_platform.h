/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __AUDIO_IPI_PLATFORM_H__
#define __AUDIO_IPI_PLATFORM_H__

#include <linux/types.h>
#include <audio_ipi_platform_common.h>
#include <audio_task.h>
#include <linux/mutex.h>

/* by chip */
uint32_t audio_get_dsp_id(const uint8_t task);
inline uint32_t msg_len_of_type(const uint8_t data_type);

struct audio_ipi_reg_dma_t {
	uint32_t magic_header;
	uint8_t task;
	uint8_t reg_flag; /* 1: register, 0: unregister */
	uint16_t __reserved;

	uint32_t a2d_size;
	uint32_t d2a_size;
	uint32_t magic_footer;
};

struct audio_task_info_t {
	uint32_t dsp_id;            /* dsp_id_t */
	uint8_t  is_dsp_support;    /* dsp_id supported or not */
	uint8_t  is_adsp;           /* adsp(HiFi) or not */
	uint8_t  is_scp;            /* scp(CM4) or not */
	uint8_t  task_ctrl;         /* task controller scene # */
};


#define AUDIO_IPI_DEVICE_NAME "audio_ipi"
#define AUDIO_IPI_IOC_MAGIC 'i'

#define AUDIO_IPI_IOCTL_SEND_MSG_ONLY _IOW(AUDIO_IPI_IOC_MAGIC, 0, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_PAYLOAD  _IOW(AUDIO_IPI_IOC_MAGIC, 1, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_DRAM     _IOW(AUDIO_IPI_IOC_MAGIC, 2, unsigned int)

#define AUDIO_IPI_IOCTL_INIT_DSP     _IOW(AUDIO_IPI_IOC_MAGIC, 20, unsigned int)
#define AUDIO_IPI_IOCTL_REG_DMA      _IOW(AUDIO_IPI_IOC_MAGIC, 21, unsigned int)

extern struct audio_task_info_t g_audio_task_info[TASK_SCENE_SIZE];

extern struct mutex reg_dma_lock;
extern struct mutex init_dsp_lock;

int audio_ipi_init_dsp_hifi3(const uint32_t dsp_id);

#endif /*__AUDIO_IPI_PLATFORM_H__ */
