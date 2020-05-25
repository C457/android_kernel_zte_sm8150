#include <linux/alarmtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/timekeeping.h>
#include <linux/wakelock.h>
#include <vendor/common/zte_misc.h>

#if (KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE)
#define KERNEL_ABOVE_4_1_0
#endif

enum charging_policy_status {
	POLICY_STATUS_IDLE = 0,
	POLICY_STATUS_RUNNING_CHARGING,
	POLICY_STATUS_RUNNING_DISCHARGING,
	POLICY_STATUS_FORCE_DISCHARGING,
};

struct dts_config_prop {
	u32			turn_on;
	u32			expired_mode_enable;
	u32			low_temp_enable;
	u32			have_power_path;
	u32			retry_times;
	u32			timeout_seconds;
	int			expired_capacity_max;
	int			expired_capacity_min;
	int			demo_capacity_max;
	int			demo_capacity_min;
	int			no_powerpath_delta_cap;
	const char	*interface_phy_name;
	const char	*battery_phy_name;
	const char	*cas_phy_name;
};

struct charger_policy_info {
	struct device				*dev;
#ifdef KERNEL_ABOVE_4_1_0
	struct power_supply *policy_psy_pointer;
#else
	struct power_supply policy_psy;
#endif
	struct notifier_block		nb;
	struct alarm				timeout_timer;
	struct workqueue_struct	*timeout_workqueue;
	struct delayed_work		timeout_work;
	struct workqueue_struct	*policy_probe_wq;
	struct delayed_work		policy_probe_work;
	struct dts_config_prop		config_prop;
	spinlock_t				timer_lock;
	struct wake_lock	policy_wake_lock;
	unsigned int force_disable;
	unsigned int policy_status;
	unsigned int policy_status_pre;
	unsigned int overtime_seconds;
	unsigned int force_disching_seconds;
	u64 timeout_interval;
	time_t charging_begin;
	bool low_temperature_enable;
	bool demo_enable;
	bool overtime_enable;
	bool overtime_status;
	bool have_powerpath;
};

struct policy_status_node {
	u32 status;
	char *status_name;
	int (*handle)(struct charger_policy_info *policy_info);
};


#define POLICY_ERR			-1
#define SEC_PER_MS			1000ULL
#define MIN_BATTERY_PROTECTED_PERCENT 50
#define MAX_BATTERY_PROTECTED_PERCENT 70
#define CHARGING_POLICY_LOWTEMP_THRESHOLD 150
#define TIMEOUT_INTERVAL_SECONDS 30
#define MOUTH_PER_SECONDS	2592000ULL

#define pr_policy(fmt, args...)	pr_info("policy: %s(): "fmt, __func__, ## args)

#define OF_READ_PROPERTY(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_u32(np,			\
					"policy," dt_property,		\
					&store);					\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_policy("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
	}										\
	pr_policy("config: " #dt_property				\
				" property: [%d]\n", store);		\
} while (0)

#define OF_READ_PROPERTY_STRINGS(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_string(np,		\
					"policy," dt_property,		\
					&(store));				\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_policy("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
		return retval;						\
	}										\
	pr_policy("config: " #dt_property				\
				" property: [%s]\n", store);		\
} while (0)

#define OF_READ_ARRAY_PROPERTY(prop_data, prop_length, prop_size, dt_property, retval) \
do { \
	if (of_find_property(np, "policy," dt_property, &prop_length)) { \
		prop_data = kzalloc(prop_length, GFP_KERNEL); \
		retval = of_property_read_u32_array(np, "policy," dt_property, \
				 (u32 *)prop_data, prop_length / sizeof(u32)); \
		if (retval) { \
			retval = -EINVAL; \
			pr_policy("Error reading " #dt_property \
				" property rc = %d\n", retval); \
			kfree(prop_data); \
			prop_data = NULL; \
			prop_length = 0; \
			return retval; \
		} else { \
			prop_length = prop_length / prop_size; \
		} \
	} else { \
		retval = -EINVAL; \
		prop_data = NULL; \
		prop_length = 0; \
		pr_policy("Error geting " #dt_property \
				" property rc = %d\n", retval); \
		return retval; \
	} \
	pr_policy("config: " #dt_property \
				" prop_length: [%d]\n", prop_length);\
} while (0)
/*
static void charge_policy_ktime_enable(struct charger_policy_info *policy_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&policy_info->timer_lock, flags);

	alarm_start_relative(&policy_info->timeout_timer, ms_to_ktime(policy_info->timeout_interval));

	spin_unlock_irqrestore(&policy_info->timer_lock, flags);
}
*/
static void charge_policy_ktime_reset(struct charger_policy_info *policy_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&policy_info->timer_lock, flags);

	alarm_try_to_cancel(&policy_info->timeout_timer);

	alarm_start_relative(&policy_info->timeout_timer, ms_to_ktime(policy_info->timeout_interval));

	spin_unlock_irqrestore(&policy_info->timer_lock, flags);
}

static void charge_policy_ktime_disable(struct charger_policy_info *policy_info)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&policy_info->timer_lock, flags);

	alarm_try_to_cancel(&policy_info->timeout_timer);

	spin_unlock_irqrestore(&policy_info->timer_lock, flags);
}

