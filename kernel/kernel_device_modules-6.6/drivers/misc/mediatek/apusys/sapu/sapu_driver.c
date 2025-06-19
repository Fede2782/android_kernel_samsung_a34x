// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "sapu_plat.h"
#include "mtk-smmu-v3.h"

static int sapu_ha_bridge(struct sapu_mem_info *mem_info,
			  struct sapu_ha_tranfer *ha_transfer)
{
	union MTEEC_PARAM param[4];
	TZ_RESULT ret;

	KREE_SESSION_HANDLE ha_sess = 0;

	pr_info("[SAPU_LOG] KREE_CreateSession\n");
	ret = KREE_CreateSession(mem_info->haSrvName, &ha_sess);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]CreateSession ha_sess fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	param[0].mem.size = sizeof(*ha_transfer);
	param[0].mem.buffer = (void *)ha_transfer;

	pr_info("[SAPU_LOG] KREE_TeeServiceCall\n");
	ret = KREE_TeeServiceCall(ha_sess, mem_info->command,
				  TZ_ParamTypes2(TZPT_MEM_INPUT,
				  TZPT_VALUE_OUTPUT), param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]TeeServiceCall fail(%d)\n", __func__, ret);
		return -EPIPE;
	}

	// return code
	pr_info("[SAPU_LOG] param[1].value.a = %x", param[1].value.a);

	pr_info("[SAPU_LOG] KREE_CloseSession\n");
	ret = KREE_CloseSession(ha_sess);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("[%s]ha_sess(0x%x) CloseSession fail(%d)\n",
			__func__, ha_sess, ret);
		return -EPIPE;
	}

	return 0;
}

