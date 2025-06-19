// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#if IS_ENABLED(CONFIG_OPTEE)
#include "mtk_hdmi_ca.h"
#endif
#include "mtk_hdmi.h"
#include "mtk_hdmi_regs.h"
#include "mtk_hdmi_ddc.h"
#include "mtk_hdmi_hdcp.h"
#include "mtk_hdmi_edid.h"
#include "mtk_hdmi_phy.h"

#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
#include "mtk_hdmi_rpt.h"
struct MTK_HDMIRX *hdmirxhandle;
struct device *hdmirxdev;
#endif


#define REPEAT_CHECK_AUTHHDCP_VALUE 25
#define MARK_LAST_AUTH_FAIL 0xf0
#define MARK_FIRST_AUTH_FAIL 0xf1

#define USE_INT_HDCP
#define SUPPORT_SIMPLAY
#define SRM_SUPPORT

#define HDMI_OK	     ((unsigned int)(0))
#define HDMI_FAIL     ((unsigned int)(-1))


#define SV_OK       ((unsigned char)(0))
#define SV_FAIL      ((unsigned char)(-1))

unsigned int hdcp_err_0x30_count;
unsigned int hdcp_err_0x30_flag;

/* #define SUPPORT_SOFT_SHA */
/* for debug message */
#define DEBUG_HDCP_RI
#define DEBUG_HDCP
/* #define DEBUG_HDCP_RI_AN_FIX */
#define SRM_DBG
/* #define SUPPORT_RI_SAME_WAIT_NEXT_SYNC_UPDATE 1
 * move it to hdmi_drv.h
 */
//static enum HDMI_HDCP_KEY_T bhdcpkey = EXTERNAL_KEY;
/* no encrypt key */
const unsigned char HDCP_NOENCRYPT_KEY[HDCP_KEY_RESERVE] = {
	0
};

/* encrypt key */
const unsigned char HDCP_ENCRYPT_KEY[HDCP_KEY_RESERVE] = {
	0
};

atomic_t hdmi_hdcp_event = ATOMIC_INIT(0);

#ifdef SRM_SUPPORT
struct SRMINFO {
	unsigned int dwVRLLenInDram;
	unsigned int dwVer;

	unsigned char bID;
};

struct SRMINFO _rSRMInfo;

unsigned char _bHdcp_Bksv[5];
//unsigned char _bLastHdcpStatus = SV_OK;

unsigned char _bHdcpStatus = SV_OK;
unsigned char _u1SRMSignatureChkFlag;
#endif

unsigned int _u14SeqMnum;

static unsigned char _bReAuthCnt;
static unsigned char _bReRepeaterPollCnt;
static unsigned char _bReCertPollCnt;
static unsigned char _bReRepeaterDoneCnt;
struct HDMI_HDCP_BKSV_INFO hdmi_hdcp_info;
static unsigned char _bReAKEtPollCnt;


#if IS_ENABLED(CONFIG_OPTEE)
static unsigned char u1CaHdcpAKsv[HDCP_AKSV_COUNT];
#endif

#define HDCP_KEY_RESERVE 287

unsigned char bHdcpKeyBuff[HDCP_KEY_RESERVE] = { };

unsigned char HDMI_AKSV[HDCP_AKSV_COUNT];
unsigned char bKsv_buff[KSV_BUFF_SIZE];
unsigned char HDCPBuff[60];
unsigned char bKsvlist_buff[KSV_LIST_SIZE];
unsigned char bKsvlist[KSV_LIST_SIZE];


unsigned char bSHABuff[20];	/* SHA hash value 20 bytes */
unsigned char _bReCheckBstatusCount;
unsigned char _bReCompRiCount;
unsigned char _bReCheckReadyBit;

unsigned char _bSRMBuff[SRM_SIZE];

unsigned char _bTxBKAV[HDCP_AKSV_COUNT] = { 0 };

unsigned char _bDevice_Count;
unsigned int _u2TxBStatus;


unsigned long hdcp_unmute_start_time;
unsigned long hdcp_logo_start_time;
unsigned int hdcp_unmute_logo_flag = 0xff;
unsigned int enable_mute_for_hdcp_flag = 0x1;
bool hdcp_unmute_start_flag;

unsigned char mtk_hdmi_hdcp_log = 1;

#define HDMI_HDCP_LOG(fmt, arg...) \
	do {	if (mtk_hdmi_hdcp_log) { \
		pr_info("[HDMI][HDCP] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_HDCP_FUNC()	\
	do {	if (mtk_hdmi_hdcp_log) \
		pr_info("[HDMI][HDCP] %s\n", __func__); \
	} while (0)

int hdcp_delay_time; //ms
static inline void set_hdcp_delay_time(unsigned int delay_time)
{
	hdcp_delay_time = delay_time;
}

unsigned int get_hdcp_delay_time(void)
{
	return hdcp_delay_time;
}

bool fgIsHDCPCtrlTimeOut(enum HDCP_CTRL_STATE_T e_hdcp_state)
{
	if (hdcp_delay_time <= 0)
		return true;
	return false;
}

void hdcp_unmute_logo(void)
{
#if IS_ENABLED(CONFIG_OPTEE)
	HDMI_HDCP_LOG("boot unmute fast logo\n");
	vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa5555);
	hdcp_unmute_logo_flag = 1;
#endif
}

void hdcp_mute_log(void)
{
#if IS_ENABLED(CONFIG_OPTEE)
	HDMI_HDCP_LOG("boot mute fast logo\n");
	vCaHDMIWriteHdcpCtrl(0x88880000, 0x5555aaaa);
	hdcp_unmute_logo_flag = 0;
#endif
}

void enable_mute_for_hdcp(bool enable)
{
	HDMI_HDCP_LOG("enable:%d\n", enable);
#if IS_ENABLED(CONFIG_OPTEE)
	if (enable) {
		vCaHDMIWriteHdcpCtrl(0x88880000, 0x5555aaaa);
		enable_mute_for_hdcp_flag = 1;
	} else {
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa5555);
		enable_mute_for_hdcp_flag = 0;
	}
#endif
}

void notify_hpd_to_hdcp(int hpd)
{
	HDMI_HDCP_LOG("hpd:%d\n", hpd);
#if IS_ENABLED(CONFIG_OPTEE)
	if (hpd == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa0005);
	else
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa0006);
#endif
}

void vCleanAuthFailInt(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00020000);
	udelay(1);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x00000000);
	HDMI_HDCP_LOG("0x14025c8c = 0x%08x\n",
		mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
	_bHdcpStatus = SV_FAIL;
}

void vHDMI2xClearINT(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0xfffffff0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0xffffffff);
	udelay(1);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x0);
}

void vHalHDCP1x_Reset(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	/* Reset hdcp 1.x */
#if IS_ENABLED(CONFIG_OPTEE)
	/* SOFT_HDCP_1P4_RST, SOFT_HDCP_1P4_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_1P4_RST, SOFT_HDCP_1P4_RST);
	udelay(100);
	/* SOFT_HDCP_1P4_NOR, SOFT_HDCP_1P4_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_1P4_NOR, SOFT_HDCP_1P4_RST);
#endif

	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, ANA_TOP);
	mtk_hdmi_mask(hdmi, HDCP1X_CTRL, 0, HDCP1X_ENC_EN);
}

void vSetHDCPState(enum HDCP_CTRL_STATE_T e_state)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	hdmi->hdcp_ctrl_state = e_state;
}


void vHDCPEncryptState(unsigned int u1Success)
{
	//HDMI_HDCP_FUNC();

	//HDMI_HDCP_LOG("u1SuccessState = %d\n", u1Success);

	//hdmi_audio_signal_state(u1Success);
}

/*
 * void vSendHdmiCmd(unsigned char u1icmd)
 * {
 *	HDMI_HDCP_FUNC();
 *	hdmi_hdmiCmd = u1icmd;
 * }
 *
 * void vClearHdmiCmd(void)
 * {
 *	HDMI_HDCP_FUNC();
 *	hdmi_hdmiCmd = 0xff;
 * }
 */
void vHDMIClearINT(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0xfffffff0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0xffffffff);
	udelay(1);
	mtk_hdmi_write(hdmi, TOP_INT_CLR00, 0x0);
	mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x0);
}

void vHdcpDdcHwPoll(bool _bhw)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (_bhw == true)
		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, 0, HDCP2X_DIS_POLL_EN);
	else
		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, HDCP2X_DIS_POLL_EN,
		HDCP2X_DIS_POLL_EN);
}

void vHalHDCP2x_Reset(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	/* Reset hdcp 2.x */
	HDMI_HDCP_LOG("0xc68=0x%x\n", mtk_hdmi_read(hdmi, HDCP2X_DDCM_STATUS));

	if (mtk_hdmi_read(hdmi, HDCP2X_CTRL_0) &
		HDCP2X_ENCRYPT_EN) {
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0, HDCP2X_ENCRYPT_EN);
		mdelay(50);
	}

	vHdcpDdcHwPoll(false);

#if IS_ENABLED(CONFIG_OPTEE)
	/* SOFT_HDCP_RST, SOFT_HDCP_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_RST, SOFT_HDCP_RST);
	/* SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);
	udelay(1);
	/* SOFT_HDCP_NOR, SOFT_HDCP_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_NOR, SOFT_HDCP_RST);
	/* SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST); */
	vCaHDMIWriteHDCPRST(SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST);
#endif

}

void vHDCPReset(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	if (hdmi->enable_hdcp == false)
		return;
	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}
	vHalHDCP1x_Reset();

	hdmi_ddc_request(ddc, 3);
	vHalHDCP2x_Reset();
	hdmi_ddc_free(ddc, 3);

	vSetHDCPState(HDCP_RECEIVER_NOT_READY);
	//hdmi_audio_signal_state(0);
	_bReCheckBstatusCount = 0;
}

void vHDCP14InitAuth(void)
{
	set_hdcp_delay_time(HDCP_WAIT_RES_CHG_OK_TIMEOUE);
	vSetHDCPState(HDCP_WAIT_RES_CHG_OK);
	_bReCheckBstatusCount = 0;
}