static int charger_policy_parse_dt(struct charger_policy_info *policy_info)
{
	int retval = 0;
	struct device_node *np = policy_info->dev->of_node;

	OF_READ_PROPERTY(policy_info->config_prop.turn_on,
			"enable", retval, 0);

	OF_READ_PROPERTY(policy_info->config_prop.retry_times,
			"retry-times", retval, 10);

	OF_READ_PROPERTY(policy_info->config_prop.expired_mode_enable,
			"expired-mode-enable", retval, 1);

	OF_READ_PROPERTY(policy_info->config_prop.low_temp_enable,
			"low-temp-enable", retval, 0);

	OF_READ_PROPERTY(policy_info->config_prop.have_power_path,
			"have-power-path", retval, 1);

	OF_READ_PROPERTY(policy_info->config_prop.timeout_seconds,
			"timeout-seconds", retval, 86400);

	OF_READ_PROPERTY(policy_info->config_prop.expired_capacity_max,
			"expired-max-capacity", retval, 70);

	OF_READ_PROPERTY(policy_info->config_prop.expired_capacity_min,
			"expired-min-capacity", retval, 50);

	OF_READ_PROPERTY(policy_info->config_prop.demo_capacity_max,
			"demo-max-capacity", retval, 70);

	OF_READ_PROPERTY(policy_info->config_prop.demo_capacity_min,
			"demo-min-capacity", retval, 50);

	OF_READ_PROPERTY(policy_info->config_prop.no_powerpath_delta_cap,
			"no-powerpath-delta-capacity", retval, 10);

	OF_READ_PROPERTY_STRINGS(policy_info->config_prop.battery_phy_name,
			"battery-phy-name", retval, "battery");

	OF_READ_PROPERTY_STRINGS(policy_info->config_prop.interface_phy_name,
			"interface-phy-name", retval, "interface");

	OF_READ_PROPERTY_STRINGS(policy_info->config_prop.cas_phy_name,
			"cas-phy-name", retval, "cas");

	return 0;
}

static int charger_policy_set_prop_by_name(const char *name, enum power_supply_property psp, int data)
{
	struct power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		pr_policy("psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = power_supply_get_by_name(name);
	if (!psy) {
		pr_policy("get %s psy failed!!\n", name);
		goto failed_loop;
	}

	val.intval = data;

	rc = power_supply_set_property(psy,
				psp, &val);
	if (rc < 0) {
		pr_policy("Failed to set %s property:%d rc=%d\n", name, psp, rc);
		return rc;
	}

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(psy);
#endif

	return 0;

failed_loop:
	return -EINVAL;
}

static int charger_policy_get_prop_by_name(const char *name, enum power_supply_property psp, int *data)
{
	struct power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		pr_policy("psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = power_supply_get_by_name(name);
	if (!psy) {
		pr_policy("get %s psy failed!!\n", name);
		goto failed_loop;
	}

	rc = power_supply_get_property(psy,
				psp, &val);
	if (rc < 0) {
		pr_policy("Failed to set %s property:%d rc=%d\n", name, psp, rc);
		return rc;
	}

	*data = val.intval;

#ifdef KERNEL_ABOVE_4_1_0
	power_supply_put(psy);
#endif

	return 0;

failed_loop:
	return -EINVAL;
}

static int charger_policy_disable_cas(struct charger_policy_info *policy_info, bool status)
{
	int rc = 0;

	pr_policy("disable cas %d\n", status);

	rc = charger_policy_set_prop_by_name(policy_info->config_prop.cas_phy_name,
					POWER_SUPPLY_PROP_AUTHENTIC, status);
	if (rc < 0) {
		pr_policy("Failed to set battery charging enabled property rc=%d\n", rc);
	}


	return 0;
}

static bool charger_policy_check_low_temperature(struct charger_policy_info *policy_info)
{
	int rc = 0, temp = 0;

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.battery_phy_name,
						POWER_SUPPLY_PROP_TEMP, &temp);
	if (rc < 0) {
		pr_policy("Get battery temp failed\n");
		return false;
	}

	if ((policy_info->low_temperature_enable) &&
					(temp <= CHARGING_POLICY_LOWTEMP_THRESHOLD)) {
		pr_policy("low_temperature_enable: 1, temp: %d(%d)\n",
								temp, CHARGING_POLICY_LOWTEMP_THRESHOLD);
		return true;
	}

	return false;
}

static int charger_policy_ctrl_charging_enable(struct charger_policy_info *policy_info,
						bool battery_in_enable, bool usb_in_enable)
{
	int rc = 0, battery_enable_now = 0, usb_enable_now = 0;

	/*get info*/
	rc = charger_policy_get_prop_by_name(policy_info->config_prop.interface_phy_name,
						POWER_SUPPLY_PROP_CHARGING_ENABLED, &usb_enable_now);
	if (rc < 0) {
		pr_policy("Get CHARGING_ENABLED failed\n");
	}

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.interface_phy_name,
						POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &battery_enable_now);
	if (rc < 0) {
		pr_policy("Get BATTERY_CHARGING_ENABLED failed\n");
	}

	pr_policy("USB(now:%d, set:%d), BATTERY(now:%d, set:%d);\n",
						usb_enable_now, usb_in_enable, battery_enable_now, battery_in_enable);

	/*set info*/
	if (usb_enable_now != usb_in_enable) {
		rc = charger_policy_set_prop_by_name(policy_info->config_prop.interface_phy_name,
						POWER_SUPPLY_PROP_CHARGING_ENABLED, usb_in_enable);
		if (rc < 0) {
			pr_policy("Set CHARGING_ENABLED failed\n");
		}
	}

	if (battery_enable_now != battery_in_enable) {
		rc = charger_policy_set_prop_by_name(policy_info->config_prop.interface_phy_name,
						POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, battery_in_enable);
		if (rc < 0) {
			pr_policy("Set BATTERY_CHARGING_ENABLED failed\n");
		}
	}

	return 0;
}

