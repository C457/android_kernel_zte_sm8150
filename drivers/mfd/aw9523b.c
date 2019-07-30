/*
*Copyright 2009-2010 ZTE, Inc. All Rights Reserved.
*The code contained herein is licensed under the GNU General Public
*License. You may obtain a copy of the GNU General Public License
*Version 2 or later at the following locations
*http://www.opensource.org/licenses/gpl-license.html
*http://www.gnu.org/copyleft/gpl.html
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/pwm.h>
#include <linux/fsl_devices.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/mfd/aw9523b.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#define AW9523B_NAME  "aw9523b"
#define AW9523B_MAJOR  0

static struct i2c_client *aw9523b_client;
static const struct of_device_id aw9523b_match_table[] = {
	{.compatible = "aw9523b"},
	{}
};

MODULE_DEVICE_TABLE(of, aw9523b_match_table);

struct aw9523b_dev {
	struct device *dev;
	struct i2c_client *client;
	int id;
	struct list_head device_entry;
};
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static int aw9523b_i2c_read(struct i2c_client *client, unsigned char reg)
{
	int data;

	data = i2c_smbus_read_byte_data(client, reg);
	dev_dbg(&client->dev, "aw9523 reg 0x%0X = 0x%X\n", reg, data);
	return data;
}

static int aw9523b_i2c_write(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	dev_dbg(&client->dev, "aw9523 reg 0x%0X = 0x%X\n", reg, val);
	return i2c_smbus_write_byte_data(client, reg, val);
}

static void aw9523b_reg_set(struct i2c_client *client, unsigned char reg, int shift)
{
	unsigned char value;

	value = aw9523b_i2c_read(client, reg);
	value |= (1<<shift);
	aw9523b_i2c_write(client, reg, value);
}

static void aw9523b_reg_clear(struct i2c_client *client, unsigned char reg, int shift)
{
	unsigned char value;

	value = aw9523b_i2c_read(client, reg);
	value &= ~(1<<shift);
	aw9523b_i2c_write(client, reg, value);
}

static int aw9523b_get_pin_group(int pin)
{
	switch (pin) {
	case AW9523_PIN_P0_0:
	case AW9523_PIN_P0_1:
	case AW9523_PIN_P0_2:
	case AW9523_PIN_P0_3:
	case AW9523_PIN_P0_4:
	case AW9523_PIN_P0_5:
	case AW9523_PIN_P0_6:
	case AW9523_PIN_P0_7:
		return GROUP_P0;
	case AW9523_PIN_P1_0:
	case AW9523_PIN_P1_1:
	case AW9523_PIN_P1_2:
	case AW9523_PIN_P1_3:
	case AW9523_PIN_P1_4:
	case AW9523_PIN_P1_5:
	case AW9523_PIN_P1_6:
	case AW9523_PIN_P1_7:
		return GROUP_P1;
	default:
		return (-1);
	}
}

static int aw9523b_get_pin_shift(int pin)
{
	switch (pin) {
	case AW9523_PIN_P0_0:
	case AW9523_PIN_P0_1:
	case AW9523_PIN_P0_2:
	case AW9523_PIN_P0_3:
	case AW9523_PIN_P0_4:
	case AW9523_PIN_P0_5:
	case AW9523_PIN_P0_6:
	case AW9523_PIN_P0_7:
		return pin;
	case AW9523_PIN_P1_0:
	case AW9523_PIN_P1_1:
	case AW9523_PIN_P1_2:
	case AW9523_PIN_P1_3:
	case AW9523_PIN_P1_4:
	case AW9523_PIN_P1_5:
	case AW9523_PIN_P1_6:
	case AW9523_PIN_P1_7:
		return (pin - 8);
	default:
		return (-1);
	}
}

static unsigned char aw9523b_get_pin_reg_address(int pin)
{
	unsigned char reg = -1;

	switch (pin) {
	case AW9523_PIN_P0_0:
		reg = 0x24;
		break;
	case AW9523_PIN_P0_1:
		reg = 0x25;
		break;
	case AW9523_PIN_P0_2:
		reg = 0x26;
		break;
	case AW9523_PIN_P0_3:
		reg = 0x27;
		break;
	case AW9523_PIN_P0_4:
		reg = 0x28;
		break;
	case AW9523_PIN_P0_5:
		reg = 0x29;
		break;
	case AW9523_PIN_P0_6:
		reg = 0x2A;
		break;
	case AW9523_PIN_P0_7:
		reg = 0x2B;
		break;
	case AW9523_PIN_P1_0:
		reg = 0x20;
		break;
	case AW9523_PIN_P1_1:
		reg = 0x21;
		break;
	case AW9523_PIN_P1_2:
		reg = 0x22;
		break;
	case AW9523_PIN_P1_3:
		reg = 0x23;
		break;
	case AW9523_PIN_P1_4:
		reg = 0x2C;
		break;
	case AW9523_PIN_P1_5:
		reg = 0x2D;
		break;
	case AW9523_PIN_P1_6:
		reg = 0x2E;
		break;
	case AW9523_PIN_P1_7:
		reg = 0x2F;
		break;
	default:
		return (-1);
	}

	return reg;
}

static void aw9523b_set_mode(struct aw9523b_dev *aw9523b, aw9523b_pin pin, aw9106b_mode mode)
{
	struct i2c_client *client = aw9523b->client;
	unsigned char reg_gpmd, reg_gpio_cfg, reg_ctl = 0;
	int group = aw9523b_get_pin_group(pin);
	int shift = aw9523b_get_pin_shift(pin);

	if (group == GROUP_P0) {
		reg_gpmd = GPMD_P0;
		reg_gpio_cfg = GPIO_CFG_P0;
		reg_ctl = OVERALL_CTL;
	} else if (group == GROUP_P1) {
		reg_gpmd = GPMD_P1;
		reg_gpio_cfg = GPIO_CFG_P1;
	} else {
		return;
	}
	switch (mode) {
	case LED_SMART_FADE:
		aw9523b_reg_clear(client, reg_gpmd, shift);
		aw9523b_reg_clear(client, reg_gpio_cfg, shift);
		break;
	case LED_BLINK:
		aw9523b_reg_clear(client, reg_gpmd, shift);
		aw9523b_reg_set(client, reg_gpio_cfg, shift);
		break;
	case GPIO_MODE_OUTPUT:
		aw9523b_reg_set(client, reg_gpmd, shift);
		aw9523b_reg_clear(client, reg_gpio_cfg, shift);
		if (reg_ctl)
			aw9523b_reg_set(client, reg_ctl, 4);
		break;
	case GPIO_MODE_INTPUT:
		aw9523b_reg_set(client, reg_gpmd, shift);
		aw9523b_reg_set(client, reg_gpio_cfg, shift);
		break;
	default:
		break;
	}
}

static void aw9523b_set_blink_brightness(
		struct aw9523b_dev *aw9523b, int pin, aw9106b_brightness_level level)
{
	struct i2c_client *client = aw9523b->client;
	unsigned char reg;

	reg = aw9523b_get_pin_reg_address(pin);
	aw9523b_i2c_write(client, reg, level);
}

static void aw9523b_set_blink_maxbrightness(struct aw9523b_dev *aw9523b, aw9523b_isel_level level)
{
	struct i2c_client *client = aw9523b->client;
	unsigned char value;

	value = aw9523b_i2c_read(client, OVERALL_CTL);
	value &= ~(ISEL_MASK << ISEL);
	value |= level;
	aw9523b_i2c_write(client, OVERALL_CTL, level);
}

static void aw9523b_blink_on(struct aw9523b_dev *aw9523b, int pin)
{
	struct i2c_client *client = aw9523b->client;

	aw9523b_reg_set(client, OVERALL_CTL, OBERALL_ISEL_VAL);
}

static void aw9523b_gpio_out(struct aw9523b_dev *aw9523b, int pin, int level)
{
	struct i2c_client *client = aw9523b->client;
	int group = aw9523b_get_pin_group(pin);
	int shift = aw9523b_get_pin_shift(pin);
	unsigned char reg;

	if (group == GROUP_P0) {
		reg = GPIO_OUTPUT_P0;
	} else if (group == GROUP_P1) {
		reg = GPIO_OUTPUT_P1;
	} else {
		return;
	}
	if (level)
		aw9523b_reg_set(client, reg, shift);
	else
		aw9523b_reg_clear(client, reg, shift);
}

static int aw9523b_gpio_in(struct aw9523b_dev *aw9523b, int pin)
{
	struct i2c_client *client = aw9523b->client;
	int group = aw9523b_get_pin_group(pin);
	int shift = aw9523b_get_pin_shift(group);
	unsigned char reg, value;

	if (group == GROUP_P0) {
		reg = GPIO_INPUT_P0;
	} else if (group == GROUP_P1) {
		reg = GPIO_INPUT_P1;
	} else {
		return group;
	}
	value = aw9523b_i2c_read(client, reg);
	value &= (1 << shift);
	return (value >> shift);
}

static void aw9523b_gpio_intn(struct aw9523b_dev *aw9523b, int pin, int int_en)
{
	struct i2c_client *client = aw9523b->client;
	int group = aw9523b_get_pin_group(pin);
	int shift = aw9523b_get_pin_shift(group);
	unsigned char reg;

	if (group == GROUP_P0) {
		reg = GPIO_INTN_P0;
	} else if (group == GROUP_P1) {
		reg = GPIO_INTN_P1;
	} else {
		return;
	}
	if (int_en)
		aw9523b_reg_clear(client, reg, shift);
	else
		aw9523b_reg_set(client, reg, shift);
}

static int aw9523b_sw_reset(struct aw9523b_dev *aw9523b)
{
	struct i2c_client *client = aw9523b->client;
	int ret = 0;

	return 0;
	ret = aw9523b_i2c_write(client, AW9523_SW_RESET_REG, 0);
	if (ret < 0) {
		dev_info(aw9523b->dev, "error:aw9523b_sw_reset failed, ret=%d\n", ret);
	}
	return ret;
}

static void aw9523b_hw_reset(int gpio_num)
{
	gpio_direction_output(gpio_num, 1);
	udelay(200);
	gpio_set_value(gpio_num, 0);
	udelay(200);
	gpio_set_value(gpio_num, 1);
	udelay(30);
}

static void aw9523b_reg_print(struct aw9523b_dev *aw9523b)
{
	struct i2c_client *client = aw9523b->client;
	int i;

	for (i = GPIO_INPUT_P0; i <= AW9523_MAX_DIM_REG; i++)
		dev_dbg(aw9523b->dev, "reg 0x%0X = 0x%X\n", i, aw9523b_i2c_read(client, i));
}

static int aw9523b_setup(struct aw9523b_dev *aw9523b)
{
	int i;
	int ret = 0;

	ret = aw9523b_sw_reset(aw9523b);
	if (ret) {
		return ret;
	}
	aw9523b_set_blink_maxbrightness(aw9523b, ISEL_18MA);
	for (i = 0; i < AW9523_PIN_MAX; i++) {
		/* aw9523b_set_mode(aw9523b, i, GPIO_MODE_OUTPUT);*/
	}
	aw9523b_reg_print(aw9523b);
	return ret;
}

