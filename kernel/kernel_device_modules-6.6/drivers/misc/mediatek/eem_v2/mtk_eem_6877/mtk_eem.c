// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

/**
 * @file	mtk_eem.
 * @brief   Driver for EEM
 *
 */

#define __MTK_EEM_C__
/*=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/math64.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/rtc.h>
#include <linux/nvmem-consumer.h>

#if IS_ENABLED(CONFIG_OF)
	#include <linux/cpu.h>
	#include <linux/of_platform.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "mtk_gpufreq.h"
#endif

#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
#include "mach/mtk_thermal.h"
#endif
#include "mtk_ppm_api.h"
#include "mtk_cpufreq_api.h"
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_defeem.h"
#include "mtk_eem_internal_ap.h"


#include "mtk_eem_internal.h"

#include <regulator/consumer.h>

#if UPDATE_TO_UPOWER
#include "mtk_unified_power.h"
#endif

#include "mtk_mcdi_api.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
#include "mcupm_ipi_id.h"
#include "mcupm_driver.h"
#endif

#include "mtk_picachu.h"
#include "mtk_picachu_reservedmem.h"


/****************************************
 * define variables for legacy and eem
 ****************************************
 */
static unsigned int ctrl_EEMSN_Enable = 1;
static unsigned int ctrl_SN_Enable = 1;
static unsigned char ctrl_agingload_enable;
struct work_struct eem_work;


/* Get time stmp to known the time period */
//static unsigned long long eem_pTime_us, eem_cTime_us, eem_diff_us;
#if !EARLY_PORTING
#if ENABLE_INIT_TIMER
/* for setting pmic pwm mode and auto mode */
struct regulator *eem_regulator_vproc1;
struct regulator *eem_regulator_vproc2;
static void eem_buck_set_mode(unsigned int mode);
#endif
#endif
static unsigned int eem_to_cputoeb(unsigned int cmd,
	struct eem_ipi_data *eem_data);

static int create_procfs(void);
static int eem_aging_dump_proc_show(struct seq_file *m, void *v);

#if UPDATE_TO_UPOWER
static void eem_update_init2_volt_to_upower
	(struct eemsn_det *det, unsigned int *pmic_volt);
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id);
#endif


static struct eemsn_devinfo eem_devinfo;
#if ENABLE_INIT_TIMER
static struct hrtimer eem_init_check_timer;
static unsigned int vboot_check_cnt;
#endif
#if UPDATE_TO_UPOWER
static unsigned int record_tbl_locked[NR_FREQ];
#endif

#define WAIT_TIME	(2500000)
#define FALL_NUM        (3)

#if SUPPORT_PICACHU
#define PICACHU_SIG					(0xA5)
#define PICACHU_SIGNATURE_SHIFT_BIT	(24)
#define EEM_PHY_TEMPSPARE0		0x112788F0
#define EEM_PHY_TEMPSPARE1		0x112788F4
#define EEM_PHY_TEMPSPARE2		0x112788F8

#endif
/******************************************
 * common variables for legacy ptp
 *******************************************
 */
static int eem_log_en;
static unsigned int eem_checkEfuse = 1;
static unsigned int informEEMisReady;
int ipi_ackdata;

phys_addr_t picachu_sn_mem_base_phys;

phys_addr_t eem_log_phy_addr, eem_log_virt_addr;
uint32_t eem_log_size;
/* static unsigned int eem_disable = 1; */
struct eemsn_log *eemsn_log;
unsigned int seq;
/* Global variable for slow idle*/
unsigned int ptp_data[3] = {0, 0, 0};
static char *cpu_name[3] = {
	"L",
	"B",
	"CCI"
};


#if IS_ENABLED(CONFIG_OF)
void __iomem *eem_base;
#endif
#if ENABLE_INIT_TIMER
/*=============================================================
 * common functions for both ap and eem
 *=============================================================
 */

static void eem_post_work(struct work_struct *work)
{
	eem_buck_set_mode(0);
}

static enum hrtimer_restart eem_init_check_timer_func(struct hrtimer *timer)
{
	int ret;

	if ((eemsn_log->init_vboot_done) || (vboot_check_cnt >= 20)) {
		eem_error("eem init check disabled, cnt:%d, done:%d\n",
			vboot_check_cnt, eemsn_log->init_vboot_done);
		/* hrtimer_cancel(&eem_init_check_timer); */
#if !EARLY_PORTING
		/* CPU vporc post-process */
		schedule_work(&eem_work);
#endif
		ret = HRTIMER_NORESTART;
	} else {
		vboot_check_cnt++;
		eem_error("eem init check restart, vboot_check_cnt:%d\n",
			vboot_check_cnt);
		hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
		ret = HRTIMER_RESTART;
	}

	return ret;
}
#endif

static unsigned int eem_to_cputoeb(unsigned int cmd,
	struct eem_ipi_data *eem_data)
{
	unsigned int ret;

#if EEM_IPI_ENABLE
	eem_debug("to_cputoeb, cmd:%d\n", cmd);
	FUNC_ENTER(EEM_FUNC_LV_MODULE);
	eem_data->cmd = cmd;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	ret = mtk_ipi_send_compl(get_mcupm_ipidev(), CH_S_EEMSN,
		/*IPI_SEND_WAIT*/IPI_SEND_POLLING, eem_data,
		sizeof(struct eem_ipi_data)/MBOX_SLOT_SIZE, 2000);
#else
	ret = 0;
#endif
	if (ret != 0)
		eem_error("IPI error(cmd:%d) ret:%d\n", cmd, ret);
	else if (ipi_ackdata < 0)
		eem_error("cmd(%d) return error ack(%d)\n",
			cmd, ipi_ackdata);
	FUNC_EXIT(EEM_FUNC_LV_MODULE);
#endif
	return ret;
}

unsigned int mt_eem_is_enabled(void)
{
	return informEEMisReady;
}

static struct eemsn_det *id_to_eem_det(enum eemsn_det_id id)
{
	if (likely(id < NR_EEMSN_DET))
		return &eemsn_detectors[id];
	else
		return NULL;
}

static unsigned int read_efuse_by_offset(__u32 offset)
{
	__u32 value;
	struct platform_device *pdev;
	struct nvmem_device *nvmem_dev;
	struct device_node *node;

	node = of_find_node_by_name(NULL, "eem-fsm");
	if (node == NULL) {
		eem_error("%s fail to get device node\n", __func__);
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (pdev == NULL) {
		eem_error("%s failed to get pdev\n", __func__);
		return -1;
	}
	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev))
		eem_error("%s ptpod failed to get mtk_efuse device\n", __func__);
	nvmem_device_read(nvmem_dev, offset, sizeof(__u32), &value);
	eem_error("[EEM_DEBUG] offset= %d, value=%d", offset, value);
	return value;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
