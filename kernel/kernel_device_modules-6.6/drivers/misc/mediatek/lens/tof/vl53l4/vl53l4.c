// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "vl53l4.h"
#include "vl53lx_platform.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#define DRIVER_NAME "vl53l4"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define VL53L4_NAME				"vl53l4"

#define VL53L4_CTRL_DELAY_US			5000

#define VL53L4_I2C_SLAVE_ADDR 0x52

#define DISTANCEMODE_SHORT             ((uint8_t)  1)
#define DISTANCEMODE_MEDIUM            ((uint8_t)  2)
#define DISTANCEMODE_LONG              ((uint8_t)  3)

struct VL53LX_Dev_t dev;
struct VL53LX_Dev_t *Dev = &dev;

// debug command
static int32_t vl53l4_log_dbg_en;
static int32_t vl53l4_start_measure;

static int g_is_tof_support;

/*VL53L4 Workqueue */
static struct workqueue_struct *vl53l4_init_wq;
static struct work_struct vl53l4_init_work;

/* vl53l4 device structure */
struct vl53l4_device {
	int driver_init;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

#define NUMBER_OF_MAX_TOF_DATA 64
struct TofInformation {
	int32_t is_tof_supported;
	int32_t num_of_rows; /* Max : 8 */
	int32_t num_of_cols; /* Max : 8 */
	int32_t ranging_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t dmax_distance[NUMBER_OF_MAX_TOF_DATA];
	int32_t error_status[NUMBER_OF_MAX_TOF_DATA];
	int32_t maximal_distance; /* Operating Range Distance */
	int64_t timestamp;
	int32_t SignalRate[NUMBER_OF_MAX_TOF_DATA];
	int32_t AmbientRate[NUMBER_OF_MAX_TOF_DATA];
	int32_t tof_id;
	int32_t fov_d;
};

struct mtk_tof_info {
	struct TofInformation *p_tof_info;
};

enum {
	// reference laser ranging distance
	STATUS_RANGING_VALID			= 0x00,
	// search range [DMAX : infinity]
	STATUS_MOVE_DMAX				= 0x01,
	// search range [xx cm : infinity], xx is the laser max ranging distance
	STATUS_MOVE_MAX_RANGING_DIST	= 0x02,
	STATUS_NOT_REFERENCE			= 0x03
} LASER_STATUS_T;

struct vl53l4_device *g_vl53l4;

/* Control commnad */
#define VIDIOC_MTK_G_TOF_INIT _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_tof_info)
#define VIDIOC_MTK_G_TOF_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct mtk_tof_info)

static inline struct vl53l4_device *to_vl53l4_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct vl53l4_device, ctrls);
}

static inline struct vl53l4_device *sd_to_vl53l4_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vl53l4_device, sd);
}

static int vl53l4_release(struct vl53l4_device *vl53l4)
{
	return 0;
}

static int vl53l4_init(struct vl53l4_device *vl53l4)
{
	struct i2c_client *client = v4l2_get_subdevdata(&vl53l4->sd);
	uint8_t i2c_ret = 0;
	int vl53status = 0;
	uint8_t currentDist;

	LOG_INF("[%s] %p\n", __func__, client);

	if (g_is_tof_support < 0) {
		LOG_INF("not support tof\n");
		return -1;
	}

	client->addr = VL53L4_I2C_SLAVE_ADDR >> 1;

	VL53LX_RdByte(Dev, 0x010F, &i2c_ret);
	LOG_INF("ST API: vl53l4 0x010F: 0x%x\n", i2c_ret);

	if (!i2c_ret) {
		g_is_tof_support = -1;
		return -1;
	}

	VL53LX_RdByte(Dev, 0x0110, &i2c_ret);
	LOG_INF("ST API: vl53l4 0x0110: 0x%x\n", i2c_ret);

	// prepare for ranging
	vl53status = VL53LX_WaitDeviceBooted(Dev);
	LOG_INF("VL53LX_WaitDeviceBooted return value:%d\n", vl53status);
	vl53status = VL53LX_DataInit(Dev);
	LOG_INF("VL53LX_DataInit return value:%d\n", vl53status);

	// set distance mode to long
	vl53status = VL53LX_SetDistanceMode(Dev, DISTANCEMODE_LONG);
	LOG_INF("VL53LX_SetDistanceMode return value:%d\n", vl53status);

	// check distance mode
	vl53status = VL53LX_GetDistanceMode(Dev, &currentDist);
	LOG_INF("VL53LX_GetDistanceMode return value:%d, mode: %d\n",
		vl53status, (int)currentDist);

	vl53status = VL53LX_StartMeasurement(Dev);
	LOG_INF("VL53LX_StartMeasurement return value:%d\n", vl53status);

	if (vl53status) {
		LOG_INF("VL53LX_StartMeasurement failed: error = %d\n", vl53status);
		return -1;
	}

	return 0;
}

