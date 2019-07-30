/* Copyright (c) 2012, ZTE. All rights reserved.
*
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 and
*only version 2 as published by the Free Software Foundation.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#if defined(CONFIG_MFD_AW9106B)
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/mfd/aw9106b.h>
#elif defined(CONFIG_MFD_AW9523B)
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/mfd/aw9523b.h>
#endif

struct aw9106b_led {
	struct led_classdev cdev;
	int num_pins;
	struct aw9106b_led_data *sub;
	struct delayed_work blink_work;
};

static struct aw9106b_led_data power_blue_led_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_0, LED_MODE, 0, LEVEL_1),
};

static struct aw9106b_led_data voice_phone_led_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P0_1, LED_MODE, 0, LEVEL_OFF),
};

static struct aw9106b_led_data net_blue_led_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_1, LED_MODE, 0, LEVEL_OFF),
};
static struct aw9106b_led_data net_green_led_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_2, LED_MODE, 0, LEVEL_OFF),
};
static struct aw9106b_led_data net_red_led_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_3, LED_MODE, 0, LEVEL_OFF),
};

static struct aw9106b_led_data wifi_power_ctrl_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_7, GPIO_MODE, 0, LEVEL_OFF),
};

static struct aw9106b_led_data sinal_strength_green_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_5, GPIO_MODE, 0, LEVEL_OFF),
};

static struct aw9106b_led_data signal_strength_blue_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_4, GPIO_MODE, 0, LEVEL_OFF),
};

static struct aw9106b_led_data signal_strength_red_data[] = {
	AW9106B_LED_DATA(0, AW9523_PIN_P1_6, GPIO_MODE, 0, LEVEL_OFF),
};

static struct led_info aw9106b_leds_info[] = {
	{
		.name		= "led:power_green",
		.flags		= ARRAY_SIZE(power_blue_led_data),
		.data		= power_blue_led_data,
	},
	{
		.name		= "led:voice_blue",
		.flags		= ARRAY_SIZE(voice_phone_led_data),
		.data		= voice_phone_led_data,
	},
	{
		.name		= "led:net_blue",
		.flags		= ARRAY_SIZE(net_blue_led_data),
		.data		= net_blue_led_data,
	},
	{
		.name		= "led:net_green",
		.flags		= ARRAY_SIZE(net_green_led_data),
		.data		= net_green_led_data,
	},
	{
		.name		= "led:net_red",
		.flags		= ARRAY_SIZE(net_red_led_data),
		.data		= net_red_led_data,
	},
	{
		.name		= "led:wifi_power_ctrl",
		.flags		= ARRAY_SIZE(wifi_power_ctrl_data),
		.data		= wifi_power_ctrl_data,
	},
	{
		.name		= "led:signal_green",
		.flags		= ARRAY_SIZE(sinal_strength_green_data),
		.data		= sinal_strength_green_data,
	},
	{
		.name		= "led:signal_blue",
		.flags		= ARRAY_SIZE(signal_strength_blue_data),
		.data		= signal_strength_blue_data,
	},
	{
		.name		= "led:signal_red",
		.flags		= ARRAY_SIZE(signal_strength_red_data),
		.data		= signal_strength_red_data,
	}
};

static struct led_platform_data aw9106b_leds_pdata = {
	.leds = aw9106b_leds_info,
	.num_leds = ARRAY_SIZE(aw9106b_leds_info),
};

static struct platform_device aw9106b_leds_device = {
	.name   = "aw9106b-leds",
	.id     = -1,
	.dev    = {
	.platform_data  = &aw9106b_leds_pdata,
	},
};

static inline int aw9106b_led_level(int brightness)
{
	switch (brightness) {
	case LEVEL_OFF:
		return AW9106B_LEVEL_OFF;
	case LEVEL_1:
		return AW9106B_LEVEL_1;
	case LEVEL_2:
		return AW9106B_LEVEL_2;
	case LEVEL_3:
		return AW9106B_LEVEL_3;
	case LEVEL_4:
		return AW9106B_LEVEL_4;
	case LEVEL_5:
		return AW9106B_LEVEL_5;
	default:
		return brightness;
	}
}

static inline int aw9106b_led_gpio_level(int brightness, int gpio_active, int level)
{
	int ret = 0;

	switch (brightness) {
	case LEVEL_OFF:
		if (gpio_active) {
			ret = 0;
		} else {
			ret = 1;
		}
		break;
	default:
		if (brightness >= level) {
			if (gpio_active) {
				ret = 1;
			} else {
				ret = 0;
			}
		} else {
			if (gpio_active)
				ret = 0;
			else
				ret = 1;
		}
	}

	return ret;
}

static void aw9106b_led_set(struct aw9106b_led *led, int brightness)
{
	int ret;
	int i;

	if (brightness < LED_OFF || brightness > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value");
		return;
	}

	for (i = 0; i < led->num_pins; i++) {
		if (led->sub[i].control_mode == GPIO_MODE) {
			led->sub[i].config.blink = 0;
			led->sub[i].config.mode = GPIO_MODE_OUTPUT;
			led->sub[i].config.level = aw9106b_led_gpio_level(brightness,
										led->sub[i].active, led->sub[i].level);
		} else {
			led->sub[i].config.blink = 0;
			led->sub[i].config.mode = LED_BLINK;
			led->sub[i].config.level = aw9106b_led_level(brightness);
		}
		#if defined(CONFIG_MFD_AW9106B)
			ret = aw9106b_config(led->sub[i].id, led->sub[i].pin, &led->sub[i].config);
		#elif defined(CONFIG_MFD_AW9523B)
			ret = aw9523b_config(led->sub[i].id, led->sub[i].pin, &led->sub[i].config);
		#endif

		if (ret) {
			dev_err(led->cdev.dev, "can't set aw9106b led\n");
			return;
		}
	}
}

static void aw9106b_brightness_set(struct led_classdev *led_cdev,
										enum led_brightness value)
{
	struct aw9106b_led *led = container_of(led_cdev, struct aw9106b_led, cdev);

	cancel_delayed_work(&led->blink_work);
	aw9106b_led_set(led, (int)value);
}

static void aw9106b_led_blink_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = container_of(work, struct delayed_work, work);
	struct aw9106b_led *led = container_of(delayed_work, struct aw9106b_led, blink_work);
	struct led_classdev *led_cdev = &led->cdev;
	unsigned long brightness;
	unsigned long delay;

	if (!led_cdev->blink_delay_on) {
		led_cdev->brightness = LED_OFF;
		aw9106b_led_set(led, led_cdev->brightness);
		return;
	}
	if (!led_cdev->blink_delay_off) {
		led_cdev->brightness = led_cdev->blink_brightness;
		aw9106b_led_set(led, led_cdev->brightness);
		return;
	}

	brightness = led_cdev->brightness;
	if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led_cdev->blink_brightness;
		delay = led_cdev->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led_cdev->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led_cdev->blink_delay_off;
	}

	led_cdev->brightness = brightness;
	aw9106b_led_set(led, led_cdev->brightness);
	schedule_delayed_work(&led->blink_work, msecs_to_jiffies(delay));
}

int aw9106b_led_software_blink(struct led_classdev *led_cdev,
					unsigned long *delay_on,
					unsigned long *delay_off)
{
	struct aw9106b_led *led = container_of(led_cdev, struct aw9106b_led, cdev);
	int current_brightness;

	current_brightness = led_cdev->brightness;
	if (current_brightness)
		led_cdev->blink_brightness = current_brightness;
	if (!led_cdev->blink_brightness)
		led_cdev->blink_brightness = led_cdev->max_brightness;

	cancel_delayed_work(&led->blink_work);
	led_cdev->blink_delay_on = *delay_on;
	led_cdev->blink_delay_off = *delay_off;
#if defined(CONFIG_BOARD_MF95) && !defined(CONFIG_BOARD_MF923)
	aw9106b_led_set(led, led_cdev->blink_brightness);
#else
	schedule_delayed_work(&led->blink_work, 0);
#endif
	return 0;
}

int	aw9106b_led_hardware_blink(struct led_classdev *led_cdev,
					unsigned long *delay_on,
					unsigned long *delay_off)
{
	struct aw9106b_led *led = container_of(led_cdev, struct aw9106b_led, cdev);
	int i, ret = -1;

	led = container_of(led_cdev, struct aw9106b_led, cdev);
	for (i = 0; i < led->num_pins; i++) {
		if (led->sub[i].control_mode == GPIO_MODE) {
			dev_err(led->cdev.dev, "hardware blink is not supported by gpio mode\n");
		} else {
			led->sub[i].config.blink = 1;
			led->sub[i].config.mode = LED_BLINK;
			led->sub[i].config.level = aw9106b_led_level(led_cdev->brightness);
			led->sub[i].config.delay_on = *delay_on;
			led->sub[i].config.delay_off = *delay_off;
	#if defined(CONFIG_MFD_AW9106B)
			ret = aw9106b_config(led->sub[i].id, led->sub[i].pin, &led->sub[i].config);
	#elif defined(CONFIG_MFD_AW9523B)
			ret = aw9523b_config(led->sub[i].id, led->sub[i].pin, &led->sub[i].config);
	#endif

			if (ret) {
				dev_err(led->cdev.dev, "%s: can't set aw9106b led\n", __func__);
				return ret;
			}
		}
	}
	return ret;
}

/****zte add delay_on/delay_off node start****/
static ssize_t led_delay_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(buf), "%lu\n", led_cdev->blink_delay_on);
}

