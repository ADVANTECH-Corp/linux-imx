/*
 * Platform driver for the Realtek RTL8367C ethernet switches
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include "rtk_switch.h"
#include "rtk_error.h"

#include "dal/dal_mgmt.h"
#include "dal/rtl8367c/rtl8367c_asicdrv_port.h"

extern void adv_set_smi_gpio(unsigned scl,unsigned sda);

static int rtl8367c_init(struct platform_device *pdev)
{
	int sck = of_get_named_gpio(pdev->dev.of_node, "gpio-sck", 0);
	int sda = of_get_named_gpio(pdev->dev.of_node, "gpio-sda", 0);
	int reset = of_get_named_gpio(pdev->dev.of_node, "reset-gpio", 0);
	int power = of_get_named_gpio(pdev->dev.of_node, "power-gpio", 0);

	if (!gpio_is_valid(sck) || !gpio_is_valid(sda) || !gpio_is_valid(reset) || !gpio_is_valid(power)) {
		dev_err(&pdev->dev, "gpios missing in devictree\n");
		return -EINVAL;
	}

	gpio_request_one(power, GPIOF_OUT_INIT_HIGH, "rtl8367c power");
	gpio_request_one(reset, GPIOF_OUT_INIT_LOW, "rtl8367c reset");
	msleep(20);
	gpio_direction_output(reset, 1);
	msleep(100);

	gpio_request_one(sck, GPIOF_OUT_INIT_HIGH, "rtl8367c sck");
	gpio_request_one(sda, GPIOF_OUT_INIT_HIGH, "rtl8367c sda");
	adv_set_smi_gpio(sck,sda);

	return 0;
}

static int  rtl8367c_probe(struct platform_device *pdev)
{
	rtk_api_ret_t retVal;
	//rtk_port_phy_ability_t phy_test_Ability;
	rtk_port_mac_ability_t imx_mac_ability;
	rtk_data_t txdelay,rxdelay;

	retVal=rtl8367c_init(pdev);
	if(retVal != RT_ERR_OK)
	{
		printk("rtl8367c_init fail!\n");
		return -ENODEV;
	}

	retVal=rtk_switch_init();
	if(retVal != RT_ERR_OK)
	{
		printk("rtk_switch_init fail!\n");
		return -ENODEV;
	}
	retVal=RT_MAPPER->port_phyEnableAll_set(ENABLED);

	rtk_led_groupConfig_set(LED_GROUP_0,LED_CONFIG_LINK_ACT);
	if(retVal != RT_ERR_OK)
	{  
		printk("dal_rtl8367c_led_groupConfig_set fail\n");
		return -EINVAL;
	}
	rtk_led_groupConfig_set(LED_GROUP_1,LED_CONFIG_SPD1000);
	if(retVal != RT_ERR_OK)
	{
		printk("dal_rtl8367c_led_groupConfig_set fail\n");
		return -EINVAL;
	}
	rtk_led_groupConfig_set(LED_GROUP_2,LED_CONFIG_SPD100);
	if(retVal != RT_ERR_OK)
	{
		printk("dal_rtl8367c_led_groupConfig_set fail\n");
		return -EINVAL;
	} 
	rtk_led_serialMode_set(LED_ACTIVE_HIGH);
	if(retVal != RT_ERR_OK)
	{
		printk("rtk_led_serialMode_set fail\n"); 
		return -EINVAL;
	}     
	imx_mac_ability.forcemode 	= MAC_FORCE;
	imx_mac_ability.speed 		= PORT_SPEED_1000M;
	imx_mac_ability.duplex 		= PORT_FULL_DUPLEX;
	imx_mac_ability.link 		= PORT_LINKUP;
	imx_mac_ability.nway 		= DISABLED;
	imx_mac_ability.txpause		= ENABLED;
	imx_mac_ability.rxpause		= ENABLED;

	retVal=rtk_port_macForceLinkExt_set(EXT_PORT0,MODE_EXT_RGMII,&imx_mac_ability);
	if(retVal != RT_ERR_OK)
		printk("rtk_port_macForceLinkExt_set fail\n");

	txdelay=1;
	rxdelay=4;
	rtk_port_rgmiiDelayExt_set(EXT_PORT0,txdelay,rxdelay);
	if(retVal != RT_ERR_OK)
		printk("rtk_port_rgmiiDelayExt_set fail\n");

	retVal=rtk_port_phyComboPortMedia_set(UTP_PORT4,PORT_MEDIA_FIBER);
	if(retVal != RT_ERR_OK)
		printk("rtk_port_phyComboPortMedia_set fail\n");
	else
		printk("switch booting ok\n");

/*
	//vlan init
	rtk_vlan_init();
	 
	rtk_vlan_cfg_t vlan100,vlan200;
	
	memset(&vlan200,0x00,sizeof(rtk_vlan_cfg_t));
	//set member
	RTK_PORTMASK_PORT_SET(vlan200.mbr,UTP_PORT0);
	RTK_PORTMASK_PORT_SET(vlan200.mbr,EXT_PORT0);
	//set untag port
	RTK_PORTMASK_PORT_SET(vlan200.untag,UTP_PORT0);
	
	rtk_vlan_set(200,&vlan200);
	
	memset(&vlan100,0x00,sizeof(rtk_vlan_cfg_t));
	//set member
	RTK_PORTMASK_PORT_SET(vlan100.mbr,UTP_PORT1);
	RTK_PORTMASK_PORT_SET(vlan100.mbr,UTP_PORT2);
	RTK_PORTMASK_PORT_SET(vlan100.mbr,UTP_PORT3);
	RTK_PORTMASK_PORT_SET(vlan100.mbr,UTP_PORT4);
	RTK_PORTMASK_PORT_SET(vlan100.mbr,EXT_PORT0);
	//set untag port
	RTK_PORTMASK_PORT_SET(vlan100.untag,UTP_PORT1);
	RTK_PORTMASK_PORT_SET(vlan100.untag,UTP_PORT2);
	RTK_PORTMASK_PORT_SET(vlan100.untag,UTP_PORT3);
	RTK_PORTMASK_PORT_SET(vlan100.untag,UTP_PORT4);
	rtk_vlan_set(100,&vlan100);
	
	//set pvid
	rtk_vlan_portPvid_set(UTP_PORT0,200,0);
	rtk_vlan_portPvid_set(UTP_PORT1,100,0);
	rtk_vlan_portPvid_set(UTP_PORT2,100,0);
	rtk_vlan_portPvid_set(UTP_PORT3,100,0);
	rtk_vlan_portPvid_set(UTP_PORT4,100,0);
	rtk_vlan_portPvid_set(EXT_PORT0,100,0);
*/
/*
	if(test_info.test_port_number>=0&&test_info.test_port_number<=4)
	{
		printk("open switch mode,test lan number is %d\n",test_info.test_port_number);
		if(test_info.test_full_100)
		{
			printk("set test lan mode is Full_100\n");
			memset(&phy_test_Ability,0x00,sizeof(rtk_port_phy_ability_t));
			phy_test_Ability.Full_100 = 1;
			retVal=rtk_port_phyAutoNegoAbility_set(test_info.test_port_number,&phy_test_Ability);
			if(retVal != RT_ERR_OK)
				printk("rtk_port_phyAutoNegoAbility_set fail\n");
		}
		retVal=RT_MAPPER->port_phyTestMode_set(test_info.test_port_number,1);
		if(retVal != RT_ERR_OK)
			printk("port_phyTestMode_set fail\n");
	}
*/
	return 0;
}

static int rtl8367c_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rtl8367c_match[] = {
	{ .compatible = "realtek,rtl8367c" },
	{},
};
MODULE_DEVICE_TABLE(of, rtl8367c_match);
#endif

static struct platform_driver rtl8367c_driver = {
	.driver = {
		.name		= "rtl8367c",
		.owner		= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(rtl8367c_match),
#endif
	},
	.probe		= rtl8367c_probe,
	.remove		= rtl8367c_remove,
};

static int __init rtl8367c_module_init(void)
{
	return platform_driver_register(&rtl8367c_driver);
}
late_initcall(rtl8367c_module_init);

static void __exit rtl8367c_module_exit(void)
{
	platform_driver_unregister(&rtl8367c_driver);
}
module_exit(rtl8367c_module_exit);

MODULE_DESCRIPTION("RTL8367C Swtich Driver");
MODULE_LICENSE("GPL v2");
