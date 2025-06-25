/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_HDR_H
#define _MTK_HDR_H

#include "mtk_hdmi.h"

#define HDR_DEBUG_DISABLE_METADATA        (1<<0)
#define HDR_DEBUG_DISABLE_HDR             (1<<1)
#define HDR_DEBUG_DISABLE_BT2020          (1<<2)
#define HDR_DEBUG_DISABLE_DV_HDR       (1<<3)
#define HDR_DEBUG_DISABLE_PHI_HDR         (1<<4)

#define DOVI_METADATA_MAX_LENGTH 100
#define PHLP_METADATA_MAX_LENGTH 100
#define HDR10_PLUS_METADATA_MAX_LENGTH 100

#define HDR_TYPE            0x87
#define HDR_VERS            0x01
#define HDR_LEN             0x1A

/* Phi Hdr Info Frame */
#define PHI_HDR_TYPE            0x81
#define PHI_HDR_VERS            0x01
#define PHI_HDR_LEN             0x04

/* DV VS Info Frame */
#define DVVSIF_TYPE            0x81
#define DVVSIF_VERS            0x01
#define DVVSIF_LEN             0x1B

/* Dynamic HDR EMP */
#define DYNAMIC_HDR_EMP_TYPE            0x7F
#define DYNAMIC_HDR_EMP_DS_TYPE         0x01
#define DYNAMIC_HDR_EMP_AFR             0x00
#define DYNAMIC_HDR_EMP_VFR             0x01
#define DYNAMIC_HDR_EMP_SYNC            0x01
#define DYNAMIC_HDR_EMP_ORGID           0x02
#define EMP_SIZE_FIRST      21
#define EMP_SIZE_MAX        28

/* Dynamic HDR10 PLUS VSIF */
#define DYNAMIC_HDR10P_VSIF_TYPE		0x01
#define DYNAMIC_HDR10P_VSIF_VERSION		0x01
#define DYNAMIC_HDR10P_VSIF_MAXLEN		27
#define DYNAMIC_HDR10P_VSIF_PKTHW		GEN_PKT_HW7

#define HDR_PACKET_DISABLE    0x00
#define HDR_PACKET_ACTIVE     0x01
#define HDR_PACKET_ZERO       0x02

enum VID_GAMMA_T {
	GAMMA_ST2084 = 1,
	GAMMA_HLG = 2,
	GAMMA_24 = 1,
	GAMMA_709 = 2,
};

enum VID_PLA_DR_TYPE_T {
	VID_PLA_DR_TYPE_SDR = 0,
	VID_PLA_DR_TYPE_HDR10,
	VID_PLA_DR_TYPE_DOVI,
	VID_PLA_DR_TYPE_PHLP_RESVERD,
	VID_PLA_DR_TYPE_DOVI_LOWLATENCY,
	VID_PLA_DR_TYPE_HDR10_PLUS,
	VID_PLA_DR_TYPE_HDR10_PLUS_VSIF,
};

struct VID_STATIC_HDMI_MD_T {
	unsigned short ui2_DisplayPrimariesX[3];
	unsigned short ui2_DisplayPrimariesY[3];
	unsigned short ui2_WhitePointX;
	unsigned short ui2_WhitePointY;
	unsigned short ui2_MaxDisplayMasteringLuminance;
	unsigned short ui2_MinDisplayMasteringLuminance;
	unsigned short ui2_MaxCLL;
	unsigned short ui2_MaxFALL;
	unsigned char fgNeedUpdStaticMeta;
};

struct HDMI_STATIC_METADATA_INFO_T {
	char ui1_EOTF;
	char ui1_Static_Metadata_ID;
	unsigned short ui2_DisplayPrimariesX0;
	unsigned short ui2_DisplayPrimariesY0;
	unsigned short ui2_DisplayPrimariesX1;
	unsigned short ui2_DisplayPrimariesY1;
	unsigned short ui2_DisplayPrimariesX2;
	unsigned short ui2_DisplayPrimariesY2;
	unsigned short ui2_WhitePointX;
	unsigned short ui2_WhitePointY;
	unsigned short ui2_MaxDisplayMasteringLuminance;
	unsigned short ui2_MinDisplayMasteringLuminance;
	unsigned short ui2_MaxCLL;
	unsigned short ui2_MaxFALL;
};

