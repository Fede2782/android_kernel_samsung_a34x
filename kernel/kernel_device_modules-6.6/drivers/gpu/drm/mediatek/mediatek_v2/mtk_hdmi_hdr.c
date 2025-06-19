// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_hdmi_regs.h"
#include "mtk_hdmi_hdr.h"
#include <linux/slab.h>

unsigned char mtk_hdr_log = 1;

#define HDMI_HDR_LOG(fmt, arg...) \
	do {	if (mtk_hdr_log) { \
		pr_info("[HDMI][HDR] %s,%d "fmt, __func__, __LINE__, ##arg); \
		} \
	} while (0)

#define HDMI_HDR_FUNC()	\
	do {	if (mtk_hdr_log) \
		pr_info("[HDMI][HDR] %s\n", __func__); \
	} while (0)


static struct PACKET_HW_T pkthw[GEN_PKT_HW_NUM] = {
	{
		.hw_num = GEN_PKT_HW1,
		.addr_header = TOP_GEN_HEADER,
		.addr_pkt = TOP_GEN_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN_EN | GEN_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW2,
		.addr_header = TOP_GEN2_HEADER,
		.addr_pkt = TOP_GEN2_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN2_EN | GEN2_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN2_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW3,
		.addr_header = TOP_GEN3_HEADER,
		.addr_pkt = TOP_GEN3_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN3_EN | GEN3_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN3_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW4,
		.addr_header = TOP_GEN4_HEADER,
		.addr_pkt = TOP_GEN4_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN4_EN | GEN4_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN4_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW5,
		.addr_header = TOP_GEN5_HEADER,
		.addr_pkt = TOP_GEN5_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN5_EN | GEN5_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN5_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW6,
		.addr_header = TOP_GEN6_HEADER,
		.addr_pkt = TOP_GEN6_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN6_EN | GEN6_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN6_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW7,
		.addr_header = TOP_GEN7_HEADER,
		.addr_pkt = TOP_GEN7_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN7_EN | GEN7_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN7_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW8,
		.addr_header = TOP_GEN8_HEADER,
		.addr_pkt = TOP_GEN8_PKT00,
		.addr_wr_en = TOP_INFO_EN,
		.mask_wr_en = GEN8_EN | GEN8_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN8_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW9,
		.addr_header = TOP_GEN9_HEADER,
		.addr_pkt = TOP_GEN9_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN9_EN | GEN9_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN9_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW10,
		.addr_header = TOP_GEN10_HEADER,
		.addr_pkt = TOP_GEN10_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN10_EN | GEN10_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN10_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW11,
		.addr_header = TOP_GEN11_HEADER,
		.addr_pkt = TOP_GEN11_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN11_EN | GEN11_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN11_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW12,
		.addr_header = TOP_GEN12_HEADER,
		.addr_pkt = TOP_GEN12_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN12_EN | GEN12_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN12_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW13,
		.addr_header = TOP_GEN13_HEADER,
		.addr_pkt = TOP_GEN13_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN13_EN | GEN13_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN13_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW14,
		.addr_header = TOP_GEN14_HEADER,
		.addr_pkt = TOP_GEN14_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN14_EN | GEN14_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN14_RPT_EN,
	},
	{
		.hw_num = GEN_PKT_HW15,
		.addr_header = TOP_GEN15_HEADER,
		.addr_pkt = TOP_GEN15_PKT00,
		.addr_wr_en = TOP_INFO_EN_EXPAND,
		.mask_wr_en = GEN15_EN | GEN15_EN_WR,
		.addr_rep_en = TOP_INFO_RPT,
		.mask_rep_en = GEN15_RPT_EN,
	},

};

char _bHdrType;
char *_bHdrMetadataBuff;
char _bStaticHdrType = GAMMA_ST2084;
bool _fgLowLatencyDVEnable;
bool _fgBackltCtrlMDPresent;
unsigned int _u4EffTmaxPQ;
bool _fgBT2020Enable;
bool _fgDVHdrEnable;
bool _use_DV_vsif;
unsigned int _u4HdrDebugDisableType;
char _bStaticHdrStatus = HDR_PACKET_DISABLE;
bool dovi_off_delay_needed;

void vHalEnablePacket(struct mtk_hdmi *hdmi,
	struct PACKET_HW_T hw, bool en)
{
	if (en) {
		mtk_hdmi_mask(hdmi, hw.addr_rep_en, hw.mask_rep_en, hw.mask_rep_en);
		mtk_hdmi_mask(hdmi, hw.addr_wr_en, hw.mask_wr_en, hw.mask_wr_en);
	} else {
		mtk_hdmi_mask(hdmi, hw.addr_wr_en, 0, hw.mask_wr_en);
		mtk_hdmi_mask(hdmi, hw.addr_rep_en, 0, hw.mask_rep_en);
	}
}

void vHalSendPacket(struct mtk_hdmi *hdmi,
	struct PACKET_HW_T hw, unsigned char hb[3], unsigned char pb[28])
{
	unsigned char i;

	vHalEnablePacket(hdmi, hw, false);
	mtk_hdmi_write(hdmi, hw.addr_header, (hb[2] << 16) +
		(hb[1] << 8) + (hb[0] << 0));
	for (i = 0; i < 4; i++) {
		mtk_hdmi_write(hdmi, hw.addr_pkt + 8 * i,
			(pb[3 + 7 * i] << 24) + (pb[2 + 7 * i] << 16) +
			(pb[1 + 7 * i] << 8) + (pb[0 + 7 * i] << 0));
		mtk_hdmi_write(hdmi, hw.addr_pkt + 8 * i + 4,
			(pb[6 + 7 * i] << 16) + (pb[5 + 7 * i] << 8) +
			(pb[4 + 7 * i] << 0));
	}
	vHalEnablePacket(hdmi, hw, true);

	HDMI_HDR_LOG("hw_num=%d, hb[0-2]=0x%02x,0x%02x,0x%02x\n",
		hw.hw_num, hb[0], hb[1], hb[2]);
	HDMI_HDR_LOG("pb[0-27]=");
	for (i = 0; i < EMP_SIZE_MAX; i++)
		HDMI_HDR_LOG(",0x%02x", pb[i]);
	HDMI_HDR_LOG("\n");
}

//can.zeng done->void vHalSendStaticHdrInfoFrame()
void mtk_hdmi_hw_static_hdr_infoframe(
	struct mtk_hdmi *hdmi, char bEnable, char *pr_bData)
{

	char bHDR_CHSUM = 0;
	char i;

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN4_EN | GEN4_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN4_RPT_EN);

	if (pr_bData == NULL)
		return;

	HDMI_HDR_LOG("bEnable = %d\n", bEnable);

	if (bEnable == HDR_PACKET_ACTIVE) {
		mtk_hdmi_write(hdmi, TOP_GEN4_HEADER, (HDR_LEN << 16) +
			(HDR_VERS << 8) + (HDR_TYPE << 0));
		bHDR_CHSUM = HDR_LEN + HDR_VERS + HDR_TYPE;
		for (i = 0; i < HDR_LEN; i++)
			bHDR_CHSUM += (*(pr_bData + i));
		bHDR_CHSUM = 0x100 - bHDR_CHSUM;
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT00,
			((*(pr_bData + 2)) << 24) + (*(pr_bData + 1) << 16) +
			((*(pr_bData + 0)) << 8) + (bHDR_CHSUM << 0));
		/* HDMI_HDR_LOG(" 1 data0=0x%x, data1=0x%x, data2=0x%x, data3=0x%x\n",
		 *	(bHDR_CHSUM << 0), ((*(pr_bData + 0)) << 8),
		 *	(*(pr_bData + 1) << 16), ((*(pr_bData + 2)) << 24));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT01,
			(((*(pr_bData + 5)) << 16) + ((*(pr_bData + 4)) << 8) +
			((*(pr_bData + 3)) << 0)));
		/* HDMI_HDR_LOG(" 2 data0=0x%x, data1=0x%x, data2=0x%x\n",
		 *	((*(pr_bData + 3)) << 0), ((*(pr_bData + 4)) << 8),
		 *	((*(pr_bData + 5)) << 16));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT02,
			((*(pr_bData + 9)) << 24) + ((*(pr_bData + 8)) << 16) +
			((*(pr_bData + 7)) << 8) + ((*(pr_bData + 6)) << 0));
		/* HDMI_HDR_LOG(" 3 data0=0x%x, data1=0x%x, data2=0x%x, data3=0x%x\n",
		 *	((*(pr_bData + 6)) << 0), ((*(pr_bData + 7)) << 8),
		 *	((*(pr_bData + 8)) << 16), ((*(pr_bData + 9)) << 24));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT03,
			((*(pr_bData + 12)) << 16) + ((*(pr_bData + 11)) << 8) +
			((*(pr_bData + 10)) << 0));
		/* HDMI_HDR_LOG(" 4 data0=0x%x, data1=0x%x, data2=0x%x\n",
		 *	((*(pr_bData + 10)) << 0), ((*(pr_bData + 11)) << 8),
		 *	((*(pr_bData + 12)) << 16));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT04,
			((*(pr_bData + 16)) << 24) + (*(pr_bData + 15) << 16) +
			((*(pr_bData + 14)) << 8) + ((*(pr_bData + 13)) << 0));
		/* HDMI_HDR_LOG(" 5 data0=0x%x, data1=0x%x, data2=0x%x, data3=0x%x\n",
		 *	((*(pr_bData + 13)) << 0), ((*(pr_bData + 14)) << 8),
		 *	(*(pr_bData + 15) << 16), ((*(pr_bData + 16)) << 24));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT05,
			(*(pr_bData + 19) << 16) + ((*(pr_bData + 18)) << 8) +
			((*(pr_bData + 17)) << 0));
		/* HDMI_HDR_LOG(" 6 data0=0x%x, data1=0x%x, data2=0x%x\n",
		 *	((*(pr_bData + 17)) << 0), ((*(pr_bData + 18)) << 8),
		 *	(*(pr_bData + 19) << 16));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT06,
			((*(pr_bData + 23)) << 24) + (*(pr_bData + 22) << 16) +
			((*(pr_bData + 21)) << 8) + ((*(pr_bData + 20)) << 0));
		/* HDMI_HDR_LOG(" 7 data0=0x%x, data1=0x%x, data2=0x%x, data3=0x%x\n",
		 *	((*(pr_bData + 20)) << 0), ((*(pr_bData + 21)) << 8),
		 *	(*(pr_bData + 22) << 16), ((*(pr_bData + 23)) << 24));
		 */
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT07,
			((*(pr_bData + 25)) << 8) + ((*(pr_bData + 24)) << 0));
		/* HDMI_HDR_LOG(" 8 data0=0x%x, data1=0x%x\n",
		 *	((*(pr_bData + 24)) << 0), ((*(pr_bData + 25)) << 8));
		 */
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN4_RPT_EN, GEN4_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, GEN4_EN |
			GEN4_EN_WR, GEN4_EN | GEN4_EN_WR);

	} else if (bEnable == HDR_PACKET_ZERO) {
		mtk_hdmi_write(hdmi, TOP_GEN4_HEADER,
				  (HDR_LEN << 16) + (HDR_VERS << 8) +
				  (HDR_TYPE << 0));
		bHDR_CHSUM = HDR_LEN + HDR_VERS + HDR_TYPE;
		bHDR_CHSUM = 0x100 - bHDR_CHSUM;
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT00, bHDR_CHSUM);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT01, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT02, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT03, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT04, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT05, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT06, 0);
		mtk_hdmi_write(hdmi, TOP_GEN4_PKT07, 0);

		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN4_RPT_EN, GEN4_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, GEN4_EN |
			GEN4_EN_WR, GEN4_EN | GEN4_EN_WR);

	}

}


