// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Richtek Technology Corp.
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: ChiYuan Huang <cy_huang@richtek.com>
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>

#include "ufcs_class.h"

#define MT6379_REG_IRQ_IND		0x0B
#define MT6379_REG_SPMI_TXDRV2		0x2B
#define MT6379_REG_UFCS_CTRL1		0x642
#define MT6379_REG_UFCS_CTRL2		0x643
#define MT6379_REG_UFCS_FLAG1		0x644
#define MT6379_REG_UFCS_MASK1		0x646
#define MT6379_REG_SRAM_CONTROL		0x649
#define MT6379_REG_TX_LENGTH		0x680
#define MT6379_REG_TX_BUFFER0		0x681
#define MT6379_REG_UFCS_RX_LENGTH	0x6C0
#define MT6379_REG_UFCS_RX_BUFFER0	0x6C1

#define MT6379_INDM_UFCS		BIT(0)
#define MT6379_RCS_INT_DONE_MASK	BIT(0)
#define MT6379_EN_PROTOCOL_MASK		GENMASK(7, 6)
#define MT6379_UFCS_PROTOCOL		2
#define MT6379_EN_HANDSHAKE_MASK	BIT(5)
#define MT6379_BAUDRATE_MASK		GENMASK(4, 3)
#define MT6379_SNDCMD_MASK		BIT(2)
#define MT6379_SNDHRST_MASK		GENMASK(1, 0)
#define MT6379_DMHZ_MASK		BIT(0)

/* MT6379_REG_UFCS_FLAG1: 0x644 */
#define MT6379_EVT_UFCS_ACK_TIMEOUT	BIT(0)
#define MT6379_EVT_UFCS_HARD_RESET	BIT(1)
#define MT6379_EVT_UFCS_DATA_READY	BIT(2)
#define MT6379_EVT_UFCS_SNDCMD		BIT(3)
#define MT6379_EVT_UFCS_CRC_ERROR	BIT(4)
#define MT6379_EVT_UFCS_BAUDRATE_ERROR	BIT(5)
#define MT6379_EVT_UFCS_HS_SUCCESS	BIT(6)
#define MT6379_EVT_UFCS_HS_FAIL		BIT(7)
/* MT6379_REG_UFCS_FLAG2: 0x645 */
#define MT6379_EVT_UFCS_TBYTE_ERROR	BIT(8)
#define MT6379_EVT_UFCS_MSG_XFER_FAIL	BIT(9)
#define MT6379_EVT_UFCS_RXBUF_BUSY	BIT(10)
#define MT6379_EVT_UFCS_FR_TIMEOUT	BIT(11)
#define MT6379_EVT_UFCS_BAUDRATE_CHANGE	BIT(12)
#define MT6379_EVT_UFCS_RXLEN_ERROR	BIT(13)
#define MT6379_EVT_UFCS_RXBUF_OVERFLOW	BIT(14)

#define UFCS_IRQ_NUMS			2

#define MT6379_UFCS_RX_DATA_NO_READ	BIT(6)
#define MT6379_SRAM_DATA_UPDATE_MASK	BIT(5)
#define UFCS_MAX_RXBUFF_SIZE		63
#define MT6379_DEV_ADDRID_MASK		BIT(1)

/*
 * A data packet includes multiple data frames.
 * The IDLE state between Tx sending data packets
 * needs to be greater than or equal to 2ms
 * mt6379 send ack after data ready time is
 * bud 115200 -> 0.59ms
 * bud 57600  -> 1.16ms
 * bud 38400  -> 1.72ms
 * delay 4ms for stable, 1.72 + 2
 */
#define UFCS_MSG_TRANS_DELAY           4000 /* us */

struct mt6379_data {
	struct device *dev;
	struct regmap *regmap;
	struct ufcs_dev ufcs;
	struct ufcs_port *port;
	ktime_t last_rx_time;
};

