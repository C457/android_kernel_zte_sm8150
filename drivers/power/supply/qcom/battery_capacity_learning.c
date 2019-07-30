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
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>

#ifndef POWER_SUPPLY_PROP_CURRENT_COUNTER_ZTE
#define POWER_SUPPLY_PROP_CURRENT_COUNTER_ZTE POWER_SUPPLY_PROP_CHARGE_COUNTER
#endif

/*compensate arguments of 0.04C~0.02C*/
struct battery_topoff_prop {
	u32			topoff_age;
	u32			topoff_capacity_delta;
};

/*compensate arguments of 3V~3.4V*/
/*compensate arguments of discharging current*/
struct battery_disch_curr_volt_prop {
	u32			disch_age;
	u32			disch_capacity_curr_150;
	u32			disch_capacity_curr_0d2c;
	u32			disch_capacity_curr_1300;
};

/*compensate arguments of reduction voltage*/
struct battery_reduc_volt_prop {
	u32			reduc_age;
	u32			reduc_volt50_capacity;
	u32			reduc_volt100_capacity;
};

struct battery_correct_capacity {
	u32			correct_capacity;
	u32			correct_capacity_full;
};

struct battery_show_capacity {
	u32			show_capacity;
	u32			avg_current_capacity;
	u32			show_capacity_full;
	u32			show_capacity_raw;
	u32			discharge_current;
};

struct battery_capacity_learn {
	u64		learned_uah;
	u64		uah_init;
	u64		uas_count;
	u64		delta_count;
	u32		cap_learn;
	int		per_current;
};

struct platform_bcl_algo {
	int			delta_init;
	int			uah_init;
	bool		begin_flag;
};


struct batt_cap_learn_parameter {
	u32			turn_on;
	u32			retry_times;
	u32			time_ms;
	int			max_start_soc;
	int			max_increment;
	int			max_decrement;
	int			min_temp;
	int			max_temp;
	int			max_cap_limit;
	int			min_cap_limit;
	int			algorithm_select;
	int			battery_voltage;
	const char	*bms_phy_name;
	const char	*battery_phy_name;
	u32			topoff_prop_step;
	u32			disch_curr_volt_prop_step;
	u32			reduc_volt_prop_step;
	struct battery_topoff_prop	*topoff_prop;
	struct battery_disch_curr_volt_prop *disch_curr_volt_prop;
	struct battery_reduc_volt_prop *reduc_volt_prop;
};

struct batt_cap_learn_info {
	struct device		*dev;
	struct power_supply	*bcl_psy;
	struct power_supply	*battery_psy;
	struct notifier_block	nb;
	struct hrtimer			learn_timer;
	struct workqueue_struct *notify_workqueue;
	struct delayed_work	notify_work;
	struct workqueue_struct *bcl_probe_wq;
	struct delayed_work	bcl_probe_work;
	struct mutex		data_lock;
	spinlock_t		timer_lock;
	struct batt_cap_learn_parameter config_par;
	struct wake_lock		bcl_wake_lock;
	struct battery_capacity_learn bcl_data;
	struct platform_bcl_algo platform_bcl_delta;
	struct battery_correct_capacity correct_capacity;
	struct battery_show_capacity show_capacity;
	ktime_t		timer_interval;
	atomic_t	init_finished;
};

#define FULL_BATT_SOC_PERCENTS		100
#define ONE_SECOND_OF_MILLI_SECOND	1000
#define ONE_MINUTE_OF_SECOND			60
#define MAH_TRANSFER_TO_UAH			1000
#define ONE_HOUR_OF_SECOND			3600
#define CURRENT_150MA					150
#define CURRENT_1300MA					1300
#define VOLTAGE_REDUCE_0MV			0
#define VOLTAGE_REDUCE_50MV			50
#define VOLTAGE_REDUCE_100MV			100
#define CURRENT_0D2C(x)				((2*x/10)/1000)
#define LIST_SIZE_NUM					10

#define USE_PLATFORM_BCL_ALGO			BIT(7)
#define ZTE_BCL_ALGO_ENABLE				BIT(4)
#define PLATFORM_BCL_ALGO_ENABLE		BIT(3)
#define PLATFORM_BCL_ALGO_DELTA_MODE	BIT(0)


#define pr_alarm(fmt, args...)	pr_info("BCL: %s(): "fmt, __func__, ## args)

#define OF_READ_PROPERTY(store, dt_property, retval, default_val)	\
do {											\
	if (retval)								\
		break;								\
											\
	retval = of_property_read_u32(np,			\
					"bcl," dt_property,		\
					&store);					\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_alarm("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
	}										\
	pr_alarm("config: " #dt_property				\
				" property: [%d]\n", store);		\
} while (0)

#define OF_READ_PROPERTY_STRINGS(store, dt_property, retval, default_val)	\
do {											\
	retval = of_property_read_string(np,		\
					"bcl," dt_property,		\
					&(store));				\
											\
	if (retval == -EINVAL) {					\
		retval = 0;							\
		store = default_val;					\
	} else if (retval) {							\
		pr_alarm("Error reading " #dt_property	\
				" property rc = %d\n", retval);	\
		return retval;						\
	}										\
	pr_alarm("config: " #dt_property				\
				" property: [%s]\n", store);		\
} while (0)