/* Power handling */
static int vl53l4_power_off(struct vl53l4_device *vl53l4)
{
	int ret;

	ret = vl53l4_release(vl53l4);
	if (ret)
		LOG_INF("vl53l4 release failed!\n");

	ret = regulator_disable(vl53l4->vin);
	if (ret)
		LOG_INF("regulator_disable vin failed!\n");

	ret = regulator_disable(vl53l4->vdd);
	if (ret)
		LOG_INF("regulator_disable vdd failed!\n");

	if (vl53l4->vcamaf_pinctrl && vl53l4->vcamaf_off)
		ret = pinctrl_select_state(vl53l4->vcamaf_pinctrl,
					vl53l4->vcamaf_off);

	return ret;
}

static int vl53l4_power_on(struct vl53l4_device *vl53l4)
{
	int ret;

	ret = regulator_enable(vl53l4->vin);
	if (ret < 0)
		LOG_INF("enable regulator vin fail\n");

	ret = regulator_enable(vl53l4->vdd);
	if (ret < 0)
		LOG_INF("enable regulator vdd fail\n");

	if (vl53l4->vcamaf_pinctrl && vl53l4->vcamaf_on)
		ret = pinctrl_select_state(vl53l4->vcamaf_pinctrl,
					vl53l4->vcamaf_on);

	if (ret < 0)
		LOG_INF("enable pinctrl fail\n");

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	/*
	 * Execute driver initialization in the first time getting
	 * TOF info to avoid increase preview launch waiting time
	 */
	vl53l4->driver_init = 1;

	usleep_range(VL53L4_CTRL_DELAY_US, VL53L4_CTRL_DELAY_US + 100);

	return 0;
}

static int vl53l4_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	LOG_INF("+\n");

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		LOG_INF("pm_runtime_get_sync failed\n");
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	LOG_INF("-\n");

	return 0;
}

static int vl53l4_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("+\n");

	if (g_is_tof_support >= 0)
		VL53LX_StopMeasurement(Dev);

	if (pm_runtime_put(sd->dev) < 0)
		LOG_INF("power down failed\n");

	LOG_INF("-\n");
	return 0;
}

