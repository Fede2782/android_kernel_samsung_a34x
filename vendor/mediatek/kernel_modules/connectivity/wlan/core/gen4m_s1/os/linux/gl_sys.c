/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /os/linux/gl_sys.c
 */

/*! \file   "gl_sys.c"
 *  \brief  This file defines the interface which can interact with users
 *          in /sys fs.
 *
 *    Detail description.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_os.h"
#include "gl_kal.h"
#include "gl_rst.h"
#include "gl_sys.h"
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#if WLAN_INCLUDE_SYS
#define SYS_FW_LOG_SUPPORT 1
#if SYS_FW_LOG_SUPPORT
#define FW_LOG_WIFI_WTF_INF_NAME "fw_log_wifi_write_log_to_file"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define MTK_INFO_MAX_SIZE 256
#if CFG_SUPPORT_CABLE_DETECT
#define CABLE_STATUS_UNKNOWN 0
#define CABLE_STATUS_PLUG_IN 1
#define CABLE_STATUS_PULL_OUT 2
#endif
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

#if SYS_FW_LOG_SUPPORT
struct fw_log_wifi_WtF_interface {
	struct cdev cdev;
	dev_t devno;
	struct class *driver_class;
	struct device *class_dev;
	int32_t fgIsAllowFWLogDump;
	wait_queue_head_t wq;
};

struct fw_log_wifi_WtF_interface fw_log_wifi_WtF_inf;

static unsigned int fw_log_wifi_WtF_poll(struct file *filp, poll_table *wait);

const struct file_operations fw_log_wifi_WtF_fops = {
	.poll = fw_log_wifi_WtF_poll,
};

#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

static struct GLUE_INFO *g_prGlueInfo;
static struct kobject *wifi_kobj;
static uint8_t aucMacAddrOverride[] = "FF:FF:FF:FF:FF:FF";
#if defined(CFG_MOUTON)
static uint8_t aucDefaultFWVersion[] = "MOUTON_DEFAULT";
#elif defined(CFG_TALBOT)
static uint8_t aucDefaultFWVersion[] = "TALBOT_DEFAULT";
#elif defined(CFG_CERVENO)
static uint8_t aucDefaultFWVersion[] = "CERVINO_DEFAULT";
#else
static uint8_t aucDefaultFWVersion[] = "Unknown";
#endif
static u_int8_t fgIsMacAddrOverride = FALSE;
static int32_t g_i4PM = -1;
static int32_t g_i4Ant = -1;
static char acVerInfo[MTK_INFO_MAX_SIZE];
static char acSoftAPInfo[MTK_INFO_MAX_SIZE];
#if SYS_FW_LOG_SUPPORT
static int32_t i4DumpInProgress;
#endif

static char acFeatureInfo[MTK_INFO_MAX_SIZE];
static uint8_t aucSolutionProvider[] = "MTK";
#if CFG_SUPPORT_CABLE_DETECT
static u_int8_t aucCableDetectStatus = CABLE_STATUS_UNKNOWN;
int32_t g_i4CableDetectGpio = -1;
#endif

uint8_t g_wifiVer[MANIFEST_BUFFER_SIZE] = {0};
uint32_t g_wifiVer_length;

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static ssize_t pm_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_i4PM),
		"%d", g_i4PM);
}

static void pm_EnterCtiaMode(void)
{
	if (!g_prGlueInfo)
		DBGLOG(INIT, ERROR, "g_prGlueInfo is null\n");
	else if (g_i4PM == -1)
		DBGLOG(INIT, TRACE, "keep default\n");
	else {
		g_prGlueInfo->prAdapter->fgEnDbgPowerMode = !g_i4PM;
		nicEnterCtiaMode(g_prGlueInfo->prAdapter,
			!g_i4PM,
			FALSE);
	}
}

static ssize_t pm_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtoint(buf, 10, &g_i4PM);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf pm fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set PM to %d.\n",
			g_i4PM);

		pm_EnterCtiaMode();
	}

	return (i4Ret == 0) ? count : 0;
}

static ssize_t macaddr_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(aucMacAddrOverride),
		"%s", aucMacAddrOverride);
}

static ssize_t macaddr_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;
	uint8_t aucMacAddrTemp[] = "FF:FF:FF:FF:FF:FF";

	kalMemCopy(&aucMacAddrTemp, buf, sizeof(aucMacAddrTemp));
	i4Ret = sscanf((uint8_t *)&aucMacAddrTemp, "%17s",
		(uint8_t *)&aucMacAddrOverride);

	if (!i4Ret)
		DBGLOG(INIT, ERROR, "sscanf mac format fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set macaddr to %s.\n",
			aucMacAddrOverride);
	}

	fgIsMacAddrOverride = TRUE;

	return (i4Ret > 0) ? count : 0;
}

static ssize_t wifiver_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(acVerInfo), "%s",
		acVerInfo);
}

static ssize_t softap_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(acSoftAPInfo), "%s", acSoftAPInfo);
}

static ssize_t memdump_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_u4Memdump),
		"%d", g_u4Memdump);
}

static ssize_t memdump_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtouint(buf, 10, &g_u4Memdump);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf memdump fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set memdump to %d.\n",
			g_u4Memdump);
	}

	return (i4Ret == 0) ? count : 0;
}

static ssize_t ant_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_i4Ant),
		"%d", g_i4Ant);
}


static void sysAntSetMode(void)
{
	struct ADAPTER *prAdapter = NULL;
	char input[4] = {0};

	if (!g_prGlueInfo)
		DBGLOG(INIT, ERROR, "g_prGlueInfo is null\n");
	else if (g_i4Ant == -1)
		DBGLOG(INIT, TRACE, "keep default\n");
	else {
		prAdapter = g_prGlueInfo->prAdapter;
		snprintf(input, sizeof(g_i4Ant), "%d", g_i4Ant-1);
		wlanCfgSet(g_prGlueInfo->prAdapter,
			"SpeIdxCtrl", input, WLAN_CFG_DEFAULT);
		prAdapter->rWifiVar.ucNSS = (g_i4Ant == 3) ? 2 : 1;
		wlanCfgSetUint32(prAdapter, "Nss", prAdapter->rWifiVar.ucNSS);
		wlanCfgSetUint32(prAdapter, "SGCfg", 0);
	}
}

static ssize_t ant_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtoint(buf, 10, &g_i4Ant);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf ant fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set ANT to %d.\n",
			g_i4Ant);
	}

	return (i4Ret == 0) ? count : 0;
}

#if SYS_FW_LOG_SUPPORT
static unsigned int fw_log_wifi_WtF_poll(struct file *filp, poll_table *wait)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	poll_wait(filp, &prWtFInf->wq, wait);
	if (prWtFInf->fgIsAllowFWLogDump) {
		DBGLOG(INIT, INFO, "Write FW log to file!");
		return POLLIN|POLLRDNORM;
	}
	return 0;
}

void fw_log_wifi_write_log_to_file(int32_t i4DumpInProgress)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	wake_up_interruptible(&prWtFInf->wq);
	prWtFInf->fgIsAllowFWLogDump = i4DumpInProgress;
}

int sysFwLogInit(void)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;
	int ret = 0;

	ret = alloc_chrdev_region(&prWtFInf->devno, 0, 1,
		FW_LOG_WIFI_WTF_INF_NAME);
	if (ret) {
		DBGLOG(INIT, ERROR,
			"alloc_chrdev_region failed, ret: %d\n",
			ret);
		goto return_fn;
	}

	cdev_init(&prWtFInf->cdev, &fw_log_wifi_WtF_fops);
	prWtFInf->cdev.owner = THIS_MODULE;

	ret = cdev_add(&prWtFInf->cdev, prWtFInf->devno, 1);
	if (ret) {
		DBGLOG(INIT, ERROR,
			"cdev_add failed, ret: %d\n",
			ret);
		goto unregister_chrdev_region;
	}

	prWtFInf->driver_class = KAL_CLASS_CREATE(FW_LOG_WIFI_WTF_INF_NAME);
	if (IS_ERR(prWtFInf->driver_class)) {
		DBGLOG(INIT, ERROR,
			"class_create failed, ret: %d\n",
			ret);
		ret = PTR_ERR(prWtFInf->driver_class);
		goto cdev_del;
	}

	prWtFInf->class_dev = device_create(prWtFInf->driver_class,
		NULL, prWtFInf->devno, NULL, FW_LOG_WIFI_WTF_INF_NAME);
	if (IS_ERR(prWtFInf->class_dev)) {
		ret = PTR_ERR(prWtFInf->class_dev);
		DBGLOG(INIT, ERROR,
			"class_device_create failed, ret: %d\n",
			ret);
		goto class_destroy;
	}

	prWtFInf->fgIsAllowFWLogDump = 0;
	init_waitqueue_head(&prWtFInf->wq);

	goto return_fn;

class_destroy:
	class_destroy(prWtFInf->driver_class);
cdev_del:
	cdev_del(&prWtFInf->cdev);
unregister_chrdev_region:
	unregister_chrdev_region(prWtFInf->devno, 1);
return_fn:
	if (ret)
		DBGLOG(INIT, ERROR, "ret: %d\n",
			ret);

	return ret;
}

void sysFwLogUninit(void)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	device_destroy(prWtFInf->driver_class, prWtFInf->devno);
	class_destroy(prWtFInf->driver_class);
	cdev_del(&prWtFInf->cdev);
	kfree(&prWtFInf->cdev);
	unregister_chrdev_region(prWtFInf->devno, 1);
}

static ssize_t logDumpStatus_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(i4DumpInProgress),
		"%d", i4DumpInProgress);
}

static ssize_t logDumpStatus_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;
	int32_t i4Value = 0;

	i4Ret = kstrtouint(buf, 10, &i4Value);

	if (i4Ret)
		DBGLOG(INIT, ERROR,
			"sscanf dump status fail u4Ret=%d\n", i4Ret);
	else {
		switch (i4Value) {
		case 0:
			DBGLOG(INIT, TRACE,
				"Write fw log to file done.\n");
			i4DumpInProgress = i4Value;
			fw_log_wifi_write_log_to_file(i4DumpInProgress);
			break;
		case 1:
			DBGLOG(INIT, TRACE,
				"Write fw log to file start.\n");
			i4DumpInProgress = i4Value;
			fw_log_wifi_write_log_to_file(i4DumpInProgress);
			break;

		default:
			DBGLOG(INIT, ERROR,
				"Unknown status mac format fail u4Ret=%d\n",
				i4Ret);
		}
	}

	return (i4Ret == 0) ? count : 0;
}
#endif

static ssize_t feature_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(acFeatureInfo), "%s", acFeatureInfo);
}

#if CFG_SUPPORT_CABLE_DETECT
void cable_detect_gpio_parse(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6877-consys");
	if (!node) {
		DBGLOG(INIT, ERROR, "parse wifi_cable_detect fail\n");
		return;
	}

	g_i4CableDetectGpio = of_get_named_gpio(node, "cable_detect_gpio", 0);
	if (g_i4CableDetectGpio < 0) {
		DBGLOG(INIT, ERROR, "parse gpio_num fail\n");
		g_i4CableDetectGpio = -1;
	}

	DBGLOG(INIT, INFO, "gpio_num is %d\n", g_i4CableDetectGpio);
	return;

}
static u_int8_t cable_detect_gpio_read(void)
{
	/*read GPIO*/
	if (gpio_is_valid(g_i4CableDetectGpio)) {
		if (gpio_get_value(g_i4CableDetectGpio))
			return CABLE_STATUS_PULL_OUT;
		else
			return CABLE_STATUS_PLUG_IN;
	}

	return CABLE_STATUS_UNKNOWN;
}