void vHDCPInitAuth(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_LOG("%s, 0xc68=0x%x, %lums\n", __func__,
		      mtk_hdmi_read(hdmi, HDCP2X_DDCM_STATUS), jiffies);

	hdcp_err_0x30_count = 0;
	hdcp_err_0x30_flag = 0;

	vHDCPReset();
	if (hdmi->hdcp_2x_support == true) {
		{
			if (hdmi->bin_is_loaded == false)
				set_hdcp_delay_time(HDCP2x_WAIT_LOADBIN_TIMEOUE);
			else
				set_hdcp_delay_time(HDCP2x_WAIT_RES_CHG_OK_TIMEOUE);
		}
		vSetHDCPState(HDCP2x_WAIT_RES_CHG_OK);
	} else {
		vHDCP14InitAuth();
	}
}

void vRepeaterOnOff(unsigned char fgIsRep)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (fgIsRep == true)
		mtk_hdmi_mask(hdmi, 0xcd0, 1 << 5, 1 << 5);
	else
		mtk_hdmi_mask(hdmi, 0xcd0, 0 << 5, 1 << 5);
}

void vStopAn(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	mtk_hdmi_mask(hdmi, 0xcd0, 1 << 4, 1 << 4);
}

void vReadAn(unsigned char *AnValue)
{
	unsigned char bIndex;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	AnValue[0] = mtk_hdmi_read(hdmi, 0xcc0) & 0xff;
	AnValue[1] = (mtk_hdmi_read(hdmi, 0xcc0) & 0xff00) >> 8;
	AnValue[2] = (mtk_hdmi_read(hdmi, 0xcc0) & 0xff0000) >> 16;
	AnValue[3] = (mtk_hdmi_read(hdmi, 0xcc0) & 0xff000000) >> 24;
	AnValue[4] = mtk_hdmi_read(hdmi, 0xcc4) & 0xff;
	AnValue[5] = (mtk_hdmi_read(hdmi, 0xcc4) & 0xff00) >> 8;
	AnValue[6] = (mtk_hdmi_read(hdmi, 0xcc4) & 0xff0000) >> 16;
	AnValue[7] = (mtk_hdmi_read(hdmi, 0xcc4) & 0xff000000) >> 24;
	for (bIndex = 0; bIndex < 8; bIndex++)
		HDMI_HDCP_LOG("[1x]AnValue[%d] =0x%02x\n",
		bIndex, AnValue[bIndex]);

}

void vSendAn(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);
	unsigned char bHDCPBuf[HDCP_AN_COUNT];

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	/* Step 1: issue command to general a new An value */
	/* (1) read the value first */
	/* (2) set An control as stop to general a An first */
	vStopAn();

	/* Step 2: Read An from Transmitter */
	vReadAn(bHDCPBuf);
	/* Step 3: Send An to Receiver */
	fgDDCDataWrite(ddc, RX_ID, RX_REG_HDCP_AN, HDCP_AN_COUNT, bHDCPBuf);

}

void vWriteBksvToTx(unsigned char *bBKsv)
{
	unsigned int temp;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_LOG("bksv 0x%x; 0x%x; 0x%x; 0x%x; 0x%x\n",
		bBKsv[0], bBKsv[1], bBKsv[2],
		      bBKsv[3], bBKsv[4]);
	temp = (((bBKsv[3]) & 0xff) << 24) +
		(((bBKsv[2]) & 0xff) << 16) +
	    (((bBKsv[1]) & 0xff) << 8) +
	    (bBKsv[0] & 0xff);
	mtk_hdmi_write(hdmi, 0xcb0, temp);
	udelay(10);
	mtk_hdmi_write(hdmi, 0xcb4, bBKsv[4]);

	mtk_hdmi_mask(hdmi, 0xcd0, 1 << 0, 1 << 0);
	udelay(100);
	mtk_hdmi_mask(hdmi, 0xcd0, 0 << 0, 1 << 0);

	HDMI_HDCP_LOG("[1x]bksv 0xcb0 =0x%08x\n",
		mtk_hdmi_read(hdmi, 0xcb0));
	HDMI_HDCP_LOG("[1x]bksv 0xcb4 =0x%08x\n",
		mtk_hdmi_read(hdmi, 0xcb4));

}

bool fgIsRepeater(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	return (hdmi->donwstream_is_repeater == true);
}

#ifdef SRM_SUPPORT
void vCompareSRM(void)
{
	unsigned int dwKsvInx = 0, dwVRLIndex = 0;
	unsigned char *ptrSRM, bNomOfDevice = 0;
	unsigned char bKSV_Sink_Index = 0, bIndex = 0, dwIndex = 0;

	if (_rSRMInfo.bID != 0x80)
		return;


#ifdef SRM_DBG
	{
		HDMI_HDCP_LOG("[HDCP]SRM Count = %d ",
			_rSRMInfo.dwVRLLenInDram);
		HDMI_HDCP_LOG("[HDCP]Key=%x, %x, %x, %x, %x",
			_bHdcp_Bksv[0], _bHdcp_Bksv[1],
			_bHdcp_Bksv[2], _bHdcp_Bksv[3], _bHdcp_Bksv[4]);
	}

#endif
#ifdef REVOKE_TEST
	_bHdcp_Bksv[0] = 0x6b;
	_bHdcp_Bksv[1] = 0x3b;
	_bHdcp_Bksv[2] = 0x60;
	_bHdcp_Bksv[3] = 0xa0;
	_bHdcp_Bksv[4] = 0x54;
#endif

	vSetSharedInfo(SI_DVD_HDCP_REVOCATION_RESULT, REVOCATION_NOT_CHK);

	dwVRLIndex = 0;
	ptrSRM = &_bSRMBuff[8];
while (_rSRMInfo.dwVRLLenInDram > dwVRLIndex) {
	bNomOfDevice = *(ptrSRM + dwVRLIndex) & 0x7F;	/* 40*N */
	dwVRLIndex++;
	for (dwKsvInx = 0; dwKsvInx < bNomOfDevice; dwKsvInx++) {
		for (dwIndex = 0; dwIndex < 5; dwIndex++) {
			if (*(ptrSRM + dwVRLIndex +
				(dwKsvInx * 5) + dwIndex) !=
			    _bHdcp_Bksv[dwIndex])
				break;
		}

		if (fgIsRepeater()) {
			for (bKSV_Sink_Index = 0;
			     bKSV_Sink_Index <
				i4SharedInfo(SI_REPEATER_DEVICE_COUNT);
			     bKSV_Sink_Index++) {
				for (bIndex = 0; bIndex < 5; bIndex++) {
					if ((((bKSV_Sink_Index + 1) * 5 -
						bIndex - 1) < 192) &&
						(*(ptrSRM + dwVRLIndex +
						(dwKsvInx * 5) + bIndex) !=
						bKsv_buff[(bKSV_Sink_Index + 1)
						* 5 - bIndex - 1]))
						break;
				}
				if (bIndex == 5)
					break;
			}
		}

		if ((dwIndex == 5) || (bIndex == 5)) {
			vSetSharedInfo(SI_DVD_HDCP_REVOCATION_RESULT,
			REVOCATION_IS_CHK | IS_REVOCATION_KEY);
			break;
		}
		vSetSharedInfo(SI_DVD_HDCP_REVOCATION_RESULT,
			       REVOCATION_IS_CHK | NOT_REVOCATION_KEY);
	}
	if ((dwIndex == 5) || (bIndex == 5))
		break;
	dwVRLIndex += bNomOfDevice * 5;
}

#ifdef SRM_DBG
	{
		HDMI_HDCP_LOG("[HDCP]Shared Info=%x",
			i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT));
		if (i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY)
			HDMI_HDCP_LOG("[HDCP]Revoked Sink Key\n");
	}
#endif

}
#endif

