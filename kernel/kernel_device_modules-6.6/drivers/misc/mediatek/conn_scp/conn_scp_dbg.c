// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "aoltest_core.h"
#include "conap_scp.h"
#include "conap_platform_data.h"

#define CONN_SCP_DBG_PROCNAME "driver/connscp_dbg"

static struct proc_dir_entry *g_conn_scp_dbg_entry;

/* proc functions */
static ssize_t conn_scp_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t conn_scp_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos);

/* action functions */
typedef int(*CONN_SCP_DBG_FUNC) (unsigned int par1, unsigned int par2, unsigned int par3);
static int msd_ctrl(unsigned int par1, unsigned int par2, unsigned int par3);
static int dfd_dump_trg_ctrl(unsigned int par1, unsigned int par2, unsigned int par3);
static int dfd_clr_buf(unsigned int par1, unsigned int par2, unsigned int par3);
static int dfd_emi_dump(unsigned int par1, unsigned int par2, unsigned int par3);
static int dfd_value_buf_info(unsigned int par1, unsigned int par2, unsigned int par3);
static int dbg_data_test(unsigned int par1, unsigned int par2, unsigned int par3);

static const CONN_SCP_DBG_FUNC g_conn_scp_dbg_func[] = {
	[0x0] = msd_ctrl,
	[0x1] = dfd_dump_trg_ctrl,
	[0x2] = dfd_clr_buf,
	[0x3] = dfd_emi_dump,
	[0x4] = dfd_value_buf_info,
	[0x5] = dbg_data_test,
};

static struct mutex g_dump_lock;

static int dfd_dump_trg_ctrl(unsigned int par1, unsigned int par2, unsigned int par3)
{
	conap_scp_dfd_cmd_handler(par2, 0, 0);
	return 0;
}

struct region_header {
	uint32_t offset;
	uint32_t size;
};

struct dfd_cmd_header {
	uint32_t conninfra_version;
	struct region_header conninfra_hdr;
	uint32_t wifi_version;
	struct region_header wifi_hdr;
	uint32_t bt_version;
	struct region_header bt_hdr;
};

struct dfd_value_header {
	struct region_header conninfra_hdr;
	struct region_header wifi_hdr;
	struct region_header bt_hdr;
};

struct dfd_header {
	uint32_t version;
	struct dfd_cmd_header cmd_hdr;
	struct dfd_value_header val_hdr;
};


static void dfd_emi_dump_value_buf(phys_addr_t addr, uint32_t count)
{
	void __iomem *vir_addr = NULL;
	uint32_t *val_ptr;
	uint32_t i;

	pr_info("[%s] addr[%p] size=[%d]", __func__, (void *)(unsigned long)addr, count);

	vir_addr = ioremap(addr, count);
	if (!vir_addr) {
		pr_err("ioremap fail");
		return;
	}

	pr_info("[%s] vir addr[%p]", __func__, vir_addr);

	val_ptr = (uint32_t *)vir_addr;

	for (i = 0; i < count/4; i += 8) {
		pr_info("[%s] [%p] [%08x][%08x][%08x][%08x] [%08x][%08x][%08x][%08x]", __func__,
					val_ptr,
					*(val_ptr), *(val_ptr+1), *(val_ptr+2), *(val_ptr+3),
					*(val_ptr+4), *(val_ptr+5), *(val_ptr+6), *(val_ptr+7));
		val_ptr += 8;
	}

	iounmap(vir_addr);
}

static int dfd_emi_dump(unsigned int par1, unsigned int par2, unsigned int par3)
{
	void __iomem *vir_addr = NULL;
	int size;
	struct dfd_header hdr;


	/* parameter par2: */
	/*   0: dfd dump header file */
	/*   1: conninfra value buffer content */
	/*   2: wifi value buffer content */
	/*   3: bt value buffer content */

	phys_addr_t cmd_addr = connsys_scp_get_dfd_cmd_addr();
	uint32_t cmd_size = connsys_scp_get_dfd_cmd_size();

	size = sizeof(hdr);
	pr_info("[%s] cmd addr=[%p] size=[%x]", __func__, (void *)(unsigned long)cmd_addr, cmd_size);

	vir_addr = ioremap(cmd_addr, size);
	if (!vir_addr) {
		pr_err("ioremap fail");
		//vfree(buf);
		return -1;
	}
	memcpy_fromio(&hdr, vir_addr, size);
	iounmap(vir_addr);

	if (par2 == 0) {
		pr_info("[%s] cmd c=[%x][%x][%x] w=[%x][%x][%x] b=[%x][%x][%x]", __func__,
			hdr.cmd_hdr.conninfra_version,
			hdr.cmd_hdr.conninfra_hdr.offset, hdr.cmd_hdr.conninfra_hdr.size,
			hdr.cmd_hdr.wifi_version,
			hdr.cmd_hdr.wifi_hdr.offset, hdr.cmd_hdr.wifi_hdr.size,
			hdr.cmd_hdr.bt_version,
			hdr.cmd_hdr.bt_hdr.offset, hdr.cmd_hdr.bt_hdr.size);

		pr_info("[%s] value c=[%x][%x] w=[%x][%x] b=[%x][%x]", __func__,
			hdr.val_hdr.conninfra_hdr.offset, hdr.val_hdr.conninfra_hdr.size,
			hdr.val_hdr.wifi_hdr.offset, hdr.val_hdr.wifi_hdr.size,
			hdr.val_hdr.bt_hdr.offset, hdr.val_hdr.bt_hdr.size);

		return 0;
	} else if (par2 == 1) {
		dfd_emi_dump_value_buf(cmd_addr + hdr.val_hdr.conninfra_hdr.offset,
					hdr.val_hdr.conninfra_hdr.size);
	} else if (par2 == 2) {
		dfd_emi_dump_value_buf(cmd_addr + hdr.val_hdr.wifi_hdr.offset,
					hdr.val_hdr.wifi_hdr.size);
	} else if (par2 == 3) {
		dfd_emi_dump_value_buf(cmd_addr + hdr.val_hdr.bt_hdr.offset,
					hdr.val_hdr.bt_hdr.size);
	}

	return 0;
}

