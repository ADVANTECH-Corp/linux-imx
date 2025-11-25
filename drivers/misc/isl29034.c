// SPDX-License-Identifier: GPL-2.0-only
/*
 * isl29034.c - Intersil  ALS Driver
 *
 * Copyright (C) 2008 Intel Corp
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Data sheet at: http://www.intersil.com/data/fn/fn6505.pdf
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/backlight.h>

struct isl29034_info {
	struct i2c_client       *client;
	struct delayed_work     light_work;
	struct backlight_device *bd;
	u32 min_brightness;
	u32 max_brightness;
};

static void isl29034_light_work(struct work_struct *work)
{
    struct isl29034_info *isl29034 =
            container_of(work, struct isl29034_info, light_work.work);
	u32 retval,retvalh,retvall;
	u32 brightness,old,gap;
	unsigned long timeout = 2*HZ;
	int i;

	retvalh = i2c_smbus_read_byte_data(isl29034->client, 0x03);
	if (retvalh < 0) {
		dev_err(&isl29034->client->dev, "read 0x03 failed.");
		goto retry;
	}
	retvall = i2c_smbus_read_byte_data(isl29034->client, 0x02);
	if (retvall < 0) {
		dev_err(&isl29034->client->dev, "read 0x02 failed.");
		goto retry;
	}
	retval = (retvalh<<8)|retvall;
	brightness = (u32)((retval*(isl29034->max_brightness-isl29034->min_brightness))>>16)+isl29034->min_brightness;
	old = isl29034->bd->props.brightness;
	gap = (brightness > old) ? (brightness - old) : (old - brightness);

	if(gap > 3) {
		for(i=1; i<=gap; i++){
			if(brightness > old)
				backlight_device_set_brightness(isl29034->bd, old+i);
			else
				backlight_device_set_brightness(isl29034->bd, old-i);
			msleep(500);
			timeout -= HZ/2;
			if(timeout==0)
				break;
		}
	}

retry:
	schedule_delayed_work(&isl29034->light_work, timeout);
}

static int als_set_default_config(struct i2c_client *client)
{
	int retval;

	retval = i2c_smbus_read_byte_data(client, 0x0f);
	if (retval < 0) {
		dev_err(&client->dev, "default read failed.");
		return retval;
	}
	retval &= ~0x80;

	retval = i2c_smbus_write_byte_data(client, 0x0f, retval);
	if (retval < 0) {
		dev_err(&client->dev, "default write failed.");
		return retval;
	}
	i2c_smbus_write_byte_data(client, 0x00, 0xa0);
	i2c_smbus_write_byte_data(client, 0x01, 0x00);

	retval = i2c_smbus_read_byte_data(client, 0x0f);
	retval |= 0x80;
	i2c_smbus_write_byte_data(client, 0x0f, retval);

	return 0;
}

static int  isl29034_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int res;
	struct isl29034_info *isl29034;

	isl29034 = devm_kzalloc(&client->dev, sizeof(struct isl29034_info),GFP_KERNEL);
	if (!isl29034)
		return -ENOMEM;

	isl29034->client = client;
	i2c_set_clientdata(client, isl29034);
	dev_set_drvdata(&client->dev, isl29034);

	res = als_set_default_config(client);
	if (res <  0)
		return res;

	isl29034->bd = backlight_device_get_by_type(BACKLIGHT_RAW);
	if (!isl29034->bd)
        return -ENODEV;
	isl29034->max_brightness = isl29034->bd->props.max_brightness;
	res = of_property_read_u32(client->dev.of_node, "min-brightness", &isl29034->min_brightness);
	if(res < 0)
		isl29034->min_brightness = 10;

	INIT_DELAYED_WORK(&isl29034->light_work, isl29034_light_work);
	schedule_delayed_work(&isl29034->light_work, 3*HZ);
	dev_info(&client->dev, "%s isl29034: ALS chip found\n", client->name);

	return 0;
}

static int isl29034_remove(struct i2c_client *client)
{
	struct isl29034_info *isl29034 = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&isl29034->light_work);
	return 0;
}

static const struct i2c_device_id isl29034_id[] = {
	{ "isl29034", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, isl29034_id);

#ifdef CONFIG_PM

static int isl29034_suspend(struct device *dev)
{
	struct isl29034_info *isl29034 = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&isl29034->light_work);
	return 0;
}

static int isl29034_resume(struct device *dev)
{
	struct isl29034_info *isl29034 = dev_get_drvdata(dev);

	als_set_default_config(isl29034->client);
	schedule_delayed_work(&isl29034->light_work, HZ);
	return 0;
}

static const struct dev_pm_ops isl29034_pm_ops = {
	.suspend = isl29034_suspend,
	.resume = isl29034_resume,
};

#define isl29034_PM_OPS (&isl29034_pm_ops)
#else	/* CONFIG_PM */
#define isl29034_PM_OPS NULL
#endif	/* CONFIG_PM */

static struct i2c_driver isl29034_driver = {
	.driver = {
		.name = "isl29034",
		.pm = isl29034_PM_OPS,
	},
	.probe = isl29034_probe,
	.remove = isl29034_remove,
	.id_table = isl29034_id,
};

static int __init isl29034_init(void)
{
	return i2c_add_driver(&isl29034_driver);
}
late_initcall(isl29034_init);

static void __exit isl29034_exit(void)
{
	i2c_del_driver(&isl29034_driver);
}
module_exit(isl29034_exit);

MODULE_DESCRIPTION("Intersil isl29034 ALS Driver");
MODULE_LICENSE("GPL v2");
