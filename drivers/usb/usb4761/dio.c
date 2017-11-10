/*
 * dio.c
 *
 *  Created on: 2013-6-15
 *      Author: 
 */
#include "kdriver.h"
#include "hw.h"

static
int daq_di_int_xfer_complete(struct urb *urb, void *context)
{
   daq_device_t  *daq_dev = (daq_device_t*)context;
   DEVICE_SHARED *shared  = &daq_dev->shared;
   FW_EVENT_FIFO *fifo;
   EVENT_DATA    *tail;
   int           activeEvent;

   if (urb->status) {
      daq_trace((KERN_ERR"xfer failed. error = %d\n", urb->status));
      return urb->status;
   }

   if (!urb->actual_length) {
      daq_trace((KERN_ERR"zero length packet\n"));
      return 0;
   }

   activeEvent = shared->IntCsr;
   fifo = (FW_EVENT_FIFO *)urb->transfer_buffer;
   tail = fifo->EventData + fifo->EventCount - 1;
   for (; tail >= fifo->EventData && activeEvent; --tail) {
      int evt_idx = KdxDiintChan0 + tail->EventType;
      if (tail->EventType < DI_INT_SRC_COUNT && (activeEvent & (0x1 << evt_idx))) {
         activeEvent &= ~(0x1 << evt_idx);
         shared->DiSnapState[tail->EventType].State[0] = tail->PortData;
         if (!shared->IsEvtSignaled[evt_idx]) {
            shared->IsEvtSignaled[evt_idx] = 1;
            daq_device_signal_event(daq_dev, evt_idx );
         }
      }
   }

   return 0;
}

//-------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------
void daq_dio_initialize_hw(daq_device_t *daq_dev)
{
   if (daq_dev->shared.InitOnLoad) {
      daq_usb_do_write_port(daq_dev, 0, DIO_PORT_COUNT, daq_dev->shared.DoPortState);
   }
}

int daq_ioctl_di_read_port(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_PORTS xbuf;
   __u8         data[DIO_PORT_COUNT];

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))) {
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (daq_usb_di_read_port(daq_dev, xbuf.PortStart, xbuf.PortCount, data) < 0) {
      return -EIO;
   }

   if (unlikely(copy_to_user(xbuf.Data, data, xbuf.PortCount))) {
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_do_write_port(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_PORTS  xbuf;
   __u8          data[DIO_PORT_COUNT];
   unsigned long flags;
   __u8*         state;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))) {
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (unlikely(copy_from_user(data, (void *)xbuf.Data, xbuf.PortCount))) {
      return -EFAULT;
   }

   if (daq_usb_do_write_port(daq_dev, xbuf.PortStart, xbuf.PortCount, data) < 0) {
      return -EIO;
   }

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   state = data;
   while( xbuf.PortCount-- )
   {
      // Save the DO port value for read-back
      daq_dev->shared.DoPortState[xbuf.PortStart++] = *state++;
      xbuf.PortStart %= DIO_PORT_COUNT;
   }
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   return 0;
}

int daq_ioctl_do_write_bit(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_RW_BIT    xbuf;
   __u8          status;
   unsigned      port, bit;
   unsigned long flags;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }
   xbuf.Port %= DIO_PORT_COUNT;

   port = (xbuf.Port) % DIO_PORT_COUNT;
   bit  = xbuf.Bit;

   status = daq_dev->shared.DoPortState[port];
   status = ((xbuf.Data & 0x1) << bit) | (~(1 << bit) & status);

   if (daq_usb_do_write_port(daq_dev, port, 1, &status) < 0){
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

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))) {
      return -EFAULT;
   }

   xbuf.PortStart %= DIO_PORT_COUNT;
   xbuf.PortCount  = min((unsigned)DIO_PORT_COUNT, xbuf.PortCount);
   if (daq_usb_do_read_port(daq_dev, xbuf.PortStart, xbuf.PortCount, data) < 0) {
      return -EIO;
   }

   if (unlikely(copy_to_user((void *)xbuf.Data, data, xbuf.PortCount))) {
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_diint_set_param(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_SET_DIINT_CFG xbuf;
   __u8              *dest;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))) {
      return -EFAULT;
   }

   if (xbuf.SrcStart + xbuf.SrcCount > DI_INT_SRC_COUNT) {
      return -EINVAL;
   }

   if (xbuf.SetWhich == DIINT_SET_TRIGEDGE) {
      dest = daq_dev->shared.DiintTrigEdge + xbuf.SrcStart;
   } else {
      return -EINVAL;
   }

   if (unlikely(copy_from_user(dest, xbuf.Buffer, min(xbuf.SrcCount, (uint32)DI_INT_SRC_COUNT)))) {
      return -EFAULT;
   }

   return 0;
}

int daq_ioctl_di_start_snap(daq_device_t *daq_dev, unsigned long arg)
{
   DIO_START_DI_SNAP xbuf;
   unsigned long     flags;
   unsigned          event_kdx, src_idx;
   int               ret;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))) {
      return -EFAULT;
   }

   event_kdx = GetEventKIndex(xbuf.EventId);
   src_idx   = event_kdx - KdxDiBegin;
   if (src_idx >= DI_SNAP_SRC_COUNT) {
      return -EINVAL;
   }

   daq_device_clear_event(daq_dev, event_kdx);
   daq_dev->shared.IsEvtSignaled[event_kdx] = 0;
   daq_dev->shared.DiSnapParam[src_idx].PortStart = (__u8)xbuf.PortStart;
   daq_dev->shared.DiSnapParam[src_idx].PortCount = (__u8)xbuf.PortCount;

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   daq_dev->shared.IntCsr |= 0x1 << event_kdx;
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   if (!is_reader_inited(&daq_dev->usb_reader)) {
      ret = daq_usb_reader_init(daq_dev->udev, daq_dev->xfer_epd,
               sizeof(FW_EVENT_FIFO), 1, daq_di_int_xfer_complete, daq_dev, &daq_dev->usb_reader);
      if (ret) {
         return ret;
      }
   }

   ret = daq_usb_reader_start(&daq_dev->usb_reader);
   if (ret) {
      return ret;
   }

   daq_usb_enable_hw_event(daq_dev, src_idx, 1, INT_SRC_DI,
      daq_dev->shared.DiintTrigEdge[src_idx] == RisingEdge ? TRIG_EDGE_RISING : TRIG_EDGE_FALLING );

   return 0;
}

int daq_ioctl_di_stop_snap(daq_device_t *daq_dev, unsigned long arg)
{
   unsigned long flags;
   unsigned      event_kdx, src_idx;

   event_kdx = GetEventKIndex((__u32)arg);
   src_idx   = event_kdx - KdxDiBegin;
   if (src_idx >= DI_SNAP_SRC_COUNT) {
      return -EINVAL;
   }

   daq_usb_enable_hw_event(daq_dev, src_idx, 0, 0, 0);

   spin_lock_irqsave(&daq_dev->dev_lock, flags);
   daq_dev->shared.IntCsr &= ~(0x1 << event_kdx);
   spin_unlock_irqrestore(&daq_dev->dev_lock, flags);

   if (!daq_dev->shared.IntCsr) {
      daq_usb_reader_stop(&daq_dev->usb_reader);
   }

   return 0;
}