static ssize_t wificable_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	/*read GPIO status*/
	aucCableDetectStatus = cable_detect_gpio_read();
	DBGLOG(INIT, INFO, "aucCableDetectStatus %d\n", aucCableDetectStatus);

	switch (aucCableDetectStatus) {
	case CABLE_STATUS_PLUG_IN:
		return snprintf(buf, sizeof("E"), "E");

	case CABLE_STATUS_PULL_OUT:
		return snprintf(buf, sizeof("D"), "D");

	default:
		return snprintf(buf, sizeof("U"), "U");
	}
}
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

static struct kobj_attribute macaddr_attr
	= __ATTR(mac_addr, 0664, macaddr_show, macaddr_store);

static struct kobj_attribute wifiver_attr
	= __ATTR(wifiver, 0664, wifiver_show, NULL);

static struct kobj_attribute softap_attr
	= __ATTR(softap, 0664, softap_show, NULL);

static struct kobj_attribute pm_attr
	= __ATTR(pm, 0664, pm_show, pm_store);

static struct kobj_attribute memdump_attr
	= __ATTR(memdump, 0664, memdump_show, memdump_store);

static struct kobj_attribute ant_attr
	= __ATTR(ant, 0664, ant_show, ant_store);

#if SYS_FW_LOG_SUPPORT
static struct kobj_attribute logver_attr
	= __ATTR(dump_in_progress, 0664,
	logDumpStatus_show, logDumpStatus_store);
