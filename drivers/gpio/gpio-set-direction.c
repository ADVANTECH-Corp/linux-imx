#ifdef CONFIG_ARCH_ADVANTECH

#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/of_gpio.h>

struct gpio_direction_data {
	int usb_host_pwr_en_gpio;
	int usb_host_oc_gpio;
	int wifi_det_gpio;
	int det_3g_gpio;
	int off_3g_gpio;
	int pcie_h_wlan_led_gpio;
	int pcie_h_wwan_led_gpio;
	int pcie_f_wlan_led_gpio;
	int pcie_f_wwan_led_gpio;
};

static const struct of_device_id of_gpio_direction_match[] = {
	{ .compatible = "gpio-set-direction", },
	{},
};

static int gpio_direction_probe(struct platform_device *pdev)
{
	struct gpio_direction_data *gpio;
	struct device_node *np = pdev->dev.of_node;
	enum of_gpio_flags usb_host_pwr_flag;
	unsigned long flags;
	int ret = 0;

	gpio = devm_kzalloc(&pdev->dev, sizeof(struct gpio_direction_data), GFP_KERNEL);
	
	if (!gpio) {
		printk("\n [gpio_direction_probe] Allocate gpio memory error... \n");
		return -ENOMEM;
	}

	/* Fetch GPIOs */
	/* USB Config */
	gpio->usb_host_pwr_en_gpio = of_get_named_gpio_flags(np, "usb-host-pwr-en", 0, &usb_host_pwr_flag);

	if (gpio_is_valid(gpio->usb_host_pwr_en_gpio)) {
		if(usb_host_pwr_flag)
			flags = GPIOF_OUT_INIT_HIGH;
		else
			flags = GPIOF_OUT_INIT_LOW;

		ret = devm_gpio_request_one(&pdev->dev,
					gpio->usb_host_pwr_en_gpio,
					flags,
					"usb-host-pwr-en");
		if (ret) {
			dev_err(&pdev->dev, "unable to get usb_host_pwr_en_gpio\n");
			return ret;
		}
	}
	
	gpio->usb_host_oc_gpio = of_get_named_gpio(np, "usb-host-oc", 0);

	if (gpio_is_valid(gpio->usb_host_oc_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->usb_host_oc_gpio,
					GPIOF_IN,
					"usb-host-oc");
		if (ret) {
			dev_err(&pdev->dev, "unable to get usb_host_oc_gpio\n");
			return ret;
		}
	}
	
	/* WIFI Config */
	gpio->wifi_det_gpio = of_get_named_gpio(np, "wifi-det", 0);

	if (gpio_is_valid(gpio->wifi_det_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->wifi_det_gpio,
					GPIOF_IN,
					"wifi-det");
		if (ret) {
			dev_err(&pdev->dev, "unable to get wifi_det_gpio\n");
			return ret;
		}
	}
	
	/* 3G Config */
	gpio->det_3g_gpio = of_get_named_gpio(np, "det-3g", 0);

	if (gpio_is_valid(gpio->det_3g_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->det_3g_gpio,
					GPIOF_IN,
					"3g-det");
		if (ret) {
			dev_err(&pdev->dev, "unable to get det_3g_gpio\n");
			return ret;
		}
	}
	
	gpio->off_3g_gpio = of_get_named_gpio(np, "off-3g", 0);

	if (gpio_is_valid(gpio->off_3g_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->off_3g_gpio,
					GPIOF_IN,
					"3g-off");
		if (ret) {
			dev_err(&pdev->dev, "unable to get off_3g_gpio\n");
			return ret;
		}
	}
	
	/* PCIE LED Config */
	gpio->pcie_h_wlan_led_gpio = of_get_named_gpio(np, "pcie-h-wlan-led", 0);

	if (gpio_is_valid(gpio->pcie_h_wlan_led_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->pcie_h_wlan_led_gpio,
					GPIOF_IN,
					"pcie-h-wlan-led");
		if (ret) {
			dev_err(&pdev->dev, "unable to get pcie_h_wlan_led_gpio\n");
			return ret;
		}
	}
	
	gpio->pcie_h_wwan_led_gpio = of_get_named_gpio(np, "pcie-h-wwan-led", 0);

	if (gpio_is_valid(gpio->pcie_h_wwan_led_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->pcie_h_wwan_led_gpio,
					GPIOF_IN,
					"pcie-h-wwan-led");
		if (ret) {
			dev_err(&pdev->dev, "unable to get pcie_h_wwan_led_gpio\n");
			return ret;
		}
	}

	gpio->pcie_f_wlan_led_gpio = of_get_named_gpio(np, "pcie-f-wlan-led", 0);

	if (gpio_is_valid(gpio->pcie_f_wlan_led_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->pcie_f_wlan_led_gpio,
					GPIOF_IN,
					"pcie-f-wlan-led");
		if (ret) {
			dev_err(&pdev->dev, "unable to get pcie_f_wlan_led_gpio\n");
			return ret;
		}
	}

	gpio->pcie_f_wwan_led_gpio = of_get_named_gpio(np, "pcie-f-wwan-led", 0);

	if (gpio_is_valid(gpio->pcie_f_wwan_led_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,
					gpio->pcie_f_wwan_led_gpio,
					GPIOF_IN,
					"pcie-f-wwan-led");
		if (ret) {
			dev_err(&pdev->dev, "unable to get pcie_f_wwan_led_gpio\n");
			return ret;
		}
	}

	return 0;
}

static struct platform_driver gpio_direction_driver = {
	.driver		= {
		.name	= "gpio-direction",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_gpio_direction_match),
	},
};

static int __init gpio_direction_init(void)
{
	return platform_driver_probe(&gpio_direction_driver, gpio_direction_probe);
}

subsys_initcall(gpio_direction_init);

MODULE_AUTHOR("Advantech");
MODULE_DESCRIPTION("GIPO SET DIRECTION driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-set-direction");

#endif