static int aw9523_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int aw9523_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations aw9523b_fops = {
	.owner		= THIS_MODULE,
	.open			= aw9523_open,
	.release		=  aw9523_close,
};

static int aw9523b_init_client(struct i2c_client *client)
{
	aw9523b_client = client;
	return 0;
}

static struct aw9523b_dev *aw9523b_get_device(int id)
{
	struct aw9523b_dev *aw9523b;
	int status = 0;

	mutex_lock(&device_list_lock);
	list_for_each_entry(aw9523b, &device_list, device_entry) {
		if (aw9523b->id == id) {
			status = 1;
			break;
		}
	}
	mutex_unlock(&device_list_lock);

	if (status)
		return aw9523b;
	else
		return NULL;
}

int aw9523b_config(int id, aw9523b_pin pin, struct aw9106b_config_data *config)
{
	struct aw9523b_dev *aw9523b = aw9523b_get_device(id);
	aw9106b_mode mode = (aw9106b_mode)config->mode;

	if (aw9523b == NULL)
		return 1;

	aw9523b_set_mode(aw9523b, pin, mode);
	if (config->mode == LED_BLINK) {
		if (config->blink) {
			if (config->delay_on) {
				aw9523b_blink_on(aw9523b, pin);
			} else {
				/*aw9523b_blink_off(aw9523b, pin);*/
				aw9523b_set_blink_brightness(aw9523b, pin, 0);
			}
		} else {
			/*aw9523b_blink_off(aw9523b, pin);*/
			aw9523b_set_blink_brightness(aw9523b, pin, config->level);
		}
	} else if (config->mode == GPIO_MODE_OUTPUT) {
		aw9523b_gpio_out(aw9523b, pin, config->level);
	} else if (config->mode == GPIO_MODE_INTPUT) {
		aw9523b_gpio_intn(aw9523b, pin, 1); /* enable the irq */
		aw9523b_gpio_in(aw9523b, pin);
	}
	return 0;
}
EXPORT_SYMBOL(aw9523b_config);