static bool charger_policy_check_usb_present(struct charger_policy_info *policy_info)
{
	int rc = 0, usb_present = 0, dc_present = 0;

	rc = charger_policy_get_prop_by_name("usb",
						POWER_SUPPLY_PROP_PRESENT, &usb_present);
	if (rc < 0) {
		pr_policy("Get usb present failed\n");
	}

	rc = charger_policy_get_prop_by_name("dc",
						POWER_SUPPLY_PROP_PRESENT, &dc_present);
	if (rc < 0) {
		pr_policy("Get dc present failed\n");
	}

	return (usb_present || dc_present) ? true : false;
}

static bool charger_policy_check_force_discharging(struct charger_policy_info *policy_info)
{
	struct timespec ts;

	if (!policy_info->have_powerpath)
		return false;

	get_monotonic_boottime(&ts);

	pr_policy("force_discharging: (%ld - %ld = %ld) force_seconds: %d\n",
					ts.tv_sec, policy_info->charging_begin,
					(ts.tv_sec - policy_info->charging_begin),
					policy_info->force_disching_seconds);

	if ((ts.tv_sec - policy_info->charging_begin) >
							policy_info->force_disching_seconds) {
		return true;
	}

	return false;
}

static bool charger_policy_check_soc_reach_max(struct charger_policy_info *policy_info)
{
	int rc = 0, capacity = 0, temp_cap = 0;

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.battery_phy_name,
						POWER_SUPPLY_PROP_CAPACITY, &capacity);
	if (rc < 0) {
		pr_policy("Get battery capacity failed\n");
		return false;
	}

	pr_policy("demo_enable(%d), overtime_enable(%d, %d), powerpath(%d)\n",
											policy_info->demo_enable,
											policy_info->overtime_enable,
											policy_info->overtime_status,
											policy_info->have_powerpath);

	if (policy_info->demo_enable) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.demo_capacity_max +
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = (policy_info->config_prop.demo_capacity_max +
							policy_info->config_prop.demo_capacity_min) / 2;
		}

		pr_policy("demo: cap_now(%d), cap_max(%d)\n", capacity, temp_cap);

		if (capacity >= temp_cap) {
			pr_policy("capacity(%d) reach demo max(%d)\n", capacity, temp_cap);
			return true;
		}
	}

	if ((policy_info->overtime_enable) && (policy_info->overtime_status)) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.expired_capacity_max +
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = (policy_info->config_prop.expired_capacity_max +
							policy_info->config_prop.expired_capacity_min) / 2;
		}

		pr_policy("overtime: cap_now(%d), cap_max(%d)\n", capacity, temp_cap);

		if (capacity >= temp_cap) {
			pr_policy("capacity(%d) reach overtime max(%d)\n", capacity, temp_cap);
			return true;
		}
	}

	return false;
}

static bool charger_policy_check_soc_reach_min(struct charger_policy_info *policy_info)
{
	int rc = 0, capacity = 0, temp_cap = 0;

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.battery_phy_name,
						POWER_SUPPLY_PROP_CAPACITY, &capacity);
	if (rc < 0) {
		pr_policy("Get battery capacity failed\n");
		return false;
	}

	pr_policy("demo_enable(%d), overtime_enable(%d, %d), powerpath(%d)\n",
											policy_info->demo_enable,
											policy_info->overtime_enable,
											policy_info->overtime_status,
											policy_info->have_powerpath);

	if (policy_info->demo_enable) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.demo_capacity_min -
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = policy_info->config_prop.demo_capacity_min;
		}

		pr_policy("demo: cap_now(%d), cap_min(%d)\n", capacity, temp_cap);

		if (capacity <= temp_cap) {
			pr_policy("capacity(%d) reach daemon min(%d)\n", capacity, temp_cap);
			return true;
		}
	}

	if ((policy_info->overtime_enable) && (policy_info->overtime_status)) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.expired_capacity_min -
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = policy_info->config_prop.expired_capacity_min;
		}

		pr_policy("overtime: cap_now(%d), cap_min(%d)\n", capacity, temp_cap);

		if (capacity <= temp_cap) {
			pr_policy("capacity(%d) reach overtime min(%d)\n", capacity, temp_cap);
			return true;
		}
	}

	return false;
}