static int mt6379_ufcs_init(struct ufcs_dev *ufcs)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	u16 unmask = 0;
	int ret;

	/* Unmask UFCS irqs */
	ret = regmap_raw_write(data->regmap, MT6379_REG_UFCS_MASK1, &unmask, sizeof(unmask));
	if (ret)
		return ret;

	/* Disable UFCS protocol by default */
	return regmap_write(data->regmap, MT6379_REG_UFCS_CTRL1, 0);
}

static int mt6379_ufcs_enable(struct ufcs_dev *ufcs, bool enable)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	struct regmap *regmap = data->regmap;
	unsigned int val = 0;
	int ret;

	if (enable) {
		val = FIELD_PREP(MT6379_EN_PROTOCOL_MASK, MT6379_UFCS_PROTOCOL);
		ret = regmap_write(regmap, MT6379_REG_UFCS_CTRL1, val);
		if (ret)
			return ret;

		val |= MT6379_EN_HANDSHAKE_MASK;
	}

	return regmap_write(regmap, MT6379_REG_UFCS_CTRL1, val);
}

static int mt6379_ufcs_config_baud_rate(struct ufcs_dev *ufcs, enum ufcs_baud_rate rate)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	unsigned int val;

	switch (rate) {
	case UFCS_BAUD_RATE_115200:
		val = 0;
		break;
	case UFCS_BAUD_RATE_57600:
		val = 1;
		break;
	case UFCS_BAUD_RATE_38400:
		val = 2;
		break;
	default:
		dev_err(data->dev, "Not supported rate %d\n", rate);
		return -EINVAL;
	}

	val = FIELD_PREP(MT6379_BAUDRATE_MASK, val);
	return regmap_update_bits(data->regmap, MT6379_REG_UFCS_CTRL1, MT6379_BAUDRATE_MASK, val);
}

static int mt6379_ufcs_transmit(struct ufcs_dev *ufcs, const struct ufcs_message *msg, u8 msglen)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	struct regmap *regmap = data->regmap;
	u16 header = be16_to_cpu(msg->header);
	ktime_t guarantee_time, now;
	bool to_emark = false;
	s64 wait_time = 0;
	int ret;

	guarantee_time = ktime_add_us(data->last_rx_time, UFCS_MSG_TRANS_DELAY);
	now = ktime_get();

	if (!ktime_after(now, guarantee_time)) {
		wait_time = ktime_us_delta(guarantee_time, now);
		usleep_range(wait_time, wait_time * 150 / 100);
		dev_dbg(data->dev, "%s delay = %lldus\n", __func__, wait_time);
	}

	to_emark = FIELD_GET(UFCS_HEADER_ROLE_MASK, header) == UFCS_EMARK;
	ret = regmap_update_bits(regmap, MT6379_REG_UFCS_CTRL2, MT6379_DEV_ADDRID_MASK,
				 to_emark ? 0xFF : 0);
	if (ret)
		return ret;

	/* Write the total message length */
	ret = regmap_write(regmap, MT6379_REG_TX_LENGTH, msglen);
	if (ret)
		return ret;

	/* Write the whole message body */
	ret = regmap_raw_write(regmap, MT6379_REG_TX_BUFFER0, msg, msglen);
	if (ret)
		return ret;

	/* When set, auto transmit tx buffer message. clear when done */
	return regmap_update_bits(regmap, MT6379_REG_UFCS_CTRL1, MT6379_SNDCMD_MASK,
				  MT6379_SNDCMD_MASK);
}

static int mt6379_ufcs_send_hard_reset(struct ufcs_dev *ufcs, enum ufcs_hard_reset_type type)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	unsigned int val;

	switch (type) {
	case UFCS_HARD_RESET_TO_SRC:
		val = 1;
		break;
	case UFCS_HARD_RESET_TO_CABLE:
		val = 2;
		break;
	default:
		dev_err(data->dev, "Unknown hardreset type %d\n", type);
		return -EINVAL;
	}

	val = FIELD_PREP(MT6379_SNDHRST_MASK, val);
	return regmap_update_bits(data->regmap, MT6379_REG_UFCS_CTRL1, MT6379_SNDHRST_MASK, val);
}