bool isKsvLegal(unsigned char ksv[HDCP_AKSV_COUNT])
{
	unsigned char i, bit_shift, one_cnt;

	one_cnt = 0;
	for (i = 0; i < HDCP_AKSV_COUNT; i++) {
		for (bit_shift = 0; bit_shift < 8; bit_shift++)
			if (ksv[i] & BIT(bit_shift))
				one_cnt++;
	}
	if (one_cnt == 20)
		return true;

	HDMI_HDCP_LOG("[HDCP],err ksv is:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		ksv[0], ksv[1], ksv[2], ksv[3], ksv[4]);
	return false;

}

void vExchangeKSVs(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	unsigned char bHDCPBuf[HDCP_AKSV_COUNT] = {0};
#ifdef SRM_SUPPORT
	unsigned char bIndx;
#endif
	/* Step 1: read Aksv from transmitter, and send to receiver */
	HDMI_HDCP_LOG("HDMI_AKSV:0x%x 0x%x 0x%x 0x%x 0x%x\n",
		HDMI_AKSV[0], HDMI_AKSV[1], HDMI_AKSV[2], HDMI_AKSV[3],
		HDMI_AKSV[4]);

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	fgDDCDataWrite(ddc, RX_ID, RX_REG_HDCP_AKSV, HDCP_AKSV_COUNT,
		HDMI_AKSV);

	/* Step 4: read Bksv from receiver, and send to transmitter */
	fgDDCDataRead(ddc, RX_ID, RX_REG_HDCP_BKSV, HDCP_BKSV_COUNT, bHDCPBuf);
	HDMI_HDCP_LOG("bHDCPBuf 0x%x; 0x%x; 0x%x; 0x%x; 0x%x\n",
		bHDCPBuf[0], bHDCPBuf[1],
		      bHDCPBuf[2], bHDCPBuf[3], bHDCPBuf[4]);
	vWriteBksvToTx(bHDCPBuf);
	HDMI_HDCP_LOG("BSKV:0x%x 0x%x 0x%x 0x%x 0x%x\n",
		bHDCPBuf[0], bHDCPBuf[1],
		bHDCPBuf[2], bHDCPBuf[3], bHDCPBuf[4]);

	for (bIndx = 0; bIndx < HDCP_AKSV_COUNT; bIndx++)
		_bTxBKAV[bIndx] = bHDCPBuf[bIndx];

#ifdef SRM_SUPPORT
	for (bIndx = 0; bIndx < HDCP_AKSV_COUNT; bIndx++)
		_bHdcp_Bksv[bIndx] = bHDCPBuf[HDCP_AKSV_COUNT - bIndx - 1];
	/* _bHdcp_Bksv[bIndx] = bHDCPBuf[bIndx]; */

	vCompareSRM();

#endif

}

void vSendAKey(unsigned char *bAkey)
{
	unsigned int i;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	for (i = 0; i < 280; i++) {
		mtk_hdmi_write(hdmi, 0xcc8, *(bAkey + i));
		udelay(10);
	}
}

unsigned char bCheckHDCPRiStatus(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	if (mtk_hdmi_read(hdmi, 0xcf4) & (1 << 25))
		return true;
	else
		return false;
}

bool fgCompareRi(void)
{
	unsigned int bTemp;
	unsigned char bHDCPBuf[4];
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	//HDMI_HDCP_FUNC();
	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return false;
	}

	bHDCPBuf[2] = mtk_hdmi_read(hdmi, 0xc78) & 0xff;
	bHDCPBuf[3] = (mtk_hdmi_read(hdmi, 0xc78) >> 8) & 0xff;

	/* Read R0'/ Ri' from Receiver */
	fgDDCDataRead(ddc, RX_ID, RX_REG_RI, HDCP_RI_COUNT,
				bHDCPBuf);

	if (hdmi->hdcp_ctrl_state == HDCP_COMPARE_R0)
		HDMI_HDCP_LOG(
			"[HDCP1.x][R0]Rx_Ri=0x%x%x Tx_Ri=0x%x%x\n",
			bHDCPBuf[0], bHDCPBuf[1], bHDCPBuf[2], bHDCPBuf[3]);
/*	else
 *		HDMI_HDCP_LOG(
 *			"[HDCP1.x]Rx_Ri=0x%x%x Tx_Ri=0x%x%x\n",
 *			bHDCPBuf[0], bHDCPBuf[1], bHDCPBuf[2], bHDCPBuf[3]);
 */
	/* compare R0 and R0' */
	for (bTemp = 0; bTemp < HDCP_RI_COUNT; bTemp++) {
		if (((bTemp + HDCP_RI_COUNT) < sizeof(bHDCPBuf))
			&& (bHDCPBuf[bTemp] == bHDCPBuf[bTemp + HDCP_RI_COUNT])) {
			continue;
		} else {	/* R0 != R0' */

			break;
		}
	}

	/* return the compare result */
	if (bTemp == HDCP_RI_COUNT) {
		_bHdcpStatus = SV_OK;
		return true;
	}
	{
		_bHdcpStatus = SV_FAIL;
		HDMI_HDCP_LOG("[HDCP][1.x]Rx_Ri=0x%x%x Tx_Ri=0x%x%x\n",
			bHDCPBuf[0], bHDCPBuf[1], bHDCPBuf[2], bHDCPBuf[3]);
		return false;
	}

}

void vEnableEncrpt(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	mtk_hdmi_mask(hdmi, 0xcd0, 1 << 6, 1 << 6);
}

void vHalWriteKsvListPort(unsigned char *prKsvData,
			unsigned char bDevice_Count,
			  unsigned char *prBstatus)
{
	unsigned char bIndex;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	if ((bDevice_Count * 5) < KSV_BUFF_SIZE) {
		mtk_hdmi_write(hdmi, 0xc1c, (*(prBstatus)) +
			((*(prBstatus + 1)) << 8));
		mtk_hdmi_mask(hdmi, 0xcd0, (1 << 3), (1 << 3));
		HDMI_HDCP_LOG("[HDCP]0xc1c = 0x%08x\n",
			mtk_hdmi_read(hdmi, 0xc1c));

		for (bIndex = 0; bIndex < (bDevice_Count * 5);
		bIndex++) {
			HDMI_HDCP_LOG("[HDCP]0xcd4 =0x%08x\n",
				(*(prKsvData + bIndex)) + (1 << 8));
			mtk_hdmi_write(hdmi, 0xcd4,
				(*(prKsvData + bIndex)) + (1 << 8));
		}
	}

}

void vHalWriteHashPort(unsigned char *prHashVBuff)
{
	unsigned char bIndex;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	for (bIndex = 0; bIndex < 5; bIndex++) {
		HDMI_HDCP_LOG("[HDCP]write v,0x%08x =0x%08x\n",
			(0xce0 + bIndex * 4),
			((*(prHashVBuff + 3 + bIndex * 4)) << 24) +
			((*(prHashVBuff + 2 + bIndex * 4)) << 16) +
			((*(prHashVBuff + 1 + bIndex * 4)) << 8) +
			(*(prHashVBuff + 0 + bIndex * 4)));

	}

	mtk_hdmi_write(hdmi, 0xce0,
		((*(prHashVBuff + 3)) << 24) +
		((*(prHashVBuff + 2)) << 16) +
		((*(prHashVBuff + 1)) << 8) +
		(*(prHashVBuff + 0)));

	mtk_hdmi_write(hdmi, 0xce4,
		((*(prHashVBuff + 7)) << 24) +
		((*(prHashVBuff + 6)) << 16) +
		((*(prHashVBuff + 5)) << 8) +
		(*(prHashVBuff + 4)));

	mtk_hdmi_write(hdmi, 0xce8,
		((*(prHashVBuff + 11)) << 24) +
		((*(prHashVBuff + 10)) << 16) +
		((*(prHashVBuff + 9)) << 8) +
		(*(prHashVBuff + 8)));

	mtk_hdmi_write(hdmi, 0xcec,
		((*(prHashVBuff + 15)) << 24) +
		((*(prHashVBuff + 14)) << 16) +
		((*(prHashVBuff + 13)) << 8) +
		(*(prHashVBuff + 12)));

	mtk_hdmi_write(hdmi, 0xcf0,
		((*(prHashVBuff + 19)) << 24) +
		((*(prHashVBuff + 18)) << 16) +
		((*(prHashVBuff + 17)) << 8) +
		(*(prHashVBuff + 16)));

	for (bIndex = 0; bIndex < 5; bIndex++) {
		HDMI_HDCP_LOG("[HDCP]read v,0x%08x =0x%08x\n",
			(0xce0 + bIndex * 4), mtk_hdmi_read(hdmi, 0xce0 + bIndex * 4));

	}
}

void vReadKSVFIFO(void)
{
	unsigned char bTemp, bIndex, bDevice_Count = 0;
	unsigned char bStatus[2] = {0}, bBstatus1 = 0;
	unsigned int u2TxBStatus = 0;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	HDMI_HDCP_FUNC();

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1 + 1, 1, &bBstatus1);
	fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);
	_u2TxBStatus = (((unsigned int)bBstatus1) << 8) | bDevice_Count;

	bDevice_Count &= DEVICE_COUNT_MASK;

	if ((bDevice_Count & MAX_DEVS_EXCEEDED) ||
		(bBstatus1 & MAX_CASCADE_EXCEEDED)) {

		fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 2, bStatus);
		fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 1, &bDevice_Count);
		bDevice_Count &= DEVICE_COUNT_MASK;
		u2TxBStatus = bStatus[0] | (bStatus[1] << 8);
		vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, bDevice_Count);
		if (i4SharedInfo(SI_REPEATER_DEVICE_COUNT) == 0)
			_bDevice_Count = 0;
		else
			_bDevice_Count = bDevice_Count;

		_u2TxBStatus = u2TxBStatus;
		vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
		//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		_bHdcpStatus = SV_FAIL;
		return;
	}

	if (bDevice_Count > 32) {
		for (bTemp = 0; bTemp < 2; bTemp++) {
			fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 1,
				&bDevice_Count);
			bDevice_Count &= DEVICE_COUNT_MASK;
			if (bDevice_Count <= 32)
				break;
		}
		if (bTemp == 2)
			bDevice_Count = 32;
	}

	vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, bDevice_Count);

	if (bDevice_Count == 0) {
		for (bIndex = 0; bIndex < 5; bIndex++)
			bKsv_buff[bIndex] = 0;

		for (bIndex = 0; bIndex < 2; bIndex++)
			bStatus[bIndex] = 0;

		for (bIndex = 0; bIndex < 20; bIndex++)
			bSHABuff[bIndex] = 0;

		mtk_hdmi_mask(hdmi, 0xcd0, (1 << 11), (1 << 11));
	} else {
		fgDDCDataRead(ddc, RX_ID, RX_REG_KSV_FIFO, bDevice_Count * 5,
			bKsv_buff);
		mtk_hdmi_mask(hdmi, 0xcd0, (0 << 11), (1 << 11));
	}

	HDMI_HDCP_LOG("[1x]bDevice_Count = %d\n", bDevice_Count);

	fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 2, bStatus);
	fgDDCDataRead(ddc, RX_ID, RX_REG_REPEATER_V, 20, bSHABuff);

	u2TxBStatus = bStatus[0] | (bStatus[1] << 8);
	HDMI_HDCP_LOG(
	"[1x]TX BSTATUS: bStatus[0]=%x, bStatus[1]=%x, u2TxBStatus=%x\n",
		   bStatus[0], bStatus[1], u2TxBStatus);


#ifdef SRM_SUPPORT
	vCompareSRM();
#endif

	vHalWriteKsvListPort(bKsv_buff, bDevice_Count, bStatus);
	vHalWriteHashPort(bSHABuff);

	vSetHDCPState(HDCP_COMPARE_V);
	/* set time-out value as 0.5 sec */
	set_hdcp_delay_time(HDCP_WAIT_V_RDY_TIMEOUE);

	for (bIndex = 0; bIndex < bDevice_Count; bIndex++) {
		if ((bIndex * 5 + 4) < KSV_BUFF_SIZE) {
			HDMI_HDCP_LOG("[HDCP1.x] KSV List: Device[%d]= %x,%x,%x,%x,%x\n",
				bIndex, bKsv_buff[bIndex * 5], bKsv_buff[bIndex * 5 + 1],
				bKsv_buff[bIndex * 5 + 2], bKsv_buff[bIndex * 5 + 3],
				bKsv_buff[bIndex * 5 + 4]);
		}
	}
	HDMI_HDCP_LOG("[HDCP][1.x]Tx BKSV: %x, %x, %x, %x, %x\n",
		_bTxBKAV[0], _bTxBKAV[1], _bTxBKAV[2],
		_bTxBKAV[3], _bTxBKAV[4]);

	if (i4SharedInfo(SI_REPEATER_DEVICE_COUNT) == 0)
		_bDevice_Count = 0;
	else
		_bDevice_Count = bDevice_Count;

	_u2TxBStatus = u2TxBStatus;

	HDMI_HDCP_LOG("[1x]_bDevice_Count = %x, _u2TxBStatus = %x\n",
		_bDevice_Count, _u2TxBStatus);

}

