#ifdef CONFIG_ARCH_ADVANTECH
/*
 * Advantech Watchdog driver
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#define ADV_WDT_MAX_RETRIES	3
#define ADV_WDT_WCR		0x00		/* Control Register */
#define ADV_WDT_WCR_WT		(0xFF << 8)	/* -> Watchdog Timeout Field */
#define ADV_WDT_WCR_WRE	(1 << 3)	/* -> WDOG Reset Enable */
#define ADV_WDT_WCR_WDE	(1 << 2)	/* -> Watchdog Enable */
#define ADV_WDT_WCR_WDZST	(1 << 0)	/* -> Watchdog timer Suspend */

#define ADV_WDT_WSR		0x02		/* Service Register */
#define ADV_WDT_SEQ1		0x5555		/* -> service sequence 1 */
#define ADV_WDT_SEQ2		0xAAAA		/* -> service sequence 2 */

#define ADV_WDT_WRSR		0x04		/* Reset Status Register */
#define ADV_WDT_WRSR_TOUT	(1 << 1)	/* -> Reset due to Timeout */

#define ADV_WDT_MAX_TIME	6527		/* in seconds */
#define ADV_WDT_DEFAULT_TIME	60		/* in seconds */

#define WDOG_SEC_TO_COUNT(s)	(s * 10)	/* Time unit for register: 100ms */

#define ADV_WDT_STATUS_STARTED	1

#define DRIVER_NAME "adv-wdt-i2c"

#define REG_WDT_WATCHDOG_TIME_OUT	0x15
#define REG_WDT_POWER_OFF_TIME 		0x16
#define REG_WDT_INT_PRE_TIME 			0x17
#define REG_WDT_REMAIN_TIME_OUT		0x25
#define REG_WDT_REMAIN_PRE_TIME 	0x26
#define REG_WDT_VERSION 					0x27
#define REG_WDT_POWER_BTN_MODE 		0x28

struct adv_wdt_device{
	struct watchdog_device wdog;
	unsigned long status;
	//unsigned int timeout;
	//unsigned int remain_time;
	int gpio_wdt_en;
	int gpio_wdt_ping;
	int wdt_ping_status;
	int wdt_en_off;
	unsigned char version[2];
};

static bool nowayout = WATCHDOG_NOWAYOUT;

module_param(nowayout, bool, 0);

MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static unsigned int timeout = ADV_WDT_DEFAULT_TIME;

module_param(timeout, uint, 0);

MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default="
				__MODULE_STRING(ADV_WDT_DEFAULT_TIME) ")");

static const struct watchdog_info adv_wdt_info = {
	.identity = "Advantech watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
};

static int adv_wdt_i2c_write_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
	u8 val[1 + len];
	u8 retry = 0;
	int err;

	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(val),
			.buf = val,
		}
	};

	val[0] = reg;
	memcpy(&val[1], buf, len);

	do {
		err = i2c_transfer(client->adapter, msg, 1);
		if (err == 1) {
			msleep(100);
			return 0;
		}

		retry++;
		dev_err(&client->dev, "adv_wdt_i2c_write_reg : i2c transfer failed, retrying\n");
		//msleep(3);
	} while (retry <= ADV_WDT_MAX_RETRIES);

	dev_err(&client->dev, "adv_wdt_i2c_write: i2c transfer failed\n");
	return -EIO;
}

static int adv_wdt_i2c_read_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
	u8 retry = 0;
	int err;

	struct i2c_msg msg[2] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	do {
		err = i2c_transfer(client->adapter, msg, 2);
		if (err == 2) {
			msleep(100);
			return 0;
		}

		retry++;
		dev_err(&client->dev, "adv_wdt_i2c_read : i2c transfer failed, retrying\n");
		//msleep(3);
	} while (retry <= ADV_WDT_MAX_RETRIES);

	dev_err(&client->dev, "adv_wdt_i2c_read: i2c transfer failed\n");
	return -EIO;
}

static int adv_wdt_i2c_fix_first_comm_issue(struct i2c_client *client, unsigned int val)
{
	int ret = 0;

	val = WDOG_SEC_TO_COUNT(val) & 0x0000FFFF;
	ret = adv_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, 2);
	msleep(100);
	val = 0;
	ret = adv_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, 2);
	return 0;
}

