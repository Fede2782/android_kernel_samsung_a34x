// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/regmap.h>

#include "mtk_sgmii_core.h"

void mediatek_sgmii_phy_gen1(struct mtk_sgmii *ss)
{
	regmap_write(ss->regmap_phy, 0x0120, 0x80000102);
	regmap_write(ss->regmap_phy, 0x0074, 0x00000080);
	regmap_write(ss->regmap_phy, 0x9008, 0x00000006);
	regmap_write(ss->regmap_phy, 0x90C0, 0x00000314);
	regmap_write(ss->regmap_phy, 0x90CC, 0x442901A0);
	regmap_write(ss->regmap_phy, 0x9010, 0x00001210);
	regmap_write(ss->regmap_phy, 0x90D0, 0x00001124);
	regmap_write(ss->regmap_phy, 0x90D4, 0x94130481);
	regmap_write(ss->regmap_phy, 0x106C, 0x00010000);
	regmap_write(ss->regmap_phy, 0x1038, 0x06060606);
	regmap_write(ss->regmap_phy, 0x107C, 0x000000A6);
	regmap_write(ss->regmap_phy, 0x100C, 0x0000FF00);
	regmap_write(ss->regmap_phy, 0x1098, 0x5089D89E);
	regmap_write(ss->regmap_phy, 0x109C, 0x00000001);
	regmap_write(ss->regmap_phy, 0x10E0, 0x00000369);
	regmap_write(ss->regmap_phy, 0x10E8, 0x00000351);
	regmap_write(ss->regmap_phy, 0x1078, 0x0FF0A0A2);
	regmap_write(ss->regmap_phy, 0x1084, 0x0049248A);
	regmap_write(ss->regmap_phy, 0x1080, 0x00659619);
	regmap_write(ss->regmap_phy, 0x20D0, 0xAA555504);
	regmap_write(ss->regmap_phy, 0x2004, 0xAAA6000F);
	regmap_write(ss->regmap_phy, 0x2040, 0x00020003);
	regmap_write(ss->regmap_phy, 0xA038, 0x0F048923);
	regmap_write(ss->regmap_phy, 0xA030, 0x0099D0C1);
	regmap_write(ss->regmap_phy, 0x30DC, 0x0000AAA1);
	regmap_write(ss->regmap_phy, 0x30E4, 0x0294A55A);
	regmap_write(ss->regmap_phy, 0x50F0, 0x00C00DAD);
	regmap_write(ss->regmap_phy, 0x6050, 0xF5FA2AA1);
	regmap_write(ss->regmap_phy, 0x6074, 0x3CF34F00);
	regmap_write(ss->regmap_phy, 0x6084, 0x00000E00);
	regmap_write(ss->regmap_phy, 0x605C, 0xE109B760);
	regmap_write(ss->regmap_phy, 0x6044, 0x78580524);
	regmap_write(ss->regmap_phy, 0x604C, 0x00550455);
	regmap_write(ss->regmap_phy, 0x6078, 0x006F56CA);
	regmap_write(ss->regmap_phy, 0x6040, 0x2828050A);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x00F8, 0x00201F21);
	regmap_write(ss->regmap_phy, 0x0030, 0x00050C00);
	regmap_write(ss->regmap_phy, 0x3040, 0x20000000);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200E800);
	usleep_range(155, 156);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200C111);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200C101);
	usleep_range(10, 11);
	regmap_write(ss->regmap_phy, 0x0070, 0x0201C111);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x0070, 0x0201C101);
	udelay(2);
	regmap_write(ss->regmap_phy, 0x90F0, 0x0000000B);
	regmap_write(ss->regmap_phy, 0x2050, 0x02000000);
	regmap_write(ss->regmap_phy, 0x205C, 0x00080000);
	regmap_write(ss->regmap_phy, 0x1050, 0x02000000);
	regmap_write(ss->regmap_phy, 0x3060, 0x03000000);
	usleep_range(10, 11);
	regmap_write(ss->regmap_phy, 0x1050, 0x00000000);
	usleep_range(10, 11);
	regmap_write(ss->regmap_phy, 0x2050, 0x00000000);
	usleep_range(512, 513);
	regmap_write(ss->regmap_phy, 0x205C, 0x00000000);
	usleep_range(50, 51);
	regmap_write(ss->regmap_phy, 0x00F8, 0x00201F01);
	regmap_write(ss->regmap_phy, 0x3040, 0x30000000);
	usleep_range(75, 76);
	regmap_write(ss->regmap_phy, 0x3060, 0x00000000);
	usleep_range(401, 402);
}