unsigned int uiReadHDCPStatus(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_LOG("[1x]0xcf4 = 0x%08x\n", mtk_hdmi_read(hdmi, 0xcf4));
	return mtk_hdmi_read(hdmi, 0xcf4);
}

unsigned int uiReadIRQStatus01(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_LOG("[1x]0x1ac = 0x%08x\n", mtk_hdmi_read(hdmi, 0x1ac));
	HDMI_HDCP_LOG("[1x]0x1bc = 0x%08x\n", mtk_hdmi_read(hdmi, 0x1bc));

	return mtk_hdmi_read(hdmi, 0x1ac);
}

void vWriteAksvKeyMask(unsigned char *PrData)
{
	unsigned char bData;
	/* - write wIdx into 92. */
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	bData = 0x00;
	mtk_hdmi_mask(hdmi, 0xccc, (bData << 24), 0xff << 24);
	bData = 0x00;
	mtk_hdmi_mask(hdmi, 0xccc, (bData << 16), 0xff << 16);
}

void vAKeyDone(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	HDMI_HDCP_FUNC();

	mtk_hdmi_mask(hdmi, 0xcd0, 1 << 2, 1 << 2);
	udelay(100);
	mtk_hdmi_mask(hdmi, 0xcd0, 0 << 0, 1 << 2);

}

unsigned int bHDMIHDCP2Err(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	return (mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) &
		HDCP2X_STATE) >> 16;
}

bool fgHDMIHdcp2Err(void)
{
	unsigned int hdcp2_txstate;

	hdcp2_txstate = bHDMIHDCP2Err();
	if ((hdcp2_txstate == 0x30)
	    || (hdcp2_txstate == 0x31)
	    || (hdcp2_txstate == 0x32)
	    || (hdcp2_txstate == 0x33)
	    || (hdcp2_txstate == 0x34)
	    || (hdcp2_txstate == 0x35)
	    || (hdcp2_txstate == 0x36)
	    || (hdcp2_txstate == 0x37)
	    || (hdcp2_txstate == 0x38)
	    || (hdcp2_txstate == 0x39)
	    || (hdcp2_txstate == 0x3a)
	    || (hdcp2_txstate == 0x3b)
	    || (hdcp2_txstate == 0x3c)
	    || (hdcp2_txstate == 0x3d)
	    || (hdcp2_txstate == 0x3e))
		return true;
	return false;
}

bool fgHDMIHdcp2Auth(void)
{
	unsigned int hdcp2_txstate;

	hdcp2_txstate = bHDMIHDCP2Err();
	if ((hdcp2_txstate == 3)
	    || (hdcp2_txstate == 4)
	    || (hdcp2_txstate == 11)
	    || (hdcp2_txstate == 14)
	    || (hdcp2_txstate == 16)
	    || (hdcp2_txstate == 18)
	    || (hdcp2_txstate == 41)
	    || (hdcp2_txstate == 24))
		return true;
	return false;
}

unsigned char u1CountNum1(unsigned char u1Data)
{
	unsigned char i, bCount = 0;

	for (i = 0; i < 8; i++) {
		if (((u1Data >> i) & 0x01) == 0x01)
			bCount++;
	}
	return bCount;
}

bool hdcp_check_err_0x30(struct mtk_hdmi *hdmi)
{
	if (fgHDMIHdcp2Err()) {
		if (bHDMIHDCP2Err() == 0x30)
			hdcp_err_0x30_count++;
		else
			hdcp_err_0x30_count = 0;
		if (hdcp_err_0x30_count > 3) {
			hdcp_err_0x30_count = 0;
			hdcp_err_0x30_flag = 1;
			vTxSignalOnOff(hdmi->hdmi_phy_base, false);
			HDMI_HDCP_LOG("err=0x30, signal off\n");
			return TRUE;
		}
	}
	return FALSE;
}

void hdcp_service(enum HDCP_CTRL_STATE_T e_hdcp_state)
{
	unsigned char bIndx = 0, bTemp = 0, BStatus[2] = {0};
	unsigned char bRptID[155];
	unsigned int readvalue = 0, i = 0, devicecnt = 0;
	unsigned int uitemp1 = 0, uitemp2 = 0, depth = 0, count1 = 0;
	bool fgRepeaterError = false;
	unsigned char ta_status[2] = {0};
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	if (hdmi->enable_hdcp == false) {
		HDMI_HDCP_LOG("hdmi->enable_hdcp==false\n");
		vSetHDCPState(HDCP_RECEIVER_NOT_READY);
		mtk_hdmi_AV_unmute(hdmi);
		//hdmi_hdcp22_monitor_stop();
		return;
	}

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

#if IS_ENABLED(CONFIG_OPTEE)
	if (hdmi->key_is_installed == false) {
		if (fgCaHDMIGetAKsv(u1CaHdcpAKsv) == true)
			hdmi->key_is_installed = true;
	}
#endif

	switch (e_hdcp_state) {
	case HDCP_RECEIVER_NOT_READY:
		HDMI_HDCP_LOG("HDCP_RECEIVER_NOT_READY\n");
		break;

	case HDCP_READ_EDID:
		break;

	case HDCP_WAIT_RES_CHG_OK:
		if (fgIsHDCPCtrlTimeOut(e_hdcp_state)) {
			if (hdmi->enable_hdcp == false) {	/* disable HDCP */
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				mtk_hdmi_AV_unmute(hdmi);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				//_bLastHdcpStatus = HDCP_RECEIVER_NOT_READY;
			}
		}
		break;

	case HDCP_INIT_AUTHENTICATION:
#if IS_ENABLED(CONFIG_OPTEE)
		/* HDCP_1P4_TCLK_EN, HDCP_1P4_TCLK_EN); */
		vCaHDMIWriteHDCPRST(HDCP_1P4_TCLK_EN, HDCP_1P4_TCLK_EN);
#endif

		if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
			if ((hdmirxhandle != NULL) &&
			(hdmirxhandle->hdcp_is_doing_auth != NULL) &&
			(hdmirxhandle->hdcp_is_rpt != NULL) &&
			(!(hdmirxhandle->hdcp_is_doing_auth(hdmirxdev)) &&
			hdmirxhandle->hdcp_is_rpt(hdmirxdev))) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				mtk_hdmi_AV_unmute(hdmi);
				HDMI_HDCP_LOG(
				"[1.x][REPEATER]upstream no need auth\n");
				break;
			}
#endif
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG("[1.x][REPEATER] hpd low\n");
				break;
			}
		}
		if (enable_mute_for_hdcp_flag)
			mtk_hdmi_AV_mute(hdmi);
		vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0);

		hdmi_ddc_request(ddc, 5);
		hdmi_ddc_free(ddc, 5);

		if (!fgDDCDataRead(ddc, RX_ID, RX_REG_BCAPS, 1,
			&bTemp)) {
			HDMI_HDCP_LOG(
			"[1.x]fail-->HDCP_INIT_AUTHENTICATION-->0\n");
			_bHdcpStatus = SV_FAIL;
			vHDCPEncryptState(0); /* for audio notify */
			set_hdcp_delay_time(HDCP_WAIT_300MS_TIMEOUT);
			break;
		}
		if (!(hdmi->dvi_mode)) {
			fgDDCDataRead(ddc, RX_ID, RX_REG_BSTATUS1, 2, BStatus);
			if ((BStatus[1] & 0x10) == 0) {
				HDMI_HDCP_LOG("[1x]BStatus=0x%x,0x%x\n",
					BStatus[0], BStatus[1]);
				_bReCheckBstatusCount++;
	/* wait for up to 4.5 seconds to detect otherwise proceed anyway */
				if (_bReCheckBstatusCount < 15) {
					_bHdcpStatus = SV_FAIL;
					vHDCPEncryptState(0);
					set_hdcp_delay_time(
						HDCP_WAIT_300MS_TIMEOUT);
					break;
				}
			}
		}

		HDMI_HDCP_LOG("[1x]RX_REG_BCAPS = 0x%08x\n", bTemp);
		for (bIndx = 0; bIndx < HDCP_AKSV_COUNT; bIndx++) {
#if IS_ENABLED(CONFIG_OPTEE)
			HDMI_AKSV[bIndx] = u1CaHdcpAKsv[bIndx];
#else
			HDMI_AKSV[bIndx] = bHdcpKeyBuff[1 + bIndx];
#endif
			HDMI_HDCP_LOG("[1x]HDMI_AKSV[%d] = 0x%x\n",
			bIndx,
				   HDMI_AKSV[bIndx]);
		}

#ifndef NO_ENCRYPT_KEY_TEST
		vWriteAksvKeyMask(&HDMI_AKSV[0]);
#endif

		fgDDCDataRead(ddc, RX_ID, RX_REG_BCAPS, 1, &bTemp);
		vSetSharedInfo(SI_REPEATER_DEVICE_COUNT, 0);
		if (bTemp & RX_BIT_ADDR_RPTR)
			hdmi->donwstream_is_repeater = true;
		else
			hdmi->donwstream_is_repeater = false;

		if (fgIsRepeater())
			vRepeaterOnOff(true);
		else
			vRepeaterOnOff(false);

		vSendAn();
		vExchangeKSVs();
		if ((!isKsvLegal(HDMI_AKSV)) ||
			(!isKsvLegal(_bTxBKAV))) {
			HDMI_HDCP_LOG(
		"[1x]fail-->HDCP_INIT_AUTHENTICATION-->isKsvLegal\n");
			if (enable_mute_for_hdcp_flag)
				mtk_hdmi_AV_mute(hdmi);
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			_bHdcpStatus = SV_FAIL;
			set_hdcp_delay_time(HDCP_WAIT_300MS_TIMEOUT);
			break;
		}

