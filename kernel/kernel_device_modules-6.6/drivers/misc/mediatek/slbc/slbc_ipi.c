// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/scmi_protocol.h>
#include <linux/module.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
#include <tinysys-scmi.h>
#endif /* CONFIG_MTK_TINYSYS_SCMI */

#include "slbc_ipi.h"
#include "slbc_ops.h"
#include "slbc.h"
#include <mtk_slbc_sram.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static int slbc_sspm_ready;
static int scmi_slbc_id;
static struct scmi_tinysys_info_st *_tinfo;
static unsigned int scmi_id;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
static struct slbc_ipi_ops *ipi_ops;
static DEFINE_MUTEX(slbc_scmi_lock);

int slbc_sspm_slb_disable(int disable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLB_DISABLE;
	slbc_ipi_d.arg1 = disable;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_sspm_slb_disable);

int slbc_sspm_slc_disable(int disable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	pr_info("#@# %s(%d) disable %d\n", __func__, __LINE__, disable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLC_DISABLE;
	slbc_ipi_d.arg1 = disable;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_sspm_slc_disable);

int slbc_sspm_enable(int enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	pr_info("#@# %s(%d) enable %d\n", __func__, __LINE__, enable);

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_ENABLE;
	slbc_ipi_d.arg1 = enable;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_sspm_enable);

int slbc_force_scmi_cmd(unsigned int force)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_FORCE;
	slbc_ipi_d.arg1 = force;

	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_force_scmi_cmd);

int slbc_mic_num_cmd(unsigned int num)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_MIC_NUM;
	slbc_ipi_d.arg1 = num;

	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_mic_num_cmd);

int slbc_inner_cmd(unsigned int inner)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_INNER;
	slbc_ipi_d.arg1 = inner;

	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_inner_cmd);

int slbc_outer_cmd(unsigned int outer)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_OUTER;
	slbc_ipi_d.arg1 = outer;

	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_outer_cmd);

int slbc_suspend_resume_notify(int suspend)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_SUSPEND_RESUME_NOTIFY;
	slbc_ipi_d.arg1 = suspend;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_suspend_resume_notify);

int slbc_get_sspm_ver(u32 *major_ver, u32 *minor_ver, u32 *patch_ver)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	u32 ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_SSPM_VER;
	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	*major_ver = rvalue.slbc_resv1;
	*minor_ver = rvalue.slbc_resv2;
	*patch_ver = rvalue.slbc_resv3;

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_get_sspm_ver);

int slbc_table_gid_set(int gid, int quota, int pri)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_SET;
	slbc_ipi_d.arg1 = gid;
	slbc_ipi_d.arg2 = pri;
	slbc_ipi_d.arg3 = quota;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_set);

int slbc_table_gid_release(int gid)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_RELEASE;
	slbc_ipi_d.arg1 = gid;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_release);

int slbc_table_gid_get(int gid)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_GET;
	slbc_ipi_d.arg1 = gid;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_get);

int slbc_table_idt_set(int index, int arid, int idt)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_SET;
	slbc_ipi_d.arg1 = index;
	slbc_ipi_d.arg2 = arid;
	slbc_ipi_d.arg3 = idt;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_idt_set);

int slbc_table_idt_release(int index)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_RELEASE;
	slbc_ipi_d.arg1 = index;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_idt_release);

int slbc_table_idt_get(int index)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_IDT_GET;
	slbc_ipi_d.arg1 = index;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_idt_get);

int slbc_table_gid_axi_set(int index, int axiid, int pg)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_SET;
	slbc_ipi_d.arg1 = index;
	slbc_ipi_d.arg2 = axiid;
	slbc_ipi_d.arg3 = pg;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_set);

int slbc_table_gid_axi_release(int index)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_RELEASE;
	slbc_ipi_d.arg1 = index;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_release);

int slbc_table_gid_axi_get(int index)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_TABLE_GID_AXI_GET;
	slbc_ipi_d.arg1 = index;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_table_gid_axi_get);

int emi_slb_select(int argv1, int argv2, int argv3)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_SLB_SELECT;
	slbc_ipi_d.arg1 = argv1;
	slbc_ipi_d.arg2 = argv2;
	slbc_ipi_d.arg3 = argv3;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_slb_select);

int emi_pmu_counter(int idx, int filter0, int bw_lat_sel)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_PMU_COUNTER;
	slbc_ipi_d.arg1 = idx;
	slbc_ipi_d.arg2 = filter0;
	slbc_ipi_d.arg3 = bw_lat_sel;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_pmu_counter);

