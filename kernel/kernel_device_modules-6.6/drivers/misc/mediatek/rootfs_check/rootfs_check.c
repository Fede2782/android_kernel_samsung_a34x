// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */
#include <linux/async.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fd.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/init.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/init_syscalls.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/ramfs.h>
#include <linux/root_dev.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/tty.h>

#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>

#include <do_mounts.h>
#include "rootfs_check.h"
#include "nfsb_key_modulus.h"
#include "rsa.h"

#pragma pack(push)
#pragma pack(1)
struct ubick_header {
	char magic[4];
	uint8_t version;
	uint8_t hash_type;
	uint32_t block_size;
	uint32_t block_interval;
	uint32_t block_count;
	uint8_t hash_salt[32];
	uint8_t rsa_sig[256];
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ubi_ec_hdr {
	uint32_t magic;
	char version;
	char padding1[3];
	uint64_t ec; /* Warning: the current limit is 31-bit anyway! */
	uint32_t vid_hdr_offset;
	uint32_t data_offset;
	uint32_t image_seq;
	char padding2[32];
	uint32_t hdr_crc;
};
#pragma pack(pop)

#define RSA_LEN 256

#define HASH_LEN 32
#define CHECK_FAIL -1
#define CHECK_PASS 0
#define MAX_BUF_LEN 32

#if IS_ENABLED(CONFIG_MTD)
#include <linux/mtd/mtd.h>
int get_rootfs_mtd_num(void)
{
	char *cmdline = saved_command_line;
	int idx_start = 0, idx_end = 0;
	int cmdline_len = strlen(cmdline);
	char tmp_buf[MAX_BUF_LEN] = {0};
	long rootfs_mtd_num;

	for (; idx_start < cmdline_len - strlen("ubi.mtd"); idx_start++) {
		if (memcmp(cmdline + idx_start, "ubi.mtd", strlen("ubi.mtd")) ==
		    0) {
			break;
		}
	}

	if (idx_start == strlen(cmdline) - strlen("ubi.mtd"))
		return -1;

	while (idx_start < cmdline_len && cmdline[idx_start] != '=')
		idx_start++;

	if (idx_start == cmdline_len)
		return -1;

	idx_end = ++idx_start;

	while (idx_end < cmdline_len && cmdline[idx_end] != ',' &&
	       cmdline[idx_end] != ' ')
		idx_end++;

	if (idx_end == cmdline_len)
		return -1;

	memcpy(tmp_buf, cmdline + idx_start, idx_end - idx_start);

	if (kstrtol(tmp_buf, 0, &rootfs_mtd_num) != 0)
		return -1;

	return (int)rootfs_mtd_num;
}

int mtd_find_goodblk(struct mtd_info *mtd, loff_t *off)
{
	u32 i = 0;
	int ret;
	loff_t real_off = (loff_t)mtd->erasesize * -1;
	loff_t tmp_logic_off = (loff_t)mtd->erasesize * -1;

	while (1) {
		int isbad = mtd_block_isbad(mtd, (loff_t)i * mtd->erasesize);

		if (isbad == -EINVAL) {
			pr_notice(
				"[%s]can not find enough good blocks for offset %lld!\n",
				__func__, *off);
			return isbad;
		}
		if (!isbad)
			tmp_logic_off += mtd->erasesize;
		real_off += mtd->erasesize;
		if (tmp_logic_off ==
		    ((*off >> mtd->erasesize_shift) << mtd->erasesize_shift))
			break;
		i++;
	}
	while (1) {
		ret = mtd_block_isbad(mtd, real_off);
		if (!ret)
			break;
		else if (ret == -EINVAL) {
			pr_notice(
				"[%s]can not find enough good blocks for offset %lld!\n",
				__func__, *off);
			return ret;
		}
		real_off += mtd->erasesize;
	}

	*off = real_off + (*off - ((*off >> mtd->erasesize_shift)
				   << mtd->erasesize_shift));

	return 0;
}

int __init mtd_verity(int mtd_num)
{
	struct mtd_info *root_mtd = get_mtd_device(NULL, mtd_num);
	size_t retlen;
	size_t ubi_image_len;
	struct ubick_header *header;
	uint32_t block_size, block_interval, block_count;
	u_char *buf;
	uint32_t block_size_order;
	struct page *pages;
	uint8_t *calculate_buffer;
	uint8_t *decrypted_content;
	uint8_t excepted_e[4] = {0x00, 0x01, 0x00, 0x01};
	uint8_t calculate_hash[HASH_LEN];
	int i = 0;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int desc_size = 0;
	int ret = CHECK_PASS;
	loff_t real_offset, log_offset;
	size_t readlen;
	uint8_t pss_sig[RSA_LEN];
	uint8_t pss_hash[HASH_LEN];
	int res = 0;

	if (root_mtd == NULL) {
		ret = CHECK_FAIL;
		goto fail1;
	}

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		ret = CHECK_FAIL;
		goto fail1;
	}

