// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/compat.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/math.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>

/* gpu header */
#include <gpu_pdma.h>
#include <mtk_gpufreq.h>
#include <ghpm_wrapper.h>
#include <ged_gpu_slc.h>

/* define */
#define PDMA_DEVNAME "gpu_pdma_driver"
#define PDMA_MISC_DEVNAME "gpu_pdma"
#define RING_BUFFER_PAGE_SIZE(x)		((1 << (x)) << PAGE_SHIFT)

/* variables */
static struct mutex gHwLockMutex;
static unsigned int g_pdma_hw_lock;
static unsigned int g_pdma_lock_kctx_id;
static unsigned int g_pdma_store_value;
static unsigned int g_pdma_open_cnt;
static unsigned long g_ringbuf_addr;
static unsigned long g_pdma_reg_base;
static unsigned long g_pdma_reg_region;
static unsigned long g_pdma_hw_sem_base;
static unsigned long g_pdma_hw_sem_offset;
static unsigned int g_page_order; /* g_page_order is 4k-based */
static unsigned int g_config_mode;
static unsigned long g_pdma_sram_base;
static unsigned int g_pdma_hw_sem_bit;
static int g_dynamic_mode;
static unsigned long long g_gid_list_discardable;
static unsigned long long g_gid_list_non_disc;
static void __iomem *g_pdma_reg_base_kva;
static void __iomem *g_pdma_hw_sem_base_kva;
static struct pdma_sram *g_pdma_sram_base_kva;
static unsigned int g_sw_ver;

/* Define */
#define CCMD_STATUS_CH0						0x20
#define CCMD_RING_BUFFER_HRPTR		0x74
#define CCMD_RING_BUFFER_HWPTR		0x78
#define CCMD_RING_BUFFER_CONTROL	0x84
#define CCMD_RING_BUFFER_PA_0_L		0x100
#define CCMD_RING_BUFFER_PA_0_H		0x104
#define CCMD_RINGBUF_PA_REG_WIDTH	0x8 /* 64-bit */
#define CCMD_RING_BUFFER_PA_VALID	0x1
#define CCMD_INIT_RINGBUG_PA			1
#define CCMD_CONFIG_MODE_AP				1
#define CCMD_CONFIG_MODE_GPUEB		0
#define CCMD_PAGE_SIZE_4K					0x1000
#define POLICY_TEX_CACHE_LSC_ALLOC	0x4
#define CCMD_POWER_ON							1
#define CCMD_POWER_OFF						0


struct tag_chipid {
	uint32_t size;
	uint32_t hw_code;
	uint32_t hw_subcode;
	uint32_t hw_ver;
	uint32_t sw_ver;
};

static int pdma_get_chipid(void)
{
	struct tag_chipid *chip_id;
	struct device_node *node = of_find_node_by_path("/chosen");

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (!node) {
		pr_notice("chosen node not found in device tree\n");
		return -ENODEV;
	}

	chip_id = (struct tag_chipid *)of_get_property(node, "atag,chipid", NULL);
	if (!chip_id) {
		pr_notice("could not found atag,chipid in chosen\n");
		return -ENODEV;
	}

	g_sw_ver = chip_id->sw_ver;

	return 0;
}

/* Must be called with hw lock held */
static void request_gid_list(void)
{
	g_gid_list_discardable = 0x7FFF << 16;
	g_gid_list_non_disc = 0xFFFE;
}
/* Must be called with hw lock held */
static void release_gid_list(void)
{
		g_gid_list_discardable = 0;
		g_gid_list_non_disc = 0;
}

