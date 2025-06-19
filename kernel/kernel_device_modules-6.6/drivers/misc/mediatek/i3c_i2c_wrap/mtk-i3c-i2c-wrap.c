// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * Author: Mingchang Jia <mingchang.jia@mediatek.com>
 */

#include "mtk-i3c-i2c-wrap.h"

#define WRAP_INFO "i3c_i2c_wrap"


int i3c_i2c_transfer(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_i2c_xfer *xfers, int nxfers)
{
	struct i3c_priv_xfer *i3c_xfer;
	struct i2c_msg *i2c_xfer;
	int idx = 0;
	int ret = 0;

	if (!i3c_i2c_dev || !xfers || (nxfers <= 0)) {
		pr_info("[%s][%s] para error,%p,%p,%d\n",
			WRAP_INFO, __func__, i3c_i2c_dev, xfers, nxfers);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev) {
			pr_info("[%s][%s] i3c_dev is NULL\n", WRAP_INFO, __func__);
			return -EINVAL;
		}
		i3c_xfer = kzalloc(sizeof(*i3c_xfer) * nxfers, GFP_KERNEL);
		if (!i3c_xfer)
			return -ENOMEM;

		for (idx = 0; idx < nxfers; idx++) {
			i3c_xfer[idx].rnw = xfers[idx].flags & 0xFF;
			i3c_xfer[idx].len = xfers[idx].len;
			i3c_xfer[idx].data.in = xfers[idx].buf;
		}
		ret = i3c_device_do_priv_xfers(i3c_i2c_dev->i3c_dev, i3c_xfer, nxfers);
		for (idx = 0; idx < nxfers; idx++)
			xfers[idx].err = i3c_xfer[idx].err;

		kfree(i3c_xfer);
	} else if (i3c_i2c_dev->protocol == I2C_PROTOCOL) {
		if (!i3c_i2c_dev->i2c_dev) {
			pr_info("[%s][%s] i2c_dev is NULL\n", WRAP_INFO, __func__);
			return -EINVAL;
		}
		i2c_xfer = kzalloc(sizeof(*i2c_xfer) * nxfers, GFP_KERNEL);
		if (!i2c_xfer)
			return -ENOMEM;

		for (idx = 0; idx < nxfers; idx++) {
			i2c_xfer[idx].addr = xfers[idx].addr;
			i2c_xfer[idx].flags = xfers[idx].flags;
			i2c_xfer[idx].len = xfers[idx].len;
			i2c_xfer[idx].buf = xfers[idx].buf;
		}
		ret = i2c_transfer(i3c_i2c_dev->i2c_dev->adapter, i2c_xfer, nxfers);
		kfree(i2c_xfer);
		if (ret == nxfers)
			ret = 0;
		else if (ret >= 0)
			ret = -EIO;
	} else {
		pr_info("[%s][%s] UNKNOWN_I3C_I2C_PRO\n", WRAP_INFO, __func__);
		return -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(i3c_i2c_transfer);

//void mtk_i3c_i2c_device_get_info(struct i3c_i2c_device *i3c_i2c_dev,
//	struct i3c_device_info *info)
//{
//	if (!i3c_i2c_dev) {
//		pr_info("[%s][%s] i3c_i2c_dev is NULL,%p\n",
//			WRAP_INFO, __func__, i3c_i2c_dev);
//		return;
//	}
//	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
//		if (!i3c_i2c_dev->i3c_dev || !info) {
//			pr_info("[%s][%s] i3c_dev or info is NULL,%p\n",
//				WRAP_INFO, __func__, info);
//			return;
//		}
//		i3c_device_get_info(i3c_i2c_dev->i3c_dev, info);
//	}
//}
//EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_get_info);

int mtk_i3c_i2c_device_set_info(struct i3c_i2c_device *i3c_i2c_dev,
	const struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev or info is NULL,%p\n",
			WRAP_INFO, __func__, i3c_i2c_dev);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = i3c_master_set_info(master, info);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_set_info);

int mtk_i3c_i2c_device_disec(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 evts)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		down_write(&master->bus.lock);
		ret = i3c_master_disec_locked(master, send_addr, evts);
		up_write(&master->bus.lock);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_disec);

int mtk_i3c_i2c_device_enec(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 evts)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		down_write(&master->bus.lock);
		ret = i3c_master_enec_locked(master, send_addr, evts);
		up_write(&master->bus.lock);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_enec);