static struct class *aw9523b_class;
static int aw9523b_major = AW9523B_MAJOR;
static int aw9523b_devices_id = -1;

static int aw9523b_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct aw9523b_platform_data *pdata;
	struct aw9523b_dev *aw9523b;
	struct device_node *np;
	int ret = 0;

	np = client->dev.of_node;
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		dev_err(&client->dev, "aw9523b_probe kzalloc failed\n");
		return ret;
	}
	dev_set_drvdata(&client->dev, pdata);
	pdata->shdn_gpio = of_get_named_gpio(np, "shdn_gpio", 0);
	if (pdata->shdn_gpio) {
		ret = gpio_request(pdata->shdn_gpio, "aw9523b reset");
		if (ret) {
			dev_err(&client->dev, "aw9523 request rst gpio failed\n");
		}
		aw9523b_hw_reset(pdata->shdn_gpio);
	} else {
		dev_err(&client->dev, "no shdn_gpio found: %d\n", pdata->shdn_gpio);
	}

	if (pdata->irq_gpio) {
		ret = gpio_request(pdata->irq_gpio, "aw9523b irq");
		if (ret) {
			dev_err(&client->dev, "aw9523 request irq gpio failed\n");
			ret = -EINVAL;
		}
		dev_dbg(&client->dev, "aw9523b irq gpio %d\n", pdata->irq_gpio);
	}

	aw9523b = kmalloc(sizeof(struct aw9523b_dev), GFP_KERNEL);
	if (!aw9523b) {
		dev_err(&client->dev, "aw9523 failed to alloc memory\n");
		ret = -ENOMEM;
		goto err_mem;
	}
	i2c_set_clientdata(client, aw9523b);
	aw9523b_init_client(client);
	INIT_LIST_HEAD(&aw9523b->device_entry);

	if (++aw9523b_devices_id < 0) {
		dev_err(&client->dev, "aw9523b id error\n");
		ret = -EINVAL;
		goto err_device;
	}
	aw9523b->id = aw9523b_devices_id;
	aw9523b->client = client;
	aw9523b->dev = device_create(aw9523b_class, &client->dev, MKDEV(aw9523b_major, aw9523b->id),
					aw9523b, AW9523B_NAME"-%d", aw9523b->id);
	if (IS_ERR(aw9523b->dev)) {
		ret = PTR_ERR(aw9523b->dev);
		goto err_device;
	}

	ret = aw9523b_setup(aw9523b);
	if (ret) {
		pr_err("failed to setup aw9523b,  error %d\n", ret);
		goto err_setup;
	}

	mutex_lock(&device_list_lock);
	list_add(&aw9523b->device_entry, &device_list);
	mutex_unlock(&device_list_lock);
	pr_info("create %s-%d\n", AW9523B_NAME, aw9523b->id);
	return ret;

