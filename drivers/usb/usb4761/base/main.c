/*
 * main.c
 *
 *  Created on: 2011-9-8
 *      Author: rocky
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <adv/bdaqdef.h>
#include <adv/linux/ioctls.h>
#include <adv/linux/biokernbase.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

/************************************************************************
 * Private Macros
 ************************************************************************/
#define KLIB_DEV_MINOR       255
#define IDMAP_INDEX(minor)   (minor / 32)
#define IDMAP_BIT(minor)     (minor % 32)

/************************************************************************
 * Private Types
 ************************************************************************/
typedef struct daq_klib_device{
   struct cdev cdev;     // char device structure
}daq_klib_device_t;

typedef struct daq_klib_file_ctx{
   spinlock_t  lock;
   daq_event_t *events[64];
}daq_klib_file_ctx_t;

/************************************************************************
 * Private data
 ************************************************************************/
static struct class      *daq_class;
static dev_t             daq_dev_id;
static u32               dev_id_bitmap[256 / 32];
static DEFINE_SPINLOCK(dev_id_map_lock);

// The only device instance
static daq_klib_device_t daq_klib_dev;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
static struct class_device *sysfs_dev;
#else
static struct device       *sysfs_dev;
#endif

/************************************************************************
 * KLib Public Methods
 *************************************************************************/
/*
 * Alloc a dev id. return (dev_t)0 if error.
 * */
dev_t daq_devid_alloc(void)
{
   dev_t id = (dev_t)0;
   int   i  = 0, j = 0;

   spin_lock(&dev_id_map_lock);
   for (i = 0; i < ARRAY_SIZE(dev_id_bitmap); ++i){
      if (dev_id_bitmap[i] != (u32)-1){
         for (j = 0; j < 32; ++j){
            // Found! Mark it as used and return.
            if ((dev_id_bitmap[i] & (0x1 << j)) == 0){
               dev_id_bitmap[i] |= 0x1 << j;
               break;
            }
         }
         break;
      }
   }
   spin_unlock(&dev_id_map_lock);

   // Found a valid minor number
   if ((i * 32 + j) < KLIB_DEV_MINOR){
      id = MKDEV(MAJOR(daq_dev_id), (i * 32 + j));
   }
   return id;
}

/*
 * Free a dev id.
 * */
void daq_devid_free(dev_t id)
{
   int ma = MAJOR(id);
   int mi = MINOR(id);

   if (ma != MAJOR(daq_dev_id) || ((u32)mi) >= KLIB_DEV_MINOR){
      return;
   }

   spin_lock(&dev_id_map_lock);
   dev_id_bitmap[IDMAP_INDEX(mi)] &= ~(0x1 << IDMAP_BIT(mi));
   spin_unlock(&dev_id_map_lock);
}

/*
 * Get device class
 * */
struct class * daq_class_get(void)
{
   return daq_class;
}

/************************************************************************
 * File Operation Methods
 *************************************************************************/
static int daq_klib_file_open(struct inode *in, struct file *fp)
{
   daq_klib_file_ctx_t *ctx = (daq_klib_file_ctx_t *)kzalloc(sizeof(*ctx), GFP_KERNEL);

   if (ctx == NULL){
      return -ENOMEM;
   }

   spin_lock_init(&ctx->lock);
   fp->private_data = ctx;
   return 0;
}

static int daq_klib_file_close(struct inode *inode, struct file *filp)
{
   daq_klib_file_ctx_t *ctx =(daq_klib_file_ctx_t *)filp->private_data;
   daq_event_t *events[64];
   int         i;

   spin_lock(&ctx->lock);
   memcpy(events, ctx->events, sizeof(ctx->events));
   memset(ctx->events, 0, sizeof(ctx->events));
   spin_unlock(&ctx->lock);

   for (i = 0; i < 64; ++i){
      if (events[i]){
         daq_event_close(events[i]);
      }
   }

   kfree(ctx);
   filp->private_data = NULL;
   return 0;
}