static int adv_wdt_i2c_set_timeout(struct i2c_client *client, unsigned int val)
{
	int ret = 0;

	val = WDOG_SEC_TO_COUNT(val) & 0x0000FFFF;
	ret = adv_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, sizeof(val));
	msleep(100);
	if (ret)
		return -EIO;
	return 0;
}

static int adv_wdt_i2c_read_remain_time(struct i2c_client *client, unsigned int *val)
{
	int ret = 0;

	ret = adv_wdt_i2c_read_reg(client, REG_WDT_REMAIN_TIME_OUT, val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

static int adv_wdt_i2c_read_version(struct i2c_client *client, unsigned int *val)
{
	int ret = 0;

	ret = adv_wdt_i2c_read_reg(client, REG_WDT_VERSION, val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

static int adv_wdt_ping(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	/* watchdog counter refresh input. Both edge trigger */
	wdev->wdt_ping_status= !wdev->wdt_ping_status;
	gpio_set_value(wdev->gpio_wdt_ping, wdev->wdt_ping_status);
	msleep(50);
	//printk("adv_wdt_ping:%x\n", adv_wdt.wdt_ping_status);
	//printk("wdt_en_ping:%x\n", gpio_get_value(gpio_wdt_en));
	
	return 0;
}

static int adv_wdt_start(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	if (test_and_set_bit(ADV_WDT_STATUS_STARTED, &wdev->status))
		return -EBUSY;

	gpio_set_value(wdev->gpio_wdt_en, wdev->wdt_en_off);
	adv_wdt_ping(wdog);
	set_bit(WDOG_HW_RUNNING, &wdog->status);

	return 0;
}

static int adv_wdt_set_timeout(struct watchdog_device *wdog,
				unsigned int new_timeout)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);

	wdog->timeout = min(new_timeout, ADV_WDT_MAX_TIME);
	adv_wdt_i2c_set_timeout(client, wdog->timeout);
	adv_wdt_ping(wdog);

	return 0;
}

static int adv_wdt_restart(struct watchdog_device *wdog, unsigned long action,
			    void *data)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	/* set timeout to 1 sec here and expect WDT_EN in restart handler */
	gpio_set_value(wdev->gpio_wdt_en, wdev->wdt_en_off);
	adv_wdt_i2c_set_timeout(client, 1);
	adv_wdt_ping(wdog);

	/* wait for reset to assert... */
	mdelay(2000);

	return 0;
}

static int adv_wdt_stop(struct watchdog_device *wdog)
{
	struct i2c_client *client = to_i2c_client(wdog->parent);
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	adv_wdt_ping(wdog);

	/* we don't need a clk_disable, it cannot be disabled once started.
	 * We use a timer to ping the watchdog while /dev/watchdog is closed */
	gpio_set_value(wdev->gpio_wdt_en, wdev->wdt_en_off);
	
	clear_bit(ADV_WDT_STATUS_STARTED, &wdev->status);
	
	return 0;
}

static const struct watchdog_ops adv_wdt_fops = {
	.owner = THIS_MODULE,
	.start = adv_wdt_start,
	.stop  = adv_wdt_stop,
	.ping = adv_wdt_ping,
	.set_timeout = adv_wdt_set_timeout,
	.restart = adv_wdt_restart,
};

static int adv_wdt_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct adv_wdt_device *wdev;
	int ret;
	unsigned int tmp_version;
	enum of_gpio_flags flags;

	if (!np)
	{
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		return -ENODEV;
	}
	
	wdev = devm_kzalloc(&client->dev, sizeof(struct adv_wdt_device), GFP_KERNEL);
	if (!wdev)
		return -ENOMEM;

	//Setting GPIO
	wdev->gpio_wdt_en = of_get_named_gpio_flags(np, "wdt-en", 0, &flags);
	if (!gpio_is_valid(wdev->gpio_wdt_en))
		return -ENODEV;	
	wdev->wdt_en_off = !flags;
	ret = devm_gpio_request_one(&client->dev, wdev->gpio_wdt_en,
				GPIOF_OUT_INIT_LOW, "adv_wdt.wdt_en");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}
	gpio_direction_output(wdev->gpio_wdt_en, flags);

	wdev->gpio_wdt_ping = of_get_named_gpio_flags(np, "wdt-ping", 0, &flags);
	if (!gpio_is_valid(wdev->gpio_wdt_ping))
		return -ENODEV;	

	ret = devm_gpio_request_one(&client->dev, wdev->gpio_wdt_ping, 
				GPIOF_OUT_INIT_LOW, "adv_wdt.wdt_ping");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}
	wdev->wdt_ping_status=flags;
	gpio_direction_output(wdev->gpio_wdt_ping, !flags);
	msleep(10);
	gpio_direction_output(wdev->gpio_wdt_ping, flags);

	wdev->wdog.timeout = clamp_t(unsigned, timeout, 1, ADV_WDT_MAX_TIME);
	if (wdev->wdog.timeout != timeout)
		dev_warn(&client->dev, "Initial timeout out of range! "
			"Clamped from %u to %u\n", timeout, wdev->wdog.timeout);

	if (of_get_property(np, "fix-first-comm-issue", NULL))
		adv_wdt_i2c_fix_first_comm_issue(client, wdev->wdog.timeout);

	ret = adv_wdt_i2c_set_timeout(client, wdev->wdog.timeout);
	if (ret)
	{
		pr_err("Set watchdog timeout err=%d\n", ret);
		//goto fail;
		return ret;
	}

	ret = adv_wdt_i2c_read_version(client, &tmp_version);
	if (ret == 0 )
	{
		wdev->version[0]= (tmp_version & 0xFF00) >> 8;
		wdev->version[1]= tmp_version & 0xFF;
		tmp_version = (unsigned int)(wdev->version[1] - '0') * 10 + (unsigned int)(wdev->version[0] - '0');
	} else {
		pr_err("Read watchdog version err=%d\n", ret);
		//goto fail;
		return ret;
	}

	wdev->wdog.info		= &adv_wdt_info;
	wdev->wdog.ops		= &adv_wdt_fops;
	wdev->wdog.min_timeout	= 1;
	wdev->wdog.max_hw_heartbeat_ms = ADV_WDT_MAX_TIME * 1000;
	wdev->wdog.parent		= &client->dev;
	wdev->wdog.bootstatus = 0;

	i2c_set_clientdata(client, wdev);
	watchdog_set_drvdata(&wdev->wdog, wdev);
	watchdog_set_nowayout(&wdev->wdog, nowayout);
	watchdog_set_restart_priority(&wdev->wdog, 128);
	watchdog_init_timeout(&wdev->wdog, wdev->wdog.timeout, &client->dev);
	watchdog_stop_ping_on_suspend(&wdev->wdog);

	dev_info(&client->dev,
						"Advantech Watchdog Timer enabled. timeout=%ds (nowayout=%d), Ver.%d\n",
						wdev->wdog.timeout, nowayout, tmp_version);
	return watchdog_register_device(&wdev->wdog);
}