#define OF_READ_ARRAY_PROPERTY(prop_data, prop_length, prop_size, dt_property, retval) \
do { \
	if (of_find_property(np, "bcl," dt_property, &prop_length)) { \
		prop_data = kzalloc(prop_length, GFP_KERNEL); \
		retval = of_property_read_u32_array(np, "bcl," dt_property, \
				(u32 *)prop_data, prop_length / sizeof(u32)); \
		if (retval) { \
			retval = -EINVAL; \
			pr_alarm("Error reading " #dt_property \
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
		pr_alarm("Error geting " #dt_property \
				" property rc = %d\n", retval); \
		return retval; \
	} \
	pr_alarm("config: " #dt_property \
				" prop_length: [%d]\n", prop_length);\
} while (0)


static void batt_cap_learn_ktime_enable(struct batt_cap_learn_info *chip);
static void batt_cap_learn_ktime_disable(struct batt_cap_learn_info *chip);
static int batt_cap_learn_battery_stop(struct batt_cap_learn_info *chip);
static int batt_cap_learn_capacity_compensate(struct batt_cap_learn_info *chip);

static int batt_cap_learn_battery_check_temp(struct batt_cap_learn_info *chip)
{
	union power_supply_propval batt_temp = {0, };
	int rc = 0;

	rc = power_supply_get_property(chip->battery_psy,
								POWER_SUPPLY_PROP_TEMP, &batt_temp);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		return false;
	}

	if ((batt_temp.intval > chip->config_par.min_temp)
		&& (batt_temp.intval < chip->config_par.max_temp)) {
		return true;
	}

	pr_alarm("battery temp is out limit: min[%d],max[%d],now[%d]\n",
			chip->config_par.min_temp, chip->config_par.max_temp, batt_temp.intval);

	return false;
}

static int batt_cap_learn_battery_begin(struct batt_cap_learn_info *chip)
{
	union power_supply_propval charge_full_prop = {0, };
	union power_supply_propval battery_capacity_prop = {0, };
	u64 learned_cap_uah = 0;
	u32 batt_soc = 0;
	int rc = 0;

	mutex_lock(&chip->data_lock);

	if (chip->bcl_data.cap_learn) {
		pr_alarm("Capacity learning already is running\n");
		goto failed_loop;
	}

	pr_alarm("### Capacity Learning Start ###\n");

	rc = power_supply_get_property(chip->battery_psy,
						POWER_SUPPLY_PROP_CHARGE_FULL, &charge_full_prop);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		goto failed_loop;
	}

	rc = power_supply_get_property(chip->battery_psy,
						POWER_SUPPLY_PROP_CAPACITY, &battery_capacity_prop);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		goto failed_loop;
	}

	if (chip->show_capacity.show_capacity)
		learned_cap_uah = chip->show_capacity.show_capacity;
	else
		learned_cap_uah = charge_full_prop.intval;

	batt_soc = battery_capacity_prop.intval;

	if (batt_soc > chip->config_par.max_start_soc) {
		pr_alarm("batt_soc is out of limits.%d%% > %d%%\n", batt_soc, chip->config_par.max_start_soc);
		goto failed_loop;
	}

	if (!batt_cap_learn_battery_check_temp(chip)) {
		goto failed_loop;
	}

	chip->bcl_data.uah_init = div64_s64(learned_cap_uah * batt_soc,
											FULL_BATT_SOC_PERCENTS);
	chip->bcl_data.uas_count = 0;

	pr_alarm("uah init: %llu, learned_cap_uah: %llu, batt_soc: %u%%\n",
					chip->bcl_data.uah_init, learned_cap_uah, batt_soc);

	batt_cap_learn_ktime_enable(chip);

	chip->bcl_data.cap_learn = true;

	wake_lock(&chip->bcl_wake_lock);

	mutex_unlock(&chip->data_lock);

	return 0;

failed_loop:
	mutex_unlock(&chip->data_lock);
	return -EINVAL;
}

static int batt_cap_learn_battery_update_check(struct batt_cap_learn_info *chip)
{
	int rc = 0;

	mutex_lock(&chip->data_lock);

	if (!chip->bcl_data.cap_learn) {
		pr_alarm("Capacity learning is stop, exit\n");
		rc = 0;
		goto exit_loop;
	}

	pr_alarm("### Capacity Learning Update Check ###\n");

	/*checking condition when capacity learning is available*/
	if (!batt_cap_learn_battery_check_temp(chip)) {
		rc = -EINVAL;
		goto exit_loop;
	}

	/*update.....*/

exit_loop:
	mutex_unlock(&chip->data_lock);

	return rc;
}

static int batt_cap_learn_battery_save(struct batt_cap_learn_info *chip)
{
	unsigned int period = 0;

	mutex_lock(&chip->data_lock);

	if (!chip->bcl_data.cap_learn) {
		pr_alarm("Capacity learning is stop, exit\n");
		goto failed_loop;
	}

	pr_alarm("### Capacity Learning Save ###\n");
	batt_cap_learn_ktime_disable(chip);

	if (chip->config_par.time_ms <= ONE_SECOND_OF_MILLI_SECOND) {
		period = ONE_SECOND_OF_MILLI_SECOND;
		do_div(period, chip->config_par.time_ms);
	} else {
		period = chip->config_par.time_ms;
		do_div(period, ONE_SECOND_OF_MILLI_SECOND);
	}

	chip->bcl_data.uas_count += chip->bcl_data.delta_count;

	pr_alarm("period calc: %d, timer: %d, uap delta: %lld\n",
				period, chip->config_par.time_ms, chip->bcl_data.uas_count);

	if (chip->config_par.time_ms <= ONE_SECOND_OF_MILLI_SECOND) {
		do_div(chip->bcl_data.uas_count, period);
	} else {
		chip->bcl_data.uas_count = chip->bcl_data.uas_count * period;
	}

	pr_alarm("uas delta: %lld\n", chip->bcl_data.uas_count);

	do_div(chip->bcl_data.uas_count, ONE_HOUR_OF_SECOND);

	pr_alarm("uah delta: %lld\n", chip->bcl_data.uas_count);

	chip->bcl_data.learned_uah = chip->bcl_data.uah_init + chip->bcl_data.uas_count;

	pr_alarm("Capacity Learned(uAh): %lld\n", chip->bcl_data.learned_uah);

	if ((chip->config_par.algorithm_select & USE_PLATFORM_BCL_ALGO) == 0)
		batt_cap_learn_capacity_compensate(chip);

	chip->bcl_data.uas_count = 0;
	chip->bcl_data.uah_init = 0;
	chip->bcl_data.per_current = 0;
	chip->bcl_data.cap_learn = false;

	wake_unlock(&chip->bcl_wake_lock);

	mutex_unlock(&chip->data_lock);

	return 0;

failed_loop:
	mutex_unlock(&chip->data_lock);
	return -EINVAL;
}

static int batt_cap_learn_battery_stop(struct batt_cap_learn_info *chip)
{

	mutex_lock(&chip->data_lock);

	if (!chip->bcl_data.cap_learn) {
		pr_alarm("Capacity learning already is stop\n");
		goto failed_loop;
	}

	pr_alarm("### [DISCHARGING] Capacity Learning Stop ###\n");

	batt_cap_learn_ktime_disable(chip);
	chip->bcl_data.uas_count = 0;
	chip->bcl_data.uah_init = 0;
	chip->bcl_data.per_current = 0;
	chip->bcl_data.cap_learn = false;

	wake_unlock(&chip->bcl_wake_lock);

failed_loop:
	mutex_unlock(&chip->data_lock);

	return 0;
}