static int ccmd_power_control(int power)
{
	int ret = 0;

	if (power == CCMD_POWER_ON) {
		/* On mfg0 and gpueb */
		ret = gpueb_ctrl(GHPM_ON, MFG1_OFF, SUSPEND_POWER_ON);
		if (ret) {
			pr_err("[CCMD] gpueb on fail, return value=%d\n", ret);
			return ret;
		}
		/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
		if (gpufreq_power_control(GPU_PWR_ON) < 0) {
			pr_err("[CCMD] Power On Failed\n");
			return 1;
		}

		/* Control runtime active-sleep state of GPU */
		if (gpufreq_active_sleep_control(GPU_PWR_ON) < 0) {
			pr_err("[CCMD] Active Failed (on)\n");
			return 1;
		}
	} else if (power == CCMD_POWER_OFF) {
		/* Control runtime active-sleep state of GPU */
		if (gpufreq_active_sleep_control(GPU_PWR_OFF) < 0) {
			pr_err("[CCMD] Sleep Failed (off)\n");
			return 1;
		}

		/* on,off/ SWCG(BG3D)/ MTCMOS/ BUCK */
		if (gpufreq_power_control(GPU_PWR_OFF) < 0){
			pr_err( "[CCMD] Power Off Failed\n");
			return 1;
		}

		/* Off mfg0 and gpueb */
		ret = gpueb_ctrl(GHPM_OFF, MFG1_OFF, SUSPEND_POWER_OFF);
		if (ret) {
			pr_err("[CCMD] gpueb off fail, return value=%d\n", ret);
			return ret;
		}
	} else {
		pr_err("%s Unexpected power state %d\n", __func__, power);
		ret = 2;
	}
	return ret;
}

/* Must be called with power on */
static void ccmd_reset_hw(void)
{
	int *ringbuf_pa0_setting_l;
	unsigned int pdma_status;

	if (g_sw_ver == 0) {
		writel(0x3, (g_pdma_reg_base_kva + CCMD_RING_BUFFER_CONTROL));
		return;
	}

	ringbuf_pa0_setting_l = g_pdma_reg_base_kva + CCMD_RING_BUFFER_PA_0_L;

	/* Disable and poll Ch0*/
	writel((readl(ringbuf_pa0_setting_l) & 0xFFFFFFFE), ringbuf_pa0_setting_l);
	pdma_status = readl((g_pdma_reg_base_kva + CCMD_STATUS_CH0));
	if (!pdma_status)
		pr_info("[CCMD] autoDMA ch0 not completed\n");

	/* reset HW */
	writel(0x3, (g_pdma_reg_base_kva + CCMD_RING_BUFFER_CONTROL));

	/* Enable ch0*/
	writel((readl(ringbuf_pa0_setting_l) | 0x1), ringbuf_pa0_setting_l);
}

/* Must be called with hw lock held */
static int init_ccmd_hw(void)
{
	unsigned long ringbuf_pa;
	int *ringbuf_pa_setting_l, *ringbuf_pa_setting_h;
	int page_num;
	int ret = 0;

	if (g_config_mode == CCMD_CONFIG_MODE_GPUEB)
		return ret;

	ringbuf_pa = virt_to_phys((void *)g_ringbuf_addr);
	pr_debug("PDMA Ring buffer addr PA0 0x%lx\n", ringbuf_pa);

	if (ccmd_power_control(CCMD_POWER_ON))
		return 1;

	for (page_num = 0; page_num < (1 << g_page_order); page_num++) {
		ringbuf_pa_setting_l =
			g_pdma_reg_base_kva + CCMD_RING_BUFFER_PA_0_L
			+ (CCMD_RINGBUF_PA_REG_WIDTH * page_num);
		ringbuf_pa_setting_h =
			g_pdma_reg_base_kva + CCMD_RING_BUFFER_PA_0_H
			+ (CCMD_RINGBUF_PA_REG_WIDTH * page_num);

		writel((ringbuf_pa | CCMD_RING_BUFFER_PA_VALID) & 0xFFFFFFFF,
			ringbuf_pa_setting_l);
		writel((ringbuf_pa >> 32) & 0xFFFFFFFF, ringbuf_pa_setting_h);

		g_pdma_sram_base_kva->ringbuf[page_num] = (ringbuf_pa >> 12);

		pr_debug("Config CCMD_RING_BUFFER_PA_%d: 0x%08x%08x\n", page_num,
			readl(ringbuf_pa_setting_h), readl(ringbuf_pa_setting_l));
			/* ring_buffer_pa config per 4k range */
			ringbuf_pa += CCMD_PAGE_SIZE_4K;
	}
	/* reset HW */
	//writel(0x3, (g_pdma_reg_base_kva + CCMD_RING_BUFFER_CONTROL));
	ccmd_reset_hw();

	if (ccmd_power_control(CCMD_POWER_OFF))
		return 2;

	return ret;
}
static const struct of_device_id pdma_of_match[] = {
	{ .compatible = "mediatek,gpupdma", },
	{/* sentinel */}
};