#if SUPPORT_PICACHU
static void get_picachu_efuse(void)
{
	unsigned int sig;
	void __iomem *addr_ptr;

	addr_ptr = (void __iomem *) picachu_reserve_mem_get_virt(PICACHU_EEM_ID);

	if (addr_ptr != NULL) {
		/* check signature */
		sig = (eem_read(addr_ptr) >> PICACHU_SIGNATURE_SHIFT_BIT)
			& 0xff;

		if (sig == PICACHU_SIG) {
			ctrl_agingload_enable = eem_read(addr_ptr) & 0x1;
			addr_ptr += 4;
			memcpy(eemsn_log->vf_tbl_det,
				addr_ptr, sizeof(eemsn_log->vf_tbl_det));
		}
	}
}
#endif
#endif

static int get_devinfo(void)
{
	int ret = 0, i = 0;
	int *val;
	unsigned int safeEfuse = 0, sn_safeEfuse = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	val = (int *)&eem_devinfo;

	/* FTPGM */
	val[0] = read_efuse_by_offset(DEVINFO_IDX_0);
	val[1] = read_efuse_by_offset(DEVINFO_IDX_1);
	val[2] = read_efuse_by_offset(DEVINFO_IDX_2);
	val[3] = read_efuse_by_offset(DEVINFO_IDX_3);
	val[4] = read_efuse_by_offset(DEVINFO_IDX_4);
	val[5] = read_efuse_by_offset(DEVINFO_IDX_5);
	val[6] = read_efuse_by_offset(DEVINFO_IDX_6);
	val[7] = read_efuse_by_offset(DEVINFO_IDX_7);
	val[8] = read_efuse_by_offset(DEVINFO_IDX_8);
	val[9] = read_efuse_by_offset(DEVINFO_IDX_9);
	val[10] = read_efuse_by_offset(DEVINFO_IDX_10);
	val[11] = read_efuse_by_offset(DEVINFO_IDX_11);
	val[12] = read_efuse_by_offset(DEVINFO_IDX_12);
	val[13] = read_efuse_by_offset(DEVINFO_IDX_13);
	val[14] = read_efuse_by_offset(DEVINFO_IDX_14);
	val[15] = read_efuse_by_offset(DEVINFO_IDX_15);
	val[16] = read_efuse_by_offset(DEVINFO_IDX_16);
	val[17] = read_efuse_by_offset(DEVINFO_IDX_17);
	val[18] = read_efuse_by_offset(DEVINFO_IDX_18);
	val[19] = read_efuse_by_offset(DEVINFO_IDX_19);
	val[20] = read_efuse_by_offset(DEVINFO_IDX_20);
	val[21] = read_efuse_by_offset(DEVINFO_IDX_21);


#if EEM_FAKE_EFUSE
	/* for verification */
	val[0] = DEVINFO_0;
	val[1] = DEVINFO_1;
	val[2] = DEVINFO_2;
	val[3] = DEVINFO_3;
	val[4] = DEVINFO_4;
	val[5] = DEVINFO_5;
	val[6] = DEVINFO_6;
	val[7] = DEVINFO_7;
	val[8] = DEVINFO_8;
	val[9] = DEVINFO_9;
	val[10] = DEVINFO_10;
	val[11] = DEVINFO_11;
	val[12] = DEVINFO_12;
	val[13] = DEVINFO_13;
	val[14] = DEVINFO_14;
	val[15] = DEVINFO_15;
	val[16] = DEVINFO_16;
	val[17] = DEVINFO_17;
	val[18] = DEVINFO_18;
	val[19] = DEVINFO_19;
	val[20] = DEVINFO_20;
	val[21] = DEVINFO_21;

#endif

	for (i = 0; i < NR_HW_RES_FOR_BANK; i++)
		eem_debug("[PTP_DUMP] RES%d: 0x%X\n",
			i, val[i]);

	if (val[0] == 0) {
		ret = 1;
		safeEfuse = 1;
		eem_error("No EFUSE (val[1]), use safe efuse\n");
	}

	if (val[IDX_HW_RES_SN] == 0) {
		sn_safeEfuse = 1;
		eem_error("No SN EFUSE (val[%d])\n", i);
	}


#if (EEM_FAKE_EFUSE)
	eem_checkEfuse = 1;
#endif

#ifdef MC50_LOAD
	safeEfuse = 1;
#endif
	if (safeEfuse) {
		val[0] = DEVINFO_0;
		val[1] = DEVINFO_1;
		val[2] = DEVINFO_2;
		val[3] = DEVINFO_3;
		val[4] = DEVINFO_4;
		val[5] = DEVINFO_5;
		val[6] = DEVINFO_6;
		val[7] = DEVINFO_7;
		val[8] = DEVINFO_8;
		val[9] = DEVINFO_9;
		val[10] = DEVINFO_10;
		val[11] = DEVINFO_11;
		val[12] = DEVINFO_12;
		val[13] = DEVINFO_13;
	}

	if (sn_safeEfuse) {
		val[14] = DEVINFO_14;
		val[15] = DEVINFO_15;
		val[16] = DEVINFO_16;
		val[17] = DEVINFO_17;
		val[18] = DEVINFO_18;
		val[19] = DEVINFO_19;
		val[20] = DEVINFO_20;
		val[21] = DEVINFO_21;
	}
#if FAKE_SN_DVT_EFUSE_FOR_DE
		val[14] = DEVINFO_14;
		val[15] = DEVINFO_15;
		val[16] = DEVINFO_16;
		val[17] = DEVINFO_17;
		val[18] = DEVINFO_18;
		val[19] = DEVINFO_19;
		val[20] = DEVINFO_20;
		val[21] = DEVINFO_21;
#endif
	FUNC_EXIT(FUNC_LV_HELP);
	return ret;
}

/*============================================================
 * function declarations of EEM detectors
 *============================================================
 */
//static void mt_ptp_lock(unsigned long *flags);
//static void mt_ptp_unlock(unsigned long *flags);

/*=============================================================
 * Local function definition
 *=============================================================
 */

