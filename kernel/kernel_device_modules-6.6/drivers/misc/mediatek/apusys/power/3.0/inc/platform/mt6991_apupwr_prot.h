/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MT6991_APUPWR_PROT_H__
#define __MT6991_APUPWR_PROT_H__
#include "apu_top.h"
#include "mt6991_apupwr.h"
/*
 * 0x4c2b0000 (mbox0 id 11) : HW semaphore for SMMU
 * 0x4c2c0000 (mbox0 id 12) : APU power driver data exchange
 * in dts, we define 0x4c2b0000 as mbox addr and its length is 0x20000
 * so we have to redefine the following offset to 0x10000 + real offset
 * ex: 0x4c2b0000 + 0x10028 (because of 0x10000 + 0x28) = 0x4c2c0028
 */
#define SPARE_DBG_REG0		0x10000 // mbox12_dummy0
#define SPARE_DBG_REG1		0x10004 // mbox12_dummy1
#define SPARE_DBG_REG2		0x10008 // mbox12_dummy2
#define SPARE_DBG_REG3		0x1000C // mbox12_dummy3
#define SPARE_DBG_REG4		0x10010 // mbox12_dummy4
#define SPARE_DBG_REG5		0x10014 // mbox12_dummy5
#define SPARE_DBG_REG6		0x10018 // mbox12_dummy6
#define SPARE_DBG_REG7		0x1001C // mbox12_dummy7
#define SPARE_DBG_REG8		0x10020 // mbox12_dummy8
#define SPARE_DBG_REG9		0x10024 // mbox12_dummy9
#define SPARE_DBG_REG10		0x10028	// mbox12_dummy10
#define SPARE_DBG_REG11		0x1002C	// mbox12_dummy11
#define SPARE_DBG_REG12		0x10030	// mbox12_dummy12
#define SPARE_DBG_REG13		0x10034	// mbox12_dummy13
#define SPARE_DBG_REG14		0x10038	// mbox12_dummy14
#define SPARE_DBG_REG15		0x1003C	// mbox12_dummy15
#define SPARE_DBG_REG16		0x10040	// mbox12_dummy16
#define SPARE_DBG_REG17		0x10044	// mbox12_dummy17
#define SPARE_DBG_REG18		0x10048	// mbox12_dummy18
#define SPARE_DBG_REG19         0x1004C // mbox12_dummy19

/*
 * The following are used for data exchange through spare register(s)
 * direction : uP write -> APMCU read
 */
#define DRV_STAT_SYNC_REG               SPARE_DBG_REG7
#define MBRAIN_DATA_SYNC_0_REG          SPARE_DBG_REG8  // pll recording
#define MBRAIN_DATA_SYNC_1_REG          SPARE_DBG_REG9  // vapu recording
#define MBRAIN_RCX_CNT                  0xF798          // PLL WA retry count, rcx on
#define MBRAIN_RCX_DUMPMNOCPLL_REG      0xF700          // PLL WA retry count, rcx on
#define MBRAIN_RCX_DUMPUPPLL_REG        0xF800          // PLL WA retry count, rcx on
#define MBRAIN_DVFS_CNT                 0xF998          // PLL WA retry count, dvfs
#define MBRAIN_DVFS_DUMPMNOCPLL_REG     0xF900          // PLL WA retry count, rcx on
#define MBRAIN_DVFS_DUMPUPPLL_REG       0xFA00          // PLL WA retry count, rcx on
#define MBRAIN_DUMP_SIZE                32

/*
 * The following are used for data exchange through spare register(s)
 * direction : APMCU write -> uP read
 */
#define APU_PBM_MONITOR_REG             SPARE_DBG_REG11
#define DEV_OPP_SYNC_REG                SPARE_DBG_REG12
#define HW_RES_SYNC_REG                 SPARE_DBG_REG13
#define DRV_CFG_SYNC_REG                SPARE_DBG_REG14
#define D_ACX_LIMIT_OPP_REG             SPARE_DBG_REG18
#define ACX0_LIMIT_OPP_REG              SPARE_DBG_REG18
#define ACX1_LIMIT_OPP_REG              SPARE_DBG_REG18
#define ACX2_LIMIT_OPP_REG              SPARE_DBG_REG18

enum {
	APUPWR_DBG_DEV_CTL = 0,
	APUPWR_DBG_DEV_SET_OPP,
	APUPWR_DBG_DVFS_DEBUG,
	APUPWR_DBG_DUMP_OPP_TBL,
	APUPWR_DBG_CURR_STATUS,
	APUPWR_DBG_PROFILING,
	APUPWR_DBG_CLK_SET_RATE,
	APUPWR_DBG_BUK_SET_VOLT,
	APUPWR_DBG_ARE,
	APUPWR_DBG_HW_VOTER,
	APUPWR_DBG_DUMP_OPP_TBL2,
};
enum apu_opp_limit_type {
	OPP_LIMIT_THERMAL = 0,	// limit by power API
	OPP_LIMIT_HAL,		// limit by i/o ctl
	OPP_LIMIT_DEBUG,	// limit by i/o ctl
};
struct drv_cfg_data {
	int8_t log_level;
	int8_t dvfs_debounce;	// debounce unit : ms
	int8_t disable_hw_meter;// 1: disable hw meter, bypass to read volt/freq
};
struct plat_cfg_data {
	int8_t aging_flag:4,
	       hw_id:4;
	int8_t vsram_vb_en;
};
struct device_opp_limit {
	int32_t vpu_max:6,
		vpu_min:6,
		dla_max:6,
		dla_min:6,
		lmt_type:8; // limit reason
};
struct cluster_dev_opp_info {
	uint32_t opp_lmt_reg;
	struct device_opp_limit dev_opp_lmt;
};
/*
 * due to this struct will be used to do data exchange through rpmsg
 * so the struct size can't over than 256 bytes
 * 4 bytes * 14 struct members = 56 bytes
 */
struct apu_pwr_curr_info {
	int buck_volt[BUCK_NUM];
	int buck_opp[BUCK_NUM];
	int pll_freq[PLL_NUM];
	int pll_opp[PLL_NUM];
};
/*
 * for satisfy size limitation of rpmsg data exchange is 256 bytes
 * we only put necessary information for opp table here
 * opp entries : 4 bytes * 6 struct members * 10 opp entries = 240 bytes
 * tbl_size : 4 bytes
 * total : 240 + 4 = 244 bytes
 */
struct tiny_dvfs_opp_entry {
	int vapu;       // = volt_bin - volt_age + volt_avs
	int vsram;
	int pll_freq[PLL_NUM];
};
struct tiny_dvfs_opp_tbl {
	int tbl_size;   // entry number
	struct tiny_dvfs_opp_entry opp[USER_MIN_OPP_VAL + 1];   // entry data
};
void mt6991_aputop_opp_limit(struct aputop_func_param *aputop,
		enum apu_opp_limit_type type);
#if IS_ENABLED(CONFIG_DEBUG_FS)
int mt6991_apu_top_dbg_open(struct inode *inode, struct file *file);
ssize_t mt6991_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos);
#endif
int mt6991_init_remote_data_sync(void __iomem *reg_base);
int mt6991_drv_cfg_remote_sync(struct aputop_func_param *aputop);
int mt6991_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src);
#endif