//can.zeng done->void vHalSendHdr10PlusVSIF()
//can.zeng todo verify
void mtk_hdmi_hw_samsung_hdr10p_vsif(
	struct mtk_hdmi *hdmi, char bEnable,
	union VID_HDR10_PLUS_METADATA_UNION_T *pr_bData)
{
	unsigned char i;
	unsigned char HB[3] = {0};
	unsigned char PB[DYNAMIC_HDR10P_VSIF_MAXLEN + 1] = {0};
	unsigned char chsum = 0;
	unsigned char size =
		(unsigned char)(pr_bData->hdr10p_metadata_info.ui4_Hdr10PlusSize & 0xff);
	unsigned char *meta =
		(unsigned char *)(unsigned long)pr_bData->hdr10p_metadata_info.ui4_Hdr10PlusAddr;

	if (size > DYNAMIC_HDR10P_VSIF_MAXLEN) {
		HDMI_HDR_LOG("Error in %s,size=%d", __func__, size);
		return;
	}

	HB[0] = 0x80 | DYNAMIC_HDR10P_VSIF_TYPE;
	HB[1] = DYNAMIC_HDR10P_VSIF_VERSION;
	HB[2] = size & 0x1f;
	chsum = HB[0] + HB[1] + HB[2];
	for (i = 0; i < size; i++) {
		PB[i + 1] = *(meta + i);
		chsum += PB[i + 1];
	}
	PB[0] = 0x100 - chsum;
	vHalSendPacket(hdmi, pkthw[DYNAMIC_HDR10P_VSIF_PKTHW], HB, PB);
}


