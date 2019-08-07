/*
 * fops.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */
#include "kdriver.h"

static
daq_file_ctx_t * daq_file_alloc_context(daq_device_t *daq_dev)
{
   daq_file_ctx_t *ctx = NULL;
   unsigned long  flags;
   int i;

   if (likely(daq_dev->file_ctx_pool_size)){
      spin_lock_irqsave(&daq_dev->dev_lock, flags);
      for (i = 0; i < daq_dev->file_ctx_pool_size; ++i){
         if (!daq_dev->file_ctx_pool[i].busy){
            ctx = &daq_dev->file_ctx_pool[i];
            ctx->busy = 1;
            break;
         }
      }
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
   }

   if (ctx == NULL){
      ctx = (daq_file_ctx_t*)kzalloc(sizeof(daq_file_ctx_t), GFP_KERNEL);
   }

   if (ctx){
      INIT_LIST_HEAD(&ctx->ctx_list);
      ctx->daq_dev = daq_dev;
   }

   return ctx;
}

static
void daq_file_free_context(daq_file_ctx_t *ctx)
{
   daq_device_t  *daq_dev = ctx->daq_dev;
   unsigned long flags;

   if (likely(daq_dev->file_ctx_pool_size
       && daq_dev->file_ctx_pool <= ctx
       && ctx < daq_dev->file_ctx_pool + daq_dev->file_ctx_pool_size)){
      spin_lock_irqsave(&daq_dev->dev_lock, flags);
      ctx->busy = 0;
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
   } else {
      kfree(ctx);
   }
}

static
int daq_ioctl_dev_get_desc(daq_device_t *daq_dev, unsigned long arg)
{
   char desc[64];
   int  len;

   len = snprintf(desc, sizeof(desc), "%s, BID#%d",
               DEVICE_NAME_FROM_PID(daq_dev->shared.ProductId), daq_dev->shared.BoardId);
   if (unlikely(copy_to_user((void *)arg, desc, len + 1))){
     return -EFAULT;
   }

   return 0;
}

static
int daq_ioctl_dev_get_borad_ver(daq_device_t *daq_dev, unsigned long arg)
{
   char ver[64];
   int  len;

   len = daq_usb_dev_get_firmware_ver(daq_dev, ver, 64);
   if (unlikely(len < 0)){
      return -EIO;
   }

   if (unlikely(copy_to_user((void *)arg, ver, strlen(ver) + 1))){
      return -EFAULT;
   }

   return 0;
}

static
int daq_ioctl_dev_get_location(daq_device_t *daq_dev, unsigned long arg)
{
   char loc[64];
   int  len;

   len = usb_make_path(daq_dev->udev, loc, 64);
   if (unlikely(len < 0)){
      return -EFAULT;
   }

   if (unlikely(copy_to_user((void *)arg, loc, len + 1))){
      return -EFAULT;
   }

   return 0;
}

static
int daq_ioctl_dev_set_board_id(daq_device_t *daq_dev, unsigned long arg)
{
   __u32 id = (__u32)arg;

   if (daq_dev->shared.BoardId == id)
      return 0;

   if (unlikely(daq_usb_dev_set_board_id_oscsd(daq_dev, id, daq_dev->shared.HwAiChType, daq_dev->shared.Oscillator) < 0)){
      return -EIO;
   }

   daq_dev->shared.BoardId = id;
   return 0;
}

static
int daq_ioctl_dev_locate_device(daq_device_t *daq_dev, unsigned long arg)
{
   __u8 enable = (__u8)arg;

   if (unlikely(daq_usb_dev_locate_device(daq_dev, enable) < 0)){
      return -EIO;
   }

   return 0;
}