void mediatek_sgmii_phy_gen2(struct mtk_sgmii *ss)
{
	regmap_write(ss->regmap_phy, 0x0120, 0x80000102);
	regmap_write(ss->regmap_phy, 0x0074, 0x00000080);
	regmap_write(ss->regmap_phy, 0x9008, 0x00000006);
	regmap_write(ss->regmap_phy, 0x90C0, 0x00000314);
	regmap_write(ss->regmap_phy, 0x90CC, 0x442901A0);
	regmap_write(ss->regmap_phy, 0x9010, 0x00001210);
	regmap_write(ss->regmap_phy, 0x90D0, 0x00001124);
	regmap_write(ss->regmap_phy, 0x90D4, 0x94130481);
	regmap_write(ss->regmap_phy, 0x106C, 0x00010000);
	regmap_write(ss->regmap_phy, 0x1038, 0x06060606);
	regmap_write(ss->regmap_phy, 0x107C, 0x000000A6);
	regmap_write(ss->regmap_phy, 0x100C, 0x0000FF00);
	regmap_write(ss->regmap_phy, 0x1098, 0x6893B13B);
	regmap_write(ss->regmap_phy, 0x109C, 0x00000001);
	regmap_write(ss->regmap_phy, 0x10E0, 0x00000369);
	regmap_write(ss->regmap_phy, 0x10E8, 0x00000351);
	regmap_write(ss->regmap_phy, 0x1078, 0x0FF0A4A6);
	regmap_write(ss->regmap_phy, 0x1084, 0x0049248A);
	regmap_write(ss->regmap_phy, 0x1080, 0x00659619);
	regmap_write(ss->regmap_phy, 0x20D0, 0xAA555500);
	regmap_write(ss->regmap_phy, 0xA038, 0x0F048923);
	regmap_write(ss->regmap_phy, 0xA030, 0x0089D0C1);
	regmap_write(ss->regmap_phy, 0x30DC, 0x0000AAA1);
	regmap_write(ss->regmap_phy, 0x30E4, 0x0294A552);
	regmap_write(ss->regmap_phy, 0x30E0, 0x99885155);
	regmap_write(ss->regmap_phy, 0x50F0, 0x00C00DBD);
	regmap_write(ss->regmap_phy, 0x6050, 0xF5FA2AA1);
	regmap_write(ss->regmap_phy, 0x6084, 0x00000E00);
	regmap_write(ss->regmap_phy, 0x605C, 0xE505B760);
	regmap_write(ss->regmap_phy, 0x6044, 0x78580A24);
	regmap_write(ss->regmap_phy, 0x604C, 0x00550455);
	regmap_write(ss->regmap_phy, 0x6078, 0x006ED6D2);
	regmap_write(ss->regmap_phy, 0x6070, 0x0000F4F0);
	regmap_write(ss->regmap_phy, 0x6040, 0x28280A0A);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x00F8, 0x00201F21);
	regmap_write(ss->regmap_phy, 0x0030, 0x00050C00);
	regmap_write(ss->regmap_phy, 0x3040, 0x20000000);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200E800);
	usleep_range(155, 156);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200C111);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x0070, 0x0200C101);
	usleep_range(10, 11);
	regmap_write(ss->regmap_phy, 0x0070, 0x0201C111);
	udelay(1);
	regmap_write(ss->regmap_phy, 0x0070, 0x0201C101);
	udelay(2);
	regmap_write(ss->regmap_phy, 0x90F0, 0x00000007);
	regmap_write(ss->regmap_phy, 0x2050, 0x02000000);
	regmap_write(ss->regmap_phy, 0x205C, 0x00080000);
	regmap_write(ss->regmap_phy, 0x1050, 0x02000000);
	regmap_write(ss->regmap_phy, 0x3060, 0x03000000);
	usleep_range(10, 11);
	regmap_write(ss->regmap_phy, 0x1050, 0x00000000);
	usleep_range(50, 51);
	regmap_write(ss->regmap_phy, 0x00F8, 0x00201F01);
	regmap_write(ss->regmap_phy, 0x3040, 0x30000000);
	usleep_range(75, 76);
	regmap_write(ss->regmap_phy, 0x3060, 0x00000000);
	usleep_range(401, 402);
}