static void vl53l4_read_tof_data(struct VL53LX_MultiRangingData_t *pMultiRangingData)
{
	int j = 0;
	int vl53status = 0;
	int no_of_object_found = 0;

	vl53status = VL53LX_WaitMeasurementDataReady(Dev);
	if (vl53l4_log_dbg_en)
		LOG_INF("VL53LX_WaitMeasurementDataReady return value:%d\n", vl53status);

	if (!vl53status) {
		vl53status = VL53LX_GetMultiRangingData(Dev, pMultiRangingData);
		if (vl53l4_log_dbg_en)
			LOG_INF("VL53LX_GetMultiRangingData return value:%d\n", vl53status);

		LOG_INF("RangeMax/RangeMin = %5d/%5d mm, Sigma = %5d mm, ExtRange = %d\n",
			pMultiRangingData->RangeData[j].RangeMaxMilliMeter,
			pMultiRangingData->RangeData[j].RangeMinMilliMeter,
			pMultiRangingData->RangeData[j].SigmaMilliMeter,
			pMultiRangingData->RangeData[j].ExtendedRange);

		no_of_object_found = pMultiRangingData->NumberOfObjectsFound;
		LOG_INF("Count = %5d, #Objs = %d\n",
			pMultiRangingData->StreamCount,
			no_of_object_found);

		for (j = 0; j < no_of_object_found; j++) {
			LOG_INF("status = %d, D = %5d mm, Signal = %d Mcps, Ambient = %d Mcps\n",
				pMultiRangingData->RangeData[j].RangeStatus,
				pMultiRangingData->RangeData[j].RangeMilliMeter,
				pMultiRangingData->RangeData[j].SignalRateRtnMegaCps/65536,
				pMultiRangingData->RangeData[j].AmbientRateRtnMegaCps/65536);
		}
		if (vl53status == 0)
			vl53status = VL53LX_ClearInterruptAndStartMeasurement(Dev);
	}
}

static int32_t vl53l4_common_status(int32_t tof_status)
{
	int32_t status = 0;

	switch (tof_status) {
	case 0:
	{
		status = STATUS_RANGING_VALID;
	}
	break;

	case 1:
	case 2:
	{
		status = STATUS_MOVE_DMAX;
	}
	break;

	case 4:
	{
		status = STATUS_MOVE_MAX_RANGING_DIST;
	}
	break;

	default:
		status = STATUS_NOT_REFERENCE;
		break;
	}

	LOG_INF("status for algo = %d, driver_status = %d\n", status, tof_status);
	return status;
}