#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
/* common part in thermal */
__weak int
tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	eem_error("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}

__weak int
tscpu_is_temp_valid(void)
{
	eem_error("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}
#endif

int base_ops_get_temp(struct eemsn_det *det)
{
#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
	enum thermal_bank_name ts_bank;

	if (det_to_id(det) == EEMSN_DET_L)
		ts_bank = THERMAL_BANK2;
	else if (det_to_id(det) == EEMSN_DET_B)
		ts_bank = THERMAL_BANK0;
	else if (det_to_id(det) == EEMSN_DET_CCI)
		ts_bank = THERMAL_BANK2;

	else
		ts_bank = THERMAL_BANK0;

	return tscpu_get_temp_by_bank(ts_bank);
#else
	return 0;
#endif
}

#if !EARLY_PORTING
#if ENABLE_INIT_TIMER
/* get regulator reference */
static int eem_buck_get(struct platform_device *pdev)
{
	int ret = 0;

	eem_regulator_vproc1 = devm_regulator_get_optional(&pdev->dev, "proc1");
	if (!eem_regulator_vproc1) {
		eem_error("eem_regulator_vproc1 error\n");
		return -EINVAL;
	}

	eem_regulator_vproc2 = devm_regulator_get_optional(&pdev->dev, "proc2");
	if (!eem_regulator_vproc2) {
		eem_error("eem_regulator_vproc2 error\n");
		return -EINVAL;
	}

	return ret;
}

static void eem_buck_set_mode(unsigned int mode)
{
	/* set pwm mode for each buck */
	eem_debug("pmic set mode (%d)\n", mode);
	if (mode) {
		regulator_set_mode(eem_regulator_vproc1, REGULATOR_MODE_FAST);
		regulator_set_mode(eem_regulator_vproc2, REGULATOR_MODE_FAST);
	} else {
		regulator_set_mode(eem_regulator_vproc1, REGULATOR_MODE_NORMAL);
		regulator_set_mode(eem_regulator_vproc2, REGULATOR_MODE_NORMAL);
	}
}
#endif
#endif

static void inherit_base_det(struct eemsn_det *det)
{
	/*
	 * Inherit ops from EEMSN_DET_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

	#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = eem_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, get_temp);

	INIT_OP(det->ops, volt_2_pmic);
	INIT_OP(det->ops, volt_2_eem);
	INIT_OP(det->ops, pmic_2_volt);
	INIT_OP(det->ops, eem_2_pmic);

	FUNC_EXIT(FUNC_LV_HELP);
}

#if UPDATE_TO_UPOWER
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id)
{
	enum upower_bank bank;

	switch (det_id) {
	case EEMSN_DET_L:
		bank = UPOWER_BANK_LL;
		break;
	case EEMSN_DET_B:
		bank = UPOWER_BANK_L;
		break;
	case EEMSN_DET_CCI:
		bank = UPOWER_BANK_CCI;
		break;
	default:
		bank = NR_UPOWER_BANK;
		break;
	}
	return bank;
}

static void eem_update_init2_volt_to_upower
	(struct eemsn_det *det, unsigned int *pmic_volt)
{
	unsigned int volt_tbl[NR_FREQ_CPU];
	enum upower_bank bank;
	int i;

	for (i = 0; i < NR_FREQ; i++)
		volt_tbl[i] = det->ops->pmic_2_volt(det, pmic_volt[i]);

	bank = transfer_ptp_to_upower_bank(det_to_id(det));
	if (bank < NR_UPOWER_BANK) {
		upower_update_volt_by_eem(bank, volt_tbl, NR_FREQ);
		 eem_debug
		  ("volt to upower (id: %d upower, pmic_volt[0] :0x%x)\n",
		  det->det_id, pmic_volt[0]);
	}
}

#endif

#if EN_EEM
#if SUPPORT_DCONFIG
static void eem_dconfig_set_det(struct eemsn_det *det, struct device_node *node)
{
	enum eemsn_det_id det_id = det_to_id(det);

	int doe_initmon = 0xFF, doe_clamp = 0;
	int doe_offset = 0xFF;
	int rc1 = 0, rc2 = 0, rc3 = 0;
#if UPDATE_TO_UPOWER
	int i;
#endif

	switch (det_id) {
	case EEMSN_DET_L:
		rc1 = of_property_read_u32(node, "eem-initmon-little",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-little",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-little",
			&doe_offset);
		break;
	case EEMSN_DET_B:
		rc1 = of_property_read_u32(node, "eem-initmon-big",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-big",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-big",
			&doe_offset);
		break;
	case EEMSN_DET_CCI:
		rc1 = of_property_read_u32(node, "eem-initmon-cci",
			&doe_initmon);
		rc2 = of_property_read_u32(node, "eem-clamp-cci",
			&doe_clamp);
		rc3 = of_property_read_u32(node, "eem-offset-cci",
			&doe_offset);
		break;
	default:
		eem_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	if ((!rc1) && (doe_initmon != 0xFF)) {
		if (det->features != doe_initmon) {
			det->features = doe_initmon;
			eemsn_log->det_log[det->det_id].features =
				(unsigned char)det->features;
			eem_error("[DCONFIG] feature modified by DT(0x%x)\n",
				doe_initmon);

			if (det_id < NR_EEMSN_DET) {
#if UPDATE_TO_UPOWER
				if (HAS_FEATURE(det, FEA_INIT01) == 0) {
					for (i = 0; i < NR_FREQ; i++)
						record_tbl_locked[i] =
					(unsigned int)det->volt_tbl_orig[i];

					eem_update_init2_volt_to_upower
					(det, record_tbl_locked);
				}
#endif
			}
		}
	}

	if (!rc2)
		det->volt_clamp = (char)doe_clamp;

	if ((!rc3) && (doe_offset != 0xFF)) {
		if (doe_offset < 1000)
			det->volt_offset = (char)(doe_offset & 0xff);
		else
			det->volt_offset = 0 -
				(char)((doe_offset - 1000) & 0xff);

		eemsn_log->det_log[det->det_id].volt_offset =
			det->volt_offset;
	}



}
#endif


static int eem_probe(struct platform_device *pdev)
{
	unsigned int ret;
	struct eemsn_det *det;
	struct eem_ipi_data eem_data;
#if IS_ENABLED(CONFIG_OF)
	struct device_node *node = NULL;
#endif
#if UPDATE_TO_UPOWER
	unsigned int locklimit = 0;
	//unsigned char lock;
	unsigned int i;
#endif

#if SUPPORT_DCONFIG
	unsigned int doe_status, sn_doe_status;
#endif
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_MODULE);
	seq = 0;

#if IS_ENABLED(CONFIG_OF)
	node = pdev->dev.of_node;
	if (!node) {
		eem_error("get eem device node err\n");
		return -ENODEV;
	}

#if SUPPORT_DCONFIG
	if (of_property_read_u32(node, "eem-status",
		&doe_status) < 0) {
		eem_debug("[DCONFIG] eem-status read error!\n");
	} else {
		eem_debug("[DCONFIG] success-> status:%d, EEM_Enable:%d\n",
			doe_status, ctrl_EEMSN_Enable);
		if (((doe_status == 1) || (doe_status == 0)) &&
			(ctrl_EEMSN_Enable != (unsigned char)doe_status)) {
			ctrl_EEMSN_Enable = (unsigned char)doe_status;
			eem_error("[DCONFIG] eem sts modified by DT(0x%x).\n",
				doe_status);
		}
	}
	if (of_property_read_u32(node, "sn-status",
		&sn_doe_status) < 0) {
		eem_debug("[DCONFIG] sn-status read error!\n");
	} else {
		eem_debug("[DCONFIG] success-> status:%d, sn_Enable:%d\n",
			sn_doe_status, ctrl_SN_Enable);
		if (((sn_doe_status == 1) || (sn_doe_status == 0)) &&
			(ctrl_SN_Enable != (unsigned char)sn_doe_status)) {
			ctrl_SN_Enable = (unsigned char)sn_doe_status;
			eem_error("[DCONFIG] sn sts modified by DT(0x%x).\n",
				sn_doe_status);
		}
	}

#endif

#if SUPPORT_PICACHU
	/* Setup IO addresses */
	eem_base = of_iomap(node, 0);
	eem_debug("[EEM] eem_base = 0x%p\n", eem_base);
#endif
#endif

	for_each_det(det)
		inherit_base_det(det);

	/* for slow idle */
	ptp_data[0] = 0xffffffff;

#if SUPPORT_DCONFIG
	for_each_det(det)
		eem_dconfig_set_det(det, node);
#endif

#if EEM_NOT_READY
	ctrl_EEMSN_Enable = 0;
	ctrl_SN_Enable = 0;
#endif

	for_each_det(det) {
		if ((det->num_freq_tbl < 8) ||
			(det->volt_tbl_orig[0] == 0) ||
			(det->freq_tbl[0] == 0)) {
			ctrl_EEMSN_Enable = 0;
			ctrl_SN_Enable = 0;
		}
	}

	eemsn_log->eemsn_enable = ctrl_EEMSN_Enable;
	eemsn_log->sn_enable = ctrl_SN_Enable;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_cputoeb(IPI_EEMSN_GET_EEM_VOLT, &eem_data);

	ptp_data[0] = 0;

#if UPDATE_TO_UPOWER
	if (ctrl_EEMSN_Enable != 0) {
		while (1) {
			if ((eemsn_log->init2_v_ready == 0) &&
				(locklimit < 5)) {
				locklimit++;
				eem_error(
		"wait init2_v_ready:%d, locklimit:%d\n",
				eemsn_log->init2_v_ready, locklimit);
				mdelay(5); /* wait 5 ms */
				continue; /* if lock, read dram again */
			} else
				break;
		}
		for_each_det(det) {
			if (eemsn_log->init2_v_ready == 0)
				for (i = 0; i < NR_FREQ; i++)
					det->volt_tbl_pmic[i] = (unsigned int)
						det->volt_tbl_orig[i];
			else {
				for (i = 0; i < NR_FREQ; i++) {
					if (
					eemsn_log->det_log[
	det->det_id].volt_tbl_pmic[i] != 0)
						det->volt_tbl_pmic[i] =
	(unsigned int) eemsn_log->det_log[det->det_id].volt_tbl_pmic[i];
					eem_debug("pmic[%d], 0x%x",
						i, det->volt_tbl_pmic[i]);
				}
			}
			eem_update_init2_volt_to_upower(det,
				det->volt_tbl_pmic);
			// eem_save_final_volt_aee(det);
		}
	} else {
		for_each_det(det) {
			for (i = 0; i < NR_FREQ; i++)
				det->volt_tbl_pmic[i] = (unsigned int)
					det->volt_tbl_orig[i];

			eem_update_init2_volt_to_upower(det,
				det->volt_tbl_pmic);
		}
	}
#endif


#if !EARLY_PORTING
#if ENABLE_INIT_TIMER
	if (ctrl_SN_Enable) {
		ret = eem_buck_get(pdev);
		if (ret != 0)
			eem_error("eem_buck_get failed\n");

		/* CPU post-process */
		eem_buck_set_mode(1);
	}
#endif
#endif

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_cputoeb(IPI_EEMSN_INIT02, &eem_data);

	if (ctrl_EEMSN_Enable == 0)
		return 0;

	informEEMisReady = 1;

	for_each_det(det) {
		cpudvfsindex = detid_to_dvfsid(det);
		mt_cpufreq_update_legacy_volt(cpudvfsindex,
			det->volt_tbl_pmic, det->num_freq_tbl);
		memcpy(det->volt_tbl_init2,
			eemsn_log->det_log[det->det_id].volt_tbl_init2,
			sizeof(det->volt_tbl_init2));
		// eem_save_init2_volt_aee(det);
	}


#if ENABLE_INIT_TIMER
	if (ctrl_SN_Enable) {
		hrtimer_start(&eem_init_check_timer,
		ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
	}
#endif


	create_procfs();

	eem_debug("%s ok\n", __func__);
	FUNC_EXIT(FUNC_LV_MODULE);
	return 0;
}


#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt_eem_of_match[] = {
	{ .compatible = "mediatek,eem_fsm", },
	{},
};
#endif

static struct platform_driver eem_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= eem_probe,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
	.name		= "mt-eem",
#if IS_ENABLED(CONFIG_OF)
	.of_match_table = mt_eem_of_match,
#endif
	},
};

#if IS_ENABLED(CONFIG_PROC_FS)
int mt_eem_opp_num(enum eemsn_det_id id)
{
	struct eemsn_det *det = id_to_eem_det(id);

	FUNC_ENTER(FUNC_LV_API);
	if (det == NULL)
		return 0;

	FUNC_EXIT(FUNC_LV_API);

	return NR_FREQ;
}
EXPORT_SYMBOL(mt_eem_opp_num);

void mt_eem_opp_freq(enum eemsn_det_id id, unsigned int *freq)
{
	struct eemsn_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	if (det == NULL)
		return;

	for (i = 0; i < NR_FREQ; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_freq);

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current EEM stauts
 */
static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s\n",
		((char *)(det->name) + 8),
		det->disabled ? "disabled" : "enable"
		);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM status by procfs interface
 */
static ssize_t eem_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_det *det = (struct eemsn_det *)pde_data(file_inode(file));
	struct eem_ipi_data eem_data;
	int ipi_ret __maybe_unused = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;
	eem_debug("in eem debug proc write 2~~~~~~~~\n");

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		eem_debug("in eem debug proc write 3~~~~~~~~\n");
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = enabled;
		ipi_ret = eem_to_cputoeb(IPI_EEMSN_DEBUG_PROC_WRITE, &eem_data);
		det->disabled = enabled;

	} else
		ret = -EINVAL;

