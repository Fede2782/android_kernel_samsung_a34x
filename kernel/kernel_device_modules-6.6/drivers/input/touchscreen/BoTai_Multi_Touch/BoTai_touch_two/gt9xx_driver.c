/* drivers/input/touchscreen/gt9xx_driver.c
 *
 * 2010 - 2016 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.2
 *      V1.0:2012/08/31,first release.
 *      V1.2:2012/10/15,add force update,GT9110P pid map
 */

#include "tpd2.h"


#include "tpd2_gt9xx_common.h"

#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
#ifdef GTP_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/proc_fs.h>  /*proc*/
#include <linux/sched/types.h>

#include <linux/dma-map-ops.h>
///#include <linux/dma-iommu.h> remove it,because not iommu api.2024-02-01- kernel-61
#include <uapi/linux/sched/types.h> // struct sched_param
#include <uapi/linux/stat.h>
#include <linux/namei.h>

#include <linux/dma-mapping.h>
#include <asm/cache.h>
#include <asm/cacheflush.h>
//#define STATX_ALL		0x00000fffU // struct sched_param

#define UPDATE_IMG_SIZE (1024*1024)
#define UPDATE_IMG_APP_UPGRADE_SIZE (512*1024)
#define UPDATE_IMG_MARK_SIZE 16
#define UPDATE_IMG_APP_VERSION_SIZE 16

#define DELAY_10_MS     10
#define DELAY_100_MS    100
#define DELAY_500_MS    500
#define DELAY_1500_MS   1500
#define DELAY_2000_MS   2000

#define F1_TRANSMIT_DATA    171
#define F2_TRANSMIT_DATA    169
#define F3_TRANSMIT_DATA    169


#define UPGRADE_STOP 0
#define UPGRADE_STARE 1
#define UPGRADE_FAILE 400
#define UPGRADE_SUCCEED 200
#define UPGRADE_SAME_TP_VERSION 300

#define UPGRADE_MCUFW  0
#define UPGRADE_TOUCHFW  1

#define FIR_PART_A  1
#define FIR_PART_B  2

#define APP_MARK_A       0x40
#define APP_MARK_B       0x50
#define TOUCH_MARK  	 0x60
#define TOUCH_CFG_VERSION_MARK  0x40
#define TOUCH_FW_VERSION_MARK  0x50
#define TOUCH_DATA_MARK  0x60

int tpd2_type_cap;
static int tpd2_flag;
static int tpd2_halt;
static int tpd2_eint_mode = 1;
static int tpd2_polling_time = 50;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
unsigned int tpd2_rst_gpio_number;
unsigned int tpd2_int_gpio_number;
static DEFINE_MUTEX(gsMutex_ti941_drv);

static unsigned char dis_screen_info[8] ={-1};
static unsigned char light_intensity_info[8] ={-1};
static unsigned char dis_screen_status[8] ={-1};
static unsigned char screen_products_id[8] ={-1};
static unsigned char screen_version_info[12] ={-1};
static unsigned char que_status[12] ={-1};

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
const u16 touch_key_array[] = tpd2_KEYS;
/* #define GTP_MAX_KEY_NUM ( sizeof( touch_key_array )/sizeof( touch_key_array[0] ) ) */
struct touch_virtual_key_map_t {
	int point_x;
	int point_y;
};
static struct touch_virtual_key_map_t touch_key_point_maping_array[] = GTP_KEY_MAP_ARRAY;
#endif

static int tpd2_keys_local[tpd2_KEY_COUNT] = tpd2_KEYS;
struct FIRMWARE_INFO
{
	//struct file *fp;
    //mm_segment_t oldfs;
    //loff_t pos;
	int file_size;
	int file_upgrade_size;
	char * fw_buf; 
	char * mark_buf; 
	char * ugrade_data_buf;
	char * touch_version_buf;
	int packet_id;
	int packet_count;
	int packet_size;  //size
	///char fw_version[11]; //version hc add
	char fw_version[17]; //version
	char request_data[2]; //request_data
	char fw_partition[8]; //version
	u8 fw_screen_info[7];//
	//u8 fw_key[2];//hc add
	u8 fw_key[3];//hc add
	int slot;
	int upgradetype;
	char upgrade_diagnosis[5];
	int checksum_appa;
	int checksum_appb;
	int checksum_touch;
	int checksum_touch_data;
	int tp_cfg_version;
	int tp_fw_version;
};
static char upgrade_status  = -1;
static int  progress_status =  0;
static int  block_max       = -1;
static int  block_return    = -1;
//static int  seed    	 =-1;
//static int  key    		 =-1;
static struct FIRMWARE_INFO fw_info;
static int MFD_key_backlight=-1;
static int MFD_backlight_temperature_status=-1;
static int MFD_backlight_power_status=-1;
static bool Upgrade_touch_flag = true;


unsigned int touch_irq;
#if (defined(tpd2_WARP_START) && defined(tpd2_WARP_END))
static int tpd2_wb_start_local[tpd2_WARP_CNT] = tpd2_WARP_START;
static int tpd2_wb_end_local[tpd2_WARP_CNT]   = tpd2_WARP_END;
#endif

#if (defined(tpd2_HAVE_CALIBRATION) && !defined(tpd2_CUSTOM_CALIBRATION))
/* static int tpd2_calmat_local[8]     = tpd2_CALIBRATION_MATRIX; */
/* static int tpd2_def_calmat_local[8] = tpd2_CALIBRATION_MATRIX; */
static int tpd2_def_calmat_local_normal[8]  = tpd2_CALIBRATION_MATRIX_ROTATION_NORMAL;
//static int tpd2_def_calmat_local_factory[8] = tpd2_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

static irqreturn_t tpd2_interrupt_handler(int irq, void *dev_id);
static int touch_event_handler(void *unused);
static int tpd2_i2c_probe(struct i2c_client *client);
static int tpd2_i2c_detect(struct i2c_client *clietpd2_CALIBRATION_MATRIX_ROTATION_NORMALnt, struct i2c_board_info *info);
//static int tpd2_i2c_remove(struct i2c_client *client);//hc add
void tpd2_i2c_remove(struct i2c_client *client);
// static void tpd2_on(void);
// static void tpd2_off(void);
int ti941_i2c_dma_write_bytes(struct i2c_client *client,
		uint8_t addr, uint8_t *readbuf, int32_t readlen);
int ti941_i2c_dma_read_bytes(struct i2c_client *client,
		uint8_t addr, uint8_t *readbuf, int32_t readlen);

#ifdef CONFIG_GTP_ESD_PROTECT
#define tpd2_ESD_CHECK_CIRCLE        2000
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
#endif

#ifdef GTP_PROXIMITY
#define tpd2_PROXIMITY_VALID_REG                   0x814E
#define tpd2_PROXIMITY_ENABLE_REG                  0x8042
static u8 tpd2_proximity_flag;
static u8 tpd2_proximity_detect = 1;	/* 0-->close ; 1--> far away */
#endif

#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE		0x8056
#endif

u32 gtp_eint_trigger_type = IRQF_TRIGGER_RISING;
struct i2c_client *i2c_client_point;
static const struct i2c_device_id tpd2_i2c_id[] = { {"ti941_two", 0}, {} };
//static unsigned short force[] = { 0x1a, 0, 0, 0 };
static unsigned short force[] = { 0x1c, 0, 0, 0 };
static const unsigned short *const forces[] = { force, NULL };
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
static const struct of_device_id ti941_dt_match[] = {
	{.compatible = "goodix,cap_touch_two"},
	{},
};

MODULE_DEVICE_TABLE(of, ti941_dt_match);
static struct i2c_driver tpd2_i2c_driver = {
	.driver = {
		   .of_match_table = of_match_ptr(ti941_dt_match),
		   },
	.probe = tpd2_i2c_probe,
	.remove = tpd2_i2c_remove,
	.detect = tpd2_i2c_detect,
	.driver.name = "ti941_two",
	.id_table = tpd2_i2c_id,
	.address_list = (const unsigned short *) forces,
};
#ifdef CONFIG_OF
static int of_get_gt9xx_platform_data(struct device *dev)
{
	/*int ret, num;*/

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(ti941_dt_match), dev);
		if (!match) {
			GTP_ERROR("Error: No device match found\n");
			return -ENODEV;
		}
	}
	//tpd2_rst_gpio_number = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	tpd2_int_gpio_number = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	//tpd2_rst_gpio_number = 2  ;
	//tpd2_int_gpio_number = 1  ;
	/*
	* ret = of_property_read_u32(dev->of_node, "rst-gpio", &num);
	if (!ret)
		tpd2_rst_gpio_number = num;
	ret = of_property_read_u32(dev->of_node, "int-gpio", &num);
	if (!ret)
		tpd2_int_gpio_number = num;
  */
	//GTP_ERROR("g_vproc_en_gpio_number %d\n", tpd2_rst_gpio_number);
	GTP_DEBUG("g_vproc_vsel_gpio_number %d\n", tpd2_int_gpio_number);
	return 0;
}
#else
static int of_get_gt9xx_platform_data(struct device *dev)
{
	return 0;
}
#endif
// static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
// 	= {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

#pragma pack(1)
struct st_tpd2_info {
	u16 pid;		/* product id    */
	u16 vid;		/* version id    */
};
#pragma pack()

struct st_tpd2_info tpd2_info;
u8 int_type;
u32 abs_x_max;
u32 abs_y_max;
u8 gtp_rawdiff_mode;
u8 cfg_len;

/* proc file system */
static struct proc_dir_entry *gt91xx_config_proc;

void print_array(char * data,int len)
{
	char s[1024];
    int offset=0,i=0;
    for(i=0;i<len;i++)
    {
    	offset+=sprintf(s+offset,"%02x,",data[i]);
    }
    s[offset-1]=0;
	pr_info("ti941----frame:%s\n",s);
}

void complete_frame(char * data,int len){
	int i=0;
	int sum=0;
	for(i=0;i<len-1;i++){
		sum+=data[i];
	}
	data[len-1]=(sum)&0xff;
}

/*
******************************************************
Function:
	Write refresh rate

Input:
	rate: refresh rate N (Duration=5+N ms, N=0~15)

Output:
	Executive outcomes.0---succeed.
******************************************************
*/
static u8 gtp_set_refresh_rate(u8 rate)
{
	u8 buf[3] = {GTP_REG_REFRESH_RATE>>8, GTP_REG_REFRESH_RATE & 0xff, rate};

	if (rate > 0xf) {
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return FAIL;
	}

	GTP_INFO("Refresh rate change to %d", rate);
	return ti941_i2c_write(i2c_client_point, buf, sizeof(buf));
}

/*
******************************************************
Function:
	Get refresh rate

Output:
	Refresh rate or error code
******************************************************
*/
static u8 gtp_get_refresh_rate(void)
{
	int ret;

	u8 buf[3] = {GTP_REG_REFRESH_RATE>>8, GTP_REG_REFRESH_RATE & 0xff};

	ret = ti941_i2c_read(i2c_client_point, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	GTP_INFO("Refresh rate is %d", buf[GTP_ADDR_LENGTH]);
	return buf[GTP_ADDR_LENGTH];
}

/* ============================================================= */
static ssize_t show_refresh_rate(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = gtp_get_refresh_rate();

	if (ret < 0)
		return 0;

	return sprintf(buf, "%d\n", ret);
}
static ssize_t store_refresh_rate(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long rate = 0;

	if (kstrtoul(buf, 16, &rate))
		return 0;
	gtp_set_refresh_rate(rate);
	return size;
}
static DEVICE_ATTR(tpd2_refresh_rate, 0664, show_refresh_rate, store_refresh_rate);

static struct device_attribute *gt9xx_attrs[] = {
	&dev_attr_tpd2_refresh_rate,
};
/* ============================================================= */

static int tpd2_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	//strcpy(info->type, "mtk-tpd2");
	strcpy(info->type, "mtk-tpd2-0");
	return 0;
}

#ifdef GTP_PROXIMITY
static s32 tpd2_get_ps_value(void)
{
	return tpd2_proximity_detect;
}

static s32 tpd2_enable_ps(s32 enable)
{
	u8  state;
	s32 ret = -1;

	if (enable) {
		state = 1;
		tpd2_proximity_flag = 1;
		GTP_INFO("tpd2 proximity function to be on.");
	} else {
		state = 0;
		tpd2_proximity_flag = 0;
		GTP_INFO("tpd2 proximity function to be off.");
	}

	ret = ti941_i2c_write_bytes(i2c_client_point, tpd2_PROXIMITY_ENABLE_REG, &state, 1);

	if (ret < 0) {
		GTP_ERROR("tpd2 %s proximity cmd failed.", state ? "enable" : "disable");
		return ret;
	}

	GTP_INFO("tpd2 proximity function %s success.", state ? "enable" : "disable");
	return 0;
}

s32 tpd2_ps_operate(void *self, u32 command, void *buff_in, s32 size_in,
		   void *buff_out, s32 size_out, s32 *actualout)
{
	s32 err = 0;
	s32 value;
	hwm_sensor_data *sensor_data;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Set delay parameter error!");
			err = -EINVAL;
		}

		/* Do nothing */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Enable sensor parameter error!");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			err = tpd2_enable_ps(value);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			GTP_ERROR("Get sensor data parameter error!");
			err = -EINVAL;
		} else {
			sensor_data = (hwm_sensor_data *)buff_out;
			sensor_data->values[0] = tpd2_get_ps_value();
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}

		break;

	default:
		GTP_ERROR("proxmy sensor operate function no this parameter %d!", command);
		err = -1;
		break;
	}

	return err;
}
#endif

