// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <asm/page.h>

#include <linux/stddef.h>
#include <uapi/drm/drm.h>

#include "dmabuf_phys_addr.h"

struct vhost_dmabuf {
	__u32 resource_id;
	int fd;
	struct mutex mutex;
	struct dma_buf *buf;
	struct sg_table *sgt;
	struct page **pages;
	size_t total_pages;
};

struct vhost_attachment {
	struct sg_table *sgt;
	enum dma_data_direction dir;
};

static int vhost_map_attach(struct dma_buf *dma_buf,
			    struct dma_buf_attachment *attach)
{
	struct vhost_attachment *attachment;

	attachment = kzalloc(sizeof(*attachment), GFP_KERNEL);
	if (!attach) {
		pr_err("[VHOST-DMABUF]failed to create vhost dmabuf attachment.\n");
		return -ENOMEM;
	}

	attachment->dir = DMA_NONE;
	attach->priv = attachment;

	return 0;
}

static void vhost_map_detach(struct dma_buf *dma_buf,
			     struct dma_buf_attachment *attach)
{
	struct vhost_attachment *attachment = attach->priv;
	struct sg_table *sgt;

	if (!attachment)
		return;

	sgt = attachment->sgt;
	if (sgt) {
		if (attachment->dir != DMA_NONE)
			dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents,
				     attachment->dir);
		sg_free_table(sgt);
		kfree(sgt);
	}

	kfree(attachment);
	attach->priv = NULL;
}

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	if (!table) {
		pr_err("[VHOST-DMABUF]%s invalid table\n", __func__);
		return ERR_PTR(-EFAULT);
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static struct sg_table *vhost_map_dma_buf(struct dma_buf_attachment *attach,
					  enum dma_data_direction dir)
{
	struct vhost_attachment *vhost_attach = attach->priv;
	struct dma_buf *dmabuf;
	struct vhost_dmabuf *vhost_dmabuf;
	struct sg_table *sgt;

	if (WARN_ON(dir == DMA_NONE || !vhost_attach))
		return ERR_PTR(-EINVAL);

	/* return the cached mapping when possible */
	if (vhost_attach->dir == dir)
		return vhost_attach->sgt;

	/*
	 * two mappings with different directions for the same attachment are
	 * not allowed
	 */
	if (WARN_ON(vhost_attach->dir != DMA_NONE))
		return ERR_PTR(-EBUSY);

	if (!attach->dmabuf || !attach->dmabuf->priv)
		return ERR_PTR(-ENODEV);

	dmabuf = attach->dmabuf;
	vhost_dmabuf = dmabuf->priv;
	sgt = dup_sg_table(vhost_dmabuf->sgt);
	if (!sgt) {
		pr_err("[VHOST-DMABUF]failed to dun sg table\n");
		return ERR_PTR(-ENOMEM);
	}

	if (!dma_map_sg(attach->dev, sgt->sgl, sgt->nents, dir)) {
		sg_free_table(sgt);
		kfree(sgt);
		sgt = ERR_PTR(-ENOMEM);
		dev_err(attach->dev, "failed to map sg table\n");
	} else {
		vhost_attach->sgt = sgt;
		vhost_attach->dir = dir;
	}

	return sgt;
}

static void vhost_unmap_dma_buf(struct dma_buf_attachment *attach,
				struct sg_table *sgt,
				enum dma_data_direction dir)
{
	/* nothing to be done here */
}

static unsigned int vhost_dmabuf_mmap_fault(struct vm_fault *vmf)
{
	struct vhost_dmabuf *vhost_dmabuf;
	struct dma_buf *dmabuf;
	struct page *pageptr;
	struct sg_page_iter sg_iter;
	pgoff_t pgoff;

	dmabuf = vmf->vma->vm_private_data;
	vhost_dmabuf = dmabuf->priv;
	pgoff = vmf->pgoff;

	__sg_page_iter_start(&sg_iter, vhost_dmabuf->sgt->sgl,
			     vhost_dmabuf->sgt->nents, pgoff);
	if (!__sg_page_iter_next(&sg_iter)) {
		pr_err("[VHOST-DMABUF]failed to get memory fault page.\n");
		return VM_FAULT_SIGBUS;
	}

	pageptr = sg_page_iter_page(&sg_iter);

	if (!pageptr) {
		pr_err("[VHOST-DMABUF]pageptr is NULL.\n");
		return -ENOMEM;
	}

	get_page(pageptr);
	vmf->page = pageptr;

	return 0;
}

static const struct vm_operations_struct vhost_dmabuf_vm_ops = {
	.fault = vhost_dmabuf_mmap_fault
};

static int vhost_dmabuf_mmap(struct dma_buf *dma_buf,
			     struct vm_area_struct *vma)
{
	vm_flags_set(vma, VM_MIXEDMAP | VM_IO | VM_DONTEXPAND);
	vma->vm_ops = &vhost_dmabuf_vm_ops;
	vma->vm_private_data = dma_buf;
	return 0;
}

static int vhost_dmabuf_vmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
	struct vhost_dmabuf *data = dma_buf->priv;
	void *vaddr;
	int ret = 0;

	vaddr = vm_map_ram(data->pages, data->total_pages, 0);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		return ret;
	}

	iosys_map_set_vaddr(map, vaddr);

	return ret;
}