#endif

static struct kobj_attribute feature_attr
	= __ATTR(feature, 0664, feature_show, NULL);

#if CFG_SUPPORT_CABLE_DETECT
static struct kobj_attribute wificable_attr
	= __ATTR(wificable, 0664, wificable_show, NULL);
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_CABLE_DETECT
void sysInitCableInfo(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &wificable_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create cableinfo entry\n");
}

void sysUninitCableInfo(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &wificable_attr.attr);
}

#endif

void sysCreateMacAddr(void)
{
	if (g_prGlueInfo) {
		uint8_t rMacAddr[MAC_ADDR_LEN];

		COPY_MAC_ADDR(rMacAddr,
			g_prGlueInfo->prAdapter->rWifiVar.aucMacAddress);

		kalSnprintf(aucMacAddrOverride,
			sizeof(aucMacAddrOverride),
			"%pM",
			rMacAddr);

		DBGLOG(INIT, TRACE,
			"Init macaddr to " MACSTR ".\n",
			MAC2STR(rMacAddr));
	}
}

void sysInitMacAddr(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &macaddr_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitMacAddr(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &macaddr_attr.attr);
}

void sysInitPM(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &pm_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitPM(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &pm_attr.attr);
}

void sysCreateWifiVer(void)
{
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

	char aucDriverVersionStr[] = STR(NIC_DRIVER_MAJOR_VERSION) "_"
		STR(NIC_DRIVER_MINOR_VERSION) "_"
		STR(NIC_DRIVER_SERIAL_VERSION) "-"
		DRIVER_BUILD_DATE;
	uint16_t u2NvramVer = 0;
	uint8_t ucOffset = 0;

	kalMemZero(acVerInfo, sizeof(acVerInfo));

	ucOffset += kalSnprintf(acVerInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "Mediatek DRIVER_VER: %s\n", aucDriverVersionStr);

	if (g_prGlueInfo)
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "FW_VER: %s\n"
			, g_prGlueInfo->prAdapter->rVerInfo.aucReleaseManifest);
	else {
		if (g_wifiVer_length == 0) {
			ucOffset += kalSnprintf(acVerInfo + ucOffset,
				MTK_INFO_MAX_SIZE - ucOffset,
				"FW_VER: %s\n", aucDefaultFWVersion);
		} else {
			ucOffset += kalSnprintf(acVerInfo + ucOffset,
				MTK_INFO_MAX_SIZE - ucOffset,
				"FW_VER: %s\n", g_wifiVer);
		}
	}

	if (g_prGlueInfo) {
		kalCfgDataRead16(g_prGlueInfo,
			OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT,
			u2Part1OwnVersion), &u2NvramVer);
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: 0x%x\n", u2NvramVer);
	} else {
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: Unknown\n");
	}
}

