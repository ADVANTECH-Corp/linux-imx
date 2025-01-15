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

static int handle_gpio_request_and_set_output(int gpio, bool active, const char *label)
{
    int err;
    
    err = gpio_request(gpio, label);
    if (err)
        return err;

    if (active)
        gpio_direction_output(gpio, GPIOF_OUT_INIT_HIGH);
    else
        gpio_direction_output(gpio, GPIOF_OUT_INIT_LOW);

    return 0;
}

static int misc_adv_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
    struct device_node *np;
    // enum of_gpio_flags flags;
    // int  minipcie_pwr_gpio;
	// int  lan2_reset_gpio;
	// bool  minipcie_pwr_active;
	// bool  lan2_reset_active;

    int  num_reset_gpios=0;
    int num_input_gpios, num_output_gpios;
    int i, gpio, err,en_gpios,ret,gpio_count,num_gpio_elements;
    bool active;
    int  reset_delay = 50;

u32  flags;
const __be32 *prop;
np = dev->of_node;
// 获取 GPIO 数量
prop = of_get_property(np, "input-gpios", NULL);
if (prop)
    num_input_gpios = of_property_count_elems_of_size(np, "input-gpios", sizeof(*prop) / sizeof(u32));

prop = of_get_property(np, "output-gpios", NULL);
if (prop)
    num_output_gpios = of_property_count_elems_of_size(np, "output-gpios", sizeof(*prop) / sizeof(u32));


// 读取设备树中的其他属性
if (of_property_read_u32(np, "reset-delay", &reset_delay))
    reset_delay = 50;

if (of_property_read_u32(np, "timing-interval", &timing_interval))
    timing_interval = 50;

// DIO 输入 GPIO 配置
for (i = 0; i < num_input_gpios; i++) {
    gpio = of_get_named_gpio(np, "input-gpios", i);
    if (gpio_is_valid(gpio)) {
        err = gpio_request(gpio, "uio_input_gpio");
        if (!err) {
            gpio_direction_input(gpio);
            // gpiod_export(gpio, false);
        }
    }
}

// DIO 输出 GPIO 配置
for (i = 0; i < num_output_gpios; i++) {
    gpio = of_get_named_gpio(np, "output-gpios", i);
    if (gpio_is_valid(gpio)) {
        err = gpio_request(gpio, "uio_output_gpio");
        if (!err) {
            // 通过设备树中的 active-low 属性判断 GPIO 是否是低电平有效
            active = !of_property_read_bool(np, "output-gpios-active-low");
            if (active)
                gpio_direction_output(gpio, GPIOF_OUT_INIT_HIGH);
            else
                gpio_direction_output(gpio, GPIOF_OUT_INIT_LOW);
            // gpiod_export(gpio, false);
        }
    }
}

// en-gpios
/* */
num_gpio_elements = of_property_count_u32_elems(np, "en-gpios");
if (num_gpio_elements < 0) {
        printk("[adv ] File=%s, Func=%s, Line=%d, Failed to count en-gpio1 elements \n",__FILE__,__FUNCTION__,__LINE__);
}

gpio_count = num_gpio_elements / 3;
if (num_gpio_elements % 3 != 0) {
        printk("[adv ] File=%s, Func=%s, Line=%d, Invalid en-gpios format: not divisible by 3 \n",__FILE__,__FUNCTION__,__LINE__);
}

for (i = 0; i < gpio_count; i++)
{
	en_gpios = of_get_named_gpio(np, "en-gpios", i);
	if (gpio_is_valid(en_gpios)) {
		ret = devm_gpio_request_one(&pdev->dev,
					    en_gpios,
					    GPIOF_OUT_INIT_HIGH,
					    "en-gpios");
	}
}

// reset-gpios
for (i = 0; i < num_reset_gpios; i++)
{
    gpio = of_get_named_gpio(np, "reset-gpios", i);  // 获取 GPIO 编号
    if (gpio_is_valid(gpio))
    {
        if (!of_property_read_u32(np, "reset-gpios-flags", &flags)) {
            active = !(flags & GPIOF_ACTIVE_LOW);  // 判断是否是活动低电平
        } else {
            active = true; // 默认活动为高电平
        }

        err = handle_gpio_request_and_set_output(gpio, active, "adv_reset_gpios");
    }
}

if (reset_delay)
    mdelay(reset_delay);

for (i = 0; i < num_reset_gpios; i++)
{
    gpio = of_get_named_gpio(np, "reset-gpios", i);  // 获取 GPIO 编号
    if (gpio_is_valid(gpio))
    {

        if (!of_property_read_u32(np, "reset-gpios-flags", &flags)) {
            active = !(flags & GPIOF_ACTIVE_LOW);  // 判断是否是活动低电平
        } else {
            active = true; // 默认活动为高电平
        }

        gpio_direction_output(gpio, active ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH);
        gpio_free(gpio);
    }
}

mdelay(10);

// minipcie reset pin
minipcie_reset_gpio = of_get_named_gpio(np, "minipcie-reset-gpio", 0);
if (minipcie_reset_gpio < 0) {
    pr_err("Failed to get minipcie-reset-gpio\n");
    return minipcie_reset_gpio;  // 错误处理，返回错误代码
}

// 获取设备树属性中的 gpio_flags，并检查是否为低电平有效
const void *gpio_flags = of_get_property(np, "gpio_flags", NULL);
minipcie_reset_active = 0;  // 默认标志为 0

if (gpio_flags) {
    // 假设 gpio_flags 存储的是整数值，进行转换和检查
    minipcie_reset_active = (*(unsigned int *)gpio_flags & GPIOF_ACTIVE_LOW) != 0;
}

// 请求 GPIO 引脚
if (gpio_is_valid(minipcie_reset_gpio)) {
    if (minipcie_reset_active) {
        gpio_request_one(minipcie_reset_gpio, GPIOF_OUT_INIT_HIGH, "minipcie 4g reset gpio");
    } else {
        gpio_request_one(minipcie_reset_gpio, GPIOF_OUT_INIT_LOW, "minipcie 4g reset gpio");
    }
}

// lan2_reset_gpio = of_get_named_gpio(np, "lan2-reset-gpio", 0);  // 获取 GPIO 编号
// if (gpio_is_valid(lan2_reset_gpio))
// {

//     if (!of_property_read_u32(np, "lan2-reset-gpio-flags",&flags)) {
//         lan2_reset_active = !(flags & GPIOF_ACTIVE_LOW);
//     } else {
//         lan2_reset_active = true; // 默认活动为高电平
//     }

//     err = handle_gpio_request_and_set_output(lan2_reset_gpio, lan2_reset_active, "lan2 reset gpio");
// }

if (timing_interval)
    mdelay(timing_interval);

if (gpio_is_valid(minipcie_reset_gpio))
{
    gpio_direction_output(minipcie_reset_gpio, !minipcie_reset_active);
    gpio_free(minipcie_reset_gpio);
    device_create_file(dev, &dev_attr_minipcie_reset);
}

// if (gpio_is_valid(lan2_reset_gpio))
// {
//     gpio_direction_output(lan2_reset_gpio, !lan2_reset_active);
//     gpio_free(lan2_reset_gpio);
// }
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
