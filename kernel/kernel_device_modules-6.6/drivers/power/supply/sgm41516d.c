// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/phy/phy.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "sgm41516d.h"
#include "mtk_gauge.h"


/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
//extern int hw_charging_get_charger_type(void);

void sgm41516d_force_dpdm_enable(struct sgm41516d_info *info, unsigned int val);
unsigned int sgm41516d_get_iidet_status(struct sgm41516d_info *info);
int g_tran_chg_info = 0;

/*SMG41516 start*/
enum chg_info {
	BQ25601D = 1,
	BQ25601,
	SGM41516,
	SGM41516D,
	SGM41516E,
	CHG_INFO_MAX,
};

enum cv_fine_val {
	CV_NORMAL=0,
	CV_POST_8MV,
	CV_NEG_8MV,
	CV_NEG_16MV,
	CV_MAX,
};

enum attach_type {
	ATTACH_TYPE_NONE,
	ATTACH_TYPE_PWR_RDY,
	ATTACH_TYPE_TYPEC,
	ATTACH_TYPE_PD,
	ATTACH_TYPE_PD_SDP,
	ATTACH_TYPE_PD_DCP,
	ATTACH_TYPE_PD_NONSTD,
};

enum usb_val {
	USB_HIZ=0,
	USB_0V,
	USB_0_6V,
	USB_3_3V,
	USB_VAL_MAX,
};

enum wdt_value {
	DISABLE_WDT=0,
	WDT_40S,
	WDT_80S,
	WDT_160S,
};

enum sgm41516d_ic_stat {
	SGM41516D_ICSTAT_DISABLE = 0,
	SGM41516D_ICSTAT_PRECHG,
	SGM41516D_ICSTAT_FASTCHG,
	SGM41516D_ICSTAT_CHGDONE,
	SGM41516D_ICSTAT_MAX,
};

enum sgm41516d_usbsw {
        SGM41516D_USBSW_CHG = 0,
        SGM41516D_USBSW_USB,
};
#define SGM41516D_PHY_MODE_BC11_SET 1
#define SGM41516D_PHY_MODE_BC11_CLR 2

const unsigned int VBAT_CV_FINE[] = {32000,8000};
/*SMG41516 end*/

enum sgm41516d_pmu_chg_type {
        SGM41516D_CHG_TYPE_NOVBUS = 0,
        SGM41516D_CHG_TYPE_SDP,
        SGM41516D_CHG_TYPE_CDP,
        SGM41516D_CHG_TYPE_DCP,
        SGM41516D_CHG_TYPE_UNKNOW=5,
        SGM41516D_CHG_TYPE_SDPNSTD,
        SGM41516D_CHG_TYPE_OTG,
        SGM41516D_CHG_TYPE_MAX,
};

//static enum charger_type bq_chg_type;
#define BQ_DET_COUNT_MAX 16
#define sgm41516d_MANUFACTURER "Transsion"

#define GETARRAYNUM(array) (ARRAY_SIZE(array))

const unsigned int VBAT_CV_VTH[] = {
	3856000, 3888000, 3920000, 3952000,
	3984000, 4016000, 4048000, 4080000,
	4112000, 4144000, 4176000, 4208000,
	4240000, 4272000, 4304000, 4336000,
	4368000, 4400000, 4432000, 4464000,
	4496000, 4528000, 4560000, 4592000,
	4624000

};

const unsigned int CS_VTH[] = {
	0, 6000, 12000, 18000, 24000,
	30000, 36000, 42000, 48000, 54000,
	60000, 66000, 72000, 78000, 84000,
	90000, 96000, 102000, 108000, 114000,
	120000, 126000, 132000, 138000, 144000,
	150000, 156000, 162000, 168000, 174000,
	180000, 186000, 192000, 198000, 204000,
	210000, 216000, 222000, 228000, 234000,
	240000, 246000, 252000, 258000, 264000,
	270000, 276000, 282000, 288000, 294000,
	300000, 306000, 312000, 318000, 324000,
	330000, 336000, 342000, 348000, 354000,
	360000
};

const unsigned int INPUT_CS_VTH[] = {
	10000, 20000, 30000, 40000,
	50000, 60000, 70000, 80000,
	90000, 100000, 110000, 120000,
	130000, 140000, 150000, 160000,
	170000, 180000, 190000, 200000,
	210000, 220000, 230000, 250000,
	260000, 270000, 280000, 290000,
	300000, 310000, 320000
};


const unsigned int VCDT_HV_VTH[] = {
	4200000, 4250000, 4300000, 4350000,
	4400000, 4450000, 4500000, 4550000,
	4600000, 6000000, 6500000, 7000000,
	7500000, 8500000, 9500000, 10500000

};


const unsigned int VINDPM_REG[] = {
	3900, 4000, 4100, 4200, 4300, 4400,
	4500, 4600, 4700, 4800, 4900, 5000,
	5100, 5200, 5300, 5400, 5500, 5600,
	5700, 5800, 5900, 6000, 6100, 6200,
	6300, 6400
};

/* SGM41516D REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 1200
};

static const u32 sgm41516d_otgcc[] = {
	500000, 1200000,1500000,2000000
};

DEFINE_MUTEX(g_input_current_mutex);
static const struct i2c_device_id sgm41516d_i2c_id[] = { {"sgm41516d", 0}, {} };
unsigned int sgm41516d_get_vbus_stat(struct sgm41516d_info *info);

static int sgm41516d_driver_probe(struct i2c_client *client);

unsigned int charging_value_to_parameter(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_info("Can't find the parameter\n");
	return parameter[0];

}

unsigned int charging_parameter_to_value(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
		unsigned int number,
		unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0;
		     i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
					pList[i], level, i);
				return pList[i];
			}
		}

		pr_info("Can't find closest level\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}

static unsigned int sgm41516d_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	if (target < min)
		return 0;

	if (target >= max)
		target = max;

	return (target - min) / step;
}

/**********************************************************
 *
 *   [I2C Function For Read/Write sgm41516d]
 *
 *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int sgm41516d_read_byte(struct sgm41516d_info *info, unsigned char cmd,
			       unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&info->sgm41516d_i2c_access);

	/* info->client->addr = ((info->client->addr) & I2C_MASK_FLAG) |
	 * I2C_WR_FLAG;
	 */
	info->client->ext_flag =
		((info->client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG |
		I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(info->client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* info->client->addr = info->client->addr & I2C_MASK_FLAG; */
		info->client->ext_flag = 0;
		mutex_unlock(&info->sgm41516d_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* info->client->addr = info->client->addr & I2C_MASK_FLAG; */
	info->client->ext_flag = 0;
	mutex_unlock(&info->sgm41516d_i2c_access);

	return 1;
}

unsigned int sgm41516d_write_byte(struct sgm41516d_info *info,  unsigned char cmd,
				unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&info->sgm41516d_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	info->client->ext_flag = ((info->client->ext_flag) & I2C_MASK_FLAG) |
			       I2C_DIRECTION_FLAG;

	ret = i2c_master_send(info->client, write_data, 2);
	if (ret < 0) {
		info->client->ext_flag = 0;
		mutex_unlock(&info->sgm41516d_i2c_access);
		return 0;
	}

	info->client->ext_flag = 0;
	mutex_unlock(&info->sgm41516d_i2c_access);
	return 1;
}
#else
unsigned int sgm41516d_read_byte(struct sgm41516d_info *info, unsigned char cmd,
			       unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&info->sgm41516d_i2c_access);

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = info->client->addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = info->client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(info->client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&info->sgm41516d_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int sgm41516d_write_byte(struct sgm41516d_info *info, unsigned char cmd,
				unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];

	mutex_lock(&info->sgm41516d_i2c_access);

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = info->client->addr,
				.flags = 0,
				.len = 1 + 1,
				.buf = buf,
			},
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(info->client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&info->sgm41516d_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif

#define VFY_NUM		(3)
int sgm41516d_read_byte_verify(struct sgm41516d_info *info, unsigned char RegNum,
			       unsigned char *val)
{
	bool verify_ok = false;
	int retry_cnt = 0;
	unsigned char reg_val[VFY_NUM];
	int i = 0;
	int ret = 0;

	while (++retry_cnt < 20 && !verify_ok) {
		for(i = 0;i < VFY_NUM; i++) {
			ret = sgm41516d_read_byte(info, RegNum, &reg_val[i]);
			if (ret < 0) {
				pr_info("[%s] read Reg[%x] failed ret = %d\n", __func__, RegNum, ret);
				return ret;
				}
			if (i > 0 && (reg_val[i] != reg_val[0]))
				break;
			if (i == (VFY_NUM -1))
				verify_ok = true;
		}
	}

	if (verify_ok == false)
		pr_info("verify i2c read error");

	pr_info("[%s] read Reg[%x]=0x%x, retry_cnt = %d\n", __func__, RegNum, reg_val[0], retry_cnt);

	if ((RegNum == sgm41516d_CON1) && (!info->sgm41516d_otg_enable_flag)) {
		if (reg_val[0] &0x20) {
			reg_val[0] = reg_val[0] & 0xdf;
			pr_info("calibrate otg status\n");
		}
	}	
	*val = reg_val[0];

	return ret;
}
/**********************************************************
 *
 *   [Read / Write Function]
 *
 *********************************************************/
unsigned int sgm41516d_read_interface(struct sgm41516d_info *info, unsigned char RegNum,
				    unsigned char *val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char sgm41516d_reg = 0;
	unsigned int ret = 0;

	mutex_lock(&info->sgm41516d_access_lock);

	ret = sgm41516d_read_byte_verify(info, RegNum, &sgm41516d_reg);
	if (ret < 0)
		pr_info("[%s] read Reg[%x] failed ret = %d\n", __func__, RegNum, ret);

	pr_info("[%s] Reg[%x]=0x%x\n", __func__, RegNum, sgm41516d_reg);

	sgm41516d_reg &= (MASK << SHIFT);
	*val = (sgm41516d_reg >> SHIFT);

	pr_info("[%s] val=0x%x\n", __func__, *val);
	mutex_unlock(&info->sgm41516d_access_lock);

	return ret;
}

unsigned int sgm41516d_config_interface(struct sgm41516d_info *info, unsigned char RegNum,
				      unsigned char val, unsigned char MASK,
				      unsigned char SHIFT)
{
	unsigned char sgm41516d_reg = 0;
	unsigned char sgm41516d_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&info->sgm41516d_access_lock);

	ret = sgm41516d_read_byte_verify(info, RegNum, &sgm41516d_reg);
	if (ret < 0)
		pr_info("[%s] read Reg[%x] failed ret = %d\n", __func__, RegNum, ret);
	sgm41516d_reg_ori = sgm41516d_reg;
	sgm41516d_reg &= ~(MASK << SHIFT);
	sgm41516d_reg |= (val << SHIFT);


	ret = sgm41516d_write_byte(info, RegNum, sgm41516d_reg);
	if (ret < 0)
		pr_info("[%s] write Reg[%x] failed ret = %d\n", __func__, RegNum, ret);

	mutex_unlock(&info->sgm41516d_access_lock);
	pr_info("[%s] write Reg[%x]=0x%x from 0x%x\n", __func__,
			     RegNum,
			     sgm41516d_reg, sgm41516d_reg_ori);

	return ret;
}
/**********************************************************
 *
 *   [platform_driver API]
 *
 *********************************************************/
unsigned char g_reg_value_sgm41516d;
static ssize_t show_sgm41516d_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value_sgm41516d);
	return sprintf(buf, "0x%x\n", (unsigned int)g_reg_value_sgm41516d);
}

static ssize_t store_sgm41516d_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;
	struct sgm41516d_info *info = dev_get_drvdata(dev);

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n", __func__, buf,
			size);

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16,
				(unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16,
				(unsigned int *)&reg_address);

		if (size > 5) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_info(
			"[%s] write sgm41516d reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, reg_value);
			//ret = sgm41516d_write_byte(info, reg_address, reg_value);

			ret = sgm41516d_config_interface(info, reg_address,
				reg_value, 0xFF, 0x0);
		} else {
			ret = sgm41516d_read_byte(info, reg_address, &g_reg_value_sgm41516d);
			pr_info(
			"[%s] read sgm41516d reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, g_reg_value_sgm41516d);
			pr_info(
			"[%s] use \"cat sgm41516d_access\" to get value\n",
			__func__);
		}
		if (ret < 0)
			pr_info("%s ret(%d)\n", __func__, ret);
	}
	return size;
}