static ssize_t gt91xx_config_read_proc(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	// char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {0};
	int len, err = -1;
	u8 point_data[30]={0,};

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page) {
		kfree(page);
		return -ENOMEM;
	}

	ptr = page;
	ptr += sprintf(ptr, "====read ti941====\n");
	ti941_i2c_dma_read_bytes(i2c_client_point,0x1a,point_data,28);
	ptr += sprintf(ptr,"%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x \n",
										point_data[0],point_data[1],point_data[2],point_data[3],point_data[4],
										point_data[5],point_data[6],point_data[7],point_data[8],point_data[9],
										point_data[10],point_data[11],point_data[12],point_data[13],point_data[14],
										point_data[15],point_data[16],point_data[17],point_data[18],point_data[19]);


	len = ptr - page;
	if (*ppos >= len) {
		kfree(page);
		return 0;
	}
	err = copy_to_user(buffer, (char *)page, len);
	*ppos += len;
	if (err) {
		kfree(page);
		return err;
	}
	kfree(page);
	return len;

	/* return (ptr - page); */
}

static ssize_t gt91xx_config_write_proc(struct file *file, const char *buffer, size_t count,
					loff_t *ppos)
{
	s32 ret = 0;
	int i = 0;
	int j=0;
	char temp[25] = {0}; /* for store special format cmd */
	unsigned int mode;
	u8 ahu_startup_done[7] = {0x80,0x1,0x0,0x0,0x0,0x0,0x82};
	// u8 question_cmd[5] = {0x81,0x0,0x49,0x0,0x0};
	u8  version_cmd[20] = {0,};

	GTP_DEBUG("write count %ld\n", (unsigned long)count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("size not match [%d:%ld]", GTP_CONFIG_MAX_LENGTH, (unsigned long)count);
		return -EFAULT;
	}

	/**********************************************/
	/* for store special format cmd  */
	if (copy_from_user(temp, buffer, sizeof(temp))) {
		GTP_ERROR("copy from user fail 2");
		return -EFAULT;
	}
	///ret = sscanf(temp, "%x ", (char *)&mode);//hc add
	ret = sscanf(temp, "%x", &mode);//hc add
	if(mode==1)
	{
		for(j=0;j<5;j++){
			ahu_startup_done[2+i]=0x82;
			// for(i=0;i<1;i++)
			{
				// ahu_startup_done[6]=i;
				ret = ti941_i2c_dma_write_bytes(i2c_client_point,0x00,ahu_startup_done, 3+i);
				if(ret < 0){
					pr_info("ti941 I2C write error. errno:%d ", ret);
				}
			
				// ret = ti941_i2c_write(i2c_client_point,ahu_startup_done, 7);
				// if(ret < 0){
				// 	pr_info("ti941 I2C write error. errno:%d ", ret);
				// }
				msleep(100);
				ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x00,version_cmd,20);
				if(ret < 0){
					pr_info("ti941 I2C write error. errno:%d ", ret);
				}
				
				pr_info("ti941 version_cmd=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x \n",
										version_cmd[0],version_cmd[1],version_cmd[2],version_cmd[3],version_cmd[4],
										version_cmd[5],version_cmd[6],version_cmd[7],version_cmd[8],version_cmd[9],
										version_cmd[10],version_cmd[11],version_cmd[12],version_cmd[13],version_cmd[14],
										version_cmd[15],version_cmd[16],version_cmd[17],version_cmd[18],version_cmd[19]);
				msleep(10);
				if(version_cmd[0]==0x4B)
					break;
			}
			ahu_startup_done[2+i]=0;
			if(version_cmd[0]==0x4B)
					break;
		}
	}

	return count;
}

int ti941_i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buffer[GTP_ADDR_LENGTH];
	u16 left = len;
	u16 offset = 0;
	u8 *data = NULL;
    
	struct i2c_msg msg[2] = {
		{
		 .addr = (client->addr),
			/* .addr = (client->addr &I2C_MASK_FLAG), */
			/* .ext_flag = I2C_ENEXT_FLAG, */
			/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
			.flags = 0,
			.buf = buffer,
			.len = GTP_ADDR_LENGTH,
		},
		{
		 .addr = (client->addr),
			/* .addr = (client->addr &I2C_MASK_FLAG), */
			/* .ext_flag = I2C_ENEXT_FLAG, */
			/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
			.flags = I2C_M_RD,
		},
	};

	if (rxbuf == NULL)
		return -1;

	data =
	    kmalloc(MAX_TRANSACTION_LENGTH <
			   (len + GTP_ADDR_LENGTH) ? MAX_TRANSACTION_LENGTH : (len + GTP_ADDR_LENGTH), GFP_KERNEL);
	if (data == NULL)
		return -1;
	msg[1].buf = data;


	// GTP_DEBUG("ti941_i2c_read_bytes to device %02X address %04X len %d", client->addr, addr, len);

	while (left > 0) {
		// buffer[0] = ((addr + offset) >> 8) & 0xFF;
		buffer[0] = (addr + offset) & 0xFF;

		msg[1].buf = &rxbuf[offset];

		if (left > MAX_TRANSACTION_LENGTH) {
			msg[1].len = MAX_TRANSACTION_LENGTH;
			left -= MAX_TRANSACTION_LENGTH;
			offset += MAX_TRANSACTION_LENGTH;
		} else {
			msg[1].len = left;
			left = 0;
		}

		if (i2c_transfer(client->adapter, &msg[0], 2) != 2) {
			GTP_ERROR("I2C read 0x%X length=%d failed", addr + offset, len);
			kfree(data);
			return -1;
		}
	}

	kfree(data);
	return 0;
}

int ti941_read_byte(struct i2c_client *client,uint8_t reg,int32_t readlen,char *readbuf)
  {

      int      ret = 0;
      struct i2c_msg msg[2];
      struct i2c_adapter *adap = client->adapter;

      msg[0].addr = client->addr;
      msg[0].flags = 0;
      msg[0].len = 1;
      msg[0].buf = &reg;

      msg[1].addr = client->addr;
      msg[1].flags = I2C_M_RD;
      msg[1].len = readlen;
      msg[1].buf = readbuf;

      ret = i2c_transfer(adap, msg, 2);
      if (ret < 0) {
          return ret;
      }
      return 0;

  }

static u8 *gpDMABuf_va=NULL;
static dma_addr_t gpDMABuf_pa;


int ti941_i2c_dma_read_bytes(struct i2c_client *client,
		uint8_t addr, uint8_t *readbuf, int32_t readlen){
	int ret = 0;
	s32 retry = 0;
	u8 buffer[GTP_DMA_MAX_TRANSACTION_LENGTH];
    
    if(client==NULL)
        return -1;
	//pr_info("ti941 dma_read_bytes: client->dev.coherent_dma_mask = DMA_BIT_MASK(32) \n");
	//client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
    if(!gpDMABuf_va){
	gpDMABuf_va = (u8 *) dma_alloc_coherent(&client->dev,
						GTP_DMA_MAX_TRANSACTION_LENGTH, (dma_addr_t *)&gpDMABuf_pa,
						GFP_KERNEL);
	///gpDMABuf_va = (u8 *) dma_alloc_coherent(&client->dev,4096, &gpDMABuf_pa, GFP_KERNEL);
	if(gpDMABuf_va){
		pr_info("ti941 ti941_i2c_dma_read_bytes: gpDMABuf_va ti941 dma buf alloc ok  \n");
		memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
		}
	else {
		//dump_stack();
		pr_info("ti941 dma buf alloc failed\n");
		return 0;
	}
}
	mutex_lock(&gsMutex_ti941_drv);
	pr_info("ti941 22222222 \n");
	buffer[0]=addr;
	if(gpDMABuf_va)
		memcpy(gpDMABuf_va,buffer,GTP_DMA_MAX_TRANSACTION_LENGTH);

	pr_info("ti941 333333 \n");
	if (readbuf == NULL){
		mutex_unlock(&gsMutex_ti941_drv);
		return -1;
	}
	pr_info("ti941 444444 \n");
	for (retry = 0; retry < 5; ++retry) {
		 //ret = i2c_master_send(client,gpDMABuf_va,1);
		 //if (ret < 0)
		 //	continue;
		 //pr_info("ti941 5555555 \n");
		//ret = ti941_read_byte(client,addr,readlen,gpDMABuf_va);
		ret = i2c_master_recv(client,gpDMABuf_va,readlen);
		if (ret < 0)
		{	
			pr_info("ti941 i2c read byte retry[%d]\n",retry);
			continue;
		}
			
		memcpy(readbuf, gpDMABuf_va, readlen);
		pr_info("ti941 666666 \n");
		mutex_unlock(&gsMutex_ti941_drv);
		return 0;
	}
	pr_info("ti941 7777777 \n");
	GTP_ERROR("DMA I2C read error: 0x%04X, %d byte(s), err-code: %d",
			addr, readlen, ret);
	mutex_unlock(&gsMutex_ti941_drv);
	return ret;
}

int ti941_i2c_dma_write_bytes(struct i2c_client *client,
		uint8_t addr, uint8_t *readbuf, int32_t readlen){
	int ret = 0;
	s32 retry = 0;
	// u8 buffer[GTP_DMA_MAX_TRANSACTION_LENGTH];
    
    if(client==NULL)
        return -1;
    if(!gpDMABuf_va){
		 pr_info("ti941 0000000 \n");
	gpDMABuf_va = (u8 *) dma_alloc_coherent(&client->dev,
						GTP_DMA_MAX_TRANSACTION_LENGTH, (dma_addr_t *)&gpDMABuf_pa,
						GFP_KERNEL);
	if(gpDMABuf_va){
		 pr_info("ti941 111111111 \n");
		pr_info("ti941 ti941_i2c_dma_write_bytes: gpDMABuf_va ti941 dma buf alloc ok  \n");
		memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
		}
	else {
		//dump_stack();
		pr_info("ti941 dma buf alloc failed\n");
		return 0;
	}
	}
	mutex_lock(&gsMutex_ti941_drv);
	 pr_info("ti941 22222222 \n");
	if(gpDMABuf_va)
	{
		memcpy(gpDMABuf_va,readbuf,readlen);
	}
		
	pr_info("ti941 send=%x,%x,%x,%x,%x,%x,%x,%x \n",gpDMABuf_va[0],gpDMABuf_va[1],gpDMABuf_va[2],gpDMABuf_va[3]
	 												,gpDMABuf_va[4],gpDMABuf_va[5],gpDMABuf_va[6],gpDMABuf_va[7]);
	pr_info("ti941 333333 \n");
	if (readbuf == NULL)
	{
		mutex_unlock(&gsMutex_ti941_drv);
		return -1;
	}
	pr_info("ti941 444444 \n");
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_master_send(client,gpDMABuf_va,readlen);
		if (ret < 0)
			continue;
		 pr_info("ti941 send len %d\n",ret);
		mutex_unlock(&gsMutex_ti941_drv);
		return 0;
	}
	pr_info("ti941 7777777 \n");
	GTP_ERROR("DMA I2C write error: 0x%04X, %d byte(s), err-code: %d",
			addr, readlen, ret);
	mutex_unlock(&gsMutex_ti941_drv);
	return ret;
}

s32 ti941_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = buf[0];
    
	ret = ti941_i2c_read_bytes(client, addr, &buf[1],len);

	if (!ret)
		return 1;

	return ret;
}

int ti941_i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
	u8 buffer[MAX_TRANSACTION_LENGTH];
	u16 left = len;
	u16 offset = 0;
    
    
	struct i2c_msg msg = {
		.addr = (client->addr),
		/* .addr = (client->addr &I2C_MASK_FLAG), */
		/* .ext_flag = I2C_ENEXT_FLAG, */
		/* .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)), */
		.flags = 0,
		.buf = buffer,
	};
    
	if (txbuf == NULL)
		return -1;

	// GTP_DEBUG("ti941_i2c_write_bytes to device %02X address %04X len %d", client->addr, addr, len);

	while (left > 0) {
		// buffer[0] = ((addr + offset) >> 8) & 0xFF;
		buffer[0] = (addr + offset) & 0xFF;

		if (left > MAX_I2C_TRANSFER_SIZE) {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], MAX_I2C_TRANSFER_SIZE);
			msg.len = MAX_TRANSACTION_LENGTH;
			left -= MAX_I2C_TRANSFER_SIZE;
			offset += MAX_I2C_TRANSFER_SIZE;
		} else {
			memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], left);
			msg.len = left + GTP_ADDR_LENGTH;
			left = 0;
		}

		/* GTP_DEBUG("byte left %d offset %d", left, offset); */

		if (i2c_transfer(client->adapter, &msg, 1) != 1) {
			GTP_ERROR("I2C write 0x%X%X length=%d failed", buffer[0], buffer[1], len);
	    return -1;
		}
	}

	return 0;
}

s32 ti941_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = buf[0];

    if(client==NULL)
        return -1;
    
    ret = ti941_i2c_write_bytes(client, addr, &buf[1], len - 1);
	if (!ret)
		return 1;

	return ret;
}



/*
******************************************************
Function:
	Send config Function.

Input:
	client:	i2c client.

Output:
	Executive outcomes.0--success,non-0--fail.
******************************************************
*/
s32 ti941_send_cfg(struct i2c_client *client)
{
	s32 ret = 0;
	return ret;
}

/*
******************************************************
Function:
	Read goodix touchscreen version function.

Input:
	client:	i2c client struct.
	version:address to store version info

Output:
	Executive outcomes.0---succeed.
******************************************************
*/
// s32 ti941_read_version(struct i2c_client *client, u16 *version)
// {
// 	s32 ret = -1;
// 	s32 i;
// 	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