static long vl53l4_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct vl53l4_device *vl53l4 = sd_to_vl53l4_ois(sd);

	switch (cmd) {

	case VIDIOC_MTK_G_TOF_INIT:
	{

		struct mtk_tof_info *info = arg;
		struct TofInformation tof_info;

		memset(&tof_info, 0, sizeof(struct TofInformation));

		LOG_INF("VIDIOC_MTK_G_TOF_INIT\n");

		/*
		 * Execute driver initialization in the first time getting
		 * TOF info to avoid increase preview launch waiting time
		 */
		if (vl53l4->driver_init == 1) {
			LOG_INF("vl53l4_init\n");
			ret = vl53l4_init(vl53l4);
			if (ret < 0) {
				LOG_INF("vl53l4_init fail\n");
				return ret;
			}

			vl53l4->driver_init = 0;
		}

		//tof_info.is_tof_supported = 1;
		if (copy_to_user((void *)info->p_tof_info, &tof_info, sizeof(tof_info)))
			ret = -EFAULT;
	}
	break;

	case VIDIOC_MTK_G_TOF_INFO:
	{
		struct mtk_tof_info *info = arg;
		struct TofInformation tof_info;
		struct VL53LX_MultiRangingData_t MultiRangingData = {};
		struct VL53LX_MultiRangingData_t *pMultiRangingData = &MultiRangingData;
		int i = 0;

		if (vl53l4_log_dbg_en)
			LOG_INF("VIDIOC_MTK_G_TOF_GETDATAS +\n");

		memset(&tof_info, 0, sizeof(struct TofInformation));

		tof_info.is_tof_supported = 1;
		tof_info.maximal_distance = 5000; /* Unit : mm */
		tof_info.tof_id = 0x5304;
		tof_info.fov_d = 18; /* Unit : degree */

		vl53l4_read_tof_data(pMultiRangingData);
		if (pMultiRangingData == NULL) {
			LOG_INF("vl53l4_read_tof_data fail\n");
			return -1;
		}

		tof_info.num_of_rows = pMultiRangingData->NumberOfObjectsFound;
		if (tof_info.num_of_rows < 1)
			tof_info.num_of_rows = 1;
		tof_info.num_of_cols = 1;

		for (i = 0; i < pMultiRangingData->NumberOfObjectsFound; i++) {
			tof_info.ranging_distance[i] =
				pMultiRangingData->RangeData[i].RangeMilliMeter;

			tof_info.error_status[i] =
				vl53l4_common_status(pMultiRangingData->RangeData[i].RangeStatus);

			tof_info.SignalRate[i] =
				pMultiRangingData->RangeData[i].SignalRateRtnMegaCps;
			tof_info.AmbientRate[i] =
				pMultiRangingData->RangeData[i].AmbientRateRtnMegaCps;
		}

		if (copy_to_user((void *)info->p_tof_info, &tof_info, sizeof(tof_info))) {
			LOG_INF("copy_to_user failed\n");
			ret = -EFAULT;
		}

		if (vl53l4_log_dbg_en)
			LOG_INF("VIDIOC_MTK_G_TOF_GETDATAS -\n");
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

/* VL53L4 Workqueue for power measurement*/
static void vl53l4_init_fx(struct work_struct *data)
{
	int j = 0;
	int vl53status = 0;
	uint8_t NewDataReady = 0;
	struct VL53LX_MultiRangingData_t MultiRangingData;
	struct VL53LX_MultiRangingData_t *pMultiRangingData = &MultiRangingData;
	int no_of_object_found = 0;
	int count = 0;

	LOG_INF("+\n");

	vl53status = VL53LX_WaitDeviceBooted(Dev);
	LOG_INF("VL53LX_WaitDeviceBooted return value:%d\n", vl53status);
	vl53status = VL53LX_DataInit(Dev);
	LOG_INF("VL53LX_DataInit return value:%d\n", vl53status);
	vl53status = VL53LX_StartMeasurement(Dev);
	LOG_INF("VL53LX_StartMeasurement return value:%d\n", vl53status);

	if (vl53status)
		LOG_INF("VL53LX_StartMeasurement failed: error = %d\n", vl53status);

	while(1) {
		vl53status = VL53LX_GetMeasurementDataReady(Dev, &NewDataReady);
		if (vl53l4_log_dbg_en)
			LOG_INF("VL53LX_GetMeasurementDataReady return value:%d\n", vl53status);

		usleep_range(10000, 10100);
		if (!vl53status && (NewDataReady!=0)) {
			vl53status = VL53LX_GetMultiRangingData(Dev, pMultiRangingData);
			if (vl53l4_log_dbg_en)
				LOG_INF("VL53LX_GetMultiRangingData return value:%d\n", vl53status);

			LOG_INF("RangeMax = %5d mm, RangeMin = %5d mm\n",
				pMultiRangingData->RangeData[j].RangeMaxMilliMeter,
				pMultiRangingData->RangeData[j].RangeMinMilliMeter);
			LOG_INF("Sigma = %5d mm, ExtendedRange = %d\n",
				pMultiRangingData->RangeData[j].SigmaMilliMeter,
				pMultiRangingData->RangeData[j].ExtendedRange);

			no_of_object_found = pMultiRangingData->NumberOfObjectsFound;
			LOG_INF("Count = %5d\n", pMultiRangingData->StreamCount);
			LOG_INF("#Objs = %d\n", no_of_object_found);
			for (j = 0; j < no_of_object_found; j++) {
				LOG_INF("status = %d, D = %5d mm, Signal = %d Mcps, Ambient = %d Mcps\n",
					pMultiRangingData->RangeData[j].RangeStatus,
					pMultiRangingData->RangeData[j].RangeMilliMeter,
					pMultiRangingData->RangeData[j].SignalRateRtnMegaCps/65536,
					pMultiRangingData->RangeData[j].AmbientRateRtnMegaCps/65536);
			}
			if (vl53status == 0)
				vl53status = VL53LX_ClearInterruptAndStartMeasurement(Dev);
		}
		count++;
		if (count % 100 == 0)
			LOG_INF("start measure thread still alive!!!!!\n");

		if (vl53l4_start_measure == 0)
			break;
	}

	VL53LX_StopMeasurement(Dev);
	LOG_INF("VL53LX_StopMeasurement done\n");

	LOG_INF("VL53L4 Workqueue for power measurement end---------------------\n");

	LOG_INF("-\n");
}

static struct class *vl53l4_class;
static struct device *vl53l4_device;
static dev_t vl53l4_devno;
/* torch status sysfs */
static ssize_t vl53l4_debug_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t vl53l4_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int val = 0, ret = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		LOG_INF("ret fail\n");

	vl53l4_log_dbg_en = val & 0x1;
	vl53l4_start_measure = (val >> 1) & 0x1;

	LOG_INF("log(%d), start_measure(%d), buf:%s\n",
		vl53l4_log_dbg_en, vl53l4_start_measure, buf);

	if (vl53l4_start_measure) {
		LOG_INF("enable TOF measure\n");

		vl53l4_power_on(g_vl53l4);
		LOG_INF("vl53l4_power_on-------\n");

		/* VL53L4 Workqueue */
		if (vl53l4_init_wq == NULL) {
			vl53l4_init_wq =
				create_singlethread_workqueue("vl53l4_init_work");
			if (!vl53l4_init_wq) {
				LOG_INF("create_singlethread_workqueue fail\n");
				return -ENOMEM;
			}

			/* init work queue */
			INIT_WORK(&vl53l4_init_work, vl53l4_init_fx);
			queue_work(vl53l4_init_wq, &vl53l4_init_work);
		}
	} else {
		LOG_INF("disable TOF measure\n");

		/* VL53L4 Workqueue */
		if (vl53l4_init_wq) {
			LOG_INF("flush work queue\n");

			/* flush work queue */
			flush_work(&vl53l4_init_work);

			flush_workqueue(vl53l4_init_wq);
			destroy_workqueue(vl53l4_init_wq);
			vl53l4_init_wq = NULL;
		}

		vl53l4_power_off(g_vl53l4);
		LOG_INF("vl53l4_power_off-------\n");
	}

	return size;
}
static DEVICE_ATTR_RW(vl53l4_debug);

static const struct v4l2_subdev_internal_ops vl53l4_int_ops = {
	.open = vl53l4_open,
	.close = vl53l4_close,
};

static const struct v4l2_subdev_core_ops vl53l4_ops_core = {
	.ioctl = vl53l4_ops_core_ioctl,
};

static const struct v4l2_subdev_ops vl53l4_ops = {
	.core = &vl53l4_ops_core,
};

static void vl53l4_subdev_cleanup(struct vl53l4_device *vl53l4)
{
	v4l2_async_unregister_subdev(&vl53l4->sd);
	v4l2_ctrl_handler_free(&vl53l4->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&vl53l4->sd.entity);
#endif
}

static int vl53l4_init_controls(struct vl53l4_device *vl53l4)
{
	struct v4l2_ctrl_handler *hdl = &vl53l4->ctrls;

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	vl53l4->sd.ctrl_handler = hdl;

	return 0;
}

static int vl53l4_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct vl53l4_device *vl53l4;
	int ret;

	LOG_INF("tof probe\n");

	vl53l4 = devm_kzalloc(dev, sizeof(*vl53l4), GFP_KERNEL);
	if (!vl53l4)
		return -ENOMEM;

	client->addr = VL53L4_I2C_SLAVE_ADDR >> 1;

	Dev->client = client;

	vl53l4->vin = devm_regulator_get(dev, "camera_tof_vin");
	if (IS_ERR(vl53l4->vin)) {
		ret = PTR_ERR(vl53l4->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	vl53l4->vdd = devm_regulator_get(dev, "camera_tof_vdd");
	if (IS_ERR(vl53l4->vdd)) {
		ret = PTR_ERR(vl53l4->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	vl53l4->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(vl53l4->vcamaf_pinctrl)) {
		ret = PTR_ERR(vl53l4->vcamaf_pinctrl);
		vl53l4->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		vl53l4->vcamaf_on = pinctrl_lookup_state(
			vl53l4->vcamaf_pinctrl, "camera_tof_en_on");

		if (IS_ERR(vl53l4->vcamaf_on)) {
			ret = PTR_ERR(vl53l4->vcamaf_on);
			vl53l4->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		vl53l4->vcamaf_off = pinctrl_lookup_state(
			vl53l4->vcamaf_pinctrl, "camera_tof_en_off");

		if (IS_ERR(vl53l4->vcamaf_off)) {
			ret = PTR_ERR(vl53l4->vcamaf_off);
			vl53l4->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}
	LOG_INF("get pinctrl done\n");

	v4l2_i2c_subdev_init(&vl53l4->sd, client, &vl53l4_ops);
	vl53l4->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	vl53l4->sd.internal_ops = &vl53l4_int_ops;

	ret = vl53l4_init_controls(vl53l4);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&vl53l4->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	vl53l4->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&vl53l4->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	/* create class */
	vl53l4_class = class_create(VL53L4_NAME);
	if (IS_ERR(vl53l4_class)) {
		pr_info("Failed to create class (%d)\n",
				(int)PTR_ERR(vl53l4_class));
		goto err_create_vl53l4_class;
	}

	/* create device */
	vl53l4_device =
	    device_create(vl53l4_class, NULL, vl53l4_devno,
				NULL, VL53L4_NAME);
	if (!vl53l4_device) {
		pr_info("Failed to create device\n");
		goto err_create_vl53l4_device;
	}

	if (device_create_file(vl53l4_device, &dev_attr_vl53l4_debug)) {
		pr_info("Failed to create device file(vl53l4_debug)\n");
		goto err_create_vl53l4_device_file;
	}

	g_vl53l4 = vl53l4;
	g_is_tof_support = 1;
	LOG_INF("probe done\n");

	return 0;

err_create_vl53l4_device_file:
	device_destroy(vl53l4_class, vl53l4_devno);
	class_destroy(vl53l4_class);
	return 0;

err_create_vl53l4_device:
	class_destroy(vl53l4_class);
	return 0;

err_create_vl53l4_class:
	return 0;

err_cleanup:
	vl53l4_subdev_cleanup(vl53l4);
	return ret;
}

static void vl53l4_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l4_device *vl53l4 = sd_to_vl53l4_ois(sd);

	LOG_INF("tof remove\n");

	vl53l4_subdev_cleanup(vl53l4);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		vl53l4_power_off(vl53l4);
	pm_runtime_set_suspended(&client->dev);
}

static int __maybe_unused vl53l4_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l4_device *vl53l4 = sd_to_vl53l4_ois(sd);

	return vl53l4_power_off(vl53l4);
}

static int __maybe_unused vl53l4_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct vl53l4_device *vl53l4 = sd_to_vl53l4_ois(sd);

	return vl53l4_power_on(vl53l4);
}

static const struct i2c_device_id vl53l4_id_table[] = {
	{ VL53L4_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, vl53l4_id_table);

static const struct of_device_id vl53l4_of_table[] = {
	{ .compatible = "mediatek,vl53l4" },
	{ },
};
MODULE_DEVICE_TABLE(of, vl53l4_of_table);

static const struct dev_pm_ops vl53l4_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(vl53l4_ois_suspend, vl53l4_ois_resume, NULL)
};

static struct i2c_driver vl53l4_i2c_driver = {
	.driver = {
		.name = VL53L4_NAME,
		.pm = &vl53l4_pm_ops,
		.of_match_table = vl53l4_of_table,
	},
	.probe  = vl53l4_probe,
	.remove = vl53l4_remove,
	.id_table = vl53l4_id_table,
};

module_i2c_driver(vl53l4_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("VL53L4 TOF driver");
MODULE_LICENSE("GPL");
