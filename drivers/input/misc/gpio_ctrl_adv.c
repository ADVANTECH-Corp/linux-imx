#ifdef CONFIG_ARCH_ADVANTECH
/*
 * Driver for GPIO timing control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>


#define GPIO_CTRL_ADV_MODNAME		"gpio-ctrl-adv"

static int gpio_ctrl_adv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
    enum of_gpio_flags flags;

    int  minipcie_pwr_gpio ;
    int  minipcie_reset_gpio;
    int  timing_interval = 0;

	minipcie_pwr_gpio = of_get_named_gpio_flags(
                         dev->of_node, "minipcie-pwr-gpio", 0, &flags);
	if (gpio_is_valid(minipcie_pwr_gpio))
	{
		gpio_request_one(minipcie_pwr_gpio, 
                        GPIOF_OUT_INIT_HIGH, "minipcie pwr gpio");
	}

	minipcie_reset_gpio = of_get_named_gpio_flags(
                        dev->of_node, "minipcie-reset-gpio", 0, &flags);
	if (gpio_is_valid(minipcie_reset_gpio))
	{
		gpio_request_one(minipcie_reset_gpio, 
                        GPIOF_OUT_INIT_HIGH, "minipcie reset gpio");
	}

        struct device_node *np;
        int len=0;
        const __be32 *parp;
        np = of_find_compatible_node(NULL, NULL, "gpio-ctrl-adv");
	if (np) {
		parp = of_get_property(np, "timing-interval", &len);
		if (parp && (len / sizeof (int) == 1)) {
			timing_interval = be32_to_cpu(parp[0]);
		}
	}
    
    gpio_direction_output(minipcie_reset_gpio, 0);
    mdelay(timing_interval);
    gpio_direction_output(minipcie_pwr_gpio, 1);
    mdelay(timing_interval);
    gpio_direction_output(minipcie_reset_gpio, 1);
    
	return 0;
}

static struct of_device_id gpio_ctrl_adv_of_match[] = {
	{ .compatible = GPIO_CTRL_ADV_MODNAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, gpio_ctrl_adv_of_match);


static struct platform_driver gpio_ctrl_adv_device_driver = {
	.probe		= gpio_ctrl_adv_probe,
	.driver		= {
		.name	= GPIO_CTRL_ADV_MODNAME,
		.owner	= THIS_MODULE,
		.of_match_table	= gpio_ctrl_adv_of_match,
	}
};

static int __init gpio_ctrl_adv_init(void)
{
	return platform_driver_register(&gpio_ctrl_adv_device_driver);
}

static void __exit gpio_ctrl_adv_exit(void)
{
	platform_driver_unregister(&gpio_ctrl_adv_device_driver);
}

subsys_initcall(gpio_ctrl_adv_init);
module_exit(gpio_ctrl_adv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("driver for GPIOs");

#endif