// 	GTP_DEBUG_FUNC();

// 	ret = ti941_i2c_read(client, buf, sizeof(buf));

// 	if (ret < 0) {
// 		GTP_ERROR("GTP read version failed");
// 		return ret;
// 	}

// 	if (version)
// 		*version = (buf[7] << 8) | buf[6];

// 	tpd2_info.vid = *version;
// 	tpd2_info.pid = 0x00;

// 	/* for gt9xx series */
// 	for (i = 0; i < 3; i++) {
// 		if (buf[i + 2] < 0x30)
// 			break;

// 		tpd2_info.pid |= ((buf[i + 2] - 0x30) << ((2 - i) * 4));
// 	}

// 	GTP_INFO("IC VERSION:%c%c%c_%02x%02x",
// 		buf[2], buf[3], buf[4], buf[7], buf[6]);

// 	return ret;
// }
/*
******************************************************
Function:
	GTP initialize function.

Input:
	client:	i2c client private struct.

Output:
	Executive outcomes.0---succeed.
******************************************************
*/
static s32 gtp_init_panel(struct i2c_client *client)
{
	s32 ret = 0;

	return ret;
}

static s8 ti941_i2c_test(struct i2c_client *client)
{
	u8 retry = 0;
	s8 ret = -1;
	u8 point_data[30]={0,};

	GTP_DEBUG_FUNC();
	pr_info("[%s-%s-%d ] ti941_i2c_test", __FILE__, __func__, __LINE__);
//while (retry++ < 10000) {
	while (retry++ < 5) {
		// ret = ti941_i2c_read_bytes(client, 0x0, (u8 *)&hw_info, sizeof(hw_info));

		ret = ti941_i2c_dma_read_bytes(client,0x1a,point_data,30);
		// pr_info("ti941 point_cmd=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x \n",
		// 	point_data[0],point_data[1],point_data[2],point_data[3],point_data[4],
		// 	point_data[5],point_data[6],point_data[7],point_data[8],point_data[9],
		// 	point_data[10],point_data[11],point_data[12],point_data[13],point_data[14],
		// 	point_data[15],point_data[16],point_data[17],point_data[18],point_data[19],
		// 	point_data[20],point_data[21],point_data[22],point_data[23],point_data[24]);

		if ((ret>=0))              /* 20121212 */
			return ret;

		// GTP_ERROR("TI941_REG_HW_INFO : %08X", hw_info);
		GTP_ERROR("TI941 i2c test failed time %d.", retry);
		msleep(20);
	}

	return 1;
}

/*
******************************************************
Function:
	Set INT pin  as input for FW sync.

Note:
  If the INT is high, It means there is pull up resistor attached on the INT pin.
  Pull low the INT pin manaully for FW sync.
******************************************************
*/
void gtp_int_sync(void)
{
	GTP_DEBUG("There is pull up resisitor attached on the INT pin~!");
	tpd2_gpio_output(GTP_INT_PORT, 0);
	//gpio_direction_output(tpd2_int_gpio_number, 0);
	msleep(50);
	tpd2_gpio_as_int(GTP_INT_PORT);
	//gpio_direction_input(tpd2_int_gpio_number);
}



static int tpd2_power_on(struct i2c_client *client)
{
	int ret = 0;
	int reset_count = 0;

reset_proc:
	// tpd2_gpio_output(GTP_INT_PORT, 0);
	//gpio_direction_output(tpd2_int_gpio_number, 0);
	// tpd2_gpio_output(GTP_RST_PORT, 0);
	//gpio_direction_output(tpd2_rst_gpio_number, 0);
	// msleep(20);
	/* power on, need confirm with SA */
	// GTP_ERROR("turn on power reg-vgp6\n");
	// ret = regulator_enable(tpd2->reg);
	// if (ret != 0)
	// 	tpd2_DMESG("Failed to enable reg-vgp6: %d\n", ret);

	GTP_ERROR("tpd2_int_gpio_number:0x%x, tpd2_rst_gpio_number:0x%x", tpd2_int_gpio_number, tpd2_rst_gpio_number);
    if(0)
    {
        ret = ti941_i2c_test(client);

        if (ret < 0) {
            GTP_ERROR("I2C communication ERROR!");

            if (reset_count < tpd2_MAX_RESET_COUNT) {
                reset_count++;
                goto reset_proc;
            } else {
                goto out;
            }
        }
    }

#ifdef CONFIG_GTP_FW_DOWNLOAD
	ret = gup_init_fw_proc(client);

	if (ret < 0)
		GTP_ERROR("Create fw download thread error.");

#endif
out:
	return ret;
}

/*
static const struct file_operations gt_upgrade_proc_fops = {
	.write = gt91xx_config_write_proc,
	.read = gt91xx_config_read_proc
};
*/
static const struct proc_ops gt_upgrade_proc_fops = {
	.proc_write = gt91xx_config_write_proc,
	.proc_read = gt91xx_config_read_proc
};

int get_upgrade_status(void)
{
    return upgrade_status;
}
///EXPORT_SYMBOL(get_upgrade_status);
int get_update_block_max(void)
{
    return block_max;
}
//EXPORT_SYMBOL(get_update_block_max);

int get_screen_active_partition(void)
{	
	int partition = -1;
	int	ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x13,fw_info.fw_screen_info,8);
	if (ret < 0) {
		GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
		return -1;
	}
	print_array(fw_info.fw_screen_info,8);
	msleep(100);
	partition = fw_info.fw_screen_info[2];
    return partition;
}

int get_screen_info(void)
{	
	int	ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x13,fw_info.fw_screen_info,8);
	if (ret < 0) {
		GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
		return -1;
	}
	print_array(fw_info.fw_screen_info,8);
	msleep(100);
	MFD_backlight_temperature_status=fw_info.fw_screen_info[1];
	MFD_key_backlight=fw_info.fw_screen_info[3];
	MFD_backlight_power_status=fw_info.fw_screen_info[5];  
    return 0;
}

int get_screen_version(void)
{	
	int	ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x15,fw_info.fw_version,13);
	if (ret < 0) {
		GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
		return -1;
	}
	print_array(fw_info.fw_version,13);
    return 0;
}

uint8_t *hex_to_ascii(uint8_t *str, uint32_t len)
{	
	uint8_t * data = NULL;
    uint8_t *hex_buf = str;
	int i;
    for ( i = 0; i < len; i++)
    {
        sprintf(&data[i * 2], "%02X", hex_buf[i]);
    }
	pr_info("data = %s",data);
    return (uint8_t *)data;
}
int upgrade_free_memory(void)
{	
#if 0
	vfree(fw_info.fw_buf);
	fw_info.fw_buf = NULL;
	vfree(fw_info.mark_buf);
	fw_info.mark_buf = NULL;
	vfree(fw_info.ugrade_data_buf);
	fw_info.ugrade_data_buf = NULL;
	vfree(fw_info.touch_version_buf);
	fw_info.touch_version_buf = NULL;
#endif
	return 0;
}

int upgrade_complete_frame(char * data,int len){
	int i=0;
	int sum=0;
	for(i=0;i<len-1;i++){
		sum+=data[i];
	}
	return sum;
}

int upgrade_send_instructions(char *cmd,uint8_t addr,int len)
{	
	
	int ret = -1;
	char *upgrade = NULL;
	upgrade = cmd ;
	//pr_info("upgrade_send_instructions");
	complete_frame(upgrade,len);
    print_array(upgrade,len);

    ret=ti941_i2c_dma_write_bytes(i2c_client_point,addr,upgrade,len);
    if(ret < 0){
        pr_info("write ti941 update extend mod error. errno:%d ", ret);
		return -1;
    }
	return 0;
}

