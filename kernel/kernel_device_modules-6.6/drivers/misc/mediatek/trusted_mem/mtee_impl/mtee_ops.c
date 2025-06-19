// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#if IS_ENABLED(CONFIG_MTK_GZ_KREE)
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include <tz_cross/ta_mem.h>
#endif
#if IS_ENABLED(CONFIG_MTK_PKVM_TMEM)
#include <asm/kvm_pkvm_module.h>
#include "../../../misc/mediatek/include/pkvm_mgmt/pkvm_mgmt.h"
#endif
#if IS_ENABLED(CONFIG_MTK_PKVM_SMMU)
#include <asm/kvm_pkvm_module.h>
#include "../../../misc/mediatek/include/pkvm_mgmt/pkvm_mgmt.h"
#endif

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_dev_desc.h"
#include "public/mtee_regions.h"
/* clang-format off */
#include "mtee_impl/mtee_ops.h"
#include "mtee_impl/tmem_ffa.h"
/* clang-format on */
#include "tee_impl/tee_invoke.h"
#include "memory_ssmr.h"

#define LOCK_BY_CALLEE (0)
#if LOCK_BY_CALLEE
#define MTEE_SESSION_LOCK_INIT() mutex_init(&sess_data->lock)
#define MTEE_SESSION_LOCK() mutex_lock(&sess_data->lock)
#define MTEE_SESSION_UNLOCK() mutex_unlock(&sess_data->lock)
#else
#define MTEE_SESSION_LOCK_INIT()
#define MTEE_SESSION_LOCK()
#define MTEE_SESSION_UNLOCK()
#endif

static const char mem_srv_name[] = "com.mediatek.geniezone.srv.mem";

struct MTEE_SESSION_DATA {
	KREE_SESSION_HANDLE session_handle;
	KREE_SECUREMEM_HANDLE append_mem_handle;
#if LOCK_BY_CALLEE
	struct mutex lock;
#endif
};

/*
 * This combination is pKVM & FF-A and GZ is disable.
 */
static int pkvm_mtee_alloc(u32 alignment, u32 size, u32 *refcount, u64 *sec_handle,
		      u8 *owner, u32 id, u32 clean, void *peer_data,
		      void *dev_desc)
{
	int ret;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;

	if (is_ffa_enabled()) {
		ret = tmem_ffa_region_alloc(mtee_dev_desc->mtee_chunks_id,
				size, alignment, sec_handle);
		if (*sec_handle == 0) {
			pr_info("tmem_ffa_region_alloc,  out of memory, ret=%d!\n",  ret);
			return -ENOMEM;
		} else if (ret != 0) {
			pr_info("[%d] tmem_ffa_region_alloc failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			return TMEM_KPOOL_ALLOC_CHUNK_FAILED;
		}
		*refcount = 1;
	}

	return TMEM_OK;
}

/*
 * This combination is pKVM & FF-A and GZ is disable.
 */
static int pkvm_mtee_free(u64 sec_handle, u8 *owner, u32 id, void *peer_data,
		     void *dev_desc)
{
	int ret;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;

	if (is_ffa_enabled()) {
		ret = tmem_ffa_region_free(mtee_dev_desc->mtee_chunks_id, sec_handle);
		if (ret != 0) {
			pr_info("[%d] tmem_ffa_region_free failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			return TMEM_KPOOL_ALLOC_CHUNK_FAILED;
		}
	}

	return TMEM_OK;
}

/* According to lock, map those CMA region into mtk pkvm smmu page table with
 * related permission.
 */
static void pkvm_smmu_region_mapping(u64 region_start, u32 region_size,
				     u32 region_id, u32 lock)
{
#if IS_ENABLED(CONFIG_MTK_PKVM_SMMU)
	static uint32_t hvc_id_map;
	static uint32_t hvc_id_unmap;
	struct arm_smccc_res res;
	uint32_t hvc_id;
	int ret;

	if (!is_pkvm_enabled()) {
		pr_info("pKVM is not enabled\n");
		return;
	}

	if (!hvc_id_map) {
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SMMU_SEC_REGION_MAP,
			0, 0, 0, 0, 0, 0, &res);
		hvc_id_map = res.a1;
	}

	if (!hvc_id_unmap) {
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SMMU_SEC_REGION_UNMAP,
			0, 0, 0, 0, 0, 0, &res);
		hvc_id_unmap = res.a1;
	}

	hvc_id = (lock == 1) ? hvc_id_map : hvc_id_unmap;

	if (hvc_id != 0) {
		ret = pkvm_el2_mod_call(hvc_id, region_start, region_size,
					region_id);

		if (ret != 0)
			pr_info("hvc_id=%#x smmu_ret=%x\n", hvc_id, ret);
	} else
		pr_info("%s hvc is invalid\n", __func__);
#endif
}

