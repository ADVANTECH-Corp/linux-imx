/*
 * Dummy power supply driver for android
 *
 * Copyright (C) 2017 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/errno.h>
#include <linux/slab.h>

#define DUMMY_POWER_NUM 2
#define POWER_SUPPLY_PROP_CAPACITY_VALUE 85
#define POWER_SUPPLY_PROP_VOLTAGE_NOW_VALUE 3600
#define POWER_SUPPLY_PROP_CURRENT_NOW_VALUE 400000
#define POWER_SUPPLY_PROP_CYCLE_COUNT_VALUE 32
#define POWER_SUPPLY_PROP_TEMP_VALUE 350
#define POWER_SUPPLY_PROP_CHARGE_FULL_VALUE 4000000
#define POWER_SUPPLY_PROP_CHARGE_COUNTER_VALUE 1900000
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX_VALUE 500000
#define POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX_VALUE 5000000

static enum power_supply_property dummy_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CYCLE_COUNT,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CHARGE_FULL,
    POWER_SUPPLY_PROP_CHARGE_COUNTER,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
    POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property dummy_usb_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static int dummy_usb_get_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = 1;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static int dummy_battery_get_property(struct power_supply *psy,
                                      enum power_supply_property psp,
                                      union power_supply_propval *val)
{
    int ret = 0;
    switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
            val->intval = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = POWER_SUPPLY_HEALTH_GOOD;
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            val->intval = true;
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            val->intval = POWER_SUPPLY_PROP_CAPACITY_VALUE;
            break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = POWER_SUPPLY_PROP_VOLTAGE_NOW_VALUE;
            break;
        case POWER_SUPPLY_PROP_CURRENT_NOW:
            val->intval = POWER_SUPPLY_PROP_CURRENT_NOW_VALUE;
            break;
        case POWER_SUPPLY_PROP_CYCLE_COUNT:
            val->intval = POWER_SUPPLY_PROP_CYCLE_COUNT_VALUE;
            break;
        case POWER_SUPPLY_PROP_TEMP:
            val->intval = POWER_SUPPLY_PROP_TEMP_VALUE;
            break;
        case POWER_SUPPLY_PROP_CHARGE_FULL:
            val->intval = POWER_SUPPLY_PROP_CHARGE_FULL_VALUE;
            break;
        case POWER_SUPPLY_PROP_CHARGE_COUNTER:
            val->intval = POWER_SUPPLY_PROP_CHARGE_COUNTER_VALUE;
            break;
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
            val->intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX_VALUE;
            break;
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
            val->intval = POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX_VALUE;
            break;
        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = true;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static struct power_supply *dummy_power_supplies[DUMMY_POWER_NUM];
static const struct power_supply_desc dummy_power_desc[] = {
    {
        .properties = dummy_battery_props,
        .num_properties = ARRAY_SIZE(dummy_battery_props),
        .get_property = dummy_battery_get_property,
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
    },
    {
        .properties = dummy_usb_props,
        .num_properties = ARRAY_SIZE(dummy_usb_props),
        .get_property = dummy_usb_get_property,
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_MAINS,
    }
};

static int __init dummy_power_init(void)
{
    struct power_supply_config psy_cfg = {};
    int i;
    int ret;
    for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++) {
        dummy_power_supplies[i] = power_supply_register(NULL,
                                                        &dummy_power_desc[i],
                                                        &psy_cfg);
        if (IS_ERR(dummy_power_supplies[i])) {
            ret = PTR_ERR(dummy_power_supplies[i]);
            goto failed;
        }
    }
    return 0;
failed:
    while (--i >= 0)
        power_supply_unregister(dummy_power_supplies[i]);
    return ret;
}

static void __exit dummy_power_exit(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++) {
        power_supply_unregister(dummy_power_supplies[i]);
    }
}

module_init(dummy_power_init);
module_exit(dummy_power_exit);

MODULE_DESCRIPTION("Power supply driver for Android devices with no battery");
MODULE_AUTHOR("Dave Rim davidrim@google.com");
MODULE_LICENSE("GPL");
