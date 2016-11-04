#ifdef CONFIG_ARCH_ADVANTECH
/*
 * Advantech Dust WSN module
 *
 * Copyright (C) 2015 Advantech Co.,Ltd.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>

#define IMX_GPIO_NR(bank, nr)		(((bank) - 1) * 32 + (nr))

static int __init advantech_dustwsn_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	dev_info(&pdev->dev, "Advantech Dust WSN module init...\n");
	/* no device tree device */
	if (!np)
	{
		dev_err(&pdev->dev, "no device tree device...\n");
		return -1;
	}

	/* Enable GPIO */
    gpio_request(IMX_GPIO_NR(1,0), "gpio-0");
    gpio_request(IMX_GPIO_NR(1,1), "gpio-1");
    gpio_request(IMX_GPIO_NR(1,2), "gpio-2");
    gpio_request(IMX_GPIO_NR(1,3), "gpio-3");
    gpio_request(IMX_GPIO_NR(1,4), "gpio-4");
    gpio_request(IMX_GPIO_NR(1,5), "gpio-5");
    gpio_request(IMX_GPIO_NR(1,6), "gpio-6");
    gpio_request(IMX_GPIO_NR(1,7), "gpio-7");

    gpio_direction_output(IMX_GPIO_NR(1,0), 0);
    gpio_direction_output(IMX_GPIO_NR(1,1), 0);
    gpio_direction_output(IMX_GPIO_NR(1,2), 0);
    gpio_direction_output(IMX_GPIO_NR(1,3), 0);
    gpio_direction_output(IMX_GPIO_NR(1,4), 0);
    gpio_direction_output(IMX_GPIO_NR(1,5), 0);
    gpio_direction_output(IMX_GPIO_NR(1,6), 0);
    gpio_direction_output(IMX_GPIO_NR(1,7), 0);

    gpio_set_value(IMX_GPIO_NR(1,0), 0);
    gpio_set_value(IMX_GPIO_NR(1,1), 0);
    gpio_set_value(IMX_GPIO_NR(1,2), 0);
    gpio_set_value(IMX_GPIO_NR(1,3), 0);
    gpio_set_value(IMX_GPIO_NR(1,4), 0);
    gpio_set_value(IMX_GPIO_NR(1,5), 0);
    gpio_set_value(IMX_GPIO_NR(1,6), 0);
    gpio_set_value(IMX_GPIO_NR(1,7), 0);

    gpio_set_value(IMX_GPIO_NR(1,1), 1);
    gpio_set_value(IMX_GPIO_NR(1,5), 1);
    mdelay(5);
    gpio_set_value(IMX_GPIO_NR(1,1), 0);
    gpio_set_value(IMX_GPIO_NR(1,5), 0);

	return 0;
}

static int __exit advantech_dustwsn_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove Advantech Dust WSN module...\n");
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id advantech_dustwsn_dt_ids[] = {
	{ .compatible = "adv,dustwsn" },
	{}
};
MODULE_DEVICE_TABLE(of, advantech_dustwsn_dt_ids);
#else
#define advantech_dustwsn_dt_ids NULL
#endif

static struct platform_driver advantech_dustwsn_driver = {
	.driver = {
		.name = "adv-dustwsn",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(advantech_dustwsn_dt_ids),
	},
	.remove = __exit_p(advantech_dustwsn_remove),
};

module_platform_driver_probe(advantech_dustwsn_driver, advantech_dustwsn_probe);

MODULE_AUTHOR("Advantech");
MODULE_DESCRIPTION("Advantech Dust WSN init Module");
MODULE_LICENSE("GPL");

#endif