static DEVICE_ATTR(sgm41516d_access, 0664, show_sgm41516d_access,
		   store_sgm41516d_access);	/* 664 */


static int sgm41516d_chg_set_usbsw(struct sgm41516d_info *info,
                                enum sgm41516d_usbsw usbsw)
{
        struct phy *phy;
        int ret, mode = (usbsw == SGM41516D_USBSW_CHG) ? SGM41516D_PHY_MODE_BC11_SET :
                                               SGM41516D_PHY_MODE_BC11_CLR;

        dev_info(info->dev, "usbsw=%d\n", usbsw);
        phy = phy_get(info->dev, "usb2-phy");
        if (IS_ERR_OR_NULL(phy)) {
                dev_notice(info->dev, "failed to get usb2-phy\n");
                return -ENODEV;
        }    
        ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
        if (ret)
                dev_notice(info->dev, "failed to set phy ext mode\n");
        phy_put(info->dev, phy);
        return ret; 
}

static bool is_sgm41516_series_product(void)
{
	return (g_tran_chg_info == SGM41516 ||
			g_tran_chg_info == SGM41516D ||
			g_tran_chg_info == SGM41516E) ? true : false;
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node = NULL;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		dev_info(dev, "usb ready = %d\n", ready);
	} else
		dev_notice(dev, "usb node missing or invalid\n");
	return ready;
}

static int sgm41516d_chg_enable_bc12(struct sgm41516d_info *info, bool en)
{
        int i, ret;
        static const int max_wait_cnt = 250;

        dev_info(info->dev, "en=%d\n", en);
        if (en) {
                /* CDP port specific process */
                dev_info(info->dev, "check CDP block\n");
                for (i = 0; i < max_wait_cnt; i++) {
                        if (is_usb_rdy(info->dev))
                                break;
                        if (!atomic_read(&info->attach))
                                return 0;
                        msleep(100);
                }
                if (i == max_wait_cnt)
                        dev_notice(info->dev, "%s: CDP timeout\n", __func__);
                else
                        dev_info(info->dev, "%s: CDP free\n", __func__);
        }
        ret = sgm41516d_chg_set_usbsw(info, en ? SGM41516D_USBSW_CHG : SGM41516D_USBSW_USB);

	return ret;
}
static void sgm41516d_bc12_detect(struct work_struct *work)
{
	unsigned int usb_status = 0,bq_detect_count=0;
	unsigned int iidet_bit=1;
	bool attach;
	struct sgm41516d_info *info = container_of(work,
	                                         struct sgm41516d_info,
	                                         bc12_work);

	mutex_lock(&info->attach_lock);

	attach = atomic_read(&info->attach);
/*BSP:modify for plug in or out no charger-icon by jianqiang.ouyang 20220802 start*/
	if(!attach){
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	}

	if (info->sgm41516d_otg_enable_flag) {
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
/*BSP:modify for plug in or out no charger-icon by jianqiang.ouyang 20220802 end*/
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	}
	sgm41516d_chg_enable_bc12(info, true);
	sgm41516d_force_dpdm_enable(info, 1);

	do{
	    msleep(50);
	    iidet_bit =sgm41516d_get_iidet_status(info);
	    bq_detect_count++;
	    pr_info("%s: count_max=%d,iidet_bit=%d,usb_status =%d\n",__func__,bq_detect_count,iidet_bit, sgm41516d_get_vbus_stat(info));
	    if(bq_detect_count>BQ_DET_COUNT_MAX)
	            iidet_bit = 0;
	}while(iidet_bit);

	usb_status = sgm41516d_get_vbus_stat(info);
	pr_info("%s: usb_stats2 = 0x%X\n", __func__, usb_status);

	switch (usb_status) {
	case SGM41516D_CHG_TYPE_SDP:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case SGM41516D_CHG_TYPE_UNKNOW:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case SGM41516D_CHG_TYPE_CDP:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case SGM41516D_CHG_TYPE_SDPNSTD:
	case SGM41516D_CHG_TYPE_DCP:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	default:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	    }
out:
	info->bc12_done = true;
	mutex_unlock(&info->attach_lock);
	if (info->psy_desc.type != POWER_SUPPLY_TYPE_USB_DCP)
		sgm41516d_chg_enable_bc12(info, false);

	if (info->psy && !(info->sgm41516d_otg_enable_flag))
		power_supply_changed(info->psy);
}

static int sgm41516d_chg_attach_process(struct sgm41516d_info *info, int intval)
 {  

	if (intval == ATTACH_TYPE_PD_DCP && info->bc12_done) {
		pr_info("%s: ignore pd_dcp after bc12 done\n", __func__);
		return 0;
	}

	atomic_set(&info->attach, !!intval);
	schedule_work(&info->bc12_work);

	return 0;
 }