static int charger_policy_select_discharging_mode(struct charger_policy_info *policy_info)
{
	int rc = 0, capacity = 0, temp_cap = 0, middle_cap = 0;

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.battery_phy_name,
						POWER_SUPPLY_PROP_CAPACITY, &capacity);
	if (rc < 0) {
		pr_policy("Get battery capacity failed\n");
		return 0;
	}

	if ((policy_info->overtime_enable) && (policy_info->overtime_status)) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.expired_capacity_max +
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = policy_info->config_prop.expired_capacity_max;
		}

		middle_cap = (policy_info->config_prop.expired_capacity_max +
						policy_info->config_prop.expired_capacity_min) / 2;

		pr_policy("dischgmode(overtime): cap_now(%d), cap_max(%d), middle_cap(%d)\n",
						capacity, temp_cap, middle_cap);
	}

	if (policy_info->demo_enable) {
		if (!policy_info->have_powerpath) {
			temp_cap = policy_info->config_prop.demo_capacity_max +
							policy_info->config_prop.no_powerpath_delta_cap;
		} else {
			temp_cap = policy_info->config_prop.demo_capacity_max;
		}

		middle_cap = (policy_info->config_prop.demo_capacity_max +
						policy_info->config_prop.demo_capacity_min) / 2;

		pr_policy("dischgmode(demo): cap_now(%d), cap_max(%d), middle_cap(%d)\n",
						capacity, temp_cap, middle_cap);
	}

	if (capacity >= temp_cap) {
		charger_policy_ctrl_charging_enable(policy_info, false, false);
	} else if (capacity <= middle_cap) {
		charger_policy_ctrl_charging_enable(policy_info, false, true);
	}

	return 0;
}


static int charger_policy_status_ide(struct charger_policy_info *policy_info)
{
	struct timespec ts;

	pr_policy("demo_enable: %d overtime_enable: %d\n",
					policy_info->demo_enable,
					policy_info->overtime_enable);

	get_monotonic_boottime(&ts);

	/*demo_enable check*/
	if (policy_info->demo_enable) {
		pr_policy("demo enabled, policy enter into runing\n");
		charger_policy_disable_cas(policy_info, true);
		policy_info->policy_status = POLICY_STATUS_RUNNING_CHARGING;
		policy_info->charging_begin = ts.tv_sec;
		pr_policy("demo init begin %ld\n", policy_info->charging_begin);
		return 0;
	}

	/*overtime init*/
	if (!policy_info->charging_begin) {
		policy_info->charging_begin = ts.tv_sec;
		pr_policy("overtime init begin %ld\n", policy_info->charging_begin);
	}

	pr_policy("overtime_now: (%ld - %ld = %ld) overtime_seconds: %d\n",
					ts.tv_sec, policy_info->charging_begin,
					(ts.tv_sec - policy_info->charging_begin),
					policy_info->overtime_seconds);

	/*overtime check*/
	if ((ts.tv_sec - policy_info->charging_begin) >= policy_info->overtime_seconds) {
		pr_policy("overtime found, policy enter into runing\n");
		charger_policy_disable_cas(policy_info, true);
		policy_info->overtime_status = true;
		policy_info->policy_status = POLICY_STATUS_RUNNING_CHARGING;
		return 0;
	}

	return 0;
}

static int charger_policy_status_runchging(struct charger_policy_info *policy_info)
{
	/*check need force discharging*/
	if (charger_policy_check_force_discharging(policy_info) == true) {
		policy_info->policy_status = POLICY_STATUS_FORCE_DISCHARGING;
		pr_policy("time is too long, force discharging\n");
		return 0;
	}

	/*select charge mode*/
	charger_policy_ctrl_charging_enable(policy_info, true, true);

	/*check soc reach max*/
	if (charger_policy_check_soc_reach_max(policy_info) != false) {
		policy_info->policy_status = POLICY_STATUS_RUNNING_DISCHARGING;
		charger_policy_ctrl_charging_enable(policy_info, false, false);
		pr_policy("soc reach max found.\n");
		return 0;
	}

	return 0;
}

static int charger_policy_status_rundischging(struct charger_policy_info *policy_info)
{
	struct timespec ts;

	/*check need force discharging*/
	if (charger_policy_check_force_discharging(policy_info) == true) {
		policy_info->policy_status = POLICY_STATUS_FORCE_DISCHARGING;
		pr_policy("time is too long, force discharging\n");
		return 0;
	}

	/*select charge mode*/
	charger_policy_select_discharging_mode(policy_info);

	/*check soc reach min*/
	if (charger_policy_check_soc_reach_min(policy_info) != false) {
		get_monotonic_boottime(&ts);
		policy_info->policy_status = POLICY_STATUS_RUNNING_CHARGING;
		charger_policy_ctrl_charging_enable(policy_info, true, true);
		policy_info->charging_begin = ts.tv_sec;
		pr_policy("soc reach min found, reinit charging_begin %ld\n",
												policy_info->charging_begin);
	}

	return 0;
}

