// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#if IS_ENABLED(CONFIG_OPTEE)
#include <linux/kernel.h>
#include <linux/slab.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_test.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"

#include "mtk_hdmi_ca.h"
#include "mtk_hdmi_hdcp.h"

unsigned char mtk_hdmi_ca_log = 1;

#define HDMI_CA_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_ca_log) { \
		pr_info("[HDMI][CA] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_CA_FUNC()	\
	do {	if (mtk_hdmi_ca_log) \
		pr_info("[HDMI][CA] %s\n", __func__); \
	} while (0)

KREE_SESSION_HANDLE ca_hdmi_session;

bool fgCaHDMICreate(void)
{
	int tz_ret = 0;

	tz_ret = KREE_CreateSession(TZ_TA_HDMI_UUID, &ca_hdmi_session);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		/* Should provide strerror style error string in UREE. */
		HDMI_CA_LOG("Create ca_hdmi_session Error: %d\n", tz_ret);
		return false;
	}
	HDMI_CA_LOG("Create ca_hdmi_session ok: %d\n", tz_ret);

	return true;
}

bool fgCaHDMIClose(void)
{
	int tz_ret = 0;

	tz_ret = KREE_CloseSession(ca_hdmi_session);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		/* Should provide strerror style error string in UREE. */
		HDMI_CA_LOG("Close ca_hdmi_session Error: %d\n", tz_ret);
		return false;
	}
	HDMI_CA_LOG("Close ca_hdmi_session ok: %d\n", tz_ret);

	return true;
}

void vCaHDMIWriteReg(unsigned int u4addr, unsigned int u4data)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return;
	}

	param[0].value.a = u4addr & 0xFFF;
	param[0].value.b = 0;
	param[1].value.a = u4data;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_WRITE_REG,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT,
				     TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS)
		HDMI_CA_LOG("[CA] HDMI_TA_WRITE_REG err:%X\n", tz_ret);

}

void vCaHDMIWriteHDCPRST(unsigned int u4addr, unsigned int u4data)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return;
	}

	param[0].value.a = u4addr;
	param[0].value.b = 0;
	param[1].value.a = u4data;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_HDCP_RST,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT,
				     TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS)
		HDMI_CA_LOG("[CA] HDMI_TA_WRITE_REG err:%X\n", tz_ret);

}

void vCaHDMIWriteHdcpCtrl(unsigned int u4addr, unsigned int u4data)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[HDMI] TEE ca_hdmi_session=0\n");
		return;
	}

	param[0].value.a = u4addr;
	param[0].value.b = 0;
	param[1].value.a = u4data;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_WRITE_REG,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT,
				     TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS)
		HDMI_CA_LOG("[CA]HDMI_TA_WRITE_REG err:%X\n", tz_ret);
}

bool fgCaHDMIInstallHdcpKey(unsigned char *pdata, unsigned int u4Len)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	KREE_SHAREDMEM_HANDLE hdmitx_shm_handle;
	struct KREE_SHAREDMEM_PARAM hdmitx_shm_param;

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}
	HDMI_CA_LOG("[CA]%s,%d\n", __func__, u4Len);

	if (u4Len > HDCPKEY_LENGTH_DRM)
		return false;

	hdmitx_shm_param.buffer = pdata;
	hdmitx_shm_param.size = u4Len;

	tz_ret = KREE_RegisterSharedmem(ca_hdmi_session,
		&hdmitx_shm_handle, &hdmitx_shm_param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_RegisterSharedmem Error\n");
		return false;
	}

	param[0].memref.handle = (uint32_t) hdmitx_shm_handle;
	param[0].memref.offset = 0;
	param[0].memref.size = u4Len;

	param[1].value.a = u4Len;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_INSTALL_HDCP_KEY,
				     TZ_ParamTypes2(TZPT_MEMREF_INPUT,
				     TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA] HDMI_TA_INSTALL_HDCP_KEY err:%X\n", tz_ret);

		tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
			hdmitx_shm_handle);
		if (tz_ret != TZ_RESULT_SUCCESS)
			HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		return false;
	}

	tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
		hdmitx_shm_handle);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		return false;
	}

	return true;
}