/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */
void sgm41516d_set_en_hiz(struct sgm41516d_info *info, bool val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_iinlim(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_stat_ctrl(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON0),
				   (unsigned char) (val),
				   (unsigned char) (CON0_STAT_IMON_CTRL_MASK),
				   (unsigned char) (CON0_STAT_IMON_CTRL_SHIFT)
				   );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/* CON1---------------------------------------------------- */
/*It can only be used once during boot initialization*/
void sgm41516d_set_reg_rst(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON11),
				       (unsigned char) (val),
				       (unsigned char) (CON11_REG_RST_MASK),
				       (unsigned char) (CON11_REG_RST_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_pfm(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_PFM_MASK),
				       (unsigned char) (CON1_PFM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_wdt_rst(struct sgm41516d_info *info, bool val)
{

	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_WDT_RST_MASK),
				       (unsigned char) (CON1_WDT_RST_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_otg_config(struct sgm41516d_info *info, bool val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_OTG_CONFIG_MASK),
				       (unsigned char) (CON1_OTG_CONFIG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}


void sgm41516d_set_chg_config(struct sgm41516d_info *info, bool val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_CHG_CONFIG_MASK),
				       (unsigned char) (CON1_CHG_CONFIG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 start*/
unsigned int sgm41516d_get_chg_config(struct sgm41516d_info *info, unsigned char *val)
{
	unsigned int ret = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON1),
			val,
			(unsigned char) (CON1_CHG_CONFIG_MASK),
			(unsigned char) (CON1_CHG_CONFIG_SHIFT)
			);

	return ret;
}
/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 end*/

void sgm41516d_set_sys_min(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_SYS_MIN_MASK),
				       (unsigned char) (CON1_SYS_MIN_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_batlowv(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_MIN_VBAT_SEL_MASK),
				       (unsigned char) (CON1_MIN_VBAT_SEL_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}



/* CON2---------------------------------------------------- */
void sgm41516d_set_rdson(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_Q1_FULLON_MASK),
				       (unsigned char) (CON2_Q1_FULLON_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_boost_lim(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_BOOST_LIM_MASK),
				       (unsigned char) (CON2_BOOST_LIM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516e_set_boost_lim(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_BOOST_CUR_LIMIT_MASK),
				       (unsigned char) (CON7_BOOST_CUR_LIMIT_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}


void sgm41516d_set_ichg(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICHG_MASK),
				       (unsigned char) (CON2_ICHG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/* CON3---------------------------------------------------- */

void sgm41516d_set_iprechg(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_IPRECHG_MASK),
				       (unsigned char) (CON3_IPRECHG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_iterm(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_ITERM_MASK),
				       (unsigned char) (CON3_ITERM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned char sgm41516d_get_iterm(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON3),
				       &val,
				       (unsigned char) (CON3_ITERM_MASK),
				       (unsigned char) (CON3_ITERM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}


/* CON4---------------------------------------------------- */

void sgm41516d_set_vreg(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VREG_MASK),
				       (unsigned char) (CON4_VREG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned char sgm41516d_get_vreg(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON4),
				       &val,
				       (unsigned char) (CON4_VREG_MASK),
				       (unsigned char) (CON4_VREG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

void sgm41516d_set_topoff_timer(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_TOPOFF_TIMER_MASK),
				       (unsigned char) (CON4_TOPOFF_TIMER_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}


void sgm41516d_set_vrechg(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VRECHG_MASK),
				       (unsigned char) (CON4_VRECHG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/* CON5---------------------------------------------------- */

void sgm41516d_set_en_term(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;
	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TERM_MASK),
				       (unsigned char) (CON5_EN_TERM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}



void sgm41516d_set_watchdog(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_WATCHDOG_MASK),
				       (unsigned char) (CON5_WATCHDOG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_en_timer(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TIMER_MASK),
				       (unsigned char) (CON5_EN_TIMER_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_chg_timer(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_CHG_TIMER_MASK),
				       (unsigned char) (CON5_CHG_TIMER_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_chg_thermal(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_TREG_MASK),
				       (unsigned char) (CON5_TREG_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/* CON6---------------------------------------------------- */

void sgm41516d_set_treg(struct sgm41516d_info *info, unsigned int val)
{
#if 0
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
#endif
}

void sgm41516d_set_vindpm(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VINDPM_MASK),
				       (unsigned char) (CON6_VINDPM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned char sgm41516d_get_vindpm(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON6),
				       &val,
				       (unsigned char) (CON6_VINDPM_MASK),
				       (unsigned char) (CON6_VINDPM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}


void sgm41516d_set_ovp(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_OVP_MASK),
				       (unsigned char) (CON6_OVP_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);

}

void sgm41516d_set_boostv(struct sgm41516d_info *info, unsigned int val)
{

	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}



/* CON7---------------------------------------------------- */

void sgm41516d_set_tmr2x_en(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_TMR2X_EN_MASK),
				       (unsigned char) (CON7_TMR2X_EN_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_batfet_disable(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_Disable_MASK),
				(unsigned char) (CON7_BATFET_Disable_SHIFT)
				);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}


void sgm41516d_set_batfet_delay(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_BATFET_DLY_MASK),
				       (unsigned char) (CON7_BATFET_DLY_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_batfet_reset_enable(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_RST_EN_MASK),
				(unsigned char) (CON7_BATFET_RST_EN_SHIFT)
				);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}


/* CON8---------------------------------------------------- */

unsigned int sgm41516d_get_system_status(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON8),
				     (&val), (unsigned char) (0xFF),
				     (unsigned char) (0x0)
				    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

unsigned int sgm41516d_get_vbus_stat(struct sgm41516d_info *info)
{
	unsigned int i, j, ret = 0;
	unsigned char val = 0;
	const int retry_count = 5;
	unsigned int unknow_count = 0,sdp_count = 0,cdp_count = 0,dcp_count = 0,sdpnstd_count = 0,otg_count = 0;
	struct chg_type_record sgm41516d_chg_type_record[] = {
			{SGM41516D_CHG_TYPE_SDP, sdp_count},
			{SGM41516D_CHG_TYPE_CDP, cdp_count},
			{SGM41516D_CHG_TYPE_DCP, dcp_count},
			{SGM41516D_CHG_TYPE_SDPNSTD, sdpnstd_count},
			{SGM41516D_CHG_TYPE_OTG, otg_count},
			{SGM41516D_CHG_TYPE_UNKNOW, unknow_count},
	};
	for (i = 0; i < retry_count; i++){
		ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON8),
					     (&val),
					     (unsigned char) (CON8_VBUS_STAT_MASK),
					     (unsigned char) (CON8_VBUS_STAT_SHIFT)
					    );
		if (ret < 0)
			pr_info("%s ret(%d)\n", __func__, ret);
		for (j = 0; j < GETARRAYNUM(sgm41516d_chg_type_record); j++){
				if (val == sgm41516d_chg_type_record[j].chg_type){
					sgm41516d_chg_type_record[j].chg_type_count ++;
					if (sgm41516d_chg_type_record[j].chg_type_count >= retry_count-2){
						return sgm41516d_chg_type_record[j].chg_type;
					}
					break;
				}
		}
	}


	return SGM41516D_CHG_TYPE_NOVBUS;
}

unsigned int sgm41516d_get_chrg_stat(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON8),
				     (&val),
				     (unsigned char) (CON8_CHRG_STAT_MASK),
				     (unsigned char) (CON8_CHRG_STAT_SHIFT)
				    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

unsigned int sgm41516d_get_vsys_stat(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON8),
				     (&val),
				     (unsigned char) (CON8_VSYS_STAT_MASK),
				     (unsigned char) (CON8_VSYS_STAT_SHIFT)
				    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

unsigned int sgm41516d_get_pg_stat(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON8),
				     (&val),
				     (unsigned char) (CON8_PG_STAT_MASK),
				     (unsigned char) (CON8_PG_STAT_SHIFT)
				    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}


/*CON10----------------------------------------------------------*/

void sgm41516d_set_int_mask(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON10),
				       (unsigned char) (val),
				       (unsigned char) (CON10_INT_MASK_MASK),
				       (unsigned char) (CON10_INT_MASK_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/*CON13 for SMG41516-----------------------------------------------------*/
void sgm41516d_set_dp(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON13),
		(unsigned char) (val),
		(unsigned char) (CON13_REG_DP_VSET_MASK),
		(unsigned char) (CON13_REG_DP_VSET_SHIFT)
	);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

void sgm41516d_set_dm(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON13),
		(unsigned char) (val),
		(unsigned char) (CON13_REG_DM_VSET_MASK),
		(unsigned char) (CON13_REG_DM_VSET_SHIFT)
	);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

/*CON15 for SMG41516-----------------------------------------------------*/
void sgm41516d_set_vindpm_os(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON15),
		(unsigned char) (val),
		(unsigned char) (CON15_REG_VINDPM_OS_MASK),
		(unsigned char) (CON15_REG_VINDPM_OS_SHIFT)
	);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned int sgm41516d_get_vindpm_os(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON15),
				     (&val),
				     (unsigned char) (CON15_REG_VINDPM_OS_MASK),
				     (unsigned char) (CON15_REG_VINDPM_OS_SHIFT)
				    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

void sgm41516d_set_cv_fine_tuning(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON15),
		(unsigned char) (val),
		(unsigned char) (CON15_REG_FINE_TUNING_MASK),
		(unsigned char) (CON15_REG_FINE_TUNING_SHIFT)
	);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned char sgm41516d_get_cv_fine_tuning(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON15),
				       &val,
				       (unsigned char) (CON15_REG_FINE_TUNING_MASK),
				       (unsigned char) (CON15_REG_FINE_TUNING_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm41516d_dump_register(struct charger_device *chg_dev)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char sgm41516d_reg[sgm41516d_REG_NUM] = { 0 };

	pr_info("[sgm41516d] ");
	for (i = 0; i < sgm41516d_REG_NUM; i++) {
		ret = sgm41516d_read_byte_verify(info, i, &sgm41516d_reg[i]);
		if (ret == 0) {
			pr_info("[sgm41516d] i2c transfor error\n");
			return 1;
		}
		pr_info("[0x%x]=0x%x ", i, sgm41516d_reg[i]);
	}
	pr_debug("\n");
	return 0;
}


/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/


static int sgm41516d_enable_charging(struct charger_device *chg_dev,
				   bool en)
{
	int status = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", en);
	if (en) {
		/* enable charging */
		sgm41516d_set_en_hiz(info, false);
		sgm41516d_set_chg_config(info, en);
	} else {
		/* disable charging */
		sgm41516d_set_chg_config(info, en);
		pr_info("[charging_enable] under test mode: disable charging\n");
	}

	return status;
}

static int sgm41516d_get_current(struct charger_device *chg_dev,
			       u32 *ichg)
{
	unsigned char ret_val = 0;
	unsigned int ret = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", *ichg);
	/* Get current level */
	ret=sgm41516d_read_interface(info, sgm41516d_CON2, &ret_val, CON2_ICHG_MASK,
			       CON2_ICHG_SHIFT);

	/* Parsing */
	*ichg = CS_VTH[ret_val]*10;

	return ret;
}

static int sgm41516d_set_current(struct charger_device *chg_dev,
			       u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("&&&& charge_current_value = %d\n", current_value);
	current_value /= 10;
	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
			 set_chr_current);
	//pr_info("&&&& charge_register_value = %d\n",register_value);
	pr_info("&&&& %s register_value = %d\n", __func__,
		register_value);
	sgm41516d_set_ichg(info, register_value);

	return status;
}

static int sgm41516d_get_input_current(struct charger_device *chg_dev,
				     u32 *aicr)
{
	int ret = 0;
	unsigned char val = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	ret=sgm41516d_read_interface(info, sgm41516d_CON0, &val, CON0_IINLIM_MASK,
			       CON0_IINLIM_SHIFT);
	*aicr = INPUT_CS_VTH[val]*10;
	pr_info("read value = %d\n", *aicr);
	return ret;
}


static int sgm41516d_set_input_current(struct charger_device *chg_dev,
				     u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", current_value);
	current_value /= 10;
	pr_info("&&&& current_value = %d\n", current_value);
	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size,
			 set_chr_current);
	pr_info("&&&& %s register_value = %d\n", __func__,
		register_value);
	sgm41516d_set_iinlim(info, register_value);

	return status;
}

static int battery_get_bat_voltage(void)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	struct mtk_gauge_sysfs_field_info *attr;
	int gp = GAUGE_PROP_BATTERY_VOLTAGE;
	int ret = 0;
	int val;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL) {
		pr_info("can not find gauge\n");
		return -ENODEV;
	}
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
	attr = gauge->attr;
	if (attr == NULL) {
		pr_info("not find gauge attr\n");
		return -ENODEV;
	}
	if (attr[gp].prop == gp) {
		ret = attr[gp].get(gauge, &attr[gp], &val);
		if (ret < 0)
			pr_info("%s ret(%d)\n", __func__, ret);
	} else {
		pr_info("%s gp:%d idx error\n", __func__, gp);
		return -ENOTSUPP;
	}
	pr_info("%s getvbat  = %d\n", __func__, val);
	return val;
}

static int sgm41516d_set_cv_voltage(struct charger_device *chg_dev,
				  u32 cv)
{
	unsigned int array_size;
	unsigned int set_cv_voltage;
	unsigned short register_value;
	unsigned int smg41516_fine=0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s value = %d\n", __func__,cv);
	info->temp_cv = cv;
	if(is_sgm41516_series_product()){
		if ((cv < 4300000) && (battery_get_bat_voltage()*1000 >= cv)) {
				info->jeita_hight_temp_cv = true;
				return 0;
		} else {
			info->jeita_hight_temp_cv = false;
		}
	}
	pr_info("%s,value = %d,info->jeita_hight_temp_cv=%d\n", __func__, cv, info->jeita_hight_temp_cv);
	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv);
	register_value = charging_parameter_to_value(VBAT_CV_VTH, array_size,set_cv_voltage);

	if(is_sgm41516_series_product()){
		if(cv==SPECIAL_CV_VAL){
			register_value=SPECIAL_CV_BIT;
			smg41516_fine=CV_NORMAL;
			goto out;
		}
		smg41516_fine=((cv-CV_OFFSET)%VBAT_CV_FINE[0])/VBAT_CV_FINE[1];
		switch(smg41516_fine){
			case 0:
				smg41516_fine=CV_NORMAL;
				break;
			case 1:
				smg41516_fine=CV_POST_8MV;
				break;
			case 2:
				register_value+=1;
				smg41516_fine=CV_NEG_16MV;
				break;
			case 3:
				register_value+=1;
				smg41516_fine=CV_NEG_8MV;
				break;
			default:
				break;
		};
	}

out:
	sgm41516d_set_vreg(info, register_value);

	if(is_sgm41516_series_product())
		sgm41516d_set_cv_fine_tuning(info, smg41516_fine);
	pr_info("cv reg value = %d %d %d smg41516_fine=%d\n", register_value,cv,set_cv_voltage,smg41516_fine);

	return 0;
}

static int sgm41516d_get_cv_voltage(struct charger_device *chg_dev,
				  u32 *cv)
{
	unsigned char vreg, sgm41516_fine;

	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	
	vreg = sgm41516d_get_vreg(info);
	*cv = vreg * 32000 + CV_OFFSET;

	sgm41516_fine = sgm41516d_get_cv_fine_tuning(info);

	switch (sgm41516_fine) {
	case CV_NORMAL:
		break;

	case CV_POST_8MV:
		*cv = *cv + 8000;
		break;

	case CV_NEG_8MV:
		*cv = *cv - 8000;
		break;

	case CV_NEG_16MV:
		*cv = *cv - 16000;
		break;

	default:
		break;
	
	}
	pr_info("cv value = %d\n", *cv);

	
	return 0;
}


static int sgm41516d_reset_watch_dog_timer(struct charger_device
		*chg_dev)
{
	unsigned int status = true;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("charging_reset_watch_dog_timer\n");

	sgm41516d_set_wdt_rst(info, true);	/* Kick watchdog */
	
	//sgm41516d_set_watchdog(info, WDT_160S);

	return status;
}


static int sgm41516d_set_vindpm_voltage(struct charger_device *chg_dev,
				      u32 vindpm)
{
	int status = 0;
	unsigned int vindpm_os=0;
	unsigned int set_value=0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", vindpm);

	if(!is_sgm41516_series_product())
		return status;

	vindpm /= 1000;
	if(vindpm<=VINDPM_OS0_MIVR_MIN)
		return status;
	if(vindpm>=VINDPM_OS3_MIVR_MAX)
		vindpm=VINDPM_OS3_MIVR_MAX;

	switch(vindpm){
		case 3900 ... 5400:
			vindpm_os=0;
			set_value=sgm41516d_closest_reg(VINDPM_OS0_MIVR_MIN,VINDPM_OS0_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		case 5500 ... 5800:
			vindpm_os=0;
			vindpm=VINDPM_OS0_MIVR_MAX;
			set_value=sgm41516d_closest_reg(VINDPM_OS0_MIVR_MIN,VINDPM_OS0_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		case 5900 ... 7400:
			vindpm_os=1;
			set_value=sgm41516d_closest_reg(VINDPM_OS1_MIVR_MIN,VINDPM_OS1_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		case 7500 ... 9000:
			vindpm_os=2;
			set_value=sgm41516d_closest_reg(VINDPM_OS2_MIVR_MIN,VINDPM_OS2_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		case 10000 ... 10400:
			vindpm_os=2;
			vindpm=VINDPM_OS2_MIVR_MAX;
			set_value=sgm41516d_closest_reg(VINDPM_OS2_MIVR_MIN,VINDPM_OS2_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		case 10500 ... 12000:
			vindpm_os=3;
			set_value=sgm41516d_closest_reg(VINDPM_OS3_MIVR_MIN,VINDPM_OS3_MIVR_MAX,VINDPM_OS_MIVR_STEP,vindpm);
			break;
		default:
			return false;
	};
	pr_info("%s vindpm =%d vindpm_os=%d set_value=%d\r\n", __func__, vindpm,vindpm_os,set_value);

	sgm41516d_set_vindpm_os(info, vindpm_os);
	sgm41516d_set_vindpm(info, set_value);
	return status;
}

static int sgm41516d_get_boost_lim(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON2),
				       &val,
				       (unsigned char) (CON2_BOOST_LIM_MASK),
				       (unsigned char) (CON2_BOOST_LIM_SHIFT)
				      );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 start*/
int __sgm41516d_is_chg_enabled(struct sgm41516d_info *info, bool *en)
{
	int ret = 0;
	unsigned char regval = 0;

	ret = sgm41516d_get_chg_config(info, &regval);

	if (ret < 0) {
		pr_info("%s:(%d) read CHG_EN fail\n", __func__, ret);
		return ret;
	}
	*en = regval;
	return ret;
}
/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 end*/

static void sgm41516d_get_ic_stat(struct sgm41516d_info *info, u32 *val)
{
	unsigned int stat,ret = 0;
	bool chg_en = false;

	stat = sgm41516d_get_chrg_stat(info);
	ret = atomic_read(&info->attach);
	__sgm41516d_is_chg_enabled(info,&chg_en);
	pr_info(" %s stat =%d,online =%d,chg_en = %d\n", __func__,stat,ret,chg_en);
	switch (stat) {
/*BSP:modify for hight temp charger-icon no disappear by jianqiang.ouyang 20220719 start*/
	case SGM41516D_ICSTAT_DISABLE:
	case SGM41516D_ICSTAT_PRECHG:
	case SGM41516D_ICSTAT_FASTCHG:
		*val = POWER_SUPPLY_STATUS_CHARGING;
		break;
/*BSP:modify for hight temp charger-icon no disappear by jianqiang.ouyang 20220719 end*/
	case SGM41516D_ICSTAT_CHGDONE:
		*val = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}
}

static int sgm41516d_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	unsigned int ret = 0,is_in_vindpm = 0, i = 0;
	unsigned char val = 0;
	const int retry_count = 5;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);

	for (i = 0;i < retry_count; i++){
		ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON10),
			(&val),
			(unsigned char) (CON10_VINDPM_STAT_MASK),
			(unsigned char) (CON10_VINDPM_STAT_SHIFT)
			);
		if (val == 0x1)
			is_in_vindpm++;
	}
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	pr_info("pass value = 0x%x\n", val);
	if (is_in_vindpm >= retry_count - 1) {
		*in_loop = true;
		return 1;
	} else {
		*in_loop = false;
		return 0;
	}

}

static int sgm41516d_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	unsigned char val = 0;
	unsigned char vindpm_bit = 0;
	unsigned int vindpm_offset = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	val=sgm41516d_get_vindpm_os(info);
	vindpm_bit=sgm41516d_get_vindpm(info);
	switch(val){
		case 0:
			vindpm_offset=VINDPM_OS0_MIVR_MIN;
			break;
		case 1:
			vindpm_offset=VINDPM_OS1_MIVR_MIN;
			break;
		case 2:
			vindpm_offset=VINDPM_OS2_MIVR_MIN;
			break;
		case 3:
			vindpm_offset=VINDPM_OS3_MIVR_MIN;
			break;
		default:
			return false;
	};

	*uV=(vindpm_offset+VINDPM_OS_MIVR_STEP*vindpm_bit)*1000;
	pr_info("%s val =%d vindpm_bit=%d vindpm_offset=%d *uV=%d\r\n", __func__, val,vindpm_bit,vindpm_offset,*uV);
	return true;
}

static int sgm41516d_get_charging_status(struct charger_device *chg_dev,
 				       bool *is_done)
 {
 	unsigned int status = true;
 	unsigned int ret_val, i;
 	unsigned int is_done_count = 0;
 	const int retry_count = 5;
 	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
 
 	if (info->jeita_hight_temp_cv == true) {
		if (battery_get_bat_voltage()*1000 <= info->temp_cv - 100000) {
			sgm41516d_set_cv_voltage(chg_dev, info->temp_cv);
			info->jeita_hight_temp_cv = false;
		}
		*is_done = true;
		return true;
	}

 	for (i = 0;i < retry_count; i++){
 		ret_val = sgm41516d_get_chrg_stat(info);
 		if (ret_val == 0x3)
 			is_done_count++;
 	}
 	if (is_done_count >= retry_count-1)
 		*is_done = true;
 	else
 		*is_done = false;
 
 	return status;
}

//BSP:CHG OTG high current reverse charger CLBLELEPUB-279 by minglong.li 20220308 start
#if IS_ENABLED(CONFIG_TRAN_CHARGER_REVERSE)
static int sgm41516d_pfm_enable(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s en = %d\n", __func__, en);
	if (en) {
		sgm41516d_set_pfm(info, 0x0);//enable pfm
	} else {
		sgm41516d_set_pfm(info, 0x1);//disable pfm
	}

	return ret;
}
#endif
//BSP:CHG OTG high current reverse charger CLBLELEPUB-279 by minglong.li 20220308 end
static int sgm41516d_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	struct regulator *regulator;
		
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	regulator = devm_regulator_get(info->dev, "usb-otg-vbus");
	if (IS_ERR(regulator)) {
		dev_notice(info->dev, "%s: failed to get otg regulator\n",
								__func__);
		return PTR_ERR(regulator);
	}
	if (en && !regulator_is_enabled(regulator))
		ret = regulator_enable(regulator);
	
	if(ret){
		dev_notice(info->dev, "%d: failed to enable otg regulator\n",ret);
	}
	
	else if (!en && regulator_is_enabled(regulator))
		regulator_disable(regulator);

	return ret;
}

static int __sgm41516d_enable_otg(struct sgm41516d_info *info, bool en)
{
	int ret = 0;

	pr_info("%s en = %d\n", __func__, en);
	
	if (en) {
		info->sgm41516d_otg_enable_flag = true;
		sgm41516d_set_chg_config(info, false);
		sgm41516d_set_otg_config(info, en);
		sgm41516d_set_watchdog(info, DISABLE_WDT);	/*default WDT_160S WDT 160s modify BSP:CHG LFQHLJYB-3278 minglong.li 20220211*/
	} else {
		info->sgm41516d_otg_enable_flag = false;
		sgm41516d_set_otg_config(info, en);
		sgm41516d_set_chg_config(info, true);
		sgm41516d_set_watchdog(info, DISABLE_WDT);
	}
	sgm41516d_set_en_hiz(info,!en);
	return ret;
}

static int sgm41516d_set_boost_current_limit(struct charger_device
		*chg_dev, u32 uA)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	struct regulator *regulator;
	int ret = 0;

	regulator = devm_regulator_get(info->dev, "usb-otg-vbus");
	if (IS_ERR(regulator)) {
		dev_notice(info->dev, "%s: failed to get otg regulator\n",
								__func__);
		return PTR_ERR(regulator);
	}
	ret = regulator_set_current_limit(regulator, uA, uA);
	devm_regulator_put(regulator);
	return ret;
}