static int get_secure_handle(struct sapu_mem_info *mem_info,
			     uint64_t *sec_handle)
{
	struct dma_buf *mem_dmabuf = NULL;

	pr_info("%s fd=%d\n", __func__, mem_info->fd);

	mem_dmabuf = dma_buf_get(mem_info->fd);
	if (!mem_dmabuf || IS_ERR(mem_dmabuf)) {
		pr_info("dma_buf_get error %d\n", __LINE__);
		return -EINVAL;
	}

	*sec_handle = dmabuf_to_secure_handle(mem_dmabuf);
	if (*sec_handle == 0) {
		pr_info("dmabuf_to_secure_handle failed!\n");
		return -EINVAL;
	}

	pr_info("%s secure_handle=%llx\n", __func__, *sec_handle);

	dma_buf_put(mem_dmabuf);
	return 0;
}
static int get_apu_iova(struct sapu_mem_info *mem_info,
			dma_addr_t *dma_addr)
{
	int ret = 0;
	struct sapu_private *sapu_priv;
	struct platform_device *pdev;
	struct platform_device *smmu_dev = NULL;
	struct device *sapu_dev;
	struct dma_buf *mem_dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct device_node *sapu_node;
	struct device_node *smmu_node = NULL;
	struct sg_table *sgt;

	pr_info("%s fd=%d\n", __func__, mem_info->fd);

	sapu_priv = get_sapu_private();
	if (sapu_priv == NULL) {
		pr_info("%s %d: sapu_priv is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	pdev = sapu_priv->pdev;
	if (pdev == NULL) {
		pr_info("%s %d: pdev is NULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	sapu_dev = &pdev->dev;

	mem_dmabuf = dma_buf_get(mem_info->fd);
	if (!mem_dmabuf || IS_ERR(mem_dmabuf)) {
		pr_info("dma_buf_get error %d\n", __LINE__);
		return -ENODEV;
	}

	if (smmu_v3_enabled()) {
		sapu_node = sapu_dev->of_node;
		if (!sapu_node) {
			pr_info("%s sapu_node is NULL\n", __func__);
			return -ENODEV;
		}

		smmu_node = of_parse_phandle(sapu_node, "smmu-device", 0);
		if (!smmu_node) {
			pr_info("%s get smmu_node failed\n", __func__);
			return -ENODEV;
		}

		smmu_dev = of_find_device_by_node(smmu_node);
		if (!smmu_dev) {
			pr_info("%s get smmu_dev failed\n", __func__);
			return -ENODEV;
		}

		attach = dma_buf_attach(mem_dmabuf, &smmu_dev->dev);
	} else {
		attach = dma_buf_attach(mem_dmabuf, sapu_dev);
	}
	if (IS_ERR(attach)) {
		pr_info("%s attach fail\n", __func__);
		ret = -EINVAL;
		goto datamem_dmabuf_put;
	}

	sgt = dma_buf_map_attachment_unlocked(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		pr_info("%s map failed, detach and return\n", __func__);
		ret = -EINVAL;
		goto datamem_dmabuf_detach;
	}

	*dma_addr = sg_dma_address(sgt->sgl);

	pr_info("%s dma_addr=%llx\n", __func__, *dma_addr);

	dma_buf_unmap_attachment_unlocked(attach, sgt, DMA_BIDIRECTIONAL);
datamem_dmabuf_detach:
	dma_buf_detach(mem_dmabuf, attach);
datamem_dmabuf_put:
	dma_buf_put(mem_dmabuf);
	return ret;
}

long apusys_sapu_internal_ioctl(struct file *filep, unsigned int cmd, void __user *arg,
	unsigned int compat)
{
	int ret;
	struct sapu_private *sapu_priv;
	struct sapu_mem_info mem_info;
	struct sapu_ha_tranfer ha_transfer;
	uint32_t power_on;

	(void)compat;

	if (_IOC_TYPE(cmd) != APUSYS_SAPU_IOC_MAGIC) {
		pr_info("%s %d -EINVAL\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
	case APUSYS_POWER_CONTROL:
		pr_info("%s APUSYS_POWER_CONTROL 0x%x\n", __func__, cmd);
		ret = copy_from_user(&power_on, arg, sizeof(u32));
		if (ret) {
			pr_info("[%s]copy_from_user failed\n", __func__);
			return ret;
		}

		sapu_priv = get_sapu_private();
		if (sapu_priv == NULL) {
			pr_info("%s %d\n", __func__, __LINE__);
			return 0;
		}
		sapu_priv->platdata->ops.power_ctrl(sapu_priv, power_on);
		break;
	case APUSYS_SAPU_DATAMEM:
		pr_info("%s APUSYS_SAPU_DATAMEM 0x%x\n", __func__, cmd);
		ret = copy_from_user(&mem_info, arg, sizeof(mem_info));
		if (ret) {
			pr_info("[%s]copy_from_user failed\n", __func__);
			return ret;
		}
		ret = get_secure_handle(&mem_info, &ha_transfer.sec_handle);
		if (ret) {
			pr_info("[%s]get_secure_handle failed\n", __func__);
			return ret;
		}

		ret = get_apu_iova(&mem_info, &ha_transfer.dma_addr);
		if (ret) {
			pr_info("[%s]get_apu_iova failed\n", __func__);
			return ret;
		}

		ret = sapu_ha_bridge(&mem_info, &ha_transfer);
		if (ret)
			pr_info("%s call to HA failed (%d)\n", __func__, ret);
		break;
	case APUSYS_SAPU_TEMPMEM:
		pr_info("%s APUSYS_SAPU_TEMPMEM 0x%x\n", __func__, cmd);
		ret = copy_from_user(&mem_info, arg, sizeof(mem_info));
		if (ret) {
			pr_info("[%s]copy_from_user failed\n", __func__);
			return ret;
		}

		ha_transfer.sec_handle = 0;
		// get_secure_handle(&mem_info, &ha_transfer.sec_handle);
		ret = get_apu_iova(&mem_info, &ha_transfer.dma_addr);
		if (ret) {
			pr_info("[%s]get_apu_iova failed\n", __func__);
			return ret;
		}

		ret = sapu_ha_bridge(&mem_info, &ha_transfer);
		if (ret)
			pr_info("%s call to HA failed (%d)\n", __func__, ret);
		break;
	case APUSYS_SAPU_MODELCHUNK:
		pr_info("%s APUSYS_SAPU_MODELCHUNK 0x%x\n", __func__, cmd);
		ret = copy_from_user(&mem_info, arg, sizeof(mem_info));
		if (ret) {
			pr_info("[%s]copy_from_user failed\n", __func__);
			return ret;
		}

		get_secure_handle(&mem_info, &ha_transfer.sec_handle);
		if (ret) {
			pr_info("[%s]get_secure_handle failed\n", __func__);
			return ret;
		}

		ha_transfer.dma_addr = 0;

		ret = sapu_ha_bridge(&mem_info, &ha_transfer);
		if (ret)
			pr_info("%s call to HA failed (%d)\n", __func__, ret);
		break;
	default:
		pr_info("%s unknown 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	break;
	}

	return ret;
}

MODULE_IMPORT_NS(DMA_BUF);
