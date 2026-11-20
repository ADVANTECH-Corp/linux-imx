/*
 * MSP430 Watchdog driver
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/notifier.h>

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

static int msp430_of_get_named_gpio_flags(struct device_node *np,
					  const char *propname, int index,
					  int *flags)
{
	struct of_phandle_args gpiospec;
	int ret;

	ret = of_parse_phandle_with_args(np, propname, "#gpio-cells", index,
					 &gpiospec);
	if (ret)
		return ret;

	if (flags)
		*flags = gpiospec.args_count > 1 ? gpiospec.args[1] : 0;

	of_node_put(gpiospec.np);

	return of_get_named_gpio(np, propname, index);
}

#define MSP430_WDT_MAX_TIME	6527		/* in seconds */
#define MSP430_WDT_DEFAULT_TIME	60		/* in seconds */

#define WDOG_SEC_TO_COUNT(s)	((s) * 10)	/* Time unit for register: 100ms */
#define WDOG_COUNT_TO_SEC(s)	((s) / 10)	/* Time unit for register: 100ms */

#define MSP430_WDT_STATUS_OPEN	0
#define MSP430_WDT_STATUS_STARTED	1
#define MSP430_WDT_EXPECT_CLOSE	2

#define DRIVER_NAME "msp430_wdt"

#define REG_WDT_WATCHDOG_TIME_OUT	0x15
#define REG_WDT_POWER_OFF_TIME 		0x16
#define REG_WDT_INT_PRE_TIME 		0x17
#define REG_WDT_REMAIN_TIME_OUT		0x25
#define REG_WDT_REMAIN_PRE_TIME 	0x26
#define REG_WDT_VERSION 			0x27
#define REG_WDT_POWER_BTN_MODE 		0x28

struct msp430_wdt_dev {
	unsigned long status;
	int gpio_wdt_ping;
	int wdt_ping_status;
	int gpio_wdt_en;
	int wdt_en_off;
	struct watchdog_device wdog;
	struct notifier_block reboot_nb;
	struct i2c_client *client;
	unsigned char version[2];
};

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static unsigned int timeout = MSP430_WDT_DEFAULT_TIME;
module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default="
				__MODULE_STRING(MSP430_WDT_DEFAULT_TIME) ")");

static struct watchdog_info msp430_wdt_info = {
	.identity = "msp430 watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
	.firmware_version = 0,
};

static int msp430_wdt_i2c_write_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
	//u8 val[1 + len];
	u8* val = kmalloc((1 + len) * sizeof(u8), GFP_KERNEL);
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = I2C_M_IGNORE_NAK,
			.len = sizeof(val),
			.buf = val,
		}
	};

	val[0] = reg;
	memcpy(&val[1], buf, len);

	if (i2c_transfer(client->adapter, msg, 1) != 1) {
		dev_err(&client->dev, "msp430_wdt_i2c_write: i2c transfer failed\n");
		kfree(val);
		return -EIO;
	}
	
	msleep(10);
	kfree(val);
	return 0;
}

static int msp430_wdt_i2c_read_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
	struct i2c_msg msg[2] = {
		{
			.addr	= client->addr,
			.flags	= I2C_M_IGNORE_NAK,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD|I2C_M_IGNORE_NAK,
			.len	= len,
			.buf	= buf,
		}
	};

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(&client->dev, "msp430_wdt_i2c_read: i2c transfer failed\n");
		return -EIO;
	}
	
	msleep(10);
	return 0;
}
static int msp430_wdt_i2c_set_timeout_2_bytes(struct i2c_client *client, unsigned int val)
{
	int ret = 0;
	val = WDOG_SEC_TO_COUNT(val) & 0x0000FFFF;
	ret = msp430_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, 2);
	msp430_wdt_i2c_read_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, sizeof(val));

	if (ret)
		return -EIO;
	return 0;
}

static int msp430_wdt_i2c_set_timeout(struct i2c_client *client, unsigned int val)
{
	int ret = 0;
	val = WDOG_SEC_TO_COUNT(val) & 0x0000FFFF;
	ret = msp430_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, sizeof(val));
	msp430_wdt_i2c_read_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

static int msp430_wdt_i2c_read_version(struct i2c_client *client, unsigned int *val)
{
	int ret = 0;
	
	ret = msp430_wdt_i2c_read_reg(client, REG_WDT_VERSION, val, sizeof(*val));
	if (ret)
		return -EIO;
	return 0;
}

static int msp430_wdt_ping(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	msp430_wdt->wdt_ping_status= !msp430_wdt->wdt_ping_status;
	gpio_set_value(msp430_wdt->gpio_wdt_ping, msp430_wdt->wdt_ping_status);

	return 0;
}

static int msp430_wdt_set_timeout(struct watchdog_device *wdog,
				unsigned int new_timeout)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);

	wdog->timeout = clamp_t(unsigned, new_timeout, 1, MSP430_WDT_MAX_TIME);
	msp430_wdt_i2c_set_timeout(client, wdog->timeout);
	msp430_wdt_ping(wdog);

	return 0;
}