static ssize_t led_delay_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	unsigned long state;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led_blink_set(led_cdev, &state, &led_cdev->blink_delay_off);
	led_cdev->blink_delay_on = state;

	return size;
}

static ssize_t led_delay_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(buf), "%lu\n", led_cdev->blink_delay_off);
}

static ssize_t led_delay_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	unsigned long state;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led_blink_set(led_cdev, &led_cdev->blink_delay_on, &state);
	led_cdev->blink_delay_off = state;

	return size;
}

static DEVICE_ATTR(delay_on, 0644, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0644, led_delay_off_show, led_delay_off_store);
/****zte add delay_on/delay_off node end****/

static int aw9106b_led_probe(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct aw9106b_led *led, *tmp_led;
	struct aw9106b_led_data *led_data;
	int i, j, rc;

	if (!pdata) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	led = kcalloc(pdata->num_leds, sizeof(*led), GFP_KERNEL);
	if (!led) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, led);

	for (i = 0; i < pdata->num_leds; i++) {
		tmp_led	= &led[i];
		tmp_led->cdev.name = pdata->leds[i].name;
		tmp_led->cdev.brightness_set = aw9106b_brightness_set;
		tmp_led->cdev.max_brightness = LED_FULL;
		tmp_led->cdev.blink_brightness = LEVEL_1;
		tmp_led->cdev.blink_set = aw9106b_led_software_blink;
		tmp_led->num_pins = pdata->leds[i].flags;
		tmp_led->sub = pdata->leds[i].data;
		tmp_led->cdev.brightness = tmp_led->sub[0].level;
		for (j = 0; j < tmp_led->num_pins; j++) {
			led_data = &tmp_led->sub[j];
			if (led_data->control_mode == GPIO_MODE) {
				led_data->config.mode = GPIO_MODE_OUTPUT;
				led_data->config.level = aw9106b_led_gpio_level(tmp_led->cdev.brightness,
					led_data->active, led_data->level);
			} else {
				led_data->config.mode = LED_BLINK;
				led_data->config.level = aw9106b_led_level(led_data->level);
			}
		};
		INIT_DELAYED_WORK(&tmp_led->blink_work, aw9106b_led_blink_work);
	}
	for (i = 0; i < pdata->num_leds; i++) {
		tmp_led	= &led[i];
		rc = led_classdev_register(&pdev->dev, &tmp_led->cdev);
		if (rc) {
			dev_err(&pdev->dev, "failed to register led\n");
			goto unreg_led_cdev;
		}
/****zte add delay_on/delay_off node start****/
		rc = device_create_file(tmp_led->cdev.dev, &dev_attr_delay_on);
		if (rc) {
			dev_err(&pdev->dev, "failed to creat dev_attr_delay_on\n");
			goto unreg_led_cdev;
		}
		rc = device_create_file(tmp_led->cdev.dev, &dev_attr_delay_off);
		if (rc) {
			dev_err(&pdev->dev, "failed to creat dev_attr_delay_off\n");
			goto unreg_led_cdev;
		}
/****zte add delay_on/delay_off node end****/
	}
	for (i = 0; i < pdata->num_leds; i++) {
		for (j = 0; j < led[i].num_pins; j++) {
			#if defined(CONFIG_MFD_AW9106B)
				aw9106b_config(led[i].sub[j].id, led[i].sub[j].pin, &(led[i].sub[j].config));
			#elif defined(CONFIG_MFD_AW9523B)
				aw9523b_config(led[i].sub[j].id, led[i].sub[j].pin, &(led[i].sub[j].config));
			#endif
		}
	}
	return 0;

unreg_led_cdev:
	while (i) {
		led_classdev_unregister(&led[--i].cdev);
	}
	kfree(led);
	return rc;
}

static int  aw9106b_led_remove(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct aw9106b_led *led = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pdata->num_leds; i++) {
		cancel_delayed_work(&led[i].blink_work);
		led_classdev_unregister(&led[i].cdev);
	}

	kfree(led);
	return 0;
}

static struct platform_driver aw9106b_led_driver = {
	.probe		= aw9106b_led_probe,
	.remove		= aw9106b_led_remove,
	.driver		= {
		.name	= "aw9106b-leds",
		.owner	= THIS_MODULE,
	},
};

static int  __init aw9106b_led_init(void)
{
	platform_driver_register(&aw9106b_led_driver);
	return platform_device_register(&aw9106b_leds_device);
}
module_init(aw9106b_led_init);

static void __exit aw9106b_led_exit(void)
{
	platform_driver_unregister(&aw9106b_led_driver);
}
module_exit(aw9106b_led_exit);

MODULE_DESCRIPTION("AW9106B LEDs driver");
MODULE_LICENSE("GPL v2");