static long daq_klib_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   switch(cmd)
   {
   case IOCTL_KLIB_CREATE_EVENT:
      {
         daq_klib_file_ctx_t *ctx = filp->private_data;
         void  *event;
         int   slot;

         event = daq_event_create();
         if (event == NULL){
            return -ENOMEM;
         }

         spin_lock(&ctx->lock);
         for (slot = 0; slot < 64; ++slot){
            if (!ctx->events[slot]){
               ctx->events[slot] = event;
               break;
            }
         }
         spin_unlock(&ctx->lock);

         if (slot >= 64) {
            daq_event_close(event);
            return -EXFULL;
         }

         if (unlikely(put_user(event, (void **)arg)) < 0){
            ctx->events[slot] = NULL;
            daq_event_close(event);
            return -EFAULT;
         }
         return 0;
      }
      break;
   case IOCTL_KLIB_CLOSE_EVENT:
      {
         daq_klib_file_ctx_t *ctx = filp->private_data;
         int   slot;

         spin_lock(&ctx->lock);
         for (slot = 0; slot < 64; ++slot){
            if (ctx->events[slot] == (void *)arg){
               ctx->events[slot] = NULL;
            }
         }
         spin_unlock(&ctx->lock);

         daq_event_close((void *)arg);
         return 0;
      }
      break;
   case IOCTL_KLIB_SET_EVENT:
      {
         daq_event_set((void *)arg);
         return 0;
      }
      break;
   case IOCTL_KLIB_RESET_EVENT:
      {
         daq_event_reset((void *)arg);
         return 0;
      }
      break;
   case IOCTL_KLIB_WAIT_EVENTS:
      {
         KLIB_WAIT_EVENTS xbuf;
         daq_event_t      *events[MAX_WAIT_OBJECTS];

         if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
            return -EFAULT;
         }

         if (xbuf.Count > MAX_WAIT_OBJECTS){
            return -EINVAL;
         }

         if (unlikely(copy_from_user(events, xbuf.Events, sizeof(void *) * xbuf.Count))){
            return -EFAULT;
         }

         xbuf.Result = daq_event_wait(xbuf.Count, events, xbuf.WaitAll, xbuf.Timeout);
         put_user(xbuf.Result, &((KLIB_WAIT_EVENTS *)arg)->Result);
         return xbuf.Result < 0 ? xbuf.Result : 0;
      }
      break;
   case IOCTL_KLIB_ADD_DEV:
      {
         ISA_DEV_INFO xbuf;
         if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
            return -EFAULT;
         }
         
         return daq_add_isa_device(xbuf.device_name, xbuf.driver_name, xbuf.iobase, xbuf.irq, xbuf.dmachan);
      }
   case IOCTL_KLIB_REMOVE_DEV:
      {
         ISA_DEV_INFO xbuf;
         if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
            return -EFAULT;
         }

         return daq_remove_isa_device(xbuf.device_name, xbuf.driver_name, xbuf.iobase);
      }
   default:
      break;
   }
   return -ENOTTY;
}

static struct file_operations daq_klib_fops = {
   .owner          = THIS_MODULE,
   .open           = daq_klib_file_open,
   .release        = daq_klib_file_close,
   .unlocked_ioctl = daq_klib_file_ioctl,
};

/************************************************************************
* sysfs support routines
************************************************************************/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
static ssize_t daq_klib_show_desc(struct class_device * dev, char * buf)
{
   return sprintf(buf, "Kernel Base of BDAQ drivers");
}
static ssize_t daq_klib_show_vendor(struct class_device *dev, char * buf)
{
   return sprintf(buf, "Advantech");
}
static ssize_t daq_klib_show_device(struct class_device *dev, char * buf)
{
   return sprintf(buf, "BDaqKernBase");
}
static ssize_t daq_klib_show_driver(struct class_device *dev, char * buf)
{
   return sprintf(buf, "biokernbase");
}

static CLASS_DEVICE_ATTR(desc,   S_IRUGO, daq_klib_show_desc,   NULL);
static CLASS_DEVICE_ATTR(vendor, S_IRUGO, daq_klib_show_vendor, NULL);
static CLASS_DEVICE_ATTR(device, S_IRUGO, daq_klib_show_device, NULL);
static CLASS_DEVICE_ATTR(driver, S_IRUGO, daq_klib_show_driver, NULL);
#else
static ssize_t daq_klib_show_desc(struct device * dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, "Kernel Base of BDAQ drivers");
}
static ssize_t daq_klib_show_vendor(struct device *dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, "Advantech");
}
static ssize_t daq_klib_show_device(struct device *dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, "BDaqKernBase");
}
static ssize_t daq_klib_show_driver(struct device *dev, struct device_attribute * attr, char * buf)
{
   return sprintf(buf, "biokernbase");
}

static DEVICE_ATTR(desc,   S_IRUGO, daq_klib_show_desc,   NULL);
static DEVICE_ATTR(vendor, S_IRUGO, daq_klib_show_vendor, NULL);
static DEVICE_ATTR(device, S_IRUGO, daq_klib_show_device, NULL);
static DEVICE_ATTR(driver, S_IRUGO, daq_klib_show_driver, NULL);
#endif

/************************************************************************
 * Private Methods
 *************************************************************************/
