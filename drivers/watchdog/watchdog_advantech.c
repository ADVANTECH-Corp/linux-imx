#ifdef CONFIG_ARCH_ADVANTECH
/*
 * Advantech Watchdog driver
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/clk.h>
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

#ifdef CONFIG_ARCH_ADVANTECH
#include <linux/proc-board.h>
#endif

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

#define ADV_WDT_MAX_TIME	65536
#define ADV_WDT_DEFAULT_TIME	60		/* in seconds */

#define WDOG_SEC_TO_COUNT(s)	(s * 10) //Time unite: 100ms -> 1s

#define ADV_WDT_STATUS_OPEN	0
#define ADV_WDT_STATUS_STARTED	1
#define ADV_WDT_EXPECT_CLOSE	2

#define DRIVER_NAME "adv-wdt-i2c"

#define REG_WDT_WATCHDOG_TIME_OUT	0x15
#define REG_WDT_POWER_OFF_TIME 		0x16
#define REG_WDT_INT_PRE_TIME 			0x17
#define REG_WDT_REMAIN_TIME_OUT		0x25
#define REG_WDT_REMAIN_PRE_TIME 	0x26
#define REG_WDT_VERSION 					0x27
#define REG_WDT_POWER_BTN_MODE 		0x28

static int gpio_wdt_en;
static int gpio_wdt_ping;
static int gpio_wdt_out;


struct i2c_client *adv_client;

static struct {
	struct clk *clk;
	unsigned timeout;
	unsigned remain_time;
	unsigned long status;
	int wdt_ping_status;
	int wdt_en_off;
	char version[2];
} adv_wdt;

static struct miscdevice adv_wdt_miscdev;

static bool nowayout = WATCHDOG_NOWAYOUT;

module_param(nowayout, bool, 0);

MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static unsigned timeout = ADV_WDT_DEFAULT_TIME;

module_param(timeout, uint, 0);

MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default="
				__MODULE_STRING(ADV_WDT_DEFAULT_TIME) ")");

struct watchdog_info adv_wdt_info = {
	.identity = "Advantech watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
	.firmware_version = 0,
};

static int adv_wdt_i2c_write_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
	u8 val[1 + len];
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

	if (i2c_transfer(client->adapter, msg, 1) != 1) {
		dev_err(&client->dev, "adv_wdt_i2c_write: i2c transfer failed\n");
		return -EIO;
	}
	
	msleep(100);
	return 0;
}

static int adv_wdt_i2c_read_reg(struct i2c_client *client, u8 reg, void *buf, size_t len)
{
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

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		dev_err(&client->dev, "adv_wdt_i2c_read: i2c transfer failed\n");
		return -EIO;
	}
	
	msleep(100);
	return 0;
}