void vDynamicHdrEMP_HeaderInit(unsigned char *hb,
	unsigned int *size,
	unsigned char *sequence_index, char *first, char *last)
{
	*(hb + 0) = DYNAMIC_HDR_EMP_TYPE;
	if (*size <= EMP_SIZE_FIRST) {
		*first = 1;
		*last = 1;
		*sequence_index = 0;
	} else if ((EMP_SIZE_FIRST + (*sequence_index) * EMP_SIZE_MAX) < *size) {
		if (*sequence_index == 0)
			*first = 1;
		else
			*first = 0;
		*last = 0;
	} else if ((EMP_SIZE_FIRST + (*sequence_index) * EMP_SIZE_MAX) >= *size) {
		*first = 0;
		*last = 1;
	}
	*(hb + 1) = (((*first) << 7) | ((*last) << 6)) & 0xc0;
	*(hb + 2) = *sequence_index;
}

//can.zeng done->void vHalSendDynamicHdrEMPs()
void mtk_hdmi_hw_dynamic_hdr_emp(struct mtk_hdmi *hdmi,
	char bEnable, union VID_HDR10_PLUS_METADATA_UNION_T *pr_bData)
{
	unsigned char HB[3] = {0};
	unsigned char PB[EMP_SIZE_MAX] = {0};
	unsigned char i, pkthw_index;
	unsigned char first, last, pb_new, pb_end, dataset_tagm,
		dataset_tagl, dataset_lenm, dataset_lenl;
	unsigned char sequence_index = 0;
	unsigned int size = pr_bData->hdr10p_metadata_info
		.ui4_Hdr10PlusSize;
	unsigned char *meta =
		(unsigned char *)(unsigned long)
		pr_bData->hdr10p_metadata_info
		.ui4_Hdr10PlusAddr;
	struct PACKET_HW_T pkthw_list[GEN_PKT_HW_NUM] = {
		pkthw[GEN_PKT_HW1], pkthw[GEN_PKT_HW2],
		pkthw[GEN_PKT_HW3], pkthw[GEN_PKT_HW4],
		pkthw[GEN_PKT_HW5], pkthw[GEN_PKT_HW6],
		pkthw[GEN_PKT_HW7], pkthw[GEN_PKT_HW8],
		pkthw[GEN_PKT_HW9], pkthw[GEN_PKT_HW10],
		pkthw[GEN_PKT_HW11], pkthw[GEN_PKT_HW12],
		pkthw[GEN_PKT_HW13], pkthw[GEN_PKT_HW14],
		pkthw[GEN_PKT_HW15],
	};

	HDMI_HDR_LOG("meta=%p, size=%d\n", meta, size);
	/*tset_size[0-3]=78,56,34,12  */

	for (pkthw_index = 0; pkthw_index < GEN_PKT_HW_NUM; pkthw_index++)
		vHalEnablePacket(hdmi, pkthw_list[pkthw_index], false);
	vDynamicHdrEMP_HeaderInit(HB, &size, &sequence_index, &first, &last);

	if (first) {
		pb_new = 1;
		pb_end = 0;
		dataset_tagm = 0x00;
		dataset_tagl = 0x04;
		dataset_lenm = (unsigned char)((size >> 8) & 0xff);
		dataset_lenl = (unsigned char)(size & 0xff);
		PB[0] = (pb_new << 7) | (pb_end << 6) |
			(DYNAMIC_HDR_EMP_DS_TYPE << 4) |
			(DYNAMIC_HDR_EMP_AFR  << 3) |
			(DYNAMIC_HDR_EMP_VFR << 2) |
			(DYNAMIC_HDR_EMP_SYNC << 1);
		PB[1] = 0;
		PB[2] = DYNAMIC_HDR_EMP_ORGID;
		PB[3] = dataset_tagm;
		PB[4] = dataset_tagl;
		PB[5] = dataset_lenm;
		PB[6] = dataset_lenl;

		if (last)
			for (i = 0; i < size; i++)
				*(PB + 7 + i) = *(meta + i);
		else
			for (i = 0; i < EMP_SIZE_FIRST; i++)
				*(PB + 7 + i) = *(meta + i);

		pkthw_index = (sequence_index % GEN_PKT_HW_NUM);
		vHalSendPacket(hdmi, pkthw_list[pkthw_index], HB, PB);
		sequence_index++;
		if (last) {
			HDMI_HDR_LOG("Return in first\n");
			return;
		}
	}

	vDynamicHdrEMP_HeaderInit(HB, &size, &sequence_index, &first, &last);
	while (last == 0) {
		for (i = 0; i < EMP_SIZE_MAX; i++)
			*(PB + i) = (*(meta + ((sequence_index - 1) *
			EMP_SIZE_MAX) + EMP_SIZE_FIRST + i));
		pkthw_index = (sequence_index % GEN_PKT_HW_NUM);
		vHalSendPacket(hdmi, pkthw_list[pkthw_index], HB, PB);
		sequence_index++;
		vDynamicHdrEMP_HeaderInit(HB, &size,
			&sequence_index, &first, &last);
	};

	for (i = 0; i < EMP_SIZE_MAX; i++)
		*(PB + i) = 0;

	HDMI_HDR_LOG("%s(),%d ,left size=%d\n", __func__, __LINE__,
		(size - ((sequence_index - 1) * EMP_SIZE_MAX) -
		EMP_SIZE_FIRST));
	for (i = 0; i < (size - ((sequence_index - 1) *
		EMP_SIZE_MAX) - EMP_SIZE_FIRST); i++)
		*(PB + i) = (*(meta + ((sequence_index - 1) *
		EMP_SIZE_MAX) + EMP_SIZE_FIRST + i));
	pkthw_index = (sequence_index % GEN_PKT_HW_NUM);
	vHalSendPacket(hdmi, pkthw_list[pkthw_index], HB, PB);
	sequence_index = 0;
}