int mtk_i3c_i2c_device_entdaa(struct i3c_i2c_device *i3c_i2c_dev)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		down_write(&master->bus.lock);
		ret = i3c_master_entdaa_locked(master);
		up_write(&master->bus.lock);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_entdaa);

int mtk_i3c_i2c_device_defslvs(struct i3c_i2c_device *i3c_i2c_dev)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		down_write(&master->bus.lock);
		ret = i3c_master_defslvs_locked(master);
		up_write(&master->bus.lock);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_defslvs);

int mtk_i3c_i2c_device_get_free_addr(struct i3c_i2c_device *i3c_i2c_dev,
	u8 start_addr)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = i3c_master_get_free_addr(master, start_addr);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_get_free_addr);

int mtk_i3c_i2c_device_add_i3c_dev(
	struct i3c_i2c_device *i3c_i2c_dev, u8 addr)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		down_write(&master->bus.lock);
		ret = i3c_master_add_i3c_dev_locked(master, addr);
		up_write(&master->bus.lock);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_add_i3c_dev);

int mtk_i3c_i2c_device_do_daa(struct i3c_i2c_device *i3c_i2c_dev)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = i3c_master_do_daa(master);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_do_daa);

int mtk_i3c_i2c_device_do_setdasa(struct i3c_i2c_device *i3c_i2c_dev)
{
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev) {
			pr_info("[%s][%s] i3c_dev is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = i3c_device_do_setdasa(i3c_i2c_dev->i3c_dev);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_do_setdasa);


#ifdef MTK_I3C_MASTER_API_EN

int mtk_i3c_i2c_device_entasx(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, u8 entas_mode)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		ret = mtk_i3c_master_entasx(master, send_addr, entas_mode);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_entasx);

int mtk_i3c_i2c_device_rstdaa(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		ret = mtk_i3c_master_rstdaa(master, send_addr);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_rstdaa);

int mtk_i3c_i2c_device_setmwl(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, struct i3c_ccc_mwl *mwl)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!mwl || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] mwl or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, mwl, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		ret = mtk_i3c_master_setmwl(master,
			&i3c_i2c_dev->i3c_dev->desc->info, send_addr, mwl);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_setmwl);

int mtk_i3c_i2c_device_setmrl(struct i3c_i2c_device *i3c_i2c_dev,
	u8 addr, struct i3c_ccc_mrl *mrl)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;
	u8 send_addr = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!mrl || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] mrl or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, mrl, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		if (addr == I3C_BROADCAST_ADDR)
			send_addr = I3C_BROADCAST_ADDR;
		else
			send_addr = i3c_i2c_dev->i3c_dev->desc->info.dyn_addr;
		ret = mtk_i3c_master_setmrl(master,
			&i3c_i2c_dev->i3c_dev->desc->info, send_addr, mrl);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_setmrl);

int mtk_i3c_i2c_device_enttm(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_ccc_enttm *enttm)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!enttm || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] enttm or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, enttm, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_enttm(master, enttm);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_enttm);

int mtk_i3c_i2c_device_setdasa(struct i3c_i2c_device *i3c_i2c_dev)
{
	struct i3c_master_controller *master = NULL;
	struct i3c_device_info *info = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		info = &i3c_i2c_dev->i3c_dev->desc->info;
		ret = mtk_i3c_master_setdasa(master, info,
			info->static_addr, info->dyn_addr);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_setdasa);

int mtk_i3c_i2c_device_setnewda(struct i3c_i2c_device *i3c_i2c_dev,
	u8 newaddr)
{
	struct i3c_master_controller *master = NULL;
	struct i3c_device_info *info = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		info = &i3c_i2c_dev->i3c_dev->desc->info;
		ret = mtk_i3c_master_setnewda(master, info,
			info->dyn_addr, newaddr);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_setnewda);