int emi_pmu_set_ctrl(int feature, int idx, int action)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_PMU_SET_CTRL;
	slbc_ipi_d.arg1 = feature;
	slbc_ipi_d.arg2 = idx;
	slbc_ipi_d.arg3 = action;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_pmu_set_ctrl);

int emi_pmu_read_counter(int idx)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_PMU_READ_COUNTER;
	slbc_ipi_d.arg1 = idx;

	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return  -1;
	}

	return rvalue.slbc_resv1;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_pmu_read_counter);

int emi_gid_pmu_counter(int idx, int set)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_GID_PMU_COUNTER;
	slbc_ipi_d.arg1 = idx;
	slbc_ipi_d.arg2 = set;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_gid_pmu_counter);

int emi_gid_pmu_read_counter(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	struct slbc_data *d = (struct slbc_data *)ptr;
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_GID_PMU_READ_COUNTER;
	slbc_ipi_d.arg1 = d->uid;

	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return -1;
	}

	d->type = rvalue.slbc_resv1;
	d->flag = rvalue.slbc_resv2;
	d->timeout = rvalue.slbc_resv3;

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_gid_pmu_read_counter);

int emi_slc_test_result(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_EMI_SLC_TEST_RESULT;

	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
			__func__, __LINE__, ret);
		return  -1;
	}

	return rvalue.slbc_resv1;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(emi_slc_test_result);

int slbc_ctrl_scmi_info(unsigned int cmd, unsigned int arg1,
		unsigned int arg2, unsigned int arg3, unsigned int arg4, void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	int ret = 0;
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status *rvalue = (struct scmi_tinysys_slbc_ctrl_status *)ptr;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = cmd;
	slbc_ipi_d.arg1 = arg1;
	slbc_ipi_d.arg2 = arg2;
	slbc_ipi_d.arg3 = arg3;
	slbc_ipi_d.arg4 = arg4;

	ret = slbc_scmi_ctrl(&slbc_ipi_d, rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_ctrl_scmi_info);

int _slbc_request_cache_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_CACHE_REQUEST_FROM_AP;
	slbc_ipi_d.arg1 = d->uid;
	slbc_ipi_d.arg2 = d->type;
	slbc_ipi_d.arg3 = d->flag;
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.slbc_resv1;
			d->size = rvalue.slbc_resv2;
			ret = d->ret = rvalue.slbc_resv3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_request_cache_scmi);