//can.zeng done-> void vHalSendDVVSIF()
void mtk_hdmi_hw_dv_vsif(
	struct mtk_hdmi *hdmi, bool fgEnable,
	bool fgLowLatency, bool fgDVSignal,
	bool fgBackltCtrlMdPresent, unsigned int u4EfftmaxPQ)
{
	unsigned char bData[6];
	unsigned char bHDR_CHSUM = 0;
	unsigned char i;

	mtk_hdmi_mask(hdmi, TOP_INFO_EN, 0, GEN5_EN | GEN5_EN_WR);
	mtk_hdmi_mask(hdmi, TOP_INFO_RPT, 0, GEN5_RPT_EN);

	if (fgEnable) {
		bData[0] = 0x46;
		bData[1] = 0xD0;
		bData[2] = 0x00;
		bData[3] = (fgDVSignal << 1) | fgLowLatency;
		bData[4] = (fgBackltCtrlMdPresent << 7) |
			((u4EfftmaxPQ >> 8) & 0x0F);
		bData[5] = u4EfftmaxPQ & 0xFF;
		if (fgLowLatency)
			bData[3] |= 1;
		if (fgDVSignal)
			bData[3] |= (1 << 1);
		if (fgBackltCtrlMdPresent)
			bData[4] |= (1 << 7);

		mtk_hdmi_write(hdmi, TOP_GEN5_HEADER,
			(DVVSIF_LEN << 16) + (DVVSIF_VERS << 8) +
			(DVVSIF_TYPE << 0));
		bHDR_CHSUM = DVVSIF_LEN + DVVSIF_VERS + DVVSIF_TYPE;
		for (i = 0; i < 6; i++)
			bHDR_CHSUM += bData[i];
		bHDR_CHSUM = 0x100 - bHDR_CHSUM;
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT00,
			(bData[2] << 24) + (bData[1] << 16) +
			(bData[0] << 8) + (bHDR_CHSUM << 0));
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT01, (bData[5] << 16) +
			(bData[4] << 8) + (bData[3] << 0));
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT02, 0);
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT03, 0);
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT04, 0);
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT05, 0);
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT06, 0);
		mtk_hdmi_write(hdmi, TOP_GEN5_PKT07, 0);
		mtk_hdmi_mask(hdmi, TOP_INFO_RPT, GEN5_RPT_EN, GEN5_RPT_EN);
		mtk_hdmi_mask(hdmi, TOP_INFO_EN, GEN5_EN |
			GEN5_EN_WR, GEN5_EN | GEN5_EN_WR);
	}
}

