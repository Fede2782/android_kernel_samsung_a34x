/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_DDC_H
#define _MTK_DDC_H

#include <linux/clk.h>
#include <linux/i2c.h>

struct mtk_hdmi_ddc {
	struct i2c_adapter adap;
	struct clk *clk;
	void __iomem *regs;
};


#define SIF1_CLOK 288		/* 26M/(v) = 100Khz */
#define DDC2_CLOK 572		/* BIM=208M/(v*4) = 90Khz */
#define DDC2_CLOK_EDID 832	/* BIM=208M/(v*4) = 62.5Khz */
/* for HF1-55 */

unsigned char fgDDCDataRead(struct mtk_hdmi_ddc *ddc,
	unsigned char bDevice, unsigned char bData_Addr,
	unsigned char bDataCount, unsigned char *prData);
unsigned char fgDDCDataWrite(struct mtk_hdmi_ddc *ddc,
	unsigned char bDevice, unsigned char bData_Addr,
	unsigned char bDataCount, unsigned char *prData);
bool hdmi_ddc_request(struct mtk_hdmi_ddc *ddc, unsigned char req);
void hdmi_ddc_free(struct mtk_hdmi_ddc *ddc, unsigned char req);


#endif /* _MTK_DDC_H */
