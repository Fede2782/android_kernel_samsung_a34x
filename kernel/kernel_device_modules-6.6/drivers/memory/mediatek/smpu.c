// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <soc/mediatek/smpu.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/delay.h>

extern int mtk_clear_smpu_log(unsigned int emi_id);
//static struct kthread_worker *smpu_kworker;
struct smpu *global_ssmpu;
EXPORT_SYMBOL_GPL(global_ssmpu);

struct smpu *global_nsmpu;
EXPORT_SYMBOL_GPL(global_nsmpu);

struct smpu *global_skp, *global_nkp;

static void set_regs(struct smpu_reg_info_t *reg_list, unsigned int reg_cnt,
		     void __iomem *smpu_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++) {
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value,
			       smpu_base + reg_list[i].offset + 4 * j);
	}
	/*
	 * Use the memory barrier to make sure the interrupt signal is
	 * de-asserted (by programming registers) before exiting the
	 * ISR and re-enabling the interrupt.
	 */
	mb();
}
static void clear_violation(struct smpu *mpu)
{
	void __iomem *mpu_base;
	//struct arm_smccc_res smc_res;

	mpu_base = mpu->mpu_base;

	set_regs(mpu->clear_reg, mpu->clear_cnt, mpu_base);
	//	pr_info("smpu clear vio done\n");
}

static void mask_irq(struct smpu *mpu)
{
	void __iomem *mpu_base;

	mpu_base = mpu->mpu_base;
	set_regs(mpu->mask_reg, mpu->mask_cnt, mpu_base);
}

static void clear_kp_violation(unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_KP, emi_id, 0, 0,
		      0, 0, 0, &smc_res);
}

char *smpu_clear_md_violation(void)
{
	struct smpu *smpu = { 0 };
	struct smpu_reg_info_t *dump_reg;
	void __iomem *mpu_base;
	bool flag = false; /* make sure md vio rg exists or not *after 6897*/
	struct arm_smccc_res smc_res;
	ssize_t msg_len = 0;
	int i;
	unsigned int parser_shift = 0x40;
	char *ret0 = "fail", *ret1 = "clear_md_vio", *ret2 = "get_md_vio_fail";

	/*
	 * get the violation log after 6897
	 */

	if ((global_nsmpu && (global_nsmpu->dump_md_cnt > 0)) ||
	    (global_ssmpu && (global_ssmpu->dump_md_cnt > 0))) {
		/*
		 * check violation from the North/South MPU
		 */
		if (!global_nsmpu)
			return ret0;

		smpu = global_nsmpu;
		mpu_base = smpu->mpu_base;
		dump_reg = smpu->dump_md_reg;
		smpu = (readl(mpu_base + dump_reg[0].offset) >
			0x2) || (readl(mpu_base + dump_reg[9].offset) > 0x2) ?
			       global_nsmpu :
			       global_ssmpu;
		mpu_base = smpu->mpu_base;
		dump_reg = smpu->dump_md_reg;

		/*
		 * Adding md register for violation info
		 */
		for (i = 0; i < smpu->dump_md_cnt; i++)
			dump_reg[i].value =
				readl(mpu_base + dump_reg[i].offset);

		if (msg_len < MTK_SMPU_MAX_CMD_LEN) {
			msg_len += scnprintf(smpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\n[SMPU]%s\n", smpu->name);
		}
		for (i = 0; i < smpu->dump_md_cnt; i++) {
			if (msg_len < MTK_SMPU_MAX_CMD_LEN)
				msg_len += scnprintf(
					smpu->vio_msg + msg_len,
					MTK_SMPU_MAX_CMD_LEN - msg_len,
					"[%x]%x;",
					dump_reg[i].offset - parser_shift,
					dump_reg[i].value);
		}

		if ((dump_reg[0].value > 0x2) || (dump_reg[9].value > 0x2)) {
			ret2 = smpu->vio_msg;
			pr_info("%s: %s", __func__, smpu->vio_msg);
		}
	}

	/*
	 *  clear the violation log after 6897
	 */
	if (global_nsmpu) {
		smpu = global_nsmpu;
		mpu_base = smpu->mpu_base;
		if (smpu->clear_md_reg) {
			set_regs(smpu->clear_md_reg, smpu->clear_md_cnt,
				 mpu_base);
			flag = true;
		}
	}
	if (global_ssmpu) {
		smpu = global_ssmpu;
		mpu_base = smpu->mpu_base;
		if (smpu->clear_md_reg)
			set_regs(smpu->clear_md_reg, smpu->clear_md_cnt,
				 mpu_base);
	}

	/*
	 *  clear the violation log before 6897
	 */
	if (!flag) {
		pr_info("smpu_clear_md_vio enter\n");
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_MD, 0, 0,
			      0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear md violation, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
			return ret0;
		}
		return ret1;
	}

	return ret2;
}
EXPORT_SYMBOL(smpu_clear_md_violation);