void vHdr10PlusVSIF_ZeroPacket(void)
{
	char testbuf[27] = {0};
	struct VID_PLA_HDR_METADATA_INFO_T hdr_metadata;

	hdr_metadata.e_DynamicRangeType = VID_PLA_DR_TYPE_HDR10_PLUS_VSIF;
	hdr_metadata.metadata_info.hdr10_plus_metadata
		.hdr10p_metadata_info.ui4_Hdr10PlusSize =
		27;
	hdr_metadata.metadata_info.hdr10_plus_metadata
		.hdr10p_metadata_info.ui4_Hdr10PlusAddr =
		(unsigned long)(&testbuf);
	hdr_metadata.fgIsMetadata = 1;
	vVdpSetHdrMetadata(true, hdr_metadata);
}

void Hdr10OffImmediately(void)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_bHdrType == VID_PLA_DR_TYPE_HDR10 ||
		_bHdrType == VID_PLA_DR_TYPE_HDR10_PLUS_VSIF) {
		_bStaticHdrStatus = HDR_PACKET_DISABLE;
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus,
			_bHdrMetadataBuff);
		if (_bHdrType == VID_PLA_DR_TYPE_HDR10)
			_bHdrType = VID_PLA_DR_TYPE_SDR;
		HDMI_HDR_LOG("HDR10 off done\n");
	} else
		HDMI_HDR_LOG("HDR10 already off\n");
}

void Hdr10pVsifOffImmediately(void)
{
	struct PACKET_HW_T mpkthw = pkthw[DYNAMIC_HDR10P_VSIF_PKTHW];
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_bHdrType == VID_PLA_DR_TYPE_HDR10_PLUS_VSIF) {
		vHalEnablePacket(hdmi, mpkthw, false);
		if (_bHdrType == VID_PLA_DR_TYPE_HDR10_PLUS_VSIF)
			_bHdrType = VID_PLA_DR_TYPE_SDR;
		HDMI_HDR_LOG("Hdr10pVsif off done\n");
	} else
		HDMI_HDR_LOG("Hdr10pVsif already off\n");
}

void HdrDelayAllOffImmediately(void)
{
	Hdr10OffImmediately();
	Hdr10pVsifOffImmediately();
}


static void hdmi_hdr_mutex_init(struct mutex *lock)
{
	mutex_init(lock);
}

static void hdmi_hdr_mutex_lock(struct mutex *lock)
{
	if (mutex_is_locked(lock)) {
		HDMI_HDR_LOG("ERROR: HDR Lock has been locked, return\n");
		return;
	}
	mutex_lock(lock);
}

static void hdmi_hdr_mutex_unlock(struct mutex *lock)
{
	if (!mutex_is_locked(lock)) {
		HDMI_HDR_LOG("ERROR: HDR Lock not locked, return\n");
		return;
	}
	mutex_unlock(lock);
}

void vSetStaticHdrType(enum VID_GAMMA_T bType)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	if (_bHdrMetadataBuff == NULL)
		return;

	if (bType == GAMMA_HLG) {
		_bStaticHdrType = GAMMA_HLG;
		*_bHdrMetadataBuff = 0x03;	/* EOTF */
	} else {
		_bStaticHdrType = GAMMA_ST2084;
		*_bHdrMetadataBuff = 0x02;	/* EOTF */
	}

	HDMI_HDR_LOG("bType = %d, _bStaticHdrType =%d\n", bType, _bStaticHdrType);

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);

}
EXPORT_SYMBOL(vSetStaticHdrType);