out:
	eem_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#if IS_ENABLED(CONFIG_MTK_PTPOD_ENG_DEBUG)
/*
 * show current aging margin
 */
static int eem_setmargin_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] volt clamp:%d\n",
		   ((char *)(det->name) + 8),
		   det->volt_clamp);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * remove aging margin
 */
static ssize_t eem_setmargin_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int aging_val[2];
	int i = 0;
	int start_oft __maybe_unused, end_oft __maybe_unused;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_det *det = (struct eemsn_det *)pde_data(file_inode(file));
	char *tok;
	char *cmd_str = NULL;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		ret = -EINVAL;

	while ((tok = strsep(&buf, " ")) != NULL) {
		if (i >= 2) {
			eem_error("number of arguments > 2!\n");
			goto out;
		}

		if (kstrtoint(tok, 10, &aging_val[i])) {
			eem_error("Invalid input: %s\n", tok);
			goto out;
		} else
			i++;
	}

	if (!strncmp(cmd_str, "aging", sizeof("aging"))) {
		start_oft = aging_val[0];
		end_oft = aging_val[1];
		//eem_calculate_aging_margin(det, start_oft, end_oft);

		ret = count;
	} else if (!strncmp(cmd_str, "clamp", sizeof("clamp"))) {
		if (aging_val[0] < 20)
			det->volt_clamp = aging_val[0];

		ret = count;
	} else {
		ret = -EINVAL;
		goto out;
	}

	//eem_set_eem_volt(det);