	real_offset = 0;
	if (mtd_find_goodblk(root_mtd, &real_offset)) {
		ret = CHECK_FAIL;
		goto fail2;
	}
	mtd_read(root_mtd, real_offset, PAGE_SIZE, &retlen, buf);

	ubi_image_len =
		be32_to_cpu(*(uint32_t *)&buf[sizeof(struct ubi_ec_hdr) -
					      2 * sizeof(uint32_t)]);

	real_offset = ubi_image_len;
	if (mtd_find_goodblk(root_mtd, &real_offset)) {
		ret = CHECK_FAIL;
		goto fail2;
	}

	readlen = root_mtd->erasesize -
		  (real_offset - ((real_offset >> root_mtd->erasesize_shift)
				  << root_mtd->erasesize_shift));
	if (readlen < PAGE_SIZE) {
		mtd_read(root_mtd, real_offset, readlen, &retlen, buf);
		real_offset = ubi_image_len + readlen;
		if (mtd_find_goodblk(root_mtd, &real_offset)) {
			ret = CHECK_FAIL;
			goto fail2;
		}
		mtd_read(root_mtd, real_offset, PAGE_SIZE - readlen, &retlen,
			 buf + readlen);
	} else {
		mtd_read(root_mtd, real_offset, PAGE_SIZE, &retlen, buf);
	}

	header = (struct ubick_header *)buf;
	if (memcmp(header->magic, "UBIC", 4) != 0) {
		pr_notice("[%s] header magic is not matched!\n", __func__);
		ret = CHECK_FAIL;
		goto fail2;
	}
	block_size = be32_to_cpu(header->block_size);
	block_interval = be32_to_cpu(header->block_interval);
	block_count = be32_to_cpu(header->block_count);

	block_size_order = get_order(block_size);
	pages = alloc_pages(GFP_KERNEL, block_size_order);
	if (pages == NULL) {
		pr_notice(
			"[%s]block_size order %d is too large,please reduce block_size\n",
			__func__, block_size_order);
		ret = CHECK_FAIL;
		goto fail2;
	}
	calculate_buffer = page_address(pages);

	tfm = crypto_alloc_shash("sha256", 0, 0);
	desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
	desc = kmalloc(desc_size, GFP_KERNEL);
	if (desc == NULL) {
		ret = CHECK_FAIL;
		goto fail2;
	}
	desc->tfm = tfm;
	crypto_shash_init(desc);

	if (block_count * (block_size + block_interval) - block_interval >
	    ubi_image_len) {
		pr_notice("[%s]error block parameters\n", __func__);
		ret = CHECK_FAIL;
		goto fail3;
	}

	for (i = 0; i < block_count; i++) {
		readlen = block_size;
		log_offset = (loff_t)i * (block_size + block_interval);
		real_offset = log_offset;
		while (1) {
			size_t bytes =
				min((size_t)(root_mtd->erasesize -
					     (log_offset -
					      ((log_offset >>
						root_mtd->erasesize_shift)
					       << root_mtd->erasesize_shift))),
				    readlen);
			if (mtd_find_goodblk(root_mtd, &real_offset)) {
				ret = CHECK_FAIL;
				goto fail3;
			}
			mtd_read(root_mtd, real_offset, bytes, &retlen,
				 calculate_buffer + (block_size - readlen));
			readlen -= bytes;
			if (!readlen)
				break;
			log_offset += bytes;
			real_offset = log_offset;
		}
		crypto_shash_update(desc, calculate_buffer, block_size);
	}

	crypto_shash_update(desc, (const u8 *)header,
			    sizeof(struct ubick_header) -
				    sizeof(header->rsa_sig));

