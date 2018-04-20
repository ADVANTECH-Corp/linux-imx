/*
 * ao.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */
#include "kdriver.h"

void daq_ao_initialize_hw(daq_device_t *daq_dev)
{
   DEVICE_SHARED *shared = &daq_dev->shared;
   int           i;

   for (i = 0; i < AO_CHL_COUNT; ++i){
     daq_usb_ao_write_channel(daq_dev, i, shared->AoChanState[i]);
   }
}

int daq_ioctl_ao_set_channel(daq_device_t *daq_dev, unsigned long arg)
{
   AO_SET_CHAN xbuf;
   __u32       gain[AO_CHL_COUNT], i;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   if (unlikely((xbuf.SetWhich & AO_SET_CHVRG) == 0)){
      return 0;
   }

   if (unlikely(xbuf.ChanCount > AO_CHL_COUNT)){
      xbuf.ChanCount = AO_CHL_COUNT;
   }

   if (unlikely(copy_from_user(gain, (void *)xbuf.Gains, sizeof(__u32) * xbuf.ChanCount))){
      return -EFAULT;
   } else {
      __u32 ch;
      for (i = 0 ; i < xbuf.ChanCount; ++i) {
         ch = (xbuf.ChanStart + i) & AO_CHL_MASK;
         daq_dev->shared.AoChanGain[ch] = gain[i];
      }
      return 0;
   }
}

int daq_ioctl_ao_write_sample(daq_device_t *daq_dev, unsigned long arg)
{
   AO_WRITE_SAMPLES xbuf;
   __u16            sample[AO_CHL_COUNT];
   __u32            errStatus;
   __u32            i;

   if (unlikely(copy_from_user(&xbuf, (void *)arg, sizeof(xbuf)))){
      return -EFAULT;
   }

   if (daq_usb_get_last_error(daq_dev, &errStatus) < 0){
      return -EIO;
   }
   if (errStatus){
      return -EBUSY;
   }

   xbuf.ChanStart &= AO_CHL_MASK;
   if (xbuf.ChanCount > AO_CHL_COUNT){
      xbuf.ChanCount = AO_CHL_COUNT;
   }

   if (unlikely(copy_from_user(sample, (void *)xbuf.Data, sizeof(__u16) * xbuf.ChanCount))){
      return -EFAULT;
   }

   for(i = 0 ; i < xbuf.ChanCount; ++i )
   {
      __u32 ch = ( xbuf.ChanStart + i ) & AO_CHL_MASK;
      if (daq_usb_ao_write_channel(daq_dev, ch, sample[i]) < 0){
         return -EIO;
      }
   }

   return 0;
}