static
int daq_ioctl_dev_reg_event(daq_device_t *daq_dev, daq_file_ctx_t *ctx, unsigned long arg)
{
   unsigned long flags;
   unsigned      kdx;
   uint32        event_id;
   HANDLE        event;

   if (unlikely(get_user(event_id, &((USER_EVENT_INFO*)arg)->EventId))){
      return -EFAULT;
   }

   kdx = GetEventKIndex(event_id);
   if (likely(kdx < KrnlSptedEventCount)){
      event = ctx->events[kdx];
      if (event == NULL){
         event = daq_event_create();

         spin_lock_irqsave(&daq_dev->dev_lock, flags);
         if (ctx->events[kdx] == NULL){
            ctx->events[kdx] = event;
         } else {
            daq_event_close(event);
            event = ctx->events[kdx];
         }
         spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
      }

      put_user(event, &((USER_EVENT_INFO*)arg)->Handle);
      return event != NULL ? 0 : -ENOMEM;
   }

   return -EINVAL;
}

static
int daq_ioctl_dev_unreg_event(daq_device_t *daq_dev, daq_file_ctx_t *ctx, unsigned long arg)
{
   unsigned long flags;
   unsigned      kdx = GetEventKIndex(arg);

   if (likely(kdx < KrnlSptedEventCount)){
      spin_lock_irqsave(&daq_dev->dev_lock, flags);
      if (ctx->events[kdx] != NULL) {
         daq_event_close(ctx->events[kdx]);
         ctx->events[kdx] = NULL;
      }
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
   }

   return 0;
}

static
int daq_ioctl_dev_dbg_usb_in(daq_device_t *daq_dev, unsigned long arg)
{
   DBG_USB_IO xbuf;
   __u8       stack_buf[DBGIO_STACKBUF_THR];
   __u8       *params = stack_buf;
   int        ret = 0;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   if (xbuf.Length > DBGIO_MAX_LENGTH){
      return -EINVAL;
   }

   if (xbuf.Length > sizeof(stack_buf)){
      params = kmalloc(xbuf.Length, GFP_KERNEL);
      if (params == NULL){
         return -ENOMEM;
      }
   }

   if (unlikely(daq_usb_dev_dbg_input(daq_dev, xbuf.MajorCmd, xbuf.MinorCmd, xbuf.Length, params) < 0)){
      ret = -EIO;
   } else {
      if (unlikely(copy_to_user((void *)xbuf.Data, params, xbuf.Length))){
         ret = -EFAULT;
      }
   }

   if (params != stack_buf){
      kfree(params);
   }

   return ret;
}

static
int daq_ioctl_dev_dbg_usb_out(daq_device_t *daq_dev, unsigned long arg)
{
   DBG_USB_IO xbuf;
   __u8       stack_buf[DBGIO_STACKBUF_THR];
   __u8       *params = stack_buf;
   int        ret     = 0;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   if (xbuf.Length > DBGIO_MAX_LENGTH){
      return -EINVAL;
   }

   if (xbuf.Length > sizeof(stack_buf)){
      params = kmalloc(xbuf.Length, GFP_KERNEL);
      if (params == NULL){
         return -ENOMEM;
      }
   }

   if (unlikely(copy_from_user(params, (void *)xbuf.Data, xbuf.Length))){
      ret = -EFAULT;
   } else {
      if (unlikely(daq_usb_dev_dbg_output(daq_dev, xbuf.MajorCmd, xbuf.MinorCmd, xbuf.Length, params) < 0)){
         ret = -EIO;
      }
   }

   if (params != stack_buf){
      kfree(params);
   }

   return ret;
}

