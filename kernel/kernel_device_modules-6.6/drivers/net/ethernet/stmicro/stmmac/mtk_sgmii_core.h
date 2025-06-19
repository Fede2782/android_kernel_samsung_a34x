/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/bitops.h>

/* SGMII subsystem config registers
 * Register to auto-negotiation restart
 */
#define SGMII_PCS_CONTROL_1		0x0

#define SGMII_PCS_SPEED_ABILITY		0x8

/* Register to control remote fault */
#define SGMII_SGMII_MODE		0x20
#define SGMII_RESERVED			0x24

/* Register to power up QPHY */
#define SGMII_QPHY_PWR_STATE_CTRL	0xe8

/* Register to select SGMII-0 for SNPS MAC */
#define SGMII_UTIF_CTRL			0xf4
#define SGMII_MAC_SEL			BIT(24)

#define SGMII_ANA_RG			0x128

#define SGMII_PHYSPEED_AN		BIT(31)
#define SGMII_PHYSPEED_MASK		GENMASK(2, 0)
#define SGMII_PHYSPEED_1000		BIT(0)
#define SGMII_PHYSPEED_2500		BIT(1)

/* struct mtk_sgmii -  This is the structure holding sgmii regmap and its
 *                     characteristics
 * @regmap:            The register map pointing at the range used to setup
 *                     SGMII modes
 * @regmap_phy:        The register map pointing at the range used to setup
 *                     SGMII phy
 * @flags:             The enum refers to which mode the sgmii wants to run on
 */

struct mtk_sgmii {
	struct regmap   *regmap;
	struct regmap   *regmap_phy;
	u32             flags;
};

int mediatek_sgmii_setup_mode_an(struct mtk_sgmii *ss);
int mediatek_sgmii_setup_mode_force(struct mtk_sgmii *ss);
int mediatek_sgmii_path_setup(struct mtk_sgmii *ss);
int mediatek_sgmii_polling_link_status(struct mtk_sgmii *ss);
int mediatek_sgmii_init(struct mtk_sgmii *ss, struct device_node *r, struct device *dev);
