// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>

#include "mcupm_plt.h"
#include "mcupm_driver.h"
#include "mcupm_ipi_id.h"

/* import from mcupm_driver */
extern int mcupm_plt_ackdata;

#if MCUPM_PLT_SERV_SUPPORT
struct plt_ctrl_s {
	unsigned int magic;
	unsigned int size;
	unsigned int mem_sz;
#if MCUPM_LOGGER_SUPPORT
	unsigned int logger_ofs;
#endif
};
#endif

#define AP_TCM_MPMM_CTRL_ADDR           (0x0C2CF5F8)    /* MPMM control */
#define AP_TCM_MPMM_DISABLE_MAGICNUM    (0xF1F2F3F4)    /* MPMM disable magic number */
#define AP_TCM_MPMM_ENABLE_MAGICNUM     (0xE1E2E3E4)    /* MPMM enable magic number */

static void __iomem *mpmm_en_reg;

#if MCUPM_PLT_SERV_SUPPORT
static ssize_t mcupm_alive_show(struct device *kobj,
				 struct device_attribute *attr, char *buf)
{

	struct mcupm_ipi_data_s ipi_data;
	int ret = 0;

	ipi_data.cmd = 0xDEAD;
	mcupm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_WAIT,
		&ipi_data,
		sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
		2000);

	return snprintf(buf, PAGE_SIZE, "%s RES_MEM(%d) SKIP_LOG(%d) MBOX%d ret=%d\n",
			mcupm_plt_ackdata ? "Alive" : "Dead", has_reserved_memory, skip_logger, CH_S_PLATFORM, ret);
}
DEVICE_ATTR_RO(mcupm_alive);

static ssize_t mpmm_show(struct device *kobj,
				 struct device_attribute *attr, char *buf)
{
	unsigned int mpmm_en_val;

	if (!mpmm_en_reg)
		mpmm_en_reg = ioremap(AP_TCM_MPMM_CTRL_ADDR, 0x4);

	mpmm_en_val = ioread32(mpmm_en_reg);

	return snprintf(buf, PAGE_SIZE, "%X\n", mpmm_en_val);
}

static ssize_t mpmm_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{
	int ret = 0;
	unsigned int mpmm_en_val;

	if (!mpmm_en_reg)
		mpmm_en_reg = ioremap(AP_TCM_MPMM_CTRL_ADDR, 0x4);

	ret = kstrtou32(buf, 0, &mpmm_en_val);
	if (ret != 0) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}
	if (mpmm_en_val == AP_TCM_MPMM_ENABLE_MAGICNUM || mpmm_en_val == AP_TCM_MPMM_DISABLE_MAGICNUM) {
		iowrite32(mpmm_en_val, mpmm_en_reg);
		pr_info("MPMM Status: 0x%X\n", mpmm_en_val);
	} else {
		pr_info("Invalid MPMM control value\n");
	}
	return n;
}

DEVICE_ATTR_RW(mpmm);

