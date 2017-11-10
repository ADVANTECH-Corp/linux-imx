/*
 * biosysfshelper.h
 *
 *  Created on: 2011-11-25
 *      Author: rocky
 */
/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 *                   !!!CAUTION!!!
 *        !!!ONLY INCLUDE THIS FILE IN ONE .c file!!!
 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
#ifndef DAQ_SYSFS_HELPER_H_
#define DAQ_SYSFS_HELPER_H_

#ifndef ADVANTECH_VID
#error  "ADVANTECH_VID not defined. !!!You MUST define it before include this file!!!"
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))

static ssize_t daq_show_desc(struct class_device * dev, char * buf)
{
   daq_device_t * daq_dev = (daq_device_t *)class_get_devdata(dev);
#ifdef DEVICE_NAME_FROM_PID
   return sprintf(buf, "%s,BID#%d", DEVICE_NAME_FROM_PID(daq_dev->shared.ProductId),  daq_dev->shared.BoardId);
#else
   return sprintf(buf, DEVICE_NAME",BID#%d", daq_dev->shared.BoardId);
#endif
}
static ssize_t daq_show_vendor(struct class_device *dev, char * buf)
{
   return sprintf(buf, "0x%x", ADVANTECH_VID);
}
static ssize_t daq_show_device(struct class_device *dev, char * buf)
{
#ifdef DEVICE_ID_FROM_PID
   daq_device_t * daq_dev = (daq_device_t *)class_get_devdata(dev);
   return sprintf(buf,"0x%x", DEVICE_ID_FROM_PID(daq_dev->shared.ProductId));
#else
   return sprintf(buf,"0x%x", DEVICE_ID);
#endif
}
static ssize_t daq_show_driver(struct class_device *dev, char * buf)
{
   return sprintf(buf, DRIVER_NAME);
}

static CLASS_DEVICE_ATTR(desc,   S_IRUGO, daq_show_desc,   NULL);
static CLASS_DEVICE_ATTR(vendor, S_IRUGO, daq_show_vendor, NULL);
static CLASS_DEVICE_ATTR(device, S_IRUGO, daq_show_device, NULL);
static CLASS_DEVICE_ATTR(driver, S_IRUGO, daq_show_driver, NULL);

#define DAQ_SYSFS_INITIALIZE(__devid, __daq_dev)  \
         ({\
            struct class_device * __sysfs_dev = class_device_create(daq_class_get(), NULL, __devid, NULL, "daq""%d", MINOR(__devid));\
            if (!IS_ERR(__sysfs_dev)){\
               class_set_devdata(__sysfs_dev, __daq_dev);\
               do{\
               if (class_device_create_file(__sysfs_dev, &class_device_attr_desc))   break;\
               if (class_device_create_file(__sysfs_dev, &class_device_attr_vendor)) break;\
               if (class_device_create_file(__sysfs_dev, &class_device_attr_device)) break;\
               if (class_device_create_file(__sysfs_dev, &class_device_attr_driver)) break;\
               }while(0);\
             }\
             __sysfs_dev;\
         })
		 
#else

static ssize_t daq_show_desc(struct device * dev, struct device_attribute * attr, char * buf)
{
   daq_device_t * daq_dev = dev_get_drvdata(dev);
#ifdef DEVICE_NAME_FROM_PID
   return sprintf(buf, "%s,BID#%d", DEVICE_NAME_FROM_PID(daq_dev->shared.ProductId),  daq_dev->shared.BoardId);
#else
   return sprintf(buf, DEVICE_NAME",BID#%d", daq_dev->shared.BoardId);
#endif
}
static ssize_t daq_show_vendor(struct device *dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, "0x%x", ADVANTECH_VID);
}
static ssize_t daq_show_device(struct device *dev, struct device_attribute * attr, char * buf)
{
#ifdef DEVICE_ID_FROM_PID
   daq_device_t * daq_dev = dev_get_drvdata(dev);
   return sprintf(buf,"0x%x", DEVICE_ID_FROM_PID(daq_dev->shared.ProductId));
#else
   return sprintf(buf,"0x%x", DEVICE_ID);
#endif
}
static ssize_t daq_show_driver(struct device *dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, DRIVER_NAME);
}

static DEVICE_ATTR(desc,   S_IRUGO, daq_show_desc,   NULL);
static DEVICE_ATTR(vendor, S_IRUGO, daq_show_vendor, NULL);
static DEVICE_ATTR(device, S_IRUGO, daq_show_device, NULL);
static DEVICE_ATTR(driver, S_IRUGO, daq_show_driver, NULL);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
	#define DAQ_SYSFS_INITIALIZE(__devid, __daq_dev)  \
         ({\
            struct device *__sysfs_dev = device_create(daq_class_get(), NULL, __devid, "daq""%d", MINOR(__devid));\
            dev_set_drvdata(__sysfs_dev, __daq_dev);\
            if (!IS_ERR(__sysfs_dev)){\
               dev_set_drvdata(__sysfs_dev, __daq_dev);\
               do{\
               if (device_create_file(__sysfs_dev, &dev_attr_desc))   break;\
               if (device_create_file(__sysfs_dev, &dev_attr_vendor)) break;\
               if (device_create_file(__sysfs_dev, &dev_attr_device)) break;\
               if (device_create_file(__sysfs_dev, &dev_attr_driver)) break;\
               }while(0);\
             }\
             __sysfs_dev;\
         })
#else
	#define DAQ_SYSFS_INITIALIZE(__devid, __daq_dev)  \
         ({\
            struct device *__sysfs_dev = device_create(daq_class_get(), NULL, __devid, __daq_dev, "daq""%d", MINOR(__devid));\
            if (!IS_ERR(__sysfs_dev)){\
               dev_set_drvdata(__sysfs_dev, __daq_dev);\
               do{\
               if (device_create_file(__sysfs_dev, &dev_attr_desc))   break;\
               if (device_create_file(__sysfs_dev, &dev_attr_vendor)) break;\
               if (device_create_file(__sysfs_dev, &dev_attr_device)) break;\
               if (device_create_file(__sysfs_dev, &dev_attr_driver)) break;\
               }while(0);\
             }\
             __sysfs_dev;\
         })
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
#  define daq_sysfs_device          class_device
#  define daq_sysfs_device_destroy  class_device_destroy
#else
#  define daq_sysfs_device          device
#  define daq_sysfs_device_destroy  device_destroy
#endif

#endif /* DAQ_SYSFS_DEF_H_ */