static int linearization_handle(int x1, int x2, int y1, int y2, int x)
{
	int y = 0, k = 0;

	if (x1 == x2) {
		pr_alarm("!!!!!! Divisor can not be '0' !!!!!\n");
		return 0;
	}

	/**y = (x - x1) * (y2 - y1) / (x2 - x1) + y1**/

	/**y = (x - x1) * k + y1**/

	k = (y2 - y1) * 1000 / (x2-x1);

	y = (x - x1) * k / 1000 + y1;

	/** pr_alarm("k = %d, y = %d\n", k, y); **/

	return y;
}

static int batt_cap_learn_linearization_topoff(struct batt_cap_learn_info *chip)
{
	struct battery_topoff_prop	linear_prop;
	u32 linear_capacity = 0;
	int i = 0, age = 0;

	memset(&linear_prop, 0, sizeof(struct battery_topoff_prop));

	age = chip->show_capacity.show_capacity *
			FULL_BATT_SOC_PERCENTS / chip->show_capacity.show_capacity_full;

	pr_alarm("age: %d; %d/%d\n", age,
				chip->show_capacity.show_capacity,
				chip->show_capacity.show_capacity_full);

	for (i = 0; i < chip->config_par.topoff_prop_step - 1; i++) {
		if ((age <= chip->config_par.topoff_prop[i].topoff_age)
			&& (age > chip->config_par.topoff_prop[i + 1].topoff_age)) {
			break;
		}
	}

	if (i >= chip->config_par.topoff_prop_step - 1) { /*last step, Not have "last + 1" level*/
		pr_alarm("age[%d] less than [%d],  reset level\n", age, chip->config_par.topoff_prop[i].topoff_age);
		i--;
	}

	pr_alarm("[age -> capacity(topoff)] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(age)\n",
							chip->config_par.topoff_prop[i].topoff_age,
							chip->config_par.topoff_prop[i + 1].topoff_age,
							chip->config_par.topoff_prop[i].topoff_capacity_delta,
							chip->config_par.topoff_prop[i + 1].topoff_capacity_delta,
							age);

	linear_capacity = linearization_handle(chip->config_par.topoff_prop[i].topoff_age,
							chip->config_par.topoff_prop[i + 1].topoff_age,
							chip->config_par.topoff_prop[i].topoff_capacity_delta,
							chip->config_par.topoff_prop[i + 1].topoff_capacity_delta,
							age);

	pr_alarm("linear_capacity: %d\n", linear_capacity);

	return linear_capacity * MAH_TRANSFER_TO_UAH;
}

static int batt_cap_learn_linearization_reduce_voltage(struct batt_cap_learn_info *chip)
{
	union power_supply_propval voltage_prop = {0, };
	struct battery_reduc_volt_prop linear_prop;
	u32 linear_capacity = 0;
	int i = 0, rc = 0, delta_voltage = 0;

	linear_prop.reduc_age = chip->show_capacity.show_capacity *
							FULL_BATT_SOC_PERCENTS / chip->show_capacity.show_capacity_full;

	for (i = 0; i < chip->config_par.reduc_volt_prop_step - 1; i++) {
		if ((linear_prop.reduc_age <= chip->config_par.reduc_volt_prop[i].reduc_age)
			&& (linear_prop.reduc_age > chip->config_par.reduc_volt_prop[i + 1].reduc_age)) {
			break;
		}
	}

	if (i >= chip->config_par.reduc_volt_prop_step - 1) { /*last step, Not have "last + 1" level*/
		pr_alarm("age[%d] less than [%d],  reset level\n", linear_prop.reduc_age,
				chip->config_par.reduc_volt_prop[i].reduc_age);
		i--;
	}

	pr_alarm("[age -> 50_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(V)\n",
						chip->config_par.reduc_volt_prop[i].reduc_age,
						chip->config_par.reduc_volt_prop[i + 1].reduc_age,
						chip->config_par.reduc_volt_prop[i].reduc_volt50_capacity,
						chip->config_par.reduc_volt_prop[i + 1].reduc_volt50_capacity,
						linear_prop.reduc_age);
	linear_prop.reduc_volt50_capacity = linearization_handle(chip->config_par.reduc_volt_prop[i].reduc_age,
						chip->config_par.reduc_volt_prop[i + 1].reduc_age,
						chip->config_par.reduc_volt_prop[i].reduc_volt50_capacity,
						chip->config_par.reduc_volt_prop[i + 1].reduc_volt50_capacity,
						linear_prop.reduc_age);

	pr_alarm("[age -> 100_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(V)\n",
						chip->config_par.reduc_volt_prop[i].reduc_age,
						chip->config_par.reduc_volt_prop[i + 1].reduc_age,
						chip->config_par.reduc_volt_prop[i].reduc_volt100_capacity,
						chip->config_par.reduc_volt_prop[i + 1].reduc_volt100_capacity,
						linear_prop.reduc_age);
	linear_prop.reduc_volt100_capacity = linearization_handle(chip->config_par.reduc_volt_prop[i].reduc_age,
						chip->config_par.reduc_volt_prop[i + 1].reduc_age,
						chip->config_par.reduc_volt_prop[i].reduc_volt100_capacity,
						chip->config_par.reduc_volt_prop[i + 1].reduc_volt100_capacity,
						linear_prop.reduc_age);

	rc = power_supply_get_property(chip->battery_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage_prop);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		goto failed_loop;
	}

	voltage_prop.intval = voltage_prop.intval / MAH_TRANSFER_TO_UAH;

	delta_voltage = chip->config_par.battery_voltage - voltage_prop.intval;

	delta_voltage = (delta_voltage < 0) ? 0 : delta_voltage;

	pr_alarm("delta voltage(%d) = design(%d) - now(%d)\n",
							delta_voltage,
							chip->config_par.battery_voltage,
							voltage_prop.intval);

	if (delta_voltage <= VOLTAGE_REDUCE_50MV) {
		pr_alarm("[voltage(now) -> capacity] x1: %d(mV); x2: %d(mV); y1: %d(mAH); y2: %d(mAH); x: %d(mV)\n",
							VOLTAGE_REDUCE_0MV,
							VOLTAGE_REDUCE_50MV,
							0,
							linear_prop.reduc_volt50_capacity,
							delta_voltage);

		linear_capacity = linearization_handle(VOLTAGE_REDUCE_0MV,
							VOLTAGE_REDUCE_50MV,
							0,
							linear_prop.reduc_volt50_capacity,
							delta_voltage);
	} else {
		pr_alarm("[voltage(now) -> capacity] x1: %d(mV); x2: %d(mV); y1: %d(mAH); y2: %d(mAH); x: %d(mV)\n",
							VOLTAGE_REDUCE_50MV,
							VOLTAGE_REDUCE_100MV,
							linear_prop.reduc_volt50_capacity,
							linear_prop.reduc_volt100_capacity,
							delta_voltage);

		linear_capacity = linearization_handle(VOLTAGE_REDUCE_50MV,
							VOLTAGE_REDUCE_100MV,
							linear_prop.reduc_volt50_capacity,
							linear_prop.reduc_volt100_capacity,
							delta_voltage);
	}

	pr_alarm("linear_capacity: %d\n", linear_capacity);

	return linear_capacity * MAH_TRANSFER_TO_UAH;