static int mt6379_ufcs_config_tx_hiz(struct ufcs_dev *ufcs, bool enable)
{
	struct mt6379_data *data = container_of(ufcs, struct mt6379_data, ufcs);
	unsigned int val = enable ? MT6379_DMHZ_MASK : 0;

	return regmap_update_bits(data->regmap, MT6379_REG_UFCS_CTRL2, MT6379_DMHZ_MASK, val);
}

static int mt6379_recover_rx_buffer(struct mt6379_data *udata, int last_ret)
{
	int ret;

	/* Recover to write SRAM data */
	ret = regmap_update_bits(udata->regmap, MT6379_REG_SRAM_CONTROL,
				 MT6379_SRAM_DATA_UPDATE_MASK, 0);
	if (ret) {
		dev_info(udata->dev, "%s, Failed to recover UFCS SRAM control(ret:%d)\n",
			 __func__, ret);
		last_ret = ret;
	}

	/* Clear buffer and wait next msg */
	ret = regmap_update_bits(udata->regmap, MT6379_REG_UFCS_CTRL2,
				 MT6379_UFCS_RX_DATA_NO_READ, 0);
	if (ret) {
		dev_info(udata->dev, "%s, Failed to clear UFCS RX data buffer(ret:%d)\n",
			 __func__, ret);
		last_ret = ret;
	}

	return last_ret ? last_ret : 0;
}

static int mt6379_get_message(struct mt6379_data *udata, struct ufcs_message *msg)
{
	int i, ret;
	u32 lens = 0, rdata;
	u8 buf[UFCS_MAX_RXBUFF_SIZE];

	/* Switch to read SRAM data */
	ret = regmap_update_bits(udata->regmap, MT6379_REG_SRAM_CONTROL,
				 MT6379_SRAM_DATA_UPDATE_MASK, 0xFF);
	if (ret) {
		dev_info(udata->dev, "%s, Failed to switch UFCS SRAM control function(ret:%d)\n",
			 __func__, ret);
		return ret;
	}

	ret = regmap_write(udata->regmap, MT6379_REG_UFCS_RX_LENGTH, 0);
	if (ret) {
		dev_err(udata->dev, "Failed to write rx length\n");
		return mt6379_recover_rx_buffer(udata, ret);
	}

	ret = regmap_read(udata->regmap, MT6379_REG_UFCS_RX_LENGTH, &lens);
	if (ret) {
		dev_err(udata->dev, "Failed to read rx length\n");
		return mt6379_recover_rx_buffer(udata, ret);
	}

	if (lens > UFCS_MAX_RXBUFF_SIZE) {
		dev_err(udata->dev, "lens(%d) out of range\n", lens);
		return mt6379_recover_rx_buffer(udata, -EINVAL);
	}

	for (i = 0; i < lens; i++) {
		ret = regmap_write(udata->regmap, MT6379_REG_UFCS_RX_BUFFER0 + i, 0);
		if (ret) {
			dev_info(udata->dev, "%s, Failed to write 0 to rx_buf[%d] (ret:%d)\n",
				 __func__, i, ret);
			return mt6379_recover_rx_buffer(udata, ret);
		}

		ret = regmap_read(udata->regmap, MT6379_REG_UFCS_RX_BUFFER0 + i, &rdata);
		if (ret) {
			dev_info(udata->dev, "%s, Failed to read rx_buf[%d] (ret:%d)\n",
				 __func__, i, ret);
			return mt6379_recover_rx_buffer(udata, ret);
		}

		*(buf + i) = rdata;
		dev_dbg(udata->dev, "%s: buf[%d]:0x%x\n", __func__, i, rdata);
	}

	memcpy(msg, buf, lens);

	return mt6379_recover_rx_buffer(udata, ret);
}