bool fgCaHDMIGetAKsv(unsigned char *pdata)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];
	unsigned char *ptr;
	unsigned char i;
	KREE_SHAREDMEM_HANDLE hdmitx_shm_handle;
	struct KREE_SHAREDMEM_PARAM hdmitx_shm_param;

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}

	ptr = kmalloc(HDCP_AKSV_COUNT, GFP_KERNEL);
	if (ptr == NULL) {
		HDMI_CA_LOG("[CA] AKSV kmalloc failure\n");
		return false;
	}

	hdmitx_shm_param.buffer = ptr;
	hdmitx_shm_param.size = HDCP_AKSV_COUNT;
	tz_ret = KREE_RegisterSharedmem(ca_hdmi_session,
		&hdmitx_shm_handle, &hdmitx_shm_param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_RegisterSharedmem Error\n");
		return false;
	}

	param[0].memref.handle = (uint32_t) hdmitx_shm_handle;
	param[0].memref.offset = 0;
	param[0].memref.size = HDCP_AKSV_COUNT;
	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_GET_HDCP_AKSV,
			TZ_ParamTypes1(TZPT_MEMREF_OUTPUT), param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]HDMI_TA_GET_HDCP_AKSV err:%X\n", tz_ret);

		tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
			hdmitx_shm_handle);
		if (tz_ret != TZ_RESULT_SUCCESS)
			HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		kfree(ptr);
		return false;
	}

	tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
		hdmitx_shm_handle);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		kfree(ptr);
		return false;
	}

	for (i = 0; i < HDCP_AKSV_COUNT; i++)
		pdata[i] = ptr[i];

	HDMI_CA_LOG("[CA]hdcp aksv : %x %x %x %x %x\n",
		   pdata[0], pdata[1], pdata[2], pdata[3], pdata[4]);
	kfree(ptr);
	return true;

}

bool fgCaHDMIGetTAStatus(unsigned char *pdata)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];
	unsigned char *ptr;
	KREE_SHAREDMEM_HANDLE hdmitx_shm_handle;
	struct KREE_SHAREDMEM_PARAM hdmitx_shm_param;

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}

	ptr = kmalloc(2, GFP_KERNEL);
	if (ptr == NULL) {
		HDMI_CA_LOG("[CA]%s\n", __func__);
		return false;
	}

	hdmitx_shm_param.buffer = ptr;
	hdmitx_shm_param.size = 2;
	tz_ret = KREE_RegisterSharedmem(ca_hdmi_session,
		&hdmitx_shm_handle, &hdmitx_shm_param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_RegisterSharedmem Error\n");
		return false;
	}

	param[0].memref.handle = (uint32_t) hdmitx_shm_handle;
	param[0].memref.offset = 0;
	param[0].memref.size = 2;
	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_READ_STATUS,
				     TZ_ParamTypes1(TZPT_MEMREF_OUTPUT), param);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]%s err:%X\n", __func__, tz_ret);

		tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
			hdmitx_shm_handle);
		if (tz_ret != TZ_RESULT_SUCCESS)
			HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		kfree(ptr);
		return false;
	}

	tz_ret = KREE_UnregisterSharedmem(ca_hdmi_session,
		hdmitx_shm_handle);
	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]KREE_UnregisterSharedmem Error\n");
		kfree(ptr);
		return false;
	}

	pdata[0] = ptr[0];
	pdata[1] = ptr[1];

	kfree(ptr);
	return true;
}

bool fgCaHDMILoadHDCPKey(void)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	HDMI_CA_LOG("[CA] %s\n", __func__);
	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}
	param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_LOAD_HDCP_KEY,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA] HDMI_TA_LOAD_HDCP_KEY err:%X\n", tz_ret);
		return false;
	}
	return true;
}

bool fgCaHDMILoadROM(void)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}

	param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_LOAD_ROM,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]HDMI_TA_LOAD_ROM err:%X\n", tz_ret);
		return false;
	}
	return true;
}


bool fgCaHDMITestHDCPVersion(void)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA] TEE ca_hdmi_session=0\n");
		return false;
	}

	param[0].value.a = 0;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_TEST_HDCP_VERSION,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS) {
		HDMI_CA_LOG("[CA]HDMI_TA_TEST_HDCP_VERSION err:%X\n", tz_ret);
		return false;
	}
	return true;
}

void fgCaHDMISetTzLogLevel(unsigned int loglevel)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[HDMI] TEE ca_hdmi_session=0\n");
		return;
	}

	param[0].value.a = loglevel;
	param[0].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_SET_LOG_LEVEL,
				     TZ_ParamTypes1(TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS)
		HDMI_CA_LOG("[CA]HDMI_TA_SET_LOG_LEVEL err:%X\n", tz_ret);
}

void fgCaHDMISetSecureRegEntry(unsigned int Tx_entry,
	unsigned int Rx_entry)
{
	int tz_ret = 0;
	union MTEEC_PARAM param[4];

	if (ca_hdmi_session == 0) {
		HDMI_CA_LOG("[CA]TEE ca_hdmi_session=0\n");
		return;
	}

	param[0].value.a = Tx_entry;
	param[0].value.b = 0;
	param[1].value.a = Rx_entry;
	param[1].value.b = 0;

	tz_ret = KREE_TeeServiceCall(ca_hdmi_session, HDMI_TA_SECURE_REG_ENTRY,
				     TZ_ParamTypes2(TZPT_VALUE_INPUT,
				     TZPT_VALUE_INPUT), param);

	if (tz_ret != TZ_RESULT_SUCCESS)
		HDMI_CA_LOG("[CA] HDMI_TA_SECURE_REG_ENTRY err:%X\n", tz_ret);
}

bool hdmi_ca_init(void)
{
	if (ca_hdmi_session == 0)
		return fgCaHDMICreate();

	return true;
}

#endif