static int charger_policy_status_forcedischging(struct charger_policy_info *policy_info)
{
	struct timespec ts;

	/*select charge mode*/
	charger_policy_ctrl_charging_enable(policy_info, false, false);

	/*check soc reach min*/
	if (charger_policy_check_soc_reach_min(policy_info) != false) {
		get_monotonic_boottime(&ts);
		policy_info->policy_status = POLICY_STATUS_RUNNING_CHARGING;
		charger_policy_ctrl_charging_enable(policy_info, true, true);
		policy_info->charging_begin = ts.tv_sec;
		pr_policy("soc reach min found, reinit charging_begin %ld\n",
												policy_info->charging_begin);
		return 0;
	}

	return 0;
}

static int charger_policy_status_disable(struct charger_policy_info *policy_info)
{
	charger_policy_ctrl_charging_enable(policy_info, true, true);

	policy_info->overtime_status = 0;

	policy_info->charging_begin = 0;

	policy_info->policy_status = POLICY_STATUS_IDLE;

	charger_policy_disable_cas(policy_info, false);

	return 0;
}

static int charger_policy_status_need_sleep(struct charger_policy_info *policy_info)
{
	bool usb_present = false;

	/* check usb leave */
	usb_present = charger_policy_check_usb_present(policy_info);
	if (!usb_present) {
		pr_policy("usb leave found.\n");
		/*reset to idle*/
		goto need_to_sleep;
	}
	/*****else if usb is present, checking *******/

	/* check enable status */
	if ((!policy_info->demo_enable)
				&& (!policy_info->overtime_enable)) {
		pr_policy("demo_enable && overtime_enable is disable\n");
		goto need_to_sleep;
	}

	/*check low temp */
	if (charger_policy_check_low_temperature(policy_info) != false) {
		pr_policy("low temperature found.\n");
		goto need_to_sleep;
	}

	/*check force disable */
	if (policy_info->force_disable) {
		pr_policy("policy force disabled\n");
		goto need_to_sleep;
	}

	/*check status abnormal */
	if (policy_info->policy_status > POLICY_STATUS_FORCE_DISCHARGING) {
		pr_policy("policy status abnormal\n");
		goto need_to_sleep;
	}

	charge_policy_ktime_reset(policy_info);

	return false;

need_to_sleep:
	if (policy_info->policy_status != POLICY_STATUS_IDLE) {
		wake_lock(&policy_info->policy_wake_lock);
		charger_policy_status_disable(policy_info);
		wake_unlock(&policy_info->policy_wake_lock);
	} else {
		charger_policy_status_disable(policy_info);
	}

	charge_policy_ktime_disable(policy_info);

	return true;
}

struct policy_status_node policy_status_list[] = {
	{
		POLICY_STATUS_IDLE,
		"POLICY_IDLE",
		charger_policy_status_ide,
	},

	{
		POLICY_STATUS_RUNNING_CHARGING,
		"POLICY_RUNNING_CHARGING",
		charger_policy_status_runchging,
	},

	{
		POLICY_STATUS_RUNNING_DISCHARGING,
		"POLICY_RUNNING_DISCHARGING",
		charger_policy_status_rundischging,
	},

	{
		POLICY_STATUS_FORCE_DISCHARGING,
		"POLICY_STATUS_FORCE_DISCHARGING",
		charger_policy_status_forcedischging,
	},
};

static int charger_policy_status_debug(struct charger_policy_info *policy_info)
{
	int charge_status = 0, rc = 0;
	bool is_charging = false;

	rc = charger_policy_get_prop_by_name(policy_info->config_prop.battery_phy_name,
						POWER_SUPPLY_PROP_STATUS, &charge_status);
	if (rc < 0) {
		pr_policy("Get charging status failed\n");
		return 0;
	}

	is_charging = ((charge_status == POWER_SUPPLY_STATUS_CHARGING)
						|| (charge_status == POWER_SUPPLY_STATUS_FULL)) ? true : false;

	pr_policy(">>> machine state %s, %s <<<\n",
					policy_status_list[policy_info->policy_status].status_name,
					is_charging ? "charging" : "discharging");

	if (policy_info->policy_status != policy_info->policy_status_pre) {
		policy_info->policy_status_pre = policy_info->policy_status;

#ifdef KERNEL_ABOVE_4_1_0
		if (policy_info->policy_psy_pointer)
			power_supply_changed(policy_info->policy_psy_pointer);
#else
		power_supply_changed(&policy_info->policy_psy);
#endif
	}

	return 0;
}

static int charger_policy_notifier_handler(struct charger_policy_info *policy_info)
{
	if(charger_policy_status_need_sleep(policy_info) == true) {
		pr_policy("policy sleeping.....\n");
		return 0;
	}

	charger_policy_status_debug(policy_info);

	wake_lock(&policy_info->policy_wake_lock);

	if (policy_status_list[policy_info->policy_status].handle)
		policy_status_list[policy_info->policy_status].handle(policy_info);

	wake_unlock(&policy_info->policy_wake_lock);

	return 0;
}

static int charger_policy_notifier_switch(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct charger_policy_info *policy_info = container_of(nb, struct charger_policy_info, nb);
	const char *name = NULL;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if (delayed_work_pending(&policy_info->timeout_work))
		return NOTIFY_OK;

#ifdef KERNEL_ABOVE_4_1_0
	name = psy->desc->name;
#else
	name = psy->name;
#endif

	if ((strcmp(name, policy_info->config_prop.battery_phy_name) == 0)
			|| (strcmp(name, "usb") == 0)) {
		/* pr_policy("Notify, update status\n"); */
		queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(200));
	}

	return NOTIFY_OK;
}