struct VID_PLA_DOVI_METADATA_INFO_T {
	unsigned short ui2_DisplayPrimariesX[3];
	unsigned int ui4_RpuIdx;
	unsigned int ui4_RpuStartAddr;
	unsigned int ui4_RpuSize;
};

union VID_PLA_DOVI_METADATA_UNION_T {
	char dovi_metada_buffer[DOVI_METADATA_MAX_LENGTH];
	struct VID_PLA_DOVI_METADATA_INFO_T dovi_metadata_info;
};

struct VID_PLA_PHLP_METADATA_INFO_T {
	unsigned int ui4_PhilipHdrAddr;
	unsigned int ui4_PhilipHdrSize;
	unsigned int ui4_Index;

};

union VID_PLA_PHLP_METADATA_UNION_T {
	char phlp_metada_buffer[DOVI_METADATA_MAX_LENGTH];
	struct VID_PLA_PHLP_METADATA_INFO_T phlp_metadata_info;
};

struct VID_DOVI_LOWLATENCY_MD_INFO_T {
	char fgBackltCtrlMdPresent;
	unsigned int ui4_EffTmaxPQ;
};

struct VID_HDR10_PLUS_METADATA_INFO_T {
	unsigned long ui4_Hdr10PlusAddr;
	unsigned int ui4_Hdr10PlusSize;
	unsigned int ui4_Hdr10PlusIdx;
	unsigned int ui4_isTrustZone;
};

union VID_HDR10_PLUS_METADATA_UNION_T {
	char hdr10_plus_metada_buffer[HDR10_PLUS_METADATA_MAX_LENGTH];
	struct VID_HDR10_PLUS_METADATA_INFO_T hdr10p_metadata_info;
};

union VID_PLA_HDR_METADATA_UNION_T {
	struct VID_STATIC_HDMI_MD_T hdr10_metadata;
	union VID_PLA_DOVI_METADATA_UNION_T dovi_metadata;
	union VID_PLA_PHLP_METADATA_UNION_T phlp_metadata;
	struct VID_DOVI_LOWLATENCY_MD_INFO_T dovi_lowlatency_metadata;
	union VID_HDR10_PLUS_METADATA_UNION_T hdr10_plus_metadata;
};

struct VID_PLA_HDR_METADATA_INFO_T {
	enum VID_PLA_DR_TYPE_T e_DynamicRangeType;
	union VID_PLA_HDR_METADATA_UNION_T metadata_info;
	unsigned char fgIsMetadata;
};

void mtk_hdmi_hw_static_hdr_infoframe(
	struct mtk_hdmi *hdmi, char bEnable, char *pr_bData);
void mtk_hdmi_hw_dv_vsif(struct mtk_hdmi *hdmi, bool fgEnable, bool fgLowLatency,
	bool fgDVSignal, bool fgBackltCtrlMdPresent, unsigned int u4EfftmaxPQ);

void vBT2020Enable(bool fgEnable);
void vHdr10Enable(bool fgEnable);
void vHdr10PlusEnable(bool fgEnable);
void vHdr10PlusVSIFEnable(bool fgEnable);
void vDVHdrEnable(bool fgEnable, bool use_DV_vsif);
void vLowLatencyDVEnable(bool fgEnable);
void vSetStaticHdrType(enum VID_GAMMA_T bType);
void vVdpSetHdrMetadata(bool enable, struct VID_PLA_HDR_METADATA_INFO_T hdr_metadata);

void Hdr10DelayOffHandler(struct work_struct *data);
void Hdr10pVsifDelayOffHandler(struct work_struct *data);
void vInitHdr(void);
void reset_hdmi_hdr_status(void);

extern char _bHdrType;
extern char _bStaticHdrType;
extern bool _fgDVHdrEnable;
extern bool _fgBT2020Enable;
extern char *_bHdrMetadataBuff;
extern char _bStaticHdrStatus;
extern bool _use_DV_vsif;
extern bool _fgLowLatencyDVEnable;
extern bool _fgBackltCtrlMDPresent;
extern unsigned int _u4EffTmaxPQ;

#endif /* _MTK_HDR_H */