	crypto_shash_final(desc, calculate_hash);

	decrypted_content =
		rsa_encryptdecrypt(header->rsa_sig, excepted_e, rsa_modulus);

	memcpy(pss_sig, decrypted_content, RSA_LEN);
	memcpy(pss_hash, calculate_hash, HASH_LEN);

	ret = pkcs_1_pss_decode_sha256(pss_hash, 32, pss_sig, RSA_LEN, 32, 2048, &res);
	if (ret)
		ret = CHECK_FAIL;
	else
		ret = CHECK_PASS;

	kfree(decrypted_content);
fail3:
	kfree(desc);
	crypto_free_shash(tfm);
	__free_pages(pages, block_size_order);
fail2:
	kfree(buf);
fail1:
	if (root_mtd)
		put_mtd_device(root_mtd);

	return ret;
}
#endif

static void hexdump(char *note, unsigned char *buf, unsigned int len)
{
	pr_info("%s", note);
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET,
			16, 1,
			buf, len, false);
}

static char saved_root_name[64] = { 0 };
static const char key_name[] = "root=";

int __init bdev_verity(dev_t rootfs_dev)
{
	struct file *root_file;
	size_t ubi_image_len;
	struct ubick_header *header;
	uint32_t block_size, block_interval, block_count;
	u_char *buf;
	uint32_t block_size_order;
	struct page *pages;
	uint8_t *calculate_buffer;
	uint8_t *decrypted_content;
	uint8_t excepted_e[4] = {0x00, 0x01, 0x00, 0x01};
	uint8_t calculate_hash[HASH_LEN];
	int i = 0;
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int desc_size = 0;
	int ret = CHECK_PASS;
	int err;
	loff_t real_offset, log_offset;
	size_t readlen;
	uint8_t pss_sig[RSA_LEN];
	uint8_t pss_hash[HASH_LEN];
	int res = 0;

	root_file = filp_open(saved_root_name, O_RDONLY, 0);
	if (IS_ERR(root_file)) {
		pr_notice("%s: open %s failed\n", saved_root_name, __func__);
		ret = CHECK_FAIL;
		goto fail0;
	}

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		ret = CHECK_FAIL;
		goto fail1;
	}

	real_offset = 0;
	kernel_read(root_file, buf, PAGE_SIZE, &real_offset);
	ubi_image_len = (*(unsigned long *)(buf + 0x400 + 0x4) &
					0xffffffff) *
					(1 << (10 + *(unsigned long *)(buf + 0x400 + 0x18)));

	real_offset = ubi_image_len;
	kernel_read(root_file, buf, PAGE_SIZE, &real_offset);

	header = (struct ubick_header *)buf;
	ret = memcmp(header->magic, "EXTC", sizeof("EXTC"));

	if (ret != 0) {
		pr_info("[%s] header magic is not matched!, ubi_image_len = 0x%x, magic = %s\n",
			__func__, ubi_image_len, header->magic);
		ret = CHECK_FAIL;
		goto fail2;
	}

	block_size = be32_to_cpu(header->block_size);
	block_interval = be32_to_cpu(header->block_interval);
	block_count = be32_to_cpu(header->block_count);

	block_size_order = get_order(block_size);
	pages = alloc_pages(GFP_KERNEL, block_size_order);
	if (pages == NULL) {
		pr_info(
			"[%s]block_size order %d is too large,please reduce block_size\n",
			__func__, block_size_order);
		ret = CHECK_FAIL;
		goto fail2;
	}
	calculate_buffer = page_address(pages);

	tfm = crypto_alloc_shash("sha256", 0, 0);
	desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
	desc = kmalloc(desc_size, GFP_KERNEL);
	if (desc == NULL) {
		ret = CHECK_FAIL;
		goto fail2;
	}
	desc->tfm = tfm;
	crypto_shash_init(desc);

	if (block_count * (block_size + block_interval) - block_interval >
	    ubi_image_len) {
		pr_info("[%s]error block parameters\n", __func__);
		ret = CHECK_FAIL;
		goto fail3;
	}

	for (i = 0; i < block_count; i++) {
		readlen = block_size;
		log_offset = (loff_t)i * (block_size + block_interval);
		real_offset = log_offset;
		kernel_read(root_file, calculate_buffer, readlen, &real_offset);
		crypto_shash_update(desc, calculate_buffer, block_size);
	}

	crypto_shash_update(desc, (const u8 *)header,
			    sizeof(struct ubick_header) -
				    sizeof(header->rsa_sig));

	crypto_shash_final(desc, calculate_hash);

	decrypted_content =
		rsa_encryptdecrypt(header->rsa_sig, excepted_e, rsa_modulus);

	memcpy(pss_sig, decrypted_content, RSA_LEN);
	memcpy(pss_hash, calculate_hash, HASH_LEN);

	ret = pkcs_1_pss_decode_sha256(pss_hash, 32, pss_sig, RSA_LEN, 32, 2048, &res);
	if (ret)
		ret = CHECK_FAIL;
	else
		ret = CHECK_PASS;

	kfree(decrypted_content);