static irqreturn_t mt6379_ufcs_evt_handler(int irq, void *data)
{
	struct mt6379_data *udata = data;
	struct regmap *regmap = udata->regmap;
	struct ufcs_message msg;
	u16 events, unmask = 0, mask = 0xffff;
	int ret;

	/* Mask UFCS irqs */
	ret = regmap_raw_write(regmap, MT6379_REG_UFCS_MASK1, &mask, sizeof(mask));
	if (ret)
		goto out;

	ret = regmap_raw_read(regmap, MT6379_REG_UFCS_FLAG1, &events, sizeof(events));
	if (ret)
		goto out;

	if (!events)
		goto out;

	ret = regmap_raw_write(regmap, MT6379_REG_UFCS_FLAG1, &events, sizeof(events));
	if (ret)
		goto out;

	/* TODO: ignore others if recv hardreset? */
	if (events & MT6379_EVT_UFCS_HARD_RESET) {
		ufcs_hard_reset(udata->port);
		//goto out;
	}

	/* sendcmd and xfer fail flag will appear at the same time if tx fail */
	if (events & MT6379_EVT_UFCS_SNDCMD) {
		if (events & MT6379_EVT_UFCS_MSG_XFER_FAIL)
			ufcs_tx_complete(udata->port, UFCS_TX_FAIL);
		else
			ufcs_tx_complete(udata->port, UFCS_TX_SUCCESS);
	}

	if (events & MT6379_EVT_UFCS_DATA_READY) {
		udata->last_rx_time = ktime_get();
		ret = mt6379_get_message(udata, &msg);
		if (!ret)
			ufcs_rx_receive(udata->port, &msg);
		else
			dev_err(udata->dev, "Failed to get rx message\n");

	}

	if (events & MT6379_EVT_UFCS_HS_SUCCESS)
		ufcs_hand_shake_state(udata->port, true);

	if (events & MT6379_EVT_UFCS_HS_FAIL)
		ufcs_hand_shake_state(udata->port, false);

out:
	/* Unmask UFCS irqs */
	ret = regmap_raw_write(regmap, MT6379_REG_UFCS_MASK1, &unmask, sizeof(unmask));
	if (ret)
		dev_err(udata->dev, "Failed to unmask UFCS IRQ\n");

	/* Retrigger */
	ret = regmap_write(regmap, MT6379_REG_SPMI_TXDRV2, MT6379_RCS_INT_DONE_MASK);
	if (ret)
		dev_err(udata->dev, "Failed to do IRQ retrigger\n");

	return IRQ_HANDLED;
}

static int mt6379_ufcs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6379_data *data;
	int ret, irq;

	dev_info(dev, "%s, ++\n", __func__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to init regmap\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return dev_err_probe(dev, irq, "Failed to get ufcs irq\n");

	data->ufcs.init = mt6379_ufcs_init;
	data->ufcs.enable = mt6379_ufcs_enable;
	data->ufcs.config_baud_rate = mt6379_ufcs_config_baud_rate;
	data->ufcs.transmit = mt6379_ufcs_transmit;
	data->ufcs.send_hard_reset = mt6379_ufcs_send_hard_reset;
	data->ufcs.config_tx_hiz = mt6379_ufcs_config_tx_hiz;

	data->port = devm_ufcs_register_port(dev, &data->ufcs);
	if (IS_ERR(data->port))
		return dev_err_probe(dev, PTR_ERR(data->port),  "Failed to register port\n");

	ret = devm_request_threaded_irq(data->dev, irq, NULL, mt6379_ufcs_evt_handler,
				   IRQF_ONESHOT, dev_name(data->dev), data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to request irq\n");

	dev_pm_set_wake_irq(data->dev, irq);
	dev_info(dev, "%s: successfully\n", __func__);
	return 0;
}

static const struct of_device_id mt6379_ufcs_dev_match[] = {
	{ .compatible = "mediatek,mt6379-ufcs" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6379_ufcs_dev_match);

static struct platform_driver mt6379_ufcs_driver = {
	.driver = {
		.name = "mt6379-ufcs",
		.of_match_table = mt6379_ufcs_dev_match,
	},
	.probe = mt6379_ufcs_probe,
};
module_platform_driver(mt6379_ufcs_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6379 UFCS Interface Driver");
MODULE_LICENSE("GPL");