void gpu_pdma_vma_open(struct vm_area_struct *vma)
{
	pr_debug("gpu_pdma VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void gpu_pdma_vma_close(struct vm_area_struct *vma)
{
	pr_debug("gpu_pdma VMA close, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

const struct vm_operations_struct gpu_pdma_vm_ops = {
	.open = gpu_pdma_vma_open,
	.close = gpu_pdma_vma_close,
};

static int gpu_pdma_open(struct inode *inode, struct file *filp)
{
	g_pdma_open_cnt++;
	pr_debug("%s open count %u\n", __func__, g_pdma_open_cnt);
	return 0;
}

static int gpu_pdma_release(struct inode *inode, struct file *filp)
{
	g_pdma_open_cnt--;
	pr_debug("%s open count %u\n", __func__, g_pdma_open_cnt);
	return 0;
}

static int gpu_pdma_mmap(struct file *const filp,
	struct vm_area_struct *const vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long phy_addr = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long ringbuf_pa = virt_to_phys((void *)g_ringbuf_addr);

	if (phy_addr == ringbuf_pa && length == RING_BUFFER_PAGE_SIZE(g_page_order)) {
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	} else if (phy_addr == g_pdma_reg_base && length == g_pdma_reg_region) {
		vma->vm_page_prot =  pgprot_noncached(vma->vm_page_prot);
	} else if (phy_addr == g_pdma_hw_sem_base && ((length >> PAGE_SHIFT) == 1)) {
		/* Expect only one page needed from hw semaphore base */
		vma->vm_page_prot =  pgprot_noncached(vma->vm_page_prot);
	} else {
		pr_err("Invalid argument! addr: %lx, length: %ld\n", phy_addr, length);
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_err("%s remap_pfn_range fail\n", __func__);
		return -EAGAIN;
	}
	vma->vm_ops = &gpu_pdma_vm_ops;
	return 0;
}

static long gpu_pdma_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	struct pdma_hw_lock hw_lock;
	struct pdma_rw_ptr rw_ptr;

	switch (cmd) {
	case GPU_PDMA_LOCKHW:
		ret = copy_from_user(&hw_lock, (void __user *)arg, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_LOCKHW copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}

		mutex_lock(&gHwLockMutex);
		if (g_pdma_hw_lock == 0) {
			/* Config HW */
			if (init_ccmd_hw()) {
				pr_err("Config ring buffer PA fail. pid/tid: %d/%d (%d)\n",
					current->tgid, current->pid, hw_lock.in.kctx_id);
				return -EFAULT;
			}

			g_pdma_hw_lock = 1;
			g_pdma_lock_kctx_id = hw_lock.in.kctx_id;
			hw_lock.out.status = 1;
			hw_lock.out.base = g_pdma_reg_base;		/* pa of ptr base */
			hw_lock.out.region_size = g_pdma_reg_region;
			hw_lock.out.ringbuf = virt_to_phys((void *)g_ringbuf_addr); /* pa of ring */
			hw_lock.out.size = RING_BUFFER_PAGE_SIZE(g_page_order);
			hw_lock.out.hw_sem_base = g_pdma_hw_sem_base;
			hw_lock.out.hw_sem_offset = g_pdma_hw_sem_offset;
			request_gid_list();
			hw_lock.out.gid_list_discardable = g_gid_list_discardable;
			hw_lock.out.gid_list_non_disc = g_gid_list_non_disc;
			hw_lock.out.sw_ver = g_sw_ver;
			pr_info("LockHW success by pid/tid: %d/%d, (%d)(%u)\n",
				current->tgid, current->pid, hw_lock.in.kctx_id, g_sw_ver);

			if (hw_lock.in.mode == 1){
				g_dynamic_mode = ged_gpu_slc_get_dynamic_mode();
				ged_gpu_slc_dynamic_mode(POLICY_TEX_CACHE_LSC_ALLOC);
			}
		} else
			memset(&hw_lock, 0, sizeof(struct pdma_hw_lock));


		if (!hw_lock.out.status) {
			pr_err("LockHW Fail by pid/tid: %d/%d (%d)\n",
				current->tgid, current->pid, hw_lock.in.kctx_id);
			mutex_unlock(&gHwLockMutex);
			return -EBUSY;
		}
		mutex_unlock(&gHwLockMutex);

		ret = copy_to_user((void __user *)arg, &hw_lock, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_LOCKHW copy_to_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		break;
	case GPU_PDMA_UNLOCKHW:

		ret = copy_from_user(&hw_lock, (void __user *)arg, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_UNLOCKHW copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}

		mutex_lock(&gHwLockMutex);
		if ((hw_lock.in.kctx_id == g_pdma_lock_kctx_id) && (g_pdma_lock_kctx_id != 0xFFFFFFFF)) {
			pr_info("GPU_PDMA_UNLOCKHW done and context %u release lock\n", g_pdma_lock_kctx_id);
			g_pdma_hw_lock = 0;
			g_pdma_lock_kctx_id = 0xFFFFFFFF;
			hw_lock.out.status = 1;
			hw_lock.out.base = 0;
			hw_lock.out.region_size = 0;
			hw_lock.out.ringbuf = 0;
			hw_lock.out.size = 0;
			hw_lock.out.hw_sem_base = 0;
			hw_lock.out.hw_sem_offset = 0;
			hw_lock.out.gid_list_discardable = 0;
			hw_lock.out.gid_list_non_disc = 0;
			release_gid_list();
			ged_gpu_slc_dynamic_mode(g_dynamic_mode);
		} else {
			pr_err("GPU_PDMA_UNLOCKHW failed. Context ID is not matched: %u != %u (%d)\n",
					hw_lock.in.kctx_id, g_pdma_lock_kctx_id, current->pid);
				mutex_unlock(&gHwLockMutex);
				return -EINVAL;
		}
		mutex_unlock(&gHwLockMutex);
		ret = copy_to_user((void __user *)arg, &hw_lock, sizeof(struct pdma_hw_lock));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_UNLOCKHW copy_to_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		break;
	case GPU_PDMA_WRITE_HWPTR:
		ret = copy_from_user(&rw_ptr, (void __user *)arg, sizeof(struct pdma_rw_ptr));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_WRITE_HWPTR copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		mutex_lock(&gHwLockMutex);
		if (g_pdma_hw_lock == 0 || (g_pdma_lock_kctx_id != rw_ptr.in.kctx_id)) {
			pr_err("Must lock hw before update ptr: %d,%u,%u\n",
				g_pdma_hw_lock, rw_ptr.in.kctx_id, g_pdma_lock_kctx_id);
			mutex_unlock(&gHwLockMutex);
			return -EINVAL;
		}

		/* write hwptr reg */
		writel(rw_ptr.in.hwptr, (g_pdma_reg_base_kva + CCMD_RING_BUFFER_HWPTR));
		pr_debug("update hwptr: 0x%08X\n", readl(g_pdma_reg_base_kva + CCMD_RING_BUFFER_HWPTR));
		mutex_unlock(&gHwLockMutex);
		break;
	case GPU_PDMA_READ_HRPTR:
		/* read hrptr reg */
		ret = copy_from_user(&rw_ptr, (void __user *)arg, sizeof(struct pdma_rw_ptr));
		if (ret) {
			pr_err("[ERROR] GPU_PDMA_WRITE_HWPTR copy_from_user Fail: %lu\n", ret);
			return -EFAULT;
		}
		mutex_lock(&gHwLockMutex);
		if (g_pdma_hw_lock == 0 || (g_pdma_lock_kctx_id != rw_ptr.in.kctx_id)) {
			pr_err("Must lock hw before read ptr: %d,%u,%u\n",
				g_pdma_hw_lock, rw_ptr.in.kctx_id, g_pdma_lock_kctx_id);
			mutex_unlock(&gHwLockMutex);
			return -EINVAL;
		}
		rw_ptr.out.hrptr = readl(g_pdma_reg_base_kva + CCMD_RING_BUFFER_HRPTR);
		pr_debug("read hrptr: 0x%08X\n", readl(g_pdma_reg_base_kva + CCMD_RING_BUFFER_HRPTR));

		ret = copy_to_user((void __user *)arg, &rw_ptr, sizeof(unsigned int));
		if (ret) {
			pr_err("[ERROR] GPU_CCMD_READ_HRPTR copy_to_user Fail: %lu\n", ret);
			mutex_unlock(&gHwLockMutex);
			return -EFAULT;
		}
		mutex_unlock(&gHwLockMutex);
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static long gpu_pdma_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret;
	void __user *data;

	data = compat_ptr((uint32_t)arg);
	ret = gpu_pdma_unlocked_ioctl(file, cmd, (unsigned long)data);

	return ret;
}

static int gpu_pdma_get_hw_sem(void)
{
	int ret = -1;

	if (g_pdma_hw_sem_base && g_pdma_hw_sem_offset)
		ret = (readl(g_pdma_hw_sem_base_kva) >> g_pdma_hw_sem_bit & 0x1);
	else
		pr_info("@%s: get hw sem status failed\n", __func__);
	return ret;
}

static void gpu_pdma_set_irq(int idx)
{
	if (g_pdma_sram_base_kva) {
		/* Clear interrupt status if irq is to be disabled*/
		if (idx != 0)
			g_pdma_sram_base_kva->interrupt_status =
				((g_pdma_sram_base_kva->interrupt_status >> 1) << 1) | (idx & 0x1);
		else
			g_pdma_sram_base_kva->interrupt_status = 0;
	} else
		pr_err("@%s: set irq failed\n", __func__);
}

const struct file_operations gpu_pdma_fops = {
	.owner = THIS_MODULE,
	.open = gpu_pdma_open,
	.release = gpu_pdma_release,
	.mmap = gpu_pdma_mmap,
	.unlocked_ioctl = gpu_pdma_unlocked_ioctl,
	.compat_ioctl = gpu_pdma_compat_ioctl,
};

static struct miscdevice gpu_pdma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gpu_pdma",
	.fops = &gpu_pdma_fops,
};

static ssize_t gpu_pdma_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;

	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"hwlock status:		0x%x\n", g_pdma_hw_lock);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"lock hw id:		0x%x\n", g_pdma_lock_kctx_id);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"hw_sem status:		0x%x\n", gpu_pdma_get_hw_sem());
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"interrupt status:	0x%x\n",
		(g_pdma_sram_base_kva->interrupt_status & 0x2)>>1);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"irq enable status:	0x%x\n",
		g_pdma_sram_base_kva->interrupt_status & 0x1);
	pos += scnprintf(buf + pos, PAGE_SIZE - pos,
		"ringbuf PA base:	0x%x\n", g_pdma_sram_base_kva->ringbuf[0]);
	return pos;
}