void vVdpSetHdrMetadata(bool enable,
	struct VID_PLA_HDR_METADATA_INFO_T hdr_metadata)
{
	struct HDMI_STATIC_METADATA_INFO_T _bStaticHdrMetadata;
	bool fgMetadataSame = false;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_u4HdrDebugDisableType & HDR_DEBUG_DISABLE_METADATA)
		return;

	if (_bHdrMetadataBuff == NULL) {
		HDMI_HDR_LOG("ERROR:MetadataBuff is NULL\n");
		return;
	}

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG("set HDR metadata Type=%d\n",
		hdr_metadata.e_DynamicRangeType);

	if (hdr_metadata.e_DynamicRangeType == VID_PLA_DR_TYPE_HDR10) {
		if (_bStaticHdrType == GAMMA_HLG)
			_bStaticHdrMetadata.ui1_EOTF = 0x03;	/* HLG */
		else
			_bStaticHdrMetadata.ui1_EOTF = 0x02;	/* 2084 */

		_bStaticHdrMetadata.ui1_Static_Metadata_ID = 0;
		_bStaticHdrMetadata.ui2_DisplayPrimariesX0 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesX[0];
		_bStaticHdrMetadata.ui2_DisplayPrimariesY0 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesY[0];
		_bStaticHdrMetadata.ui2_DisplayPrimariesX1 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesX[1];
		_bStaticHdrMetadata.ui2_DisplayPrimariesY1 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesY[1];
		_bStaticHdrMetadata.ui2_DisplayPrimariesX2 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesX[2];
		_bStaticHdrMetadata.ui2_DisplayPrimariesY2 =
		    hdr_metadata.metadata_info.hdr10_metadata
		    .ui2_DisplayPrimariesY[2];
		_bStaticHdrMetadata.ui2_WhitePointX =
		    hdr_metadata.metadata_info.hdr10_metadata.ui2_WhitePointX;
		_bStaticHdrMetadata.ui2_WhitePointY =
		    hdr_metadata.metadata_info.hdr10_metadata.ui2_WhitePointY;
		_bStaticHdrMetadata.ui2_MaxDisplayMasteringLuminance =
		    hdr_metadata
		    .metadata_info
		    .hdr10_metadata.ui2_MaxDisplayMasteringLuminance;
		_bStaticHdrMetadata.ui2_MinDisplayMasteringLuminance =
		    hdr_metadata
		    .metadata_info
		    .hdr10_metadata.ui2_MinDisplayMasteringLuminance;
		_bStaticHdrMetadata.ui2_MaxCLL =
		    hdr_metadata.metadata_info.hdr10_metadata.ui2_MaxCLL;
		_bStaticHdrMetadata.ui2_MaxFALL =
		    hdr_metadata.metadata_info.hdr10_metadata.ui2_MaxFALL;

		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesX0=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesX0);
		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesY0=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesY0);
		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesX1=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesX1);
		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesY1=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesY1);
		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesX2=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesX2);
		HDMI_HDR_LOG(" set HDR ui2_DisplayPrimariesY2=0x%x\n",
			      _bStaticHdrMetadata.ui2_DisplayPrimariesX2);
		HDMI_HDR_LOG(" set HDR ui2_WhitePointX=0x%x\n",
			      _bStaticHdrMetadata.ui2_WhitePointX);
		HDMI_HDR_LOG(" set HDR ui2_WhitePointY=0x%x\n",
			      _bStaticHdrMetadata.ui2_WhitePointY);
		HDMI_HDR_LOG(" set HDR ui2_MaxDisplayMasteringLuminance=0x%x\n",
			      _bStaticHdrMetadata
			      .ui2_MaxDisplayMasteringLuminance);
		HDMI_HDR_LOG(" set HDR ui2_MinDisplayMasteringLuminance=0x%x\n",
			      _bStaticHdrMetadata
			      .ui2_MinDisplayMasteringLuminance);
		HDMI_HDR_LOG(" set HDR ui2_MaxCLL=0x%x\n",
			      _bStaticHdrMetadata.ui2_MaxCLL);
		memcpy(_bHdrMetadataBuff, &_bStaticHdrMetadata,
			sizeof(struct HDMI_STATIC_METADATA_INFO_T));
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus, _bHdrMetadataBuff);
	} else if (hdr_metadata.e_DynamicRangeType == VID_PLA_DR_TYPE_DOVI_LOWLATENCY) {
		if ((_fgBackltCtrlMDPresent ==
		hdr_metadata.metadata_info.dovi_lowlatency_metadata.fgBackltCtrlMdPresent)
		&& (_u4EffTmaxPQ ==
		hdr_metadata.metadata_info.dovi_lowlatency_metadata.ui4_EffTmaxPQ)) {
			fgMetadataSame = true;
		} else {
			_fgBackltCtrlMDPresent  =
			 hdr_metadata.metadata_info.dovi_lowlatency_metadata.fgBackltCtrlMdPresent;
			if (_fgBackltCtrlMDPresent)
				_u4EffTmaxPQ =
				hdr_metadata.metadata_info.dovi_lowlatency_metadata.ui4_EffTmaxPQ;
			else
				_u4EffTmaxPQ = 0;
			HDMI_HDR_LOG(" %d, _u4EffTmaxPQ =0x%x\n ",
				_fgBackltCtrlMDPresent, _u4EffTmaxPQ);
		}
		if ((!fgMetadataSame) && _fgLowLatencyDVEnable)
			mtk_hdmi_hw_dv_vsif(hdmi, true, true, true,
			_fgBackltCtrlMDPresent, _u4EffTmaxPQ);
	} else if (hdr_metadata.e_DynamicRangeType == VID_PLA_DR_TYPE_HDR10_PLUS) {
		HDMI_HDR_LOG("HDR10+\n");
		mtk_hdmi_hw_dynamic_hdr_emp(hdmi, enable,
			&(hdr_metadata.metadata_info.hdr10_plus_metadata));
	} else if (hdr_metadata.e_DynamicRangeType == VID_PLA_DR_TYPE_HDR10_PLUS_VSIF) {
		HDMI_HDR_LOG("HDR10+ VSIF\n");
		mtk_hdmi_hw_samsung_hdr10p_vsif(hdmi, enable,
			&(hdr_metadata.metadata_info.hdr10_plus_metadata));
	}

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vVdpSetHdrMetadata);