int mtk_i3c_i2c_device_getmwl(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getmwl(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getmwl);

int mtk_i3c_i2c_device_getmrl(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getmrl(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getmrl);

int mtk_i3c_i2c_device_getpid(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getpid(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getpid);

int mtk_i3c_i2c_device_getbcr(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getbcr(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getbcr);

int mtk_i3c_i2c_device_getdcr(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getdcr(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getdcr);

int mtk_i3c_i2c_device_getmxds(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_getmxds(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_getmxds);

int mtk_i3c_i2c_device_gethdrcap(struct i3c_i2c_device *i3c_i2c_dev,
	struct i3c_device_info *info)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!info || !i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] info or i3c_dev or desc is NULL,%p,%p\n",
				WRAP_INFO, __func__, info, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_gethdrcap(master, &i3c_i2c_dev->i3c_dev->desc->info);
		if (ret)
			pr_info("[%s][%s] fail.\n", WRAP_INFO, __func__);
		else
			*info = i3c_i2c_dev->i3c_dev->desc->info;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_gethdrcap);

int mtk_i3c_i2c_device_change_i3c_speed(struct i3c_i2c_device *i3c_i2c_dev,
	unsigned int scl_rate_i3c)
{
	struct i3c_master_controller *master = NULL;
	int ret = 0;

	if (!i3c_i2c_dev || !scl_rate_i3c) {
		pr_info("[%s][%s] i3c_i2c_dev is NULL or scl_rate_i3c=%u\n",
			WRAP_INFO, __func__, scl_rate_i3c);
		return -EINVAL;
	}
	if (i3c_i2c_dev->protocol == I3C_PROTOCOL) {
		if (!i3c_i2c_dev->i3c_dev || !i3c_i2c_dev->i3c_dev->desc) {
			pr_info("[%s][%s] i3c_dev or desc is NULL,%p\n",
				WRAP_INFO, __func__, i3c_i2c_dev->i3c_dev);
			return -EINVAL;
		}
		master = i3c_dev_get_master(i3c_i2c_dev->i3c_dev->desc);
		if (!master) {
			pr_info("[%s][%s] master is NULL\n",
				WRAP_INFO, __func__);
			return -EINVAL;
		}
		ret = mtk_i3c_master_change_i3c_speed(master, scl_rate_i3c);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_device_change_i3c_speed);

#endif

static int mtk_i2c_probe_new(struct i2c_client *client)
{
	struct i2c_driver *i2c_drv = to_i2c_driver(client->dev.driver);
	struct i3c_i2c_driver *drv;
	struct i3c_i2c_device i3c_i2c_dev;

	if (!i2c_drv) {
		pr_info("[%s][%s] i2c_drv is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	drv = container_of(i2c_drv, struct i3c_i2c_driver, _i2c_drv);
	if (!drv || !drv->probe) {
		pr_info("[%s][%s] drv=%p,drv->probe is NULL\n",
			WRAP_INFO, __func__, drv);
		return -EINVAL;
	}
	i3c_i2c_dev.i3c_dev = NULL;
	i3c_i2c_dev.i2c_dev = client;
	i3c_i2c_dev.id = NULL;
	i3c_i2c_dev.protocol = I2C_PROTOCOL;

	return drv->probe(&i3c_i2c_dev);
}

static void mtk_i2c_remove(struct i2c_client *client)
{
	struct i2c_driver *i2c_drv = to_i2c_driver(client->dev.driver);
	struct i3c_i2c_driver *drv;
	struct i3c_i2c_device i3c_i2c_dev;

	if (!i2c_drv) {
		pr_info("[%s][%s] i2c_drv is NULL\n",
			WRAP_INFO, __func__);
		return;
	}
	drv = container_of(i2c_drv, struct i3c_i2c_driver, _i2c_drv);
	if (!drv || !drv->remove) {
		pr_info("[%s][%s] drv=%p,drv->remove is NULL\n",
			WRAP_INFO, __func__, drv);
		return;
	}
	i3c_i2c_dev.i3c_dev = NULL;
	i3c_i2c_dev.i2c_dev = client;
	i3c_i2c_dev.id = NULL;
	i3c_i2c_dev.protocol = I2C_PROTOCOL;

	drv->remove(&i3c_i2c_dev);
}

static void mtk_i2c_shutdown(struct i2c_client *client)
{
	struct i2c_driver *i2c_drv = to_i2c_driver(client->dev.driver);
	struct i3c_i2c_driver *drv;
	struct i3c_i2c_device i3c_i2c_dev;

	if (!i2c_drv) {
		pr_info("[%s][%s] i2c_drv is NULL\n",
			WRAP_INFO, __func__);
		return;
	}
	drv = container_of(i2c_drv, struct i3c_i2c_driver, _i2c_drv);
	if (!drv || !drv->shutdown) {
		pr_info("[%s][%s] drv=%p,drv->shutdown is NULL\n",
			WRAP_INFO, __func__, drv);
		return;
	}
	i3c_i2c_dev.i3c_dev = NULL;
	i3c_i2c_dev.i2c_dev = client;
	i3c_i2c_dev.id = NULL;
	i3c_i2c_dev.protocol = I2C_PROTOCOL;

	drv->shutdown(&i3c_i2c_dev);
}

static int mtk_i3c_probe(struct i3c_device *i3cdev)
{
	struct i3c_driver *i3c_drv = drv_to_i3cdrv(i3cdev->dev.driver);
	struct i3c_i2c_driver *drv;
	struct i3c_i2c_device i3c_i2c_dev;

	if (!i3c_drv) {
		pr_info("[%s][%s] i3c_drv is NULL\n",
			WRAP_INFO, __func__);
		return -EINVAL;
	}
	drv = container_of(i3c_drv, struct i3c_i2c_driver, _i3c_drv);
	if (!drv || !drv->probe) {
		pr_info("[%s][%s] drv=%p,drv->probe is NULL\n",
			WRAP_INFO, __func__, drv);
		return -EINVAL;
	}
	i3c_i2c_dev.i3c_dev = i3cdev;
	i3c_i2c_dev.i2c_dev = NULL;
	i3c_i2c_dev.id = NULL;
	i3c_i2c_dev.protocol = I3C_PROTOCOL;

	return drv->probe(&i3c_i2c_dev);
}

static void mtk_i3c_remove(struct i3c_device *i3cdev)
{
	struct i3c_driver *i3c_drv = drv_to_i3cdrv(i3cdev->dev.driver);
	struct i3c_i2c_driver *drv;
	struct i3c_i2c_device i3c_i2c_dev;

	if (!i3c_drv) {
		pr_info("[%s][%s] i3c_drv is NULL\n",
			WRAP_INFO, __func__);
		return;
	}
	drv = container_of(i3c_drv, struct i3c_i2c_driver, _i3c_drv);
	if (!drv || !drv->remove) {
		pr_info("[%s][%s] drv=%p,drv->remove is NULL\n",
			WRAP_INFO, __func__, drv);
		return;
	}
	i3c_i2c_dev.i3c_dev = i3cdev;
	i3c_i2c_dev.i2c_dev = NULL;
	i3c_i2c_dev.id = NULL;
	i3c_i2c_dev.protocol = I3C_PROTOCOL;

	drv->remove(&i3c_i2c_dev);
}

int mtk_i3c_i2c_driver_register(struct i3c_i2c_driver *drv)
{
	struct i3c_driver *i3c_drv = &drv->_i3c_drv;
	struct i2c_driver *i2c_drv = &drv->_i2c_drv;
	int ret = 0;

	i2c_drv->driver = drv->driver;
	i2c_drv->probe = mtk_i2c_probe_new;
	i2c_drv->remove = mtk_i2c_remove;
	if (drv->id_table)
		i2c_drv->id_table = drv->id_table->i2c_id_table;
	else
		i2c_drv->id_table = NULL;
	i2c_drv->shutdown = mtk_i2c_shutdown;
	i2c_drv->flags = drv->flags;
	ret = i2c_add_driver(i2c_drv);
	if (ret) {
		pr_info("[%s][%s] i2c_add_driver error, ret=%d\n",
			WRAP_INFO, __func__, ret);
		return ret;
	}

	i3c_drv->driver = drv->driver;
	i3c_drv->probe = mtk_i3c_probe;
	i3c_drv->remove = mtk_i3c_remove;
	if ((!drv->id_table) || (!drv->id_table->i3c_id_table))
		pr_info("[%s][%s] skip i3c_driver_register\n", WRAP_INFO, __func__);
	else {
		i3c_drv->id_table = drv->id_table->i3c_id_table;
		ret = i3c_driver_register(i3c_drv);
		if (ret)
			pr_info("[%s][%s] i3c_driver_register error, ret=%d\n",
				WRAP_INFO, __func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_driver_register);

void mtk_i3c_i2c_driver_unregister(struct i3c_i2c_driver *drv)
{
	struct i3c_driver *i3c_drv = &drv->_i3c_drv;
	struct i2c_driver *i2c_drv = &drv->_i2c_drv;

	i2c_del_driver(i2c_drv);

	if ((!drv->id_table) || (!drv->id_table->i3c_id_table))
		pr_info("[%s][%s] skip i3c_driver_unregister\n", WRAP_INFO, __func__);
	else
		i3c_driver_unregister(i3c_drv);

}
EXPORT_SYMBOL_GPL(mtk_i3c_i2c_driver_unregister);

MODULE_AUTHOR("Mingchang Jia <mingchang.jia@mediatek.com>");
MODULE_DESCRIPTION("MTK I3C I2C WRAP driver");
MODULE_LICENSE("GPL");