static int __sgm41516d_set_otgcc(struct sgm41516d_info *info, u32 cc)
{
	int num = ARRAY_SIZE(sgm41516d_otgcc);
	int index = charging_parameter_to_value(sgm41516d_otgcc,num,cc);
	dev_info(info->dev, "%s cc = %d index = %d\n", __func__, cc, index);

	if (info->vendor_id == SGM41516_VENDOR_ID ||
		info->vendor_id == SGM41516D_VENDOR_ID) {
		if (index >= 1)
			sgm41516d_set_boost_lim(info, 0x01);
		else
			sgm41516d_set_boost_lim(info, 0x00);
	} else if (info->vendor_id == SGM41516E_VENDOR_ID) {
			sgm41516e_set_boost_lim(info, (num -1) - index);
	} else {
		/* others products */
	}
	return 0;
}


static int sgm41516d_enable_safetytimer(struct charger_device *chg_dev,
				      bool en)
{
	int status = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", en);
	if (en)
		sgm41516d_set_en_timer(info, 0x1);
	else
		sgm41516d_set_en_timer(info, 0x0);
	return status;
}

static int sgm41516d_get_is_safetytimer_enable(struct charger_device
		*chg_dev, bool *en)
{
	unsigned char val = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	sgm41516d_read_interface(info, sgm41516d_CON5, &val, CON5_EN_TIMER_MASK,
			       CON5_EN_TIMER_SHIFT);
	*en = (bool)val;
	pr_info("pass value = %d\n", val);
	return val;
}