static int charger_policy_register_notifier(struct charger_policy_info *policy_info)
{
	int rc = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	policy_info->nb.notifier_call = charger_policy_notifier_switch;

	rc = power_supply_reg_notifier(&policy_info->nb);
	if (rc < 0) {
		pr_policy("Couldn't register psy notifier rc = %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

static int charger_policy_check_retry(struct charger_policy_info *policy_info)
{
	static int probe_count = 0;

	if (!policy_info) {
		return -EINVAL;
	}

	probe_count++;
	if (probe_count < policy_info->config_prop.retry_times) {
		pr_policy("charger policy driver retry[%d]!!!\n", probe_count);
		queue_delayed_work(policy_info->policy_probe_wq, &policy_info->policy_probe_work,
							msecs_to_jiffies(5000));
		return true;
	}

	return false;
}

static void charger_policy_timeout_handler_work(struct work_struct *work)
{
	struct charger_policy_info *policy_info =
			container_of(work, struct charger_policy_info, timeout_work.work);

	charger_policy_notifier_handler(policy_info);
}

enum alarmtimer_restart charger_policy_timeout_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct charger_policy_info *policy_info = container_of(alarm, struct charger_policy_info,
					timeout_timer);

	pr_policy("\n");

	alarm_forward_now(alarm, ms_to_ktime(policy_info->timeout_interval));

	if (delayed_work_pending(&policy_info->timeout_work))
		return ALARMTIMER_RESTART;

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(100));

	return ALARMTIMER_RESTART;
}

int charger_policy_demo_sts_set(const char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;
	int enable = 0;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return -EINVAL;
	}

	sscanf(val, "%d", &enable);

	enable = !!enable;

	pr_policy("Set demo_enable: %d\n", enable);

	if (policy_info->demo_enable != enable) {
		pr_policy("demo_enable status changed!!!\n");
		policy_info->demo_enable = enable;

		/*need reset policy status*/
		if (!policy_info->demo_enable) {
			wake_lock(&policy_info->policy_wake_lock);
			charger_policy_status_disable(policy_info);
			wake_unlock(&policy_info->policy_wake_lock);
		}

		queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(100));
	}

	return 0;
}

int charger_policy_demo_sts_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	pr_policy("Get demo_enable = 0x%x\n", policy_info->demo_enable);

	if (policy_info->demo_enable)
		return snprintf(val, PAGE_SIZE, "1");
	else
		return snprintf(val, PAGE_SIZE, "0");

	return 0;
}

int charger_policy_expired_sts_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	pr_policy("overtime_status = 0x%x\n", policy_info->overtime_status);

	if (policy_info->overtime_status)
		return snprintf(val, PAGE_SIZE, "1");
	else
		return snprintf(val, PAGE_SIZE, "0");

	return 0;
}

int charger_policy_expired_sec_set(const char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;
	int sec = 0;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return -EINVAL;
	}

	sscanf(val, "%d", &sec);

	policy_info->overtime_seconds = sec;

	pr_policy("Set overtime_seconds = %d\n", policy_info->overtime_seconds);

	return 0;
}

int charger_policy_expired_sec_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	pr_policy("Get overtime_seconds = %d\n", policy_info->overtime_seconds);

	return snprintf(val, PAGE_SIZE, "%u", policy_info->overtime_seconds);
}

int charger_policy_force_disching_sec_set(const char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;
	int sec = 0;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return -EINVAL;
	}

	sscanf(val, "%d", &sec);

	policy_info->force_disching_seconds = sec;

	pr_policy("Set force_disching_seconds = %d\n", policy_info->force_disching_seconds);

	return 0;
}

int charger_policy_force_disching_sec_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	pr_policy("Get force_disching_seconds = %d\n", policy_info->force_disching_seconds);

	return snprintf(val, PAGE_SIZE, "%u", policy_info->force_disching_seconds);
}

int charger_policy_cap_min_set(const char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;
	int cap = 0;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return -EINVAL;
	}

	sscanf(val, "%d", &cap);

	policy_info->config_prop.demo_capacity_min = cap;
	policy_info->config_prop.expired_capacity_min = cap;
	pr_policy("expired_capacity_min = %d\n", policy_info->config_prop.expired_capacity_min);
	return 0;
}

int charger_policy_cap_min_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	return snprintf(val, PAGE_SIZE, "%u", policy_info->config_prop.expired_capacity_min);
}

int charger_policy_cap_max_set(const char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;
	int cap = 0;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return -EINVAL;
	}

	sscanf(val, "%d", &cap);

	policy_info->config_prop.demo_capacity_max = cap;
	policy_info->config_prop.expired_capacity_max = cap;
	policy_info->config_prop.no_powerpath_delta_cap = 0;
	pr_policy("expired_capacity_max = %d\n", policy_info->config_prop.expired_capacity_max);
	return 0;
}