err_setup:
	device_destroy(aw9523b_class, MKDEV(aw9523b_major, aw9523b->id));
err_device:
	kfree(aw9523b);
err_mem:
	kfree(aw9523b);

	return ret;
}

static int  aw9523b_remove(struct i2c_client *client)
{
	const struct aw9523b_platform_data *pdata = client->dev.platform_data;
	struct aw9523b_dev *aw9523b = i2c_get_clientdata(client);

	device_destroy(aw9523b_class, MKDEV(aw9523b_major, aw9523b->id));
	kfree(aw9523b);
	if (pdata->irq_gpio)
		gpio_free(pdata->irq_gpio);
	if (pdata->shdn_gpio)
		gpio_free(pdata->shdn_gpio);
	return 0;
}

static int aw9523b_detect(struct i2c_client *client,
			struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_info("aw9523b i2c function check failed\n");
		return -ENODEV;
	}
	return 0;
}

static const struct i2c_device_id aw9523b_id[] = {
	{"aw9523b", 0},
	{},
};

static struct i2c_driver aw9523b_driver = {
	.driver = {
				.name = "aw9523b",
				.owner = THIS_MODULE,
				.of_match_table = aw9523b_match_table,
				},
	.probe = aw9523b_probe,
	.remove = aw9523b_remove,
	.detect = aw9523b_detect,
	.id_table = aw9523b_id,
};

static int __init aw9523b_init(void)
{
	int ret = 0;

	aw9523b_major = register_chrdev(aw9523b_major, AW9523B_NAME, &aw9523b_fops);
	if (aw9523b_major < 0) {
		ret = aw9523b_major;
		pr_err("can not register aw9523b chrdev, err = %d\n", aw9523b_major);
		goto err_register;
	}

	aw9523b_class = class_create(THIS_MODULE, AW9523B_NAME);
	if (IS_ERR(aw9523b_class)) {
		ret = PTR_ERR(aw9523b_class);
		pr_err("register aw9523b class failed: %d\n", ret);
		goto err_class;
	}

	ret = i2c_add_driver(&aw9523b_driver);
	if (ret < 0) {
		pr_err("add aw9523b i2c driver failed\n");
		goto err_driver;
	}
	pr_info("add aw9523b i2c driver\n");
	return ret;

err_driver:
	class_destroy(aw9523b_class);
err_class:
	unregister_chrdev(aw9523b_major, AW9523B_NAME);
err_register:
	return ret;
}

static void __exit aw9523b_exit(void)
{
	i2c_del_driver(&aw9523b_driver);
	class_destroy(aw9523b_class);
	unregister_chrdev(aw9523b_major, AW9523B_NAME);
	pr_info("remove aw9523 i2c driver\n");
}

module_init(aw9523b_init);
module_exit(aw9523b_exit);

MODULE_AUTHOR("MSF ZTE, Inc.");
MODULE_DESCRIPTION("aw9523b led backlight  driver");
MODULE_LICENSE("GPL");
