/*
 * Driver for zte misc functions
 * function1: used for translate hardware GPIO to SYS GPIO number
 * function2: update fingerprint status to kernel from fingerprintd,2016/01/18
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <soc/qcom/socinfo.h>
#include "zte_misc.h"

int smb1351_is_good = 0;
module_param(smb1351_is_good, int, 0644);

struct zte_gpio_info {
	int sys_num;
	const char *name;
};

#define MAX_SUPPORT_GPIOS 16
struct zte_gpio_info zte_gpios[MAX_SUPPORT_GPIOS];

static const struct of_device_id zte_misc_of_match[] = {
	{ .compatible = "zte-misc", },
	{ },
};
MODULE_DEVICE_TABLE(of, zte_misc_of_match);

#define CHARGER_BUF_SIZE 0x32
int get_sysnumber_byname(char *name)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_GPIOS; i++) {
		if (zte_gpios[i].name) {
			if (!strcmp(zte_gpios[i].name, name))
				return zte_gpios[i].sys_num;
		}
	}
	return 0;
}

static int get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	int count = -1;

	pr_info("zte_misc: translate hardware pin to system pin\n");
	node = dev->of_node;
	if (node == NULL)
		return -ENODEV;

	pp = NULL;
	while ((pp = of_get_next_child(node, pp))) {
		if (!of_find_property(pp, "label", NULL)) {
			dev_warn(dev, "Found without labels\n");
			continue;
		}
		count++;
		if (count >= MAX_SUPPORT_GPIOS) {
			pr_err("zte_gpio count out of range.\n");
			break;
		}
		zte_gpios[count].name = kstrdup(of_get_property(pp, "label", NULL),
								GFP_KERNEL);
		zte_gpios[count].sys_num = of_get_gpio(pp, 0);
		pr_info("zte_misc: sys_number=%d name=%s\n", zte_gpios[count].sys_num, zte_gpios[count].name);
	}
	return 0;
}

static int shipmode_zte = -1;
static int zte_misc_set_ship_mode(const char *val, const struct kernel_param *kp)
{
	struct power_supply *batt_psy;
	int rc;
	const union power_supply_propval enable = {0,};

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		if (shipmode_zte == 0) {
			rc = batt_psy->desc->set_property(batt_psy,
					POWER_SUPPLY_PROP_SET_SHIP_MODE, &enable);
			if (rc) {
				pr_err("Warnning:ship enter error %d\n", rc);
			}
			pr_info("%s: enter into shipmode 10s later\n", __func__);
		} else {
			pr_info("%s: shipmode. Wrong value.Doing nothing\n", __func__);
		}
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return 0;
}

static int zte_misc_get_ship_mode(char *val, const struct kernel_param *kp)
{
	struct power_supply *batt_psy;
	int rc;
	union power_supply_propval pval = {0,};

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		rc = batt_psy->desc->get_property(batt_psy,
					POWER_SUPPLY_PROP_SET_SHIP_MODE, &pval);
		if (rc) {
			pr_err("Get shipmode node error: %d\n", rc);
			return snprintf(val, CHARGER_BUF_SIZE, "%d", -1);
		}
		pr_info("%s: shipmode status %d\n", __func__, pval.intval);
		shipmode_zte = pval.intval;
	} else
		pr_err("%s: batt_psy is NULL\n", __func__);

	return snprintf(val, CHARGER_BUF_SIZE, "%d", pval.intval);
}

module_param_call(shipmode_zte, zte_misc_set_ship_mode, zte_misc_get_ship_mode,
					&shipmode_zte, 0644);

struct charging_policy_ops *g_charging_policy_ops = NULL;

static int demo_charging_policy = 0;
static int demo_charging_policy_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_demo_sts_set)
		g_charging_policy_ops->charging_policy_demo_sts_set(g_charging_policy_ops, demo_charging_policy);
	else
		pr_err("can not set demo charging policy\n");

	pr_info("demo_charging_policy is %d\n", demo_charging_policy);

	return 0;
}
static int demo_charging_policy_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_demo_sts_get)
		demo_charging_policy = g_charging_policy_ops->charging_policy_demo_sts_get(g_charging_policy_ops);
	else
		pr_err("can not get demo charging policy\n");

	pr_info("demo_charging_policy is %d\n", demo_charging_policy);
	return snprintf(val, 0x2, "%d",  demo_charging_policy);
}
module_param_call(demo_charging_policy, demo_charging_policy_set,
	demo_charging_policy_get, &demo_charging_policy, 0644);

static int expired_charging_policy = 0;
static int expired_charging_policy_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	pr_info("expired_charging_policy_set.\n");
	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}
	pr_info("expired_charging_policy is %d\n", expired_charging_policy);

	return 0;
}
static int expired_charging_policy_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sts_get)
		expired_charging_policy = g_charging_policy_ops->charging_policy_expired_sts_get(g_charging_policy_ops);
	else
		pr_err("can not get expired charging policy\n");

	pr_info("expired_charging_policy is %d\n", expired_charging_policy);
	return snprintf(val, 0x2, "%d",  expired_charging_policy);
}
module_param_call(expired_charging_policy, expired_charging_policy_set,
	expired_charging_policy_get, &expired_charging_policy, 0644);

static int driver_charging_policy = 0;
static int driver_charging_policy_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_driver_sts_set)
		g_charging_policy_ops->charging_policy_driver_sts_set(g_charging_policy_ops, driver_charging_policy);
	else
		pr_err("can not set driver charging policy\n");

	pr_info("driver_charging_policy is %d\n", driver_charging_policy);

	return 0;
}
static int driver_charging_policy_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_driver_sts_get)
		driver_charging_policy = g_charging_policy_ops->charging_policy_driver_sts_get(g_charging_policy_ops);
	else
		pr_err("can not get driver charging policy\n");

	pr_info("driver_charging_policy is %d\n", driver_charging_policy);
	return snprintf(val, 0x2, "%d",  driver_charging_policy);
}
module_param_call(driver_charging_policy, driver_charging_policy_set,
	driver_charging_policy_get, &driver_charging_policy, 0644);

static int charging_time_sec = 0;
static int expired_charging_policy_sec_set(const char *val, const struct kernel_param *kp)
{
	int rc;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sec_set)
		g_charging_policy_ops->charging_policy_expired_sec_set(g_charging_policy_ops,
			charging_time_sec);
	else
		pr_err("can not set expired charging policy sec\n");

	pr_info("expired_charging_policy_sec_set is %d\n", charging_time_sec);

	return 0;
}
static int expired_charging_policy_sec_get(char *val, const struct kernel_param *kp)
{
	if (g_charging_policy_ops && g_charging_policy_ops->charging_policy_expired_sec_get)
		charging_time_sec =
			g_charging_policy_ops->charging_policy_expired_sec_get(g_charging_policy_ops);
	else
		pr_err("can not get expired charging policy\n");

	pr_info("expired_charging_policy is %d\n", charging_time_sec);
	return snprintf(val, 0x2, "%d",  charging_time_sec);
}
module_param_call(charging_time_sec, expired_charging_policy_sec_set,
	expired_charging_policy_sec_get, &charging_time_sec, 0644);

int enable_to_dump_reg = 0;
static int set_enable_to_dump_reg(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_info("enable_to_dump_reg %d\n", enable_to_dump_reg);
	return 0;
}

module_param_call(enable_to_dump_reg, set_enable_to_dump_reg, param_get_int,
			&enable_to_dump_reg, 0644);

int zte_misc_register_charging_policy_ops(struct charging_policy_ops *ops)
{
	int ret = 0;

	pr_info("%s.\n", __func__);
	g_charging_policy_ops = ops;

	return ret;
}

/*
vendor_id    02    03    04    17    10   12    15    20      20
vendor_name  ATL   cos   BYD   BAK   LG   SONY  SANYO samsung samsung
resistance   10K   20K   33K   82K   180K 240K  330K  430K    470K
*/
#define BATTERY_VENDOR_NUM 9
int battery_vendor_id[BATTERY_VENDOR_NUM] = {02, 03, 04, 17, 10, 12, 15, 20, 20};
int resistance_kohm[BATTERY_VENDOR_NUM] = {10, 20, 33, 82, 180, 240, 330, 430, 470};
static int battery_module_pack_vendor = 0;
static int battery_module_pack_vendor_get(char *val, const struct kernel_param *kp)
{
	struct power_supply *bms_psy;
	union power_supply_propval prop = {0,};
	int resistance = 0, i, rc;

	bms_psy = power_supply_get_by_name("bms");
	if (bms_psy) {
		rc = bms_psy->desc->get_property(bms_psy,
				POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
		if (rc)
			pr_err("failed to battery module pack vendor, error:%d.\n", rc);
		else {
			resistance = prop.intval / 1000;
			if (resistance > (resistance_kohm[BATTERY_VENDOR_NUM - 1] * 109 / 100)
				|| resistance < (resistance_kohm[0] * 91 / 100)) {
				pr_err("resistance is out of range, %dkohm\n", resistance);
			} else {
				for (i = 0; i < (BATTERY_VENDOR_NUM - 1); i++) {
					if (resistance < resistance_kohm[i] * 109 / 100
						&& resistance > resistance_kohm[i] * 91 / 100) {
						battery_module_pack_vendor = battery_vendor_id[i];
						break;
					}
				}
				pr_info("battery resistance is %dkohm, battery_vendor_id = %2d\n",
					resistance, battery_module_pack_vendor);
			}
		}
	} else
		pr_err("bms_psy is NULL.\n");

	return snprintf(val, 0x4, "%2d", battery_module_pack_vendor);
}
module_param_call(battery_module_pack_vendor, NULL,
	battery_module_pack_vendor_get, &battery_module_pack_vendor, 0644);

/*
  *Emode function to enable/disable 0% shutdown
  */
int enable_to_shutdown = 1;
static int set_enable_to_shutdown(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	pr_warn("set_enable_to_shutdown to %d\n", enable_to_shutdown);
	return 0;
}

module_param_call(enable_to_shutdown, set_enable_to_shutdown, param_get_uint,
			&enable_to_shutdown, 0644);


enum charger_types_oem charge_type_oem = CHARGER_TYPE_DEFAULT;
module_param_named(
	charge_type_oem, charge_type_oem, int, 0644
);

static int design_capacity = 4000;
static int zte_misc_get_design_capacity(char *val, const struct kernel_param *kp)
{
	int rc = 0;
	int zte_design_capacity = 0;
	struct power_supply *bms_psy;
	union power_supply_propval prop = {0,};

	bms_psy = power_supply_get_by_name("bms");
	if (bms_psy) {
		rc = bms_psy->desc->get_property(bms_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &prop);
		if (rc)
			pr_err("failed to battery design capacity, error:%d.\n", rc);
		else {
			/* convert unit from uAh to mAh if need*/
			if (prop.intval >= 1000000) {
				prop.intval /= 1000;
			}
			zte_design_capacity = prop.intval;
			pr_info("battery design capacity = %dmAh\n",
				zte_design_capacity);
		}
	} else
		pr_err("bms_psy is NULL.\n");

	return  snprintf(val, CHARGER_BUF_SIZE, "%d", zte_design_capacity);
}
module_param_call(design_capacity, NULL, zte_misc_get_design_capacity,
			&design_capacity, 0644);

int wireless_charging_signal_good = 0;
static int zte_misc_get_wireless_charging_signal(char *val, const struct kernel_param *kp)
{
	return  snprintf(val, CHARGER_BUF_SIZE, "%d", wireless_charging_signal_good);
}
module_param_call(wireless_charging_signal_good, NULL, zte_misc_get_wireless_charging_signal,
			&wireless_charging_signal_good, 0644);

int screen_on = SCREEN_OFF;
static int zte_misc_screen_on_set(const char *val, const struct kernel_param *kp)
{
	int rc = 0;
	struct power_supply *batt_psy = NULL;
	struct power_supply *dc_psy = NULL;
	union power_supply_propval value = {0, };

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("%s: error setting value %d\n", __func__, rc);
		return rc;
	}

	value.intval = SCREEN_CHARGE_CONTROL_LIMIT;
	dc_psy = power_supply_get_by_name("dc");
	if (dc_psy) {
		rc = dc_psy->desc->set_property(dc_psy,
					POWER_SUPPLY_PROP_CURRENT_MAX, &value);
		if (rc) {
			pr_err("Can not set DC POWER_SUPPLY_PROP_CURRENT_MAX %d\n", rc);
		}
		rc = dc_psy->desc->set_property(dc_psy,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &value);
		if (rc) {
			pr_err("Can not set DC POWER_SUPPLY_PROP_VOLTAGE_MAX %d\n", rc);
		}
	} else {
		pr_err("%s: Can not get dc psy\n", __func__);
	}

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		value.intval = SCREEN_CHARGE_CONTROL_LIMIT;
		rc = batt_psy->desc->set_property(batt_psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &value);
		if (rc) {
			pr_err("Can not set POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX %d\n", rc);
		}
	} else {
		pr_err("%s: Can not get batt psy\n", __func__);
	}

	pr_info("screen is %s\n", screen_on ? "on" : "off");
	return 0;
}

static int zte_misc_screen_on_get(char *val, const struct kernel_param *kp)
{
	pr_info("zte_misc_screen_on is %d\n", screen_on);
	return snprintf(val, 0x2, "%d",  screen_on);
}

module_param_call(screen_on, zte_misc_screen_on_set,
	zte_misc_screen_on_get, &screen_on, 0644);

static int zte_misc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int error;

	error = get_devtree_pdata(dev);
	if (error)
		return error;

	pr_info("%s done\n", __func__);
	return 0;
}

static int  zte_misc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver zte_misc_device_driver = {
	.probe		= zte_misc_probe,
	.remove		= zte_misc_remove,
	.driver		= {
		.name	= "zte-misc",
		.owner	= THIS_MODULE,
		.of_match_table = zte_misc_of_match,
	}
};

int __init zte_misc_init(void)
{
	return platform_driver_register(&zte_misc_device_driver);
}

static void __exit zte_misc_exit(void)
{
	platform_driver_unregister(&zte_misc_device_driver);
}
fs_initcall(zte_misc_init);
module_exit(zte_misc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Misc driver for zte");
MODULE_ALIAS("platform:zte-misc");