void sysInitWifiVer(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &wifiver_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create wifiver entry\n");

	sysCreateWifiVer();
}

void sysUninitWifiVer(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &wifiver_attr.attr);
}

void sysCreateSoftap(void)
{
	struct REG_INFO *prRegInfo = NULL;

	uint8_t ucOffset = 0;
	u_int8_t fgDbDcModeEn = FALSE;

	/* Log SoftAP/hotspot information into .softap.info
	 * #Support wifi and hotspot at the same time?
	 * DualBandConcurrency=no
	 * # Supporting 5Ghz
	 * 5G=check NVRAM ucEnable5GBand
	 * # Max support client count
	 * maxClient=P2P_MAXIMUM_CLIENT_COUNT
	 * #Supporting android_net_wifi_set_Country_Code_Hal
	 * HalFn_setCountryCodeHal=yes ,
	 * call mtk_cfg80211_vendor_set_country_code
	 * #Supporting android_net_wifi_getValidChannels
	 * HalFn_getValidChannels=yes,
	 * call mtk_cfg80211_vendor_get_channel_list
	*/

	if (g_prGlueInfo) {
		prRegInfo = &(g_prGlueInfo->rRegInfo);
#if CFG_SUPPORT_DBDC
		fgDbDcModeEn = g_prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn;
#endif
	}

	kalMemZero(acSoftAPInfo, sizeof(acSoftAPInfo));

	ucOffset = 0;

	if (g_prGlueInfo) {
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=%s\n"
			, fgDbDcModeEn ? "yes" : "no");
	} else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=no\n");

	if (prRegInfo)
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=%s\n", prRegInfo->ucEnable5GBand ? "yes" : "no");
	else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=yes\n");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "maxClient=%d\n", P2P_MAXIMUM_CLIENT_COUNT);

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_setCountryCodeHal=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_getValidChannels=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "DualInterface=%s\n", "yes");
}