#ifdef SRM_SUPPORT
		if (
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
			((hdmi->repeater_hdcp == true) && ((hdmirxhandle != NULL) &&
			(hdmirxhandle->hdcp_is_rpt != NULL) &&
			(!(hdmirxhandle->hdcp_is_rpt(hdmirxdev)) &&
			((i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY) && (_rSRMInfo.bID == 0x80))))) ||
#endif
			((hdmi->repeater_hdcp == false) &&
			((i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY) && (_rSRMInfo.bID == 0x80)))) {
			HDMI_HDCP_LOG(
			"[1x]fail-->HDCP_INIT_AUTHENTICATION-->1\n");
			if (enable_mute_for_hdcp_flag)
				mtk_hdmi_AV_mute(hdmi);
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			_bHdcpStatus = SV_FAIL;
			set_hdcp_delay_time(HDCP_WAIT_300MS_TIMEOUT);
			break;

		}
#endif
#if IS_ENABLED(CONFIG_OPTEE)
		fgCaHDMILoadHDCPKey();
#else
		vSendAKey(&bHdcpKeyBuff[6]);	/* around 190msec */
#endif

		/* set time-out value as 100 ms */
		set_hdcp_delay_time(HDCP_WAIT_R0_TIMEOUT);
		vAKeyDone();

		/* change state as waiting R0 */
		vSetHDCPState(HDCP_WAIT_R0);

		break;


	case HDCP_WAIT_R0:
		bTemp = bCheckHDCPRiStatus();
		if (bTemp == true) {
			vSetHDCPState(HDCP_COMPARE_R0);
		} else {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bHdcpStatus = SV_FAIL;
			vHDCPEncryptState(0); /* for audio notify */
			break;
		}

	case HDCP_COMPARE_R0:

		if (fgCompareRi() == true) {

			vEnableEncrpt();	/* Enabe encrption */

			/* change state as check repeater */
			vSetHDCPState(HDCP_CHECK_REPEATER);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0x01);
		} else {
			vSetHDCPState(HDCP_RE_COMPARE_R0);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			HDMI_HDCP_LOG("[1x]fail-->HDCP_WAIT_R0-->1\n");
			vHDCPEncryptState(0); /* for audio notify */
			_bReCompRiCount = 0;
		}

		break;

	case HDCP_RE_COMPARE_R0:

		_bReCompRiCount++;
		if (fgIsHDCPCtrlTimeOut(e_hdcp_state) && _bReCompRiCount > 3) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bHdcpStatus = SV_FAIL;
			_bReCompRiCount = 0;
			vHDCPEncryptState(0); /* for audio notify */
			HDMI_HDCP_LOG("[1x]fail-->HDCP_WAIT_R0-->2\n");

		} else {
			if (fgCompareRi() == true) {
				vEnableEncrpt();	/* Enabe encrption */

				/* change state as check repeater */
				vSetHDCPState(HDCP_CHECK_REPEATER);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT, 0x01);
			} else {
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}

		}
		break;

	case HDCP_CHECK_REPEATER:
		HDMI_HDCP_LOG("[1x]HDCP_CHECK_REPEATER\n");
		/* if the device is a Repeater, */
		if (fgIsRepeater()) {
			_bReCheckReadyBit = 0;
			vSetHDCPState(HDCP_WAIT_KSV_LIST);
			set_hdcp_delay_time(HDCP_WAIT_KSV_LIST_TIMEOUT);
		} else {

			_bDevice_Count = 0;
			_u2TxBStatus = 0;
			if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
				if ((hdmirxhandle != NULL) &&
				(hdmirxhandle->set_ksv != NULL))
					hdmirxhandle->set_ksv(
					hdmirxdev, 0, _u2TxBStatus,
					&_bTxBKAV[0], &bKsv_buff[0], true);
#endif
			}
			vSetHDCPState(HDCP_WAIT_RI);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}

		break;

	case HDCP_WAIT_KSV_LIST:

		fgDDCDataRead(ddc, RX_ID, RX_REG_BCAPS, 1, &bTemp);
		if ((bTemp & RX_BIT_ADDR_READY)) {
			_bReCheckReadyBit = 0;
			vSetHDCPState(HDCP_READ_KSV_LIST);
		} else {
			if (_bReCheckReadyBit >
				HDCP_CHECK_KSV_LIST_RDY_RETRY_COUNT) {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				_bReCheckReadyBit = 0;
				_bHdcpStatus = SV_FAIL;
				HDMI_HDCP_LOG("[1x]HDCP_WAIT_KSV_LIST, fail\n");
			} else {
				_bReCheckReadyBit++;
				vSetHDCPState(HDCP_WAIT_KSV_LIST);
				set_hdcp_delay_time(HDCP_WAIT_KSV_LIST_RETRY_TIMEOUT);
			}
			break;
		}

	case HDCP_READ_KSV_LIST:

		vReadKSVFIFO();
#ifdef SRM_SUPPORT
		if (
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
		((hdmi->repeater_hdcp == true) &&
		((hdmirxhandle != NULL) &&
		(hdmirxhandle->hdcp_is_rpt != NULL) &&
		(!(hdmirxhandle->hdcp_is_rpt(hdmirxdev)) &&
		((i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) &
		IS_REVOCATION_KEY) && (_rSRMInfo.bID == 0x80))))) ||
#endif
		((hdmi->repeater_hdcp == false) &&
		(i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) & IS_REVOCATION_KEY)
		 && (_rSRMInfo.bID == 0x80))) {
			if (enable_mute_for_hdcp_flag)
				mtk_hdmi_AV_mute(hdmi);
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			_bHdcpStatus = SV_FAIL;
			/* 2007/12/27 add 300 ms  issue next coomand */
			set_hdcp_delay_time(HDCP_WAIT_300MS_TIMEOUT);
			break;
		}
#endif
		break;

	case HDCP_COMPARE_V:

		uitemp1 = uiReadHDCPStatus();
		uitemp2 = uiReadIRQStatus01();
		mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00004000);
		udelay(10);
		mtk_hdmi_write(hdmi, TOP_INT_CLR01, 0x00000000);
		if ((uitemp2 & (1 << 14)) || (uitemp1 & (1 << 28))) {
			if ((uitemp2 & (1 << 14))) {
				if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
					if ((hdmirxhandle != NULL) &&
					(hdmirxhandle->set_ksv != NULL))
						hdmirxhandle->set_ksv(
						hdmirxdev, 0, _u2TxBStatus,
						&_bTxBKAV[0], &bKsv_buff[0], true);
#endif
				}
				vSetHDCPState(HDCP_WAIT_RI);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				HDMI_HDCP_LOG("[1x]HDCP_COMPARE_V, pass\n");
				vSetSharedInfo(SI_HDMI_HDCP_RESULT,
					(i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x02));
				/* step 2 OK. */
			} else {
				vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				HDMI_HDCP_LOG("[1x]HDCP_COMPARE_V, fail\n");
				_bHdcpStatus = SV_FAIL;
				if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
					if ((hdmirxhandle != NULL) &&
					(hdmirxhandle->set_ksv != NULL))
						hdmirxhandle->set_ksv(
						hdmirxdev, 0, _u2TxBStatus,
						&_bTxBKAV[0], &bKsv_buff[0], false);
#endif
				}
			}
		} else {
			HDMI_HDCP_LOG("[HDCP]V Not RDY\n");
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			_bHdcpStatus = SV_FAIL;
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
				if ((hdmirxhandle != NULL) &&
				(hdmirxhandle->set_ksv != NULL))
					hdmirxhandle->set_ksv(
					hdmirxdev, 0, _u2TxBStatus,
					&_bTxBKAV[0], &bKsv_buff[0], false);
#endif
			}
		}

		break;

	case HDCP_WAIT_RI:

		mtk_hdmi_AV_unmute(hdmi);
		//hdmi_audio_signal_state(1);
		hdcp_unmute_start_flag = true;
		//hdcp_set_unmute_start_time();
		//HDMI_HDCP_LOG("[HDCP1.x]pass, %lums\n", jiffies);
		vHDCPEncryptState(1); /* for audio notify */
		break;

	case HDCP_CHECK_LINK_INTEGRITY:
#ifdef SRM_SUPPORT

		if (
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
		((hdmi->repeater_hdcp == true) && ((hdmirxhandle != NULL) &&
		(hdmirxhandle->hdcp_is_rpt != NULL) &&
		((!(hdmirxhandle->hdcp_is_rpt(hdmirxdev)) &&
		((i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT)
		& IS_REVOCATION_KEY) && (_rSRMInfo.bID == 0x80)))
		|| (!(hdmirxhandle->hdcp_is_doing_auth(hdmirxdev))
		&& hdmirxhandle->hdcp_is_rpt(hdmirxdev))))) ||
#endif
		((hdmi->repeater_hdcp == false) && ((i4SharedInfo(SI_DVD_HDCP_REVOCATION_RESULT) &
			IS_REVOCATION_KEY) && (_rSRMInfo.bID == 0x80)))) {
			if (enable_mute_for_hdcp_flag)
				mtk_hdmi_AV_mute(hdmi);
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			_bHdcpStatus = SV_FAIL;
			vHDCPEncryptState(0); /* for audio notify */
			set_hdcp_delay_time(HDCP_WAIT_300MS_TIMEOUT);
			break;

		}