out:
	eem_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}
#endif /* CONFIG_MTK_PTPOD_ENG_DEBUG */

static void dump_sndata_to_de(struct seq_file *m)
{
	int *val = (int *)&eem_devinfo;
	int i, j, addr;

	seq_printf(m,
	"[%d]=================Start EEMSN dump===================\n",
	seq++);

	for (i = 0; i < sizeof(struct eemsn_devinfo) / sizeof(unsigned int);
		i++)
		seq_printf(m, "[%d]M_HW_RES%d\t= 0x%08X\n",
		seq++, i, val[i]);

	seq_printf(m, "[%d]Start dump_CPE:\n", seq++);
	for (i = 0; i < MIN_SIZE_SN_DUMP_CPE; i++) {
		if (i == 5)
			addr = SN_CPEIRQSTS;
		else if (i == 6)
			addr = SN_CPEINTSTSRAW;
		else
			addr = (SN_COMPAREDVOP + i * 4);

		seq_printf(m, "[%d]0x%x = 0x%x\n",
			seq++, addr,
			eemsn_log->sn_log.reg_dump_cpe[i]);

	}
	seq_printf(m, "[%d]Start dump_sndata:\n", seq++);
	for (i = 0; i < SIZE_SN_DUMP_SENSOR; i++) {
		seq_printf(m, "[%d]0x%x = 0x%x\n",
			seq++, (SN_C0ASENSORDATA + i * 4),
			eemsn_log->sn_log.reg_dump_sndata[i]);
	}

	seq_printf(m, "[%d]start dump_sn_cpu:\n", seq++);
	for (i = 0; i < NUM_SN_CPU; i++) {
		for (j = 0; j < SIZE_SN_MCUSYS_REG; j++) {
			seq_printf(m, "[%d]0x%x = 0x%x\n",
				seq++, (sn_mcysys_reg_base[i] +
				sn_mcysys_reg_dump_off[j]),
				eemsn_log->sn_log.reg_dump_sn_cpu[i][j]);
		}
	}
	seq_printf(m,
	"[%d]=================End EEMSN dump===================\n",
	seq++);


}

