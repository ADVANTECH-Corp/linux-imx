/*
 * Driver for GPIO timing control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>

#define MISC_ADV_GPIO_MODNAME		"misc-adv-gpio"

static int  minipcie_reset_gpio=-1;
static bool  minipcie_reset_active;
static int  timing_interval = 0;

static ssize_t minipcie_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (gpio_is_valid(minipcie_reset_gpio))
	{
		if(minipcie_reset_active)
			gpio_request_one(minipcie_reset_gpio, 
                        GPIOF_OUT_INIT_HIGH, "minipcie 4g reset gpio");
		else
			gpio_request_one(minipcie_reset_gpio, 
                        GPIOF_OUT_INIT_LOW, "minipcie 4g reset gpio");
	}

	if (gpio_is_valid(minipcie_reset_gpio))
	{
		gpio_direction_output(minipcie_reset_gpio, minipcie_reset_active);
	}

	mdelay(timing_interval);
	if (gpio_is_valid(minipcie_reset_gpio))
	{
		gpio_direction_output(minipcie_reset_gpio, !minipcie_reset_active);
		gpio_free(minipcie_reset_gpio);
	}

    return count;
}
static DEVICE_ATTR(minipcie_reset, S_IWUSR|S_IWGRP, NULL, minipcie_reset_store);

static int misc_adv_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
    struct device_node *np;
    enum of_gpio_flags flags;
    int  minipcie_pwr_gpio;
	int  lan2_reset_gpio;
	bool  minipcie_pwr_active;
	bool  lan2_reset_active;

    np = dev->of_node;
	minipcie_reset_gpio = of_get_named_gpio_flags(np, "minipcie-reset-gpio", 0, &flags);
	if (gpio_is_valid(minipcie_reset_gpio))
	{
		minipcie_reset_active = !(flags & OF_GPIO_ACTIVE_LOW);
		if(minipcie_reset_active)
			gpio_request_one(minipcie_reset_gpio, 
                        GPIOF_OUT_INIT_HIGH, "minipcie 4g reset gpio");
		else
			gpio_request_one(minipcie_reset_gpio, 
                        GPIOF_OUT_INIT_LOW, "minipcie 4g reset gpio");
	}

	lan2_reset_gpio = of_get_named_gpio_flags(np, "lan2-reset-gpio", 0, &flags);
	if (gpio_is_valid(lan2_reset_gpio))
	{
		lan2_reset_active = !(flags & OF_GPIO_ACTIVE_LOW);
		if(lan2_reset_active)
			gpio_request_one(lan2_reset_gpio, 
						GPIOF_OUT_INIT_HIGH, "lan2 reset gpio");
		else
			gpio_request_one(lan2_reset_gpio, 
						GPIOF_OUT_INIT_LOW, "lan2 reset gpio");
	}

	minipcie_pwr_gpio = of_get_named_gpio_flags(np, "minipcie-pwr-gpio", 0, &flags);
	if (gpio_is_valid(minipcie_pwr_gpio))
	{
		minipcie_pwr_active = !(flags & OF_GPIO_ACTIVE_LOW);
		if(minipcie_pwr_active)
			gpio_request_one(minipcie_pwr_gpio, 
                        GPIOF_OUT_INIT_HIGH, "minipcie pwr gpio");
		else
			gpio_request_one(minipcie_pwr_gpio, 
                        GPIOF_OUT_INIT_LOW, "minipcie pwr gpio");

		gpio_free(minipcie_pwr_gpio);
	}

	if (of_property_read_u32(np,"timing-interval",&timing_interval))
		timing_interval = 50;
	if(timing_interval)
		mdelay(timing_interval);

	if (gpio_is_valid(minipcie_reset_gpio))
	{
		gpio_direction_output(minipcie_reset_gpio, !minipcie_reset_active);
		gpio_free(minipcie_reset_gpio);
	}

	if (gpio_is_valid(lan2_reset_gpio))
	{
		gpio_direction_output(lan2_reset_gpio, !lan2_reset_active);
		gpio_free(lan2_reset_gpio);
	}

	if (device_create_file(dev, &dev_attr_minipcie_reset))
	{
        dev_err(dev, "sys file creation failed\n");
        return -ENODEV;
	}

	return 0;
}

static const struct of_device_id misc_adv_gpio_of_match[] = {
	{ .compatible = MISC_ADV_GPIO_MODNAME, },
	{ },
};
MODULE_DEVICE_TABLE(of, misc_adv_gpio_of_match);

static struct platform_driver misc_adv_gpio_driver = {
	.probe		= misc_adv_gpio_probe,
	.driver		= {
		.name	= MISC_ADV_GPIO_MODNAME,
		.of_match_table	=  of_match_ptr(misc_adv_gpio_of_match),
	}
};

static int __init misc_adv_gpio_init(void)
{
	return platform_driver_register(&misc_adv_gpio_driver);
}

static void __exit misc_adv_gpio_exit(void)
{
	platform_driver_unregister(&misc_adv_gpio_driver);
}

late_initcall(misc_adv_gpio_init);
module_exit(misc_adv_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("misc gpio driver for advantech");