#endif
		if (fgCompareRi() == true) {
			vSetSharedInfo(SI_HDMI_HDCP_RESULT,
				(i4SharedInfo(SI_HDMI_HDCP_RESULT) |
				       0x04));
			/* step 3 OK. */
			if (fgIsRepeater()) {
				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) ==
					0x07) {	/* step 1, 2, 3. */
					vSetSharedInfo(SI_HDMI_HDCP_RESULT,
					(i4SharedInfo(SI_HDMI_HDCP_RESULT) |
						       0x08));
					/* all ok. */
				}
			} else {	/* not repeater, don't need step 2. */

				if (i4SharedInfo(SI_HDMI_HDCP_RESULT) ==
					0x05) {	/* step 1, 3. */
					vSetSharedInfo(SI_HDMI_HDCP_RESULT,
					(i4SharedInfo(SI_HDMI_HDCP_RESULT) |
						       0x08));
					/* all ok. */
				}
			}
		} else {
			_bReCompRiCount = 0;
			vSetHDCPState(HDCP_RE_COMPARE_RI);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			vHDCPEncryptState(0); /* for audio notify */
			HDMI_HDCP_LOG("[1x]fai-->HDCP_CHECK_LINK_INTEGRITY\n");

		}
		break;

	case HDCP_RE_COMPARE_RI:
		HDMI_HDCP_LOG("[1x]HDCP_RE_COMPARE_RI\n");
		_bReCompRiCount++;
		if (_bReCompRiCount > 5) {
			vSetHDCPState(HDCP_RE_DO_AUTHENTICATION);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			_bReCompRiCount = 0;
			_bHdcpStatus = SV_FAIL;
			HDMI_HDCP_LOG("[1x]fai-->HDCP_RE_COMPARE_RI\n");
			vHDCPEncryptState(0); /* for audio notify */
		} else {
			if (fgCompareRi() == true) {
				_bReCompRiCount = 0;
				vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vSetSharedInfo(SI_HDMI_HDCP_RESULT,
				(i4SharedInfo(SI_HDMI_HDCP_RESULT) | 0x04));
				/* step 3 OK. */
				if (fgIsRepeater()) {
					if (i4SharedInfo(
						SI_HDMI_HDCP_RESULT) ==
					0x07) {	/* step 1, 2, 3. */
						vSetSharedInfo(
							SI_HDMI_HDCP_RESULT,
					(i4SharedInfo(SI_HDMI_HDCP_RESULT) |
								0x08));
						/* all ok. */
					}
				} else {
					if (i4SharedInfo(SI_HDMI_HDCP_RESULT) ==
					0x05) {	/* step 1, 3. */
						vSetSharedInfo(
						SI_HDMI_HDCP_RESULT,
					(i4SharedInfo(SI_HDMI_HDCP_RESULT) |
								0x08));
						/* all ok. */
					}
				}

			} else {
				vSetHDCPState(HDCP_RE_COMPARE_RI);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			}
		}
		break;

	case HDCP_RE_DO_AUTHENTICATION:
		HDMI_HDCP_LOG("[1x]HDCP_RE_DO_AUTHENTICATION\n");
		if (enable_mute_for_hdcp_flag)
			mtk_hdmi_AV_mute(hdmi);
		vHDCPReset();
		if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			vSetHDCPState(HDCP_WAIT_RESET_OK);
		if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
			if ((hdmirxhandle != NULL) &&
			(hdmirxhandle->hdcp_is_rpt != NULL) &&
			hdmirxhandle->hdcp_is_rpt(hdmirxdev))
				set_hdcp_delay_time(50);
			else
#endif
				set_hdcp_delay_time(HDCP_WAIT_RE_DO_AUTHENTICATION);
		} else
			set_hdcp_delay_time(HDCP_WAIT_RE_DO_AUTHENTICATION);

		}
		break;

	case HDCP_WAIT_RESET_OK:
		if (fgIsHDCPCtrlTimeOut(e_hdcp_state)) {
			vSetHDCPState(HDCP_INIT_AUTHENTICATION);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
		break;

		/*hdcp2 code start here */
	case HDCP2x_WAIT_RES_CHG_OK:
		HDMI_HDCP_LOG("HDCP2x_WAIT_RES_CHG_OK, %lums\n",
			jiffies);
		if (fgIsHDCPCtrlTimeOut(e_hdcp_state)) {
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG("set state HDCP_RECEIVER_NOT_READY\n");
			} else if (hdmi->enable_hdcp == false) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				mtk_hdmi_AV_unmute(hdmi);
				HDMI_HDCP_LOG("set state HDCP_RECEIVER_NOT_READY\n");
			} else {
				if (enable_mute_for_hdcp_flag)
					mtk_hdmi_AV_mute(hdmi);

				if (hdcp_err_0x30_flag == 1) {
					hdcp_err_0x30_flag = 0;
					hdmi_ddc_request(ddc, 3);
					hdmi_ddc_free(ddc, 3);
					if (mtk_hdmi_tmds_over_340M(hdmi) == TRUE)
						mtk_hdmi_send_TMDS_configuration(hdmi,
							SCRAMBLING_ENABLE | TMDS_BIT_CLOCK_RATION);
					else
						mtk_hdmi_send_TMDS_configuration(hdmi, 0);
					HDMI_HDCP_LOG("err=0x30, signal on\n");
					vTxSignalOnOff(hdmi->hdmi_phy_base, true);
				}

				_bReRepeaterPollCnt = 0;
				_bReCertPollCnt = 0;
				_bReAuthCnt = 0;
				_u14SeqMnum = 0;
				vHDMI2xClearINT();
				vSetHDCPState(HDCP2x_LOAD_BIN);
		/* set_hdcp_delay_time(HDCP2x_WAIT_LOADBIN_TIMEOUE); */
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);

				mtk_hdmi_mask(hdmi, HDCP2X_RPT_SEQ_NUM, 0,
					HDCP2X_RPT_SEQ_NUM_M);
				mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0,
					HDCP2X_ENCRYPT_EN);
				mtk_hdmi_mask(hdmi, HPD_DDC_CTRL,
					DDC2_CLOK << DDC_DELAY_CNT_SHIFT,
						 DDC_DELAY_CNT);
				//_bLastHdcpStatus = HDCP_RECEIVER_NOT_READY;
			}
		}
		break;

	case HDCP2x_LOAD_BIN:
		if (hdmi->repeater_hdcp == true) {
			if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
				vSetHDCPState(HDCP_RECEIVER_NOT_READY);
				HDMI_HDCP_LOG(
				"[HDMI][HDCP2.x][REPEATER]TMDS is off\n");
				break;
			}
		}
		HDMI_HDCP_LOG("HDCP2x_LOAD_BIN, flag = %d\n", hdmi->bin_is_loaded);

#if IS_ENABLED(CONFIG_OPTEE)
		/* SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST); */
		vCaHDMIWriteHDCPRST(SOFT_HDCP_CORE_RST, SOFT_HDCP_CORE_RST);
#endif

		if (hdmi->bin_is_loaded == false) {
			HDMI_HDCP_LOG("HDCP 2.x trustzone load Ram & Rom\n");
			fgCaHDMILoadROM();
			hdmi->bin_is_loaded = true;
		}
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_CUPD_START,
			HDCP2X_CUPD_START);
#if IS_ENABLED(CONFIG_OPTEE)
		/* SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST); */
		vCaHDMIWriteHDCPRST(SOFT_HDCP_CORE_NOR, SOFT_HDCP_CORE_RST);
		/* HDCP_TCLK_EN, HDCP_TCLK_EN); */
		vCaHDMIWriteHDCPRST(HDCP_TCLK_EN, HDCP_TCLK_EN);
#endif

		if (hdmi->hdcp_ctrl_state != HDCP2x_LOAD_BIN) {
			HDMI_HDCP_LOG("hdcp state changed by other thread\n");
			break;
		}

		vSetHDCPState(HDCP2x_INITAIL_OK);
		set_hdcp_delay_time(HDCP2x_WAIT_INITAIL_TIMEOUE);
		break;

	case HDCP2x_INITAIL_OK:
		HDMI_HDCP_LOG("HDCP2x_INITAIL_OK, %lums\n", jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		HDMI_HDCP_LOG("reg 0x1A8 = 0x%08x\n", readvalue);
#if IS_ENABLED(CONFIG_OPTEE)
		fgCaHDMIGetTAStatus(ta_status);
#endif
		if ((readvalue & HDCP2X_CCHK_DONE_INT_STA) &&
			((ta_status[0] & 0x03) == 0)) {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is done\n");
			vSetHDCPState(HDCP2x_AUTHENTICATION);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			HDMI_HDCP_LOG("hdcp2.2 ram/rom check is fail, %x\n",
				ta_status[0]);
			vHDCPInitAuth();
			_bHdcpStatus = SV_FAIL;
			hdmi->bin_is_loaded = false;
		}
		break;

	case HDCP2x_AUTHENTICATION:
		HDMI_HDCP_LOG("HDCP2x_AUTHENTICATION, %lums\n", jiffies);
		/* enable reauth_req irq */
		mtk_hdmi_mask(hdmi, TOP_INT_MASK00, 0x02000000, 0x02000000);
		mtk_hdmi_mask(hdmi, HDCP2X_TEST_TP0, 0x75 <<
			HDCP2X_TP1_SHIFT, HDCP2X_TP1);

		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0,
			HDCP2X_HDMIMODE,
			HDCP2X_HDMIMODE |
			HDCP2X_ENCRYPT_EN);
		mtk_hdmi_mask(hdmi, SI2C_CTRL, RX_CAP_RD_TRIG,
			RX_CAP_RD_TRIG);

		mtk_hdmi_mask(hdmi, HDCP2X_POL_CTRL, 0x3402,
			HDCP2X_POL_VAL1 | HDCP2X_POL_VAL0);

		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP0, 0x2a01be03);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP1, 0x09026411);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP2, 0xa7111110);
		mtk_hdmi_write(hdmi, HDCP2X_TEST_TP3, 0x00fa0d7d);
		mtk_hdmi_mask(hdmi, HDCP2X_GP_IN, 0x0 <<
			HDCP2X_GP_IN2_SHIFT, HDCP2X_GP_IN2);
		mtk_hdmi_mask(hdmi, HDCP2X_GP_IN, 0x0 <<
			HDCP2X_GP_IN3_SHIFT, HDCP2X_GP_IN3);