static int eem_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	unsigned int locklimit = 0;
	unsigned char lock, oppidx;
	enum sn_det_id i;
	struct eemsn_det *det;

	FUNC_ENTER(FUNC_LV_HELP);


	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_DUMP_PROC_SHOW, &eem_data);
	seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	/* Print initial data */
	seq_printf(m, "ctrl_agingload_enable=%d\n", ctrl_agingload_enable);
	seq_printf(m, "[%d]========Start sn_trigger_sensing!\n", seq++);

	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* eem_error("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* eem_error("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}

	for (i = 0; i < NR_SN_DET; i++) {
		det = id_to_eem_det((enum eemsn_det_id)i);

		seq_printf(m, "[%d]T_SVT_HV_%sCPU:%d %d %d %d\n",
			seq, cpu_name[i],
			eemsn_log->sn_cal_data[i].T_SVT_HV_HT,
			eemsn_log->sn_cal_data[i].T_SVT_LV_HT,
			eemsn_log->sn_cal_data[i].T_SVT_HV_RT,
			eemsn_log->sn_cal_data[i].T_SVT_LV_RT);

#if VMIN_PREDICT_ENABLE
		seq_printf(m, "[%d]id:%d, ATE_Temp_decode:%d, T_SVT_current:0, ",
			seq++, i, eem_devinfo.ATE_TEMP);

		seq_printf(m, "[%d]SN_Vmin:0x%x, CPE_Vmin:0x%x, init2[0]:0x%x, ",
			seq++, eemsn_log->sn_log.sd[i].SN_Vmin,
			eemsn_log->sn_log.sd[i].CPE_Vmin,
			eemsn_log->det_log[i].volt_tbl_init2[0]);
		seq_printf(m, "sn_aging:%d, SN_temp:0, CPE_temp:0\n",
			eemsn_log->sn_cal_data[i].sn_aging);

#else
		seq_printf(m, "[%d]id:%d, ATE_Temp_decode:%d, T_SVT_current:%d\n",
			seq++, i, eem_devinfo.ATE_TEMP,
			eemsn_log->sn_log.sd[i].T_SVT_current);

		seq_printf(m,
			"[%d]CPE_temp_RT:%d %d, CPE_temp_HT:%d %d\n",
			seq++,
			eemsn_log->sn_log.sd[i].CPE_temp_RT[0],
			eemsn_log->sn_log.sd[i].CPE_temp_RT[1],
			eemsn_log->sn_log.sd[i].CPE_temp_HT[0],
			eemsn_log->sn_log.sd[i].CPE_temp_HT[1]);

		seq_printf(m,
			"[%d]SN_Vmin:0x%x, CPE_Vmin:0x%x 0x%x, init2[0]:0x%x, ",
			seq++, eemsn_log->sn_log.sd[i].SN_Vmin,
			eemsn_log->sn_log.sd[i].CPE_Vmin[0],
			eemsn_log->sn_log.sd[i].CPE_Vmin[1],
			eemsn_log->det_log[i].volt_tbl_init2[0]);
		seq_printf(m, "sn_aging:%d %d, SN_temp:%d %d, CPE_temp:%d %d\n",
			eemsn_log->sn_log.sd[i].cur_sn_aging[0],
			eemsn_log->sn_log.sd[i].cur_sn_aging[1],
			eemsn_log->sn_log.sd[i].SN_temp[0],
			eemsn_log->sn_log.sd[i].SN_temp[1],
			eemsn_log->sn_log.sd[i].final_CPE_temp[0],
			eemsn_log->sn_log.sd[i].final_CPE_temp[1]);
#endif
		oppidx = eemsn_log->sn_log.sd[i].cur_oppidx;
		seq_printf(m, "cur_opp:%d, dst_volt_pmic:0x%x, footprint:0x%x\n",
			oppidx,
			eemsn_log->sn_log.sd[i].dst_volt_pmic,
			eemsn_log->sn_log.footprint[i]);
		seq_printf(m,
			"[%d]cur_volt:%d, new dst_volt_pmic:%d, max_temp:%d, min_temp:%d",
			seq++, eemsn_log->sn_log.sd[i].cur_volt,
			det->ops->pmic_2_volt(det,
			eemsn_log->sn_log.sd[i].dst_volt_pmic),
			eemsn_log->sn_log.sd[i].max_temp,
			eemsn_log->sn_log.sd[i].min_temp);
		seq_printf(m, " cur_volt_ptp:%d\n",
			det->ops->pmic_2_volt(det,
			eemsn_log->det_log[det->det_id].volt_tbl_pmic[oppidx]
			));
	}


	seq_printf(m, "allfp:0x%x\n",
		eemsn_log->sn_log.allfp);

	dump_sndata_to_de(m);

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_aging_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;
	unsigned char lock;
	unsigned char i, j, itbl;
	unsigned int locklimit = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_AGING_DUMP_PROC_SHOW, &eem_data);

	for (i = 0; i < NR_EEMSN_DET; i++) {
		seq_printf(m, "id:%d, vf_tbl_det pi_vf_num:%d\n",
		i, eemsn_log->vf_tbl_det[i].pi_vf_num);
		if (eemsn_log->vf_tbl_det[i].pi_vf_num <= NR_PI_VF)
			for (j = 0; j < eemsn_log->vf_tbl_det[i].pi_vf_num; j++)
				seq_printf(m, "idx:%d, f:%d, v:0x%x\n",
				j, eemsn_log->vf_tbl_det[i].pi_freq_tbl[j],
				eemsn_log->vf_tbl_det[i].pi_volt_tbl[j]);
	}

	seq_printf(m, "T_SVT_HV_LCPU:%d %d %d %d\n",
		eem_devinfo.T_SVT_HV_LCPU_HT,
		eem_devinfo.T_SVT_LV_LCPU_HT,
		eem_devinfo.T_SVT_HV_LCPU_RT,
		eem_devinfo.T_SVT_LV_LCPU_RT);

	seq_printf(m, "T_SVT_HV_BCPU:%d %d %d %d\n",
		eem_devinfo.T_SVT_HV_BCPU_HT,
		eem_devinfo.T_SVT_LV_BCPU_HT,
		eem_devinfo.T_SVT_HV_BCPU_RT,
		eem_devinfo.T_SVT_LV_BCPU_RT);

	seq_printf(m, "IN init_det, LCPU_A_T0_SVT:%d, LVT:%d, ",
		eem_devinfo.LCPU_A_T0_SVT,
		eem_devinfo.LCPU_A_T0_LVT);
	seq_printf(m, "ULVT:%d, ATE_TEMP:%d\n",
		eem_devinfo.LCPU_A_T0_ULVT,
		eem_devinfo.ATE_TEMP);

	seq_printf(m, "IN init_det, BCPU_A_T0_SVT:%d, LVT:%d, ",
		eem_devinfo.BCPU_A_T0_SVT,
		eem_devinfo.BCPU_A_T0_LVT);
	seq_printf(m, "ULVT:%d\n",
		eem_devinfo.BCPU_A_T0_ULVT);

	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* eem_error("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* eem_error("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}

	for (i = 0; i < NR_SN_DET; i++) {
		seq_printf(m, "id:%d\n", i);

		seq_printf(m, "cal_sn_aging, atvt A_Tused_SVT:%d, LVT:%d, ",
			eemsn_log->sn_cal_data[i].atvt.A_Tused_SVT,
			eemsn_log->sn_cal_data[i].atvt.A_Tused_LVT);
		seq_printf(m, "ULVT:%d, cur temp:%d\n",
			eemsn_log->sn_cal_data[i].atvt.A_Tused_ULVT,
			eemsn_log->sn_cal_data[i].TEMP_CAL);

		for (itbl = 0; (itbl < NR_PI_VF); itbl++) {
			if (eemsn_log->sn_cal_data[i].cpe_init_aging[itbl]) {
				seq_printf(m,
				"[cal_sn_aging]id:%d, cpe_init_aging:%llu, ",
					i, eemsn_log->sn_cal_data[i].cpe_init_aging[itbl]);
				seq_printf(m, "CPE_Aging:%d, sn_anging:%d, itbl:%d\n",
					eemsn_log->sn_cal_data[i].CPE_Aging[itbl],
					eemsn_log->sn_cal_data[i].sn_aging[itbl], itbl);
			}
		}
		seq_printf(m, "volt_cross:%d, count_cross:%d\n",
			eemsn_log->sn_cal_data[i].volt_cross,
			eemsn_log->sn_cal_data[i].count_cross);
	}

	if (ipi_ret != 0)
		seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_sn_sram_proc_show(struct seq_file *m, void *v)
{
	phys_addr_t sn_mem_base_phys;
	phys_addr_t sn_mem_size;
	phys_addr_t sn_mem_base_virt = 0;
	void __iomem *addr_ptr;

	FUNC_ENTER(FUNC_LV_HELP);

	/* sn_mem_size = NR_FREQ * 2; */
	sn_mem_size = OFFS_SN_VOLT_E_4B - OFFS_SN_VOLT_S_4B;

	sn_mem_base_phys = OFFS_SN_VOLT_S_4B;
	if ((void __iomem *)sn_mem_base_phys != NULL)
		sn_mem_base_virt =
		(phys_addr_t)(uintptr_t)ioremap_wc(
		sn_mem_base_phys,
		sn_mem_size);

	if ((void __iomem *)(sn_mem_base_virt) != NULL) {
		for (addr_ptr = (void __iomem *)(sn_mem_base_virt);
			addr_ptr <= ((void __iomem *)(sn_mem_base_virt) +
			OFFS_SN_VOLT_E_4B - OFFS_SN_VOLT_S_4B);
			(addr_ptr += 4))
			seq_printf(m, "0x%08X\n",
				(unsigned int)eem_read(addr_ptr));
	}
	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_hrid_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);
	for (i = 0; i < 4; i++)
		seq_printf(m, "%s[HRID][%d]: 0x%08X\n", EEM_TAG, i,
			read_efuse_by_offset(DEVINFO_HRID_0 + i));

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_efuse_proc_show(struct seq_file *m, void *v)
{
	int *val = (int *)&eem_devinfo;
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);
	for (i = 0; i < IDX_HW_RES_SN; i++)
		seq_printf(m, "%s[PTP_DUMP] ORIG_RES%d: 0x%08X\n", EEM_TAG, i,
			read_efuse_by_offset(DEVINFO_IDX_0 + i));

	/* Depend on EFUSE location */
	for (i = 0; i < NR_HW_RES_FOR_BANK; i++)
		seq_printf(m, "%s[PTP_DUMP] RES%d: 0x%08X\n", EEM_TAG, i,
			val[i]);

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

