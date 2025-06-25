/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef MTK_I3C_I2C_WRAP_H
#define MTK_I3C_I2C_WRAP_H


#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i3c/master.h>
#include <linux/i3c/device.h>
#include <linux/i3c/ccc.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_I3C_MASTER_MT69XX)
#include "mtk-i3c-master-api.h"
#define MTK_I3C_MASTER_API_EN
#endif

enum I3C_I2C_PROTOLCOL {
	UNKNOWN_I3C_I2C_PRO,
	I3C_PROTOCOL,
	I2C_PROTOCOL,
};

struct i3c_i2c_xfer {
	__u16 addr;
	__u16 flags;
	__u16 len;
	__u8 *buf;
	enum i3c_error_code err;
};


struct i3c_i2c_device {
	struct i3c_device *i3c_dev;
	struct i2c_client *i2c_dev;
	const struct i2c_device_id *id;
	enum I3C_I2C_PROTOLCOL protocol;
};

struct i3c_i2c_device_id {
	const struct i3c_device_id *i3c_id_table;
	const struct i2c_device_id *i2c_id_table;
};


struct i3c_i2c_driver {
	struct i3c_driver _i3c_drv;
	struct i2c_driver _i2c_drv;
	struct device_driver driver;
	int (*probe)(struct i3c_i2c_device *dev);
	void (*remove)(struct i3c_i2c_device *dev);
	struct i3c_i2c_device_id *id_table;
	//int (*probe_new)(struct i3c_i2c_device *dev);
	void (*shutdown)(struct i3c_i2c_device *dev);
	u32 flags;
};

#ifdef MTK_I3C_MASTER_API_EN

int mtk_i3c_i2c_device_entasx(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 entas_mode);
int mtk_i3c_i2c_device_rstdaa(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr);
int mtk_i3c_i2c_device_setmwl(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, struct i3c_ccc_mwl *mwl);
int mtk_i3c_i2c_device_setmrl(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, struct i3c_ccc_mrl *mrl);
int mtk_i3c_i2c_device_enttm(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_ccc_enttm *enttm);
int mtk_i3c_i2c_device_setdasa(struct i3c_i2c_device *i3c_i2c_dev);
int mtk_i3c_i2c_device_setnewda(struct i3c_i2c_device *i3c_i2c_dev,
	u8 newaddr);
int mtk_i3c_i2c_device_getmwl(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_getmrl(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_getpid(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_getbcr(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_getdcr(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_getmxds(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_gethdrcap(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info);
int mtk_i3c_i2c_device_change_i3c_speed(struct i3c_i2c_device *i3c_i2c_dev,
	unsigned int scl_rate_i3c);

#endif

int i3c_i2c_transfer(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_i2c_xfer *xfers, int nxfers);
//void mtk_i3c_i2c_device_get_info(struct i3c_i2c_device *i3c_i2c_dev,
//	struct i3c_device_info *info);
int mtk_i3c_i2c_device_set_info(struct i3c_i2c_device *i3c_i2c_dev,
	const struct i3c_device_info *info);
int mtk_i3c_i2c_device_disec(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 evts);
int mtk_i3c_i2c_device_enec(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 evts);
int mtk_i3c_i2c_device_entdaa(struct i3c_i2c_device *i3c_i2c_dev);
int mtk_i3c_i2c_device_defslvs(struct i3c_i2c_device *i3c_i2c_dev);
int mtk_i3c_i2c_device_get_free_addr(struct i3c_i2c_device *i3c_i2c_dev,
	u8 start_addr);
int mtk_i3c_i2c_device_add_i3c_dev(
	struct i3c_i2c_device *i3c_i2c_dev, u8 addr);
int mtk_i3c_i2c_device_do_daa(struct i3c_i2c_device *i3c_i2c_dev);
int mtk_i3c_i2c_device_do_setdasa(struct i3c_i2c_device *i3c_i2c_dev);

int mtk_i3c_i2c_driver_register(struct i3c_i2c_driver *drv);
void mtk_i3c_i2c_driver_unregister(struct i3c_i2c_driver *drv);

#define module_mtk_i3c_i2c_driver(__i3c_i2c_drv)    \
		module_driver(__i3c_i2c_drv,                \
					mtk_i3c_i2c_driver_register,    \
					mtk_i3c_i2c_driver_unregister)

#endif /* MTK_I3C_I2C_WRAP_H */