//-------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------
int daq_file_open(struct inode *in, struct file *fp)
{
   daq_device_t   *daq_dev = container_of(in->i_cdev, daq_device_t, cdev);
   daq_file_ctx_t *ctx;
   unsigned long  flags;
   int            first_user, fmode_chk_ret = 0;

   if (unlikely(daq_dev->udev == NULL)){
      return -ENODEV;
   }

   ctx = daq_file_alloc_context(daq_dev);
   if (unlikely(ctx == NULL)){
      return -ENOMEM;
   }
   ctx->write_access = fp->f_mode & FMODE_WRITE;

   spin_lock_irqsave(&daq_dev->dev_lock, flags);

   first_user = list_empty(&daq_dev->file_ctx_list);
   if (ctx->write_access){
      daq_file_ctx_t *curr;
      list_for_each_entry(curr, &daq_dev->file_ctx_list, ctx_list){
         if (curr->write_access) {
            fmode_chk_ret = -EPERM;
            break;
         }
      }
   }

   if (!fmode_chk_ret){
      list_add(&ctx->ctx_list, &daq_dev->file_ctx_list);
      fp->private_data = ctx;
   }

   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   if (fmode_chk_ret){
      daq_file_free_context(ctx);
   } else {
      if (first_user){
         daq_usb_dev_init_firmware(daq_dev, 1);
         //daq_usb_cntr_get_base_clk(daq_dev, &daq_dev->shared.CntrBaseClk);
      }
   }

   return fmode_chk_ret;
}

int daq_file_close(struct inode *inode, struct file *filp)
{
   daq_file_ctx_t *ctx     = filp->private_data;
   daq_device_t   *daq_dev = ctx->daq_dev;
   unsigned long  flags;
   int            i, last_user;

   if (ctx->write_access) {
      daq_cntr_reset(daq_dev, 0, CNTR_CHL_COUNT);
      daq_fai_stop_acquisition(daq_dev, 1);
   }

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   list_del(&ctx->ctx_list);
   last_user = list_empty(&daq_dev->file_ctx_list);
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   for (i = 0; i < KrnlSptedEventCount; ++i){
      if (ctx->events[i]){
         daq_event_close(ctx->events[i]);
         ctx->events[i] = NULL;
      }
   }

   daq_file_free_context(ctx);

   if (last_user){
      daq_usb_dev_init_firmware(daq_dev, 0);
      if (daq_dev->udev == NULL){
         daq_device_cleanup(daq_dev);
      }
   }

   return 0;
}

int daq_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
   if (vma->vm_pgoff == 0){
      daq_device_t *daq_dev = ((daq_file_ctx_t *)filp->private_data)->daq_dev;
      if (vma->vm_end - vma->vm_start > PAGE_SIZE){
         return -EIO;
      }

      return remap_pfn_range(vma, vma->vm_start,
               virt_to_phys((void *)daq_dev) >> PAGE_SHIFT,
               vma->vm_end - vma->vm_start, vma->vm_page_prot);
   }
   return -EIO;
}