fail3:
	kfree(desc);
	crypto_free_shash(tfm);
	__free_pages(pages, block_size_order);
fail2:
	kfree(buf);
fail1:
	fput(root_file);
fail0:

	return ret;
}

static int get_key_node(char *node, struct device_node *of_chosen)
{
	char *bootargs = NULL;
	char *name_start = NULL;
	char *name_end = NULL;

	bootargs = (char *)of_get_property(
				of_chosen, node, NULL);
	if (!bootargs)
		pr_info("%s: failed to get bootargs\n", __func__);
	else {
		pr_info("%s: bootargs: %s\n", __func__, bootargs);
		name_start = strstr(bootargs, key_name);
		if (name_start) {
			name_start = name_start + sizeof(key_name) - 1;
			name_end = strchr(name_start, ' ');
			if (name_end)
				memcpy(saved_root_name, name_start, name_end - name_start);
			else
				memcpy(saved_root_name, name_start, strlen(name_start));
			pr_info("%s: saved_root_name: %s\n", __func__, saved_root_name);
			if (strstr(saved_root_name, "sdc"))
				return 0;
			else
				return -1;
		} else {
			return -1;
		}
	}
}

static int get_root_name(void)
{
	struct device_node *of_chosen = NULL;
	const char *rootfs_check_enable;

	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		if (of_property_read_string(of_chosen, "rootfs_check,enable", &rootfs_check_enable) == 0) {
			if (strncmp(rootfs_check_enable, "yes", 3))
				goto err;
		} else {
			goto err;
		}
		if (get_key_node("bootargs_ext", of_chosen))
			get_key_node("bootargs", of_chosen);
	} else {
		pr_info("%s: failed to get /chosen\n", __func__);
		return -1;
	}

	return 0;

err:
	pr_info("%s: rootfs_check not enable\n", __func__);
	return -1;
}

static int __init rootfs_check_init(void)
{
#if IS_ENABLED(CONFIG_MTD)
	int rootfs_mtd_num;
#endif

	pr_info("[rootfs_check]checking rootfs integrity...\n");
#if IS_ENABLED(CONFIG_MTD)
	rootfs_mtd_num = get_rootfs_mtd_num();
	if (rootfs_mtd_num >= 0) {
		if (mtd_verity(get_rootfs_mtd_num()) != 0)
			goto verity_fail;
	} else
#endif
	{
		dev_t rootfs_dev;
		int error;
		struct block_device *rootfs_blk;

		if (get_root_name())
			return -1;
		pr_info("[rootfs_check] wait for %s...\n",
				saved_root_name);

		error = lookup_bdev(saved_root_name, &rootfs_dev);
		if (error) {
			pr_info("[rootfs_check]lookup_bdev failed %d\n", error);
			return ERR_PTR(error);
		}

		rootfs_blk = blkdev_get_by_dev(rootfs_dev,
			FMODE_READ|FMODE_WRITE,
			NULL, NULL);
		//set_disk_ro(rootfs_blk->bd_disk, 1);
		if (bdev_verity(rootfs_dev) != 0)
			goto verity_fail;
	}
	goto verity_pass;
verity_fail:
	panic("[rootfs_check] rootfs cannot pass integrity check,system hang...\n");
verity_pass:
	pr_info("[rootfs_check]rootfs pass integrity check,boot continue...\n");

	return 0;
}

static void rootfs_check_exit(void)
{
	;
}

module_init(rootfs_check_init)
module_exit(rootfs_check_exit)
MODULE_LICENSE("GPL");