//can.zeng modification done->void vHDMIVideoOutput(char ui1ColorSpace)
void mtk_hdmi_colorspace_setting(struct mtk_hdmi *hdmi)
{
	unsigned int video_mute;

	video_mute = mtk_hdmi_read(hdmi, TOP_VMUTE_CFG1) & REG_VMUTE_EN;
	/* keep the video mute bit to avoid screen flicker */

	if ((hdmi->csp == HDMI_COLORSPACE_YUV444) ||
		(hdmi->csp == HDMI_COLORSPACE_YUV420)) {
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG1, 0x8000 | video_mute);
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG2, 0x80001000);
	} else if (hdmi->csp == HDMI_COLORSPACE_YUV422) {
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG1, 0x8000 | video_mute);
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG2, 0x00001000);
	} else {
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG1, 0x1000 | video_mute);
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG2, 0x10001000);
	}

	if (_fgLowLatencyDVEnable) {
		/* in current solution, when low latency DV,
		 * ETHDR only send colorspace format ycbcr422
		 */
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG1, 0x8000 | video_mute);
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG2, 0x00001000);
	}
	if (_fgDVHdrEnable) {
		/* DV case: not mute video to avoid destroy metadata */
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG1, 0x8000);
		mtk_hdmi_write(hdmi, TOP_VMUTE_CFG2, 0x80001000);
	}
}

void vBT2020Enable(bool fgEnable)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_u4HdrDebugDisableType & HDR_DEBUG_DISABLE_BT2020)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG(" _fgBT2020Enable =%d\n", fgEnable);
	_fgBT2020Enable = fgEnable;
	//can.zeng todo verify how to process avi infoframe
	mtk_hdmi_setup_avi_infoframe(hdmi, &hdmi->mode);
	mtk_hdmi_colorspace_setting(hdmi);

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vBT2020Enable);

//can.zeng modification done->void vHdrEnable(bool fgEnable)
void vHdr10Enable(bool fgEnable)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_u4HdrDebugDisableType & HDR_DEBUG_DISABLE_HDR)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG("fgEnable = %d, _bStaticHdrStatus =%d,  _bHdrType=%d\n",
		fgEnable, _bStaticHdrStatus, _bHdrType);

	if (fgEnable) {
		HdrDelayAllOffImmediately();
		_bStaticHdrStatus = HDR_PACKET_ACTIVE;
		mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus, _bHdrMetadataBuff);
		_bHdrType = VID_PLA_DR_TYPE_HDR10;
	} else {
		if (_bStaticHdrStatus == HDR_PACKET_ACTIVE) {
			_bStaticHdrStatus = HDR_PACKET_ZERO;
			mtk_hdmi_hw_static_hdr_infoframe(hdmi, _bStaticHdrStatus,
				_bHdrMetadataBuff);
			//vSetHdr10TimeDelayOff(2000);
			/* per HDMI specification: shall send HDR10 zero packet
			 * for two 2seconds then stop HDR10 packet
			 */
			queue_delayed_work(hdmi->hdmi_wq, &hdmi->hdr10_delay_work,
				msecs_to_jiffies(2000));
			HDMI_HDR_LOG("HDR10 SET TIMER\n");
		}
	}

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vHdr10Enable);

void vHdr10PlusEnable(bool fgEnable)
{
	unsigned char pkthw_index;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG("fgEnable=%d\n", fgEnable);
	if (fgEnable) {
		HdrDelayAllOffImmediately();
		for (pkthw_index = 0; pkthw_index < GEN_PKT_HW_NUM; pkthw_index++)
			vHalEnablePacket(hdmi, pkthw[pkthw_index], true);
		_bHdrType = VID_PLA_DR_TYPE_HDR10_PLUS;
	} else {
		for (pkthw_index = 0; pkthw_index < GEN_PKT_HW_NUM; pkthw_index++)
			vHalEnablePacket(hdmi, pkthw[pkthw_index], false);
		_bHdrType = VID_PLA_DR_TYPE_SDR;
	}

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vHdr10PlusEnable);

void vHdr10PlusVSIFEnable(bool fgEnable)
{
	struct PACKET_HW_T mpkthw = pkthw[DYNAMIC_HDR10P_VSIF_PKTHW];
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG("fgEnable=%d\n", fgEnable);

	if (fgEnable) {
		HdrDelayAllOffImmediately();
		vHalEnablePacket(hdmi, mpkthw, true);
		_bHdrType = VID_PLA_DR_TYPE_HDR10_PLUS_VSIF;
	} else {
		vHdr10PlusVSIF_ZeroPacket();
		//vSetHdr10pVsifTimeDelayOff(2000);
		queue_delayed_work(hdmi->hdmi_wq, &hdmi->hdr10vsif_delay_work,
			msecs_to_jiffies(2000));
	}

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vHdr10PlusVSIFEnable);