void sgm41516d_force_dpdm_enable(struct sgm41516d_info *info, unsigned int val)
{
	unsigned int ret = 0;

	ret = sgm41516d_config_interface(info, (unsigned char) (sgm41516d_CON7),
		(unsigned char) (val),
		(unsigned char) (CON7_FORCE_DPDM_MASK),
		(unsigned char) (CON7_FORCE_DPDM_SHIFT)
	);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
}

unsigned int sgm41516d_get_iidet_status(struct sgm41516d_info *info)
{
	unsigned char val = 0;
	unsigned int ret = 0;

	ret = sgm41516d_read_byte_verify(info, sgm41516d_CON7, &val);
	val &= (CON7_FORCE_DPDM_MASK << CON7_FORCE_DPDM_SHIFT);
	val = (val >> CON7_FORCE_DPDM_SHIFT);
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
        return val;
}
/*
static int mtk_ext_chgdet(struct charger_device *chg_dev)
{
    unsigned int usb_status = 0,bq_detect_count=0;
    unsigned int iidet_bit=1;
    struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

    sgm41516d_dump_register(chg_dev);
    pr_info( "kernel_sgm41516d_chg_type detect]\n");
    Charger_Detect_Init();

        bq_chg_type = CHARGER_UNKNOWN;

    sgm41516d_force_dpdm_enable(info, 1);
    //usb_status = sgm41516d_get_vbus_stat();
    //pr_info("%s: usb_stats1 = 0x%X\n", __func__, usb_status);
    do{
        msleep(50);
        iidet_bit =sgm41516d_get_iidet_status(info);
        bq_detect_count++;
        pr_info("%s: count_max=%d,iidet_bit=%d,usb_status =%d\n",__func__,bq_detect_count,iidet_bit, sgm41516d_get_vbus_stat(info));
        if(bq_detect_count>BQ_DET_COUNT_MAX)
                iidet_bit = 0;
    }while(iidet_bit);
    usb_status = sgm41516d_get_vbus_stat(info);
    pr_info("%s: usb_stats2 = 0x%X\n", __func__, usb_status);

    switch (usb_status) {
    case SGM41516D_CHG_TYPE_SDP:
            bq_chg_type = STANDARD_HOST;
            break;
    case SGM41516D_CHG_TYPE_UNKNOW:
            bq_chg_type = NONSTANDARD_CHARGER;
            break;
    case SGM41516D_CHG_TYPE_CDP:
            bq_chg_type = CHARGING_HOST;
            break;
    case SGM41516D_CHG_TYPE_DCP:
            bq_chg_type = STANDARD_CHARGER;
            break;
    case SGM41516D_CHG_TYPE_SDPNSTD:
            bq_chg_type = APPLE_2_1A_CHARGER;
            break;

    default:
                bq_chg_type = CHARGER_UNKNOWN;
                break;
        }
    pr_info("%s: bq_chg_type = %d\n", __func__, bq_chg_type);
        Charger_Detect_Release();
        return bq_chg_type;
}
*/
#if IS_ENABLED(CONFIG_TRAN_CHARGER_OTG_LPO_SUPPORT)
static int sgm41516d_get_boost_status(struct charger_device *chg_dev)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned char val = 0;

	pr_info("%s enter!\n", __func__);
	val = sgm41516d_get_vbus_stat(info);
	val = ((val == 7)? 1:0);
	pr_info("pass value = %d\n", val);
	return val;
}
#endif

