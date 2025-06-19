#ifndef _AFC_CHARGER_INTF
#define _AFC_CHARGER_INTF

#include <linux/kernel.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
#include <linux/muic/common/vt_muic/vt_muic.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

#if IS_ENABLED(CONFIG_MTK_CHARGER)
#include "../drivers/power/supply/charger_class.h"
#endif

/* AFC Unit Interval */
#define UI				160
#define MPING			(UI << 4)
#define DATA_DELAY		(UI << 3)
#define SYNC_PULSE		(UI >> 2)
#define RESET_DELAY		(UI * 100)
#define MPING_GUARD_TIME (UI * 3)

#define AFC_RETRY_CNT			10
#define AFC_OP_CNT			3
#define AFC_RETRY_MAX			10
#define VBUS_RETRY_MAX			10
#define AFC_SPING_CNT			7
#define AFC_SPING_MIN			2
#define AFC_SPING_MAX			18

enum afc_voltage_code {
	AFC_5V = 0,
	AFC_6V,
	AFC_7V,
	AFC_8V,
	AFC_9V,
	AFC_10V,
	AFC_11V,
	AFC_12V,
	AFC_13V,
	AFC_14V,
	AFC_15V,
	AFC_16V,
	AFC_17V,
	AFC_18V,
	AFC_19V,
	AFC_20V,
};

enum afc_current_code {
	AFC_750MA = 0,
	AFC_900MA,
	AFC_1050MA,
	AFC_1200MA,
	AFC_1350MA,
	AFC_1500MA,
	AFC_1650MA,
	AFC_1800MA,
	AFC_1950MA,
	AFC_2100MA,
	AFC_2250MA,
	AFC_2400MA,
	AFC_2550MA,
	AFC_2700MA,
	AFC_2850MA,
	AFC_3000MA,
};

struct gpio_afc_pdata {
	int gpio_afc_switch;
	int gpio_afc_data;
	int gpio_afc_hvdcp;
	int gpio_hiccup;
};

struct gpio_afc_ddata {
	struct device *dev;
	struct gpio_afc_pdata *pdata;
	struct wakeup_source ws;
	struct mutex mutex;
	struct kthread_worker kworker;
	struct kthread_work kwork;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	struct charger_device *chg_dev;
#endif
	spinlock_t spin_lock;
	bool gpio_input;
	bool check_performance;
	int water_state;

	struct delayed_work set_gpio_afc_disable;

	int curr_voltage;
	int set_voltage;

	bool dpdm_ctrl_on;
	bool hiccup_mode;

	unsigned int rp_currentlvl;
	struct notifier_block typec_nb;
	struct mutex tcm_notify_lock;
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	struct vt_muic_ic_data afc_ic_data;
#endif
};

#if IS_ENABLED(CONFIG_AFC_CHARGER)
extern void set_afc_voltage_for_performance(bool enable);
#else /* NOT CONFIG_AFC_CHARGER */
static inline void set_afc_voltage_for_performance(bool enable)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_AFC_CHARGER */
#if IS_ENABLED(CONFIG_MTK_CHARGER)
extern int charger_dev_get_vbus(struct charger_device *chg_dev, u32 *vbus);
#endif
#endif /* _AFC_CHSRGER_INTF */