failed_loop:
	return 0;
}

static int batt_cap_learn_linearization_disch_volt_curr(struct batt_cap_learn_info *chip)
{
	struct battery_disch_curr_volt_prop linear_prop;
	u32 linear_capacity = 0;
	int i = 0;

	memset(&linear_prop, 0, sizeof(struct battery_disch_curr_volt_prop));

	linear_prop.disch_age = chip->show_capacity.show_capacity *
							FULL_BATT_SOC_PERCENTS / chip->show_capacity.show_capacity_full;

	for (i = 0; i < chip->config_par.disch_curr_volt_prop_step - 1; i++) {
		if ((linear_prop.disch_age <= chip->config_par.disch_curr_volt_prop[i].disch_age)
			&& (linear_prop.disch_age > chip->config_par.disch_curr_volt_prop[i + 1].disch_age)) {
			break;
		}
	}

	if (i >= chip->config_par.disch_curr_volt_prop_step - 1) { /*last step, Not have "last + 1" level*/
		pr_alarm("age[%d] less than [%d],  reset level\n", linear_prop.disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_age);
		i--;
	}

	pr_alarm("[age_now -> 150mA_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(age)\n",
				chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_150,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_150,
				linear_prop.disch_age);
	linear_prop.disch_capacity_curr_150 = linearization_handle(chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_150,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_150,
				linear_prop.disch_age);

	pr_alarm("[age_now -> 0.02C_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(age)\n",
				chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_0d2c,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_0d2c,
				linear_prop.disch_age);

	linear_prop.disch_capacity_curr_0d2c = linearization_handle(chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_0d2c,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_0d2c,
				linear_prop.disch_age);

	pr_alarm("[age_now -> 1300mA_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(age)\n",
				chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_1300,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_1300,
				linear_prop.disch_age);

	linear_prop.disch_capacity_curr_1300 = linearization_handle(chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_1300,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_1300,
				linear_prop.disch_age);

	if (!chip->show_capacity.discharge_current) {
		chip->show_capacity.discharge_current = CURRENT_0D2C(chip->show_capacity.show_capacity_full);
	}

	if (chip->show_capacity.discharge_current < CURRENT_150MA) {
		chip->show_capacity.discharge_current = CURRENT_150MA;
	}

	pr_alarm("discharge_current %d\n", chip->show_capacity.discharge_current);

	if (chip->show_capacity.discharge_current <=
		CURRENT_0D2C(chip->show_capacity.show_capacity_full)) {
		pr_alarm("[current -> cap(3-3.4)] x1: %d(mA); x2: %d(mA); y1: %d(mah); y2: %d(mah); x: %d(mA)\n",
				CURRENT_150MA,
				CURRENT_0D2C(chip->show_capacity.show_capacity_full),
				linear_prop.disch_capacity_curr_150,
				linear_prop.disch_capacity_curr_0d2c,
				chip->show_capacity.discharge_current);
		linear_capacity = linearization_handle(CURRENT_150MA,
				CURRENT_0D2C(chip->show_capacity.show_capacity_full),
				linear_prop.disch_capacity_curr_150,
				linear_prop.disch_capacity_curr_0d2c,
				chip->show_capacity.discharge_current);

	} else {
		pr_alarm("[current -> cap(3-3.4)] x1: %d(mA); x2: %d(mA); y1: %d(mah); y2: %d(mAH); x: %d(mA)\n",
				CURRENT_0D2C(chip->show_capacity.show_capacity_full),
				CURRENT_1300MA,
				linear_prop.disch_capacity_curr_0d2c,
				linear_prop.disch_capacity_curr_1300,
				chip->show_capacity.discharge_current);
		linear_capacity = linearization_handle(CURRENT_0D2C(chip->show_capacity.show_capacity_full),
				CURRENT_1300MA,
				linear_prop.disch_capacity_curr_0d2c,
				linear_prop.disch_capacity_curr_1300,
				chip->show_capacity.discharge_current);
	}

	pr_alarm("discharge current linear capacity: %d\n", linear_capacity);

	return linear_capacity * MAH_TRANSFER_TO_UAH;
}

static int batt_cap_learn_linearization_disch_volt(struct batt_cap_learn_info *chip)
{
	struct battery_disch_curr_volt_prop linear_prop;
	u32 linear_capacity = 0;
	int i = 0;

	memset(&linear_prop, 0, sizeof(struct battery_disch_curr_volt_prop));

	linear_prop.disch_age = chip->show_capacity.show_capacity *
							FULL_BATT_SOC_PERCENTS / chip->show_capacity.show_capacity_full;

	for (i = 0; i < chip->config_par.disch_curr_volt_prop_step - 1; i++) {
		if ((linear_prop.disch_age <= chip->config_par.disch_curr_volt_prop[i].disch_age)
			&& (linear_prop.disch_age > chip->config_par.disch_curr_volt_prop[i + 1].disch_age)) {
			break;
		}
	}

	if (i >= chip->config_par.disch_curr_volt_prop_step - 1) { /*last step, Not have "last + 1" level*/
		pr_alarm("age[%d] less than [%d],  reset level\n", linear_prop.disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_age);
		i--;
	}

	pr_alarm("[age_now -> 0.02C_capacity] x1: %d(age); x2: %d(age); y1: %d(mAH); y2: %d(mAH); x: %d(age)\n",
				chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_0d2c,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_0d2c,
				linear_prop.disch_age);

	linear_capacity = linearization_handle(chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_0d2c,
				chip->config_par.disch_curr_volt_prop[i + 1].disch_capacity_curr_0d2c,
				linear_prop.disch_age);


	pr_alarm("discharge voltage linear capacity: %d\n", linear_capacity);

	return linear_capacity * MAH_TRANSFER_TO_UAH;
}


