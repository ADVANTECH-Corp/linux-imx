#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <adv/bdaqdef.h>
#include <adv/linux/ioctls.h>
#include <adv/linux/biokernbase.h>


LIST_HEAD(daq_isa_drivers);
static DEFINE_SPINLOCK(dev_list_lock);

int daq_isa_match_one_device(struct daq_isa_device *device, struct daq_isa_driver *driver)
{
   if ((strcmp(device->driver_name, driver->name) == 0)) {
      return 0;
   }

   return 1;
}

int daq_isa_register_driver(struct daq_isa_driver *driver)
{
   daq_trace((KERN_INFO "Register %s driver\n", driver->name));

   INIT_LIST_HEAD(&driver->devices);
   INIT_LIST_HEAD(&driver->isa_drivers);

   if (driver->match == NULL) {
      driver->match = daq_isa_match_one_device;
   }

   spin_lock(&dev_list_lock);
   list_add_tail(&driver->isa_drivers, &daq_isa_drivers);
   spin_unlock(&dev_list_lock);
   
   return 0;
}


int daq_isa_unregister_driver(struct daq_isa_driver *driver)
{
   struct daq_isa_driver *drv;
   struct daq_isa_device *dev;

   if (unlikely(list_empty(&daq_isa_drivers))) {
      daq_trace((KERN_INFO "DAQ driver list is empty\n"));
      return 0;
   }

   spin_lock(&dev_list_lock);
   list_for_each_entry(drv, &daq_isa_drivers, isa_drivers) {
      if (drv == driver){
         while(!list_empty(&drv->devices)){
            dev = list_entry(drv->devices.next, typeof(*dev), device_list);
            list_del(&dev->device_list);
            drv->remove(dev);
            kfree(dev);
            dev = NULL;
         }

         list_del(&drv->isa_drivers);
         spin_unlock(&dev_list_lock);
         return 0;
      }
   }
   spin_unlock(&dev_list_lock);
   
   return 0;  
}

int daq_isa_add_device(struct daq_isa_driver *driver, struct daq_isa_device *device)
{
   daq_trace((KERN_INFO "add device %s for %s\n", device->device_name, driver->name));
   
   if ((driver == NULL) || (device == NULL)) {
      return -EFAULT;
   }

   spin_lock(&dev_list_lock);
   list_add_tail(&device->device_list, &driver->devices);
   device->driver = driver;
   spin_unlock(&dev_list_lock);

   return 0;
}

int daq_isa_remove_device(struct daq_isa_driver *driver, struct daq_isa_device *device)
{
   struct daq_isa_device *dev;
   daq_trace((KERN_INFO "remove device %s\n", device->device_name));

   if ((driver == NULL) || (device == NULL)) {
      return -EFAULT;
   }

   if (unlikely(list_empty(&driver->devices))) {
      daq_trace((KERN_INFO "DAQ driver list is empty\n"));
      return 0;
   }

 //  spin_lock(&dev_list_lock);
   list_for_each_entry(dev, &driver->devices, device_list) {
      if (dev == device) {
         list_del(&dev->device_list);
         break;
      }
   }
//   spin_unlock(&dev_list_lock);

   return 0;
}

int daq_add_isa_device(char *device_name, char * driver_name, uint32 iobase, uint32 irq, uint32 dmachan)
{
   int ret = 0;
   struct daq_isa_device * isa_dev = NULL;
   struct daq_isa_driver * isa_drv = NULL;

   isa_dev = kzalloc(sizeof(*isa_dev), GFP_KERNEL);
   if (!isa_dev) {
      daq_trace((KERN_INFO "Fail to allocate memory for isa device\n"));
      return -ENOMEM;
   }
   isa_dev->iobase  = iobase;
   isa_dev->irq     = irq;
   isa_dev->dmachan = dmachan;

   memset((void *)isa_dev->device_name, 0, ISA_NAME_MAX_LEN);
   memset((void *)isa_dev->driver_name, 0, ISA_NAME_MAX_LEN);
   memcpy((void *)isa_dev->device_name, device_name, ISA_NAME_MAX_LEN - 1);
   memcpy((void *)isa_dev->driver_name, driver_name, ISA_NAME_MAX_LEN - 1);

   if (unlikely(list_empty(&daq_isa_drivers))) {
      daq_trace((KERN_INFO "isa driver list is empty\n"));
      kfree(isa_dev);
      return -ENODEV;
   }

   spin_lock(&dev_list_lock);
   list_for_each_entry(isa_drv, &daq_isa_drivers, isa_drivers) {
      if (!isa_drv->match) {
         daq_trace((KERN_INFO "match function is empty\n"));
         kfree(isa_dev);
         spin_unlock(&dev_list_lock);
         return -EFAULT;
      }

      if (isa_drv->match(isa_dev, isa_drv) == 0) {
         if (!isa_drv->probe) {
            daq_trace((KERN_INFO "prove function is empty\n"));
            kfree(isa_dev);
            spin_unlock(&dev_list_lock);
            return -EFAULT;
         }
         
         ret = isa_drv->probe(isa_dev);
         if (ret) {
            daq_trace((KERN_INFO "Fail to probe isa device\n"));
            kfree(isa_dev);
            spin_unlock(&dev_list_lock);
            return ret;
         }

//         daq_isa_add_device(isa_drv, isa_dev);
         break;
      }
   }
   spin_unlock(&dev_list_lock);

   daq_isa_add_device(isa_drv, isa_dev);
   if (&isa_drv->isa_drivers == &daq_isa_drivers) {
      daq_trace((KERN_INFO "isa driver do not exist or match\n"));
      kfree(isa_dev);
      return -ENODEV;
   }
   return 0;
}


int daq_remove_isa_device(char *device_name, char * driver_name, uint32 iobase)
{
   int dev_rm_flag = 0;
   struct daq_isa_device * isa_dev = NULL;
   struct daq_isa_driver * isa_drv = NULL;
   device_name[ISA_NAME_MAX_LEN - 1] = 0;
   driver_name[ISA_NAME_MAX_LEN - 1] = 0;

   spin_lock(&dev_list_lock);
   list_for_each_entry(isa_drv, &daq_isa_drivers, isa_drivers) {
      list_for_each_entry(isa_dev, &isa_drv->devices, device_list) {
         if ((isa_drv->match(isa_dev,isa_drv) == 0)
             && (strcmp(isa_dev->device_name, device_name) == 0)
             && (isa_dev->iobase == iobase)) {
            if (!isa_drv->remove) {
               daq_trace((KERN_INFO "remove function is empty\n"));
               spin_unlock(&dev_list_lock);
               return -EFAULT;
            }

            dev_rm_flag = 1;
//            isa_drv->remove(isa_dev);
            break;
         }
      }

      if (dev_rm_flag) { 
         break;
      }
   }
   spin_unlock(&dev_list_lock);  
 
   if (!dev_rm_flag) {
      daq_trace((KERN_INFO "isa driver do not exist or match\n"));
      return -ENODEV;
   }else {
      isa_drv->remove(isa_dev);
   }

   daq_isa_remove_device(isa_drv, isa_dev);
   kfree(isa_dev);
  
   return 0;
}

EXPORT_SYMBOL_GPL(daq_isa_register_driver);
EXPORT_SYMBOL_GPL(daq_isa_unregister_driver);
EXPORT_SYMBOL_GPL(daq_isa_add_device);
EXPORT_SYMBOL_GPL(daq_isa_remove_device);
EXPORT_SYMBOL_GPL(daq_add_isa_device);
EXPORT_SYMBOL_GPL(daq_remove_isa_device);