static void vhost_dmabuf_vunmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
	struct vhost_dmabuf *data = dma_buf->priv;

	vm_unmap_ram(map->vaddr, data->total_pages);
}

static int vhost_dmabuf_destroy(struct vhost_dmabuf *dmabuf)
{
	if (!dmabuf)
		return 0;

	if (dmabuf->sgt) {
		sg_free_table(dmabuf->sgt);
		kfree(dmabuf->sgt);
	}

	kfree(dmabuf->pages);

	kfree(dmabuf);

	return 0;
}

static void vhost_dmabuf_release(struct dma_buf *buf)
{
	vhost_dmabuf_destroy(buf->priv);
}

static const struct dma_buf_ops vhost_dmabuf_ops = {
	.attach = vhost_map_attach,
	.detach = vhost_map_detach,
	.map_dma_buf = vhost_map_dma_buf,
	.unmap_dma_buf = vhost_unmap_dma_buf,
	.mmap = vhost_dmabuf_mmap,
	.vmap = vhost_dmabuf_vmap,
	.vunmap = vhost_dmabuf_vunmap,
	.release = vhost_dmabuf_release,
};
#ifdef USE_VM_ADDR
/* to-do: need modification to support 4G DRAM */
static phys_addr_t dmabuf_user_v2p(unsigned long va)
{
	unsigned long pageOffset = (va & (PAGE_SIZE - 1));
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	phys_addr_t pa;

	if (current == NULL) {
		pr_err("[VHOST-DMABUF]warning: %s, current is NULL!\n", __func__);
		return 0;
	}
	if (current->mm == NULL) {
		pr_err(
		       "[VHOST-DMABUF]%s, current->mm is NULL! tgid=0x%x, name=%s\n",
			__func__, current->tgid, current->comm);
		return 0;
	}

	pgd = pgd_offset(current->mm, va); /* what is tsk->mm */
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		pr_err("[VHOST-DMABUF]%s(), va=0x%lx, pgd invalid!\n", __func__,
				 va);
		return 0;
	}

	p4d = p4d_offset(pgd, va);
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		pr_err("[VHOST-DMABUF]%s(), va=0x%lx, p4d invalid!\n", __func__,
				 va);
		return 0;
	}

	pud = pud_offset(p4d, va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		pr_err("[VHOST-DMABUF]%s(), va=0x%lx, pud invalid!\n", __func__,
				 va);
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		pr_err("[VHOST-DMABUF]%s(), va=0x%lx, pmd invalid!\n", __func__,
				 va);
		return 0;
	}

	pte = pte_offset_map(pmd, va);
	if (pte_present(*pte)) {
		/* pa=(pte_val(*pte) & (PAGE_MASK)) | pageOffset; */
		pa = (pte_val(*pte) & (PHYS_MASK) & (~((phys_addr_t)0xfff))) |
		     pageOffset;
		pte_unmap(pte);
		return pa;
	}

	pte_unmap(pte);

	pr_err("[VHOST-DMABUF]%s(), va=0x%lx, pte invalid!\n", __func__, va);
	return 0;
}
#endif
static struct vhost_dmabuf *vhost_dmabuf_create(u32 resource_id, int nents,
					 struct virtio_dmabuf_mem_entry *ents)
{
	struct vhost_dmabuf *dmabuf;
	phys_addr_t paddr;
	struct page *page;
	struct scatterlist *sgl;
	struct sg_page_iter sg_iter;

	int ret;
	size_t total = 0;
	int i;

	struct dma_buf_export_info exp_info;