static int batt_cap_learn_capacity_compensate(struct batt_cap_learn_info *chip)
{
	union power_supply_propval capacity_prop = {0, };
	union power_supply_propval status_prop = {0, };
	int rc = 0, capacity_learn = 0, capacity_current = 0, limit_capacity = 0;

	rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_STATUS, &status_prop);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		goto failed_loop;
	}

	if (status_prop.intval != POWER_SUPPLY_STATUS_FULL) {
		goto failed_loop;
	}

	if (chip->config_par.algorithm_select & USE_PLATFORM_BCL_ALGO) {
		if (chip->config_par.algorithm_select & PLATFORM_BCL_ALGO_DELTA_MODE) {
			rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_CURRENT_COUNTER_ZTE, &capacity_prop);
			if (rc < 0) {
				pr_alarm("Failed to get present property rc=%d\n", rc);
				goto failed_loop;
			}

			pr_alarm("[0] platform delta mode, init: %d, end: %d\n",
						chip->platform_bcl_delta.delta_init,
						capacity_prop.intval);

			capacity_learn = capacity_prop.intval - chip->platform_bcl_delta.delta_init;
		} else {
			rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &capacity_prop);
			if (rc < 0) {
				pr_alarm("Failed to get present property rc=%d\n", rc);
				goto failed_loop;
			}

			if (chip->show_capacity.show_capacity_raw == capacity_prop.intval) {
				pr_alarm("Invaild charge full found\n");
				goto failed_loop;
			}

			pr_alarm("[0] platform integral mode, init: %d, end: %d\n",
						chip->platform_bcl_delta.delta_init,
						capacity_prop.intval);

			capacity_learn = capacity_prop.intval;
		}
	} else { /*USE_ZTE_BCL_ALGO*/
		capacity_learn = chip->bcl_data.learned_uah;
	}

	chip->show_capacity.show_capacity_raw = capacity_learn;

	pr_alarm("[1] learned capacity: %d\n", capacity_learn);

	capacity_learn += batt_cap_learn_linearization_topoff(chip);

	pr_alarm("[2] compensate topoff: %d\n", capacity_learn);

	capacity_learn += batt_cap_learn_linearization_reduce_voltage(chip);

	pr_alarm("[3] learned compensate reduce voltage: %d\n", capacity_learn);

	capacity_current = capacity_learn;

	capacity_learn += batt_cap_learn_linearization_disch_volt(chip);

	pr_alarm("[4] compensate disch volt: %d\n", capacity_learn);

	capacity_current += batt_cap_learn_linearization_disch_volt_curr(chip);

	pr_alarm("[4] compensate disch volt curr: %d\n", capacity_current);

	if (chip->config_par.max_cap_limit) {
		limit_capacity = chip->show_capacity.show_capacity_full *
				chip->config_par.max_cap_limit / FULL_BATT_SOC_PERCENTS;
		if (capacity_learn > limit_capacity) {
			pr_alarm("[5] limit max capacity: %d\n", limit_capacity);
			capacity_learn = limit_capacity;
		}

		if (capacity_current > limit_capacity) {
			pr_alarm("[5] limit max capacity(current): %d\n", limit_capacity);
			capacity_current = limit_capacity;
		}
	}

	if (chip->config_par.min_cap_limit) {
		limit_capacity = chip->show_capacity.show_capacity_full *
				chip->config_par.min_cap_limit / FULL_BATT_SOC_PERCENTS;
		if (capacity_learn < limit_capacity) {
			pr_alarm("[6] limit min capacity: %d\n", limit_capacity);
			capacity_learn = limit_capacity;
		}

		if (capacity_current < limit_capacity) {
			pr_alarm("[6] limit min capacity(current): %d\n", limit_capacity);
			capacity_current = limit_capacity;
		}
	}

	chip->show_capacity.show_capacity = capacity_learn;

	chip->show_capacity.avg_current_capacity = capacity_current;

	if (chip->bcl_psy)
		power_supply_changed(chip->bcl_psy);

failed_loop:
	return 0;
}

static int batt_cap_learn_battery_zte_bcl(struct batt_cap_learn_info *chip)
{
	union power_supply_propval batt_prop = {0, };
	int rc = 0;

	rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		return rc;
	}

	switch (batt_prop.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (!chip->bcl_data.cap_learn) {
			batt_cap_learn_battery_begin(chip);
		} else if (batt_cap_learn_battery_update_check(chip) < 0) {
			batt_cap_learn_battery_stop(chip);
		}
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		batt_cap_learn_battery_stop(chip);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		batt_cap_learn_battery_save(chip);
		break;
	default:
		pr_alarm("### Charge Status Is Unknown ###\n");
		break;
	}

	return rc;
}

static int batt_cap_learn_platform_delta_begin(struct batt_cap_learn_info *chip)
{
	union power_supply_propval capacity_prop = {0, };
	union power_supply_propval capacity_counter_prop = {0, };
	int rc = 0;

	mutex_lock(&chip->data_lock);

	rc = power_supply_get_property(chip->battery_psy,
						POWER_SUPPLY_PROP_CAPACITY, &capacity_prop);
	if (rc < 0) {
		pr_alarm("[delta] Failed to get CAPACITY property rc=%d\n", rc);
		goto failed_loop;
	}

	rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_CURRENT_COUNTER_ZTE, &capacity_counter_prop);
	if (rc < 0) {
		pr_alarm("[delta] Failed to get CHARGE_FULL property rc=%d\n", rc);
		goto failed_loop;
	}

	if (capacity_prop.intval > chip->config_par.max_start_soc) {
		pr_alarm("[delta] batt_soc is out of limits.%d%% > %d%%\n",
					capacity_prop.intval, chip->config_par.max_start_soc);
		goto failed_loop;
	}

	if (!batt_cap_learn_battery_check_temp(chip)) {
		goto failed_loop;
	}

	pr_alarm("### [delta] Capacity Learning Start ###\n");

	chip->platform_bcl_delta.uah_init =
		capacity_prop.intval * chip->show_capacity.show_capacity_full / FULL_BATT_SOC_PERCENTS;

	chip->platform_bcl_delta.delta_init = capacity_counter_prop.intval;

	chip->platform_bcl_delta.begin_flag = true;

	pr_alarm("[delta] set delta begin flag to 1, init uah %d, int delat %d\n",
									chip->platform_bcl_delta.uah_init,
									chip->platform_bcl_delta.delta_init);