//reset
int upgrade_reset(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_reset[12] = {0xf0,0x3c,0x28,0x02,0x11,0x01,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_reset[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_reset[5];
	ret = upgrade_send_instructions(upgrade_reset,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_reset errno:%d ", ret);
		return -1;
	}
	msleep(100);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_reset upgrade_nrc =%d",upgrade_nrc);
	return 0;
}

int upgrade_reboot(char* path)
{	
	int ret;
	if(strcmp(path, "reboot") !=0 )
	{
		return -1;
	}
	ret = upgrade_reset();
	if (ret < 0) {
		pr_info("ti941 upgrade_reset faile:%d ", ret);
		return -1;
	}
	progress_status = UPGRADE_STOP;
	//msleep(1000);
	//fw_info.slot = get_screen_active_partition();
	//pr_info("slot = %d",fw_info.slot);
	return 0;
}

int update_return_judgment(int nrc,int cmd)
{	
	int retry  = 0;
	for(retry = 0;retry < 30;retry++)
	{	
		if(retry == 29)
		{
			progress_status = UPGRADE_FAILE;
			pr_info("ti941 update_return_judgment nrc:%x,cmd:%x,retry:%d ", nrc,cmd,retry);
			upgrade_free_memory();
			return -1;
		}
		nrc = get_upgrade_status();
		if(nrc == 0x7f || nrc !=((cmd+0x40)&0xff))
		{	
			pr_info("ti941 update_return_judgment retry:%d ",retry);
			msleep(DELAY_100_MS);
			continue;
		}else{
			break;
		}

	}
	return 0;
}




//Upgrade into extended mode
int update_into_extend_mod(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char update_into_extend_mod[12] = {0xf0,0x3c,0x28,0x02,0x10,0x03,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = update_into_extend_mod[4];
	fw_info.upgrade_diagnosis[1] =	update_into_extend_mod[5];
	progress_status = UPGRADE_STARE;
	ret = upgrade_send_instructions(update_into_extend_mod,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 update_into_extend_mod errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 update_into_extend_mod upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x10);
	if (ret < 0) {
		pr_info("ti941 update_into_extend_mod errno:%d ", ret);
		return -1;
	}
	return 0;
}
//Pre-upgrade condition check
int upgrade_check_before(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_check_before[12] = {0xf0,0x3c,0x28,0x04,0x31,0x01,0x02,0x03,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_check_before[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_check_before[5];
	ret = upgrade_send_instructions(upgrade_check_before,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_check_before errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
#if 1
	pr_info("ti941 upgrade_check_before upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x31);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
#endif
	return 0;
}

//Pre-upgrade condition check
int upgrade_disable_fault_codes(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_disable_fault_codes[12] = {0xf0,0x3c,0x28,0x02,0x85,0x02,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_disable_fault_codes[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_disable_fault_codes[5];
	ret = upgrade_send_instructions(upgrade_disable_fault_codes,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_disable_fault_codes errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_disable_fault_codes upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x85);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

//upgrade_disable_non_refresh
int upgrade_disable_non_refresh(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_disable_non_refresh[12]  = {0xf0,0x3c,0x28,0x03,0x28,0x03,0x03,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_disable_non_refresh[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_disable_non_refresh[5];
	ret = upgrade_send_instructions(upgrade_disable_non_refresh,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_disable_non_refresh errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_disable_non_refresh upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x28);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}
//upgrade_enter_prog_session
int upgrade_enter_prog_session(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_enter_prog_session[12]     = {0xf0,0x3c,0x28,0x02,0x10,0x02,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_enter_prog_session[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_enter_prog_session[5];
	if(fw_info.upgradetype ==UPGRADE_TOUCHFW)
	{
		upgrade_enter_prog_session[5] = 0x04;
	}
	ret = upgrade_send_instructions(upgrade_enter_prog_session,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_enter_prog_session errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	if(fw_info.upgradetype ==UPGRADE_TOUCHFW)
	{
		msleep(DELAY_1500_MS);
	}
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_enter_prog_session upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x10);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

//upgrade_security_verification1
int upgrade_security_verification1(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_security_verification1[12]  = {0xf0,0x3c,0x28,0x02,0x27,0x01,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_security_verification1[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_security_verification1[5];
	ret = upgrade_send_instructions(upgrade_security_verification1,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_security_verification1 errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_security_verification1 upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x27);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}


#if 0
static u8 ChkSecurityKey(u8* Seedarray)
{
	#define TOPBIT 0x8000
	#define POLYNOM_1 0x8408
	#define POLYNOM_2 0x8025
	#define BITMASK 0x0080
	#define INITIAL_REMINDER 0xFFFE
	#define MSG_LEN 2 /* seed length in bytes */
	u8 bSeed[2];
	u16 remainder;
	u8 n;
	u8 i;
	bSeed[0] = Seedarray[0]; /* MSB */
	bSeed[1] = Seedarray[1]; /* LSB */
	remainder = INITIAL_REMINDER;
	for (n = 0; n < MSG_LEN; n++)
	{
		/* Bring the next byte into the remainder. */
		remainder ^= ((bSeed[n]) << 8);
		/* Perform modulo-2 division, a bit at a time. */
		for (i = 0; i < 8; i++)
		{
			/* Try to divide the current data bit. */
			if (remainder & TOPBIT)
			{
				if(remainder & BITMASK)
				{
				remainder = (remainder << 1) ^ POLYNOM_1;
				}
				else
				{
				remainder = (remainder << 1) ^ POLYNOM_2;
				}
			}
			else
			{
				remainder = (remainder << 1);
			}
		}
	}

	fw_info.fw_key[0] = remainder>>8;
	fw_info.fw_key[1] = remainder;
    
    return 0;
}
#endif

//upgrade_security_verification2
int upgrade_security_verification2(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
	char upgrade_security_verification2[12]  = {0xf0,0x3c,0x28,0x05,0x27,0x02,0xaa,0xaa,0xaa,0xaa,0x0,0x0};
	//pr_info("fw_key0 = %x,fw_key1=%x",fw_info.fw_key[0],fw_info.fw_key[1]);
	//ChkSecurityKey(fw_info.fw_key);
	//upgrade_security_verification2[6]        = fw_info.fw_key[0];
	//upgrade_security_verification2[7]        = fw_info.fw_key[1];
	//pr_info("fw_key0 = %x,fw_key1=%x",fw_info.fw_key[0],fw_info.fw_key[1]);
	ret = upgrade_send_instructions(upgrade_security_verification2,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_security_verification2 errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_security_verification2 upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x27);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

//1upgrade_record_fingerprint
int upgrade_record_fingerprint(void)
{
	int ret = -1;
	//int upgrade_nrc = -1;
    char upgrade_record_fingerprint[12] = {0xf0,0x3c,0x28,0x10,0x0a,0x2e,0xf1,0x83,0x20,0x22,0xb,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_record_fingerprint[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_record_fingerprint[5];
	ret = upgrade_send_instructions(upgrade_record_fingerprint,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_record_fingerprint errno:%d ", ret);
		return -1;
	}
	msleep(100);
#if 0
	
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_record_fingerprint upgrade_nrc =%d",upgrade_nrc);
	if(upgrade_nrc ==0x12 || upgrade_nrc ==0x13||upgrade_nrc ==0x22||upgrade_nrc ==0x33||upgrade_nrc ==0x72)
	{
		pr_info("ti941 upgrade_record_fingerprint upgrade_nrc =:0x%x ", upgrade_nrc);
		return -1;
	}
#endif
	return 0;
}

//upgrade_record_fingerprint_extend
int upgrade_record_fingerprint_extend(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_record_fingerprint_extend[11]= {0xf1,0x3c,0x28,0x21,0x01,0x01,0x0,0x0,0x0,0x0,0x0};
	ret = upgrade_send_instructions(upgrade_record_fingerprint_extend,0xF1,11);
	if (ret < 0) {
		pr_info("ti941 upgrade_record_fingerprint_extend errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_record_fingerprint_extend upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x2e);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}


//upgrade_request_download
int upgrade_request_download(void)
{
	int ret = -1;
	//int upgrade_nrc = -1;
    char upgrade_request_download[12] = {0xf0,0x3c,0x28,0x10,0x0b,0x34,0x0,0x44,0x0,0x0,0xc,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_request_download[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_request_download[5];
	ret = upgrade_send_instructions(upgrade_request_download,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_request_download errno:%d ", ret);
		return -1;
	}
#if 0
	msleep(100);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_request_download upgrade_nrc =%d",upgrade_nrc);
	if(upgrade_nrc ==0x12 || upgrade_nrc ==0x13||upgrade_nrc ==0x22||upgrade_nrc ==0x33||upgrade_nrc ==0x70)
	{
		pr_info("ti941 upgrade_request_download upgrade_nrc =:0x%x ", upgrade_nrc);
		return -1;
	}
#endif
	return 0;
}

//upgrade_request_download_extend
int upgrade_request_download_extend(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_request_download_extend[12] = {0xf1,0x3c,0x28,0x21,0x30,0x0,0x0,0x0,0x50,0x0,0x0,0x0};

	ret = upgrade_send_instructions(upgrade_request_download_extend,0xF1,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_request_download_extend errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_request_download_extend upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x34);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

#if 1
//upgrade transfer data
int upgrade_transfer_data(char *data,int lenght)
{	
	int ret             = -1;
	int block_sum       =  1;
	int i               =  1;
	int j               =  0;
	int size            =  0;
	int ext_lenght      =  0;
	int sum             =  0;
	int retry           =  0;
	int upgrade_nrc     =  0;
	bool data_integrity = false;
 	u8 upgrade_data_f0[12] = {0xf0,0x3c,0x28,0x12,0x02,0x36,0x0,0x0,0x0,0x0,0xB1,0x0};
	u8 upgrade_data_f1[F1_TRANSMIT_DATA+6] = {0xf1,0x3c,0x28,0x21,0x0,};
	u8 upgrade_data_f2[F2_TRANSMIT_DATA+6] = {0xf2,0x3c,0x28,0x22,0x0,};
	u8 upgrade_data_f3[F3_TRANSMIT_DATA+6] = {0xf3,0x3c,0x28,0x23,0x0,};
	fw_info.upgrade_diagnosis[0] = upgrade_data_f0[4];
	fw_info.upgrade_diagnosis[1] = upgrade_data_f0[5];
	upgrade_data_f1[F1_TRANSMIT_DATA+4] =F2_TRANSMIT_DATA+6;
	upgrade_data_f1[F1_TRANSMIT_DATA+5] =0x0;
	upgrade_data_f2[F2_TRANSMIT_DATA+4] =F3_TRANSMIT_DATA+6;
	upgrade_data_f2[F2_TRANSMIT_DATA+5] =0x0;
	upgrade_data_f3[F3_TRANSMIT_DATA+4] =0x0;
	upgrade_data_f3[F3_TRANSMIT_DATA+5] =0x0;
	block_sum = lenght/512;
	if(data == NULL)
	{	
		pr_info("upgrade_transfer_data data is null");
		return -1;
	}
	if(lenght%256 != 0)
	{
		block_sum +=1;
		data_integrity = true;
	}
	pr_info("lenght =%d block_sum =%d",lenght,block_sum);
	for(i=1;i<= block_sum;i++)//for(i;i<= block_sum;i++)
	{	
		upgrade_data_f0[6] = i;
		pr_info("block_sum =%d",i);
		if(i == block_sum && data_integrity) //last frame
		{
			size = lenght - ((i-1)*171);
			pr_info("last frame size =%d",size);
			if( 128 < size) //may only F2
			{
				size +=5;
				pr_info("size =%d",size);
				upgrade_data_f0[3] = 0x10;
				upgrade_data_f0[4] = size;
				
			}
			else{    //only F1
				ext_lenght =size+6;
				size +=2;
				pr_info("size  =%d",size);
				pr_info("ext_lenght =%d",ext_lenght);
				upgrade_data_f0[3] = 16;
				upgrade_data_f0[4] = size;
				upgrade_data_f0[10] = ext_lenght-3;
			}
		}
		for(j = 0;j<3;j++) 
		{
			upgrade_data_f0[j+7] =data[j+((i-1)*512)];
		}
		ret = upgrade_send_instructions(upgrade_data_f0,0xF0,12);
		if (ret < 0) {
			pr_info("ti941 upgrade_data_f0 errno:%d ", ret);
			return -1;
		}
		//f1 data frame
		for(j = 0;j<F1_TRANSMIT_DATA;j++) 
		{	
			if(j+((i-1)*512) <= lenght)
			{	
				sum = j+((i-1)*512+3);
				upgrade_data_f1[j+4] =data[sum];
			}
			else
			{	
				upgrade_data_f1[j+4] =0x0;
			}		
		}
		msleep(DELAY_100_MS);
		if(i == block_sum && data_integrity) //last frame
		{
			size = lenght - ((i-1)*512)-175;
			if( 0 < size ) //may only F2
			{
				size +=6;
				pr_info("size0=%d",size);
				upgrade_data_f1[132] =size;
				ret = upgrade_send_instructions(upgrade_data_f1,0xF1,134);
				if (ret < 0) {
					pr_info("ti941 upgrade_data_f1 errno:%d ", ret);
					return -1;
				}	
			}
			else //only F1
			{	
				size = lenght - ((i-1)*512) -3;
				size +=6;
				pr_info("size1 =%d",size);
				upgrade_data_f1[size -2] = 0;
				ret = upgrade_send_instructions(upgrade_data_f1,0xF1,size);
				if (ret < 0) {
					pr_info("ti941 upgrade_data_f1 errno:%d ", ret);
					return -1;
				}	
			}
		}
		else{ //normal frame F1
			ret = upgrade_send_instructions(upgrade_data_f1,0xF1,F1_TRANSMIT_DATA+6);
			if (ret < 0) {
				pr_info("ti941 upgrade_data_f1 errno:%d ", ret);
				return -1;
			}	
		}
		////f2 data frame
		for(j = 0;j<F2_TRANSMIT_DATA;j++) 
		{	
			if(j+((i-1)*512+3+F1_TRANSMIT_DATA) <= lenght)
			{	
				sum = j+((i-1)*512)+F1_TRANSMIT_DATA+3;
				upgrade_data_f2[j+4] =data[sum];
			}
			else
			{	
				upgrade_data_f2[j+4] =0x0;
			}
		}
		msleep(DELAY_100_MS);
		if(i == block_sum && data_integrity) //last frame
		{
			size = lenght - ((i-1)*512)-175;
			if( 0 < size ) //may only F2
			{
				size +=6;
				pr_info("size2 =%d",size);
				ret = upgrade_send_instructions(upgrade_data_f2,0xF2,size);
				if (ret < 0) {
				pr_info("ti941 upgrade_data_f2 errno:%d ", ret);
				return -1;
				}
			}
			else //not F2
			{

			}
		}
		else{ //normal frame
#if 1
			ret = upgrade_send_instructions(upgrade_data_f2,0xF2,F2_TRANSMIT_DATA+6);
			if (ret < 0) {
				pr_info("ti941 upgrade_data_f2 errno:%d ", ret);
				return -1;
			}
#endif
		}
		msleep(DELAY_100_MS);
		////f3 data frame
		for(j = 0;j<F3_TRANSMIT_DATA;j++) 
		{	if((!data_integrity))
			{
				sum = j+((i-1)*512)+3+F3_TRANSMIT_DATA+F1_TRANSMIT_DATA;
				upgrade_data_f3[j+4] =data[sum];
			}

		}
		if(block_sum >= 1 && (!data_integrity))
		{
			ret = upgrade_send_instructions(upgrade_data_f3,0xF3,F3_TRANSMIT_DATA+6);
			if (ret < 0) {
				pr_info("ti941 upgrade_data_f3 errno:%d ", ret);
				return -1;
			}
		}	
		msleep(DELAY_100_MS);
		progress_status = (i  *100) / block_sum + UPGRADE_STARE;
		if(progress_status >100)
		{
			progress_status =100;
		}
		pr_info("block =%d,block_return = %d,upgrade_status =%d",i&0xff,block_return,upgrade_status);
	
		for(retry = 0;retry < 11;retry++)//for(retry;retry < 11;retry++)
		{
			if(retry == 10)
			{
				progress_status = UPGRADE_FAILE;
				upgrade_free_memory();
				return -1;
			}
			if((i&0xff) != block_return)
			{	
				msleep(DELAY_10_MS);
				continue;
			}else{
				break;
			}
		}

		upgrade_nrc = get_upgrade_status();
		ret = update_return_judgment(upgrade_nrc,0x36);
		if (ret < 0) {
			pr_info("ti941 update_into_extend_mod errno:%d ", ret);
			return -1;
		}
	}
	return 0;
}
#endif

//upgrade_request_exit
int upgrade_request_exit(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_request_exit[12] = {0xf0,0x3c,0x28,0x01,0x37,0xaa,0xaa,0xaa,0xaa,0xaa,0x0,0x0};	
	fw_info.upgrade_diagnosis[0] = upgrade_request_exit[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_request_exit[5];
	ret = upgrade_send_instructions(upgrade_request_exit,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_request_exit errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	if(fw_info.upgradetype ==UPGRADE_TOUCHFW)
	{
		msleep(DELAY_2000_MS);
	}
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_request_exit upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x37);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

//upgrade_erase_falsh
int upgrade_erase_falsh(void)
{
	int ret = -1;
    char upgrade_erase_falsh[12] = {0xf0,0x3c,0x28,0x0c,0x31,0x01,0xff,0x0,0x0,0x0,0xc,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_erase_falsh[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_erase_falsh[5];
	ret = upgrade_send_instructions(upgrade_erase_falsh,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_erase_falsh errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	return 0;

}

//upgrade_erase_falsh_extend
int upgrade_erase_falsh_extend(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_erase_falsh_extend[12] = {0xf1,0x3c,0x28,0x21,0x30,0x0,0x0,0x0,0x50,0x0,0x0,0x0};
	ret = upgrade_send_instructions(upgrade_erase_falsh_extend,0xF1,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_erase_falsh_extend errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_erase_falsh_extend upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x31);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}

//upgrade_check_complete
int upgrade_check_complete(void)
{
	int ret = -1;
	int upgrade_nrc = -1;
    char upgrade_check_complete[12] = {0xf0,0x3c,0x28,0x04,0x31,0x01,0xff,0x01,0xaa,0xaa,0x0,0x0};
	fw_info.upgrade_diagnosis[0] = upgrade_check_complete[4];
	fw_info.upgrade_diagnosis[1] =	upgrade_check_complete[5];
	ret = upgrade_send_instructions(upgrade_check_complete,0xF0,12);
	if (ret < 0) {
		pr_info("ti941 upgrade_check_complete errno:%d ", ret);
		return -1;
	}
	msleep(DELAY_100_MS);
	upgrade_nrc = get_upgrade_status();
	pr_info("ti941 upgrade_check_complete upgrade_nrc =%d",upgrade_nrc);
	ret = update_return_judgment(upgrade_nrc,0x31);
	if (ret < 0) {
		pr_info("ti941 update_return_judgment errno:%d ", ret);
		return -1;
	}
	return 0;
}


int upgrade_progress(void)
{
	return progress_status;
}

int upgrade_touch_version_comparison(int upgradetype)
{	
	int upgrfwver  = -1;
	int upgrcfgver = -1;
	if( UPGRADE_TOUCHFW == upgradetype && Upgrade_touch_flag)
	{	
		get_screen_version();
		//upgrfwver   = fw_info.fw_version[6] << 8 | fw_info.fw_version[7];
		upgrcfgver  = fw_info.fw_version[7] << 8 | fw_info.fw_version[6];
		pr_info("fw_info.tp_cfg_version = %02x,fw_info.tp_fw_version = %02x",fw_info.tp_cfg_version,fw_info.tp_fw_version);
		pr_info("upgrcfgver = %02x,upgrfwver = %02x",upgrcfgver,upgrfwver);
		if((upgrcfgver < fw_info.tp_cfg_version))
		//if((upgrcfgver < fw_info.tp_cfg_version) || (upgrfwver < fw_info.tp_fw_version))
		{

		}
		else
		{	
			progress_status = UPGRADE_SAME_TP_VERSION;
			return -1;
		}
	}
	return 0;
}

int upgrade_lcm_img(char* path,int upgradetype)
{	
	int ret  = -1;

//#if 1
#if 0
	ret = sreen_firmware_data_analysis(path,upgradetype);
	if (ret < 0) {
		pr_info("ti941 sreen_firmware_data_analysis faile:%d ", ret);
		return -1;
	}
#endif

#if 1
	ret = update_into_extend_mod();
	if (ret < 0) {
		pr_info("ti941 update_into_extend_mod faile:%d ", ret);
		return -1;
	}

	ret = upgrade_check_before();
	if (ret < 0) {
		pr_info("ti941 upgrade_check_before faile:%d ", ret);
		return -1;
	}

	ret = upgrade_disable_fault_codes();
	if (ret < 0) {
		pr_info("ti941 upgrade_disable_fault_codes faile:%d ", ret);
		return -1;
	}

	ret = upgrade_disable_non_refresh();
	if (ret < 0) {
		pr_info("ti941 upgrade_disable_non_refresh faile:%d ", ret);
		return -1;
	}

	ret = upgrade_enter_prog_session();
	if (ret < 0) {
		pr_info("ti941 upgrade_enter_prog_session faile:%d ", ret);
		return -1;
	}

	ret = upgrade_security_verification1();
	if (ret < 0) {
		pr_info("ti941 upgrade_security_verification1 faile:%d ", ret);
		return -1;
	}

	ret = upgrade_security_verification2();
	if (ret < 0) {
		pr_info("ti941 upgrade_security_verification2 faile:%d ", ret);
		return -1;
	}

	ret = upgrade_record_fingerprint();
	if (ret < 0) {
		pr_info("ti941 upgrade_record_fingerprint faile:%d ", ret);
		return -1;
	}

	ret = upgrade_record_fingerprint_extend();
	if (ret < 0) {
		pr_info("ti941 upgrade_record_fingerprint_extend faile:%d ", ret);
		return -1;
	}

	ret = upgrade_erase_falsh();
	if (ret < 0) {
		pr_info("ti941 upgrade_erase_falsh faile:%d ", ret);
		return -1;
	}

	ret = upgrade_erase_falsh_extend();
	if (ret < 0) {
		pr_info("ti941 upgrade_erase_falsh_extend faile:%d ", ret);
		return -1;
	}
	ret = upgrade_request_download();
	if (ret < 0) {
		pr_info("ti941 upgrade_request_download faile:%d ", ret);
		return -1;
	}

	ret = upgrade_request_download_extend();
	if (ret < 0) {
		pr_info("ti941 upgrade_request_download_extend faile:%d ", ret);
		return -1;
	}

	ret = upgrade_transfer_data(fw_info.fw_buf,fw_info.file_upgrade_size);
	if (ret < 0) {
		pr_info("ti941 upgrade_transfer_data faile:%d ", ret);
		return -1;
	}
#if 0
	if(fw_info.upgradetype ==UPGRADE_TOUCHFW)
	{
		ret = upgrade_transfer_data(fw_info.ugrade_data_buf,fw_info.file_upgrade_size);
		if (ret < 0) {
			pr_info("ti941 upgrade_transfer_data faile:%d ", ret);
			return -1;
		}
	}
	else
	{
		if(fw_info.slot == FIR_PART_B){
			ret = upgrade_transfer_data(fw_info.ugrade_data_buf,fw_info.file_upgrade_size-4);
			if (ret < 0) {
				pr_info("ti941 upgrade_transfer_data faile:%d ", ret);
				return -1;
			}
		}
		if(fw_info.slot == FIR_PART_A){
			ret = upgrade_transfer_data(fw_info.ugrade_data_buf,fw_info.file_upgrade_size-4);
			if (ret < 0) {
				pr_info("ti941 upgrade_transfer_data faile:%d ", ret);
				return -1;
			}
		}
	}

#endif

	ret = upgrade_check_complete();
	if (ret < 0) {
		pr_info("ti941 upgrade_check_complete faile:%d ", ret);
		return -1;
	}

	ret = upgrade_request_exit();
	if (ret < 0) {
		pr_info("ti941 upgrade_request_exit faile:%d ", ret);
		return -1;
	}
	progress_status = UPGRADE_SUCCEED;
	upgrade_free_memory();
#endif
	return 0;
}
//EXPORT_SYMBOL(upgrade_lcm_img);


void get_version(char* buf)
{
    char show_buff[32];
    char mode_user[]="Mode:user";
    char mode_boot[]="Mode:boot";
    char mode_null[]="Mode:null";
    memcpy(show_buff,fw_info.fw_version,16);
    show_buff[16]=0x20;
    if(fw_info.fw_version[16]<=1)
        memcpy(&show_buff[17],fw_info.fw_version[16]==0?mode_user:mode_boot,9);
    else
        memcpy(&show_buff[17],mode_null,9);
    show_buff[26]=0;
    memcpy(buf,show_buff,32);
}
//EXPORT_SYMBOL(get_version);
int get_packet_id(void)
{
    return fw_info.packet_id;
}
//EXPORT_SYMBOL(get_packet_id);
int get_packet_count(void)
{
    return fw_info.packet_count;
}
//EXPORT_SYMBOL(get_packet_count);
int query_status(void)
{
    int ret;
    char query[7]={0x81,0,0,0,0,0,0x82};
    ret=ti941_i2c_dma_write_bytes(i2c_client_point,0x00,query, 7);
    if(ret < 0){
        pr_info("ti941 query status error. errno:%d ", ret);
    }
    return 0;
}
//EXPORT_SYMBOL(query_status);

int mfd_set_key_backlight_level(int level,int mode)
{
    int ret;
    char query[6]={0x83,0,0,0,0,0x0};
	int dlevel = 0;
	int last_level = 0;
	pr_info("mfd_set_key_backlight_level level %d",level);
	dlevel = level;
    if(level<0||level>255)
    {
        pr_info("ti941----mfd_set_key_backlight_level not between 1~255(%d)\n",level);
        return -1;
    }
    if(mode<0||mode>2)
    {
        pr_info("ti941----mfd_set_key_backlight_mode not between 0~2(%d)\n",mode);
        return -1;
    }
	//last_level = dlevel/2.55;
	last_level = dlevel/(255 / 100);
    query[1]=((int)last_level)&0xff;
    query[3]=mode&0xff;
    complete_frame(query,6);
    print_array(query,6);
    ret=ti941_i2c_dma_write_bytes(i2c_client_point,0x83,query, 6);
    if(ret < 0){
        pr_info("ti941 mfd_set_key_backlight_level error. errno:%d ", ret);
    }
    return 0;
}
//EXPORT_SYMBOL(mfd_set_key_backlight_level);


int mfd_set_backlight_level(int level,int mode)
{
    int ret;
    char query[11]={0x83,0,0,0,0,0,0,0,0,0,0};
	if( upgrade_progress()!=0 )
	{
		pr_info("ti941----mfd_set_backlight_level Upgrade cannot set backlighting level = %d",level);
        return -1;
	}
    if(level<0)
    {
        pr_info("ti941----mfd_set_backlight_level not between > 0(%d)\n",level);
        return -1;
    }
    if(mode<0||mode>2)
    {
        pr_info("ti941----mfd_set_backlight_level not between 0~2(%d)\n",mode);
        return -1;
    }
    query[1]=level&0xff;
    query[3]=mode&0xff;
    complete_frame(query,11);
    print_array(query,11);
    ret=ti941_i2c_dma_write_bytes(i2c_client_point,0x83,query, 11);
    if(ret < 0){
        pr_info("ti941 mfd_set_backlight_level error. errno:%d ", ret);
    }
    return 0;
}
//EXPORT_SYMBOL(mfd_set_backlight_level);

int mfd_get_backlight_level(void)
{	
	u8  point_data[8] = {0,};
	int	ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x13,point_data,8);
	if (ret < 0) {
		GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
		return -1;
	}
	print_array(point_data,8);
	MFD_backlight_temperature_status=point_data[1];
	MFD_key_backlight=point_data[3];
	MFD_backlight_power_status=point_data[5];  
	pr_info("MFD_key_backlight =%d",MFD_key_backlight);
    return MFD_key_backlight;
}
//EXPORT_SYMBOL(mfd_get_backlight_level);

static ssize_t mfd_bl_level_read_proc(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	int len, err = -1;
    int level=0;

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!page) {
		kfree(page);
		return -ENOMEM;
	}

	ptr = page;
    
    
    level=mfd_get_backlight_level();
    ptr += sprintf(ptr, "%d\n",level);
    
    
	len = ptr - page;
	if (*ppos >= len) {
		kfree(page);
		return 0;
	}
	err = copy_to_user(buffer, (char *)page, len);
	*ppos += len;
	if (err) {
		kfree(page);
		return err;
	}
	kfree(page);
	return len;

	/* return (ptr - page); */
}

static ssize_t mfd_bl_level_write_proc(struct file *file, const char *buffer, size_t count,
					loff_t *ppos)
{
	char temp[25] = {0}; /* for store special format cmd */
    unsigned int level;
	unsigned int mode;
	//pr_info("write count %ld\n", (unsigned long)count);


	/**********************************************/
	/* for store special format cmd  */
	if (copy_from_user(temp, buffer, sizeof(temp))) {
		pr_info("copy from user fail \n");
		return -EFAULT;
	}
	//ret = sscanf(temp, "%d %d", (char *)&level,(char *)&mode);//hc add
	sscanf(temp, "%d %d", &level, &mode);//hc add
	mfd_set_backlight_level(level,mode);

	return count;
}
/*
static const struct file_operations mfd_bl_level_ops = {
	.write = mfd_bl_level_write_proc,
	.read = mfd_bl_level_read_proc
};
*/
static const struct proc_ops mfd_bl_level_ops = {
	.proc_write = mfd_bl_level_write_proc,
	.proc_read = mfd_bl_level_read_proc
};

#if 1
static ssize_t upgrade_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "otg_mode_store no dev\n");
		return 0;
	}
	return sprintf(buf, "%d %x %x %x %x %x\n",progress_status,fw_info.upgrade_diagnosis[0],fw_info.upgrade_diagnosis[1],fw_info.upgrade_diagnosis[2],fw_info.upgrade_diagnosis[3],fw_info.upgrade_diagnosis[4]);
}

static ssize_t upgrade_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	char mode[128];


	if (!dev) {
		dev_dbg(dev, "upgrade_store no dev\n");
		return count;
	}

	if (sscanf(buf, "%s", mode) == 1) {
		pr_info("ti941  upgrade_store--------%s\n",mode);
	}

    upgrade_lcm_img(mode,UPGRADE_MCUFW);

	return count;
}

static ssize_t upgrade_touch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "upgrade_touch_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%d %x %x %x %x %x\n",progress_status,fw_info.upgrade_diagnosis[0],fw_info.upgrade_diagnosis[1],fw_info.upgrade_diagnosis[2],fw_info.upgrade_diagnosis[3],fw_info.upgrade_diagnosis[4]);
}

static ssize_t request_upgrade_touch_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	char mode[128];


	if (!dev) {
		dev_dbg(dev, "request_upgrade_touch_store no dev\n");
		return count;
	}

	if (sscanf(buf, "%s", mode) == 1) {
		pr_info("ti941  request_upgrade_touch_store--------%s\n",mode);
	}
	return count;
}
static ssize_t request_upgrade_touch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "request_upgrade_touch_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%x\n",fw_info.request_data[1]);
}

static ssize_t upgrade_touch_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	char mode[128];


	if (!dev) {
		dev_dbg(dev, "upgrade_store no dev\n");
		return count;
	}

	if (sscanf(buf, "%s", mode) == 1) {
		pr_info("ti941  upgrade_touch_store--------%s\n",mode);
	}

    upgrade_lcm_img(mode,UPGRADE_TOUCHFW);

	return count;
}

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "otg_mode_store no dev\n");
		return 0;
	}
	//get_screen_version();
	return sprintf(buf,"HW_Version:%02d_%02d\nSW_Version:%02d_%02d_%02d\nTP_Version:%02x_%02x_%02x\nVendor_code:%02x_%02x_%02x_%02x\n",\
	screen_version_info[0],screen_version_info[1],screen_version_info[2],screen_version_info[3],screen_version_info[4],screen_version_info[5],\
	screen_version_info[6],screen_version_info[7],\
	screen_version_info[8],screen_version_info[9],screen_version_info[10],screen_version_info[11]);
}

static ssize_t version_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int mode;

	if (!dev) {
		dev_dbg(dev, "otg_mode_store no dev\n");
		return count;
	}

	if (sscanf(buf, "%ud", &mode) == 1) {
		if (mode == 0) {
		} else {
		}
	}
	return count;
}


int hu_get_function(int index)
{
	if(index > 15) return 0;
	return que_status[index];
}
//EXPORT_SYMBOL(hu_get_function);

static ssize_t screen_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "screen_info_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",\
	dis_screen_info[0],dis_screen_info[1],dis_screen_info[2],dis_screen_info[3],\
	dis_screen_info[4],dis_screen_info[5],dis_screen_info[6],dis_screen_info[7]);
}

static ssize_t screen_info_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	char mode[128];
	if (!dev) {
		dev_dbg(dev, "screen_info_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {//hc add
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 ====%s,%s\n",mode,__func__);
		//query_status();
	}
	return count;
}


static ssize_t light_intensity_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "light_intensity_info_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",\
	light_intensity_info[0],light_intensity_info[1],light_intensity_info[2],light_intensity_info[3],\
	light_intensity_info[4],light_intensity_info[5],light_intensity_info[6],light_intensity_info[7]);
}

static ssize_t light_intensity_info_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	char mode[128];
	if (!dev) {
		dev_dbg(dev, "light_intensity_info_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 ====%s,%s\n",mode,__func__);
	}
	return count;
}

static ssize_t dis_screen_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "dis_screen_status_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",\
	dis_screen_status[0],dis_screen_status[1],dis_screen_status[2],dis_screen_status[3],\
	dis_screen_status[4],dis_screen_status[5],dis_screen_status[6],dis_screen_status[7]);
}

static ssize_t dis_screen_status_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	char mode[128];
	if (!dev) {
		dev_dbg(dev, "dis_screen_status_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 ====%s,%s\n",mode,__func__);
	}
	return count;
}

static ssize_t screen_products_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "screen_products_id_show no dev\n");
		return 0;
	}
	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x\n",\
	screen_products_id[0],screen_products_id[1],screen_products_id[2],screen_products_id[3],\
	screen_products_id[4],screen_products_id[5],screen_products_id[6],screen_products_id[7]);
}

static ssize_t screen_products_id_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	char mode[128];
	if (!dev) {
		dev_dbg(dev, "screen_products_id_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 ====%s,%s\n",mode,__func__);
	}
	return count;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "debug_show no dev\n");
		return 0;
	}

	return sprintf(buf, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",que_status[0],\
	que_status[1],que_status[2],que_status[3],que_status[4],que_status[5],\
	que_status[6],que_status[7],que_status[8],que_status[9],que_status[10],\
	que_status[11]);
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	char mode[128];
	if (!dev) {
		dev_dbg(dev, "debug_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 ====%s,%s\n",mode,__func__);
		//query_status();
	}
	Upgrade_touch_flag = false;
	return count;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		dev_dbg(dev, "status_show no dev\n");
		return 0;
	}

	return sprintf(buf, "%d\n",get_screen_active_partition());
}

static ssize_t status_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	char mode[128];

	if (!dev) {
		dev_dbg(dev, "status_store no dev\n");
		return count;
	}

	//if (sscanf(buf, "%s", &mode) == 1) {
	if (sscanf(buf, "%127s", mode) == 1) {//hc add
		pr_info("ti941 status_store ====%s\n",mode);
	}
	upgrade_reboot(mode);
	return count;
}


