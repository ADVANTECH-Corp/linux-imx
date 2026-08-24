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

        mdelay(timing_interval);
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
    struct gpio_descs *reset_gpios;
    int reset_delay = 50;

    int i;

np = dev->of_node;

if (of_property_read_u32(np, "timing-interval", &timing_interval))
    timing_interval = 50;

// en-gpios
struct gpio_descs *gpios = gpiod_get_array(dev, "en", GPIOD_OUT_HIGH);
if (!IS_ERR(gpios))
{
for (i = 0; i < gpios->ndescs; i++)
gpiod_set_value(gpios->desc[i], 1);
gpiod_put_array(gpios);
}

    reset_gpios = gpiod_get_array_optional(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(reset_gpios)) {
        return dev_err_probe(dev, PTR_ERR(reset_gpios), "Failed to get reset-gpios\n");
    }

    if (reset_gpios) {
        if (of_property_read_u32(np, "reset-delay", &reset_delay))
            reset_delay = 50;

        if (reset_delay)
            msleep(reset_delay);

        for (i = 0; i < reset_gpios->ndescs; i++) {
            gpiod_set_value(reset_gpios->desc[i], 0);
        }

        /* 5. 释放 GPIO 资源，对应原代码的 gpio_free() */
        gpiod_put_array(reset_gpios);
    }


// minipcie reset pin
minipcie_reset_gpio = of_get_named_gpio(np, "minipcie-reset-gpio", 0);
if (minipcie_reset_gpio < 0) {
    pr_err("Failed to get minipcie-reset-gpio\n");
    return minipcie_reset_gpio;
}

const void *gpio_flags = of_get_property(np, "gpio_flags", NULL);
minipcie_reset_active = 0;

if (gpio_flags) {
    minipcie_reset_active = (*(unsigned int *)gpio_flags & GPIOF_ACTIVE_LOW) != 0;
}


if (gpio_is_valid(minipcie_reset_gpio)) {
    if (minipcie_reset_active) {
        gpio_request_one(minipcie_reset_gpio, GPIOF_OUT_INIT_HIGH, "minipcie 4g reset gpio");
    } else {
        gpio_request_one(minipcie_reset_gpio, GPIOF_OUT_INIT_LOW, "minipcie 4g reset gpio");
    }
}

if (timing_interval)
    mdelay(timing_interval);

if (gpio_is_valid(minipcie_reset_gpio))
{
    gpio_direction_output(minipcie_reset_gpio, !minipcie_reset_active);
    gpio_free(minipcie_reset_gpio);
    device_create_file(dev, &dev_attr_minipcie_reset);
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

fs_initcall(misc_adv_gpio_init);
module_exit(misc_adv_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("misc gpio driver for advantech");
