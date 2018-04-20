/*
 * usbreader.c
 *
 *  Created on: 2011-11-21
 *      Author: rocky
 */

//#ifdef CONFIG_USB  disable it because debain

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <adv/bdaqdef.h>
#include <adv/linux/biokernbase.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
//#endif

static
void daq_reader_on_complete(struct urb * urb)
{
   daq_usb_reader_t *reader = urb->context;

   if (reader->complete
      && reader->complete(urb, reader->context) == DAQ_UR_CONTINUE){
      usb_submit_urb(urb, GFP_ATOMIC);
   }
}

int daq_usb_reader_init(
   struct usb_device                    *udev,
   const struct usb_endpoint_descriptor *epd,
   unsigned int                         packet_size,
   unsigned int                         packet_count,
   daq_usb_complete_t                   on_read_complete,
   void                                 *context,
   daq_usb_reader_t                     *reader)
{
   struct urb *cur_urb;
   void       *data_buf;
   unsigned   pipe;

   // Required packet count exceeds limitation.
   if (packet_count > MAX_PENDING_PACKET_COUNT){
      return -EINVAL;
   }

   // Not a input ENDPOINT.
   if (USB_EP_DIR(epd) != USB_DIR_IN)
   {
      return -EINVAL;
   }

   if (USB_EP_XFER_TYPE(epd) == USB_ENDPOINT_XFER_INT){
      pipe = usb_rcvintpipe(udev, epd->bEndpointAddress);
   } else if (USB_EP_XFER_TYPE(epd) == USB_ENDPOINT_XFER_BULK){
      pipe = usb_rcvbulkpipe(udev, epd->bEndpointAddress);
   } else {
      return -EINVAL; // Not supported pipe type.
   }

   memset(reader, 0, sizeof(*reader));

   // prepare URBs
   for (;reader->urb_count < packet_count; ++reader->urb_count)
   {
      // allocate URB
      cur_urb = usb_alloc_urb(0, GFP_KERNEL);
      if (cur_urb == NULL){
         break;
      }

      // allocate DMA buffer for the URB
      data_buf = usb_alloc_coherent(udev, packet_size, GFP_KERNEL, &cur_urb->transfer_dma);
      if (data_buf == NULL){
         usb_free_urb(cur_urb);
         break;
      }

      // fill the URB
      if (USB_EP_XFER_TYPE(epd) == USB_ENDPOINT_XFER_INT){
         daq_trace((KERN_INFO "init int xfer, interval = %d\n", epd->bInterval));
         usb_fill_int_urb(cur_urb, udev, pipe, data_buf, packet_size,
            (usb_complete_t)daq_reader_on_complete, reader, epd->bInterval);
      } else{
         usb_fill_bulk_urb(cur_urb, udev, pipe,data_buf, packet_size,
            (usb_complete_t)daq_reader_on_complete, reader);
      }

      cur_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
      reader->urbs[reader->urb_count] = cur_urb;
   }

   // Something is wrong, rolling back now.
   if (reader->urb_count < packet_count){
      while (reader->urb_count){
         struct urb *cur_urb = reader->urbs[--reader->urb_count];
         reader->urbs[reader->urb_count] = NULL;
         usb_free_coherent(udev, packet_size, cur_urb->transfer_buffer, cur_urb->transfer_dma);
         usb_free_urb(cur_urb);
      }
      return -ENOMEM;
   }

   // Okay, everything looks good. go ahead.
   reader->complete = on_read_complete;
   reader->context  = context;
   return 0;
}

int daq_usb_reader_start(daq_usb_reader_t *reader)
{
   int i, ret = 0;

   if (!reader->urb_count){
      return -EINVAL;
   }

   if (!reader->running){
      for (i = 0; i < reader->urb_count; ++i){
         ret = usb_submit_urb(reader->urbs[i], GFP_ATOMIC);
         if (ret){
            daq_trace((KERN_ERR"submit %dth URB failed:%d\n", i, ret));
            break;
         }
      }
      reader->running = i;
   }

   return ret;
}

void daq_usb_reader_stop(daq_usb_reader_t *reader)
{
   while (reader->running){
      --reader->running;
      usb_kill_urb(reader->urbs[reader->running]);
   }
}

void daq_usb_reader_cleanup(daq_usb_reader_t *reader)
{
   if (reader->running){
      daq_usb_reader_stop(reader);
   }

   while(reader->urb_count){
      struct urb *cur_urb = reader->urbs[--reader->urb_count];
      usb_free_coherent(cur_urb->dev, cur_urb->transfer_buffer_length, cur_urb->transfer_buffer, cur_urb->transfer_dma);
      usb_free_urb(cur_urb);
   }

   memset(reader, 0, sizeof(*reader));
}

EXPORT_SYMBOL_GPL(daq_usb_reader_init);
EXPORT_SYMBOL_GPL(daq_usb_reader_start);
EXPORT_SYMBOL_GPL(daq_usb_reader_stop);
EXPORT_SYMBOL_GPL(daq_usb_reader_cleanup);

#endif