DEVICE_ATTR(request_upgrade_touch, 0664, request_upgrade_touch_show, request_upgrade_touch_store);
DEVICE_ATTR(upgrade_touchfw, 0664, upgrade_touch_show, upgrade_touch_store);
DEVICE_ATTR(upgrade_smcufw, 0664, upgrade_show, upgrade_store);
DEVICE_ATTR(upgrade_debug, 0664, status_show, status_store);


DEVICE_ATTR(screen_info, 0664, screen_info_show, screen_info_store);
DEVICE_ATTR(light_intensity_info, 0664, light_intensity_info_show, light_intensity_info_store);
DEVICE_ATTR(dis_screen_status, 0664, dis_screen_status_show, dis_screen_status_store);
DEVICE_ATTR(screen_products_id, 0664, screen_products_id_show, screen_products_id_store);

DEVICE_ATTR(hbsmcu_ver, 0664, version_show, version_store);
DEVICE_ATTR(que_mfd_status, 0664, debug_show, debug_store);



static struct attribute *touch_attributes[] = {
	&dev_attr_request_upgrade_touch.attr,
	&dev_attr_upgrade_smcufw.attr,
	&dev_attr_upgrade_touchfw.attr,
	&dev_attr_upgrade_debug.attr,
	&dev_attr_screen_info.attr,
	&dev_attr_light_intensity_info.attr,
	&dev_attr_dis_screen_status.attr,
	&dev_attr_screen_products_id.attr,
	&dev_attr_hbsmcu_ver.attr,
	&dev_attr_que_mfd_status.attr,
	NULL
};