static int adv_wdt_remove(struct i2c_client *client)
{
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	watchdog_unregister_device(&wdev->wdog);

	return 0;
}

static void adv_wdt_i2c_shutdown(struct i2c_client *client)
{
	struct adv_wdt_device *wdev = i2c_get_clientdata(client);

	if (test_bit(ADV_WDT_STATUS_STARTED, &wdev->status)) {
		/* set timeout to 1 sec here and expect WDT_EN in restart handler */
		gpio_set_value(wdev->gpio_wdt_en, wdev->wdt_en_off);
		adv_wdt_i2c_set_timeout(client, 1);
		adv_wdt_ping(&wdev->wdog);

		pr_warn("Device shutdown: Expect reboot!\n");
	}
	clear_bit(ADV_WDT_STATUS_STARTED, &wdev->status);
}

static const struct i2c_device_id adv_wdt_i2c_id[] = {
	{DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, adv_wdt_i2c_id);

static const struct of_device_id adv_wdt_i2c_dt_ids[] = {
	{ .compatible = "fsl,adv-wdt-i2c", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, adv_wdt_i2c_dt_ids);

static struct i2c_driver adv_wdt_i2c_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = adv_wdt_i2c_dt_ids,
	},
	.probe = adv_wdt_i2c_probe,
	.remove = adv_wdt_remove,
	.shutdown	= adv_wdt_i2c_shutdown,
	.id_table = adv_wdt_i2c_id,
};

static int __init adv_wdt_i2c_init(void)
{
	return i2c_add_driver(&adv_wdt_i2c_driver);
}

static void __exit adv_wdt_i2c_exit(void)
{
	i2c_del_driver(&adv_wdt_i2c_driver);
}

module_init(adv_wdt_i2c_init);
module_exit(adv_wdt_i2c_exit);

MODULE_DESCRIPTION("Advantech Watchdog I2C Driver");
MODULE_LICENSE("GPL");

#endif
