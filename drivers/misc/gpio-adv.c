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
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>

#define MISC_ADV_GPIO_MODNAME		"misc-adv-gpio"

static struct gpio_descs *sleep_gpios;

static int misc_adv_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
    struct device_node *np;
    struct gpio_descs *reset_gpios, *en_gpios;
    int timing_interval = 50;
    int i;

    np = dev->of_node;

    if (of_property_read_u32(np, "timing-interval", &timing_interval))
        timing_interval = 50;

    // en-gpios
    en_gpios = devm_gpiod_get_array_optional(dev, "en", GPIOD_ASIS);
    if (!IS_ERR(en_gpios) && en_gpios)
    {
        for (i = 0; i < en_gpios->ndescs; i++)
            gpiod_direction_output(en_gpios->desc[i], 1);
    }

    // reset-gpios
    reset_gpios = devm_gpiod_get_array_optional(dev, "reset", GPIOD_ASIS);
    if (!IS_ERR(reset_gpios) && reset_gpios)
    {
        for (i = 0; i < reset_gpios->ndescs; i++)
            gpiod_direction_output(reset_gpios->desc[i], 1);

        msleep(timing_interval);

        for (i = 0; i < reset_gpios->ndescs; i++)
            gpiod_direction_output(reset_gpios->desc[i], 0);
    }

    sleep_gpios = devm_gpiod_get_array_optional(dev, "sleep", GPIOD_ASIS);
    if (!IS_ERR(sleep_gpios) && sleep_gpios)
    {   
        for (i = 0; i < sleep_gpios->ndescs; i++) {
            gpiod_direction_output(sleep_gpios->desc[i], 1);
        }
    }

	return 0;
}

static int adv_gpio_suspend_resume(struct device *dev, int mode)
{
    int i;

    if (!IS_ERR(sleep_gpios) && sleep_gpios)
    {
        for (i = 0; i < sleep_gpios->ndescs; i++) {
            if(mode)
                gpiod_direction_output(sleep_gpios->desc[i], 1);
            else
                gpiod_direction_output(sleep_gpios->desc[i], 0);
        }
    }
	return 0;
}

static int misc_adv_gpio_suspend(struct device *dev)
{
    adv_gpio_suspend_resume(dev, 0);
    return 0;
}

static int misc_adv_gpio_resume(struct device *dev)
{
    adv_gpio_suspend_resume(dev, 1);

    return 0;
}

static const struct dev_pm_ops misc_adv_gpio_pm_ops = {
    .suspend_late = misc_adv_gpio_suspend,
    .resume_early = misc_adv_gpio_resume,
};

static const struct of_device_id misc_adv_gpio_of_match[] = {
	{ .compatible = MISC_ADV_GPIO_MODNAME, },
	{ },
};
MODULE_DEVICE_TABLE(of, misc_adv_gpio_of_match);

static struct platform_driver misc_adv_gpio_driver = {
	.probe		= misc_adv_gpio_probe,
	.driver		= {
		.name	= MISC_ADV_GPIO_MODNAME,
		.of_match_table	=  misc_adv_gpio_of_match,
        .pm = &misc_adv_gpio_pm_ops,
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

subsys_initcall(misc_adv_gpio_init);
module_exit(misc_adv_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("misc gpio driver for advantech");
