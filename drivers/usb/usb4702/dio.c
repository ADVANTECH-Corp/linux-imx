/*
 * dio.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */
#include "kdriver.h"

void daq_dio_initialize_hw(daq_device_t *daq_dev)
{
   if (daq_dev->shared.InitOnLoad){
      daq_usb_do_write_port(daq_dev, daq_dev->shared.DoPortState);
   }
}

int daq_ioctl_di_read_port(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_PORTS xbuf;
   __u8         data[DIO_PORT_COUNT];

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (daq_usb_di_read_port(daq_dev, data) < 0){
      return -EIO;
   }

   if (unlikely(copy_to_user(xbuf.Data, data, xbuf.PortCount))){
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_do_write_port(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_PORTS xbuf;
   __u8         data[DIO_PORT_COUNT];
   unsigned long flags;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (unlikely(copy_from_user(data, (void *)xbuf.Data, xbuf.PortCount))) {
      return -EFAULT;
   }

   if (daq_usb_do_write_port(daq_dev, data) < 0) {
      return -EIO;
   }

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   memcpy(daq_dev->shared.DoPortState, data, sizeof(data));
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   return 0;
}

int daq_ioctl_do_write_bit(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_BIT    xbuf;
   __u8          status;
   __u8          data[DIO_PORT_COUNT];
   unsigned      port, bit;
   unsigned long flags;
   
   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }
   xbuf.Port %= DIO_PORT_COUNT;

   port = (xbuf.Port) % DIO_PORT_COUNT;
   bit  = xbuf.Bit;

   memcpy(data, daq_dev->shared.DoPortState, sizeof(data));

   status = daq_dev->shared.DoPortState[port];
   status = ((xbuf.Data & 0x1) << bit) | (~(1 << bit) & status);
   data[port] = status;


   if (daq_usb_do_write_port(daq_dev, data) < 0) {
      return -EIO;
   }
   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   daq_dev->shared.DoPortState[port] = status;
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   return 0;
}

int daq_ioctl_do_read_port(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_PORTS xbuf;
   __u8         data[DIO_PORT_COUNT];

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (daq_usb_do_read_port(daq_dev, data) < 0){
      return -EIO;
   }

   if (unlikely(copy_to_user((void *)xbuf.Data, data, xbuf.PortCount))){
      return -EFAULT;
   }

   return 0;
}