static ssize_t gpu_pdma_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{
	if (kstrtouint(buf, 0, &g_pdma_store_value) == 0){
		if (g_pdma_store_value == 0 || g_pdma_store_value == 1)
			gpu_pdma_set_irq(g_pdma_store_value);
		else
			pr_info("@%s: Invalid value for gpu_pdma\n", __func__);
	}

	return n;
}
DEVICE_ATTR_RW(gpu_pdma);
/* /sys/class/misc/gpu_pdma/gpu_pdma */
static int __create_file(void)
{
	int ret = 0;

	ret = misc_register(&gpu_pdma_device);
	if (unlikely(ret != 0)) {
		pr_err("@%s: misc register failed\n", __func__);
		return ret;
	}

	ret = device_create_file(gpu_pdma_device.this_device,
		&dev_attr_gpu_pdma);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}
static void __delete_file(void)
{
	device_remove_file(gpu_pdma_device.this_device,
		&dev_attr_gpu_pdma);
	misc_deregister(&gpu_pdma_device);
}

static int gpu_pdma_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	gfp_t gfp = (GFP_HIGHUSER | __GFP_ZERO);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		pr_err("PDMA platform_get_resource fail\n");
		ret = -ENODEV;
	} else {
		g_pdma_reg_base = res->start;
		g_pdma_reg_region = res->end - res->start + 1;
		g_pdma_reg_base_kva = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(g_pdma_reg_base_kva))
			return PTR_ERR(g_pdma_reg_base_kva);

		pr_info("@%s: PDMA reg base: 0x%lx, size: 0x%lx, kva: 0x%p\n", __func__,
			g_pdma_reg_base, g_pdma_reg_region, g_pdma_reg_base_kva);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		pr_err("PDMA platform_get_resource 1 fail\n");
		ret = -ENODEV;
	} else {
		g_pdma_hw_sem_base = res->start & 0xFFFFF000;
		g_pdma_hw_sem_offset = res->start & 0xFFF;
		g_pdma_hw_sem_base_kva = devm_ioremap(&pdev->dev, res->start, 0x4);
		if (IS_ERR(g_pdma_hw_sem_base_kva))
			return PTR_ERR(g_pdma_hw_sem_base_kva);

		pr_info("@%s: HW Sem base: 0x%lx, offset: 0x%lx, size: 0x%llx, kva: 0x%p\n", __func__,
			g_pdma_hw_sem_base, g_pdma_hw_sem_offset, resource_size(res), g_pdma_hw_sem_base_kva);
	}

	if (!of_property_read_u32(node, "ringbuf-page-order", &g_page_order)) {
		pr_info("PDMA get page size order of ring buffer: %u. PAGE_SIZE: 0x%lx\n",
			g_page_order, PAGE_SIZE);
		if (CCMD_PAGE_SIZE_4K != PAGE_SIZE)
			/* 16 KB page */
			g_ringbuf_addr = __get_free_pages(gfp, (g_page_order - 2));
		else
			g_ringbuf_addr = __get_free_pages(gfp, g_page_order);

		if (!g_ringbuf_addr) {
			pr_err("__get_free_pages failed\n");
			return -ENOMEM;
		}
	} else {
		pr_err("PDMA get ringbuf-page-order fail\n");
		ret = -ENODEV;
	}

	if (!of_property_read_u32(node, "config-mode", &g_config_mode)) {
		pr_err("PDMA get config-mode: %u\n", g_config_mode);
	} else {
		pr_err("PDMA get g_config_mode fail\n");
		g_config_mode = CCMD_CONFIG_MODE_GPUEB;
	}

	/* sram */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		pr_err("PDMA platform_get_resource 2 fail\n");
		ret = -ENODEV;
	} else {
		g_pdma_sram_base = res->start;
		g_pdma_sram_base_kva = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(g_pdma_sram_base_kva))
			return PTR_ERR(g_pdma_sram_base_kva);

		/* init sram values */
		g_pdma_sram_base_kva->ccmd_hw_reset = 0;
		g_pdma_sram_base_kva->interrupt_status = 0;

		pr_info("@%s: SRAM base: 0x%lx, size: 0x%llx, kva: 0x%p\n", __func__,
			g_pdma_sram_base, resource_size(res), g_pdma_sram_base_kva);
	}

	if (!of_property_read_u32(node, "hw-semaphore-bit", &g_pdma_hw_sem_bit)) {
		pr_info("PDMA hw-semaphore-bit: %u\n", g_pdma_hw_sem_bit);
	} else {
		pr_err("PDMA get g_pdma_hw_sem_bit fail\n");
		ret = -ENODEV;
	}

	ret = __create_file();
	if (ret)
		pr_err("@%s: __create_files failed\n", __func__);

	ret =	pdma_get_chipid();
	if (ret)
		pr_err("@%s: __get sw version fail\n", __func__);

	return ret;
}

