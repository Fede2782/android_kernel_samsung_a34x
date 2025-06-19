/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __hdmihdcp_h__
#define __hdmihdcp_h__

#include <linux/timer.h>

enum HDMI_HDCP_VERSION {
	HDMI_HDCP_1_x,
	HDMI_HDCP_2_x
};

struct HDMI_HDCP_BKSV_INFO {
	unsigned char bksv_list[160];
	unsigned int bstatus;
	unsigned char is_plug_in;
	unsigned char is_hdcp_ok;
};

enum HDCP_CTRL_STATE_T {
	HDCP_RECEIVER_NOT_READY = 0x00,
	HDCP_READ_EDID,
	HDCP_INIT_AUTHENTICATION,
	HDCP_WAIT_R0,
	HDCP_COMPARE_R0,
	HDCP_WAIT_RI,
	HDCP_CHECK_LINK_INTEGRITY,
	HDCP_RE_DO_AUTHENTICATION,
	HDCP_RE_COMPARE_RI,
	HDCP_RE_COMPARE_R0,
	HDCP_CHECK_REPEATER,
	HDCP_WAIT_KSV_LIST,
	HDCP_READ_KSV_LIST,
	HDCP_COMPARE_V,
	HDCP_RETRY_FAIL,
	HDCP_WAIT_RESET_OK,
	HDCP_WAIT_RES_CHG_OK,
	HDCP_WAIT_SINK_UPDATE_RI_DDC_PORT,

	HDCP2x_WAIT_RES_CHG_OK,
	HDCP2x_SWITCH_OK,
	HDCP2x_LOAD_BIN,
	HDCP2x_INITAIL_OK,
	HDCP2x_AUTHENTICATION,
	HDCP2x_AUTHEN_CHECK,
	HDCP2x_CHECK_AKE_OK,
	HDCP2x_CHECK_CERT_OK,
	HDCP2x_REPEATER_CHECK,
	HDCP2x_REPEATER_CHECK_OK,
	HDCP2x_RESET_RECEIVER,
	HDCP2x_REPEAT_MSG_DONE,
	HDCP2x_ENCRYPTION,
	HDCP2x_POLLING_RXSTATUS
};

enum HDMI_CTRL_STATE_T {
	HDMI_STATE_IDLE = 0,
	HDMI_STATE_HOT_PLUG_OUT,
	HDMI_STATE_HOT_PLUGIN_AND_POWER_ON,
	HDMI_STATE_HOT_PLUG_IN_ONLY,
	HDMI_STATE_READ_EDID,
	HDMI_STATE_POWER_ON_READ_EDID,
	HDMI_STATE_POWER_OFF_HOT_PLUG_OUT,

};

enum HDMI_HDCP_KEY_T {
	EXTERNAL_KEY = 0,
	INTERNAL_NOENCRYPT_KEY,
	INTERNAL_ENCRYPT_KEY
};

/* Notice: there are three device ID for SiI9993 (Receiver) */
#define RX_ID       0x3A	/* Max'0308'04, 0x74 */
/* (2.2) Define the desired register address of receiver */
/* Software Reset Register */
#define RX_REG_SRST           0x05
/* An register (total 8 bytes, address from 0x18 ~ 0x1F) */
#define RX_REG_HDCP_AN        0x18
/* Aksv register (total 5 bytes, address from 0x10 ~ 0x14) */
#define RX_REG_HDCP_AKSV      0x10
/* Bksv register (total 5 bytes, address from 0x00 ~ 0x04) */
#define RX_REG_HDCP_BKSV      0x00
/* BCAPS register */
#define RX_REG_BCAPS          0x40
#define RX_BIT_ADDR_RPTR      0x40	/* bit 6 */
#define RX_BIT_ADDR_READY     0x20	/* bit 5 */
#define RX_REG_HDCP2VERSION   0x50
#define RX_REG_RXSTATUS   0x70
#define RX_RXSTATUS_REAUTH_REQ (0x0800)

#define RX_REG_BSTATUS1       0x41
#define DEVICE_COUNT_MASK     0xff
#define MAX_DEVS_EXCEEDED  (0x01<<7)
#define MAX_CASCADE_EXCEEDED (0x01<<3)

#define RX_REG_KSV_FIFO       0x43
#define RX_REG_REPEATER_V     0x20
#define RX_REG_TMDS_CONFIG     0x20
#define SCRAMBLING_ENABLE     (1<<0)
#define TMDS_BIT_CLOCK_RATION  (1<<1)

/* Ri register (total 2 bytes, address from 0x08 ~ 0x09) */
#define RX_REG_RI             0x08
#define RX_REG_SCRAMBLE             0xA8

/* (2) Define the counter for An register byte */
#define HDCP_AN_COUNT                 8

/* (3) Define the counter for HDCP Aksv register byte */
#define HDCP_AKSV_COUNT               5

/* (3) Define the counter for HDCP Bksv register byte */
#define HDCP_BKSV_COUNT               5