static int dfd_value_buf_info(unsigned int par1, unsigned int par2, unsigned int par3)
{
	phys_addr_t addr;
	uint32_t size;

	conap_scp_dfd_get_value_info(&addr, &size);

	pr_info("[%s] value buffer addr=[%p] size=[%x]", __func__, (void *)(unsigned long)addr, size);
	return 0;
}

#define CONNSCP_TEST_BUFFER_SIZE 2048
uint8_t g_test_buffer[CONNSCP_TEST_BUFFER_SIZE];
uint32_t g_test_seq;
static int dbg_data_test(unsigned int par1, unsigned int par2, unsigned int par3)
{
	int i;
	int ret;

	if (par2 > CONNSCP_TEST_BUFFER_SIZE) {
		pr_notice("%s: buffer size too large\n", __func__);
		return 0;
	}

	for (i = 0; i < par2; i++)
		g_test_buffer[i] = g_test_seq;
	g_test_seq++;

	pr_info("[%s] start send data size=[%d]", __func__, par2);
	ret = aoltest_core_send_dbg_data(&g_test_buffer[0], par2);
	pr_info("[%s] end ret=[%d]", __func__, ret);

	return 0;
}

static int dfd_clr_buf(unsigned int par1, unsigned int par2, unsigned int par3)
{
	return conap_scp_dfd_clr_buf_handler();
}

int msd_ctrl(unsigned int par1, unsigned int par2, unsigned int par3)
{
	pr_info("[%s] [%x][%x][%x]", __func__, par1, par2, par3);
	aoltest_core_send_dbg_msg(par2, par3);
	return 0;
}

ssize_t conn_scp_dbg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

ssize_t conn_scp_dbg_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count;
	char buf[256];
	char *pBuf;
	unsigned int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	char *pDelimiter = " \t";
	long res = 0;
	static char dbg_enabled;

	pr_info("write parameter len = %d\n\r", (int) len);
	if (len >= sizeof(buf)) {
		pr_notice("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_info("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			x = (unsigned int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			y = (unsigned int)res;
	}

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		if (kstrtol(pToken, 16, &res) == 0)
			z = (unsigned int)res;
	}

	pr_info("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	/* For eng and userdebug load, have to enable wmt_dbg by writing 0xDB9DB9 to
	 * "/proc/driver/wmt_dbg" to avoid some malicious use
	 */
	if (x == 0xDB9DB9) {
		dbg_enabled = 1;
		return len;
	}

	if (dbg_enabled == 0)
		return len;

	if (ARRAY_SIZE(g_conn_scp_dbg_func) > x && NULL != g_conn_scp_dbg_func[x])
		(*g_conn_scp_dbg_func[x]) (x, y, z);
	else
		pr_notice("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

int conn_scp_dbg_init(void)
{
	static const struct proc_ops conn_scp_dbg_fops = {
		.proc_read = conn_scp_dbg_read,
		.proc_write = conn_scp_dbg_write,
	};
	int ret = 0;

	g_conn_scp_dbg_entry = proc_create(CONN_SCP_DBG_PROCNAME, 0664, NULL, &conn_scp_dbg_fops);
	if (g_conn_scp_dbg_entry == NULL) {
		pr_notice("Unable to create connscp_dbg proc entry\n\r");
		ret = -1;
	}

	mutex_init(&g_dump_lock);

	//memset(g_dump_buf, '\0', CONNINFRA_DBG_DUMP_BUF_SIZE);
	return ret;
}

int conn_scp_dbg_deinit(void)
{
	mutex_destroy(&g_dump_lock);

	if (g_conn_scp_dbg_entry != NULL) {
		proc_remove(g_conn_scp_dbg_entry);
		g_conn_scp_dbg_entry = NULL;
	}

	return 0;
}