failed_loop:
	mutex_unlock(&chip->data_lock);
	return 0;
}

static int batt_cap_learn_platform_delta_update_check(struct batt_cap_learn_info *chip)
{
	int rc = 0;

	mutex_lock(&chip->data_lock);

	if (!chip->platform_bcl_delta.begin_flag) {
		pr_alarm("[delta] Capacity learning is stop, exit\n");
		rc = 0;
		goto exit_loop;
	}

	pr_alarm("[delta] Capacity Learning Update Check ###\n");

	/*checking condition when capacity learning is available*/
	if (!batt_cap_learn_battery_check_temp(chip)) {
		rc = -EINVAL;
		goto exit_loop;
	}

	/*update.....*/

exit_loop:
	mutex_unlock(&chip->data_lock);
	return rc;
}


static int batt_cap_learn_platform_delta_stop(struct batt_cap_learn_info *chip)
{
	mutex_lock(&chip->data_lock);

	if (chip->platform_bcl_delta.begin_flag == true) {
		pr_alarm("[delta] set delta begin flag to 0\n");
		chip->platform_bcl_delta.begin_flag = false;
		chip->platform_bcl_delta.delta_init = 0;
		chip->platform_bcl_delta.uah_init = 0;
	}

	mutex_unlock(&chip->data_lock);

	return 0;
}

static int batt_cap_learn_battery_platform_bcl_delta(struct batt_cap_learn_info *chip)
{
	union power_supply_propval batt_prop = {0, };
	int rc = 0;

	rc = power_supply_get_property(chip->battery_psy,
				POWER_SUPPLY_PROP_STATUS, &batt_prop);
	if (rc < 0) {
		pr_alarm("Failed to get STATUS property rc=%d\n", rc);
		return rc;
	}

	switch (batt_prop.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (!chip->platform_bcl_delta.begin_flag) {
			batt_cap_learn_platform_delta_begin(chip);
		} else if (batt_cap_learn_platform_delta_update_check(chip) < 0) {
			batt_cap_learn_platform_delta_stop(chip);
		}
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		batt_cap_learn_platform_delta_stop(chip);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (chip->platform_bcl_delta.begin_flag) {
			batt_cap_learn_capacity_compensate(chip);
			batt_cap_learn_platform_delta_stop(chip);
		}
		break;
	default:
		pr_alarm("### Charge Status Is Unknown ###\n");
		break;
	}

	return rc;
}


static int batt_cap_learn_notifier_switch(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct batt_cap_learn_info *chip = container_of(nb, struct batt_cap_learn_info, nb);

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, chip->config_par.bms_phy_name) == 0)
		|| (strcmp(psy->desc->name, chip->config_par.battery_phy_name) == 0)
		|| (strcmp(psy->desc->name, "usb") == 0)
		|| (strcmp(psy->desc->name, "ac") == 0)) {

		if (delayed_work_pending(&chip->notify_work))
			return NOTIFY_DONE;

		queue_delayed_work(chip->notify_workqueue, &chip->notify_work, msecs_to_jiffies(50));
	}

	return NOTIFY_DONE;
}

static void batt_cap_learn_notify_handler_work(struct work_struct *work)
{
	struct batt_cap_learn_info *chip =
			container_of(work, struct batt_cap_learn_info, notify_work.work);

	if (chip->config_par.algorithm_select & ZTE_BCL_ALGO_ENABLE) {
			batt_cap_learn_battery_zte_bcl(chip);
	}

	if (chip->config_par.algorithm_select & USE_PLATFORM_BCL_ALGO) {
		if (chip->config_par.algorithm_select & PLATFORM_BCL_ALGO_DELTA_MODE) {
			batt_cap_learn_battery_platform_bcl_delta(chip);
		} else {
			batt_cap_learn_capacity_compensate(chip);
		}
	}
}

static int batt_cap_learn_register_notifier(struct batt_cap_learn_info *chip)
{
	int rc;

	chip->nb.notifier_call = batt_cap_learn_notifier_switch;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_alarm("Couldn't register psy notifier rc = %d\n", rc);
		return -EINVAL;
	}

	return 0;
}

/* hrtimer function handler */
static enum hrtimer_restart batt_cap_learn_timer(struct hrtimer *timer)
{
	struct batt_cap_learn_info *chip =
			container_of(timer, struct batt_cap_learn_info, learn_timer);
	union power_supply_propval current_now = {0, };
	int rc = 0;

	hrtimer_forward_now(&chip->learn_timer, chip->timer_interval);

	rc = power_supply_get_property(chip->battery_psy,
						POWER_SUPPLY_PROP_CURRENT_NOW, &current_now);
	if (rc < 0) {
		pr_alarm("Failed to get present property rc=%d\n", rc);
		return HRTIMER_NORESTART;
	}

	current_now.intval = 0 - current_now.intval;

	chip->bcl_data.uas_count += current_now.intval;

	chip->bcl_data.delta_count += (current_now.intval - chip->bcl_data.per_current) / 2;
/*
	pr_alarm("uas now: %lld, uas delta: %d, delta count: %lld, delta now: %d\n",
				chip->bcl_data.uas_count, current_now.intval,
				chip->bcl_data.delta_count, (current_now.intval - chip->bcl_data.per_current) / 2);
*/
	chip->bcl_data.per_current = current_now.intval;

	return HRTIMER_RESTART;
}

static int batt_cap_learn_check_retry(struct batt_cap_learn_info *chip)
{
	static u32 probe_count = 0;

	probe_count++;

	pr_alarm("Battery Capacity Learning Driver Retry[%d/%d]!!!\n", probe_count, chip->config_par.retry_times);
	if (probe_count < chip->config_par.retry_times) {
		queue_delayed_work(chip->bcl_probe_wq, &chip->bcl_probe_work, msecs_to_jiffies(300));
		return true;
	}

	return false;
}