int _slbc_release_cache_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_CACHE_RELEASE_FROM_AP;
	slbc_ipi_d.arg1 = d->uid;
	slbc_ipi_d.arg2 = d->type;
	slbc_ipi_d.arg3 = d->flag;
	if (d->type == TP_CACHE) {
		ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
		if (!ret) {
			d->paddr = (void __iomem *)(long long)rvalue.slbc_resv1;
			d->size = rvalue.slbc_resv2;
			ret = d->ret = rvalue.slbc_resv3;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_release_cache_scmi);

int _slbc_buffer_status_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_BUFFER_STATUS;
	slbc_ipi_d.arg1 = d->uid;
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
		if (!ret) {
			ret = rvalue.slbc_resv1;
			pr_info("#@# %s(%d) uid %d return ref(%d)\n",
					__func__, __LINE__, d->uid, ret);
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_buffer_status_scmi);

int _slbc_request_buffer_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_BUFFER_REQUEST_FROM_AP;
	slbc_ipi_d.arg1 = d->uid;
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
		if (!ret) {
			//pr_info("slbc request: paddr: %x, size: %d, ref: %d, empty: %d, ret: %d\n",
			//rvalue.slbc_resv1, rvalue.slbc_resv2, rvalue.slbc_resv3, rvalue.slbc_resv4, rvalue.ret);
			d->paddr = (void __iomem *)(long long)rvalue.slbc_resv1;
			d->size = rvalue.slbc_resv2;
			d->ref = rvalue.slbc_resv3;
			ret = d->ret = rvalue.ret;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_request_buffer_scmi);

int _slbc_release_buffer_scmi(void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct slbc_data *d = (struct slbc_data *)ptr;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_BUFFER_RELEASE_FROM_AP;
	slbc_ipi_d.arg1 = d->uid;
	if (d->type == TP_BUFFER) {
		ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
		if (!ret) {
			//pr_info("slbc release: paddr: %x, size: %d, ref: %d, empty: %d, ret: %d\n",
			//rvalue.slbc_resv1, rvalue.slbc_resv2, rvalue.slbc_resv3, rvalue.slbc_resv4, rvalue.ret);
			d->paddr = (void __iomem *)(long long)rvalue.slbc_resv1;
			d->size = rvalue.slbc_resv2;
			d->ref = rvalue.slbc_resv3;
			ret = d->ret = rvalue.ret;
		} else {
			pr_info("#@# %s(%d) return fail(%d)\n",
					__func__, __LINE__, ret);
			ret = -1;
		}
	} else {
		pr_info("#@# %s(%d) wrong type(0x%x)\n",
				__func__, __LINE__, d->type);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_release_buffer_scmi);

int _slbc_ach_scmi(unsigned int cmd, enum slc_ach_uid uid, int gid, struct slbc_gid_data *data)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd		= cmd;
	slbc_ipi_d.arg1		= uid;
	slbc_ipi_d.arg2		= gid;
	if (cmd == IPI_SLBC_GID_REQUEST_FROM_AP) {
		slbc_ipi_d.arg3 = data->flag;
	} else if (cmd == IPI_SLBC_ROI_UPDATE_FROM_AP) {
		slbc_ipi_d.arg3		= data->bw;
		slbc_ipi_d.arg4		= data->dma_size;
	} else if (cmd == IPI_SLBC_GID_READ_INVALID_FROM_AP) {
		slbc_ipi_d.arg3		= data->bw;	// re-use bw as enable argument
	}

	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_ach_scmi);

int _slbc_sspm_shared_dram_scmi(unsigned int phys_addr, unsigned int mem_size)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};
	int ret = 0;

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));

	slbc_ipi_d.cmd = IPI_SLBC_SETUP_SSPM_SHARED_DRAM;
	slbc_ipi_d.arg1 = phys_addr;
	slbc_ipi_d.arg2 = mem_size;

	ret = slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
	if (ret) {
		pr_info("#@# %s(%d) return fail(%d)\n",
				__func__, __LINE__, ret);
		ret = -1;
	}

	return ret;
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(_slbc_sspm_shared_dram_scmi);

int slbc_sspm_sram_update(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	struct slbc_ipi_data slbc_ipi_d;
	struct scmi_tinysys_slbc_ctrl_status rvalue = {0};

	memset(&slbc_ipi_d, 0, sizeof(slbc_ipi_d));
	slbc_ipi_d.cmd = IPI_SLBC_SRAM_UPDATE;
	slbc_ipi_d.arg1 = 0;
	return slbc_scmi_ctrl(&slbc_ipi_d, &rvalue);
#else
	return 0;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}
EXPORT_SYMBOL_GPL(slbc_sspm_sram_update);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
static void slbc_scmi_handler(u32 r_feature_id, scmi_tinysys_report *report)
{
	struct slbc_data d;
	unsigned int cmd;
	unsigned int arg;
	unsigned int arg2;
	unsigned int arg3;

	if (scmi_slbc_id != r_feature_id)
		return;

	cmd = report->p1;
	arg = report->p2;
	arg2 = report->p3;
	arg3 = report->p4;
	/* pr_info("#@# %s(%d) report 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__, */
			/* report->p1, report->p2, report->p3, report->p4); */

	switch (cmd) {
	case IPI_SLBC_SYNC_TO_AP:
		break;
	case IPI_SLBC_ACP_REQUEST_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_request_acp)
				ipi_ops->slbc_request_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_REQUEST_TO_AP",
					d.type);
		break;
	case IPI_SLBC_ACP_RELEASE_TO_AP:
		ui_to_slbc_data(&d, arg);
		if (d.type == TP_ACP) {
			if (ipi_ops && ipi_ops->slbc_release_acp)
				ipi_ops->slbc_release_acp(&d);
		} else
			pr_info("#@# %s(%d) wrong cmd(%s) and type(0x%x)\n",
					__func__, __LINE__,
					"IPI_SLBC_ACP_RELEASE_TO_AP",
					d.type);
		break;
	case IPI_SLBC_MEM_BARRIER:
		if (ipi_ops && ipi_ops->slbc_mem_barrier)
			ipi_ops->slbc_mem_barrier();
		break;
	case IPI_SLBC_BUFFER_CB_NOTIFY:
		if (ipi_ops && ipi_ops->slbc_buffer_cb_notify)
			ipi_ops->slbc_buffer_cb_notify(arg, arg2, arg3);
		break;
	case IPI_SLBC_DCC_CTRL_TO_AP:
		if (ipi_ops && ipi_ops->slbc_dcc_ctrl)
			ipi_ops->slbc_dcc_ctrl(arg);
		break;
	default:
		pr_info("wrong slbc IPI command: %d\n",
				cmd);
	}
}
#endif /* CONFIG_MTK_TINYSYS_SCMI */