void sysInitSoftap(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &softap_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");

	sysCreateSoftap();
}

void sysUninitSoftap(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &softap_attr.attr);
}

void sysInitMemdump(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &memdump_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");
}

void sysUninitMemdump(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &memdump_attr.attr);
}

void sysInitAnt(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &ant_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitAnt(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &ant_attr.attr);
}

#if SYS_FW_LOG_SUPPORT
void sysInitDumpInProgress(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &logver_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR,
			"Unable to create dump_in_progress entry\n");
}

void sysUninitDumpInProgress(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &logver_attr.attr);
}
#endif


struct FS_SW_ILD_T sw_feature_set_table[] = {
	{FS_SW_PNO_ID, FS_SW_PNO_LEN,
		{FS_SW_PNO_SUPPORT|FS_SW_PNO_UNASSOC|FS_SW_PNO_ASSOC} },
	{FS_SW_TWT_ID, FS_SW_TWT_LEN,
		{0} },
	{FS_SW_OPTI_ID, FS_SW_OPTI_LEN,
		{0} },
	{FS_SW_SCHE_PM_ID, FS_SW_SCHE_PM_LEN,
		{0} },
	{FS_SW_D_WAKEUP_ID, FS_SW_D_WAKEUP_LEN,
		{0} },
	{FS_SW_RFC8325_ID, FS_SW_RFC8325_LEN,
		{FS_SW_RFC8325_SUPPORT} },
	{FS_SW_MHS_ID, FS_SW_MHS_LEN,
		{FS_SW_MHS_GET_VALID_CH|FS_SW_MHS_WPA3,
		FS_SW_MHS_DUAL_INF|FS_SW_MHS_5G|
		(FS_SW_MHS_MAX_CLIENT<<FS_SW_MHS_MAX_CLIENT_OFFSET)|
		FS_SW_MHS_CC_HAL} },
	{FS_SW_ROAM_ID, FS_SW_ROAM_LEN,
		{FS_SW_ROAM_VER_MAJOR, FS_SW_ROAM_VER_MINOR,
		FS_SW_ROAM_3B_B0|FS_SW_ROAM_3B_B1|FS_SW_ROAM_3B_B2|
		FS_SW_ROAM_3B_B3|FS_SW_ROAM_3B_B6|FS_SW_ROAM_3B_B7,
		FS_SW_ROAM_4B_B0|FS_SW_ROAM_4B_B1|FS_SW_ROAM_4B_B2|
		FS_SW_ROAM_4B_B3} },
	{FS_SW_NCHO_ID, FS_SW_NCHO_LEN,
		{FS_SW_NCHO_VER_MAJOR, FS_SW_NCHO_VER_MINOR} },
	{FS_SW_ASSUR_ID, FS_SW_ASSUR_LEN,
		{FS_SW_ASSUR_SUPPORT} },
	{FS_SW_FRAMEPCAP_ID, FS_SW_FRAMEPCAP_LEN,
		{0} },
	{FS_SW_SEC_ID, FS_SW_SEC_LEN,
		{FS_SW_SEC_WPA3_SAE_HASH, FS_SW_SEC_ENHANCED_OPEN} },
#if CFG_SUPPORT_NAN
	{FS_SW_P2P_ID, FS_SW_P2P_LEN,
		{FS_SW_P2P_NAN_REL_VER << FS_SW_P2P_NAN_REL_OFFSET,
		0, 0, FS_SW_P2P_STA_P2P|FS_SW_P2P_STA_SAP|FS_SW_P2P_STA_NAN|
		FS_SW_P2P_STA_TDLS,
		FS_SW_P2P_TDLS_MAX_NUM|
		(FS_SW_P2P_NAN_MAX_NDP << FS_SW_P2P_NAN_MAX_OFFSET),
		FS_SW_P2P_NAN|FS_SW_P2P_TDLS|FS_SW_P2P_TDLS_PS} },
#else
	{FS_SW_P2P_ID, FS_SW_P2P_LEN,
		{0, 0, 0,
		FS_SW_P2P_STA_P2P|FS_SW_P2P_STA_SAP|FS_SW_P2P_STA_TDLS,
		FS_SW_P2P_TDLS_MAX_NUM,
		FS_SW_P2P_TDLS|FS_SW_P2P_TDLS_PS} },
#endif
	{FS_SW_BIGD_ID, FS_SW_BIGD_LEN,
		{FS_SW_BIGD_GETBSSINFO|FS_SW_BIGD_GETSTAINFO} },
};