static const struct attribute_group touch_attr_group = {
	.attrs = touch_attributes,
};
#endif

static int tpd2_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	GTP_INFO("tpd2_irq_registration start  %s, %d", __func__, __LINE__);
	//node = of_find_compatible_node(NULL, NULL, "mediatek,cap_touch");
	node = of_find_compatible_node(NULL, NULL, "goodix,cap_touch_two");
	GTP_INFO("tpd2_irq_registration  node \r\n"  );
	if (node) {
		int irqflags;
		
		tpd2_int_gpio_number = of_get_named_gpio(node, "int-gpio", 0);
		
		if (tpd2_int_gpio_number < 0 ){
			pr_info("[%s-%s-%d ] tpd2_int_gpio_number:%d ", __FILE__, __func__, __LINE__,tpd2_int_gpio_number);
		}

		gpio_direction_input(tpd2_int_gpio_number);
		touch_irq = gpio_to_irq(tpd2_int_gpio_number);
		//touch_irq = irq_of_parse_and_map(node, 0);
		/*if (!touch_irq)
			touch_irq = gpio_to_irq(tpd2_int_gpio_number);*/
		ret = of_property_read_u32(node, "irqflags", &irqflags);
		if (ret) {
				pr_info("%s, Invalid irq-flags", __func__);
				return -EINVAL;
		}

		pr_info("tpd2_irq_registration1 touch_irq %d , irqflags=%d\r\n",touch_irq, irqflags);
               //touch_irq  = 1;   //interrupt  1  
               //int_type = 1 ;
			ret = request_irq(touch_irq, tpd2_interrupt_handler, irqflags, tpd2_DEVICE, NULL);
//					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, tpd2_DEVICE, NULL);
			gtp_eint_trigger_type = irqflags;
			if (ret > 0)
				GTP_ERROR("tpd2 request_irq IRQ LINE NOT AVAILABLE!.");
		// pr_info("%s, irq type:%d\n", __func__, irqd_get_trigger_type(irq_get_irq_data(touch_irq)));
	} else {
		GTP_ERROR("[%s] tpd2 request_irq can not find touch eint device node!.", __func__);
	}
	return 0;
}


static s32 tpd2_i2c_probe(struct i2c_client *client)
{   
	
	s32 err = 0;
	s32 ret = 0;
	// u16 version_info;
	struct task_struct *thread = NULL;
	// u8  probe_data[20] = {0,};
	// u8 ahu_startup_done[6] = {0x80,0x1,0x0,0x0,0x0,0x82};
	// u8 question_cmd[5] = {0x81,0x0,0x49,0x0,0x0};
	// u8  version_cmd[20] = {0,};
#if 0 /*#ifdef CONFIG_GTP_HAVE_TOUCH_KEY */
	s32 idx = 0;
#endif
#ifdef GTP_PROXIMITY
	struct hwmsen_object obj_ps;
#endif
    GTP_INFO("tpd2_i2c_probe start");
	of_get_gt9xx_platform_data(&client->dev);

	/* add from kernel 4.4, 64-bit will get dummy dma ops without this, -1 denote no matters */
	//arch_setup_dma_ops(&client->dev, -1, -1, NULL, 0);//remove it ,because no dma 
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	pr_info("ti941 i2c:0x1a----tpd2_i2c_probe dma_read_bytes: client->dev.coherent_dma_mask = DMA_BIT_MASK(32) \n");
	i2c_client_point = client;
	ret = tpd2_power_on(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		// goto out;
	}


#ifdef CONFIG_GTP_AUTO_UPDATE
	ret = gup_init_update_proc(client);

	if (ret < 0) {
		GTP_ERROR("Create update thread error.");
		goto out;
	}

#endif



#ifdef VELOCITY_CUSTOM
	tpd2_v_magnify_x = tpd2_VELOCITY_CUSTOM_X;
	tpd2_v_magnify_y = tpd2_VELOCITY_CUSTOM_Y;

#endif


	// ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x00,probe_data,20);
	// if (ret < 0) {
	// 	GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
	// }
	// pr_info("ti941 probe_data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x \n",
	// 								probe_data[0],probe_data[1],probe_data[2],probe_data[3],probe_data[4],
	// 								probe_data[5],probe_data[6],probe_data[7],probe_data[8],probe_data[9],
	// 								probe_data[10],probe_data[11],probe_data[12],probe_data[13],probe_data[14],
	// 								probe_data[15],probe_data[16],probe_data[17],probe_data[18],probe_data[19]);
	// ret = ti941_i2c_dma_write_bytes(i2c_client_point,0x00,ahu_startup_done, 6);
	// if(ret < 0){
	// 	pr_info("ti941 I2C write error. errno:%d ", ret);
	// }
	// // ret = ti941_i2c_dma_write_bytes(i2c_client_point,0x81,question_cmd, 5);
	// // if(ret < 0){
	// // 	pr_info("ti941 I2C write error. errno:%d ", ret);
	// // }
	// ret=ti941_i2c_dma_read_bytes(i2c_client_point,0x00,version_cmd,20);
	// if(ret < 0){
	// 	pr_info("ti941 I2C write error. errno:%d ", ret);
	// }
	// pr_info("ti941 version_cmd=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x \n",
	// 						version_cmd[0],version_cmd[1],version_cmd[2],version_cmd[3],version_cmd[4],
	// 						version_cmd[5],version_cmd[6],version_cmd[7],version_cmd[8],version_cmd[9],
	// 						version_cmd[10],version_cmd[11],version_cmd[12],version_cmd[13],version_cmd[14],
	// 						version_cmd[15],version_cmd[16],version_cmd[17],version_cmd[18],version_cmd[19]);


	// ret = ti941_read_version(client, &version_info);

	// if (ret < 0) {
	// 	GTP_ERROR("Read version failed.");
	// 	goto out;
	// }

	ret = gtp_init_panel(client);
	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		goto out;
	}
	GTP_DEBUG("gtp_init_panel success");
	/* Create proc file system */
	gt91xx_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0660, NULL, &gt_upgrade_proc_fops);

	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed", GT91XX_CONFIG_PROC_FILE);
		goto out;
	}

#ifdef CONFIG_GTP_CREATE_WR_NODE
	init_wr_node(client);
#endif

	thread = kthread_run(touch_event_handler, 0, tpd2_DEVICE);

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_ERROR(tpd2_DEVICE " failed to create kernel thread: %d", err);
		goto out;
	}

#if 0/* #ifdef CONFIG_GTP_HAVE_TOUCH_KEY */

	for (idx = 0; idx < tpd2_KEY_COUNT; idx++)
		input_set_capability(tpd2->dev, EV_KEY, touch_key_array[idx]);

#endif

        /* Configure gpio to irq and request irq */
	//tpd2_gpio_as_int(GTP_INT_PORT);
	//tpd2_gpio_as_int(GTP_INT_PORT);
	msleep(50);
        pr_info("tpd2_int_gpio_number %d \n", tpd2_int_gpio_number );
	tpd2_irq_registration();
       
	//enable_irq(touch_irq);


#ifdef GTP_PROXIMITY
	/* obj_ps.self = cm3623_obj; */
	obj_ps.polling = 0;         /* 0--interrupt mode;1--polling mode; */
	obj_ps.sensor_operate = tpd2_ps_operate;

	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err)
		GTP_ERROR("hwmsen attach fail, return:%d.", err);

#endif

#if 0
        ret = sysfs_create_group(&client->dev.kobj, &touch_attr_group);
        if (ret < 0) {
            dev_dbg(&client->dev, "Cannot register touch upgrade sysfs attributes: %d\n",
                ret);
        }
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, tpd2_ESD_CHECK_CIRCLE);
#endif

	tpd2_load_status = 1;
	err = sysfs_create_group(&client->dev.kobj, &touch_attr_group);
	if (err < 0) {
		dev_dbg(&client->dev, "Cannot register touch upgrade sysfs attributes: %d\n",
			err);
	}
    ///if(proc_create("mfd_backlight", 0666, NULL, &mfd_bl_level_ops)==NULL)
	if(proc_create("mfd_backlight2", 0666, NULL, &mfd_bl_level_ops)==NULL)
    {
        dev_dbg(&client->dev, "Cannot register proc mfd_bl_level: %d\n",
			err);
    }
	GTP_INFO("%s, success run Done", __func__);
	return 0;
out:
	//gpio_free(tpd2_rst_gpio_number);
	//gpio_free(tpd2_int_gpio_number);
	return -1;
}

static irqreturn_t tpd2_interrupt_handler(int irq, void *dev_id)
{
	tpd2_DEBUG_PRINT_INT;
	tpd2_flag = 1;
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}
//static int tpd2_i2c_remove(struct i2c_client *client)//hc add
void tpd2_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_GTP_CREATE_WR_NODE
	uninit_wr_node();
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

	//gpio_free(tpd2_rst_gpio_number);
	//gpio_free(tpd2_int_gpio_number);
	//return 0;
}