//BSP:add XLEWCHWQL-89 by ronghe.cheng 20190107 start
static int sgm41516d_set_term_current(struct charger_device *chg_dev, u32 current_value)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned int iterm_reg = 0x2;

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", current_value);
	if(current_value > 960000){
                iterm_reg = 0xF; /*Termination current = 960ma */
        }else if(current_value < 60000){
                iterm_reg = 0x0;        /*Termination current = 60ma */
        }else{
                iterm_reg = (current_value/60000)-1;
        }

	sgm41516d_set_iterm(info, iterm_reg);
	return 0;
}
//BSP:add XLEWCHWQL-89 by ronghe.cheng 20190107 end

static int sgm41516d_get_term_current(struct charger_device *chg_dev, u32 *current_value)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	unsigned char iterm_reg;

	pr_info("%s enter!\n", __func__);

	iterm_reg = sgm41516d_get_iterm(info);
	*current_value = iterm_reg * 60000 + 60000;

	pr_info("term_current = %d\n", *current_value);
	return 0;
}

static unsigned int charging_hw_init(struct sgm41516d_info *info)
{
	unsigned int status = 0;

	sgm41516d_set_en_hiz(info, false);
	sgm41516d_set_vindpm(info, 0x6);	/* VIN DPM check 4.5V */
	sgm41516d_set_wdt_rst(info, true);	/* Kick watchdog */
	sgm41516d_set_sys_min(info, 0x5);	/* Minimum system voltage 3.5V */
	sgm41516d_set_iprechg(info, 0x7);	/* Precharge current 480mA */
	sgm41516d_set_iterm(info, 0x2);	/* Termination current 180mA */
	sgm41516d_set_vreg(info, 0x11);	/* VREG 4.4V */
	sgm41516d_set_pfm(info, 0x1);//disable pfm
	sgm41516d_set_rdson(info, 0x0);     /*close rdson*/
	sgm41516d_set_batlowv(info, 0x1);	/* BATLOWV 3.0V */
	sgm41516d_set_vrechg(info, 0x0);	/* VRECHG 0.1V */
	sgm41516d_set_en_term(info, 0x1);	/* Enable termination */
	sgm41516d_set_watchdog(info, 0x3);	/* WDT 160s */
	sgm41516d_set_en_timer(info, 0x1);	/* Enable charge timer */
	sgm41516d_set_int_mask(info, 0x0);	/* Disable fault interrupt */
	sgm41516d_set_batfet_reset_enable(info, 0x0);/*disable batfet*/
	#if IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT) || IS_ENABLED(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)
	sgm41516d_set_ovp(info, 0x2);
	sgm41516d_set_chg_thermal(info, 0x1);//SGM41516 thermal to 120
	#endif
	//BSP:add for reverse charge by fan.hong 20220110 start
	sgm41516d_set_boostv(info,0x3);
	//BSP:add for reverse charge by fan.hong 20220110 end
	info->sgm41516d_otg_enable_flag = false;
	pr_info("%s: hw_init down!\n", __func__);
	return status;
}

static int sgm41516d_parse_dt(struct sgm41516d_info *info,
                           struct device *dev)
{
       struct device_node *np = dev->of_node;
       //int sgm41516d_en_pin = 0;

       pr_info("%s\n", __func__);
       if (!np) {
              pr_info("%s: no of node\n", __func__);
               return -ENODEV;
       }

       if (of_property_read_string(np, "charger_name",
                                   &info->chg_dev_name) < 0) {
               info->chg_dev_name = "primary_chg";
               pr_info("%s: no charger name\n", __func__);
       }

       if (of_property_read_string(np, "alias_name",
                                   &(info->chg_props.alias_name)) < 0) {
               info->chg_props.alias_name = "sgm41516d";
               pr_info("%s: no alias name\n", __func__);
       }
       /*
        * sgm41516d_en_pin = of_get_named_gpio(np,"gpio_sgm41516d_en",0);
        * if(sgm41516d_en_pin < 0){
        * pr_info("%s: no sgm41516d_en_pin\n", __func__);
        * return -ENODATA;
        * }
        * gpio_request(sgm41516d_en_pin,"sgm41516d_en_pin");
        * gpio_direction_output(sgm41516d_en_pin,0);
        * gpio_set_value(sgm41516d_en_pin,0);
        */
       /*
        * if (of_property_read_string(np, "eint_name", &info->eint_name) < 0) {
        * info->eint_name = "chr_stat";
        * pr_debug("%s: no eint name\n", __func__);
        * }
        */
       return 0;
}

static int sgm41516d_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{/*
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
*/
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s event = %d\n", __func__, event);

	power_supply_changed(info->psy);
	return 0;
}


static int sgm41516d_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
/*
    struct charger_manager *chg_mgr = NULL;

    chg_mgr = charger_dev_get_drvdata(chg_dev);
    if (!chg_mgr)
            return -EINVAL;

    chg_mgr->pe2.profile[0].vchr = 9000000;
    chg_mgr->pe2.profile[1].vchr = 9000000;
    chg_mgr->pe2.profile[2].vchr = 9000000;
    chg_mgr->pe2.profile[3].vchr = 9000000;
    chg_mgr->pe2.profile[4].vchr = 9000000;
    chg_mgr->pe2.profile[5].vchr = 9000000;
    chg_mgr->pe2.profile[6].vchr = 9000000;
    chg_mgr->pe2.profile[7].vchr = 9000000;
    chg_mgr->pe2.profile[8].vchr = 9000000;
    chg_mgr->pe2.profile[9].vchr = 9000000;
*/
        return 0;
}

struct timespec64 ptime[13];
static int cptime[13][2];

static int dtime(int i)
{
    struct timespec64 time;

    time = timespec64_sub(ptime[i], ptime[i-1]);
    return time.tv_nsec/1000000;
}

#define PEOFFTIME 40
#define PEONTIME 90

static int sgm41516d_set_pep20_current_pattern(struct charger_device *chg_dev,
                                                u32 uV)
{
    int value;
    int i, j = 0;
    int flag;
    int errcnt = 0;
    ktime_t ktime_now;

	sgm41516d_set_current(chg_dev,2000000);
   // bq25892_set_ico_en_start(0);

    mdelay(20);
    value = (uV - 5500000) / 500000;

    sgm41516d_set_input_current(chg_dev,0);
    mdelay(150);

    ktime_now = ktime_get_boottime();
    ptime[j++] = ktime_to_timespec64(ktime_now);
    for (i = 4; i >= 0; i--) {
        flag = value & (1 << i);
        if (flag == 0) {
            sgm41516d_set_input_current(chg_dev,800000);
            mdelay(PEOFFTIME);
            ktime_now = ktime_get_boottime();
            ptime[j] = ktime_to_timespec64(ktime_now);
            cptime[j][0] = PEOFFTIME;
            cptime[j][1] = dtime(j);
            if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				errcnt = 1;
				return -1;
			}
            j++;
			sgm41516d_set_input_current(chg_dev,0);
            mdelay(PEONTIME);
            ktime_now = ktime_get_boottime();
            ptime[j] = ktime_to_timespec64(ktime_now);
            cptime[j][0] = PEONTIME;
            cptime[j][1] = dtime(j);
            if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				errcnt = 1;
				return -1;
            }
            j++;
        } else {
			sgm41516d_set_input_current(chg_dev,800000);
            mdelay(PEONTIME);
            ktime_now = ktime_get_boottime();
            ptime[j] = ktime_to_timespec64(ktime_now);
            cptime[j][0] = PEONTIME;
            cptime[j][1] = dtime(j);
            if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				errcnt = 1;
				return -1;
            }
			j++;
			sgm41516d_set_input_current(chg_dev,0);
            mdelay(PEOFFTIME);
            ktime_now = ktime_get_boottime();
            ptime[j] = ktime_to_timespec64(ktime_now);
            cptime[j][0] = PEOFFTIME;
            cptime[j][1] = dtime(j);
            if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				errcnt = 1;
				return -1;
            }
            j++;
        }
	}

	sgm41516d_set_input_current(chg_dev,800000);
    mdelay(200);
    ktime_now = ktime_get_boottime();
    ptime[j] = ktime_to_timespec64(ktime_now);
    cptime[j][0] = 200;
    cptime[j][1] = dtime(j);
    if (cptime[j][1] < 180 || cptime[j][1] > 240) {
            errcnt = 1;
            return -1;
    }
    j++;

	sgm41516d_set_input_current(chg_dev,0);
    mdelay(150);
	sgm41516d_set_input_current(chg_dev,800000);

	//sgm41516d_set_input_current(chg_dev,2000000);

	mdelay(300);

    if (errcnt == 0)
		return 0;
	return -1;
}

static int __maybe_unused sgm41516d_tc30_enable(struct charger_device *chg_dev,bool stat)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	if(stat) {
		sgm41516d_set_dp(info, USB_3_3V);
		sgm41516d_set_dm(info, USB_0V);
		pr_info("%s successfully!\n", __func__);

	} else {
		sgm41516d_set_dp(info, USB_HIZ);
		sgm41516d_set_dm(info, USB_HIZ);
		pr_info("reset dpdm successfully!\n");
	}

	return 0;
}

int sgm41516d_enable_hiz_mode(struct charger_device *chg_dev, bool enable)
{
	int status = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s enter!\n", __func__);
	pr_info("pass value = %d\n", enable);
	sgm41516d_set_en_hiz(info, enable);
	return status;
}