/* (4) Define the counter for Ri register byte */
#define HDCP_RI_COUNT                 2

#define HDCP_WAIT_5MS_TIMEOUT          5	/* 5 ms, */
#define HDCP_WAIT_10MS_TIMEOUT          10	/* 10 ms, */
#define HDCP_WAIT_300MS_TIMEOUT          300	/* 10 ms, */
/* 25//for timer 5ms      // 100 ms, */
#define HDCP_WAIT_R0_TIMEOUT          110
/* 4600//5000//5500//1100//for timer 5ms //5000   // 5.5 sec */
#define HDCP_WAIT_KSV_LIST_TIMEOUT    100
#define HDCP_WAIT_KSV_LIST_RETRY_TIMEOUT    100
/* 500//for timer 5ms //2000   // 2.5 sec, */
#define HDCP_WAIT_RI_TIMEOUT          2500
/* 20////for timer 20ms 200 //kenny 100->200 */
#define HDCP_WAIT_RE_DO_AUTHENTICATION 200
/* 50//for 20ms timer //1000   // 1 sec, 50*20ms */
#define HDCP_WAIT_RECEIVER_READY      1000
#define HDCP_WAIT_RES_CHG_OK_TIMEOUE 500	/* 30//6 */
#define HDCP_WAIT_V_RDY_TIMEOUE 500	/* 30//6 */
#define HDCP_CHECK_KSV_LIST_RDY_RETRY_COUNT    56	/* 10 */

#define HDCP2x_WAIT_RES_CHG_OK_TIMEOUE 500
#define HDCP2x_WAIT_AUTHEN_TIMEOUE 100
#define HDCP2x_WAIT_RAMROM_TIMEOUE 50	/* 3 */
#define HDCP2x_WAIT_AUTHEN_AGAIN_TIMEOUE 100
#define HDCP2x_WAIT_LOADBIN_TIMEOUE 10
#define HDCP2x_WAIT_POLLING_TIMEOUE 1000
#define HDCP2x_WAIT_AITHEN_DEALY_TIMEOUE 250
#define HDCP2x_WAIT_REPEATER_POLL_TIMEOUE 100
#define HDCP2x_WAIT_RESET_RECEIVER_TIMEOUE 10
#define HDCP2x_WAIT_REPEATER_DONE_TIMEOUE 100
#define HDCP2x_WAIT_REPEATER_CHECK_TIMEOUE 150
#define HDCP2x_WAIT_SWITCH_TIMEOUE 300
#define HDCP2x_WAIT_CERT_TIMEOUE 20
#define HDCP2x_WAIT_INITAIL_TIMEOUE 10
#define HDCP2x_WAIT_AKE_TIMEOUE 10

#define SRM_SIZE 5120


#define  HDMI_H0  0x67452301
#define  HDMI_H1  0xefcdab89
#define  HDMI_H2  0x98badcfe
#define  HDMI_H3  0x10325476
#define  HDMI_H4  0xc3d2e1f0

#define  HDMI_K0  0x5a827999
#define  HDMI_K1  0x6ed9eba1
#define  HDMI_K2  0x8f1bbcdc
#define  HDMI_K3  0xca62c1d6

/* for HDCP key access method */
/* Key accessed by host */
#define HOST_ACCESS                  1
/* Key auto accessed by HDCP hardware from eeprom */
#define NON_HOST_ACCESS_FROM_EEPROM  2
/* Key auto accessed by HDCP hardware from MCM */
#define NON_HOST_ACCESS_FROM_MCM     3
#define NON_HOST_ACCESS_FROM_GCPU    4
/* for HDCP */
#define KSV_BUFF_SIZE 192
#define KSV_LIST_SIZE 60
#define HDCP_KEY_RESERVE 287

/* for SI_DVD_HDCP_REVOCATION_RESULT */
#define REVOCATION_NOT_CHK  0
#define IS_REVOCATION_KEY      (1<<0)
#define NOT_REVOCATION_KEY     (1<<1)
#define REVOCATION_IS_CHK      (1<<2)

#define HDCPKEY_LENGTH_DRM 512

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_HDMI_HDCP)
bool fgIsHDCPCtrlTimeOut(enum HDCP_CTRL_STATE_T e_hdcp_state);
bool fgHDMIHdcp2Auth(void);
unsigned int bHDMIHDCP2Err(void);
void hdcp2x_load_rom_ram(void);
void hdcp_service(enum HDCP_CTRL_STATE_T e_hdcp_state);
void vSetHDCPState(enum HDCP_CTRL_STATE_T e_state);
void vHDCPInitAuth(void);
extern atomic_t hdmi_hdcp_event;
void vReadHdcpVersion(void);
void vHDCPReset(void);
void enable_mute_for_hdcp(bool enable);
void notify_hpd_to_hdcp(int hpd);
extern int hdcp_delay_time;

#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
#include "mtk_hdmi_rpt.h"
extern struct MTK_HDMIRX *hdmirxhandle;
extern struct device *hdmirxdev;
#endif

#endif

#endif