#if IS_ENABLED(CONFIG_OPTEE)
		/* set SM */
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa0000);
#endif

		vHdcpDdcHwPoll(true);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_EN, HDCP2X_EN);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_REAUTH_SW,
			HDCP2X_REAUTH_SW);
		udelay(1);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, 0, HDCP2X_REAUTH_SW);
		vSetHDCPState(HDCP2x_CHECK_CERT_OK);
		set_hdcp_delay_time(HDCP2x_WAIT_CERT_TIMEOUE);
		break;

	case HDCP2x_CHECK_AKE_OK:
		HDMI_HDCP_LOG("HDCP2x_CHECK_AKE_OK, 0x1a8=0x%08x, 0xc60=0x%08x, 0xc8c=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if (readvalue & HDCP2X_AKE_SENT_RCVD_INT_STA) {
			vSetHDCPState(HDCP2x_CHECK_CERT_OK);
			set_hdcp_delay_time(HDCP2x_WAIT_CERT_TIMEOUE);
			HDMI_HDCP_LOG("HDCP2x_CHECK_AKE_OK, _bReAKEtPollCnt = %d\n",
				_bReAKEtPollCnt);
			_bReAKEtPollCnt = 0;
		} else {
			vSetHDCPState(HDCP2x_CHECK_AKE_OK);
			set_hdcp_delay_time(HDCP2x_WAIT_AKE_TIMEOUE);
			_bReAKEtPollCnt++;
		}
		break;

	case HDCP2x_CHECK_CERT_OK:
		HDMI_HDCP_LOG("HDCP2x_CHECK_CERT_OK, 0x1a8=0x%08x, 0xc60=0x%08x, 0xc8c=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if (readvalue & HDCP2X_CERT_SEND_RCVD_INT_STA) {
			vSetHDCPState(HDCP2x_REPEATER_CHECK);
			set_hdcp_delay_time(HDCP2x_WAIT_REPEATER_CHECK_TIMEOUE);
			_bReCertPollCnt = 0;
			hdcp_err_0x30_count = 0;
		} else if (_bReCertPollCnt < 20) {
			_bReCertPollCnt++;
			if (hdcp_check_err_0x30(hdmi) == TRUE) {
				hdmi_ddc_request(ddc, 3);
				hdmi_ddc_free(ddc, 3);
				vSetHDCPState(HDCP2x_WAIT_RES_CHG_OK);
				set_hdcp_delay_time(100);
				break;
			}
			if (fgHDMIHdcp2Err()) {
				_bReCertPollCnt = 0;
				_bHdcpStatus = SV_FAIL;
				vSetHDCPState(HDCP2x_WAIT_RES_CHG_OK);
				//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				vHdcpDdcHwPoll(false);
				HDMI_HDCP_LOG("hdcp2_err=%x, HDCP2X_STATE = %d\n",
				     bHDMIHDCP2Err(), _bReCertPollCnt);
				break;
			}

			HDMI_HDCP_LOG("_bReCertPollCnt=%d\n", _bReCertPollCnt);

			vSetHDCPState(HDCP2x_CHECK_CERT_OK);
			set_hdcp_delay_time(10);
		} else {
			HDMI_HDCP_LOG("0x1a8=0x%08x,0xc60=0x%08x,0xc8c=0x%08x, %d\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), _bReCertPollCnt);
			_bReCertPollCnt = 0;
			_bHdcpStatus = SV_FAIL;
			hdcp_err_0x30_count = 0;
			vHDCPInitAuth();
		}
		break;

	case HDCP2x_REPEATER_CHECK:
		HDMI_HDCP_LOG("HDCP2x_REPEATER_CHECK, 0x1a8=0x%08x, 0xc60=0x%08x, 0xc8c=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0));
		readvalue = mtk_hdmi_read(hdmi, HDCP2X_STATUS_0);
		if (readvalue & HDCP2X_RPT_REPEATER) {
			HDMI_HDCP_LOG("downstream device is repeater\n");
			hdmi->donwstream_is_repeater = true;
			vSetHDCPState(HDCP2x_REPEATER_CHECK_OK);
			set_hdcp_delay_time(HDCP2x_WAIT_REPEATER_POLL_TIMEOUE);
		} else {
			HDMI_HDCP_LOG("downstream device is receiver\n");
			hdmi->donwstream_is_repeater = false;
			vSetHDCPState(HDCP2x_AUTHEN_CHECK);
			set_hdcp_delay_time(HDCP2x_WAIT_AUTHEN_TIMEOUE);

			_bDevice_Count = 0;
			_u2TxBStatus = 0;
			_bTxBKAV[4] = mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff;
			_bTxBKAV[3] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff00) >> 8;
			_bTxBKAV[2] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff0000) >> 16;
			_bTxBKAV[1] = (mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff000000) >> 24;
			_bTxBKAV[0] = (mtk_hdmi_read(hdmi, HDCP2X_RPT_SEQ) & 0xff000000) >> 24;

			_u2TxBStatus = ((mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) & 0x3c) >> 2)
			    + (((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff00) >> 8) << 4)
			    + ((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff) << 9);
			HDMI_HDCP_LOG("[2.x]_bTxBKAV=0x%x;0x%x;0x%x;0x%x;0x%x\n",
				_bTxBKAV[0], _bTxBKAV[1], _bTxBKAV[2],
				_bTxBKAV[3], _bTxBKAV[4]);
			if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
				if ((hdmirxhandle != NULL) &&
				(hdmirxhandle->set_ksv != NULL))
					hdmirxhandle->set_ksv(hdmirxdev, 1,
					_u2TxBStatus, &_bTxBKAV[0],
					&bKsv_buff[0], true);
#endif
			}
			HDMI_HDCP_LOG("[2.x]_u2TxBStatus=0x%x\n", _u2TxBStatus);
		}
		break;

	case HDCP2x_REPEATER_CHECK_OK:
		HDMI_HDCP_LOG
("0x1a8=0x%08x,0xc60=0x%08x,0xc8c=0x%08x,0xc90=0x%08x\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_1));
/*  0x1a0[23] can not work sometime, so add 0x1a0[24][16]  */
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_RPT_RCVID_CHANGED_INT_STA)
		    || (readvalue & HDCP2X_RPT_SMNG_XFER_DONE_INT_STA)
		    || (readvalue & HDCP2X_AUTH_DONE_INT_STA)) {
			_bReRepeaterPollCnt = 0;
			vSetHDCPState(HDCP2x_RESET_RECEIVER);
			set_hdcp_delay_time(HDCP2x_WAIT_RESET_RECEIVER_TIMEOUE);

		} else if ((_bReRepeaterPollCnt <= 30) &&
		(fgHDMIHdcp2Err() == false)) {
			_bReRepeaterPollCnt++;
			HDMI_HDCP_LOG("_bReRepeaterPollCnt=%d\n",
				      _bReRepeaterPollCnt);
			vSetHDCPState(HDCP2x_REPEATER_CHECK_OK);
			set_hdcp_delay_time(HDCP2x_WAIT_REPEATER_POLL_TIMEOUE);
		} else {
			HDMI_HDCP_LOG
("[HDMI][HDCP2.x]hdcp2.2 assume repeater failure, hdcp2_err=%x\n",
			     bHDMIHDCP2Err());
			vHDCPInitAuth();
			_bHdcpStatus = SV_FAIL;
			_bReRepeaterPollCnt = 0;
			if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
				if ((hdmirxhandle != NULL) &&
				(hdmirxhandle->set_ksv != NULL))
					hdmirxhandle->set_ksv(hdmirxdev, 1,
					_u2TxBStatus, &_bTxBKAV[0],
					&bKsv_buff[0], false);
#endif
			}

			set_hdcp_delay_time(HDCP2x_WAIT_LOADBIN_TIMEOUE);
			vSetHDCPState(HDCP2x_WAIT_RES_CHG_OK);
		}
		break;

	case HDCP2x_RESET_RECEIVER:
		HDMI_HDCP_LOG
		("0x1a8 = 0x%08x, 0xc60 = 0x%08x, 0xc8c = 0x%08x, %lums\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, HDCP2X_RPT_RCVID_RD_START,
				 HDCP2X_RPT_RCVID_RD_START);
		udelay(1);
		mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0, HDCP2X_RPT_RCVID_RD_START);
		devicecnt =
		    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & HDCP2X_RPT_DEVCNT) >>
		    HDCP2X_RPT_DEVCNT_SHIFT;

		depth = mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
			HDCP2X_RPT_DEPTH;
		if ((depth == 0) && (devicecnt != 0))
			fgRepeaterError = true;
		count1 = 0;

		bRptID[0] =
		    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
		    HDCP2X_RPT_RCVID_OUT) >>
		    HDCP2X_RPT_RCVID_OUT_SHIFT;
		count1 = count1 + u1CountNum1(bRptID[0]);
		for (i = 1; i < 5 * devicecnt; i++) {
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2,
				HDCP2X_RPT_RCVID_RD,
				HDCP2X_RPT_RCVID_RD);
			udelay(1);
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0,
				HDCP2X_RPT_RCVID_RD);
			if (i < 155) {
				bRptID[i] =
				    (mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) &
				    HDCP2X_RPT_RCVID_OUT) >>
				    HDCP2X_RPT_RCVID_OUT_SHIFT;
				count1 = count1+u1CountNum1(bRptID[i]);
				if ((i % 5) == 4) {
					if (count1 != 20)
						fgRepeaterError = true;
					count1 = 0;
				}
			} else
				HDMI_HDCP_LOG("device count exceed\n");
		}

		for (i = 0; i < 5 * devicecnt; i++) {
			if ((i % 5) == 0)
				HDMI_HDCP_LOG("ID[%d]:", i / 5);

			HDMI_HDCP_LOG("0x%x,", bRptID[i]);

			if ((i % 5) == 4)
				HDMI_HDCP_LOG("\n");
		}
		if (fgRepeaterError) {
			HDMI_HDCP_LOG("repeater parameter invalid\n");
			if (enable_mute_for_hdcp_flag)
				mtk_hdmi_AV_mute(hdmi);
			vHDCPReset();
			vHDCPInitAuth();
			break;
		}

		_bDevice_Count = devicecnt;
		vSetHDCPState(HDCP2x_REPEAT_MSG_DONE);
		set_hdcp_delay_time(HDCP2x_WAIT_REPEATER_DONE_TIMEOUE);

		_bTxBKAV[4] =
			mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff;
		_bTxBKAV[3] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff00) >> 8;
		_bTxBKAV[2] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff0000) >> 16;
		_bTxBKAV[1] =
			(mtk_hdmi_read(hdmi, HDCP2X_RCVR_ID) & 0xff000000) >> 24;
		_bTxBKAV[0] =
			(mtk_hdmi_read(hdmi, HDCP2X_RPT_SEQ) & 0xff000000) >> 24;
		_u2TxBStatus =
			((mtk_hdmi_read(hdmi, HDCP2X_STATUS_0) & 0x3c) >> 2)
		+ (((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff00) >> 8) << 4)
		+ ((mtk_hdmi_read(hdmi, HDCP2X_STATUS_1) & 0xff) << 9);

		if (hdmi->repeater_hdcp == true) {
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
			if ((hdmirxhandle != NULL) &&
			(hdmirxhandle->set_ksv != NULL))
				hdmirxhandle->set_ksv(hdmirxdev, 1,
					_u2TxBStatus, &_bTxBKAV[0], &bRptID[0], true);
#endif
		}
		break;

	case HDCP2x_REPEAT_MSG_DONE:
		HDMI_HDCP_LOG
		  ("0x1a8 = 0x%08x, 0xc60 = 0x%08x, 0xc8c = 0x%08x, %lums\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_RPT_SMNG_XFER_DONE_INT_STA)
		    || (readvalue & HDCP2X_AUTH_DONE_INT_STA)) {
			_bReRepeaterDoneCnt = 0;
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_2, 0,
				HDCP2X_RPT_SMNG_WR_START);
			vSetHDCPState(HDCP2x_AUTHEN_CHECK);
			set_hdcp_delay_time(HDCP2x_WAIT_AUTHEN_TIMEOUE);
		} else if ((_bReRepeaterDoneCnt < 10) &&
		(fgHDMIHdcp2Err() == false)) {
			_bReRepeaterDoneCnt++;
			HDMI_HDCP_LOG("_bReRepeaterDoneCnt=%d\n",
				      _bReRepeaterDoneCnt);
			vSetHDCPState(HDCP2x_REPEAT_MSG_DONE);
			set_hdcp_delay_time(HDCP2x_WAIT_REPEATER_DONE_TIMEOUE);
		} else {
			HDMI_HDCP_LOG(
		"repeater smsg done failure, hdcp2_err=%x\n",
				      bHDMIHDCP2Err());
			vHDCPInitAuth();

			_bReRepeaterDoneCnt = 0;
			_bHdcpStatus = SV_FAIL;
		}
		break;

	case HDCP2x_AUTHEN_CHECK:
		HDMI_HDCP_LOG