int sgm41516d_get_dev_id(struct sgm41516d_info *info)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = sgm41516d_read_interface(info, (unsigned char) (sgm41516d_CON11),
				     (&val),
				     (unsigned char) (CON11_PN_MASK),
				     (unsigned char) (CON11_PN_SHIFT)
	    );
	if (ret < 0)
		pr_info("%s ret(%d)\n", __func__, ret);
	return val;
}

static int sgm41516d_reset_ta(struct charger_device *chg_dev)
{
	pr_info("%s enter!\n", __func__);
	sgm41516d_set_current(chg_dev,800000); //512mA

	sgm41516d_set_input_current(chg_dev,0);
	msleep(250);//250ms
	sgm41516d_set_input_current(chg_dev,2000000);

	return 0;
}

static int __maybe_unused sgm41516d_hvdcp20_set_dp_0_6_v(struct charger_device *chg_dev)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	sgm41516d_set_dp(info, USB_HIZ);
	sgm41516d_set_dm(info, USB_HIZ);

	msleep(500);
	sgm41516d_set_dp(info, USB_0_6V);//D+ set out 0.6V

	msleep(1800);
	pr_info("%s successfully!\n", __func__);

	return 0;
}

static int __maybe_unused sgm41516d_hvdcp20_set_dp_3_0_v(struct charger_device *chg_dev,bool stat)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	if(stat) {
		sgm41516d_set_dp(info, USB_3_3V);
	} else {
		sgm41516d_set_dp(info, USB_0V);
	}
	pr_info("%s successfully! stat=%d\n", __func__,stat);
	return 0;
}

static int __maybe_unused sgm41516d_hvdcp20_set_dm_0_6_v(struct charger_device *chg_dev)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	sgm41516d_set_dm(info, USB_0_6V);//D- set out 0.6V
	pr_info("%s",__func__);

	return 0;
}

static int __maybe_unused sgm41516d_set_dp_dm_recover(struct charger_device *chg_dev)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	if(!is_sgm41516_series_product())
		return -1;

	sgm41516d_set_dp(info, USB_HIZ);
	sgm41516d_set_dm(info, USB_HIZ);
	pr_info("%s successfully!\n", __func__);

	return 0;
}

static int sgm41516d_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	pr_info("%s enter!\n", __func__);
	sgm41516d_set_watchdog(info, WDT_160S);

	ret = sgm41516d_enable_charging(chg_dev, true);
	if (ret < 0) {
		pr_info( "%s en fail(%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int sgm41516d_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);
	info->jeita_hight_temp_cv = false;

	pr_info("%s enter!\n", __func__);
	/* Disable charging */
	ret = sgm41516d_enable_charging(chg_dev, false);
	if (ret < 0) {
		pr_info("%s en chg fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = sgm41516d_enable_hiz_mode(chg_dev, false);
	if (ret < 0)
		pr_info("%s en hz fail(%d)\n", __func__, ret);

	/* Disable WDT */
	sgm41516d_set_watchdog(info, DISABLE_WDT);

	info->bc12_done = false;

#if IS_ENABLED(CONFIG_MTK_HVDCP20_SUPPORT)||IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
	sgm41516d_set_dp_dm_recover(chg_dev);
#endif

	return 0;
}

/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 start*/
int sgm41516d_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sgm41516d_info *info = dev_get_drvdata(&chg_dev->dev);

	return __sgm41516d_is_chg_enabled(info, en);

}
/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 end*/

static struct charger_ops sgm41516d_chg_ops = {
	.plug_in = sgm41516d_plug_in,
	.plug_out = sgm41516d_plug_out,
	/* Normal charging */
	.set_eoc_current = sgm41516d_set_term_current,
	.dump_registers = sgm41516d_dump_register,
	.enable = sgm41516d_enable_charging,
	.get_charging_current = sgm41516d_get_current,
	.set_charging_current = sgm41516d_set_current,
	.get_input_current = sgm41516d_get_input_current,
	.set_input_current = sgm41516d_set_input_current,
	/*.get_constant_voltage = sgm41516d_get_battery_voreg,*/
	.set_constant_voltage = sgm41516d_set_cv_voltage,
	.kick_wdt = sgm41516d_reset_watch_dog_timer,
	.set_mivr = sgm41516d_set_vindpm_voltage,
	.get_mivr = sgm41516d_get_mivr,
	.get_mivr_state = sgm41516d_get_mivr_state,
	.is_charging_done = sgm41516d_get_charging_status,

	/* Safety timer */
	.enable_safety_timer = sgm41516d_enable_safetytimer,
	.is_safety_timer_enabled = sgm41516d_get_is_safetytimer_enable,
	
	/* PE+20/HVDCP */
	.set_pe20_efficiency_table = sgm41516d_set_pep20_efficiency_table,
	.send_ta20_current_pattern = sgm41516d_set_pep20_current_pattern,
	.reset_ta = sgm41516d_reset_ta,
#if IS_ENABLED(CONFIG_MTK_HVDCP20_SUPPORT)
	.set_dm_0_6_v = sgm41516d_hvdcp20_set_dm_0_6_v,
	.set_dp_0_6_v = sgm41516d_hvdcp20_set_dp_0_6_v,
	.set_dp_3_0_v = sgm41516d_hvdcp20_set_dp_3_0_v,
	.set_dp_dm_recover = sgm41516d_set_dp_dm_recover,
#endif

#if IS_ENABLED(CONFIG_MTK_TC30_SUPPORT)
	.set_tc30_gpio_enable = sgm41516d_tc30_enable,
#endif

#if IS_ENABLED(CONFIG_TRAN_CHARGER_OTG_LPO_SUPPORT)
	.get_boost_status = sgm41516d_get_boost_status,
#endif
//BSP:CHG OTG high current reverse charger CLBLELEPUB-279 by minglong.li 20220308 start
#if IS_ENABLED(CONFIG_TRAN_CHARGER_REVERSE)
	.set_pfm_mode = sgm41516d_pfm_enable,
#endif
//BSP:CHG OTG high current reverse charger CLBLELEPUB-279 by minglong.li 20220308 end
	.enable_hz = sgm41516d_enable_hiz_mode,
//	.get_ext_chgtyp = mtk_ext_chgdet,

/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 start*/
	.is_enabled =  sgm41516d_is_charging_enabled,
/*BSP:modify for Fix frequent pop ups by jianqiang.ouyang 20220718 end*/

	/* OTG */
	.enable_otg = sgm41516d_enable_otg,
	.set_boost_current_limit = sgm41516d_set_boost_current_limit,
	.event = sgm41516d_do_event,
};


static int sgm41516d_hw_component_detect(struct sgm41516d_info *info)
{
	unsigned char vendor_id = 0;
	vendor_id = sgm41516d_get_dev_id(info);
	pr_info("sgm41516d vendor_id=%d!!!\r\n",vendor_id);
	switch(vendor_id){
		case bq25600d_VENDOR_ID:
			g_tran_chg_info=BQ25601D;
			break;
		case bq25601_VENDOR_ID:
			g_tran_chg_info=BQ25601;
			break;
		case SGM41516_VENDOR_ID:
			g_tran_chg_info=SGM41516;
			break;
		case SGM41516D_VENDOR_ID:
			g_tran_chg_info=SGM41516D;
			break;
		case SGM41516E_VENDOR_ID:
			g_tran_chg_info=SGM41516E;
			break;
		default:
			return false;
	};
	info->vendor_id = vendor_id;
	pr_info("sgm41516d g_tran_chg_info=%d!!!\r\n",g_tran_chg_info);
	return true;
}

static int sgm41516d_init_chg(struct sgm41516d_info *info)
{

	info->chg_dev = charger_device_register(info->chg_dev_name,
						info->dev, info,
						&sgm41516d_chg_ops,
						&info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev))
		return -EPROBE_DEFER;

	return 0;
}

/*regulator otg ops*/
int sgm41516d_enable_regulator_otg(struct regulator_dev *rdev)
{
	struct sgm41516d_info *info = rdev_get_drvdata(rdev);

	dev_info(info->dev, "%s\n", __func__);
	/* otg current to 1.2A */
	if (info->vendor_id == SGM41516E_VENDOR_ID) {
		sgm41516e_set_boost_lim(info, 0x00);
	} else {
		sgm41516d_set_boost_lim(info, 0x01);
	}
	return __sgm41516d_enable_otg(info, true);
}

int sgm41516d_disable_regulator_otg(struct regulator_dev *rdev)
{
	struct sgm41516d_info *info = rdev_get_drvdata(rdev);

	dev_info(info->dev, "%s\n", __func__);
	return __sgm41516d_enable_otg(info, false);
}

static int sgm41516d_set_current_limit(struct regulator_dev *rdev,
						int min_uA, int max_uA)
{
	struct sgm41516d_info *info = rdev_get_drvdata(rdev);
	int num = ARRAY_SIZE(sgm41516d_otgcc);
	int boost_cur_set = bmt_find_closest_level(sgm41516d_otgcc,num,min_uA);

	return __sgm41516d_set_otgcc(info, boost_cur_set);
}

static int sgm41516d_get_current_limit(struct regulator_dev *rdev)
{
	int ret = 0;
	int val = 0;
	struct sgm41516d_info *info = rdev_get_drvdata(rdev);

	val = sgm41516d_get_boost_lim(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read otg_cc fail\n", __func__);
		return ret;
	}

	ret = val ? sgm41516d_otgcc[1] : sgm41516d_otgcc[0];
	return val;

}

//BSP:modify for otg detect by lei.shi5 20220501 start
static int sgm41516d_otg_is_enabled(struct regulator_dev *rdev)
{
	struct sgm41516d_info *info = rdev_get_drvdata(rdev);
	return info->sgm41516d_otg_enable_flag;
}

static const struct regulator_ops sgm41516d_chg_otg_ops = {
	.enable = sgm41516d_enable_regulator_otg,
	.disable = sgm41516d_disable_regulator_otg,
	.is_enabled = sgm41516d_otg_is_enabled,
	.set_current_limit = sgm41516d_set_current_limit,
	.get_current_limit = sgm41516d_get_current_limit,
};

static const struct regulator_desc sgm41516d_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sgm41516d_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5150000,
	.n_voltages = 1,
};