int adv_wdt_i2c_set_timeout(struct i2c_client *client, int val)
{
	int ret = 0;
	val = WDOG_SEC_TO_COUNT(val) & 0x0000FFFF;
	ret = adv_wdt_i2c_write_reg(client, REG_WDT_WATCHDOG_TIME_OUT, &val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

int adv_wdt_i2c_read_timeout(struct i2c_client *client, int *val)
{
	int ret = 0;
	
	ret = adv_wdt_i2c_read_reg(client, REG_WDT_WATCHDOG_TIME_OUT, val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

int adv_wdt_i2c_read_remain_time(struct i2c_client *client, int *val)
{
	int ret = 0;
	
	ret = adv_wdt_i2c_read_reg(client, REG_WDT_REMAIN_TIME_OUT, val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

int adv_wdt_i2c_read_version(struct i2c_client *client, int *val)
{
	int ret = 0;
	
	ret = adv_wdt_i2c_read_reg(client, REG_WDT_VERSION, val, sizeof(val));
	if (ret)
		return -EIO;
	return 0;
}

static inline void adv_wdt_ping(void)
{
	msleep(800);
	adv_wdt.wdt_ping_status= !adv_wdt.wdt_ping_status;
	gpio_set_value(gpio_wdt_ping, adv_wdt.wdt_ping_status);
	msleep(100);
	//printk("adv_wdt_ping:%x\n", adv_wdt.wdt_ping_status);
	//printk("wdt_en_ping:%x\n", gpio_get_value(gpio_wdt_en));
}

static void adv_wdt_start(void)
{
	if (!test_and_set_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status)) 
	{
		/* at our first start we enable clock and do initialisations */
		gpio_set_value(gpio_wdt_en, !adv_wdt.wdt_en_off);
	} 

	/* Watchdog is enabled - time to reload the timeout value */
	adv_wdt_ping();
}

static void adv_wdt_stop(void)
{
	adv_wdt_ping();

	/* we don't need a clk_disable, it cannot be disabled once started.
	 * We use a timer to ping the watchdog while /dev/watchdog is closed */
	gpio_set_value(gpio_wdt_en, adv_wdt.wdt_en_off);
}

static int adv_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(ADV_WDT_STATUS_OPEN, &adv_wdt.status))
		return -EBUSY;
	adv_wdt_start();
	return nonseekable_open(inode, file);
}

static int adv_wdt_close(struct inode *inode, struct file *file)
{
	if (test_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status) && !nowayout)
		adv_wdt_stop();
	else {
		dev_crit(adv_wdt_miscdev.parent,
			"Unexpected close: Expect reboot!\n");
		adv_wdt_ping();
	}

	clear_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_OPEN, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status);
	return 0;
}

static long adv_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_value = 0;
//	u16 val;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		adv_wdt_ping();
		return copy_to_user(argp, &adv_wdt_info,
			sizeof(struct watchdog_info)) ? -EFAULT : 0;

	case WDIOC_GETSTATUS:
		return put_user(adv_wdt.status, p);
/*
	case WDIOC_GETBOOTSTATUS:
		val = __raw_readw(adv_wdt.base + ADV_WDT_WRSR);
		new_value = val & ADV_WDT_WRSR_TOUT ? WDIOF_CARDRESET : 0;
		return put_user(new_value, p);
*/
	case WDIOC_KEEPALIVE:
		adv_wdt_ping();
		return 0;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_value, p))
			return -EFAULT;
		if ((new_value < 1) || (new_value > ADV_WDT_MAX_TIME))
		{
			return -EINVAL;
		}
		adv_wdt.timeout = new_value;
		adv_wdt_i2c_set_timeout(adv_client, adv_wdt.timeout);
		adv_wdt_ping();
	
		/* Fallthrough to return current value */
	case WDIOC_GETTIMEOUT:
		adv_wdt_i2c_read_timeout(adv_client, &adv_wdt.timeout);
		//printk("WDIOC_GETTIMEOUT:%x\n", adv_wdt.timeout);
		return put_user(adv_wdt.timeout & 0xFFFF, p);

	case WDIOC_GETTIMELEFT:
		adv_wdt_i2c_read_remain_time(adv_client, &adv_wdt.remain_time);
		//printk("WDIOC_GETTIMELEFT:%x\n", adv_wdt.remain_time);
		return put_user(adv_wdt.remain_time & 0xFFFF, p);
	
	default:
		return -ENOTTY;
	}
}

static ssize_t adv_wdt_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	size_t i;
	char c;

	if (len == 0)	/* Can we see this even ? */
		return 0;

	clear_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status);
	/* scan to see whether or not we got the magic character */
	for (i = 0; i != len; i++) {
		if (get_user(c, data + i))
			return -EFAULT;
		if (c == 'V')
			set_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status);
	}

	adv_wdt_ping();
	
	return len;
}

static const struct file_operations adv_wdt_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = adv_wdt_ioctl,
	.open = adv_wdt_open,
	.release = adv_wdt_close,
	.write = adv_wdt_write,
};

static struct miscdevice adv_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &adv_wdt_fops,
};

static int adv_wdt_restart_handle(struct notifier_block *this, unsigned long mode,
			      void *cmd)
{
	if (test_and_set_bit(ADV_WDT_STATUS_OPEN, &adv_wdt.status))
		return -EBUSY;
	adv_wdt_start();
	adv_wdt.timeout = 10;
	adv_wdt_i2c_set_timeout(adv_client, adv_wdt.timeout / 10);
	//wait to show "Rebooting..." messages
	mdelay(500);
	adv_wdt_ping();
	return NOTIFY_DONE;
}

static struct notifier_block adv_wdt_restart_handler = {
	.notifier_call = adv_wdt_restart_handle,
	.priority = 128,
};