void flush_region_mem(u64 pa, u32 size)
{
	struct sg_table table;
	struct device *dev = get_ssmr_dev();

	sg_alloc_table(&table, 1, GFP_KERNEL);
	sg_set_page(table.sgl, phys_to_page(pa), size, 0);
	table.sgl->dma_address = pa;

	/*
	 * For region uncached buffers, we need to initially flush cpu cache,
	 * since the __GFP_ZERO on the allocation means the zeroing was done by
	 * the cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 * To flush cpu cache should be after memory allocation and before
	 * hypervisor protection.
	 */
	dma_map_sgtable(dev, &table, DMA_BIDIRECTIONAL, 0);
	dma_unmap_sgtable(dev, &table, DMA_BIDIRECTIONAL, 0);
	sg_free_table(&table);
}

/*
 * This combination is pKVM & FF-A and GZ is disable.
 */
static int pkvm_mtee_mem_reg_add(u64 pa, u32 size, void *peer_data, void *dev_desc)
{
	int ret;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;

	MTEE_SESSION_LOCK();

	pkvm_smmu_region_mapping(pa, size, mtee_dev_desc->mtee_chunks_id, 1);
#if IS_ENABLED(CONFIG_MTK_PKVM_TMEM)
	if (is_pkvm_enabled()) {
		struct arm_smccc_res res;
		uint32_t hvc_id;
		int ret;

		flush_region_mem(pa, size);
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_TMEM_REGION_PROTECT, 0, 0, 0, 0, 0,
						0, &res);
		hvc_id = res.a1;
		if (hvc_id != 0) {
			ret = pkvm_el2_mod_call(hvc_id,
						mtee_dev_desc->mtee_chunks_id, pa, size);
			if (ret != 0)
				pr_info("pKVM append reg mem failed:%d\n", ret);
		} else
			pr_info("%s: hvc is invalid\n", __func__);

	}
#endif

	if (is_ffa_enabled()) {
		ret = tmem_carveout_create(mtee_dev_desc->mtee_chunks_id, pa, size);
		if (ret != 0) {
			pr_info("[%d] tmem_carveout_create failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_KPOOL_APPEND_MEMORY_FAILED;
		}

		pr_info("[%d] tmem_ff_heap[%d] created PASS: PA=0x%llx, size=0x%x\n",
				mtee_dev_desc->kern_tmem_type, mtee_dev_desc->mtee_chunks_id,
				pa, size);
	}

	MTEE_SESSION_UNLOCK();

	return TMEM_OK;
}

/*
 * This combination is pKVM & FF-A and GZ is disable.
 */
static int pkvm_mtee_mem_reg_remove(void *peer_data, void *dev_desc)
{
	int ret;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;

	MTEE_SESSION_LOCK();

#if IS_ENABLED(CONFIG_MTK_PKVM_TMEM)
	if (is_pkvm_enabled()) {
		struct arm_smccc_res res;
		uint32_t hvc_id;
		int ret;

		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_TMEM_REGION_UNPROTECT, 0, 0, 0, 0, 0,
						0, &res);
		hvc_id = res.a1;
		if (hvc_id != 0) {
			ret = pkvm_el2_mod_call(hvc_id, mtee_dev_desc->mtee_chunks_id);
			if (ret != 0)
				pr_info("pKVM release reg mem failed:%d\n", ret);
		} else
			pr_info("%s: hvc is invalid\n", __func__);
	}
#endif

	pkvm_smmu_region_mapping(0, 0, mtee_dev_desc->mtee_chunks_id, 0);

	if (is_ffa_enabled()) {
		ret = tmem_carveout_destroy(mtee_dev_desc->mtee_chunks_id);
		if (ret != 0) {
			pr_info("[%d] tmem_carveout_destroy failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_KPOOL_APPEND_MEMORY_FAILED;
		}

		pr_info("[%d] tmem_ffa_heap[%d] destroy PASS\n",
				mtee_dev_desc->kern_tmem_type, mtee_dev_desc->mtee_chunks_id);
	}

	MTEE_SESSION_UNLOCK();

	return TMEM_OK;
}