static int __init daq_klib_init(void)
{
   dev_t klib_devid = 0;
   int   dev_inited = 0;
   int   ret        = 0;

   daq_trace((KERN_INFO "daq: klib initialized\n"));

   do{
      /* Allocate dynamic device number */
      if ((ret = alloc_chrdev_region(&daq_dev_id, 0, 256, "daq")) != 0) {
         daq_trace((KERN_ERR "failed to alloc device number.0x%x\n",ret));
         break;
      }

      /* This device using the minor number '255' */
      klib_devid = MKDEV(MAJOR(daq_dev_id), KLIB_DEV_MINOR);
      dev_id_bitmap[IDMAP_INDEX(KLIB_DEV_MINOR)] |= 0x1 << IDMAP_BIT(KLIB_DEV_MINOR);

      /* Register our device class */
      daq_class = class_create(THIS_MODULE, "daq");
      if (IS_ERR(daq_class)){
         daq_trace((KERN_ERR "failed in creating device class.\n"));
         ret = PTR_ERR(daq_class);
         break;
      }

      /* Register the device into kernel */
      cdev_init(&daq_klib_dev.cdev, &daq_klib_fops);
      daq_klib_dev.cdev.owner = THIS_MODULE;
      if ((ret = cdev_add(&daq_klib_dev.cdev, klib_devid, 1)) != 0) {
         daq_trace((KERN_ERR "daq: register char device failed.0x%x\n", ret));
         break;
      }
      dev_inited = 1;

      /* Register the device into sysfs */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
      sysfs_dev = class_device_create(daq_class, NULL, klib_devid, NULL,"daq""%d", MINOR(klib_devid));
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
      sysfs_dev = device_create(daq_class, NULL, klib_devid, "daq""%d", MINOR(klib_devid));
#else
      sysfs_dev = device_create(daq_class, NULL, klib_devid, &daq_klib_dev, "daq""%d", MINOR(klib_devid));
#endif
#endif

      if (IS_ERR(sysfs_dev)){
         daq_trace( (KERN_INFO "Registered character device failed\n"));
         ret = PTR_ERR(sysfs_dev);
         break;
      }
	  
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
      class_set_devdata(sysfs_dev, &daq_klib_dev);
	  /* Create attribute files of the device */
      if (class_device_create_file(sysfs_dev, &class_device_attr_desc))   break;
      if (class_device_create_file(sysfs_dev, &class_device_attr_vendor)) break;
      if (class_device_create_file(sysfs_dev, &class_device_attr_device)) break;
      if (class_device_create_file(sysfs_dev, &class_device_attr_driver)) break;
#else
      dev_set_drvdata(sysfs_dev, &daq_klib_dev);
      /* Create attribute files of the device */
      if (device_create_file(sysfs_dev, &dev_attr_desc))   break;
      if (device_create_file(sysfs_dev, &dev_attr_vendor)) break;
      if (device_create_file(sysfs_dev, &dev_attr_device)) break;
      if (device_create_file(sysfs_dev, &dev_attr_driver)) break;
#endif

   } while(0);

   /* Some error occured, clean up now*/
   if (ret != 0){
      if (sysfs_dev != NULL && !IS_ERR(sysfs_dev)){
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
         class_device_destroy(daq_class, klib_devid);
#else
         device_destroy(daq_class, klib_devid);
#endif
      }

      if (dev_inited){
         cdev_del(&daq_klib_dev.cdev);
      }

      if (daq_class != NULL && !IS_ERR(daq_class)){
         class_destroy(daq_class);
      }

      if (daq_dev_id != 0){
         unregister_chrdev_region(daq_dev_id, 256);
      }
   }

   return ret;
}

/************************************************************************
 * static void __exit advdrv_cleanup(void)
 *
 * Description:  Close driver - Clean up remaining memory and unregister
 *                         the major.256
 *************************************************************************/
static void __exit daq_klib_exit(void)
{
   daq_trace((KERN_INFO "daq: klib uninitialized\n"));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))
   class_device_destroy(daq_class, MKDEV(MAJOR(daq_dev_id), KLIB_DEV_MINOR));
#else
   device_destroy(daq_class, MKDEV(MAJOR(daq_dev_id), KLIB_DEV_MINOR));
#endif
   class_destroy(daq_class);
   cdev_del(&daq_klib_dev.cdev);
   unregister_chrdev_region(daq_dev_id, 256);
}

module_init(daq_klib_init);
module_exit(daq_klib_exit);

MODULE_LICENSE("GPL");
EXPORT_SYMBOL_GPL(daq_devid_alloc);
EXPORT_SYMBOL_GPL(daq_devid_free);
EXPORT_SYMBOL_GPL(daq_class_get);