static int batt_cap_learn_parse_dt(struct batt_cap_learn_info *chip)
{
	int retval = 0, i = 0;
	struct device_node *np = chip->dev->of_node;

	OF_READ_PROPERTY(chip->config_par.turn_on,
					"enable", retval, 0);

	OF_READ_PROPERTY(chip->config_par.retry_times,
					"retry-times", retval, 10);

	OF_READ_PROPERTY(chip->config_par.time_ms,
					"timer-period", retval, 1000);

	OF_READ_PROPERTY(chip->config_par.max_start_soc,
					"max-start-capacity", retval, 15);

	OF_READ_PROPERTY(chip->config_par.max_temp,
					"max-temp-decidegc", retval, 450);

	OF_READ_PROPERTY(chip->config_par.min_temp,
					"min-temp-decidegc", retval, 150);

	OF_READ_PROPERTY(chip->config_par.max_cap_limit,
					"max-limit-deciperc", retval, 100);

	OF_READ_PROPERTY(chip->config_par.min_cap_limit,
					"min-limit-deciperc", retval, 50);

	OF_READ_PROPERTY(chip->config_par.max_increment,
					"max-increment-deciperc", retval, 50);

	OF_READ_PROPERTY(chip->config_par.max_decrement,
					"max-decrement-deciperc", retval, 100);

	OF_READ_PROPERTY_STRINGS(chip->config_par.bms_phy_name,
							"bms-phy-name", retval, "bms");

	OF_READ_PROPERTY_STRINGS(chip->config_par.battery_phy_name,
							"battery-phy-name", retval, "battery");

	OF_READ_PROPERTY(chip->config_par.algorithm_select,
					"algorithm-select", retval, 0);

	OF_READ_PROPERTY(chip->config_par.battery_voltage,
					"battery-specify-voltage", retval, 4400);

	OF_READ_PROPERTY(chip->show_capacity.show_capacity_full,
					"battery-specify-capacity", retval, 3500);

	OF_READ_ARRAY_PROPERTY(chip->config_par.topoff_prop,
						chip->config_par.topoff_prop_step,
						sizeof(struct battery_topoff_prop),
						"topoff-prop", retval);

	for (i = 0; i < chip->config_par.topoff_prop_step; i++) {
		pr_alarm("topoff-prop data:%d\t%d\n", chip->config_par.topoff_prop[i].topoff_age,
				chip->config_par.topoff_prop[i].topoff_capacity_delta);
	}

	OF_READ_ARRAY_PROPERTY(chip->config_par.disch_curr_volt_prop,
						chip->config_par.disch_curr_volt_prop_step,
						sizeof(struct battery_disch_curr_volt_prop),
						"disch-curr-volt-prop", retval);

	for (i = 0; i < chip->config_par.disch_curr_volt_prop_step; i++) {
		pr_alarm("disch-curr-volt-prop data:%d\t%d\t%d\t%d\n",
				chip->config_par.disch_curr_volt_prop[i].disch_age,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_150,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_0d2c,
				chip->config_par.disch_curr_volt_prop[i].disch_capacity_curr_1300);
	}

	OF_READ_ARRAY_PROPERTY(chip->config_par.reduc_volt_prop,
						chip->config_par.reduc_volt_prop_step,
						sizeof(struct battery_reduc_volt_prop),
						"reduc-volt-prop", retval);

	for (i = 0; i < chip->config_par.reduc_volt_prop_step; i++) {
		pr_alarm("reduc-volt-prop data:%d\t%d\t%d\n",
				chip->config_par.reduc_volt_prop[i].reduc_age,
				chip->config_par.reduc_volt_prop[i].reduc_volt50_capacity,
				chip->config_par.reduc_volt_prop[i].reduc_volt100_capacity);
	}

	return 0;
}

static int bcl_psy_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *pval)
{
	struct batt_cap_learn_info *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!chip || !atomic_read(&chip->init_finished)) {
		pr_alarm("BCL Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = chip->show_capacity.show_capacity_full;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pval->intval = chip->show_capacity.show_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		pval->intval = chip->show_capacity.show_capacity_raw;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		pval->intval = chip->show_capacity.discharge_current;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		pval->intval = chip->config_par.algorithm_select;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		pval->intval = chip->show_capacity.avg_current_capacity;
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int bcl_psy_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *pval)
{
	struct batt_cap_learn_info *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	if (!chip || !atomic_read(&chip->init_finished)) {
		pr_alarm("BCL Uninitialized!!!\n");
		return -ENODATA;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		chip->show_capacity.show_capacity = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		chip->bcl_data.learned_uah = pval->intval;
		pr_alarm("Set Capacity Learned(uAh): %lld\n",
				chip->bcl_data.learned_uah);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (chip->show_capacity.discharge_current != pval->intval) {
			chip->show_capacity.discharge_current = pval->intval;
			pr_alarm("Set Discharge Current(uAh): %d\n",
					chip->show_capacity.discharge_current);
		}
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		chip->config_par.algorithm_select = pval->intval;
		pr_alarm("Set Algorithm Select: %d\n", pval->intval);
		if (chip->bcl_psy)
			power_supply_changed(chip->bcl_psy);
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int bcl_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_AUTHENTIC:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		return 1;
	default:
		break;
	}

	return 0;
}

static void bcl_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}


static enum power_supply_property bcl_psy_props[] = {
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CURRENT_AVG,
};

static const struct power_supply_desc bcl_psy_desc = {
	.name = "bcl",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = bcl_psy_props,
	.num_properties = ARRAY_SIZE(bcl_psy_props),
	.get_property = bcl_psy_get_property,
	.set_property = bcl_psy_set_property,
	.external_power_changed = bcl_external_power_changed,
	.property_is_writeable = bcl_property_is_writeable,
};

static void batt_cap_learn_charge_full_init(struct batt_cap_learn_info *chip)
{
	union power_supply_propval capacity_prop = {0, };
	int rc = 0, retry_num = 10;

	do {
		if (chip->config_par.algorithm_select & USE_PLATFORM_BCL_ALGO) {
			if (chip->config_par.algorithm_select & PLATFORM_BCL_ALGO_DELTA_MODE) {
				chip->show_capacity.show_capacity_raw = 0;
			} else {
				rc = power_supply_get_property(chip->battery_psy,
					POWER_SUPPLY_PROP_CHARGE_FULL, &capacity_prop);
				if (rc < 0) {
					pr_alarm("Failed to get CHARGE_FULL property rc=%d\n", rc);
				} else if (capacity_prop.intval) {
					chip->show_capacity.show_capacity_raw = capacity_prop.intval;
					break;
				}
			}
		}
		msleep(500);
	} while (retry_num--);

	pr_alarm("init show_capacity_raw %d\n", chip->show_capacity.show_capacity_raw);

}