int slbc_scmi_ctrl(void *buffer, void *ptr)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	int ret;
	unsigned int local_id;
	struct slbc_ipi_data *slbc_ipi_d = buffer;
	struct scmi_tinysys_slbc_ctrl_status *rvalue = ptr;

	if (slbc_sspm_ready != 1) {
		ret = -1;
		pr_info("slbc scmi not ready, skip cmd=%d\n", slbc_ipi_d->cmd);
		goto error;
	}

	/* pr_info("#@# %s(%d) id 0x%x cmd 0x%x arg 0x%x\n", */
			/* __func__, __LINE__, */
			/* scmi_slbc_id, slbc_ipi_d->cmd, slbc_ipi_d.arg1); */

	mutex_lock(&slbc_scmi_lock);

	local_id = ++scmi_id;
	slbc_sram_write(SLBC_SCMI_AP, local_id);

	ret = scmi_tinysys_slbc_ctrl(_tinfo->ph, slbc_ipi_d->cmd, slbc_ipi_d->arg1,
		slbc_ipi_d->arg2, slbc_ipi_d->arg3, slbc_ipi_d->arg4, rvalue);

	/* scmi timeout WA */
	if (ret == -ETIMEDOUT) {
		mdelay(3);
		if (local_id == slbc_sram_read(SLBC_SCMI_SSPM)) {
			ret = 0;
			rvalue->slbc_resv1 = slbc_sram_read(SLBC_SCMI_RET1);
			rvalue->slbc_resv2 = slbc_sram_read(SLBC_SCMI_RET2);
			rvalue->slbc_resv3 = slbc_sram_read(SLBC_SCMI_RET3);
			rvalue->slbc_resv4 = slbc_sram_read(SLBC_SCMI_RET4);
			rvalue->ret = slbc_sram_read(SLBC_SCMI_RET_VAL);
			pr_info("slbc scmi timed out!(id=%u) return 0x%x 0x%x 0x%x 0x%x ret=%u\n",
					local_id,
					rvalue->slbc_resv1,
					rvalue->slbc_resv2,
					rvalue->slbc_resv3,
					rvalue->slbc_resv4,
					rvalue->ret);
		}
	}

	mutex_unlock(&slbc_scmi_lock);

	if (ret) {
		pr_info("slbc scmi cmd %d send fail, ret = %d\n",
				slbc_ipi_d->cmd, ret);

		goto error;
	}

error:
	return ret;
#else
	return -1;
#endif /* CONFIG_MTK_TINYSYS_SCMI */
}

int slbc_scmi_init(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	if (!(_tinfo && _tinfo->sdev)) {
		pr_info("slbc call get_scmi_tinysys_info() fail\n");
		return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi-slbc",
			&scmi_slbc_id);
	if (ret) {
		pr_info("get slbc scmi_slbc fail, ret %d\n", ret);
		slbc_sspm_ready = -2;
		return -EINVAL;
	}
	pr_info("#@# %s(%d) scmi_slbc_id %d\n",
			__func__, __LINE__, scmi_slbc_id);

	scmi_tinysys_register_event_notifier(scmi_slbc_id,
			(f_handler_t)slbc_scmi_handler);

	ret = scmi_tinysys_event_notify(scmi_slbc_id, 1);

	if (ret) {
		pr_info("event notify fail ...");
		return -EINVAL;
	}

	slbc_sspm_ready = 1;

	pr_info("slbc scmi is ready!\n");

#endif /* CONFIG_MTK_TINYSYS_SCMI */
	return 0;
}
EXPORT_SYMBOL_GPL(slbc_scmi_init);

void slbc_register_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = ops;
}
EXPORT_SYMBOL_GPL(slbc_register_ipi_ops);

void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops)
{
	ipi_ops = NULL;
}
EXPORT_SYMBOL_GPL(slbc_unregister_ipi_ops);

MODULE_DESCRIPTION("SLBC scmi Driver v0.1");
MODULE_LICENSE("GPL");