#ifdef CONFIG_GTP_ESD_PROTECT
static void force_reset_guitar(void)
{
	s32 i;
	s32 ret;

	GTP_INFO("force_reset_guitar");

	/* Power off TP */
	// ret = regulator_disable(tpd2->reg);
	// if (ret != 0)
	// 	tpd2_DMESG("Failed to disable reg-vgp6: %d\n", ret);

	// msleep(30);
	/* Power on TP */
	ret = regulator_enable(tpd2->reg);
	if (ret != 0)
		tpd2_DMESG("Failed to enable reg-vgp6: %d\n", ret);

	msleep(30);
	for (i = 0; i < 5; i++) {
		/* Reset Guitar */

		/* Send config */
		ret = ti941_send_cfg(i2c_client_point);

		if (ret < 0)
			continue;

		break;
	}

}

static void gtp_esd_check_func(struct work_struct *work)
{
	int i;
	int ret = -1;
	u8 test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

	if (tpd2_halt)
		return;

	for (i = 0; i < 3; i++) {
		ret = ti941_i2c_read(i2c_client_point, test, 3);

		if (ret > 0)
			break;

	}

	if (i >= 3)
		force_reset_guitar();

	if (!tpd2_halt)
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, tpd2_ESD_CHECK_CIRCLE);

}
#endif
static int tpd2_history_x = 0, tpd2_history_y;
static void tpd2_down(s32 x, s32 y, s32 size, s32 id)
{
	if ((!size) && (!id)) {
		input_report_abs(tpd2->dev, ABS_MT_PRESSURE, 100);
		input_report_abs(tpd2->dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		input_report_abs(tpd2->dev, ABS_MT_PRESSURE, size);
		input_report_abs(tpd2->dev, ABS_MT_TOUCH_MAJOR, size);
		/* track id Start 0 */
		input_report_abs(tpd2->dev, ABS_MT_TRACKING_ID, id);
	}
	input_report_key(tpd2->dev, BTN_TOUCH, 1);

	input_report_abs(tpd2->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd2->dev, ABS_MT_POSITION_Y, y);

	input_mt_sync(tpd2->dev);
	tpd2_history_x  = x;
	tpd2_history_y  = y;
	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x + y); */

#ifdef CONFIG_MTK_BOOT
	if (tpd2_dts_data.use_tpd2_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
			tpd2_button(x, y, 1);
	}
#endif
}

static void tpd2_up(s32 x, s32 y, s32 id)
{	
	pr_info("tpd2_up x = %d,y =%d ,id=%d",x,y,id);
	/* input_report_abs(tpd2->dev, ABS_MT_PRESSURE, 0); */
	input_report_key(tpd2->dev, BTN_TOUCH, 0);
	/* input_report_abs(tpd2->dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(tpd2->dev);
	///tpd2_DEBUG_SET_TIME;
	///tpd2_EM_PRINT(tpd2_history_x, tpd2_history_y, tpd2_history_x, tpd2_history_y, id, 0);
	tpd2_history_x = 0;
	tpd2_history_y = 0;
	/* MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x + y); */

#ifdef CONFIG_MTK_BOOT
	if (tpd2_dts_data.use_tpd2_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
			tpd2_button(x, y, 0);
	}
#endif
}
#if 0
/*Coordination mapping*/
static void tpd2_calibrate_driver(int *x, int *y)
{
	int tx;

	GTP_DEBUG("Call tpd2_calibrate of this driver ..\n");

	tx = ((tpd2_def_calmat[0] * (*x)) + (tpd2_def_calmat[1] * (*y)) + (tpd2_def_calmat[2])) >> 12;
	*y = ((tpd2_def_calmat[3] * (*x)) + (tpd2_def_calmat[4] * (*y)) + (tpd2_def_calmat[5])) >> 12;
	*x = tx;
}
#endif 