static const struct regulator_init_data sgm41516d_vbus_init_data = {
	.constraints = {
	.valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,
	.min_uA = 500000,
	.max_uA = 1200000,
	},
};

static const struct regulator_init_data sgm41516e_vbus_init_data = {
	.constraints = {
	.valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,
	.min_uA = 500000,
	.max_uA = 2000000,
	},
};

static int sgm41516d_init_regulator(struct sgm41516d_info *info)
{
	struct regulator_config config = { };

	dev_info(info->dev, "%s\n", __func__);

	config.dev = info->dev;
	config.driver_data = info;
	if (info->vendor_id == SGM41516E_VENDOR_ID) {
		config.init_data = &sgm41516e_vbus_init_data;
	}else {
		config.init_data = &sgm41516d_vbus_init_data;
	}
	info->otg_rdev = devm_regulator_register(info->dev, &sgm41516d_otg_rdesc,
						&config);
	if(info->otg_rdev != NULL)
		dev_info(info->dev, "%s succesfully!\n", __func__);
	return IS_ERR_OR_NULL(info->otg_rdev);
}
//BSP:modify for otg detect by lei.shi5 20220501 end
/* ======================= */
/* sgm41516d Power Supply Ops */
/* ======================= */

static int sgm41516d_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sgm41516d_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	dev_dbg(info->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = sgm41516d_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = atomic_read(&info->attach);
		dev_notice(info->dev, "%s: online = %d\n", __func__,val->intval);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = info->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		sgm41516d_get_ic_stat(info,&(val->intval));
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm41516d_get_current(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm41516d_get_cv_voltage(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm41516d_get_input_current(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm41516d_get_mivr(info->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sgm41516d_get_term_current(info->chg_dev, &(val->intval));
		break;
	default:
		ret = -ENODATA;
		break;
	}
	return ret;
}

static int sgm41516d_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct sgm41516d_info *info = power_supply_get_drvdata(psy);
	int ret;

	dev_dbg(info->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:

		ret = sgm41516d_chg_attach_process(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm41516d_set_current(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm41516d_set_cv_voltage(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm41516d_set_input_current(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm41516d_set_vindpm_voltage(info->chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sgm41516d_set_term_current(info->chg_dev, val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sgm41516d_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property sgm41516d_charger_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_usb_type sgm41516d_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const struct power_supply_desc sgm41516d_charger_desc = {
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sgm41516d_charger_properties,
	.num_properties		= ARRAY_SIZE(sgm41516d_charger_properties),
	.get_property		= sgm41516d_charger_get_property,
	.set_property		= sgm41516d_charger_set_property,
	.property_is_writeable	= sgm41516d_charger_property_is_writeable,
	.usb_types		= sgm41516d_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm41516d_charger_usb_types),
};

static char *sgm41516d_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};


static int sgm41516d_init_psy(struct sgm41516d_info *info)
{
	struct power_supply_config charger_cfg = {};

	dev_info(info->dev, "%s\n", __func__);

	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	/* power supply register */
	memcpy(&info->psy_desc,
		&sgm41516d_charger_desc, sizeof(info->psy_desc));
	info->psy_desc.name = "charger"; //dev_name(info->dev);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = info->dev->of_node;
	charger_cfg.supplied_to = sgm41516d_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(sgm41516d_charger_supplied_to);
	info->psy = devm_power_supply_register(info->dev,
					&info->psy_desc, &charger_cfg);
	return IS_ERR_OR_NULL(info->psy);
}

static int sgm41516d_driver_probe(struct i2c_client *client)
{
	int ret = 0;
	struct sgm41516d_info *info = NULL;

	pr_info("[%s]\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct sgm41516d_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &client->dev;
	info->client = client;
	i2c_set_clientdata(client, info);
	mutex_init(&info->sgm41516d_i2c_access);
	mutex_init(&info->sgm41516d_access_lock);
	mutex_init(&info->attach_lock);
	atomic_set(&info->attach, 0);

	INIT_WORK(&info->bc12_work, sgm41516d_bc12_detect);
	ret = sgm41516d_parse_dt(info, &client->dev);
	if (ret < 0)
		goto err_parse_dt;

	if(!sgm41516d_hw_component_detect(info)){
		pr_info("sgm41516d component not exist!!!\r\n");
		ret = -ENODEV;
		goto err_parse_dt;
	}

	ret = charging_hw_init(info);
	if (ret < 0)
		goto err_parse_dt;

	/* Register charger device */
	ret = sgm41516d_init_chg(info);
	if (ret) {
		ret = -EPROBE_DEFER;
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		goto err_register_chg_dev;
	}
	ret = device_create_file(&client->dev, &dev_attr_sgm41516d_access);
	if (ret < 0) {
		pr_info( "%s create file fail(%d)\n",
				      __func__, ret);
		goto err_create_file;
	}

	ret = sgm41516d_init_regulator(info);
	if (ret) {
		ret = PTR_ERR(info->otg_rdev);
		dev_notice(info->dev, "%s regulator register fail\n", __func__);
		goto err_regulator_dev;
	}

	ret = sgm41516d_init_psy(info);
	if (ret) {
		ret = PTR_ERR(info->psy);
		dev_notice(info->dev,
			"Fail to register power supply dev, is NULL = %d\n",
							(info->psy == NULL));
		goto err_register_psy;
	}

	sgm41516d_dump_register(info->chg_dev);

	return 0;
err_register_psy:
	if (!IS_ERR_OR_NULL(info->psy))
		power_supply_put(info->psy);
err_regulator_dev:
err_create_file:
	device_remove_file(&client->dev, &dev_attr_sgm41516d_access);
err_register_chg_dev:
	charger_device_unregister(info->chg_dev);
err_parse_dt:
	mutex_destroy(&info->attach_lock);
	mutex_destroy(&info->sgm41516d_i2c_access);
	mutex_destroy(&info->sgm41516d_access_lock);
	devm_kfree(info->dev, info);
	return ret;
}



static void sgm41516d_remove(struct i2c_client *client)
{
	struct sgm41516d_info *info = i2c_get_clientdata(client);

	dev_info(info->dev, "%s\n", __func__);

	if (info->psy)
		power_supply_put(info->psy);	
	device_remove_file(&client->dev, &dev_attr_sgm41516d_access);
	charger_device_unregister(info->chg_dev);

	mutex_destroy(&info->sgm41516d_i2c_access);
	mutex_destroy(&info->sgm41516d_access_lock);
}

static void sgm41516d_shutdown(struct i2c_client *client)
{
	struct sgm41516d_info *info = i2c_get_clientdata(client);

	if(!info)
		return;

	sgm41516d_set_reg_rst(info, 0x1);
/*BSP:modify for fixed kpoc_charger LGQNHBJE-2911 by zhenglong.hong 20221205 start*/
	atomic_set(&info->attach, 0);
/*BSP:modify for fixed kpoc_charger LGQNHBJE-2911 by zhenglong.hong 20221205 end*/

	dev_info(info->dev, "%s end\n", __func__);
/*BSP:modify for fixed kernel down by jianqiang.ouyang 20220719 start*/
	i2c_set_clientdata(client, NULL);
/*BSP:modify for fixed kernel down by jianqiang.ouyang 20220719 start*/
}

#ifdef CONFIG_OF
static const struct of_device_id sgm41516d_of_match[] = {
	{.compatible = "mediatek,sgm41516d"},
	{},
};
#else
static struct i2c_board_info i2c_sgm41516d __initdata = {
	I2C_BOARD_INFO("sgm41516d", (sgm41516d_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver sgm41516d_driver = {
	.driver = {
		.name = "sgm41516d",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sgm41516d_of_match,
#endif
	},
	.probe = sgm41516d_driver_probe,
	.remove = sgm41516d_remove,
	.shutdown = sgm41516d_shutdown,
	.id_table = sgm41516d_i2c_id,
};

static int __init sgm41516d_init(void)
{
	//int ret = 0;

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	pr_info("[%s] init start with i2c DTS", __func__);
#else
	pr_info("[%s] init start. ch=%d\n", __func__, sgm41516d_BUSNUM);
	i2c_register_board_info(sgm41516d_BUSNUM, &i2c_sgm41516d, 1);
#endif
	if (i2c_add_driver(&sgm41516d_driver) != 0) {
		pr_info(
			"[%s] failed to register sgm41516d i2c driver.\n",
			__func__);
	} else {
		pr_info(
			"[%s] Success to register sgm41516d i2c driver.\n",
			__func__);
	}


	return 0;
}

static void __exit sgm41516d_exit(void)
{
	i2c_del_driver(&sgm41516d_driver);
}
module_init(sgm41516d_init);
module_exit(sgm41516d_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sgm41516d Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
//BSP:modify for XLJLHLJY-3079 by yongxian.wang  20200325 end