int charger_policy_cap_max_get(char *val, const void *arg)
{
	struct charger_policy_info *policy_info = (struct charger_policy_info *) arg;

	if (!policy_info) {
		pr_policy("policy_info is null\n");
		return snprintf(val, PAGE_SIZE, "arg is null");
	}

	return snprintf(val, PAGE_SIZE, "%u", policy_info->config_prop.expired_capacity_max);
}

struct zte_misc_ops demo_charging_policy_node = {
	.node_name = "demo_charging_policy",
	.set = charger_policy_demo_sts_set,
	.get = charger_policy_demo_sts_get,
	.free = NULL,
	.arg = NULL,
};

struct zte_misc_ops expired_charging_policy_node = {
	.node_name = "expired_charging_policy",
	.set = NULL,
	.get = charger_policy_expired_sts_get,
	.free = NULL,
	.arg = NULL,
};

struct zte_misc_ops charging_time_sec_node = {
	.node_name = "charging_time_sec",
	.set = charger_policy_expired_sec_set,
	.get = charger_policy_expired_sec_get,
	.free = NULL,
	.arg = NULL,
};

struct zte_misc_ops force_disching_sec_node = {
	.node_name = "force_disching_sec",
	.set = charger_policy_force_disching_sec_set,
	.get = charger_policy_force_disching_sec_get,
	.free = NULL,
	.arg = NULL,
};

struct zte_misc_ops policy_cap_min_node = {
	.node_name = "policy_cap_min",
	.set = charger_policy_cap_min_set,
	.get = charger_policy_cap_min_get,
	.free = NULL,
	.arg = NULL,
};

struct zte_misc_ops policy_cap_max_node = {
	.node_name = "policy_cap_max",
	.set = charger_policy_cap_max_set,
	.get = charger_policy_cap_max_get,
	.free = NULL,
	.arg = NULL,
};

bool charger_policy_get_status(void)
{
	int status = 0, rc = 0;

	rc = charger_policy_get_prop_by_name("policy", POWER_SUPPLY_PROP_AUTHENTIC, &status);

	return (rc < 0) ? (false) : (!!status);
}
EXPORT_SYMBOL_GPL(charger_policy_get_status);

static int policy_psy_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *pval)
{
	struct charger_policy_info *policy_info = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!policy_info) {
		pr_policy("policy_info is null!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		if ((policy_info->demo_enable)
					|| ((policy_info->overtime_enable)
					&& (policy_info->overtime_status)))
			pval->intval = true;
		else
			pval->intval = false;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		pval->intval = policy_info->policy_status;
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int policy_psy_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *pval)
{
	struct charger_policy_info *policy_info = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!policy_info) {
		pr_policy("policy_info is null!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		pr_policy("Set policy enable %d\n", pval->intval);
		policy_info->force_disable = !pval->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		policy_info->policy_status = pval->intval;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(100));

	return rc;
}

static int policy_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		break;
	}

	return 0;
}

static void policy_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}


static enum power_supply_property policy_psy_props[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_ONLINE,
};


#ifdef KERNEL_ABOVE_4_1_0
static const struct power_supply_desc policy_psy_desc = {
	.name = "policy",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = policy_psy_props,
	.num_properties = ARRAY_SIZE(policy_psy_props),
	.get_property = policy_psy_get_property,
	.set_property = policy_psy_set_property,
	.external_power_changed = policy_external_power_changed,
	.property_is_writeable = policy_property_is_writeable,
};
#endif