static int eem_freq_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det;
	unsigned int i;
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	for_each_det(det) {
		cpudvfsindex = detid_to_dvfsid(det);
		for (i = 0; i < NR_FREQ_CPU; i++) {
			if (det->det_id <= EEMSN_DET_CCI) {
				seq_printf(m,
					"%s[DVFS][CPU_%s][OPP%d] volt:%d, freq:%d\n",
					EEM_TAG, cpu_name[cpudvfsindex], i,
					det->ops->pmic_2_volt(det,
					det->volt_tbl_orig[i]) * 10,
#if SET_PMIC_VOLT_TO_DVFS
					mt_cpufreq_get_freq_by_idx(cpudvfsindex,
									i)
					/ 1000
#else
					0
#endif
					);
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}

/*
 * show current voltage
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	struct eemsn_det *det = (struct eemsn_det *)m->private;
	u32 rdata = 0, i;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt(det);

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);

	/* update volt_tbl_pmic info from mcupm */
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_PULL_DATA, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);

	if (det->features != 0) {
		for (i = 0; i < NR_FREQ; i++)
			seq_printf(m, "[%d],freq = [%u], eem = [%x], pmic = [%x], volt = [%d]\n",
			i,
			det->freq_tbl[i],
			eemsn_log->det_log[det->det_id].volt_tbl_init2[i],
			eemsn_log->det_log[det->det_id].volt_tbl_pmic[i],
			det->ops->pmic_2_volt(det,
			eemsn_log->det_log[det->det_id].volt_tbl_pmic[i]));
	}
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current EEM status
 */
static int eem_status_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "bank = %d, feature:0x%x, T(%d) - (",
		   det->det_id, det->features, det->ops->get_temp(det));
	for (i = 0; i < NR_FREQ - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (",
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));

	for (i = 0; i < NR_FREQ - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
/*
 * set EEM log enable by procfs interface
 */

static int eem_log_en_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_LOGEN_PROC_SHOW, &eem_data);
	seq_printf(m, "kernel:%d, EB:%d\n", eem_log_en, ipi_ret);
	FUNC_EXIT(FUNC_LV_HELP);


	return 0;
}

static ssize_t eem_log_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret __maybe_unused = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &eem_log_en)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = eem_log_en;
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_LOGEN_PROC_WRITE, &eem_data);


out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

static int eem_en_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_EN_PROC_SHOW, &eem_data);
	seq_printf(m, "kernel:%d, EB:%d\n", ctrl_EEMSN_Enable, ipi_ret);
	FUNC_EXIT(FUNC_LV_HELP);


	return 0;
}

static ssize_t eem_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret __maybe_unused = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &ctrl_EEMSN_Enable)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eemsn_log->eemsn_enable = ctrl_EEMSN_Enable;
	eem_data.u.data.arg[0] = ctrl_EEMSN_Enable;
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_EN_PROC_WRITE, &eem_data);


out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

static int eem_sn_en_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_SNEN_PROC_SHOW, &eem_data);
	seq_printf(m, "kernel:%d, EB:%d\n", ctrl_SN_Enable, ipi_ret);
	FUNC_EXIT(FUNC_LV_HELP);


	return 0;
}

static ssize_t eem_sn_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret __maybe_unused = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &ctrl_SN_Enable)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}


	ret = 0;
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eemsn_log->sn_enable = ctrl_SN_Enable;
	eem_data.u.data.arg[0] = ctrl_SN_Enable;
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_SNEN_PROC_WRITE, &eem_data);


out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

static int eem_force_sensing_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_FORCE_SN_SENSING, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int eem_pull_data_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
#if ENABLE_COUNT_SNTEMP
	unsigned int i;
	unsigned char lock;
	unsigned int locklimit = 0;
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cputoeb(IPI_EEMSN_PULL_DATA, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);
#if ENABLE_COUNT_SNTEMP
	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* eem_error("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* eem_error("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}
	for (i = 0; i < NR_SN_DET; i++) {
		seq_printf(m,
		"id:%d, sn_temp_cnt -1:%d, -2:%d, -3:%d, -4:%d, -5:%d\n",
		i,
		eemsn_log->sn_temp_cnt[i][0],
		eemsn_log->sn_temp_cnt[i][1],
		eemsn_log->sn_temp_cnt[i][2],
		eemsn_log->sn_temp_cnt[i][3],
		eemsn_log->sn_temp_cnt[i][4]);
	}
#endif
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show EEM offset
 */
static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set EEM offset by procfs
 */
static ssize_t eem_offset_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eemsn_det *det = (struct eemsn_det *)pde_data(file_inode(file));
	unsigned int ipi_ret __maybe_unused = 0;
	struct eem_ipi_data eem_data;


	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = offset;
		ipi_ret = eem_to_cputoeb(IPI_EEMSN_OFFSET_PROC_WRITE, &eem_data);
		/* to show in eem_offset_proc_show */
		det->volt_offset = (signed char)offset;
		eem_debug("set volt_offset %d(%d)\n", offset, det->volt_offset);
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#if IS_ENABLED(CONFIG_PROC_FS)
#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(				\
		struct inode *inode,				\
		struct file *file)				\
	{							\
		return single_open(				\
			file,					\
			name ## _proc_show,			\
			pde_data(inode));			\
	}							\
	static const struct proc_ops name ## _proc_fops =	\
	{								\
		.proc_open	= name ## _proc_open,			\
		.proc_read	= seq_read,				\
		.proc_lseek	= seq_lseek,				\
		.proc_release	= single_release,			\
		.proc_write	= name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(				\
		struct inode *inode,				\
		struct file *file)				\
	{							\
		return single_open(				\
			file,					\
			name ## _proc_show,			\
			pde_data(inode));			\
	}							\
	static const struct proc_ops name ## _proc_fops = {	\
		.proc_open	= name ## _proc_open,			\
		.proc_read	= seq_read,				\
		.proc_lseek	= seq_lseek,				\
		.proc_release	= single_release,			\
	}

#define PROC_ENTRY(name)					\
	{							\
		__stringify(name),				\
		&name ## _proc_fops				\
	}