static int msp430_wdt_restart(struct watchdog_device *wdog, unsigned long action,
			    void *data)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	/* set timeout to 1 sec here and expect WDT_EN in restart handler */
	gpio_set_value(msp430_wdt->gpio_wdt_en, !msp430_wdt->wdt_en_off);

	/* wait for reset to assert... */
	mdelay(2000);

	return 0;
}

static int msp430_wdt_start(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	if (test_and_set_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status))
		return -EBUSY;

	gpio_set_value(msp430_wdt->gpio_wdt_en, !msp430_wdt->wdt_en_off);
	msp430_wdt_ping(wdog);
	set_bit(WDOG_HW_RUNNING, &wdog->status);

	return 0;
}

static int msp430_wdt_stop(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	msp430_wdt_ping(wdog);

	/* we don't need a clk_disable, it cannot be disabled once started.
	 * We use a timer to ping the watchdog while /dev/watchdog is closed */
	gpio_set_value(msp430_wdt->gpio_wdt_en, msp430_wdt->wdt_en_off);

	clear_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status);

	return 0;
}


static const struct watchdog_ops msp430_wdt_fops = {
	.owner = THIS_MODULE,
	.start = msp430_wdt_start,
	.stop  = msp430_wdt_stop,
	.ping = msp430_wdt_ping,
	.set_timeout = msp430_wdt_set_timeout,
	.restart = msp430_wdt_restart,
};


static int msp430_wdt_reboot_notifier(struct notifier_block *nb,
				    unsigned long code, void *data)
{
	struct msp430_wdt_dev *msp430_wdt;
	char *cmd = (char*)data;

	if(cmd) {
		if(strcmp(cmd, "recovery") == 0)
			return NOTIFY_DONE;

		if(strcmp(cmd, "bootloader") == 0)
			return NOTIFY_DONE;

		if(strcmp(cmd, "loader") == 0)
			return NOTIFY_DONE;

		if(strcmp(cmd, "backup") == 0)
			return NOTIFY_DONE;
	}

	if (code == SYS_RESTART) {
		printk("System is rebooting, set watchdog timeout to 1 second\n");
		msp430_wdt = container_of(nb, struct msp430_wdt_dev, reboot_nb);
		msp430_wdt_i2c_set_timeout(msp430_wdt->client, 1);
	}

	return NOTIFY_DONE;
}

static int msp430_wdt_i2c_probe(struct i2c_client *client)
{
	int ret;
	unsigned int tmp_version;
	struct device_node *np = client->dev.of_node;
	int flags;
	struct msp430_wdt_dev *msp430_wdt;

	if (!np)
	{
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		return -ENODEV;
	}

	msp430_wdt = devm_kzalloc(&client->dev, sizeof(struct msp430_wdt_dev), GFP_KERNEL);
	if (!msp430_wdt)
		return -ENOMEM;

	i2c_set_clientdata(client, msp430_wdt);
	dev_set_drvdata(&client->dev, msp430_wdt);

	msp430_wdt->client = client;
	//Setting GPIO
	msp430_wdt->gpio_wdt_en = msp430_of_get_named_gpio_flags(np, "wdt-en", 0, &flags);
	if (!gpio_is_valid(msp430_wdt->gpio_wdt_en))
		return -ENODEV;	
	msp430_wdt->wdt_en_off = flags;
	ret = devm_gpio_request_one(&client->dev, msp430_wdt->gpio_wdt_en,
				GPIOF_OUT_INIT_LOW, "msp430_wdt.wdt_en");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}
	gpio_direction_output(msp430_wdt->gpio_wdt_en, msp430_wdt->wdt_en_off);
	
	msp430_wdt->gpio_wdt_ping = msp430_of_get_named_gpio_flags(np, "wdt-ping", 0, &flags);
	if (!gpio_is_valid(msp430_wdt->gpio_wdt_ping))
		return -ENODEV;	

	ret = devm_gpio_request_one(&client->dev, msp430_wdt->gpio_wdt_ping, 
				GPIOF_OUT_INIT_LOW, "msp430_wdt.wdt_ping");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}

	msleep(10);
	gpio_direction_output(msp430_wdt->gpio_wdt_ping, 1);
	msp430_wdt->wdt_ping_status=1;

	msp430_wdt->wdog.timeout  = clamp_t(unsigned, timeout, 1, MSP430_WDT_MAX_TIME);
	if (msp430_wdt->wdog.timeout != timeout)
		dev_warn(&client->dev, "Initial timeout out of range! "
			"Clamped from %u to %u\n", timeout, msp430_wdt->wdog.timeout);


	ret = msp430_wdt_i2c_read_version(client, &tmp_version);
	if (ret == 0 )
	{
		msp430_wdt->version[0]= (tmp_version & 0xFF00) >> 8;
		msp430_wdt->version[1]= tmp_version & 0xFF;
		msp430_wdt_info.firmware_version = (unsigned int)(msp430_wdt->version[1] - '0') * 10 + (unsigned int)(msp430_wdt->version[0] - '0');
	} else {
		pr_err("Read watchdog version err=%d\n", ret);
		return ret;
	}

	//There is NAK when first time to set 4 bytes timeout.
	msp430_wdt_i2c_set_timeout_2_bytes(client, msp430_wdt->wdog.timeout);

	ret = msp430_wdt_i2c_set_timeout(client, msp430_wdt->wdog.timeout);
	if (ret)
	{
		pr_err("Set MSP430 watchdog timeout err=%d\n", ret);
		return ret;
	}

	msp430_wdt->wdog.info		= &msp430_wdt_info;
	msp430_wdt->wdog.ops		= &msp430_wdt_fops;
	msp430_wdt->wdog.min_timeout	= 1;
	msp430_wdt->wdog.max_hw_heartbeat_ms = MSP430_WDT_MAX_TIME * 1000;
	msp430_wdt->wdog.parent		= &client->dev;
	watchdog_set_nowayout(&msp430_wdt->wdog, nowayout);
	watchdog_set_restart_priority(&msp430_wdt->wdog, 255);
	watchdog_init_timeout(&msp430_wdt->wdog, msp430_wdt->wdog.timeout, &client->dev);
	watchdog_stop_ping_on_suspend(&msp430_wdt->wdog);

	msp430_wdt->reboot_nb.notifier_call = msp430_wdt_reboot_notifier;
	register_reboot_notifier(&msp430_wdt->reboot_nb);

	dev_info(&client->dev,
						"MSP430 Watchdog Timer enabled. timeout=%ds (nowayout=%d), Ver.%d\n",
						msp430_wdt->wdog.timeout, nowayout, msp430_wdt_info.firmware_version);

	return watchdog_register_device(&msp430_wdt->wdog);
}