static int touch_event_handler(void *unused)
{	
	
    struct sched_param param = { .sched_priority = 4 };
	u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	u8  point_data[128] = {0,};
	//u8  tp_data[3] = {0};
	//u8  ext_frame_data[30] = {0,};
	s32 touch_data[10][4]={{0,},};
	u8 pretouch_status[5]={0,};
	//u8 ahu_startup_done[6] = {0x83,0x00,0x00,0x02,0x00,0x85};
	//u8 ahu_stopup_done[6] = {0x83,0x00,0x00,0x02,0x00,0x83};
	//u8 ahu_packet_num[7] = {0xF1,0x0,0x0,0x0,0x0,0x0,0x82};
	//u8 ahu_packet[134] = {0xF2,};
	//int packet_id=0;
	//int packet_realsize=0;
	// u8  version_cmd[20] = {0,};
	// u8 count;
	u8  check_sum = 0;
	u8  ext_frame_len = 0;
	u8  touch_num = 0;
	u8  pre_touch_num = 0;
	u8  finger = 0;
	static u8 pre_touch;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i  = 0;
	s32 ret = -1;
#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
	u8  key_value = 0;
	static u8 pre_key;
#endif
#ifdef GTP_PROXIMITY
	s32 err = 0;
	hwm_sensor_data sensor_data;
	u8 proximity_status;
#endif
#ifdef CONFIG_GTP_CHANGE_X2Y
	s32 temp;
#endif
	sched_setscheduler(current, SCHED_RR, &param);
	pr_info("%s--ti941 touch_event_handler  in in in\n", __func__);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if (tpd2_eint_mode) {
			wait_event_interruptible(waiter, tpd2_flag != 0);
			tpd2_flag = 0;
		} else {
			msleep(tpd2_polling_time);
		}
		set_current_state(TASK_RUNNING);

		mutex_lock(&i2c_access);
		disable_irq(touch_irq);
		pr_info("zxczxczxc ti941 handle irq\n");
		if (tpd2_halt) {
			mutex_unlock(&i2c_access);
			GTP_DEBUG("return for interrupt after suspend...  ");
			continue;
		}
#if 1
		{	
			ret=ti941_i2c_dma_read_bytes(i2c_client_point,0xFE,point_data,30);
			if (ret < 0) {
				GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
				goto exit_work_func;
			}
			if(point_data[0]==0x01){
				print_array(point_data,16);
				check_sum=0;
#if 1
				for(i=0;i<15;i++)
				{
					check_sum=(u8)(check_sum+point_data[i]);
				}
				if(((check_sum)&0xff)!=point_data[15]){
					
					pr_info("ti941 check_sum failed 0x%x,0x%x \n",check_sum,point_data[15]);
					goto exit_work_func;
				}
#endif
				pre_touch_num = touch_num;
				touch_num=point_data[1];
				ext_frame_len=point_data[14];
				pr_info("touch_num ==%d",touch_num);
				memset(touch_data,0,20);
				if(ext_frame_len==0xFF){
					for(i=0;i<(2);i++){
						touch_data[i][0]=point_data[2+i*6];
						touch_data[i][1]=point_data[3+i*6];
						touch_data[i][2]=(point_data[4+i*6]<<8)+point_data[5+i*6];
						touch_data[i][3]=(point_data[6+i*6]<<8)+point_data[7+i*6];
						if((touch_data[i][1]&0x40)!=0){
							pretouch_status[i]=1;
							tpd2_down(touch_data[i][2], touch_data[i][3], 0,touch_data[i][0]);
						}
						else if((touch_data[i][1]&0x20)!=0){
							if(pretouch_status[i]==1){
								pretouch_status[i]=0;
								tpd2_up(0,0,touch_data[i][0]);
							}
						}
					}
				}
				else{ //
					for(i=0;i<(touch_num<=2?touch_num:2);i++){
						touch_data[i][0]=point_data[2+i*6];
						touch_data[i][1]=point_data[3+i*6];
						touch_data[i][2]=(point_data[4+i*6]<<8)+point_data[5+i*6];
						touch_data[i][3]=(point_data[6+i*6]<<8)+point_data[7+i*6];
					}
					for(i=0;i<(touch_num<=2?touch_num:2);i++){
						pr_info("touch_data[i][1] =%x",touch_data[i][1]);
						if((touch_data[i][1]&0x40)!=0){
							tpd2_down(touch_data[i][2], touch_data[i][3], 0,touch_data[i][0]);
						}else if(touch_data[i][1] == 0x90){
							tpd2_down(touch_data[i][2], touch_data[i][3], 0,touch_data[i][0]);
						}else if(touch_data[i][1] == 0xA0){
							tpd2_up(0,0,touch_data[i][0]);
						}
					}
					if((touch_num<=2)&&(ext_frame_len==0)){
						if (tpd2 != NULL && tpd2->dev != NULL)
						{	
							input_sync(tpd2->dev);
						}
								
					}else if(ext_frame_len==0&&pre_touch_num<=2){
						if (tpd2 != NULL && tpd2->dev != NULL)
						{	
							input_sync(tpd2->dev);
						}
								
					}
				}
				if(ext_frame_len !=0)
				{	
					
					ret=ti941_i2c_dma_read_bytes(i2c_client_point,0xFE,point_data,ext_frame_len);
					if (ret < 0) {
						GTP_ERROR("ti941 I2C transfer error. errno:%d ", ret);
						goto exit_work_func;
					}
					print_array(point_data,ext_frame_len);
#if 1
					check_sum = 0;
					for(i=0;i<(ext_frame_len-1);i++)
					{
						check_sum=(u8)(check_sum+point_data[i]);
					}
					if(((check_sum)&0xff)!=point_data[ext_frame_len-1]){
						pr_info("ti941 check_sum2 failed 0x%x,0x%x \n",check_sum,point_data[ext_frame_len-1]);
						goto exit_work_func;
					}
#endif
					if(ext_frame_len==0xFF){
						for(i=0;i<3;i++){
							touch_data[i+2][0]=point_data[1+i*6];
							touch_data[i+2][1]=point_data[2+i*6];
							touch_data[i+2][2]=(point_data[3+i*6]<<8)+point_data[4+i*6];
							touch_data[i+2][3]=(point_data[5+i*6]<<8)+point_data[6+i*6];
							if((touch_data[i+2][1]&0x40)!=0){
								pretouch_status[2+i]=1;
								tpd2_down(touch_data[i+2][2], touch_data[i+2][3], 0,touch_data[i+2][0]);
							}
							else if((touch_data[i+2][1]&0x20)!=0){
								if(pretouch_status[2+i]==1){
									pretouch_status[2+i]=0;
									tpd2_up(0,0,touch_data[i+2][0]);
								}
							}
						}
						if (tpd2 != NULL && tpd2->dev != NULL)
								input_sync(tpd2->dev);
					}
					else{ //
						for(i=0;i<(touch_num>2?touch_num-2:3);i++){
							touch_data[i+2][0]=point_data[1+i*6];
							touch_data[i+2][1]=point_data[2+i*6];
							touch_data[i+2][2]=(point_data[3+i*6]<<8)+point_data[4+i*6];
							touch_data[i+2][3]=(point_data[5+i*6]<<8)+point_data[6+i*6];
						}
						for(i=2;i<touch_num;i++){
							if((touch_data[i][1]&0x40)!=0){
								tpd2_down(touch_data[i][2], touch_data[i][3], 0,touch_data[i][0]);
							}else if(touch_data[i][1] == 0x90){
								tpd2_down(touch_data[i][2], touch_data[i][3], 0,touch_data[i][0]);
							}else if(touch_data[i][1] == 0xA0){
								tpd2_up(0,0,touch_data[i][0]);
							}
						}
						if(touch_num>2){
						if (tpd2 != NULL && tpd2->dev != NULL)
						{	
							input_sync(tpd2->dev);
						}
								
						}
					}
					ext_frame_len=0;
					}
			}
			if(point_data[0]==0x10){
				print_array(point_data,11); 
				for(i=0;i<8;i++)
				{
					dis_screen_info[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x11){
				print_array(point_data,11);
				for(i=0;i<8;i++)
				{
					light_intensity_info[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x13){
				print_array(point_data,11);
				for(i=0;i<8;i++)
				{
					dis_screen_status[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x14){
				print_array(point_data,11);
				for(i=0;i<8;i++)
				{
					screen_products_id[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x15){
				print_array(point_data,15);
				for(i=0;i<12;i++)
				{
					screen_version_info[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x16){
				print_array(point_data,15);
				for(i=0;i<12;i++)
				{
					que_status[i]=point_data[i+1];
				}
			}
			if(point_data[0]==0x17){
				print_array(point_data,16);  
				for(i=0;i<13;i++)
				{

				}
				upgrade_status     = point_data[4];
				block_return       = point_data[5];
				fw_info.fw_key[0]  = point_data[6];
				fw_info.fw_key[1]  = point_data[7];
				fw_info.fw_key[2]  = point_data[8];

				fw_info.upgrade_diagnosis[2] = point_data[4];
				fw_info.upgrade_diagnosis[3] = point_data[5];
				fw_info.upgrade_diagnosis[4] = point_data[6];
			}
			if(point_data[0]==0x20){
				print_array(point_data,16); 
			}
		}
if(0){
		ret = ti941_i2c_read(i2c_client_point, point_data, 12);

		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d ", ret);
			goto exit_work_func;
		}

		finger = point_data[GTP_ADDR_LENGTH];

		if ((finger & 0x80) == 0) {

			enable_irq(touch_irq);

			mutex_unlock(&i2c_access);
			GTP_ERROR("buffer not ready");
			continue;
		}

#ifdef GTP_PROXIMITY

		if (tpd2_proximity_flag == 1) {
			proximity_status = point_data[GTP_ADDR_LENGTH];
			GTP_DEBUG("REG INDEX[0x814E]:0x%02X", proximity_status);

			if (proximity_status & 0x60) {
				/* proximity or large touch detect,enable hwm_sensor. */
				tpd2_proximity_detect = 0;
				/* sensor_data.values[0] = 0; */
			} else {
				tpd2_proximity_detect = 1;
				/* sensor_data.values[0] = 1; */
			}

			/* get raw data */
			GTP_DEBUG(" ps change");
			GTP_DEBUG("PROXIMITY STATUS:0x%02X", tpd2_proximity_detect);
			/* map and store data to hwm_sensor_data */
			sensor_data.values[0] = tpd2_get_ps_value();
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
			/* report to the up-layer */
			ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

			if (ret)
				GTP_ERROR("Call hwmsen_get_interrupt_data fail = %d", err);
		}

#endif

		touch_num = finger & 0x0f;

		if (touch_num > GTP_MAX_TOUCH) {
			GTP_ERROR("Bad number of fingers!");
			goto exit_work_func;
		}

		if (touch_num > 1) {
			u8 buf[8 * GTP_MAX_TOUCH] = {(GTP_READ_COOR_ADDR + 10) >> 8, (GTP_READ_COOR_ADDR + 10) & 0xff};

			ret = ti941_i2c_read(i2c_client_point, buf, 2 + 8 * (touch_num - 1));
			memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
		}
#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
		key_value = point_data[3 + 8 * touch_num];

		if (key_value || pre_key) {
			for (i = 0; i < tpd2_KEY_COUNT; i++) {
				/* input_report_key(tpd2->dev, touch_key_array[i], key_value & (0x01 << i)); */
				if (key_value&(0x01<<i)) {/* key=1 menu ;key=2 home; key =4 back; */
					input_x = touch_key_point_maping_array[i].point_x;
					input_y = touch_key_point_maping_array[i].point_y;
					GTP_DEBUG("button =%d %d", input_x, input_y);

					tpd2_down(input_x, input_y, 0, 0);
				}
			}

			if ((pre_key != 0) && (key_value == 0))
				tpd2_up(0, 0, 0);

			touch_num = 0;
			pre_touch = 0;
		}
	pre_key = key_value;
#endif

		GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

		if (touch_num) {
			for (i = 0; i < touch_num; i++) {
				coor_data = &point_data[i * 8 + 3];

				id = coor_data[0]&0x0F;
				input_x  = coor_data[1] | coor_data[2] << 8;
				input_y  = coor_data[3] | coor_data[4] << 8;
				input_w  = coor_data[5] | coor_data[6] << 8;

				GTP_DEBUG("Original touch point : [X:%04d, Y:%04d]", input_x, input_y);

				input_x = tpd2_WARP_X(abs_x_max, input_x);
				input_y = tpd2_WARP_Y(abs_y_max, input_y);
				//tpd2_calibrate_driver(&input_x, &input_y);   // jhu modified 20190925
                                if(input_x<1025)
	                              input_x = 1024 - input_x;     
				  
				GTP_DEBUG("Touch point after calibration: [X:%04d, Y:%04d]", input_x, input_y);

#ifdef CONFIG_GTP_CHANGE_X2Y
				temp  = input_x;
				input_x = input_y;
				input_y = temp;
#endif

				tpd2_down(input_x, input_y, input_w, id);
			}
		} else if (pre_touch) {
			GTP_DEBUG("Touch Release!");
			tpd2_up(0, 0, 0);
		} else {
			GTP_DEBUG("Additional Eint!");
		}
		pre_touch = touch_num;
		/* input_report_key(tpd2->dev, BTN_TOUCH, (touch_num || key_value)); */

		if (tpd2 != NULL && tpd2->dev != NULL)
			input_sync(tpd2->dev);



		if (!gtp_rawdiff_mode) {
			ret = ti941_i2c_write(i2c_client_point, end_cmd, 3);

			if (ret < 0)
				GTP_INFO("I2C write end_cmd  error!");

		}
}
#endif
exit_work_func:
		enable_irq(touch_irq);

		mutex_unlock(&i2c_access);

	} while (!kthread_should_stop());

	return 0;
}

static int tpd2_local_init(void)
{   
	// int retval;

    pr_info("ti941 touch panel tpd2_local_init 2023-05-25 \r\n ");
	// tpd2->reg = regulator_get(tpd2->tpd2_dev, "vtouch");
	// retval = regulator_set_voltage(tpd2->reg, 2800000, 3300000);
	// if (retval != 0) {
	// 	tpd2_DMESG("Failed to set reg-vgp6 voltage: %d\n", retval);
	// 	return -1;
	// }
	//msleep(5000);
	if (i2c_add_driver(&tpd2_i2c_driver) != 0) {
		GTP_INFO("tpd2_local_init unable to add i2c driver.");
		return -1;
	}

	if (tpd2_load_status == 0)	{
		/* if(tpd2_load_status == 0) // disable auto load touch driver for linux3.0 porting */
		GTP_INFO("add error touch panel driver.");
		i2c_del_driver(&tpd2_i2c_driver);
		return -1;
	}
	input_set_abs_params(tpd2->dev, ABS_MT_TRACKING_ID, 0, (GTP_MAX_TOUCH-1), 0, 0);
	if (tpd2_dts_data.use_tpd2_button) {
		/*initialize tpd2 button data*/
		tpd2_button_setting(tpd2_dts_data.tpd2_key_num, tpd2_dts_data.tpd2_key_local,
		tpd2_dts_data.tpd2_key_dim_local);
	}
	tpd2_button_setting_ti941(tpd2_KEY_COUNT,tpd2_keys_local);
	// tpd2_button_init();

#if (defined(tpd2_WARP_START) && defined(tpd2_WARP_END))
	tpd2_DO_WARP = 1;
	memcpy(tpd2_wb_start, tpd2_wb_start_local, tpd2_WARP_CNT * 4);
	memcpy(tpd2_wb_end, tpd2_wb_start_local, tpd2_WARP_CNT * 4);
#endif

#if (defined(tpd2_HAVE_CALIBRATION) && !defined(tpd2_CUSTOM_CALIBRATION))
	/* memcpy(tpd2_calmat, tpd2_def_calmat_local, 8 * 4); */
	/* memcpy(tpd2_def_calmat, tpd2_def_calmat_local, 8 * 4); */

#ifdef CONFIG_MTK_BOOT
	if (get_boot_mode() == FACTORY_BOOT) {
		tpd2_DEBUG("Factory mode is detected!\n");
		memcpy(tpd2_calmat, tpd2_def_calmat_local_factory, 8 * 4);
		memcpy(tpd2_def_calmat, tpd2_def_calmat_local_factory, 8 * 4);
	} else {
#endif
		tpd2_DEBUG("Normal mode is detected!\n");
		memcpy(tpd2_calmat, tpd2_def_calmat_local_normal, 8 * 4);
		memcpy(tpd2_def_calmat, tpd2_def_calmat_local_normal, 8 * 4);
#ifdef CONFIG_MTK_BOOT
	}
#endif
#endif

	/* set vendor string */
	tpd2->dev->id.vendor = 0x00;
	tpd2->dev->id.product = tpd2_info.pid;
	tpd2->dev->id.version = tpd2_info.vid;

	GTP_INFO("end %s, %d", __func__, __LINE__);
	tpd2_type_cap = 1;

	return 0;
}


/*
******************************************************
Function:
	Eter sleep function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
******************************************************
*/
// static s8 gtp_enter_sleep(struct i2c_client *client)
// {
// 	s8 ret = -1;
// #ifndef CONFIG_GTP_POWER_CTRL_SLEEP
// 	s8 retry = 0;
// 	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 5};

// 	tpd2_gpio_output(GTP_INT_PORT, 0);
// 	//gpio_direction_output(tpd2_int_gpio_number, 0);
// 	msleep(20);

// 	while (retry++ < 5) {
// 		ret = ti941_i2c_write(client, i2c_control_buf, 3);

// 		if (ret > 0) {
// 			GTP_INFO("GTP enter sleep!");
// 			return ret;
// 		}
// 		msleep(20);
// 	}

// #else

// 	tpd2_gpio_output(GTP_RST_PORT, 0);
// 	//gpio_direction_output(tpd2_rst_gpio_number, 0);
// 	msleep(20);

// 	// ret = regulator_disable(tpd2->reg);
// 	// if (ret != 0)
// 	// 	tpd2_DMESG("Failed to disable reg-vgp6: %d\n", ret);

// 	GTP_INFO("GTP enter sleep!");
// 	return 0;

// #endif
// 	GTP_ERROR("GTP send sleep cmd failed.");
// 	return ret;
// }

/*
******************************************************
Function:
	Wakeup from sleep mode Function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
******************************************************
*/
// static s8 gtp_wakeup_sleep(struct i2c_client *client)
// {
// 	u8 retry = 0;
// 	s8 ret = -1;

// 	GTP_INFO("GTP wakeup begin.");
// #ifdef CONFIG_GTP_POWER_CTRL_SLEEP

// 	while (retry++ < 5) {
// 		ret = tpd2_power_on(client);

// 		if (ret < 0)
// 			GTP_ERROR("I2C Power on ERROR!");

// 		ret = ti941_send_cfg(client);

// 		if (ret > 0) {
// 			GTP_DEBUG("Wakeup sleep send config success.");
// 			return ret;
// 		}
// 	}

// #else

// 	while (retry++ < 10) {
// 		tpd2_gpio_output(GTP_INT_PORT, 1);
// 		//gpio_direction_output(tpd2_int_gpio_number, 1);
// 		msleep(20);
// 		tpd2_gpio_output(GTP_INT_PORT, 0);
// 		//gpio_direction_output(tpd2_int_gpio_number, 0);
// 		msleep(20);
// 		ret = ti941_i2c_test(client);

// 		if (ret >= 0) {
// 			gtp_int_sync();
// 			return ret;
// 		}

// 	}

// #endif

// 	GTP_ERROR("GTP wakeup sleep failed.");
// 	return ret;
// }
/* Function to manage low power suspend */
static void tpd2_suspend(struct device *h)
{
	// s32 ret = -1;
	GTP_INFO("GTP suspend.");
	mutex_lock(&i2c_access);

	disable_irq(touch_irq);

	tpd2_halt = 1;
	mutex_unlock(&i2c_access);

	// ret = gtp_enter_sleep(i2c_client_point);
	// if (ret < 0)
	// 	GTP_ERROR("GTP early suspend failed.");

#ifdef CONFIG_GTP_ESD_PROTECT
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif


#ifdef GTP_PROXIMITY

	if (tpd2_proximity_flag == 1)
		return;

#endif
}

/* Function to manage power-on resume */
static void tpd2_resume(struct device *h)
{
	// s32 ret = -1;

	// ret = gtp_wakeup_sleep(i2c_client_point);

	// if (ret < 0)
	// 	GTP_ERROR("GTP later resume failed.");

	GTP_INFO("GTP wakeup sleep.");
	mutex_lock(&i2c_access);
	tpd2_halt = 0;

	enable_irq(touch_irq);

	mutex_unlock(&i2c_access);

#ifdef GTP_PROXIMITY
	if (tpd2_proximity_flag == 1)
		return;
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, tpd2_ESD_CHECK_CIRCLE);
#endif



}

// static void tpd2_off(void)
// {

// 	// int ret;

// 	// ret = regulator_disable(tpd2->reg);
// 	// if (ret != 0)
// 	// 	tpd2_DMESG("Failed to disable reg-vgp6: %d\n", ret);

// 	GTP_INFO("GTP enter sleep!");

// 	tpd2_halt = 1;
// 	disable_irq(touch_irq);
// }

// static void tpd2_on(void)
// {
// 	s32 ret = -1, retry = 0;

// 	while (retry++ < 5) {
// 		ret = tpd2_power_on(i2c_client_point);

// 		if (ret < 0)
// 			GTP_ERROR("I2C Power on ERROR!");

// 		ret = ti941_send_cfg(i2c_client_point);

// 		if (ret > 0)
// 			GTP_DEBUG("Wakeup sleep send config success.");
// 	}
// 	if (ret < 0)
// 		GTP_ERROR("GTP later resume failed.");

// 	enable_irq(touch_irq);
// 	tpd2_halt = 0;
// }
static struct tpd2_driver_t tpd2_device_driver = {

	.tpd2_device_name = "ti941_two",
	.tpd2_local_init = tpd2_local_init,
	.suspend = tpd2_suspend,
	.resume = tpd2_resume,
	.attrs = {
		.attr = gt9xx_attrs,
		.num  = ARRAY_SIZE(gt9xx_attrs),
	},
};

/* called when loaded into kernel */
//static int __init tpd2_driver_init(void)
void tpd2_driver_init_two(void)
{   
	GTP_INFO("MediaTek ti941 touch panel driver init");
	pr_info("ti941 touch panel tpd2_driver_init \r\n ");
	tpd2_get_dts_info();
	if (tpd2_driver_add(&tpd2_device_driver) < 0){
		GTP_INFO("add generic driver failed");
	pr_info("ti941 tpd2_driver_add device failed \r\n ");
	}
	//return 0; 
}

/* should never be called */
//static void __exit tpd2_driver_exit(void)
void tpd2_driver_exit_two(void)
{
	GTP_INFO("MediaTek ti941 touch panel driver exit");
	/* input_unregister_device(tpd2->dev); */
	tpd2_driver_remove(&tpd2_device_driver);
}
EXPORT_SYMBOL_GPL(tpd2_driver_init_two);
EXPORT_SYMBOL_GPL(tpd2_driver_exit_two);
//module_init(tpd2_driver_init);
//module_exit(tpd2_driver_exit);
