#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

static int netcard_powerctrl_gpio = -1;

static const struct of_device_id of_zte_netcard_powerctrl_match[] = {
	{.compatible = "zte,netcard-power-ctrl",},
	{},
};

MODULE_DEVICE_TABLE(of, of_zte_netcard_powerctrl_match);

static struct delayed_work powerctrlwork;
static void zte_powerctrl_work(struct work_struct *work)
{
	gpio_set_value(netcard_powerctrl_gpio, 1);
	pr_info("%s: gpio %d is set to 1\n", __func__, netcard_powerctrl_gpio);
}

static int netcard_powerctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

	pr_info("%s is called\n", __func__);

	if (node) {
		netcard_powerctrl_gpio =
		    of_get_named_gpio(node, "power-gpio", 0);
		if (!gpio_is_valid(netcard_powerctrl_gpio))
			pr_err("netcard_powerctrl_gpio is not available\n");
		ret = gpio_request(netcard_powerctrl_gpio, "netcard-powerctrl");
		if (ret)
			pr_err("gpio request %d failed ret = %d\n",
			       netcard_powerctrl_gpio, ret);

		gpio_direction_output(netcard_powerctrl_gpio, 0);
		gpio_set_value(netcard_powerctrl_gpio, 0);
		pr_info("%s: gpio %d is set to 0\n", __func__, netcard_powerctrl_gpio);
	}
	INIT_DELAYED_WORK(&powerctrlwork, zte_powerctrl_work);
	schedule_delayed_work(&powerctrlwork, HZ);

	return 0;
}

static struct platform_driver netcard_powerctrl_driver = {
	.probe = netcard_powerctrl_probe,
	.driver = {
		   .name = "zte-netcard-power-ctrl",
		   .of_match_table = of_zte_netcard_powerctrl_match,
		   },
};

static int __init netcard_powerctrl_init(void)
{
	return platform_driver_register(&netcard_powerctrl_driver);
}

module_init(netcard_powerctrl_init);