("0x1a8 = 0x%08x, 0xc60 = 0x%08x, 0xc8c = 0x%08x, %lums\n",
		     mtk_hdmi_read(hdmi, TOP_INT_STA00),
		     mtk_hdmi_read(hdmi, HPD_DDC_STATUS),
		     mtk_hdmi_read(hdmi, HDCP2X_STATUS_0), jiffies);
		readvalue = mtk_hdmi_read(hdmi, TOP_INT_STA00);
		if ((readvalue & HDCP2X_AUTH_DONE_INT_STA) && (fgHDMIHdcp2Err() == FALSE)) {
			if (hdmi->repeater_hdcp == true) {
			/* can.zeng todo check
			 *	if (fgHdmiRepeaterIsBypassMode())
			 *		vHDMI2TxNotifyToRx(1);
			 */
			}
			HDMI_HDCP_LOG("[HDCP2.x]pass, %lums\n", jiffies);
			vSetHDCPState(HDCP2x_ENCRYPTION);
			set_hdcp_delay_time(HDCP2x_WAIT_AITHEN_DEALY_TIMEOUE);
			_bReAuthCnt = 0;
		} else if (((readvalue & HDCP2X_AUTH_FAIL_INT_STA) &&
				(_bReAuthCnt != 0))
			   || (_bReAuthCnt > REPEAT_CHECK_AUTHHDCP_VALUE) ||
			   fgHDMIHdcp2Err()) {
			HDMI_HDCP_LOG
("[HDMI][HDCP2.x]hdcp2.2 authentication fail-->1, hdcp2_err=%x\n",
			     bHDMIHDCP2Err());
			vHDCPInitAuth();
			_bReAuthCnt = 0;
			vHDCPEncryptState(0); /* for audio notify */
			if (readvalue & HDCP2X_AUTH_FAIL_INT_STA) {
				vCleanAuthFailInt();
				vHDCPEncryptState(0); /* for audio notify */
				HDMI_HDCP_LOG(
				"hdcp2.2 authentication fail-->2\n");
			}
		} else {
			if ((readvalue & HDCP2X_AUTH_FAIL_INT_STA) &&
				(_bReAuthCnt == 0)) {
				vCleanAuthFailInt();
				vHDCPEncryptState(0); /* for audio notify */
				HDMI_HDCP_LOG("hdcp2.2 authentication fail-->3\n");
			}
			_bReAuthCnt++;
			HDMI_HDCP_LOG("hdcp2.2 authentication wait=%d\n",
				      _bReAuthCnt);
			vSetHDCPState(HDCP2x_AUTHEN_CHECK);
			set_hdcp_delay_time(HDCP2x_WAIT_AUTHEN_TIMEOUE);
		}
		break;

	case HDCP2x_ENCRYPTION:
		if (hdmi->hpd != HDMI_PLUG_IN_AND_SINK_POWER_ON) {
			vSetHDCPState(HDCP_RECEIVER_NOT_READY);
			//vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else {
			mtk_hdmi_mask(hdmi, HDCP2X_CTRL_0, HDCP2X_ENCRYPT_EN,
				HDCP2X_ENCRYPT_EN);
			_bReAuthCnt = 0;
			_bHdcpStatus = SV_OK;
			mtk_hdmi_AV_unmute(hdmi);
			//hdmi_audio_signal_state(1);
			hdcp_unmute_start_flag = true;
			//hdcp_set_unmute_start_time();
			//HDMI_HDCP_LOG("[HDCP2.x]pass, %lums\n", jiffies);
			vHDCPEncryptState(1); /* for audio notify */
		}
		break;

	default:
		break;
	}

}

void vHDCPBStatus(void)
{
/* _u2TxBStatus */
	unsigned int u2Temp = 0;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (hdmi->hdcp_2x_support == true) {
		HDMI_HDCP_LOG("hdcp2.2\n");
		if (fgIsRepeater()) {
			HDMI_HDCP_LOG("Bstatus = 0x%x\n", _u2TxBStatus);

			if (_u2TxBStatus & (0x1 << 2))
				HDMI_HDCP_LOG("MAX_CASCADE_EXCEEDED = 1\n");
			else
				HDMI_HDCP_LOG("MAX_CASCADE_EXCEEDED = 0\n");

			u2Temp = (_u2TxBStatus >> 9) & (0x7);
			HDMI_HDCP_LOG("DEPTH = %d\n", u2Temp);

			if (_u2TxBStatus & (0x1 << 3))
				HDMI_HDCP_LOG("MAX_DEVS_EXCEEDED = 1\n");
			else
				HDMI_HDCP_LOG("MAX_DEVS_EXCEEDED = 0\n");

			u2Temp = (_u2TxBStatus >> 4) & (0x1f);
			HDMI_HDCP_LOG("DEVICE_COUNT = %d\n", u2Temp);

			u2Temp = (_u2TxBStatus >> 1) & (0x1);
			if (u2Temp)
				HDMI_HDCP_LOG
	("presence of an hdcp20 compliant repeater in the topology\n");

			u2Temp = (_u2TxBStatus >> 0) & (0x1);
			if (u2Temp)
				HDMI_HDCP_LOG
	("presence of an hdcp1x compliant repeater in the topology\n");
		} else {
			HDMI_HDCP_LOG("A Connected device is only Sink!!!\n");
		}

	} else {
		HDMI_HDCP_LOG("hdcp1.4\n");
		if (fgIsRepeater()) {
			HDMI_HDCP_LOG("Bstatus = 0x%x\n", _u2TxBStatus);
			if (_u2TxBStatus & (0x1 << 12))
				HDMI_HDCP_LOG("HDMI_MODE = 1\n");
			else
				HDMI_HDCP_LOG("HDMI_MODE = 0\n");

			if (_u2TxBStatus & (0x1 << 11))
				HDMI_HDCP_LOG("MAX_CASCADE_EXCEEDED = 1\n");
			else
				HDMI_HDCP_LOG("MAX_CASCADE_EXCEEDED = 0\n");

			u2Temp = (_u2TxBStatus >> 8) & (0x7);
			HDMI_HDCP_LOG("DEPTH = %d\n", u2Temp);

			if (_u2TxBStatus & (0x1 << 7))
				HDMI_HDCP_LOG("MAX_DEVS_EXCEEDED = 1\n");
			else
				HDMI_HDCP_LOG("MAX_DEVS_EXCEEDED = 0\n");

			u2Temp = _u2TxBStatus & 0x7F;
			HDMI_HDCP_LOG("DEVICE_COUNT = %d\n", u2Temp);
		} else {
			HDMI_HDCP_LOG("A Connected device is only Sink!!!\n");
		}
	}
}

void vReadHdcpVersion(void)
{
	unsigned char bTemp = 0;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;
	struct mtk_hdmi_ddc *ddc = hdmi_ddc_ctx_from_mtk_hdmi(hdmi);

	if (ddc == NULL) {
		HDMI_HDCP_LOG("NULL pointer\n");
		return;
	}

	if (!fgDDCDataRead(ddc, RX_ID, RX_REG_HDCP2VERSION,
		1, &bTemp)) {
		HDMI_HDCP_LOG("read hdcp version fail from sink\n");
		hdmi->hdcp_2x_support = false;
	} else if (bTemp & 0x4) {
		hdmi->hdcp_2x_support = true;
		HDMI_HDCP_LOG("sink support hdcp2.2 version\n");
	} else {
		hdmi->hdcp_2x_support = false;
		HDMI_HDCP_LOG("sink support hdcp1.x version\n");
	}
}