long daq_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   daq_file_ctx_t *ctx     = filp->private_data;
   daq_device_t   *daq_dev = ctx->daq_dev;
   int            ret;

   switch(cmd)
   {
   //******************************************************************************************
   // IOCTL for device operation                                                              *
   //******************************************************************************************
   case IOCTL_DEVICE_GET_DESC:
      ret = daq_ioctl_dev_get_desc(daq_dev, arg);
      break;
   case IOCTL_DEVICE_GET_BOARD_VER:
      ret = daq_ioctl_dev_get_borad_ver(daq_dev, arg);
      break;
   case IOCTL_DEVICE_GET_LOCATION_INFO:
      ret = daq_ioctl_dev_get_location(daq_dev, arg);
      break;
   case IOCTL_DEVICE_SET_BOARDID:
      ret = daq_ioctl_dev_set_board_id(daq_dev, arg);
      break;
   case IOCTL_DEVICE_LOCATE_DEVICE:
      ret = daq_ioctl_dev_locate_device(daq_dev, arg);
      break;
   case IOCTL_DEVICE_REGISTER_USER_EVENT:
      ret = daq_ioctl_dev_reg_event(daq_dev, ctx, arg);
      break;
   case IOCTL_DEVICE_UNREGISTER_USER_EVENT:
      ret = daq_ioctl_dev_unreg_event(daq_dev, ctx, arg);
      break;
   case IOCTL_DEVICE_NOTIFY_PROP_CHGED:
      ret = daq_device_signal_event(daq_dev, KdxDevPropChged);
      break;
   case IOCTL_DEVICE_DBG_USB_IN:
      ret = daq_ioctl_dev_dbg_usb_in(daq_dev, arg);
      break;
   case IOCTL_DEVICE_DBG_USB_OUT:
      ret = daq_ioctl_dev_dbg_usb_out(daq_dev, arg);
      break;
   //******************************************************************************************
   // IOCTL for AI operation                                                                 *
   //******************************************************************************************
   case IOCTL_AI_SET_CHAN_CFG:
      ret = daq_ioctl_ai_set_channel(daq_dev, arg);
      break;
   case IOCTL_AI_READ_SAMPLES:
      ret = daq_ioctl_ai_read_sample(daq_dev, arg);
      break;
   case IOCTL_FAI_SET_PARAM:
      ret = daq_ioctl_fai_set_param(daq_dev, arg);
      break;
   case IOCTL_FAI_SET_BUFFER:
      ret = daq_ioctl_fai_set_buffer(daq_dev, arg);
      break;
   case IOCTL_FAI_START:
      ret = daq_ioctl_fai_start(daq_dev, arg);
      break;
   case IOCTL_FAI_STOP:
      ret = daq_ioctl_fai_stop(daq_dev, arg);
      break;
   //******************************************************************************************
   // IOCTL for AO operation                                                                 *
   //******************************************************************************************
   case IOCTL_AO_SET_CHAN_CFG:
      ret = daq_ioctl_ao_set_channel(daq_dev, arg);
      break;
   case IOCTL_AO_WRITE_SAMPLES:
      ret = daq_ioctl_ao_write_sample(daq_dev, arg);
      break;

   //******************************************************************************************
   // IOCTL for DIO operation                                                                 *
   //******************************************************************************************
   case IOCTL_DIO_READ_DI_PORTS:
      ret = daq_ioctl_di_read_port(daq_dev, arg);
      break;
   case IOCTL_DIO_WRITE_DO_PORTS:
      ret = daq_ioctl_do_write_port(daq_dev, arg);
      break;
   case IOCTL_DIO_READ_DO_PORTS:
      ret = daq_ioctl_do_read_port(daq_dev, arg);
      break;
   case IOCTL_DIO_WRITE_DO_BIT:
      ret = daq_ioctl_do_write_bit(daq_dev, arg);
      break;
      
   //******************************************************************************************
   // IOCTL for COUNTER operation                                                              *
   //******************************************************************************************
   case IOCTL_CNTR_SET_CFG:
      ret = daq_ioctl_cntr_set_param(daq_dev, arg);
      break;
   case IOCTL_CNTR_START:
      ret = daq_ioctl_cntr_start(daq_dev, arg);
      break;
   case IOCTL_CNTR_READ:
      ret = daq_ioctl_cntr_read(daq_dev, arg);
      break;
   case IOCTL_CNTR_RESET:
      ret = daq_ioctl_cntr_reset(daq_dev, arg);
      break;
   default:
      ret = -ENOTTY;
      break;
   }

   return ret;
}

int daq_device_signal_event(daq_device_t *daq_dev, unsigned kdx)
{
   daq_file_ctx_t *curr;
   unsigned long  flags;

   if (kdx < KrnlSptedEventCount){
      spin_lock_irqsave(&daq_dev->dev_lock, flags);
      list_for_each_entry(curr, &daq_dev->file_ctx_list, ctx_list){
         if (curr->events[kdx] != NULL){
            daq_event_set(curr->events[kdx]);
         }
      }
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
      return 0;
   }

   return -EINVAL;
}

int daq_device_clear_event(daq_device_t *daq_dev, unsigned kdx)
{
   daq_file_ctx_t *curr;
   unsigned long  flags;

   if (kdx < KrnlSptedEventCount){
      spin_lock_irqsave(&daq_dev->dev_lock, flags);
      list_for_each_entry(curr, &daq_dev->file_ctx_list, ctx_list){
         if (curr->events[kdx] != NULL){
            daq_event_reset(curr->events[kdx]);
         }
      }
      spin_unlock_irqrestore(&daq_dev->dev_lock, flags);
      return 0;
   }

   return -EINVAL;
}