static void charger_policy_probe_work(struct work_struct *work)
{
	struct charger_policy_info *policy_info =
			container_of(work, struct charger_policy_info, policy_probe_work.work);
#ifdef KERNEL_ABOVE_4_1_0
	struct power_supply_config policy_psy_cfg;
#else
	int rc = 0;
#endif

	pr_policy("charge policy driver init begin\n");

	spin_lock_init(&policy_info->timer_lock);

	policy_info->policy_status = POLICY_STATUS_IDLE;
	policy_info->overtime_seconds = policy_info->config_prop.timeout_seconds;
	policy_info->force_disching_seconds = MOUTH_PER_SECONDS;
	policy_info->timeout_interval = TIMEOUT_INTERVAL_SECONDS * SEC_PER_MS;
	policy_info->charging_begin = 0;
	policy_info->demo_enable = false;
	policy_info->overtime_enable = policy_info->config_prop.expired_mode_enable;
	policy_info->overtime_status = false;
	policy_info->have_powerpath = policy_info->config_prop.have_power_path;
	policy_info->low_temperature_enable = policy_info->config_prop.low_temp_enable;

	alarm_init(&policy_info->timeout_timer, ALARM_BOOTTIME, charger_policy_timeout_alarm_cb);

	policy_info->timeout_workqueue = create_singlethread_workqueue("charger-policy-service");
	INIT_DELAYED_WORK(&policy_info->timeout_work, charger_policy_timeout_handler_work);

	if (charger_policy_register_notifier(policy_info) < 0) {
		pr_policy("init register notifier info failed\n");
		goto register_notifier_failed;
	}

	/* Register the power supply */
#ifdef KERNEL_ABOVE_4_1_0
	memset(&policy_psy_cfg, 0, sizeof(struct power_supply_config));
	policy_psy_cfg.drv_data = policy_info;
	policy_psy_cfg.of_node = NULL;
	policy_psy_cfg.supplied_to = NULL;
	policy_psy_cfg.num_supplicants = 0;
	policy_info->policy_psy_pointer = devm_power_supply_register(policy_info->dev,
				&policy_psy_desc, &policy_psy_cfg);
	if (IS_ERR(policy_info->policy_psy_pointer)) {
		pr_policy("failed to register policy_psy rc = %ld\n",
				PTR_ERR(policy_info->policy_psy_pointer));
		goto register_power_supply_failed;
	}
#else
	memset(&policy_info->policy_psy, 0, sizeof(struct power_supply));
	policy_info->policy_psy.name = "policy";
	policy_info->policy_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
	policy_info->policy_psy.properties = policy_psy_props;
	policy_info->policy_psy.num_properties = ARRAY_SIZE(policy_psy_props);
	policy_info->policy_psy.get_property = policy_psy_get_property;
	policy_info->policy_psy.set_property = policy_psy_set_property;
	policy_info->policy_psy.external_power_changed = policy_external_power_changed;
	policy_info->policy_psy.property_is_writeable = policy_property_is_writeable;
	policy_info->policy_psy.drv_data = policy_info;
	policy_info->policy_psy.of_node = NULL;
	policy_info->policy_psy.supplied_to = NULL;
	policy_info->policy_psy.num_supplicants = 0;

	rc = power_supply_register(policy_info->dev, &policy_info->policy_psy);
	if (rc < 0) {
		pr_policy("failed to register bcl_psy rc = %d\n", rc);
		goto register_power_supply_failed;
	}
#endif
	zte_misc_register_callback(&demo_charging_policy_node, policy_info);

	zte_misc_register_callback(&expired_charging_policy_node, policy_info);

	zte_misc_register_callback(&charging_time_sec_node, policy_info);

	zte_misc_register_callback(&force_disching_sec_node, policy_info);
	zte_misc_register_callback(&policy_cap_min_node, policy_info);
	zte_misc_register_callback(&policy_cap_max_node, policy_info);

	wake_lock_init(&policy_info->policy_wake_lock, WAKE_LOCK_SUSPEND, "charger_policy_service");

	queue_delayed_work(policy_info->timeout_workqueue, &policy_info->timeout_work, msecs_to_jiffies(100));

	pr_policy("charge policy driver init finished\n");

	return;

register_power_supply_failed:
	power_supply_unreg_notifier(&policy_info->nb);
register_notifier_failed:
	if (charger_policy_check_retry(policy_info) == true) {
		return;
	}

	devm_kfree(policy_info->dev, policy_info);

	pr_policy("Charge Arbitrate Driver Init Failed!!!\n");
}

static int charger_policy_probe(struct platform_device *pdev)
{
	struct charger_policy_info *policy_info;

#ifdef ZTE_FEATURE_PV_AR
	pr_policy("charge policy driver probe exit, in pv mode\n");
	return 0;
#else
	pr_policy("charge policy driver probe begin\n");
#endif

	if (zte_poweroff_charging_status()) {
		pr_policy("charge policy driver probe exit, in charging mode\n");
		return 0;
	}

	policy_info = devm_kzalloc(&pdev->dev, sizeof(*policy_info), GFP_KERNEL);
	if (!policy_info) {
		pr_policy("devm_kzalloc failed\n");
		return -ENOMEM;
	}

	policy_info->dev = &pdev->dev;

	platform_set_drvdata(pdev, policy_info);

	if (charger_policy_parse_dt(policy_info) < 0) {
		pr_policy("Parse dts failed\n");
		goto parse_dt_failed;
	}

	if (!policy_info->config_prop.turn_on) {
		pr_policy("charge policy disabled, please config policy,enable\n");
		goto parse_dt_failed;
	}

	policy_info->policy_probe_wq = create_singlethread_workqueue("policy_probe_wq");

	INIT_DELAYED_WORK(&policy_info->policy_probe_work, charger_policy_probe_work);

	queue_delayed_work(policy_info->policy_probe_wq, &policy_info->policy_probe_work, msecs_to_jiffies(5000));

	pr_policy("charge policy driver probe finished\n");

	return 0;

parse_dt_failed:
	devm_kfree(policy_info->dev, policy_info);
	policy_info = NULL;

	pr_policy("charger policy driver failed\n");

	return 0;
}

static int charger_policy_remove(struct platform_device *pdev)
{
	struct charger_policy_info *policy_info = platform_get_drvdata(pdev);

	pr_policy("charger policy driver remove begin\n");

	if (policy_info == NULL) {
		goto ExitLoop;
	}

	power_supply_unreg_notifier(&policy_info->nb);

	wake_lock_destroy(&policy_info->policy_wake_lock);

	devm_kfree(policy_info->dev, policy_info);
	policy_info = NULL;

ExitLoop:
	pr_policy("charger policy driver remove finished\n");

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "zte,charger-policy-service", },
	{ },
};

static struct platform_driver charger_policy_driver = {
	.driver		= {
		.name		= "zte,charger-policy-service",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= charger_policy_probe,
	.remove		= charger_policy_remove,
};

module_platform_driver(charger_policy_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zte.charger <zte.charger@zte.com>");
MODULE_DESCRIPTION("Charge policy Service Driver");