static int adv_wdt_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int tmp_version;
	struct device_node *np = client->dev.of_node;
	enum of_gpio_flags flags;
	
	if (!np)
	{
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		return -ENODEV;
	}	

	i2c_set_clientdata(client, &adv_wdt);

	adv_client = client;
	//Setting GPIO
	gpio_wdt_en = of_get_named_gpio_flags(np, "wdt-en", 0, &flags);
	if (!gpio_is_valid(gpio_wdt_en))
		return -ENODEV;	
	adv_wdt.wdt_en_off = flags;
	ret = devm_gpio_request_one(&client->dev, gpio_wdt_en,
				GPIOF_OUT_INIT_LOW, "adv_wdt.wdt_en");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}
	gpio_direction_output(gpio_wdt_en, adv_wdt.wdt_en_off);
	
	gpio_wdt_ping = of_get_named_gpio_flags(np, "wdt-ping", 0, &flags);
	if (!gpio_is_valid(gpio_wdt_ping))
		return -ENODEV;	

	ret = devm_gpio_request_one(&client->dev, gpio_wdt_ping, 
				GPIOF_OUT_INIT_LOW, "adv_wdt.wdt_ping");
	if (ret < 0) {
		dev_err(&client->dev, "request gpio failed: %d\n", ret);
		return ret;
	}
	adv_wdt.wdt_ping_status=flags;
	gpio_direction_output(gpio_wdt_ping, !flags);
	msleep(10);
	gpio_direction_output(gpio_wdt_ping, flags);

	/* We use common gpio pin to be watchdog-out pin (output-low) at present. We wait H/W rework, then remove.  */
	if (IS_ROM_7421) {
		gpio_wdt_out = of_get_named_gpio_flags(np, "wdt-out", 0, &flags);

        	if (!gpio_is_valid(gpio_wdt_out))
                	return -ENODEV;

		ret = devm_gpio_request_one(&client->dev, gpio_wdt_out, GPIOF_OUT_INIT_LOW, "adv_wdt.wdt_out`");

		if (ret < 0) {
			dev_err(&client->dev, "request gpio failed: %d\n", ret);
			return ret;
		}

	}

	adv_wdt.timeout = clamp_t(unsigned, timeout, 1, ADV_WDT_MAX_TIME);
	if (adv_wdt.timeout != timeout)
		dev_warn(&client->dev, "Initial timeout out of range! "
			"Clamped from %u to %u\n", timeout, adv_wdt.timeout);

	adv_wdt_miscdev.parent = &client->dev;
	ret = misc_register(&adv_wdt_miscdev);
	if (ret)
	{
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
	     WATCHDOG_MINOR, ret);
		goto fail;
	}
	
	ret = adv_wdt_i2c_read_version(client, &tmp_version);
	
	if (ret == 0 )
	{
     adv_wdt.version[0]= (tmp_version & 0xFF00) >> 8;
     adv_wdt.version[1]= tmp_version & 0xFF;
     adv_wdt_info.firmware_version = (int)(adv_wdt.version[1] - '0') * 10 + (int)(adv_wdt.version[0] - '0');
	}
	
	dev_info(&client->dev,
						"Advantech Watchdog Timer enabled. timeout=%ds (nowayout=%d), Ver.%d\n",
						adv_wdt.timeout, nowayout, adv_wdt_info.firmware_version);
	
	ret = register_restart_handler(&adv_wdt_restart_handler);
	if (ret) {
		pr_err("cannot register restart handler (err=%d)\n", ret);
		goto fail;
	}

	return 0;

fail:
	adv_wdt_miscdev.parent = NULL;
	return ret;
}

static int __exit adv_wdt_i2c_remove(struct i2c_client *client)
{
	misc_deregister(&adv_wdt_miscdev);
	unregister_restart_handler(&adv_wdt_restart_handler);

	if (test_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status))
	{	
		gpio_set_value(gpio_wdt_en, adv_wdt.wdt_en_off);
		dev_crit(adv_wdt_miscdev.parent, "Device removed: Expect reboot!\n");
	}
	clear_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_OPEN, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status);
	adv_wdt_miscdev.parent = NULL;
	return 0;
}

static int adv_wdt_i2c_resume(struct device *dev)
{
	if (test_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status))
	{
		gpio_set_value(gpio_wdt_en, !adv_wdt.wdt_en_off);
		adv_wdt_i2c_set_timeout(adv_client, adv_wdt.timeout / 10);
		adv_wdt_ping();
	}
	return 0;
}

static int adv_wdt_i2c_suspend(struct device *dev)
{
	if (test_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status)) {
		adv_wdt_stop();
	}
	return 0;
}

static void adv_wdt_i2c_shutdown(struct i2c_client *client)
{
	if (test_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status)) {
		/* we are running, we need to delete the timer but will give
		 * max timeout before reboot will take place */
		gpio_set_value(gpio_wdt_en, adv_wdt.wdt_en_off);
		adv_wdt_i2c_set_timeout(client, ADV_WDT_MAX_TIME / 10);
		adv_wdt_ping();

		dev_crit(adv_wdt_miscdev.parent,
			"Device shutdown: Expect reboot!\n");
	}
	clear_bit(ADV_WDT_EXPECT_CLOSE, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_OPEN, &adv_wdt.status);
	clear_bit(ADV_WDT_STATUS_STARTED, &adv_wdt.status);
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

static const struct dev_pm_ops adv_wdt_device_pm_ops = {
	.resume = adv_wdt_i2c_resume,
	.suspend = adv_wdt_i2c_suspend,
};

static struct i2c_driver adv_wdt_i2c_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = adv_wdt_i2c_dt_ids,
		   .pm = &adv_wdt_device_pm_ops,
		   },
	.probe = adv_wdt_i2c_probe,
	.remove = adv_wdt_i2c_remove,
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
