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

struct misc_adv_gpio {
	struct gpio_descs *sleep_gpios;
    struct gpio_desc *minipcie_reset_gpio;
	int timing_interval;
};

static int pmic_match(struct device *dev, const void *data)
{
    return strncmp(dev_name(dev), "bd71847-pmic", 12) == 0;
}
static struct device *misc_find_pmic_device(void)
{
    return bus_find_device(&platform_bus_type, NULL, NULL, pmic_match);
}

static ssize_t minipcie_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct misc_adv_gpio *priv = dev_get_drvdata(dev);

    if (!IS_ERR(priv->minipcie_reset_gpio) && priv->minipcie_reset_gpio)
    {
        gpiod_set_value_cansleep(priv->minipcie_reset_gpio, 1);

        msleep(priv->timing_interval);

        gpiod_set_value_cansleep(priv->minipcie_reset_gpio, 0);
    }

    return count;
}
static DEVICE_ATTR(minipcie_reset, S_IWUSR|S_IWGRP, NULL, minipcie_reset_store);

static int misc_adv_gpio_probe(struct platform_device *pdev)
{
    struct device *pmic_pdev;
    struct device_link *link;
	struct device *dev = &pdev->dev;
    struct device_node *np;
    struct gpio_descs *reset_gpios, *en_gpios, *in_gpios, *dual_gpios;
    struct misc_adv_gpio *priv;
    int i;

    priv = devm_kzalloc(dev, sizeof(struct misc_adv_gpio), GFP_KERNEL);
    if (!priv)
		return -ENOMEM;
    
    np = dev->of_node;

    if (of_property_read_u32(np, "timing-interval", &priv->timing_interval))
        priv->timing_interval = 50;

    // input-gpios
    in_gpios = devm_gpiod_get_array_optional(dev, "input", GPIOD_IN);
    if (!IS_ERR(in_gpios) && in_gpios)
    {
        for (i = 0; i < in_gpios->ndescs; i++)
            gpiod_export(in_gpios->desc[i], false);
    }

    // dual-gpios,default input
    dual_gpios = devm_gpiod_get_array_optional(dev, "dual", GPIOD_IN);
    if (!IS_ERR(dual_gpios) && dual_gpios)
    {
        for (i = 0; i < dual_gpios->ndescs; i++)
            gpiod_export(dual_gpios->desc[i], true);
    }

    // en-gpios
    en_gpios = devm_gpiod_get_array_optional(dev, "en", GPIOD_ASIS);
    if (!IS_ERR(en_gpios) && en_gpios)
    {
        for (i = 0; i < en_gpios->ndescs; i++)
            gpiod_direction_output(en_gpios->desc[i], 1);
    }

    priv->sleep_gpios = devm_gpiod_get_array_optional(dev, "sleep", GPIOD_ASIS);
    if (!IS_ERR(priv->sleep_gpios) && priv->sleep_gpios)
    {   
        for (i = 0; i < priv->sleep_gpios->ndescs; i++) {
            gpiod_direction_output(priv->sleep_gpios->desc[i], 1);
        }
    }

    priv->minipcie_reset_gpio = devm_gpiod_get_optional(dev, "minipcie-reset", GPIOD_OUT_HIGH);

    // reset-gpios
    reset_gpios = devm_gpiod_get_array_optional(dev, "reset", GPIOD_ASIS);
    if (!IS_ERR(reset_gpios) && reset_gpios)
    {
        for (i = 0; i < reset_gpios->ndescs; i++)
            gpiod_direction_output(reset_gpios->desc[i], 1);

        msleep(priv->timing_interval);

        for (i = 0; i < reset_gpios->ndescs; i++)
            gpiod_direction_output(reset_gpios->desc[i], 0);
    }

    if (!IS_ERR(priv->minipcie_reset_gpio) && priv->minipcie_reset_gpio)
    {
        if (IS_ERR(reset_gpios) || !reset_gpios)
            msleep(priv->timing_interval);
  
        gpiod_set_value_cansleep(priv->minipcie_reset_gpio, 0);
        device_create_file(dev, &dev_attr_minipcie_reset);
    }
    platform_set_drvdata(pdev, priv);

    pmic_pdev = misc_find_pmic_device();
    if (!pmic_pdev) {
        dev_info(&pdev->dev, "bd71847-pmic not ready, deferring probe\n");
        return -EPROBE_DEFER;
    }

    link = device_link_add(&pdev->dev, pmic_pdev, DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
    put_device(pmic_pdev);
    if (!link) {
        dev_err(&pdev->dev, "Failed to create device link\n");
        return -EINVAL;
    }

	return 0;
}

static int adv_gpio_suspend_resume(struct device *dev, int mode)
{
    struct misc_adv_gpio *priv = dev_get_drvdata(dev);
    int i;

    if (!IS_ERR(priv->sleep_gpios) && priv->sleep_gpios)
    {
        for (i = 0; i < priv->sleep_gpios->ndescs; i++) {
            if(mode)
                gpiod_direction_output(priv->sleep_gpios->desc[i], 1);
            else
                gpiod_direction_output(priv->sleep_gpios->desc[priv->sleep_gpios->ndescs-1-i], 0);
        }
    }
	return 0;
}

static int misc_adv_gpio_suspend(struct device *dev)
{
    struct misc_adv_gpio *priv = dev_get_drvdata(dev);

    if (!IS_ERR(priv->minipcie_reset_gpio) && priv->minipcie_reset_gpio)
        gpiod_set_value_cansleep(priv->minipcie_reset_gpio, 1);
    adv_gpio_suspend_resume(dev, 0);
    return 0;
}

static int misc_adv_gpio_resume(struct device *dev)
{
    struct misc_adv_gpio *priv = dev_get_drvdata(dev);

    adv_gpio_suspend_resume(dev, 1);
    if (!IS_ERR(priv->minipcie_reset_gpio) && priv->minipcie_reset_gpio)
        gpiod_set_value_cansleep(priv->minipcie_reset_gpio, 0);

    return 0;
}

static const struct dev_pm_ops misc_adv_gpio_pm_ops = {
    //.suspend_late = misc_adv_gpio_suspend,
    //.resume_early = misc_adv_gpio_resume,
    SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(misc_adv_gpio_suspend, misc_adv_gpio_resume)
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

subsys_initcall_sync(misc_adv_gpio_init);
module_exit(misc_adv_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("misc gpio driver for advantech");
