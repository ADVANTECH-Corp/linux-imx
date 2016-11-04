#ifdef CONFIG_ARCH_ADVANTECH
/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup Framebuffer Framebuffer Driver for SDC and ADC.
 */

/*!
 * @file mxcfb_epson_vga.c
 *
 * @brief MXC Frame buffer driver for SDC
 *
 * @ingroup Framebuffer
 */

/*!
 * Include files
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/mxcfb.h>
#include <linux/ipu.h>
#include <linux/fsl_devices.h>
//#include <hardware.h>

static struct i2c_client *ch7055_client;

static ssize_t ch7055_show_brightness(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{	
	unsigned long val;
	val = i2c_smbus_read_byte_data(ch7055_client, 0x04);
	
	return sprintf(buf, "%lu\n", val);
}

static ssize_t ch7055_store_brightness(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	if ((kstrtoul(buf, 10, &val) < 0) ||
	    (val > 127))
		return -EINVAL;
  
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x04, val);
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x05, val);
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x06, val);

	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(brightness, S_IWUSR | S_IRUGO,
		   ch7055_show_brightness, ch7055_store_brightness);

static struct attribute *ch7055_attributes[] = {
	&dev_attr_brightness.attr,
	NULL
};

static const struct attribute_group ch7055_attr_group = {
	.attrs = ch7055_attributes,
};

static int ch7055_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int err, ret;
	ch7055_client = client;	
	
	/* setup default brightness */
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x04, 0x40);
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x05, 0x40);
	ret = i2c_smbus_write_byte_data(ch7055_client, 0x06, 0x40);
	if (ret < 0)
		return ret;
	
	/* register sys hook */
	err = sysfs_create_group(&client->dev.kobj, &ch7055_attr_group);
	if (err)
		goto exit_kfree;
	
	exit_kfree:		
	return 0;
}

static int ch7055_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ch7055_id[] = {
	{"ch7055", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ch7055_id);

static struct i2c_driver ch7055_driver = {
	.driver = {
		   .name = "ch7055",
		   },
	.probe = ch7055_probe,
	.remove = ch7055_remove,
	.id_table = ch7055_id,
};

static int __init ch7055_init(void)
{
	printk("ch7055_init\n");	
	return i2c_add_driver(&ch7055_driver);
}

static void __exit ch7055_exit(void)
{
	i2c_del_driver(&ch7055_driver);
}

module_init(ch7055_init);
module_exit(ch7055_exit);

MODULE_DESCRIPTION("CH7055 VGA driver");
MODULE_LICENSE("GPL");

#endif