static void batt_cap_learn_probe_work(struct work_struct *work)
{
	struct batt_cap_learn_info *chip =
			container_of(work, struct batt_cap_learn_info, bcl_probe_work.work);
	struct power_supply_config bcl_psy_cfg;

	pr_alarm("Battery Capacity Learning Driver Init Begin.....\n");
	atomic_set(&chip->init_finished, 0);

	chip->battery_psy = power_supply_get_by_name(chip->config_par.battery_phy_name);
	if (!chip->battery_psy) {
		pr_alarm("Get battery psy failed!!!!\n");
		goto battery_psy_failed;
	}

	/* waiting BRD set data*/
	chip->show_capacity.show_capacity = 0;

	chip->platform_bcl_delta.begin_flag = false;
	/*chip->show_capacity.show_capacity = chip->show_capacity.show_capacity_full;*/

	/* Register the power supply */
	bcl_psy_cfg.drv_data = chip;
	bcl_psy_cfg.of_node = NULL;
	bcl_psy_cfg.supplied_to = NULL;
	bcl_psy_cfg.num_supplicants = 0;
	chip->bcl_psy = devm_power_supply_register(chip->dev, &bcl_psy_desc,
					&bcl_psy_cfg);
	if (IS_ERR(chip->bcl_psy)) {
		pr_alarm("failed to register bcl_psy rc = %ld\n",
				PTR_ERR(chip->bcl_psy));
		goto register_power_supply_failed;
	}

	wake_lock_init(&chip->bcl_wake_lock, WAKE_LOCK_SUSPEND, "battery_capacity_learning");

	mutex_init(&chip->data_lock);

	spin_lock_init(&chip->timer_lock);

	chip->timer_interval = ms_to_ktime(chip->config_par.time_ms);
	hrtimer_init(&chip->learn_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	chip->learn_timer.function = batt_cap_learn_timer;

	chip->notify_workqueue = create_singlethread_workqueue("battery_capacity_learning");
	INIT_DELAYED_WORK(&chip->notify_work, batt_cap_learn_notify_handler_work);

	if (batt_cap_learn_register_notifier(chip) < 0) {
		pr_alarm("Battery Capacity Learning  register notifier failed!!!!\n");
		goto register_notifier_failed;
	}

	batt_cap_learn_charge_full_init(chip);

	atomic_set(&chip->init_finished, 1);
	pr_alarm("Battery Capacity Learning Driver Init Finished!!!\n");

	return;
register_power_supply_failed:
	power_supply_unreg_notifier(&chip->nb);
register_notifier_failed:
	power_supply_put(chip->battery_psy);
	if (chip->bcl_psy)
		power_supply_unregister(chip->bcl_psy);
battery_psy_failed:
	if (batt_cap_learn_check_retry(chip) == true) {
		return;
	}

	devm_kfree(chip->dev, chip);

	pr_alarm("Battery Capacity Learning Driver Init Failed!!!\n");
}

static int batt_cap_learn_probe(struct platform_device *pdev)
{
	struct batt_cap_learn_info *chip;

	pr_alarm("Battery Capacity Learning Driver Probe Begin.....\n");

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_alarm("devm_kzalloc failed!!!!\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	atomic_set(&chip->init_finished, 0);

	platform_set_drvdata(pdev, chip);

	if (batt_cap_learn_parse_dt(chip) < 0) {
		pr_alarm("Parse dts failed!!!!\n");
		goto parse_dt_failed;
	}

	if (!chip->config_par.turn_on) {
		pr_alarm("Battery Capacity Learning Disabled,Please Config \"bcl,enable\"!!!!\n");
		goto parse_dt_failed;
	}

	chip->bcl_probe_wq = create_singlethread_workqueue("bcl_probe_wq");

	INIT_DELAYED_WORK(&chip->bcl_probe_work, batt_cap_learn_probe_work);

	queue_delayed_work(chip->bcl_probe_wq, &chip->bcl_probe_work, msecs_to_jiffies(100));

	pr_alarm("Battery Capacity Learning Driver Probe Finished.....\n");

	return 0;

parse_dt_failed:
	devm_kfree(chip->dev, chip);
	chip = NULL;

	pr_alarm("Battery Capacity Learning Driver Probe Failed!!!\n");

	return 0;
}
static int batt_cap_learn_remove(struct platform_device *pdev)
{
	struct batt_cap_learn_info *chip = platform_get_drvdata(pdev);

	pr_alarm("Battery Capacity Learning Driver Remove Begin.....\n");

	if (!chip) {
		pr_alarm("chip is null\n");
		goto failed_loop;
	}

	atomic_set(&chip->init_finished, 0);

	batt_cap_learn_battery_stop(chip);

	power_supply_unreg_notifier(&chip->nb);

	mutex_destroy(&chip->data_lock);

	power_supply_put(chip->battery_psy);

	power_supply_unregister(chip->bcl_psy);

	devm_kfree(chip->dev, chip);

	chip = NULL;

failed_loop:
	pr_alarm("Battery Capacity Learning Driver Remove Failed!!!\n");

	return 0;
}

static void batt_cap_learn_ktime_enable(struct batt_cap_learn_info *chip)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&chip->timer_lock, flags);

	hrtimer_start(&chip->learn_timer, chip->timer_interval, HRTIMER_MODE_REL);

	spin_unlock_irqrestore(&chip->timer_lock, flags);
}

static void batt_cap_learn_ktime_disable(struct batt_cap_learn_info *chip)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&chip->timer_lock, flags);

	hrtimer_cancel(&chip->learn_timer);

	spin_unlock_irqrestore(&chip->timer_lock, flags);
}

static const struct of_device_id match_table[] = {
	{ .compatible = "zte,battery-capacity-learning", },
	{ },
};

static struct platform_driver batt_cap_learn_driver = {
	.driver		= {
		.name		= "zte,battery-capacity-learning",
		.owner		= THIS_MODULE,
		.of_match_table	= match_table,
	},
	.probe		= batt_cap_learn_probe,
			.remove		= batt_cap_learn_remove,
				};

module_platform_driver(batt_cap_learn_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cui.xiaochuan <cui.xiaochuan@zte.com>");
MODULE_DESCRIPTION("Battery Capacity Learning Driver");