int mediatek_sgmii_setup_mode_an(struct mtk_sgmii *ss)
{
	int ret = 0;

	if (!ss->regmap)
		return -EINVAL;

	ret = mediatek_sgmii_setup_mode_force(ss);
	if (ret)
		return ret;

	/* Enable SGMII AN */
	regmap_update_bits(ss->regmap,SGMII_PCS_CONTROL_1 , GENMASK(31,0), 0x41140);

	/* Disable SGMII Fprce mode setting */
	regmap_update_bits(ss->regmap, SGMII_SGMII_MODE, GENMASK(31,0), 0x3112010B);

	/* Restart SGMII AN */
	regmap_update_bits(ss->regmap,SGMII_PCS_CONTROL_1 , GENMASK(31,0), 0x41340);

	return 0;
}

int mediatek_sgmii_setup_mode_force(struct mtk_sgmii *ss)
{
	unsigned int val = 0;
	int mode;

	if (!ss->regmap)
		return -EINVAL;

	udelay(2);

	/* Enable SGMII force mode setting */
	regmap_write(ss->regmap, SGMII_SGMII_MODE, 0x31120009);

	/* Disable SGMII AN */
	regmap_write(ss->regmap, SGMII_PCS_CONTROL_1, 0x01000140);

	regmap_write(ss->regmap, SGMII_RESERVED, 0x001000FA);

	/* Select PHY speed */
	mode = ss->flags & SGMII_PHYSPEED_MASK;
	if (mode == SGMII_PHYSPEED_2500)
		regmap_write(ss->regmap, SGMII_ANA_RG, 0x00014017);
	else
		regmap_write(ss->regmap, SGMII_ANA_RG, 0x00014013);

	/* Switch SGMII to SNPS MAC */
	regmap_read(ss->regmap, SGMII_UTIF_CTRL, &val);
	val |= SGMII_MAC_SEL;
	regmap_write(ss->regmap, SGMII_UTIF_CTRL, val);

	/* Release PHYA power down state */
	regmap_write(ss->regmap, SGMII_QPHY_PWR_STATE_CTRL, 0x00000000);

	usleep_range(161, 162);

	/* Set SGMII PHY parameter */
	if(mode == SGMII_PHYSPEED_2500)
		mediatek_sgmii_phy_gen2(ss);
	else
		mediatek_sgmii_phy_gen1(ss);

	return 0;
}

int mediatek_sgmii_path_setup(struct mtk_sgmii *ss)
{
	int err;

	/* Setup SGMIISYS with the determined property */
	if ((ss->flags & SGMII_PHYSPEED_AN) == SGMII_PHYSPEED_AN)
		err = mediatek_sgmii_setup_mode_an(ss);
	else
		err = mediatek_sgmii_setup_mode_force(ss);

	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL_GPL(mediatek_sgmii_path_setup);

int mediatek_sgmii_init(struct mtk_sgmii *ss, struct device_node *r, struct device *dev)
{
	struct device_node *np, *np_phy;
	const char *str;
	int err;

	np = of_parse_phandle(r, "mediatek,sgmiisys", 0);
	if (!np)
		return -ENOMEM;

	ss->regmap = syscon_node_to_regmap(np);
	if (IS_ERR(ss->regmap))
		return PTR_ERR(ss->regmap);

	err = of_property_read_string(np, "mediatek,physpeed", &str);
	if (err)
		return err;

	if (!strcmp(str, "2500"))
		ss->flags |= SGMII_PHYSPEED_2500;
	else if (!strcmp(str, "1000"))
		ss->flags |= SGMII_PHYSPEED_1000;
	else if (!strcmp(str, "auto"))
		ss->flags |= SGMII_PHYSPEED_AN;
	else
		return -EINVAL;

	/* SGMII PHY init */
	np_phy = of_parse_phandle(r, "mediatek,sgmiisys-phy", 0);
	if (!np_phy)
		return -ENOMEM;

	ss->regmap_phy = syscon_node_to_regmap(np_phy);
	if (IS_ERR(ss->regmap_phy))
		return PTR_ERR(ss->regmap_phy);

	return 0;
}
EXPORT_SYMBOL_GPL(mediatek_sgmii_init);

int mediatek_sgmii_polling_link_status(struct mtk_sgmii *ss)
{
	unsigned int val = 0;

	if (!ss->regmap)
		return -EINVAL;

	if ((ss->flags & SGMII_PHYSPEED_2500) == SGMII_PHYSPEED_2500) {
		regmap_read(ss->regmap, SGMII_PCS_CONTROL_1, &val);
		if (val & BIT(18))
			return 0;
	} else {
		regmap_read(ss->regmap, SGMII_PCS_SPEED_ABILITY, &val);
		if (val & BIT(31))
			return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mediatek_sgmii_polling_link_status);