int mtk_smpu_isr_hook_register(smpu_isr_hook hook)
{
	struct smpu *ssmpu, *nsmpu, *skp, *nkp;

	ssmpu = global_ssmpu;
	nsmpu = global_nsmpu;
	skp = global_skp;
	nkp = global_nkp;

	if (!nsmpu || !nkp)
		return -EINVAL;

	pr_info("%s:hook-register half", __func__);

	if (!hook) {
		pr_info("%s: hook is NULL\n", __func__);
		return -EINVAL;
	}

	nsmpu->by_plat_isr_hook = hook;
	nkp->by_plat_isr_hook = hook;

	if (ssmpu && skp) {
		ssmpu->by_plat_isr_hook = hook;
		skp->by_plat_isr_hook = hook;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_smpu_isr_hook_register);

int mtk_smpu_md_handling_register(smpu_md_handler md_handling_func)
{
	struct smpu *ssmpu, *nsmpu, *skp, *nkp;

	ssmpu = global_ssmpu;
	nsmpu = global_nsmpu;
	skp = global_skp;
	nkp = global_nkp;

	if (!nsmpu || !nkp)
		return -EINVAL;

	if (!md_handling_func) {
		pr_info("%s: md_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	nsmpu->md_handler = md_handling_func;
	nkp->md_handler = md_handling_func;

	if (ssmpu && skp) {
		ssmpu->md_handler = md_handling_func;
		skp->md_handler = md_handling_func;
	}

	pr_info("%s:md_handling_func registered!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_smpu_md_handling_register);

static irqreturn_t smpu_violation_thread(int irq, void *dev_id)
{
	struct smpu *mpu = (struct smpu *)dev_id;
	struct arm_smccc_res smc_res;
	unsigned int prefetch_mask = 0x200000; //e00/e80's b[21] = 1 -> prefetch

	/*
	 * CPU will cause WCE violation
	 */
	struct device_node *smpu_node = of_find_node_by_name(NULL, "smpu");
	int by_pass_aid[3] = { 240, 241, 243 };
	int by_pass_region[10] = { 22, 28, 39, 41, 44, 45, 57, 59, 61, 62 };
	int i, j, by_pass_flag = 0;
	/*var for WCE violation end*/

	pr_info("%s: %s", __func__, mpu->vio_msg);

	/* check vio region addr */
	if (!(strcmp(mpu->name, "nsmpu")) || !(strcmp(mpu->name, "ssmpu"))) {
		if (mpu->dump_reg[7].value != 0) {
			/*type(0 start_addr, 1 end_addr) region aid_shift*/
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      0, mpu->dump_reg[7].value, 0, 0, 0, 0,
				      &smc_res);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      1, mpu->dump_reg[7].value, 0, 0, 0, 0,
				      &smc_res);
		}
		if (mpu->dump_reg[16].value != 0) {
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      0, mpu->dump_reg[16].value, 0, 0, 0, 0,
				      &smc_res);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      1, mpu->dump_reg[16].value, 0, 0, 0, 0,
				      &smc_res);
		}

		msleep(30);

		for (i = 0; i < 3;
		     i++) { // by pass WCE, this will be temp patch
			for (j = 0; j < 10; j++) {
				if (mpu->dump_reg[5].value == by_pass_aid[i] &&
				    mpu->dump_reg[7].value ==
					    by_pass_region[j]) {
					by_pass_flag++;
					break;
				}
			}
			if (by_pass_flag > 0)
				break;
		}
		//add prefetch mask
		if ((mpu->dump_reg[0].value & prefetch_mask) ||
		    (mpu->dump_reg[9].value & prefetch_mask) ||
		    (mpu->is_prefetch == true)) {
			pr_info("%s:Prefetch without KERNEL_API!!\n", __func__);
		} else if (by_pass_flag >
				   0 && // by pass WCE, this will be temp patch
			   of_property_count_elems_of_size(
				   smpu_node, "bypass-wce", sizeof(char))) {
			pr_info("%s:AID == 0x%x && region = 0x%x without KERNEL_API!!\n",
				__func__, mpu->dump_reg[5].value,
				mpu->dump_reg[7].value);
		} else if (!mpu->is_bypass) // by pass GPU write vio
			aee_kernel_exception("SMPU",
					     mpu->vio_msg); // for smpu_vio case
	} else
		aee_kernel_exception("SMPU", mpu->vio_msg); // for KP case

	/* for chip before 6989/6897 might need to remove the kp clear node in dts */
	clear_violation(mpu);

	mpu->is_bypass = false;
	mpu->is_vio = false;

	return IRQ_HANDLED;
}

static irqreturn_t smpu_violation(int irq, void *dev_id)
{
	struct smpu *mpu = (struct smpu *)dev_id;
	struct smpu_reg_info_t *dump_reg = mpu->dump_reg;
	void __iomem *mpu_base;
	int i, vio_dump_idx, vio_dump_pos, prefetch;
	int vio_type = 6;
	bool violation;
	ssize_t msg_len = 0;
	//	struct task_struct *tsk;

	irqreturn_t irqret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	violation = false;
	mpu_base = mpu->mpu_base;

	if (!(strcmp(mpu->name, "nsmpu")))
		vio_type = VIO_TYPE_NSMPU;
	else if (!(strcmp(mpu->name, "ssmpu")))
		vio_type = VIO_TYPE_SSMPU;
	else if (!(strcmp(mpu->name, "nkp")))
		vio_type = VIO_TYPE_NKP;
	else if (!(strcmp(mpu->name, "skp")))
		vio_type = VIO_TYPE_SKP;
	else
		goto clear_violation;

	//	pr_info("%s:vio_type = %d\n", __func__, vio_type);
	//record dump reg
	for (i = 0; i < mpu->dump_cnt; i++)
		dump_reg[i].value = readl(mpu_base + dump_reg[i].offset);

	//record vioinfo
	for (i = 0; i < mpu->vio_dump_cnt; i++) {
		vio_dump_idx = mpu->vio_reg_info[i].vio_dump_idx;
		vio_dump_pos = mpu->vio_reg_info[i].vio_dump_pos;
		if (CHECK_BIT(dump_reg[vio_dump_idx].value, vio_dump_pos)) {
			violation = true;
			mpu->is_vio = true;
		}
	}

	if (!violation) {
		if (__ratelimit(&ratelimit))
			pr_info("smpu no violation");
		clear_violation(mpu);
		return IRQ_HANDLED;
	}

	if (violation) {
		if (vio_type == VIO_TYPE_NSMPU || vio_type == VIO_TYPE_SSMPU) {
			//smpu violation
			if (mpu->by_plat_isr_hook) { // move it to bottom half?
				irqret = mpu->by_plat_isr_hook(
					dump_reg, mpu->dump_cnt, vio_type);

				if (irqret == IRQ_HANDLED) {
					violation = true;
					mpu->is_vio = true;
					mpu->is_bypass = true;
					goto clear_violation;
				}
			}
		}

		if (msg_len < MTK_SMPU_MAX_CMD_LEN) {
			prefetch = mtk_clear_smpu_log(vio_type % 2);
			mpu->is_prefetch = prefetch == 1 ? true : false;
			msg_len += scnprintf(mpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\ncpu-prefetch:%d", prefetch);
			msg_len += scnprintf(mpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\n[SMPU]%s\n", mpu->name);
		}

		for (i = 0; i < mpu->dump_cnt; i++) {
			if (msg_len < MTK_SMPU_MAX_CMD_LEN)
				msg_len += scnprintf(
					mpu->vio_msg + msg_len,
					MTK_SMPU_MAX_CMD_LEN - msg_len,
					"[%x]%x;", dump_reg[i].offset,
					dump_reg[i].value);
		}
	}

clear_violation:
	mask_irq(mpu);
	/*for chip before 6989/6897 */
	if (vio_type == VIO_TYPE_NKP || vio_type == VIO_TYPE_SKP)
		clear_kp_violation(vio_type % 2);

	/* if there is violation happened, wake up the thread */
	if (violation)
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static const struct of_device_id smpu_of_ids[] = {
	{
		.compatible = "mediatek,smpu",
	},
	{}
};
MODULE_DEVICE_TABLE(of, smpu_of_ids);

/* As SLC b mode enable CPU will write clean evict, which may trigger SMPU violation.
 * and those data may cached in CPU L3 cache through CPU prefetch.
 */
static void smpu_clean_cpu_write_vio(struct smpu *mpu)
{
	int sec_cpu_aid = 240;
	int ns_cpu_aid = 241;
	int hyp_cpu_aid = 243;
	/* for new SLC work around*/
	int WCE_bypass[2] = { 244, 245 };
	bool WCE_flag = false;
	bool slc_enable = mpu->slc_b_mode;
	int i;
	void __iomem *mpu_base = mpu->mpu_base;
	struct smpu_reg_info_t *dump_reg = mpu->dump_reg;
	int vio_type = 6, prefetch = 0;
	ssize_t msg_len = 0;

	/* smpu check violation */
	if (slc_enable) {
		if (!(strcmp(mpu->name, "nsmpu")))
			vio_type = VIO_TYPE_NSMPU;
		else if (!(strcmp(mpu->name, "ssmpu")))
			vio_type = VIO_TYPE_SSMPU;
		else if (!(strcmp(mpu->name, "nkp")))
			vio_type = VIO_TYPE_NKP;
		else if (!(strcmp(mpu->name, "skp")))
			vio_type = VIO_TYPE_SKP;
		/* read SMPU/KP vio reg */
		for (i = 0; i < mpu->dump_cnt; i++)
			dump_reg[i].value =
				readl(mpu_base + dump_reg[i].offset);

		/* check whether cpu type master lead this smpu violation */
		if (!(strcmp(mpu->name, "nsmpu")) ||
		    !(strcmp(mpu->name, "ssmpu"))) {
			for (i = 0; i < 2; i++)
				if (mpu->dump_reg[5].value == WCE_bypass[i])
					WCE_flag = true;
			/* check smpu write violation aid reg */
			if ((mpu->dump_reg[5].value == sec_cpu_aid) ||
			    (mpu->dump_reg[5].value == ns_cpu_aid) ||
			    (mpu->dump_reg[5].value == hyp_cpu_aid) ||
			    (WCE_flag == true)) {
				if (msg_len < MTK_SMPU_MAX_CMD_LEN) {
					prefetch = mtk_clear_smpu_log(vio_type %
								      2);
					mpu->is_prefetch =
						prefetch == 1 ? true : false;
					msg_len += scnprintf(
						mpu->vio_msg + msg_len,
						MTK_SMPU_MAX_CMD_LEN - msg_len,
						"\ncpu-prefetch:%d", prefetch);
					msg_len += scnprintf(
						mpu->vio_msg + msg_len,
						MTK_SMPU_MAX_CMD_LEN - msg_len,
						"\n[SMPU]%s\n", mpu->name);
				}
				for (i = 0; i < mpu->dump_cnt; i++) {
					if (msg_len < MTK_SMPU_MAX_CMD_LEN)
						msg_len += scnprintf(
							mpu->vio_msg + msg_len,
							MTK_SMPU_MAX_CMD_LEN -
								msg_len,
							"[%x]%x;",
							dump_reg[i].offset,
							dump_reg[i].value);
				}
				pr_info("%s: %s", __func__, mpu->vio_msg);
				clear_violation(mpu);
			}
		} else {
			/* check kp write violation aid reg */
			if ((MTK_SMPU_KP_AID(mpu->dump_reg[1].value) ==
			     sec_cpu_aid) ||
			    (MTK_SMPU_KP_AID(mpu->dump_reg[1].value) ==
			     ns_cpu_aid) ||
			    (MTK_SMPU_KP_AID(mpu->dump_reg[1].value) ==
			     hyp_cpu_aid)) {
				pr_info("%s: %s", __func__, mpu->vio_msg);
				clear_violation(mpu);
			}
		}
		pr_info("%s: WCE dump finish!!\n", __func__);
	}
}

static int smpu_probe(struct platform_device *pdev)
{
	struct device_node *smpu_node = pdev->dev.of_node;
	struct smpu *mpu;
	const char *name = NULL;

	int ret, i, size, axi_set_num;
	unsigned int *dump_list, *miumpu_bypass_list, *gpu_bypass_list;
	//	struct resource *res;

	dev_info(&pdev->dev, "driver probe");
	if (!smpu_node) {
		dev_err(&pdev->dev, "No smpu-reg");
		return -ENXIO;
	}

	mpu = devm_kzalloc(&pdev->dev, sizeof(struct smpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;

	if (!of_property_read_string(smpu_node, "name", &name))
		mpu->name = name;
	//is_vio default value
	mpu->is_vio = false;
	mpu->is_bypass = false;

	// dump_reg
	size = of_property_count_elems_of_size(smpu_node, "dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No smpu node dump\n");
		return -ENXIO;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENXIO;

	size >>= 2;
	mpu->dump_cnt = size;
	ret = of_property_read_u32_array(smpu_node, "dump", dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "no smpu dump\n");
		return -ENXIO;
	}

	mpu->dump_reg = devm_kmalloc(
		&pdev->dev, size * sizeof(struct smpu_reg_info_t), GFP_KERNEL);
	if (!(mpu->dump_reg))
		return -ENOMEM;

	for (i = 0; i < mpu->dump_cnt; i++) {
		mpu->dump_reg[i].offset = dump_list[i];
		mpu->dump_reg[i].value = 0;
		mpu->dump_reg[i].leng = 0;
	}
	//dump_reg end
	//dump_clear
	size = of_property_count_elems_of_size(smpu_node, "clear",
					       sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear smpu");
		return -ENXIO;
	}
	mpu->clear_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;

	mpu->clear_cnt = size / sizeof(struct smpu_reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(
		smpu_node, "clear", (unsigned int *)(mpu->clear_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear reg");
		return -ENXIO;
	}
	//dump_clear end
	//dump_clear_md
	size = of_property_count_elems_of_size(smpu_node, "clear-md",
					       sizeof(char));
	if (size <= 0)
		dev_err(&pdev->dev, "No clear_md smpu");
	if (size > 0) {
		mpu->clear_md_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!(mpu->clear_md_reg))
			return -ENOMEM;

		mpu->clear_md_cnt = size / sizeof(struct smpu_reg_info_t);
		size >>= 2;
		ret = of_property_read_u32_array(
			smpu_node, "clear-md",
			(unsigned int *)(mpu->clear_md_reg), size);
		if (ret) {
			dev_err(&pdev->dev, "No clear-md reg");
			return -ENXIO;
		}
	}

	//dump_clear_md_end
	//dump vio info
	size = of_property_count_elems_of_size(smpu_node, "vio-info",
					       sizeof(char));

	mpu->vio_dump_cnt = size / sizeof(struct smpu_vio_dump_info_t);
	mpu->vio_reg_info = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->vio_reg_info))
		return -ENOMEM;
	size >>= 2;
	ret = of_property_read_u32_array(smpu_node, "vio-info",
					 (unsigned int *)(mpu->vio_reg_info),
					 size);
	if (ret)
		return -ENXIO;
	//dump vio-info end
	size = of_property_count_elems_of_size(smpu_node, "mask", sizeof(char));
	if (size <= 0) {
		pr_info("No clear smpu\n");
		return -ENXIO;
	}
	mpu->mask_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;

	mpu->mask_cnt = size / sizeof(struct smpu_reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(smpu_node, "mask",
					 (unsigned int *)(mpu->mask_reg), size);
	if (ret) {
		pr_info("No mask reg\n");
		return -ENXIO;
	}

	//only for smpu node
	if ((!(strcmp(mpu->name, "ssmpu"))) ||
	    (!(strcmp(mpu->name, "nsmpu")))) {
		/*
		 *  get the md violation register content.
		 */
		size = of_property_count_elems_of_size(smpu_node, "dump-md",
						       sizeof(char));
		if (size <= 0) {
			pr_debug("No smpu node dump-md\n");
			mpu->dump_md_cnt = 0;
		} else {
			dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
			if (!dump_list)
				return -ENXIO;

			size >>= 2;
			mpu->dump_md_cnt = size;
			ret = of_property_read_u32_array(smpu_node, "dump-md",
							 dump_list, size);
			if (ret) {
				pr_debug("no smpu dump-md\n");
				return -ENXIO;
			}

			mpu->dump_md_reg = devm_kmalloc(
				&pdev->dev,
				size * sizeof(struct smpu_reg_info_t),
				GFP_KERNEL);
			if (!(mpu->dump_md_reg))
				return -ENOMEM;

			for (i = 0; i < mpu->dump_cnt; i++) {
				mpu->dump_md_reg[i].offset = dump_list[i];
				mpu->dump_md_reg[i].value = 0;
				mpu->dump_md_reg[i].leng = 0;
			}
		}
		/* get md reg content end*/
		//bypass_axi
		size = of_property_count_elems_of_size(smpu_node, "bypass-axi",
						       sizeof(char));
		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		axi_set_num = AXI_SET_NUM(size);
		mpu->bypass_axi_num = axi_set_num;
		ret = of_property_read_u32_array(smpu_node, "bypass-axi",
						 miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_axi = devm_kmalloc(
			&pdev->dev,
			axi_set_num * sizeof(struct bypass_axi_info_t),
			GFP_KERNEL);
		if (!(mpu->bypass_axi))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_axi_num; i++) {
			mpu->bypass_axi[i].port =
				miumpu_bypass_list[(i * 3) + 0];
			mpu->bypass_axi[i].axi_mask =
				miumpu_bypass_list[(i * 3) + 1];
			mpu->bypass_axi[i].axi_value =
				miumpu_bypass_list[(i * 3) + 2];
		}

		//bypass_axi end
		//bypass miumpu start
		size = of_property_count_elems_of_size(smpu_node, "bypass",
						       sizeof(char));

		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		mpu->bypass_miu_reg_num = size;
		ret = of_property_read_u32_array(smpu_node, "bypass",
						 miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_miu_reg = devm_kmalloc(
			&pdev->dev, size * sizeof(unsigned int), GFP_KERNEL);
		if (!(mpu->bypass_miu_reg))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_miu_reg_num; i++)
			mpu->bypass_miu_reg[i] = miumpu_bypass_list[i];

		size = of_property_count_elems_of_size(smpu_node, "bypass-gpu",
						       sizeof(char));
		if (size > 0) {
			gpu_bypass_list =
				devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
			if (!gpu_bypass_list)
				return -ENOMEM;

			size >>= 2;
			ret = of_property_read_u32_array(
				smpu_node, "bypass-gpu", gpu_bypass_list, size);
			if (!gpu_bypass_list) {
				pr_info("no gpu-bypass\n");
				return -ENXIO;
			}

			mpu->gpu_bypass_list =
				devm_kmalloc(&pdev->dev,
					     size * sizeof(unsigned int),
					     GFP_KERNEL);

			if (!mpu->gpu_bypass_list)
				return -ENOMEM;

			for (i = 0; i < 2; i++)
				mpu->gpu_bypass_list[i] = gpu_bypass_list[i];
		}
		//bypass_miumpu end

	} //only for smpu end

	//reg base
	mpu->mpu_base = of_iomap(smpu_node, 0);
	if (IS_ERR(mpu->mpu_base)) {
		dev_err(&pdev->dev, "Failed to map smpu range base");
		return -EIO;
	}
	//reg base end

	mpu->vio_msg =
		devm_kmalloc(&pdev->dev, MTK_SMPU_MAX_CMD_LEN, GFP_KERNEL);
	if (!(mpu->vio_msg))
		return -ENOMEM;

	mpu->vio_msg_gpu =
		devm_kmalloc(&pdev->dev, MAX_GPU_VIO_LEN, GFP_KERNEL);
	if (!mpu->vio_msg_gpu)
		return -ENOMEM;

	//transt global

	if (!strcmp(mpu->name, "ssmpu"))
		global_ssmpu = mpu;
	if (!strcmp(mpu->name, "nsmpu"))
		global_nsmpu = mpu;
	if (!strcmp(mpu->name, "skp"))
		global_skp = mpu;
	if (!strcmp(mpu->name, "nkp"))
		global_nkp = mpu;

	if (of_property_read_bool(smpu_node, "mediatek,slc-b-mode"))
		mpu->slc_b_mode = true;

	smpu_clean_cpu_write_vio(mpu);

	mpu->irq = irq_of_parse_and_map(smpu_node, 0);
	if (mpu->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	/*
	 * change it to threaded irq
	 */
	ret = request_threaded_irq(mpu->irq, (irq_handler_t)smpu_violation,
				   (irq_handler_t)smpu_violation_thread,
				   IRQF_ONESHOT, "smpu", mpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		return -EINVAL;
	}

	devm_kfree(&pdev->dev, dump_list);

	return 0;
}
static int smpu_remove(struct platform_device *pdev)
{
	struct smpu *mpu = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(mpu->irq, mpu);

	if (!strcmp(mpu->name, "ssmpu"))
		global_ssmpu = NULL;
	else if (!strcmp(mpu->name, "nsmpu"))
		global_nsmpu = NULL;
	else if (!strcmp(mpu->name, "nkp"))
		global_skp = NULL;
	else if (!strcmp(mpu->name, "skp"))
		global_nkp = NULL;

	return 0;
}

static struct platform_driver smpu_driver = {
	.probe = smpu_probe,
	.remove = smpu_remove,
	.driver = {
		.name = "smpu_driver",
		.owner = THIS_MODULE,
		.of_match_table = smpu_of_ids,
	},
};

static __init int smpu_init(void)
{
	int ret;

	pr_info("smpu was loaded\n");

	ret = platform_driver_register(&smpu_driver);
	if (ret) {
		pr_info("smpu:failed to register driver");
		return ret;
	}

	return 0;
}

module_init(smpu_init);

MODULE_DESCRIPTION("MediaTek SMPU Driver");
MODULE_LICENSE("GPL");