static struct MTEE_SESSION_DATA *
mtee_create_session_data(enum TRUSTED_MEM_TYPE mem_type)
{
	struct MTEE_SESSION_DATA *sess_data;

	sess_data = mld_kmalloc(sizeof(struct MTEE_SESSION_DATA), GFP_KERNEL);
	if (INVALID(sess_data)) {
		pr_info("%s:%d %d:out of memory!\n", __func__, __LINE__,
		       mem_type);
		return NULL;
	}

	memset(sess_data, 0x0, sizeof(struct MTEE_SESSION_DATA));

	MTEE_SESSION_LOCK_INIT();
	return sess_data;
}

static void mtee_destroy_session_data(struct MTEE_SESSION_DATA *sess_data)
{
	if (VALID(sess_data))
		mld_kfree(sess_data);
}

static int mtee_session_open(void **peer_data, void *dev_desc)
{
	int ret = 0;
	struct MTEE_SESSION_DATA *sess_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(dev_desc);

	if (is_pkvm_enabled())
		return TMEM_OK;

	sess_data = mtee_create_session_data(mtee_dev_desc->kern_tmem_type);
	if (INVALID(sess_data)) {
		pr_info("[%d] Create session data failed: out of memory!\n",
		       mtee_dev_desc->kern_tmem_type);
		return TMEM_MTEE_CREATE_SESSION_FAILED;
	}

	MTEE_SESSION_LOCK();

	if (unlikely(ops_data->service_name))
		ret = KREE_CreateSession(ops_data->service_name,
					 &sess_data->session_handle);
	else
		ret = KREE_CreateSession(mem_srv_name,
					 &sess_data->session_handle);
	if (ret != 0) {
		pr_info("[%d] MTEE open session failed:%d (srv=%s)\n",
		       mtee_dev_desc->kern_tmem_type, ret,
		       (ops_data->service_name ? ops_data->service_name
					       : mem_srv_name));
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_CREATE_SESSION_FAILED;
	}

	*peer_data = (void *)sess_data;
	MTEE_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mtee_session_close(void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);

	if (is_pkvm_enabled())
		return TMEM_OK;

	MTEE_SESSION_LOCK();

	ret = KREE_CloseSession(sess_data->session_handle);
	if (ret != 0) {
		pr_info("[%d] MTEE close session failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_CLOSE_SESSION_FAILED;
	}

	MTEE_SESSION_UNLOCK();
	mtee_destroy_session_data(sess_data);
	return TMEM_OK;
}

static int mtee_alloc(u32 alignment, u32 size, u32 *refcount, u64 *sec_handle,
		      u8 *owner, u32 id, u32 clean, void *peer_data,
		      void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	if (is_pkvm_enabled()) {
		/* pKVM & FF-A */
		ret = pkvm_mtee_alloc(alignment, size, refcount, sec_handle, owner, id,
				clean, peer_data, dev_desc);
		return ret;
	}

	/* GZ & FF-A */
	if (is_ffa_enabled()) {
		ret = tmem_ffa_region_alloc(mtee_dev_desc->mtee_chunks_id,
				size, alignment, sec_handle);
		if (*sec_handle == 0) {
			pr_info("tmem_ffa_region_alloc,  out of memory, ret=%d!\n",  ret);
			return -ENOMEM;
		} else if (ret != 0) {
			pr_info("[%d] tmem_ffa_region_alloc failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			return TMEM_KPOOL_ALLOC_CHUNK_FAILED;
		}
		*refcount = 1;
	} else {
		UNUSED(ops_data);
		MTEE_SESSION_LOCK();

		if (clean) {
			ret = KREE_ION_ZallocChunkmem(sess_data->session_handle,
						      sess_data->append_mem_handle,
						      (u32 *)sec_handle, alignment, size);
		} else {
			ret = KREE_ION_AllocChunkmem(sess_data->session_handle,
						     sess_data->append_mem_handle,
						     (u32 *)sec_handle, alignment, size);
		}

		if (*sec_handle == 0) {
			pr_info("%s:%d out of memory, ret=%d!\n", __func__, __LINE__, ret);
			MTEE_SESSION_UNLOCK();
			return -ENOMEM;
		} else if (ret != 0) {
			pr_info("[%d] MTEE alloc chunk memory failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_MTEE_ALLOC_CHUNK_FAILED;
		}

		*refcount = 1;
		MTEE_SESSION_UNLOCK();
	}

	return TMEM_OK;
}

static int mtee_free(u64 sec_handle, u8 *owner, u32 id, void *peer_data,
		     void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	if (is_pkvm_enabled()) {
		/* pKVM & FF-A */
		ret = pkvm_mtee_free(sec_handle, owner, id, peer_data, dev_desc);
		return ret;
	}

	/* GZ & FF-A */
	if (is_ffa_enabled()) {
		ret = tmem_ffa_region_free(mtee_dev_desc->mtee_chunks_id, sec_handle);
		if (ret != 0) {
			pr_info("[%d] tmem_ffa_region_free failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			return TMEM_KPOOL_ALLOC_CHUNK_FAILED;
		}
	} else {
		UNUSED(ops_data);
		MTEE_SESSION_LOCK();

		ret = KREE_ION_UnreferenceChunkmem(sess_data->session_handle,
						   (u32) sec_handle);
		if (ret != 0) {
			pr_info("[%d] MTEE free chunk memory failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_MTEE_FREE_CHUNK_FAILED;
		}

		MTEE_SESSION_UNLOCK();
	}

	return TMEM_OK;
}

static int mtee_mem_reg_add(u64 pa, u32 size, void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;
	KREE_SHAREDMEM_PARAM mem_param;

	if (is_pkvm_enabled()) {
		/* pKVM & FF-A */
		ret = pkvm_mtee_mem_reg_add(pa, size, peer_data, dev_desc);
		return ret;
	}

	/* GZ & FF-A */
	mem_param.buffer = (void *)pa;
	mem_param.size = size;
	mem_param.mapAry = NULL;
	mem_param.region_id = mtee_dev_desc->mtee_chunks_id;

	UNUSED(ops_data);
	MTEE_SESSION_LOCK();

	ret = KREE_AppendSecureMultichunkmem(sess_data->session_handle,
					     &sess_data->append_mem_handle,
					     &mem_param);
	if (ret != 0) {
		pr_info("[%d] MTEE append reg mem failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_APPEND_MEMORY_FAILED;
	}

	pr_info("[%d] MTEE append reg mem PASS: PA=0x%llx, size=0x%x\n",
				mtee_dev_desc->kern_tmem_type, pa, size);

	if (mtee_dev_desc->notify_remote && mtee_dev_desc->notify_remote_fn) {
		ret = mtee_dev_desc->notify_remote_fn(
			pa, size, mtee_dev_desc->tee_smem_type);
		if (ret != 0) {
			pr_info("[%d] MTEE notify reg mem add to TEE failed:%d\n",
			       mtee_dev_desc->tee_smem_type, ret);
//			MTEE_SESSION_UNLOCK();
//			return TMEM_MTEE_NOTIFY_MEM_ADD_CFG_TO_TEE_FAILED;
		}
	}

	if (is_ffa_enabled()) {
		ret = tmem_carveout_create(mtee_dev_desc->mtee_chunks_id, pa, size);
		if (ret != 0) {
			pr_info("[%d] tmem_carveout_create failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_KPOOL_APPEND_MEMORY_FAILED;
		}

		pr_info("[%d] tmem_ff_heap[%d] created PASS: PA=0x%llx, size=0x%x\n",
				mtee_dev_desc->kern_tmem_type, mtee_dev_desc->mtee_chunks_id,
				pa, size);
	}

	MTEE_SESSION_UNLOCK();

	return TMEM_OK;
}

static int mtee_mem_reg_remove(void *peer_data, void *dev_desc)
{
	int ret;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;

	UNUSED(ops_data);

	if (is_pkvm_enabled()) {
		/* pKVM & FF-A */
		ret = pkvm_mtee_mem_reg_remove(peer_data, dev_desc);
		return ret;
	}

	/* GZ & FF-A */
	MTEE_SESSION_LOCK();

	ret = KREE_ReleaseSecureMultichunkmem(sess_data->session_handle,
					      sess_data->append_mem_handle);
	if (ret != 0) {
		pr_info("[%d] MTEE release reg mem failed:%d\n",
		       mtee_dev_desc->kern_tmem_type, ret);
		MTEE_SESSION_UNLOCK();
		return TMEM_MTEE_RELEASE_MEMORY_FAILED;
	}

	if (mtee_dev_desc->notify_remote && mtee_dev_desc->notify_remote_fn) {
		ret = mtee_dev_desc->notify_remote_fn(
			0x0ULL, 0x0, mtee_dev_desc->tee_smem_type);
		if (ret != 0) {
			pr_info("[%d] MTEE notify reg mem remove to TEE failed:%d\n",
			       mtee_dev_desc->tee_smem_type, ret);
//			MTEE_SESSION_UNLOCK();
//			return TMEM_MTEE_NOTIFY_MEM_REMOVE_CFG_TO_TEE_FAILED;
		}
	}

	if (is_ffa_enabled()) {
		ret = tmem_carveout_destroy(mtee_dev_desc->mtee_chunks_id);
		if (ret != 0) {
			pr_info("[%d] tmem_carveout_destroy failed:%d\n",
			       mtee_dev_desc->kern_tmem_type, ret);
			MTEE_SESSION_UNLOCK();
			return TMEM_KPOOL_APPEND_MEMORY_FAILED;
		}

		pr_info("[%d] tmem_ffa_heap[%d] destroy PASS\n",
				mtee_dev_desc->kern_tmem_type, mtee_dev_desc->mtee_chunks_id);
	}

	MTEE_SESSION_UNLOCK();

	return TMEM_OK;
}

static int mtee_drv_execute(KREE_SESSION_HANDLE session_handle, u32 cmd,
			    struct mtee_driver_params *drv_params)
{
	union MTEEC_PARAM svc_call_param[4];

	svc_call_param[0].mem.buffer = drv_params;
	svc_call_param[0].mem.size = sizeof(struct mtee_driver_params);

	return KREE_TeeServiceCall(session_handle, cmd,
				   TZ_ParamTypes1(TZPT_MEM_INOUT),
				   svc_call_param);
}

static int mtee_mem_srv_execute(KREE_SESSION_HANDLE session_handle, u32 cmd,
				struct mtee_driver_params *drv_params)
{
	int ret = TMEM_OK;

	switch (cmd) {
	case TZCMD_MEM_CONFIG_CHUNKMEM_INFO_ION:
		ret = KREE_ConfigSecureMultiChunkMemInfo(
			session_handle, drv_params->param0, drv_params->param1,
			drv_params->param2);
		break;
	default:
		pr_info("%s:%d operation is not implemented yet!\n", __func__,
		       __LINE__);
		ret = TMEM_OPERATION_NOT_IMPLEMENTED;
		break;
	}

	return ret;
}

static int mtee_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			       void *peer_data, void *dev_desc)
{
	int ret = TMEM_OK;
	struct MTEE_SESSION_DATA *sess_data =
		(struct MTEE_SESSION_DATA *)peer_data;
	struct tmem_device_description *mtee_dev_desc =
		(struct tmem_device_description *)dev_desc;
	struct mtee_peer_ops_data *ops_data = &mtee_dev_desc->u_ops_data.mtee;
	struct mtee_driver_params drv_params = {0};

	if (is_pkvm_enabled())
		return TMEM_OK;

	if (INVALID(invoke_params))
		return TMEM_PARAMETER_ERROR;

	drv_params.param0 = invoke_params->param0;
	drv_params.param1 = invoke_params->param1;
	drv_params.param2 = invoke_params->param2;
	drv_params.param3 = invoke_params->param3;

	if (unlikely(ops_data->service_name)) {
		ret = mtee_drv_execute(sess_data->session_handle,
				       invoke_params->cmd, &drv_params);
		if (ret) {
			pr_info("%s:%d invoke failed! cmd:%d, ret:0x%x\n",
			       __func__, __LINE__, invoke_params->cmd, ret);
			return TMEM_MTEE_INVOKE_COMMAND_FAILED;
		}
	} else {
		ret = mtee_mem_srv_execute(sess_data->session_handle,
					   invoke_params->cmd, &drv_params);
		if (ret) {
			pr_info("%s:%d invoke failed! cmd:%d, ret:0x%x\n",
			       __func__, __LINE__, invoke_params->cmd, ret);
			return TMEM_MTEE_INVOKE_COMMAND_FAILED;
		}
	}

	invoke_params->param0 = drv_params.param0;
	invoke_params->param1 = drv_params.param1;
	invoke_params->param2 = drv_params.param2;
	invoke_params->param3 = drv_params.param3;

	return TMEM_OK;
}

static struct trusted_driver_operations mtee_peer_ops = {
	.session_open = mtee_session_open,
	.session_close = mtee_session_close,
	.memory_alloc = mtee_alloc,
	.memory_free = mtee_free,
	.memory_grant = mtee_mem_reg_add,
	.memory_reclaim = mtee_mem_reg_remove,
	.invoke_cmd = mtee_invoke_command,
};

void get_mtee_peer_ops(struct trusted_driver_operations **ops)
{
	pr_info("MTEE_OPS set\n");
	*ops = &mtee_peer_ops;
}
