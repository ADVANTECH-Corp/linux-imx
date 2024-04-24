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

#define UART_MODE_MODNAME		"uart-mode"
#define UART_MODE_MAXPORT		4
#define UART_MODE_GPIO_NUM		4

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

enum uart_mode_type {
	RS232_MODE,
	RS485_MODE,
	RS422_MODE,
	MAX_MODE
};

static int g_port_type[UART_MODE_MAXPORT][2] = {{0xFF}, {0xFF}, {0xFF}, {0xFF}};

int adv_get_uart_mode(int index)
{
	int i;
	int mode = 0;

	for(i = 0; i < UART_MODE_MAXPORT; i++)
	{
		if(index == g_port_type[i][0])
		{
			mode = g_port_type[i][1];
			break;
		}
	}

	return mode;
}

static int __init setup_uart_mode(char *buf)
{
	int i=0;
	char *p, *options;

	if (!buf)
		return -EINVAL;

	p = buf;
	options = strchr(p, ':');
	while(p && options && (i<UART_MODE_MAXPORT)) {
		++options;
		g_port_type[i][0] = simple_strtol(p, NULL, 10);
		g_port_type[i][1] = simple_strtol(options, NULL, 10);
		++i;
		p = strchr(options, ',');
		if(p) {
			++p;
			options = strchr(p, ':');
		}
	}
	
	return 0;
}
early_param("uart_mode", setup_uart_mode);

static ssize_t uart_mode_mode_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int i = 0;
	int len = 0;
	for(i = 0; i < UART_MODE_MAXPORT; i++)
	{
		len += sprintf(buf+len, "%d:%d ", g_port_type[i][0],g_port_type[i][1]);
	}
	len += sprintf(buf+len, "\n");
	return len;
}

static DEVICE_ATTR(mode, S_IRUGO|S_IRUSR,uart_mode_mode_show, NULL);

static int uart_mode_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int i = 0;
	int elems;
	u32 *conf;
	int sel0_gpio,sel1_gpio;
	int term_gpio,slew_gpio;
	char *lable;
	u32 rs232_mode_table[UART_MODE_GPIO_NUM]={0};
	u32 rs485_mode_table[UART_MODE_GPIO_NUM]={0};
	u32 rs422_mode_table[UART_MODE_GPIO_NUM]={0};
	int index = -1;
	int mode = -1;

	of_property_read_u32(np, "index", &index);
	of_property_read_u32(np, "default_mode", &mode);

	if((index >= 0) && (mode >= 0) && (mode < MAX_MODE))
	{
		for(i = 0; i < UART_MODE_MAXPORT; i++)
		{
			if(g_port_type[i][0] == index)
				break;
			else if(g_port_type[i][0] == 0xFF)
			{
				g_port_type[i][0] = index;
				g_port_type[i][1] = mode;
				break;
			}
		}
	} else {
		dev_err(dev, "invalid param!\n");
		return -EINVAL;
	}

	for(i = 0; i < UART_MODE_MAXPORT; i++)
	{
		if((index == g_port_type[i][0]) 
			&& (g_port_type[i][0] != 0xFF) 
			&& (g_port_type[i][1] < MAX_MODE))
		{
			if(RS232_MODE == g_port_type[i][1])
				conf = rs232_mode_table;
			else if(RS485_MODE == g_port_type[i][1])
				conf = rs485_mode_table;
			else if(RS422_MODE == g_port_type[i][1])
				conf = rs422_mode_table;

			break;
		}
	}

	elems = of_property_count_u32_elems(np, "rs232_mode_table");
	if (elems > 0)
		of_property_read_u32_array(np, "rs232_mode_table", rs232_mode_table, MIN(UART_MODE_GPIO_NUM,elems)); 
	elems = of_property_count_u32_elems(np, "rs485_mode_table");
	if (elems > 0)
		of_property_read_u32_array(np, "rs485_mode_table", rs485_mode_table, MIN(UART_MODE_GPIO_NUM,elems));
	elems = of_property_count_u32_elems(np, "rs422_mode_table");
	if (elems > 0)
		of_property_read_u32_array(np, "rs422_mode_table", rs422_mode_table, MIN(UART_MODE_GPIO_NUM,elems));

	sel0_gpio = of_get_named_gpio_flags(np, "sel0_gpio", 0, &flags);
	sel1_gpio = of_get_named_gpio_flags(np, "sel1_gpio", 0, &flags);
	term_gpio = of_get_named_gpio_flags(np, "term_gpio", 0, &flags);
	slew_gpio = of_get_named_gpio_flags(np, "slew_gpio", 0, &flags);
	if(gpio_is_valid(sel0_gpio) && gpio_is_valid(sel1_gpio))
	{
		lable = devm_kasprintf(dev, GFP_KERNEL, "port%d sel0", index);
		if(conf[0] == 0)
			gpio_request_one(sel0_gpio, GPIOF_OUT_INIT_LOW, lable);
		else
			gpio_request_one(sel0_gpio, GPIOF_OUT_INIT_HIGH, lable);
		devm_kfree(dev,lable);

		lable = devm_kasprintf(dev, GFP_KERNEL, "port%d sel1", index);
		if(conf[1] == 0)
			gpio_request_one(sel1_gpio, GPIOF_OUT_INIT_LOW, lable);
		else
			gpio_request_one(sel1_gpio, GPIOF_OUT_INIT_HIGH, lable);
		devm_kfree(dev,lable);

		if(gpio_is_valid(term_gpio))
		{
			lable = devm_kasprintf(dev, GFP_KERNEL, "port%d term", index);
			if(conf[2] == 0)
				gpio_request_one(term_gpio, GPIOF_OUT_INIT_LOW, lable);
			else
				gpio_request_one(term_gpio, GPIOF_OUT_INIT_HIGH, lable);
			devm_kfree(dev,lable);
		}

		if(gpio_is_valid(slew_gpio))
		{
			lable = devm_kasprintf(dev, GFP_KERNEL, "port%d slew", index);
			if(conf[3] == 0)
				gpio_request_one(slew_gpio, GPIOF_OUT_INIT_LOW, lable);
			else
				gpio_request_one(slew_gpio, GPIOF_OUT_INIT_HIGH, lable);
			devm_kfree(dev,lable);
		}
	} else if (sel0_gpio == -EPROBE_DEFER) {
		return sel0_gpio;
	}

	if (device_create_file(dev, &dev_attr_mode))
	{
        	dev_err(dev, "sys file creation failed\n");
        	return -ENODEV;
	}

	return 0;
}

static const struct of_device_id uart_mode_of_match[] = {
	{ .compatible = UART_MODE_MODNAME, },
	{ },
};
MODULE_DEVICE_TABLE(of, uart_mode_of_match);

static struct platform_driver uart_mode_driver = {
	.probe		= uart_mode_probe,
	.driver		= {
		.name	= UART_MODE_MODNAME,
		.owner	= THIS_MODULE,
		.of_match_table	=  of_match_ptr(uart_mode_of_match),
	}
};

static int __init uart_mode_init(void)
{
	return platform_driver_register(&uart_mode_driver);
}

static void __exit uart_mode_exit(void)
{
	platform_driver_unregister(&uart_mode_driver);
}

subsys_initcall(uart_mode_init);
module_exit(uart_mode_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rs232/485/422 mode driver for advantech");