void sysCreateFeature(void)
{
	int			i, j;
	u_int16_t	fs_hw_feature = 0;
	u_int8_t	ucOffset = 0;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	kalMemZero(acFeatureInfo, sizeof(acFeatureInfo));

	ucOffset = 0;

#if defined(CFG_FS_WIFI6_MIMO)
	/* WIFI6 & MIMO */
	fs_hw_feature =   FS_HW_STANDARD_WIFI6
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_CONCURRENCY_MODE_ONE << FS_HW_CONCURRENCY_MODE_OFFSET
	| FS_HW_NUM_ANT_MIMO << FS_HW_NUM_ANT_OFFSET;
#elif defined(CFG_FS_WIFI5_MIMO)
	/* WIFI5 & MIMO */
	fs_hw_feature = FS_HW_STANDARD_WIFI5
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_NUM_ANT_MIMO << FS_HW_NUM_ANT_OFFSET;
#else
	/* WIFI5 & SISO */
	fs_hw_feature =   FS_HW_STANDARD_WIFI5
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_NUM_ANT_SISO << FS_HW_NUM_ANT_OFFSET;
#endif

	/* Feature Version */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", FS_VERSION);

	/* Solution Provider */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%s", aucSolutionProvider);

	/* HW Feature Length */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%02X", FS_HW_FEATURE_LEN);

	/* HW Feature Set */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", fs_hw_feature);

	/* SW Feature Length */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", FS_SW_FEATURE_LEN);

	/* SW Feature Set */
	for (i = 0; i < FS_SW_FEATURE_NUM; i++) {
		ucOffset += kalSnprintf(acFeatureInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "%02X%02X", sw_feature_set_table[i].u8ID,
						sw_feature_set_table[i].u8Len);

		for (j = 0; j < sw_feature_set_table[i].u8Len; j++) {
			ucOffset += kalSnprintf(acFeatureInfo + ucOffset
				, MTK_INFO_MAX_SIZE - ucOffset
				, "%02X", sw_feature_set_table[i].aucData[j]);
		}
	}

	DBGLOG(INIT, INFO, "[%s] Feature Set\n", acFeatureInfo);

}

void sysInitFeature(void)
{
	int32_t fsRet = 0;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	fsRet = sysfs_create_file(wifi_kobj, &feature_attr.attr);
	if (fsRet)
		DBGLOG(INIT, ERROR,
			"Unable to create feature_attr entry\n");

	sysCreateFeature();
}

void sysUninitFeature(void)
{

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &feature_attr.attr);
}

int32_t sysCreateFsEntry(struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	g_prGlueInfo = prGlueInfo;

	sysCreateMacAddr();
	pm_EnterCtiaMode();
	sysCreateWifiVer();
	sysCreateSoftap();
	sysAntSetMode();

	return 0;
}

int32_t sysRemoveSysfs(void)
{
	g_prGlueInfo = NULL;

	return 0;
}

int32_t sysInitFs(void)
{
	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	wifi_kobj = kobject_create_and_add("wifi", NULL);
	kobject_get(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_ADD);

	sysInitMacAddr();
	/*sysInitWifiVer();*/
	sysInitSoftap();
	sysInitPM();
	sysInitMemdump();
	sysInitAnt();
#if SYS_FW_LOG_SUPPORT
	sysInitDumpInProgress();
#endif
	sysInitFeature();
#if CFG_SUPPORT_CABLE_DETECT
	sysInitCableInfo();
#endif

	return 0;
}

int32_t sysUninitSysFs(void)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	sysUninitMemdump();
	sysUninitPM();
	sysUninitSoftap();
	sysUninitWifiVer();
	sysUninitMacAddr();
	sysUninitAnt();
#if SYS_FW_LOG_SUPPORT
	sysUninitDumpInProgress();
#endif
	sysUninitFeature();
#if CFG_SUPPORT_CABLE_DETECT
	sysUninitCableInfo();
#endif

	kobject_put(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_REMOVE);
	wifi_kobj = NULL;

	return 0;
}

void sysMacAddrOverride(uint8_t *prMacAddr)
{
	DBGLOG(INIT, TRACE,
		"Override=%d\n", fgIsMacAddrOverride);

	if (!fgIsMacAddrOverride)
		return;

	wlanHwAddrToBin(
		aucMacAddrOverride,
		prMacAddr);

	DBGLOG(INIT, TRACE,
		"Init macaddr to " MACSTR ".\n",
		MAC2STR(prMacAddr));
}

#endif