static void __exit msp430_wdt_i2c_remove(struct i2c_client *client)
{
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	watchdog_unregister_device(&msp430_wdt->wdog);
}

static int msp430_wdt_i2c_resume(struct device *dev)
{
	struct msp430_wdt_dev *msp430_wdt = dev_get_drvdata(dev);
	msp430_wdt_i2c_set_timeout(msp430_wdt->client, msp430_wdt->wdog.timeout);

	if (test_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status))
	{
		gpio_set_value(msp430_wdt->gpio_wdt_en, !msp430_wdt->wdt_en_off);
		msp430_wdt_ping(&msp430_wdt->wdog);
	}

	return 0;
}

static int msp430_wdt_i2c_suspend(struct device *dev)
{
	struct msp430_wdt_dev *msp430_wdt = dev_get_drvdata(dev);

	if (test_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status)) {
		msp430_wdt_ping(&msp430_wdt->wdog);
		gpio_set_value(msp430_wdt->gpio_wdt_en, msp430_wdt->wdt_en_off);
	}

	gpio_direction_output(msp430_wdt->gpio_wdt_ping, 0);
	msp430_wdt->wdt_ping_status=0;

	return 0;
}

static void msp430_wdt_i2c_shutdown(struct i2c_client *client)
{
	struct msp430_wdt_dev *msp430_wdt = i2c_get_clientdata(client);

	if (test_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status)) {
		/* we are running, we need to delete the timer but will give
		 * max timeout before reboot will take place */
		gpio_set_value(msp430_wdt->gpio_wdt_en, msp430_wdt->wdt_en_off);

		dev_crit(&client->dev,
			"Device shutdown: Expect reboot!\n");
	}
	clear_bit(MSP430_WDT_EXPECT_CLOSE, &msp430_wdt->status);
	clear_bit(MSP430_WDT_STATUS_OPEN, &msp430_wdt->status);
	clear_bit(MSP430_WDT_STATUS_STARTED, &msp430_wdt->status);
}

static const struct i2c_device_id msp430_wdt_i2c_id[] = {
	{DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, msp430_wdt_i2c_id);

static const struct of_device_id msp430_wdt_i2c_dt_ids[] = {
	{ .compatible = "std,msp430-wdt", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, msp430_wdt_i2c_dt_ids);

static const struct dev_pm_ops msp430_wdt_device_pm_ops = {
	.resume = msp430_wdt_i2c_resume,
	.suspend = msp430_wdt_i2c_suspend,
};

static struct i2c_driver msp430_wdt_i2c_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = msp430_wdt_i2c_dt_ids,
		   .pm = &msp430_wdt_device_pm_ops,
		   },
	.probe = msp430_wdt_i2c_probe,
	.remove = msp430_wdt_i2c_remove,
	.shutdown	= msp430_wdt_i2c_shutdown,
	.id_table = msp430_wdt_i2c_id,
};

static int __init msp430_wdt_i2c_init(void)
{
	return i2c_add_driver(&msp430_wdt_i2c_driver);
}

static void __exit msp430_wdt_i2c_exit(void)
{
	i2c_del_driver(&msp430_wdt_i2c_driver);
}

module_init(msp430_wdt_i2c_init);
module_exit(msp430_wdt_i2c_exit);

MODULE_DESCRIPTION("MSP430 Watchdog I2C Driver");
MODULE_LICENSE("GPL");