	dmabuf = kmalloc(sizeof(struct vhost_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return ERR_PTR(-ENOMEM);

	dmabuf->resource_id = resource_id;
	dmabuf->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!dmabuf->sgt) {
		ret = -ENOMEM;
		pr_err("[VHOST-DMABUF]failed to create sg table.\n");
		goto out_vhost_dmabuf;
	}

	ret = sg_alloc_table(dmabuf->sgt, nents, GFP_KERNEL);
	if (unlikely(ret)) {
		ret = -ENOMEM;
		pr_err("[VHOST-DMABUF]failed to alloc table.\n");
		goto out_sgt;
	}

	sgl = dmabuf->sgt->sgl;
	for (i = 0; i < nents; i++) {
		paddr = ents[i].addr;
/*
 *		paddr = dmabuf_user_v2p(ents[i].addr);
 *		if (paddr == 0) {
 *			pr_err(
 *				"failed to page for address:%llx, ret:%d\n",
 *				ents[i].addr, ret);
 *			goto out_sgt_sgl;
 *		}
 */
		page = phys_to_page(paddr);
		sg_set_page(sgl, page, ents[i].length, 0);
		sgl = sg_next(sgl);
		total += ents[i].length;
	}

	dmabuf->total_pages = total / PAGE_SIZE;
	dmabuf->pages = kmalloc_array(dmabuf->total_pages, sizeof(struct page *),
				GFP_KERNEL);
	if (!dmabuf->pages) {
		ret = -ENOMEM;
		pr_err("[VHOST-DMABUF]failed to alloc page list.\n");
		goto out_sgt_sgl;
	}

	i = 0;
	for_each_sg_page(dmabuf->sgt->sgl, &sg_iter, dmabuf->total_pages, 0) {
		dmabuf->pages[i++] = sg_page_iter_page(&sg_iter);
	}

	exp_info.exp_name = "Vhost DmaBuf";
	exp_info.owner = NULL;
	exp_info.ops = &vhost_dmabuf_ops;
	exp_info.size = total;
	exp_info.flags = DRM_RDWR;
	exp_info.priv = dmabuf;


	dmabuf->buf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf->buf)) {
		ret = PTR_ERR(dmabuf->buf);
		pr_err("[VHOST-DMABUF]failed to export vhost dmabuf.\n");
		goto out_free_pages;
	}

	dmabuf->fd = -1;
	mutex_init(&dmabuf->mutex);

	return dmabuf;

out_free_pages:
	kfree(dmabuf->pages);

out_sgt_sgl:
	sg_free_table(dmabuf->sgt);

out_sgt:
	kfree(dmabuf->sgt);

out_vhost_dmabuf:
	kfree(dmabuf);

	return ERR_PTR(ret);
}

static int vhost_dmabuf_fd(struct vhost_dmabuf *dmabuf)
{
	if (!dmabuf)
		return -EFAULT;

	return dma_buf_fd(dmabuf->buf, O_CLOEXEC);
}

static long exporter_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct export_dmabuf export;
	struct vhost_dmabuf *dmabuf;
	struct virtio_dmabuf_mem_entry *entries;
	int ret = -EFAULT;
	int fd;

	ret = -EFAULT;
	if (copy_from_user(&export, (void __user *)arg, sizeof(export))) {
		pr_err("[VHOST-DMABUF]failed to copy param from user.\n");
		goto out;
	}

	if (export.nr_entries == 0) {
		pr_err("[VHOST-DMABUF]invalid param: no valid entry.\n");
		return -EINVAL;
	}

	if (export.nr_entries > 5600) {
		pr_err("[VHOST-DMABUF]too many memory entries.\n");
		return -EINVAL;
	}

	entries = kmalloc(sizeof(*entries) * export.nr_entries, GFP_KERNEL);
	if (entries == NULL) {
		pr_err("[VHOST-DMABUF]failed to kmalloc.\n");
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(entries, (void __user *)export.entries,
						sizeof(*entries) * export.nr_entries)) {
		pr_err("[VHOST-DMABUF]failed to copy entries.\n");
		goto out_free_entries;
	}

	dmabuf = vhost_dmabuf_create(/*resource_id=*/0, export.nr_entries,
				     entries);
	if (IS_ERR(dmabuf)) {
		pr_err("[VHOST-DMABUF]failed to do vhost_dmabuf_create:%ld\n",
			PTR_ERR(dmabuf));
		ret = PTR_ERR(dmabuf);
		goto out_free_entries;
	}

	fd = vhost_dmabuf_fd(dmabuf);
	if (fd < 0) {
		pr_err("[VHOST-DMABUF]failed to do vhost_dmabuf_fd:%d\n", fd);
		goto out_release_dmabuf;
	}

	export.fd = fd;

	if (copy_to_user((void __user *)arg, &export, sizeof(export))) {
		pr_err("[VHOST-DMABUF]failed to copy param to user.\n");
		goto out_release_dmabuf;
	}

	kfree(entries);
	return 0;

out_release_dmabuf:
	vhost_dmabuf_destroy(dmabuf);

out_free_entries:
	kfree(entries);
out:
	return ret;
}

static int exporter_release(struct inode *inode, struct file *f)
{
	return 0;
}

static const struct file_operations exporter_fops = {
	.owner = THIS_MODULE,
	.release = exporter_release,
	.unlocked_ioctl = exporter_ioctl,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vhost_dmabuf_exporter_phys",
	.fops = &exporter_fops,
};

static int __init exporter_init(void)
{
	return misc_register(&mdev);
}

module_init(exporter_init)
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