int mcupm_plt_module_init(void)
{
	phys_addr_t phys_addr, virt_addr, mem_sz;
	struct mcupm_ipi_data_s ipi_data;
	struct plt_ctrl_s *plt_ctl;
	int ret = 0;
	unsigned int last_ofs;
	unsigned int mpmm_support = 0;
#if MCUPM_LOGGER_SUPPORT
	unsigned int last_sz;
#endif
	unsigned int *mark;
	unsigned char *b;
	struct device_node *node = NULL;
	char mcupm_desc[] = "mediatek,mcupm";


	if (mcupm_sysfs_init()) {
		pr_info("[MCUPM] Sysfs Init Failed\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, mcupm_desc);
	if (!node)
		pr_notice("of_find_compatible_node unable to find mcupm device node\n");

	if (!of_property_read_u32(node, "mpmm-node-support", &mpmm_support)) {
		if (mpmm_support) {
			ret = mcupm_sysfs_create_file(&dev_attr_mpmm);
			mpmm_en_reg = ioremap(AP_TCM_MPMM_CTRL_ADDR, 0x4);
			iowrite32(0x0, mpmm_en_reg);
		}
	} else {
		pr_info("Failed to get mpmm support index from dts.\n");
	}

	ret = mcupm_sysfs_create_file(&dev_attr_mcupm_alive);
	if (unlikely(ret != 0))
		goto error;

	if (has_reserved_memory) {
		phys_addr = mcupm_reserve_mem_get_phys(MCUPM_MEM_ID);
		if (phys_addr == 0) {
			pr_info("MCUPM: Can't get logger phys mem\n");
			goto error;
		}

		virt_addr = (phys_addr_t)mcupm_reserve_mem_get_virt(MCUPM_MEM_ID);
		if (virt_addr == 0) {
			pr_info("MCUPM: Can't get logger virt mem\n");
			goto error;
		}

		mem_sz = mcupm_reserve_mem_get_size(MCUPM_MEM_ID);
		if (mem_sz == 0) {
			pr_info("MCUPM: Can't get logger mem size\n");
			goto error;
		}

		b = (unsigned char *) (uintptr_t)virt_addr;
		for (last_ofs = 0; last_ofs < sizeof(*plt_ctl); last_ofs++)
			b[last_ofs] = 0x0;

		mark = (unsigned int *) (uintptr_t)virt_addr;
		*mark = MCUPM_PLT_INIT;
		mark = (unsigned int *) ((unsigned char *) (uintptr_t)
					virt_addr + mem_sz - 4);
		*mark = MCUPM_PLT_INIT;

		plt_ctl = (struct plt_ctrl_s *) (uintptr_t)virt_addr;
		plt_ctl->magic = MCUPM_PLT_INIT;

		plt_ctl->size = sizeof(*plt_ctl);
#if MCUPM_LOGGER_SUPPORT
		if (skip_logger)
			plt_ctl->size = sizeof(*plt_ctl) - 4;
#endif

		plt_ctl->mem_sz = mem_sz;

		last_ofs = plt_ctl->size;


		pr_info("MCUPM: %s(): after plt, ofs=0x%x plt_ctl size=0x%lx\n", __func__,
			last_ofs, sizeof(*plt_ctl));

#if MCUPM_LOGGER_SUPPORT
		if (!skip_logger) {
			plt_ctl->logger_ofs = last_ofs;
			last_sz = mcupm_logger_init(virt_addr + last_ofs, mem_sz - last_ofs);


			if (last_sz == 0) {
				pr_info("MCUPM: mcupm_logger_init return fail\n");
				goto error;
			}

			last_ofs += last_sz;
			pr_info("MCUPM: %s(): after logger, ofs=0x%x\n", __func__, last_ofs);
		}
#endif

		ipi_data.cmd = MCUPM_PLT_INIT;
		ipi_data.u.ctrl.phys = phys_addr;
		ipi_data.u.ctrl.size = mem_sz;
		mcupm_plt_ackdata = 0;

		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_POLLING,
			&ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);

		if (ret) {
			pr_info("MCUPM: plt IPI fail ret=%d, ackdata=%d\n",
				ret, mcupm_plt_ackdata);
			goto error;
		}

		if (mcupm_plt_ackdata < 0) {
			pr_info("MCUPM: plt IPI init fail, ackdata=%d\n",
					mcupm_plt_ackdata);
			goto error;
		}

		pr_info("MCUPM: plt IPI success ret=%d, ackdata=%d\n",
			ret, mcupm_plt_ackdata);

#if MCUPM_LOGGER_SUPPORT
		if (!skip_logger)
			mcupm_logger_init_done();
#endif
		return 0;
	} else {
		struct mcupm_ipi_data_s ipi_data;
		int ret = 0;

		ipi_data.cmd = 0xDEAD;
		mcupm_plt_ackdata = 0;
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_WAIT,
			&ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);
		pr_info("MCUPM: %s RES_MEM(%d) SKIP_LOG(%d) ret(%d)\n",
			mcupm_plt_ackdata ? "Alive" : "Dead",
			has_reserved_memory, skip_logger, ret);
	}
	return 0;
 error:
	return -1;
}
void mcupm_plt_module_exit(void)
{
    //Todo release resource
	pr_info("[MCUPM] mcupm plt module exit.\n");
}
#endif