void vDVHdrEnable(bool fgEnable, bool use_DV_vsif)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (_u4HdrDebugDisableType & HDR_DEBUG_DISABLE_DV_HDR)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	dovi_off_delay_needed = false;
	if ((_fgDVHdrEnable == true) && (fgEnable == false))
		dovi_off_delay_needed = true;

	HDMI_HDR_LOG("fgEnable=%d, use_DV_vsif=%d\n", fgEnable, use_DV_vsif);
	_fgDVHdrEnable = fgEnable;
	_use_DV_vsif = use_DV_vsif;

	if (fgEnable) {
#ifdef CANZENG_TODO_VERIFY
		//can.zeng todo, not link to TA temporarily
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa5551);
#endif
		HdrDelayAllOffImmediately();
		if (use_DV_vsif)
			mtk_hdmi_hw_dv_vsif(hdmi, true, false, true, 0, 0);
		else
			mtk_hdmi_setup_h14b_vsif(hdmi, &hdmi->mode);
		_bHdrType = VID_PLA_DR_TYPE_DOVI;
	} else {
#ifdef CANZENG_TODO_VERIFY
		vCaHDMIWriteHdcpCtrl(0x88880000, 0xaaaa5550);
#endif
		if (use_DV_vsif)
			mtk_hdmi_hw_dv_vsif(hdmi, true, 0, 0, 0, 0);
		else
			mtk_hdmi_setup_h14b_vsif(hdmi, &hdmi->mode);
		_bHdrType = VID_PLA_DR_TYPE_SDR;
	}
	mtk_hdmi_setup_avi_infoframe(hdmi, &hdmi->mode);
	mtk_hdmi_colorspace_setting(hdmi);

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vDVHdrEnable);

void vLowLatencyDVEnable(bool fgEnable)
{
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	if (fgEnable == _fgLowLatencyDVEnable)
		return;

	hdmi_hdr_mutex_lock(&hdmi->hdr_mutex);

	HDMI_HDR_LOG("fgEnable=%d\n", fgEnable);
	_fgLowLatencyDVEnable = fgEnable;

	if (fgEnable) {
		HdrDelayAllOffImmediately();
		vBT2020Enable(true);
		mtk_hdmi_hw_dv_vsif(hdmi, true, true, true,
			_fgBackltCtrlMDPresent, _u4EffTmaxPQ);
		//low latency DV use DVVSIF
		//mtk_hdmi_setup_h14b_vsif(hdmi, &hdmi->mode);
		_bHdrType = VID_PLA_DR_TYPE_DOVI_LOWLATENCY;
	} else {
		vBT2020Enable(false);
		mtk_hdmi_hw_dv_vsif(hdmi, true, 0, 0, 0, 0);
		//low latency DV use DVVSIF
		//mtk_hdmi_setup_h14b_vsif(hdmi, &hdmi->mode);
		_bHdrType = VID_PLA_DR_TYPE_SDR;
	}

	hdmi_hdr_mutex_unlock(&hdmi->hdr_mutex);
}
EXPORT_SYMBOL(vLowLatencyDVEnable);

void Hdr10DelayOffHandler(struct work_struct *data)
{
	HDMI_HDR_FUNC();
	if (_bStaticHdrStatus == HDR_PACKET_ZERO) {
		Hdr10OffImmediately();
		HDMI_HDR_LOG("[delay]HDR10 off done\n");
	} else
		HDMI_HDR_LOG("[delay]HDR10 already off\n");

}

void Hdr10pVsifDelayOffHandler(struct work_struct *data)
{
	HDMI_HDR_FUNC();
	Hdr10OffImmediately();
	Hdr10pVsifOffImmediately();
	HDMI_HDR_LOG("[delay]HDR10VSIF off done\n");
}

void vInitHdr(void)
{
	struct HDMI_STATIC_METADATA_INFO_T initbuf;
	struct mtk_hdmi *hdmi = global_mtk_hdmi;

	if (!hdmi)
		return;

	hdmi_hdr_mutex_init(&hdmi->hdr_mutex);

	_bHdrMetadataBuff = kmalloc(256, GFP_KERNEL);
	if (_bHdrMetadataBuff == NULL) {
		HDMI_HDR_LOG("[HDR] vInitHdrMetadata Buffer NULL\n");
		return;
	}

	initbuf.ui1_EOTF = 0x02;
	initbuf.ui1_Static_Metadata_ID = 0;
	initbuf.ui2_DisplayPrimariesX0 = 35400;
	initbuf.ui2_DisplayPrimariesY0 = 14600;
	initbuf.ui2_DisplayPrimariesX1 = 8500;
	initbuf.ui2_DisplayPrimariesY1 = 39850;
	initbuf.ui2_DisplayPrimariesX2 = 6550;
	initbuf.ui2_DisplayPrimariesY2 = 2300;
	initbuf.ui2_WhitePointX = 15635;
	initbuf.ui2_WhitePointY = 16450;
	initbuf.ui2_MaxDisplayMasteringLuminance = 1000;
	initbuf.ui2_MinDisplayMasteringLuminance = 50;
	initbuf.ui2_MaxCLL = 1000;
	initbuf.ui2_MaxFALL = 400;
	memcpy(_bHdrMetadataBuff, &initbuf,
		sizeof(struct HDMI_STATIC_METADATA_INFO_T));

	_u4HdrDebugDisableType = 0;

	_fgBT2020Enable = 0;
	_bHdrType = VID_PLA_DR_TYPE_SDR;

}

void reset_hdmi_hdr_status(void)
{
	_bHdrType = VID_PLA_DR_TYPE_SDR;
	_bStaticHdrType = GAMMA_ST2084;
	_fgLowLatencyDVEnable = false;
	_fgBackltCtrlMDPresent = false;
	_fgBT2020Enable = false;
	_fgDVHdrEnable = false;
	_use_DV_vsif = false;
	_bStaticHdrStatus = HDR_PACKET_DISABLE;
	memset(_bHdrMetadataBuff, 0,
		sizeof(struct HDMI_STATIC_METADATA_INFO_T));
}