static int gpu_pdma_remove(struct platform_device *dev)
{
	int ret;

	ret = 0;
	__delete_file();

	return ret;
}

static struct platform_driver gpu_pdma_driver = {
	.probe = gpu_pdma_probe,
	.remove = gpu_pdma_remove,
	.driver = {
		.name  = PDMA_DEVNAME,
		.owner = THIS_MODULE,
		.of_match_table = pdma_of_match,
	},
};

static int __init gpu_pdma_init(void)
{
	int ret;

	/* init driver */
	g_pdma_open_cnt = 0;
	g_pdma_hw_lock = 0;
	g_pdma_lock_kctx_id = 0xFFFFFFFF;
	mutex_init(&gHwLockMutex);

	ret = platform_driver_register(&gpu_pdma_driver);
	if (ret)
		pr_err("Fail to register PDMA platform driver\n");

	return ret;
}

static void __exit gpu_pdma_exit(void)
{
	platform_driver_unregister(&gpu_pdma_driver);
}

void pdma_lock_reclaim(u32 kctx_id)
{
	mutex_lock(&gHwLockMutex);
	if (g_pdma_hw_lock == 0) {
		pr_debug("[PDMA] HW is not locked.\n");
		mutex_unlock(&gHwLockMutex);
		return;
	}

	if (kctx_id == g_pdma_lock_kctx_id) {
		/* release lock and reset HW */
		g_pdma_hw_lock = 0;
		g_pdma_lock_kctx_id = 0xFFFFFFFF;
		/* return gid */
		pr_info("%s reclaim done and kctx %u release lock\n", __func__, kctx_id);
	}
	mutex_unlock(&gHwLockMutex);
}
EXPORT_SYMBOL_GPL(pdma_lock_reclaim);

module_init(gpu_pdma_init);
module_exit(gpu_pdma_exit);
MODULE_LICENSE("GPL");