#endif /* CONFIG_PROC_FS */

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_status);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RO(eem_aging_dump);
PROC_FOPS_RO(eem_sn_sram);
PROC_FOPS_RO(eem_hrid);
PROC_FOPS_RO(eem_efuse);
PROC_FOPS_RO(eem_freq);
PROC_FOPS_RW(eem_log_en);
PROC_FOPS_RW(eem_en);
PROC_FOPS_RW(eem_sn_en);
PROC_FOPS_RO(eem_force_sensing);
PROC_FOPS_RO(eem_pull_data);
#if IS_ENABLED(CONFIG_MTK_PTPOD_ENG_DEBUG)
PROC_FOPS_RW(eem_setmargin);
#endif /* CONFIG_MTK_PTPOD_ENG_DEBUG */

static int create_procfs(void)
{
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct eemsn_det *det;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_status),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
		#if IS_ENABLED(CONFIG_MTK_PTPOD_ENG_DEBUG)
		PROC_ENTRY(eem_setmargin),
		#endif /* CONFIG_MTK_PTPOD_ENG_DEBUG */
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_aging_dump),
		PROC_ENTRY(eem_sn_sram),
		PROC_ENTRY(eem_hrid),
		PROC_ENTRY(eem_efuse),
		PROC_ENTRY(eem_freq),
		PROC_ENTRY(eem_log_en),
		PROC_ENTRY(eem_en),
		PROC_ENTRY(eem_sn_en),
		PROC_ENTRY(eem_force_sensing),
		PROC_ENTRY(eem_pull_data),
	};

	FUNC_ENTER(FUNC_LV_HELP);

	/* create procfs root /proc/eem */
	eem_dir = proc_mkdir("eem", NULL);

	if (!eem_dir) {
		eem_error("[%s]: mkdir /proc/eem failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	/* if ctrl_EEMSN_Enable =1, and has efuse value,
	 * create other banks procfs
	 */
	if (ctrl_EEMSN_Enable != 0 && eem_checkEfuse == 1) {
		for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
			if (!proc_create(eem_entries[i].name, 0664,
						eem_dir, eem_entries[i].fops)) {
				eem_error("[%s]: create /proc/eem/%s failed\n",
						__func__,
						eem_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->features == 0)
				continue;

			det_dir = proc_mkdir(det->name, eem_dir);

			if (!det_dir) {
				eem_debug("[%s]: mkdir /proc/eem/%s failed\n"
						, __func__, det->name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -2;
			}

			for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
				if (!proc_create_data(det_entries[i].name,
					0664,
					det_dir,
					det_entries[i].fops, det)) {
					eem_debug
			("[%s]: create /proc/eem/%s/%s failed\n", __func__,
			det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
				}
			}
		}

	} /* if (ctrl_EEMSN_Enable != 0) */

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
#endif /* CONFIG_PROC_FS */


unsigned int get_efuse_status(void)
{
	return eem_checkEfuse;
}


/*
 * Module driver
 */
static int __init eem_init(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	struct eem_ipi_data eem_data;
struct eemsn_det *det;
#endif
	int err = 0;
#if defined(MC50_LOAD)
	/* d __iomem *spare1_phys; */
#endif

	eem_debug("[EEM] ctrl_EEMSN_Enable=%d\n", ctrl_EEMSN_Enable);
	get_devinfo();

#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)

	err = mtk_ipi_register(get_mcupm_ipidev(), CH_S_EEMSN, NULL, NULL,
		(void *)&ipi_ackdata);
	if (err != 0) {
		eem_error("%s error ret:%d\n", __func__, err);
		return 0;
	}

	eem_log_phy_addr =
		mcupm_reserve_mem_get_phys(MCUPM_EEMSN_MEM_ID);
	eem_log_virt_addr =
		mcupm_reserve_mem_get_virt(MCUPM_EEMSN_MEM_ID);
	eem_log_size = sizeof(struct eemsn_log);
	eemsn_log = (struct eemsn_log *)eem_log_virt_addr;

	memset(eemsn_log, 0, sizeof(struct eemsn_log));
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = eem_log_phy_addr;
	eem_data.u.data.arg[1] = eem_log_size;

	memcpy(&(eemsn_log->efuse_devinfo), &eem_devinfo,
		sizeof(struct eemsn_devinfo));
	eemsn_log->segCode = read_efuse_by_offset(DEVINFO_SEG_IDX)
			& 0xFF;

#if SUPPORT_PICACHU
	get_picachu_efuse();
#endif

#if defined(MC50_LOAD)
	/* force set freq table */
	memcpy(eemsn_log->vf_tbl_det,
		mc50_tbl, sizeof(eemsn_log->vf_tbl_det));
#endif

	/* get original volt from cpu dvfs before init01 */
	for_each_det(det) {

		get_freq_table_cpu(det);
		memcpy(eemsn_log->det_log[det->det_id].freq_tbl,
			det->freq_tbl, sizeof(det->freq_tbl));

		eemsn_log->det_log[det->det_id].num_freq_tbl =
			det->num_freq_tbl;

		get_orig_volt_table_cpu(det);
		memcpy(eemsn_log->det_log[det->det_id].volt_tbl_orig,
			det->volt_tbl_orig, sizeof(det->volt_tbl_orig));
		eemsn_log->det_log[det->det_id].features =
				(unsigned char)det->features;
	}

#if IS_ENABLED(CONFIG_ARM64) && IS_ENABLED(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)
	if (strstr(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES,
						"aging") != NULL) {
		eem_error("@%s: AGING flavor name: %s\n",
			__func__, CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
		ctrl_agingload_enable = 1;
	}
#endif

	eemsn_log->ctrl_aging_Enable = ctrl_agingload_enable;
	eem_to_cputoeb(IPI_EEMSN_SHARERAM_INIT, &eem_data);
#else
	return 0;
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	eem_debug("AP:eem_log_size:%d, eemsn_log:%lu\n",
		eem_data.u.data.arg[1], sizeof(struct eemsn_log));
#endif
	eem_debug("AP:%d, %d, %d, %d, %d\n",
	sizeof(struct eemsn_log_det),
	sizeof(struct sn_log_data),
	sizeof(struct sn_log_cal_data),
	sizeof(struct sn_param),
	sizeof(struct eemsn_devinfo));

	if (eem_checkEfuse == 0) {
		eem_error("eem_checkEfuse = 0\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

#if ENABLE_INIT_TIMER
	/* init timer for log / volt */
	hrtimer_init(&eem_init_check_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	eem_init_check_timer.function = eem_init_check_timer_func;
	INIT_WORK(&eem_work, eem_post_work);
#endif
	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&eem_driver);

	if (err) {
		eem_debug("EEM driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	return 0;
}

static void __exit eem_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	eem_debug("eem de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

#endif /* EN_EEM */

#if IS_BUILTIN(CONFIG_MTK_PTPOD_LEGACY)
late_initcall(eem_init);
#else
module_init(eem_init);
#endif
module_exit(eem_exit);


MODULE_DESCRIPTION("MediaTek EEM Driver v0.3");
MODULE_LICENSE("GPL");

#undef __MTK_EEM_C__
